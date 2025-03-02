//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PROP_PORTAL_H
#define PROP_PORTAL_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"
#include "PortalSimulation.h"

// FIX ME
#include "portal_shareddefs.h"

static const char *s_pDelayedPlacementContext = "DelayedPlacementContext";
static const char *s_pTestRestingSurfaceContext = "TestRestingSurfaceContext";
static const char *s_pFizzleThink = "FizzleThink";

class CFunc_Portalled : public CBaseEntity
{
	DECLARE_CLASS( CFunc_Portalled, CBaseEntity )
	DECLARE_DATADESC()
public:

	void OnPrePortalled( CBaseEntity *pEntity, bool m_bFireType );
	void OnPostPortalled( CBaseEntity *pEntity, bool m_bFireType );
	
private:

	bool m_bFireOnDeparture;
	bool m_bFireOnArrival;
	bool m_bFireOnPlayer;

	COutputEvent m_OnEntityPrePortalled;
	COutputEvent m_OnEntityPostPortalled;
};

//#define DONT_USE_MICROPHONESORSPEAKERS

class CPhysicsCloneArea;

class CProp_Portal : public CBaseAnimating, public CPortalSimulatorEventCallbacks
{
public:
	DECLARE_CLASS( CProp_Portal, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

							CProp_Portal( void );
	virtual					~CProp_Portal( void );

	CNetworkHandle( CProp_Portal, m_hLinkedPortal ); //the portal this portal is linked to
	

	VMatrix					m_matrixThisToLinked; //the matrix that will transform a point relative to this portal, to a point relative to the linked portal
	CNetworkVar( bool, m_bIsPortal2 ); //For teleportation, this doesn't matter, but for drawing and moving, it matters
	Vector	m_vPrevForward; //used for the indecisive push in find closest passable spaces when portal is moved

	bool	m_bSharedEnvironmentConfiguration; //this will be set by an instance of CPortal_Environment when two environments are in close proximity
#ifndef DONT_USE_MICROPHONESORSPEAKERS
	EHANDLE	m_hMicrophone; //the microphone for teleporting sound
	EHANDLE	m_hSpeaker; //the speaker for teleported sound
#endif
	CSoundPatch		*m_pAmbientSound;

	Vector		m_vAudioOrigin;
	Vector		m_vDelayedPosition;
	QAngle		m_qDelayedAngles;
	int			m_iDelayedFailure;
	CNetworkHandle(CWeaponPortalgun, m_hPlacedBy);

	COutputEvent m_OnPlacedSuccessfully;		// Output in hammer for when this portal was successfully placed (not attempted and fizzed).

	COutputEvent m_OnEntityTeleportFromMe;
	COutputEvent m_OnPlayerTeleportFromMe;
	COutputEvent m_OnEntityTeleportToMe;
	COutputEvent m_OnPlayerTeleportToMe;

	COutputEvent m_OnFizzled;
	COutputEvent m_OnStolen;

	Vector4D m_plane_Origin; //a portal plane on the entity origin
	
	CNetworkVector( m_ptOrigin );
	Vector m_vForward, m_vUp, m_vRight;
	CNetworkQAngle( m_qAbsAngle );

	CPhysicsCloneArea		*m_pAttachedCloningArea;
	
	bool	IsPortal2() const;
	void	SetIsPortal2( bool bIsPortal2 );
	const VMatrix& MatrixThisToLinked() const;

	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}


	virtual void			Precache( void );
	virtual void			CreateSounds( void );
	virtual void			StopLoopingSounds( void );
	virtual void			Spawn( void );
	virtual void			Activate( void );
	virtual void			OnRestore( void );

	virtual void			UpdateOnRemove( void );
	
	virtual bool			IsActive( void ) const { return m_bActivated; }
	virtual bool			GetOldActiveState( void ) const { return m_bOldActivatedState; }
	virtual	void			SetActive( bool bActive );
	
	bool					IsFloorPortal( float fThreshold = 0.8f ) const;
	bool					IsCeilingPortal( float fThreshold = -0.8f ) const;

	void					DelayedPlacementThink( void );
	void					TestRestingSurfaceThink ( void );
	void					FizzleThink( void );
	
	bool					IsActivedAndLinked( void ) const;

    void					WakeNearbyEntities( void ); //wakes all nearby entities in-case there's been a significant change in how they can rest near a portal

	void					ForceEntityToFitInPortalWall( CBaseEntity *pEntity ); //projects an object's center into the middle of the portal wall hall, and traces back to where it wants to be

	void					PlacePortal( const Vector &vOrigin, const QAngle &qAngles, float fPlacementSuccess, bool bDelay = false );
	void					NewLocation( const Vector &vOrigin, const QAngle &qAngles );

	virtual void			PreTeleportTouchingEntity( CBaseEntity *pOther );
	virtual void			PostTeleportTouchingEntity( CBaseEntity *pOther );

