#include "cbase.h"

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		FireAxe - Multiplayer Melee Weapon with Charge Attack
//
//=============================================================================//

#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"
#include "vstdlib/random.h"
#include "npcevent.h"

#if defined( CLIENT_DLL )
#include "c_hl2mp_player.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Panel.h"
#define CWeaponFireAxe C_WeaponFireAxe
#else
#include "hl2mp_player.h"
#include "ai_basenpc.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Server-side ConVars for Dynamic Balancing
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
ConVar sk_fireaxe_range("sk_fireaxe_range", "75.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Fire axe attack range.");
ConVar sk_fireaxe_refire("sk_fireaxe_refire", "0.4", FCVAR_REPLICATED | FCVAR_CHEAT, "Fire axe attack delay/refire rate.");
ConVar sk_fireaxe_charge_time("sk_fireaxe_charge_time", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Time in seconds required to reach 100% charge.");
ConVar sk_fireaxe_charge_speed("sk_fireaxe_charge_speed", "100.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Player movement speed while charging.");
ConVar sk_fireaxe_min_damage("sk_fireaxe_min_damage", "25.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Damage dealt with 0% charge.");
ConVar sk_fireaxe_max_damage("sk_fireaxe_max_damage", "80.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Damage dealt at 100% full charge.");
#endif

// Shared Safe Accessor Helpers
static float GetAxeRange()
{
#ifndef CLIENT_DLL
	return sk_fireaxe_range.GetFloat();
#else
	static ConVarRef sk_range("sk_fireaxe_range");
	return sk_range.IsValid() ? sk_range.GetFloat() : 75.0f;
#endif
}

static float GetAxeRefire()
{
#ifndef CLIENT_DLL
	return sk_fireaxe_refire.GetFloat();
#else
	static ConVarRef sk_refire("sk_fireaxe_refire");
	return sk_refire.IsValid() ? sk_refire.GetFloat() : 0.4f;
#endif
}

static float GetAxeChargeTime()
{
#ifndef CLIENT_DLL
	return sk_fireaxe_charge_time.GetFloat();
#else
	static ConVarRef sk_chargetime("sk_fireaxe_charge_time");
	return sk_chargetime.IsValid() ? sk_chargetime.GetFloat() : 1.0f;
#endif
}

static float GetAxeChargeSpeed()
{
#ifndef CLIENT_DLL
	return sk_fireaxe_charge_speed.GetFloat();
#else
	static ConVarRef sk_chargespeed("sk_fireaxe_charge_speed");
	return sk_chargespeed.IsValid() ? sk_chargespeed.GetFloat() : 100.0f;
#endif
}

static float GetAxeMinDamage()
{
#ifndef CLIENT_DLL
	return sk_fireaxe_min_damage.GetFloat();
#else
	static ConVarRef sk_mindmg("sk_fireaxe_min_damage");
	return sk_mindmg.IsValid() ? sk_mindmg.GetFloat() : 25.0f;
#endif
}

static float GetAxeMaxDamage()
{
#ifndef CLIENT_DLL
	return sk_fireaxe_max_damage.GetFloat();
#else
	static ConVarRef sk_maxdmg("sk_fireaxe_max_damage");
	return sk_maxdmg.IsValid() ? sk_maxdmg.GetFloat() : 80.0f;
#endif
}

//-----------------------------------------------------------------------------
// CWeaponFireAxe Class Declaration
//-----------------------------------------------------------------------------

class CWeaponFireAxe : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS(CWeaponFireAxe, CBaseHL2MPCombatWeapon);

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

	CWeaponFireAxe();

	float		GetRange(void) { return GetAxeRange(); }
	float		GetFireRate(void) { return GetAxeRefire(); }
	virtual bool AllowsUnderwaterAttack(void) { return true; }

	virtual void Precache(void);
	virtual void PrimaryAttack(void);
	virtual void SecondaryAttack(void) { return; }
	virtual void ItemPostFrame(void);
	virtual bool Holster(CBaseCombatWeapon* pSwitchTo = NULL);
	virtual bool Deploy(void);

#ifdef CLIENT_DLL
	virtual void Redraw(void);
#endif

	void		StartCharging(void);
	void		StopCharging(void);
	void		Swing(float flChargeRatio = 0.0f);
	void		AddViewKick(void);
	float		GetDamageForActivity(Activity hitActivity);

	void		Drop(const Vector& vecVelocity);

	// Networked Charge Variables
	CNetworkVar(bool, m_bCharging);
	CNetworkVar(float, m_flChargeStartTime);

#ifndef CLIENT_DLL
	virtual int WeaponMeleeAttack1Condition(float flDot, float flDist);
	virtual void Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);

