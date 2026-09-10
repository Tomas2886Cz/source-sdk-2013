#include "cbase.h"
#include "c_baseanimating.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"         
#include "engine/ivmodelinfo.h"  
#include "igamesystem.h"         
#include <vgui_controls/Frame.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include "hl2mp_playermodels.h"
#include "model_types.h"
#include "usermessages.h"

class CTeamJoinMenu;
class CModelSelectionMenu;

CTeamJoinMenu* g_pTeamJoinMenu = NULL;
CModelSelectionMenu* g_pModelMenu = NULL;

static ConVar cl_bypass_team_menu("cl_bypass_team_menu", "0", FCVAR_ARCHIVE, "Bypass team and model selection menu on map load for testing");

// ---------------------------------------------------------------------------------
// 3D Model Preview Panel
// ---------------------------------------------------------------------------------
class CModelPreviewPanel : public vgui::EditablePanel
{
    DECLARE_CLASS_SIMPLE(CModelPreviewPanel, vgui::EditablePanel);
public:
    CModelPreviewPanel(vgui::Panel* parent, const char* name, int modelIndex);
    ~CModelPreviewPanel();

    virtual void Paint();
    virtual void OnTick();
    virtual void OnMousePressed(vgui::MouseCode code);
    virtual void OnCursorEntered();
    virtual void OnCursorExited();
    virtual void PerformLayout();

    void UpdateCount(int count);

private:
    C_BaseAnimating* m_pModel;
    C_BaseAnimating* m_pWeapon;
    vgui::Label* m_pNameLabel;
    vgui::Label* m_pCountLabel;
    int m_nModelIndex;
    bool m_bIsHovered;
};

// ---------------------------------------------------------------------------------
// Team Selection Menu
// ---------------------------------------------------------------------------------
class CTeamJoinMenu : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CTeamJoinMenu, vgui::Frame);
public:
    CTeamJoinMenu();
    virtual void Paint();
    virtual void OnCommand(const char* command);

private:
    vgui::Button* m_pRebelsButton;
    vgui::Button* m_pCombineButton;
    vgui::Button* m_pSpectateButton;
};

// ---------------------------------------------------------------------------------
// Character Selection Menu
// ---------------------------------------------------------------------------------
class CModelSelectionMenu : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CModelSelectionMenu, vgui::Frame);
public:
    CModelSelectionMenu(int teamNum);
    void UpdateCounts(const CUtlVector<byte>& counts);
    virtual void Paint();
    virtual void OnCommand(const char* command);

private:
    CUtlVector<CModelPreviewPanel*> m_pPreviews;
    vgui::Button* m_pBackButton;
    int m_nTeamNum;
};

// ---------------------------------------------------------------------------------
// Game System Trigger
// ---------------------------------------------------------------------------------
class CModelMenuSystem : public CAutoGameSystem
{
public:
    CModelMenuSystem() : CAutoGameSystem("CModelMenuSystem") {}

    virtual void LevelInitPostEntity()
    {
        if (cl_bypass_team_menu.GetBool())
            return;

        if (!g_pTeamJoinMenu)
        {
            g_pTeamJoinMenu = new CTeamJoinMenu();
        }
        g_pTeamJoinMenu->SetVisible(true);
    }
};

static CModelMenuSystem s_ModelMenuSystem;

// ---------------------------------------------------------------------------------
// CModelPreviewPanel Implementation
// ---------------------------------------------------------------------------------
CModelPreviewPanel::CModelPreviewPanel(vgui::Panel* parent, const char* name, int modelIndex) : BaseClass(parent, name)
{
    m_nModelIndex = modelIndex;
    m_bIsHovered = false;

    LoadControlSettings("Resource/UI/ModelPreviewItem.res");

    m_pNameLabel = dynamic_cast<vgui::Label*>(FindChildByName("NameLabel"));
    if (m_pNameLabel)
    {
        m_pNameLabel->SetText(GetPlayerModels()[modelIndex].szDisplayName);
    }

    m_pCountLabel = dynamic_cast<vgui::Label*>(FindChildByName("CountLabel"));
    if (m_pCountLabel)
    {
        m_pCountLabel->SetText("Active: 0");
    }

    m_pModel = new C_BaseAnimating();
    if (m_pModel->InitializeAsClientEntity(GetPlayerModels()[modelIndex].szPath, RENDER_GROUP_OPAQUE_ENTITY))
    {
        m_pModel->AddEffects(EF_NODRAW);

        int seq = m_pModel->LookupSequence(GetPlayerModels()[modelIndex].szAnimName);
        if (seq < 0) seq = m_pModel->LookupSequence("idle_subtle");
        if (seq < 0) seq = m_pModel->LookupSequence("idle1");

        m_pModel->SetSequence(seq >= 0 ? seq : 0);
        m_pModel->SetCycle(0.0f);
    }

    m_pWeapon = new C_BaseAnimating();
    if (m_pWeapon->InitializeAsClientEntity(GetPlayerModels()[modelIndex].szWeaponModel, RENDER_GROUP_OPAQUE_ENTITY))
    {
        m_pWeapon->AddEffects(EF_NODRAW);
        int attachIndex = m_pModel->LookupAttachment("anim_attachment_RH");
        if (attachIndex > 0)
        {
            m_pWeapon->SetParent(m_pModel, attachIndex);
            m_pWeapon->SetLocalOrigin(vec3_origin);
            m_pWeapon->SetLocalAngles(vec3_angle);
        }
    }

    vgui::ivgui()->AddTickSignal(GetVPanel());
}

