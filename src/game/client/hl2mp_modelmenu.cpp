#include "cbase.h"
#include "c_baseanimating.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"         // FIX: For CViewSetup and VIEW_CLEAR_DEPTH
#include "ienginevgui.h"         // FIX: For enginevgui and PANEL_CLIENTDLL
#include "engine/ivmodelinfo.h"  // FIX: For STUDIO_RENDER
#include "igamesystem.h"         // FIX: For our custom message hook
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui/IVGui.h>
#include "hl2mp_playermodels.h"
#include "model_types.h"
#include "usermessages.h"

// ---------------------------------------------------------------------------------
// The 3D Render Panel
// ---------------------------------------------------------------------------------
class CModelPreviewPanel : public vgui::Panel
{
    DECLARE_CLASS_SIMPLE(CModelPreviewPanel, vgui::Panel);
public:
    CModelPreviewPanel(vgui::Panel* parent, const char* name, int modelIndex);
    ~CModelPreviewPanel();

    virtual void Paint();
    virtual void OnTick();
    virtual void OnMousePressed(vgui::MouseCode code);

    void UpdateCount(int count);

private:
    C_BaseAnimating* m_pModel;
    vgui::Label* m_pNameLabel;
    vgui::Label* m_pCountLabel;
    int m_nModelIndex;
};

CModelPreviewPanel::CModelPreviewPanel(vgui::Panel* parent, const char* name, int modelIndex) : BaseClass(parent, name)
{
    m_nModelIndex = modelIndex;
    SetSize(120, 200);
    vgui::ivgui()->AddTickSignal(GetVPanel());

    m_pNameLabel = new vgui::Label(this, "NameLabel", g_PlayerModels[modelIndex].szDisplayName);
    m_pNameLabel->SetPos(0, 160);
    m_pNameLabel->SetSize(120, 20);
    m_pNameLabel->SetContentAlignment(vgui::Label::a_center);

    m_pCountLabel = new vgui::Label(this, "CountLabel", "Active: 0");
    m_pCountLabel->SetPos(0, 180);
    m_pCountLabel->SetSize(120, 20);
    m_pCountLabel->SetContentAlignment(vgui::Label::a_center);

    m_pModel = new C_BaseAnimating();
    if (m_pModel->InitializeAsClientEntity(g_PlayerModels[modelIndex].szPath, RENDER_GROUP_OPAQUE_ENTITY))
    {
        m_pModel->AddEffects(EF_NODRAW); // Keep it out of the real world
        int seq = m_pModel->LookupSequence("idle_subtle");
        m_pModel->SetSequence(seq > 0 ? seq : 0);
    }
}

CModelPreviewPanel::~CModelPreviewPanel()
{
    if (m_pModel)
    {
        m_pModel->Release();
    }
}

void CModelPreviewPanel::UpdateCount(int count)
{
    m_pCountLabel->SetText(VarArgs("Active: %d", count));
}

void CModelPreviewPanel::OnTick()
{
    BaseClass::OnTick();
    if (m_pModel)
    {
        m_pModel->FrameAdvance(gpGlobals->frametime);
    }
}

void CModelPreviewPanel::Paint()
{
    BaseClass::Paint();
    if (!m_pModel) return;

    // Get the absolute screen position of *this specific child panel*
    int x, y, w, h;
    GetBounds(x, y, w, h);

    // Walk up the parent hierarchy to get true screen coordinates
    vgui::VPANEL pPanel = GetVPanel();
    vgui::ipanel()->GetAbsPos(pPanel, x, y);

    CMatRenderContextPtr pRenderContext(materials);

    CViewSetup view;
    memset(&view, 0, sizeof(view));
    view.x = x;
    view.y = y;
    view.width = w;
    view.height = h - 40; // Restrict to the upper image area above the text labels
    view.fov = 30.0f;
    view.origin = vec3_origin;
    view.angles = vec3_angle;
    view.zNear = 1.0f;
    view.zFar = 2000.0f;

    VPlane frustum[FRUSTUM_NUMPLANES];
    render->Push3DView(view, VIEW_CLEAR_DEPTH, NULL, frustum);

    // Center the model precisely inside this panel's localized view frustum
    m_pModel->SetAbsOrigin(Vector(65, 0, -15));
    m_pModel->SetAbsAngles(QAngle(0, 180, 0));
    m_pModel->InvalidateBoneCache();

    static Vector white[6] = { Vector(0.7,0.7,0.7), Vector(0.7,0.7,0.7), Vector(0.7,0.7,0.7), Vector(0.7,0.7,0.7), Vector(0.7,0.7,0.7), Vector(0.7,0.7,0.7) };
    g_pStudioRender->SetAmbientLightColors(white);
    g_pStudioRender->SetLocalLights(0, NULL);

    m_pModel->DrawModel(STUDIO_RENDER);

    render->PopView(frustum);
}

