#include "cbase.h"
#include "gashunt_createserver.h"
#include "filesystem.h"
#include <KeyValues.h>

CGashuntCreateServerDialog::CGashuntCreateServerDialog(vgui::VPANEL parent) : BaseClass(NULL, "GashuntCreateServer")
{
    SetParent(parent);
    SetTitle("CREATE SERVER", true);
    SetSize(520, 580); // <--- Expanded overall window size
    SetSizeable(false);
    MoveToCenterOfScreen();
    MakePopup();

    m_iCurrentCampaign = 0;

    m_pSheet = new vgui::PropertySheet(this, "PropertySheet");
    m_pSheet->SetBounds(10, 30, 500, 500); // <--- Expanded sheet size

    // --- PAGE 1: SERVER (Loaded via .res file) ---
    m_pServerPage = new vgui::PropertyPage(m_pSheet, "ServerPage");

    m_pThumbnail = new vgui::ImagePanel(m_pServerPage, "Thumbnail");
    m_pCampaignNameLabel = new vgui::Label(m_pServerPage, "CampName", "");
    m_pCampaignDescLabel = new vgui::Label(m_pServerPage, "CampDesc", "");
    m_pPrevButton = new vgui::Button(m_pServerPage, "Prev", "<");
    m_pNextButton = new vgui::Button(m_pServerPage, "Next", ">");

    m_pServerPage->LoadControlSettings("Resource/UI/GashuntCreateServer.res");

    // Force buttons to target this dialog directly so clicks register
    m_pPrevButton->AddActionSignalTarget(this);
    m_pPrevButton->SetCommand("PrevCamp");
    m_pNextButton->AddActionSignalTarget(this);
    m_pNextButton->SetCommand("NextCamp");

    m_pSheet->AddPage(m_pServerPage, "Server");

    // --- PAGE 2: GAME (Modular Options) ---
    m_pGamePage = new vgui::PropertyPage(m_pSheet, "GamePage");
    m_pOptionsList = new vgui::PanelListPanel(m_pGamePage, "OptionsList");
    m_pOptionsList->SetBounds(10, 10, 480, 440); // <--- Wider options list

    m_pSheet->AddPage(m_pGamePage, "Game");

    // --- BOTTOM BUTTONS (Pushed to bottom-right of new 520x580 window) ---
    m_pStartButton = new vgui::Button(this, "StartBtn", "Start", this, "StartServer");
    m_pStartButton->SetBounds(330, 540, 80, 24);

    m_pCancelButton = new vgui::Button(this, "CancelBtn", "Cancel", this, "Cancel");
    m_pCancelButton->SetBounds(420, 540, 80, 24);

    LoadCampaigns();
    LoadOptions();
    UpdateCampaignUI();
}

void CGashuntCreateServerDialog::PerformLayout()
{
    BaseClass::PerformLayout();
    if (m_pSheet) m_pSheet->SetBounds(10, 30, 500, 500);

    // This line ensures the Game Tab's options list still scales correctly
    if (m_pOptionsList) m_pOptionsList->SetBounds(10, 10, 480, 440);
}

void CGashuntCreateServerDialog::LoadCampaigns()
{
    KeyValues* pKV = new KeyValues("GashuntCampaigns");
    if (pKV->LoadFromFile(g_pFullFileSystem, "scripts/gashunt_campaigns.txt", "MOD"))
    {
        for (KeyValues* pSub = pKV->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey())
        {
            GashuntCampaignUI_t camp;
            Q_strncpy(camp.szInternalName, pSub->GetName(), sizeof(camp.szInternalName));
            Q_strncpy(camp.szUIName, pSub->GetString("ui_name", "Unknown"), sizeof(camp.szUIName));
            Q_strncpy(camp.szDescription, pSub->GetString("ui_description", "No description available."), sizeof(camp.szDescription));
            Q_strncpy(camp.szThumbnail, pSub->GetString("ui_thumbnail", "vgui/hud/icon_locked"), sizeof(camp.szThumbnail));
            Q_strncpy(camp.szFirstMap, pSub->GetString("first_map", "hl2dm_map1"), sizeof(camp.szFirstMap));
            m_Campaigns.AddToTail(camp);
        }
    }
    pKV->deleteThis();
}

