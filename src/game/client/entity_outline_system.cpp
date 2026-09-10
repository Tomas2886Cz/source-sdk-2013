#include "cbase.h"
#include "entity_outline_system.h"
#include "filesystem.h"
#include "tier0/memdbgon.h"

#ifndef GLOWS_ENABLE
#define GLOWS_ENABLE 1
#endif

#include "icliententitylist.h"
#include "glow_outline_effect.h" 

CEntityOutlineManager g_EntityOutlineManager("CEntityOutlineManager");

void CEntityOutlineManager::Shutdown()
{
    ClearAllGlows();
}

void CEntityOutlineManager::LevelInitPreEntity()
{
    LoadConfig();
}

void CEntityOutlineManager::LevelShutdownPostEntity()
{
    ClearAllGlows();
    m_OutlineSettings.Purge();
}

void CEntityOutlineManager::LoadConfig()
{
    m_OutlineSettings.Purge();

    KeyValues* pKV = new KeyValues("EntityOutlines");
    if (pKV->LoadFromFile(filesystem, "scripts/entity_outlines.txt", "MOD"))
    {
        for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
        {
            const char* pszClassname = pKey->GetName();
            const char* pszColorStr = pKey->GetString("color", "255 255 255 255");
            float flDistance = pKey->GetFloat("distance", 1000.0f);

            int r = 255, g = 255, b = 255, a = 255;
            sscanf(pszColorStr, "%d %d %d %d", &r, &g, &b, &a);

            OutlineConfig_t config;
            config.glowColor = Color(r, g, b, a);
            config.flMaxDistance = flDistance;

            m_OutlineSettings.Insert(pszClassname, config);
        }
    }
    pKV->deleteThis();
}

bool CEntityOutlineManager::GetConfigForClass(const char* pszClassname, OutlineConfig_t& config)
{
    int idx = m_OutlineSettings.Find(pszClassname);
    if (idx != m_OutlineSettings.InvalidIndex())
    {
        config = m_OutlineSettings[idx];
        return true;
    }
    return false;
}

void CEntityOutlineManager::ClearAllGlows()
{
    for (int i = 0; i < m_ActiveGlows.Count(); i++)
    {
        g_GlowObjectManager.UnregisterGlowObject(m_ActiveGlows[i].nGlowHandle);
    }
    m_ActiveGlows.Purge();
}

// Runs every frame to manage dynamic outlines
void CEntityOutlineManager::Update(float frametime)
{
    C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
    if (!pLocalPlayer) return;

    // 1. Clean up invalid, dormant, or out-of-range glows
    for (int i = m_ActiveGlows.Count() - 1; i >= 0; i--)
    {
        C_BaseEntity* pEnt = m_ActiveGlows[i].hEntity.Get();
        bool bShouldRemove = false;

        if (!pEnt || pEnt->IsDormant())
        {
            bShouldRemove = true;
        }
        else
        {
            OutlineConfig_t config;
            const char* pszNetName = pEnt->GetClientClass() ? pEnt->GetClientClass()->GetName() : "";

            if (GetConfigForClass(pszNetName, config) || GetConfigForClass(pEnt->GetClassname(), config))
            {
                float flDist = (pEnt->GetAbsOrigin() - pLocalPlayer->GetAbsOrigin()).Length();
                if (flDist > config.flMaxDistance)
                    bShouldRemove = true;
            }
            else
            {
                // Config was removed or fallback testing expired
                if (!Q_stristr(pszNetName, "Physics"))
                    bShouldRemove = true;
            }
        }

        if (bShouldRemove)
        {
            g_GlowObjectManager.UnregisterGlowObject(m_ActiveGlows[i].nGlowHandle);
            m_ActiveGlows.Remove(i);
        }
    }

    // 2. Iterate ALL client entities reliably (bypasses spatial partition issues)
    int nHighestIndex = ClientEntityList().GetHighestEntityIndex();
    for (int i = 0; i <= nHighestIndex; i++)
    {
        C_BaseEntity* pEntity = ClientEntityList().GetBaseEntity(i);
        if (!pEntity || pEntity == pLocalPlayer || pEntity->IsDormant())
            continue;

        const char* pszNetName = pEntity->GetClientClass() ? pEntity->GetClientClass()->GetName() : "";
        const char* pszClassname = pEntity->GetClassname();

        bool bAlreadyGlowing = false;
        for (int j = 0; j < m_ActiveGlows.Count(); j++)
        {
            if (m_ActiveGlows[j].hEntity.Get() == pEntity)
            {
                bAlreadyGlowing = true;
                break;
            }
        }

        if (bAlreadyGlowing)
            continue;

        OutlineConfig_t config;

        // --- FOOLPROOF TEST ---
        // If the entity is a physics prop, force it to glow RED unconditionally.
        if (Q_stristr(pszNetName, "Physics"))
        {
            config.glowColor = Color(255, 0, 0, 255);
            config.flMaxDistance = 2000.0f;
        }
        // Otherwise, attempt to read from your config file
        else if (!GetConfigForClass(pszNetName, config) && !GetConfigForClass(pszClassname, config))
        {
            continue;
        }

        float flDist = (pEntity->GetAbsOrigin() - pLocalPlayer->GetAbsOrigin()).Length();
        if (flDist <= config.flMaxDistance)
        {
            Vector vColor(config.glowColor.r() / 255.0f, config.glowColor.g() / 255.0f, config.glowColor.b() / 255.0f);
            float flAlpha = config.glowColor.a() / 255.0f;

            int handle = g_GlowObjectManager.RegisterGlowObject(pEntity, vColor, flAlpha, true, true, -1);

            GlowTarget_t target;
            target.hEntity = pEntity;
            target.nGlowHandle = handle;
            m_ActiveGlows.AddToTail(target);

            // Print a message directly to the developer console so we know the code works!
            Msg("[GLOW SYSTEM] Registered glow for entity: %s\n", pszNetName);
        }
    }
}