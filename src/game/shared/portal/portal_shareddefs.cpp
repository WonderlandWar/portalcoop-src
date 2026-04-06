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
	memset( m_szAssociatedMapSet, 0, sizeof( m_szAssociatedMapSet ) );
#ifdef CLIENT_DLL
	V_strcpy( m_szCreditsFile, "scripts/credits.txt" );
#endif
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
	const char *associated_mapset = pMapData->GetString( "associated_mapset", NULL );
	if ( associated_mapset )
	{
		V_strcpy( g_MapInfo.m_szAssociatedMapSet, associated_mapset );
	}
	else
	{
		memset( g_MapInfo.m_szAssociatedMapSet, 0, sizeof( g_MapInfo.m_szAssociatedMapSet ) );
	}
#ifdef CLIENT_DLL
	V_strcpy( g_MapInfo.m_szCreditsFile, pMapData->GetString( "credits_file", "scripts/credits.txt" ) );
#endif

	pMapData->deleteThis();
}

bool MapSetIsOfficial( const char *mapsetname )
{
	KeyValues *mapsets_official = new KeyValues( "mapsets" );
	if ( !mapsets_official->LoadFromFile( g_pFullFileSystem, "scripts/mapsets/mapsets_official.txt", "GAME" ) )
	{
		mapsets_official->deleteThis();
		return false;
	}
	
	for ( KeyValues *mapset = mapsets_official->GetFirstSubKey(); mapset != NULL; mapset = mapset->GetNextKey() )
	{
		if ( !V_stricmp( mapset->GetName(), mapsetname ) )
		{
			mapsets_official->deleteThis();
			return true;
		}
	}

	mapsets_official->deleteThis();
	return false;
}

void ExecuteLoadingMapSetFunction( MapSetFunc func, void *pData )
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
		func( szFullDirectory, pData );

		pDirFileName = g_pFullFileSystem->FindNext( dirHandle );
	}
}

static void GetMapSetTitle( const char *pFilename, void *pData )
{
	char szFullDirectory[_MAX_PATH];
	Q_snprintf( szFullDirectory, sizeof( szFullDirectory ), "%s/mapsets.txt", pFilename );

	KeyValues *mapsets = new KeyValues( "mapsets" );
	if ( !mapsets->LoadFromFile( g_pFullFileSystem, szFullDirectory, "GAME" ) )
	{
		mapsets->deleteThis();
		return;
	}
	
	void **array = (void**)pData;
	char *pszName = (char*)array[0];
	char *pszTitle = (char*)array[1];
	
	for ( KeyValues *mapset = mapsets->GetFirstSubKey(); mapset != NULL; mapset = mapset->GetNextKey() )
	{
		if ( !V_stricmp( mapset->GetName(), pszName ) )
		{
			V_strcpy( pszTitle, mapset->GetString( "name" ) );
		}
	}

	mapsets->deleteThis();
}

void GetTitleForMapSet( char *szTitle, const char *mapset )
{
	void *array[2] = 
	{
		(void*)mapset,
		szTitle
	};
	ExecuteLoadingMapSetFunction( GetMapSetTitle, array );
}