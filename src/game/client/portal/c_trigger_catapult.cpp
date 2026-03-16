#include "cbase.h"
#include "c_trigger_catapult.h"

#undef CTriggerCatapult
IMPLEMENT_CLIENTCLASS_DT( C_TriggerCatapult, DT_TriggerCatapult, CTriggerCatapult )
	RecvPropArray3( RECVINFO_ARRAY(m_flRefireDelay), RecvPropFloat(RECVINFO_ARRAY(m_flRefireDelay)) ),
	RecvPropFloat( RECVINFO( m_flPlayerVelocity ) ),
	RecvPropFloat( RECVINFO( m_flPhysicsVelocity ) ),
	RecvPropQAngles( RECVINFO( m_vecLaunchAngles ) ),
	RecvPropInt( RECVINFO( m_ExactVelocityChoice ) ),
	RecvPropBool( RECVINFO( m_bUseExactVelocity ) ),
	RecvPropBool( RECVINFO( m_bUseThresholdCheck ) ),
	RecvPropBool( RECVINFO( m_bOnlyVelocityCheck ) ),
	RecvPropFloat( RECVINFO( m_flLowerThreshold ) ),
	RecvPropFloat( RECVINFO( m_flUpperThreshold ) ),
	RecvPropBool( RECVINFO( m_bApplyAngularImpulse ) ),
	RecvPropFloat( RECVINFO( m_flEntryAngleTolerance ) ),
	RecvPropEHandle( RECVINFO( m_hLaunchTarget ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_TriggerCatapult )
	DEFINE_PRED_ARRAY( m_flRefireDelay, FIELD_FLOAT, MAX_PLAYERS+1, FTYPEDESC_INSENDTABLE )
END_PREDICTION_DATA()

void C_TriggerCatapult::Spawn( void )
{
	BaseClass::Spawn();
	SetPredictionEligible( true );
}

bool C_TriggerCatapult::ShouldPredict( void )
{
	return true;
}

bool C_TriggerCatapult::PredictionErrorShouldResetLatchedForAllPredictables( void )
{
	return false;
}