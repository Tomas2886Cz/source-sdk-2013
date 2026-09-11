#ifndef WEAPON_PISTOL_STEALTH_H
#define WEAPON_PISTOL_STEALTH_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#if defined( CLIENT_DLL )
	#define CWeaponPistolStealth C_WeaponPistolStealth
#endif

class CWeaponPistolStealth : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponPistolStealth, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponPistolStealth();

	virtual void	Precache( void );
	virtual void	ItemPreFrame( void );
	virtual void	ItemBusyFrame( void );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual void	AddViewKick( void );
	virtual void	DryFire( void );
	virtual bool	Reload(void);

	virtual void	UpdatePenaltyTime( void );

	virtual Vector GetBulletSpread(WeaponProficiency_t proficiency);

	virtual int		GetMinBurst() { return 1; }
	virtual int		GetMaxBurst() { return 1; }
	virtual float	GetFireRate( void ) { return 0.5f; }

	// --------------------------------------------------------
	// Custom Mechanic: Manual cycle state
	// --------------------------------------------------------
	CNetworkVar( bool, m_bNeedsCocking );

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	float	m_flAccuracyPenalty;
	int		m_nNumShotsFired;

	CWeaponPistolStealth( const CWeaponPistolStealth & );
};

#endif // WEAPON_PISTOL_STEALTH_H