void CGashuntCreateServerDialog::LoadOptions()
{
    // Grab the larger scheme font
    vgui::HScheme scheme = vgui::scheme()->GetScheme("ClientScheme");
    vgui::HFont hLargeFont = vgui::scheme()->GetIScheme(scheme)->GetFont("DefaultLarge", false);

    KeyValues* pKV = new KeyValues("ServerOptions");
    if (pKV->LoadFromFile(g_pFullFileSystem, "scripts/gashunt_server_options.txt", "MOD"))
    {
        for (KeyValues* pSub = pKV->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey())
        {
            const char* szType = pSub->GetString("type", "bool");
            const char* szLabel = pSub->GetString("label", pSub->GetName());
            const char* szDef = pSub->GetString("default", "");

            vgui::Panel* pRow = new vgui::Panel(m_pOptionsList, "Row");
            pRow->SetSize(460, 32); // Slightly taller row to fit larger text cleanly

            GashuntCVarUI_t cvar;
            Q_strncpy(cvar.szCVarName, pSub->GetName(), sizeof(cvar.szCVarName));
            Q_strncpy(cvar.szType, szType, sizeof(cvar.szType));

            if (Q_stricmp(szType, "bool") == 0)
            {
                vgui::CheckButton* pCheck = new vgui::CheckButton(pRow, "Check", szLabel);
                pCheck->SetBounds(80, 2, 300, 28);
                pCheck->SetFont(hLargeFont); // Apply large font
                pCheck->SetSelected(Q_atoi(szDef) != 0);
                cvar.pControl = pCheck;
            }
            else
            {
                vgui::Label* pLbl = new vgui::Label(pRow, "Lbl", szLabel);
                pLbl->SetBounds(50, 2, 160, 28);
                pLbl->SetFont(hLargeFont); // Apply large font
                pLbl->SetContentAlignment(vgui::Label::a_west);

                vgui::TextEntry* pText = new vgui::TextEntry(pRow, "Text");
                pText->SetBounds(220, 2, 200, 26);
                pText->SetFont(hLargeFont); // Apply large font
                pText->SetText(szDef);
                cvar.pControl = pText;
            }

            m_CVars.AddToTail(cvar);
            m_pOptionsList->AddItem(NULL, pRow);
        }
    }
    pKV->deleteThis();
}

void CGashuntCreateServerDialog::UpdateCampaignUI()
{
    if (m_Campaigns.Count() == 0) return;
    m_pCampaignNameLabel->SetText(m_Campaigns[m_iCurrentCampaign].szUIName);
    m_pCampaignDescLabel->SetText(m_Campaigns[m_iCurrentCampaign].szDescription);
    m_pThumbnail->SetImage(m_Campaigns[m_iCurrentCampaign].szThumbnail);
}

void CGashuntCreateServerDialog::StartServer()
{
    // Apply all modular CVars
    for (int i = 0; i < m_CVars.Count(); i++)
    {
        if (Q_stricmp(m_CVars[i].szType, "bool") == 0)
        {
            vgui::CheckButton* pCheck = (vgui::CheckButton*)m_CVars[i].pControl;
            engine->ClientCmd_Unrestricted(VarArgs("%s %d\n", m_CVars[i].szCVarName, pCheck->IsSelected() ? 1 : 0));
        }
        else
        {
            vgui::TextEntry* pText = (vgui::TextEntry*)m_CVars[i].pControl;
            char szBuf[256];
            pText->GetText(szBuf, sizeof(szBuf));
            engine->ClientCmd_Unrestricted(VarArgs("%s \"%s\"\n", m_CVars[i].szCVarName, szBuf));
        }
    }

    if (m_Campaigns.Count() > 0)
    {
        // Execute the campaign launch command natively
        engine->ClientCmd_Unrestricted(VarArgs("gashunt_active_slot 1; gashunt_campaign_name %s; map %s\n",
            m_Campaigns[m_iCurrentCampaign].szInternalName,
            m_Campaigns[m_iCurrentCampaign].szFirstMap));

        engine->ClientCmd_Unrestricted("gashunt_reset_character\n");
    }

    Close();
}

void CGashuntCreateServerDialog::OnCommand(const char* command)
{
    if (Q_stricmp(command, "NextCamp") == 0)
    {
        m_iCurrentCampaign++;
        if (m_iCurrentCampaign >= m_Campaigns.Count()) m_iCurrentCampaign = 0;
        UpdateCampaignUI();
    }
    else if (Q_stricmp(command, "PrevCamp") == 0)
    {
        m_iCurrentCampaign--;
        if (m_iCurrentCampaign < 0) m_iCurrentCampaign = m_Campaigns.Count() - 1;
        UpdateCampaignUI();
    }
    else if (Q_stricmp(command, "Cancel") == 0)
    {
        Close();
    }
    else if (Q_stricmp(command, "StartServer") == 0)
    {
        StartServer();
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

// ---------------------------------------------------------
// Global command to open the window from GameMenu.res
// ---------------------------------------------------------
static CGashuntCreateServerDialog* g_pCreateServer = NULL;

CON_COMMAND(gashunt_open_createserver, "Opens the Gashunt Create Server UI")
{
    if (g_pCreateServer)
    {
        g_pCreateServer->MarkForDeletion();
    }
    g_pCreateServer = new CGashuntCreateServerDialog(NULL);
    g_pCreateServer->Activate();
}