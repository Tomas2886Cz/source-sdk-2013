//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shotgun implementation that locks firing states until reload finishes completely.
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#else
#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponDoubleBarrel C_WeaponDoubleBarrel
#endif

extern ConVar sk_auto_reload_time;
extern ConVar sk_plr_num_doublebarrel_pellets;

class CWeaponDoubleBarrel : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS(CWeaponDoubleBarrel, CBaseHL2MPCombatWeapon);

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

private:
	CNetworkVar(bool, m_bNeedPump);		// When emptied completely
	CNetworkVar(bool, m_bDelayedFire1);	// Fire primary when finished reloading
	CNetworkVar(bool, m_bDelayedFire2);	// Fire secondary when finished reloading
	CNetworkVar(bool, m_bDelayedReload);	// Reload when finished pump

public:
	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone = VECTOR_CONE_15DEGREES;
		return cone;
	}

	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 3; }

	bool StartReload(void);
	bool Reload(void);
	void FillClip(void);
	void FinishReload(void);
	void CheckHolsterReload(void);
	void Pump(void);
	void ItemHolsterFrame(void);
	void ItemPostFrame(void);
	void PrimaryAttack(void);
	void SecondaryAttack(void);
	void DryFire(void);
	virtual float GetFireRate(void) { return 0.3; };

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

	CWeaponDoubleBarrel(void);

private:
	CWeaponDoubleBarrel(const CWeaponDoubleBarrel&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponDoubleBarrel, DT_WeaponDoubleBarrel)

BEGIN_NETWORK_TABLE(CWeaponDoubleBarrel, DT_WeaponDoubleBarrel)
#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bNeedPump)),
RecvPropBool(RECVINFO(m_bDelayedFire1)),
RecvPropBool(RECVINFO(m_bDelayedFire2)),
RecvPropBool(RECVINFO(m_bDelayedReload)),
#else
SendPropBool(SENDINFO(m_bNeedPump)),
SendPropBool(SENDINFO(m_bDelayedFire1)),
SendPropBool(SENDINFO(m_bDelayedFire2)),
SendPropBool(SENDINFO(m_bDelayedReload)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponDoubleBarrel)
DEFINE_PRED_FIELD(m_bNeedPump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bDelayedFire1, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bDelayedFire2, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_doublebarrel, CWeaponDoubleBarrel);
PRECACHE_WEAPON_REGISTER(weapon_doublebarrel);

#ifndef CLIENT_DLL
acttable_t	CWeaponDoubleBarrel::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SHOTGUN,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SHOTGUN,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SHOTGUN,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SHOTGUN,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SHOTGUN,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SHOTGUN,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SHOTGUN,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SHOTGUN,				false },
};
IMPLEMENT_ACTTABLE(CWeaponDoubleBarrel);
#endif

