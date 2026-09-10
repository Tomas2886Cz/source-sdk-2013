#include "cbase.h"
#include "filesystem.h"
#include <KeyValues.h>

#if defined( GAME_DLL )

// --- Server-Specific Includes ---
#include "gashunt_manager.h"
#include "hl2mp_player.h"
#include "ammodef.h"

// ConVars to track state across map loads
ConVar gashunt_active_slot("gashunt_active_slot", "-1", FCVAR_NONE, "Currently active campaign slot (-1 = off)");
ConVar gashunt_campaign_name("gashunt_campaign_name", "main_campaign", FCVAR_NONE, "Name of the campaign definition to use");

CGashuntManager g_GashuntManager;
CGashuntManager* g_pGashuntManager = &g_GashuntManager;

CGashuntManager::CGashuntManager() : CAutoGameSystem("GashuntManager")
{
    m_pActiveSaveData = NULL;
    m_pCampaignDef = NULL;
}

void CGashuntManager::LevelShutdownPreEntity()
{
    if (m_pActiveSaveData)
    {
        m_pActiveSaveData->deleteThis();
        m_pActiveSaveData = NULL;
    }
    if (m_pCampaignDef)
    {
        m_pCampaignDef->deleteThis();
        m_pCampaignDef = NULL;
    }
}

void CGashuntManager::LevelInitPostEntity()
{
    if (gashunt_active_slot.GetInt() != -1)
    {
        LoadProgress();
    }
}

void CGashuntManager::LoadProgress()
{
    int slot = gashunt_active_slot.GetInt();
    if (slot == -1) return;

    // Load Campaign Definitions
    m_pCampaignDef = new KeyValues("GashuntCampaigns");
    if (!m_pCampaignDef->LoadFromFile(filesystem, "scripts/gashunt_campaigns.txt", "MOD"))
    {
        Warning("GASHUNT: Could not load scripts/gashunt_campaigns.txt\n");
    }

    // Load Active Save
    char szSavePath[128];
    Q_snprintf(szSavePath, sizeof(szSavePath), "save/gashunt_slot%d.sav", slot);

    m_pActiveSaveData = new KeyValues("GashuntSave");
    if (m_pActiveSaveData->LoadFromFile(filesystem, szSavePath, "MOD"))
    {
        // Restore persistent map entities
        KeyValues* kvEnts = m_pActiveSaveData->FindKey("entity_data");
        if (kvEnts)
        {
            for (KeyValues* pKey = kvEnts->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
            {
                CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, pKey->GetName());
                if (pEnt && FClassnameIs(pEnt, "gashunt_persistentdata"))
                {
                    int restoredVal = pKey->GetInt();
                    variant_t variant;
                    variant.SetInt(restoredVal);
                    pEnt->AcceptInput("SetValue", NULL, NULL, variant, 0);

                    // Fire the OnRestored output for map logic
                    pEnt->KeyValue("m_OnRestored", "1"); // Trigger it
                }
            }
        }
    }
}

void CGashuntManager::SaveProgress(const char* szNextMap)
{
    int slot = gashunt_active_slot.GetInt();
    if (slot == -1 || !m_pCampaignDef) return;

    KeyValues* kvSave = new KeyValues("GashuntSave");
    kvSave->SetString("campaign", gashunt_campaign_name.GetString());
    kvSave->SetString("current_map", szNextMap);

    // 1. Save All Connected Players
    KeyValues* kvPlayers = new KeyValues("players");
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CHL2MP_Player* pPlayer = (CHL2MP_Player*)UTIL_PlayerByIndex(i);
        if (pPlayer && pPlayer->IsConnected())
        {
            // Use SteamID or LAN ID as the unique key
            KeyValues* kvPlayer = new KeyValues(pPlayer->GetNetworkIDString());
            kvPlayer->SetInt("health", pPlayer->GetHealth());
            kvPlayer->SetInt("armor", pPlayer->ArmorValue());
            kvPlayer->SetInt("score", pPlayer->FragCount());
            kvPlayer->SetInt("deaths", pPlayer->DeathCount());

            // Dump Weapons
            KeyValues* kvWeapons = new KeyValues("weapons");
            for (int w = 0; w < MAX_WEAPONS; w++)
            {
                CBaseCombatWeapon* pWeapon = pPlayer->GetWeapon(w);
                if (pWeapon)
                {
                    KeyValues* kvWep = new KeyValues("weapon");
                    kvWep->SetString("classname", pWeapon->GetClassname());
                    kvWep->SetInt("clip1", pWeapon->m_iClip1);
                    kvWep->SetInt("clip2", pWeapon->m_iClip2);
                    kvWeapons->AddSubKey(kvWep);
                }
            }
            kvPlayer->AddSubKey(kvWeapons);

            // Dump Backpack Ammo
            KeyValues* kvAmmo = new KeyValues("ammo");
            CAmmoDef* pAmmoDef = GetAmmoDef();
            for (int a = 0; a < MAX_AMMO_SLOTS; a++)
            {
                int count = pPlayer->GetAmmoCount(a);
                if (count > 0 && pAmmoDef->GetAmmoOfIndex(a))
                {
                    kvAmmo->SetInt(pAmmoDef->GetAmmoOfIndex(a)->pName, count);
                }
            }
            kvPlayer->AddSubKey(kvAmmo);
            kvPlayers->AddSubKey(kvPlayer);
        }
    }
    kvSave->AddSubKey(kvPlayers);

    // 2. Save Map Entities
    KeyValues* pDef = m_pCampaignDef->FindKey(gashunt_campaign_name.GetString());
    if (pDef)
    {
        const char* pszPersist = pDef->GetString("persist_entities", "");
        if (pszPersist[0] != '\0')
        {
            KeyValues* kvEnts = new KeyValues("entity_data");
            CUtlStringList vecNames;
            V_SplitString(pszPersist, ",", vecNames);

            for (int i = 0; i < vecNames.Count(); i++)
            {
                CBaseEntity* pEnt = gEntList.FindEntityByName(NULL, vecNames[i]);
                if (pEnt && FClassnameIs(pEnt, "gashunt_persistentdata"))
                {
                    // Assuming value can be fetched (we'll ensure the entity class supports it)
                    int val = pEnt->GetHealth(); // Using m_iHealth temporarily to store var state cleanly
                    kvEnts->SetInt(vecNames[i], val);
                }
            }
            kvSave->AddSubKey(kvEnts);
        }
    }

    char szSavePath[128];
    Q_snprintf(szSavePath, sizeof(szSavePath), "save/gashunt_slot%d.sav", slot);
    filesystem->CreateDirHierarchy("save", "MOD");
    kvSave->SaveToFile(filesystem, szSavePath, "MOD");
    kvSave->deleteThis();
}

