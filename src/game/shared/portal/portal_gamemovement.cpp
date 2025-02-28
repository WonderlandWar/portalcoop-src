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
#include "portal_gamemovement.h"
#include "portal/weapon_physcannon.h"

#if defined( CLIENT_DLL )
	#include "c_portal_player.h"
	#include "c_rumble.h"
	#include "prediction.h"
	#include "c_basetoggle.h"
#else
	#include "portal_player.h"
	#include "env_player_surface_trigger.h"
	#include "portal_gamestats.h"
	#include "physicsshadowclone.h"
	#include "recipientfilter.h"
	#include "SoundEmitterSystem/isoundemittersystembase.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_player_trace_through_portals("sv_player_trace_through_portals", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Causes player movement traces to trace through portals." );
ConVar sv_player_funnel_into_portals("sv_player_funnel_into_portals", "0", FCVAR_REPLICATED | FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Forces player funnel" );
ConVar pcoop_avoidplayers( "pcoop_avoidplayers", "1", FCVAR_REPLICATED ); 
ConVar sv_portal_with_gamemovement( "sv_portal_with_gamemovement", "0", FCVAR_REPLICATED, "Sets if player teleportations should be handled by the gamemovement system" );

#ifdef CLIENT_DLL
//ConVar cl_player_funnel_into_portals("cl_player_funnel_into_portals", "1", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Causes the player to auto correct toward the center of floor portals." );
static ConVar cl_player_funnel_into_portals("cl_player_funnel_into_portals", "1", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Causes the player to auto correct toward the center of floor portals.");
#endif

//DIST_EPSILON == 0.03125
ConVar portal_player_interaction_quadtest_epsilon( "portal_player_interaction_quadtest_epsilon", "-0.03125", FCVAR_REPLICATED | FCVAR_CHEAT );


ConVar sv_portal_new_player_trace( "sv_portal_new_player_trace", "0", FCVAR_REPLICATED | FCVAR_CHEAT );


#if defined( CLIENT_DLL )
ConVar cl_vertical_elevator_fix( "cl_vertical_elevator_fix", "1" );
#endif

//trace that has special understanding of how to handle portals
CTrace_PlayerAABB_vs_Portals::CTrace_PlayerAABB_vs_Portals( void )
{
	UTIL_ClearTrace( *this );
	m_bContactedPortalTransitionRamp = false;
}

bool CTrace_PlayerAABB_vs_Portals::HitPortalRamp( const Vector &vUp )
{
	return sv_portal_new_player_trace.GetBool() && (m_bContactedPortalTransitionRamp && DidHit() && (plane.normal.Dot( vUp ) > 0.0f));
}

class CReservePlayerSpot;

#define PORTAL_FUNNEL_AMOUNT 6.0f

extern bool g_bAllowForcePortalTrace;
extern bool g_bForcePortalTrace;

void TracePortalPlayerAABB( CPortal_Player *pPortalPlayer, CProp_Portal *pPortal, const Ray_t &ray_local, const Ray_t &ray_remote, const Vector &vRemoteShift, unsigned int fMask, int collisionGroup, CTrace_PlayerAABB_vs_Portals &pm, bool bSkipRemoteTubeCheck = false );
Vector CalculateExtentShift( const Vector &vLocalExtents, const Vector &vLocalNormal, const Vector &vRemoteExtents, const Vector &vRemoteNormal );

#if defined( CLIENT_DLL )
void CPortalGameMovement::ClientVerticalElevatorFixes( CBasePlayer *pPlayer, CMoveData *pMove )
{
	//find root move parent of our ground entity
	CBaseEntity *pRootMoveParent = pPlayer->GetGroundEntity();
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
				pPredictableGroundEntity->m_hPredictionOwner = pPlayer;
			}
			else if( cl_vertical_elevator_fix.GetBool() )
			{
				Vector vNewOrigin = pPredictableGroundEntity->PredictPosition( player->PredictedServerTime() + TICK_INTERVAL );
				if( (vNewOrigin - pPredictableGroundEntity->GetLocalOrigin()).LengthSqr() > 0.01f )
				{
					bAdjustedRootZ = (vNewOrigin.z != pPredictableGroundEntity->GetLocalOrigin().z);
					pPredictableGroundEntity->SetLocalOrigin( vNewOrigin );

					//invalidate abs transforms for upcoming traces
					C_BaseEntity *pParent = pPlayer->GetGroundEntity();
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
		TracePlayerBBox( pMove->GetAbsOrigin(), pMove->GetAbsOrigin() - Vector( 0.0f, 0.0f, GetPlayerMaxs().z ), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trElevator );

		if( trElevator.startsolid )
		{
			//started in solid, and we think it's an elevator. Pop up the player if at all possible

			//trace up, ignoring the ground entity hierarchy
			Ray_t playerRay;
			playerRay.Init( pMove->GetAbsOrigin(), pMove->GetAbsOrigin() + Vector( 0.0f, 0.0f, GetPlayerMaxs().z ), GetPlayerMins(), GetPlayerMaxs() );

			CTraceFilterSimpleList ignoreGroundEntityHeirarchy( COLLISION_GROUP_PLAYER_MOVEMENT );
			{
				ignoreGroundEntityHeirarchy.AddEntityToIgnore( pPlayer );
				C_BaseEntity *pParent = pPlayer->GetGroundEntity();
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
				TracePlayerBBox( vStart, pMove->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trElevator );
				if( !trElevator.startsolid &&
					(trElevator.m_pEnt == pPlayer->GetGroundEntity()) )
				{
					//if we landed back on the ground entity, call it good
					pMove->SetAbsOrigin( trElevator.endpos );
					pPlayer->SetNetworkOrigin( trElevator.endpos ); //paint code loads from network origin after handling paint powers
				}
			}
		}
		else if( (trElevator.endpos.z < pMove->GetAbsOrigin().z) && (trElevator.m_pEnt == pPlayer->GetGroundEntity()) )
		{
			//re-seat on ground entity
			pMove->SetAbsOrigin( trElevator.endpos );
			pPlayer->SetNetworkOrigin( trElevator.endpos ); //paint code loads from network origin after handling paint powers
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

	//!!HACK HACK: Adrian - slow down all player movement by this factor.
	//!!Blame Yahn for this one.
	gpGlobals->frametime *= pPlayer->GetLaggedMovementValue();

	ResetGetPointContentsCache();

	// Cropping movement speed scales mv->m_fForwardSpeed etc. globally
	// Once we crop, we don't want to recursively crop again, so we set the crop
	//  flag globally here once per usercmd cycle.
	m_iSpeedCropped = SPEED_CROPPED_RESET;

	player = pPlayer;
	mv = pMove;
	mv->m_flMaxSpeed = sv_maxspeed.GetFloat();
	
	/*
#if defined( CLIENT_DLL ) && 1
	CPortal_Player *pPortalPlayer = GetPortalPlayer();
	if( 
		pPortalPlayer->m_hPortalEnvironment &&
		pPortalPlayer->m_hPortalMoveEnvironment &&
		pPortalPlayer->m_hPortalEnvironment != pPortalPlayer->m_hPortalMoveEnvironment
		)
	{
		mv->SetAbsOrigin( pPortalPlayer->m_vMoveOriginHack );
		pPortalPlayer->m_bMoveUseOriginHack = false;
	}
#endif
	*/
	// Ironically this actually makes it worse.
#if defined( CLIENT_DLL )
	//ClientVerticalElevatorFixes( pPlayer, pMove ); //fixup vertical elevator discrepancies between client and server as best we can
#endif

#if USEMOVEMENTFORPORTALLING
	m_vMoveStartPosition = mv->GetAbsOrigin();
	m_vVelocityStart = mv->m_vecVelocity;
#endif

	m_bInPortalEnv = (((CPortal_Player *)pPlayer)->m_hPortalEnvironment != NULL);

	g_bAllowForcePortalTrace = m_bInPortalEnv;
	g_bForcePortalTrace = m_bInPortalEnv;

	// Run the command.
	PlayerMove();
#if USEMOVEMENTFORPORTALLING	
	HandlePortallingLegacy();
#endif
	FinishMove();

//	g_bAllowForcePortalTrace = false;
//	g_bForcePortalTrace = false;
	
	pPlayer->UnforceButtons( IN_DUCK );
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
	if ( BaseClass::CheckJumpButton() && GetPortalPlayer() )
	{
		GetPortalPlayer()->DoAnimationEvent( PLAYERANIMEVENT_JUMP, 0 );
	//	MoveHelper()->PlayerSetAnimation(PLAYER_JUMP);

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
	
#ifdef GAME_DLL

	bool bShouldFunnel;

	const char *sTrue = "1";
	const char *sPingHudHint = NULL;

	sPingHudHint = engine->GetClientConVarValue(engine->IndexOfEdict(player->edict()), "cl_player_funnel_into_portals");

	if (!strcmp(sPingHudHint, sTrue))
	{
		bShouldFunnel = true;
	}
	else
	{
		bShouldFunnel = false;
	}
	
#endif

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
	else if ( bShouldFunnel || sv_player_funnel_into_portals.GetBool() )
#else
	else if ( cl_player_funnel_into_portals.GetBool() || sv_player_funnel_into_portals.GetBool() )
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

	// Don't do this client sided
#ifdef GAME_DLL
	{
		if ( fvol >= 1.0 )
		{
			// Play the future shoes sound
			CRecipientFilter filter;

			filter.AddRecipientsByPAS( player->GetAbsOrigin() );
			filter.UsePredictionRules();

			CSoundParameters params;
			if ( CBaseEntity::GetParametersForSound( "PortalPlayer.FallRecover", params, NULL ) )
			{
				EmitSound_t ep( params );
				ep.m_nPitch = 125.0f - player->m_Local.m_flFallVelocity * 0.03f;					// lower pitch the harder they land
				ep.m_flVolume = MIN( player->m_Local.m_flFallVelocity * 0.00075f - 0.38, 1.0f );	// louder the harder they land
				
				CBaseEntity::EmitSound( filter, player->entindex(), ep );
			}
		}
	}
#endif
}

void TracePlayerBBoxForGround2( const Vector& start, const Vector& end, const Vector& minsSrc,
							   const Vector& maxsSrc, IHandleEntity *player, unsigned int fMask,
							   int collisionGroup, CTrace_PlayerAABB_vs_Portals& pm )
{

	VPROF( "TracePlayerBBoxForGround" );

	CPortal_Player *pPortalPlayer = dynamic_cast<CPortal_Player *>(player->GetRefEHandle().Get());
#ifdef GAME_DLL
	CProp_Portal *pPlayerPortal = pPortalPlayer->m_hPortalEnvironment.Get();
#else
	//CProp_Portal *pPlayerPortal = pPortalPlayer->m_hPortalMoveEnvironment.Get();
	CProp_Portal *pPlayerPortal = pPortalPlayer->m_hPortalEnvironment.Get();
#endif

#ifndef CLIENT_DLL
	if( pPlayerPortal && pPlayerPortal->m_PortalSimulator.IsReadyToSimulate() == false )
		pPlayerPortal = NULL;
#endif

	Vector vTransformedStart, vTransformedEnd;
	Vector transformed_minsSrc, transformed_maxsSrc;
	Vector vRemoteShift = vec3_origin;
	bool bSkipRemoteTube = false;
	if( pPlayerPortal && pPlayerPortal->IsActivedAndLinked() && sv_portal_new_player_trace.GetBool() )
	{
		const PS_InternalData_t &portalSimulator = pPlayerPortal->m_PortalSimulator.GetInternalData();

		CProp_Portal *pLinkedPortal = pPlayerPortal->m_hLinkedPortal;
		const PS_InternalData_t &linkedPortalSimulator = pLinkedPortal->m_PortalSimulator.GetInternalData();

		Vector vOriginToCenter = (maxsSrc + minsSrc) * 0.5f;
		vTransformedStart = pPlayerPortal->m_matrixThisToLinked * (start + vOriginToCenter);
		vTransformedEnd = pPlayerPortal->m_matrixThisToLinked * (end + vOriginToCenter);
		transformed_minsSrc = minsSrc;
		transformed_maxsSrc = maxsSrc;

		Vector vLocalExtents = (maxsSrc - minsSrc) * 0.5f;
		Vector vTeleportExtents = (transformed_maxsSrc - transformed_minsSrc) * 0.5f;

		vRemoteShift = CalculateExtentShift( vLocalExtents, portalSimulator.Placement.PortalPlane.m_Normal, vTeleportExtents, linkedPortalSimulator.Placement.PortalPlane.m_Normal );
		
		//just recompute origin offset for extent shifting again in case it changed
		vOriginToCenter = (transformed_maxsSrc + transformed_minsSrc) * 0.5f;
		//transformed_minsSrc -= vOriginToCenter;
		//transformed_maxsSrc -= vOriginToCenter;
		vTransformedStart -= vOriginToCenter;
		vTransformedEnd -= vOriginToCenter;
	}
	else
	{
		vTransformedStart = start;
		vTransformedEnd = end;
		transformed_minsSrc = minsSrc;
		transformed_maxsSrc = maxsSrc;
	}

	Ray_t ray_local, ray_remote;
	Vector mins, maxs;

	float fraction = pm.fraction;
	Vector endpos = pm.endpos;

	// Check the -x, -y quadrant
	mins = minsSrc;
	maxs.Init( MIN( 0, maxsSrc.x ), MIN( 0, maxsSrc.y ), maxsSrc.z );
	ray_local.Init( start, end, mins, maxs );
	
	ray_remote.Init( vTransformedStart, vTransformedEnd, mins, maxs );
	ray_remote.m_Start += vRemoteShift;
	ray_remote.m_StartOffset += vRemoteShift;
		
	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the +x, +y quadrant
	mins.Init( MAX( 0, minsSrc.x ), MAX( 0, minsSrc.y ), minsSrc.z );
	maxs = maxsSrc;
	ray_local.Init( start, end, mins, maxs );
	
	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the -x, +y quadrant
	mins.Init( minsSrc.x, MAX( 0, minsSrc.y ), minsSrc.z );
	maxs.Init( MIN( 0, maxsSrc.x ), maxsSrc.y, maxsSrc.z );
	ray_local.Init( start, end, mins, maxs );
	
	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the +x, -y quadrant
	mins.Init( MAX( 0, minsSrc.x ), minsSrc.y, minsSrc.z );
	maxs.Init( maxsSrc.x, MIN( 0, maxsSrc.y ), maxsSrc.z );
	ray_local.Init( start, end, mins, maxs );
	
	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

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
	CTrace_PlayerAABB_vs_Portals pm;

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

	point[0] = mv->GetAbsOrigin()[0];
	point[1] = mv->GetAbsOrigin()[1];
	point[2] = mv->GetAbsOrigin()[2] - 2;

	Vector bumpOrigin;
	bumpOrigin = mv->GetAbsOrigin();

	// Shooting up really fast.  Definitely not on ground.
	// On ladder moving up, so not on ground either
	// NOTE: 145 is a jump.
	if ( mv->m_vecVelocity[2] > 140 || 
		( mv->m_vecVelocity[2] > 0.0f && player->GetMoveType() == MOVETYPE_LADDER ) )   
	{
		SetGroundEntity( NULL );
	}
	else
	{
		// Try and move down.
		TracePlayerBBox( bumpOrigin, point, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

		// LAGFIX: When you are in a portal environment, pm.plane.normal[2] must be equal to 1 on a flat surface.
		// Strangely, depending on the portal's location, the value is set to 0 for the server but is always 1 for the client
		// This is actually very bad for the server because it lowers
#if 0
#ifdef CLIENT_DLL
			Msg("(server) pm.plane.normal[2]: %f\n", pm.plane.normal[2]);
#else
			Msg("(client) pm.plane.normal[2]: %f\n", pm.plane.normal[2]);
#endif
#endif
		// If we hit a steep plane, we are not on ground
		if ( pm.plane.normal[2] < 0.7)
		{
			// Test four sub-boxes, to see if any of them would have found shallower slope we could
			// actually stand on

			// LAGFIX: Only the server reports this
#if 0
#ifdef CLIENT_DLL
			Msg("(server) pm.plane.normal[2] < 0.7\n");
#else
			Msg("(client) pm.plane.normal[2] < 0.7\n");
#endif
#endif
			

			TracePlayerBBoxForGround2( bumpOrigin, point, GetPlayerMins(), GetPlayerMaxs(), mv->m_nPlayerHandle.Get(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );
			if ( pm.plane.normal[2] < 0.7)
			{

				SetGroundEntity( NULL );	// too steep
				// probably want to add a check for a +z velocity too!
				if ( ( mv->m_vecVelocity.z > 0.0f ) && ( player->GetMoveType() != MOVETYPE_NOCLIP ) )
				{
					player->m_surfaceFriction = 0.25f;
				}
			}
			else
			{
				SetGroundEntity( &pm );  // Otherwise, point to index of ent under us.
			}
		}
		else
		{
			SetGroundEntity( &pm );  // Otherwise, point to index of ent under us.
		}

		// If we are on something...
		if (player->GetGroundEntity() != NULL)
		{
			// Then we are not in water jump sequence
			player->m_flWaterJumpTime = 0;

			// If we could make the move, drop us down that 1 pixel
			if ( player->GetWaterLevel() < WL_Waist && !pm.startsolid && !pm.allsolid )
			{
				// check distance we would like to move -- this is supposed to just keep up
				// "on the ground" surface not stap us back to earth (i.e. on move origin to
				// end position when the ground is within .5 units away) (2 units)
				if( pm.fraction )
					//				if( pm.fraction < 0.5)
				{
					mv->SetAbsOrigin( pm.endpos );
				}
			}
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

void CPortalGameMovement::Duck( void )
{
	return BaseClass::Duck();
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


//#define TRACE_DEBUG_ENABLED //uncomment for trace debugging goodness
#if defined( TRACE_DEBUG_ENABLED )
bool g_bTraceDebugging = false;
struct StackPair_t
{
	void *pStack[32];
	int iCount;
	CPortal_Base2D *pPortal;
	Vector vStart;
	Vector vEnd;
	Ray_t ray;
	trace_t realTrace;
	trace_t portalTrace1;
	trace_t portalTrace2;
	trace_t remoteTubeTrace;
	trace_t angledWallTrace;
	trace_t finalTrace;

	bool bCopiedRemoteTubeTrace;
	bool bCopiedAngledWallTrace;
	bool bCopiedPortal2Trace;
};

struct StackPairWithBuf_t : public StackPair_t
{
	char szStackTranslated[2048];
};
StackPairWithBuf_t g_TraceRealigned[32]; //for debugging
#define TRACE_DEBUG_ONLY(x) x
#else
#define TRACE_DEBUG_ONLY(x)
#endif

class CTraceTypeWrapper : public ITraceFilter
{
public:
	CTraceTypeWrapper( TraceType_t type, ITraceFilter *pBaseFilter )
		: m_Type( type ), m_pBase( pBaseFilter )
	{ }

	virtual bool ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
	{
		return m_pBase->ShouldHitEntity( pEntity, contentsMask );
	}

	virtual TraceType_t	GetTraceType() const
	{
		return m_Type;
	}

private:
	TraceType_t m_Type;
	ITraceFilter *m_pBase;
};

Vector CalculateExtentShift( const Vector &vLocalExtents, const Vector &vLocalNormal, const Vector &vRemoteExtents, const Vector &vRemoteNormal )
{
	//for visualization of what fClosestExtentDiff is for.....
	//if walking into the floor (standing hull) and coming out a wall (duck hull but doesn't matter). fClosestExtentsDiff will be negative.
	//when this negative value is applied to the linked portal plane normal, it shifts it relatively backwards to it, but closer to us in local space
	//the net effect is that the local ray's foot is exactly on the portal plane, and the exit ray's side is exactly on the portal plane.
	//without the modifier, the exit ray would be roughly 20 units behind the exit plane in this hypothetical
#if 1
	float fClosestExtentDiff = 0.0f;
	for( int i = 0; i != 3; ++i )
	{
		fClosestExtentDiff -= fabs( vLocalNormal[i] * vLocalExtents[i] );
		fClosestExtentDiff += fabs( vRemoteNormal[i] * vRemoteExtents[i] );
	}
#else
	float fMaxLocalExtentDiff = fabs( vLocalNormal[0] * vLocalExtents[0] );
	int iMaxLocalExtent = 0;
	float fMaxRemoteExtentDiff = fabs( vRemoteNormal[0] * vRemoteExtents[0] );
	int iMaxRemoteExtent = 0;
	for( int i = 1; i != 3; ++i )
	{
		float fLocalExtentDiff = fabs( vLocalNormal[i] * vLocalNormal[i] );
		if( fLocalExtentDiff > fMaxLocalExtentDiff )
		{
			fMaxLocalExtentDiff = fLocalExtentDiff;
			iMaxLocalExtent = i;
		}

		float fRemoteExtentDiff = fabs( vRemoteNormal[i] * vRemoteExtents[i] );
		if( fRemoteExtentDiff > fMaxRemoteExtentDiff )
		{
			fMaxRemoteExtentDiff = fRemoteExtentDiff;
			iMaxRemoteExtent = i;
		}
	}
#if 0
	float fClosestExtentDiff = fMaxRemoteExtentDiff - fMaxLocalExtentDiff;
#else
	float fClosestExtentDiff = vRemoteNormal[iMaxRemoteExtent] - vLocalExtents[iMaxLocalExtent];
#endif
#endif

	return (vRemoteNormal * fClosestExtentDiff);
}


ConVar sv_portal_new_player_trace_vs_remote_ents( "sv_portal_new_player_trace_vs_remote_ents", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

void TracePortalPlayerAABB( CPortal_Player *pPortalPlayer, CProp_Portal *pPortal, const Ray_t &ray_local, const Ray_t &ray_remote, const Vector &vRemoteShift, unsigned int fMask, int collisionGroup, CTrace_PlayerAABB_vs_Portals &pm, bool bSkipRemoteTubeCheck )
{
#ifdef CLIENT_DLL
	CTraceFilterSimple traceFilter( pPortalPlayer, collisionGroup );
#else
	CTraceFilterSimple baseFilter( pPortalPlayer, collisionGroup );
	CTraceFilterTranslateClones traceFilter( &baseFilter );

	//verify agreement on whether the player is in a portal simulator or not
	Assert( (pPortalPlayer->m_hPortalEnvironment.Get() != NULL) || (CPortalSimulator::GetSimulatorThatOwnsEntity( pPortalPlayer ) == NULL) );
#endif

	pm.m_bContactedPortalTransitionRamp = false;

#if 1
	if( sv_portal_new_player_trace.GetBool() )
	{
#if defined( TRACE_DEBUG_ENABLED )
		static StackPair_t s_LastTrace[32];
		static int s_iLastTrace = 0;
		StackPair_t &traceDebugEntry = s_LastTrace[s_iLastTrace];
		if( g_bTraceDebugging )
		{
			for( int i = 0; i < 32; ++i )
			{
				int iOldestIterate = (s_iLastTrace + i) % 32; //s_iLastTrace starts pointing at the oldest entry
				memcpy( &g_TraceRealigned[31 - i], &s_LastTrace[iOldestIterate], sizeof( StackPair_t ) );			
			}

			for( int i = 0; i < 32; ++i )
			{
				TranslateStackInfo( g_TraceRealigned[i].pStack, g_TraceRealigned[i].iCount, g_TraceRealigned[i].szStackTranslated, sizeof( g_TraceRealigned[i].szStackTranslated ), "\n" );
			}
			_asm nop;
		}
		else
		{
			traceDebugEntry.vStart = ray_local.m_Start + ray_local.m_StartOffset;
			traceDebugEntry.vEnd = ray_local.m_Start + ray_local.m_Delta + ray_local.m_StartOffset;
			traceDebugEntry.pPortal = pPortalPlayer->m_hPortalEnvironment;
			traceDebugEntry.iCount = GetCallStack_Fast( traceDebugEntry.pStack, 32, 0 );
			traceDebugEntry.ray = ray_local;


			UTIL_ClearTrace( traceDebugEntry.realTrace );
			UTIL_ClearTrace( traceDebugEntry.portalTrace1 );
			UTIL_ClearTrace( traceDebugEntry.portalTrace2 );
			UTIL_ClearTrace( traceDebugEntry.remoteTubeTrace );
			UTIL_ClearTrace( traceDebugEntry.angledWallTrace );
			UTIL_ClearTrace( traceDebugEntry.finalTrace );

			traceDebugEntry.bCopiedRemoteTubeTrace = false;
			traceDebugEntry.bCopiedAngledWallTrace = false;
			traceDebugEntry.bCopiedPortal2Trace = false;
		}		
		++s_iLastTrace;
		s_iLastTrace = s_iLastTrace % 32;
#endif

		trace_t RealTrace;
		UTIL_ClearTrace( RealTrace );
		enginetrace->TraceRay( ray_local, fMask, &traceFilter, &RealTrace );
		TRACE_DEBUG_ONLY( traceDebugEntry.realTrace = RealTrace );


		/*RayInPortalHoleResult_t rayPortalIntersection = RIPHR_NOT_TOUCHING_HOLE;
		if( pPortal && pPortal->m_PortalSimulator.IsReadyToSimulate() )
		{
			rayPortalIntersection = pPortal->m_PortalSimulator.IsRayInPortalHole( ray_local );
		}

		if( rayPortalIntersection != RIPHR_NOT_TOUCHING_HOLE )*/
		if( pPortal && pPortal->IsActivedAndLinked() && pPortal->m_PortalSimulator.IsReadyToSimulate() && (RealTrace.startsolid || (ray_local.m_IsSwept && (RealTrace.fraction < 1.0f))) )
		{
#if defined( DEBUG_PORTAL_TRACE_BOXES )
			color24 debugBoxColor = { 0, 0, 0 };
#endif
			trace_t PortalTrace;
			UTIL_ClearTrace( PortalTrace );
			UTIL_Portal_TraceRay( pPortal, ray_local, fMask, &traceFilter, &PortalTrace, true );
			TRACE_DEBUG_ONLY( traceDebugEntry.portalTrace1 = PortalTrace );

			//Assert( RealTrace.startsolid || !PortalTrace.startsolid || (RealTrace.plane.dist == PortalTrace.plane.dist) );

			if( RealTrace.startsolid || (!PortalTrace.startsolid && (ray_local.m_IsSwept && (PortalTrace.fraction >= RealTrace.fraction))) ) //portal trace provides a better option
			{
#if defined( DEBUG_PORTAL_TRACE_BOXES )
				if( !PortalTrace.startsolid )
				{
					debugBoxColor.g = 255;
				}
#endif

				if( RealTrace.m_pEnt && RealTrace.m_pEnt->IsWorld() )
				{
					RealTrace.m_pEnt = pPortal->m_PortalSimulator.GetInternalData().Simulation.hCollisionEntity;
				}

				const PS_InternalData_t &portalSimulator = pPortal->m_PortalSimulator.GetInternalData();

				CProp_Portal *pLinkedPortal = pPortal->m_hLinkedPortal;
				//const PS_InternalData_t &linkedPortalSimulator = pLinkedPortal->m_PortalSimulator.GetInternalData();

				bool bContactedTransitionRamp = false;
				trace_t PortalTrace2;
				UTIL_ClearTrace( PortalTrace2 );

#define REMOTE_TRACE_WALL_IS_ONLY_TUBE
				
				//trace as an AABB against only the world in the remote space. We don't trace entities because of the projected floor to wall dillemma where we can ledge walk in the middle of the portal
				CTraceTypeWrapper worldOnlyTraceFilterWrapper( TRACE_WORLD_ONLY, &traceFilter );
				ITraceFilter *pRemoteTraceFilter;
				if( sv_portal_new_player_trace_vs_remote_ents.GetBool() ) //weird, couldn't use ternary operator for this assignment
				{
					pRemoteTraceFilter = &traceFilter;
				}
				else
				{
					pRemoteTraceFilter = &worldOnlyTraceFilterWrapper;
				}
#if defined( REMOTE_TRACE_WALL_IS_ONLY_TUBE )
				UTIL_Portal_TraceRay( pLinkedPortal, ray_remote, fMask, pRemoteTraceFilter, &PortalTrace2, false );	
#else
				UTIL_Portal_TraceRay( pLinkedPortal, ray_remote, fMask, pRemoteTraceFilter, &PortalTrace2, true );	
#endif
				TRACE_DEBUG_ONLY( traceDebugEntry.portalTrace2 = PortalTrace2 );

				//slightly angled portal transitions can present their remote-space collision as an extremely steep slope.
				//Step code fails on that kind of step because it's both non-standable and just barely too far forward to step onto
				//this next bit is to detect those cases and present the slope (no matter how steep) as standable
				if( portalSimulator.Placement.pAABBAngleTransformCollideable )
				{
					trace_t AngleTrace;
					UTIL_ClearTrace( AngleTrace );
					physcollision->TraceBox( ray_remote, portalSimulator.Placement.pAABBAngleTransformCollideable, portalSimulator.Placement.ptaap_ThisToLinked.ptOriginTransform, portalSimulator.Placement.ptaap_ThisToLinked.qAngleTransform, &AngleTrace );
					TRACE_DEBUG_ONLY( traceDebugEntry.angledWallTrace = AngleTrace );

					//allow super steep slope climbs if the player is contacting the transition ramp
					if( AngleTrace.startsolid || (ray_remote.m_IsSwept && (AngleTrace.fraction <= PortalTrace.fraction) && (AngleTrace.fraction < 1.0f)) ) //note, only has to go as far as local trace to work
					{
						bContactedTransitionRamp = true;
#if defined( DEBUG_PORTAL_TRACE_BOXES )
						debugBoxColor.r = 255;
#endif
					}

#if 0 //on second thought, maybe you shouldn't actually walk on this magic ramp
					if( ray_remote.m_IsSwept && (AngleTrace.fraction < PortalTrace2.fraction) )
					{
						PortalTrace2 = AngleTrace;
						bContactedTransitionRamp = true;
						TRACE_DEBUG_ONLY( traceDebugEntry.bCopiedAngledWallTrace = true );
					}
#endif
				}

#if defined( REMOTE_TRACE_WALL_IS_ONLY_TUBE )
				//This trace is SUPER IMPORTANT. While the AABB will always be consistent. It can be relatively different when portalling.
				//We need to test that the player will fit through the portal in both configurations.
				//To do that, we'll create a super AABB which will be a local-space maximal AABB of the local AABB and the transformed OBB
				if( !bSkipRemoteTubeCheck && portalSimulator.Simulation.Static.Wall.Local.Tube.pCollideable )
				{
					trace_t TubeTrace;
					UTIL_ClearTrace( TubeTrace );
					physcollision->TraceBox( ray_remote, portalSimulator.Simulation.Static.Wall.Local.Tube.pCollideable, portalSimulator.Placement.ptaap_ThisToLinked.ptOriginTransform, portalSimulator.Placement.ptaap_ThisToLinked.qAngleTransform, &TubeTrace );			
					TRACE_DEBUG_ONLY( traceDebugEntry.remoteTubeTrace = TubeTrace );

					if( TubeTrace.startsolid || (ray_remote.m_IsSwept && (TubeTrace.fraction < PortalTrace2.fraction)) )
					{
						PortalTrace2 = TubeTrace;
						PortalTrace2.surface = portalSimulator.Simulation.Static.SurfaceProperties.surface;
						PortalTrace2.contents = portalSimulator.Simulation.Static.SurfaceProperties.contents;
						PortalTrace2.m_pEnt = portalSimulator.Simulation.Static.SurfaceProperties.pEntity;
						TRACE_DEBUG_ONLY( traceDebugEntry.bCopiedRemoteTubeTrace = true );
					}
				}
#endif

				//if the remote portal trace yielded collision results, translate them back to local space
				if( PortalTrace2.startsolid || (ray_local.m_IsSwept && (PortalTrace2.fraction < PortalTrace.fraction)) )
				{					
#if defined( DBGFLAG_ASSERT )
					Vector vTranslatedStart = (pLinkedPortal->m_matrixThisToLinked * (PortalTrace2.startpos - vRemoteShift - ray_remote.m_StartOffset)) + ray_local.m_StartOffset;
					Assert( RealTrace.startsolid || ((vTranslatedStart - RealTrace.startpos).Length() < 0.1f) );
#endif

					//transformed trace hit something sooner than local trace. Have to respect it, we'd be hitting it if we teleported this frame
					PortalTrace = PortalTrace2;
					PortalTrace.m_pEnt = portalSimulator.Simulation.hCollisionEntity;
					
					PortalTrace.startpos = RealTrace.startpos;						
					PortalTrace.endpos = (pLinkedPortal->m_matrixThisToLinked * (PortalTrace2.endpos - vRemoteShift - ray_remote.m_StartOffset)) + ray_local.m_StartOffset;
					//PortalTrace.endpos = RealTrace.startpos + (ray_local.m_Delta * PortalTrace2.fraction);
					PortalTrace.plane.normal = pLinkedPortal->m_matrixThisToLinked.ApplyRotation( PortalTrace2.plane.normal );
					Vector vTranslatedPointOnPlane = pLinkedPortal->m_matrixThisToLinked * ((PortalTrace2.plane.normal * PortalTrace2.plane.dist) - vRemoteShift);
					PortalTrace.plane.dist = PortalTrace.plane.normal.Dot( vTranslatedPointOnPlane );

					TRACE_DEBUG_ONLY( traceDebugEntry.bCopiedPortal2Trace = true );

#if defined( DEBUG_PORTAL_TRACE_BOXES )
					debugBoxColor.b = 255;
					//NDebugOverlay::Box( (PortalTrace2.endpos - ray_remote.m_StartOffset - vRemoteShift), -ray_remote.m_Extents, ray_remote.m_Extents, 0, 0, 255, 100, 0.0f );
					//NDebugOverlay::Box( (pLinkedPortal->m_matrixThisToLinked * (PortalTrace2.endpos - ray_remote.m_StartOffset)), -ray_local.m_Extents, ray_local.m_Extents, 0, 0, 255, 100, 0.0f );
#endif
				}

				//verify that the portal trace is still the better option.
				//Several cases can fail this part
				// 1. We're simply "near" the portal and hitting remote collision geometry that overlaps our local space in non-euclidean ways
				// 2. The collision represention of the wall isn't as precise as we'd like and juts out a bit from the actual wall
				// 3. The translated portal tube or collision juts out from real space just a bit. If we' switch over to portal traces, we have to commit 100%
				if( RealTrace.startsolid || (!PortalTrace.startsolid && (ray_local.m_IsSwept && (PortalTrace.fraction >= RealTrace.fraction))) )
				{
					*((trace_t *)&pm) = PortalTrace;
					pm.m_bContactedPortalTransitionRamp = bContactedTransitionRamp;
				}
				else //portal trace results got crappy after incorporating AABB vs remote environment results, and real trace still valid
				{
					*((trace_t *)&pm) = RealTrace;
					pm.m_bContactedPortalTransitionRamp = false;
				}
			}
			else //real trace gives us better results
			{				
				*((trace_t *)&pm) = RealTrace;
				pm.m_bContactedPortalTransitionRamp = false;
			}

#if defined( DEBUG_PORTAL_TRACE_BOXES )
			if( sv_portal_new_trace_debugboxes.GetBool() )
			{
				NDebugOverlay::Box( ray_remote.m_Start, -ray_remote.m_Extents, ray_remote.m_Extents, debugBoxColor.r, debugBoxColor.g, debugBoxColor.b, 100, 0.0f );
				NDebugOverlay::Box( ray_local.m_Start, -ray_local.m_Extents, ray_local.m_Extents, debugBoxColor.r, debugBoxColor.g, debugBoxColor.b, 100, 0.0f );
			}
#endif
		}
		else //ray not touching portal hole
		{
			*((trace_t *)&pm) = RealTrace;
			pm.m_bContactedPortalTransitionRamp = false;
		}

		TRACE_DEBUG_ONLY( traceDebugEntry.finalTrace = *((trace_t *)&pm) );
	}
	else // if( sv_portal_new_player_trace.GetBool() )
#endif
	{
		//old trace behavior

		//Okay, so setting pPortal to NULL right here will "fix" laggy movement while in the portal environment, but also disallows you to enter the portal hole.
		UTIL_Portal_TraceRay_With( pPortal, ray_local, fMask, &traceFilter, &pm );


		// LAGFIX: The client barely reports this
		/*
		if(pPortal)
#ifdef CLIENT_DLL
		Msg("(server)Moving player in portal environment\n");
#else
		Msg("(client)Moving player in portal environment\n");
#endif
		*/
		// If we're moving through a portal and failed to hit anything with the above ray trace
		// Use UTIL_Portal_TraceEntity to test this movement through a portal and override the trace with the result
		if ( pm.fraction == 1.0f && UTIL_DidTraceTouchPortals( ray_local, pm ) && sv_player_trace_through_portals.GetBool() )
		{
			trace_t tempTrace;
			Vector start = ray_local.m_Start + ray_local.m_StartOffset;
			UTIL_Portal_TraceEntity( pPortalPlayer, start, start + ray_local.m_Delta, fMask, &traceFilter, &tempTrace );

			if ( tempTrace.DidHit() && tempTrace.fraction < pm.fraction && !tempTrace.startsolid && !tempTrace.allsolid )
			{
				*((trace_t *)&pm) = tempTrace;
				pm.m_bContactedPortalTransitionRamp = false;
			}
		}
	}
}


void CPortalGameMovement::TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, CTrace_PlayerAABB_vs_Portals& pm )
{
	VPROF( "CGameMovement::TracePlayerBBox" );
	//UTIL_ClearTrace( pm );
	//memset( &pm, 0, sizeof( trace_t ) );
	
	CPortal_Player *pPortalPlayer = (CPortal_Player *)((CBaseEntity *)mv->m_nPlayerHandle.Get());

	// LAGFIX: Aha! So this is what causes laggy movement! Arbitrarily setting pPortal to NULL will completely get rid of laggy movement in multiplayer, but also removes your ability to walk through portals
#ifdef GAME_DLL
	CProp_Portal *pPortal = pPortalPlayer->m_hPortalEnvironment;
#else
	//CProp_Portal *pPortal = pPortalPlayer->m_hPortalMoveEnvironment;
	CProp_Portal *pPortal = pPortalPlayer->m_hPortalEnvironment;
#endif

	

	Ray_t ray_local;
	ray_local.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );

	if( pPortal && pPortal->IsActivedAndLinked() )
	{
		const PS_InternalData_t &portalSimulator = pPortal->m_PortalSimulator.GetInternalData();

		CProp_Portal *pLinkedPortal = pPortal->m_hLinkedPortal;
		const PS_InternalData_t &linkedPortalSimulator = pLinkedPortal->m_PortalSimulator.GetInternalData();

		Vector vTeleportExtents = ray_local.m_Extents; //if we would adjust our extents due to a portal teleport, that change should be reflected here

		//Need to take AABB extent changes from possible ducking into account
		//bool bForcedDuck = false;

		Vector vRemoteShift = CalculateExtentShift( ray_local.m_Extents, portalSimulator.Placement.PortalPlane.m_Normal, vTeleportExtents, linkedPortalSimulator.Placement.PortalPlane.m_Normal );

		//this is roughly the ray we'd use if we had just teleported, the exception being the centered startoffset
		Ray_t ray_remote = ray_local;
		ray_remote.m_Start = (pPortal->m_matrixThisToLinked * ray_local.m_Start) + vRemoteShift;
		ray_remote.m_Delta = pPortal->m_matrixThisToLinked.ApplyRotation( ray_local.m_Delta );
		ray_remote.m_StartOffset = vec3_origin;
		//ray_remote.m_StartOffset.z -= vTeleportExtents.z;
		ray_remote.m_Extents = vTeleportExtents;
		

		TracePortalPlayerAABB( pPortalPlayer, pPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm );
	}
	else
	{
		TracePortalPlayerAABB( pPortalPlayer, NULL, ray_local, ray_local, vec3_origin, fMask, collisionGroup, pm );
	}
}
#if !USEMOVEMENTFORPORTALLING
void CPortalGameMovement::TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	VPROF( "CGameMovement::TracePlayerBBox" );
	//UTIL_ClearTrace( pm );
	//memset( &pm, 0, sizeof( trace_t ) );
	
	CPortal_Player *pPortalPlayer = (CPortal_Player *)((CBaseEntity *)mv->m_nPlayerHandle.Get());
	CProp_Portal *pPortal = pPortalPlayer->m_hPortalEnvironment;

	Ray_t ray_local;
	ray_local.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );

	if( pPortal && pPortal->IsActivedAndLinked() )
	{
		const PS_InternalData_t &portalSimulator = pPortal->m_PortalSimulator.GetInternalData();

		CProp_Portal *pLinkedPortal = pPortal->m_hLinkedPortal;
		const PS_InternalData_t &linkedPortalSimulator = pLinkedPortal->m_PortalSimulator.GetInternalData();

		Vector vTeleportExtents = ray_local.m_Extents; //if we would adjust our extents due to a portal teleport, that change should be reflected here

		//Need to take AABB extent changes from possible ducking into account
		//bool bForcedDuck = false;
		bool bDuckToFit = false; //ShouldPortalTransitionCrouch( pPortal );
		bool bDuckToFling = false; //ShouldMaintainFlingAssistCrouch( pLinkedPortal, pPortal->m_matrixThisToLinked.ApplyRotation( mv->m_vecVelocity ) );
		if( bDuckToFit || bDuckToFling )
		{
			//significant change in up axis
			//bForcedDuck = true;
			vTeleportExtents = (GetPlayerMaxs(true) - GetPlayerMins(true)) * 0.5f;
		}

		Vector vRemoteShift = CalculateExtentShift( ray_local.m_Extents, portalSimulator.Placement.PortalPlane.m_Normal, vTeleportExtents, linkedPortalSimulator.Placement.PortalPlane.m_Normal );

		//this is roughly the ray we'd use if we had just teleported, the exception being the centered startoffset
		Ray_t ray_remote = ray_local;
		ray_remote.m_Start = (pPortal->m_matrixThisToLinked * ray_local.m_Start) + vRemoteShift;
		ray_remote.m_Delta = pPortal->m_matrixThisToLinked.ApplyRotation( ray_local.m_Delta );
		ray_remote.m_StartOffset = vec3_origin;
		//ray_remote.m_StartOffset.z -= vTeleportExtents.z;
		ray_remote.m_Extents = vTeleportExtents;		

		TracePortalPlayerAABB( pPortalPlayer, pPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bDuckToFling );
	}
	else
	{
		TracePortalPlayerAABB( pPortalPlayer, NULL, ray_local, ray_local, vec3_origin, fMask, collisionGroup, pm );
	}
}
#else
void CPortalGameMovement::TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	CTrace_PlayerAABB_vs_Portals pm2;
	*(trace_t *)&pm2 = pm;
	TracePlayerBBox( start, end, fMask, collisionGroup, pm2 );
	pm = *(trace_t *)&pm2;
}
#endif

//Portalling in Portal 2 is handled by gamemovement code which is a whole different story. I may use it as a last resort.
#if USEMOVEMENTFORPORTALLING

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
		//now that you're here, go up and re-run the TracePlayerBBox() above
#endif
#if defined( TRACE_DEBUG_ENABLED )
		g_bTraceDebugging = true;
		TracePlayerBBox( pos, pos, MASK_PLAYERSOLID, collisionGroup, pm ); //hook into the existing portal special trace functionality
#endif
		return pm.m_pEnt->GetRefEHandle();
	}
#ifndef CLIENT_DLL
	else if ( pm.startsolid && pm.m_pEnt && CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pm.m_pEnt ) )
	{
		// Stuck in a portal environment object, so unstick them!
		CPortal_Player *pPortalPlayer = (CPortal_Player *)((CBaseEntity *)mv->m_nPlayerHandle.Get());
		pPortalPlayer->SetStuckOnPortalCollisionObject();

		return INVALID_EHANDLE_INDEX;
	}
#endif
	else
	{	
		return INVALID_EHANDLE_INDEX;
	}
}

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

						if( (fabs( vPortalPlayerOriginDiff.Dot( portalSimulator.Placement.vRight ) ) > PORTAL_HALF_WIDTH) ||
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
			pPortalPlayer->m_hPortalEnvironment = NULL;
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

	if( m_vVelocityStart.Dot( pPortal->m_PortalSimulator.GetInternalData().Placement.vForward ) > 0.0f )
	{
		return;
	}

	bool bPastPortalHole = false;
	// Test for entity's center being past portal plane
	if(pPortalSimulator->GetInternalData().Placement.PortalPlane.m_Normal.Dot( ptPlayerCenter ) < pPortalSimulator->GetInternalData().Placement.PortalPlane.m_Dist)
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
	const PS_InternalData_t &LocalPortalDataAccess = pPortalSimulator->GetInternalData();

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