private:
	void HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
#endif
};

//-----------------------------------------------------------------------------
// Network Table & Prediction
//-----------------------------------------------------------------------------

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponFireAxe, DT_WeaponFireAxe)

#if defined( CLIENT_DLL )
BEGIN_RECV_TABLE(CWeaponFireAxe, DT_WeaponFireAxe)
RecvPropBool(RECVINFO(m_bCharging)),
RecvPropFloat(RECVINFO(m_flChargeStartTime))
END_RECV_TABLE()
#else
BEGIN_SEND_TABLE(CWeaponFireAxe, DT_WeaponFireAxe)
SendPropBool(SENDINFO(m_bCharging)),
SendPropFloat(SENDINFO(m_flChargeStartTime))
END_SEND_TABLE()
#endif

BEGIN_PREDICTION_DATA(CWeaponFireAxe)
#if defined( CLIENT_DLL )
DEFINE_PRED_FIELD(m_bCharging, FIELD_BOOLEAN, 0),
DEFINE_PRED_FIELD(m_flChargeStartTime, FIELD_FLOAT, 0)
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_fireaxe, CWeaponFireAxe);
PRECACHE_WEAPON_REGISTER(weapon_fireaxe);

#ifndef CLIENT_DLL

acttable_t	CWeaponFireAxe::m_acttable[] =
{
	{ ACT_MP_STAND_IDLE,				ACT_HL2MP_IDLE_MELEE,					false },
	{ ACT_MP_RUN,						ACT_HL2MP_RUN_MELEE,					false },
	{ ACT_MP_CROUCH_IDLE,				ACT_HL2MP_IDLE_CROUCH_MELEE,			false },
	{ ACT_MP_CROUCHWALK,				ACT_HL2MP_WALK_CROUCH_MELEE,			false },
	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,	ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,	false },
	{ ACT_MP_JUMP,						ACT_HL2MP_JUMP_MELEE,					false },
};

IMPLEMENT_ACTTABLE(CWeaponFireAxe);

#endif

//-----------------------------------------------------------------------------
// Constructor & Base Overrides
//-----------------------------------------------------------------------------
CWeaponFireAxe::CWeaponFireAxe(void)
{
	m_bCharging = false;
	m_flChargeStartTime = 0.0f;
}

void CWeaponFireAxe::Precache(void)
{
	BaseClass::Precache();
}

bool CWeaponFireAxe::Deploy(void)
{
	m_bCharging = false;
	m_flChargeStartTime = 0.0f;

	return BaseClass::Deploy();
}

bool CWeaponFireAxe::Holster(CBaseCombatWeapon* pSwitchTo)
{
	StopCharging();
	return BaseClass::Holster(pSwitchTo);
}

