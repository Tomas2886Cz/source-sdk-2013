#pragma once

#include "cbase.h"
#include "tier1/keyvalues.h"
#include "igamesystem.h"

struct OutlineConfig_t
{
    Color glowColor;
    float flMaxDistance;
};

// Tracks entities that are currently glowing
struct GlowTarget_t
{
    EHANDLE hEntity;
    int nGlowHandle;
};

// Inherit from CAutoGameSystemPerFrame to tick every frame
class CEntityOutlineManager : public CAutoGameSystemPerFrame
{
public:
    CEntityOutlineManager(char const* pName) : CAutoGameSystemPerFrame(pName) {}

    virtual bool Init() { return true; }
    virtual void Shutdown();
    virtual void LevelInitPreEntity();
    virtual void LevelShutdownPostEntity();
    virtual void Update(float frametime); // Our new per-frame loop

    bool GetConfigForClass(const char* pszClassname, OutlineConfig_t& config);

private:
    void LoadConfig();
    void ClearAllGlows();

    CUtlDict< OutlineConfig_t, int > m_OutlineSettings;
    CUtlVector< GlowTarget_t > m_ActiveGlows;
};

extern CEntityOutlineManager g_EntityOutlineManager;