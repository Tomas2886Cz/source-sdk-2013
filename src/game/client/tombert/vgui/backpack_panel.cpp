#include "cbase.h"
#include "backpack_panel.h"
#include "c_hl2mp_player.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IInput.h>
#include "mp_shareddefs.h"

CBackpackPanel::CBackpackPanel(vgui::VPANEL parent) : BaseClass(NULL, "BackpackPanel")
{
    SetParent(parent);
    SetBounds(0, 0, 750, 780);
    MoveToCenterOfScreen();
    SetTitle("Player Backpack Inventory", true);
    SetSizeable(false);
    SetAutoDelete(false);

    LoadControlSettings("resource/UI/BackpackPanel.res");

    SetVisible(false);
    SetKeyBoardInputEnabled(true);
    SetMouseInputEnabled(true);

    m_iScrollOffset = 0;

    m_nBackgroundTextureID = vgui::surface()->CreateNewTextureID();
    vgui::surface()->DrawSetTextureFile(m_nBackgroundTextureID, "vgui/hud/backpack/backpack_background", true, false);

    PopulateInventoryLists();
    InvalidateLayout(true, true);
}

CBackpackPanel::~CBackpackPanel() {}

void CBackpackPanel::SetVisible(bool state)
{
    BaseClass::SetVisible(state);

    if (state)
    {
        m_iScrollOffset = 0;
        PopulateInventoryLists();
        InvalidateLayout(true, true);
        SetKeyBoardInputEnabled(true);
        SetMouseInputEnabled(true);
    }
    else
    {
        SetKeyBoardInputEnabled(false);
        SetMouseInputEnabled(false);
        engine->ClientCmd("-duck; -forward; -back; -moveleft; -moveright; -jump; -attack; -attack2");
    }
}

void CBackpackPanel::PaintBackground()
{
    BaseClass::PaintBackground();

    int wide, tall;
    GetSize(wide, tall);

    vgui::surface()->DrawSetColor(255, 255, 255, 128);
    vgui::surface()->DrawSetTexture(m_nBackgroundTextureID);
    vgui::surface()->DrawTexturedRect(0, 0, wide, tall);
}

