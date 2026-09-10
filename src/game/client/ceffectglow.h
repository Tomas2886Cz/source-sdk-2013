#pragma once

#include "cbase.h"

class CEntGlowEffect
{
public:
	static CEntGlowEffect& GetInstance()
	{
		static CEntGlowEffect instance;
		return instance;
	}

	void Init(void);
	void Shutdown(void);
	void DrawGlowEffects(void);

	bool IsVisible() const { return m_bVisible; }
	void SetVisible(bool state) { m_bVisible = state; }

private:
	CEntGlowEffect() : m_bVisible(true) {}
	bool m_bVisible;

	CTextureReference m_GlowBuff1;
	CTextureReference m_GlowBuff2;

	// Declare the material pointers here
	IMaterial* m_pBlurX;
	IMaterial* m_pBlurY;
	IMaterial* m_pEffectMaterial;
};

CEntGlowEffect& GetGlowEffect();