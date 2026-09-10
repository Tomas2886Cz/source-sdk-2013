//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Blueprint item that channels to construct/grant a weapon_medkit.
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
#define CWeaponBlueprintMedkit C_WeaponBlueprintMedkit
#endif

#define MEDKIT_HEAL_DURATION 5.0f // channel time

// Replicated ConVar for ammo cost requirement
ConVar sk_blueprint_medkit_cost("sk_blueprint_medkit_cost", "250", FCVAR_REPLICATED | FCVAR_NOTIFY, "Ammo required to construct a medkit using the blueprint.");

//-----------------------------------------------------------------------------
// def
//-----------------------------------------------------------------------------
class CWeaponBlueprintMedkit : public CBaseHL2MPCombatWeapon
{
    DECLARE_CLASS(CWeaponBlueprintMedkit, CBaseHL2MPCombatWeapon);
public:

    CWeaponBlueprintMedkit(void);

    virtual void    Precache(void);
    virtual void	PrimaryAttack(void);
    virtual void    ItemPostFrame(void);
    virtual bool    IsDropAllowed(void) { return false; } // Prevents weapon drop

#ifdef CLIENT_DLL
    virtual void    Redraw(void);
#endif

    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
    DECLARE_ACTTABLE();
#endif

private:
    CNetworkVar(bool, m_bIsHealing);
    CNetworkVar(float, m_flHealStartTime);
    CNetworkHandle(CBaseEntity, m_hHealTarget);

    CWeaponBlueprintMedkit(const CWeaponBlueprintMedkit&);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponBlueprintMedkit, DT_WeaponBlueprintMedkit)

BEGIN_NETWORK_TABLE(CWeaponBlueprintMedkit, DT_WeaponBlueprintMedkit)
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

BEGIN_PREDICTION_DATA(CWeaponBlueprintMedkit)
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_blueprint_medkit, CWeaponBlueprintMedkit);
PRECACHE_WEAPON_REGISTER(weapon_blueprint_medkit);

#ifndef CLIENT_DLL
acttable_t CWeaponBlueprintMedkit::m_acttable[] =
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

IMPLEMENT_ACTTABLE(CWeaponBlueprintMedkit);
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponBlueprintMedkit::CWeaponBlueprintMedkit(void)
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
// Purpose: precache
//-----------------------------------------------------------------------------
void CWeaponBlueprintMedkit::Precache(void)
{
    BaseClass::Precache();

#ifndef CLIENT_DLL
    PrecacheModel("models/weapons/w_medkit.mdl");
#endif
}

//-----------------------------------------------------------------------------
// Purpose: hud
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CWeaponBlueprintMedkit::Redraw(void)
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
void CWeaponBlueprintMedkit::PrimaryAttack(void)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner || m_bIsHealing)
        return;

    int iAmmoType = m_iPrimaryAmmoType;
    if (iAmmoType < 0)
    {
        iAmmoType = GetPrimaryAmmoType();
    }

    int nCost = sk_blueprint_medkit_cost.GetInt();

#ifndef CLIENT_DLL
    // Server-side ammo verification and HUD notification
    if (iAmmoType >= 0 && pOwner->GetAmmoCount(iAmmoType) < nCost)
    {
        // Display a HUD message in the center of the player's screen
        ClientPrint(pOwner, HUD_PRINTCENTER, UTIL_VarArgs("Not enough Medical Scrap. Need %d total to craft a Medkit.", nCost));

        // Alternative bottom-left chat message (uncomment if preferred):
        // ClientPrint(pOwner, HUD_PRINTTALK, UTIL_VarArgs("Not enough ammo! Requires %d ammo.", nCost));

        return;
    }
#else
    // Client-side prediction check
    if (iAmmoType >= 0 && pOwner->GetAmmoCount(iAmmoType) < nCost)
    {
        return;
    }
#endif

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

    if (pTarget)
    {
        m_hHealTarget = pTarget;
    }
    else
    {
        m_hHealTarget = pOwner;
    }

    m_bIsHealing = true;
    m_flHealStartTime = gpGlobals->curtime;

    SendWeaponAnim(ACT_VM_PRIMARYATTACK);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBlueprintMedkit::ItemPostFrame(void)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    if (m_bIsHealing)
    {
        // Enforce movement lock during channeling
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
            int nCost = sk_blueprint_medkit_cost.GetInt();
            int iAmmoType = (m_iPrimaryAmmoType >= 0) ? m_iPrimaryAmmoType : GetPrimaryAmmoType();

            // Deduct ammo if valid
            if (iAmmoType >= 0 && pOwner->GetAmmoCount(iAmmoType) >= nCost)
            {
                pOwner->RemoveAmmo(nCost, iAmmoType);
            }

            // Create and grant weapon_medkit
            CBaseEntity* pEnt = CreateEntityByName("item_ammo_medkit");
            if (pEnt)
            {
                pEnt->SetAbsOrigin(pOwner->GetAbsOrigin());
                pEnt->Spawn();

                CBaseCombatWeapon* pWeapon = dynamic_cast<CBaseCombatWeapon*>(pEnt);
                if (pWeapon)
                {
                    pOwner->Weapon_Equip(pWeapon);
                }
            }

            CPASAttenuationFilter filter(pOwner);
            EmitSound(filter, pOwner->entindex(), "HealthKit.Touch");
#endif
            m_bIsHealing = false;
            m_hHealTarget = NULL;
            SendWeaponAnim(ACT_VM_IDLE);
            return;
        }
    }
    else
    {
        BaseClass::ItemPostFrame();
    }
}