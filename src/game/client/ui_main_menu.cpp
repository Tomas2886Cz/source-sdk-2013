#include "cbase.h"

#if defined( CLIENT_DLL )

#include "ui_main_menu.h"
#include <vgui/ISurface.h>
#include <KeyValues.h>
#include "filesystem.h"

// Define the ConVar to enable/disable the custom menu overlay (defaults to enabled: "1")
static ConVar cl_custom_menu("cl_custom_menu", "1", FCVAR_ARCHIVE, "Enable or disable custom centered main menu overlay (1 = Custom, 0 = Default).");

CUIMainMenu::CUIMainMenu(vgui::VPANEL parent) : BaseClass(NULL, "UIMainMenu")
{
    SetParent(parent);
    SetScheme("ClientScheme");

    int wide, tall;
    vgui::surface()->GetScreenSize(wide, tall);
    SetSize(wide, tall);

    m_iButtonWidth = 200;
    m_iButtonHeight = 30;
    m_iButtonSpacing = 8;
    Q_strncpy(m_szAlignment, "center", sizeof(m_szAlignment));
    m_iXOffset = 0;
    m_iYOffset = 0;

    KeyValues* pSettings = new KeyValues("UIMainMenu");
    if (pSettings->LoadFromFile(g_pFullFileSystem, "resource/UI/uimainmenu.res"))
    {
        m_iButtonWidth = pSettings->GetInt("button_width", 200);
        m_iButtonHeight = pSettings->GetInt("button_height", 30);
        m_iButtonSpacing = pSettings->GetInt("button_spacing", 8);

        const char* pszAlign = pSettings->GetString("alignment", "center");
        Q_strncpy(m_szAlignment, pszAlign, sizeof(m_szAlignment));

        m_iXOffset = pSettings->GetInt("x_offset", 0);
        m_iYOffset = pSettings->GetInt("y_offset", 0);
    }
    pSettings->deleteThis();

    m_pCenteredButton = new vgui::Button(this, "CenteredButton", "HL2DM Custom Button");
    m_pCenteredButton->SetSize(m_iButtonWidth, m_iButtonHeight);
}

CUIMainMenu::~CUIMainMenu()
{
}

void CUIMainMenu::PerformLayout()
{
    BaseClass::PerformLayout();

    // If the ConVar is set to 0, bypass custom layout and hide the panel elements
    if (!cl_custom_menu.GetBool())
    {
        SetVisible(false);
        return;
    }

    SetVisible(true);

    int parentWide, parentTall;
    GetSize(parentWide, parentTall);

    if (m_pCenteredButton)
    {
        int totalButtons = 1;
        int totalBlockHeight = (totalButtons * m_iButtonHeight) + ((totalButtons - 1) * m_iButtonSpacing);
        int startY = ((parentTall - totalBlockHeight) / 2) + m_iYOffset;

        int xPos = 0;
        if (Q_stricmp(m_szAlignment, "left") == 0)
        {
            xPos = m_iXOffset;
        }
        else if (Q_stricmp(m_szAlignment, "right") == 0)
        {
            xPos = parentWide - m_iButtonWidth - m_iXOffset;
        }
        else
        {
            xPos = ((parentWide - m_iButtonWidth) / 2) + m_iXOffset;
        }

        m_pCenteredButton->SetSize(m_iButtonWidth, m_iButtonHeight);
        m_pCenteredButton->SetPos(xPos, startY);
    }
}

#endif // CLIENT_DLL