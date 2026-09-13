#ifndef BACKPACK_PANEL_H
#define BACKPACK_PANEL_H
#pragma once

#include <vgui_controls/Frame.h>
#include "weapon_hl2mpbasehlmpcombatweapon.h"

struct BackpackItem_t
{
    char szClassname[64];
    char szPrintName[64];
    int iAmmo;
    int iAmmoType;
    CHudTexture* pIcon;
    bool bIsBlueprint;
    int iEntityIndex;
};

class CBackpackPanel : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CBackpackPanel, vgui::Frame);

public:
    CBackpackPanel(vgui::VPANEL parent);
    virtual ~CBackpackPanel();

    virtual void SetVisible(bool state) OVERRIDE;
    virtual void PerformLayout() OVERRIDE;
    virtual void PaintBackground() OVERRIDE;
    virtual void Paint() OVERRIDE;
    virtual void OnClose() OVERRIDE;
    virtual void OnMousePressed(vgui::MouseCode code) OVERRIDE;
    virtual void OnKeyCodePressed(vgui::KeyCode code) OVERRIDE;
    virtual void OnMouseWheeled(int delta) OVERRIDE;
    virtual void OnThink() OVERRIDE;

    void PopulateInventoryLists();

private:
    CUtlVector<BackpackItem_t> m_WeaponItems;
    CUtlVector<BackpackItem_t> m_BlueprintItems;
    int m_iScrollOffset;
    int m_nBackgroundTextureID;
};

#endif // BACKPACK_PANEL_H