CModelPreviewPanel::~CModelPreviewPanel()
{
    if (m_pWeapon) m_pWeapon->Release();
    if (m_pModel) m_pModel->Release();
}

void CModelPreviewPanel::PerformLayout()
{
    BaseClass::PerformLayout();
}

void CModelPreviewPanel::UpdateCount(int count)
{
    if (m_pCountLabel) m_pCountLabel->SetText(VarArgs("Active: %d", count));
}

void CModelPreviewPanel::OnCursorEntered() { m_bIsHovered = true; }
void CModelPreviewPanel::OnCursorExited()
{
    m_bIsHovered = false;
    if (m_pModel) m_pModel->SetCycle(0.0f);
}

void CModelPreviewPanel::OnTick()
{
    BaseClass::OnTick();
    if (m_pModel)
    {
        if (m_bIsHovered)
            m_pModel->FrameAdvance(gpGlobals->frametime);
        else
            m_pModel->SetCycle(0.0f);
    }
}

void CModelPreviewPanel::Paint()
{
    BaseClass::Paint();
    if (!m_pModel) return;

    int x, y, w, h;
    GetBounds(x, y, w, h);
    vgui::VPANEL pPanel = GetVPanel();
    vgui::ipanel()->GetAbsPos(pPanel, x, y);

    CMatRenderContextPtr pRenderContext(materials);

    CViewSetup view;
    memset(&view, 0, sizeof(view));
    view.x = x;
    view.y = y;
    view.width = w;
    view.height = h - 45;
    view.fov = 32.0f;
    view.origin = Vector(0, 0, 36);
    view.angles = vec3_angle;
    view.zNear = 1.0f;
    view.zFar = 2000.0f;

    VPlane frustum[FRUSTUM_NUMPLANES];
    render->Push3DView(view, VIEW_CLEAR_DEPTH, NULL, frustum);

    m_pModel->SetAbsOrigin(Vector(54, 0, -4));
    m_pModel->SetAbsAngles(QAngle(0, 180, 0));
    m_pModel->InvalidateBoneCache();

    IMaterial* pShadowMat = materials->FindMaterial("vgui/model_shadow", TEXTURE_GROUP_OTHER);
    if (pShadowMat && !pShadowMat->IsErrorMaterial())
    {
        pRenderContext->Bind(pShadowMat);
        IMesh* pMesh = pRenderContext->GetDynamicMesh(true);
        CMeshBuilder meshBuilder;
        meshBuilder.Begin(pMesh, MATERIAL_QUADS, 1);

        float radius = 30.0f;
        float zPos = -3.9f;
        float centerX = 54.0f;
        float centerY = 0.0f;

        meshBuilder.Position3f(centerX - radius, centerY - radius, zPos);
        meshBuilder.TexCoord2f(0, 0.0f, 0.0f);
        meshBuilder.Color4f(0.0f, 0.0f, 0.0f, 0.7f);
        meshBuilder.AdvanceVertex();

        meshBuilder.Position3f(centerX + radius, centerY - radius, zPos);
        meshBuilder.TexCoord2f(0, 1.0f, 0.0f);
        meshBuilder.Color4f(0.0f, 0.0f, 0.0f, 0.7f);
        meshBuilder.AdvanceVertex();

        meshBuilder.Position3f(centerX + radius, centerY + radius, zPos);
        meshBuilder.TexCoord2f(0, 1.0f, 1.0f);
        meshBuilder.Color4f(0.0f, 0.0f, 0.0f, 0.7f);
        meshBuilder.AdvanceVertex();

        meshBuilder.Position3f(centerX - radius, centerY + radius, zPos);
        meshBuilder.TexCoord2f(0, 0.0f, 1.0f);
        meshBuilder.Color4f(0.0f, 0.0f, 0.0f, 0.7f);
        meshBuilder.AdvanceVertex();

        meshBuilder.End();
        pMesh->Draw();
    }

    static Vector brightWhite[6] = {
        Vector(2.3, 2.3, 2.3), Vector(2.3, 2.3, 2.3),
        Vector(2.3, 2.3, 2.3), Vector(2.3, 2.3, 2.3),
        Vector(2.3, 2.3, 2.3), Vector(2.3, 2.3, 2.3)
    };
    g_pStudioRender->SetAmbientLightColors(brightWhite);
    g_pStudioRender->SetLocalLights(0, NULL);

    m_pModel->DrawModel(STUDIO_RENDER);
    if (m_pWeapon) m_pWeapon->DrawModel(STUDIO_RENDER);

    render->PopView(frustum);
}

