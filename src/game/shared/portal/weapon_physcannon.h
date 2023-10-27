//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_PHYSCANNON_H
#define WEAPON_PHYSCANNON_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "prop_portal_shared.h"
#include "portal_util_shared.h"
#include "gamerules.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "physics.h"
#include "in_buttons.h"
#include "IEffects.h"
#include "debugoverlay_shared.h"
#include "shake.h"
#include "beam_shared.h"
#include "Sprite.h"
#include "physics_saverestore.h"
#include "movevars_shared.h"
#include "weapon_portalbasecombatweapon.h"
#include "vphysics/friction.h"
#include "saverestore_utlvector.h"
#include "portal_gamerules.h"
#include "citadel_effects_shared.h"
#include "model_types.h"
#include "rumble_shared.h"
#include "gamestats.h"
#include "player_pickup.h"

#ifdef GAME_DLL
#include "player.h"
#include "weapon_portalgun.h"
#include "soundent.h"
#include "portal_player.h"
#include "hl2_player.h"
#include "util.h"
#include "ai_basenpc.h"
#include "physics_prop_ragdoll.h"
#include "globalstate.h"
#include "props.h"
#include "te_effect_dispatch.h"
#include "prop_combine_ball.h"
#include "physobj.h"
#include "eventqueue.h"
#include "ai_interactions.h"
#else
#include "c_baseplayer.h"
#include "vcollide_parse.h"
#include "c_weapon_portalgun.h"
#include "c_portal_player.h"
#include "cdll_util.h"
#include "c_ai_basenpc.h"
#include "c_props.h"
#include "c_te_effect_dispatch.h"
#include "c_prop_combine_ball.h"
#include "fx.h"
#include "particles_localspace.h"
#include "view.h"
#include "particles_attractor.h"
#endif

#include "networkvar.h"

#ifdef CLIENT_DLL
#define CWeaponPhysCannon C_WeaponPhysCannon
#endif

#ifdef CLIENT_DLL
//#define CLIENTSHOULDNOTSEEPHYSCANNON
//#define CLIENTSHOULDNOTSEEGRABCONTROLLER
#else
#define SENDPHYSCANNONINFOTOCLIENT
//#define SENDGRABCONTROLLERINFOTOCLIENT
#endif

//-----------------------------------------------------------------------------
// Do we have the super-phys gun?
//-----------------------------------------------------------------------------
extern bool PlayerHasMegaPhysCannon();

//class CGrabController;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// derive from this so we can add save/load data to it
struct game_shadowcontrol_params_t : public hlshadowcontrol_params_t
{
	DECLARE_SIMPLE_DATADESC();
};


//-----------------------------------------------------------------------------
// Physcannon
//-----------------------------------------------------------------------------


#define	NUM_BEAMS	4
#define	NUM_SPRITES	6

#ifdef CLIENT_DLL
#define CGrabController C_GrabController
#endif

class CGrabController : public IMotionEvent
{
public:
	DECLARE_CLASS_NOBASE( CGrabController );
	DECLARE_NETWORKCLASS_NOBASE();
	DECLARE_SIMPLE_DATADESC();

	CGrabController( void );
	~CGrabController( void );
	void AttachEntity( CPortal_Player *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition );
	void DetachEntity( bool bClearVelocity );
	void OnRestore();

	bool UpdateObject( CPortal_Player *pPlayer, float flError );

	void SetTargetPosition( const Vector &target, const QAngle &targetOrientation );
	void GetTargetPosition( Vector *target, QAngle *targetOrientation );
	float ComputeError();
	float GetLoadWeight( void ) const { return m_flLoadWeight; }
	void SetAngleAlignment( float alignAngleCosine ) { m_angleAlignment = alignAngleCosine; }
	void SetIgnorePitch( bool bIgnore ) { m_bIgnoreRelativePitch = bIgnore; }
	QAngle TransformAnglesToPlayerSpace( const QAngle &anglesIn, CPortal_Player *pPlayer );
	QAngle TransformAnglesFromPlayerSpace( const QAngle &anglesIn, CPortal_Player *pPlayer );

