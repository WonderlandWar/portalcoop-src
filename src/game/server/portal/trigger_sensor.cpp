#include "cbase.h"
#include "triggers.h"
#include "saverestore_utlvector.h"

struct TouchingEntities_t
{
	DECLARE_SIMPLE_DATADESC();
	EHANDLE hEntity;
	bool bBehind;
};

BEGIN_SIMPLE_DATADESC( TouchingEntities_t )
	DEFINE_FIELD( hEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( bBehind, FIELD_BOOLEAN ),
END_DATADESC()

class CTriggerSensor : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerSensor, CBaseTrigger );
	DECLARE_DATADESC();

	CTriggerSensor();

	void Spawn( void );
	virtual void Disable( void );

	void StartTouch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

	bool IsBehindTrigger( CBaseEntity *pOther );
	
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );

private:

	bool m_bActivated;
	QAngle m_qTriggerDirection;

	COutputEvent m_OnActivate;
	COutputEvent m_OnDeactivate;

	CUtlVector< TouchingEntities_t > m_SensorEntities;
};

LINK_ENTITY_TO_CLASS( trigger_sensor, CTriggerSensor );
BEGIN_DATADESC( CTriggerSensor )
	
	DEFINE_UTLVECTOR( m_SensorEntities, FIELD_EMBEDDED ),
	DEFINE_FIELD( m_bActivated, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_qTriggerDirection, FIELD_VECTOR, "TriggerDirection" ),

	DEFINE_OUTPUT( m_OnActivate, "OnActivate" ),
	DEFINE_OUTPUT( m_OnDeactivate, "OnDeactivate" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate", InputDeactivate ),

END_DATADESC()

CTriggerSensor::CTriggerSensor()
{
	m_bActivated = false;
}

void CTriggerSensor::Spawn( void )
{
	InitTrigger();
	BaseClass::Spawn();
}

void CTriggerSensor::Disable( void )
{
	m_SensorEntities.Purge();
	BaseClass::Disable();
}

bool CTriggerSensor::IsBehindTrigger( CBaseEntity *pOther )
{
	Vector vTargetDir = GetAbsOrigin() - pOther->WorldSpaceCenter();
	VectorNormalize(vTargetDir);

	Vector vTriggerDirection;
	AngleVectors( m_qTriggerDirection, &vTriggerDirection );

	float fDotPr = DotProduct( vTriggerDirection, vTargetDir );
	return fDotPr > 0;
}

void CTriggerSensor::StartTouch( CBaseEntity *pOther )
{
	if ( !PassesTriggerFilters( pOther ) )
		return;
	
	for ( int i = 0; i < m_SensorEntities.Count(); ++i )
	{
		if ( pOther == m_SensorEntities[i].hEntity 
			|| !m_SensorEntities[i].hEntity ) // If the entity was deleted, remove it from the list
		{
			m_SensorEntities.Remove( i );
			i = 0;
		}
	}

	TouchingEntities_t touching;
	touching.hEntity = pOther;
	touching.bBehind = IsBehindTrigger( pOther );
	m_SensorEntities.AddToTail( touching );
}

void CTriggerSensor::EndTouch( CBaseEntity *pOther )
{
	if ( !PassesTriggerFilters( pOther ) )
		return;

	TouchingEntities_t *touching = NULL;
	for ( int i = 0; i < m_SensorEntities.Count(); ++i )
	{
		if ( pOther == m_SensorEntities[i].hEntity )
		{
			touching = &m_SensorEntities[i];
			break;
		}
	}

	if ( !touching )
		return;

	bool bToggleState = false;
	if ( touching->bBehind )
	{
		bToggleState = !IsBehindTrigger( pOther );
	}
	else
	{
		bToggleState = IsBehindTrigger( pOther );
	}

	if ( bToggleState )
	{
		m_bActivated = !m_bActivated;
		if ( m_bActivated )
		{
			m_OnActivate.FireOutput( pOther, pOther );
		}
		else
		{
			m_OnDeactivate.FireOutput( pOther, pOther );
		}
	}
	
	for ( int i = 0; i < m_SensorEntities.Count(); ++i )
	{
		if ( pOther == m_SensorEntities[i].hEntity )
		{
			m_SensorEntities.Remove( i );
			break;
		}
	}
}

void CTriggerSensor::InputActivate( inputdata_t &inputdata )
{
	if ( m_bActivated )
		return;

	m_OnActivate.FireOutput( inputdata.pActivator, inputdata.pCaller );
	m_bActivated = true;
}

void CTriggerSensor::InputDeactivate( inputdata_t &inputdata )
{
	if ( m_bActivated == false )
		return;

	m_OnDeactivate.FireOutput( inputdata.pActivator, inputdata.pCaller );
	m_bActivated = false;
}