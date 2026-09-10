#include "cbase.h"
#include "physics_prop_ragdoll.h"
#include "props.h"
#include "vphysics/constraints.h"

class CPropDoorLoose : public CPhysicsProp
{
public:
	DECLARE_CLASS(CPropDoorLoose, CPhysicsProp);
	DECLARE_DATADESC();

	CPropDoorLoose()
	{
		m_bitsDamageFilter = DMG_BLAST; // Default to blast damage
	}

	void Spawn(void)
	{
		BaseClass::Spawn();

		VPhysicsInitNormal(SOLID_VPHYSICS, 0, false);

		IPhysicsObject* pPhysicsObj = VPhysicsGetObject();
		if (pPhysicsObj)
		{
			pPhysicsObj->EnableMotion(true);
		}

		m_iHealth = 100;
		m_takedamage = DAMAGE_YES;

		CreateInternalHinge();
	}

	void CreateInternalHinge(void)
	{
		CBaseEntity* pHinge = CreateEntityByName("phys_hinge");
		if (!pHinge)
			return;

		pHinge->SetAbsOrigin(GetAbsOrigin());
		pHinge->SetAbsAngles(GetAbsAngles());

		if (GetEntityName() == NULL_STRING)
		{
			char szName[64];
			Q_snprintf(szName, sizeof(szName), "loose_door_%d", entindex());
			SetName(AllocPooledString(szName));
		}

		pHinge->KeyValue("attach1", GetDebugName());
		pHinge->KeyValue("targetname", "auto_hinge");
		pHinge->KeyValue("hingeaxis", "0 0 1");
		pHinge->KeyValue("friction", "1.0");

		DispatchSpawn(pHinge);
		pHinge->Activate();

		m_hHinge = pHinge;
	}

	virtual int OnTakeDamage(const CTakeDamageInfo& info)
	{
		// Check if the hinge is intact and matches the configured damage type filter
		if (m_hHinge.Get())
		{
			if (m_bitsDamageFilter == 0 || (info.GetDamageType() & m_bitsDamageFilter))
			{
				BreakHinge();
			}
		}

		return BaseClass::OnTakeDamage(info);
	}

	void BreakHinge(void)
	{
		if (m_hHinge.Get())
		{
			UTIL_Remove(m_hHinge);
			m_hHinge = NULL;
			EmitSound("Wood.Break");
		}
	}

private:
	EHANDLE m_hHinge;
	int m_bitsDamageFilter;
};

LINK_ENTITY_TO_CLASS(prop_door_rotating_loose, CPropDoorLoose);

BEGIN_DATADESC(CPropDoorLoose)
DEFINE_FIELD(m_hHinge, FIELD_EHANDLE),
DEFINE_KEYFIELD(m_bitsDamageFilter, FIELD_INTEGER, "damagetype"),
END_DATADESC()