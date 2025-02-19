//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef PORTAL_PLAYER_H
#define PORTAL_PLAYER_H
#pragma once

class CPortal_Player;

#include "player.h"
#include "portal_playeranimstate.h"
#include "hl2_playerlocaldata.h"
#include "hl2_player.h"
#include "simtimer.h"
#include "soundenvelope.h"
#include "portal_player_shared.h"
#include "prop_portal.h"
#include "weapon_portalbase.h"
#include "in_buttons.h"
#include "func_liquidportal.h"
#include "ai_speech.h"			// For expresser host


class CInfoPlayerPortalCoop : public CPointEntity
{
public:
	DECLARE_CLASS(CInfoPlayerPortalCoop, CPointEntity);
	DECLARE_DATADESC();

	void PlayerSpawned( CBasePlayer *pPlayer );

	bool CanSpawnOnMe( CBasePlayer *pPlayer );

	bool m_bSpawnWithPortalgun;
	int m_iPortalgunType;
	int m_iValidPlayerIndex;

	//Outputs

	COutputEvent m_OnPlayerSpawned;

	// Inputs
	
	void InputEnablePortalgunSpawn( inputdata_t &inputdata )	{ m_bSpawnWithPortalgun = true; }
	void InputDisablePortalgunSpawn( inputdata_t &inputdata )	{ m_bSpawnWithPortalgun = false; }
	
	void InputSetPortalgunType( inputdata_t &inputdata )		{ m_iPortalgunType = inputdata.value.Int(); }
	void InputSetPlayer( inputdata_t &inputdata )				{ m_iValidPlayerIndex = inputdata.value.Int(); }
};

class CWeaponPortalgun;

//=============================================================================
// >> Portal_Player
//=============================================================================
class CPortal_Player : public CAI_ExpresserHost<CHL2_Player> 
{
public:
	DECLARE_CLASS( CPortal_Player, CHL2_Player );

	CPortal_Player();
	~CPortal_Player( void );
	
	static CPortal_Player *CreatePlayer( const char *className, edict_t *ed )
	{
		CPortal_Player::s_PlayerEdict = ed;
		return (CPortal_Player*)CreateEntityByName( className );
	}

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void CreateSounds( void );
	virtual void StopLoopingSounds( void );
	virtual void Spawn( void );
	virtual void OnRestore( void );
	virtual void Activate( void );

	virtual void NotifySystemEvent( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

	virtual void PostThink( void );
	virtual void PreThink( void );
	virtual void PlayerDeathThink( void );
//	void HudHintThink( void );
	
	CNetworkVar (bool, m_bHasSprintDevice);
	CNetworkVar (bool, m_bSprintEnabled);
	
	void UpdatePortalPlaneSounds( void );
	void UpdateWooshSounds( void );

	Activity TranslateActivity( Activity ActToTranslate, bool *pRequired = NULL );
	virtual void Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity );

	//Activity TranslateTeamActivity( Activity ActToTranslate );

	virtual void SetAnimation( PLAYER_ANIM playerAnim );

	virtual CAI_Expresser* GetExpresser( void );