void CGashuntManager::ApplyPlayerStats(CBasePlayer* pPlayer)
{
    if (!m_pActiveSaveData) return;

    KeyValues* kvPlayers = m_pActiveSaveData->FindKey("players");
    if (!kvPlayers) return;

    // Find this specific player's data by their SteamID
    KeyValues* kvPlayer = kvPlayers->FindKey(pPlayer->GetNetworkIDString());
    if (!kvPlayer) return;

    // Apply Stats
    pPlayer->SetHealth(kvPlayer->GetInt("health", 100));
    pPlayer->SetArmorValue(kvPlayer->GetInt("armor", 0));
    pPlayer->ResetFragCount();
    pPlayer->AddPoints(kvPlayer->GetInt("score", 0), false);
    pPlayer->SetDeathCount(kvPlayer->GetInt("deaths", 0));

    pPlayer->RemoveAllItems(false); // Remove default spawn gear

    // Restore Weapons
    KeyValues* kvWeapons = kvPlayer->FindKey("weapons");
    if (kvWeapons)
    {
        for (KeyValues* kvWep = kvWeapons->GetFirstSubKey(); kvWep; kvWep = kvWep->GetNextKey())
        {
            const char* szClass = kvWep->GetString("classname");
            CBaseCombatWeapon* pSpawnedWep = (CBaseCombatWeapon*)pPlayer->GiveNamedItem(szClass);
            if (pSpawnedWep)
            {
                pSpawnedWep->m_iClip1 = kvWep->GetInt("clip1", -1);
                pSpawnedWep->m_iClip2 = kvWep->GetInt("clip2", -1);
            }
        }
    }

    // Restore Ammo
    pPlayer->RemoveAllAmmo(); // Clear default ammo granted by GiveNamedItem
    KeyValues* kvAmmo = kvPlayer->FindKey("ammo");
    if (kvAmmo)
    {
        CAmmoDef* pAmmoDef = GetAmmoDef();
        for (KeyValues* kvA = kvAmmo->GetFirstSubKey(); kvA; kvA = kvA->GetNextKey())
        {
            int ammoIndex = pAmmoDef->Index(kvA->GetName());
            if (ammoIndex != -1) pPlayer->SetAmmoCount(kvA->GetInt(), ammoIndex);
        }
    }
}

// Console Commands for Map Logic & UI
CON_COMMAND(gashunt_start_campaign, "Starts a new campaign: gashunt_start_campaign <slot> <campaign_name> <first_map>")
{
    if (args.ArgC() < 4) return;
    int slot = Q_atoi(args[1]);
    gashunt_active_slot.SetValue(slot);
    gashunt_campaign_name.SetValue(args[2]);

    // Wipe old save file
    char szSavePath[128];
    Q_snprintf(szSavePath, sizeof(szSavePath), "save/gashunt_slot%d.sav", slot);
    filesystem->RemoveFile(szSavePath, "MOD");

    // Tell clients to reset character locks
    UTIL_ClientPrintAll(HUD_PRINTCONSOLE, "gashunt_reset_character\n");

    char cmd[128];
    Q_snprintf(cmd, sizeof(cmd), "map %s", args[3]);
    engine->ServerCommand(cmd);
}

