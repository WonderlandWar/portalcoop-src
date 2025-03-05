//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Player for Portal.
//
//=============================================================================//

#include "cbase.h"
#include "portal_player.h"
#include "globalstate.h"
#include "trains.h"
#include "game.h"
#include "portal_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "portal_gamerules.h"
#include "weapon_portalgun.h"
#include "portal\weapon_physcannon.h"
#include "KeyValues.h"
#include "team.h"
#include "eventqueue.h"
#include "weapon_portalbase.h"
#include "engine/IEngineSound.h"
#include "ai_basenpc.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "prop_portal_shared.h"
#include "player_pickup.h"	// for player pickup code
#include "vphysics/player_controller.h"
#include "datacache/imdlcache.h"
#include "bone_setup.h"
#include "portal_gamestats.h"
#include "physics_prop_ragdoll.h"
#include "soundenvelope.h"
#include "ai_speech.h"		// For expressors, vcd playing
#include "sceneentity.h"	// has the VCD precache function
#include "portal_gamerules.h"
#include "doors.h"
#include "trains.h"
#include "point_ping_linker.h"
#include "hierarchy.h"
#include "dt_utlvector_send.h"
#include "physicsshadowclone.h"

extern CBaseEntity* g_pLastSpawn;

extern void respawn(CBaseEntity* pEdict, bool fCopyCorpse);

BEGIN_DATADESC( CInfoPlayerPortalCoop )

DEFINE_OUTPUT( m_OnPlayerSpawned, "OnPlayerSpawned" ),

DEFINE_INPUTFUNC( FIELD_VOID, "EnablePortalgunSpawn", InputEnablePortalgunSpawn ),
DEFINE_INPUTFUNC( FIELD_VOID, "DisablePortalgunSpawn", InputDisablePortalgunSpawn ),
DEFINE_INPUTFUNC( FIELD_INTEGER, "SetPortalgunType", InputSetPortalgunType ),
DEFINE_INPUTFUNC( FIELD_INTEGER, "SetPlayer", InputSetPlayer ),

DEFINE_KEYFIELD( m_bSpawnWithPortalgun, FIELD_BOOLEAN, "SpawnWithPortalgun" ),
DEFINE_KEYFIELD( m_iPortalgunType, FIELD_INTEGER, "PortalgunType" ),
DEFINE_KEYFIELD( m_iValidPlayerIndex, FIELD_INTEGER, "ValidPlayerIndex" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_player_portalcoop, CInfoPlayerPortalCoop )

void CInfoPlayerPortalCoop::PlayerSpawned( CBasePlayer *pPlayer )
{
	Assert( !m_iValidPlayerIndex || m_iValidPlayerIndex == pPlayer->entindex() );

	PortalGunSpawnInfo_t info;
	info.m_bSpawnWithPortalgun = m_bSpawnWithPortalgun;

	if ( m_bSpawnWithPortalgun )
	{
		if (m_iPortalgunType == 0)
		{
			info.m_bCanFirePortal1 = false;
			info.m_bCanFirePortal2 = false;
		}
		else if (m_iPortalgunType == 1)
		{
			info.m_bCanFirePortal1 = true;
			info.m_bCanFirePortal2 = false;
		}
		else if (m_iPortalgunType == 2)
		{
			info.m_bCanFirePortal1 = false;
			info.m_bCanFirePortal2 = true;
		}
		else if (m_iPortalgunType == 3)
		{
			info.m_bCanFirePortal1 = true;
			info.m_bCanFirePortal2 = true;
		}
	}

	CPortal_Player *pPortalPlayer = ToPortalPlayer(pPlayer);
	pPortalPlayer->m_PortalGunSpawnInfo = info;

	m_OnPlayerSpawned.FireOutput(pPlayer, pPlayer);
}

bool CInfoPlayerPortalCoop::CanSpawnOnMe( CBasePlayer *pPlayer )
{
	if ( !m_iValidPlayerIndex || m_iValidPlayerIndex == pPlayer->entindex() )
	{
		return true;
	}

	return false;
}

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS(CTEPlayerAnimEvent, CBaseTempEntity);
	DECLARE_SERVERCLASS();

	CTEPlayerAnimEvent(const char* name) : CBaseTempEntity(name)
	{
	}

	CNetworkHandle(CBasePlayer, m_hPlayer);
	CNetworkVar(int, m_iEvent);
	CNetworkVar(int, m_nData);
};

IMPLEMENT_SERVERCLASS_ST_NOBASE(CTEPlayerAnimEvent, DT_TEPlayerAnimEvent)
SendPropEHandle(SENDINFO(m_hPlayer)),
SendPropInt(SENDINFO(m_iEvent), Q_log2(PLAYERANIMEVENT_COUNT) + 1, SPROP_UNSIGNED),
SendPropInt(SENDINFO(m_nData), 32),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent("PlayerAnimEvent");

void TE_PlayerAnimEvent(CBasePlayer* pPlayer, PlayerAnimEvent_t event, int nData)
{
	CPVSFilter filter((const Vector&)pPlayer->EyePosition());

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create(filter, 0);
}

ConVar  *sv_cheats = NULL;

CON_COMMAND(invisible, "Makes the command user invisible")
{
	if (sv_cheats->GetBool() == false)
		return;
	
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

	if (pPlayer->m_bInvisible)
		pPlayer->m_bInvisible = false;
	else
		pPlayer->m_bInvisible = true;

	if (pPlayer->m_bInvisible)
	{
		pPlayer->SetRenderMode(kRenderTransAdd);
		pPlayer->SetRenderColorA(0);
		pPlayer->AddEffects(EF_NOSHADOW);
		if (pWeapon)
		{
			pWeapon->SetRenderMode(kRenderTransAdd);
			pWeapon->SetRenderColorA(0);
			pWeapon->AddEffects(EF_NOSHADOW);
		}
	}
	else
	{
		pPlayer->SetRenderMode(kRenderNormal);
		pPlayer->SetRenderColorA(255);
		pPlayer->RemoveEffects(EF_NOSHADOW);
		if (pWeapon)
		{
			pWeapon->SetRenderMode(kRenderNormal);
			pWeapon->SetRenderColorA(255);
		}
	}
}

//=================================================================================
//
// Ragdoll Entity
//
class CPortalRagdoll : public CBaseAnimatingOverlay, public CDefaultPlayerPickupVPhysics
{
public:

	DECLARE_CLASS(CPortalRagdoll, CBaseAnimatingOverlay);
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPortalRagdoll()
	{
		m_hPlayer.Set(NULL);
		m_vecRagdollOrigin.Init();
		m_vecRagdollVelocity.Init();
	}

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}

	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle(CBaseEntity, m_hPlayer);	// networked entity handle 
	CNetworkVector(m_vecRagdollVelocity);
	CNetworkVector(m_vecRagdollOrigin);
};

LINK_ENTITY_TO_CLASS(portal_ragdoll, CPortalRagdoll);

IMPLEMENT_SERVERCLASS_ST_NOBASE(CPortalRagdoll, DT_PortalRagdoll)
SendPropVector(SENDINFO(m_vecRagdollOrigin), -1, SPROP_COORD),
SendPropEHandle(SENDINFO(m_hPlayer)),
SendPropModelIndex(SENDINFO(m_nModelIndex)),
SendPropInt(SENDINFO(m_nForceBone), 8, 0),
SendPropVector(SENDINFO(m_vecForce), -1, SPROP_NOSCALE),
SendPropVector(SENDINFO(m_vecRagdollVelocity)),
END_SEND_TABLE()


BEGIN_DATADESC(CPortalRagdoll)

DEFINE_FIELD(m_vecRagdollOrigin, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_hPlayer, FIELD_EHANDLE),
DEFINE_FIELD(m_vecRagdollVelocity, FIELD_VECTOR),

END_DATADESC()

CEntityPortalledNetworkMessage::CEntityPortalledNetworkMessage( void )
{
	m_hEntity = NULL;
	m_hPortal = NULL;
	m_fTime = 0.0f;
	m_bForcedDuck = false;
	m_iMessageCount = 0;;
}

BEGIN_SEND_TABLE_NOBASE( CEntityPortalledNetworkMessage, DT_EntityPortalledNetworkMessage )
		SendPropEHandle( SENDINFO_NOCHECK(m_hEntity) ),
		SendPropEHandle( SENDINFO_NOCHECK(m_hPortal) ),
		SendPropFloat( SENDINFO_NOCHECK(m_fTime), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),
		SendPropBool( SENDINFO_NOCHECK(m_bForcedDuck) ),
		SendPropInt( SENDINFO_NOCHECK(m_iMessageCount) ),
END_SEND_TABLE()

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CPortal_Player, DT_PortalLocalPlayerExclusive )
	//a message buffer for entity teleportations that's guaranteed to be in sync with the post-teleport updates for said entities
	SendPropUtlVector( SENDINFO_UTLVECTOR( m_EntityPortalledNetworkMessages ), CPortal_Player::MAX_ENTITY_PORTALLED_NETWORK_MESSAGES, SendPropDataTable( NULL, 0, &REFERENCE_SEND_TABLE( DT_EntityPortalledNetworkMessage ) ) ),
	SendPropInt( SENDINFO( m_iEntityPortalledNetworkMessageCount ) ),
	//SendPropBool( SENDINFO( m_bPaused ) ),
END_SEND_TABLE()

enum
{
	MODEL_CHELL,
	MODEL_MEL,
	MODEL_ABBY,
	MODEL_MALE_PORTAL_PLAYER
};

const char *g_ppszPortalMPModels[] =
{
	"models/player/Chell.mdl",
	"models/player/Mel.mdl",
	"models/player/Abby.mdl",
	"models/player/male_portal_player.mdl"
};

void BotSetupModelConVarValue( CPortal_Player *pBot )
{	
	int nHeads = ARRAYSIZE(g_ppszPortalMPModels);
	const char *pszModel = g_ppszPortalMPModels[ RandomInt(0, nHeads ) ];
	engine->SetFakeClientConVarValue( pBot->edict(), "cl_playermodel", pszModel );
}

LINK_ENTITY_TO_CLASS(player, CPortal_Player);

