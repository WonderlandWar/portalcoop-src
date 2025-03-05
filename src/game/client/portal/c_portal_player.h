//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef PORTAL_PLAYER_H
#define PORTAL_PLAYER_H
#pragma once

#include "portal_playeranimstate.h"
#include "c_basehlplayer.h"
#include "portal_player_shared.h"
#include "c_prop_portal.h"
#include "weapon_portalbase.h"
#include "c_func_liquidportal.h"
#include "colorcorrectionmgr.h"
#include "in_buttons.h"

class C_EntityPortalledNetworkMessage : public CMemZeroOnNew
{
public:	
	DECLARE_CLASS_NOBASE( C_EntityPortalledNetworkMessage );

	CHandle<C_BaseEntity> m_hEntity;
	CHandle<C_Prop_Portal> m_hPortal;
	float m_fTime;
	bool m_bForcedDuck;
	uint32 m_iMessageCount;
};

//=============================================================================
// >> Portal_Player
//=============================================================================
class C_Portal_Player : public C_BaseHLPlayer
{
public:
	DECLARE_CLASS( C_Portal_Player, C_BaseHLPlayer );

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();


	C_Portal_Player();
	~C_Portal_Player( void );

	virtual void Spawn( void );

	virtual void CreateSounds( void );
	virtual void StopLoopingSounds( void );
	void ClientThink( void );
	void FixTeleportationRoll( void );
	void PostThink( void );
	
	static inline C_Portal_Player* GetLocalPortalPlayer()
	{
		return (C_Portal_Player*)C_BasePlayer::GetLocalPlayer();
	}

	static inline C_Portal_Player* GetLocalPlayer()
	{
		return (C_Portal_Player*)C_BasePlayer::GetLocalPlayer();
	}

	virtual const QAngle& GetRenderAngles();

	virtual void UpdateClientSideAnimation();
	void DoAnimationEvent( PlayerAnimEvent_t event, int nData );

	virtual int DrawModel( int flags );
	virtual void Simulate();
	
	virtual bool CreateMove( float flInputSampleTime, CUserCmd *pCmd );

	void SetupMove( CUserCmd *ucmd, IMoveHelper *pHelper );

	virtual void AddEntity( void );

	//virtual bool IsPredicted() { return true; }
	//virtual bool ShouldPredict() { return true; }