	CBaseEntity *GetAttached() { return m_attachedEntity.Get(); }

	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	float GetSavedMass( IPhysicsObject *pObject );

	bool IsObjectAllowedOverhead( CBaseEntity *pEntity );
		
	//set when a held entity is penetrating another through a portal. Needed for special fixes
	void SetPortalPenetratingEntity( CBaseEntity *pPenetrated );

	QAngle			m_attachedAnglesPlayerSpace;
	Vector			m_attachedPositionObjectSpace;

private:
	// Compute the max speed for an attached object
#ifdef GAME_DLL
	void ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics );
#endif
	game_shadowcontrol_params_t	m_shadow;
	float			m_timeToArrive;
	float			m_errorTime;
	float			m_error;
	float			m_contactAmount;
	float			m_angleAlignment;
	bool			m_bCarriedEntityBlocksLOS;

	float			m_flLoadWeight;
	float			m_savedRotDamping[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	float			m_savedMass[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	//EHANDLE			m_attachedEntity;
	EHANDLE	m_attachedEntity;
	EHANDLE m_OldattachedEntity;
#ifdef GAME_DLL
//	CNetworkQAngle ( m_vecPreferredCarryAngles );
//	CNetworkVar(bool, m_bHasPreferredCarryAngles);
//	CNetworkVar(bool, m_bIgnoreRelativePitch);
#endif
	QAngle			m_vecPreferredCarryAngles;
	bool			m_bHasPreferredCarryAngles;
	bool			m_bIgnoreRelativePitch;

	float			m_flDistanceOffset;


	IPhysicsMotionController *m_controller;

	// NVNT player controlling this grab controller
	CPortal_Player*	m_pControllingPlayer;

	bool			m_bAllowObjectOverhead; // Can the player hold this object directly overhead? (Default is NO)

	//set when a held entity is penetrating another through a portal. Needed for special fixes
	EHANDLE			m_PenetratedEntity;

	friend class CWeaponPhysCannon;
	friend void GetSavedParamsForCarriedPhysObject( CGrabController *pGrabController, IPhysicsObject *pObject, float *pSavedMassOut, float *pSavedRotationalDampingOut );
};




struct thrown_objects_t
{
	float				fTimeThrown;
	EHANDLE				hEntity;

	DECLARE_SIMPLE_DATADESC();
};


class CWeaponPhysCannon : public CBasePortalCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponPhysCannon, CBasePortalCombatWeapon );

	DECLARE_NETWORKCLASS();

	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeaponPhysCannon( void );

	void	Drop( const Vector &vecVelocity );
	void	Precache();
	virtual void	Spawn();
	virtual void	OnRestore();
	virtual void	StopLoopingSounds();
	virtual void	UpdateOnRemove(void);
	void	PrimaryAttack();
	void	SecondaryAttack();
	void	WeaponIdle();
	void	ItemPreFrame();
	void	ItemPostFrame();
	void	ItemBusyFrame();

	virtual float GetMaxAutoAimDeflection() { return 0.90f; }

	void	ForceDrop( void );
	bool	DropIfEntityHeld( CBaseEntity *pTarget );	// Drops its held entity if it matches the entity passed in

	CGrabController &GetGrabController() { return m_grabController; }

	bool	CanHolster( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	bool	Deploy( void );

	bool	HasAnyAmmo( void ) { return true; }

	void	InputBecomeMegaCannon( inputdata_t &inputdata );

	void	BeginUpgrade();

	virtual void SetViewModel( void );
	virtual const char *GetShootSound( int iIndex ) const;
	
#ifndef CLIENT_DLL
	CNetworkQAngle	( m_attachedAnglesPlayerSpace );
#else
	QAngle m_attachedAnglesPlayerSpace;
#endif

	CNetworkVector	( m_attachedPositionObjectSpace );

	CNetworkHandle( CBaseEntity, m_hAttachedObject );

	void	RecordThrownObject( CBaseEntity *pObject );
	void	PurgeThrownObjects();
	bool	IsAccountableForObject( CBaseEntity *pObject );
	
	bool	ShouldDisplayHUDHint() { return true; }

	EHANDLE m_hOldAttachedObject;

protected:
	enum FindObjectResult_t
	{
		OBJECT_FOUND = 0,
		OBJECT_NOT_FOUND,
		OBJECT_BEING_DETACHED,
	};

	void	DoMegaEffect( int effectType, Vector *pos = NULL );
	void	DoEffect( int effectType, Vector *pos = NULL );

	void	OpenElements( void );
	void	CloseElements( void );

	// Pickup and throw objects.
	bool	CanPickupObject( CBaseEntity *pTarget );
	void	CheckForTarget( void );

	FindObjectResult_t		FindObject( void );
	void					FindObjectTrace( CBasePlayer *pPlayer, trace_t *pTraceResult );
#ifdef GAME_DLL
	CBaseEntity *MegaPhysCannonFindObjectInCone( const Vector &vecOrigin, const Vector &vecDir, float flCone, float flCombineBallCone, bool bOnlyCombineBalls );
	CBaseEntity *FindObjectInCone( const Vector &vecOrigin, const Vector &vecDir, float flCone );
#endif
	bool	AttachObject( CBaseEntity *pObject, const Vector &vPosition );
	void	UpdateObject( void );
	void	DetachObject( bool playSound = true, bool wasLaunched = false );
	void	LaunchObject( const Vector &vecDir, float flForce );
	void	StartEffects( void );	// Initialize all sprites and beams
	void	StopEffects( bool stopSound = true );	// Hide all effects temporarily
	void	DestroyEffects( void );	// Destroy all sprites and beams

	// Punt objects - this is pointing at an object in the world and applying a force to it.
	void	PuntNonVPhysics( CBaseEntity *pEntity, const Vector &forward, trace_t &tr );
	void	PuntVPhysics( CBaseEntity *pEntity, const Vector &forward, trace_t &tr );
#ifdef GAME_DLL
	void	PuntRagdoll( CBaseEntity *pEntity, const Vector &forward, trace_t &tr );
#endif
	// Velocity-based throw common to punt and launch code.
#ifdef GAME_DLL
	void	ApplyVelocityBasedForce( CBaseEntity *pEntity, const Vector &forward, const Vector &vecHitPos, PhysGunForce_t reason );
#endif
	// Physgun effects
	void	DoEffectClosed( void );
	void	DoMegaEffectClosed( void );
	
	void	DoEffectReady( void );
	void	DoMegaEffectReady( void );

	void	DoMegaEffectHolding( void );
	void	DoEffectHolding( void );

	void	DoMegaEffectLaunch( Vector *pos );
	void	DoEffectLaunch( Vector *pos );

	void	DoEffectNone( void );
	void	DoEffectIdle( void );

	// Trace length
	float	TraceLength();

	// Do we have the super-phys gun?
	inline bool	IsMegaPhysCannon()
	{
		return PlayerHasMegaPhysCannon();
	}

	// Sprite scale factor 
	float	SpriteScaleFactor();

	float			GetLoadPercentage();
	CSoundPatch		*GetMotorSound( void );

	void	DryFire( void );
	void	PrimaryFireEffect( void );

	// What happens when the physgun picks up something 
	void	Physgun_OnPhysGunPickup( CBaseEntity *pEntity, CBasePlayer *pOwner, PhysGunPickup_t reason );

#ifdef CLIENT_DLL
	
	CInterpolatedValue		m_ElementParameter;							// Used to interpolate the position of the articulated elements
	
	virtual void	ClientThink( void );
	virtual void	OnDataChanged( DataUpdateType_t type );
	void			ManagePredictedObject( void );
	void			UpdateElementPosition( void );

	int				m_nOldEffectState;	// Used for parity checks
	bool			m_bOldOpen;			// Used for parity checks

#endif

	// Wait until we're done upgrading
	void	WaitForUpgradeThink();

	bool	EntityAllowsPunts( CBaseEntity *pEntity );
	CNetworkVar(bool, m_bActive);
	CNetworkVar(bool, m_bCanHolster);
	CNetworkVar(bool, m_bAllowsPunts);
	CNetworkVar(bool, m_bOpen);
	
	bool	m_bResetOwnerEntity;

	CNetworkVar(bool, m_bCanPickupObject);
	CNetworkVar(int, m_EffectState);		// Current state of the effects on the gun

	int		m_nChangeState;			//For delayed state change of elements
	float	m_flCheckSuppressTime;	//Amount of time to suppress the checking for targets
	bool	m_flLastDenySoundPlayed;	//Debounce for deny sound
	int		m_nAttack2Debounce;

	CNetworkVar( bool, m_bIsCurrentlyUpgrading );

	float	m_flElementDebounce;
	float	m_flElementPosition;
	float	m_flElementDestination;
#ifdef GAME_DLL
	CHandle<CBeam>		m_hBeams[NUM_BEAMS];
	CHandle<CSprite>	m_hGlowSprites[NUM_SPRITES];
	CHandle<CSprite>	m_hEndSprites[2];
	float				m_flEndSpritesOverride[2];
	CHandle<CSprite>	m_hCenterSprite;
	CHandle<CSprite>	m_hBlastSprite;
#endif
	CSoundPatch			*m_sndMotor;		// Whirring sound for the gun

	CGrabController		m_grabController;
	
	bool				m_bPhyscannonState;

	// A list of the objects thrown or punted recently, and the time done so.
	CUtlVector< thrown_objects_t >	m_ThrownEntities;

	float				m_flTimeNextObjectPurge;

#ifdef CLIENT_DLL
public:

	virtual int DrawModel(int flags);

private:

	bool	SetupEmitter(void);

	bool	m_bWasUpgraded;

	CSmartPtr<CLocalSpaceEmitter>	m_pLocalEmitter;
	CSmartPtr<CSimpleEmitter>		m_pEmitter;
	CSmartPtr<CParticleAttractor>	m_pAttractor;
#endif

};