IMPLEMENT_SERVERCLASS_ST(CPortal_Player, DT_Portal_Player)
	/*
	SendPropExclude("DT_BaseAnimating", "m_flPlaybackRate"),
	SendPropExclude("DT_BaseAnimating", "m_nSequence"),
	SendPropExclude("DT_BaseAnimating", "m_nNewSequenceParity"),
	SendPropExclude("DT_BaseAnimating", "m_nResetEventsParity"),
	*/

	SendPropExclude("DT_BaseEntity", "m_angRotation"),
	SendPropExclude("DT_BaseAnimatingOverlay", "overlay_vars"),
	SendPropExclude("DT_BaseFlex", "m_viewtarget"),
	SendPropExclude("DT_BaseFlex", "m_flexWeight"),
	SendPropExclude("DT_BaseFlex", "m_blinktoggle"),
	SendPropExclude("DT_BaseAnimating", "m_flPoseParameter"),

	// portal_playeranimstate and clientside animation takes care of these on the client
	//SendPropExclude("DT_ServerAnimationData", "m_flCycle"),
	//SendPropExclude("DT_AnimTimeMustBeFirst", "m_flAnimTime"),


	SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11, SPROP_CHANGES_OFTEN),
	SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11, SPROP_CHANGES_OFTEN),
	SendPropEHandle(SENDINFO(m_hRagdoll)),
	SendPropInt(SENDINFO(m_iSpawnInterpCounter), 4),
	SendPropBool(SENDINFO(m_bPitchReorientation)),
	SendPropEHandle(SENDINFO(m_hPortalEnvironment)),
	SendPropEHandle(SENDINFO(m_hSurroundingLiquidPortal)),
	SendPropEHandle(SENDINFO(m_hHeldObjectPortal)),
	SendPropBool(SENDINFO(m_bHasSprintDevice)),
	SendPropBool(SENDINFO(m_bSprintEnabled)),
	SendPropBool(SENDINFO(m_bSilentDropAndPickup)),
	SendPropBool(SENDINFO(m_bSuppressingCrosshair)),
	SendPropBool(SENDINFO(m_bIsListenServerHost)),
	SendPropBool(SENDINFO(m_bHeldObjectOnOppositeSideOfPortal)),
	SendPropInt(SENDINFO(m_iCustomPortalColorSet)),
	
	SendPropVector( SENDINFO( m_vecAnimStateBaseVelocity ), 0, SPROP_COORD_MP | SPROP_CHANGES_OFTEN ),

	// Data that only gets sent to the local player
	SendPropDataTable( "portallocaldata", 0, &REFERENCE_SEND_TABLE(DT_PortalLocalPlayerExclusive), SendProxy_SendLocalDataTable ),

END_SEND_TABLE()

BEGIN_DATADESC(CPortal_Player)

DEFINE_SOUNDPATCH(m_pWooshSound),

	DEFINE_FIELD(m_bHeldObjectOnOppositeSideOfPortal, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bIntersectingPortalPlane, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bStuckOnPortalCollisionObject, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fTimeLastHurt, FIELD_TIME),
	DEFINE_FIELD(m_fFlags, FIELD_FLOAT),

#if 0
	DEFINE_FIELD(m_StatsThisLevel.iNumPortalsPlaced, FIELD_INTEGER),
	DEFINE_FIELD(m_StatsThisLevel.iNumStepsTaken, FIELD_INTEGER),
	DEFINE_FIELD(m_StatsThisLevel.fNumSecondsTaken, FIELD_FLOAT),
#endif
		
	
	DEFINE_FIELD(m_bLookingForUseEntity, FIELD_BOOLEAN),
	DEFINE_FIELD(m_flLookForUseEntityTime, FIELD_FLOAT),
	DEFINE_FIELD(m_bPitchReorientation, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bIsRegenerating, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fNeuroToxinDamageTime, FIELD_TIME),
	DEFINE_FIELD(m_hPortalEnvironment, FIELD_EHANDLE),
	DEFINE_FIELD(m_flExpressionLoopTime, FIELD_TIME),
	DEFINE_FIELD(m_iszExpressionScene, FIELD_STRING),
	DEFINE_FIELD(m_hExpressionSceneEnt, FIELD_EHANDLE),
	DEFINE_FIELD(m_vecTotalBulletForce, FIELD_VECTOR),
	DEFINE_FIELD(m_bSilentDropAndPickup, FIELD_BOOLEAN),
	DEFINE_FIELD(m_hRagdoll, FIELD_EHANDLE),
	DEFINE_FIELD(m_angEyeAngles, FIELD_VECTOR),
	DEFINE_FIELD(m_hSurroundingLiquidPortal, FIELD_EHANDLE),
	DEFINE_FIELD(m_flLastPingTime, FIELD_FLOAT),
	DEFINE_FIELD(m_iCustomPortalColorSet, FIELD_INTEGER),

//DEFINE_THINKFUNC( HudHintThink ),

DEFINE_INPUTFUNC( FIELD_VOID, "DoPingHudHint", InputDoPingHudHint ),

DEFINE_EMBEDDEDBYREF(m_pExpresser),

END_DATADESC()

ConVar sv_regeneration_wait_time("sv_regeneration_wait_time", "1.0", FCVAR_REPLICATED);

#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 5.0f
#define TEAM_CHANGE_INTERVAL 5.0f

#define PORTALPLAYER_PHYSDAMAGE_SCALE 4.0f

extern ConVar sv_turbophysics;

//----------------------------------------------------
// Player Physics Shadow
//----------------------------------------------------
#define VPHYS_MAX_DISTANCE		2.0
#define VPHYS_MAX_VEL			10
#define VPHYS_MAX_DISTSQR		(VPHYS_MAX_DISTANCE*VPHYS_MAX_DISTANCE)
#define VPHYS_MAX_VELSQR		(VPHYS_MAX_VEL*VPHYS_MAX_VEL)

ConVar sv_portal_coop_ping_cooldown_time( "sv_portal_coop_ping_cooldown_time", "0.5", FCVAR_REPLICATED, "Time (in seconds) between coop pings", true, 0.1f, false, 60.0f );
#if 0
ConVar sv_portal_coop_ping_indicator_show_to_all_players( "sv_portal_coop_ping_indicator_show_to_all_players", "1", FCVAR_REPLICATED, "Sets if pinging should show for all players");
#endif
ConVar sv_portal_coop_allow_ping("sv_portal_coop_allow_ping", "1", FCVAR_REPLICATED, "Sets if players are allowed to ping");

#define COOP_PING_SOUNDSCRIPT_NAME "Player.Coop_Ping"
#define COOP_PING_HUD_SOUNDSCRIPT_NAME "npc/roller/mine/rmine_chirp_answer1.wav"
//#define COOP_PING_PARTICLE_NAME "command_target_ping"
#define COOP_PING_PARTICLE_NAME_ORANGE "command_target_ping_orange"
#define COOP_PING_PARTICLE_NAME_RED "command_target_ping_red"
#define COOP_PING_PARTICLE_NAME_PURPLE "command_target_ping_purple"
#define COOP_PING_PARTICLE_NAME_GREEN "command_target_ping_green"

extern float IntervalDistance(float x, float x0, float x1);

//disable 'this' : used in base member initializer list
#pragma warning( disable : 4355 )

CPortal_Player::CPortal_Player()
{
//	m_PlayerAnimState = CreatePortalPlayerAnimState(this);
	CreateExpresser();

	UseClientSideAnimation();

	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_iSpawnInterpCounter = 0;

	m_bHeldObjectOnOppositeSideOfPortal = false;
	m_hHeldObjectPortal = NULL;

	m_bIntersectingPortalPlane = false;

	m_bPitchReorientation = false;

	m_bSilentDropAndPickup = false;

	m_flLastPingTime = 0.0f;
	
	m_flImplicitVerticalStepSpeed = 0.0f;
	
	m_EntityPortalledNetworkMessages.SetCount( MAX_ENTITY_PORTALLED_NETWORK_MESSAGES );

	m_bPortalFunnel = false;

	m_iszExpressionScene = NULL_STRING;
	m_hExpressionSceneEnt = NULL;
	m_flExpressionLoopTime = 0.0f;

}

CPortal_Player::~CPortal_Player(void)
{
	ClearSceneEvents(NULL, true);

	if (m_PlayerAnimState)
		m_PlayerAnimState->Release();

	CPortalRagdoll* pRagdoll = dynamic_cast<CPortalRagdoll*>(m_hRagdoll.Get());
	if (pRagdoll)
	{
		UTIL_Remove(pRagdoll);
	}
	
	// All of this should be handled by gamerules now.

	//Hopefully solves the bug where disconnecting doesn't dissolve the player's portals
	/*
	CWeaponPortalgun *pPortalgun = dynamic_cast<CWeaponPortalgun*>(Weapon_OwnsThisType("weapon_portalgun"));
	if (pPortalgun)
	{
		pPortalgun->FizzleOwnedPortals();
	}
	*/
}

void CPortal_Player::UpdateOnRemove(void)
{
	BaseClass::UpdateOnRemove();
}

void CPortal_Player::Precache(void)
{
	BaseClass::Precache();

	//Precache Citizen models
	int nHeads = ARRAYSIZE(g_ppszPortalMPModels);
	int i;

	for (i = 0; i < nHeads; ++i)
		PrecacheModel(g_ppszPortalMPModels[i]);
	
	//PrecacheParticleSystem( COOP_PING_PARTICLE_NAME );
	PrecacheParticleSystem( COOP_PING_PARTICLE_NAME_ORANGE );
	PrecacheParticleSystem( COOP_PING_PARTICLE_NAME_RED );
	PrecacheParticleSystem( COOP_PING_PARTICLE_NAME_PURPLE );
	PrecacheParticleSystem( COOP_PING_PARTICLE_NAME_GREEN );
	//PrecacheParticleSystem( "command_target_ping_just_arrows" );
	PrecacheScriptSound( COOP_PING_SOUNDSCRIPT_NAME );

	PrecacheScriptSound("PortalPlayer.EnterPortal");
	PrecacheScriptSound("PortalPlayer.ExitPortal");

	PrecacheScriptSound("PortalPlayer.Woosh");
	PrecacheScriptSound("PortalPlayer.FallRecover");

	PrecacheModel("sprites/glow01.vmt");
	
	PrecacheScriptSound("NPC_Citizen.die");
}

void CPortal_Player::CreateSounds()
{
	if (!m_pWooshSound)
	{
		CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter(this);
		filter.UsePredictionRules();

		m_pWooshSound = controller.SoundCreate(filter, entindex(), "PortalPlayer.Woosh");
		controller.Play(m_pWooshSound, 0, 100);
	}
}

void CPortal_Player::StopLoopingSounds()
{
	if (m_pWooshSound)
	{
		CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();

		controller.SoundDestroy(m_pWooshSound);
		m_pWooshSound = NULL;
	}

	BaseClass::StopLoopingSounds();
}

void CPortal_Player::GiveAllItems(void)
{
	EquipSuit();

	CBasePlayer::GiveAmmo(255, "Pistol");
	CBasePlayer::GiveAmmo(32, "357");

	CBasePlayer::GiveAmmo(255, "AR2");
	CBasePlayer::GiveAmmo(3, "AR2AltFire");
	CBasePlayer::GiveAmmo(255, "SMG1");
	CBasePlayer::GiveAmmo(3, "smg1_grenade");

	CBasePlayer::GiveAmmo(255, "Buckshot");
	CBasePlayer::GiveAmmo(16, "XBowBolt");

	CBasePlayer::GiveAmmo(3, "rpg_round");
	CBasePlayer::GiveAmmo(6, "grenade");

	GiveNamedItem("weapon_crowbar");
	GiveNamedItem("weapon_physcannon");

	GiveNamedItem("weapon_pistol");
	GiveNamedItem("weapon_357");

	GiveNamedItem("weapon_smg1");
	GiveNamedItem("weapon_ar2");

	GiveNamedItem("weapon_shotgun");
	GiveNamedItem("weapon_crossbow");

	GiveNamedItem("weapon_rpg");
	GiveNamedItem("weapon_frag");

	GiveNamedItem("weapon_bugbait");

	//GiveNamedItem( "weapon_physcannon" );
	CWeaponPortalgun* pPortalGun = static_cast<CWeaponPortalgun*>(GiveNamedItem("weapon_portalgun"));

	if (!pPortalGun)
	{
		pPortalGun = static_cast<CWeaponPortalgun*>(Weapon_OwnsThisType("weapon_portalgun"));
	}

	if (pPortalGun)
	{
		pPortalGun->SetCanFirePortal1();
		pPortalGun->SetCanFirePortal2();
	}
}

