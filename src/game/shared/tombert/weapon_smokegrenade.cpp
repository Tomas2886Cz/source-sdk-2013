#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#define CWeaponSmokeGrenade C_WeaponSmokeGrenade
#else
#include "items.h"
#endif

class CWeaponSmokeGrenade : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeaponSmokeGrenade, CBaseHL2MPCombatWeapon);
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponSmokeGrenade();

	void	Precache(void);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	void	DecrementAmmo(CBaseCombatCharacter* pOwner);
	void	ItemPostFrame(void);

	bool	Deploy(void);
	bool	Holster(CBaseCombatWeapon* pSwitchingTo = NULL);
	bool	Reload(void) { return false; }

private:
	void	ThrowGrenade(CBasePlayer* pPlayer, bool bLowThrow);

	CNetworkVar(bool, m_bRedraw);
	CNetworkVar(int, m_AttackPaused);
	CNetworkVar(bool, m_fLowThrow);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponSmokeGrenade, DT_WeaponSmokeGrenade)

BEGIN_NETWORK_TABLE(CWeaponSmokeGrenade, DT_WeaponSmokeGrenade)
#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bRedraw)),
RecvPropInt(RECVINFO(m_AttackPaused)),
RecvPropBool(RECVINFO(m_fLowThrow)),
#else
SendPropBool(SENDINFO(m_bRedraw)),
SendPropInt(SENDINFO(m_AttackPaused)),
SendPropBool(SENDINFO(m_fLowThrow)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponSmokeGrenade)
DEFINE_PRED_FIELD(m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_AttackPaused, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_fLowThrow, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_smokegrenade, CWeaponSmokeGrenade);
PRECACHE_WEAPON_REGISTER(weapon_smokegrenade);

CWeaponSmokeGrenade::CWeaponSmokeGrenade()
{
	m_bRedraw = false;
	m_AttackPaused = 0;
	m_fLowThrow = false;
}

void CWeaponSmokeGrenade::Precache(void)
{
	BaseClass::Precache();
}

bool CWeaponSmokeGrenade::Deploy(void)
{
	m_bRedraw = false;
	m_AttackPaused = 0;
	m_fLowThrow = false;
	return BaseClass::Deploy();
}

bool CWeaponSmokeGrenade::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	m_bRedraw = false;
	m_AttackPaused = 0;
	m_fLowThrow = false;
	return BaseClass::Holster(pSwitchingTo);
}

void CWeaponSmokeGrenade::PrimaryAttack(void)
{
	if (m_AttackPaused)
		return;

	CBaseCombatCharacter* pOwner = GetOwner();
	if (!pOwner)
		return;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return;

	m_fLowThrow = false;
	SendWeaponAnim(ACT_VM_PULLPIN);
	m_AttackPaused = 1;
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

void CWeaponSmokeGrenade::SecondaryAttack(void)
{
	if (m_AttackPaused)
		return;

	CBaseCombatCharacter* pOwner = GetOwner();
	if (!pOwner)
		return;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return;

	m_fLowThrow = true;
	SendWeaponAnim(ACT_VM_PULLPIN);
	m_AttackPaused = 1;
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

void CWeaponSmokeGrenade::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
		return;

	if (m_AttackPaused == 1)
	{
		// Wait for button release to throw
		if (!(pOwner->m_nButtons & (IN_ATTACK | IN_ATTACK2)))
		{
			ThrowGrenade(pOwner, m_fLowThrow);
			DecrementAmmo(pOwner);
			m_AttackPaused = 2;
			m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
		}
	}
	else if (m_AttackPaused == 2)
	{
		if (gpGlobals->curtime >= m_flTimeWeaponIdle)
		{
			if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
			{
#ifndef CLIENT_DLL
				pOwner->Weapon_Drop(this, NULL, NULL);
				UTIL_Remove(this);
#endif
			}
			else
			{
				SendWeaponAnim(ACT_VM_DRAW);
				m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
				m_AttackPaused = 0;
			}
		}
	}

	BaseClass::ItemPostFrame();
}

void CWeaponSmokeGrenade::DecrementAmmo(CBaseCombatCharacter* pOwner)
{
	pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
}

void CWeaponSmokeGrenade::ThrowGrenade(CBasePlayer* pPlayer, bool bLowThrow)
{
	SendWeaponAnim(ACT_VM_THROW);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL
	Vector vecForward, vecRight, vecUp;
	pPlayer->EyeVectors(&vecForward, &vecRight, &vecUp);

	Vector vecSrc = pPlayer->Weapon_ShootPosition() + vecForward * 16.0f + vecUp * -4.0f;

	// Primary throws far; secondary drops/rolls softly right in front
	float flSpeed = bLowThrow ? 200.0f : 1000.0f;
	Vector vecVelocity = vecForward * flSpeed;

	if (bLowThrow)
	{
		vecVelocity.z += 50.0f;
	}
	else
	{
		vecVelocity.z += 250.0f; // Arc upward for primary throw
	}

	Vector vecPlayerVel;
	pPlayer->GetVelocity(&vecPlayerVel, NULL);
	vecVelocity += vecPlayerVel * 0.5f;

	CBaseEntity* pGrenade = CreateEntityByName("npc_grenade_smoke");
	if (pGrenade)
	{
		pGrenade->SetAbsOrigin(vecSrc);
		pGrenade->SetOwnerEntity(pPlayer);

		DispatchSpawn(pGrenade);

		// Apply velocity directly to VPhysics so it actually flies
		IPhysicsObject* pPhysics = pGrenade->VPhysicsGetObject();
		if (pPhysics)
		{
			pPhysics->SetVelocity(&vecVelocity, NULL);
			AngularImpulse spin(600, random->RandomInt(-1200, 1200), 0);
			pPhysics->AddVelocity(NULL, &spin);
		}
		else
		{
			pGrenade->SetAbsVelocity(vecVelocity);
		}
	}
#endif
}