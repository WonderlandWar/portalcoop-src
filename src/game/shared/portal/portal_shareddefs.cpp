//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "portal_shareddefs.h"
#include "filesystem.h"
#ifdef CLIENT_DLL
#include "replay/IEngineReplay.h"
#include "c_playerresource.h"
#else
#include "player_resource.h"
#endif

ConVar pcoop_require_all_players( "pcoop_require_all_players", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Effectively pauses the game when there are not enough players in the server" );
ConVar pcoop_require_all_players_force_amount( "pcoop_require_all_players_force_amount", "-2", FCVAR_NOTIFY | FCVAR_REPLICATED, "Force a certain amount of required players instead of using max players. -1 = use max players\n-2 = use the map's data file", true, -2, true, MAX_PLAYERS );

#ifdef GAME_DLL
ConVar sv_portal_game_update_on_map_load( "sv_portal_game_update_on_map_load", "1", FCVAR_REPLICATED, "Updates the server's portal game type\n" );
#endif
ConVar sv_portal_game( "sv_portal_game", "0", FCVAR_REPLICATED, "The server's portal game type, automatically changes on map load if sv_portal_game_update_on_map_load is enabled\n0 = Portal\n1 = Rexaura\n" );

char *g_ppszPortalPassThroughMaterials[] = 
{ 
	"lights/light_orange001", 
	NULL,
};

PortalColorSet_t ConvertLinkageIDToColorSet( int iPortalLinkageID )
{
	// The % PORTAL_COLOR_SET_LAST is necessary for >3 maxplayer servers
	return (PortalColorSet_t)(iPortalLinkageID % (PORTAL_COLOR_SET_LAST+1));
}

PortalColorSet_t GetColorSetForPlayer( int iPlayer )
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayer );
	if ( pPlayer )
	{
		if ( pPlayer->IsObserver() )
		{
			return PORTAL_COLOR_SET_OBSERVER;
		}
	}

	// Linkage IDs are based on the player index
	return ConvertLinkageIDToColorSet( iPlayer );
}

KeyValues *LoadRadioData()
{	
	KeyValues *radios = new KeyValues( "radios.txt" );
	if ( !radios->LoadFromFile( g_pFullFileSystem, RADIO_DATA_FILE, "MOD" ) )
	{
		AssertMsg( false, "Failed to load radio data" );
		radios->deleteThis();
		return NULL;
	}

	return radios;
}

CMapInfo::CMapInfo()
{
	Reset();
}

void CMapInfo::Reset( void )
{
	m_iRequiredPlayers = -1;
#ifdef CLIENT_DLL
	V_strcpy( m_szCreditsFile, "scripts/credits.txt" );
#endif
}

const char *g_pszAllPcoopMaps[] =
{
	// Normal 2 player
	//
	"p2coop_0",
	"p2coop_1",
	"p2coop_2",
	"p2coop_3",
	"p2coop_4",
	"p2coop_5",
	"p2coop_6",
	"p2coop_7",
	//"p2coop_7_challenge",
	"p2coop_8",
	"p2coop_9",
	"p2coop_10",

	// Normal 3 player
	//
	"p3coop_00",
	"p3coop_01",

	// Rexaura 2 player
	//
	"rex2c_00",
	"rex2c_01_destroyer_intro",
	"rex2c_02_box_reflector_intro",
	"rex2c_03_inf_ball_intro",
	"rex2c_04_double_reflect",
	"rex2c_05_field_intro",
	"rex2c_06_portalgun_upgrade",
	//"rex2c_06_portalgun_upgrade_advanced",
	"rex2c_07_dissolver",
	"rex2c_08_ball_juggle",
	"rex2c_09_timed_catcher_intro",
	"rex2c_10_box_deliver",
	"rex2c_11_portal_save",
	//"rex2c_11_portal_save_advanced",
	//"rex2c_box_separation",

	//"pcoop_playground"
};

int GetPCoopMapCount()
{
	return ARRAYSIZE( g_pszAllPcoopMaps );
}

CMapInfo g_MapInfo;

