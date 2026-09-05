//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_SMOKE_H
#define GRENADE_SMOKE_H
#pragma once

class CBaseGrenade;
struct edict_t;

CBaseGrenade* Smokegrenade_Create(const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float timer, bool combineSpawned);
bool	Smokegrenade_WasPunted(const CBaseEntity* pEntity);
bool	Smokegrenade_WasCreatedByCombine(const CBaseEntity* pEntity);

#endif // GRENADE_SMOKE_H
