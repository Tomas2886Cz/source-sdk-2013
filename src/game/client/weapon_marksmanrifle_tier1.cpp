
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include <prediction.h>
#else
#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponMarksmanRifle_Tier1 C_WeaponMarksmanRifle_Tier1
#endif

//-----------------------------------------------------------------------------
// CWeaponMarksmanRifle_Tier1
//-----------------------------------------------------------------------------

class CWeaponMarksmanRifle_Tier1 : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeaponMarksmanRifle_Tier1, CBaseHL2MPCombatWeapon);
public:

	CWeaponMarksmanRifle_Tier1(void);

	bool Holster(CBaseCombatWeapon* pSwitchingTo = NULL);  // Required so that you know to un-zoom when switching weapons
	void ItemPostFrame(void);                              // Called every frame during normal weapon idle
	void ItemBusyFrame(void);                              // Called when the weapon is 'busy' e.g. reloading

	void	PrimaryAttack(void);
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:

	void ToggleZoom(void);                                 // If the weapon is zoomed, un-zoom and vice versa
	void CheckZoomToggle(void);                            // Check if the secondary attack button has been pressed

	bool m_bInZoom;                                        // Set to true when you are zooming, false when not

	CWeaponMarksmanRifle_Tier1(const CWeaponMarksmanRifle_Tier1&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponMarksmanRifle_Tier1, DT_WeaponMarksmanRifle_Tier1)

BEGIN_NETWORK_TABLE(CWeaponMarksmanRifle_Tier1, DT_WeaponMarksmanRifle_Tier1)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CWeaponMarksmanRifle_Tier1)
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_marksmanrifle_tier1, CWeaponMarksmanRifle_Tier1);
PRECACHE_WEAPON_REGISTER(weapon_marksmanrifle_tier1);


#ifndef CLIENT_DLL
acttable_t CWeaponMarksmanRifle_Tier1::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_PISTOL,				false },
};



IMPLEMENT_ACTTABLE(CWeaponMarksmanRifle_Tier1);

#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponMarksmanRifle_Tier1::CWeaponMarksmanRifle_Tier1(void)
{
	m_bReloadsSingly = false;
	m_bFiresUnderwater = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponMarksmanRifle_Tier1::PrimaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
	{
		return;
	}

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.3;
		}

		return;
	}

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3;

	m_iClip1--;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	FireBulletsInfo_t info(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
	info.m_pAttacker = pPlayer;

	// Fire the bullets, and force the first shot to be perfectly accuracy
	pPlayer->FireBullets(info);



#ifdef CLIENT_DLL
	//Disorient the player
	if (prediction->IsFirstTimePredicted())
	{
		QAngle angles;
		engine->GetViewAngles(angles);
		angles.x += random->RandomInt(-1, 1);
		angles.y += random->RandomInt(-1, 1);
		angles.z += 0.0f;
		engine->SetViewAngles(angles);
	}
#endif // CLIENT_DLL

	pPlayer->ViewPunch(QAngle(-2, random->RandomFloat(-2, 2), 0));

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}
}
/**
 * Check for weapon being holstered so we can disable scope zoom
 */
bool CWeaponMarksmanRifle_Tier1::Holster(CBaseCombatWeapon* pSwitchingTo /* = NULL  */)
{
	if (m_bInZoom)
	{
		ToggleZoom();
	}

	return BaseClass::Holster(pSwitchingTo);
}

/**
 * Check the status of the zoom key every frame to see if player is still zoomed in
 */
void CWeaponMarksmanRifle_Tier1::ItemPostFrame()
{
	// Allow zoom toggling
	CheckZoomToggle();

	BaseClass::ItemPostFrame();
}

/**
* Check the status of the zoom key every frame to see if player is still zoomed in
*/
void CWeaponMarksmanRifle_Tier1::ItemBusyFrame(void)
{
	// Allow zoom toggling even when we're reloading
	CheckZoomToggle();

	BaseClass::ItemBusyFrame();
}

/**
 * Check if the zoom key was pressed in the last input tick
 */
void CWeaponMarksmanRifle_Tier1::CheckZoomToggle(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer && (pPlayer->m_afButtonPressed & IN_ATTACK2)) //We need to include "in_buttons.h" for IN_ATTACK2
	{
		ToggleZoom();
	}
}

/**
 * If we're zooming, stop. If we're not, start.
 */
void CWeaponMarksmanRifle_Tier1::ToggleZoom(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;
#ifndef CLIENT_DLL
	if (m_bInZoom)
	{
		// Narrowing the Field Of View here is what gives us the zoomed effect
		if (pPlayer->SetFOV(this, 0, 0.2f))
		{
			m_bInZoom = false;

			// Send a message to hide the scope
			CSingleUserRecipientFilter filter(pPlayer);
			UserMessageBegin(filter, "ShowScope");
			WRITE_BYTE(0);
			MessageEnd();
		}
	}
	else
	{
		if (pPlayer->SetFOV(this, 20, 0.1f))
		{
			m_bInZoom = true;

			// Send a message to Show the scope
			CSingleUserRecipientFilter filter(pPlayer);
			UserMessageBegin(filter, "ShowScope");
			WRITE_BYTE(1);
			MessageEnd();
		}
	}
#endif
}