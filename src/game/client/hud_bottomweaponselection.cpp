#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include "c_basecombatweapon.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Controls.h>
#include <vgui/IInput.h>

using namespace vgui;

class CHudBottomWeaponSelection : public CHudElement, public Panel
{
    DECLARE_CLASS_SIMPLE(CHudBottomWeaponSelection, Panel);
public:
    CHudBottomWeaponSelection(const char* pElementName);

    virtual void Init();
    virtual void ApplySchemeSettings(vgui::IScheme* pScheme);
    virtual void Paint();

    int KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding);
    virtual void OnMousePressed(vgui::MouseCode code);

    void CycleNext();
    void CyclePrev();
    void SelectSlot(int slot);
    void OpenSelection();
    void HideSelection();
    void ConfirmSelection();
    bool IsMenuOpen() const { return m_bMenuOpen; }

private:
    void GatherWeapons();

    CUtlVector<C_BaseCombatWeapon*> m_Weapons;
    CHandle<C_BaseCombatWeapon> m_hSelectedWeapon;
    float m_flHideTime;
    bool m_bMenuOpen;
    vgui::HFont m_hFont;
};

DECLARE_HUDELEMENT(CHudBottomWeaponSelection);
CHudBottomWeaponSelection* g_pBottomWeaponSelection = NULL;

