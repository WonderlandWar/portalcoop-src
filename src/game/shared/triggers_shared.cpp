#include "cbase.h"
#include "triggers_shared.h"
#include "in_buttons.h"
#ifdef GAME_DLL
#include "ai_basenpc.h"
#include "iservervehicle.h"
#include "ilagcompensationmanager.h"
#else
#include "c_ai_basenpc.h"
#include "iclientvehicle.h"
#endif

#ifdef CLIENT_DLL
#define CTriggerPlayerMovement C_TriggerPlayerMovement
#define CTriggerPush C_TriggerPush
#else
extern ConVar showtriggers;
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( BaseTrigger, DT_BaseTrigger )
BEGIN_NETWORK_TABLE( CBaseTrigger, DT_BaseTrigger )
#ifdef GAME_DLL
	SendPropBool( SENDINFO( m_bDisabled ) ),
	SendPropInt( SENDINFO(m_spawnflags), -1, SPROP_NOSCALE ),
	SendPropEHandle( SENDINFO( m_hFilter ) )
#else
	RecvPropBool( RECVINFO( m_bDisabled ) ),
	RecvPropInt( RECVINFO(m_spawnflags) ),
	RecvPropEHandle( RECVINFO( m_hFilter ) )
#endif
END_NETWORK_TABLE()


//-----------------------------------------------------------------------------
// Purpose: Returns true if this entity passes the filter criteria, false if not.
// Input  : pOther - The entity to be filtered.
//-----------------------------------------------------------------------------
bool CBaseTrigger::PassesTriggerFilters(CBaseEntity *pOther)
{
#ifdef CLIENT_DLL
	AssertMsg( IsPredicted(), "We shouldn't be touched, how is this being called?" );
#endif
	// First test spawn flag filters
	if ( HasSpawnFlags(SF_TRIGGER_ALLOW_ALL) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_CLIENTS) && (pOther->GetFlags() & FL_CLIENT)) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_NPCS) && (pOther->GetFlags() & FL_NPC)) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_PUSHABLES) && FClassnameIs(pOther, "func_pushable")) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_PHYSICS) && pOther->GetMoveType() == MOVETYPE_VPHYSICS) 
#if defined( HL2_EPISODIC ) || defined( TF_DLL ) || defined ( TF_CLIENT_DLL )	
		||
		(	HasSpawnFlags(SF_TRIG_TOUCH_DEBRIS) && 
			(pOther->GetCollisionGroup() == COLLISION_GROUP_DEBRIS ||
			pOther->GetCollisionGroup() == COLLISION_GROUP_DEBRIS_TRIGGER || 
			pOther->GetCollisionGroup() == COLLISION_GROUP_INTERACTIVE_DEBRIS)
		)
#endif
		)
	{
		if ( pOther->GetFlags() & FL_NPC )
		{
#ifndef CLIENT_DLL
			CAI_BaseNPC *pNPC = pOther->MyNPCPointer();

			if ( HasSpawnFlags( SF_TRIGGER_ONLY_PLAYER_ALLY_NPCS ) )
			{
				if ( !pNPC || !pNPC->IsPlayerAlly() )
				{
					return false;
				}
			}

			if ( HasSpawnFlags( SF_TRIGGER_ONLY_NPCS_IN_VEHICLES ) )
			{
				if ( !pNPC || !pNPC->IsInAVehicle() )
					return false;
			}
#else
			return false;
#endif
		}

		bool bOtherIsPlayer = pOther->IsPlayer();

		if ( bOtherIsPlayer )
		{
			CBasePlayer *pPlayer = (CBasePlayer*)pOther;
			if ( !pPlayer->IsAlive() )
				return false;

			if ( HasSpawnFlags(SF_TRIGGER_ONLY_CLIENTS_IN_VEHICLES) )
			{
#ifndef CLIENT_DLL
				if ( !pPlayer->IsInAVehicle() )
					return false;

				// Make sure we're also not exiting the vehicle at the moment
				IServerVehicle *pVehicle = pPlayer->GetVehicle();

				if ( pVehicle == NULL )
					return false;

				if ( pVehicle->IsPassengerExiting() )
					return false;
#else
				return false;
#endif
			}

			if ( HasSpawnFlags(SF_TRIGGER_ONLY_CLIENTS_OUT_OF_VEHICLES) )
			{
				if ( pPlayer->IsInAVehicle() )
					return false;
			}
#ifndef CLIENT_DLL
			if ( HasSpawnFlags( SF_TRIGGER_DISALLOW_BOTS ) )
			{
				if ( pPlayer->IsFakeClient() )
					return false;
			}
#endif
		}

		CBaseFilter *pFilter = m_hFilter.Get();
		return (!pFilter) ? true : pFilter->PassesFilter( this, pOther );
	}
	return false;
}

