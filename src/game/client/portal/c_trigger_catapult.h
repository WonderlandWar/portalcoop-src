//=============================================================================//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef C_TRIGGER_CATAPULT_H
#define C_TRIGGER_CATAPULT_H
#ifdef _WIN32
#pragma once
#endif

#include "c_triggers.h"

#define CTriggerCatapult C_TriggerCatapult
class C_TriggerCatapult : public C_BaseTrigger
{
	DECLARE_CLASS( C_TriggerCatapult, C_BaseTrigger );
	DECLARE_PREDICTABLE();
	DECLARE_CLIENTCLASS();

public:
	
	void			LaunchByTarget( C_BaseEntity *pVictim, C_BaseEntity *pTarget  );
	Vector			CalculateLaunchVector( C_BaseEntity *pVictim, C_BaseEntity *pTarget  );
	Vector			CalculateLaunchVectorPreserve( Vector vecInitialVelocity, C_BaseEntity *pVictim, C_BaseEntity *pTarget, bool bForcePlayer = false );
	
	void			LaunchByDirection( CBaseEntity *pVictim  );
	void			OnLaunchedVictim( CBaseEntity *pVictim );

	void			Spawn( void );
	bool			ShouldPredict( void );
	virtual bool	IsPredicted( void ) { return true; }

	void			StartTouch( C_BaseEntity *pEntity );
	
	virtual bool PredictionErrorShouldResetLatchedForAllPredictables( void ) OVERRIDE;

	float m_flRefireDelay[MAX_PLAYERS + 1];
	float m_flPlayerVelocity;
	float m_flPhysicsVelocity;
	QAngle m_vecLaunchAngles;
	int m_ExactVelocityChoice;
	bool m_bUseExactVelocity;
	bool m_bUseThresholdCheck;
	float m_flLowerThreshold;
	float m_flUpperThreshold;
	float m_flEntryAngleTolerance;
	EHANDLE m_hLaunchTarget;
	bool m_bOnlyVelocityCheck;
	bool m_bApplyAngularImpulse;
	bool m_bPlayersPassTriggerFilters;
	bool m_bDirectionSuppressAirControl;
};

#endif