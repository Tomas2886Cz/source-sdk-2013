//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
//=============================================================================//

#include "cbase.h"

#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "hl2mp_player.h"
#include "globalstate.h"
#include "game.h"
#include "gamerules.h"
#include "hl2mp_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "hl2mp_gamerules.h"
#include "KeyValues.h"
#include "team.h"
#include "weapon_hl2mpbase.h"
#include "grenade_satchel.h"
#include "eventqueue.h"
#include "gamestats.h"
#include "ammodef.h"
#include "NextBot.h"

#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#include "ilagcompensationmanager.h"

int g_iLastCitizenModel = 0;
int g_iLastCombineModel = 0;

CBaseEntity* g_pLastCombineSpawn = NULL;
CBaseEntity* g_pLastRebelSpawn = NULL;
extern CBaseEntity* g_pLastSpawn;

ConVar hl2mp_spawn_frag_fallback_radius("hl2mp_spawn_frag_fallback_radius", "48", FCVAR_NONE, "If no spawns are available, kill players with this radius to allow new players to spawn.");

// Customizable Downed ConVars
ConVar sv_downed_enable("sv_downed_enable", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Enable or disable the downed state feature (1 = Enabled, 0 = Disabled).");
ConVar sv_downed_bleedout_time("sv_downed_bleedout_time", "30", FCVAR_REPLICATED | FCVAR_NOTIFY, "Base time in seconds before a downed player bleeds out.");
ConVar sv_downed_speed("sv_downed_speed", "40.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Movement crawl speed while downed.");

ConVar sv_downed_time_1("sv_downed_time_1", "30", FCVAR_REPLICATED | FCVAR_NOTIFY, "Bleedout time in seconds for the 1st down.");
ConVar sv_downed_time_2("sv_downed_time_2", "20", FCVAR_REPLICATED | FCVAR_NOTIFY, "Bleedout time in seconds for the 2nd down.");
ConVar sv_downed_time_3("sv_downed_time_3", "10", FCVAR_REPLICATED | FCVAR_NOTIFY, "Bleedout time in seconds for the 3rd down.");
ConVar sv_downed_time_4("sv_downed_time_4", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "Bleedout time in seconds for the 4th down.");
ConVar sv_downed_time_5("sv_downed_time_5", "3", FCVAR_REPLICATED | FCVAR_NOTIFY, "Bleedout time in seconds for the 5th down and beyond.");

ConVar sv_downed_revive_enable("sv_downed_revive_enable", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Allows or blocks teammates from reviving downed players.");
ConVar sv_downed_revive_duration("sv_downed_revive_duration", "5.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Time required in seconds for a teammate to revive a downed player.", true, 0.1f, false, 0.0f);

void RegisterDownedConVars()
{
	if (!cvar)
		return;

	static bool s_bRegistered = false;
	if (s_bRegistered)
		return;

	cvar->RegisterConCommand(&sv_downed_enable);
	cvar->RegisterConCommand(&sv_downed_bleedout_time);
	cvar->RegisterConCommand(&sv_downed_speed);
	cvar->RegisterConCommand(&sv_downed_time_1);
	cvar->RegisterConCommand(&sv_downed_time_2);
	cvar->RegisterConCommand(&sv_downed_time_3);
	cvar->RegisterConCommand(&sv_downed_time_4);
	cvar->RegisterConCommand(&sv_downed_time_5);
	cvar->RegisterConCommand(&sv_downed_revive_enable);
	cvar->RegisterConCommand(&sv_downed_revive_duration);
	s_bRegistered = true;
}

#define HL2MP_COMMAND_MAX_RATE 0.3

void DropPrimedFragGrenade(CHL2MP_Player* pPlayer, CBaseCombatWeapon* pGrenade);

LINK_ENTITY_TO_CLASS(player, CHL2MP_Player);

LINK_ENTITY_TO_CLASS(info_player_combine, CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_rebel, CPointEntity);

BEGIN_SEND_TABLE_NOBASE(CHL2MP_Player, DT_HL2MPLocalPlayerExclusive)
SendPropVectorXY(SENDINFO(m_vecOrigin), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY),
SendPropFloat(SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ),

SendPropFloat(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f),
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN),

END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE(CHL2MP_Player, DT_HL2MPNonLocalPlayerExclusive)
SendPropVectorXY(SENDINFO(m_vecOrigin), -1, SPROP_COORD_MP_LOWPRECISION | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY),
SendPropFloat(SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_COORD_MP_LOWPRECISION | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ),

SendPropFloat(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f),
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN),

END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST(CHL2MP_Player, DT_HL2MP_Player)
SendPropExclude("DT_BaseEntity", "m_vecOrigin"),

SendPropExclude("DT_BasePlayer", "m_flMaxspeed"),

SendPropDataTable("hl2mplocaldata", 0, &REFERENCE_SEND_TABLE(DT_HL2MPLocalPlayerExclusive), SendProxy_SendLocalDataTable),
SendPropDataTable("hl2mpnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_HL2MPNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable),

SendPropEHandle(SENDINFO(m_hRagdoll)),
SendPropInt(SENDINFO(m_iSpawnInterpCounter)),
SendPropInt(SENDINFO(m_iPlayerSoundType)),

// Downed state datatable network properties
SendPropBool(SENDINFO(m_bIsDowned)),
SendPropFloat(SENDINFO(m_flReviveProgress)),
SendPropFloat(SENDINFO(m_flBleedoutTimer)),
SendPropEHandle(SENDINFO(m_hRevivingTeammate)),

SendPropExclude("DT_BaseAnimating", "m_flPoseParameter"),
SendPropExclude("DT_BaseFlex", "m_viewtarget"),
END_SEND_TABLE()

BEGIN_DATADESC(CHL2MP_Player)
END_DATADESC()

BEGIN_ENT_SCRIPTDESC(CHL2MP_Player, CHL2_Player, "Half-Life 2: Deathmatch Player")
END_SCRIPTDESC();

const char* g_ppszRandomCitizenModels[] =
{
	"models/humans/group03/male_01.mdl",
	"models/humans/group03/male_02.mdl",
	"models/humans/group03/female_01.mdl",
	"models/humans/group03/male_03.mdl",
	"models/humans/group03/female_02.mdl",
	"models/humans/group03/male_04.mdl",
	"models/humans/group03/female_03.mdl",
	"models/humans/group03/male_05.mdl",
	"models/humans/group03/female_04.mdl",
	"models/humans/group03/male_06.mdl",
	"models/humans/group03/female_06.mdl",
	"models/humans/group03/male_07.mdl",
	"models/humans/group03/female_07.mdl",
	"models/humans/group03/male_08.mdl",
	"models/humans/group03/male_09.mdl",
};

const char* g_ppszRandomCombineModels[] =
{
	"models/combine_soldier.mdl",
	"models/combine_soldier_prisonguard.mdl",
	"models/combine_super_soldier.mdl",
	"models/police.mdl",
};

#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 5.0f
#define TEAM_CHANGE_INTERVAL 5.0f

#define HL2MPPLAYER_PHYSDAMAGE_SCALE 4.0f

#pragma warning( disable : 4355 )

CHL2MP_Player::CHL2MP_Player() : m_PlayerAnimState(this)
{
	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	m_iSpawnInterpCounter = 0;

	m_bEnterObserver = false;
	m_bReady = false;

	m_bIsDowned = false;
	m_flReviveProgress = 0.0f;
	m_flBleedoutTimer = 0.0f;
	m_hRevivingTeammate = NULL;
	m_iDownedCount = 0;

	BaseClass::ChangeTeam(0);
}

CHL2MP_Player::~CHL2MP_Player(void)
{

}

void CHL2MP_Player::UpdateOnRemove(void)
{
	if (m_hRagdoll)
	{
		UTIL_RemoveImmediate(m_hRagdoll);
		m_hRagdoll = NULL;
	}

	BaseClass::UpdateOnRemove();
}

void CHL2MP_Player::Precache(void)
{
	BaseClass::Precache();

	PrecacheModel("sprites/glow01.vmt");

	int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);
	int i;

	for (i = 0; i < nHeads; ++i)
		PrecacheModel(g_ppszRandomCitizenModels[i]);

	nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

	for (i = 0; i < nHeads; ++i)
		PrecacheModel(g_ppszRandomCombineModels[i]);

	PrecacheFootStepSounds();

	PrecacheScriptSound("NPC_MetroPolice.Die");
	PrecacheScriptSound("NPC_CombineS.Die");
	PrecacheScriptSound("NPC_Citizen.die");
}

