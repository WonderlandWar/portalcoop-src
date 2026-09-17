//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Special handling for Portal usable ladders
//
//=============================================================================//
#include "cbase.h"
#include "in_buttons.h"
#include "utlrbtree.h"
#include "movevars_shared.h"
#include "portal_shareddefs.h"
#include "portal_collideable_enumerator.h"
#include "prop_portal_shared.h"
#include "rumble_shared.h"
#include "portal_player_shared.h"
#include "hl_gamemovement.h"

#ifdef WIN32
#pragma once
#endif

// This would be great to have. It solves the problem of player pushing fighting the portal funnel.
// It also reduces prediction errors, but unfortunately certain behaviors can't be translated to cmd code.
//#define USE_CMD_FOR_PORTAL_FUNNEL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CReservePlayerSpot;

extern bool g_bAllowForcePortalTrace;
extern bool g_bForcePortalTrace;