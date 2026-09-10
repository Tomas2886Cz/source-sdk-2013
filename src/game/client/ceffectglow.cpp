#include "cbase.h"
#include "ceffectglow.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier0/vprof.h"
#include "ivrenderview.h"
#include "materialsystem/itexture.h"
#include "igamesystem.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "tier1/utldict.h"

class CEntGlowSystem : public CAutoGameSystem
{
public:
	CEntGlowSystem() : CAutoGameSystem("CEntGlowSystem") {}

	virtual bool Init()
	{
		GetGlowEffect().Init();
		return true;
	}

	virtual void Shutdown()
	{
		GetGlowEffect().Shutdown();
	}
};

static CEntGlowSystem s_EntGlowSystem;

ConVar cl_glow_max_distance("cl_glow_max_distance", "400.0", FCVAR_ARCHIVE, "Maximum distance items will display an outline glow.");

struct GlowConfig_t
{
	float color[3];
};

static CUtlDict<GlowConfig_t, int> g_GlowPresetsDict;

void LoadGlowPresets(void)
{
	g_GlowPresetsDict.Purge();

	KeyValues* pKV = new KeyValues("GlowPresets");
	if (pKV->LoadFromFile(filesystem, "scripts/entity_outlines.txt", "MOD"))
	{
		for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey != NULL; pKey = pKey->GetNextKey())
		{
			const char* pszClassname = pKey->GetName();

			GlowConfig_t config;
			config.color[0] = pKey->GetFloat("r", 255.0f) / 255.0f;
			config.color[1] = pKey->GetFloat("g", 128.0f) / 255.0f;
			config.color[2] = pKey->GetFloat("b", 0.0f) / 255.0f;

			g_GlowPresetsDict.Insert(pszClassname, config);
		}
	}
	pKV->deleteThis();
}

bool GetColorForEntity(C_BaseEntity* pEntity, float flColorOut[3])
{
	if (!pEntity)
		return false;

	const char* pszClassname = pEntity->GetClassname();
	if (!pszClassname)
		return false;

	int idx = g_GlowPresetsDict.Find(pszClassname);
	if (idx != g_GlowPresetsDict.InvalidIndex())
	{
		flColorOut[0] = g_GlowPresetsDict[idx].color[0];
		flColorOut[1] = g_GlowPresetsDict[idx].color[1];
		flColorOut[2] = g_GlowPresetsDict[idx].color[2];
		return true;
	}
	return false;
}

void CEntGlowEffect::Init(void)
{
	m_GlowBuff1.InitRenderTarget(ScreenWidth() / 2, ScreenHeight() / 2, RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SHARED, false, "_rt_GlowBuffer1");
	m_GlowBuff2.InitRenderTarget(ScreenWidth() / 2, ScreenHeight() / 2, RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_NONE, false, "_rt_GlowBuffer2");

	LoadGlowPresets();

	KeyValues* pKVBlurX = new KeyValues("BlurFilterX");
	pKVBlurX->SetString("$basetexture", "_rt_GlowBuffer1");
	pKVBlurX->SetInt("$translucent", 1);
	m_pBlurX = materials->CreateMaterial("Glow_Runtime_BlurX", pKVBlurX);

	KeyValues* pKVBlurY = new KeyValues("BlurFilterY");
	pKVBlurY->SetString("$basetexture", "_rt_GlowBuffer2");
	pKVBlurY->SetInt("$translucent", 1);
	m_pBlurY = materials->CreateMaterial("Glow_Runtime_BlurY", pKVBlurY);

	KeyValues* pKVCombine = new KeyValues("UnlitGeneric");
	pKVCombine->SetString("$basetexture", "_rt_GlowBuffer1");
	pKVCombine->SetInt("$additive", 1);
	pKVCombine->SetInt("$translucent", 1);
	pKVCombine->SetInt("$ignorez", 1);
	pKVCombine->SetInt("$vertexcolor", 1);
	pKVCombine->SetInt("$vertexalpha", 1);
	m_pEffectMaterial = materials->CreateMaterial("Glow_Runtime_Combine", pKVCombine);
}

void CEntGlowEffect::Shutdown(void)
{
	m_GlowBuff1.Shutdown();
	m_GlowBuff2.Shutdown();
}

