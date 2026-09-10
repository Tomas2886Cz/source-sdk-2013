//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: // l4d2 medkit
//=============================================================================//

#include "cbase.h"

#include "engine/IEngineSound.h"
#include "soundstartparams.h" 
#include "soundflags.h"

#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include <prediction.h>
#include "vgui/ISurface.h" 
#include "vgui_controls/Controls.h"
#else
#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponMedkit C_WeaponMedkit
#endif

#define MEDKIT_HEAL_DURATION 5.0f // heal time

//-----------------------------------------------------------------------------
// def
//-----------------------------------------------------------------------------
class CWeaponMedkit : public CBaseHL2MPCombatWeapon
{
    DECLARE_CLASS(CWeaponMedkit, CBaseHL2MPCombatWeapon);
public:

    CWeaponMedkit(void);

    virtual void    Precache(void);
    virtual void	PrimaryAttack(void);
    virtual void	SecondaryAttack(void);
    virtual void    ItemPostFrame(void);
    virtual bool    CanDeploy(void);

#ifdef CLIENT_DLL
    virtual void    Redraw(void);
#endif

    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    void            HealTarget(CBaseCombatCharacter* pTarget, float flAmount);

#ifndef CLIENT_DLL
    DECLARE_ACTTABLE();
#endif

private:
    CNetworkVar(bool, m_bIsHealing);
    CNetworkVar(float, m_flHealStartTime);
    CNetworkHandle(CBaseEntity, m_hHealTarget);

    CWeaponMedkit(const CWeaponMedkit&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponMedkit, DT_WeaponMedkit)

BEGIN_NETWORK_TABLE(CWeaponMedkit, DT_WeaponMedkit)
#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bIsHealing)),
RecvPropTime(RECVINFO(m_flHealStartTime)),
RecvPropEHandle(RECVINFO(m_hHealTarget)),
#else
SendPropBool(SENDINFO(m_bIsHealing)),
SendPropTime(SENDINFO(m_flHealStartTime)),
SendPropEHandle(SENDINFO(m_hHealTarget)),
#endif
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
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponMedkit::CWeaponMedkit(void)
{
    m_fMinRange1 = 0;
    m_fMaxRange1 = 64;
    m_bReloadsSingly = false;
    m_bFiresUnderwater = false;

    m_bIsHealing = false;
    m_flHealStartTime = 0.0f;
    m_hHealTarget = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if the weapon can be deployed/equipped
//-----------------------------------------------------------------------------
bool CWeaponMedkit::CanDeploy(void)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return false;

    int iAmmoType = (m_iPrimaryAmmoType >= 0) ? m_iPrimaryAmmoType : GetPrimaryAmmoType();

    if (m_iClip1 <= 0 && (iAmmoType >= 0 && pOwner->GetAmmoCount(iAmmoType) <= 0))
    {
        return false;
    }

    return BaseClass::CanDeploy();
}

//-----------------------------------------------------------------------------
// Purpose: precache
//-----------------------------------------------------------------------------
void CWeaponMedkit::Precache(void)
{
    BaseClass::Precache();

#ifndef CLIENT_DLL
    PrecacheModel("models/weapons/w_medkit.mdl");
    PrecacheModel("models/items/healthkit.mdl"); // Healthkit pickup model
#endif
}

//-----------------------------------------------------------------------------
// Purpose: hud
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CWeaponMedkit::Redraw(void)
{
    BaseClass::Redraw();

    if (!m_bIsHealing)
        return;

    float flElapsed = gpGlobals->curtime - m_flHealStartTime;
    float flPercentage = clamp(flElapsed / MEDKIT_HEAL_DURATION, 0.0f, 1.0f);

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
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMedkit::PrimaryAttack(void)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner || m_bIsHealing)
        return;

    if (m_iClip1 <= 0 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
        return;

    trace_t tr;
    Vector vecSrc = pOwner->Weapon_ShootPosition();
    Vector vecAiming = pOwner->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
    Vector vecEnd = vecSrc + vecAiming * m_fMaxRange1;

    UTIL_TraceLine(vecSrc, vecEnd, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);

    CBasePlayer* pTarget = NULL;
    if (tr.m_pEnt && tr.m_pEnt->IsPlayer())
    {
        pTarget = ToBasePlayer(tr.m_pEnt);
    }

    if (pTarget && pTarget->GetHealth() < pTarget->GetMaxHealth())
    {
        m_hHealTarget = pTarget;
    }
    else if (pOwner->GetHealth() < pOwner->GetMaxHealth())
    {
        m_hHealTarget = pOwner;
    }
    else
    {
        return;
    }

    m_bIsHealing = true;
    m_flHealStartTime = gpGlobals->curtime;

    SendWeaponAnim(ACT_VM_PRIMARYATTACK);
}

void CWeaponMedkit::SecondaryAttack(void)
{
#ifndef CLIENT_DLL
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner || m_bIsHealing)
        return;

