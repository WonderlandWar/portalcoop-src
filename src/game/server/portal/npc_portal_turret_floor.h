#ifndef NPC_PORTAL_TURRET_FLOOR_H
#define NPC_PORTAL_TURRET_FLOOR_H

#ifdef _WIN32
#pragma once
#endif

#include "ai_senses.h"
#include "ai_memory.h"
#include "rope.h"
#include "rope_shared.h"
#include "npc_turret_floor.h"

#define PORTAL_FLOOR_TURRET_NUM_ROPES 4

class CNPC_Portal_FloorTurret : public CNPC_FloorTurret
{
	DECLARE_CLASS( CNPC_Portal_FloorTurret, CNPC_FloorTurret );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:

	CNPC_Portal_FloorTurret( void );

	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual void	UpdateOnRemove( void );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );

	virtual bool	ShouldAttractAutoAim( CBaseEntity *pAimingEnt );
	virtual float	GetAutoAimRadius();
	virtual Vector	GetAutoAimCenter();

	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );

	virtual void	NotifySystemEvent( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

	virtual bool	PreThink( turretState_e state );
	virtual void	Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy, bool bStrict = false );
	virtual void	SetEyeState( eyeState_t state );

	virtual bool	OnSide( void );

	virtual float	GetAttackDamageScale( CBaseEntity *pVictim );
	virtual Vector	GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget );

	// Think functions
	virtual void	Retire( void );
	virtual void	Deploy( void );
	virtual void	ActiveThink( void );
	virtual void	SearchThink( void );
	virtual void	AutoSearchThink( void );
	virtual void	TippedThink( void );
	virtual void	HeldThink( void );
	virtual void	InactiveThink( void );
	virtual void	SuppressThink( void );
	virtual void	DisabledThink( void );
	virtual void	HackFindEnemy( void );
	virtual void	BurnThink( void );
	//virtual void	BreakThink( void );
	void			TractorBeamThink( void );

	void	OnExitedTractorBeam( void );
	void	OnEnteredTractorBeam( void );

	virtual void	StartTouch( CBaseEntity *pOther );

	bool	IsLaserOn( void ) { return m_bLaserOn; }
	void	LaserOff( void );
	void	LaserOn( void );
	void	RopesOn();
	void	RopesOff();

	void	FireBullet( const char *pTargetName );

	// Inputs
	void	InputFireBullet( inputdata_t &inputdata );

private:

	CHandle<CRopeKeyframe>	m_hRopes[ PORTAL_FLOOR_TURRET_NUM_ROPES ];
	
	bool AllowedToIgnite() { return true; }

	CNetworkVar( bool, m_bOutOfAmmo );
	CNetworkVar( bool, m_bLaserOn );
	CNetworkVar( int, m_sLaserHaloSprite );

	bool	m_bIsDead;

	int		m_iBarrelAttachments[ 4 ];
	bool	m_bShootWithBottomBarrels;
	bool	m_bDamageForce;

	float	m_fSearchSpeed;
	float	m_fMovingTargetThreashold;
	float	m_flDistToEnemy;
	float	m_flBurnExplodeTime;

	turretState_e	m_iLastState;
	float			m_fNextTalk;
	bool			m_bDelayTippedTalk;

};

#endif // NPC_PORTAL_TURRET_FLOOR_H