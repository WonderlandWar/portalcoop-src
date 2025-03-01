//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Special handling for Portal usable ladders
//
//=============================================================================//
#include "cbase.h"
#include "hl_gamemovement.h"
#include "in_buttons.h"
#include "utlrbtree.h"
#include "movevars_shared.h"
#include "portal_shareddefs.h"
#include "portal_collideable_enumerator.h"
#include "prop_portal_shared.h"
#include "rumble_shared.h"
#include "debugoverlay_shared.h"
#include "coordsize.h"
#include "collisionutils.h"

#if defined( CLIENT_DLL )
	#include "c_portal_player.h"
	#include "c_rumble.h"
	#include "prediction.h"
	#include "c_recipientfilter.h"
	#include "c_basetoggle.h"
#else
	#include "portal_player.h"
	#include "env_player_surface_trigger.h"
	#include "portal_gamestats.h"
	#include "physicsshadowclone.h"
	#include "recipientfilter.h"
	#include "SoundEmitterSystem/isoundemittersystembase.h"
#endif

#include "portal/weapon_physcannon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
extern ConVar cl_player_funnel_into_portals;
#endif

ConVar sv_player_trace_through_portals("sv_player_trace_through_portals", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Causes player movement traces to trace through portals." );
ConVar sv_player_funnel_gimme_dot("sv_player_funnel_gimme_dot", "0.9", FCVAR_REPLICATED);

ConVar portal_player_interaction_quadtest_epsilon( "portal_player_interaction_quadtest_epsilon", "-0.03125", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar pcoop_avoidplayers( "pcoop_avoidplayers", "1", FCVAR_REPLICATED ); 

ConVar sv_portal_new_player_trace( "sv_portal_new_player_trace", "1", FCVAR_REPLICATED | FCVAR_CHEAT );

#if defined( CLIENT_DLL )
ConVar cl_vertical_elevator_fix( "cl_vertical_elevator_fix", "1" );
#endif

class CReservePlayerSpot;

#define COORD_FRACTIONAL_BITS		5
#define COORD_DENOMINATOR			(1<<(COORD_FRACTIONAL_BITS))
#define COORD_RESOLUTION			(1.0/(COORD_DENOMINATOR))

#define	NUM_CROUCH_HINTS	3

#define PORTAL_FUNNEL_AMOUNT 6.0f

extern bool g_bAllowForcePortalTrace;
extern bool g_bForcePortalTrace;

static inline CBaseEntity *TranslateGroundEntity( CBaseEntity *pGroundEntity )
{
#ifndef CLIENT_DLL
	CPhysicsShadowClone *pClone = NULL;

	if ( CPhysicsShadowClone::IsShadowClone( pGroundEntity ) )
		static_cast<CPhysicsShadowClone *>(pGroundEntity);

	if( pClone && pClone->IsUntransformedClone() )
	{
		CBaseEntity *pSource = pClone->GetClonedEntity();

		if( pSource )
			return pSource;
	}
#endif //#ifndef CLIENT_DLL

	return pGroundEntity;
}

class CPortalGameMovementFilter : public CTraceFilterSimple
{
public:
	
	CPortalGameMovementFilter( const IHandleEntity *passentity, int collisionGroup, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL ) :
		CTraceFilterSimple( passentity, collisionGroup, pExtraShouldHitCheckFn )
	{

	}	

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
#if defined ( CLIENT_DLL ) && 0
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity )
		{
			IPhysicsObject *pObj = pEntity->VPhysicsGetObject(); // If it's held, don't collide
			if ( pObj && (pObj->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
			{
				return false;
			}
		}
#endif
		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

#define PORTAL_SLIDE_DEBUG
//-----------------------------------------------------------------------------
// Purpose: Portal specific movement code
//-----------------------------------------------------------------------------
class CPortalGameMovement : public CGameMovement
{
	typedef CGameMovement BaseClass;
public:
#if 1
	CPortalGameMovement();

// Overrides
	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual bool CheckJumpButton( void );

	void FunnelIntoPortal( CProp_Portal *pPortal, Vector &wishdir );

	virtual void AirAccelerate( Vector& wishdir, float wishspeed, float accel );
	virtual void AirMove( void );

	virtual void PlayerRoughLandingEffects( float fvol );

	virtual void CategorizePosition( void );

	// Traces the player bbox as it is swept from start to end
	virtual void TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );
	void TracePlayerBBoxForGround2( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );

	// Tests the player position
	virtual CBaseHandle	TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm );

	virtual int CheckStuck( void );

	virtual void SetGroundEntity( trace_t *pm );

	void HandlePortallingLegacy( void ); // This is Portal 1 code but ported to gamemovement partially mixed with Portal 2 code

private:


	CPortal_Player	*GetPortalPlayer();
	
#if defined( CLIENT_DLL )
	void ClientVerticalElevatorFixes();
#endif

	Vector m_vMoveStartPosition; //where the player started before the movement code ran
	Vector m_vVelocityStart;
#endif
};
#if 1
#if defined( CLIENT_DLL )
void CPortalGameMovement::ClientVerticalElevatorFixes()
{
	//find root move parent of our ground entity
	CBaseEntity *pRootMoveParent = player->GetGroundEntity();
	while( pRootMoveParent )
	{
		C_BaseEntity *pTestParent = pRootMoveParent->GetMoveParent();
		if( !pTestParent )
			break;

		pRootMoveParent = pTestParent;
	}

	//if it's a C_BaseToggle (func_movelinear / func_door) then enable prediction if it chooses to
	bool bRootMoveParentIsLinearMovingBaseToggle = false;
	bool bAdjustedRootZ = false;
	if( pRootMoveParent && !pRootMoveParent->IsWorld() )
	{
		C_BaseToggle *pPredictableGroundEntity = dynamic_cast<C_BaseToggle *>(pRootMoveParent);
		if( pPredictableGroundEntity && (pPredictableGroundEntity->m_movementType == MOVE_TOGGLE_LINEAR) )
		{
			bRootMoveParentIsLinearMovingBaseToggle = true;
			if( !pPredictableGroundEntity->GetPredictable() )
			{
				pPredictableGroundEntity->SetPredictionEligible( true );
				pPredictableGroundEntity->m_hPredictionOwner = player;
			}
			else if( cl_vertical_elevator_fix.GetBool() )
			{
				Vector vNewOrigin = pPredictableGroundEntity->PredictPosition( player->PredictedServerTime() + TICK_INTERVAL );
				if( (vNewOrigin - pPredictableGroundEntity->GetLocalOrigin()).LengthSqr() > 0.01f )
				{
					bAdjustedRootZ = (vNewOrigin.z != pPredictableGroundEntity->GetLocalOrigin().z);
					pPredictableGroundEntity->SetLocalOrigin( vNewOrigin );

					//invalidate abs transforms for upcoming traces
					C_BaseEntity *pParent = player->GetGroundEntity();
					while( pParent )
					{
						pParent->AddEFlags( EFL_DIRTY_ABSTRANSFORM );
						pParent = pParent->GetMoveParent();
					}
				}
			}
		}
	}

	//re-seat player on vertical elevators
	if( bRootMoveParentIsLinearMovingBaseToggle && 
		cl_vertical_elevator_fix.GetBool() && 
		bAdjustedRootZ )
	{
		trace_t trElevator;
		TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin() - Vector( 0.0f, 0.0f, GetPlayerMaxs().z ), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trElevator );

		if( trElevator.startsolid )
		{
			//started in solid, and we think it's an elevator. Pop up the player if at all possible

			//trace up, ignoring the ground entity hierarchy
			Ray_t playerRay;
			playerRay.Init( mv->GetAbsOrigin(), mv->GetAbsOrigin() + Vector( 0.0f, 0.0f, GetPlayerMaxs().z ), GetPlayerMins(), GetPlayerMaxs() );

			CTraceFilterSimpleList ignoreGroundEntityHeirarchy( COLLISION_GROUP_PLAYER_MOVEMENT );
			{
				ignoreGroundEntityHeirarchy.AddEntityToIgnore( player );
				C_BaseEntity *pParent = player->GetGroundEntity();
				while( pParent )
				{
					ignoreGroundEntityHeirarchy.AddEntityToIgnore( pParent );
					pParent = pParent->GetMoveParent();
				}
			}			

			enginetrace->TraceRay( playerRay, MASK_PLAYERSOLID, &ignoreGroundEntityHeirarchy, &trElevator );
			if( !trElevator.startsolid ) //success
			{
				//now trace back down
				Vector vStart = trElevator.endpos;
				TracePlayerBBox( vStart, mv->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trElevator );
				if( !trElevator.startsolid &&
					(trElevator.m_pEnt == player->GetGroundEntity()) )
				{
					//if we landed back on the ground entity, call it good
					mv->SetAbsOrigin( trElevator.endpos );
					player->SetNetworkOrigin( trElevator.endpos ); //paint code loads from network origin after handling paint powers
				}
			}
		}
		else if( (trElevator.endpos.z < mv->GetAbsOrigin().z) && (trElevator.m_pEnt == player->GetGroundEntity()) )
		{
			//re-seat on ground entity
			mv->SetAbsOrigin( trElevator.endpos );
			player->SetNetworkOrigin( trElevator.endpos ); //paint code loads from network origin after handling paint powers
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPortalGameMovement::CPortalGameMovement()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CPortal_Player	*CPortalGameMovement::GetPortalPlayer()
{
	return static_cast< CPortal_Player * >( player );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMove - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	Assert( pMove && pPlayer );
	
	float flStoreFrametime = gpGlobals->frametime;

	player = pPlayer;
	mv = pMove;
	
#if defined( CLIENT_DLL )
	ClientVerticalElevatorFixes(); //fixup vertical elevator discrepancies between client and server as best we can
#endif
	
	m_vMoveStartPosition = mv->GetAbsOrigin();
	m_vVelocityStart = mv->m_vecVelocity;

	//!!HACK HACK: Adrian - slow down all player movement by this factor.
	//!!Blame Yahn for this one.
	gpGlobals->frametime *= pPlayer->GetLaggedMovementValue();

	ResetGetPointContentsCache();

	// Cropping movement speed scales mv->m_fForwardSpeed etc. globally
	// Once we crop, we don't want to recursively crop again, so we set the crop
	//  flag globally here once per usercmd cycle.
	m_iSpeedCropped = SPEED_CROPPED_RESET;

	mv->m_flMaxSpeed = sv_maxspeed.GetFloat();
	
	bool bInPortalEnv = (((CPortal_Player *)pPlayer)->m_hPortalEnvironment != NULL);

	g_bAllowForcePortalTrace = bInPortalEnv;
	g_bForcePortalTrace = bInPortalEnv;
	
	// Run the command.
	PlayerMove();
	HandlePortallingLegacy();
	FinishMove();

	g_bAllowForcePortalTrace = false;
	g_bForcePortalTrace = false;
	//if ( !pPlayer->m_bForceDuckedByTriggerPlayerMove )
	//{
		pPlayer->UnforceButtons( IN_DUCK );
	//}
	pPlayer->UnforceButtons( IN_JUMP );

	//This is probably not needed, but just in case.
	gpGlobals->frametime = flStoreFrametime;
}

//-----------------------------------------------------------------------------
// Purpose: Base jump behavior, plus an anim event
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPortalGameMovement::CheckJumpButton()
{
	if ( BaseClass::CheckJumpButton() )
	{
#ifdef CLIENT_DLL
		if ( prediction->IsFirstTimePredicted() )
#endif
		GetPortalPlayer()->DoAnimationEvent( PLAYERANIMEVENT_JUMP, 0 );
		return true;
	}

	return false;
}

void CPortalGameMovement::FunnelIntoPortal( CProp_Portal *pPortal, Vector &wishdir )
{
	// Make sure there's a portal
	if ( !pPortal )
		return;

	// Get portal vectors
	Vector vPortalForward, vPortalRight, vPortalUp;
	pPortal->GetVectors( &vPortalForward, &vPortalRight, &vPortalUp );

	// Make sure it's a floor portal
	if ( vPortalForward.z < 0.8f )
		return;

	vPortalRight.z = 0.0f;
	vPortalUp.z = 0.0f;
	VectorNormalize( vPortalRight );
	VectorNormalize( vPortalUp );

	// Make sure the player is looking downward
	CPortal_Player *pPlayer = GetPortalPlayer();

	Vector vPlayerForward;
	pPlayer->EyeVectors( &vPlayerForward );

	if ( vPlayerForward.z > -0.1f )
		return;

	Vector vPlayerOrigin = pPlayer->GetAbsOrigin();
	Vector vPlayerToPortal = pPortal->GetAbsOrigin() - vPlayerOrigin;

	// Make sure the player is trying to air control, they're falling downward and they are vertically close to the portal
	if ( fabsf( wishdir[ 0 ] ) > 64.0f || fabsf( wishdir[ 1 ] ) > 64.0f || mv->m_vecVelocity[ 2 ] > -165.0f || vPlayerToPortal.z < -512.0f )
		return;

	// Make sure we're in the 2D portal rectangle
	if ( ( vPlayerToPortal.Dot( vPortalRight ) * vPortalRight ).Length() > PORTAL_HALF_WIDTH * 1.5f )
		return;
	if ( ( vPlayerToPortal.Dot( vPortalUp ) * vPortalUp ).Length() > PORTAL_HALF_HEIGHT * 1.5f )
		return;

	if ( vPlayerToPortal.z > -8.0f )
	{
		// We're too close the the portal to continue correcting, but zero the velocity so our fling velocity is nice
		mv->m_vecVelocity[ 0 ] = 0.0f;
		mv->m_vecVelocity[ 1 ] = 0.0f;
	}
	else
	{
		// Funnel toward the portal
		float fFunnelX = vPlayerToPortal.x * PORTAL_FUNNEL_AMOUNT - mv->m_vecVelocity[ 0 ];
		float fFunnelY = vPlayerToPortal.y * PORTAL_FUNNEL_AMOUNT - mv->m_vecVelocity[ 1 ];

		wishdir[ 0 ] += fFunnelX;
		wishdir[ 1 ] += fFunnelY;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wishdir - 
//			accel - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::AirAccelerate( Vector& wishdir, float wishspeed, float accel )
{
	int i;
	float addspeed, accelspeed, currentspeed;
	float wishspd;

	wishspd = wishspeed;

	if (player->pl.deadflag)
		return;

	if (player->m_flWaterJumpTime)
		return;

	// Cap speed
	if (wishspd > 60.0f)
		wishspd = 60.0f;

	// Determine veer amount
	currentspeed = mv->m_vecVelocity.Dot(wishdir);

	// See how much to add
	addspeed = wishspd - currentspeed;

	// If not adding any, done.
	if (addspeed <= 0)
		return;

	// Determine acceleration speed after acceleration
	accelspeed = accel * wishspeed * gpGlobals->frametime * player->m_surfaceFriction;

	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust pmove vel.
	for (i=0 ; i<3 ; i++)
	{
		mv->m_vecVelocity[i] += accelspeed * wishdir[i];
		mv->m_outWishVel[i] += accelspeed * wishdir[i];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::AirMove( void )
{
	int			i;
	Vector		wishvel;
	float		fmove, smove;
	Vector		wishdir;
	float		wishspeed;
	Vector forward, right, up;

	AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;

	// Zero out z components of movement vectors
	forward[2] = 0;
	right[2]   = 0;
	VectorNormalize(forward);  // Normalize remainder of vectors
	VectorNormalize(right);    // 

	for (i=0 ; i<2 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	wishvel[2] = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move

	//
	// Don't let the player screw their fling because of adjusting into a floor portal
	//
	if ( mv->m_vecVelocity[ 0 ] * mv->m_vecVelocity[ 0 ] + mv->m_vecVelocity[ 1 ] * mv->m_vecVelocity[ 1 ] > MIN_FLING_SPEED * MIN_FLING_SPEED )
	{
		if ( mv->m_vecVelocity[ 0 ] > MIN_FLING_SPEED * 0.5f && wishdir[ 0 ] < 0.0f )
			wishdir[ 0 ] = 0.0f;
		else if ( mv->m_vecVelocity[ 0 ] < -MIN_FLING_SPEED * 0.5f && wishdir[ 0 ] > 0.0f )
			wishdir[ 0 ] = 0.0f;

		if ( mv->m_vecVelocity[ 1 ] > MIN_FLING_SPEED * 0.5f && wishdir[ 1 ] < 0.0f )
			wishdir[ 1 ] = 0.0f;
		else if ( mv->m_vecVelocity[ 1 ] < -MIN_FLING_SPEED * 0.5f && wishdir[ 1 ] > 0.0f )
			wishdir[ 1 ] = 0.0f;
	}

	//
	// Try to autocorrect the player to fall into the middle of the portal
	//
#ifdef GAME_DLL
	else if ( GetPortalPlayer()->m_bPortalFunnel )
#else
	else if ( cl_player_funnel_into_portals.GetBool() )
#endif
	{
		int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
		if( iPortalCount != 0 )
		{
			CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
			for( int i = 0; i != iPortalCount; ++i )
			{
				CProp_Portal *pTempPortal = pPortals[i];
				if( pTempPortal->IsActivedAndLinked() )
				{
					FunnelIntoPortal( pTempPortal, wishdir );
				}
			}
		}
	}

	wishspeed = VectorNormalize(wishdir);

	//
	// clamp to server defined max speed
	//
	if ( wishspeed != 0 && (wishspeed > mv->m_flMaxSpeed))
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	AirAccelerate( wishdir, wishspeed, 15.0f );

	// Add in any base velocity to the current velocity.
	VectorAdd(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	TryPlayerMove();

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
}

void CPortalGameMovement::PlayerRoughLandingEffects( float fvol )
{
	BaseClass::PlayerRoughLandingEffects( fvol );
#ifdef CLIENT_DLL
	Assert( prediction->InPrediction() );

	if ( !prediction->IsFirstTimePredicted() )
		return;
#endif
	
	if ( fvol >= 1.0 )
	{
		// Play the future shoes sound
#ifdef GAME_DLL
		CRecipientFilter filter;
		filter.AddRecipientsByPAS( player->GetAbsOrigin() );
		filter.RemoveRecipient( player );
#else
		C_RecipientFilter filter;
		filter.AddRecipient( player );
#endif
		//filter.UsePredictionRules();
		CSoundParameters params;
		if ( CBaseEntity::GetParametersForSound( "PortalPlayer.FallRecover", params, NULL ) )
		{
			EmitSound_t ep( params );
			ep.m_nPitch = 125.0f - player->m_Local.m_flFallVelocity * 0.03f;					// lower pitch the harder they land
			ep.m_flVolume = MIN( player->m_Local.m_flFallVelocity * 0.00075f - 0.38, 1.0f );	// louder the harder they land

			player->EmitSound( filter, player->entindex(), ep );
		}
	}
}

void CPortalGameMovement::TracePlayerBBoxForGround2( const Vector& start, const Vector& end, unsigned int fMask,int collisionGroup, trace_t& pm )
{
	VPROF( "TracePlayerBBoxForGround" );

	CPortal_Player *pPortalPlayer = static_cast<CPortal_Player *>(player);
	CProp_Portal *pPlayerPortal = pPortalPlayer->m_hPortalEnvironment;

#ifndef CLIENT_DLL
	if( pPlayerPortal && pPlayerPortal->m_PortalSimulator.IsReadyToSimulate() == false )
		pPlayerPortal = NULL;
#endif

	pPlayerPortal = NULL;

	Ray_t ray;
	Vector mins, maxs;
	Vector minsSrc = GetPlayerMins();
	Vector maxsSrc = GetPlayerMaxs();

	float fraction = pm.fraction;
	Vector endpos = pm.endpos;

	// Check the -x, -y quadrant
	mins = minsSrc;
	maxs.Init( MIN( 0, maxsSrc.x ), MIN( 0, maxsSrc.y ), maxsSrc.z );
	TracePlayerBBox( start, end, fMask, collisionGroup, pm );
	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the +x, +y quadrant
	mins.Init( MAX( 0, minsSrc.x ), MAX( 0, minsSrc.y ), minsSrc.z );
	maxs = maxsSrc;
	TracePlayerBBox( start, end, fMask, collisionGroup, pm );
	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the -x, +y quadrant
	mins.Init( minsSrc.x, MAX( 0, minsSrc.y ), minsSrc.z );
	maxs.Init( MIN( 0, maxsSrc.x ), maxsSrc.y, maxsSrc.z );
	TracePlayerBBox( start, end, fMask, collisionGroup, pm );
	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the +x, -y quadrant
	mins.Init( MAX( 0, minsSrc.x ), minsSrc.y, minsSrc.z );
	maxs.Init( maxsSrc.x, MIN( 0, maxsSrc.y ), maxsSrc.z );
	TracePlayerBBox( start, end, fMask, collisionGroup, pm );
	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	pm.fraction = fraction;
	pm.endpos = endpos;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &input - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::CategorizePosition( void )
{
	Vector point;
	trace_t pm;

	// Reset this each time we-recategorize, otherwise we have bogus friction when we jump into water and plunge downward really quickly
	player->m_surfaceFriction = 1.0f;

	// if the player hull point one unit down is solid, the player
	// is on ground
	
	// see if standing on something solid	

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	CheckWater();

	// observers don't have a ground entity
	if ( player->IsObserver() )
		return;

	float flOffset = 2.0f;

	point[0] = mv->GetAbsOrigin()[0];
	point[1] = mv->GetAbsOrigin()[1];
	point[2] = mv->GetAbsOrigin()[2] - flOffset;

	Vector bumpOrigin;
	bumpOrigin = mv->GetAbsOrigin();

	// Shooting up really fast.  Definitely not on ground.
	// On ladder moving up, so not on ground either
	// NOTE: 145 is a jump.
#define NON_JUMP_VELOCITY 140.0f

	float zvel = mv->m_vecVelocity[2];
	bool bMovingUp = zvel > 0.0f;
	bool bMovingUpRapidly = zvel > NON_JUMP_VELOCITY;
	float flGroundEntityVelZ = 0.0f;
	if ( bMovingUpRapidly )
	{
		// Tracker 73219, 75878:  ywb 8/2/07
		// After save/restore (and maybe at other times), we can get a case where we were saved on a lift and 
		//  after restore we'll have a high local velocity due to the lift making our abs velocity appear high.  
		// We need to account for standing on a moving ground object in that case in order to determine if we really 
		//  are moving away from the object we are standing on at too rapid a speed.  Note that CheckJump already sets
		//  ground entity to NULL, so this wouldn't have any effect unless we are moving up rapidly not from the jump button.
		CBaseEntity *ground = player->GetGroundEntity();
		if ( ground )
		{
			flGroundEntityVelZ = ground->GetAbsVelocity().z;
			bMovingUpRapidly = ( zvel - flGroundEntityVelZ ) > NON_JUMP_VELOCITY;
		}
	}

	// Was on ground, but now suddenly am not
	if ( bMovingUpRapidly || 
		( bMovingUp && player->GetMoveType() == MOVETYPE_LADDER ) )   
	{
		SetGroundEntity( NULL );
	}
	else
	{
		// Try and move down.
		TracePlayerBBox( bumpOrigin, point, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );
		
		// Was on ground, but now suddenly am not.  If we hit a steep plane, we are not on ground
		if ( !pm.m_pEnt || pm.plane.normal[2] < 0.7 )
		{
			// Test four sub-boxes, to see if any of them would have found shallower slope we could actually stand on
			TracePlayerBBoxForGround2( bumpOrigin, point, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

			if ( !pm.m_pEnt || pm.plane.normal[2] < 0.7 )
			{
				SetGroundEntity( NULL );
				// probably want to add a check for a +z velocity too!
				if ( ( mv->m_vecVelocity.z > 0.0f ) && 
					( player->GetMoveType() != MOVETYPE_NOCLIP ) )
				{
					player->m_surfaceFriction = 0.25f;
				}
			}
			else
			{
				SetGroundEntity( &pm );
			}
		}
		else
		{
			SetGroundEntity( &pm );  // Otherwise, point to index of ent under us.
		}

#ifndef CLIENT_DLL		
		//Adrian: vehicle code handles for us.
		if ( player->IsInAVehicle() == false )
		{
			// If our gamematerial has changed, tell any player surface triggers that are watching
			IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
			surfacedata_t *pSurfaceProp = physprops->GetSurfaceData( pm.surface.surfaceProps );
			char cCurrGameMaterial = pSurfaceProp->game.material;
			if ( !player->GetGroundEntity() )
			{
				cCurrGameMaterial = 0;
			}

			// Changed?
			if ( player->m_chPreviousTextureType != cCurrGameMaterial )
			{
				CEnvPlayerSurfaceTrigger::SetPlayerSurface( player, cCurrGameMaterial );
			}

			player->m_chPreviousTextureType = cCurrGameMaterial;
		}
#endif
	}
}

int CPortalGameMovement::CheckStuck( void )
{
	if( BaseClass::CheckStuck() )
	{
		CPortal_Player *pPortalPlayer = GetPortalPlayer();

#ifndef CLIENT_DLL
		if( pPortalPlayer->IsAlive() )
			g_PortalGameStats.Event_PlayerStuck( pPortalPlayer );
#endif

		//try to fix it, then recheck
		Vector vIndecisive;
		if( pPortalPlayer->m_hPortalEnvironment )
		{
			pPortalPlayer->m_hPortalEnvironment->GetVectors( &vIndecisive, NULL, NULL );
		}
		else
		{
			vIndecisive.Init( 0.0f, 0.0f, 1.0f );
		}
		Vector ptOldOrigin = pPortalPlayer->GetAbsOrigin();

		if( pPortalPlayer->m_hPortalEnvironment )
		{
			if( !FindClosestPassableSpace( pPortalPlayer, vIndecisive ) )
			{
#ifndef CLIENT_DLL
				DevMsg( "Hurting the player for FindClosestPassableSpaceFailure!" );

				CTakeDamageInfo info( pPortalPlayer, pPortalPlayer, vec3_origin, vec3_origin, 1e10, DMG_CRUSH );
				pPortalPlayer->OnTakeDamage( info );
#endif
			}

			//make sure we didn't get put behind the portal >_<
			Vector ptCurrentOrigin = pPortalPlayer->GetAbsOrigin();
			if( vIndecisive.Dot( ptCurrentOrigin - ptOldOrigin ) < 0.0f )
			{
				pPortalPlayer->SetAbsOrigin( ptOldOrigin + (vIndecisive * 5.0f) ); //this is an anti-bug hack, since this would have probably popped them out of the world, we're just going to move them forward a few units
			}
		}

		mv->SetAbsOrigin( pPortalPlayer->GetAbsOrigin() );
		return BaseClass::CheckStuck();
	}
	else
	{
		return 0;
	}
}

void CPortalGameMovement::SetGroundEntity( trace_t *pm )
{
#ifndef CLIENT_DLL
	if ( !player->GetGroundEntity() && pm && pm->m_pEnt )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_player_touchedground" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}
#endif

	BaseClass::SetGroundEntity( pm );
}

ConVar sv_portal_new_player_trace_vs_remote_ents( "sv_portal_new_player_trace_vs_remote_ents", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

#define DEBUG_PORTAL_TRACE_BOXES //uncomment to draw collision debug info near portals

#if defined( DEBUG_PORTAL_TRACE_BOXES )
ConVar sv_portal_new_trace_debugboxes( "sv_portal_new_trace_debugboxes", "0", FCVAR_REPLICATED );
#endif

void CPortalGameMovement::TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	VPROF( "CGameMovement::TracePlayerBBox" );
	
	CPortal_Player *pPortalPlayer = (CPortal_Player *)player;

	Ray_t ray;
	ray.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );

#ifdef CLIENT_DLL
	CPortalGameMovementFilter traceFilter( player, collisionGroup );
#else
	CPortalGameMovementFilter baseFilter( player, collisionGroup );
	CTraceFilterTranslateClones traceFilter( &baseFilter );
#endif

	UTIL_Portal_TraceRay_With( pPortalPlayer->m_hPortalEnvironment, ray, fMask, &traceFilter, &pm );

	// If we're moving through a portal and failed to hit anything with the above ray trace
	// Use UTIL_Portal_TraceEntity to test this movement through a portal and override the trace with the result
	if ( pm.fraction == 1.0f && UTIL_DidTraceTouchPortals( ray, pm ) && sv_player_trace_through_portals.GetBool() )
	{
		trace_t tempTrace;
		UTIL_Portal_TraceEntity( pPortalPlayer, start, end, fMask, &traceFilter, &tempTrace );

		if ( tempTrace.DidHit() && tempTrace.fraction < pm.fraction && !tempTrace.startsolid && !tempTrace.allsolid )
		{
			pm = tempTrace;
		}
	}
}

CBaseHandle CPortalGameMovement::TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm )
{
	TracePlayerBBox( pos, pos, MASK_PLAYERSOLID, collisionGroup, pm ); //hook into the existing portal special trace functionality

	//Ray_t ray;
	//ray.Init( pos, pos, GetPlayerMins(), GetPlayerMaxs() );
	//UTIL_TraceRay( ray, MASK_PLAYERSOLID, mv->m_nPlayerHandle.Get(), collisionGroup, &pm );
	if( pm.startsolid && pm.m_pEnt && (pm.contents & MASK_PLAYERSOLID) )
	{
#ifdef _DEBUG
		Warning( "The player got stuck on something. Break in portal_gamemovement.cpp on line " __LINE__AS_STRING " to investigate\n" ); //happens enough to just leave in a perma-debugger
		Assert( 0 );
		//now that you're here, go up and re-run the TracePlayerBBox() above
#endif
#if defined( TRACE_DEBUG_ENABLED )
		g_bTraceDebugging = true;
		TracePlayerBBox( pos, pos, MASK_PLAYERSOLID, collisionGroup, pm ); //hook into the existing portal special trace functionality
#endif
		return pm.m_pEnt->GetRefEHandle();
	}
#ifdef GAME_DLL
	else if ( pm.startsolid && pm.m_pEnt && CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pm.m_pEnt ) )
	{
		// Stuck in a portal environment object, so unstick them!
		CPortal_Player *pPortalPlayer = (CPortal_Player *)player;
		pPortalPlayer->SetStuckOnPortalCollisionObject();

		return INVALID_EHANDLE_INDEX;
	}
#endif
	else
	{	
		return INVALID_EHANDLE_INDEX;
	}
}

#define PLAYERPORTALDEBUGSPEW 1

void CPortalGameMovement::HandlePortallingLegacy( void )
{
	CPortal_Player *pPortalPlayer = GetPortalPlayer();
#if defined( CLIENT_DLL )	
	pPortalPlayer->UnrollPredictedTeleportations(player->m_pCurrentCommand->command_number);
#endif

	CProp_Portal *pPortalEnvironment = pPortalPlayer->m_hPortalEnvironment;
	Vector ptPrevPlayerCenter = m_vMoveStartPosition + ((GetPlayerMaxs() + GetPlayerMins()) * 0.5f);
	Vector ptPlayerCenter = mv->GetAbsOrigin() + ((GetPlayerMaxs() + GetPlayerMins()) * 0.5f);
	Vector vPlayerExtentsFromCenter = (GetPlayerMaxs() - GetPlayerMins()) * 0.5f;
	
	CProp_Portal *pPortal = NULL;
	{
		Ray_t ray;
		ray.Init( m_vMoveStartPosition, mv->GetAbsOrigin(), GetPlayerMins(), GetPlayerMaxs() );
		//ray.Init( mv->GetAbsOrigin(), mv->GetAbsOrigin(), GetPlayerMins(), GetPlayerMaxs() );
		
		int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
		CProp_Portal **pAllPortals = CProp_Portal_Shared::AllPortals.Base();

		float fMaxDistSquared = FLT_MAX;
		for( int i = 0; i != iPortalCount; ++i )
		{
			if( pAllPortals[i]->IsActivedAndLinked() )
			{
				trace_t tr;
				UTIL_ClearTrace( tr );
				if( pAllPortals[i]->TestCollision( ray, CONTENTS_SOLID, tr ) )
				{
					const PS_InternalData_t &portalSimulator = pAllPortals[i]->m_PortalSimulator.GetInternalData();
					if( portalSimulator.Placement.PortalPlane.m_Normal.Dot( ptPrevPlayerCenter ) <= portalSimulator.Placement.PortalPlane.m_Dist )
					{
						//old origin must be in front of the plane

						if( pAllPortals[i] != pPortalEnvironment ) //special exception if we were pushed past the plane but did not move past it
							continue;
					}

					float fCenterDist = portalSimulator.Placement.PortalPlane.m_Normal.Dot( ptPlayerCenter ) - portalSimulator.Placement.PortalPlane.m_Dist;
					if( fCenterDist < 0.0f )
					{
						//if new origin is behind the plane, require that the player center hover over the portal quad to be considered
						Vector vPortalPlayerOriginDiff = ptPlayerCenter - portalSimulator.Placement.ptCenter;
						vPortalPlayerOriginDiff -= vPortalPlayerOriginDiff.Dot( portalSimulator.Placement.PortalPlane.m_Normal ) * portalSimulator.Placement.PortalPlane.m_Normal;

						if( (fabs( vPortalPlayerOriginDiff.Dot( portalSimulator.Placement.vRight ) ) > PORTAL_HALF_WIDTH ) ||
							(fabs( vPortalPlayerOriginDiff.Dot( portalSimulator.Placement.vUp ) ) > PORTAL_HALF_HEIGHT) )
						{
							continue;
						}
					}
					else
					{
						//require that a line from the center of the player to their most-penetrating extent passes through the portal quad
						//Avoids case where you can butt up against a portal side on an angled panel
						Vector vTestExtent = ptPlayerCenter;
						vTestExtent.x -= Sign( portalSimulator.Placement.PortalPlane.m_Normal.x ) * vPlayerExtentsFromCenter.x;
						vTestExtent.y -= Sign( portalSimulator.Placement.PortalPlane.m_Normal.y ) * vPlayerExtentsFromCenter.y;
						vTestExtent.z -= Sign( portalSimulator.Placement.PortalPlane.m_Normal.z ) * vPlayerExtentsFromCenter.z;

						float fTestDist = portalSimulator.Placement.PortalPlane.m_Normal.Dot( vTestExtent ) - portalSimulator.Placement.PortalPlane.m_Dist;

						if( fTestDist < portal_player_interaction_quadtest_epsilon.GetFloat() )
						{
							float fTotalDist = fCenterDist - fTestDist;
							if( fTotalDist != 0.0f )
							{
								Vector vPlanePoint = (vTestExtent * (fCenterDist/fTotalDist)) - (ptPlayerCenter * (fTestDist/fTotalDist));
								Vector vPortalCenterToPlanePoint = vPlanePoint - portalSimulator.Placement.ptCenter;

								if( (fabs( vPortalCenterToPlanePoint.Dot( portalSimulator.Placement.vRight ) ) > PORTAL_HALF_WIDTH + 1.0f) ||
									(fabs( vPortalCenterToPlanePoint.Dot( portalSimulator.Placement.vUp ) ) > PORTAL_HALF_HEIGHT + 1.0f) )
								{
									continue;
								}
							}
						}
					}


					Vector vDiff = pAllPortals[i]->GetAbsOrigin() - ptPlayerCenter;
					float fDistSqr = vDiff.LengthSqr();
					if( fDistSqr < fMaxDistSquared )
					{
						pPortal = pAllPortals[i];
						fMaxDistSquared = fDistSqr;
					}
				}
			}
		}
	}
	
	if ( pPortalEnvironment != pPortal )
	{
		if ( pPortalEnvironment )
		{
			pPortalEnvironment->m_PortalSimulator.ReleaseOwnershipOfEntity( player );
		}
		if ( pPortal )
		{
			pPortalPlayer->m_hPortalEnvironment = pPortal;
			pPortal->m_PortalSimulator.TakeOwnershipOfEntity( player );
		}
	}

	if ( !pPortal ) // Found nothing, so don't handle teleportations
		return;

	// Check to see if we should teleport
	
	CPortalSimulator *pPortalSimulator = &pPortal->m_PortalSimulator;
	
	if( !pPortalSimulator->OwnsEntity( player ) ) //can't teleport an entity we don't own
	{
		return;
	}

	const PS_InternalData_t &LocalPortalDataAccess = pPortalSimulator->GetInternalData();

	if( m_vVelocityStart.Dot( LocalPortalDataAccess.Placement.vForward ) > 0.0f )
	{
		return;
	}

	bool bPastPortalHole = false;
	// Test for entity's center being past portal plane
	if(LocalPortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptPlayerCenter ) < LocalPortalDataAccess.Placement.PortalPlane.m_Dist)
	{
		//entity wants to go further into the plane
		if( pPortalSimulator->EntityIsInPortalHole( player ) )
		{
			bPastPortalHole = true;
		}
	}

	if ( !bPastPortalHole )
		return;

	// Now do the teleportation
	
	const PS_InternalData_t &RemotePortalDataAccess = pPortal->m_hLinkedPortal->m_PortalSimulator.GetInternalData();

	bool bForcedDuck = false;
	
	if( fabs( LocalPortalDataAccess.Placement.vForward.z ) > 0.0f )
	{
		//we may have to compensate for the fact that AABB's don't rotate ever
			
		float fAbsLocalZ = fabs( LocalPortalDataAccess.Placement.vForward.z );
		float fAbsRemoteZ = fabs( RemotePortalDataAccess.Placement.vForward.z );
			
		bForcedDuck = ( (fabs(fAbsLocalZ - 1.0f) < 0.01f) &&
							(fabs(fAbsRemoteZ - 1.0f) < 0.01f) );

		if( bForcedDuck )
			//(fabs( LocalPortalDataAccess.Placement.vForward.z + RemotePortalDataAccess.Placement.vForward.z ) < 0.01f) )
		{
			//portals are both aligned on the z axis, no need to shrink the player
				
		}
		else
		{
			//curl the player up into a little ball
			pPortalPlayer->SetGroundEntity( NULL ); // No need to use CGameMovement::SetGroundEntity in this case.

			if( !pPortalPlayer->IsDucked() )
			{
				pPortalPlayer->ForceDuckThisFrame( mv->GetAbsOrigin(), m_vVelocityStart );

				if( LocalPortalDataAccess.Placement.vForward.z > 0.0f )
					ptPlayerCenter.z -= 16.0f; //portal facing up, shrink downwards
				else
					ptPlayerCenter.z += 16.0f; //portal facing down, shrink upwards
			}
		}			
	}

	Vector ptNewOrigin;
	Vector vNewVelocity;

	ptNewOrigin = pPortal->m_matrixThisToLinked * ptPlayerCenter;
	ptNewOrigin += mv->GetAbsOrigin() - ptPlayerCenter;

#ifdef GAME_DLL
	//Msg( "SERVER Portal: %i ptNewOrigin: %f %f %f\n", pPortal->m_bIsPortal2 ? 2 : 1 , ptNewOrigin.x, ptNewOrigin.y, ptNewOrigin.z );
#else
	//Msg( "CLIENT Portal: %i ptNewOrigin: %f %f %f\n", pPortal->m_bIsPortal2 ? 2 : 1 , ptNewOrigin.x, ptNewOrigin.y, ptNewOrigin.z );
#endif

	// Reorient the velocity		
	vNewVelocity = pPortal->m_matrixThisToLinked.ApplyRotation( m_vVelocityStart );

	//help camera reorientation for the player
	{	
		Vector vPlayerForward;
		AngleVectors( mv->m_vecViewAngles, &vPlayerForward, NULL, NULL );

		float fPlayerForwardZ = vPlayerForward.z;
		vPlayerForward.z = 0.0f;

		float fForwardLength = vPlayerForward.Length();

		if ( fForwardLength > 0.0f )
		{
			VectorNormalize( vPlayerForward );
		}

		float fPlayerFaceDotPortalFace = LocalPortalDataAccess.Placement.vForward.Dot( vPlayerForward );
		float fPlayerFaceDotPortalUp = LocalPortalDataAccess.Placement.vUp.Dot( vPlayerForward );

		CBaseEntity *pHeldEntity = GetPlayerHeldEntity( player );

		// Sometimes reorienting by pitch is more desirable than by roll depending on the portals' orientations and the relative player facing direction
		if ( pHeldEntity )	// never pitch reorient while holding an object
		{
			pPortalPlayer->m_bPitchReorientation = false;
		}
		else if ( LocalPortalDataAccess.Placement.vUp.z > 0.99f && // entering wall portal
					( fForwardLength == 0.0f ||			// facing strait up or down
					fPlayerFaceDotPortalFace > 0.5f ||	// facing mostly away from portal
					fPlayerFaceDotPortalFace < -0.5f )	// facing mostly toward portal
				)
		{
			pPortalPlayer->m_bPitchReorientation = true;
		}
		else if ( ( LocalPortalDataAccess.Placement.vForward.z > 0.99f || LocalPortalDataAccess.Placement.vForward.z < -0.99f ) &&	// entering floor or ceiling portal
					( RemotePortalDataAccess.Placement.vForward.z > 0.99f || RemotePortalDataAccess.Placement.vForward.z < -0.99f ) && // exiting floor or ceiling portal 
					(	fPlayerForwardZ < -0.5f || fPlayerForwardZ > 0.5f )		// facing mustly up or down
				)
		{
			pPortalPlayer->m_bPitchReorientation = true;
		}
		else if ( ( RemotePortalDataAccess.Placement.vForward.z > 0.75f && RemotePortalDataAccess.Placement.vForward.z <= 0.99f ) && // exiting wedge portal
					( fPlayerFaceDotPortalUp > 0.0f ) // facing toward the top of the portal
				)
		{
			pPortalPlayer->m_bPitchReorientation = true;
		}
		else
		{
			pPortalPlayer->m_bPitchReorientation = false;
		}
	}
	
	//velocity hacks
	{
		//minimum floor exit velocity if both portals are on the floor or the player is coming out of the floor
		if( RemotePortalDataAccess.Placement.vForward.z > 0.7071f )
		{
			if( vNewVelocity.z < MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER ) 
				vNewVelocity.z = MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER;
		}


		if ( vNewVelocity.LengthSqr() > (MAXIMUM_PORTAL_EXIT_VELOCITY * MAXIMUM_PORTAL_EXIT_VELOCITY)  )
			vNewVelocity *= (MAXIMUM_PORTAL_EXIT_VELOCITY / vNewVelocity.Length());
	}
	
	//untouch the portal(s), will force a touch on destination after the teleport
	{
		pPortalSimulator->ReleaseOwnershipOfEntity( player, true );
		//this->PhysicsNotifyOtherOfUntouch( this, pOther );
		//pOther->PhysicsNotifyOtherOfUntouch( pOther, this );

		pPortal->m_hLinkedPortal->m_PortalSimulator.TakeOwnershipOfEntity( player );

		//m_hLinkedPortal->PhysicsNotifyOtherOfUntouch( m_hLinkedPortal, pOther );
		//pOther->PhysicsNotifyOtherOfUntouch( pOther, m_hLinkedPortal );
	}
	
	pPortalPlayer->SetGroundEntity( NULL );
	
#ifdef GAME_DLL
	//NDebugOverlay::Box(ptNewOrigin, Vector(4, 4, 4), -Vector(4, 4, 4), 255, 0, 0, 128, 3.0 );
#endif

	mv->SetAbsOrigin( ptNewOrigin );
	
	
#if defined( CLIENT_DLL )
	{
		C_Portal_Player::PredictedPortalTeleportation_t entry;
		entry.flTime = gpGlobals->curtime;
		entry.pEnteredPortal = pPortal;
		entry.iCommandNumber = player->m_pCurrentCommand->command_number;
		entry.fDeleteServerTimeStamp = -1.0f;
		entry.matUnroll = pPortal->m_hLinkedPortal->MatrixThisToLinked();
		entry.bDuckForced = bForcedDuck;
		pPortalPlayer->m_PredictedPortalTeleportations.AddToTail(entry);
	}
#endif
	
#if defined( CLIENT_DLL )
	//engine view angles (for mouse input smoothness)
	{
		QAngle qEngineAngles;
		engine->GetViewAngles( qEngineAngles );
		engine->SetViewAngles( TransformAnglesToWorldSpace( qEngineAngles, pPortal->MatrixThisToLinked().As3x4() ) );
	}

	//predicted view angles
	{
		QAngle qPredViewAngles;
		prediction->GetViewAngles( qPredViewAngles );
		prediction->SetViewAngles( TransformAnglesToWorldSpace( qPredViewAngles, pPortal->MatrixThisToLinked().As3x4() ) );
	}

#endif

	//pl.v_angle
	{
		player->pl.v_angle = TransformAnglesToWorldSpace( player->pl.v_angle, pPortal->MatrixThisToLinked().As3x4() );
	}

	//player entity angle
	{
		QAngle qPlayerAngle;
#if defined( GAME_DLL )
		qPlayerAngle = player->GetAbsAngles();
#else
		qPlayerAngle = player->GetNetworkAngles();
#endif

		qPlayerAngle = TransformAnglesToWorldSpace( qPlayerAngle, pPortal->MatrixThisToLinked().As3x4() );

#if defined( GAME_DLL )
		player->SetAbsAngles( qPlayerAngle );
#else
		player->SetNetworkAngles( qPlayerAngle );
#endif
	}


	mv->m_vecVelocity = vNewVelocity;
	
#if defined( GAME_DLL )
	pPortalPlayer->ApplyPortalTeleportation( pPortal, mv );
#else
	pPortalPlayer->ApplyPredictedPortalTeleportation( pPortal, mv, bForcedDuck );		
#endif
#if 0
	CBaseEntity *pHeldEntity = GetPlayerHeldEntity( player );
	if( pHeldEntity )
	{
		pPortalPlayer->ToggleHeldObjectOnOppositeSideOfPortal();
		if( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
		{
			pPortalPlayer->SetHeldObjectPortal( pPortal->m_hLinkedPortal );
		}
		else
		{
			pPortalPlayer->SetHeldObjectPortal( NULL );

			//we need to make sure the held object and player don't interpenetrate when the player's shape changes
			Vector vTargetPosition;
			QAngle qTargetOrientation;
			UpdateGrabControllerTargetPosition( pPortalPlayer, &vTargetPosition, &qTargetOrientation );

#ifdef GAME_DLL
			pHeldEntity->Teleport( &vTargetPosition, &qTargetOrientation, 0 );
#else
			pHeldEntity->SetAbsOrigin( vTargetPosition );
			pHeldEntity->SetAbsAngles( qTargetOrientation );
#endif
			FindClosestPassableSpace( pHeldEntity, RemotePortalDataAccess.Placement.vForward );
		}
	}
#endif
	//force the entity to be touching the other portal right this millisecond
	{
		trace_t Trace;
		memset( &Trace, 0, sizeof(trace_t) );
		//UTIL_TraceEntity( pOther, ptNewOrigin, ptNewOrigin, MASK_SOLID, pOther, COLLISION_GROUP_NONE, &Trace ); //fires off some asserts, and we just need a dummy anyways

		player->PhysicsMarkEntitiesAsTouching( pPortal->m_hLinkedPortal.Get(), Trace );
		pPortal->m_hLinkedPortal.Get()->PhysicsMarkEntitiesAsTouching( player, Trace );
	}
	
#ifdef GAME_DLL
	// Notify the entity that it's being teleported
	// Tell the teleported entity of the portal it has just arrived at
	notify_teleport_params_t paramsTeleport;
	paramsTeleport.prevOrigin		= mv->GetAbsOrigin();
	paramsTeleport.prevAngles		= mv->m_vecViewAngles;
	paramsTeleport.physicsRotate	= true;
	notify_system_event_params_t eventParams ( &paramsTeleport );
	player->NotifySystemEvent( pPortal, NOTIFY_EVENT_TELEPORT, eventParams );
	
	//notify clients of the teleportation
	EntityPortalled( pPortal, player, ptNewOrigin, mv->m_vecAngles, false );

#ifdef _DEBUG
	{
		Vector ptTestCenter = mv->GetAbsOrigin() + ((GetPlayerMaxs() + GetPlayerMins()) * 0.5f);

		float fNewDist, fOldDist;
		fNewDist = RemotePortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptTestCenter ) - RemotePortalDataAccess.Placement.PortalPlane.m_Dist;
		fOldDist = LocalPortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptPlayerCenter ) - LocalPortalDataAccess.Placement.PortalPlane.m_Dist;
		AssertMsg( fNewDist >= 0.0f, "Entity portalled behind the destination portal." );
	}
#endif


	player->NetworkProp()->NetworkStateForceUpdate();
#endif
	player->pl.NetworkStateChanged();
}
#endif
// Expose our interface.
static CPortalGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );

