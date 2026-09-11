//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_grenadelauncher_pump.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GAME_DLL
#include "basegrenade_shared.h"

//-----------------------------------------------------------------------------
// CGLGrenade
//-----------------------------------------------------------------------------
class CGLGrenade : public CBaseGrenade
{
	DECLARE_CLASS(CGLGrenade, CBaseGrenade);
public:
	void Spawn(void);
	void Precache(void);
	void GrenadeTouch(CBaseEntity* pOther);

	static CGLGrenade* Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float damage, float radius);

	DECLARE_DATADESC();
};

BEGIN_DATADESC(CGLGrenade)
DEFINE_ENTITYFUNC(GrenadeTouch),
END_DATADESC()

LINK_ENTITY_TO_CLASS(gl_grenade, CGLGrenade);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGLGrenade::Spawn(void)
{
	Precache();
	SetModel("models/w_models/weapons/w_he_grenade.mdl");

	SetMoveType(MOVETYPE_FLYGRAVITY);
	SetSolid(SOLID_BBOX);
	SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

	UTIL_SetSize(this, Vector(-3, -3, -3), Vector(3, 3, 3));
	SetTouch(&CGLGrenade::GrenadeTouch);

	m_flDamage = 120.0f;
	m_DmgRadius = 250.0f;
	m_takedamage = DAMAGE_NO;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGLGrenade::Precache(void)
{
	PrecacheModel("models/w_models/weapons/w_he_grenade.mdl");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGLGrenade::GrenadeTouch(CBaseEntity* pOther)
{
	if (pOther == GetOwnerEntity())
		return;

	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
		return;

	trace_t tr;
	tr = BaseClass::GetTouchTrace();
	Explode(&tr, DMG_BLAST);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGLGrenade* CGLGrenade::Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float damage, float radius)
{
	CGLGrenade* pGrenade = (CGLGrenade*)CreateEntityByName("gl_grenade");
	pGrenade->SetAbsAngles(angles);
	pGrenade->Spawn();
	pGrenade->SetOwnerEntity(pOwner);
	UTIL_SetOrigin(pGrenade, position);

	pGrenade->SetAbsVelocity(velocity);
	pGrenade->ApplyLocalAngularVelocityImpulse(angVelocity);

	pGrenade->m_flDamage = damage;
	pGrenade->m_DmgRadius = radius;

	return pGrenade;
}
#endif // GAME_DLL

//-----------------------------------------------------------------------------
// CWeaponGrenadeLauncherPump
//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED(WeaponGrenadeLauncherPump, DT_WeaponGrenadeLauncherPump)

BEGIN_NETWORK_TABLE(CWeaponGrenadeLauncherPump, DT_WeaponGrenadeLauncherPump)
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponGrenadeLauncherPump)
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_grenadelauncher_pump, CWeaponGrenadeLauncherPump);
PRECACHE_WEAPON_REGISTER(weapon_grenadelauncher_pump);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponGrenadeLauncherPump::CWeaponGrenadeLauncherPump(void)
{
	m_fMinRange1 = 64;
	m_fMaxRange1 = 1500;
	m_bFiresUnderwater = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeLauncherPump::Precache(void)
{
	BaseClass::Precache();
#ifdef GAME_DLL
	UTIL_PrecacheOther("gl_grenade");
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeLauncherPump::PrimaryAttack(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
		return;

	if (m_iClip1 <= 0)
	{
		WeaponSound(EMPTY);
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		return;
	}

	m_iClip1--;

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pOwner->SetAnimation(PLAYER_ATTACK1);
	WeaponSound(SINGLE);

#ifdef GAME_DLL
	Vector vecSrc = pOwner->Weapon_ShootPosition();
	Vector vecAiming = pOwner->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	vecAiming.z += 0.15f;
	VectorNormalize(vecAiming);

	Vector vecVelocity = vecAiming * 1200.0f;
	AngularImpulse angVelocity(random->RandomFloat(-100, -500), 0, 0);

	CGLGrenade::Create(vecSrc, pOwner->EyeAngles(), vecVelocity, angVelocity, pOwner, 120.0f, 250.0f);
#endif

	float flAnimLength = SequenceDuration();

	m_flNextPrimaryAttack = gpGlobals->curtime + flAnimLength;
	m_flTimeWeaponIdle = gpGlobals->curtime + flAnimLength;

	AddViewKick();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeLauncherPump::AddViewKick(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
		return;

	QAngle viewPunch;
	viewPunch.x = random->RandomFloat(-3.0f, -5.0f);
	viewPunch.y = 10.0f;
	viewPunch.z = 5.0f;

	pOwner->ViewPunch(viewPunch);
}