//-----------------------------------------------------------------------------
// Direct Redraw HUD Progress Rendering
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CWeaponFireAxe::Redraw(void)
{
	BaseClass::Redraw();

	if (!m_bCharging)
		return;

	float flElapsed = gpGlobals->curtime - m_flChargeStartTime;
	float flPercentage = clamp(flElapsed / GetAxeChargeTime(), 0.0f, 1.0f);

	int screenWidth = ScreenWidth();
	int screenHeight = ScreenHeight();

	int barWidth = 400;
	int barHeight = 24;
	int xPos = (screenWidth / 2) - (barWidth / 2);
	int yPos = (screenHeight / 2) + 60;

	vgui::surface()->DrawSetColor(0, 0, 0, 200);
	vgui::surface()->DrawFilledRect(xPos, yPos, xPos + barWidth, yPos + barHeight);

	int progressFillWidth = (int)(barWidth * flPercentage);
	vgui::surface()->DrawSetColor(255, 230, 185, 255);
	vgui::surface()->DrawFilledRect(xPos + 2, yPos + 2, xPos + progressFillWidth - 2, yPos + barHeight - 2);
}
#endif

//-----------------------------------------------------------------------------
// Logic & Frame Processing
//-----------------------------------------------------------------------------
void CWeaponFireAxe::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
		return;

	if (pOwner->m_nButtons & IN_ATTACK)
	{
		if (m_flNextPrimaryAttack <= gpGlobals->curtime)
		{
			if (!m_bCharging)
			{
				StartCharging();
			}
			else
			{
				// Max charge auto-release
				if (gpGlobals->curtime >= (m_flChargeStartTime + GetAxeChargeTime()))
				{
					StopCharging();
					Swing(1.0f);
				}
			}
		}
	}
	else
	{
		// Released attack button early: Swing with current charge ratio
		if (m_bCharging)
		{
			float flChargeRatio = (gpGlobals->curtime - m_flChargeStartTime) / GetAxeChargeTime();
			flChargeRatio = clamp(flChargeRatio, 0.0f, 1.0f);

			StopCharging();
			Swing(flChargeRatio);
		}
		else
		{
			WeaponIdle();
		}
	}
}

void CWeaponFireAxe::PrimaryAttack(void)
{
}

void CWeaponFireAxe::StartCharging(void)
{
	m_bCharging = true;
	m_flChargeStartTime = gpGlobals->curtime;
	SendWeaponAnim(ACT_VM_PULLBACK);

#ifndef CLIENT_DLL
	CHL2MP_Player* pOwner = ToHL2MPPlayer(GetOwner());
	if (pOwner)
	{
		pOwner->SetMaxSpeed(GetAxeChargeSpeed());
	}
#endif
}

void CWeaponFireAxe::StopCharging(void)
{
	m_bCharging = false;
	m_flChargeStartTime = 0.0f;

#ifndef CLIENT_DLL
	CHL2MP_Player* pOwner = ToHL2MPPlayer(GetOwner());
	if (pOwner)
	{
		pOwner->SetMaxSpeed(190.0f);
	}
#endif
}

void CWeaponFireAxe::Swing(float flChargeRatio)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
		return;

	SendWeaponAnim(ACT_VM_HITCENTER);
	pOwner->SetAnimation(PLAYER_ATTACK1);

	WeaponSound(SINGLE);

	Vector vecSrc = pOwner->Weapon_ShootPosition();
	Vector vecAiming;
	pOwner->EyeVectors(&vecAiming);
	Vector vecEnd = vecSrc + (vecAiming * GetRange());

	trace_t tr;
	UTIL_TraceHull(vecSrc, vecEnd, Vector(-16, -16, -16), Vector(16, 16, 16), MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0f)
	{
		if (tr.m_pEnt)
		{
#ifndef CLIENT_DLL
			ClearMultiDamage();

			// Scale damage based on charge duration via ConVars
			float flMinDmg = GetAxeMinDamage();
			float flMaxDmg = GetAxeMaxDamage();
			float flDamage = flMinDmg + (flChargeRatio * (flMaxDmg - flMinDmg));

			CTakeDamageInfo info(this, pOwner, flDamage, DMG_CLUB);
			CalculateMeleeDamageForce(&info, vecAiming, tr.endpos);
			tr.m_pEnt->DispatchTraceAttack(info, vecAiming, &tr);

			ApplyMultiDamage();
#endif
			WeaponSound(MELEE_HIT);
		}
		else
		{
			WeaponSound(MELEE_HIT_WORLD);
		}

#ifndef CLIENT_DLL
		UTIL_ImpactTrace(&tr, DMG_CLUB);
#endif
	}

	AddViewKick();

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
}