	virtual void PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper);
	
	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}
	
	virtual bool ClientCommand( const CCommand &args );
	virtual void CreateViewModel( int viewmodelindex = 0 );
	virtual bool BecomeRagdollOnClient( const Vector &force );
	virtual int	OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual int	OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual	bool ShouldCollide( int collisionGroup, int contentsMask ) const;
	virtual bool WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const;
	virtual void FireBullets ( const FireBulletsInfo_t &info );
	virtual bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0);
	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );
	virtual void ShutdownUseEntity( void );

	virtual const Vector&	WorldSpaceCenter( ) const;

	virtual void VPhysicsShadowUpdate( IPhysicsObject *pPhysics );

	//virtual bool StartReplayMode( float fDelay, float fDuration, int iEntity  );
	//virtual void StopReplayMode();
 	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void Jump( void );

	bool UseFoundEntity( CBaseEntity *pUseEntity );
	CBaseEntity* FindUseEntity( void );
	CBaseEntity* FindUseEntityThroughPortal( void );

	virtual bool PlayerUse( void );
	//virtual bool StartObserverMode( int mode );
	virtual void GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void UpdateOnRemove( void );

	virtual void SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );
	virtual void UpdatePortalViewAreaBits( unsigned char *pvs, int pvssize );
	
	bool	ValidatePlayerModel( const char *pModel );

	QAngle GetAnimEyeAngles( void ) { return m_angEyeAngles.Get(); }

	Vector GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );

	void CheatImpulseCommands( int iImpulse );
	void CreateRagdollEntity( const CTakeDamageInfo &info );
	void GiveAllItems( void );
	void GiveDefaultItems( void );

	void NoteWeaponFired( void );

	void ResetAnimation( void );

	void SetPlayerModel( void );

	void SetupSkin( void );

	bool PortalColorSetWasDifferent( void );
	
	int	GetPlayerConcept( void );
	void UpdateExpression ( void );
	void ClearExpression ( void );
	
	float GetImplicitVerticalStepSpeed() const;
	void SetImplicitVerticalStepSpeed( float speed );

	void ForceDuckThisFrame( void );
	void UnDuck ( void );
	inline void ForceJumpThisFrame( void ) { ForceButtons( IN_JUMP ); }

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	void SetupBones( matrix3x4_t *pBoneToWorld, int boneMask );

	// physics interactions
	virtual void PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize );
	virtual void ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldingThis );

	void ToggleHeldObjectOnOppositeSideOfPortal( void ) { m_bHeldObjectOnOppositeSideOfPortal = !m_bHeldObjectOnOppositeSideOfPortal; }
	void SetHeldObjectOnOppositeSideOfPortal( bool p_bHeldObjectOnOppositeSideOfPortal ) { m_bHeldObjectOnOppositeSideOfPortal = p_bHeldObjectOnOppositeSideOfPortal; }
	bool IsHeldObjectOnOppositeSideOfPortal( void ) { return m_bHeldObjectOnOppositeSideOfPortal; }
	CProp_Portal *GetHeldObjectPortal( void ) { return m_hHeldObjectPortal; }
	void SetHeldObjectPortal( CProp_Portal *pPortal ) { m_hHeldObjectPortal = pPortal; }

	void SetStuckOnPortalCollisionObject( void ) { m_bStuckOnPortalCollisionObject = true; }

	CWeaponPortalBase* GetActivePortalWeapon() const;
	
	void SetNeuroToxinDamageTime( float fCountdownSeconds ) { m_fNeuroToxinDamageTime = gpGlobals->curtime + fCountdownSeconds; }
	
	virtual void ApplyPortalTeleportation( const CProp_Portal *pEnteredPortal, CMoveData *pMove ); 
	
	Vector m_vecTotalBulletForce;	//Accumulator for bullet force in a single frame

	CNetworkVar(bool, m_bSilentDropAndPickup);

	// Tracks our ragdoll entity.
	CNetworkHandle( CBaseEntity, m_hRagdoll );	// networked entity handle

	void SuppressCrosshair( bool bState ) { m_bSuppressingCrosshair = bState; }

	// In multiplayer, last time we used a coop ping to draw our partner's attention
	CNetworkVar(float, m_flLastPingTime);

	CProp_Portal *m_pPrimaryPortal;
	CProp_Portal *m_pSecondaryPortal;

	CNetworkVar(int, m_iCustomPortalColorSet);
	
	void SetEyeUpOffset( const Vector& vOldUp, const Vector& vNewUp );
	void SetEyeOffset( const Vector& vOldOrigin, const Vector& vNewOrigin );

	CWeaponPortalgun *m_pSpawnedPortalgun;

	void SetLookingForUseEntity( bool bLookingForUseEntity ) { m_bLookingForUseEntity = bLookingForUseEntity; }
	void SetLookForUseEntity( bool bLookForUseEntity ) { m_bLookForUseEntity = bLookForUseEntity; }

	bool m_bGotPortalMessage;

	CNetworkHandle( CProp_Portal, m_hHeldObjectPortal );	// networked entity handle