ConVar hud_bottom_gap("hud_bottom_gap", "4", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Gap between weapon buckets.");
ConVar hud_bottom_y_offset("hud_bottom_y_offset", "100", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Distance from the bottom of the screen.");
ConVar hud_bottom_icon_w("hud_bottom_icon_w", "100", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Width of the weapon icon box.");
ConVar hud_bottom_icon_h("hud_bottom_icon_h", "30", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Height of the weapon icon box.");

CON_COMMAND(bottom_invnext, "Cycle next in bottom weapon selection") { if (g_pBottomWeaponSelection) g_pBottomWeaponSelection->CycleNext(); }
CON_COMMAND(bottom_invprev, "Cycle prev in bottom weapon selection") { if (g_pBottomWeaponSelection) g_pBottomWeaponSelection->CyclePrev(); }
CON_COMMAND(bottom_slot1, "Select bottom slot 1 (Bucket 9)") { if (g_pBottomWeaponSelection) g_pBottomWeaponSelection->SelectSlot(8); }
CON_COMMAND(bottom_slot2, "Select bottom slot 2 (Bucket 10)") { if (g_pBottomWeaponSelection) g_pBottomWeaponSelection->SelectSlot(9); }
CON_COMMAND(bottom_slot3, "Select bottom slot 3 (Bucket 11)") { if (g_pBottomWeaponSelection) g_pBottomWeaponSelection->SelectSlot(10); }

CHudBottomWeaponSelection::CHudBottomWeaponSelection(const char* pElementName)
    : CHudElement(pElementName), Panel(NULL, "HudBottomWeaponSelection")
{
    vgui::Panel* pParent = g_pClientMode->GetViewport();
    SetParent(pParent);

    g_pBottomWeaponSelection = this;
    m_bMenuOpen = false;
    m_flHideTime = 0;

    // Explicitly enable mouse input so clicks register on this panel
    SetMouseInputEnabled(true);
    SetKeyBoardInputEnabled(false);

    SetHiddenBits(HIDEHUD_WEAPONSELECTION | HIDEHUD_PLAYERDEAD);
}

void CHudBottomWeaponSelection::Init() { Reset(); }

void CHudBottomWeaponSelection::ApplySchemeSettings(vgui::IScheme* pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    m_hFont = pScheme->GetFont("Default");

    int screenW, screenH;
    surface()->GetScreenSize(screenW, screenH);
    SetBounds(0, 0, screenW, screenH);
}

int WeaponSortFunc(C_BaseCombatWeapon* const* p1, C_BaseCombatWeapon* const* p2)
{
    if ((*p1)->GetSlot() != (*p2)->GetSlot())
        return (*p1)->GetSlot() - (*p2)->GetSlot();
    return (*p1)->GetPosition() - (*p2)->GetPosition();
}

void CHudBottomWeaponSelection::GatherWeapons()
{
    m_Weapons.RemoveAll();
    C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
    if (!pPlayer) return;

    for (int i = 0; i < MAX_WEAPONS; ++i)
    {
        C_BaseCombatWeapon* pWpn = pPlayer->GetWeapon(i);
        if (pWpn && pWpn->GetSlot() >= 8 && pWpn->GetSlot() <= 10)
        {
            m_Weapons.AddToTail(pWpn);
        }
    }
    m_Weapons.Sort(WeaponSortFunc);
}

void CHudBottomWeaponSelection::OpenSelection()
{
    m_bMenuOpen = true;
    m_flHideTime = gpGlobals->curtime + 3.0f;

    // Force VGUI to route mouse input to this panel when open
    SetMouseInputEnabled(true);
    vgui::input()->SetMouseFocus(GetVPanel());
}

void CHudBottomWeaponSelection::HideSelection()
{
    m_bMenuOpen = false;
    m_hSelectedWeapon = NULL;

    // Release focus when closed
    SetMouseInputEnabled(false);
}

void CHudBottomWeaponSelection::ConfirmSelection()
{
    if (m_hSelectedWeapon.Get())
    {
        engine->ClientCmd(VarArgs("use %s", m_hSelectedWeapon.Get()->GetName()));
    }
    HideSelection();
}

void CHudBottomWeaponSelection::CycleNext()
{
    GatherWeapons();
    if (m_Weapons.Count() == 0) return;

    int idx = m_Weapons.Find(m_hSelectedWeapon.Get());
    idx = (idx == m_Weapons.InvalidIndex()) ? 0 : (idx + 1) % m_Weapons.Count();
    m_hSelectedWeapon = m_Weapons[idx];

    ConVarRef hud_fastswitch("hud_fastswitch");
    if (hud_fastswitch.GetBool()) ConfirmSelection();
    else OpenSelection();
}

void CHudBottomWeaponSelection::CyclePrev()
{
    GatherWeapons();
    if (m_Weapons.Count() == 0) return;

    int idx = m_Weapons.Find(m_hSelectedWeapon.Get());
    idx = (idx == m_Weapons.InvalidIndex()) ? (m_Weapons.Count() - 1) : (idx - 1 + m_Weapons.Count()) % m_Weapons.Count();
    m_hSelectedWeapon = m_Weapons[idx];

    ConVarRef hud_fastswitch("hud_fastswitch");
    if (hud_fastswitch.GetBool()) ConfirmSelection();
    else OpenSelection();
}

void CHudBottomWeaponSelection::SelectSlot(int slot)
{
    GatherWeapons();
    C_BaseCombatWeapon* pFirstInSlot = NULL;
    for (int i = 0; i < m_Weapons.Count(); ++i)
    {
        if (m_Weapons[i]->GetSlot() == slot)
        {
            pFirstInSlot = m_Weapons[i];
            break;
        }
    }

    if (!pFirstInSlot) return;

    if (m_bMenuOpen && m_hSelectedWeapon.Get() && m_hSelectedWeapon.Get()->GetSlot() == slot)
    {
        int idx = m_Weapons.Find(m_hSelectedWeapon.Get());
        int nextIdx = (idx + 1) % m_Weapons.Count();
        while (m_Weapons[nextIdx]->GetSlot() != slot && nextIdx != idx)
        {
            nextIdx = (nextIdx + 1) % m_Weapons.Count();
        }
        m_hSelectedWeapon = m_Weapons[nextIdx];
    }
    else
    {
        m_hSelectedWeapon = pFirstInSlot;
    }

    ConVarRef hud_fastswitch("hud_fastswitch");
    if (hud_fastswitch.GetBool()) ConfirmSelection();
    else OpenSelection();
}

int CHudBottomWeaponSelection::KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
    if (!m_bMenuOpen)
        return 1; // Menu is closed, let the game handle input normally

    // If the menu is open, capture ALL left clicks and attack inputs
    if (down && (keynum == MOUSE_LEFT || (pszCurrentBinding && (FStrEq(pszCurrentBinding, "+attack") || FStrEq(pszCurrentBinding, "+attack2")))))
    {
        // Try to select if they clicked an item, otherwise just close the menu safely
        ConfirmSelection();
        return 0; // Return 0 blocks the engine from firing the weapon!
    }

    return 1;
}

void CHudBottomWeaponSelection::OnMousePressed(vgui::MouseCode code)
{
    if (!m_bMenuOpen)
    {
        BaseClass::OnMousePressed(code);
        return;
    }

    if (code == MOUSE_LEFT)
    {
        int cursorX, cursorY;
        vgui::input()->GetCursorPos(cursorX, cursorY);

        int screenW, screenH;
        surface()->GetScreenSize(screenW, screenH);

        int boxW = hud_bottom_icon_w.GetInt();
        int boxH = hud_bottom_icon_h.GetInt();
        int padding = hud_bottom_gap.GetInt();
        int slots[] = { 8, 9, 10 };
        int numBuckets = 3;

        int totalW = (numBuckets * boxW) + ((numBuckets - 1) * padding);
        int startX = (screenW - totalW) / 2;
        int baseY = screenH - hud_bottom_y_offset.GetInt();

        GatherWeapons();
        bool bClickedItem = false;

        for (int i = 0; i < numBuckets; ++i)
        {
            int currentSlot = slots[i];
            int x = startX + i * (boxW + padding);

            int drawnInSlot = 0;
            for (int j = 0; j < m_Weapons.Count(); ++j)
            {
                C_BaseCombatWeapon* pWpn = m_Weapons[j];
                if (pWpn->GetSlot() == currentSlot)
                {
                    int wpnY = baseY - ((drawnInSlot + 1) * (boxH + padding));

                    if (cursorX >= x && cursorX <= (x + boxW) && cursorY >= wpnY && cursorY <= (wpnY + boxH))
                    {
                        m_hSelectedWeapon = pWpn;
                        ConfirmSelection();
                        bClickedItem = true;
                        break;
                    }
                    drawnInSlot++;
                }
            }
            if (bClickedItem) break;
        }

        // If they clicked outside the boxes, just close the menu safely without firing
        if (!bClickedItem)
        {
            HideSelection();
        }
    }
}

void CHudBottomWeaponSelection::Paint()
{
    if (!m_bMenuOpen) return;
    if (gpGlobals->curtime > m_flHideTime)
    {
        HideSelection();
        return;
    }

    GatherWeapons();
    if (m_Weapons.Count() == 0)
    {
        HideSelection();
        return;
    }

    if (!m_Weapons.HasElement(m_hSelectedWeapon.Get()))
        m_hSelectedWeapon = m_Weapons[0];

    int screenW, screenH;
    surface()->GetScreenSize(screenW, screenH);

    int boxW = hud_bottom_icon_w.GetInt();
    int boxH = hud_bottom_icon_h.GetInt();
    int padding = hud_bottom_gap.GetInt();
    int slots[] = { 8, 9, 10 };
    int numBuckets = 3;

    int totalW = (numBuckets * boxW) + ((numBuckets - 1) * padding);
    int startX = (screenW - totalW) / 2;
    int baseY = screenH - hud_bottom_y_offset.GetInt();

    for (int i = 0; i < numBuckets; ++i)
    {
        int currentSlot = slots[i];
        int x = startX + i * (boxW + padding);

        surface()->DrawSetColor(0, 0, 0, 150);
        surface()->DrawFilledRect(x, baseY, x + boxW, baseY + 10);

        int drawnInSlot = 0;
        for (int j = 0; j < m_Weapons.Count(); ++j)
        {
            C_BaseCombatWeapon* pWpn = m_Weapons[j];
            if (pWpn->GetSlot() == currentSlot)
            {
                int wpnY = baseY - ((drawnInSlot + 1) * (boxH + padding));
                bool bSelected = (pWpn == m_hSelectedWeapon.Get());

                surface()->DrawSetColor(bSelected ? 255 : 0, bSelected ? 150 : 0, 0, bSelected ? 200 : 150);
                surface()->DrawFilledRect(x, wpnY, x + boxW, wpnY + boxH);

                const CHudTexture* pIcon = bSelected ? pWpn->GetSpriteActive() : pWpn->GetSpriteInactive();
                if (pIcon)
                {
                    int iconX = x + (boxW - pIcon->Width()) / 2;
                    int iconY = wpnY + (boxH - pIcon->Height()) / 2;
                    pIcon->DrawSelf(iconX, iconY, Color(255, 255, 255, 255));
                }

                drawnInSlot++;
            }

        }
    }

}
int BottomWeaponSelection_KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
    if (g_pBottomWeaponSelection && g_pBottomWeaponSelection->IsMenuOpen())
    {
        return g_pBottomWeaponSelection->KeyInput(down, keynum, pszCurrentBinding);
    }
    return 1; // Let engine handle input normally if menu is closed
}