void CPortal_Player::GiveDefaultItems(void)
{
	castable_string_t st("suit_no_sprint");
	GlobalEntity_SetState(st, GLOBAL_OFF);
}

const char *s_pHudHintContext = "HudHintContext";

//-----------------------------------------------------------------------------
// Purpose: Sets  specific defaults.
//-----------------------------------------------------------------------------
void CPortal_Player::Spawn(void)
{
	SetPlayerModel();

	BaseClass::Spawn();
	
	ResetAnimation();

	CreateSounds();

	pl.deadflag = false;
	RemoveSolidFlags(FSOLID_NOT_SOLID);

	RemoveEffects(EF_NODRAW);
	StopObserverMode();

	GiveDefaultItems();

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;

	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	m_impactEnergyScale = PORTALPLAYER_PHYSDAMAGE_SCALE;

	RemoveFlag(FL_FROZEN);

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);

	m_bIsListenServerHost = ( this == UTIL_GetListenServerHost() );

//	HudHintThink();
	
#ifdef PORTAL_MP
	PickTeam();
#endif

	if ( IsBot() )
		BotSetupModelConVarValue( this );
}

void CPortal_Player::Activate(void)
{
	BaseClass::Activate();
	//SetContextThink(&CPortal_Player::HudHintThink, gpGlobals->curtime + 10, s_pHudHintContext);
	//SetNextThink(gpGlobals->curtime + 10.0f);
	
	const char *pszName = engine->GetClientConVarValue( entindex(), "cl_player_funnel_into_portals" );
	m_bPortalFunnel = atoi( pszName ) != 0;
}

void CPortal_Player::NotifySystemEvent(CBaseEntity* pNotify, notify_system_event_t eventType, const notify_system_event_params_t& params)
{
	// On teleport, we send event for tracking fling achievements
	if (eventType == NOTIFY_EVENT_TELEPORT)
	{
		CProp_Portal* pEnteredPortal = dynamic_cast<CProp_Portal*>(pNotify);
		IGameEvent* event = gameeventmanager->CreateEvent("portal_player_portaled");
		if (event)
		{
			event->SetInt("userid", GetUserID());
			event->SetBool("portal2", pEnteredPortal->m_bIsPortal2);
			gameeventmanager->FireEvent(event);
		}
	}

	BaseClass::NotifySystemEvent(pNotify, eventType, params);
}

void CPortal_Player::OnRestore(void)
{
	BaseClass::OnRestore();
	if (m_pExpresser)
	{
		m_pExpresser->SetOuter(this);
	}
}

//bool CPortal_Player::StartObserverMode( int mode )
//{
//	//Do nothing.
//
//	return false;
//}

bool CPortal_Player::ValidatePlayerModel(const char* pModel)
{
	int iModels = ARRAYSIZE(g_ppszPortalMPModels);
	int i;

	for (i = 0; i < iModels; ++i)
	{
		if (!Q_stricmp(g_ppszPortalMPModels[i], pModel))
		{
			return true;
		}
	}

	return false;
}

const char* DefaultPlayerModel()
{
	// Some mods don't use Chell, so this function is being setup just in case another mod is added in the future that uses a different model
	return "models/player/chell.mdl";
}

void CPortal_Player::SetPlayerModel(void)
{
	const char* szModelName = NULL;
	const char *pszCurrentModelName = modelinfo->GetModelName(GetModel());

	szModelName = engine->GetClientConVarValue( entindex(), "cl_playermodel");
		
	if (ValidatePlayerModel(szModelName) == false)
	{
		char szReturnString[512];
		
		if ( ValidatePlayerModel( pszCurrentModelName ) == false )
		{
			pszCurrentModelName = DefaultPlayerModel();
		}

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", pszCurrentModelName);
		engine->ClientCommand(edict(), szReturnString);

		szModelName = pszCurrentModelName;
	}

	int modelIndex = modelinfo->GetModelIndex(szModelName);

	if (modelIndex == -1)
	{
		szModelName = DefaultPlayerModel();

		char szReturnString[512];

		Q_snprintf(szReturnString, sizeof(szReturnString), "cl_playermodel %s\n", szModelName);
		engine->ClientCommand(edict(), szReturnString);
	}
		
	if (m_PlayerAnimState)
		m_PlayerAnimState->Release();

	SetModel(szModelName);
	ResetAnimation();
	m_PlayerAnimState = CreatePortalPlayerAnimState(this);
	ResetAnimation();

	UpdateExpression();
}

void CPortal_Player::SetupSkin( void )
{
	int iPortalColorSet;
	UTIL_Ping_Color( this, Color(), iPortalColorSet );

	if (iPortalColorSet == 1)
		m_nSkin = 1;
	else if (iPortalColorSet == 3)
		m_nSkin = 2;
	else
		m_nSkin = 0;

}

void CPortal_Player::ResetAnimation(void)
{
	if ( IsAlive() )
	{
		SetSequence ( -1 );
		SetActivity( ACT_INVALID );

		if (!GetAbsVelocity().x && !GetAbsVelocity().y)
			SetAnimation( PLAYER_IDLE );
		else if ((GetAbsVelocity().x || GetAbsVelocity().y) && ( GetFlags() & FL_ONGROUND ))
			SetAnimation( PLAYER_WALK );
		else if (GetWaterLevel() > 1)
			SetAnimation( PLAYER_WALK );
	}
}


bool CPortal_Player::Weapon_Switch(CBaseCombatWeapon* pWeapon, int viewmodelindex)
{
	bool bRet = BaseClass::Weapon_Switch(pWeapon, viewmodelindex);

	if ( bRet == true )
	{
		ResetAnimation();
	}

	return bRet;
}

int CPortal_Player::GetPlayerConcept( void )
{
	const char *pszPlayerModel = GetModelName().ToCStr();

	if (!strcmp(pszPlayerModel, g_ppszPortalMPModels[MODEL_CHELL])) // Chell
		return CONCEPT_CHELL_IDLE;
	else if (!strcmp(pszPlayerModel, g_ppszPortalMPModels[MODEL_MEL])) // Mel
	{
		if ( GlobalEntity_GetState("pcoop_escape_expressions") == GLOBAL_ON )
		{
			return CONCEPT_MEL_ESCAPE_IDLE; // We don't want Mel smiling when she's entering a fire pit or escaping
		}
		else
		{
			return CONCEPT_MEL_IDLE;
		}
	}
	else if (!strcmp(pszPlayerModel, g_ppszPortalMPModels[MODEL_ABBY])) // Abby
		return CONCEPT_ABBY_IDLE;
	else if (!strcmp(pszPlayerModel, g_ppszPortalMPModels[MODEL_MALE_PORTAL_PLAYER])) // male_portal_player
		return CONCEPT_MALE_PORTAL_PLAYER_IDLE;

	return CONCEPT_CHELL_IDLE;

}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortal_Player::UpdateExpression( void )
{
	if ( !m_pExpresser )
	{
		if (this != UTIL_GetListenServerHost())
			Warning("!m_pExpresser\n");
		return;
	}

	int iConcept = GetPlayerConcept();
	if ( IsDead() )
	{
		iConcept = CONCEPT_PLAYER_DEAD;
	}

	GetExpresser()->SetOuter( this );

	ClearExpression();
	AI_Response response;
	bool result = SpeakFindResponse( response, g_pszPortalPlayerConcepts[iConcept] );
	if ( !result )
	{
		m_flExpressionLoopTime = gpGlobals->curtime + RandomFloat(30,40);
		return;
	}

	char const *szScene = response.GetResponsePtr();

	// Ignore updates that choose the same scene
	if ( m_iszExpressionScene != NULL_STRING && stricmp( STRING(m_iszExpressionScene), szScene ) == 0 )
		return;

	if ( m_hExpressionSceneEnt )
	{
		ClearExpression();
	}

	m_iszExpressionScene = AllocPooledString( szScene );
	float flDuration = InstancedScriptedScene( this, szScene, &m_hExpressionSceneEnt, 0.0, true, NULL );
	m_flExpressionLoopTime = gpGlobals->curtime + flDuration;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortal_Player::ClearExpression(void)
{
	if (m_hExpressionSceneEnt != NULL)
	{
		StopScriptedScene(this, m_hExpressionSceneEnt);
	}
	m_flExpressionLoopTime = gpGlobals->curtime;
}

#define PINGTIME 3.0

void ShowAnnotation( Vector location, int follow_entindex, int entindex, int forcedpingicon = -1 )
{
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "show_annotation" );
	if ( pEvent )
	{
		pEvent->SetString( "text", "Ping!" );
					
		pEvent->SetFloat( "worldPosX", location.x );
		pEvent->SetFloat( "worldPosY", location.y );
		pEvent->SetFloat( "worldPosZ", location.z );
		pEvent->SetFloat( "lifetime", PINGTIME );
		pEvent->SetString( "play_sound", COOP_PING_HUD_SOUNDSCRIPT_NAME );
		if ( follow_entindex != -1 )
			pEvent->SetInt( "follow_entindex", follow_entindex );
		pEvent->SetInt( "owner_entindex", entindex );
		pEvent->SetInt( "id", entindex ); // 1 annotation per player
		if ( forcedpingicon != -1 )
			pEvent->SetInt( "forcedpingicon", forcedpingicon );

		gameeventmanager->FireEvent( pEvent );
	}
}

extern ConVar sv_allow_customized_portal_colors;