	QAngle GetAnimEyeAngles( void ) { return m_angEyeAngles; }
	void SetAnimEyeAngles( QAngle angEyeAngles ) { m_angEyeAngles = angEyeAngles; }
	Vector GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );

	// Used by prediction, sets the view angles for the player
	virtual void SetLocalViewAngles( const QAngle &viewAngles );
	virtual void SetViewAngles( const QAngle &ang );

	// Should this object cast shadows?
	virtual ShadowType_t	ShadowCastType( void );
	virtual C_BaseAnimating* BecomeRagdollOnClient();
	virtual bool			ShouldDraw( void );
	virtual const QAngle&	EyeAngles();
	virtual void			OnPreDataChanged( DataUpdateType_t type );
	virtual void			PreDataUpdate( DataUpdateType_t updateType );
	virtual void			OnDataChanged( DataUpdateType_t type );
	virtual float			GetFOV( void );
	virtual CStudioHdr*		OnNewModel( void );
	virtual void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void			ItemPreFrame( void );
	virtual void			ItemPostFrame( void );
	virtual float			GetMinFOV()	const { return 5.0f; }
	virtual Vector			GetAutoaimVector( float flDelta );
	virtual bool			ShouldReceiveProjectedTextures( int flags );
	virtual void			PostDataUpdate( DataUpdateType_t updateType );
	virtual void			GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void			PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void			PreThink( void );
	virtual void			DoImpactEffect( trace_t &tr, int nDamageType );

	virtual Vector			EyePosition();
	Vector					EyeFootPosition( const QAngle &qEyeAngles );//interpolates between eyes and feet based on view angle roll
	inline Vector			EyeFootPosition( void ) { return EyeFootPosition( EyeAngles() ); };
	void					PlayerPortalled( C_Prop_Portal *pEnteredPortal, float fTime, bool bForcedDuck );
	void					CheckPlayerAboutToTouchPortal( void );

	virtual void	CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );
	void			CalcPortalView( Vector &eyeOrigin, QAngle &eyeAngles, float &fov );
	virtual void	CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles);
	virtual void	ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldindThis );

	void			EnableSprint( bool bEnable );


	
	void UpdatePortalPlaneSounds( void );
	void UpdateWooshSounds( void );
	
	bool UseFoundEntity( CBaseEntity *pUseEntity );
	CBaseEntity*	FindUseEntity( void );
	CBaseEntity*	FindUseEntityThroughPortal( void );
	
	virtual bool PlayerUse( void );

	inline bool		IsCloseToPortal( void ) //it's usually a good idea to turn on draw hacks when this is true
	{
		return ((PortalEyeInterpolation.m_bEyePositionIsInterpolating) || (m_hPortalEnvironment.Get() != NULL));	
	} 

	bool	CanSprint( void );
	void	StartSprinting( void );
	void	StopSprinting( void );
	void	HandleSpeedChanges( void );
	void	UpdateLookAt( void );
	void	Initialize( void );
	CBaseEntity *GetTargetIDEnt() const;
	void	UpdateIDTarget( void );
	bool	ShouldCollide( int collisionGroup, int contentsMask ) const;
	void	AvoidPlayers( CUserCmd *pCmd );
	
	bool m_bEnableSeparation;		// Keeps separation forces on when player stops moving, but still penetrating
	Vector m_vSeparationVelocity;	// Velocity used to keep player seperate from teammates

	// Separation force
	bool	IsSeparationEnabled( void ) const	{ return m_bEnableSeparation; }
	void	SetSeparation( bool bEnable )		{ m_bEnableSeparation = bEnable; }
	const Vector &GetSeparationVelocity( void ) const { return m_vSeparationVelocity; }
	void	SetSeparationVelocity( const Vector &vSeparationVelocity ) { m_vSeparationVelocity = vSeparationVelocity; }

	bool m_bHasSprintDevice;
	bool m_bSprintEnabled;
	
	
	struct PredictedPortalTeleportation_t 
	{
		float flTime;
		C_Prop_Portal *pEnteredPortal;
		int iCommandNumber;
		float fDeleteServerTimeStamp;
		bool bDuckForced;
		VMatrix matUnroll; //sometimes the portals move/fizzle between an apply and an unroll. Store the undo matrix ahead of time
	};
	CUtlVector<PredictedPortalTeleportation_t> m_PredictedPortalTeleportations;
	
	float GetImplicitVerticalStepSpeed() const;
	void SetImplicitVerticalStepSpeed( float speed );
	
	void ForceDuckThisFrame( Vector origin, Vector velocity );
	void UnDuck ( void );
	inline void ForceJumpThisFrame( void ) { ForceButtons( IN_JUMP ); }
	
	virtual void					GetToolRecordingState( KeyValues *msg );

	// physics interactions
	bool m_bSilentDropAndPickup;
	void PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize );


	void ToggleHeldObjectOnOppositeSideOfPortal( void ) { m_bHeldObjectOnOppositeSideOfPortal = !m_bHeldObjectOnOppositeSideOfPortal; }
	void SetHeldObjectOnOppositeSideOfPortal( bool p_bHeldObjectOnOppositeSideOfPortal ) { m_bHeldObjectOnOppositeSideOfPortal = p_bHeldObjectOnOppositeSideOfPortal; }
	bool IsHeldObjectOnOppositeSideOfPortal( void ) { return m_bHeldObjectOnOppositeSideOfPortal; }
	CProp_Portal *GetHeldObjectPortal( void ) { return m_hHeldObjectPortal; }
	void SetHeldObjectPortal( CProp_Portal *pPortal )
	{
		// What is calling this function?
	//	Assert(!pPortal);
		m_hHeldObjectPortal = pPortal; 
	}
	
	IPhysicsObject *GetHeldPhysicsPortal(void) { return m_pHeldPhysicsPortal; }
	void SetHeldPhysicsPortal( IPhysicsObject *pPhys ) { m_pHeldPhysicsPortal = pPhys; }
	
