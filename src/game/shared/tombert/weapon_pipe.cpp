#include "cbase.h"

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Pipe - Multiplayer Melee Weapon with Charge Attack
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
#define CWeaponPipe C_WeaponPipe
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
ConVar sk_pipe_range("sk_pipe_range", "75.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Pipe attack range.");
ConVar sk_pipe_refire("sk_pipe_refire", "0.4", FCVAR_REPLICATED | FCVAR_CHEAT, "Pipe attack delay/refire rate.");
ConVar sk_pipe_charge_time("sk_pipe_charge_time", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Time in seconds required to reach 100% charge.");
ConVar sk_pipe_charge_speed("sk_pipe_charge_speed", "100.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Player movement speed while charging.");
ConVar sk_pipe_min_damage("sk_pipe_min_damage", "25.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Damage dealt with 0% charge.");
ConVar sk_pipe_max_damage("sk_pipe_max_damage", "80.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Damage dealt at 100% full charge.");
#endif

// Shared Safe Accessor Helpers
static float GetPipeRange()
{
#ifndef CLIENT_DLL
	return sk_pipe_range.GetFloat();
#else
	static ConVarRef sk_range("sk_pipe_range");
	return sk_range.IsValid() ? sk_range.GetFloat() : 75.0f;
#endif
}

static float GetPipeRefire()
{
#ifndef CLIENT_DLL
	return sk_pipe_refire.GetFloat();
#else
	static ConVarRef sk_refire("sk_pipe_refire");
	return sk_refire.IsValid() ? sk_refire.GetFloat() : 0.4f;
#endif
}

static float GetPipeChargeTime()
{
#ifndef CLIENT_DLL
	return sk_pipe_charge_time.GetFloat();
#else
	static ConVarRef sk_chargetime("sk_pipe_charge_time");
	return sk_chargetime.IsValid() ? sk_chargetime.GetFloat() : 1.0f;
#endif
}

static float GetPipeChargeSpeed()
{
#ifndef CLIENT_DLL
	return sk_pipe_charge_speed.GetFloat();
#else
	static ConVarRef sk_chargespeed("sk_pipe_charge_speed");
	return sk_chargespeed.IsValid() ? sk_chargespeed.GetFloat() : 100.0f;
#endif
}

static float GetPipeMinDamage()
{
#ifndef CLIENT_DLL
	return sk_pipe_min_damage.GetFloat();
#else
	static ConVarRef sk_mindmg("sk_pipe_min_damage");
	return sk_mindmg.IsValid() ? sk_mindmg.GetFloat() : 25.0f;
#endif
}

static float GetPipeMaxDamage()
{
#ifndef CLIENT_DLL
	return sk_pipe_max_damage.GetFloat();
#else
	static ConVarRef sk_maxdmg("sk_pipe_max_damage");
	return sk_maxdmg.IsValid() ? sk_maxdmg.GetFloat() : 80.0f;
#endif
}

//-----------------------------------------------------------------------------
// CWeaponPipe Class Declaration
//-----------------------------------------------------------------------------

class CWeaponPipe : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS(CWeaponPipe, CBaseHL2MPCombatWeapon);

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

	CWeaponPipe();

	float		GetRange(void) { return GetPipeRange(); }
	float		GetFireRate(void) { return GetPipeRefire(); }
	virtual bool AllowsUnderwaterAttack(void) { return true; }

	virtual void Precache(void);
	virtual void PrimaryAttack(void);
	virtual void SecondaryAttack(void) { return; }
	virtual void ItemPreFrame(void);
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

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponPipe, DT_WeaponPipe)

#if defined( CLIENT_DLL )
BEGIN_RECV_TABLE(CWeaponPipe, DT_WeaponPipe)
RecvPropBool(RECVINFO(m_bCharging)),
RecvPropFloat(RECVINFO(m_flChargeStartTime))
END_RECV_TABLE()
#else
BEGIN_SEND_TABLE(CWeaponPipe, DT_WeaponPipe)
SendPropBool(SENDINFO(m_bCharging)),
SendPropFloat(SENDINFO(m_flChargeStartTime))
END_SEND_TABLE()
#endif

BEGIN_PREDICTION_DATA(CWeaponPipe)
#if defined( CLIENT_DLL )
DEFINE_PRED_FIELD(m_bCharging, FIELD_BOOLEAN, 0),
DEFINE_PRED_FIELD(m_flChargeStartTime, FIELD_FLOAT, 0)
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_pipe, CWeaponPipe);
PRECACHE_WEAPON_REGISTER(weapon_pipe);

#ifndef CLIENT_DLL

acttable_t	CWeaponPipe::m_acttable[] =
{
	{ ACT_MP_STAND_IDLE,				ACT_HL2MP_IDLE_MELEE,					false },
	{ ACT_MP_RUN,						ACT_HL2MP_RUN_MELEE,					false },
	{ ACT_MP_CROUCH_IDLE,				ACT_HL2MP_IDLE_CROUCH_MELEE,			false },
	{ ACT_MP_CROUCHWALK,				ACT_HL2MP_WALK_CROUCH_MELEE,			false },
	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,	ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,	false },
	{ ACT_MP_JUMP,						ACT_HL2MP_JUMP_MELEE,					false },
};

IMPLEMENT_ACTTABLE(CWeaponPipe);

#endif

//-----------------------------------------------------------------------------
// Constructor & Base Overrides
//-----------------------------------------------------------------------------
CWeaponPipe::CWeaponPipe(void)
{
	m_bCharging = false;
	m_flChargeStartTime = 0.0f;
}

