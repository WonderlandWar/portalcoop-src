#include "cbase.h"
#include "prop_box.h"
#include "prop_combine_ball.h"
#include "trigger_portal_cleanser.h"

#define BALL_DESTROYER_DAMAGE 50
#define BALL_DESTROYER_DAMAGETYPE DMG_SHOCK

class CTriggerBallDestroyer : public CBaseEntity
{
public:
	DECLARE_CLASS( CTriggerBallDestroyer, CBaseEntity );
	DECLARE_DATADESC();

	CTriggerBallDestroyer();

	void Spawn( void );
	void Precache( void );
	void UpdateOnRemove( void );
	bool CreateVPhysics();

	void StartTouch( CBaseEntity *pOther );
	
	// Trigger hurt stuff
	void HurtThink( void );
	void Touch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );
	bool HurtPlayer( CBasePlayer *pOther, float damage );
	int HurtAllTouchers( float dt );

	bool ShouldTouch( CBaseEntity *pEntity );

private:

	bool m_bEnergyBallsOnly;

	COutputEvent m_OnDissolve;
	COutputEvent m_OnDissolveBox;
	COutputEvent m_OnExplodeBall;

	// Trigger hurt stuff
	CUtlVector<CHandle<CBasePlayer>>	m_hurtPlayers;
};

PRECACHE_REGISTER( trigger_ball_destroyer );

static const char *g_pszHurtThink = "HurtThink";

BEGIN_DATADESC( CTriggerBallDestroyer )

	// Function Pointers
	DEFINE_FUNCTION( HurtThink ),

	DEFINE_KEYFIELD( m_bEnergyBallsOnly, FIELD_BOOLEAN, "EnergyBallsOnly" ),
	
	DEFINE_UTLVECTOR( m_hurtPlayers, FIELD_EHANDLE ),

	DEFINE_OUTPUT( m_OnDissolve, "OnDissolve" ),
	DEFINE_OUTPUT( m_OnDissolveBox, "OnDissolveBox" ),
	DEFINE_OUTPUT( m_OnExplodeBall, "OnExplodeBall" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_ball_destroyer, CTriggerBallDestroyer )

CTriggerBallDestroyer::CTriggerBallDestroyer()
{
	m_bEnergyBallsOnly = false;
}

void CTriggerBallDestroyer::Spawn( void )
{
	BaseClass::Spawn();
	
	SetSolid( GetParent() ? SOLID_VPHYSICS : SOLID_BSP );	
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_TRIGGER );	
	
	AddEffects( EF_NODRAW );

	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );    // set size and link into world
}

void CTriggerBallDestroyer::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Rexaura.Ball_Force_Explosion" );
}

void CTriggerBallDestroyer::UpdateOnRemove( void )
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
bool CTriggerBallDestroyer::CreateVPhysics()
{
	IPhysicsObject *pPhysics = VPhysicsInitShadow( false, false );
	
	pPhysics->BecomeTrigger();
	return true;
}

void CTriggerBallDestroyer::StartTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() )
		return;

	if ( !ShouldTouch( pOther ) )
		return;

	CPropCombineBall *pBall = dynamic_cast<CPropCombineBall*>( pOther );
	if ( pBall )
	{
		EmitSound( "Rexaura.Ball_Force_Explosion" );
		m_OnExplodeBall.FireOutput( pOther, pOther );
		pBall->ExplodeThink(); // Pretty bad but it will do
	}

	if ( m_bEnergyBallsOnly )
		return;

	if ( FClassnameIs( pOther, "prop_box" ) )
	{
		m_OnDissolveBox.FireOutput( pOther, pOther );
	}
	
	m_OnDissolve.FireOutput( pOther, pOther );

	CTriggerPortalCleanser::FizzleBaseAnimating( pOther, NULL );
}


