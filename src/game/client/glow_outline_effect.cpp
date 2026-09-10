//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Functionality to render a glowing outline around client renderable objects.
//
//===============================================================================

#ifndef GLOWS_ENABLE
#define GLOWS_ENABLE 1
#endif

#include "cbase.h"
#include "glow_outline_effect.h"
#include "model_types.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "view_shared.h"
#include "viewpostprocess.h"

#define FULL_FRAME_TEXTURE "_rt_FullFrameFB"

#ifdef GLOWS_ENABLE

ConVar glow_outline_effect_enable("glow_outline_effect_enable", "1", FCVAR_ARCHIVE, "Enable entity outline glow effects.");
ConVar glow_outline_effect_width("glow_outline_width", "2.0f", FCVAR_CHEAT, "Pixel width of the glow outline."); // Changed default to 2.0 pixels

extern bool g_bDumpRenderTargets;

CGlowObjectManager g_GlowObjectManager;

static CMaterialReference g_MatGlowColor;
static CMaterialReference g_MatHaloAddToScreen;

struct ShaderStencilState_t
{
	bool m_bEnable;
	StencilOperation_t m_FailOp;
	StencilOperation_t m_ZFailOp;
	StencilOperation_t m_PassOp;
	StencilComparisonFunction_t m_CompareFunc;
	int m_nReferenceValue;
	uint32 m_nTestMask;
	uint32 m_nWriteMask;

	ShaderStencilState_t()
	{
		m_bEnable = false;
		m_PassOp = m_FailOp = m_ZFailOp = STENCILOPERATION_KEEP;
		m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
		m_nReferenceValue = 0;
		m_nTestMask = m_nWriteMask = 0xFFFFFFFF;
	}

	void SetStencilState(CMatRenderContextPtr& pRenderContext)
	{
		pRenderContext->SetStencilEnable(m_bEnable);
		pRenderContext->SetStencilFailOperation(m_FailOp);
		pRenderContext->SetStencilZFailOperation(m_ZFailOp);
		pRenderContext->SetStencilPassOperation(m_PassOp);
		pRenderContext->SetStencilCompareFunction(m_CompareFunc);
		pRenderContext->SetStencilReferenceValue(m_nReferenceValue);
		pRenderContext->SetStencilTestMask(m_nTestMask);
		pRenderContext->SetStencilWriteMask(m_nWriteMask);
	}
};

void CGlowObjectManager::RenderGlowEffects(const CViewSetup* pSetup, int nSplitScreenSlot)
{
	if (g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_0())
	{
		if (glow_outline_effect_enable.GetBool())
		{
			CMatRenderContextPtr pRenderContext(materials);

			int nX, nY, nWidth, nHeight;
			pRenderContext->GetViewport(nX, nY, nWidth, nHeight);

			PIXEvent _pixEvent(pRenderContext, "EntityGlowEffects");
			ApplyEntityGlowEffects(pSetup, nSplitScreenSlot, pRenderContext, glow_outline_effect_width.GetFloat(), nX, nY, nWidth, nHeight);
		}
	}
}

static void SetRenderTargetAndViewPort(ITexture* rt, int w, int h)
{
	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->SetRenderTarget(rt);
	pRenderContext->Viewport(0, 0, w, h);
}

