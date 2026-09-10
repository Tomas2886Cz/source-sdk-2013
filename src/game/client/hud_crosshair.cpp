//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_crosshair.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "ivrenderview.h"
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "client_virtualreality.h"
#include "sourcevr/isourcevirtualreality.h"
#include "in_buttons.h"

#ifdef SIXENSE
#include "sixense/in_sixense.h"
#endif

#ifdef PORTAL
#include "c_portal_player.h"
#endif // PORTAL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar crosshair("crosshair", "1", FCVAR_ARCHIVE);
ConVar cl_observercrosshair("cl_observercrosshair", "1", FCVAR_ARCHIVE);
ConVar cl_crosshair_dynamic_sway("cl_crosshair_dynamic_sway", "1", FCVAR_ARCHIVE, "Enables or disables dramatic viewmodel sway tracking on the crosshair.");
ConVar cl_crosshair_sway_weight("cl_crosshair_sway_weight", "1.0", FCVAR_ARCHIVE, "Controls the intensity of the viewmodel sway offset matching.");
ConVar cl_crosshair_sprint_fade("cl_crosshair_sprint_fade", "1", FCVAR_ARCHIVE, "Fades out the crosshair when sprinting.");
ConVar cl_crosshair_fade_out_speed("cl_crosshair_fade_out_speed", "8.0", FCVAR_ARCHIVE, "Speed at which the crosshair fades out when sprinting.");
ConVar cl_crosshair_fade_in_delay("cl_crosshair_fade_in_delay", "0.2", FCVAR_ARCHIVE, "Delay in seconds before the crosshair starts fading back in after stopping sprint.");
ConVar cl_crosshair_fade_in_speed("cl_crosshair_fade_in_speed", "0.2", FCVAR_ARCHIVE, "Speed at which the crosshair fades back in when stopping.");

using namespace vgui;

int ScreenTransform(const Vector& point, Vector& screen);

#ifdef TF_CLIENT_DLL
// If running TF, we use CHudTFCrosshair instead (which is derived from CHudCrosshair)
#else
DECLARE_HUDELEMENT(CHudCrosshair);
#endif

CHudCrosshair::CHudCrosshair(const char* pElementName) :
	CHudElement(pElementName), BaseClass(NULL, "HudCrosshair")
{
	vgui::Panel* pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	m_pCrosshair = 0;

	m_clrCrosshair = Color(0, 0, 0, 0);

	m_vecCrossHairOffsetAngle.Init();

	SetHiddenBits(HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR);
}

CHudCrosshair::~CHudCrosshair()
{
}

