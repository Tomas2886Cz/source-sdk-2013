#include "cbase.h"
#include "c_physicsprop.h"
#include "dlight.h"
#include "iefx.h"

class C_PropGasCan : public C_PhysicsProp
{
public:
    DECLARE_CLASS(C_PropGasCan, C_PhysicsProp);

    virtual void OnDataChanged(DataUpdateType_t updateType) OVERRIDE;
    virtual void ClientThink(void) OVERRIDE;
};

LINK_ENTITY_TO_CLASS(item_gashunt_gascan, C_PropGasCan);

void C_PropGasCan::OnDataChanged(DataUpdateType_t updateType)
{
    BaseClass::OnDataChanged(updateType);

    if (updateType == DATA_UPDATE_CREATED)
    {
        SetNextClientThink(CLIENT_THINK_ALWAYS);
    }
}

void C_PropGasCan::ClientThink(void)
{
    BaseClass::ClientThink();

    if (effects)
    {
        // Use effects pointer and lowercase 'l' in CL_AllocDlight
        dlight_t* pLight = effects->CL_AllocDlight(entindex());
        if (pLight)
        {
            pLight->origin = GetAbsOrigin();

            // Orange RGB Glow
            pLight->color.r = 255;
            pLight->color.g = 120;
            pLight->color.b = 0;
            pLight->color.exponent = 5;

            pLight->radius = 150.0f;
            pLight->decay = 0.0f;

            // Keep light alive through the next frame
            pLight->die = gpGlobals->curtime + 0.05f;
        }
    }
}