void CEntGlowEffect::RenderGlowModels(void)
{
	if (!m_bInitialized)
	{
		Init();
		m_bInitialized = true;
	}

	VPROF("CEntGlowEffect::RenderGlowModels");

	C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pLocalPlayer)
		return;

	CMatRenderContextPtr pRenderContext(materials);
	ITexture* pOldRT = pRenderContext->GetRenderTarget(); // Save old render target

	int nWidth = ScreenWidth();
	int nHeight = ScreenHeight();

	// Pass 1: Stencil Mask Generation
	pRenderContext->SetRenderTarget(m_GlowBuff1);
	pRenderContext->Viewport(0, 0, nWidth / 2, nHeight / 2);
	pRenderContext->ClearColor4ub(0, 0, 0, 0);
	pRenderContext->ClearBuffers(true, true, true);

	pRenderContext->SetStencilEnable(true);
	pRenderContext->SetStencilWriteMask(0xFF);
	pRenderContext->SetStencilTestMask(0xFF);
	pRenderContext->SetStencilReferenceValue(1);
	pRenderContext->SetStencilCompareFunction(STENCILCOMPARISONFUNCTION_ALWAYS);
	pRenderContext->SetStencilPassOperation(STENCILOPERATION_REPLACE);
	pRenderContext->SetStencilFailOperation(STENCILOPERATION_KEEP);
	pRenderContext->SetStencilZFailOperation(STENCILOPERATION_KEEP);

	IMaterial* pFlatColorMat = materials->FindMaterial("debug/debugfullbright", TEXTURE_GROUP_OTHER);
	if (pFlatColorMat && !pFlatColorMat->IsErrorMaterial())
	{
		pRenderContext->Bind(pFlatColorMat);
		float flMaxDist = cl_glow_max_distance.GetFloat();

		for (int i = 0; i <= cl_entitylist->GetHighestEntityIndex(); i++)
		{
			C_BaseEntity* pEntity = cl_entitylist->GetBaseEntity(i);
			if (!pEntity || pEntity->IsDormant())
				continue;

			float flEntityColor[3];
			if (GetColorForEntity(pEntity, flEntityColor))
			{
				if (pLocalPlayer->GetAbsOrigin().DistTo(pEntity->GetAbsOrigin()) <= flMaxDist)
				{
					pEntity->SetupBones(NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime);
					render->SetColorModulation(flEntityColor);
					pEntity->DrawModel(0);
				}
			}
		}
	}

	pRenderContext->SetStencilEnable(false);

	// CRITICAL FIX: Restore original render target and viewport immediately 
	// so the engine can render viewmodels and the main scene normally.
	pRenderContext->SetRenderTarget(pOldRT);
	pRenderContext->Viewport(0, 0, nWidth, nHeight);

	float flResetColor[3] = { 1.0f, 1.0f, 1.0f };
	render->SetColorModulation(flResetColor);
}

void CEntGlowEffect::RenderGlowPostProcess(void)
{
	if (!m_bInitialized || !m_pBlurX || !m_pBlurY || !m_pEffectMaterial)
		return;

	VPROF("CEntGlowEffect::RenderGlowPostProcess");

	CMatRenderContextPtr pRenderContext(materials);
	ITexture* pOldRT = pRenderContext->GetRenderTarget();

	int nWidth = ScreenWidth();
	int nHeight = ScreenHeight();

	// Pass 2: Horizontal Blur
	pRenderContext->SetRenderTarget(m_GlowBuff2);
	pRenderContext->Viewport(0, 0, nWidth / 2, nHeight / 2);
	pRenderContext->Bind(m_pBlurX);
	pRenderContext->DrawScreenSpaceRectangle(
		m_pBlurX, 0, 0, nWidth / 2, nHeight / 2,
		0, 0, m_GlowBuff1->GetActualWidth() - 1, m_GlowBuff1->GetActualHeight() - 1,
		m_GlowBuff1->GetActualWidth(), m_GlowBuff1->GetActualHeight()
	);

	// Pass 3: Vertical Blur
	pRenderContext->SetRenderTarget(m_GlowBuff1);
	pRenderContext->Bind(m_pBlurY);
	pRenderContext->DrawScreenSpaceRectangle(
		m_pBlurY, 0, 0, nWidth / 2, nHeight / 2,
		0, 0, m_GlowBuff2->GetActualWidth() - 1, m_GlowBuff2->GetActualHeight() - 1,
		m_GlowBuff2->GetActualWidth(), m_GlowBuff2->GetActualHeight()
	);

	// Pass 4: Combine Pass
	pRenderContext->SetRenderTarget(pOldRT);
	pRenderContext->Viewport(0, 0, nWidth, nHeight);

	pRenderContext->Bind(m_pEffectMaterial);
	pRenderContext->DrawScreenSpaceRectangle(
		m_pEffectMaterial, 0, 0, nWidth, nHeight,
		0, 0, m_GlowBuff1->GetActualWidth() - 1, m_GlowBuff1->GetActualHeight() - 1,
		m_GlowBuff1->GetActualWidth(), m_GlowBuff1->GetActualHeight()
	);
}

void CEntGlowEffect::DrawGlowEffects(void)
{
	RenderGlowModels();
	RenderGlowPostProcess();
}

CEntGlowEffect& GetGlowEffect()
{
	return CEntGlowEffect::GetInstance();
}