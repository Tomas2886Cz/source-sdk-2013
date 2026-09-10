#pragma once

#include "tier1/KeyValues.h"
#include "tier1/utlvector.h"
#include "filesystem.h"

struct PlayerModelInfo_t
{
    char szPath[256];
    char szDisplayName[128];
    int nTeam;
    char szWeaponModel[256];
    char szAnimName[64];
};

inline CUtlVector<PlayerModelInfo_t>& GetPlayerModels()
{
    static CUtlVector<PlayerModelInfo_t> s_Models;
    static bool s_bLoaded = false;

    if (!s_bLoaded)
    {
        s_bLoaded = true;

        KeyValues* pKV = new KeyValues("PlayerModels");
        // Use g_pFullFileSystem instead of filesystem on the client side
        if (g_pFullFileSystem && pKV->LoadFromFile(g_pFullFileSystem, "scripts/ui_playermodelselector.txt", "MOD"))
        {
            for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
            {
                PlayerModelInfo_t info;
                Q_strncpy(info.szPath, pKey->GetString("model", ""), sizeof(info.szPath));
                Q_strncpy(info.szDisplayName, pKey->GetString("name", "Unknown"), sizeof(info.szDisplayName));
                info.nTeam = pKey->GetInt("team", 2);
                Q_strncpy(info.szWeaponModel, pKey->GetString("weapon", ""), sizeof(info.szWeaponModel));
                Q_strncpy(info.szAnimName, pKey->GetString("animation", "idle_subtle"), sizeof(info.szAnimName));

                if (Q_strlen(info.szPath) > 0)
                {
                    s_Models.AddToTail(info);
                }
            }
        }
        pKV->deleteThis();

        // Fallback defaults if scripts/playermodels.txt is missing or empty
        if (s_Models.Count() == 0)
        {
            PlayerModelInfo_t defaults[10] = {
                { "models/humans/group03/male_01.mdl", "Rebel Male 1", 2, "models/weapons/w_smg1.mdl", "idle_subtle" },
                { "models/humans/group03/male_02.mdl", "Rebel Male 2", 2, "models/weapons/w_shotgun.mdl", "idle_subtle" },
                { "models/humans/group03/female_01.mdl", "Rebel Female 1", 2, "models/weapons/w_irifle.mdl", "idle_subtle" },
                { "models/humans/group03/female_02.mdl", "Rebel Female 2", 2, "models/weapons/w_pistol.mdl", "idle_subtle" },
                { "models/humans/group03/male_03.mdl", "Rebel Male 3", 2, "models/weapons/w_crossbow.mdl", "idle_subtle" },
                { "models/combine_soldier.mdl", "Combine Soldier", 3, "models/weapons/w_irifle.mdl", "idle1" },
                { "models/combine_soldier_prisonguard.mdl", "Prison Guard", 3, "models/weapons/w_shotgun.mdl", "idle1" },
                { "models/combine_super_soldier.mdl", "Elite Soldier", 3, "models/weapons/w_irifle.mdl", "idle1" },
                { "models/police.mdl", "Metrocop", 3, "models/weapons/w_pistol.mdl", "idle" },
                { "models/combine_soldier.mdl", "Shotgunner", 3, "models/weapons/w_shotgun.mdl", "idle1" }
            };

            for (int i = 0; i < 10; i++)
            {
                s_Models.AddToTail(defaults[i]);
            }
        }
    }

    return s_Models;
}