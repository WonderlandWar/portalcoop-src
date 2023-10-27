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

//trace that has special understanding of how to handle portals
class CTrace_PlayerAABB_vs_Portals : public CGameTrace
{
public:
	CTrace_PlayerAABB_vs_Portals( void );
	bool HitPortalRamp( const Vector &vUp );

	bool m_bContactedPortalTransitionRamp;
};

class CReservePlayerSpot;

extern bool g_bAllowForcePortalTrace;
extern bool g_bForcePortalTrace;

static inline CBaseEntity *TranslateGroundEntity( CBaseEntity *pGroundEntity )
{
	CPhysicsShadowClone *pClone = dynamic_cast<CPhysicsShadowClone *>(pGroundEntity);

	if( pClone && pClone->IsUntransformedClone() )
	{
		CBaseEntity *pSource = pClone->GetClonedEntity();

		if( pSource )
			return pSource;
	}

	return pGroundEntity;
}



//-----------------------------------------------------------------------------
// Purpose: Portal specific movement code
//-----------------------------------------------------------------------------
class CPortalGameMovement : public CHL2GameMovement
{
	typedef CGameMovement BaseClass;
public:

	CPortalGameMovement();

	bool	m_bInPortalEnv;
// Overrides
	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual bool CheckJumpButton( void );

	void FunnelIntoPortal( CProp_Portal *pPortal, Vector &wishdir );

	virtual void AirAccelerate( Vector& wishdir, float wishspeed, float accel );
	virtual void AirMove( void );

	virtual void PlayerRoughLandingEffects( float fvol );

	virtual void CategorizePosition( void );

	// Traces the player bbox as it is swept from start to end
	virtual void TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, CTrace_PlayerAABB_vs_Portals& pm );
	virtual void TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );

	// Tests the player position
	virtual CBaseHandle	TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm );

	virtual void Duck( void );				// Check for a forced duck

	virtual int CheckStuck( void );

	virtual void SetGroundEntity( trace_t *pm );
	
#if defined( CLIENT_DLL )
	void ClientVerticalElevatorFixes( CBasePlayer *pPlayer, CMoveData *pMove );
#endif

#if USEMOVEMENTFORPORTALLING
	void HandlePortalling( void );

	Vector m_vMoveStartPosition; //where the player started before the movement code ran

#endif

private:


	CPortal_Player	*GetPortalPlayer();
};