void CWeaponPipe::Precache(void)
{
	BaseClass::Precache();
}

bool CWeaponPipe::Deploy(void)
{
	m_bCharging = false;
	m_flChargeStartTime = 0.0f;

	return BaseClass::Deploy();
}

bool CWeaponPipe::Holster(CBaseCombatWeapon* pSwitchTo)
{
	StopCharging();
	return BaseClass::Holster(pSwitchTo);
}

//-----------------------------------------------------------------------------
// Direct Redraw HUD Progress Rendering
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CWeaponPipe::Redraw(void)
{
	BaseClass::Redraw();

	if (!m_bCharging)
		return;

	float flElapsed = gpGlobals->curtime - m_flChargeStartTime;
	float flPercentage = clamp(flElapsed / GetPipeChargeTime(), 0.0f, 1.0f);

	int screenWidth = ScreenWidth();
	int screenHeight = ScreenHeight();

	int barWidth = 200;
	int barHeight = 16;
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
void CWeaponPipe::ItemPreFrame(void)
{
	BaseClass::ItemPreFrame();

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (pOwner && m_bCharging)
	{
		pOwner->m_nButtons &= ~IN_JUMP;
		pOwner->m_afButtonPressed &= ~IN_JUMP;

#ifndef CLIENT_DLL
		pOwner->m_afButtonDisabled |= IN_JUMP;
#endif
	}
}

void CWeaponPipe::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
		return;

	if (m_bCharging)
	{
		pOwner->m_nButtons &= ~IN_JUMP;
		pOwner->m_afButtonPressed &= ~IN_JUMP;

#ifndef CLIENT_DLL
		pOwner->m_afButtonDisabled |= IN_JUMP;
#endif
	}

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
#ifndef CLIENT_DLL
				// Dynamically apply/remove slowdown based on ground state
				CHL2MP_Player* pHL2Player = ToHL2MPPlayer(pOwner);
				if (pHL2Player)
				{
					if (pHL2Player->GetFlags() & FL_ONGROUND)
					{
						float flSpeedScale = GetPipeChargeSpeed() / 190.0f;
						pHL2Player->SetLaggedMovementValue(flSpeedScale);
					}
					else
					{
						pHL2Player->SetLaggedMovementValue(1.0f);
					}
				}
#endif

				// Max charge auto-release
				if (gpGlobals->curtime >= (m_flChargeStartTime + GetPipeChargeTime()))
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
			float flChargeRatio = (gpGlobals->curtime - m_flChargeStartTime) / GetPipeChargeTime();
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

void CWeaponPipe::PrimaryAttack(void)
{
}

void CWeaponPipe::StartCharging(void)
{
	m_bCharging = true;
	m_flChargeStartTime = gpGlobals->curtime;
	SendWeaponAnim(ACT_VM_PULLBACK);

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (pOwner)
	{
		pOwner->m_nButtons &= ~IN_JUMP;
		pOwner->m_afButtonPressed &= ~IN_JUMP;

#ifndef CLIENT_DLL
		pOwner->m_afButtonDisabled |= IN_JUMP;
#endif
	}

#ifndef CLIENT_DLL
	CHL2MP_Player* pHL2Player = ToHL2MPPlayer(pOwner);
	if (pHL2Player && (pHL2Player->GetFlags() & FL_ONGROUND))
	{
		float flSpeedScale = GetPipeChargeSpeed() / 190.0f;
		pHL2Player->SetLaggedMovementValue(flSpeedScale);
	}
#endif
}

void CWeaponPipe::StopCharging(void)
{
	m_bCharging = false;
	m_flChargeStartTime = 0.0f;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (pOwner)
	{
#ifndef CLIENT_DLL
		pOwner->m_afButtonDisabled &= ~IN_JUMP;
		pOwner->SetLaggedMovementValue(1.0f);
#endif
	}
}

void CWeaponPipe::Swing(float flChargeRatio)
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
			float flMinDmg = GetPipeMinDamage();
			float flMaxDmg = GetPipeMaxDamage();
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

float CWeaponPipe::GetDamageForActivity(Activity hitActivity)
{
	return GetPipeMinDamage();
}

void CWeaponPipe::AddViewKick(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return;

	QAngle punchAng;
	punchAng.x = SharedRandomFloat("pipepax", 2.0f, 4.0f);
	punchAng.y = SharedRandomFloat("pipepay", -3.0f, -1.0f);
	punchAng.z = 0.0f;

	pPlayer->ViewPunch(punchAng);
}

#ifndef CLIENT_DLL

void CWeaponPipe::HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
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

void CWeaponPipe::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
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

ConVar sk_pipe_lead_time("sk_pipe_lead_time", "0.9");

int CWeaponPipe::WeaponMeleeAttack1Condition(float flDot, float flDist)
{
	CAI_BaseNPC* pNPC = GetOwner() ? GetOwner()->MyNPCPointer() : NULL;
	if (!pNPC)
		return COND_NONE;

	CBaseEntity* pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity = pEnemy->GetSmoothedVelocity();

	float dt = sk_pipe_lead_time.GetFloat();
	dt += SharedRandomFloat("pipemelee1", -0.3f, 0.2f);
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

void CWeaponPipe::Drop(const Vector& vecVelocity)
{
#ifndef CLIENT_DLL
	UTIL_Remove(this);
#endif
}