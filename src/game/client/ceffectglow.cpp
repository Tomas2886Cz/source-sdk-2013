#include "cbase.h"
#include "ceffectglow.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier0/vprof.h"
#include "ivrenderview.h"
#include "materialsystem/itexture.h"
#include "igamesystem.h"

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

	virtual void Update(float frametime)
	{
		// Runs every frame automatically
	}

	virtual void PreRender()
	{
		GetGlowEffect().DrawGlowEffects();
	}
};

static CEntGlowSystem s_EntGlowSystem;

ConVar cl_glow_max_distance("cl_glow_max_distance", "400.0", FCVAR_ARCHIVE, "Maximum distance items will display an outline glow.");

ConVar cl_glow_color_orange("cl_glow_color_orange", "255 128 0", FCVAR_ARCHIVE, "Glow (Orange)");
ConVar cl_glow_color_blue("cl_glow_color_blue", "0 128 255", FCVAR_ARCHIVE, "Glow (Blue)");
ConVar cl_glow_color_white("cl_glow_color_white", "200 200 200", FCVAR_ARCHIVE, "Glow (White)");
ConVar cl_glow_color_red("cl_glow_color_red", "255 50 50", FCVAR_ARCHIVE, "Glow (Red)");
ConVar cl_glow_color_yellow("cl_glow_color_yellow", "255 255 0", FCVAR_ARCHIVE, "Glow (Yellow)");
ConVar cl_glow_color_green("cl_glow_color_green", "50 255 50", FCVAR_ARCHIVE, "Glow (Green)");
ConVar cl_glow_color_cyan("cl_glow_color_cyan", "0 255 255", FCVAR_ARCHIVE, "Glow (Cyan)");
ConVar cl_glow_color_purple("cl_glow_color_rpg", "200 50 255", FCVAR_ARCHIVE, "Glow (Purple)");
ConVar cl_glow_color_teal("cl_glow_color_teal", "0 255 150", FCVAR_ARCHIVE, "Glow (Teal)");
ConVar cl_glow_color_gold("cl_glow_color_gold", "255 215 0", FCVAR_ARCHIVE, "Glow (Gold)");

struct GlowPreset_t
{
	const char* szClassname;
	ConVar* pColorConVar;
};

static GlowPreset_t g_GlowPresets[] = {
	{ "weapon_flaregun", &cl_glow_color_orange },
	{ "weapon_blueprint_medkit", &cl_glow_color_blue},
	{ "weapon_pistol", &cl_glow_color_white },
	{ "weapon_shotgun", &cl_glow_color_red },
	{ "weapon_smg1", &cl_glow_color_yellow },
	{ "weapon_crowbar", &cl_glow_color_green },
	{ "weapon_stunstick", &cl_glow_color_cyan },
	{ "weapon_rpg", &cl_glow_color_purple },
	{ "item_ammo_crate", &cl_glow_color_teal },
	{ "item_battery", &cl_glow_color_gold },
};

bool GetColorForEntity(C_BaseEntity* pEntity, float flColorOut[3])
{
	if (!pEntity)
		return false;

	int numPresets = sizeof(g_GlowPresets) / sizeof(g_GlowPresets[0]);
	for (int i = 0; i < numPresets; i++)
	{
		if (FClassnameIs(pEntity, g_GlowPresets[i].szClassname))
		{
			int r = 255, g = 255, b = 255;
			sscanf(g_GlowPresets[i].pColorConVar->GetString(), "%d %d %d", &r, &g, &b);
			flColorOut[0] = r / 255.0f;
			flColorOut[1] = g / 255.0f;
			flColorOut[2] = b / 255.0f;
			return true;
		}
	}
	return false;
}



void CEntGlowEffect::Init(void)
{
	m_GlowBuff1.InitRenderTarget(ScreenWidth() / 2, ScreenHeight() / 2, RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_NONE, false, "_rt_GlowBuffer1");
	m_GlowBuff2.InitRenderTarget(ScreenWidth() / 2, ScreenHeight() / 2, RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_NONE, false, "_rt_GlowBuffer2");
}

