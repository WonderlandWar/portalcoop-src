#include "cbase.h"
#include "prop_box.h"
#include "trigger_box_reflector.h"
#include "prop_combine_ball.h"
#include "trigger_portal_cleanser.h"
#include "collisionutils.h"

ConVar sv_trigger_box_reflector_temporary_time( "sv_trigger_box_reflector_temporary_time", "2", FCVAR_CHEAT );

static const char *g_pszTemporaryDetachThink = "TemporaryDetachThink";

PRECACHE_REGISTER( trigger_box_reflector );

BEGIN_DATADESC( CTriggerBoxReflector )

	DEFINE_KEYFIELD( m_iszAttachToEntity, FIELD_STRING, "AttachToEntity" ),
	
	DEFINE_KEYFIELD( m_bTemporary, FIELD_BOOLEAN, "temporary" ),

	DEFINE_FIELD( m_hAttachEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hAttachedBox, FIELD_EHANDLE ),

	DEFINE_OUTPUT( m_OnAttached, "OnAttached" ),
	DEFINE_OUTPUT( m_OnDetached, "OnDetached" ),
	DEFINE_OUTPUT( m_OnEnergyBallHit, "OnEnergyBallHit" ),

	DEFINE_THINKFUNC( TemporaryDetachThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_box_reflector, CTriggerBoxReflector )

CTriggerBoxReflector::CTriggerBoxReflector()
{
	m_bTemporary = false;
	m_flTemporaryDetachTime = 0;
}

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
	PrecacheScriptSound( "Rexaura.Ball_Force_Explosion" );
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

void CTriggerBoxReflector::Touch( CBaseEntity *pOther )
{
	if ( m_hAttachedBox )
		return;

	if ( !FClassnameIs( pOther, "prop_box" ) )
		return;

	if ( m_bTemporary )
	{
		if ( m_flTemporaryDetachTime >= gpGlobals->curtime )
		{
			return;
		}
	}

	CBaseEntity *pAttachEntity = m_hAttachEnt;
	if ( !pAttachEntity )
	{
		Warning( "%s doesn't have an attach entity!\n", GetDebugName() );
		return;
	}

	CPropBox *pBox = static_cast<CPropBox*>( pOther );

	// Test to see if any players are in the way:
#if 0
	for ( int i = 1; i < gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		bool bDucked = (pPlayer->GetFlags() & FL_DUCKING) != 0;

		Vector vecPlayerOrigin = pPlayer->GetAbsOrigin();
		Vector vPlayerMins = ( bDucked ? VEC_DUCK_HULL_MIN : VEC_HULL_MIN ) + vecPlayerOrigin;
		Vector vPlayerMaxs = ( bDucked ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX ) + vecPlayerOrigin;
		
		Vector vecBoxOrigin = pAttachEntity->GetAbsOrigin();
		Vector vBoxMins = pBox->CollisionProp()->OBBMins() + vecBoxOrigin;
		Vector vBoxMaxs = pBox->CollisionProp()->OBBMaxs() + vecBoxOrigin;
		
		NDebugOverlay::Box( vec3_origin, vPlayerMins, vPlayerMaxs, 255, 0, 0, 100, 0.05 );
		NDebugOverlay::Box( vec3_origin, vBoxMins, vBoxMaxs, 255, 0, 0, 100, 0.05 );

		if ( IsBoxIntersectingBox( vPlayerMins, vPlayerMaxs, vBoxMins, vBoxMaxs ) )
		{
			return;
		}
	}
#else
	Vector vecBoxOrigin = pAttachEntity->GetAbsOrigin();
		
	CTraceFilterOnlyPlayer filter( NULL, COLLISION_GROUP_NONE );

	trace_t tr;
	UTIL_TraceHull( vecBoxOrigin, vecBoxOrigin, pBox->CollisionProp()->OBBMins() * 1.4, pBox->CollisionProp()->OBBMaxs() * 1.4, MASK_ALL, &filter, &tr );
	
	if ( tr.m_pEnt )
	{
		Assert( tr.m_pEnt->IsPlayer() );
		return;
	}
#endif

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
	
	if ( m_bTemporary )
	{
		SetContextThink( &CTriggerBoxReflector::TemporaryDetachThink, gpGlobals->curtime + sv_trigger_box_reflector_temporary_time.GetFloat(), g_pszTemporaryDetachThink );
	}
}


void CTriggerBoxReflector::EndTouch( CBaseEntity *pOther )
{
	if ( !m_hAttachedBox )
		return;
	
	CPropBox *pAttachedBox = m_hAttachedBox;
	if ( pAttachedBox && pAttachedBox != pOther )
		return;
	
	if ( m_bTemporary )
	{
		SetContextThink( NULL, gpGlobals->curtime, g_pszTemporaryDetachThink );
	}

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
	EmitSound( "Rexaura.Ball_Force_Explosion" );
	m_OnEnergyBallHit.FireOutput( pBall, pBall );
}

void CTriggerBoxReflector::TemporaryDetachThink( void )
{
	if ( m_hAttachedBox )
	{
		DetachBox( m_hAttachedBox, true );
		m_flTemporaryDetachTime = gpGlobals->curtime + 0.4;
	}
}

// Global Savedata for base trigger
BEGIN_DATADESC( CFuncBoxReflectorShield )

	DEFINE_KEYFIELD( m_iszBoxReflector,	FIELD_STRING,	"BoxReflector" ),
	DEFINE_FIELD( m_hBoxReflector,	FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( func_box_reflector_shield, CFuncBoxReflectorShield );

void CFuncBoxReflectorShield::Spawn( void )
{
	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetModel( STRING( GetModelName() ) );
	AddEffects( EF_NODRAW );
	CreateVPhysics();
	VPhysicsGetObject()->EnableCollisions( !m_bDisabled );
}


bool CFuncBoxReflectorShield::CreateVPhysics( void )
{
	VPhysicsInitStatic();
	return true;
}

void CFuncBoxReflectorShield::Activate( void ) 
{ 
	// Get a handle to my filter entity if there is one
	if (m_iszBoxReflector != NULL_STRING)
	{
		m_hBoxReflector = dynamic_cast<CTriggerBoxReflector*>(gEntList.FindEntityByName(NULL, m_iszBoxReflector));
	}
	BaseClass::Activate();
}

bool CFuncBoxReflectorShield::ForceVPhysicsCollide( CBaseEntity *pEntity )
{
	if ( dynamic_cast<CPropCombineBall*>( pEntity ) != NULL )
		return true;

	return false;
}

void CFuncBoxReflectorShield::InputEnable( inputdata_t &inputdata )
{
	VPhysicsGetObject()->EnableCollisions(true);
	m_bDisabled = false;
}

void CFuncBoxReflectorShield::InputDisable( inputdata_t &inputdata )
{
	VPhysicsGetObject()->EnableCollisions(false);
	m_bDisabled = true;
}

void CFuncBoxReflectorShield::EnergyBallHit( CPropCombineBall *pBall )
{
	CTriggerBoxReflector *pReflector = m_hBoxReflector;
	if ( pReflector )
	{
		CPropBox *pBox = pReflector->GetBox();
		if ( pBox )
		{
			CTriggerPortalCleanser::FizzleBaseAnimating( pBox, NULL );
		}
	}
}