private:
	
	bool m_bLookingForUseEntity;
	bool m_bLookForUseEntity;
	float m_flLookForUseEntityTime;
	
	virtual CAI_Expresser* CreateExpresser( void );

	CSoundPatch		*m_pWooshSound;

	CNetworkQAngle( m_angEyeAngles );

	CPortalPlayerAnimState*   m_PlayerAnimState;
	
	float m_flImplicitVerticalStepSpeed;	// When moving with step code, the player has an implicit vertical
											// velocity that keeps her on ramps, steps, etc. We need this to
											// correctly transform her velocity when she teleports.

	int m_iLastWeaponFireUsercmd;
	CNetworkVar( int, m_iSpawnInterpCounter );
	CNetworkVar( bool, m_bSuppressingCrosshair );

	CNetworkVar( bool, m_bIsListenServerHost )

	CNetworkVar(bool, m_bHeldObjectOnOppositeSideOfPortal);

	bool m_bIntersectingPortalPlane;
	bool m_bStuckOnPortalCollisionObject;


	float m_fTimeLastHurt;
	bool  m_bIsRegenerating;		// Is the player currently regaining health

	float m_fNeuroToxinDamageTime;
			
	QAngle						m_qPrePortalledViewAngles;
	bool						m_bFixEyeAnglesFromPortalling;
	float						m_flTimeToWaitForPortalMessage;
	bool						m_bPendingPortalMessage;
	VMatrix						m_matLastPortalled;
	CAI_Expresser				*m_pExpresser;
	string_t					m_iszExpressionScene;
	EHANDLE						m_hExpressionSceneEnt;
	float						m_flExpressionLoopTime;

	Vector m_vEyeOffset;

	
	struct RecentPortalTransform_t
	{
		int command_number;
		CHandle<CProp_Portal> Portal;
		matrix3x4_t matTransform;
	};

	CUtlVector<RecentPortalTransform_t> m_PendingPortalTransforms; //portal transforms we've sent to the client but they have not yet acknowledged, needed for some input fixup
	
	// stick camera
	void RotateUpVector( Vector& vForward, Vector& vUp );
	void SnapCamera( bool bLookingInBadDirection );
	void PostTeleportationCameraFixup( const CProp_Portal *pEnteredPortal );

public:
	
	CNetworkVar( bool, m_bPitchReorientation );

	CNetworkHandle( CProp_Portal, m_hPortalEnvironment ); //if the player is in a portal environment, this is the associated portal
	CNetworkHandle( CFunc_LiquidPortal, m_hSurroundingLiquidPortal ); //if the player is standing in a liquid portal, this will point to it

	void InputDoPingHudHint(inputdata_t &inputdata);
	
	// Coop ping effect
	void	PlayCoopPingEffect( void );
	bool	PingChildrenOfChildParent( CBaseAnimating *pAnimating, Vector vColor );
	bool	PingChildrenOfEntity( trace_t &tr, Vector vColor, bool bShouldCreateCrosshair );

	friend class CProp_Portal;
	
	virtual CBaseEntity* EntSelectSpawnPoint( void );

#ifdef PORTAL_MP
public:
	void PickTeam( void );
#endif
};

inline CPortal_Player *ToPortalPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return static_cast<CPortal_Player*>( pEntity );
}

inline CPortal_Player *GetPortalPlayer( int iPlayerIndex )
{
	return static_cast<CPortal_Player*>( UTIL_PlayerByIndex( iPlayerIndex ) );
}

#endif //PORTAL_PLAYER_H
