//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_SHAREDDEFS_H
#define PORTAL_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif


#define PORTAL_HALF_WIDTH 32.0f
#define PORTAL_HALF_HEIGHT 54.0f
#define PORTAL_HALF_DEPTH 2.0f
#define PORTAL_BUMP_FORGIVENESS 2.0f

#define PORTAL_ANALOG_SUCCESS_NO_BUMP 1.0f
#define PORTAL_ANALOG_SUCCESS_STEAL 0.9f
#define PORTAL_ANALOG_SUCCESS_BUMPED 0.3f
#define PORTAL_ANALOG_SUCCESS_CANT_FIT 0.1f
#define PORTAL_ANALOG_SUCCESS_CLEANSER 0.028f
#define PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED 0.027f
#define PORTAL_ANALOG_SUCCESS_NEAR 0.0265f
#define PORTAL_ANALOG_SUCCESS_INVALID_VOLUME 0.026f
#define PORTAL_ANALOG_SUCCESS_INVALID_SURFACE 0.025f
#define PORTAL_ANALOG_SUCCESS_PASSTHROUGH_SURFACE 0.0f

#define MIN_FLING_SPEED 300

#define PORTAL_HIDE_PLAYER_RAGDOLL 1

enum PortalFizzleType_t
{
	PORTAL_FIZZLE_SUCCESS = 0,			// Placed fine (no fizzle)
	PORTAL_FIZZLE_CANT_FIT,
	PORTAL_FIZZLE_OVERLAPPED_LINKED,
	PORTAL_FIZZLE_BAD_VOLUME,
	PORTAL_FIZZLE_BAD_SURFACE,
	PORTAL_FIZZLE_KILLED,
	PORTAL_FIZZLE_CLEANSER,
	PORTAL_FIZZLE_CLOSE,
	PORTAL_FIZZLE_NEAR_BLUE,
	PORTAL_FIZZLE_NEAR_RED,
	PORTAL_FIZZLE_STOLEN,
	PORTAL_FIZZLE_NONE,

	NUM_PORTAL_FIZZLE_TYPES
};


enum PortalPlacedByType
{
	PORTAL_PLACED_BY_FIXED = 0,
	PORTAL_PLACED_BY_PEDESTAL,
	PORTAL_PLACED_BY_PLAYER
};

enum PortalLevelStatType
{
	PORTAL_LEVEL_STAT_NUM_PORTALS = 0,
	PORTAL_LEVEL_STAT_NUM_STEPS,
	PORTAL_LEVEL_STAT_NUM_SECONDS,

	PORTAL_LEVEL_STAT_TOTAL
};

enum PortalChallengeType
{
	PORTAL_CHALLENGE_NONE = 0,
	PORTAL_CHALLENGE_PORTALS,
	PORTAL_CHALLENGE_STEPS,
	PORTAL_CHALLENGE_TIME,

	PORTAL_CHALLENGE_TOTAL
};

enum PortalEvent_t
{
	PORTALEVENT_LINKED,					// This portal has linked to another portal and opened
	PORTALEVENT_FIZZLE,					// Portal has fizzled 
	PORTALEVENT_MOVED,					// Portal has moved its position
	PORTALEVENT_ENTITY_TELEPORTED_TO,	// Entity (player or not) has teleported to this portal
	PORTALEVENT_ENTITY_TELEPORTED_FROM,	// Entity (player or not) has teleported away from this portal
	PORTALEVENT_PLAYER_TELEPORTED_TO,	// Player has teleported to this portal
	PORTALEVENT_PLAYER_TELEPORTED_FROM,	// Player has teleported away from this portal
};

enum PortalColorSet_t
{
	PORTAL_COLOR_SET_INVALID = -1,

	PORTAL_COLOR_SET_BLUE_ORANGE,
	PORTAL_COLOR_SET_LIGHTBLUE_PURPLE,
	PORTAL_COLOR_SET_YELLOW_RED,
	PORTAL_COLOR_SET_GREEN_PINK, // The last "real" color set