IMPLEMENT_NETWORKCLASS_ALIASED( BaseVPhysicsTrigger, DT_BaseVPhysicsTrigger )
BEGIN_NETWORK_TABLE( CBaseVPhysicsTrigger, DT_BaseVPhysicsTrigger )
#ifdef GAME_DLL
	SendPropBool( SENDINFO( m_bDisabled ) ),
	SendPropInt( SENDINFO(m_spawnflags), -1, SPROP_NOSCALE ),
	SendPropEHandle( SENDINFO( m_hFilter ) )
#else
	RecvPropBool( RECVINFO( m_bDisabled ) ),
	RecvPropInt( RECVINFO(m_spawnflags) ),
	RecvPropEHandle( RECVINFO( m_hFilter ) )
#endif
END_NETWORK_TABLE()

//------------------------------------------------------------------------------
// Spawn
//------------------------------------------------------------------------------
void CBaseVPhysicsTrigger::Spawn()
{
#ifndef CLIENT_DLL
	Precache();

	SetSolid( SOLID_VPHYSICS );	
	AddSolidFlags( FSOLID_NOT_SOLID );

	// NOTE: Don't make yourself FSOLID_TRIGGER here or you'll get game 
	// collisions AND vphysics collisions.  You don't want any game collisions
	// so just use FSOLID_NOT_SOLID

	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );    // set size and link into world
	if ( showtriggers.GetInt() == 0 )
	{
		AddEffects( EF_NODRAW );
	}

	CreateVPhysics();
#endif
}

//------------------------------------------------------------------------------
// Create VPhysics
//------------------------------------------------------------------------------
bool CBaseVPhysicsTrigger::CreateVPhysics()
{
	IPhysicsObject *pPhysics;
	if ( !HasSpawnFlags( SF_VPHYSICS_MOTION_MOVEABLE ) )
	{
		pPhysics = VPhysicsInitStatic();
	}
	else
	{
		pPhysics = VPhysicsInitShadow( false, false );
	}

	pPhysics->BecomeTrigger();
	return true;
}

//------------------------------------------------------------------------------
// Cleanup
//------------------------------------------------------------------------------
void CBaseVPhysicsTrigger::UpdateOnRemove()
{
	if ( VPhysicsGetObject())
	{
		VPhysicsGetObject()->RemoveTrigger();
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this entity passes the filter criteria, false if not.
// Input  : pOther - The entity to be filtered.
//-----------------------------------------------------------------------------
bool CBaseVPhysicsTrigger::PassesTriggerFilters(CBaseEntity *pOther)
{
#ifdef CLIENT_DLL
	AssertMsg( IsPredicted(), "We shouldn't be touched, how is this being called?" );
#endif
	if ( pOther->GetMoveType() != MOVETYPE_VPHYSICS && !pOther->IsPlayer() )
		return false;

	// First test spawn flag filters
	if ( HasSpawnFlags(SF_TRIGGER_ALLOW_ALL) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_CLIENTS) && (pOther->GetFlags() & FL_CLIENT)) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_NPCS) && (pOther->GetFlags() & FL_NPC)) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_PUSHABLES) && FClassnameIs(pOther, "func_pushable")) ||
		(HasSpawnFlags(SF_TRIGGER_ALLOW_PHYSICS) && pOther->GetMoveType() == MOVETYPE_VPHYSICS))
	{
		bool bOtherIsPlayer = pOther->IsPlayer();
		if( HasSpawnFlags(SF_TRIGGER_ONLY_PLAYER_ALLY_NPCS) && !bOtherIsPlayer )
		{
#ifndef CLIENT_DLL
			CAI_BaseNPC *pNPC = pOther->MyNPCPointer();

			if( !pNPC || !pNPC->IsPlayerAlly() )
			{
				return false;
			}
#else
			return false;
#endif
		}

		if ( HasSpawnFlags(SF_TRIGGER_ONLY_CLIENTS_IN_VEHICLES) && bOtherIsPlayer )
		{
			if ( !((CBasePlayer*)pOther)->IsInAVehicle() )
				return false;
		}

		if ( HasSpawnFlags(SF_TRIGGER_ONLY_CLIENTS_OUT_OF_VEHICLES) && bOtherIsPlayer )
		{
			if ( ((CBasePlayer*)pOther)->IsInAVehicle() )
				return false;
		}

		CBaseFilter *pFilter = m_hFilter.Get();
		return (!pFilter) ? true : pFilter->PassesFilter( this, pOther );
	}
	return false;
}
	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseVPhysicsTrigger::StartTouch( CBaseEntity *pOther )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseVPhysicsTrigger::EndTouch( CBaseEntity *pOther )
{
}