void CPortal_Player::PlayCoopPingEffect( void )
{
	Vector vecForward;
	AngleVectors( EyeAngles(), &vecForward );
	// Hit anything they can 'see' thats directly down their crosshair
	trace_t tr;

	CTraceFilterSimpleClassnameList traceFilter( this, COLLISION_GROUP_NONE );
	traceFilter.AddClassnameToIgnore( "player" );
	traceFilter.AddClassnameToIgnore( "prop_energy_ball" );
	traceFilter.AddClassnameToIgnore( "prop_combine_ball" );
#if 0
	bool bPortalBulletTrace = g_bBulletPortalTrace;
	g_bBulletPortalTrace = true;
	Ray_t ray;
	ray.Init( EyePosition(), EyePosition() + vecForward*MAX_COORD_FLOAT );
	UTIL_Portal_TraceRay( ray, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	g_bBulletPortalTrace = bPortalBulletTrace;
#else
	UTIL_TraceLine( EyePosition(), EyePosition() + vecForward*MAX_COORD_FLOAT, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
#endif

	if ( tr.DidHit() )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_player_ping" );

		if ( event )
		{
			event->SetInt("userid", GetUserID() );
			event->SetFloat("ping_x", tr.endpos.x );
			event->SetFloat("ping_y", tr.endpos.y );
			event->SetFloat("ping_z", tr.endpos.z );
			gameeventmanager->FireEvent( event );
		}
		
		QAngle angNormal;
		VectorAngles( tr.plane.normal, angNormal );
		angNormal.x += 90.0f;
		
		CDisablePredictionFiltering filter(true);
		//DispatchParticleEffect( COOP_PING_PARTICLE_NAME, tr.endpos, vec3_angle );
		
		// Get our ping color information
		Vector vColor;
		int iPortalColorSet;

		UTIL_Ping_Color( this, vColor, iPortalColorSet );


		Ray_t ray;
		ray.Init( tr.startpos, tr.endpos );

		float flMustBeCloserThan = 2.0f;

		CProp_Portal *pPortal = UTIL_Portal_FirstAlongRayAll( ray , flMustBeCloserThan );
		
		CBaseAnimating *pAnimating = NULL;
		
		if ( tr.m_pEnt )
			pAnimating = tr.m_pEnt->GetBaseAnimating();

		const char* pszAnimatingName = NULL;
		
		if (pAnimating)
			pszAnimatingName = pAnimating->GetClassname();

		CPointPingLinker *pPingLinker = NULL;

		//Find a ping linker to use
		CBaseEntity *pEntityTemp = NULL;
		while ( ( pEntityTemp = gEntList.FindEntityByClassname( pEntityTemp, "point_ping_linker" ) ) != NULL )
		{
			pPingLinker = dynamic_cast<CPointPingLinker*>( pEntityTemp );
			if ( !pPingLinker )
				continue;

			if ( pPingLinker->HasThisEntity( pAnimating ) )
			{
				break;
			}
			else
			{
				pPingLinker = NULL;
			}
		}
		
		bool bShouldCreateCrosshair = true;
		
		if (pAnimating)
		{
			if (PingChildrenOfChildParent(pAnimating, vColor))
			{
				EmitSound( COOP_PING_SOUNDSCRIPT_NAME );
				return;
			}

			if (pPingLinker)
			{
				pPingLinker->PingLinkedEntities( PINGTIME, vColor, this, COOP_PING_HUD_SOUNDSCRIPT_NAME );
			}
			else
			{
				if (pAnimating->m_bGlowEnabled)
				{
					pAnimating->RemoveGlowEffect();
				}

				pAnimating->SetGlowEffectColor(vColor.x, vColor.y, vColor.z);
				pAnimating->AddGlowTime(gpGlobals->curtime);
				pAnimating->RemoveGlowTime(PINGTIME);

				
				//if (bResult)
				{
					ShowAnnotation( pAnimating->GetAbsOrigin(), pAnimating->entindex(), entindex() );
				}
			}

			bShouldCreateCrosshair = false;
		}
		else if ( !pAnimating && pPortal )
		{
			ShowAnnotation( pPortal->GetAbsOrigin(), pPortal->entindex(), entindex() );			
			bShouldCreateCrosshair = false;
		}
		else if ( !pAnimating && !pPortal )
		{
			bShouldCreateCrosshair = PingChildrenOfEntity( tr, vColor, bShouldCreateCrosshair );
		}

		if (bShouldCreateCrosshair)
		{
			if (iPortalColorSet == 1)
				DispatchParticleEffect( COOP_PING_PARTICLE_NAME_PURPLE, tr.endpos, angNormal, this );
			else if (iPortalColorSet == 2)
				DispatchParticleEffect( COOP_PING_PARTICLE_NAME_RED, tr.endpos, angNormal, this );
			else if (iPortalColorSet == 3)
				DispatchParticleEffect( COOP_PING_PARTICLE_NAME_GREEN, tr.endpos, angNormal, this );
			else
				DispatchParticleEffect( COOP_PING_PARTICLE_NAME_ORANGE, tr.endpos, angNormal, this );

			ShowAnnotation( tr.endpos, -1, entindex() );
		}

		EmitSound( COOP_PING_SOUNDSCRIPT_NAME );
	//	UTIL_DecalTrace( &tr, "Portal2.CoopPingDecal" );

	}

	// Note this in the player proxy
	FirePlayerProxyOutput( "OnCoopPing", variant_t(), this, this );
}

//-----------------------------------------------------------------------------
// Purpose: Ping all of my siblings
// TODO: Does this need to be a member function?
//-----------------------------------------------------------------------------
bool CPortal_Player::PingChildrenOfChildParent( CBaseAnimating *pAnimating, Vector vColor )
{
	if (!pAnimating)
		return false;
	if (!pAnimating->GetParent())
		return false;
	
	bool bResult = false;

	CBaseEntity *pParent = pAnimating->GetParent();

	CBaseDoor *pDoor = dynamic_cast<CBaseDoor *>(pAnimating->GetParent());
	CFuncTrackTrain *pTrain = dynamic_cast<CFuncTrackTrain*>(pAnimating->GetParent());
	if ( !pDoor && !pTrain )
		return false;
	
	CUtlVector<CBaseEntity *> children;
	GetAllChildren( pParent, children );
	for (int i = 0; i < children.Count(); i++ )
	{
		CBaseEntity *pEnt = children.Element( i );

		if (!pEnt)
			continue;

		CBaseAnimating *pChild = pEnt->GetBaseAnimating();
			
		if ( pChild )
		{
			if (pChild->m_bGlowEnabled)
			{
				pChild->RemoveGlowEffect();
				m_bGlowEnabled.Set(false);
			}

			pChild->SetGlowEffectColor(vColor.x, vColor.y, vColor.z);
			pChild->AddGlowTime(gpGlobals->curtime);
			pChild->RemoveGlowTime(PINGTIME);
			bResult = true;
		}
	}

	if (bResult)
	{
		ShowAnnotation( pParent->GetAbsOrigin(), -1, entindex() );
	}

	return bResult;
}

bool CPortal_Player::PingChildrenOfEntity( trace_t &tr, Vector vColor, bool bShouldCreateCrosshair )
{
	bool bTempShouldCreateCrosshair = bShouldCreateCrosshair;

	CBaseEntity *pEntity = tr.m_pEnt;
	CBaseDoor *pDoor = dynamic_cast<CBaseDoor*>( pEntity );
	CFuncTrackTrain *pTrain = dynamic_cast<CFuncTrackTrain*>( pEntity );
	if ( !pDoor && !pTrain )
		return bShouldCreateCrosshair;
		
	if (pDoor)
	{
		CBaseAnimating *pChild = NULL;
		CBaseAnimating *pChildForLinker = NULL;
		CPointPingLinker *pPingLinker = NULL;

		bool bShouldGetChild = true;
		
		CUtlVector<CBaseEntity *> children;
		GetAllChildren( pEntity, children );
		for (int i = 0; i < children.Count(); i++ )
		{
			CBaseEntity *pEnt = children.Element( i );

			if (!pEnt)
				continue;

			pChild = pEnt->GetBaseAnimating();
			
			if ( pChild )
			{						
				if (pChild->m_bGlowEnabled)
				{
					pChild->RemoveGlowEffect();
				}

				if (bShouldGetChild)
				{
					pChildForLinker = pChild;
					bShouldGetChild = false;
				}

				pChild->SetGlowEffectColor(vColor.x, vColor.y, vColor.z);
				pChild->AddGlowTime(gpGlobals->curtime);
				pChild->RemoveGlowTime(PINGTIME);
				bTempShouldCreateCrosshair = false;
			}
		}

		//Find a ping linker to use
		CBaseEntity *pEntityTemp = NULL;
		while ( ( pEntityTemp = gEntList.FindEntityByClassname( pEntityTemp, "point_ping_linker" ) ) != NULL )
		{
			pPingLinker = dynamic_cast<CPointPingLinker*>( pEntityTemp );
			if ( !pPingLinker )
				continue;
			
			if ( pPingLinker->HasThisEntity( pChildForLinker ) )
			{
				break;
			}
			else
			{
				pPingLinker = NULL;
			}
		}

		if (pPingLinker)
		{
			pPingLinker->PingLinkedEntities( PINGTIME, vColor, this, COOP_PING_HUD_SOUNDSCRIPT_NAME );
		}

		if (!pPingLinker) // Ping Linkers fire their own events
		{
			ShowAnnotation( pEntity->GetAbsOrigin(), pEntity->entindex(), entindex() );
		}
	}

	return bTempShouldCreateCrosshair;

}

void CPortal_Player::PreThink(void)
{
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	if (m_flLookForUseEntityTime >= gpGlobals->curtime && m_bLookForUseEntity)
	{
		SetLookingForUseEntity(true);

		if (PlayerUse())
		{
			SetLookForUseEntity(false);
			m_flLookForUseEntityTime = gpGlobals->curtime;
		}

		// This should allow us to play the sound again if we press use again while we're already searching
		SetLookingForUseEntity(false);
	}

	vTempAngles = EyeAngles();

	if (vTempAngles[PITCH] > 180.0f)
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles(vTempAngles);

	BaseClass::PreThink();

	if ((m_afButtonPressed & IN_JUMP))
	{
		Jump();
	}
	
	//if ( GameRules()->IsMultiplayer() )
	{
		// Send a ping
		if ( m_afButtonPressed & IN_COOP_PING && sv_portal_coop_allow_ping.GetBool())
		{
			if ( ( m_flLastPingTime + sv_portal_coop_ping_cooldown_time.GetFloat() ) < gpGlobals->curtime && IsAlive() )
			{
				PlayCoopPingEffect();
				m_flLastPingTime = gpGlobals->curtime;
			}
		}
	}


	
	int iPortalColorSet;

	Color color;

	UTIL_Ping_Color( this, color, iPortalColorSet );

	m_flGlowR = color.r() / 255;
	m_flGlowG = color.g() / 255;
	m_flGlowB = color.b() / 255;

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;

	SetLocalAngles(vOldAngles);
}

void CPortal_Player::PostThink(void)
{
	BaseClass::PostThink();
	
	m_PlayerAnimState->Update(m_angEyeAngles[YAW], m_angEyeAngles[PITCH]);

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles(angles);

	// Regenerate heath after 3 seconds
	if (IsAlive() && GetHealth() < GetMaxHealth())
	{
		// Color to overlay on the screen while the player is taking damage
		color32 hurtScreenOverlay = { 64,0,0,64 };

		if (gpGlobals->curtime > m_fTimeLastHurt + sv_regeneration_wait_time.GetFloat())
		{
			TakeHealth(1, DMG_GENERIC);
			m_bIsRegenerating = true;

			if (GetHealth() >= GetMaxHealth())
			{
				m_bIsRegenerating = false;
			}
		}
		else
		{
			m_bIsRegenerating = false;
			UTIL_ScreenFade(this, hurtScreenOverlay, 1.0f, 0.1f, FFADE_IN | FFADE_PURGE);
		}
	}

	UpdatePortalPlaneSounds();
	UpdateWooshSounds();
	
	if (IsAlive() && m_flExpressionLoopTime >= 0 && gpGlobals->curtime > m_flExpressionLoopTime)
	{
		// Random expressions need to be cleared, because they don't loop. So if we
		// pick the same one again, we want to restart it.
		ClearExpression();
		m_iszExpressionScene = NULL_STRING;
		UpdateExpression();
	}

//	UpdateSecondsTaken();
	// Try to fix the player if they're stuck
	if (m_bStuckOnPortalCollisionObject)
	{
		Vector vForward = ((CProp_Portal*)m_hPortalEnvironment.Get())->m_vPrevForward;
		Vector vNewPos = GetAbsOrigin() + vForward * gpGlobals->frametime * -1000.0f;
		Teleport(&vNewPos, NULL, &vForward);
		m_bStuckOnPortalCollisionObject = false;
	}
}

ConVar sv_portalgun_fizzle_on_owner_death_always ("sv_portalgun_fizzle_on_owner_death_always", "1", FCVAR_REPLICATED, "Sets if the portalgun should always die if the player dies");

void CPortal_Player::PlayerDeathThink(void)
{
	float flForward;

	SetNextThink(gpGlobals->curtime + 0.1f);

	if (GetFlags() & FL_ONGROUND)
	{
		flForward = GetAbsVelocity().Length() - 20;
		if (flForward <= 0)
		{
			SetAbsVelocity(vec3_origin);
		}
		else
		{
			Vector vecNewVelocity = GetAbsVelocity();
			VectorNormalize(vecNewVelocity);
			vecNewVelocity *= flForward;
			SetAbsVelocity(vecNewVelocity);
		}
	}

	if (HasWeapons())
	{
		if (sv_portalgun_fizzle_on_owner_death_always.GetBool())
		{
			CWeaponPortalgun *pPortalgun = static_cast<CWeaponPortalgun*>(Weapon_OwnsThisType("weapon_portalgun"));
			if (pPortalgun)
			{
				pPortalgun->FizzleOwnedPortals();
			}
		}

		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	if (GetModelIndex() && (!IsSequenceFinished()) && (m_lifeState == LIFE_DYING))
	{
		StudioFrameAdvance();

		m_iRespawnFrames++;
		if (m_iRespawnFrames < 60)  // animations should be no longer than this
			return;
	}

	if (m_lifeState == LIFE_DYING)
		m_lifeState = LIFE_DEAD;

	StopAnimation();

	AddEffects(EF_NOINTERP);
	m_flPlaybackRate = 0.0;

	int fAnyButtonDown = (m_nButtons & ~IN_SCORE);

	// Strip out the duck key from this check if it's toggled
	if ((fAnyButtonDown & IN_DUCK) && GetToggledDuckState())
	{
		fAnyButtonDown &= ~IN_DUCK;
	}

	// wait for all buttons released
	if (m_lifeState == LIFE_DEAD)
	{
		if (fAnyButtonDown || gpGlobals->curtime < m_flDeathTime + DEATH_ANIMATION_TIME)
			return;

		if (g_pGameRules->FPlayerCanRespawn(this))
		{
			m_lifeState = LIFE_RESPAWNABLE;
		}

		return;
	}
	
	// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if (!fAnyButtonDown
		&& !(g_pGameRules->IsMultiplayer() && forcerespawn.GetInt() > 0 && (gpGlobals->curtime > (m_flDeathTime + 5))))
		return;

	m_nButtons = 0;
	m_iRespawnFrames = 0;

	//Msg( "Respawn\n");

	respawn(this, !IsObserver());// don't copy a corpse if we're in deathcam.
	SetNextThink(TICK_NEVER_THINK);
}

void CPortal_Player::InputDoPingHudHint( inputdata_t &inputdata )
{
	UTIL_HudHintText(this, "#portalcoop_Hint_Ping_Marker");
}

void CPortal_Player::UpdatePortalPlaneSounds(void)
{
	CProp_Portal* pPortal = m_hPortalEnvironment;
	if (pPortal && pPortal->IsActive())
	{
		Vector vVelocity = GetAbsVelocity();
		//GetVelocity(&vVelocity, NULL);

		if (!vVelocity.IsZero())
		{
			Vector vMin, vMax;
			CollisionProp()->WorldSpaceAABB(&vMin, &vMax);

			Vector vEarCenter = (vMax + vMin) / 2.0f;
			Vector vDiagonal = vMax - vMin;

			if (!m_bIntersectingPortalPlane)
			{
				vDiagonal *= 0.25f;

				if (UTIL_IsBoxIntersectingPortal(vEarCenter, vDiagonal, pPortal))
				{
					m_bIntersectingPortalPlane = true;

					CPASAttenuationFilter filter(this);
					filter.RemoveRecipient(this);
					CSoundParameters params;
					if (GetParametersForSound("PortalPlayer.EnterPortal", params, NULL))
					{
						EmitSound_t ep(params);
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = min(0.3f + vVelocity.Length() * 0.00075f, 1.0f);

						EmitSound(filter, entindex(), ep);
					}
				}
			}
			else
			{
				vDiagonal *= 0.30f;

				if (!UTIL_IsBoxIntersectingPortal(vEarCenter, vDiagonal, pPortal))
				{
					m_bIntersectingPortalPlane = false;

					CPASAttenuationFilter filter(this);
					filter.RemoveRecipient(this);
					CSoundParameters params;
					if (GetParametersForSound("PortalPlayer.ExitPortal", params, NULL))
					{
						EmitSound_t ep(params);
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = min(0.3f + vVelocity.Length() * 0.00075f, 1.0f);

						EmitSound(filter, entindex(), ep);
					}
				}
			}
		}
	}
	else if (m_bIntersectingPortalPlane)
	{
		m_bIntersectingPortalPlane = false;

		CPASAttenuationFilter filter(this);
		filter.RemoveRecipient(this);
		CSoundParameters params;
		if (GetParametersForSound("PortalPlayer.ExitPortal", params, NULL))
		{
			EmitSound_t ep(params);
			Vector vVelocity;
			GetVelocity(&vVelocity, NULL);
			ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
			ep.m_flVolume = min(0.3f + vVelocity.Length() * 0.00075f, 1.0f);

			EmitSound(filter, entindex(), ep);
		}
	}
}

void CPortal_Player::UpdateWooshSounds(void)
{
	if (m_pWooshSound)
	{
		CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();

		float fWooshVolume = GetAbsVelocity().Length() - MIN_FLING_SPEED;

		if (fWooshVolume < 0.0f)
		{
			controller.SoundChangeVolume(m_pWooshSound, 0.0f, 0.1f);
			return;
		}

		fWooshVolume /= 2000.0f;
		if (fWooshVolume > 1.0f)
			fWooshVolume = 1.0f;

		controller.SoundChangeVolume(m_pWooshSound, fWooshVolume, 0.1f);
		//		controller.SoundChangePitch( m_pWooshSound, fWooshVolume + 0.5f, 0.1f );
	}
}

void CPortal_Player::FireBullets(const FireBulletsInfo_t& info)
{
	NoteWeaponFired();

	BaseClass::FireBullets(info);
}

void CPortal_Player::NoteWeaponFired(void)
{
	Assert(m_pCurrentCommand);
	if (m_pCurrentCommand)
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;

bool CPortal_Player::WantsLagCompensationOnEntity(const CBasePlayer* pPlayer, const CUserCmd* pCmd, const CBitVec<MAX_EDICTS>* pEntityTransmitBits) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if (!(pCmd->buttons & IN_ATTACK) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5))
		return false;

	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if (pEntityTransmitBits && !pEntityTransmitBits->Get(pPlayer->entindex()))
		return false;

	const Vector& vMyOrigin = GetAbsOrigin();
	const Vector& vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time, 
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if (vHisOrigin.DistTo(vMyOrigin) < maxDistance)
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors(pCmd->viewangles, &vForward);

	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize(vDiff);

	float flCosAngle = 0.707107f;	// 45 degree angle
	if (vForward.Dot(vDiff) < flCosAngle)
		return false;

	return true;
}

