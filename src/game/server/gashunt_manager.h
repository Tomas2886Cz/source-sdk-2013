#pragma once
#include "cbase.h"

#if defined( GAME_DLL )

class CGashuntManager : public CAutoGameSystem
{
public:
    CGashuntManager();
    virtual void LevelInitPostEntity() OVERRIDE;
    virtual void LevelShutdownPreEntity() OVERRIDE;

    void SaveProgress(const char* szNextMap);
    void LoadProgress();
    void ApplyPlayerStats(CBasePlayer* pPlayer);
    void ResetSlot(int slot);

    KeyValues* m_pActiveSaveData;
    KeyValues* m_pCampaignDef;
};

extern CGashuntManager* g_pGashuntManager;

#endif // GAME_DLL