//-----------------------------------------------------------------------------
// Purpose: Disables auto movement on players that touch it
//-----------------------------------------------------------------------------
class CTriggerPlayerMovement : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerPlayerMovement, CBaseTrigger );
	DECLARE_NETWORKCLASS();
public:

	virtual bool IsPredicted( void ) OVERRIDE { return true; }
	void Spawn( void );
	void StartTouch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
};

IMPLEMENT_NETWORKCLASS_ALIASED( TriggerPlayerMovement, DT_TriggerPlayerMovement )
BEGIN_NETWORK_TABLE( CTriggerPlayerMovement, DT_TriggerPlayerMovement )
END_NETWORK_TABLE()
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTriggerPlayerMovement )

END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_playermovement, CTriggerPlayerMovement );
#endif

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CTriggerPlayerMovement::Spawn( void )
{
#ifndef CLIENT_DLL
	if( HasSpawnFlags( SF_TRIGGER_ONLY_PLAYER_ALLY_NPCS ) )
	{
		// @Note (toml 01-07-04): fix up spawn flag collision coding error. Remove at some point once all maps fixed up please!
		DevMsg("*** trigger_playermovement using obsolete spawnflag. Remove and reset with new value for \"Disable auto player movement\"\n" );
		RemoveSpawnFlags(SF_TRIGGER_ONLY_PLAYER_ALLY_NPCS);
		AddSpawnFlags(SF_TRIGGER_MOVE_AUTODISABLE);
	}
#endif
	BaseClass::Spawn();
#ifndef CLIENT_DLL
	InitTrigger();
#endif
}


// UNDONE: This will not support a player touching more than one of these
// UNDONE: Do we care?  If so, ref count automovement in the player?
void CTriggerPlayerMovement::StartTouch( CBaseEntity *pOther )
{	
	if (!PassesTriggerFilters(pOther))
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pOther );

	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->m_bForceDuckedByTriggerPlayerMove = true;
		pPlayer->ForceButtons( IN_DUCK );
	}
	
	if ( HasSpawnFlags( SF_TRIGGER_AUTO_WALK ) )
	{
		pPlayer->ForceButtons( IN_SPEED );
	}

	// UNDONE: Currently this is the only operation this trigger can do
	if ( HasSpawnFlags(SF_TRIGGER_MOVE_AUTODISABLE) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = false;
	}
}

void CTriggerPlayerMovement::EndTouch( CBaseEntity *pOther )
{
	if (!PassesTriggerFilters(pOther))
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pOther );

	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->m_bForceDuckedByTriggerPlayerMove = false;
		pPlayer->UnforceButtons( IN_DUCK );
	}
	
	if ( HasSpawnFlags( SF_TRIGGER_AUTO_WALK ) )
	{
		pPlayer->UnforceButtons( IN_SPEED );
	}

	if ( HasSpawnFlags(SF_TRIGGER_MOVE_AUTODISABLE) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: A trigger that pushes the player, NPCs, or objects.
//-----------------------------------------------------------------------------
class CTriggerPush : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerPush, CBaseTrigger );
	DECLARE_NETWORKCLASS();
	
	virtual bool IsPredicted( void ) OVERRIDE { return true; }

#ifndef CLIENT_DLL
	void Spawn( void );
	void Activate( void );
#endif
	void Touch( CBaseEntity *pOther );
	void Untouch( CBaseEntity *pOther );

	CNetworkVector( m_vecPushDir );

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
	float m_flAlternateTicksFix; // Scale factor to apply to the push speed when running with alternate ticks
#endif
	CNetworkVar( float, m_flPushSpeed );
};

IMPLEMENT_NETWORKCLASS_ALIASED( TriggerPush, DT_TriggerPush )
BEGIN_NETWORK_TABLE( CTriggerPush, DT_TriggerPush )
#ifdef GAME_DLL
SendPropVector( SENDINFO( m_vecPushDir ) ),
SendPropFloat( SENDINFO( m_flPushSpeed ) ),
#else
RecvPropVector( RECVINFO( m_vecPushDir ) ),
RecvPropFloat( RECVINFO( m_flPushSpeed ) ),
#endif
END_NETWORK_TABLE()

#ifndef CLIENT_DLL
BEGIN_DATADESC( CTriggerPush )
	DEFINE_KEYFIELD( m_vecPushDir, FIELD_VECTOR, "pushdir" ),
	DEFINE_KEYFIELD( m_flAlternateTicksFix, FIELD_FLOAT, "alternateticksfix" ),
	//DEFINE_FIELD( m_flPushSpeed, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_push, CTriggerPush );
