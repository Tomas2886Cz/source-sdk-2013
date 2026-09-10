#pragma once

//TOMBERT MOD PLAYERMODEL SELEKTOR

struct PlayerModelInfo_t
{
    const char *szPath;
    const char *szDisplayName;
    int nTeam; 
};

// 10 Models total: First 5 are Rebels (Team 2), Last 5 are Combine (Team 3)
const PlayerModelInfo_t g_PlayerModels[10] = {
    { "models/humans/group03/male_01.mdl", "Rebel Male 1", 2 },
    { "models/humans/group03/male_02.mdl", "Rebel Male 2", 2 },
    { "models/humans/group03/male_03.mdl", "Rebel Female 1", 2 },
    { "models/humans/group03/male_04.mdl", "Rebel Female 2", 2 },
    { "models/humans/group03/male_05.mdl", "Rebel Male 3", 2 },

    { "models/humans/group03/male_06.mdl", "Combine Soldier", 3 },
    { "models/humans/group03/male_07.mdl", "Prison Guard", 3 },
    { "models/humans/group03/male_08.mdl", "Elite Soldier", 3 },
    { "models/humans/group03/male_09.mdl", "Metrocop", 3 },
    { "models/humans/group03/female_01.mdl", "Shotgunner", 3 }
};