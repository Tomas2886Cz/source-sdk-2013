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
#define CWeaponMedkit C_WeaponMedkit
#endif

//-----------------------------------------------------------------------------
// Medkit
//-----------------------------------------------------------------------------

class CWeaponMedkit : public CBaseHL2MPCombatWeapon
{
    DECLARE_CLASS(CWeaponMedkit, CBaseHL2MPCombatWeapon);
public:

    CWeaponMedkit(void);

    void	PrimaryAttack(void);
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
    DECLARE_ACTTABLE();
#endif

private:

    CWeaponMedkit(const CWeaponMedkit&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponMedkit, DT_WeaponMedkit)

BEGIN_NETWORK_TABLE(CWeaponMedkit, DT_WeaponMedkit)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CWeaponMedkit)
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_medkit, CWeaponMedkit);
PRECACHE_WEAPON_REGISTER(weapon_medkit);

#ifndef CLIENT_DLL
acttable_t CWeaponMedkit::m_acttable[] =
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

IMPLEMENT_ACTTABLE(CWeaponMedkit);

#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponMedkit::CWeaponMedkit(void)
{
    m_bReloadsSingly = false;
    m_bFiresUnderwater = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponMedkit::PrimaryAttack(void)
{
    CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer) return;

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
            m_flNextPrimaryAttack = 0.15;
        }

        return;
    }

    // spawn item_healthkit
    CBaseEntity* pKit = CreateEntityByName("item_healthkit");
    if (pKit)
    {
        Vector vecOrigin = pPlayer->Weapon_ShootPosition();
        QAngle vecAngles = pPlayer->EyeAngles();

        pKit->SetAbsOrigin(vecOrigin);
        pKit->SetAbsAngles(vecAngles);
        pKit->Spawn();

        m_iClip1--;

        // drop force
        Vector vecForward;
        AngleVectors(vecAngles, &vecForward);
        IPhysicsObject* pPhys = pKit->VPhysicsGetObject();
        if (pPhys)
        {
            Vector vecVel = vecForward * 400;
            pPhys->SetVelocity(&vecVel, NULL);
        }
    }
    this->m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f; // Cooldown
};