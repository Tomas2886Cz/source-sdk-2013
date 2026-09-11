#include "cbase.h"
#include "weapon_pistol_stealth.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PISTOL_FASTEST_REFIRE_TIME 0.1f
#define PISTOL_ACCURACY_SHOT_PENALTY_TIME 0.2f
#define PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME 1.5f

ConVar pistol_stealth_use_new_accuracy( "pistol_stealth_use_new_accuracy", "1", FCVAR_REPLICATED );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPistolStealth, DT_WeaponPistolStealth )

BEGIN_NETWORK_TABLE(CWeaponPistolStealth, DT_WeaponPistolStealth)
#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bNeedsCocking)),
#else
SendPropBool(SENDINFO(m_bNeedsCocking)),
#endif
END_NETWORK_TABLE()

// WRAP THE PREDICTION IN CLIENT_DLL SO THE SERVER IGNORES IT
#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponPistolStealth)
DEFINE_PRED_FIELD(m_bNeedsCocking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_pistol_stealth, CWeaponPistolStealth );
PRECACHE_WEAPON_REGISTER( weapon_pistol_stealth );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponPistolStealth::CWeaponPistolStealth( void )
{
	m_flSoonestPrimaryAttack = 0.0f;
	m_flAccuracyPenalty = 0.0f;
	m_bNeedsCocking = false; // Gun starts ready to fire
	m_bFiresUnderwater = true;
}

void CWeaponPistolStealth::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Core Mechanic: Intercepting the Attack Button
//-----------------------------------------------------------------------------
void CWeaponPistolStealth::ItemPostFrame(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (pOwner == NULL)
		return;

	// Check if the gun needs cocking AND the player presses RIGHT CLICK (IN_ATTACK2)
	if (m_bNeedsCocking && (pOwner->m_nButtons & IN_ATTACK2) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		CBaseViewModel* pVM = pOwner->GetViewModel();
		if (pVM)
		{
			int nSequence = pVM->LookupSequence("base_fire_end");
			if (nSequence != -1)
			{
				pVM->SendViewModelMatchingSequence(nSequence);

				// Get the exact length of the animation
				float flDuration = pVM->SequenceDuration(nSequence);

				// Delay the next shot AND the idle animation so it doesn't get cut off
				m_flNextPrimaryAttack = gpGlobals->curtime + flDuration;
				m_flTimeWeaponIdle = gpGlobals->curtime + flDuration;
			}
		}

		// State resets
		m_bNeedsCocking = false;

		// Swallow the right-click button so it doesn't trigger alt-fire behaviors
		pOwner->m_nButtons &= ~IN_ATTACK2;
		return;
	}

	// If we don't need to cock the weapon, proceed with normal weapon logic
	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Reload
//-----------------------------------------------------------------------------
bool CWeaponPistolStealth::Reload(void)
{
	// Attempt to reload the weapon
	bool bSuccess = BaseClass::Reload();

	if (bSuccess)
	{
		// A fresh clip is in, so the gun doesn't need to be manually cocked for the first shot
		m_bNeedsCocking = false;

		// Reset the accuracy penalty to give them a clean first shot
		m_flAccuracyPenalty = 0.0f;
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Firing the Weapon
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistolStealth::PrimaryAttack(void)
{
	if (m_bNeedsCocking)
	{
		// Play the empty sound from the weapon script
		WeaponSound(EMPTY);

		// Add a short delay to prevent the sound from spamming every frame
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		return;
	}

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	if (pOwner)
	{
		pOwner->ViewPunchReset();
	}

	BaseClass::PrimaryAttack();

	m_bNeedsCocking = true;
	m_flNextPrimaryAttack = gpGlobals->curtime + PISTOL_FASTEST_REFIRE_TIME;
	m_flAccuracyPenalty += PISTOL_ACCURACY_SHOT_PENALTY_TIME;
}

//-----------------------------------------------------------------------------
// Spread & Accuracy Mechanics ported from weapon_pistol
//-----------------------------------------------------------------------------
Vector CWeaponPistolStealth::GetBulletSpread(WeaponProficiency_t proficiency)
{
	static Vector cone;

	if ( pistol_stealth_use_new_accuracy.GetBool() )
	{
		float ramp = RemapValClamped( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME, 0.0f, 1.0f ); 
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone ); //[cite: 3]
	}
	else
	{
		cone = VECTOR_CONE_4DEGREES;
	}

	return cone;
}

void CWeaponPistolStealth::UpdatePenaltyTime( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL ) return;

	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) ) //[cite: 3]
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME ); //[cite: 3]
	}
}

void CWeaponPistolStealth::ItemPreFrame( void )
{
	UpdatePenaltyTime(); //[cite: 3]
	BaseClass::ItemPreFrame();
}

void CWeaponPistolStealth::ItemBusyFrame( void )
{
	UpdatePenaltyTime(); //[cite: 3]
	BaseClass::ItemBusyFrame();
}

void CWeaponPistolStealth::DryFire( void )
{
	WeaponSound( EMPTY ); //[cite: 3]
	SendWeaponAnim( ACT_VM_DRYFIRE ); //[cite: 3]
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration(); //[cite: 3]
}

void CWeaponPistolStealth::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL ) return;

	QAngle viewPunch;
	viewPunch.x = random->RandomFloat( 0.25f, 0.5f ); //[cite: 3]
	viewPunch.y = random->RandomFloat( -.6f, .6f ); //[cite: 3]
	viewPunch.z = 0.0f; //[cite: 3]

	pPlayer->ViewPunch( viewPunch ); //[cite: 3]
}