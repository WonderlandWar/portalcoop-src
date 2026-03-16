//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef TRIGGER_CATAPULT_H
#define TRIGGER_CATAPULT_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"


class CTriggerCatapult : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerCatapult, CBaseTrigger );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:

	CTriggerCatapult( void );

	virtual void	Spawn( void );
	virtual void	StartTouch( CBaseEntity *pOther );
	virtual void	EndTouch( CBaseEntity *pOther );
	virtual int		DrawDebugTextOverlays(void);
	virtual void	DrawDebugGeometryOverlays( void );

	void			LaunchThink( void );
	
	virtual bool	 IsPredicted( void ) { return true; }

private:
	void			InputSetPlayerSpeed( inputdata_t &in );
	void			InputSetPhysicsSpeed( inputdata_t &in );
	void			InputSetLaunchTarget( inputdata_t &in );
	void			InputSetExactVelocityChoiceType( inputdata_t &in );

	void			LaunchByTarget( CBaseEntity *pVictim, CBaseEntity *pTarget  );
	Vector			CalculateLaunchVector( CBaseEntity *pVictim, CBaseEntity *pTarget  );
	Vector			CalculateLaunchVectorPreserve( Vector vecInitialVelocity, CBaseEntity *pVictim, CBaseEntity *pTarget, bool bForcePlayer = false );

	void			LaunchByDirection( CBaseEntity *pVictim  );
	void			OnLaunchedVictim( CBaseEntity *pVictim );

	CNetworkArray( float, m_flRefireDelay, MAX_PLAYERS + 1 ); // 0 for physics object the rest for each player userid
	CNetworkVar( float, m_flPlayerVelocity );
	CNetworkVar( float, m_flPhysicsVelocity );
	CNetworkQAngle( m_vecLaunchAngles );
	string_t m_strLaunchTarget;

	CNetworkVar( int, m_ExactVelocityChoice );
	CNetworkVar( bool, m_bUseExactVelocity );

	CNetworkVar( bool, m_bUseThresholdCheck );
	CNetworkVar( float, m_flLowerThreshold );
	CNetworkVar( float, m_flUpperThreshold );
	CNetworkVar( float, m_flEntryAngleTolerance );

	CNetworkHandle( CBaseEntity, m_hLaunchTarget );

	CNetworkVar( bool, m_bOnlyVelocityCheck );
	CNetworkVar( bool, m_bApplyAngularImpulse );

	COutputEvent	m_OnCatapulted;

	CUtlVector< EHANDLE > m_hAbortedLaunchees;
};

#endif // TRIGGER_CATAPULT_H  
