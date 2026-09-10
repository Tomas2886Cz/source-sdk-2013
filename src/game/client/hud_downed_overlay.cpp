#include "cbase.h"
#include "hud.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "c_hl2mp_player.h"

#include "tier0/memdbgon.h"

class CHudDownedOverlay : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHudDownedOverlay, vgui::Panel);

public:
	CHudDownedOverlay(vgui::Panel* pParent) : BaseClass(pParent, "HudDownedOverlay")
	{
		int sw, sh;
		vgui::surface()->GetScreenSize(sw, sh);
		SetBounds(0, 0, sw, sh);
		SetVisible(true);
		SetEnabled(true);

		m_nOverlayTextureId = -1;
	}

	virtual void Paint(void) override
	{
		C_HL2MP_Player* pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();

		ConVar* pEnableCVar = cvar->FindVar("sv_downed_enable");
		if (pEnableCVar && !pEnableCVar->GetBool())
			return;

		if (!pPlayer || !pPlayer->IsAlive())
			return;

		int sw, sh;
		vgui::surface()->GetScreenSize(sw, sh);

		// 1. Draw Downed Overlay and Bleedout Bar if local player is downed
		if (pPlayer->IsDowned())
		{
			if (m_nOverlayTextureId == -1)
			{
				m_nOverlayTextureId = vgui::surface()->CreateNewTextureID();
				vgui::surface()->DrawSetTextureFile(m_nOverlayTextureId, "materials/vgui/hud/downed_overlay", true, false);
			}

			if (vgui::surface()->IsTextureIDValid(m_nOverlayTextureId))
			{
				vgui::surface()->DrawSetColor(255, 255, 255, 255);
				vgui::surface()->DrawSetTexture(m_nOverlayTextureId);
				vgui::surface()->DrawTexturedRect(0, 0, sw, sh);
			}

			float flProgress = clamp(pPlayer->GetBleedoutTimer(), 0.0f, 1.0f);

			int barWidth = XRES(200);
			int barHeight = YRES(12);
			int barX = (sw - barWidth) / 2;
			int barY = sh - YRES(120);

			// Background for bleedout bar
			vgui::surface()->DrawSetColor(20, 20, 20, 220);
			vgui::surface()->DrawFilledRect(barX - 2, barY - 2, barX + barWidth + 2, barY + barHeight + 2);

			// Red fill for bleedout bar
			int fillWidth = (int)(barWidth * flProgress);
			vgui::surface()->DrawSetColor(180, 30, 30, 255);
			vgui::surface()->DrawFilledRect(barX, barY, barX + fillWidth, barY + barHeight);
		}

		// 2. Draw Revive Progress Bar on the reviver's HUD if they are currently reviving someone
		C_HL2MP_Player* pRevivingTarget = pPlayer->m_hRevivingTeammate.Get();
		if (pRevivingTarget && pRevivingTarget->IsDowned())
		{
			float flReviveProg = clamp(pRevivingTarget->GetReviveProgress(), 0.0f, 1.0f);

			int rBarWidth = XRES(200);
			int rBarHeight = YRES(12);
			int rBarX = (sw - rBarWidth) / 2;
			int rBarY = sh - YRES(160);

			// Background for revive bar
			vgui::surface()->DrawSetColor(20, 20, 20, 220);
			vgui::surface()->DrawFilledRect(rBarX - 2, rBarY - 2, rBarX + rBarWidth + 2, rBarY + rBarHeight + 2);

			// Green fill for revive bar
			int rFillWidth = (int)(rBarWidth * flReviveProg);
			vgui::surface()->DrawSetColor(30, 180, 30, 255);
			vgui::surface()->DrawFilledRect(rBarX, rBarY, rBarX + rFillWidth, rBarY + rBarHeight);
		}
	}

private:
	int m_nOverlayTextureId;
};

void InitDownedHUDOverlay()
{
	static CHudDownedOverlay* s_pDownedOverlay = NULL;
	if (!s_pDownedOverlay && g_pClientMode && g_pClientMode->GetViewport())
	{
		s_pDownedOverlay = new CHudDownedOverlay(g_pClientMode->GetViewport());
	}
}

class CDownedOverlayAutoLoader : public CAutoGameSystem
{
public:
	CDownedOverlayAutoLoader(void) : CAutoGameSystem("CDownedOverlayAutoLoader") {}
	virtual void PostInit() override { InitDownedHUDOverlay(); }
	virtual void LevelInitPostEntity() override { InitDownedHUDOverlay(); }
};

static CDownedOverlayAutoLoader s_DownedOverlayAutoLoader;