void CHL2MP_Player::GiveAllItems(void)
{
	EquipSuit();

	CBasePlayer::GiveAmmo(255, "Pistol");
	CBasePlayer::GiveAmmo(255, "AR2");
	CBasePlayer::GiveAmmo(5, "AR2AltFire");
	CBasePlayer::GiveAmmo(255, "SMG1");
	CBasePlayer::GiveAmmo(1, "smg1_grenade");
	CBasePlayer::GiveAmmo(255, "Buckshot");
	CBasePlayer::GiveAmmo(32, "357");
	CBasePlayer::GiveAmmo(3, "rpg_round");
	CBasePlayer::GiveAmmo(16, "XBowBolt");

	CBasePlayer::GiveAmmo(1, "grenade");
	CBasePlayer::GiveAmmo(2, "slam");

	GiveNamedItem("weapon_crowbar");
	GiveNamedItem("weapon_stunstick");
	GiveNamedItem("weapon_pistol");
	GiveNamedItem("weapon_357");

	GiveNamedItem("weapon_smg1");
	GiveNamedItem("weapon_ar2");

	GiveNamedItem("weapon_shotgun");
	GiveNamedItem("weapon_frag");

	GiveNamedItem("weapon_crossbow");

	GiveNamedItem("weapon_rpg");

	GiveNamedItem("weapon_slam");

	GiveNamedItem("weapon_physcannon");
}

void CHL2MP_Player::GiveDefaultItems(void)
{
	EquipSuit();

	GiveNamedItem("weapon_crowbar");
	GiveNamedItem("weapon_medkit");
	GiveNamedItem("weapon_flaregun");

	CBasePlayer::GiveAmmo(4, "FlareRound");
	CBasePlayer::GiveAmmo(25, "Scrap_Medical");
	CBasePlayer::GiveAmmo(25, "Scrap_Weapon");
	CBasePlayer::GiveAmmo(25, "Scrap_Utility");

	GiveNamedItem("weapon_blueprint_medkit");

	const char* szDefaultWeaponName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_defaultweapon");

	CBaseCombatWeapon* pDefaultWeapon = Weapon_OwnsThisType(szDefaultWeaponName);

	if (pDefaultWeapon)
	{
		Weapon_Switch(pDefaultWeapon);
	}
	else
	{
		Weapon_Switch(Weapon_OwnsThisType("weapon_crowbar"));
	}
}

void CHL2MP_Player::PickDefaultSpawnTeam(void)
{
	if (GetTeamNumber() == 0)
	{
		if (HL2MPRules()->IsTeamplay() == false)
		{
			if (GetModelPtr() == NULL)
			{
				const char* szModelName = NULL;
				szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

				if (ValidatePlayerModel(szModelName) == false)
				{
					char szReturnString[512];

					Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel models/combine_soldier.mdl\n");
					engine->ClientCommand(edict(), szReturnString);
				}

				ChangeTeam(TEAM_UNASSIGNED);
			}
		}
		else
		{
			CTeam* pCombine = g_Teams[TEAM_COMBINE];
			CTeam* pRebels = g_Teams[TEAM_REBELS];

			if (pCombine == NULL || pRebels == NULL)
			{
				ChangeTeam(random->RandomInt(TEAM_COMBINE, TEAM_REBELS));
			}
			else
			{
				if (pCombine->GetNumPlayers() > pRebels->GetNumPlayers())
				{
					ChangeTeam(TEAM_REBELS);
				}
				else if (pCombine->GetNumPlayers() < pRebels->GetNumPlayers())
				{
					ChangeTeam(TEAM_COMBINE);
				}
				else
				{
					ChangeTeam(random->RandomInt(TEAM_COMBINE, TEAM_REBELS));
				}
			}
		}
	}
}

void CHL2MP_Player::Spawn(void)
{
	RegisterDownedConVars();

	m_flNextModelChangeTime = 0.0f;
	m_flNextTeamChangeTime = 0.0f;

	m_bIsDowned = false;
	m_flReviveProgress = 0.0f;
	m_flBleedoutTimer = 0.0f;
	m_hRevivingTeammate = NULL;
	m_iDownedCount = 0;

	PickDefaultSpawnTeam();

	BaseClass::Spawn();

	if (!IsObserver())
	{
		pl.deadflag = false;
		RemoveSolidFlags(FSOLID_NOT_SOLID);

		RemoveEffects(EF_NODRAW);

		GiveDefaultItems();
	}

	SetNumAnimOverlays(3);
	ResetAnimation();

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;

	AddFlag(FL_ONGROUND);

	m_impactEnergyScale = HL2MPPLAYER_PHYSDAMAGE_SCALE;

	if (HL2MPRules()->IsIntermission())
	{
		AddFlag(FL_FROZEN);
	}
	else
	{
		RemoveFlag(FL_FROZEN);
	}

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);

	m_bReady = false;
}

bool CHL2MP_Player::ValidatePlayerModel(const char* pModel)
{
	int iModels = ARRAYSIZE(g_ppszRandomCitizenModels);
	int i;

	for (i = 0; i < iModels; ++i)
	{
		if (!Q_stricmp(g_ppszRandomCitizenModels[i], pModel))
		{
			return true;
		}
	}

	iModels = ARRAYSIZE(g_ppszRandomCombineModels);

	for (i = 0; i < iModels; ++i)
	{
		if (!Q_stricmp(g_ppszRandomCombineModels[i], pModel))
		{
			return true;
		}
	}

	return false;
}

ConVar hl2mp_allow_pickup("hl2mp_allow_pickup", "0", FCVAR_GAMEDLL);