void CPortal_Player::DoAnimationEvent(PlayerAnimEvent_t event, int nData)
{
	m_PlayerAnimState->DoAnimationEvent(event, nData);
	TE_PlayerAnimEvent(this, event, nData);	// Send to any clients who can see this guy.
}

//-----------------------------------------------------------------------------
// Purpose: Override setup bones so that is uses the render angles from
//			the Portal animation state to setup the hitboxes.
//-----------------------------------------------------------------------------
void CPortal_Player::SetupBones(matrix3x4_t* pBoneToWorld, int boneMask)
{
	VPROF_BUDGET("CBaseAnimating::SetupBones", VPROF_BUDGETGROUP_SERVER_ANIM);

	// Set the mdl cache semaphore.
	MDLCACHE_CRITICAL_SECTION();

	// Get the studio header.
	Assert(GetModelPtr());
	CStudioHdr* pStudioHdr = GetModelPtr();

	Vector pos[MAXSTUDIOBONES];
	Quaternion q[MAXSTUDIOBONES];

	// Adjust hit boxes based on IK driven offset.
	Vector adjOrigin = GetAbsOrigin() + Vector(0, 0, m_flEstIkOffset);

	// FIXME: pass this into Studio_BuildMatrices to skip transforms
	CBoneBitList boneComputed;
	if (m_pIk)
	{
		m_iIKCounter++;
		m_pIk->Init(pStudioHdr, GetAbsAngles(), adjOrigin, gpGlobals->curtime, m_iIKCounter, boneMask);
		GetSkeleton(pStudioHdr, pos, q, boneMask);

		m_pIk->UpdateTargets(pos, q, pBoneToWorld, boneComputed);
		CalculateIKLocks(gpGlobals->curtime);
		m_pIk->SolveDependencies(pos, q, pBoneToWorld, boneComputed);
	}
	else
	{
		GetSkeleton(pStudioHdr, pos, q, boneMask);
	}

	CBaseAnimating* pParent = NULL;

	if ( GetMoveParent() )
		pParent = GetMoveParent()->GetBaseAnimating();

	if (pParent)
	{
		// We're doing bone merging, so do special stuff here.
		CBoneCache* pParentCache = pParent->GetBoneCache();
		if (pParentCache)
		{
			BuildMatricesWithBoneMerge(
				pStudioHdr,
				m_PlayerAnimState->GetRenderAngles(),
				adjOrigin,
				pos,
				q,
				pBoneToWorld,
				pParent,
				pParentCache);

			return;
		}
	}

	Studio_BuildMatrices(
		pStudioHdr,
		m_PlayerAnimState->GetRenderAngles(),
		adjOrigin,
		pos,
		q,
		-1,
		1.0f,
		pBoneToWorld,
		boneMask);
}


// Set the activity based on an event or current state
void CPortal_Player::SetAnimation( PLAYER_ANIM playerAnim )
{

}

CAI_Expresser* CPortal_Player::CreateExpresser()
{
	Assert(!m_pExpresser);

	if (m_pExpresser)
	{
		delete m_pExpresser;
	}

	m_pExpresser = new CAI_Expresser(this);
	if (!m_pExpresser)
	{
		return NULL;
	}
	m_pExpresser->Connect(this);

	return m_pExpresser;
}

//-----------------------------------------------------------------------------

CAI_Expresser* CPortal_Player::GetExpresser()
{
	if (m_pExpresser)
	{
		m_pExpresser->Connect(this);
	}
	return m_pExpresser;
}


