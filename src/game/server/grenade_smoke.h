#ifndef GRENADE_SMOKE_H
#define GRENADE_SMOKE_H
#pragma once

#include "basegrenade_shared.h"

class CSmokeGrenadeProjectile : public CBaseGrenade
{
	DECLARE_CLASS(CSmokeGrenadeProjectile, CBaseGrenade);
	DECLARE_DATADESC();

public:
	void Spawn(void);
	void Precache(void);
	void Detonate(void);
	void BounceSound(void);
	void SmokeTouch(CBaseEntity* pOther);

	static CSmokeGrenadeProjectile* Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float timer);
};

#endif // GRENADE_SMOKE_H