#include "cbase.h"
#include "prop_box.h"
#include "trigger_box_reflector.h"

ConVar sv_trigger_box_reflector_temporary_time( "sv_trigger_box_reflector_temporary_time", "2", FCVAR_CHEAT );

static const char *g_pszTemporaryDetachThink = "TemporaryDetachThink";

BEGIN_DATADESC( CTriggerBoxReflector )

	DEFINE_KEYFIELD( m_iszAttachToEntity, FIELD_STRING, "AttachToEntity" ),
	
	DEFINE_KEYFIELD( m_bTemporary, FIELD_BOOLEAN, "temporary" ),

	DEFINE_FIELD( m_hAttachEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hAttachedBox, FIELD_EHANDLE ),

	DEFINE_OUTPUT( m_OnAttached, "OnAttached" ),
	DEFINE_OUTPUT( m_OnDetached, "OnDetached" ),
	DEFINE_OUTPUT( m_OnEnergyBallHit, "OnEnergyBallHit" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_box_reflector, CTriggerBoxReflector )

void CTriggerBoxReflector::Spawn( void )
{
	BaseClass::Spawn();
	
	SetSolid( GetParent() ? SOLID_VPHYSICS : SOLID_BSP );	
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_TRIGGER );	
	
	AddEffects( EF_NODRAW );

	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );    // set size and link into world
}

void CTriggerBoxReflector::Activate( void )
{
	m_hAttachEnt = gEntList.FindEntityByName( NULL, m_iszAttachToEntity.ToCStr() );
	AssertMsg( m_hAttachEnt, "Failed to find the attach entity" );

	BaseClass::Activate();
}

void CTriggerBoxReflector::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Rexaura.BoxReflector_Attach" );
	PrecacheScriptSound( "Rexaura.BoxReflector_Detach" );
	PrecacheScriptSound( "Rexaura.BoxReflector_Explosion" );
}

void CTriggerBoxReflector::UpdateOnRemove( void )
{
	if ( VPhysicsGetObject())
	{
		VPhysicsGetObject()->RemoveTrigger();
	}

	BaseClass::UpdateOnRemove();
}

//------------------------------------------------------------------------------
// Create VPhysics
//------------------------------------------------------------------------------
bool CTriggerBoxReflector::CreateVPhysics()
{
	IPhysicsObject *pPhysics = VPhysicsInitShadow( false, false );
	
	pPhysics->BecomeTrigger();
	return true;
}

void CTriggerBoxReflector::StartTouch( CBaseEntity *pOther )
{
	if ( m_hAttachedBox )
		return;

	if ( !FClassnameIs( pOther, "prop_box" ) )
		return;

	CBaseEntity *pAttachEntity = m_hAttachEnt;
	if ( !pAttachEntity )
	{
		Warning( "%s doesn't have an attach entity!\n", GetDebugName() );
		return;
	}

	CPropBox *pBox = static_cast<CPropBox*>( pOther );
	
	Assert( pBox->m_hAttached != this );

	pBox->m_hAttached = this;
	
	IPhysicsObject *pBoxPhysics = pAttachEntity->VPhysicsGetObject();
	if ( pBoxPhysics != NULL )
	{
		pBoxPhysics->EnableMotion( false );
	}

	pBox->Teleport( &pAttachEntity->GetAbsOrigin(), &pAttachEntity->GetAbsAngles(), &vec3_origin );

	EmitSound( "Rexaura.BoxReflector_Attach" );
	
	m_OnAttached.FireOutput( pBox, this );

	m_hAttachedBox = pBox;
	
	SetContextThink( &CTriggerBoxReflector::TemporaryDetachThink, gpGlobals->curtime + sv_trigger_box_reflector_temporary_time.GetFloat(), g_pszTemporaryDetachThink );
}


void CTriggerBoxReflector::EndTouch( CBaseEntity *pOther )
{
	if ( !m_hAttachedBox )
		return;
	
	CPropBox *pAttachedBox = m_hAttachedBox;
	if ( pAttachedBox && pAttachedBox != pOther )
		return;

	SetContextThink( NULL, gpGlobals->curtime, g_pszTemporaryDetachThink );

	DetachBox( pAttachedBox );
}

void CTriggerBoxReflector::DetachBox( CPropBox *pAttachedBox, bool bPush /*= false*/ )
{	
	IPhysicsObject *pBoxPhysics = pAttachedBox->VPhysicsGetObject();
	if ( pBoxPhysics != NULL )
	{
		pBoxPhysics->EnableMotion( true );
	}

	Assert( pAttachedBox->m_hAttached == this );

	pAttachedBox->m_hAttached = NULL;
	
	EmitSound( "Rexaura.BoxReflector_Detach" );

	m_OnDetached.FireOutput( pAttachedBox, this );

	if ( bPush )
	{
		Vector forward;
		pAttachedBox->GetVectors( &forward, NULL, NULL );

		Vector velocity;
		pAttachedBox->ApplyAbsVelocityImpulse( forward * 100 );
	}

	m_hAttachedBox = NULL;
}

void CTriggerBoxReflector::EnergyBallHit( CBaseEntity *pBall )
{
	EmitSound( "Rexaura.BoxReflector_Explosion" );
	m_OnEnergyBallHit.FireOutput( pBall, pBall );
}

void CTriggerBoxReflector::TemporaryDetachThink( void )
{
	if ( m_hAttachedBox )
	{
		DetachBox( m_hAttachedBox, true );
	}
}