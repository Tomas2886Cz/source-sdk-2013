//TOMBERT_LOOTABLES_NEW
// prop_container_lootable.cpp
#include "cbase.h"
#include "baseanimating.h"
#include "player.h"
#include "lootables_shared.h"

class CPropContainerLootable : public CBaseAnimating
{
	DECLARE_CLASS(CPropContainerLootable, CBaseAnimating);
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CPropContainerLootable();

	virtual void Spawn(void);
	virtual void Precache(void);

	void InputRefill(inputdata_t& inputdata);
	void GenerateLoot();
	void GiveItemToPlayer(CBasePlayer* pPlayer, int iIndex);

	COutputEvent m_OnLooted;
	COutputEvent m_OnMenuOpened;
	COutputEvent m_OnMenuClosed;
	COutputEvent m_OnUnlocking;
	COutputEvent m_OnUnlockingStopped;

	string_t m_iszLootTable;
	string_t m_iszOpenAnimation;
	string_t m_iszOpenSound;
	string_t m_iszContainerName;
	float m_flUnlockTime;
	int m_iLockState;
	float m_flRarityOverride;
	float m_flLastLootTime;

	CNetworkVar(int, m_iContainerState);
	CNetworkVar(int, m_iLootCount);
	CNetworkArray(int, m_iLootIndices, MAX_LOOT_ITEMS);
	char m_szContainerName[64];
};

LINK_ENTITY_TO_CLASS(prop_container_lootable, CPropContainerLootable);

IMPLEMENT_SERVERCLASS_ST(CPropContainerLootable, DT_PropContainerLootable)
SendPropInt(SENDINFO(m_iContainerState), 4),
SendPropInt(SENDINFO(m_iLootCount), 5),
SendPropArray3(SENDINFO_ARRAY3(m_iLootIndices), SendPropInt(SENDINFO_ARRAY(m_iLootIndices), 9)),
SendPropString(SENDINFO(m_szContainerName)),
END_SEND_TABLE()

BEGIN_DATADESC(CPropContainerLootable)
DEFINE_KEYFIELD(m_iszLootTable, FIELD_STRING, "loot_table"),
DEFINE_KEYFIELD(m_iszContainerName, FIELD_STRING, "container_name"),
DEFINE_KEYFIELD(m_iszOpenAnimation, FIELD_STRING, "open_animation"),
DEFINE_KEYFIELD(m_iszOpenSound, FIELD_STRING, "open_sound"),
DEFINE_KEYFIELD(m_flUnlockTime, FIELD_FLOAT, "unlock_time"),
DEFINE_KEYFIELD(m_iLockState, FIELD_INTEGER, "lock_state"),
DEFINE_KEYFIELD(m_flRarityOverride, FIELD_FLOAT, "rarity_override"),

DEFINE_FIELD(m_iContainerState, FIELD_INTEGER),
DEFINE_FIELD(m_iLootCount, FIELD_INTEGER),
DEFINE_ARRAY(m_iLootIndices, FIELD_INTEGER, MAX_LOOT_ITEMS),
DEFINE_ARRAY(m_szContainerName, FIELD_CHARACTER, 64),

DEFINE_OUTPUT(m_OnLooted, "OnLooted"),
DEFINE_OUTPUT(m_OnMenuOpened, "OnMenuOpened"),
DEFINE_OUTPUT(m_OnMenuClosed, "OnMenuClosed"),
DEFINE_OUTPUT(m_OnUnlocking, "OnUnlocking"),
DEFINE_OUTPUT(m_OnUnlockingStopped, "OnUnlockingStopped"),

DEFINE_INPUTFUNC(FIELD_VOID, "Refill", InputRefill),
END_DATADESC()

CPropContainerLootable::CPropContainerLootable()
{
	m_iLootCount = 0;
	m_iContainerState = LOOT_STATE_UNLOCKED;
	m_szContainerName[0] = 0;
	m_flUnlockTime = 0.0f;
	m_iLockState = 0;
	m_flLastLootTime = -1.0f;

	// Negative means "no override". Zero here would silently delete legendary
	// rolls on every container whose Hammer field was left blank.
	m_flRarityOverride = -1.0f;

	for (int i = 0; i < MAX_LOOT_ITEMS; i++)
		m_iLootIndices.Set(i, -1);
}

void CPropContainerLootable::Precache(void)
{
	if (GetModelName() != NULL_STRING)
		PrecacheModel(STRING(GetModelName()));

	BaseClass::Precache();
}

void CPropContainerLootable::Spawn(void)
{
	BaseClass::Spawn();
	Precache();

	if (GetModelName() != NULL_STRING)
		SetModel(STRING(GetModelName()));

	CLootablesManager::Init();

	Q_strncpy(m_szContainerName, STRING(m_iszContainerName), sizeof(m_szContainerName));

	// Hammer garbage should not produce a fourth state.
	if (m_iLockState < 0 || m_iLockState > 2)
		m_iLockState = 0;

	if (m_iLockState == 2)
		m_iContainerState = (RandomFloat(0.0f, 1.0f) < lootables_global_lock_chance.GetFloat()) ? LOOT_STATE_LOCKED : LOOT_STATE_UNLOCKED;
	else
		m_iContainerState = m_iLockState;

	GenerateLoot();

	SetMoveType(MOVETYPE_NONE);
	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
}