extern int	gEvilImpulse101;
//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon. 
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CPortal_Player::BumpWeapon(CBaseCombatWeapon* pWeapon)
{
	CBaseCombatCharacter* pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
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

	// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
	if (!pWeapon->FVisible(this, MASK_SOLID) && !(GetFlags() & FL_NOTARGET))
	{
		return false;
	}

	CWeaponPortalgun* pPickupPortalgun = dynamic_cast<CWeaponPortalgun*>(pWeapon);

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType());

	if (bOwnsWeaponAlready == true)
	{
		// If we picked up a second portal gun set the bool to alow secondary fire
		if (pPickupPortalgun)
		{
			CWeaponPortalgun* pPortalGun = static_cast<CWeaponPortalgun*>(Weapon_OwnsThisType(pWeapon->GetClassname()));

			if (pPickupPortalgun->CanFirePortal1())
				pPortalGun->SetCanFirePortal1();

			if (pPickupPortalgun->CanFirePortal2())
				pPortalGun->SetCanFirePortal2();

			UTIL_Remove(pWeapon);
			return true;
		}

		//If we have room for the ammo, then "take" the weapon too.
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

	// If we're holding and object before picking up portalgun, drop it
	if (pPickupPortalgun)
	{
		ForceDropOfCarriedPhysObjects(GetPlayerHeldEntity(this));
	}

	return true;
}

void CPortal_Player::ShutdownUseEntity(void)
{
	ShutdownPickupController(m_hUseEntity);
}

void CPortal_Player::Teleport( const Vector* newPosition, const QAngle* newAngles, const Vector* newVelocity )
{
	BaseClass::Teleport(newPosition, newAngles, newVelocity);
	m_angEyeAngles = pl.v_angle;

	m_PlayerAnimState->Teleport(newPosition, newAngles, this);
}

void CPortal_Player::VPhysicsShadowUpdate(IPhysicsObject* pPhysics)
{
	if (m_hPortalEnvironment.Get() == NULL)
		return BaseClass::VPhysicsShadowUpdate(pPhysics);


	//below is mostly a cut/paste of existing CBasePlayer::VPhysicsShadowUpdate code with some minor tweaks to avoid getting stuck in stuff when in a portal environment
	if (sv_turbophysics.GetBool())
		return;

	Vector newPosition;

	bool physicsUpdated = m_pPhysicsController->GetShadowPosition(&newPosition, NULL) > 0 ? true : false;

	// UNDONE: If the player is penetrating, but the player's game collisions are not stuck, teleport the physics shadow to the game position
	if (pPhysics->GetGameFlags() & FVPHYSICS_PENETRATING)
	{
		CUtlVector<CBaseEntity*> list;
		PhysGetListOfPenetratingEntities(this, list);
		for (int i = list.Count() - 1; i >= 0; --i)
		{
			// filter out anything that isn't simulated by vphysics
			// UNDONE: Filter out motion disabled objects?
			if (list[i]->GetMoveType() == MOVETYPE_VPHYSICS)
			{
				// I'm currently stuck inside a moving object, so allow vphysics to 
				// apply velocity to the player in order to separate these objects
				m_touchedPhysObject = true;
			}
		}
	}

	if (m_pPhysicsController->IsInContact() || (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER))
	{
		m_touchedPhysObject = true;
	}

	if (IsFollowingPhysics())
	{
		m_touchedPhysObject = true;
	}

	if (GetMoveType() == MOVETYPE_NOCLIP)
	{
		m_oldOrigin = GetAbsOrigin();
		return;
	}

	if (phys_timescale.GetFloat() == 0.0f)
	{
		physicsUpdated = false;
	}

	if (!physicsUpdated)
		return;

	IPhysicsObject* pPhysGround = GetGroundVPhysics();

	Vector newVelocity;
	pPhysics->GetPosition(&newPosition, 0);
	m_pPhysicsController->GetShadowVelocity(&newVelocity);



	Vector tmp = GetAbsOrigin() - newPosition;
	if (!m_touchedPhysObject && !(GetFlags() & FL_ONGROUND))
	{
		tmp.z *= 0.5f;	// don't care about z delta as much
	}

	float dist = tmp.LengthSqr();
	float deltaV = (newVelocity - GetAbsVelocity()).LengthSqr();

	float maxDistErrorSqr = VPHYS_MAX_DISTSQR;
	float maxVelErrorSqr = VPHYS_MAX_VELSQR;
	if (IsRideablePhysics(pPhysGround))
	{
		maxDistErrorSqr *= 0.25;
		maxVelErrorSqr *= 0.25;
	}

	if (dist >= maxDistErrorSqr || deltaV >= maxVelErrorSqr || (pPhysGround && !m_touchedPhysObject))
	{
		if (m_touchedPhysObject || pPhysGround)
		{
			// BUGBUG: Rewrite this code using fixed timestep
			if (deltaV >= maxVelErrorSqr)
			{
				Vector dir = GetAbsVelocity();
				float len = VectorNormalize(dir);
				float dot = DotProduct(newVelocity, dir);
				if (dot > len)
				{
					dot = len;
				}
				else if (dot < -len)
				{
					dot = -len;
				}

				VectorMA(newVelocity, -dot, dir, newVelocity);

				if (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER)
				{
					float val = Lerp(0.1f, len, dot);
					VectorMA(newVelocity, val - len, dir, newVelocity);
				}

				if (!IsRideablePhysics(pPhysGround))
				{
					if (!(m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER) && IsSimulatingOnAlternateTicks())
					{
						newVelocity *= 0.5f;
					}
					ApplyAbsVelocityImpulse(newVelocity);
				}
			}

			trace_t trace;
			UTIL_TraceEntity(this, newPosition, newPosition, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace);
			if (!trace.allsolid && !trace.startsolid)
			{
				SetAbsOrigin(newPosition);
			}
		}
		else
		{
			trace_t trace;

			Ray_t ray;
			ray.Init(GetAbsOrigin(), GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs());

			CTraceFilterSimple OriginalTraceFilter(this, COLLISION_GROUP_PLAYER_MOVEMENT);
			CTraceFilterTranslateClones traceFilter(&OriginalTraceFilter);
			UTIL_Portal_TraceRay_With(m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace);

			// current position is not ok, fixup
			if (trace.allsolid || trace.startsolid)
			{
				//try again with new position
				ray.Init(newPosition, newPosition, WorldAlignMins(), WorldAlignMaxs());
				UTIL_Portal_TraceRay_With(m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace);

				if (trace.startsolid == false)
				{
					SetAbsOrigin(newPosition);
				}
				else
				{
					if (!FindClosestPassableSpace(this, newPosition - GetAbsOrigin(), MASK_PLAYERSOLID))
					{
						// Try moving the player closer to the center of the portal
						CProp_Portal* pPortal = m_hPortalEnvironment.Get();
						newPosition += (pPortal->GetAbsOrigin() - WorldSpaceCenter()) * 0.1f;
						SetAbsOrigin(newPosition);

						DevMsg("Hurting the player for FindClosestPassableSpaceFailure!");

						// Deal 1 damage per frame... this will kill a player very fast, but allow for the above correction to fix some cases
						CTakeDamageInfo info(this, this, vec3_origin, vec3_origin, 1, DMG_CRUSH);
						OnTakeDamage(info);
					}
				}
			}
		}
	}
	else
	{
		if (m_touchedPhysObject)
		{
			// check my position (physics object could have simulated into my position
			// physics is not very far away, check my position
			trace_t trace;
			UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin(),
				MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace);

			// is current position ok?
			if (trace.allsolid || trace.startsolid)
			{
				// stuck????!?!?
				//Msg("Stuck on %s\n", trace.m_pEnt->GetClassname());
				SetAbsOrigin(newPosition);
				UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin(),
					MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace);
				if (trace.allsolid || trace.startsolid)
				{
					//Msg("Double Stuck\n");
					SetAbsOrigin(m_oldOrigin);
				}
			}
		}
	}
	m_oldOrigin = GetAbsOrigin();
}

//bool CPortal_Player::StartReplayMode( float fDelay, float fDuration, int iEntity )
//{
//	if ( !BaseClass::StartReplayMode( fDelay, fDuration, 1 ) )
//		return false;
//
//	CSingleUserRecipientFilter filter( this );
//	filter.MakeReliable();
//
//	UserMessageBegin( filter, "KillCam" );
//
//	EHANDLE hPlayer = this;
//
//	if ( m_hObserverTarget.Get() )
//	{
//		WRITE_EHANDLE( m_hObserverTarget );	// first target
//		WRITE_EHANDLE( hPlayer );	//second target
//	}
//	else
//	{
//		WRITE_EHANDLE( hPlayer );	// first target
//		WRITE_EHANDLE( 0 );			//second target
//	}
//	MessageEnd();
//
//	return true;
//}
//
//void CPortal_Player::StopReplayMode()
//{
//	BaseClass::StopReplayMode();
//
//	CSingleUserRecipientFilter filter( this );
//	filter.MakeReliable();
//
//	UserMessageBegin( filter, "KillCam" );
//	WRITE_EHANDLE( 0 );
//	WRITE_EHANDLE( 0 );
//	MessageEnd();
//}

void CPortal_Player::PlayerRunCommand(CUserCmd* ucmd, IMoveHelper* moveHelper)
{
	//============================================================================
	// Fix the eye angles after portalling. The client may have sent commands with
	// the old view angles before it knew about the teleportation.

	//sorry for crappy name, the client sent us a command, we acknowledged it, and they are now telling us the latest one they received an acknowledgement for in this brand new command
	int iLastCommandAcknowledgementReceivedOnClientForThisCommand = ucmd->command_number - ucmd->command_acknowledgements_pending;
	while( (m_PendingPortalTransforms.Count() > 0) && (iLastCommandAcknowledgementReceivedOnClientForThisCommand >= m_PendingPortalTransforms[0].command_number) )
	{
		m_PendingPortalTransforms.Remove( 0 );
	}

	// The server changed the angles, and the user command was created after the teleportation, but before the client knew they teleported. Need to fix up the angles into the new space
	if( m_PendingPortalTransforms.Count() > ucmd->predictedPortalTeleportations )
	{
		matrix3x4_t matComputeFinalTransform[2];
		int iFlip = 0;

		//most common case will be exactly 1 transform
		matComputeFinalTransform[0] = m_PendingPortalTransforms[ucmd->predictedPortalTeleportations].matTransform;

		for( int i = ucmd->predictedPortalTeleportations + 1; i < m_PendingPortalTransforms.Count(); ++i )
		{
			ConcatTransforms( m_PendingPortalTransforms[i].matTransform, matComputeFinalTransform[iFlip], matComputeFinalTransform[1-iFlip] );
			iFlip = 1 - iFlip;
		}

		//apply the final transform
		matrix3x4_t matAngleTransformIn, matAngleTransformOut;
		AngleMatrix( ucmd->viewangles, matAngleTransformIn );
		ConcatTransforms( matComputeFinalTransform[iFlip], matAngleTransformIn, matAngleTransformOut );
		MatrixAngles( matAngleTransformOut, ucmd->viewangles );
	}

	BaseClass::PlayerRunCommand(ucmd, moveHelper);
}


bool CPortal_Player::ClientCommand(const CCommand& args)
{
	if (FStrEq(args[0], "spectate"))
	{
		// do nothing.
		return true;
	}

	return BaseClass::ClientCommand(args);
}

