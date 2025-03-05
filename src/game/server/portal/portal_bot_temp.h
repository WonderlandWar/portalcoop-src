//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PORTAL_BOT_TEMP_H
#define PORTAL_BOT_TEMP_H
#ifdef _WIN32
#pragma once
#endif


// If iTeam or iClass is -1, then a team or class is randomly chosen.
CBasePlayer *BotPutInServer( bool bFrozen, int iTeam, int iClass );

void Bot_RunAll();


#endif // PORTAL_BOT_TEMP_H
