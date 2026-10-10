//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which bumps portal placement. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "triggers.h"
#include "portal_player.h"
#include "weapon_portalgun.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "physobj.h"
#include "portal/weapon_physcannon.h"
#include "model_types.h"
#include "rumble_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Removes anything that touches it. If the trigger has a targetname,
//			firing it will toggle state.
//-----------------------------------------------------------------------------
class CTriggerPortalCleanser : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerPortalCleanser, CBaseTrigger );
	DECLARE_SERVERCLASS();

public:

	CTriggerPortalCleanser();
	
	virtual bool IsPredicted(void) { return true; }
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	//CNetworkVar(bool, m_bDisabled);

	static void FizzleBaseAnimating( CBaseEntity *pOther, CTriggerPortalCleanser *pTrigger );

	DECLARE_DATADESC();

private:
	bool m_bSuppressDissolveOutput;

	// Outputs
	COutputEvent m_OnDissolve;
	COutputEvent m_OnFizzle;
	COutputEvent m_OnDissolveBox;
	COutputEvent m_OnDissolveSphere;
	
	virtual int UpdateTransmitState( void )	
	{
		return SetTransmitState( FL_EDICT_PVSCHECK );
	}
};