void CHL2MP_Player::PickupObject(CBaseEntity* pObject, bool bLimitMassAndSize)
{
	// Disallow grabbing objects with the USE key while downed
	if (sv_downed_enable.GetBool() && m_bIsDowned)
		return;

	if (!hl2mp_allow_pickup.GetBool())
		return;

	return BaseClass::PickupObject(pObject, bLimitMassAndSize);
}

void CHL2MP_Player::PlayerRunCommand(CUserCmd* ucmd, IMoveHelper* moveHelper)
{
	// Intercept user commands directly before movement processing
	if (sv_downed_enable.GetBool() && m_bIsDowned && ucmd)
	{
		ucmd->buttons &= ~IN_JUMP;
	}

	BaseClass::PlayerRunCommand(ucmd, moveHelper);
}

void CHL2MP_Player::SetPlayerTeamModel(void)
{
	const char* szModelName = NULL;
	szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

	int modelIndex = modelinfo->GetModelIndex(szModelName);

	if (modelIndex == -1 || ValidatePlayerModel(szModelName) == false)
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", szModelName);
		engine->ClientCommand(edict(), szReturnString);
	}

	if (GetTeamNumber() == TEAM_COMBINE)
	{
		if (Q_stristr(szModelName, "models/human"))
		{
			int nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

			g_iLastCombineModel = (g_iLastCombineModel + 1) % nHeads;
			szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];
		}

		m_iModelType = TEAM_COMBINE;
	}
	else if (GetTeamNumber() == TEAM_REBELS)
	{
		if (!Q_stristr(szModelName, "models/human"))
		{
			int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);

			g_iLastCitizenModel = (g_iLastCitizenModel + 1) % nHeads;
			szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];
		}

		m_iModelType = TEAM_REBELS;
	}

	SetModel(szModelName);
	SetupPlayerSoundsByModel(szModelName);

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetPlayerModel(void)
{
	const char* szModelName = NULL;
	const char* pszCurrentModelName = modelinfo->GetModelName(GetModel());

	szModelName = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_playermodel");

	if (ValidatePlayerModel(szModelName) == false)
	{
		char szReturnString[512];

		if (ValidatePlayerModel(pszCurrentModelName) == false)
		{
			pszCurrentModelName = "models/Combine_Soldier.mdl";
		}

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", pszCurrentModelName);
		engine->ClientCommand(edict(), szReturnString);

		szModelName = pszCurrentModelName;
	}

	if (GetTeamNumber() == TEAM_COMBINE)
	{
		int nHeads = ARRAYSIZE(g_ppszRandomCombineModels);

		g_iLastCombineModel = (g_iLastCombineModel + 1) % nHeads;
		szModelName = g_ppszRandomCombineModels[g_iLastCombineModel];

		m_iModelType = TEAM_COMBINE;
	}
	else if (GetTeamNumber() == TEAM_REBELS)
	{
		int nHeads = ARRAYSIZE(g_ppszRandomCitizenModels);

		g_iLastCitizenModel = (g_iLastCitizenModel + 1) % nHeads;
		szModelName = g_ppszRandomCitizenModels[g_iLastCitizenModel];

		m_iModelType = TEAM_REBELS;
	}
	else
	{
		if (Q_strlen(szModelName) == 0)
		{
			szModelName = g_ppszRandomCitizenModels[0];
		}

		if (Q_stristr(szModelName, "models/human"))
		{
			m_iModelType = TEAM_REBELS;
		}
		else
		{
			m_iModelType = TEAM_COMBINE;
		}
	}

	int modelIndex = modelinfo->GetModelIndex(szModelName);

	if (modelIndex == -1)
	{
		szModelName = "models/Combine_Soldier.mdl";
		m_iModelType = TEAM_COMBINE;

		char szReturnString[512];

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", szModelName);
		engine->ClientCommand(edict(), szReturnString);
	}

	SetModel(szModelName);
	SetupPlayerSoundsByModel(szModelName);

	m_flNextModelChangeTime = gpGlobals->curtime + MODEL_CHANGE_INTERVAL;
}

void CHL2MP_Player::SetupPlayerSoundsByModel(const char* pModelName)
{
	if (Q_stristr(pModelName, "models/human"))
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_CITIZEN;
	}
	else if (Q_stristr(pModelName, "police"))
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_METROPOLICE;
	}
	else if (Q_stristr(pModelName, "combine"))
	{
		m_iPlayerSoundType = (int)PLAYER_SOUNDS_COMBINESOLDIER;
	}
}

void CHL2MP_Player::ResetAnimation(void)
{
	if (IsAlive())
	{
		SetSequence(-1);
		SetActivity(ACT_INVALID);

		if (!GetAbsVelocity().x && !GetAbsVelocity().y)
			SetAnimation(PLAYER_IDLE);
		else if ((GetAbsVelocity().x || GetAbsVelocity().y) && (GetFlags() & FL_ONGROUND))
			SetAnimation(PLAYER_WALK);
		else if (GetWaterLevel() > 1)
			SetAnimation(PLAYER_WALK);
		else if ((GetFlags() & FL_ONGROUND) != FL_ONGROUND)
			SetAnimation(PLAYER_JUMP);
		else
			SetAnimation(PLAYER_IDLE);
	}
}

bool CHL2MP_Player::Weapon_Switch(CBaseCombatWeapon* pWeapon, int viewmodelindex)
{
	if (sv_downed_enable.GetBool() && m_bIsDowned)
		return false;

	bool bRet = BaseClass::Weapon_Switch(pWeapon, viewmodelindex);

	if (bRet == true)
	{
		ResetAnimation();
	}

	return bRet;
}

void CHL2MP_Player::DownPlayer(const CTakeDamageInfo& info)
{
	m_bIsDowned = true;
	m_flReviveProgress = 0.0f;
	m_flBleedoutTimer = 1.0f;
	m_hRevivingTeammate = NULL;

	m_iDownedCount++;

	SetHealth(100);
	AddFlag(FL_DUCKING);
	SetViewOffset(VEC_DUCK_VIEW);

	IGameEvent* event = gameeventmanager->CreateEvent("player_downed");
	if (event)
	{
		event->SetInt("userid", GetUserID());
		gameeventmanager->FireEvent(event);
	}
}

void CHL2MP_Player::RevivePlayer(CHL2MP_Player* pReviver)
{
	m_bIsDowned = false;
	m_flReviveProgress = 0.0f;
	m_flBleedoutTimer = 0.0f;
	m_hRevivingTeammate = NULL;

	RemoveFlag(FL_DUCKING);
	SetViewOffset(VEC_VIEW);
	SetHealth(30);
}

