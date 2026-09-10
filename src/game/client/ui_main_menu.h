#pragma once
#include "cbase.h"
#if defined( CLIENT_DLL )
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Button.h>

class CUIMainMenu : public vgui::EditablePanel
{
    DECLARE_CLASS_SIMPLE(CUIMainMenu, vgui::EditablePanel);

public:
    CUIMainMenu(vgui::VPANEL parent);
    virtual ~CUIMainMenu();

    virtual void PerformLayout() OVERRIDE;

private:
    vgui::Button* m_pCenteredButton;
    int m_iButtonWidth;
    int m_iButtonHeight;
    int m_iButtonSpacing;
    char m_szAlignment[32];
    int m_iXOffset;
    int m_iYOffset;
};

#endif // CLIENT_DLL