void CPortal_Player::CheatImpulseCommands(int iImpulse)
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

void CPortal_Player::CreateViewModel(int index /*=0*/)
{
	BaseClass::CreateViewModel(index);
	return;
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

bool CPortal_Player::BecomeRagdollOnClient(const Vector& force)
{
	return true;//BaseClass::BecomeRagdollOnClient( force );
}

void CPortal_Player::CreateRagdollEntity(const CTakeDamageInfo& info)
{
#if PORTAL_HIDE_PLAYER_RAGDOLL
	AddSolidFlags(FSOLID_NOT_SOLID);
	AddEffects(EF_NODRAW | EF_NOSHADOW);
	AddEFlags(EFL_NO_DISSOLVE);
#endif // PORTAL_HIDE_PLAYER_RAGDOLL

#if 0
	if (m_hRagdoll)
	{
		UTIL_Remove(m_hRagdoll);
		m_hRagdoll = NULL;
	}
	CBaseEntity* pRagdoll = CreateServerRagdoll(this, m_nForceBone, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true);
	pRagdoll->m_takedamage = DAMAGE_NO;
	m_hRagdoll = pRagdoll;

	/*
		// If we already have a ragdoll destroy it.
		CPortalRagdoll *pRagdoll = dynamic_cast<CPortalRagdoll*>( m_hRagdoll.Get() );
		if( pRagdoll )
		{
			UTIL_Remove( pRagdoll );
			pRagdoll = NULL;
		}
		Assert( pRagdoll == NULL );
		// Create a ragdoll.
		pRagdoll = dynamic_cast<CPortalRagdoll*>( CreateEntityByName( "portal_ragdoll" ) );
		if ( pRagdoll )
		{

			pRagdoll->m_hPlayer = this;
			pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
			pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
			pRagdoll->m_nModelIndex = m_nModelIndex;
			pRagdoll->m_nForceBone = m_nForceBone;
			pRagdoll->CopyAnimationDataFrom( this );
			pRagdoll->SetOwnerEntity( this );
			pRagdoll->m_flAnimTime = gpGlobals->curtime;
			pRagdoll->m_flPlaybackRate = 0.0;
			pRagdoll->SetCycle( 0 );
			pRagdoll->ResetSequence( 0 );
			float fSequenceDuration = SequenceDuration( GetSequence() );
			float fPreviousCycle = clamp(GetCycle()-( 0.1 * ( 1 / fSequenceDuration ) ),0.f,1.f);
			float fCurCycle = GetCycle();
			matrix3x4_t pBoneToWorld[MAXSTUDIOBONES], pBoneToWorldNext[MAXSTUDIOBONES];
			SetupBones( pBoneToWorldNext, BONE_USED_BY_ANYTHING );
			SetCycle( fPreviousCycle );
			SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING );
			SetCycle( fCurCycle );
			pRagdoll->InitRagdoll( info.GetDamageForce(), m_nForceBone, info.GetDamagePosition(), pBoneToWorld, pBoneToWorldNext, 0.1f, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
			pRagdoll->SetMoveType( MOVETYPE_VPHYSICS );
			pRagdoll->SetSolid( SOLID_VPHYSICS );
			if ( IsDissolving() )
			{
				pRagdoll->TransferDissolveFrom( this );
			}
			Vector mins, maxs;
			mins = CollisionProp()->OBBMins();
			maxs = CollisionProp()->OBBMaxs();
			pRagdoll->CollisionProp()->SetCollisionBounds( mins, maxs );
			pRagdoll->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
		}
		// Turn off the player.
		AddSolidFlags( FSOLID_NOT_SOLID );
		AddEffects( EF_NODRAW | EF_NOSHADOW );
		SetMoveType( MOVETYPE_NONE );
		// Save ragdoll handle.
		m_hRagdoll = pRagdoll;
	*/
#endif
}

void CPortal_Player::Jump(void)
{
	g_PortalGameStats.Event_PlayerJump(GetAbsOrigin(), GetAbsVelocity());
	BaseClass::Jump();
}

void CPortal_Player::Event_Killed(const CTakeDamageInfo& info)
{
	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce(m_vecTotalBulletForce);

	// show killer in death cam mode
	// chopped down version of SetObserverTarget without the team check
	//if( info.GetAttacker() )
	//{
	//	// set new target
	//	m_hObserverTarget.Set( info.GetAttacker() ); 
	//}
	//else
	//	m_hObserverTarget.Set( NULL );

	UpdateExpression();

	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.
	CreateRagdollEntity(info);

	BaseClass::Event_Killed(subinfo);

#if PORTAL_HIDE_PLAYER_RAGDOLL
	/*
	// Fizzle all portals so they don't see the player disappear
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	CWeaponPortalgun* pPortalgun = (CWeaponPortalgun*)GetActiveWeapon();
	if( pPortalgun )
	{
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			//HACKISH: Do this before the chain to base... basecombatcharacer will dump the weapon
			// and then we won't know what linkage ID to fizzle. This is relevant in multiplayer,
			// where we want only the dying player's portals to fizzle.
			if( pTempPortal && ( pPortalgun->m_iPortalLinkageGroupID == pTempPortal->GetLinkageGroup() ) )
			{
				pTempPortal->Fizzle();
			}
		}
	}
	*/
#endif // PORTAL_HIDE_PLAYER_RAGDOLL
#if !PORTAL_HIDE_PLAYER_RAGDOLL
	if ((info.GetDamageType() & DMG_DISSOLVE) && !(m_hRagdoll.Get()->GetEFlags() & EFL_NO_DISSOLVE))
	{
		if (m_hRagdoll)
		{
			m_hRagdoll->GetBaseAnimating()->Dissolve(NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL);
		}
	}
#endif
	m_lifeState = LIFE_DYING;
	StopZooming();

	if (GetObserverTarget())
	{
		//StartReplayMode( 3, 3, GetObserverTarget()->entindex() );
		//StartObserverMode( OBS_MODE_DEATHCAM );
	}
}

int CPortal_Player::OnTakeDamage(const CTakeDamageInfo& inputInfo)
{
	CTakeDamageInfo inputInfoCopy(inputInfo);

	// If you shoot yourself, make it hurt but push you less
	if (inputInfoCopy.GetAttacker() == this && inputInfoCopy.GetDamageType() == DMG_BULLET)
	{
		inputInfoCopy.ScaleDamage(5.0f);
		inputInfoCopy.ScaleDamageForce(0.05f);
	}

	CBaseEntity* pAttacker = inputInfoCopy.GetAttacker();
	CBaseEntity* pInflictor = inputInfoCopy.GetInflictor();
	/*
	if (pAttacker && pAttacker->IsMarkedForDeletion())
		pAttacker = NULL;

	if ( pInflictor && pInflictor->IsMarkedForDeletion())
		pInflictor = NULL;
	*/
	bool bIsTurret = false;

	if (pAttacker && FClassnameIs(pAttacker, "npc_portal_turret_floor"))
		bIsTurret = true;

	// Refuse damage from prop_glados_core.
	if ((pAttacker && FClassnameIs(pAttacker, "prop_glados_core")) ||
		(pInflictor && FClassnameIs(pInflictor, "prop_glados_core")))
	{
		inputInfoCopy.SetDamage(0.0f);
	}

	if (bIsTurret && (inputInfoCopy.GetDamageType() & DMG_BULLET))
	{
		Vector vLateralForce = inputInfoCopy.GetDamageForce();
		vLateralForce.z = 0.0f;

		// Push if the player is moving against the force direction
		if (GetAbsVelocity().Dot(vLateralForce) < 0.0f)
			ApplyAbsVelocityImpulse(vLateralForce);
	}
	else if ((inputInfoCopy.GetDamageType() & DMG_CRUSH))
	{
		if (bIsTurret)
		{
			inputInfoCopy.SetDamage(inputInfoCopy.GetDamage() * 0.5f);
		}

		if (inputInfoCopy.GetDamage() >= 10.0f)
		{
			EmitSound("PortalPlayer.BonkYelp");
		}
	}
	else if ((inputInfoCopy.GetDamageType() & DMG_SHOCK) || (inputInfoCopy.GetDamageType() & DMG_BURN))
	{
		EmitSound("PortalPortal.PainYelp");
	}

	int ret = BaseClass::OnTakeDamage(inputInfoCopy);

	// Copy the multidamage damage origin over what the base class wrote, because
	// that gets translated correctly though portals.
	m_DmgOrigin = inputInfo.GetDamagePosition();

	if (GetHealth() < 100)
	{
		m_fTimeLastHurt = gpGlobals->curtime;
	}

	return ret;
}

int CPortal_Player::OnTakeDamage_Alive(const CTakeDamageInfo& info)
{
	// set damage type sustained
	m_bitsDamageType |= info.GetDamageType();

	if (!CBaseCombatCharacter::OnTakeDamage_Alive(info))
		return 0;

	CBaseEntity* attacker = info.GetAttacker();

	if (!attacker)
		return 0;

	Vector vecDir = vec3_origin;
	if (info.GetInflictor())
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector(0, 0, 10) - WorldSpaceCenter();
		VectorNormalize(vecDir);
	}

	if (info.GetInflictor() && (GetMoveType() == MOVETYPE_WALK) &&
		(!attacker->IsSolidFlagSet(FSOLID_TRIGGER)))
	{
		Vector force = vecDir;// * -DamageForce( WorldAlignSize(), info.GetBaseDamage() );
		if (force.z > 250.0f)
		{
			force.z = 250.0f;
		}
		ApplyAbsVelocityImpulse(force);
	}

	// fire global game event

	IGameEvent* event = gameeventmanager->CreateEvent("player_hurt");
	if (event)
	{
		event->SetInt("userid", GetUserID());
		event->SetInt("health", max(0, m_iHealth));
		event->SetInt("priority", 5);	// HLTV event priority, not transmitted

		if (attacker->IsPlayer())
		{
			CBasePlayer* player = ToBasePlayer(attacker);
			event->SetInt("attacker", player->GetUserID()); // hurt by other player
		}
		else
		{
			event->SetInt("attacker", 0); // hurt by "world"
		}

		gameeventmanager->FireEvent(event);
	}

	// Insert a combat sound so that nearby NPCs hear battle
	if (attacker->IsNPC())
	{
		CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 512, 0.5, this);//<<TODO>>//magic number
	}

	return 1;
}

extern ConVar pcoop_avoidplayers;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPortal_Player::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( ( ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ) && pcoop_avoidplayers.GetBool() ) )
	{
		return false;		
	}
	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

void CPortal_Player::ForceDropOfCarriedPhysObjects(CBaseEntity* pOnlyIfHoldingThis)
{
	m_bHeldObjectOnOppositeSideOfPortal = false;
	BaseClass::ForceDropOfCarriedPhysObjects(pOnlyIfHoldingThis);
}


