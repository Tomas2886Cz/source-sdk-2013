#pragma
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_FISTS_H
#define WEAPON_FISTS_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#define	FISTS_RANGE	75.0f
#define	FISTS_REFIRE	0.4f

//-----------------------------------------------------------------------------
// CWeaponFists
//-----------------------------------------------------------------------------

class CWeaponFists : public CBaseHLBludgeonWeapon
{
public:
	DECLARE_CLASS(CWeaponFists, CBaseHLBludgeonWeapon);

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponFists();

	float		GetRange(void) { return	FISTS_RANGE; }
	float		GetFireRate(void) { return	FISTS_REFIRE; }

	void		AddViewKick(void);
	float		GetDamageForActivity(Activity hitActivity);

	virtual int WeaponMeleeAttack1Condition(float flDot, float flDist);
	void		SecondaryAttack(void) { return; }

	// Animation event
	virtual void Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
};

#endif // WEAPON_FISTS_H
