//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "portal\weapon_physcannon.h"

#ifdef CLIENT_DLL
#include "c_portal_player.h"
#include "prediction.h"
#include "c_baseentity.h"
#define CRecipientFilter C_RecipientFilter
#else
#include "portal_player.h"
#include "ai_basenpc.h"
#include "portal_gamestats.h"
#include "util.h"
#include "trains.h"
#endif
#include "portal_player_shared.h"

#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

// Max mass the player can lift with +use
#define PORTAL_PLAYER_MAX_LIFT_MASS 85
#define PORTAL_PLAYER_MAX_LIFT_SIZE 128

acttable_t	unarmedActtable[] = 
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_MELEE,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_MELEE,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_MELEE,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_MELEE,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_MELEE,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_MELEE,					false },
};

const char *g_pszPortalPlayerConcepts[] =
{
	"CONCEPT_PLAYER_DEAD",
	"CONCEPT_CHELL_IDLE",
	"CONCEPT_ABBY_IDLE",
	"CONCEPT_MEL_IDLE",
	"CONCEPT_MALE_PORTAL_PLAYER_IDLE",
	//Escape Idles
	"CONCEPT_MEL_ESCAPE_IDLE",
};

extern ConVar sv_footsteps;
extern ConVar sv_debug_player_use;

extern float IntervalDistance( float x, float x0, float x1 );


//-----------------------------------------------------------------------------
// Consider the weapon's built-in accuracy, this character's proficiency with
// the weapon, and the status of the target. Use this information to determine
// how accurately to shoot at the target.
//-----------------------------------------------------------------------------
Vector CPortal_Player::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetBulletSpread( WEAPON_PROFICIENCY_PERFECT );
	
	return VECTOR_CONE_15DEGREES;
}

void CPortal_Player::GetStepSoundVelocities( float *velwalk, float *velrun )
{
	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING ) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		*velwalk = 10;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		*velrun = 60;		
	}
	else
	{
		*velwalk = 90;
		*velrun = 220;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
void CPortal_Player::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
#ifndef CLIENT_DLL
	PortalGameRules()->IncrementStepsTaken();
#endif

	BaseClass::PlayStepSound( vecOrigin, psurface, fvol, force );
}

Activity CPortal_Player::TranslateActivity( Activity baseAct, bool *pRequired /* = NULL */ )
{
	Activity translated = baseAct;

	if ( GetActiveWeapon() )
	{
		translated = GetActiveWeapon()->ActivityOverride( baseAct, pRequired );
	}
	else if ( unarmedActtable )
	{
		acttable_t *pTable = unarmedActtable;
		int actCount = ARRAYSIZE(unarmedActtable);

		for ( int i = 0; i < actCount; i++, pTable++ )
		{
			if ( baseAct == pTable->baseAct )
			{
				translated = (Activity)pTable->weaponAct;
			}
		}
	}
	else if (pRequired)
	{
		*pRequired = false;
	}

	return translated;
}

CWeaponPortalBase* CPortal_Player::GetActivePortalWeapon() const
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		return dynamic_cast< CWeaponPortalBase* >( pWeapon );
	}
	else
	{
		return NULL;
	}
}