void CHL2MP_Player::PlayerUse(void)
{
	// Block standard world/object interactions while downed
	if (sv_downed_enable.GetBool() && m_bIsDowned)
		return;

	trace_t tr;
	Vector forward;
	EyeVectors(&forward);
	Vector start = EyePosition();
	Vector end = start + (forward * 72.0f);

	UTIL_TraceLine(start, end, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	if (tr.m_pEnt && tr.m_pEnt->IsPlayer())
	{
		CHL2MP_Player* pTarget = ToHL2MPPlayer(tr.m_pEnt);

		if (pTarget &&
			sv_downed_enable.GetBool() &&
			sv_downed_revive_enable.GetBool() &&
			pTarget->IsDowned() &&
			pTarget->GetTeamNumber() == GetTeamNumber())
		{
			// Mark active revive relationship
			m_hRevivingTeammate = pTarget;
			pTarget->m_hRevivingTeammate = this;

			// Advance progress based on sv_downed_revive_duration ConVar
			float flDuration = MAX(0.1f, sv_downed_revive_duration.GetFloat());
			pTarget->m_flReviveProgress += (gpGlobals->frametime / flDuration);

			if (pTarget->m_flReviveProgress >= 1.0f)
			{
				pTarget->RevivePlayer(this);
				m_hRevivingTeammate = NULL;
			}
			return;
		}
	}

	// If player is not looking at a downed teammate, clear any active revive link
	if (m_hRevivingTeammate)
	{
		m_hRevivingTeammate->m_flReviveProgress = 0.0f;
		m_hRevivingTeammate->m_hRevivingTeammate = NULL;
		m_hRevivingTeammate = NULL;
	}

	BaseClass::PlayerUse();
}

void CHL2MP_Player::PreThink(void)
{
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	vTempAngles = EyeAngles();

	if (vTempAngles[PITCH] > 180.0f)
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles(vTempAngles);

	// If downed feature is disabled mid-game while downed, kill cleanly
	if (!sv_downed_enable.GetBool() && m_bIsDowned)
	{
		m_bIsDowned = false;
		m_hRevivingTeammate = NULL;
		CBaseEntity* pWorld = CBaseEntity::Instance(0);
		if (!pWorld)
			pWorld = this;

		CTakeDamageInfo fatalInfo(pWorld, pWorld, 1000.0f, DMG_GENERIC);
		Event_Killed(fatalInfo);
	}

	// Handle Reviving State Freeze (Speed = 0, Attack = disabled)
	if (sv_downed_enable.GetBool() && m_hRevivingTeammate != NULL)
	{
		SetMaxSpeed(0.001f);

		bool bButtonReleased = !(m_nButtons & IN_USE);
		bool bTooFar = EyePosition().DistTo(m_hRevivingTeammate->EyePosition()) > 96.0f;
		bool bReviveTurnedOff = !sv_downed_revive_enable.GetBool();

		if (bButtonReleased || bTooFar || bReviveTurnedOff)
		{
			m_hRevivingTeammate->m_flReviveProgress = 0.0f;
			m_hRevivingTeammate->m_hRevivingTeammate = NULL;
			m_hRevivingTeammate = NULL;
		}
	}

	if (sv_downed_enable.GetBool() && m_bIsDowned && m_lifeState == LIFE_ALIVE)
	{
		// Forcefully strip ALL jump input buttons
		m_nButtons &= ~IN_JUMP;
		m_afButtonPressed &= ~IN_JUMP;
		m_afButtonReleased &= ~IN_JUMP;

		// Freeze speed to 0 if being actively revived, otherwise apply crawl speed
		if (m_hRevivingTeammate != NULL)
		{
			SetMaxSpeed(0.001f);
		}
		else
		{
			SetMaxSpeed(sv_downed_speed.GetFloat());

			// Only deplete bleedout timer if NOT currently being revived
			float flEffectiveBleedoutTime = 30.0f;
			switch (m_iDownedCount)
			{
			case 1:
				flEffectiveBleedoutTime = sv_downed_time_1.GetFloat();
				break;
			case 2:
				flEffectiveBleedoutTime = sv_downed_time_2.GetFloat();
				break;
			case 3:
				flEffectiveBleedoutTime = sv_downed_time_3.GetFloat();
				break;
			case 4:
				flEffectiveBleedoutTime = sv_downed_time_4.GetFloat();
				break;
			default:
				flEffectiveBleedoutTime = sv_downed_time_5.GetFloat();
				break;
			}

			flEffectiveBleedoutTime = MAX(1.0f, flEffectiveBleedoutTime);
			m_flBleedoutTimer -= (gpGlobals->frametime / flEffectiveBleedoutTime);

			if (m_flBleedoutTimer <= 0.0f)
			{
				m_flBleedoutTimer = 0.0f;
				m_bIsDowned = false;
				m_hRevivingTeammate = NULL;

				CBaseEntity* pWorld = CBaseEntity::Instance(0);
				if (!pWorld)
					pWorld = this;

				CTakeDamageInfo fatalInfo(pWorld, pWorld, 1000.0f, DMG_GENERIC);
				Event_Killed(fatalInfo);
			}
		}
	}

	BaseClass::PreThink();
	State_PreThink();

	m_vecTotalBulletForce = vec3_origin;
	SetLocalAngles(vOldAngles);
}

void CHL2MP_Player::PostThink(void)
{
	BaseClass::PostThink();

	if (GetFlags() & FL_DUCKING)
	{
		SetCollisionBounds(VEC_CROUCH_TRACE_MIN, VEC_CROUCH_TRACE_MAX);
	}

	m_PlayerAnimState.Update();

	m_angEyeAngles = EyeAngles();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles(angles);
}

void CHL2MP_Player::PlayerDeathThink()
{
	if (!IsObserver())
	{
		BaseClass::PlayerDeathThink();
	}
}

void CHL2MP_Player::ItemPreFrame(void)
{
	if (GetFlags() & FL_FROZEN)
		return;

	if (sv_downed_enable.GetBool() && m_bIsDowned)
	{
		// Scrub attacks, reload, use, and jump inputs
		m_nButtons &= ~(IN_ATTACK | IN_ATTACK2 | IN_RELOAD | IN_USE | IN_JUMP);
		return;
	}

	// Block attacks while actively reviving a teammate
	if (sv_downed_enable.GetBool() && m_hRevivingTeammate != NULL)
	{
		m_nButtons &= ~(IN_ATTACK | IN_ATTACK2);
	}

	if (m_nButtons & IN_ZOOM)
	{
		m_nButtons &= ~(IN_ATTACK | IN_ATTACK2);
	}

	BaseClass::ItemPreFrame();
}

void CHL2MP_Player::ItemPostFrame(void)
{
	if (GetFlags() & FL_FROZEN)
		return;

	if (sv_downed_enable.GetBool() && m_bIsDowned)
	{
		m_nButtons &= ~(IN_ATTACK | IN_ATTACK2 | IN_RELOAD | IN_USE | IN_JUMP);
		return;
	}

	if (sv_downed_enable.GetBool() && m_hRevivingTeammate != NULL)
	{
		m_nButtons &= ~(IN_ATTACK | IN_ATTACK2);
	}

	BaseClass::ItemPostFrame();
}

void CHL2MP_Player::FireBullets(const FireBulletsInfo_t& info)
{
	if (sv_downed_enable.GetBool() && (m_bIsDowned || m_hRevivingTeammate != NULL))
		return;

	lagcompensation->StartLagCompensation(this, this->GetCurrentCommand());

	FireBulletsInfo_t modinfo = info;

	CWeaponHL2MPBase* pWeapon = dynamic_cast<CWeaponHL2MPBase*>(GetActiveWeapon());

	if (pWeapon)
	{
		modinfo.m_iPlayerDamage = modinfo.m_flDamage = pWeapon->GetHL2MPWpnData().m_iPlayerDamage;
	}

	NoteWeaponFired();

	BaseClass::FireBullets(modinfo);

	lagcompensation->FinishLagCompensation(this);

	if (pWeapon)
		this->OnMyWeaponFired(pWeapon);
}

void CHL2MP_Player::OnMyWeaponFired(CBaseCombatWeapon* weapon)
{
	BaseClass::OnMyWeaponFired(weapon);

	TheNextBots().OnWeaponFired(this, weapon);
}

void CHL2MP_Player::NoteWeaponFired(void)
{
	Assert(m_pCurrentCommand);
	if (m_pCurrentCommand)
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;

bool CHL2MP_Player::WantsLagCompensationOnEntity(const CBasePlayer* pPlayer, const CUserCmd* pCmd, const CBitVec<MAX_EDICTS>* pEntityTransmitBits) const
{
	if (!(pCmd->buttons & IN_ATTACK) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5))
		return false;

	if (pEntityTransmitBits && !pEntityTransmitBits->Get(pPlayer->entindex()))
		return false;

	const Vector& vMyOrigin = GetAbsOrigin();
	const Vector& vHisOrigin = pPlayer->GetAbsOrigin();

	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	if (vHisOrigin.DistTo(vMyOrigin) < maxDistance)
		return true;

	Vector vForward;
	AngleVectors(pCmd->viewangles, &vForward);

	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize(vDiff);

	float flCosAngle = 0.707107f;
	if (vForward.Dot(vDiff) < flCosAngle)
		return false;

	return true;
}

Activity CHL2MP_Player::TranslateTeamActivity(Activity ActToTranslate)
{
	if (m_iModelType == TEAM_COMBINE)
		return ActToTranslate;

	if (ActToTranslate == ACT_RUN)
		return ACT_RUN_AIM_AGITATED;

	if (ActToTranslate == ACT_IDLE)
		return ACT_IDLE_AIM_AGITATED;

	if (ActToTranslate == ACT_WALK)
		return ACT_WALK_AIM_AGITATED;

	return ActToTranslate;
}

extern ConVar hl2_normspeed;

void CHL2MP_Player::SetAnimation(PLAYER_ANIM playerAnim)
{
	int animDesired;

	float speed;

	speed = GetAbsVelocity().Length2D();

	if (GetFlags() & (FL_FROZEN | FL_ATCONTROLS))
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	Activity idealActivity = ACT_HL2MP_RUN;

	if (playerAnim == PLAYER_JUMP)
	{
		idealActivity = ACT_HL2MP_JUMP;
	}
	else if (playerAnim == PLAYER_DIE)
	{
		if (m_lifeState == LIFE_ALIVE)
		{
			return;
		}
	}
	else if (playerAnim == PLAYER_ATTACK1)
	{
		if (GetActivity() == ACT_HOVER ||
			GetActivity() == ACT_SWIM ||
			GetActivity() == ACT_HOP ||
			GetActivity() == ACT_LEAP ||
			GetActivity() == ACT_DIESIMPLE)
		{
			idealActivity = GetActivity();
		}
		else
		{
			idealActivity = ACT_HL2MP_GESTURE_RANGE_ATTACK;
		}
	}
	else if (playerAnim == PLAYER_RELOAD)
	{
		idealActivity = ACT_HL2MP_GESTURE_RELOAD;
	}
	else if (playerAnim == PLAYER_IDLE || playerAnim == PLAYER_WALK)
	{
		if (!(GetFlags() & FL_ONGROUND) && GetActivity() == ACT_HL2MP_JUMP)
		{
			idealActivity = GetActivity();
		}
		else
		{
			if (GetFlags() & FL_DUCKING)
			{
				if (speed > 0)
				{
					idealActivity = ACT_HL2MP_WALK_CROUCH;
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE_CROUCH;
				}
			}
			else
			{
				if (speed > 0)
				{
					idealActivity = ACT_HL2MP_RUN;
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE;
				}
			}
		}

		idealActivity = TranslateTeamActivity(idealActivity);
	}

	if (idealActivity == ACT_HL2MP_GESTURE_RANGE_ATTACK)
	{
		RestartGesture(Weapon_TranslateActivity(idealActivity));
		return;
	}
	else if (idealActivity == ACT_HL2MP_GESTURE_RELOAD)
	{
		RestartGesture(Weapon_TranslateActivity(idealActivity));
		return;
	}
	else
	{
		SetActivity(idealActivity);

		animDesired = SelectWeightedSequence(Weapon_TranslateActivity(idealActivity));

		if (animDesired == -1)
		{
			animDesired = SelectWeightedSequence(idealActivity);

			if (animDesired == -1)
			{
				animDesired = 0;
			}
		}

		if (GetSequence() == animDesired)
			return;

		m_flPlaybackRate = 1.0;
		ResetSequence(animDesired);
		SetCycle(0);
		return;
	}

	if (GetSequence() == animDesired)
		return;

	ResetSequence(animDesired);
	SetCycle(0);
}

extern int	gEvilImpulse101;

bool CHL2MP_Player::BumpWeapon(CBaseCombatWeapon* pWeapon)
{
	// Disallow picking up weapons while downed
	if (sv_downed_enable.GetBool() && m_bIsDowned)
		return false;

	CBaseCombatCharacter* pOwner = pWeapon->GetOwner();

	if (!IsAllowedToPickupWeapons())
		return false;

	if (pOwner || !Weapon_CanUse(pWeapon) || !g_pGameRules->CanHavePlayerItem(this, pWeapon))
	{
		if (gEvilImpulse101)
		{
			UTIL_Remove(pWeapon);
		}
		return false;
	}

	if (!pWeapon->FVisible(this, MASK_SOLID) && !(GetFlags() & FL_NOTARGET))
	{
		return false;
	}

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType());

	if (bOwnsWeaponAlready == true)
	{
		if (Weapon_EquipAmmoOnly(pWeapon))
		{
			pWeapon->CheckRespawn();

			UTIL_Remove(pWeapon);
			return true;
		}
		else
		{
			return false;
		}
	}

	pWeapon->CheckRespawn();
	Weapon_Equip(pWeapon);

	return true;
}

void CHL2MP_Player::ChangeTeam(int iTeam)
{
	bool bKill = false;

	if (HL2MPRules()->IsTeamplay() != true && iTeam != TEAM_SPECTATOR)
	{
		iTeam = TEAM_UNASSIGNED;
	}

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (iTeam != GetTeamNumber() && GetTeamNumber() != TEAM_UNASSIGNED)
		{
			bKill = true;
		}
	}

	BaseClass::ChangeTeam(iTeam);

	m_flNextTeamChangeTime = gpGlobals->curtime + TEAM_CHANGE_INTERVAL;

	if (HL2MPRules()->IsTeamplay() == true)
	{
		SetPlayerTeamModel();
	}
	else
	{
		SetPlayerModel();
	}

	if (iTeam == TEAM_SPECTATOR)
	{
		RemoveAllItems(true);

		State_Transition(STATE_OBSERVER_MODE);
	}

	if (bKill == true)
	{
		CommitSuicide();
	}
}

bool CHL2MP_Player::HandleCommand_JoinTeam(int team)
{
	if (!GetGlobalTeam(team) || team == 0)
	{
		Warning("HandleCommand_JoinTeam( %d ) - invalid team index.\n", team);
		return false;
	}

	if (team == TEAM_SPECTATOR)
	{
		if (!mp_allowspectators.GetInt() && !IsHLTV())
		{
			ClientPrint(this, HUD_PRINTCENTER, "#Cannot_Be_Spectator");
			return false;
		}

		if (GetTeamNumber() != TEAM_UNASSIGNED && !IsDead())
		{
			m_fNextSuicideTime = gpGlobals->curtime;

			CommitSuicide();

			IncrementFragCount(1);
		}

		ChangeTeam(TEAM_SPECTATOR);

		return true;
	}
	else
	{
		StopObserverMode();
		State_Transition(STATE_ACTIVE);
	}

	ChangeTeam(team);

	return true;
}