//-----------------------------------------------------------------------------
// Purpose: When touched, a hurt trigger does BALL_DESTROYER_DAMAGE points of damage each half-second.
// Input  : pOther - The entity that is touching us.
//-----------------------------------------------------------------------------
bool CTriggerBallDestroyer::HurtPlayer( CBasePlayer *pOther, float damage )
{
	Assert( m_bEnergyBallsOnly == false );
	if ( !pOther->m_takedamage )
		return false;

	// If player is disconnected, we're probably in this routine via the
	//  PhysicsRemoveTouchedList() function to make sure all Untouch()'s are called for the
	//  player. Calling TakeDamage() in this case can get into the speaking criteria, which
	//  will then loop through the control points and the touched list again. We shouldn't
	//  need to hurt players that are disconnected, so skip all of this...
	bool bPlayerDisconnected = pOther->IsPlayer() && ( ((CBasePlayer *)pOther)->IsConnected() == false );
	if ( bPlayerDisconnected )
		return false;

	if ( damage < 0 )
	{
		pOther->TakeHealth( -damage, BALL_DESTROYER_DAMAGETYPE );
	}
	else
	{
		// The damage position is the nearest point on the damaged entity
		// to the trigger's center. Not perfect, but better than nothing.
		Vector vecCenter = CollisionProp()->WorldSpaceCenter();

		Vector vecDamagePos;
		pOther->CollisionProp()->CalcNearestPoint( vecCenter, &vecDamagePos );

		CTakeDamageInfo info( this, this, damage, BALL_DESTROYER_DAMAGETYPE );
		info.SetDamagePosition( vecDamagePos );
		GuessDamageForce( &info, ( vecDamagePos - vecCenter ), vecDamagePos );
		
		pOther->TakeDamage( info );
	}

	CHandle<CBasePlayer> hOther = pOther;
	m_hurtPlayers.AddToTail( hOther );
	//NDebugOverlay::Box( pOther->GetAbsOrigin(), pOther->WorldAlignMins(), pOther->WorldAlignMaxs(), 255,0,0,0,0.5 );
	return true;
}

void CTriggerBallDestroyer::HurtThink()
{
	// if I hurt anyone, think again
	if ( HurtAllTouchers( 0.5 ) <= 0 )
	{
		SetThink(NULL);
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.5f );
	}
}

void CTriggerBallDestroyer::EndTouch( CBaseEntity *pOther )
{
	if ( m_bEnergyBallsOnly )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pOther );
	if ( !pPlayer )
		return;

	CHandle<CBasePlayer> hOther;
	hOther = pPlayer;

	// if this guy has never taken damage, hurt him now
	if ( !m_hurtPlayers.HasElement( hOther ) )
	{
		HurtPlayer( pPlayer, BALL_DESTROYER_DAMAGE * 0.5 );
	}

	BaseClass::EndTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: called from RadiationThink() as well as HurtThink()
//			This function applies damage to any entities currently touching the
//			trigger
// Input  : dt - time since last call
// Output : int - number of entities actually hurt
//-----------------------------------------------------------------------------
#define TRIGGER_HURT_FORGIVE_TIME	3.0f	// time in seconds
int CTriggerBallDestroyer::HurtAllTouchers( float dt )
{
	int hurtCount = 0;
	// half second worth of damage
	float fldmg = BALL_DESTROYER_DAMAGE * dt;

	m_hurtPlayers.RemoveAll();

	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		for ( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
		{
			CBasePlayer *pTouch = ToBasePlayer( link->entityTouched );
			if ( pTouch )
			{
				if ( HurtPlayer( pTouch, fldmg ) )
				{
					hurtCount++;
				}
			}
		}
	}

	return hurtCount;
}

void CTriggerBallDestroyer::Touch( CBaseEntity *pOther )
{
	if ( m_bEnergyBallsOnly )
		return;

	if ( !pOther->IsPlayer() )
		return;

	if ( m_pfnThink == NULL )
	{
		SetThink( &CTriggerBallDestroyer::HurtThink );
		SetNextThink( gpGlobals->curtime );
	}
}

bool CTriggerBallDestroyer::ShouldTouch( CBaseEntity *pEntity )
{
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
		return true;

	return false;
}