	// Not a real color set
	PORTAL_COLOR_SET_OBSERVER,
};

#define PORTAL_COLOR_SET_DEFAULT PORTAL_COLOR_SET_BLUE_ORANGE
#define PORTAL_COLOR_SET_LAST PORTAL_COLOR_SET_GREEN_PINK

#define PORTAL_COLOR_ORANGE Color(255,160,32,255)
#define PORTAL_COLOR_BLUE Color(64,160,255,255)

#define PORTAL_COLOR_LIGHTBLUE Color(128,244,255,255)
#define PORTAL_COLOR_PURPLE Color(128,0,255,255)

#define PORTAL_COLOR_YELLOW Color(255,255,0,255)
#define PORTAL_COLOR_RED Color(255,0,0,255)

#define PORTAL_COLOR_GREEN Color(0,255,0,255)
#define PORTAL_COLOR_PINK Color(255,0,255,255)

#define MAX_MAPSET_LENGTH 16
#define MAX_MAPSET_TITLE_LENGTH 32

#define MAX_USER_CONVAR_LENGTH 255
#define MAPSET_ID_LENGTH 2
#define MAPSET_ID_DELIMITER ','
#define MAPSET_ID_DELIMITER_CONST ","

// ConVar Length divided by the ID and delimiter
#define MAX_NETWORKED_MAPSETS MAX_USER_CONVAR_LENGTH / (MAPSET_ID_LENGTH+1)

PortalColorSet_t ConvertLinkageIDToColorSet( int iPortalLinkageID );
PortalColorSet_t GetColorSetForPlayer( int iPlayer );

// It's better to have a list of Portal mods instead of having a 
// single cvar for Rexaura in case we want to add more mods (Portal: Prelude, Portal: Pro, Blue Portals, etc...)
enum PortalGameType_t
{
	PORTAL_GAME_PORTAL,
	PORTAL_GAME_REXAURA,
};

extern ConVar sv_portal_game;

extern char *g_ppszPortalPassThroughMaterials[];

extern ConVar pcoop_require_all_players;
extern ConVar pcoop_require_all_players_force_amount;

#define USE_BASIC_RADIOS

#define RADIO_DATA_FILE "scripts/radios.txt"
extern KeyValues *LoadRadioData();

// Map data stuff
//
extern KeyValues *LoadMapDataForMap( const char *map );

class CMapInfo
{
public:
	CMapInfo();
	void Reset();

	int GetRequiredPlayers() { return m_iRequiredPlayers; }
	const char* GetAssociatedMapSet( void ) { return m_szAssociatedMapSet; }
#ifdef CLIENT_DLL
	const char *GetCreditsFile( void ) { return m_szCreditsFile; }
#endif

private:
	int m_iRequiredPlayers;
	char m_szAssociatedMapSet[MAX_MAPSET_LENGTH];
#ifdef CLIENT_DLL
	char m_szCreditsFile[64];
#endif

	friend class CMapDataLoader;
};

extern CMapInfo g_MapInfo;

int GetRequiredPlayers();

bool PlayerShouldPlay( int index );

bool Map_Is2Player( const char *pMapName );
bool Map_Is3Player( const char *pMapName );

inline bool Map_IsNormal( const char *pMapName )
{
	return V_stristr( pMapName, "p2coop_" ) || V_stristr( pMapName, "p3coop_" );
}

inline bool Map_IsRexaura( const char *pMapName )
{
	return V_stristr( pMapName, "rex2c_" ) || V_stristr( pMapName, "rex3c_" ) || V_stristr( pMapName, "rex_" );
}

typedef bool (MapSetFunc)(const char *pDirectory, void *pData );

void GetTitleForMapSet( char *szTitle, const char *mapset );

bool MapSetIsOfficial( const char *mapsetname );

// If the MapSetFunc returns true, then the game will stop the search
void ExecuteLoadingMapSetFunction( MapSetFunc func, void *pData = NULL );

#endif // PORTAL_SHAREDDEFS_H