bool CHL2MP_Player::ClientCommand(const CCommand& args)
{
	if (FStrEq(args[0], "spectate"))
	{
		if (ShouldRunRateLimitedCommand(args))
		{
			HandleCommand_JoinTeam(TEAM_SPECTATOR);
		}
		return true;
	}
	else if (FStrEq(args[0], "jointeam"))
	{
		if (args.ArgC() < 2)
		{
			Warning("Player sent bad jointeam syntax\n");
		}

		if (ShouldRunRateLimitedCommand(args))
		{
			int iTeam = atoi(args[1]);
			HandleCommand_JoinTeam(iTeam);
		}
		return true;
	}
	else if (FStrEq(args[0], "joingame"))
	{
		return true;
	}

	return BaseClass::ClientCommand(args);
}

void CHL2MP_Player::CheatImpulseCommands(int iImpulse)
{
	switch (iImpulse)
	{
	case 101:
	{
		if (sv_cheats->GetBool())
		{
			GiveAllItems();
		}
	}
	break;

	default:
		BaseClass::CheatImpulseCommands(iImpulse);
	}
}

bool CHL2MP_Player::ShouldRunRateLimitedCommand(const CCommand& args)
{
	int i = m_RateLimitLastCommandTimes.Find(args[0]);
	if (i == m_RateLimitLastCommandTimes.InvalidIndex())
	{
		m_RateLimitLastCommandTimes.Insert(args[0], gpGlobals->curtime);
		return true;
	}
	else if ((gpGlobals->curtime - m_RateLimitLastCommandTimes[i]) < HL2MP_COMMAND_MAX_RATE)
	{
		return false;
	}
	else
	{
		m_RateLimitLastCommandTimes[i] = gpGlobals->curtime;
		return true;
	}
}

void CHL2MP_Player::CreateViewModel(int index /*=0*/)
{
	Assert(index >= 0 && index < MAX_VIEWMODELS);

	if (GetViewModel(index))
		return;

	CPredictedViewModel* vm = (CPredictedViewModel*)CreateEntityByName("predicted_viewmodel");
	if (vm)
	{
		vm->SetAbsOrigin(GetAbsOrigin());
		vm->SetOwner(this);
		vm->SetIndex(index);
		DispatchSpawn(vm);
		vm->FollowEntity(this, false);
		m_hViewModel.Set(index, vm);
	}
}

bool CHL2MP_Player::BecomeRagdollOnClient(const Vector& force)
{
	return true;
}

class CHL2MPRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS(CHL2MPRagdoll, CBaseAnimatingOverlay);
	DECLARE_SERVERCLASS();

	virtual int UpdateTransmitState()
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}

public:
	CNetworkHandle(CBaseEntity, m_hPlayer);
	CNetworkVector(m_vecRagdollVelocity);
	CNetworkVector(m_vecRagdollOrigin);
};

LINK_ENTITY_TO_CLASS(hl2mp_ragdoll, CHL2MPRagdoll);

IMPLEMENT_SERVERCLASS_ST_NOBASE(CHL2MPRagdoll, DT_HL2MPRagdoll)
SendPropVector(SENDINFO(m_vecRagdollOrigin), -1, SPROP_COORD),
SendPropEHandle(SENDINFO(m_hPlayer)),
SendPropModelIndex(SENDINFO(m_nModelIndex)),
SendPropInt(SENDINFO(m_nForceBone), 8, 0),
SendPropVector(SENDINFO(m_vecForce), -1, SPROP_NOSCALE),
SendPropVector(SENDINFO(m_vecRagdollVelocity))
END_SEND_TABLE()

void CHL2MP_Player::CreateRagdollEntity(void)
{
	if (m_hRagdoll)
	{
		UTIL_RemoveImmediate(m_hRagdoll);
		m_hRagdoll = NULL;
	}

	CHL2MPRagdoll* pRagdoll = dynamic_cast<CHL2MPRagdoll*>(m_hRagdoll.Get());

	if (!pRagdoll)
	{
		pRagdoll = dynamic_cast<CHL2MPRagdoll*>(CreateEntityByName("hl2mp_ragdoll"));
	}

	if (pRagdoll)
	{
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
		pRagdoll->SetAbsOrigin(GetAbsOrigin());
	}

	m_hRagdoll = pRagdoll;
}

int CHL2MP_Player::FlashlightIsOn(void)
{
	return IsEffectActive(EF_DIMLIGHT);
}

extern ConVar flashlight;

void CHL2MP_Player::FlashlightTurnOn(void)
{
	if (flashlight.GetInt() > 0 && IsAlive())
	{
		AddEffects(EF_DIMLIGHT);
		EmitSound("HL2Player.FlashlightOn");
	}
}

void CHL2MP_Player::FlashlightTurnOff(void)
{
	RemoveEffects(EF_DIMLIGHT);

	if (IsAlive())
	{
		EmitSound("HL2Player.FlashlightOff");
	}
}

void CHL2MP_Player::Weapon_Drop(CBaseCombatWeapon* pWeapon, const Vector* pvecTarget, const Vector* pVelocity)
{
	if (GetActiveWeapon())
	{
		CBaseCombatWeapon* pGrenade = Weapon_OwnsThisType("weapon_frag");

		if (GetActiveWeapon() == pGrenade)
		{
			if ((m_nButtons & IN_ATTACK) || (m_nButtons & IN_ATTACK2))
			{
				DropPrimedFragGrenade(this, pGrenade);
				return;
			}
		}
	}

	BaseClass::Weapon_Drop(pWeapon, pvecTarget, pVelocity);
}

int CHL2MP_Player::GetMaxAmmo(int iAmmoIndex) const
{
	if (iAmmoIndex == -1)
		return 0;

	if (GetAmmoDef()->MaxCarry(iAmmoIndex) == INFINITE_AMMO)
		return 999;

	return GetAmmoDef()->MaxCarry(iAmmoIndex);
}

void CHL2MP_Player::DetonateTripmines(void)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = gEntList.FindEntityByClassname(pEntity, "npc_satchel")) != NULL)
	{
		CSatchelCharge* pSatchel = dynamic_cast<CSatchelCharge*>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() == this)
		{
			g_EventQueue.AddEvent(pSatchel, "Explode", 0.20, this, this);
		}
	}

	EmitSound("Weapon_SLAM.SatchelDetonate");
}