//-----------------------------------------------------------------------------
// Purpose: Update the area bits variable which is networked down to the client to determine
//			which area portals should be closed based on visibility.
// Input  : *pvs - pvs to be used to determine visibility of the portals
//-----------------------------------------------------------------------------
void CPortal_Player::UpdatePortalViewAreaBits(unsigned char* pvs, int pvssize)
{
	Assert(pvs);

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if (iPortalCount == 0)
		return;

	CProp_Portal** pPortals = CProp_Portal_Shared::AllPortals.Base();
	int* portalArea = (int*)stackalloc(sizeof(int) * iPortalCount);
	bool* bUsePortalForVis = (bool*)stackalloc(sizeof(bool) * iPortalCount);

	unsigned char* portalTempBits = (unsigned char*)stackalloc(sizeof(unsigned char) * 32 * iPortalCount);
	COMPILE_TIME_ASSERT((sizeof(unsigned char) * 32) >= sizeof(((CPlayerLocalData*)0)->m_chAreaBits));

	// setup area bits for these portals
	for (int i = 0; i < iPortalCount; ++i)
	{
		CProp_Portal* pLocalPortal = pPortals[i];
		// Make sure this portal is active before adding it's location to the pvs
		if (pLocalPortal && pLocalPortal->IsActive())
		{
			CProp_Portal* pRemotePortal = pLocalPortal->m_hLinkedPortal.Get();

			// Make sure this portal's linked portal is in the PVS before we add what it can see
			if (pRemotePortal && pRemotePortal->IsActive() && pRemotePortal->NetworkProp() &&
				pRemotePortal->NetworkProp()->IsInPVS(edict(), pvs, pvssize))
			{
				portalArea[i] = engine->GetArea(pPortals[i]->GetAbsOrigin());

				if (portalArea[i] >= 0)
				{
					bUsePortalForVis[i] = true;
				}

				engine->GetAreaBits(portalArea[i], &portalTempBits[i * 32], sizeof(unsigned char) * 32);
			}
		}
	}

	// Use the union of player-view area bits and the portal-view area bits of each portal
	for (int i = 0; i < m_Local.m_chAreaBits.Count(); i++)
	{
		for (int j = 0; j < iPortalCount; ++j)
		{
			// If this portal is active, in PVS and it's location is valid
			if (bUsePortalForVis[j])
			{
				m_Local.m_chAreaBits.Set(i, m_Local.m_chAreaBits[i] | portalTempBits[(j * 32) + i]);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// AddPortalCornersToEnginePVS
// Subroutine to wrap the adding of portal corners to the PVS which is called once for the setup of each portal.
// input - pPortal: the portal we are viewing 'out of' which needs it's corners added to the PVS
//////////////////////////////////////////////////////////////////////////
void AddPortalCornersToEnginePVS(CProp_Portal* pPortal)
{
	Assert(pPortal);

	if (!pPortal)
		return;

	Vector vForward, vRight, vUp;
	pPortal->GetVectors(&vForward, &vRight, &vUp);

	// Center of the remote portal
	Vector ptOrigin = pPortal->GetAbsOrigin();

	// Distance offsets to the different edges of the portal... Used in the placement checks
	Vector vToTopEdge = vUp * (PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS);
	Vector vToBottomEdge = -vToTopEdge;
	Vector vToRightEdge = vRight * (PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS);
	Vector vToLeftEdge = -vToRightEdge;

	// Distance to place PVS points away from portal, to avoid being in solid
	Vector vForwardBump = vForward * 1.0f;

	// Add center and edges to the engine PVS
	engine->AddOriginToPVS(ptOrigin + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToTopEdge + vToLeftEdge + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToTopEdge + vToRightEdge + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToBottomEdge + vToLeftEdge + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToBottomEdge + vToRightEdge + vForwardBump);
}

void PortalSetupVisibility(CBaseEntity* pPlayer, int area, unsigned char* pvs, int pvssize)
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if (iPortalCount == 0)
		return;

	CProp_Portal** pPortals = CProp_Portal_Shared::AllPortals.Base();
	for (int i = 0; i != iPortalCount; ++i)
	{
		CProp_Portal* pPortal = pPortals[i];

		if (pPortal && pPortal->IsActive())
		{
			if (pPortal->NetworkProp()->IsInPVS(pPlayer->edict(), pvs, pvssize))
			{
				if (engine->CheckAreasConnected(area, pPortal->NetworkProp()->AreaNum()))
				{
					CProp_Portal* pLinkedPortal = static_cast<CProp_Portal*>(pPortal->m_hLinkedPortal.Get());
					if (pLinkedPortal)
					{
						AddPortalCornersToEnginePVS(pLinkedPortal);
					}
				}
			}
		}
	}
}

void CPortal_Player::SetupVisibility(CBaseEntity* pViewEntity, unsigned char* pvs, int pvssize)
{
	BaseClass::SetupVisibility(pViewEntity, pvs, pvssize);

	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();

	// At this point the EyePosition has been added as a view origin, but if we are currently stuck
	// in a portal, our EyePosition may return a point in solid. Find the reflected eye position
	// and use that as a vis origin instead.
	if (m_hPortalEnvironment)
	{
		CProp_Portal* pPortal = NULL, * pRemotePortal = NULL;
		pPortal = m_hPortalEnvironment;
		pRemotePortal = pPortal->m_hLinkedPortal;

		if (pPortal && pRemotePortal && pPortal->IsActive() && pRemotePortal->IsActive())
		{
			Vector ptPortalCenter = pPortal->GetAbsOrigin();
			Vector vPortalForward;
			pPortal->GetVectors(&vPortalForward, NULL, NULL);

			Vector eyeOrigin = EyePosition();
			Vector vEyeToPortalCenter = ptPortalCenter - eyeOrigin;

			float fPortalDist = vPortalForward.Dot(vEyeToPortalCenter);
			if (fPortalDist > 0.0f) //eye point is behind portal
			{
				// Move eye origin to it's transformed position on the other side of the portal
				UTIL_Portal_PointTransform(pPortal->MatrixThisToLinked(), eyeOrigin, eyeOrigin);

				// Use this as our view origin (as this is where the client will be displaying from)
				engine->AddOriginToPVS(eyeOrigin);
				if (!pViewEntity || pViewEntity->IsPlayer())
				{
					area = engine->GetArea(eyeOrigin);
				}
			}
		}
	}

	PortalSetupVisibility(this, area, pvs, pvssize);
}


CBaseEntity* CPortal_Player::EntSelectSpawnPoint(void)
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "info_player_portalcoop" ) ) != NULL )
	{
		CInfoPlayerPortalCoop *pCoopSpawn = static_cast<CInfoPlayerPortalCoop*>( pEntity );
		if ( pCoopSpawn->CanSpawnOnMe( this ) )
		{
			return pCoopSpawn;
		}
	}

	// If a normal coop spawn wasn't found, use the normal behavior
	return BaseClass::EntSelectSpawnPoint();
}

#ifdef PORTAL_MP


void CPortal_Player::PickTeam(void)
{
	//picks lowest or random
	CTeam* pCombine = g_Teams[TEAM_COMBINE];
	CTeam* pRebels = g_Teams[TEAM_REBELS];
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

#endif

void CPortal_Player::ApplyPortalTeleportation( const CProp_Portal *pEnteredPortal, CMoveData *pMove )
{
#if PLAYERPORTALDEBUGSPEW == 1
	Warning( "SERVER CPortal_Player::ApplyPortalTeleportation( %f %i )\n", gpGlobals->curtime, m_pCurrentCommand->command_number );
#endif

	//catalog the pending transform
	{
		RecentPortalTransform_t temp;
		temp.command_number = GetCurrentUserCommand()->command_number;
		temp.Portal = pEnteredPortal;
		temp.matTransform = pEnteredPortal->m_matrixThisToLinked.As3x4();

		m_PendingPortalTransforms.AddToTail( temp );

		//prune the pending transforms so it doesn't get ridiculously huge if the client stops responding while in an infinite fall or something
		while( m_PendingPortalTransforms.Count() > 64 )
		{
			m_PendingPortalTransforms.Remove( 0 );
		}
	}
#if 1
	CBaseEntity *pHeldEntity = GetPlayerHeldEntity( this );
	if ( pHeldEntity )
	{
		ToggleHeldObjectOnOppositeSideOfPortal();
		SetHeldObjectPortal( IsHeldObjectOnOppositeSideOfPortal() ? pEnteredPortal->m_hLinkedPortal.Get() : NULL );
	}
#endif
	//transform m_PlayerAnimState yaws
	m_PlayerAnimState->TransformYAWs( pEnteredPortal->m_matrixThisToLinked.As3x4() );

	//physics transform
	{
		SetVCollisionState( pMove->GetAbsOrigin(), pMove->m_vecVelocity, IsDucked() ? VPHYS_CROUCH : VPHYS_WALK );
	}

	//transform local velocity
	{
		//Vector vTransformedLocalVelocity;
		//VectorRotate( GetAbsVelocity(), pEnteredPortal->m_matrixThisToLinked.As3x4(), vTransformedLocalVelocity );
		//SetAbsVelocity( vTransformedLocalVelocity );
		SetAbsVelocity( pMove->m_vecVelocity );
	}

	//transform base velocity
	{
		Vector vTransformedBaseVelocity;
		VectorRotate( GetBaseVelocity(), pEnteredPortal->m_matrixThisToLinked.As3x4(), vTransformedBaseVelocity );
		SetBaseVelocity( vTransformedBaseVelocity );
	}

	CollisionRulesChanged();
}


void CPortal_Player::NetworkPortalTeleportation( CBaseEntity *pOther, CProp_Portal *pPortal, float fTime, bool bForcedDuck )
{
	CEntityPortalledNetworkMessage &writeTo = m_EntityPortalledNetworkMessages[m_iEntityPortalledNetworkMessageCount % MAX_ENTITY_PORTALLED_NETWORK_MESSAGES];

	writeTo.m_hEntity = pOther;
	writeTo.m_hPortal = pPortal;
	writeTo.m_fTime = fTime;
	writeTo.m_bForcedDuck = bForcedDuck;
	writeTo.m_iMessageCount = m_iEntityPortalledNetworkMessageCount;
	++m_iEntityPortalledNetworkMessageCount;

	//NetworkProp()->NetworkStateChanged( offsetof( CPortal_Player, m_EntityPortalledNetworkMessages ) );
	NetworkProp()->NetworkStateChanged();
}

CON_COMMAND(startadmiregloves, "Starts the admire gloves animation.")
{
	CPortal_Player* pPlayer = (CPortal_Player*)UTIL_GetCommandClient();
	if (pPlayer == NULL)
		pPlayer = GetPortalPlayer(1); //last ditch effort

	if (pPlayer)
		pPlayer->StartAdmireGlovesAnimation();
}

CON_COMMAND(displayportalplayerstats, "Displays current level stats for portals placed, steps taken, and seconds taken.")
{
	{
		int iMinutes = static_cast<int>(PortalGameRules()->NumSecondsTaken() / 60.0f);
		int iSeconds = static_cast<int>(PortalGameRules()->NumSecondsTaken()) % 60;

		CFmtStr msg;
		NDebugOverlay::ScreenText(0.5f, 0.5f, msg.sprintf("Portals Placed: %d\nSteps Taken: %d\nTime: %d:%d", PortalGameRules()->NumPortalsPlaced(), PortalGameRules()->NumStepsTaken(), iMinutes, iSeconds), 255, 255, 255, 150, 5.0f);
	}
}