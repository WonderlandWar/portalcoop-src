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


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CReservePlayerSpot;

extern bool g_bAllowForcePortalTrace;
extern bool g_bForcePortalTrace;