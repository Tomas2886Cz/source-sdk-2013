//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========
//
// Purpose: Simple model entity that randomly moves and changes direction
//			when activated.
//
//=============================================================================

#include "cbase.h"
#include "player.h"
#include "props.h"
#include "gamerules.h"
#include "items.h"
#include "eventlist.h"
#include "npcevent.h"

class CCurrencySmall : public CPhysicsProp
{
public:
	DECLARE_CLASS(CCurrencySmall, CPhysicsProp);
	DECLARE_DATADESC();

	CCurrencySmall()
	{
		m_bActive = false;
	}

	void Spawn(void);
	void Precache(void);

	virtual int	ObjectCaps() { return BaseClass::ObjectCaps() | FCAP_WCEDIT_POSITION; };

	void InputKill(inputdata_t& data);

	virtual void VPhysicsCollision(int index, gamevcollisionevent_t* pEvent);

	void MoveThink(void);

	// Input function
	void InputToggle(inputdata_t& inputData);


private:

	bool	m_bActive;
	float	m_flNextChangeTime;
};

LINK_ENTITY_TO_CLASS(item_currency_small, CCurrencySmall);

// Start of our data description for the class
BEGIN_DATADESC(CCurrencySmall)

// Save/restore our active state
DEFINE_FIELD(m_bActive, FIELD_BOOLEAN),
DEFINE_FIELD(m_flNextChangeTime, FIELD_TIME),

// Links our input name from Hammer to our input member function
DEFINE_INPUTFUNC(FIELD_VOID, "Toggle", InputToggle),

// Declare our think function
DEFINE_THINKFUNC(MoveThink),

END_DATADESC()

// Name of our entity's model
#define	ENTITY_MODEL	"models/items/crafting_metal/resin_puck01.mdl"

//-----------------------------------------------------------------------------
// Purpose: Precache assets used by the entity
//-----------------------------------------------------------------------------
void CCurrencySmall::Precache(void)
{
	PrecacheModel(ENTITY_MODEL);

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the entity's initial state
//-----------------------------------------------------------------------------
void CCurrencySmall::Spawn(void)
{
	Precache();

	SetModel(ENTITY_MODEL);
	SetSolid(SOLID_BBOX);
	UTIL_SetSize(this, -Vector(20, 20, 20), Vector(20, 20, 20));
	CreateVPhysics();
}

//-----------------------------------------------------------------------------
// Purpose: Think function to randomly move the entity
//-----------------------------------------------------------------------------
void CCurrencySmall::MoveThink(void)
{
	// See if we should change direction again
	if (m_flNextChangeTime < gpGlobals->curtime)
	{
		// Randomly take a new direction and speed
		Vector vecNewVelocity = RandomVector(-64.0f, 64.0f);
		SetAbsVelocity(vecNewVelocity);

		// Randomly change it again within one to three seconds
		m_flNextChangeTime = gpGlobals->curtime + random->RandomFloat(1.0f, 3.0f);
	}

	// Snap our facing to where we're heading
	Vector velFacing = GetAbsVelocity();
	QAngle angFacing;
	VectorAngles(velFacing, angFacing);
	SetAbsAngles(angFacing);

	// Think every 20Hz
	SetNextThink(gpGlobals->curtime + 0.05f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCurrencySmall::VPhysicsCollision(int index, gamevcollisionevent_t* pEvent)
{
	float flDamageScale = 1.0f;
	if (FClassnameIs(pEvent->pEntities[!index], "prop_vehicle_airboat") ||
		FClassnameIs(pEvent->pEntities[!index], "prop_vehicle_jeep"))
	{
		flDamageScale = 2.0f;
	}

	m_impactEnergyScale *= flDamageScale;
	BaseClass::VPhysicsCollision(index, pEvent);
	m_impactEnergyScale /= flDamageScale;
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the movement of the entity
//-----------------------------------------------------------------------------
void CCurrencySmall::InputToggle(inputdata_t& inputData)
{
	// Toggle our active state
	if (!m_bActive)
	{
		// Start thinking
		SetThink(&CCurrencySmall::MoveThink);

		SetNextThink(gpGlobals->curtime + 0.05f);

		// Start moving
		SetMoveType(MOVETYPE_VPHYSICS);

		// Force MoveThink() to choose a new speed and direction immediately
		m_flNextChangeTime = gpGlobals->curtime;

		// Update m_bActive to reflect our new state
		m_bActive = true;
	}
	else
	{
		// Stop thinking
		SetThink(NULL);

		// Stop moving
		SetAbsVelocity(vec3_origin);
		SetMoveType(MOVETYPE_NONE);

		m_bActive = false;
	}
}