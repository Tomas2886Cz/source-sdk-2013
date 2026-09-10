#include "cbase.h"
#include "hud.h"
#include "iclientmode.h"
#include "ammodef.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>
#include "c_hl2mp_player.h"

// memdbgon must be the last include file in a .cpp file
#include "tier0/memdbgon.h"

// ============================================================================
// CONVARS CONFIGURATION
// ============================================================================
ConVar hud_scrap_enable("hud_scrap_enable", "1", FCVAR_ARCHIVE, "Enable/Disable the Scrap Counter HUD element.");
ConVar hud_scrap_xpos("hud_scrap_xpos", "40", FCVAR_ARCHIVE, "X-offset from the right edge of the screen.");
ConVar hud_scrap_ypos("hud_scrap_ypos", "380", FCVAR_ARCHIVE, "Y-position from the top of the screen.");
ConVar hud_scrap_gap("hud_scrap_gap", "4", FCVAR_ARCHIVE, "Gap in pixels between text numbers and icons.");
ConVar hud_scrap_icon_size("hud_scrap_icon_size", "16", FCVAR_ARCHIVE, "Icon size (height/width) in scaled YRES units.");
ConVar hud_scrap_row_spacing("hud_scrap_row_spacing", "24", FCVAR_ARCHIVE, "Vertical spacing between scrap rows.");

ConVar hud_scrap_color_r("hud_scrap_color_r", "255", FCVAR_ARCHIVE, "Text Red color channel (0-255).");
ConVar hud_scrap_color_g("hud_scrap_color_g", "220", FCVAR_ARCHIVE, "Text Green color channel (0-255).");
ConVar hud_scrap_color_b("hud_scrap_color_b", "0", FCVAR_ARCHIVE, "Text Blue color channel (0-255).");
ConVar hud_scrap_color_a("hud_scrap_color_a", "255", FCVAR_ARCHIVE, "Text Alpha transparency channel (0-255).");

class CHudScrapCounter : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHudScrapCounter, vgui::Panel);

public:
	CHudScrapCounter(vgui::Panel* pParent);
	virtual void ApplySchemeSettings(vgui::IScheme* pScheme) override;
	virtual void Paint(void) override;

private:
	vgui::HFont m_hTextFont;

	int m_iMedicalIndex;
	int m_iUtilityIndex;
	int m_iWeaponIndex;

	int m_nMedicalIconID;
	int m_nUtilityIconID;
	int m_nWeaponIconID;
};

CHudScrapCounter::CHudScrapCounter(vgui::Panel* pParent) : BaseClass(pParent, "HudScrapCounter")
{
	SetVisible(true);
	SetEnabled(true);

	m_nMedicalIconID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_nMedicalIconID, "vgui/hud/icon_scrap_medical", true, false);

	m_nUtilityIconID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_nUtilityIconID, "vgui/hud/icon_scrap_utility", true, false);

	m_nWeaponIconID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_nWeaponIconID, "vgui/hud/icon_scrap_weapon", true, false);

	m_iMedicalIndex = GetAmmoDef()->Index("Scrap_Medical");
	m_iUtilityIndex = GetAmmoDef()->Index("Scrap_Utility");
	m_iWeaponIndex = GetAmmoDef()->Index("Scrap_Weapon");
}

void CHudScrapCounter::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	m_hTextFont = pScheme->GetFont("HudNumbers");
}

void CHudScrapCounter::Paint(void)
{
	if (!hud_scrap_enable.GetBool())
		return;

	C_HL2MP_Player* pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
	if (!pPlayer || !pPlayer->IsAlive())
		return;

	// Dynamically calculate panel positioning using ConVar screen offsets
	int sw, sh;
	vgui::surface()->GetScreenSize(sw, sh);

	int panelWidth = XRES(100);
	int panelHeight = YRES(100);
	int panelX = sw - panelWidth - XRES(hud_scrap_xpos.GetInt());
	int panelY = YRES(hud_scrap_ypos.GetInt());

	SetBounds(panelX, panelY, panelWidth, panelHeight);

	int medAmmo = pPlayer->GetAmmoCount(m_iMedicalIndex);
	int utilAmmo = pPlayer->GetAmmoCount(m_iUtilityIndex);
	int weaponAmmo = pPlayer->GetAmmoCount(m_iWeaponIndex);

	Color textColor(
		hud_scrap_color_r.GetInt(),
		hud_scrap_color_g.GetInt(),
		hud_scrap_color_b.GetInt(),
		hud_scrap_color_a.GetInt()
	);

	vgui::surface()->DrawSetTextFont(m_hTextFont);
	vgui::surface()->DrawSetTextColor(textColor);

	int iconSize = YRES(hud_scrap_icon_size.GetInt());
	int fontTall = vgui::surface()->GetFontTall(m_hTextFont);

	int iconX = XRES(54);
	int gap = XRES(hud_scrap_gap.GetInt());
	int rowSpacing = YRES(hud_scrap_row_spacing.GetInt());

	char szBuf[16];
	wchar_t wszBuf[16];

	auto ConvertToUnicode = [](const char* szSource, wchar_t* wszDest, int maxChars)
		{
			int i = 0;
			while (szSource[i] != '\0' && i < maxChars - 1)
			{
				wszDest[i] = (wchar_t)szSource[i];
				i++;
			}
			wszDest[i] = L'\0';
		};

	auto DrawScrapRow = [&](int yPos, int ammoCount, int iconID)
		{
			Q_snprintf(szBuf, sizeof(szBuf), "%d", ammoCount);
			ConvertToUnicode(szBuf, wszBuf, ARRAYSIZE(wszBuf));

			int textWidth = 0, textTall = 0;
			vgui::surface()->GetTextSize(m_hTextFont, wszBuf, textWidth, textTall);

			int textX = iconX - gap - textWidth;
			int textY = yPos + (iconSize - fontTall) / 2;

			vgui::surface()->DrawSetTextPos(textX, textY);
			vgui::surface()->DrawPrintText(wszBuf, Q_wcslen(wszBuf));

			vgui::surface()->DrawSetColor(255, 255, 255, 255);
			vgui::surface()->DrawSetTexture(iconID);
			vgui::surface()->DrawTexturedRect(iconX, yPos, iconX + iconSize, yPos + iconSize);
		};

	DrawScrapRow(0 * rowSpacing, medAmmo, m_nMedicalIconID);
	DrawScrapRow(1 * rowSpacing, utilAmmo, m_nUtilityIconID);
	DrawScrapRow(2 * rowSpacing, weaponAmmo, m_nWeaponIconID);
}

void UpdateScrapCounterHUD()
{
	static CHudScrapCounter* s_pScrapCounter = NULL;
	if (!s_pScrapCounter && g_pClientMode && g_pClientMode->GetViewport())
	{
		s_pScrapCounter = new CHudScrapCounter(g_pClientMode->GetViewport());
	}
}

class CScrapCounterAutoLoader : public CAutoGameSystem
{
public:
	CScrapCounterAutoLoader(void) : CAutoGameSystem("CScrapCounterAutoLoader") {}

	virtual void PostInit() override { UpdateScrapCounterHUD(); }
	virtual void LevelInitPostEntity() override { UpdateScrapCounterHUD(); }
};

static CScrapCounterAutoLoader s_ScrapCounterAutoLoader;