//-----------------------------------------------------------------------------
// Purpose: Starts single shell insertion pass
//-----------------------------------------------------------------------------
bool CWeaponDoubleBarrel::StartReload(void)
{
	if (m_bNeedPump)
		return false;

	CBaseCombatCharacter* pOwner = GetOwner();
	if (pOwner == NULL)
		return false;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	int j = MIN(1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));
	if (j <= 0)
		return false;

	SendWeaponAnim(ACT_SHOTGUN_RELOAD_START);
	SetBodygroup(1, 0); // Make shell visible

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	m_bInReload = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates incremental shell fills
//-----------------------------------------------------------------------------
bool CWeaponDoubleBarrel::Reload(void)
{
	if (!m_bInReload)
	{
		Warning("ERROR: Shotgun Reload called incorrectly!\n");
	}

	CBaseCombatCharacter* pOwner = GetOwner();
	if (pOwner == NULL)
		return false;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	int j = MIN(1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));
	if (j <= 0)
		return false;

	FillClip();
	WeaponSound(RELOAD);
	SendWeaponAnim(ACT_VM_RELOAD);

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload animation and close the chamber
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::FinishReload(void)
{
	SetBodygroup(1, 1); // Hide shell matching closed chamber geometry

	CBaseCombatCharacter* pOwner = GetOwner();
	if (pOwner == NULL)
		return;

	m_bInReload = false;
	SendWeaponAnim(ACT_SHOTGUN_RELOAD_FINISH);

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Transports ammo pool units to internal magazine clip
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::FillClip(void)
{
	CBaseCombatCharacter* pOwner = GetOwner();
	if (pOwner == NULL)
		return;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) > 0)
	{
		if (Clip1() < GetMaxClip1())
		{
			m_iClip1++;
			pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cycles action mechanical pump sequence
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::Pump(void)
{
	CBaseCombatCharacter* pOwner = GetOwner();
	if (pOwner == NULL)
		return;

	m_bNeedPump = false;

	if (m_bDelayedReload)
	{
		m_bDelayedReload = false;
		StartReload();
	}

	WeaponSound(SPECIAL1);
	SendWeaponAnim(ACT_SHOTGUN_PUMP);

	pOwner->m_flNextAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}
//-----------------------------------------------------------------------------
// Purpose: dry fire audio responses
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::DryFire(void)
{
	WeaponSound(EMPTY);
	SendWeaponAnim(ACT_VM_DRYFIRE);

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Primary Firing Actions
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::PrimaryAttack(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer || m_bInReload) // LOCKED: Prevent firing mid-reload sequence
	{
		return;
	}

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();
	SendWeaponAnim(ACT_VM_PRIMARYATTACK);

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_iClip1 -= 1;

	pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	FireBulletsInfo_t info(20, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
	info.m_pAttacker = pPlayer;

	pPlayer->FireBullets(info);

	QAngle punch;
	punch.Init(SharedRandomFloat("shotgunpax", -8, -8), SharedRandomFloat("shotgunpay", -8, 8), 0);
	pPlayer->ViewPunch(punch);

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	m_bNeedPump = true;
}

//-----------------------------------------------------------------------------
// Purpose: Secondary Fire Attack Logic
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::SecondaryAttack(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer || m_bInReload) // LOCKED: Prevent firing mid-reload sequence
	{
		return;
	}

	pPlayer->m_nButtons &= ~IN_ATTACK2;
	WeaponSound(WPN_DOUBLE);
	pPlayer->DoMuzzleFlash();
	SendWeaponAnim(ACT_VM_SECONDARYATTACK);

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_iClip1 -= 2;

	pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	FireBulletsInfo_t info(12, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
	info.m_pAttacker = pPlayer;

	pPlayer->FireBullets(info);
	pPlayer->ViewPunch(QAngle(SharedRandomFloat("shotgunpax", -20, 20), SharedRandomFloat("shotgunpay", -20, 20), 0));

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	m_bNeedPump = true;
}

//-----------------------------------------------------------------------------
// Purpose: Frame calculation loops containing updated input gate logic
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
	{
		return;
	}

	if (m_bNeedPump && (pOwner->m_nButtons & IN_RELOAD))
	{
		m_bDelayedReload = true;
	}

	if (m_bInReload)
	{
		// FIXED: Removed the early breakout blocks that used to let IN_ATTACK 
		// and IN_ATTACK2 interrupt the active reload cycle.
		if (m_flNextPrimaryAttack <= gpGlobals->curtime)
		{
			if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
			{
				FinishReload();
				return;
			}
			if (m_iClip1 < GetMaxClip1())
			{
				Reload();
				return;
			}
			else
			{
				FinishReload();
				return;
			}
		}
	}
	else
	{
		SetBodygroup(1, 1); // Close shell chamber
	}

	if ((m_bNeedPump) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		Pump();
		return;
	}

	// BLOCKED: Firing checks will only execute if m_bInReload is completely false
	if (!m_bInReload)
	{
		if ((m_bDelayedFire2 || pOwner->m_nButtons & IN_ATTACK2) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
		{
			m_bDelayedFire2 = false;

			if ((m_iClip1 <= 1 && UsesClipsForAmmo1()))
			{
				if (m_iClip1 == 1)
				{
					PrimaryAttack();
				}
				else if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
				{
					DryFire();
				}
				else
				{
					StartReload();
				}
			}
			else if (GetOwner()->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
			{
				WeaponSound(EMPTY);
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
				return;
			}
			else
			{
				if (pOwner->m_afButtonPressed & IN_ATTACK2)
				{
					m_flNextPrimaryAttack = gpGlobals->curtime;
				}
				SecondaryAttack();
			}
		}
		else if ((m_bDelayedFire1 || pOwner->m_nButtons & IN_ATTACK) && m_flNextPrimaryAttack <= gpGlobals->curtime)
		{
			m_bDelayedFire1 = false;
			if ((m_iClip1 <= 0 && UsesClipsForAmmo1()) || (!UsesClipsForAmmo1() && !pOwner->GetAmmoCount(m_iPrimaryAmmoType)))
			{
				if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
				{
					DryFire();
				}
				else
				{
					StartReload();
				}
			}
			else if (pOwner->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
			{
				WeaponSound(EMPTY);
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
				return;
			}
			else
			{
				if (pOwner->m_afButtonPressed & IN_ATTACK)
				{
					m_flNextPrimaryAttack = gpGlobals->curtime;
				}
				PrimaryAttack();
			}
		}
	}

	if (pOwner->m_nButtons & IN_RELOAD && UsesClipsForAmmo1() && !m_bInReload)
	{
		StartReload();
	}
	else if (!m_bInReload)
	{
		m_bFireOnEmpty = false;

		if (!HasAnyAmmo() && m_flNextPrimaryAttack < gpGlobals->curtime)
		{
			if (!(GetWeaponFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && pOwner->SwitchToNextBestWeapon(this))
			{
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
				return;
			}
		}
		else
		{
			if (m_iClip1 <= 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime)
			{
				if (StartReload())
				{
					return;
				}
			}
		}

		WeaponIdle();
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor Initializations
//-----------------------------------------------------------------------------
CWeaponDoubleBarrel::CWeaponDoubleBarrel(void)
{
	m_bReloadsSingly = true;
	m_bNeedPump = false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;

	m_fMinRange1 = 0.0;
	m_fMaxRange1 = 300;
	m_fMinRange2 = 0.0;
	m_fMaxRange2 = 150;
}

//-----------------------------------------------------------------------------
// Purpose: Background holster tracking adjustments
//-----------------------------------------------------------------------------
void CWeaponDoubleBarrel::ItemHolsterFrame(void)
{
	if (GetOwner() && GetOwner()->IsPlayer() == false)
		return;

	if (GetOwner()->GetActiveWeapon() == this)
		return;

	if ((gpGlobals->curtime - m_flHolsterTime) > sk_auto_reload_time.GetFloat())
	{
		m_flHolsterTime = gpGlobals->curtime;

		if (GetOwner() == NULL)
			return;

		if (m_iClip1 == GetMaxClip1())
			return;

		int ammoFill = MIN((GetMaxClip1() - m_iClip1), GetOwner()->GetAmmoCount(GetPrimaryAmmoType()));
		GetOwner()->RemoveAmmo(ammoFill, GetPrimaryAmmoType());
		m_iClip1 += ammoFill;
	}
}