void CBackpackPanel::Paint()
{
    BaseClass::Paint();

    int wide, tall;
    GetSize(wide, tall);

    // Get current cursor position and convert to local panel coordinates for hover checks
    int cursorX, cursorY;
    vgui::input()->GetCursorPos(cursorX, cursorY);
    ScreenToLocal(cursorX, cursorY);

    vgui::HFont iconFont = vgui::scheme()->GetIScheme(GetScheme())->GetFont("WeaponIcons");
    if (!iconFont)
    {
        iconFont = vgui::scheme()->GetIScheme(GetScheme())->GetFont("DefaultBold");
    }

    vgui::HFont ammoFont = vgui::scheme()->GetIScheme(GetScheme())->GetFont("DefaultBold");
    if (!ammoFont)
    {
        ammoFont = vgui::scheme()->GetIScheme(GetScheme())->GetFont("Default");
    }

    int startX = 40;
    int startY = 60;
    int slotSize = 84;
    int slotSpacing = 16;
    int maxVisible = 6;

    // Draw Weapon Slots
    for (int i = m_iScrollOffset; i < m_WeaponItems.Count() && (i - m_iScrollOffset) < maxVisible; i++)
    {
        int slotIndex = i - m_iScrollOffset;
        int y = startY + slotIndex * (slotSize + slotSpacing);

        // Define the ammo mini-icon position and size
        int smallIconSize = 36;
        int smallIconX = startX + slotSize + 20;
        int smallIconY = y + (slotSize / 2) + 2;

        // Square bounding box centered on the ammo icon with 4px margin
        int ammoBoxPadding = 4;
        int ammoBoxX1 = smallIconX - ammoBoxPadding;
        int ammoBoxY1 = smallIconY - ammoBoxPadding;
        int ammoBoxX2 = smallIconX + smallIconSize + ammoBoxPadding;
        int ammoBoxY2 = smallIconY + smallIconSize + ammoBoxPadding;

        // Hover bounding checks
        bool bWeaponHovered = (cursorX >= startX && cursorX <= startX + slotSize && cursorY >= y && cursorY <= y + slotSize);
        bool bAmmoHovered = (cursorX >= ammoBoxX1 && cursorX <= ammoBoxX2 && cursorY >= ammoBoxY1 && cursorY <= ammoBoxY2);

        // Weapon slot highlight
        if (bWeaponHovered)
        {
            vgui::surface()->DrawSetColor(255, 255, 255, 40);
            vgui::surface()->DrawFilledRect(startX, y, startX + slotSize, y + slotSize);
            vgui::surface()->DrawSetColor(255, 255, 255, 200);
            vgui::surface()->DrawOutlinedRect(startX, y, startX + slotSize, y + slotSize);
        }

        // Draw weapon icon
        if (m_WeaponItems[i].pIcon)
        {
            if (m_WeaponItems[i].pIcon->bRenderUsingFont)
            {
                vgui::HFont hFont = m_WeaponItems[i].pIcon->hFont;
                if (!hFont)
                {
                    hFont = iconFont;
                }
                vgui::surface()->DrawSetTextFont(hFont);
                vgui::surface()->DrawSetTextColor(255, 255, 255, 255);
                vgui::surface()->DrawSetTextPos(startX + (slotSize / 2) - 16, y + (slotSize / 2) - 16);

                wchar_t weaponChar[2] = { (wchar_t)m_WeaponItems[i].pIcon->cCharacterInFont, 0 };
                vgui::surface()->DrawPrintText(weaponChar, 1);
            }
            else
            {
                vgui::surface()->DrawSetColor(255, 255, 255, 255);
                m_WeaponItems[i].pIcon->DrawSelf(startX, y, slotSize, slotSize, Color(255, 255, 255, 255));
            }
        }

        // Draw yellow ammo text above the ammo icon
        vgui::surface()->DrawSetTextFont(ammoFont);
        wchar_t wszAmmo[32];
        swprintf_s(wszAmmo, L"AMMO: %d", m_WeaponItems[i].iAmmo);
        vgui::surface()->DrawSetTextPos(startX + slotSize + 20, y + (slotSize / 2) - 18);
        vgui::surface()->DrawSetTextColor(255, 255, 0, 255);
        vgui::surface()->DrawPrintText(wszAmmo, wcslen(wszAmmo));

        // Highlight square around ammo icon when hovered
        if (bAmmoHovered)
        {
            vgui::surface()->DrawSetColor(255, 255, 255, 40);
            vgui::surface()->DrawFilledRect(ammoBoxX1, ammoBoxY1, ammoBoxX2, ammoBoxY2);
            vgui::surface()->DrawSetColor(255, 255, 255, 200);
            vgui::surface()->DrawOutlinedRect(ammoBoxX1, ammoBoxY1, ammoBoxX2, ammoBoxY2);
        }

        // Draw the ammo mini-icon
        if (m_WeaponItems[i].pIcon)
        {
            if (m_WeaponItems[i].pIcon->bRenderUsingFont)
            {
                vgui::HFont hFont = m_WeaponItems[i].pIcon->hFont;
                if (!hFont) hFont = iconFont;
                vgui::surface()->DrawSetTextFont(hFont);
                vgui::surface()->DrawSetTextColor(60, 60, 60, 255);
                vgui::surface()->DrawSetTextPos(smallIconX, smallIconY);

                wchar_t smallChar[2] = { (wchar_t)m_WeaponItems[i].pIcon->cCharacterInFont, 0 };
                vgui::surface()->DrawPrintText(smallChar, 1);
            }
            else
            {
                vgui::surface()->DrawSetColor(255, 255, 255, 255);
                m_WeaponItems[i].pIcon->DrawSelf(smallIconX, smallIconY, smallIconSize, smallIconSize, Color(70, 70, 70, 255));
            }
        }
    }

    // Draw Blueprint Slots
    int blueStartX = wide - 40 - slotSize;
    for (int i = m_iScrollOffset; i < m_BlueprintItems.Count() && (i - m_iScrollOffset) < maxVisible; i++)
    {
        int slotIndex = i - m_iScrollOffset;
        int y = startY + slotIndex * (slotSize + slotSpacing);

        bool bBlueHovered = (cursorX >= blueStartX && cursorX <= blueStartX + slotSize && cursorY >= y && cursorY <= y + slotSize);
        if (bBlueHovered)
        {
            vgui::surface()->DrawSetColor(255, 255, 255, 40);
            vgui::surface()->DrawFilledRect(blueStartX, y, blueStartX + slotSize, y + slotSize);
            vgui::surface()->DrawSetColor(255, 255, 255, 200);
            vgui::surface()->DrawOutlinedRect(blueStartX, y, blueStartX + slotSize, y + slotSize);
        }

        if (m_BlueprintItems[i].pIcon)
        {
            if (m_BlueprintItems[i].pIcon->bRenderUsingFont)
            {
                vgui::HFont hFont = m_BlueprintItems[i].pIcon->hFont;
                if (!hFont)
                {
                    hFont = iconFont;
                }
                vgui::surface()->DrawSetTextFont(hFont);
                vgui::surface()->DrawSetTextColor(255, 255, 255, 255);
                vgui::surface()->DrawSetTextPos(blueStartX + (slotSize / 2) - 16, y + (slotSize / 2) - 16);

                wchar_t bpChar[2] = { (wchar_t)m_BlueprintItems[i].pIcon->cCharacterInFont, 0 };
                vgui::surface()->DrawPrintText(bpChar, 1);
            }
            else
            {
                vgui::surface()->DrawSetColor(255, 255, 255, 255);
                m_BlueprintItems[i].pIcon->DrawSelf(blueStartX, y, slotSize, slotSize, Color(255, 255, 255, 255));
            }
        }
    }
}

void CBackpackPanel::OnClose()
{
    BaseClass::OnClose();
    engine->ClientCmd("server_toggle_backpack");
}