void CPropContainerLootable::GenerateLoot()
{
	m_iLootCount = 0;
	for (int i = 0; i < MAX_LOOT_ITEMS; i++)
	{
		int iIndex = CLootablesManager::GenerateRandomItemIndex(m_flRarityOverride);
		if (iIndex >= 0)
		{
			m_iLootIndices.Set(i, iIndex);
			m_iLootCount++;
		}
		else
		{
			m_iLootIndices.Set(i, -1);
		}
	}
}

void CPropContainerLootable::GiveItemToPlayer(CBasePlayer* pPlayer, int iIndex)
{
	if (iIndex < 0 || iIndex >= m_iLootCount)
		return;

	int iItemDefIndex = m_iLootIndices[iIndex];
	LootItemDef_t* pDef = CLootablesManager::GetItemByIndex(iItemDefIndex);
	if (!pDef)
		return;

	if (!pDef->bIsDummy)
	{
		CBaseEntity* pGiven = pPlayer->GiveNamedItem(pDef->szClassName);
		if (!pGiven)
			DevWarning("lootables: GiveNamedItem failed for '%s'\n", pDef->szClassName);
	}

	for (int i = iIndex; i < m_iLootCount - 1; i++)
		m_iLootIndices.Set(i, m_iLootIndices[i + 1]);

	m_iLootCount--;
	m_iLootIndices.Set(m_iLootCount, -1);

	m_OnLooted.FireOutput(this, this);

	if (m_iLootCount == 0)
		m_iContainerState = LOOT_STATE_EMPTY;
}

void CPropContainerLootable::InputRefill(inputdata_t& inputdata)
{
	GenerateLoot();
	m_iContainerState = (m_iLootCount > 0) ? LOOT_STATE_UNLOCKED : LOOT_STATE_EMPTY;
}

// Traces the local player's eyes at whatever they are looking at. Shared by
// every loot_* command below so the behaviour matches the client's trace.
static CPropContainerLootable* Lootables_TraceContainer(CBasePlayer* pPlayer)
{
	if (!pPlayer)
		return NULL;

	trace_t tr;
	UTIL_TraceLine(pPlayer->EyePosition(), pPlayer->EyePosition() + pPlayer->EyeDirection3D() * 84.0f, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction >= 1.0f || !tr.m_pEnt)
		return NULL;

	return dynamic_cast<CPropContainerLootable*>(tr.m_pEnt);
}

CON_COMMAND_F(loot_item, "Loot the selected item from the container you are looking at", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	if (args.ArgC() < 2)
		return;

	CBasePlayer* pPlayer = ToBasePlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CPropContainerLootable* pContainer = Lootables_TraceContainer(pPlayer);
	if (!pContainer || pContainer->m_iContainerState != LOOT_STATE_UNLOCKED)
		return;

	// Throttle: a held key or a scripted spam loop must not drain the container
	// in a single tick burst.
	if (gpGlobals->curtime - pContainer->m_flLastLootTime < 0.2f)
		return;
	pContainer->m_flLastLootTime = gpGlobals->curtime;

	pContainer->GiveItemToPlayer(pPlayer, atoi(args[1]));
}

CON_COMMAND_F(loot_open_menu, "Notify the server that the quickloot menu opened", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	CBasePlayer* pPlayer = ToBasePlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CPropContainerLootable* pContainer = Lootables_TraceContainer(pPlayer);
	if (!pContainer || pContainer->m_iContainerState != LOOT_STATE_UNLOCKED)
		return;

	pContainer->m_OnMenuOpened.FireOutput(pPlayer, pContainer);

	if (pContainer->m_iszOpenAnimation != NULL_STRING)
	{
		int iSeq = pContainer->LookupSequence(STRING(pContainer->m_iszOpenAnimation));
		if (iSeq >= 0)
			pContainer->ResetSequence(iSeq);
		else
			DevWarning("lootables: unknown open_animation '%s'\n", STRING(pContainer->m_iszOpenAnimation));
	}

	if (pContainer->m_iszOpenSound != NULL_STRING)
		pContainer->EmitSound(STRING(pContainer->m_iszOpenSound));
}

CON_COMMAND_F(loot_close_menu, "Notify the server that the quickloot menu closed", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	CBasePlayer* pPlayer = ToBasePlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CPropContainerLootable* pContainer = Lootables_TraceContainer(pPlayer);
	if (!pContainer)
		return;

	pContainer->m_OnMenuClosed.FireOutput(pPlayer, pContainer);
}

CON_COMMAND_F(loot_unlock_start, "Notify the server that a player started unlocking", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	CBasePlayer* pPlayer = ToBasePlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CPropContainerLootable* pContainer = Lootables_TraceContainer(pPlayer);
	if (!pContainer || pContainer->m_iContainerState != LOOT_STATE_LOCKED)
		return;

	pContainer->m_OnUnlocking.FireOutput(pPlayer, pContainer);
}

CON_COMMAND_F(loot_unlock_stop, "Notify the server that a player gave up unlocking", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	CBasePlayer* pPlayer = ToBasePlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CPropContainerLootable* pContainer = Lootables_TraceContainer(pPlayer);
	if (!pContainer)
		return;

	pContainer->m_OnUnlockingStopped.FireOutput(pPlayer, pContainer);
}

CON_COMMAND_F(loot_unlock_finish, "Notify the server that the unlock hold completed", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	CBasePlayer* pPlayer = ToBasePlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CPropContainerLootable* pContainer = Lootables_TraceContainer(pPlayer);
	if (!pContainer || pContainer->m_iContainerState != LOOT_STATE_LOCKED)
		return;

	pContainer->m_iContainerState = LOOT_STATE_UNLOCKED;
}
//TOMBERT_LOOTABLES_NEW_konec