void CHL2MP_Player::Event_Killed(const CTakeDamageInfo& info)
{
	m_bIsDowned = false;
	m_flBleedoutTimer = 0.0f;
	m_flReviveProgress = 0.0f;
	m_hRevivingTeammate = NULL;
	m_iDownedCount = 0;

	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce(m_vecTotalBulletForce);

	SetNumAnimOverlays(0);

	CreateRagdollEntity();

	DetonateTripmines();

	BaseClass::Event_Killed(subinfo);

	if (info.GetDamageType() & DMG_DISSOLVE)
	{
		if (m_hRagdoll)
		{
			m_hRagdoll->GetBaseAnimating()->Dissolve(NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL);
		}
	}

	CBaseEntity* pAttacker = info.GetAttacker();

	if (pAttacker)
	{
		int iScoreToAdd = 1;

		if (pAttacker == this)
		{
			iScoreToAdd = -1;
		}

		GetGlobalTeam(pAttacker->GetTeamNumber())->AddScore(iScoreToAdd);
	}

	FlashlightTurnOff();

	m_lifeState = LIFE_DEAD;

	RemoveEffects(EF_NODRAW);
	StopZooming();
}

int CHL2MP_Player::OnTakeDamage(const CTakeDamageInfo& inputInfo)
{
	if (gpGlobals->curtime < m_flSlamProtectTime && (inputInfo.GetDamageType() == DMG_BLAST))
		return 0;

	m_vecTotalBulletForce += inputInfo.GetDamageForce();

	gamestats->Event_PlayerDamage(this, inputInfo);

	if (sv_downed_enable.GetBool() && !m_bIsDowned && m_lifeState == LIFE_ALIVE)
	{
		if ((GetHealth() - inputInfo.GetDamage()) <= 0)
		{
			DownPlayer(inputInfo);
			return 0;
		}
	}

	return BaseClass::OnTakeDamage(inputInfo);
}

void CHL2MP_Player::DeathSound(const CTakeDamageInfo& info)
{
	if (m_hRagdoll && m_hRagdoll->GetBaseAnimating()->IsDissolving())
		return;

	char szStepSound[128];

	Q_snprintf(szStepSound, sizeof(szStepSound), "%s.Die", GetPlayerModelSoundPrefix());

	const char* pModelName = STRING(GetModelName());

	CSoundParameters params;
	if (GetParametersForSound(szStepSound, params, pModelName) == false)
		return;

	Vector vecOrigin = GetAbsOrigin();

	CRecipientFilter filter;
	filter.AddRecipientsByPAS(vecOrigin);

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound(filter, entindex(), ep);
}

CBaseEntity* CHL2MP_Player::EntSelectSpawnPoint(void)
{
	CBaseEntity* pSpot = NULL;
	CBaseEntity* pLastSpawnPoint = g_pLastSpawn;
	edict_t* player = edict();
	const char* pSpawnpointName = "info_player_deathmatch";

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			pSpawnpointName = "info_player_combine";
			pLastSpawnPoint = g_pLastCombineSpawn;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			pSpawnpointName = "info_player_rebel";
			pLastSpawnPoint = g_pLastRebelSpawn;
		}

		if (gEntList.FindEntityByClassname(NULL, pSpawnpointName) == NULL)
		{
			pSpawnpointName = "info_player_deathmatch";
			pLastSpawnPoint = g_pLastSpawn;
		}
	}

	pSpot = pLastSpawnPoint;

	for (int i = random->RandomInt(1, 5); i > 0; i--)
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	if (!pSpot)
		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);

	CBaseEntity* pFirstSpot = pSpot;

	do
	{
		if (pSpot)
		{
			if (g_pGameRules->IsSpawnPointValid(pSpot, this))
			{
				if (pSpot->GetLocalOrigin() == vec3_origin)
				{
					pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
					continue;
				}

				goto ReturnSpot;
			}
		}

		pSpot = gEntList.FindEntityByClassname(pSpot, pSpawnpointName);
	} while (pSpot != pFirstSpot);

	if (pSpot)
	{
		CBaseEntity* ent = NULL;
		for (CEntitySphereQuery sphere(pSpot->GetAbsOrigin(), hl2mp_spawn_frag_fallback_radius.GetFloat()); (ent = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
		{
			if (ent->IsPlayer() && !(ent->edict() == player))
				ent->TakeDamage(CTakeDamageInfo(GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), 300, DMG_GENERIC));
		}
		goto ReturnSpot;
	}

	if (!pSpot)
	{
		pSpot = gEntList.FindEntityByClassname(pSpot, "info_player_start");

		if (pSpot)
			goto ReturnSpot;
	}

ReturnSpot:

	if (HL2MPRules()->IsTeamplay() == true)
	{
		if (GetTeamNumber() == TEAM_COMBINE)
		{
			g_pLastCombineSpawn = pSpot;
		}
		else if (GetTeamNumber() == TEAM_REBELS)
		{
			g_pLastRebelSpawn = pSpot;
		}
	}

	g_pLastSpawn = pSpot;

	m_flSlamProtectTime = gpGlobals->curtime + 0.5;

	return pSpot;
}

CON_COMMAND(timeleft, "prints the time remaining in the match")
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());

	int iTimeRemaining = (int)HL2MPRules()->GetMapRemainingTime();

	if (iTimeRemaining == 0)
	{
		if (pPlayer)
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "This game has no timelimit.");
		}
		else
		{
			Msg("* No Time Limit *\n");
		}
	}
	else
	{
		int iMinutes, iSeconds;
		iMinutes = iTimeRemaining / 60;
		iSeconds = iTimeRemaining % 60;

		char minutes[8];
		char seconds[8];

		Q_snprintf(minutes, sizeof(minutes), "%d", iMinutes);
		Q_snprintf(seconds, sizeof(seconds), "%2.2d", iSeconds);

		if (pPlayer)
		{
			ClientPrint(pPlayer, HUD_PRINTTALK, "Time left in map: %s1:%s2", minutes, seconds);
		}
		else
		{
			Msg("Time Remaining:  %s:%s\n", minutes, seconds);
		}
	}
}

CON_COMMAND(revive_self, "Instantly revives the local player from the downed state (requires sv_cheats 1).")
{
	if (!sv_cheats->GetBool())
	{
		Msg("revive_self requires sv_cheats set to 1.\n");
		return;
	}

	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());
	if (pPlayer && pPlayer->IsDowned())
	{
		pPlayer->RevivePlayer(pPlayer);
		ClientPrint(pPlayer, HUD_PRINTTALK, "You have been revived by debug command.");
	}
	else
	{
		Msg("You must be in a downed state to use revive_self.\n");
	}
}

void CHL2MP_Player::Reset()
{
	ResetDeathCount();
	ResetFragCount();
}

bool CHL2MP_Player::IsReady()
{
	return m_bReady;
}

void CHL2MP_Player::SetReady(bool bReady)
{
	m_bReady = bReady;
}