void CModelPreviewPanel::OnMousePressed(vgui::MouseCode code)
{
    engine->ClientCmd(VarArgs("select_playermodel %d", m_nModelIndex));
    if (g_pModelMenu) g_pModelMenu->SetVisible(false);
}

// ---------------------------------------------------------------------------------
// CTeamJoinMenu Implementation
// ---------------------------------------------------------------------------------
CTeamJoinMenu::CTeamJoinMenu() : BaseClass(NULL, "TeamJoinMenu")
{
    int screenW, screenH;
    vgui::surface()->GetScreenSize(screenW, screenH);
    SetSize(screenW, screenH);
    SetPos(0, 0);

    SetSizeable(false);
    SetCloseButtonVisible(false);
    MakePopup();

    LoadControlSettings("Resource/UI/TeamJoinMenu.res");
}

void CTeamJoinMenu::Paint()
{
    BaseClass::Paint();
    int w, h;
    GetSize(w, h);

    static int nTeamBgId = -1;
    if (nTeamBgId == -1)
    {
        nTeamBgId = vgui::surface()->CreateNewTextureID();
        vgui::surface()->DrawSetTextureFile(nTeamBgId, "vgui/ui_playermodelselector_bgr_team", true, false);
    }

    vgui::surface()->DrawSetColor(255, 255, 255, 255);
    vgui::surface()->DrawSetTexture(nTeamBgId);
    vgui::surface()->DrawTexturedRect(0, 0, w, h);
}