void CEntGlowEffect::Shutdown(void)
{
	m_GlowBuff1.Shutdown();
	m_GlowBuff2.Shutdown();
}

void CEntGlowEffect::DrawGlowEffects(void)
{
	if (!m_bVisible)
		return;

	VPROF("CEntGlowEffect::DrawGlowEffects");

	C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pLocalPlayer)
		return;

	CMatRenderContextPtr pRenderContext(materials);
	ITexture* pOldRT = pRenderContext->GetRenderTarget();

	int nWidth = ScreenWidth();
	int nHeight = ScreenHeight();

	// -------------------------------------------------------------------------
	// Pass 1: Render glowing items into the Stencil Buffer / Buffer 1
	// -------------------------------------------------------------------------
	pRenderContext->SetRenderTarget(m_GlowBuff1);
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
	if (pFlatColorMat)
	{
		pRenderContext->Bind(pFlatColorMat);
		float flMaxDist = cl_glow_max_distance.GetFloat();

		for (int i = 0; i <= cl_entitylist->GetHighestEntityIndex(); i++)
		{
			C_BaseEntity* pEntity = cl_entitylist->GetBaseEntity(i);
			if (!pEntity)
				continue;

			float flEntityColor[3];
			if (GetColorForEntity(pEntity, flEntityColor))
			{
				if (pLocalPlayer->GetAbsOrigin().DistTo(pEntity->GetAbsOrigin()) <= flMaxDist)
				{
					render->SetColorModulation(flEntityColor);
					pEntity->DrawModel(0);
				}
			}
		}
	}

	pRenderContext->SetStencilEnable(false);

	// -------------------------------------------------------------------------
	// Pass 2: Horizontal Gaussian Blur (from GlowBuff1 -> GlowBuff2)
	// -------------------------------------------------------------------------
	pRenderContext->SetRenderTarget(m_GlowBuff2);
	pRenderContext->Viewport(0, 0, nWidth / 2, nHeight / 2);
	pRenderContext->Bind(m_pBlurX);
	pRenderContext->DrawScreenSpaceRectangle(
		m_pBlurX, 0, 0, nWidth / 2, nHeight / 2,
		0, 0, m_GlowBuff1->GetActualWidth() - 1, m_GlowBuff1->GetActualHeight() - 1,
		m_GlowBuff1->GetActualWidth(), m_GlowBuff1->GetActualHeight()
	);

	// -------------------------------------------------------------------------
	// Pass 3: Vertical Gaussian Blur (from GlowBuff2 -> GlowBuff1)
	// -------------------------------------------------------------------------
	pRenderContext->SetRenderTarget(m_GlowBuff1);
	pRenderContext->Bind(m_pBlurY);
	pRenderContext->DrawScreenSpaceRectangle(
		m_pBlurY, 0, 0, nWidth / 2, nHeight / 2,
		0, 0, m_GlowBuff2->GetActualWidth() - 1, m_GlowBuff2->GetActualHeight() - 1,
		m_GlowBuff2->GetActualWidth(), m_GlowBuff2->GetActualHeight()
	);

	// -------------------------------------------------------------------------
	// Pass 4: Combine Pass (Blend blurred outline back over the main screen)
	// -------------------------------------------------------------------------
	pRenderContext->SetRenderTarget(pOldRT);
	pRenderContext->Viewport(0, 0, nWidth, nHeight);

	if (m_pEffectMaterial)
	{
		pRenderContext->Bind(m_pEffectMaterial);
		pRenderContext->DrawScreenSpaceRectangle(
			m_pEffectMaterial, 0, 0, nWidth, nHeight,
			0, 0, m_GlowBuff1->GetActualWidth() - 1, m_GlowBuff1->GetActualHeight() - 1,
			m_GlowBuff1->GetActualWidth(), m_GlowBuff1->GetActualHeight()
		);
	}

	// Reset global color modulation back to white
	float flResetColor[3] = { 1.0f, 1.0f, 1.0f };
	render->SetColorModulation(flResetColor);
}

CEntGlowEffect& GetGlowEffect()
{
	return CEntGlowEffect::GetInstance();
}