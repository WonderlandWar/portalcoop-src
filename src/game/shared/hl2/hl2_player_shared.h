//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HL2_PLAYER_SHARED_H
#define HL2_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// Shared header file for players
#if defined( CLIENT_DLL )
#define CHL2_Player C_BaseHLPlayer	//FIXME: Lovely naming job between server and client here...
#include "c_basehlplayer.h"
#else
#include "hl2_player.h"
#endif

#ifdef CLIENT_DLL

ConVar hl2_walkspeed("hl2_walkspeed", "150", FCVAR_REPLICATED);
ConVar hl2_normspeed("hl2_normspeed", "190", FCVAR_REPLICATED);
ConVar hl2_sprintspeed("hl2_sprintspeed", "320", FCVAR_REPLICATED);

#endif

#endif // HL2_PLAYER_SHARED_H