#if USEMOVEMENTFORPORTALLING

	virtual void ApplyTransformToInterpolators( const VMatrix &matTransform, float fUpToTime, bool bIsRevertingPreviousTransform, bool bDuckForced );
	
	//single player doesn't predict portal teleportations. This is the call you'll receive when we determine the server portalled us.
	virtual void ApplyUnpredictedPortalTeleportation( const C_Prop_Portal *pEnteredPortal, float flTeleportationTime, bool bForcedDuck );

	//WARNING: predicted teleportations WILL need to be undone A LOT. Prediction rolls time forward and backward like mad. Optimally an apply then undo should revert to the starting state. But an easier and somewhat acceptable solution is to have an undo then (assumed) re-apply be a NOP.
	virtual void ApplyPredictedPortalTeleportation( C_Prop_Portal *pEnteredPortal, CMoveData *pMove, bool bForcedDuck );
	virtual void UndoPredictedPortalTeleportation( const C_Prop_Portal *pEnteredPortal, float fOriginallyAppliedTime, const VMatrix &matUndo, bool bDuckForced ); //fOriginallyAppliedTime is the value of gpGlobals->curtime when ApplyPredictedPortalTeleportation was called. Which will be in the future when this gets called

	void UnrollPredictedTeleportations( int iCommandNumber ); //unroll all predicted teleportations at or after the target tick

#endif

	Activity TranslateActivity( Activity baseAct, bool *pRequired = NULL );
	CWeaponPortalBase* GetActivePortalWeapon() const;

	bool IsSuppressingCrosshair( void ) { return m_bSuppressingCrosshair; }
	
	int m_iCustomPortalColorSet;
	
	float GetLatestServerTeleport() { return m_fLatestServerTeleport; }

	CSoundPatch		*m_pWooshSound;
	bool	m_bIntersectingPortalPlane;

	C_Prop_Portal *m_pPortalEnvironments;
	C_Prop_Portal *m_pTransformPortal;

	VMatrix m_PendingPortalMatrix;

protected:

	virtual void	FireGameEvent( IGameEvent *event );