float CWeaponFireAxe::GetDamageForActivity(Activity hitActivity)
{
	return GetAxeMinDamage();
}

void CWeaponFireAxe::AddViewKick(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return;

	QAngle punchAng;
	punchAng.x = SharedRandomFloat("fireaxepax", 2.0f, 4.0f);
	punchAng.y = SharedRandomFloat("fireaxepay", -3.0f, -1.0f);
	punchAng.z = 0.0f;

	pPlayer->ViewPunch(punchAng);
}

#ifndef CLIENT_DLL

void CWeaponFireAxe::HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	Vector vecDirection;
	AngleVectors(GetAbsAngles(), &vecDirection);

	Vector vecEnd;
	VectorMA(pOperator->Weapon_ShootPosition(), 50, vecDirection, vecEnd);
	CBaseEntity* pHurt = pOperator->CheckTraceHullAttack(pOperator->Weapon_ShootPosition(), vecEnd,
		Vector(-16, -16, -16), Vector(36, 36, 36), GetDamageForActivity(GetActivity()), DMG_CLUB, 0.75);

	if (pHurt)
	{
		WeaponSound(MELEE_HIT);

		trace_t traceHit;
		UTIL_TraceLine(pOperator->Weapon_ShootPosition(), pHurt->GetAbsOrigin(), MASK_SHOT_HULL, pOperator, COLLISION_GROUP_NONE, &traceHit);
		UTIL_ImpactTrace(&traceHit, DMG_CLUB);
	}
	else
	{
		WeaponSound(MELEE_MISS);
	}
}

void CWeaponFireAxe::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_MELEE_HIT:
		HandleAnimEventMeleeHit(pEvent, pOperator);
		break;

	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

ConVar sk_fireaxe_lead_time("sk_fireaxe_lead_time", "0.9");

int CWeaponFireAxe::WeaponMeleeAttack1Condition(float flDot, float flDist)
{
	CAI_BaseNPC* pNPC = GetOwner() ? GetOwner()->MyNPCPointer() : NULL;
	if (!pNPC)
		return COND_NONE;

	CBaseEntity* pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity = pEnemy->GetSmoothedVelocity();

	float dt = sk_fireaxe_lead_time.GetFloat();
	dt += SharedRandomFloat("fireaxemelee1", -0.3f, 0.2f);
	if (dt < 0.0f)
		dt = 0.0f;

	Vector vecExtrapolatedPos;
	VectorMA(pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos);

	Vector vecDelta;
	VectorSubtract(vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta);

	if (fabs(vecDelta.z) > 70)
		return COND_TOO_FAR_TO_ATTACK;

	Vector vecForward = pNPC->BodyDirection2D();
	vecDelta.z = 0.0f;
	float flExtrapolatedDist = Vector2DNormalize(vecDelta.AsVector2D());
	if ((flDist > 64) && (flExtrapolatedDist > 64))
		return COND_TOO_FAR_TO_ATTACK;

	float flExtrapolatedDot = DotProduct2D(vecDelta.AsVector2D(), vecForward.AsVector2D());
	if ((flDot < 0.7) && (flExtrapolatedDot < 0.7))
		return COND_NOT_FACING_ATTACK;

	return COND_CAN_MELEE_ATTACK1;
}

#endif

void CWeaponFireAxe::Drop(const Vector& vecVelocity)
{
#ifndef CLIENT_DLL
	UTIL_Remove(this);
#endif
}