    int iAmmoType = (m_iPrimaryAmmoType >= 0) ? m_iPrimaryAmmoType : GetPrimaryAmmoType();

    // Prevent throwing if ammo count is zero
    if (iAmmoType >= 0 && pOwner->GetAmmoCount(iAmmoType) <= 0)
        return;

    Vector vecForward;
    pOwner->EyeVectors(&vecForward);
    Vector vecSpawnOrigin = pOwner->Weapon_ShootPosition() + (vecForward * 96.0f);
    QAngle vecSpawnAngles = pOwner->EyeAngles();
    Vector vecThrowVelocity = (vecForward * 400.0f) + Vector(0, 0, 200.0f);

    // Spawn standard healthkit item
    CBaseEntity* pEnt = CreateEntityByName("item_ammo_medkit");
    if (pEnt)
    {
        pEnt->SetAbsOrigin(vecSpawnOrigin);
        pEnt->SetAbsAngles(vecSpawnAngles);
        pEnt->Spawn();

        // Apply physics momentum
        IPhysicsObject* pPhysics = pEnt->VPhysicsGetObject();
        if (pPhysics)
        {
            pPhysics->Wake();
            pPhysics->AddVelocity(&vecThrowVelocity, NULL);
        }
        else
        {
            pEnt->SetAbsVelocity(vecThrowVelocity);
        }
    }

    // Deduct 1 ammo
    if (iAmmoType >= 0)
    {
        pOwner->RemoveAmmo(1, iAmmoType);
    }

    m_flNextSecondaryAttack = gpGlobals->curtime + 0.8f;

    // Switch weapon if out of ammo
    if (iAmmoType >= 0 && pOwner->GetAmmoCount(iAmmoType) <= 0)
    {
        pOwner->SwitchToNextBestWeapon(this);
    }
#else
    return;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMedkit::ItemPostFrame(void)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    if (m_bIsHealing)
    {
        pOwner->m_nButtons &= ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_DUCK);
        pOwner->SetAbsVelocity(vec3_origin);
        if (!(pOwner->m_nButtons & IN_ATTACK))
        {
            m_bIsHealing = false;
            m_hHealTarget = NULL;
            SendWeaponAnim(ACT_VM_IDLE);
            BaseClass::ItemPostFrame();
            return;
        }

        CBasePlayer* pTarget = ToBasePlayer(m_hHealTarget.Get());
        if (!pTarget || !pTarget->IsAlive())
        {
            m_bIsHealing = false;
            m_hHealTarget = NULL;
            SendWeaponAnim(ACT_VM_IDLE);
            BaseClass::ItemPostFrame();
            return;
        }

        if (pTarget != pOwner)
        {
            float flDistance = (pOwner->GetAbsOrigin() - pTarget->GetAbsOrigin()).Length();
            if (flDistance > (m_fMaxRange1 + 16.0f))
            {
                m_bIsHealing = false;
                m_hHealTarget = NULL;
                SendWeaponAnim(ACT_VM_IDLE);
                BaseClass::ItemPostFrame();
                return;
            }
        }

        if (gpGlobals->curtime - m_flHealStartTime >= MEDKIT_HEAL_DURATION)
        {
#ifndef CLIENT_DLL
            if (pTarget == pOwner)
            {
                HealTarget(pOwner, 40.0f);
            }
            else
            {
                HealTarget(pTarget, 40.0f);
            }

            int iAmmoType = (m_iPrimaryAmmoType >= 0) ? m_iPrimaryAmmoType : GetPrimaryAmmoType();
            if (iAmmoType >= 0)
            {
                pOwner->RemoveAmmo(1, iAmmoType);
            }

            if (iAmmoType >= 0 && pOwner->GetAmmoCount(iAmmoType) <= 0)
            {
                pOwner->SwitchToNextBestWeapon(this);
            }
#endif
            m_bIsHealing = false;
            m_hHealTarget = NULL;
            return;
        }
    }
    else
    {
        BaseClass::ItemPostFrame();
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponMedkit::HealTarget(CBaseCombatCharacter* pTarget, float flAmount)
{
#ifndef CLIENT_DLL
    if (!pTarget)
        return;

    int nNewHealth = MIN(pTarget->GetHealth() + flAmount, pTarget->GetMaxHealth());
    pTarget->SetHealth(nNewHealth);

    CPASAttenuationFilter filter(pTarget);
    EmitSound(filter, pTarget->entindex(), "HealthKit.Touch");
#else
    return;
#endif
}