void CHudCrosshair::ApplySchemeSettings(IScheme* scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	m_pDefaultCrosshair = gHUD.GetIcon("crosshair_default");
	SetPaintBackgroundEnabled(false);

	SetSize(ScreenWidth(), ScreenHeight());

	SetForceStereoRenderToFrameBuffer(true);
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudCrosshair::ShouldDraw(void)
{
	bool bNeedsDraw;

	if (m_bHideCrosshair)
		return false;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return false;

	C_BaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
	if (pWeapon && !pWeapon->ShouldDrawCrosshair())
		return false;

#ifdef PORTAL
	C_Portal_Player* portalPlayer = ToPortalPlayer(pPlayer);
	if (portalPlayer && portalPlayer->IsSuppressingCrosshair())
		return false;
#endif // PORTAL

	/* disabled to avoid assuming it's an HL2 player.
	// suppress crosshair in zoom.
	if ( pPlayer->m_HL2Local.m_bZooming )
		return false;
	*/

	// draw a crosshair only if alive or spectating in eye
	if (IsX360())
	{
		bNeedsDraw = m_pCrosshair &&
			!engine->IsDrawingLoadingImage() &&
			!engine->IsPaused() &&
			(!pPlayer->IsSuitEquipped() || g_pGameRules->IsMultiplayer()) &&
			g_pClientMode->ShouldDrawCrosshair() &&
			!(pPlayer->GetFlags() & FL_FROZEN) &&
			(pPlayer->entindex() == render->GetViewEntity()) &&
			(pPlayer->IsAlive() || (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE) || (cl_observercrosshair.GetBool() && pPlayer->GetObserverMode() == OBS_MODE_ROAMING));
	}
	else
	{
		bNeedsDraw = m_pCrosshair &&
			crosshair.GetInt() &&
			!engine->IsDrawingLoadingImage() &&
			!engine->IsPaused() &&
			g_pClientMode->ShouldDrawCrosshair() &&
			!(pPlayer->GetFlags() & FL_FROZEN) &&
			(pPlayer->entindex() == render->GetViewEntity()) &&
			!pPlayer->IsInVGuiInputMode() &&
			(pPlayer->IsAlive() || (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE) || (cl_observercrosshair.GetBool() && pPlayer->GetObserverMode() == OBS_MODE_ROAMING));
	}

	return (bNeedsDraw && CHudElement::ShouldDraw());
}

#ifdef TF_CLIENT_DLL
extern ConVar cl_crosshair_red;
extern ConVar cl_crosshair_green;
extern ConVar cl_crosshair_blue;
extern ConVar cl_crosshair_scale;
#endif


void CHudCrosshair::GetDrawPosition(float* pX, float* pY, bool* pbBehindCamera, QAngle angleCrosshairOffset)
{
	QAngle curViewAngles = CurrentViewAngles();
	Vector curViewOrigin = CurrentViewOrigin();

	int vx, vy, vw, vh;
	vgui::surface()->GetFullscreenViewport(vx, vy, vw, vh);

	float screenWidth = vw;
	float screenHeight = vh;

	float x = screenWidth / 2;
	float y = screenHeight / 2;

	bool bBehindCamera = false;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if ((pPlayer != NULL) && (pPlayer->GetObserverMode() == OBS_MODE_NONE))
	{
		bool bUseOffset = false;

		Vector vecStart;
		Vector vecEnd;

		if (UseVR())
		{
			// These are the correct values to use, but they lag the high-speed view data...
			vecStart = pPlayer->Weapon_ShootPosition();
			Vector vecAimDirection = pPlayer->GetAutoaimVector(1.0f);
			// ...so in some aim modes, they get zapped by something completely up-to-date.
			g_ClientVirtualReality.OverrideWeaponHudAimVectors(&vecStart, &vecAimDirection);
			vecEnd = vecStart + vecAimDirection * MAX_TRACE_LENGTH;

			bUseOffset = true;
		}

#ifdef SIXENSE
		// TODO: actually test this Sixsense code interaction with things like HMDs & stereo.
		if (g_pSixenseInput->IsEnabled() && !UseVR())
		{
			// Never autoaim a predicted weapon (for now)
			vecStart = pPlayer->Weapon_ShootPosition();
			Vector aimVector;
			AngleVectors(CurrentViewAngles() - g_pSixenseInput->GetViewAngleOffset(), &aimVector);
			// calculate where the bullet would go so we can draw the cross appropriately
			vecEnd = vecStart + aimVector * MAX_TRACE_LENGTH;
			bUseOffset = true;
		}
#endif

		if (bUseOffset)
		{
			trace_t tr;
			UTIL_TraceLine(vecStart, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr);

			Vector screen;
			screen.Init();
			bBehindCamera = ScreenTransform(tr.endpos, screen) != 0;

			x = 0.5f * (1.0f + screen[0]) * screenWidth + 0.5f;
			y = 0.5f * (1.0f - screen[1]) * screenHeight + 0.5f;
		}
	}

	// MattB - angleCrosshairOffset is the autoaim angle.
	// if we're not using autoaim, just draw in the middle of the 
	// screen
	if (angleCrosshairOffset != vec3_angle)
	{
		QAngle angles;
		Vector forward;
		Vector point, screen;

		// this code is wrong
		angles = curViewAngles + angleCrosshairOffset;
		AngleVectors(angles, &forward);
		VectorAdd(curViewOrigin, forward, point);
		ScreenTransform(point, screen);

		x += 0.5f * screen[0] * screenWidth + 0.5f;
		y += 0.5f * screen[1] * screenHeight + 0.5f;
	}

	// High-pass baseline filter combined with full viewmodel movement/sway matching.
	if (cl_crosshair_dynamic_sway.GetBool() && pPlayer != NULL && pPlayer->GetObserverMode() == OBS_MODE_NONE)
	{
		C_BaseViewModel* pVM = pPlayer->GetViewModel();
		if (pVM)
		{
			int iAttachment = pVM->LookupAttachment("muzzle");
			Vector vOrigin;
			QAngle angAngles;
			if (pVM->GetAttachment(iAttachment, vOrigin, angAngles))
			{
				Vector screen;
				screen.Init();
				if (ScreenTransform(vOrigin, screen) == 0)
				{
					float flCenterX = screenWidth / 2;
					float flCenterY = screenHeight / 2;
					float flAttachScreenX = 0.5f * (1.0f + screen[0]) * screenWidth + 0.5f;
					float flAttachScreenY = 0.5f * (1.0f - screen[1]) * screenHeight + 0.5f;

					static float s_flBaseX = -1.0f;
					static float s_flBaseY = -1.0f;

					if (s_flBaseX < 0.0f)
					{
						s_flBaseX = flAttachScreenX;
						s_flBaseY = flAttachScreenY;
					}
					else
					{
						s_flBaseX += (flAttachScreenX - s_flBaseX) * 0.015f;
						s_flBaseY += (flAttachScreenY - s_flBaseY) * 0.015f;
					}

					float flOffsetX = (flAttachScreenX - s_flBaseX);
					float flOffsetY = (flAttachScreenY - s_flBaseY);

					float flWeight = cl_crosshair_sway_weight.GetFloat();
					x = flCenterX + (flOffsetX * flWeight);
					y = flCenterY + (flOffsetY * flWeight);
				}
			}
		}
	}

	*pX = x;
	*pY = y;
	*pbBehindCamera = bBehindCamera;
}


void CHudCrosshair::Paint(void)
{
	if (!m_pCrosshair)
		return;

	if (!IsCurrentViewAccessAllowed())
		return;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	float x, y;
	bool bBehindCamera;
	GetDrawPosition(&x, &y, &bBehindCamera, m_vecCrossHairOffsetAngle);

	if (bBehindCamera)
		return;

	// Handle smooth sprinting fade calculation with fade-in delay and separate speeds
	static float s_flCrosshairAlpha = 1.0f;
	static float s_flStopSprintTime = 0.0f;
	static bool s_bWasSprinting = false;

	bool bIsSprinting = false;
	if (cl_crosshair_sprint_fade.GetBool())
	{
		if ((pPlayer->m_nButtons & IN_SPEED) && pPlayer->GetAbsVelocity().Length2D() > 50.0f)
		{
			bIsSprinting = true;
		}
	}

	if (bIsSprinting)
	{
		s_bWasSprinting = true;
	}
	else
	{
		if (s_bWasSprinting)
		{
			s_flStopSprintTime = gpGlobals->curtime;
			s_bWasSprinting = false;
		}
	}

	float flTargetAlpha = 1.0f;
	float flCurrentFadeRate = cl_crosshair_fade_in_speed.GetFloat();

	if (bIsSprinting)
	{
		flTargetAlpha = 0.0f;
		flCurrentFadeRate = cl_crosshair_fade_out_speed.GetFloat();
	}
	else if (gpGlobals->curtime < s_flStopSprintTime + cl_crosshair_fade_in_delay.GetFloat())
	{
		// Hold alpha at 0 during the configured fade-in delay period
		flTargetAlpha = 0.0f;
	}

	float flFadeStep = flCurrentFadeRate * gpGlobals->frametime;
	s_flCrosshairAlpha = Approach(flTargetAlpha, s_flCrosshairAlpha, flFadeStep);

	if (s_flCrosshairAlpha <= 0.001f)
		return;

	float flWeaponScale = 1.f;
	int iTextureW = m_pCrosshair->Width();
	int iTextureH = m_pCrosshair->Height();
	C_BaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
	if (pWeapon)
	{
		pWeapon->GetWeaponCrosshairScale(flWeaponScale);
	}

	float flPlayerScale = 1.0f;
#ifdef TF_CLIENT_DLL
	Color clr(cl_crosshair_red.GetInt(), cl_crosshair_green.GetInt(), cl_crosshair_blue.GetInt(), 255);
	flPlayerScale = cl_crosshair_scale.GetFloat() / 32.0f;  // the player can change the scale in the options/multiplayer tab
#else
	Color clr = m_clrCrosshair;
#endif

	// Apply alpha fade scaling
	clr[3] = (int)((float)clr[3] * s_flCrosshairAlpha);

	float flWidth = flWeaponScale * flPlayerScale * (float)iTextureW;
	float flHeight = flWeaponScale * flPlayerScale * (float)iTextureH;
	int iWidth = (int)(flWidth + 0.5f);
	int iHeight = (int)(flHeight + 0.5f);
	int iX = (int)(x + 0.5f);
	int iY = (int)(y + 0.5f);

	m_pCrosshair->DrawSelfCropped(
		iX - (iWidth / 2), iY - (iHeight / 2),
		0, 0,
		iTextureW, iTextureH,
		iWidth, iHeight,
		clr);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudCrosshair::SetCrosshairAngle(const QAngle& angle)
{
	VectorCopy(angle, m_vecCrossHairOffsetAngle);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudCrosshair::SetCrosshair(CHudTexture* texture, const Color& clr)
{
	m_pCrosshair = texture;
	m_clrCrosshair = clr;
}

//-----------------------------------------------------------------------------
// Purpose: Resets the crosshair back to the default
//-----------------------------------------------------------------------------
void CHudCrosshair::ResetCrosshair()
{
	SetCrosshair(m_pDefaultCrosshair, Color(255, 255, 255, 255));
}