void CModelPreviewPanel::OnMousePressed(vgui::MouseCode code)
{
    engine->ClientCmd(VarArgs("select_playermodel %d", m_nModelIndex));
    GetParent()->SetVisible(false);
}


// ---------------------------------------------------------------------------------
// The Main Menu Frame
// ---------------------------------------------------------------------------------
class CModelSelectionMenu : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CModelSelectionMenu, vgui::Frame);
public:
    CModelSelectionMenu(vgui::VPANEL parent);
    void UpdateCounts(int counts[10]);

private:
    CModelPreviewPanel* m_pPreviews[10];
};

CModelSelectionMenu* g_pModelMenu = NULL;

CModelSelectionMenu::CModelSelectionMenu(vgui::VPANEL parent) : BaseClass(NULL, "ModelSelectionMenu")
{
    SetParent(parent);
    SetTitle("Select Team and Character", true);
    SetSize(820, 520); // Widened window to comfortably fit 5 columns
    MoveToCenterOfScreen();
    SetSizeable(false);
    SetCloseButtonVisible(false);
    MakePopup();

    vgui::Label* pRebelLabel = new vgui::Label(this, "RebelLabel", "REBELS");
    pRebelLabel->SetPos(20, 30);

    vgui::Label* pCombineLabel = new vgui::Label(this, "CombineLabel", "COMBINE");
    pCombineLabel->SetPos(20, 270);

    for (int i = 0; i < 10; i++)
    {
        m_pPreviews[i] = new CModelPreviewPanel(this, VarArgs("Preview%d", i), i);

        int col = i % 5;
        int row = i / 5;

        // Adjusted spacing so all 5 columns fit nicely inside the 820px width
        int xPos = 25 + (col * 155);
        int yPos = row == 0 ? 55 : 295;

        m_pPreviews[i]->SetPos(xPos, yPos);
    }
}

void CModelSelectionMenu::UpdateCounts(int counts[10])
{
    for (int i = 0; i < 10; i++)
    {
        m_pPreviews[i]->UpdateCount(counts[i]);
    }
}

// ---------------------------------------------------------------------------------
// Hooks & Commands
// ---------------------------------------------------------------------------------
CON_COMMAND(show_model_menu, "Opens the model selection menu")
{
    if (!g_pModelMenu)
    {
        g_pModelMenu = new CModelSelectionMenu(enginevgui->GetPanel(PANEL_CLIENTDLL));
    }
    g_pModelMenu->SetVisible(true);
}

void __MsgFunc_UpdateModelCounts(bf_read& msg)
{
    if (!g_pModelMenu) return;

    int counts[10];
    for (int i = 0; i < 10; i++)
    {
        counts[i] = msg.ReadByte();
    }
    g_pModelMenu->UpdateCounts(counts);
}
class CModelMenuMsgHook : public CAutoGameSystem
{
public:
    CModelMenuMsgHook() : CAutoGameSystem("CModelMenuMsgHook") {}

    virtual bool Init()
    {
        usermessages->HookMessage("UpdateModelCounts", __MsgFunc_UpdateModelCounts);
        return true;
    }
};
CModelMenuMsgHook g_ModelMenuMsgHook;