void CTeamJoinMenu::OnCommand(const char* command)
{
    if (Q_stricmp(command, "join_rebels") == 0)
    {
        engine->ClientCmd("jointeam 2");
        SetVisible(false);
        if (g_pModelMenu) { g_pModelMenu->MarkForDeletion(); g_pModelMenu = NULL; }
        g_pModelMenu = new CModelSelectionMenu(2);
        g_pModelMenu->SetVisible(true);
    }
    else if (Q_stricmp(command, "join_combine") == 0)
    {
        engine->ClientCmd("jointeam 3");
        SetVisible(false);
        if (g_pModelMenu) { g_pModelMenu->MarkForDeletion(); g_pModelMenu = NULL; }
        g_pModelMenu = new CModelSelectionMenu(3);
        g_pModelMenu->SetVisible(true);
    }
    else if (Q_stricmp(command, "join_spectate") == 0)
    {
        engine->ClientCmd("jointeam 1");
        SetVisible(false);
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

// ---------------------------------------------------------------------------------
// CModelSelectionMenu Implementation
// ---------------------------------------------------------------------------------
CModelSelectionMenu::CModelSelectionMenu(int teamNum) : BaseClass(NULL, "ModelSelectionMenu"), m_nTeamNum(teamNum)
{
    int screenW, screenH;
    vgui::surface()->GetScreenSize(screenW, screenH);
    SetSize(screenW, screenH);
    SetPos(0, 0);

    SetSizeable(false);
    SetCloseButtonVisible(false);
    MakePopup();

    LoadControlSettings("Resource/UI/ModelSelectionMenu.res");

    vgui::Label* pHeaderLabel = dynamic_cast<vgui::Label*>(FindChildByName("HeaderLabel"));
    if (pHeaderLabel)
    {
        pHeaderLabel->SetText((teamNum == 2) ? "SELECT REBEL CHARACTER" : "SELECT COMBINE CHARACTER");
    }

    CUtlVector<PlayerModelInfo_t>& allModels = GetPlayerModels();
    CUtlVector<int> filteredIndices;

    for (int i = 0; i < allModels.Count(); i++)
    {
        if (allModels[i].nTeam == teamNum)
        {
            filteredIndices.AddToTail(i);
        }
    }

    int numModels = filteredIndices.Count();
    m_pPreviews.SetCount(numModels);

    for (int i = 0; i < numModels; i++)
    {
        m_pPreviews[i] = new CModelPreviewPanel(this, VarArgs("Preview%d", i), filteredIndices[i]);
    }

    int numRows = (numModels + 4) / 5;
    int panelBaseW = 310;
    int panelBaseH = screenH - 120;
    int colGap = 4;
    int rowGap = 6;

    int totalGridH = 0;
    CUtlVector<int> rowHeights;
    rowHeights.SetCount(numRows);

    for (int r = 0; r < numRows; r++)
    {
        int countInRow = (r == numRows - 1) ? (numModels - (r * 5)) : 5;
        float rowMiddleIndex = (countInRow - 1) / 2.0f;
        int rowMaxH = 0;

        for (int c = 0; c < countInRow; c++)
        {
            float distFromCenter = abs(c - rowMiddleIndex);
            float scale = 1.0f - (distFromCenter * 0.03f);
            int h = (int)(panelBaseH * scale);
            if (h > rowMaxH) rowMaxH = h;
        }

        rowHeights[r] = rowMaxH;
        totalGridH += rowMaxH;
        if (r < numRows - 1) totalGridH += rowGap;
    }

    int startY = (screenH - totalGridH) / 2 + 10;

    for (int r = 0; r < numRows; r++)
    {
        int countInRow = (r == numRows - 1) ? (numModels - (r * 5)) : 5;
        float rowMiddleIndex = (countInRow - 1) / 2.0f;

        int rowTotalW = 0;
        CUtlVector<int> rowWidths;
        rowWidths.SetCount(countInRow);

        for (int c = 0; c < countInRow; c++)
        {
            float distFromCenter = abs(c - rowMiddleIndex);
            float scale = 1.0f - (distFromCenter * 0.03f);
            int w = (int)(panelBaseW * scale);
            rowWidths[c] = w;
            rowTotalW += w;
            if (c < countInRow - 1) rowTotalW += colGap;
        }

        int currentX = (screenW - rowTotalW) / 2;

        for (int c = 0; c < countInRow; c++)
        {
            int modelIdx = (r * 5) + c;
            float distFromCenter = abs(c - rowMiddleIndex);
            float scale = 1.0f - (distFromCenter * 0.03f);
            int w = rowWidths[c];
            int h = (int)(panelBaseH * scale);

            m_pPreviews[modelIdx]->SetBounds(currentX, startY, w, h);
            m_pPreviews[modelIdx]->SetVisible(true);
            m_pPreviews[modelIdx]->SetEnabled(true);

            currentX += w + colGap;
        }

        startY += rowHeights[r] + rowGap;
    }
}

void CModelSelectionMenu::Paint()
{
    BaseClass::Paint();
    int w, h;
    GetSize(w, h);

    static int nGreenBgId = -1;
    static int nYellowBgId = -1;

    int textureId = -1;
    if (m_nTeamNum == 2)
    {
        if (nGreenBgId == -1)
        {
            nGreenBgId = vgui::surface()->CreateNewTextureID();
            vgui::surface()->DrawSetTextureFile(nGreenBgId, "vgui/ui_playermodelselector_bgr_green", true, false);
        }
        textureId = nGreenBgId;
    }
    else
    {
        if (nYellowBgId == -1)
        {
            nYellowBgId = vgui::surface()->CreateNewTextureID();
            vgui::surface()->DrawSetTextureFile(nYellowBgId, "vgui/ui_playermodelselector_bgr_yellow", true, false);
        }
        textureId = nYellowBgId;
    }

    vgui::surface()->DrawSetColor(255, 255, 255, 255);
    vgui::surface()->DrawSetTexture(textureId);
    vgui::surface()->DrawTexturedRect(0, 0, w, h);
}

void CModelSelectionMenu::UpdateCounts(const CUtlVector<byte>& counts) {}

void CModelSelectionMenu::OnCommand(const char* command)
{
    if (Q_stricmp(command, "back_to_teams") == 0)
    {
        SetVisible(false);
        if (g_pTeamJoinMenu)
        {
            g_pTeamJoinMenu->SetVisible(true);
        }
        else
        {
            g_pTeamJoinMenu = new CTeamJoinMenu();
            g_pTeamJoinMenu->SetVisible(true);
        }
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

// ---------------------------------------------------------------------------------
// Hooks & Commands
// ---------------------------------------------------------------------------------
CON_COMMAND(show_team_menu, "Opens the team selection menu")
{
    if (!g_pTeamJoinMenu)
    {
        g_pTeamJoinMenu = new CTeamJoinMenu();
    }
    g_pTeamJoinMenu->SetVisible(true);
}

CON_COMMAND(show_model_menu, "Opens the model selection menu")
{
    int teamArg = (args.ArgC() > 1) ? Q_atoi(args[1]) : 2;
    if (g_pModelMenu)
    {
        g_pModelMenu->MarkForDeletion();
        g_pModelMenu = NULL;
    }
    g_pModelMenu = new CModelSelectionMenu(teamArg);
    g_pModelMenu->SetVisible(true);
}