void CBackpackPanel::PerformLayout()
{
    BaseClass::PerformLayout();
}

void CBackpackPanel::OnMousePressed(vgui::MouseCode code)
{
    if (code == MOUSE_LEFT)
    {
        int x, y;
        vgui::input()->GetCursorPos(x, y);
        ScreenToLocal(x, y);

        int startX = 40;
        int startY = 60;
        int slotSize = 84;
        int slotSpacing = 16;
        int maxVisible = 6;

        for (int i = m_iScrollOffset; i < m_WeaponItems.Count() && (i - m_iScrollOffset) < maxVisible; i++)
        {
            int slotIndex = i - m_iScrollOffset;
            int slotY = startY + slotIndex * (slotSize + slotSpacing);

            int weaponBoxX1 = startX;
            int weaponBoxY1 = slotY;
            int weaponBoxX2 = startX + slotSize;
            int weaponBoxY2 = slotY + slotSize;

            int smallIconSize = 36;
            int smallIconX = startX + slotSize + 20;
            int smallIconY = slotY + (slotSize / 2) + 2;

            int ammoBoxPadding = 4;
            int ammoBoxX1 = smallIconX - ammoBoxPadding;
            int ammoBoxY1 = smallIconY - ammoBoxPadding;
            int ammoBoxX2 = smallIconX + smallIconSize + ammoBoxPadding;
            int ammoBoxY2 = smallIconY + smallIconSize + ammoBoxPadding;

            if (x >= weaponBoxX1 && x <= weaponBoxX2 && y >= weaponBoxY1 && y <= weaponBoxY2)
            {
                char szCmd[64];
                Q_snprintf(szCmd, sizeof(szCmd), "backpack_drop_index %d", m_WeaponItems[i].iEntityIndex);
                engine->ClientCmd(szCmd);

                m_WeaponItems.FastRemove(i);
                InvalidateLayout(true, true);
                return;
            }
            else if (x >= ammoBoxX1 && x <= ammoBoxX2 && y >= ammoBoxY1 && y <= ammoBoxY2)
            {
                if (m_WeaponItems[i].iEntityIndex > 0)
                {
                    char szCmd[64];
                    Q_snprintf(szCmd, sizeof(szCmd), "backpack_drop_ammo %d", m_WeaponItems[i].iEntityIndex);
                    engine->ClientCmd(szCmd);

                    char szEnt[64];
                    int iDropSize = 0;
                    if (GetAmmoDropInfo(m_WeaponItems[i].iAmmoType, szEnt, sizeof(szEnt), iDropSize))
                    {
                        m_WeaponItems[i].iAmmo = MAX(0, m_WeaponItems[i].iAmmo - iDropSize);
                        InvalidateLayout(true, true);
                    }
                }
                return;
            }
        }
    }

    BaseClass::OnMousePressed(code);
}

void CBackpackPanel::OnMouseWheeled(int delta)
{
    m_iScrollOffset -= delta;
    int maxScroll = MAX(0, m_WeaponItems.Count() - 6);
    m_iScrollOffset = clamp(m_iScrollOffset, 0, maxScroll);
    Repaint();
}

void CBackpackPanel::OnKeyCodePressed(vgui::KeyCode code)
{
    if (code == KEY_ESCAPE)
    {
        Close();
        return;
    }

    BaseClass::OnKeyCodePressed(code);
}

void CBackpackPanel::OnThink()
{
    BaseClass::OnThink();

    if (IsVisible())
    {
        PopulateInventoryLists();
    }
}

void CBackpackPanel::PopulateInventoryLists()
{
    C_HL2MP_Player* pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
    if (!pPlayer)
    {
        m_WeaponItems.Purge();
        m_BlueprintItems.Purge();
        return;
    }

    m_WeaponItems.Purge();
    m_BlueprintItems.Purge();

    for (int i = 0; i < MAX_WEAPONS; i++)
    {
        CBaseCombatWeapon* pWeapon = pPlayer->GetWeapon(i);
        if (!pWeapon)
            continue;

        BackpackItem_t item;
        Q_strncpy(item.szClassname, pWeapon->GetClassname(), sizeof(item.szClassname));
        Q_strncpy(item.szPrintName, pWeapon->GetPrintName(), sizeof(item.szPrintName));

        int iClip = pWeapon->Clip1();
        int iAmmoType = pWeapon->GetPrimaryAmmoType();
        int iReserve = (iAmmoType > 0) ? pPlayer->GetAmmoCount(iAmmoType) : 0;
        item.iAmmo = (iClip >= 0 ? iClip : 0) + iReserve;

        item.iAmmoType = iAmmoType;
        item.iEntityIndex = pWeapon->entindex();

        const FileWeaponInfo_t& info = pWeapon->GetWpnData();
        item.pIcon = info.iconActive;

        if (Q_stristr(item.szClassname, "blueprint"))
        {
            item.bIsBlueprint = true;
            m_BlueprintItems.AddToTail(item);
        }
        else
        {
            item.bIsBlueprint = false;
            m_WeaponItems.AddToTail(item);
        }
    }
}