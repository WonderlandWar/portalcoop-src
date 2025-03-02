//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "portal_shareddefs.h"
#include "filesystem.h"

#ifdef GAME_DLL
ConVar sv_portal_game_update_on_map_load( "sv_portal_game_update_on_map_load", "1", FCVAR_REPLICATED, "Updates the server's portal game type\n" );
#endif
ConVar sv_portal_game( "sv_portal_game", "0", FCVAR_REPLICATED, "The server's portal game type, automatically changes on map load if sv_portal_game_update_on_map_load is enabled\n0 = Portal\n1 = Rexaura\n" );

char *g_ppszPortalPassThroughMaterials[] = 
{ 
	"lights/light_orange001", 
	NULL,
};

KeyValues *LoadRadioData()
{	
	KeyValues *radios = new KeyValues( "radios.txt" );
	if ( !radios->LoadFromFile( g_pFullFileSystem, RADIO_DATA_FILE, "MOD" ) )
	{
		AssertMsg( false, "Failed to load radio data" );
		return NULL;
	}

	return radios;
}