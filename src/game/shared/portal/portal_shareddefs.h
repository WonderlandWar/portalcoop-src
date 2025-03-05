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

// It's better to have a list of Portal mods instead of having a 
// single cvar for Rexaura in case we want to add more mods (Portal: Prelude, Portal: Pro, Blue Portals, etc...)
enum PortalGameType_t
{
	PORTAL_GAME_PORTAL,
	PORTAL_GAME_REXAURA,
};

extern ConVar sv_portal_game;

extern char *g_ppszPortalPassThroughMaterials[];

#define RADIO_DATA_FILE "scripts/radios.txt"
extern KeyValues *LoadRadioData();

#define INSTALL_BITS_PORTAL		(1<<0)
#define INSTALL_BITS_REXAURA	(1<<1)

#endif // PORTAL_SHAREDDEFS_H
