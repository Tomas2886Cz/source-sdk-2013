#include "cbase.h"
#include "props.h"

#ifndef SF_PHYSPROP_OVERRIDE
#define SF_PHYSPROP_OVERRIDE 1
#endif

#define GASCAN_MODEL "models/gamemode_gashunt/item_gashunt_gascan.mdl"

class CPropGasCan : public CPhysicsProp
{
public:
    DECLARE_CLASS(CPropGasCan, CPhysicsProp);
    DECLARE_DATADESC();

    void Spawn(void) OVERRIDE;
    void Precache(void) OVERRIDE;
};

LINK_ENTITY_TO_CLASS(item_gashunt_gascan, CPropGasCan);

BEGIN_DATADESC(CPropGasCan)
END_DATADESC()

void CPropGasCan::Precache(void)
{
    SetModelName(AllocPooledString(GASCAN_MODEL));
    PrecacheModel(GASCAN_MODEL);
    BaseClass::Precache();
}

void CPropGasCan::Spawn(void)
{
    AddSpawnFlags(SF_PHYSPROP_OVERRIDE);

    Precache();
    SetModel(GASCAN_MODEL);

    BaseClass::Spawn();

    SetSolid(SOLID_VPHYSICS);
    SetMoveType(MOVETYPE_VPHYSICS);

    m_iHealth = 20;
    m_takedamage = DAMAGE_YES;

    CreateVPhysics();
}