// force the physcannon to drop an object (if carried)
void PhysCannonForceDrop( CBaseCombatWeapon *pActiveWeapon, CBaseEntity *pOnlyIfHoldingThis );
void PhysCannonBeginUpgrade( CBaseAnimating *pAnim );

bool PlayerPickupControllerIsHoldingEntity( CBaseEntity *pPickupController, CBaseEntity *pHeldEntity );
void ShutdownPickupController( CBaseEntity *pPickupControllerEntity );
float PlayerPickupGetHeldObjectMass( CBaseEntity *pPickupControllerEntity, IPhysicsObject *pHeldObject );

float PhysCannonGetHeldObjectMass( CBaseCombatWeapon *pActiveWeapon, IPhysicsObject *pHeldObject );
CBaseEntity *PhysCannonGetHeldEntity( CBaseCombatWeapon *pActiveWeapon );
CBasePlayer *GetPlayerHoldingEntity(CBaseEntity *pEntity);

CBaseEntity *GetPlayerHeldEntity(CBasePlayer *pPlayer);

CGrabController *GetGrabControllerForPlayer( CBasePlayer *pPlayer );
CGrabController *GetGrabControllerForPhysCannon( CBaseCombatWeapon *pActiveWeapon );
void GetSavedParamsForCarriedPhysObject( CGrabController *pGrabController, IPhysicsObject *pObject, float *pSavedMassOut, float *pSavedRotationalDampingOut );
void UpdateGrabControllerTargetPosition( CPortal_Player *pPlayer, Vector *vPosition, QAngle *qAngles );

bool PhysCannonAccountableForObject( CBaseCombatWeapon *pPhysCannon, CBaseEntity *pObject );

void GrabController_SetPortalPenetratingEntity( CGrabController *pController, CBaseEntity *pPenetrated );

#endif // WEAPON_PHYSCANNON_H