CON_COMMAND(gashunt_save_and_transition, "Saves progress and changes level: gashunt_save_and_transition <next_map>")
{
    if (args.ArgC() < 2) return;
    if (gashunt_active_slot.GetInt() != -1)
    {
        g_pGashuntManager->SaveProgress(args[1]);
    }
    char cmd[128];
    Q_snprintf(cmd, sizeof(cmd), "changelevel %s", args[1]);
    engine->ServerCommand(cmd);
}

CON_COMMAND(gashunt_resume_campaign, "Resumes a slot: gashunt_resume_campaign <slot>")
{
    if (args.ArgC() < 2) return;
    int slot = Q_atoi(args[1]);

    char szSavePath[128];
    Q_snprintf(szSavePath, sizeof(szSavePath), "save/gashunt_slot%d.sav", slot);

    KeyValues* kvLoad = new KeyValues("GashuntSave");
    if (kvLoad->LoadFromFile(filesystem, szSavePath, "MOD"))
    {
        gashunt_active_slot.SetValue(slot);
        const char* szMap = kvLoad->GetString("current_map");
        char cmd[128];
        Q_snprintf(cmd, sizeof(cmd), "map %s", szMap);
        engine->ServerCommand(cmd);
    }
    kvLoad->deleteThis();
}
#endif // GAME_DLL

// ==============================================================================
// CLIENT SIDE: MAIN MENU SWAPPER LOGIC
// ==============================================================================
#if defined( CLIENT_DLL )

// Create dummy variables on the client so the engine doesn't reject them before the server boots
static ConVar gashunt_active_slot("gashunt_active_slot", "-1", FCVAR_NONE);
static ConVar gashunt_campaign_name("gashunt_campaign_name", "main_campaign", FCVAR_NONE);

static ConVar cl_gashunt_selected_campaign("cl_gashunt_selected_campaign", "1", FCVAR_ARCHIVE, "Currently selected Gashunt campaign (1-3)");

CON_COMMAND(gashunt_cycle_campaign, "Swaps between 3 Gashunt campaigns")
{
    int next = cl_gashunt_selected_campaign.GetInt() + 1;
    if (next > 3) next = 1;
    cl_gashunt_selected_campaign.SetValue(next);

    // Play a UI sound so the player knows the button worked
    engine->ClientCmd("play ui/buttonclick.wav\n");
    Msg("\n>>> SWAPPED TO GASHUNT CAMPAIGN SLOT %d <<<\n\n", next);
}

CON_COMMAND(gashunt_start_selected, "Starts the currently selected campaign")
{
    int sel = cl_gashunt_selected_campaign.GetInt();
    char cmd[256];

    // Define your 3 campaigns and their starting maps here:
    if (sel == 1)
    {
        Q_snprintf(cmd, sizeof(cmd), "gashunt_active_slot 1; gashunt_campaign_name main_campaign; map hl2dm_map1");
    }
    else if (sel == 2)
    {
        Q_snprintf(cmd, sizeof(cmd), "gashunt_active_slot 2; gashunt_campaign_name second_campaign; map hl2dm_map4");
    }
    else if (sel == 3)
    {
        Q_snprintf(cmd, sizeof(cmd), "gashunt_active_slot 3; gashunt_campaign_name third_campaign; map hl2dm_map7");
    }

    // Wipe old save file locally to ensure a clean start
    char szSavePath[128];
    Q_snprintf(szSavePath, sizeof(szSavePath), "save/gashunt_slot%d.sav", sel);
    g_pFullFileSystem->RemoveFile(szSavePath, "MOD");

    // Clear the character lock so the player can pick a new team/model
    engine->ClientCmd("gashunt_reset_character");

    // Launch the server
    engine->ClientCmd(cmd);
}

CON_COMMAND(gashunt_resume_selected, "Resumes the currently selected campaign")
{
    int sel = cl_gashunt_selected_campaign.GetInt();
    char szSavePath[128];
    Q_snprintf(szSavePath, sizeof(szSavePath), "save/gashunt_slot%d.sav", sel);

    KeyValues* kvLoad = new KeyValues("GashuntSave");
    if (kvLoad->LoadFromFile(g_pFullFileSystem, szSavePath, "MOD"))
    {
        const char* szMap = kvLoad->GetString("current_map", "hl2dm_map1");
        const char* szCamp = kvLoad->GetString("campaign", "main_campaign");

        char cmd[256];
        Q_snprintf(cmd, sizeof(cmd), "gashunt_active_slot %d; gashunt_campaign_name %s; map %s", sel, szCamp, szMap);
        engine->ClientCmd(cmd);
    }
    else
    {
        engine->ClientCmd("play common/wpn_denyselect.wav\n");
        Msg("\n[GASHUNT] No save file found for Campaign Slot %d!\n\n", sel);
    }
    kvLoad->deleteThis();
}

#endif // CLIENT_DLL