private:
	
	bool m_bIsListenServerHost;
	
	C_Portal_Player( const C_Portal_Player & );

	void UpdatePortalEyeInterpolation( void );
	
	CPortalPlayerAnimState *m_PlayerAnimState;

	QAngle	m_angEyeAngles;
	CDiscontinuousInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	CDiscontinuousInterpolatedVar< Vector > m_iv_vEyeOffset;
	
	float m_flImplicitVerticalStepSpeed;	// When moving with step code, the player has an implicit vertical
											// velocity that keeps her on ramps, steps, etc. We need this to
											// correctly transform her velocity when she teleports.

	virtual IRagdoll		*GetRepresentativeRagdoll() const;
	EHANDLE	m_hRagdoll;

	int	m_headYawPoseParam;
	int	m_headPitchPoseParam;
	float m_headYawMin;
	float m_headYawMax;
	float m_headPitchMin;
	float m_headPitchMax;
	bool m_bSuppressingCrosshair;

	bool m_isInit;
	Vector m_vLookAtTarget;

	float m_flCurrentHeadYaw;
	float m_flCurrentHeadPitch;

	EHANDLE	m_hIDEnt;
	
	CountdownTimer m_blinkTimer;

	int	  m_iSpawnInterpCounter;
	int	  m_iSpawnInterpCounterCache;

	bool  m_bHeldObjectOnOppositeSideOfPortal;
	CHandle<CProp_Portal> m_hHeldObjectPortal;

	IPhysicsObject *m_pHeldPhysicsPortal;

	int	m_iForceNoDrawInPortalSurface; //only valid for one frame, used to temp disable drawing of the player model in a surface because of freaky artifacts
	
	struct PortalEyeInterpolation_t
	{
		bool	m_bEyePositionIsInterpolating; //flagged when the eye position would have popped between two distinct positions and we're smoothing it over
		Vector	m_vEyePosition_Interpolated; //we'll be giving the interpolation a certain amount of instant movement per frame based on how much an uninterpolated eye would have moved
		Vector	m_vEyePosition_Uninterpolated; //can't have smooth movement without tracking where we just were
		//bool	m_bNeedToUpdateEyePosition;
		//int		m_iFrameLastUpdated;

		int		m_iTickLastUpdated;
		float	m_fTickInterpolationAmountLastUpdated;
		bool	m_bDisableFreeMovement; //used for one frame usually when error in free movement is likely to be high
		bool	m_bUpdatePosition_FreeMove;

		PortalEyeInterpolation_t( void ) : m_iTickLastUpdated(0), m_fTickInterpolationAmountLastUpdated(0.0f), m_bDisableFreeMovement(false), m_bUpdatePosition_FreeMove(false) { };
	} PortalEyeInterpolation;

	struct PreDataChanged_Backup_t
	{
		CHandle<C_Prop_Portal>	m_hPortalEnvironment;
		CHandle<C_Func_LiquidPortal>	m_hSurroundingLiquidPortal;
		//Vector					m_ptPlayerPosition;
		QAngle					m_qEyeAngles;
		uint32					m_iEntityPortalledNetworkMessageCount;
	} PreDataChanged_Backup;

	Vector	m_ptEyePosition_LastCalcView;
	QAngle	m_qEyeAngles_LastCalcView; //we've got some VERY persistent single frame errors while teleporting, this will be updated every frame in CalcView() and will serve as a central source for fixed angles
#ifdef CCDEATH
	ClientCCHandle_t	m_CCDeathHandle;	// handle to death cc effect
	float				m_flDeathCCWeight;	// for fading in cc effect	
#endif //CCDEATH

	bool m_bToolMode_EyeHasPortalled_LastRecord; //when recording, keep track of whether we teleported the camera position last capture or not. Need to avoid interpolating when switching

public:

	unsigned char m_iPortalLinkageGroupID; //which portal linkage group this gun is tied to, usually set by mapper, or inherited from owning player's index

	bool	m_bPitchReorientation;
	float	m_fReorientationRate;
	bool	m_bEyePositionIsTransformedByPortal; //when the eye and body positions are not on the same side of a portal

	CHandle<C_Prop_Portal>	m_hPortalEnvironment; //a portal whose environment the player is currently in, should be invalid most of the time

	float m_fLatestServerTeleport;
	VMatrix m_matLatestServerTeleportationInverseMatrix;

	void FixPortalEnvironmentOwnership( void ); //if we run prediction, there are multiple cases where m_hPortalEnvironment != CPortalSimulator::GetSimulatorThatOwnsEntity( this ), and that's bad
	CHandle<C_Func_LiquidPortal>	m_hSurroundingLiquidPortal; //a liquid portal whose volume the player is standing in

	Vector m_vecAnimStateBaseVelocity;
private:
	
	CUtlVector<C_EntityPortalledNetworkMessage> m_EntityPortalledNetworkMessages;
	enum 
	{
		MAX_ENTITY_PORTALLED_NETWORK_MESSAGES = 32,
	};
	uint32 m_iEntityPortalledNetworkMessageCount;

};

inline C_Portal_Player *ToPortalPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return static_cast<C_Portal_Player*>( pEntity );
}

inline C_Portal_Player *GetPortalPlayer( void )
{
	return static_cast<C_Portal_Player*>( C_BasePlayer::GetLocalPlayer() );
}

#endif //Portal_PLAYER_H
