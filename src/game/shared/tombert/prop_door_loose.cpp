#include "cbase.h"
#include "props.h"
#include "vphysics/constraints.h"

#define SF_PHYSPROP_DEBRIS (1 << 2) // [4] - Don't collide with the player or other debris

class CPropDoorLoose : public CPhysicsProp
{
public:
	DECLARE_CLASS(CPropDoorLoose, CPhysicsProp);
	DECLARE_DATADESC();

	CPropDoorLoose();

	void Spawn(void) override;
	void CreateConstraints(void);
	virtual int OnTakeDamage(const CTakeDamageInfo& info) override;
	virtual void Event_Killed(const CTakeDamageInfo& info) override;
	void BreakConstraints(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

private:
	EHANDLE m_hSocket;
	EHANDLE m_hUpright;
	int m_bitsDamageFilter;
	COutputEvent m_OnBreak;
};

LINK_ENTITY_TO_CLASS(prop_door_loose, CPropDoorLoose);

BEGIN_DATADESC(CPropDoorLoose)
DEFINE_FIELD(m_hSocket, FIELD_EHANDLE),
DEFINE_FIELD(m_hUpright, FIELD_EHANDLE),
DEFINE_KEYFIELD(m_bitsDamageFilter, FIELD_INTEGER, "damagetype"),
DEFINE_OUTPUT(m_OnBreak, "OnBreak"),
END_DATADESC()

CPropDoorLoose::CPropDoorLoose()
{
	m_bitsDamageFilter = DMG_BLAST;
}

void CPropDoorLoose::Spawn(void)
{
	BaseClass::Spawn();

	VPhysicsInitNormal(SOLID_VPHYSICS, 0, false);
	IPhysicsObject* pPhysicsObj = VPhysicsGetObject();
	if (pPhysicsObj)
	{
		pPhysicsObj->EnableMotion(true);

		float fLinearDamping = 0.05f;
		float fAngularDamping = 2.0f;
		pPhysicsObj->SetDamping(&fLinearDamping, &fAngularDamping);
	}

	if (m_iHealth <= 0)
	{
		m_iHealth = 100;
	}
	m_takedamage = DAMAGE_YES;

	CreateConstraints();
}

void CPropDoorLoose::CreateConstraints(void)
{
	if (GetEntityName() == NULL_STRING)
	{
		char szName[64];
		Q_snprintf(szName, sizeof(szName), "loose_door_%d", entindex());
		SetName(AllocPooledString(szName));
	}

	Vector vOrigin = GetAbsOrigin();
	Vector vMins = CollisionProp()->OBBMins();
	Vector vMaxs = CollisionProp()->OBBMaxs();

	Vector vLocalSocketOffset(vMins.x, 0.0f, vMaxs.z * 0.9f);
	Vector vWorldSocketPos;
	VectorTransform(vLocalSocketOffset, EntityToWorldTransform(), vWorldSocketPos);

	CBaseEntity* pSocket = CreateEntityByName("phys_ballsocket");
	if (pSocket)
	{
		pSocket->SetAbsOrigin(vWorldSocketPos);
		pSocket->SetAbsAngles(GetAbsAngles());
		pSocket->KeyValue("attach1", GetDebugName());
		pSocket->KeyValue("targetname", "auto_socket");

		DispatchSpawn(pSocket);
		pSocket->Activate();
		m_hSocket = pSocket;
	}

	CBaseEntity* pUpright = CreateEntityByName("phys_keepupright");
	if (pUpright)
	{
		pUpright->SetAbsOrigin(vOrigin);
		pUpright->SetAbsAngles(GetAbsAngles());
		pUpright->KeyValue("attach1", GetDebugName());
		pUpright->KeyValue("angularlimit", "500");

		DispatchSpawn(pUpright);
		pUpright->Activate();
		m_hUpright = pUpright;
	}
}

int CPropDoorLoose::OnTakeDamage(const CTakeDamageInfo& info)
{
	if (m_hSocket.Get() || m_hUpright.Get())
	{
		if (m_bitsDamageFilter == 0 || (info.GetDamageType() & m_bitsDamageFilter))
		{
			BreakConstraints();
			return 0;
		}
	}

	return BaseClass::OnTakeDamage(info);
}

void CPropDoorLoose::Event_Killed(const CTakeDamageInfo& info)
{
	if (m_hSocket.Get() || m_hUpright.Get())
	{
		BreakConstraints();
	}

	bool bIsWood = false;
	IPhysicsObject* pPhysicsObj = VPhysicsGetObject();
	if (pPhysicsObj && physprops)
	{
		const surfacedata_t* pSurfaceData = physprops->GetSurfaceData(pPhysicsObj->GetMaterialIndex());
		// Check the material character code ('W' represents wood in Source physics properties)
		if (pSurfaceData && pSurfaceData->game.material == 'W')
		{
			bIsWood = true;
		}
	}

	if (bIsWood)
	{
		BaseClass::Event_Killed(info);
	}
	else
	{
		CBaseEntity* pNewProp = CreateEntityByName("prop_physics");
		if (pNewProp)
		{
			pNewProp->SetAbsOrigin(GetAbsOrigin());
			pNewProp->SetAbsAngles(GetAbsAngles());
			pNewProp->SetModel(STRING(GetModelName()));
			pNewProp->AddSpawnFlags(SF_PHYSPROP_DEBRIS);

			DispatchSpawn(pNewProp);
			pNewProp->Activate();
		}

		UTIL_Remove(this);
	}
}

void CPropDoorLoose::BreakConstraints(void)
{
	bool bBrokeAny = false;

	if (m_hSocket.Get())
	{
		UTIL_Remove(m_hSocket);
		m_hSocket = NULL;
		bBrokeAny = true;
	}

	if (m_hUpright.Get())
	{
		UTIL_Remove(m_hUpright);
		m_hUpright = NULL;
		bBrokeAny = true;
	}

	if (bBrokeAny)
	{
		EmitSound("Wood.Break");
		m_OnBreak.FireOutput(this, this);

		IPhysicsObject* pPhysicsObj = VPhysicsGetObject();
		if (pPhysicsObj)
		{
			pPhysicsObj->Wake();
		}
	}
}

void CPropDoorLoose::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (m_hSocket.Get() || m_hUpright.Get())
	{
		IPhysicsObject* pPhysicsObj = VPhysicsGetObject();
		if (pPhysicsObj)
		{
			Vector vForward;
			GetVectors(&vForward, NULL, NULL);
			pPhysicsObj->ApplyTorqueCenter(vForward * 2000.0f);
		}
	}
}