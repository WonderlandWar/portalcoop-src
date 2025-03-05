//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== portal_client.cpp ========================================================

  Portal client/server game specific stuff

*/

#include "cbase.h"
#include "portal_player.h"
#include "portal_gamerules.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "viewport_panel_names.h"
#include "weapon_portalgun.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;

void FinishClientPutInServer( CPortal_Player *pPlayer )
{
	pPlayer->InitialSpawn();
	pPlayer->Spawn();
	
#if 0
	const ConVar *hostname = cvar->FindVar( "hostname" );
	const char *title = (hostname) ? hostname->GetString() : "MESSAGE OF THE DAY";

	KeyValues *data = new KeyValues("data");
	data->SetString( "title", title );		// info panel title
	data->SetString( "type", "1" );			// show userdata from stringtable entry
	data->SetString( "msg",	"motd" );		// use this stringtable entry
	data->SetBool( "unload", sv_motd_unload_on_dismissal.GetBool() );

	pPlayer->ShowViewPortPanel( PANEL_INFO, true, data );

	data->deleteThis();
#endif
}

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBasePlayer for pev, and call spawn
	CPortal_Player *pPlayer = CPortal_Player::CreatePlayer( "player", pEdict );
	pPlayer->PlayerData()->netname = AllocPooledString( playername );
	pPlayer->SetPlayerName(playername);
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{

	CPortal_Player *pPlayer = ToPortalPlayer(CBaseEntity::Instance(pEdict));
	FinishClientPutInServer(pPlayer);

	/*
	CPortal_Player *pPlayer = dynamic_cast< CPortal_Player* >( CBaseEntity::Instance( pEdict ) );
	Assert( pPlayer );

	pPlayer->InitialSpawn();

	if ( !bLoadGame )
	{
		pPlayer->Spawn();
	}
	*/
}


/*
===============
const char *GetGameDescription()

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Portal: Cooperative";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		return (FindPickerEntityClass( static_cast<CBasePlayer*>(GetContainingEntity(pEdict)), classname ));
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	CBaseEntity::PrecacheModel("models/player.mdl");
	CBaseEntity::PrecacheModel( "models/gibs/agibs.mdl" );
	CBaseEntity::PrecacheModel("models/weapons/v_hands.mdl");

	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowAmmo" );
	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowHealth" );

	CBaseEntity::PrecacheScriptSound( "Missile.ShotDown" );
	CBaseEntity::PrecacheScriptSound( "Bullets.DefaultNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.GunshipNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.StriderNearmiss" );
	
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepHigh" );
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepLow" );

	CBaseEntity::PrecacheModel( "models/portals/portal1.mdl" );
	CBaseEntity::PrecacheModel( "models/portals/portal2.mdl" );
}

// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	CPortalGameRulesProxy *pProxy = static_cast<CPortalGameRulesProxy*>( gEntList.FindEntityByClassname( NULL, "portal_gamerules" ) );
	if ( pProxy && pProxy->m_bSuspendRespawn )
	{
		return;
	}

	if ( fCopyCorpse )
	{
		// make a copy of the dead body for appearances sake
		((CPortal_Player *)pEdict)->CreateCorpse();
	}

	// respawn player
	pEdict->Spawn();
}

void RespawnAllPlayers()
{	
	CPortalGameRulesProxy *pProxy = static_cast<CPortalGameRulesProxy*>( gEntList.FindEntityByClassname( NULL, "portal_gamerules" ) );
	if ( pProxy && pProxy->m_bSuspendRespawn )
	{
		return;
	}

	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer)
			continue;
		
		respawn(pPlayer, false);

		CWeaponPortalgun *pPortalgun = dynamic_cast<CWeaponPortalgun*>(pPlayer->Weapon_OwnsThisType("weapon_portalgun"));

		if (pPortalgun)
		{
			pPortalgun->FizzleOwnedPortals();
			pPortalgun->SetLastFiredPortal(0);
		}
	}

}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");
	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = (teamplay.GetInt() != 0);

	extern void Bot_RunAll();
	Bot_RunAll();
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	CreateGameRulesObject("CPortalGameRules");
}

