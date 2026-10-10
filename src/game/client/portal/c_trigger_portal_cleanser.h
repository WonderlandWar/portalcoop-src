//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which bumps portal placement. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//
#ifndef TRIGGER_PORTAL_CLEANSER_H
#define TRIGGER_PORTAL_CLEANSER_H

#include "cbase.h"
#include "c_triggers.h"
#include "c_portal_player.h"
#include "c_weapon_portalgun.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
//#include "portal/c_weapon_physcannon.h"
#include "model_types.h"
#include "rumble_shared.h"
#include "c_triggers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Removes anything that touches it. If the trigger has a targetname,
//			firing it will toggle state.
//-----------------------------------------------------------------------------
class C_TriggerPortalCleanser : public C_BaseTrigger
{
	DECLARE_CLASS( C_TriggerPortalCleanser, C_BaseTrigger );
	DECLARE_CLIENTCLASS();
public:

	virtual bool IsPredicted(void) { return true; }
	void Touch( C_BaseEntity *pOther );
};
#endif