int GetRequiredPlayers()
{
	int nRequiredPlayers = 0;
	if ( pcoop_require_all_players_force_amount.GetInt() == -1 )
	{
		nRequiredPlayers = gpGlobals->maxClients;
	}
	else if ( pcoop_require_all_players_force_amount.GetInt() == -2 )
	{
		if ( g_MapInfo.GetRequiredPlayers() != -1 )
		{
			nRequiredPlayers = g_MapInfo.GetRequiredPlayers();
		}
		else
		{
			//Warning( "Map didn't have required players set, using maxplayers\n" );
			// Just use maxclients if it fails to load
			nRequiredPlayers = gpGlobals->maxClients;
		}
	}
	else
	{
		nRequiredPlayers = pcoop_require_all_players_force_amount.GetInt();
	}

	//Msg("nRequiredPlayers: %i\n", nRequiredPlayers);
	return nRequiredPlayers;
}

bool PlayerShouldPlay( int index )
{
	return index <= GetRequiredPlayers();
}

bool Map_Is2Player( const char *pMapName )
{
	return V_stristr( pMapName, "p2coop_" ) || V_stristr( pMapName, "rex2c_" );
}

bool Map_Is3Player( const char *pMapName )
{
	return V_stristr( pMapName, "p3coop_" ) || V_stristr( pMapName, "rex3c_" );
}

KeyValues *LoadMapDataForMap( const char *pszMapName )
{
	char szMapFilePath[MAX_MAP_NAME];
	Q_snprintf( szMapFilePath, sizeof(szMapFilePath), "maps/mapdata/%s.txt", pszMapName );
	KeyValues *pMapData = new KeyValues("mapdata");
	if ( !pMapData->LoadFromFile( g_pFullFileSystem, szMapFilePath, "MOD" ) )
	{
		AssertMsg( false, "Map data not found" );
		pMapData->deleteThis();
		return NULL;
	}

	return pMapData;
}

class CMapDataLoader : public CAutoGameSystem
{
public:
	virtual void LevelInitPreEntity();
};

CMapDataLoader g_MapDataLoader;

void CMapDataLoader::LevelInitPreEntity()
{
#ifdef GAME_DLL
	const char *pszMapName = gpGlobals->mapname.ToCStr();
#else
	const char *pszMapName = g_pEngineClientReplay->GetLevelNameShort();
#endif

	KeyValues *pMapData = LoadMapDataForMap( pszMapName );
	if ( !pMapData )
	{
#ifdef DEBUG // Only assert on 2 player and 3 player maps
		if ( (V_stristr( pszMapName, "p2coop_" ) || V_stristr( pszMapName, "p3coop_" )) &&
			(V_stristr( pszMapName, "rex2c_" ) || V_stristr( pszMapName, "rex3c_" )) )
		{
			AssertMsg( false, "Failed to load map data" );
		}
#endif

		g_MapInfo.Reset();
		return;
	}

	Msg("Map loaded: %s\n", pszMapName);

	g_MapInfo.m_iRequiredPlayers = pMapData->GetInt( "required_players", -1 );
#ifdef CLIENT_DLL
	V_strcpy( g_MapInfo.m_szCreditsFile, pMapData->GetString( "credits_file", "scripts/credits.txt" ) );
#endif

	pMapData->deleteThis();
}

void ExecuteLoadingMapSetFunction( MapSetFunc func )
{
	// Check the soundscripts
	const char* pCurrentPath = "scripts/mapsets/";
	
	char szDirectory[_MAX_PATH];
	Q_snprintf( szDirectory, sizeof( szDirectory ), "%s*", pCurrentPath );

	FileFindHandle_t dirHandle;
	const char *pDirFileName = g_pFullFileSystem->FindFirst( szDirectory, &dirHandle );

	while (pDirFileName)
	{
		// Skip it if it's not a directory, is the root, is back, or is an invalid folder
		if ( !g_pFullFileSystem->FindIsDirectory( dirHandle ) || 
		Q_strcmp( pDirFileName, "." ) == 0 || 
		Q_strcmp( pDirFileName, ".." ) == 0 )
		{
			pDirFileName = g_pFullFileSystem->FindNext( dirHandle );
			continue;
		}

		char szFullDirectory[_MAX_PATH];
		Q_snprintf( szFullDirectory, sizeof( szFullDirectory ), "scripts/mapsets/%s", pDirFileName );
		func( szFullDirectory );

		pDirFileName = g_pFullFileSystem->FindNext( dirHandle );
	}
}