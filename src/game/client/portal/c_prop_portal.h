//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PROP_PORTAL_H
#define C_PROP_PORTAL_H

#ifdef _WIN32
#pragma once
#endif

#include "portalrenderable_flatbasic.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "viewrender.h"
#include "PortalSimulation.h"
#include "C_PortalGhostRenderable.h" 
#include "PhysicsCloneArea.h"

// FIX ME
#include "portal_shareddefs.h"

static const char *s_pDelayedPlacementContext = "DelayedPlacementContext";
static const char *s_pTestRestingSurfaceContext = "TestRestingSurfaceContext";
static const char *s_pFizzleThink = "FizzleThink";

struct dlight_t;
class C_DynamicLight;
class C_PhysicsCloneArea;

class C_Prop_Portal : public CPortalRenderable_FlatBasic
{
public:
	DECLARE_CLASS( C_Prop_Portal, CPortalRenderable_FlatBasic );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

							C_Prop_Portal( void );
	virtual					~C_Prop_Portal( void );

	// Handle recording for the SFM
	virtual void GetToolRecordingState( KeyValues *msg );
	
	virtual bool ShouldPredict( void );
	virtual C_BasePlayer *GetPredictionOwner( void );

	CHandle<C_Prop_Portal>	m_hLinkedPortal; //the portal this portal is linked to
	CHandle<C_Prop_Portal>	GetLinkedPortal() { return m_hLinkedPortal; } //the portal this portal is linked to

	bool					m_bSharedEnvironmentConfiguration; //this will be set by an instance of CPortal_Environment when two environments are in close proximity
	
	cplane_t				m_plane_Origin;	// The plane on which this portal is placed, normal facing outward (matching model forward vec)

	virtual void			Spawn( void );
	virtual void			Precache( void );
	virtual void			Activate( void );
	virtual void			ClientThink( void );
	virtual void			UpdateOnRemove( void );
	virtual void			OnRestore( void );
	

	virtual void			Simulate();

	
	virtual bool			IsActive( void ) const { return m_bActivated; }
	virtual bool			GetOldActiveState( void ) const { return m_bOldActivatedState; }
	virtual	void			SetActive( bool bActive );
	
	bool					IsFloorPortal( float fThreshold = 0.8f ) const;
	bool					IsCeilingPortal( float fThreshold = -0.8f ) const;

	virtual void			OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	
	void					CreateAttachedParticles( void );
	void					DestroyAttachedParticles( void );
	
	void					DoFizzleEffect( int iEffect, int iLinkageGroupID = 0, bool bDelayedPos = true ); //display cool visual effect
	void					Fizzle( void ); //display cool visual effect

	void					PlacePortal( const Vector &vOrigin, const QAngle &qAngles, float fPlacementSuccess, bool bDelay = false );
	void SetFiredByPlayer( CBasePlayer *pPlayer );
	inline CBasePlayer		*GetFiredByPlayer( void ) const { return (CBasePlayer *)m_hFiredByPlayer.Get(); }
	void					DelayedPlacementThink( void );
	void					NewLocation( const Vector &vOrigin, const QAngle &qAngles );
	
	virtual void			OnPortalMoved( void ); //this portal has moved
	virtual void			OnActiveStateChanged( void ); //this portal's active status has changed
	virtual void			OnLinkageChanged( C_Prop_Portal *pOldLinkage );

	virtual void			StartTouch(C_BaseEntity *pOther);
	virtual void			Touch(C_BaseEntity  *pOther);
	virtual void			EndTouch(C_BaseEntity  *pOther);

	bool					ShouldTeleportTouchingEntity(C_BaseEntity *pOther); //assuming the entity is or was just touching the portal, check for teleportation conditions
	void					TeleportTouchingEntity(C_BaseEntity *pOther);
	virtual void			UpdatePartitionListEntry();
	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	bool					SharedEnvironmentCheck(C_BaseEntity *pEntity); //does all testing to verify that the object is better handled with this portal instead of the other
	bool					ShouldCreateAttachedParticles();
	virtual bool			IsPredicted() const { return true; }

	virtual void			PortalSimulator_TookOwnershipOfEntity(C_BaseEntity *pEntity);
	virtual void			PortalSimulator_ReleasedOwnershipOfEntity(C_BaseEntity *pEntity);

	struct Portal_PreDataChanged
	{
		bool					m_bActivated;
		bool					m_bOldActivatedState;
		bool					m_bIsPortal2;
		Vector					m_vOrigin;
		QAngle					m_qAngles;
		CHandle<C_Prop_Portal> m_hLinkedTo;
	} PreDataChanged;

	struct TransformedLightingData_t
	{
		ClientShadowHandle_t	m_LightShadowHandle;
		dlight_t				*m_pEntityLight;
	} TransformedLighting;
	
	EHANDLE		m_hFiredByPlayer; //needed for multiplayer portal coloration

	CUtlReference<CNewParticleEffect>	m_hEdgeEffect;	// the particles effect that attaches to an active portal
	CUtlReference<CNewParticleEffect>	m_hParticleEffect;	// the particles effect that attaches to an active portal
	
	Vector		m_vDelayedPosition;
	QAngle		m_qDelayedAngles;
	int			m_iDelayedFailure;
	EHANDLE		m_hPlacedBy;

	virtual void			OnPreDataChanged( DataUpdateType_t updateType );
	void					HandleNetworkChanges( void );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual int				DrawModel( int flags );
	void					UpdateOriginPlane( void );
	void					UpdateGhostRenderables( void );
	
	C_PhysicsCloneArea		*m_pAttachedCloningArea;

	void					SetIsPortal2( bool bValue );

	bool					IsActivedAndLinked( void ) const;

	CPortalSimulator		m_PortalSimulator;

	virtual C_BaseEntity *	PortalRenderable_GetPairedEntity( void ) { return this; };
	
	int m_nPlacementAttemptParity;
	
	unsigned char			m_iLinkageGroupID; //a group ID specifying which portals this one can possibly link to
	
	inline unsigned char	GetLinkageGroup( void ) const { return m_iLinkageGroupID; };
	
	virtual void HandlePredictionError( bool bErrorInThisEntity );

	//Portal Render functions
	
	// return value indicates that something was done, and render lists should be rebuilt afterwards
	//bool DrawPortalsUsingStencils( CViewRender *pViewRender, int iLinkageGroupID ); 

	C_Prop_Portal			*m_pHitPortal;
	C_Prop_Portal			*m_pAttackingPortal;

private:


	CUtlVector<EHANDLE>		m_hGhostingEntities;
	CUtlVector<C_PortalGhostRenderable *>		m_GhostRenderables;
	float					m_fGhostRenderablesClip[4];
	friend class CPortalRenderable;


	friend void __MsgFunc_EntityPortalled(bf_read &msg);
protected:
	bool					m_bActivated; //a portal can exist and not be active
	bool					m_bOldActivatedState; //state the portal was in before it was created this instance

	CPhysCollide			*m_pCollisionShape;
};

typedef C_Prop_Portal CProp_Portal;

#endif //#ifndef C_PROP_PORTAL_H