CBaseEntity *CPortal_Player::FindUseEntity()
{
	Vector forward, up;
	EyeVectors( &forward, NULL, &up );
	
	Vector vNetworkPosOffset = vec3_origin;
#ifdef CLIENT_DLL
	vNetworkPosOffset = GetNetworkOrigin() - GetAbsOrigin();
#endif

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition() + vNetworkPosOffset;

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;

	// TODO: Removed because we no longer have ghost animatings. We may need similar code that clips rays against transformed objects.
	/*
#ifndef CLIENT_DLL
	//Check for ghost animatings (these aren't hit in the normal trace because they aren't solid)
	if ( !IsUseableEntity(pObject, 0) )
	{
		Ray_t rayGhostAnimating;
		rayGhostAnimating.Init( searchCenter, searchCenter + forward * 1024 );

		CBaseEntity *list[1024];
		int nCount = UTIL_EntitiesAlongRay( list, 1024, rayGhostAnimating, 0 );

		//Loop through all entities along the pick up ray
		for ( int i = 0; i < nCount; i++ )
		{
			CGhostAnimating *pGhostAnimating = dynamic_cast<CGhostAnimating*>( list[i] );

			//If the entity is a ghost animating
			if( pGhostAnimating )
			{
				trace_t trGhostAnimating;
				enginetrace->ClipRayToEntity( rayGhostAnimating, MASK_ALL, pGhostAnimating, &trGhostAnimating );

				if ( trGhostAnimating.fraction < tr.fraction )
				{
					//If we're not grabbing the clipped ghost
					VPlane plane = pGhostAnimating->GetLocalClipPlane();
					UTIL_Portal_PlaneTransform( pGhostAnimating->GetCloneTransform(), plane, plane );
					if ( plane.GetPointSide( trGhostAnimating.endpos ) != SIDE_FRONT )
					{
						tr = trGhostAnimating;
						pObject = tr.m_pEnt;
					}
				}
			}
		}
	}
#endif
	*/

	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 7;
	while ( !IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = forward - tangents[count]*up;
		VectorNormalize(down);
		UTIL_TraceHull( searchCenter, searchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if ( IsUseableEntity(pObject, 0) )
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
		float dist = delta.Length();
		if ( dist < PLAYER_USE_RADIUS )
		{
#ifndef CLIENT_DLL

			if ( sv_debug_player_use.GetBool() )
			{
				NDebugOverlay::Line( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
				NDebugOverlay::Cross3D( tr.endpos, 16, 0, 255, 0, true, 30 );
			}

			if ( pObject->MyNPCPointer() && pObject->MyNPCPointer()->IsPlayerAlly( this ) )
			{
				// If about to select an NPC, do a more thorough check to ensure
				// that we're selecting the right one from a group.
				pObject = DoubleCheckUseNPC( pObject, searchCenter, forward );
			}

			g_PortalGameStats.Event_PlayerUsed( searchCenter, forward, pObject );
#endif

			return pObject;
		}
	}

#ifndef CLIENT_DLL
	CBaseEntity *pFoundByTrace = pObject;
#endif

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for ( CEntitySphereQuery sphere( searchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( searchCenter, &point );

		Vector dir = point - searchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, forward );

		// Need to be looking at the object more or less
		if ( dot < 0.8 )
			continue;

		if ( dot > nearestDot )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( searchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject )
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

#ifndef CLIENT_DLL
	if ( !pNearest )
	{
		// Haven't found anything near the player to use, nor any NPC's at distance.
		// Check to see if the player is trying to select an NPC through a rail, fence, or other 'see-though' volume.
		trace_t trAllies;
		UTIL_TraceLine( searchCenter, searchCenter + forward * PLAYER_USE_RADIUS, MASK_OPAQUE_AND_NPCS, this, COLLISION_GROUP_NONE, &trAllies );

		if ( trAllies.m_pEnt && IsUseableEntity( trAllies.m_pEnt, 0 ) && trAllies.m_pEnt->MyNPCPointer() && trAllies.m_pEnt->MyNPCPointer()->IsPlayerAlly( this ) )
		{
			// This is an NPC, take it!
			pNearest = trAllies.m_pEnt;
		}
	}

	if ( pNearest && pNearest->MyNPCPointer() && pNearest->MyNPCPointer()->IsPlayerAlly( this ) )
	{
		pNearest = DoubleCheckUseNPC( pNearest, searchCenter, forward );
	}

	if ( sv_debug_player_use.GetBool() )
	{
		if ( !pNearest )
		{
			NDebugOverlay::Line( searchCenter, tr.endpos, 255, 0, 0, true, 30 );
			NDebugOverlay::Cross3D( tr.endpos, 16, 255, 0, 0, true, 30 );
		}
		else if ( pNearest == pFoundByTrace )
		{
			NDebugOverlay::Line( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
			NDebugOverlay::Cross3D( tr.endpos, 16, 0, 255, 0, true, 30 );
		}
		else
		{
			NDebugOverlay::Box( pNearest->WorldSpaceCenter(), Vector(-8, -8, -8), Vector(8, 8, 8), 0, 255, 0, true, 30 );
		}
	}

	g_PortalGameStats.Event_PlayerUsed( searchCenter, forward, pNearest );
#endif

	return pNearest;
}

CBaseEntity* CPortal_Player::FindUseEntityThroughPortal( void )
{
	Vector forward, up;
	EyeVectors( &forward, NULL, &up );

	CProp_Portal *pPortal = GetHeldObjectPortal();

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	Vector vTransformedForward, vTransformedUp, vTransformedSearchCenter;

	VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
	UTIL_Portal_PointTransform( matThisToLinked, searchCenter, vTransformedSearchCenter );
	UTIL_Portal_VectorTransform( matThisToLinked, forward, vTransformedForward );
	UTIL_Portal_VectorTransform( matThisToLinked, up, vTransformedUp );


	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	//UTIL_TraceLine( vTransformedSearchCenter, vTransformedSearchCenter + vTransformedForward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
	Ray_t rayLinked;
	rayLinked.Init( searchCenter, searchCenter + forward * 1024 );
	UTIL_PortalLinked_TraceRay( pPortal, rayLinked, useableContents, this, COLLISION_GROUP_NONE, &tr );

	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;
	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 7;
	while ( !IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = vTransformedForward - tangents[count]*vTransformedUp;
		VectorNormalize(down);
		UTIL_TraceHull( vTransformedSearchCenter, vTransformedSearchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if ( IsUseableEntity(pObject, 0) )
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
		float dist = delta.Length();
		if ( dist < PLAYER_USE_RADIUS )
		{
#ifndef CLIENT_DLL

			if ( pObject->MyNPCPointer() && pObject->MyNPCPointer()->IsPlayerAlly( this ) )
			{
				// If about to select an NPC, do a more thorough check to ensure
				// that we're selecting the right one from a group.
				pObject = DoubleCheckUseNPC( pObject, vTransformedSearchCenter, vTransformedForward );
			}
#endif

			return pObject;
		}
	}

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for ( CEntitySphereQuery sphere( vTransformedSearchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( vTransformedSearchCenter, &point );

		Vector dir = point - vTransformedSearchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, vTransformedForward );

		// Need to be looking at the object more or less
		if ( dot < 0.8 )
			continue;

		if ( dot > nearestDot )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( vTransformedSearchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject )
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

#ifndef CLIENT_DLL
	if ( !pNearest )
	{
		// Haven't found anything near the player to use, nor any NPC's at distance.
		// Check to see if the player is trying to select an NPC through a rail, fence, or other 'see-though' volume.
		trace_t trAllies;
		UTIL_TraceLine( vTransformedSearchCenter, vTransformedSearchCenter + vTransformedForward * PLAYER_USE_RADIUS, MASK_OPAQUE_AND_NPCS, this, COLLISION_GROUP_NONE, &trAllies );

		if ( trAllies.m_pEnt && IsUseableEntity( trAllies.m_pEnt, 0 ) && trAllies.m_pEnt->MyNPCPointer() && trAllies.m_pEnt->MyNPCPointer()->IsPlayerAlly( this ) )
		{
			// This is an NPC, take it!
			pNearest = trAllies.m_pEnt;
		}
	}

	if ( pNearest && pNearest->MyNPCPointer() && pNearest->MyNPCPointer()->IsPlayerAlly( this ) )
	{
		pNearest = DoubleCheckUseNPC( pNearest, vTransformedSearchCenter, vTransformedForward );
	}

#endif

	return pNearest;
}

#if OLD_HL2DM_ANIMSTATE

//==========================
// ANIMATION CODE
//==========================

// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES	45.0f
// After this, need to start turning feet
#define MAX_TORSO_ANGLE		90.0f
// Below this amount, don't play a turning animation/perform IK
#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION		15.0f

static ConVar tf2_feetyawrunscale( "tf2_feetyawrunscale", "2", FCVAR_REPLICATED, "Multiplier on tf2_feetyawrate to allow turning faster when running." );
extern ConVar sv_backspeed;
extern ConVar mp_feetyawrate;
extern ConVar mp_facefronttime;
extern ConVar mp_ik;

CPlayerAnimState::CPlayerAnimState( CPortal_Player *outer )
	: m_pOuter( outer )
{
	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_flCurrentTorsoYaw = 0.0f;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
	m_flTurnCorrectionTime = 0.0f;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::Update()
{
	m_angRender = GetOuter()->GetLocalAngles();

	ComputePoseParam_BodyYaw();
	ComputePoseParam_BodyPitch( GetOuter()->GetModelPtr() );
	ComputePoseParam_BodyLookYaw();

	ComputePlaybackRate();

#ifdef CLIENT_DLL
	GetOuter()->UpdateLookAt();
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePlaybackRate()
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();

	bool isMoving = ( speed > 0.5f ) ? true : false;

	float maxspeed = GetOuter()->GetSequenceGroundSpeed( GetOuter()->GetSequence() );
	
	if ( isMoving && ( maxspeed > 0.0f ) )
	{
		float flFactor = 1.0f;

		// Note this gets set back to 1.0 if sequence changes due to ResetSequenceInfo below
		GetOuter()->SetPlaybackRate( ( speed * flFactor ) / maxspeed );

		// BUG BUG:
		// This stuff really should be m_flPlaybackRate = speed / m_flGroundSpeed
	}
	else
	{
		GetOuter()->SetPlaybackRate( 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CPortal_Player *CPlayerAnimState::GetOuter()
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::EstimateYaw( void )
{
	float dt = gpGlobals->frametime;

	if ( !dt )
	{
		return;
	}

	Vector est_velocity;
	QAngle	angles;

	GetOuterAbsVelocity( est_velocity );

	angles = GetOuter()->GetLocalAngles();

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = angles[YAW] - m_flGaitYaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		m_flGaitYaw += flYawDiff;
		m_flGaitYaw = m_flGaitYaw - (int)(m_flGaitYaw / 360) * 360;
	}
	else
	{
		m_flGaitYaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);

		if (m_flGaitYaw > 180)
			m_flGaitYaw = 180;
		else if (m_flGaitYaw < -180)
			m_flGaitYaw = -180;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyYaw( void )
{
	int iYaw = GetOuter()->LookupPoseParameter( "move_yaw" );
	if ( iYaw < 0 )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	QAngle	angles = GetOuter()->GetLocalAngles();
	float ang = angles[ YAW ];
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}
	
	GetOuter()->SetPoseParameter( iYaw, flYaw );

#ifndef CLIENT_DLL
	//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
	GetOuter()->SetLocalAngles( QAngle( GetOuter()->GetAnimEyeAngles().x, m_flCurrentFeetYaw, 0 ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	// Get pitch from v_angle
	float flPitch = GetOuter()->GetLocalAngles()[ PITCH ];

	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.x = 0.0f;
	m_angRender = absangles;

	// See if we have a blender for pitch
	GetOuter()->SetPoseParameter( pStudioHdr, "aim_pitch", -flPitch );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CPlayerAnimState::ConvergeAngles( float goal,float maxrate, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	float anglediffabs = fabs( anglediff );

	anglediff = AngleNormalize( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( fabs( anglediff ) < maxmove )
	{
		current = goal;
	}
	else
	{
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CPlayerAnimState::ComputePoseParam_BodyLookYaw( void )
{
	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.y = AngleNormalize( absangles.y );
	m_angRender = absangles;

	// See if we even have a blender for pitch
	int upper_body_yaw = GetOuter()->LookupPoseParameter( "aim_yaw" );
	if ( upper_body_yaw < 0 )
	{
		return;
	}

	// Assume upper and lower bodies are aligned and that we're not turning
	float flGoalTorsoYaw = 0.0f;
	int turning = TURN_NONE;
	float turnrate = 360.0f;

	Vector vel;
	
	GetOuterAbsVelocity( vel );

	bool isMoving = ( vel.Length() > 1.0f ) ? true : false;

	if ( !isMoving )
	{
		// Just stopped moving, try and clamp feet
		if ( m_flLastTurnTime <= 0.0f )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAnimEyeAngles().y;
			// Snap feet to be perfectly aligned with torso/eyes
			m_flGoalFeetYaw		= GetOuter()->GetAnimEyeAngles().y;
			m_flCurrentFeetYaw	= m_flGoalFeetYaw;
			m_nTurningInPlace	= TURN_NONE;
		}

		// If rotating in place, update stasis timer
		if ( m_flLastYaw != GetOuter()->GetAnimEyeAngles().y )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAnimEyeAngles().y;
		}

		if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
		}

		turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );

		QAngle eyeAngles = GetOuter()->GetAnimEyeAngles();
		QAngle vAngle = GetOuter()->GetLocalAngles();

		// See how far off current feetyaw is from true yaw
		float yawdelta = GetOuter()->GetAnimEyeAngles().y - m_flCurrentFeetYaw;
		yawdelta = AngleNormalize( yawdelta );

		bool rotated_too_far = false;

		float yawmagnitude = fabs( yawdelta );

		// If too far, then need to turn in place
		if ( yawmagnitude > 45 )
		{
			rotated_too_far = true;
		}

		// Standing still for a while, rotate feet around to face forward
		// Or rotated too far
		// FIXME:  Play an in place turning animation
		if ( rotated_too_far || 
			( gpGlobals->curtime > m_flLastTurnTime + mp_facefronttime.GetFloat() ) )
		{
			m_flGoalFeetYaw		= GetOuter()->GetAnimEyeAngles().y;
			m_flLastTurnTime	= gpGlobals->curtime;

		/*	float yd = m_flCurrentFeetYaw - m_flGoalFeetYaw;
			if ( yd > 0 )
			{
				m_nTurningInPlace = TURN_RIGHT;
			}
			else if ( yd < 0 )
			{
				m_nTurningInPlace = TURN_LEFT;
			}
			else
			{
				m_nTurningInPlace = TURN_NONE;
			}

			turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );
			yawdelta = GetOuter()->GetAnimEyeAngles().y - m_flCurrentFeetYaw;*/

		}

		// Snap upper body into position since the delta is already smoothed for the feet
		flGoalTorsoYaw = yawdelta;
		m_flCurrentTorsoYaw = flGoalTorsoYaw;
	}
	else
	{
		m_flLastTurnTime = 0.0f;
		m_nTurningInPlace = TURN_NONE;
		m_flCurrentFeetYaw = m_flGoalFeetYaw = GetOuter()->GetAnimEyeAngles().y;
		flGoalTorsoYaw = 0.0f;
		m_flCurrentTorsoYaw = GetOuter()->GetAnimEyeAngles().y - m_flCurrentFeetYaw;
	}


	if ( turning == TURN_NONE )
	{
		m_nTurningInPlace = turning;
	}

	if ( m_nTurningInPlace != TURN_NONE )
	{
		// If we're close to finishing the turn, then turn off the turning animation
		if ( fabs( m_flCurrentFeetYaw - m_flGoalFeetYaw ) < MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION )
		{
			m_nTurningInPlace = TURN_NONE;
		}
	}

	// Rotate entire body into position
	absangles = GetOuter()->GetAbsAngles();
	absangles.y = m_flCurrentFeetYaw;
	m_angRender = absangles;

	GetOuter()->SetPoseParameter( upper_body_yaw, clamp( m_flCurrentTorsoYaw, -60.0f, 60.0f ) );

	/*
	// FIXME: Adrian, what is this?
	int body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );

	if ( body_yaw >= 0 )
	{
		GetOuter()->SetPoseParameter( body_yaw, 30 );
	}
	*/

}


 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CPlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	// Not even standing still, sigh
	if ( activity != ACT_IDLE )
		return activity;

	// Not turning
	switch ( m_nTurningInPlace )
	{
	default:
	case TURN_NONE:
		return activity;
	/*
	case TURN_RIGHT:
		return ACT_TURNRIGHT45;
	case TURN_LEFT:
		return ACT_TURNLEFT45;
	*/
	case TURN_RIGHT:
	case TURN_LEFT:
		return mp_ik.GetBool() ? ACT_TURN : activity;
	}

	Assert( 0 );
	return activity;
}

const QAngle& CPlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CPlayerAnimState::Teleport( Vector *pOldOrigin, QAngle *pOldAngles )
{
	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.x = 0.0f;
	m_angRender = absangles;

	m_flCurrentFeetYaw = m_flGoalFeetYaw = m_flLastYaw = m_angRender.y;
	m_flLastTurnTime = 0.0f;
	m_nTurningInPlace = TURN_NONE;
}

void CPlayerAnimState::GetOuterAbsVelocity( Vector& vel )
{
#if defined( CLIENT_DLL )
	GetOuter()->EstimateAbsVelocity( vel );
#else
	vel = GetOuter()->GetAbsVelocity();
#endif
}
#endif // #if 0


//-----------------------------------------------------------------------------
// Purpose: Overload for portal-- Our player can lift his own mass.
// Input  : *pObject - The object to lift
//			bLimitMassAndSize - check for mass/size limits
//-----------------------------------------------------------------------------
void CPortal_Player::PickupObject(CBaseEntity* pObject, bool bLimitMassAndSize)
{
	// can't pick up what you're standing on
	if (GetGroundEntity() == pObject)
		return;

	if (bLimitMassAndSize == true)
	{
		if (CBasePlayer::CanPickupObject(pObject, PORTAL_PLAYER_MAX_LIFT_MASS, PORTAL_PLAYER_MAX_LIFT_SIZE) == false)
			return;
	}

	// Can't be picked up if NPCs are on me
	if (pObject->HasNPCsOnIt())
		return;
#ifdef GAME_DLL
	SetLookingForUseEntity(false);
	SetLookForUseEntity(false);
#endif
	PlayerPickupObject(this, pObject);
}

float CPortal_Player::GetImplicitVerticalStepSpeed() const
{
	return m_flImplicitVerticalStepSpeed;
}

void CPortal_Player::SetImplicitVerticalStepSpeed( float speed )
{
	Assert( !IS_NAN( speed ) );
	m_flImplicitVerticalStepSpeed = speed;
}

void CPortal_Player::ForceDuckThisFrame( Vector origin, Vector velocity )
{
	if (m_Local.m_bDucked != true)
	{
		//m_Local.m_bDucking = false;
		m_Local.m_bDucked = true;
		m_Local.m_bInDuckJump = true;
		ForceButtons(IN_DUCK);

		AddFlag(FL_DUCKING);
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		
#ifdef GAME_DLL // FIXME!! Needs to be on the client
		SetVCollisionState( origin, velocity, VPHYS_CROUCH);
#endif
	}
}

void CPortal_Player::UnDuck(void)
{
	if (m_Local.m_bDucked != false)
	{
		m_Local.m_bDucked = false;
		UnforceButtons(IN_DUCK);
		RemoveFlag(FL_DUCKING);
		SetVCollisionState(GetAbsOrigin(), GetAbsVelocity(), VPHYS_WALK);
	}
}

bool CPortal_Player::UseFoundEntity(CBaseEntity* pUseEntity)
{
	bool usedSomething = false;

#ifdef GAME_DLL
	CPointCommentaryNode *pCommentaryNode = dynamic_cast<CPointCommentaryNode*>(pUseEntity);	
#endif

	//!!!UNDONE: traceline here to prevent +USEing buttons through walls			
	int caps = pUseEntity->ObjectCaps();
#ifdef GAME_DLL
	variant_t emptyVariant;
#endif

#ifdef GAME_DLL	
	if (m_afButtonPressed & IN_USE || m_bLookingForUseEntity)
#else
	if (m_afButtonPressed & IN_USE)
#endif
	{
#ifdef GAME_DLL	
		if (pCommentaryNode)
		{
			inputdata_t inputdata;
			inputdata.pActivator = this;
			inputdata.pCaller = this;
			pCommentaryNode->InputUse(inputdata);
		}
#endif
		// Robin: Don't play sounds for NPCs, because NPCs will allow respond with speech.
		if (!pUseEntity->MyNPCPointer() )
		{
			//Using commentary nodes shouldn't play a sound
#ifdef GAME_DLL
			if (!pCommentaryNode)
#endif
			EmitSound("HL2Player.Use");
		}
	}

#ifdef GAME_DLL
	if (((m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE)) ||
		((m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE | FCAP_ONOFF_USE))) || m_bLookingForUseEntity )
#else
	if (((m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE)) ||
		((m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE | FCAP_ONOFF_USE))))
#endif
	{
#ifdef GAME_DLL
		if (caps & FCAP_CONTINUOUS_USE)
			m_afPhysicsFlags |= PFLAG_USING;

		pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);
#endif
		usedSomething = true;
	}
	// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
	else if ((m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE))	// BUGBUG This is an "off" use
	{
#ifdef GAME_DLL
		pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);
#endif
		usedSomething = true;
	}

#if	HL2_SINGLE_PRIMARY_WEAPON_MODE

	//Check for weapon pick-up
	if (m_afButtonPressed & IN_USE)
	{
		CBaseCombatWeapon* pWeapon = dynamic_cast<CBaseCombatWeapon*>(pUseEntity);

		if ((pWeapon != NULL) && (Weapon_CanSwitchTo(pWeapon)))
		{
			//Try to take ammo or swap the weapon
			if (Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType()))
			{
				Weapon_EquipAmmoOnly(pWeapon);
			}
			else
			{
				Weapon_DropSlot(pWeapon->GetSlot());
				Weapon_Equip(pWeapon);
			}

			usedSomething = true;
		}
	}
#endif

	return usedSomething;
}

bool CPortal_Player::PlayerUse( void )
{
	// Was use pressed or released?
#ifdef GAME_DLL
	if (!m_bLookingForUseEntity)
#endif
		if (!((m_nButtons | m_afButtonPressed | m_afButtonReleased) & IN_USE))
			return false;
#ifdef CLIENT_DLL
	if (m_afButtonPressed & IN_USE)
#else
	if (m_afButtonPressed & IN_USE || m_bLookingForUseEntity)	
#endif
	{
		// Currently using a latched entity?
#ifdef CLIENT_DLL
		if (ClearUseEntity())
#else
		if (ClearUseEntity() && !m_bLookingForUseEntity)		
#endif
		{
			return true;
		}
		else
		{
#ifdef GAME_DLL
			if (m_afPhysicsFlags & PFLAG_DIROVERRIDE)
			{
				m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				return false;
			}
			else
			{	// Start controlling the train!
				CBaseEntity* pTrain = GetGroundEntity();
				if (pTrain && !(m_nButtons & IN_JUMP) && (GetFlags() & FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls(this))
				{
					m_afPhysicsFlags |= PFLAG_DIROVERRIDE;
					m_iTrain = TrainSpeed(pTrain->m_flSpeed, ((CFuncTrackTrain*)pTrain)->GetMaxSpeed());
					m_iTrain |= TRAIN_NEW;
					EmitSound("HL2Player.TrainUse");
					return true;
				}
			}
#endif
		}

		// Tracker 3926:  We can't +USE something if we're climbing a ladder
		if (GetMoveType() == MOVETYPE_LADDER)
		{
			return false;
		}
	}

	CBaseEntity* pUseEntity = FindUseEntity();

	bool usedSomething = false;

	// Found an object
	if (pUseEntity)
	{
		SetHeldObjectOnOppositeSideOfPortal(false);

		// TODO: Removed because we no longer have ghost animatings. May need to rework this code.
		// If we found a ghost animating then it needs to be held across a portal
		/*
		CGhostAnimating *pGhostAnimating = dynamic_cast<CGhostAnimating*>( pUseEntity );
		if ( pGhostAnimating )
		{
			CProp_Portal *pPortal = NULL;

			CPortalSimulator *pPortalSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pGhostAnimating->GetSourceEntity() );

		//	HACKHACK: This assumes all portal simulators are a member of a prop_portal
			pPortal = (CProp_Portal *)(((char *)pPortalSimulator) - ((int)&(((CProp_Portal *)0)->m_PortalSimulator)));
			Assert( (&(pPortal->m_PortalSimulator)) == pPortalSimulator ); // doublechecking the hack

			if ( pPortal )
			{
				SetHeldObjectPortal( pPortal->m_hLinkedPortal );
				SetHeldObjectOnOppositeSideOfPortal( true );
			}
		}
		*/
		usedSomething = UseFoundEntity(pUseEntity);
	}
	else
	{
		Vector forward;
		EyeVectors(&forward, NULL, NULL);
		Vector start = EyePosition();

		Ray_t rayPortalTest;
		rayPortalTest.Init(start, start + forward * PLAYER_USE_RADIUS);

		float fMustBeCloserThan = 2.0f;

		CProp_Portal* pPortal = UTIL_Portal_FirstAlongRay(rayPortalTest, fMustBeCloserThan);
				
		if (pPortal)
		{
			SetHeldObjectPortal(pPortal);
			pUseEntity = FindUseEntityThroughPortal();
		}

#ifdef GAME_DLL
		if (!pUseEntity && !m_bLookingForUseEntity) // Don't search for commentary nodes
		{
			pUseEntity = dynamic_cast<CBaseEntity*>(GetNodeUnderCrosshair());
		}
#endif
		if (pUseEntity)
		{
			if (pPortal)
				SetHeldObjectOnOppositeSideOfPortal(true);

			usedSomething = UseFoundEntity(pUseEntity);
		}
#ifdef GAME_DLL
		else if (m_afButtonPressed & IN_USE && !m_bLookingForUseEntity)
		{
			// Signal that we want to play the deny sound, unless the user is +USEing on a ladder!
			// The sound is emitted in ItemPostFrame, since that occurs after GameMovement::ProcessMove which
			// lets the ladder code unset this flag.

			m_bPlayUseDenySound = true;

			SetLookForUseEntity(true);

			m_flLookForUseEntityTime = gpGlobals->curtime + 0.25;
			return false;
		}
#else
		else if (m_afButtonPressed & IN_USE)
		{
			// Signal that we want to play the deny sound, unless the user is +USEing on a ladder!
			// The sound is emitted in ItemPostFrame, since that occurs after GameMovement::ProcessMove which
			// lets the ladder code unset this flag.

			m_bPlayUseDenySound = true;
			return false;
		}
#endif
	}

	// Debounce the use key
	if (usedSomething && pUseEntity)
	{
		m_Local.m_nOldButtons |= IN_USE;
		m_afButtonPressed &= ~IN_USE;
	}

	return usedSomething || pUseEntity != NULL;
}