void CGlowObjectManager::RenderGlowModels(const CViewSetup* pSetup, int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext)
{
	pRenderContext->PushRenderTargetAndViewport();

	Vector vOrigColor;
	render->GetColorModulation(vOrigColor.Base());
	float flOrigBlend = render->GetBlend();

	ITexture* pRtFullFrame = materials->FindTexture(FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET);
	SetRenderTargetAndViewPort(pRtFullFrame, pSetup->width, pSetup->height);

	pRenderContext->ClearColor3ub(0, 0, 0);
	pRenderContext->ClearBuffers(true, false, false);

	if (!g_MatGlowColor) g_MatGlowColor.Init("dev/glow_color", TEXTURE_GROUP_OTHER);
	g_pStudioRender->ForcedMaterialOverride(g_MatGlowColor);

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = false;
	stencilState.m_nReferenceValue = 0;
	stencilState.m_nTestMask = 0xFF;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_KEEP;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
	stencilState.SetStencilState(pRenderContext);

	for (int i = 0; i < m_GlowObjectDefinitions.Count(); ++i)
	{
		if (m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw(nSplitScreenSlot))
			continue;

		render->SetBlend(m_GlowObjectDefinitions[i].m_flGlowAlpha);
		Vector vGlowColor = m_GlowObjectDefinitions[i].m_vGlowColor * m_GlowObjectDefinitions[i].m_flGlowAlpha;
		render->SetColorModulation(&vGlowColor[0]);

		m_GlowObjectDefinitions[i].DrawModel();
	}

	if (g_bDumpRenderTargets) DumpTGAofRenderTarget(pSetup->width, pSetup->height, "GlowModels");

	g_pStudioRender->ForcedMaterialOverride(NULL);
	render->SetColorModulation(vOrigColor.Base());
	render->SetBlend(flOrigBlend);

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	stencilStateDisable.SetStencilState(pRenderContext);

	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::ApplyEntityGlowEffects(const CViewSetup* pSetup, int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext, float flBloomScale, int x, int y, int w, int h)
{
	if (!g_MatGlowColor) g_MatGlowColor.Init("dev/glow_color", TEXTURE_GROUP_OTHER);
	if (!g_MatHaloAddToScreen) g_MatHaloAddToScreen.Init("dev/halo_add_to_screen", TEXTURE_GROUP_OTHER);

	g_pStudioRender->ForcedMaterialOverride(g_MatGlowColor);

	// =========================================================================
	// 1. STENCIL PASS - Mask out the entity's exact shape
	// =========================================================================
	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;

	// CRITICAL FIX: Stop the stencil pass from actually drawing white pixels to the screen!
	pRenderContext->OverrideColorWriteEnable(true, false);
	pRenderContext->OverrideDepthEnable(true, false);

	int iNumGlowObjects = 0;

	for (int i = 0; i < m_GlowObjectDefinitions.Count(); ++i)
	{
		if (m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw(nSplitScreenSlot))
			continue;

		ShaderStencilState_t stencilState;
		stencilState.m_bEnable = true;
		stencilState.m_nReferenceValue = 1;
		stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
		stencilState.m_PassOp = STENCILOPERATION_REPLACE;
		stencilState.m_FailOp = STENCILOPERATION_KEEP;
		stencilState.m_ZFailOp = STENCILOPERATION_REPLACE; // Draw even if behind walls
		stencilState.SetStencilState(pRenderContext);

		m_GlowObjectDefinitions[i].DrawModel();
		iNumGlowObjects++;
	}

	pRenderContext->OverrideColorWriteEnable(false, true); // Restore color writing
	pRenderContext->OverrideDepthEnable(false, false);
	stencilStateDisable.SetStencilState(pRenderContext);
	g_pStudioRender->ForcedMaterialOverride(NULL);

	if (iNumGlowObjects <= 0)
		return;

	// =========================================================================
	// 2. COLOR PASS - Render the solid colored models to a separate texture
	// =========================================================================
	{
		PIXEvent pixEvent(pRenderContext, "RenderGlowModels");
		RenderGlowModels(pSetup, nSplitScreenSlot, pRenderContext);
	}

	// =========================================================================
	// 3. COMPOSITE PASS - Draw the texture shifted to create the hollow outline
	// =========================================================================
	int nSrcWidth = pSetup->width;
	int nSrcHeight = pSetup->height;
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport(nViewportX, nViewportY, nViewportWidth, nViewportHeight);

	ITexture* pRtFullFrame = materials->FindTexture(FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET);

	// Activate the stencil mask so the outline only draws OUTSIDE the models
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nWriteMask = 0x0;
	stencilState.m_nTestMask = 0xFF;
	stencilState.m_nReferenceValue = 0x0;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_EQUAL;
	stencilState.m_PassOp = STENCILOPERATION_KEEP;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
	stencilState.SetStencilState(pRenderContext);

	// Draw 4 offset passes. This acts as an absolutely bulletproof "blur" replacement.
	float flOffset = glow_outline_effect_width.GetFloat(); // E.g., 2.0 pixels

	// LEFT
	pRenderContext->DrawScreenSpaceRectangle(g_MatHaloAddToScreen, 0 - flOffset, 0, nViewportWidth, nViewportHeight,
		0, 0, nSrcWidth - 1, nSrcHeight - 1, pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight());
	// RIGHT
	pRenderContext->DrawScreenSpaceRectangle(g_MatHaloAddToScreen, 0 + flOffset, 0, nViewportWidth, nViewportHeight,
		0, 0, nSrcWidth - 1, nSrcHeight - 1, pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight());
	// UP
	pRenderContext->DrawScreenSpaceRectangle(g_MatHaloAddToScreen, 0, 0 - flOffset, nViewportWidth, nViewportHeight,
		0, 0, nSrcWidth - 1, nSrcHeight - 1, pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight());
	// DOWN
	pRenderContext->DrawScreenSpaceRectangle(g_MatHaloAddToScreen, 0, 0 + flOffset, nViewportWidth, nViewportHeight,
		0, 0, nSrcWidth - 1, nSrcHeight - 1, pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight());

	stencilStateDisable.SetStencilState(pRenderContext);
}

void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	if (m_hEntity.Get())
	{
		m_hEntity->DrawModel(STUDIO_RENDER);
		C_BaseEntity* pAttachment = m_hEntity->FirstMoveChild();

		while (pAttachment != NULL)
		{
			if (!g_GlowObjectManager.HasGlowEffect(pAttachment) && pAttachment->ShouldDraw())
			{
				pAttachment->DrawModel(STUDIO_RENDER);
			}
			pAttachment = pAttachment->NextMovePeer();
		}
	}
}
#endif // GLOWS_ENABLE