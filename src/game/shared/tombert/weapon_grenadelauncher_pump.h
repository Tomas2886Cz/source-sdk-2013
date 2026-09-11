//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_GRENADELAUNCHER_PUMP_H
#define WEAPON_GRENADELAUNCHER_PUMP_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#if defined( CLIENT_DLL )
#define CWeaponGrenadeLauncherPump C_WeaponGrenadeLauncherPump
#endif

class CWeaponGrenadeLauncherPump : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS(CWeaponGrenadeLauncherPump, CBaseHL2MPCombatWeapon);
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponGrenadeLauncherPump();

	virtual void	Precache(void);
	virtual void	PrimaryAttack(void);
	virtual void	AddViewKick(void);

	virtual int		GetMinBurst(void) { return 1; }
	virtual int		GetMaxBurst(void) { return 1; }
	virtual float	GetFireRate(void) { return 0.8f; }

private:
	CWeaponGrenadeLauncherPump(const CWeaponGrenadeLauncherPump&);
};

#endif // WEAPON_GRENADELAUNCHER_PUMP_H