#include "cbase.h"
#include "basegrenade_shared.h"
#include "engine/IEngineSound.h"
#include "physics.h"

ConVar sv_smoke_duration("sv_smoke_duration", "15.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Duration of the smoke cloud in seconds.");
ConVar sv_smoke_radius("sv_smoke_radius", "120.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Radius and spread of the smoke cloud.");
ConVar sv_smoke_color("sv_smoke_color", "180 180 180", FCVAR_REPLICATED | FCVAR_NOTIFY, "RGB color of the smoke cloud.");
ConVar sv_smoke_opacity("sv_smoke_opacity", "220", FCVAR_REPLICATED | FCVAR_NOTIFY, "Opacity/thickness of the smoke cloud (0-255).");

class CGrenadeSmoke : public CBaseGrenade
{
	DECLARE_CLASS(CGrenadeSmoke, CBaseGrenade);

public:
	void Spawn(void);
	void Precache(void);
	void Detonate(void);
	void SmokeThink(void);
	void BounceSound(void);

	DECLARE_DATADESC();

private:
	CHandle<CBaseEntity> m_hSmokeStack;
};

LINK_ENTITY_TO_CLASS(npc_grenade_smoke, CGrenadeSmoke);

BEGIN_DATADESC(CGrenadeSmoke)
DEFINE_THINKFUNC(SmokeThink),
END_DATADESC()

void CGrenadeSmoke::Precache(void)
{
	PrecacheModel("models/Weapons/w_eq_smokegrenade.mdl");
	PrecacheScriptSound("BaseSmokeEffect.Sound");
	PrecacheModel("particle/particle_smokegrenade.vmt");
	BaseClass::Precache();
}

void CGrenadeSmoke::Spawn(void)
{
	Precache();
	SetModel("models/Weapons/w_grenade.mdl");

	if (modelinfo->GetModelIndex("models/Weapons/w_eq_smokegrenade.mdl") == -1)
	{
		SetModel("models/Weapons/w_grenade.mdl");
	}

	SetMoveType(MOVETYPE_FLYGRAVITY);
	SetSolid(SOLID_BBOX);
	SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

	VPhysicsInitNormal(SOLID_BBOX, 0, false);

	m_takedamage = DAMAGE_NO;
	m_flDamage = 0;

	SetSize(-Vector(4, 4, 4), Vector(4, 4, 4));

	SetThink(&CGrenadeSmoke::Detonate);
	SetNextThink(gpGlobals->curtime + 2.5f);
}

void CGrenadeSmoke::Detonate(void)
{
	SetAbsVelocity(vec3_origin);
	SetMoveType(MOVETYPE_NONE);

	EmitSound("BaseSmokeEffect.Sound");

	CBaseEntity* pSmoke = CreateEntityByName("env_smokestack");
	if (pSmoke)
	{
		pSmoke->SetAbsOrigin(GetAbsOrigin());
		pSmoke->SetOwnerEntity(this);
		pSmoke->SetParent(this);

		float radius = sv_smoke_radius.GetFloat();
		char buf[128];

		pSmoke->KeyValue("InitialState", "1");
		pSmoke->KeyValue("BaseSpread", "30");
		Q_snprintf(buf, sizeof(buf), "%f", radius);
		pSmoke->KeyValue("SpreadSpeed", buf);
		pSmoke->KeyValue("Speed", "40");
		Q_snprintf(buf, sizeof(buf), "%f", radius * 0.8f);
		pSmoke->KeyValue("StartSize", buf);
		Q_snprintf(buf, sizeof(buf), "%f", radius * 1.5f);
		pSmoke->KeyValue("EndSize", buf);
		pSmoke->KeyValue("Rate", "80");
		pSmoke->KeyValue("JetLength", "100");
		pSmoke->KeyValue("Twist", "10");
		pSmoke->KeyValue("RenderColor", sv_smoke_color.GetString());
		pSmoke->KeyValue("RenderAmt", sv_smoke_opacity.GetString());
		pSmoke->KeyValue("SmokeMaterial", "particle/particle_smokegrenade.vmt");

		DispatchSpawn(pSmoke);
		pSmoke->Activate();

		m_hSmokeStack = pSmoke;
	}

	SetThink(&CGrenadeSmoke::SmokeThink);
	SetNextThink(gpGlobals->curtime + sv_smoke_duration.GetFloat());
}

void CGrenadeSmoke::SmokeThink(void)
{
	if (m_hSmokeStack)
	{
		m_hSmokeStack->AcceptInput("TurnOff", this, this, variant_t(), 0);
		m_hSmokeStack->SUB_Remove();
	}
	UTIL_Remove(this);
}

void CGrenadeSmoke::BounceSound(void)
{
	EmitSound("WeaponFrag.Bounce");
}