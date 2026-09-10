#ifndef CEFFECTGLOW_H
#define CEFFECTGLOW_H

#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"

class CEntGlowEffect
{
public:
	static CEntGlowEffect& GetInstance()
	{
		static CEntGlowEffect s_Instance;
		return s_Instance;
	}

	void Init(void);
	void Shutdown(void);
	void RenderGlowModels(void);
	void RenderGlowPostProcess(void);
	void DrawGlowEffects(void);

private:
	bool m_bInitialized = false;
	CTextureReference m_GlowBuff1;
	CTextureReference m_GlowBuff2;
	IMaterial* m_pBlurX = nullptr;	
	IMaterial* m_pBlurY = nullptr;
	IMaterial* m_pEffectMaterial = nullptr;
};

CEntGlowEffect& GetGlowEffect();

#endif // CEFFECTGLOW_H