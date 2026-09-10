/*
#include "cbase.h"
#include "entity_outline_system.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "iviewrender.h"
#include "model_types.h"

static IMaterial* g_pFlatColorMaterial = nullptr;

void InitEntityGlowMaterial()
{
    if (!g_pFlatColorMaterial)
    {
        KeyValues* pKV = new KeyValues("VertexLitGeneric");
        pKV->SetString("$basetexture", "vgui/white");
        pKV->SetInt("$ignorez", 1); // Draw through walls
        pKV->SetInt("$model", 1);   // Tells the material it is attached to 3D geometry
        pKV->SetInt("$selfillum", 1); // Forces the material to be fully bright (emulating UnlitGeneric)

        g_pFlatColorMaterial = materials->CreateMaterial("__ent_glow_mat", pKV);
    }
}

void ShutdownEntityGlowMaterial()
{
    if (g_pFlatColorMaterial)
    {
        g_pFlatColorMaterial->DecrementReferenceCount();
        g_pFlatColorMaterial = nullptr;
    }
}

void RenderEntityOutlines()
{
    InitEntityGlowMaterial();
    if (!g_pFlatColorMaterial)
        return;

    C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
    if (!pLocalPlayer)
        return;

    C_BaseEntity* pEntity = NULL;
    for (CEntitySphereQuery sphere(pLocalPlayer->GetAbsOrigin(), 4000.0f); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
    {
        if (!pEntity || pEntity == pLocalPlayer)
            continue;

        OutlineConfig_t config;
        if (!g_EntityOutlineManager.GetConfigForClass(pEntity->GetClassname(), config))
            continue;

        float flDist = (pEntity->GetAbsOrigin() - pLocalPlayer->GetAbsOrigin()).Length();
        if (flDist > config.flMaxDistance)
            continue;

        // Apply material override and draw the model silhouette
        modelrender->ForcedMaterialOverride(g_pFlatColorMaterial);

        float color[3] = {
            config.glowColor.r() / 255.0f,
            config.glowColor.g() / 255.0f,
            config.glowColor.b() / 255.0f
        };
        render->SetColorModulation(color);
        render->SetBlend(config.glowColor.a() / 255.0f);

        // Pass 1 (which equals STUDIO_RENDER). STUDIO_NONE (0) causes the model to become invisible.
        pEntity->DrawModel(1);
    }

    // Reset material override after rendering outlines to prevent corrupting the rest of the game
    modelrender->ForcedMaterialOverride(NULL);
    float defaultColor[3] = { 1.0f, 1.0f, 1.0f };
    render->SetColorModulation(defaultColor);
    render->SetBlend(1.0f);
}
*/