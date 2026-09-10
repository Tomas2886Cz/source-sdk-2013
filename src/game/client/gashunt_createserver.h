#pragma once
#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/CheckButton.h>

struct GashuntCampaignUI_t
{
    char szInternalName[64];
    char szUIName[64];
    char szDescription[256];
    char szThumbnail[128];
    char szFirstMap[64];
};

struct GashuntCVarUI_t
{
    char szCVarName[64];
    char szType[32];
    vgui::Panel* pControl;
};

class CGashuntCreateServerDialog : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CGashuntCreateServerDialog, vgui::Frame);
public:
    CGashuntCreateServerDialog(vgui::VPANEL parent);

    virtual void OnCommand(const char* command) OVERRIDE;
    virtual void PerformLayout() OVERRIDE;

private:
    void LoadCampaigns();
    void LoadOptions();
    void UpdateCampaignUI();
    void StartServer();

    vgui::PropertySheet* m_pSheet;
    vgui::PropertyPage* m_pServerPage;
    vgui::PropertyPage* m_pGamePage;
    vgui::PanelListPanel* m_pOptionsList;

    // Server Tab Elements
    vgui::ImagePanel* m_pThumbnail;
    vgui::Label* m_pCampaignNameLabel;
    vgui::Label* m_pCampaignDescLabel;
    vgui::Button* m_pPrevButton;
    vgui::Button* m_pNextButton;

    // Bottom Buttons
    vgui::Button* m_pStartButton;
    vgui::Button* m_pCancelButton;

    CUtlVector<GashuntCampaignUI_t> m_Campaigns;
    CUtlVector<GashuntCVarUI_t> m_CVars;
    int m_iCurrentCampaign;
};