void CHL2MP_Player::CheckChatText(char* p, int bufsize)
{
	char* buf = new char[bufsize];
	int pos = 0;

	for (char* pSrc = p; pSrc != NULL && *pSrc != 0 && pos < bufsize - 1; pSrc++)
	{
		buf[pos] = *pSrc;
		pos++;
	}

	buf[pos] = '\0';

	Q_strncpy(p, buf, bufsize);

	delete[] buf;

	const char* pReadyCheck = p;

	HL2MPRules()->CheckChatForReadySignal(this, pReadyCheck);
}

void CHL2MP_Player::State_Transition(HL2MPPlayerState newState)
{
	State_Leave();
	State_Enter(newState);
}

void CHL2MP_Player::State_Enter(HL2MPPlayerState newState)
{
	m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo(newState);

	if (m_pCurStateInfo && m_pCurStateInfo->pfnEnterState)
		(this->*m_pCurStateInfo->pfnEnterState)();
}

void CHL2MP_Player::State_Leave()
{
	if (m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState)
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}

void CHL2MP_Player::State_PreThink()
{
	if (m_pCurStateInfo && m_pCurStateInfo->pfnPreThink)
	{
		(this->*m_pCurStateInfo->pfnPreThink)();
	}
}

CHL2MPPlayerStateInfo* CHL2MP_Player::State_LookupInfo(HL2MPPlayerState state)
{
	static CHL2MPPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CHL2MP_Player::State_Enter_ACTIVE, NULL, &CHL2MP_Player::State_PreThink_ACTIVE },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CHL2MP_Player::State_Enter_OBSERVER_MODE,	NULL, &CHL2MP_Player::State_PreThink_OBSERVER_MODE }
	};

	for (int i = 0; i < ARRAYSIZE(playerStateInfos); i++)
	{
		if (playerStateInfos[i].m_iPlayerState == state)
			return &playerStateInfos[i];
	}

	return NULL;
}

bool CHL2MP_Player::StartObserverMode(int mode)
{
	if (m_bEnterObserver == true)
	{
		VPhysicsDestroyObject();
		return BaseClass::StartObserverMode(mode);
	}
	return false;
}

void CHL2MP_Player::StopObserverMode()
{
	m_bEnterObserver = false;
	BaseClass::StopObserverMode();
}

void CHL2MP_Player::State_Enter_OBSERVER_MODE()
{
	int observerMode = m_iObserverLastMode;
	if (IsNetClient())
	{
		const char* pIdealMode = engine->GetClientConVarValue(engine->IndexOfEdict(edict()), "cl_spec_mode");
		if (pIdealMode)
		{
			observerMode = atoi(pIdealMode);
			if (observerMode <= OBS_MODE_FIXED || observerMode > OBS_MODE_ROAMING)
			{
				observerMode = m_iObserverLastMode;
			}
		}
	}
	m_bEnterObserver = true;
	StartObserverMode(observerMode);
}

void CHL2MP_Player::State_PreThink_OBSERVER_MODE()
{
	Assert(m_takedamage == DAMAGE_NO);
	Assert(IsSolidFlagSet(FSOLID_NOT_SOLID));

	Assert(m_lifeState == LIFE_DEAD);
	Assert(pl.deadflag);
}

void CHL2MP_Player::State_Enter_ACTIVE()
{
	SetMoveType(MOVETYPE_WALK);

	m_Local.m_iHideHUD = 0;
}

void CHL2MP_Player::State_PreThink_ACTIVE()
{
}

bool CHL2MP_Player::CanHearAndReadChatFrom(CBasePlayer* pPlayer)
{
	if (!pPlayer)
		return false;

	return true;
}

bool CHL2MP_Player::IsThreatAimingTowardMe(CBaseEntity* threat, float cosTolerance) const
{
	CHL2MP_Player* player = ToHL2MPPlayer(threat);
	Vector to = GetAbsOrigin() - threat->GetAbsOrigin();
	Vector forward;

	if (player == NULL)
	{
		return false;
	}

	player->EyeVectors(&forward);

	if (DotProduct(to, forward) > cosTolerance)
	{
		return true;
	}

	return false;
}

bool CHL2MP_Player::IsThreatFiringAtMe(CBaseEntity* threat) const
{
	if (IsThreatAimingTowardMe(threat))
	{
		CHL2MP_Player* player = ToHL2MPPlayer(threat);

		if (player)
		{
			return player->IsFiringWeapon();
		}
	}

	return false;
}

//DROPPING
CON_COMMAND(drop, "Drops the currently held weapon.")
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CBaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
	if (!pWeapon)
		return;

	// Check drop exception rule
	CBaseHL2MPCombatWeapon* pHLEWpn = dynamic_cast<CBaseHL2MPCombatWeapon*>(pWeapon);
	if (pHLEWpn && !pHLEWpn->CanBeDropped())
	{
		ClientPrint(pPlayer, HUD_PRINTCONSOLE, "This weapon cannot be dropped.\n");
		return;
	}

	// Apply forward velocity and drop
	Vector vecForward;
	pPlayer->EyeVectors(&vecForward);
	Vector vecVelocity = vecForward * 100.0f + Vector(0, 0, 50.0f);

	pPlayer->Weapon_Drop(pWeapon, &vecVelocity);
}
CON_COMMAND(dropammo, "Drops a clip of ammo for the currently held weapon.")
{
	CHL2MP_Player* pPlayer = ToHL2MPPlayer(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CBaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
	if (!pWeapon)
	{
		ClientPrint(pPlayer, HUD_PRINTCONSOLE, "You must be holding a weapon to drop ammo.\n");
		return;
	}

	int iAmmoType = pWeapon->GetPrimaryAmmoType();
	if (iAmmoType <= 0)
	{
		ClientPrint(pPlayer, HUD_PRINTCONSOLE, "This weapon does not use primary ammo.\n");
		return;
	}

	int iCurrentAmmo = pPlayer->GetAmmoCount(iAmmoType);
	int iDropAmount = pWeapon->GetDefaultClip1(); // Drop one clip's worth, or fallback to 10
	if (iDropAmount <= 0)
		iDropAmount = 10;

	if (iCurrentAmmo < iDropAmount)
	{
		ClientPrint(pPlayer, HUD_PRINTCONSOLE, "Not enough ammo to drop.\n");
		return;
	}

	// Remove ammo from player's inventory
	pPlayer->RemoveAmmo(iDropAmount, iAmmoType);

	// Spawn a dropped ammo packet in front of the player
	Vector vecForward;
	pPlayer->EyeVectors(&vecForward);
	Vector vecSrc = pPlayer->Weapon_ShootPosition() + vecForward * 32.0f;
	Vector vecVelocity = vecForward * 100.0f + Vector(0, 0, 50.0f);

	// You can spawn a generic item or map it to your mod's ammo entity classnames
	CBaseEntity* pAmmoDrop = CreateEntityByName("item_ammo_crate"); // Or a custom ammo drop entity
	if (pAmmoDrop)
	{
		pAmmoDrop->SetAbsOrigin(vecSrc);
		pAmmoDrop->SetAbsVelocity(vecVelocity);
		DispatchSpawn(pAmmoDrop);
	}

	ClientPrint(pPlayer, HUD_PRINTCONSOLE, "Dropped ammo.\n");
}