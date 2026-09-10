#ifndef MENU_BACKGROUND_H
#define MENU_BACKGROUND_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include "materialsystem/imaterial.h"
#include "vgui/VGUI.h"
#include "video/ivideoservices.h"

class CMainMenu : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CMainMenu, vgui::Panel);

public:
	CMainMenu(vgui::Panel* parent, const char* pElementName);
	virtual ~CMainMenu();

	virtual void ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void Paint();

	bool IsVideoPlaying();
	void StartVideo();
	void StopVideo();
	void SetBlackBackground(bool bBlack);
	void DoModal();
	void OnDisconnectFromGame();

private:
	bool BeginPlayback(const char* pFilename);
	void ReleaseVideo();
	void GetPanelPos(int& xpos, int& ypos);

	IVideoMaterial* m_VideoMaterial;
	IMaterial* m_pMaterial;
	CMaterialReference m_MainMenuRef;

	int m_nPlaybackWidth;
	int m_nPlaybackHeight;
	float m_flU;
	float m_flV;
	bool m_bPaintVideo;
	bool m_bBlackBackground;
	bool m_bAllowAlternateMedia;
	bool m_bToolsMode;
	bool m_bLoaded;
};

#endif // MENU_BACKGROUND_H