#

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CTriggerPush::Spawn()
{
	// Convert pushdir from angles to a vector
	Vector vecAbsDir;
	QAngle angPushDir = QAngle(m_vecPushDir.GetX(), m_vecPushDir.GetY(), m_vecPushDir.GetZ());
	AngleVectors(angPushDir, &vecAbsDir);

	// Transform the vector into entity space
	VectorIRotate( vecAbsDir, EntityToWorldTransform(), m_vecPushDir.GetForModify() );

	BaseClass::Spawn();

	InitTrigger();

	if (m_flSpeed == 0)
	{
		m_flSpeed = 100;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTriggerPush::Activate()
{
	// Fix problems with triggers pushing too hard under sv_alternateticks.
	// This is somewhat hacky, but it's simple and we're really close to shipping.
	ConVarRef sv_alternateticks( "sv_alternateticks" );
	if ( ( m_flAlternateTicksFix != 0 ) && sv_alternateticks.GetBool() )
	{
		m_flPushSpeed = m_flSpeed * m_flAlternateTicksFix;
	}
	else
	{
		m_flPushSpeed = m_flSpeed;
	}
	
	BaseClass::Activate();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CTriggerPush::Touch( CBaseEntity *pOther )
{
	if ( !pOther->IsSolid() || (pOther->GetMoveType() == MOVETYPE_PUSH || pOther->GetMoveType() == MOVETYPE_NONE ) )
		return;

	if (!PassesTriggerFilters(pOther))
		return;

	// FIXME: If something is hierarchically attached, should we try to push the parent?
	if (pOther->GetMoveParent())
		return;

	// Transform the push dir into global space
	Vector vecAbsDir;
	VectorRotate( m_vecPushDir, EntityToWorldTransform(), vecAbsDir );

	// Instant trigger, just transfer velocity and remove
	if (HasSpawnFlags(SF_TRIG_PUSH_ONCE))
	{
#ifndef CLIENT_DLL // FIXME: We need to find a way to predict this
		pOther->ApplyAbsVelocityImpulse( m_flPushSpeed * vecAbsDir );

		if ( vecAbsDir.z > 0 )
		{
			pOther->SetGroundEntity( NULL );
		}
		UTIL_Remove( this );
#endif
		return;
	}

	switch( pOther->GetMoveType() )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_NOCLIP:
		break;

	case MOVETYPE_VPHYSICS:
		{
			IPhysicsObject *pPhys = pOther->VPhysicsGetObject();
			if ( pPhys )
			{
				// UNDONE: Assume the velocity is for a 100kg object, scale with mass
				pPhys->ApplyForceCenter( m_flPushSpeed * vecAbsDir * 100.0f * gpGlobals->frametime );
				return;
			}
		}
		break;

	default:
		{
#if defined( HL2_DLL ) || defined ( HL2_CLIENT_DLL )
			// HACK HACK  HL2 players on ladders will only be disengaged if the sf is set, otherwise no push occurs.
			if ( pOther->IsPlayer() && 
				 pOther->GetMoveType() == MOVETYPE_LADDER )
			{
				if ( !HasSpawnFlags(SF_TRIG_PUSH_AFFECT_PLAYER_ON_LADDER) )
				{
					// Ignore the push
					return;
				}
			}
#endif

			Vector vecPush = (m_flPushSpeed * vecAbsDir);
			if ( ( pOther->GetFlags() & FL_BASEVELOCITY )
#ifndef CLIENT_DLL
				&& !lagcompensation->IsCurrentlyDoingLagCompensation()
#endif
				)
			{
				vecPush = vecPush + pOther->GetBaseVelocity();
			}
			if ( vecPush.z > 0 && (pOther->GetFlags() & FL_ONGROUND) )
			{
				pOther->SetGroundEntity( NULL );
				Vector origin = pOther->GetAbsOrigin();
				origin.z += 1.0f;
				pOther->SetAbsOrigin( origin );
			}

#if defined ( HL1_DLL ) || defined ( HL1_CLIENT_DLL )
			// Apply the z velocity as a force so it counteracts gravity properly
			Vector vecImpulse( 0, 0, vecPush.z * 0.025 );//magic hack number

			pOther->ApplyAbsVelocityImpulse( vecImpulse );

			// apply x, y as a base velocity so we travel at constant speed on conveyors
			vecPush.z = 0;
#endif			

			pOther->SetBaseVelocity( vecPush );
			pOther->AddFlag( FL_BASEVELOCITY );
		}
		break;
	}
}