	void					ResetModel( void ); //sets the model and bounding box
	void					DoFizzleEffect( int iEffect, int iLinkageGroupID = 0, bool bDelayedPos = true ); //display cool visual effect
	void					Fizzle( void ); //go inactive
	void					PunchPenetratingPlayer( CBaseEntity *pPlayer ); // adds outward force to player intersecting the portal plane
	void					PunchAllPenetratingPlayers( void ); // adds outward force to player intersecting the portal plane

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );
	bool					ShouldTeleportTouchingEntity( CBaseEntity *pOther ); //assuming the entity is or was just touching the portal, check for teleportation conditions
	void					TeleportTouchingEntity( CBaseEntity *pOther );
	void					InputSetActivatedState( inputdata_t &inputdata );
	void					InputFizzle( inputdata_t &inputdata );
	void					InputNewLocation( inputdata_t &inputdata );

	void					CreatePortalMicAndSpeakers( void );
	void					UpdatePortalLinkage( void );
	void					UpdatePortalTeleportMatrix( void ); //computes the transformation from this portal to the linked portal, and will update the remote matrix as well

	void					SetupPortalColorSet( void );

	//void					SendInteractionMessage( CBaseEntity *pEntity, bool bEntering ); //informs clients that the entity is interacting with a portal (mostly used for clip planes)

	bool					SharedEnvironmentCheck( CBaseEntity *pEntity ); //does all testing to verify that the object is better handled with this portal instead of the other
	bool m_bFoundFilter;

	// The four corners of the portal in worldspace, updated on placement. The four points will be coplanar on the portal plane.
	Vector m_vPortalCorners[4];

	CNetworkVarEmbedded( CPortalSimulator, m_PortalSimulator );	
	
	inline Vector			GetLocalMins( void ) const { return Vector( 0.0f, -PORTAL_HALF_WIDTH, -PORTAL_HALF_HEIGHT ); }
	inline Vector			GetLocalMaxs( void ) const { return Vector( 64.0f, PORTAL_HALF_WIDTH, PORTAL_HALF_HEIGHT ); }

	void					UpdateCollisionShape( void );

	//virtual bool			CreateVPhysics( void );
	//virtual void			VPhysicsDestroyObject( void );

	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	virtual void			PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity );
	virtual void			PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity );
	
	// Add or remove listeners
	void					AddPortalEventListener( EHANDLE hListener );
	void					RemovePortalEventListener( EHANDLE hListener );
	
	void					BroadcastPortalEvent( PortalEvent_t nEventType );
	
	CUtlVector<EHANDLE>		m_PortalEventListeners;			// Collection of entities (by handle) who wish to receive notification of portal events (fizzle, moved, etc)

	CNetworkVar( unsigned char, m_iLinkageGroupID )
			
	virtual bool			IsPredicted() const { return true; }

	void	StealPortal( CProp_Portal *pHitPortal );

	//CProp_Portal			*m_pHitPortal;
	//CProp_Portal			*m_pPortalReplacingMe;

	CNetworkVar(int, m_iCustomPortalColorSet);
	int	m_iPortalColorSet;
	
	void	OnEntityTeleportedToPortal( CBaseEntity *pEntity );
	void	OnEntityTeleportedFromPortal( CBaseEntity *pEntity );

	void	OnStolen( CBaseEntity *pActivator, CBaseEntity *pCaller );

protected:
	
	CNetworkVar(bool, m_bActivated); //a portal can exist and not be active
	CNetworkVar(bool, m_bOldActivatedState); //the old state

private:

	unsigned char			m_bShouldUseLinkageID; //test if we should use linkage ID
	
	CNetworkHandle( CBaseEntity, m_hFiredByPlayer );	
	CHandle<CFunc_Portalled> m_NotifyOnPortalled; //an entity that forwards notifications of teleports to map logic entities

	CPhysCollide			*m_pCollisionShape;
	void					RemovePortalMicAndSpeaker();	// Cleans up the portal's internal audio members
	void					UpdateCorners( void );			// Updates the four corners of this portal on spawn and placement

public:
	
	inline float			GetHalfWidth( void ) const { return PORTAL_HALF_WIDTH; }
	inline float			GetHalfHeight( void ) const { return PORTAL_HALF_HEIGHT; }

	inline unsigned char	GetLinkageGroup( void ) const { return m_iLinkageGroupID; };
	void					ChangeLinkageGroup( unsigned char iLinkageGroupID );
	void SetFiredByPlayer( CBasePlayer *pPlayer );
	inline CBasePlayer *GetFiredByPlayer( void ) const { return (CBasePlayer *)m_hFiredByPlayer.Get(); }
	
	inline void SetFuncPortalled( CFunc_Portalled *pPortalledEnt = NULL ) { m_NotifyOnPortalled = pPortalledEnt; }

	//find a portal with the designated attributes, or creates one with them, favors active portals over inactive
	static CProp_Portal		*FindPortal( unsigned char iLinkageGroupID, bool bPortal2, bool bCreateIfNothingFound = false );
	static const CUtlVector<CProp_Portal *> *GetPortalLinkageGroup( unsigned char iLinkageGroupID );

	
	virtual float GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity );
	virtual float GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity );
	
	//does all the gruntwork of figuring out flooriness and calling the two above
	static void				GetExitSpeedRange( CProp_Portal *pEntrancePortal, bool bPlayer, float &fExitMinimum, float &fExitMaximum, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity );

	CNetworkVar(int, m_nPlacementAttemptParity); //Increments every time we try to move the portal in a predictable way. Will send a network packet to catch cases where placement succeeds on the client, but fails on the server.
};


//-----------------------------------------------------------------------------
// inline state querying methods
//-----------------------------------------------------------------------------
inline bool	CProp_Portal::IsPortal2() const
{
	return m_bIsPortal2;
}

inline void	CProp_Portal::SetIsPortal2( bool bIsPortal2 )
{
	m_bIsPortal2 = bIsPortal2;
}

inline const VMatrix& CProp_Portal::MatrixThisToLinked() const
{
	return m_matrixThisToLinked;
}

extern void EntityPortalled( CProp_Portal *pPortal, CBaseEntity *pOther, const Vector &vNewOrigin, const QAngle &qNewAngles, bool bForcedDuck );

#endif //#ifndef PROP_PORTAL_H
