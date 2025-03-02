//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "portal_placement.h"
#include "prop_portal_shared.h"
#include "weapon_portalgun_shared.h"
#include "portal_shareddefs.h"
#include "mathlib/polyhedron.h"
#include "tier1/callqueue.h"
#include "debugoverlay_shared.h"
#include "collisionutils.h"
#include "weapon_physcannon.h"
#include "portal_gamemovement.h"

#ifdef CLIENT_DLL
#include "c_func_portal_orientation.h"
#include "c_portal_player.h"
#include "c_basedoor.h"
#include "prediction.h"
#else
#include "func_portal_orientation.h"
#include "portal_player.h"
#include "portal_gamestats.h"
#include "env_debughistory.h"

extern CUtlVector<CProp_Portal *> s_PortalLinkageGroups[256];
#endif

#include "PhysicsCloneArea.h"

extern CMoveData *g_pMoveData;
extern IGameMovement *g_pGameMovement;

ConVar sv_portal_unified_velocity( "sv_portal_unified_velocity", "1", FCVAR_REPLICATED, "An attempt at removing patchwork velocity tranformation in portals, moving to a unified approach." );
extern ConVar sv_allow_customized_portal_colors;

extern ConVar sv_portal_debug_touch;

extern CCallQueue *GetPortalCallQueue();


CUtlVector<CProp_Portal *> CProp_Portal_Shared::AllPortals;

const Vector CProp_Portal_Shared::vLocalMins( 0.0f, -PORTAL_HALF_WIDTH, -PORTAL_HALF_HEIGHT );
const Vector CProp_Portal_Shared::vLocalMaxs( 64.0f, PORTAL_HALF_WIDTH, PORTAL_HALF_HEIGHT );

void CProp_Portal_Shared::UpdatePortalTransformationMatrix( const matrix3x4_t &localToWorld, const matrix3x4_t &remoteToWorld, VMatrix *pMatrix )
{
	VMatrix matPortal1ToWorldInv, matPortal2ToWorld, matRotation;

	//inverse of this
	MatrixInverseTR( localToWorld, matPortal1ToWorldInv );

	//180 degree rotation about up
	matRotation.Identity();
	matRotation.m[0][0] = -1.0f;
	matRotation.m[1][1] = -1.0f;

	//final
	matPortal2ToWorld = remoteToWorld;	
	*pMatrix = matPortal2ToWorld * matRotation * matPortal1ToWorldInv;
}

static char *g_pszPortalNonTeleportable[] = 
{ 
	"func_door", 
	"func_door_rotating", 
	"prop_door_rotating",
	"func_tracktrain",
	//"env_ghostanimating",
	"physicsshadowclone"
};

bool CProp_Portal_Shared::IsEntityTeleportable( CBaseEntity *pEntity )
{

	do
	{

#ifdef CLIENT_DLL
		//client
	
		if( dynamic_cast<C_BaseDoor *>(pEntity) != NULL )
			return false;

#else
		//server
		
		for( int i = 0; i != ARRAYSIZE(g_pszPortalNonTeleportable); ++i )
		{
			if( FClassnameIs( pEntity, g_pszPortalNonTeleportable[i] ) )
				return false;
		}

#endif

		Assert( pEntity != pEntity->GetMoveParent() );
		pEntity = pEntity->GetMoveParent();
	} while( pEntity );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the portal active
//-----------------------------------------------------------------------------
void CProp_Portal::SetActive( bool bActive )
{
	m_bOldActivatedState = m_bActivated;
	m_bActivated = bActive;

#ifdef GAME_DLL
	if ( !bActive )
	{
		PunchAllPenetratingPlayers();
		m_OnFizzled.FireOutput( this, this );
	}
#endif
}

void CProp_Portal::PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity )
{
	if( pEntity->IsPlayer() )
	{
		((CPortal_Player *)pEntity)->m_hPortalEnvironment = this;
	}
}

void CProp_Portal::PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity )
{
	if( pEntity->IsPlayer() && (((CPortal_Player *)pEntity)->m_hPortalEnvironment.Get() == this) )
	{
		((CPortal_Player *)pEntity)->m_hPortalEnvironment = NULL;
	}
}
// This is causing issues with portal bumping
void CProp_Portal::UpdateCollisionShape( void )
{
#if 1
	if( m_pCollisionShape )
	{
		physcollision->DestroyCollide( m_pCollisionShape );
		m_pCollisionShape = NULL;
	}

	Vector vLocalMins = CProp_Portal_Shared::vLocalMins;
	Vector vLocalMaxs = CProp_Portal_Shared::vLocalMaxs;

	if( (vLocalMaxs.x <= vLocalMins.x) || (vLocalMaxs.y <= vLocalMins.y) || (vLocalMaxs.z <= vLocalMins.z) )
		return; //volume is 0 (or less)


	//create the collision shape.... TODO: consider having one shared collideable between all portals
	float fPlanes[6*4];
	fPlanes[(0*4) + 0] = 1.0f;
	fPlanes[(0*4) + 1] = 0.0f;
	fPlanes[(0*4) + 2] = 0.0f;
	fPlanes[(0*4) + 3] = vLocalMaxs.x;

	fPlanes[(1*4) + 0] = -1.0f;
	fPlanes[(1*4) + 1] = 0.0f;
	fPlanes[(1*4) + 2] = 0.0f;
	fPlanes[(1*4) + 3] = -vLocalMins.x;

	fPlanes[(2*4) + 0] = 0.0f;
	fPlanes[(2*4) + 1] = 1.0f;
	fPlanes[(2*4) + 2] = 0.0f;
	fPlanes[(2*4) + 3] = vLocalMaxs.y;

	fPlanes[(3*4) + 0] = 0.0f;
	fPlanes[(3*4) + 1] = -1.0f;
	fPlanes[(3*4) + 2] = 0.0f;
	fPlanes[(3*4) + 3] = -vLocalMins.y;

	fPlanes[(4*4) + 0] = 0.0f;
	fPlanes[(4*4) + 1] = 0.0f;
	fPlanes[(4*4) + 2] = 1.0f;
	fPlanes[(4*4) + 3] = vLocalMaxs.z;

	fPlanes[(5*4) + 0] = 0.0f;
	fPlanes[(5*4) + 1] = 0.0f;
	fPlanes[(5*4) + 2] = -1.0f;
	fPlanes[(5*4) + 3] = -vLocalMins.z;

	CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, 0.00001f, true );
	Assert( pPolyhedron != NULL );
	CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
	pPolyhedron->Release();
	Assert( pConvex != NULL );
	m_pCollisionShape = physcollision->ConvertConvexToCollide( &pConvex, 1 );
#endif
}

//unify how we determine the velocity of objects when portalling them
Vector Portal_FindUsefulVelocity( CBaseEntity *pOther )
{
	Vector vOtherVelocity;
	IPhysicsObject *pOtherPhysObject = pOther->VPhysicsGetObject();
	if( sv_portal_unified_velocity.GetBool() )
	{
		if( (pOther->GetMoveType() == MOVETYPE_VPHYSICS) && (pOtherPhysObject != NULL) )
		{
			pOtherPhysObject->GetVelocity( &vOtherVelocity, NULL );
		}
		else
		{
			vOtherVelocity = pOther->GetAbsVelocity();
			if( pOtherPhysObject )
			{
				Vector vPhysVelocity;
				pOtherPhysObject->GetVelocity( &vPhysVelocity, NULL );

				if( vPhysVelocity.LengthSqr() > vOtherVelocity.LengthSqr() )
				{
					vOtherVelocity = vPhysVelocity;
				}
			}
		}
	}
	else
	{
		if( pOther->GetMoveType() == MOVETYPE_VPHYSICS )
		{
			if( pOtherPhysObject && (pOtherPhysObject->GetShadowController() == NULL) )
			{
				pOtherPhysObject->GetVelocity( &vOtherVelocity, NULL );
			}
			else
			{
#if defined( GAME_DLL )
				pOther->GetVelocity( &vOtherVelocity );
#else
				vOtherVelocity = pOther->GetAbsVelocity();
#endif
			}
		}
		else if ( pOther->IsPlayer() && pOther->VPhysicsGetObject() )
		{
			pOther->VPhysicsGetObject()->GetVelocity( &vOtherVelocity, NULL );

			if ( vOtherVelocity == vec3_origin )
			{
				vOtherVelocity = pOther->GetAbsVelocity();
			}
		}
		else
		{
#if defined( GAME_DLL )
			pOther->GetVelocity( &vOtherVelocity );
#else
			vOtherVelocity = pOther->GetAbsVelocity();
#endif
		}

		if( vOtherVelocity == vec3_origin )
		{
			// Recorded velocity is sometimes zero under pushed or teleported movement, or after position correction.
			// In these circumstances, we want implicit velocity ((last pos - this pos) / timestep )
			if ( pOtherPhysObject )
			{
				Vector vOtherImplicitVelocity;
				pOtherPhysObject->GetImplicitVelocity( &vOtherImplicitVelocity, NULL );
				vOtherVelocity += vOtherImplicitVelocity;
			}
		}
	}

	return vOtherVelocity;
}



void CProp_Portal::SetFiredByPlayer( CBasePlayer *pPlayer )
{
	m_hFiredByPlayer = pPlayer;
	if( pPlayer )
	{
		SetPlayerSimulated( pPlayer );
	}
	else
	{
		UnsetPlayerSimulated();
	}
}

void CProp_Portal::PlacePortal( const Vector &vOrigin, const QAngle &qAngles, float fPlacementSuccess, bool bDelay /*= false*/ )
{
	Vector vOldOrigin = GetLocalOrigin();
	QAngle qOldAngles = GetLocalAngles();

	Vector vNewOrigin = vOrigin;
	QAngle qNewAngles = qAngles;

	UTIL_TestForOrientationVolumes( qNewAngles, vNewOrigin, this );

	if ( sv_portal_placement_never_fail.GetBool() )
	{
		fPlacementSuccess = PORTAL_FIZZLE_SUCCESS;
	}

	if ( fPlacementSuccess < 0.5f )
	{
		// Prepare fizzle
		m_vDelayedPosition = vOrigin;
		m_qDelayedAngles = qAngles;

		if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_CANT_FIT )
			m_iDelayedFailure = PORTAL_FIZZLE_CANT_FIT;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED )
			m_iDelayedFailure = PORTAL_FIZZLE_OVERLAPPED_LINKED;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_INVALID_VOLUME )
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_VOLUME;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_INVALID_SURFACE )
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_SURFACE;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_PASSTHROUGH_SURFACE )
			m_iDelayedFailure = PORTAL_FIZZLE_NONE;

#ifdef GAME_DLL
		CWeaponPortalgun *pPortalGun = ( m_hPlacedBy.Get() );

		if( pPortalGun )
		{
			CPortal_Player *pFiringPlayer = dynamic_cast<CPortal_Player *>( pPortalGun->GetOwner() );
			if( pFiringPlayer )
			{
				g_PortalGameStats.Event_PortalPlacement( pFiringPlayer->GetAbsOrigin(), vOrigin, m_iDelayedFailure );
			}
		}
#endif
		return;
	}
	
	SetupPortalColorSet();
	
	m_vDelayedPosition = vNewOrigin;
	m_qDelayedAngles = qNewAngles;
	m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;

	if ( !bDelay )
	{
		NewLocation( vNewOrigin, qNewAngles );
	}

#ifdef CLIENT_DLL
//	HandleNetworkChanges( true );
#endif

#ifdef GAME_DLL
	CWeaponPortalgun *pPortalGun = ( m_hPlacedBy.Get() );

	if( pPortalGun )
	{
		CPortal_Player *pFiringPlayer = ToPortalPlayer( pPortalGun->GetOwner() );
		if( pFiringPlayer )
		{
			g_PortalGameStats.Event_PortalPlacement( pFiringPlayer->GetAbsOrigin(), vOrigin, m_iDelayedFailure );
		}
	}	
#endif
}


void CProp_Portal::StealPortal( CProp_Portal *pHitPortal )
{
	if (!pHitPortal)
		return;

	CWeaponPortalgun *pPortalGun = (m_hPlacedBy.Get());
	
	CBaseEntity *pActivator = this;

	int iLinkageGroupID = 0;

	if (pPortalGun)
	{
		if (pHitPortal)
			iLinkageGroupID = pPortalGun->m_iPortalLinkageGroupID;
		
		if (pPortalGun->GetOwner())
			pActivator = pPortalGun->GetOwner();
		else
			pActivator = pPortalGun;
	}

	if ( pHitPortal && (pHitPortal->GetLinkageGroup() != iLinkageGroupID))
	{
		
#if defined( CLIENT_DLL )
		if ( ShouldPredict() && prediction->InPrediction() )
#endif		
		{
#ifdef GAME_DLL
			// HACK!! For some inexplicable reason, if we don't make the caller pHitPortal, the output won't fire.
			pHitPortal->OnStolen( pActivator, pHitPortal );
#endif
			pHitPortal->DoFizzleEffect( PORTAL_FIZZLE_STOLEN, pHitPortal->m_iPortalColorSet, false );
			pHitPortal->Fizzle();
			//pHitPortal->SetActive( false );	// HACK: For replacing the portal, we need this!+

#ifdef GAME_DLL
			pHitPortal->PunchAllPenetratingPlayers();
#endif
			//m_pHitPortal->m_pPortalReplacingMe = NULL;
			//m_pHitPortal = NULL;
		}
	}
}

extern ConVar sv_portal_placement_never_fail;
//-----------------------------------------------------------------------------
// Purpose: Runs when a fired portal shot reaches it's destination wall. Detects current placement valididty state.
//-----------------------------------------------------------------------------
void CProp_Portal::DelayedPlacementThink( void )
{
	Vector vOldOrigin = m_ptOrigin; //GetLocalOrigin();
	QAngle qOldAngles = m_qAbsAngle; //GetLocalAngles();

	Vector vForward;
	AngleVectors( m_qDelayedAngles, &vForward );
	
	// Check if something made the spot invalid mid flight
	// Bad surface and near fizzle effects take priority
	if ( m_iDelayedFailure != PORTAL_FIZZLE_BAD_SURFACE && m_iDelayedFailure != PORTAL_FIZZLE_NEAR_BLUE && m_iDelayedFailure != PORTAL_FIZZLE_NEAR_RED )
	{
		CProp_Portal *pHitPortal = GetOverlappedPartnerPortal( this, m_vDelayedPosition, m_qDelayedAngles );
		if( pHitPortal )
		{
			m_vDelayedPosition = pHitPortal->GetAbsOrigin();
			m_qDelayedAngles = pHitPortal->GetAbsAngles();
			m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;
			StealPortal( pHitPortal );
		}
		else if ( IsPortalOverlappingOtherPortals( this, m_vDelayedPosition, m_qDelayedAngles ) )
		{
			m_iDelayedFailure = PORTAL_FIZZLE_OVERLAPPED_LINKED;
		}
		else if ( IsPortalIntersectingNoPortalVolume( m_vDelayedPosition, m_qDelayedAngles, vForward ) )
		{
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_VOLUME;
		}
	}

	if ( sv_portal_placement_never_fail.GetBool() )
	{
		m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;
	}

	DoFizzleEffect( m_iDelayedFailure );


	if ( m_iDelayedFailure != PORTAL_FIZZLE_SUCCESS )
	{
		// It didn't successfully place
		return;
	}
	
	// Do effects at old location if it was active
	if (IsActive())
	{
		DoFizzleEffect( PORTAL_FIZZLE_CLOSE, m_iPortalColorSet, false );
	}


#if defined( GAME_DLL )
	CWeaponPortalgun *pPortalGun = (m_hPlacedBy.Get());

	if( pPortalGun )
	{
		CPortal_Player *pFiringPlayer = ToPortalPlayer( pPortalGun->GetOwner() );
		if( pFiringPlayer )
		{
			PortalGameRules()->IncrementPortalsPlaced();
		}

		// Placement successful, fire the output
		m_OnPlacedSuccessfully.FireOutput( pPortalGun, this );
	}
#endif
	
	// Move to new location
	NewLocation( m_vDelayedPosition, m_qDelayedAngles );

#if defined( GAME_DLL )
	SetContextThink( &CProp_Portal::TestRestingSurfaceThink, gpGlobals->curtime + 0.1f, s_pTestRestingSurfaceContext );
#endif
}


void CProp_Portal::TeleportTouchingEntity( CBaseEntity *pOther )
{
	if ( GetPortalCallQueue() )
	{
		GetPortalCallQueue()->QueueCall( this, &CProp_Portal::TeleportTouchingEntity, pOther );
		return;
	}

	Assert( m_hLinkedPortal.Get() != NULL );

	Vector ptOtherOrigin = pOther->GetAbsOrigin();
	Vector ptOtherCenter;

	bool bPlayer = pOther->IsPlayer();
	QAngle qPlayerEyeAngles;
	CPortal_Player *pOtherAsPlayer;

	
	if( bPlayer )
	{
		//NDebugOverlay::EntityBounds( pOther, 255, 0, 0, 128, 60.0f );
		pOtherAsPlayer = (CPortal_Player *)pOther;
		qPlayerEyeAngles = pOtherAsPlayer->pl.v_angle;
#if USEMOVEMENTFORPORTALLING
		Warning( "PORTALLING PLAYER SHOULD BE DONE IN GAMEMOVEMENT\n" );
#endif
	}
	else
	{
		pOtherAsPlayer = NULL;
	}

	ptOtherCenter = pOther->WorldSpaceCenter();

	bool bNonPhysical = false; //special case handling for non-physical objects such as the energy ball and player

	
	
	QAngle qOtherAngles;

	Vector vOtherVelocity = Portal_FindUsefulVelocity(pOther);
	vOtherVelocity -= GetAbsVelocity(); //subtract the portal's velocity if it's moving. It's all relative.

	const PS_InternalData_t &RemotePortalDataAccess = m_hLinkedPortal->m_PortalSimulator.GetInternalData();
	const PS_InternalData_t &LocalPortalDataAccess = m_PortalSimulator.GetInternalData();

	
	if( bPlayer )
	{
		qOtherAngles = pOtherAsPlayer->EyeAngles();

		bNonPhysical = true;
		//if( (fabs( RemotePortalDataAccess.Placement.vForward.z ) + fabs( LocalPortalDataAccess.Placement.vForward.z )) > 0.7071f ) //some combination of floor/ceiling
		if( fabs( LocalPortalDataAccess.Placement.vForward.z ) > 0.0f )
		{
			//we may have to compensate for the fact that AABB's don't rotate ever
			
			float fAbsLocalZ = fabs( LocalPortalDataAccess.Placement.vForward.z );
			float fAbsRemoteZ = fabs( RemotePortalDataAccess.Placement.vForward.z );
			
			if( (fabs(fAbsLocalZ - 1.0f) < 0.01f) &&
				(fabs(fAbsRemoteZ - 1.0f) < 0.01f) )
				//(fabs( LocalPortalDataAccess.Placement.vForward.z + RemotePortalDataAccess.Placement.vForward.z ) < 0.01f) )
			{
				//portals are both aligned on the z axis, no need to shrink the player
				
			}
			else
			{
				//curl the player up into a little ball
				pOtherAsPlayer->SetGroundEntity( NULL );
//#ifdef GAME_DLL
				if( !pOtherAsPlayer->IsDucked() )
				{
					pOtherAsPlayer->ForceDuckThisFrame( pOtherAsPlayer->GetAbsOrigin(), pOtherAsPlayer->GetAbsVelocity() );
					pOtherAsPlayer->m_Local.m_bInDuckJump = true;

					if( LocalPortalDataAccess.Placement.vForward.z > 0.0f )
						ptOtherCenter.z -= 16.0f; //portal facing up, shrink downwards
					else
						ptOtherCenter.z += 16.0f; //portal facing down, shrink upwards
				}
//#endif
			}			
		}
	}
	else
	{
		qOtherAngles = pOther->GetAbsAngles();
		bNonPhysical = FClassnameIs( pOther, "prop_energy_ball" );
	}

	
	Vector ptNewOrigin;
	QAngle qNewAngles;
	Vector vNewVelocity;
	//apply transforms to relevant variables (applied to the entity later)
	{
		if( bPlayer )
		{
			ptNewOrigin = m_matrixThisToLinked * ptOtherCenter;
			ptNewOrigin += ptOtherOrigin - ptOtherCenter;	
		}
		else
		{
			ptNewOrigin = m_matrixThisToLinked * ptOtherOrigin;
		}
		
		// Reorient object angles, originally we did a transformation on the angles, but that doesn't quite work right for gimbal lock cases
		qNewAngles = TransformAnglesToWorldSpace( qOtherAngles, m_matrixThisToLinked.As3x4() );

		qNewAngles.x = AngleNormalizePositive( qNewAngles.x );
		qNewAngles.y = AngleNormalizePositive( qNewAngles.y );
		qNewAngles.z = AngleNormalizePositive( qNewAngles.z );

		// Reorient the velocity		
		vNewVelocity = m_matrixThisToLinked.ApplyRotation( vOtherVelocity );
	}

	//help camera reorientation for the player
	if( bPlayer )
	{
		Vector vPlayerForward;
		AngleVectors( qOtherAngles, &vPlayerForward, NULL, NULL );

		float fPlayerForwardZ = vPlayerForward.z;
		vPlayerForward.z = 0.0f;

		float fForwardLength = vPlayerForward.Length();

		if ( fForwardLength > 0.0f )
		{
			VectorNormalize( vPlayerForward );
		}

		float fPlayerFaceDotPortalFace = LocalPortalDataAccess.Placement.vForward.Dot( vPlayerForward );
		float fPlayerFaceDotPortalUp = LocalPortalDataAccess.Placement.vUp.Dot( vPlayerForward );

		CBaseEntity *pHeldEntity = GetPlayerHeldEntity( pOtherAsPlayer );

		// Sometimes reorienting by pitch is more desirable than by roll depending on the portals' orientations and the relative player facing direction
		if ( pHeldEntity )	// never pitch reorient while holding an object
		{
			pOtherAsPlayer->m_bPitchReorientation = false;
		}
		else if ( LocalPortalDataAccess.Placement.vUp.z > 0.99f && // entering wall portal
				  ( fForwardLength == 0.0f ||			// facing strait up or down
				    fPlayerFaceDotPortalFace > 0.5f ||	// facing mostly away from portal
					fPlayerFaceDotPortalFace < -0.5f )	// facing mostly toward portal
				)
		{
			pOtherAsPlayer->m_bPitchReorientation = true;
		}
		else if ( ( LocalPortalDataAccess.Placement.vForward.z > 0.99f || LocalPortalDataAccess.Placement.vForward.z < -0.99f ) &&	// entering floor or ceiling portal
				  ( RemotePortalDataAccess.Placement.vForward.z > 0.99f || RemotePortalDataAccess.Placement.vForward.z < -0.99f ) && // exiting floor or ceiling portal 
				  (	fPlayerForwardZ < -0.5f || fPlayerForwardZ > 0.5f )		// facing mustly up or down
				)
		{
			pOtherAsPlayer->m_bPitchReorientation = true;
		}
		else if ( ( RemotePortalDataAccess.Placement.vForward.z > 0.75f && RemotePortalDataAccess.Placement.vForward.z <= 0.99f ) && // exiting wedge portal
				  ( fPlayerFaceDotPortalUp > 0.0f ) // facing toward the top of the portal
				)
		{
			pOtherAsPlayer->m_bPitchReorientation = true;
		}
		else
		{
			pOtherAsPlayer->m_bPitchReorientation = false;
		}
	}

	//velocity hacks
	{
		//minimum floor exit velocity if both portals are on the floor or the player is coming out of the floor
		if( RemotePortalDataAccess.Placement.vForward.z > 0.7071f )
		{
			if ( bPlayer )
			{
				if( vNewVelocity.z < MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER ) 
					vNewVelocity.z = MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER;
			}
			else
			{
				if( LocalPortalDataAccess.Placement.vForward.z > 0.7071f )
				{
					if( vNewVelocity.z < MINIMUM_FLOOR_TO_FLOOR_PORTAL_EXIT_VELOCITY ) 
						vNewVelocity.z = MINIMUM_FLOOR_TO_FLOOR_PORTAL_EXIT_VELOCITY;
				}
				else
				{
					if( vNewVelocity.z < MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY )
						vNewVelocity.z = MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY;
				}
			}
		}


		if ( vNewVelocity.LengthSqr() > (MAXIMUM_PORTAL_EXIT_VELOCITY * MAXIMUM_PORTAL_EXIT_VELOCITY)  )
			vNewVelocity *= (MAXIMUM_PORTAL_EXIT_VELOCITY / vNewVelocity.Length());
	}

	//untouch the portal(s), will force a touch on destination after the teleport
	{
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther, true );
		this->PhysicsNotifyOtherOfUntouch( this, pOther );
		pOther->PhysicsNotifyOtherOfUntouch( pOther, this );

		m_hLinkedPortal->m_PortalSimulator.TakeOwnershipOfEntity( pOther );

		//m_hLinkedPortal->PhysicsNotifyOtherOfUntouch( m_hLinkedPortal, pOther );
		//pOther->PhysicsNotifyOtherOfUntouch( pOther, m_hLinkedPortal );
	}

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "PORTAL %i TELEPORTING: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() );
	}
#ifdef GAME_DLL
#if !defined ( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "PORTAL %i TELEPORTING: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() ) );
	}
#endif
#endif
	//do the actual teleportation
	{
		pOther->SetGroundEntity( NULL );

		if( bPlayer )
		{	
			QAngle qTransformedEyeAngles = TransformAnglesToWorldSpace( qPlayerEyeAngles, m_matrixThisToLinked.As3x4() );
			qTransformedEyeAngles.x = AngleNormalizePositive( qTransformedEyeAngles.x );
			qTransformedEyeAngles.y = AngleNormalizePositive( qTransformedEyeAngles.y );
			qTransformedEyeAngles.z = AngleNormalizePositive( qTransformedEyeAngles.z );

#if defined( GAME_DLL )
#if 0
			
			pOtherAsPlayer->SetAbsOrigin( ptNewOrigin );
			pOtherAsPlayer->SetAbsVelocity( vNewVelocity );

			pOtherAsPlayer->m_vecPrevOrigin = vec3_origin;

			pOtherAsPlayer->m_PlayerAnimState->Teleport(&ptNewOrigin, &qNewAngles, pOtherAsPlayer);
#else
			
			pOtherAsPlayer->pl.v_angle = qTransformedEyeAngles;
			pOtherAsPlayer->pl.fixangle = FIXANGLE_ABSOLUTE;			
				
			pOtherAsPlayer->Teleport( &ptNewOrigin, &qNewAngles, &vNewVelocity );
							
			pOtherAsPlayer->UpdateVPhysicsPosition( ptNewOrigin, vNewVelocity, 0.0f );

#endif
#else


			if (pOtherAsPlayer->IsLocalPlayer() && pOtherAsPlayer->m_hPortalEnvironment.Get() == this)
			{
				pOtherAsPlayer->SetAbsOrigin( ptNewOrigin );
				pOtherAsPlayer->SetLocalOrigin( ptNewOrigin );
				pOtherAsPlayer->SetNetworkOrigin( ptNewOrigin );
				

				pOtherAsPlayer->SetAbsAngles( qNewAngles );
				pOtherAsPlayer->SetLocalAngles( qNewAngles );
				pOtherAsPlayer->SetNetworkAngles( qNewAngles );

				/*
				pOtherAsPlayer->SetViewAngles( qTransformedEyeAngles );
				engine->SetViewAngles( qTransformedEyeAngles );
				prediction->SetViewAngles( qTransformedEyeAngles );
				*/

				pOtherAsPlayer->SetAbsVelocity( vNewVelocity );
				pOtherAsPlayer->SetLocalVelocity( vNewVelocity );
				pOtherAsPlayer->SetBaseVelocity( vNewVelocity );				

			}
	
	
			
			//pOtherAsPlayer->m_bWantsToStoreAngles = true;
			pOtherAsPlayer->m_PendingPortalMatrix = MatrixThisToLinked();
#endif
		}
		else
		{
			if( bNonPhysical )
			{
#if defined( GAME_DLL )
				pOther->Teleport( &ptNewOrigin, &qNewAngles, &vNewVelocity );
#else
				pOther->SetAbsOrigin( ptNewOrigin );
				pOther->SetAbsAngles( qNewAngles );
				pOther->SetAbsVelocity( vNewVelocity );		

				IPhysicsObject *pHeldPhysics = pOther->VPhysicsGetObject();
				if (pHeldPhysics)
				{

					const AngularImpulse angImpulse;

					pHeldPhysics->SetPosition(ptNewOrigin, qNewAngles, true);
					pHeldPhysics->SetVelocity(&vec3_origin, &angImpulse);
				}
#endif
			}
			else
			{
				//doing velocity in two stages as a bug workaround, setting the velocity to anything other than 0 will screw up how objects rest on this entity in the future
#if defined( GAME_DLL )
				pOther->Teleport( &ptNewOrigin, &qNewAngles, &vec3_origin );
#else
				pOther->SetAbsOrigin( ptNewOrigin );
				pOther->SetAbsAngles( qNewAngles );
				pOther->SetAbsVelocity( vec3_origin );
				
				IPhysicsObject *pHeldPhysics = pOther->VPhysicsGetObject();
				if ( pHeldPhysics )
				{

					const AngularImpulse angImpulse;

					pHeldPhysics->SetPosition( ptNewOrigin, qNewAngles, true );
					pHeldPhysics->SetVelocity( &vec3_origin, &angImpulse );
				}

#endif
				pOther->ApplyAbsVelocityImpulse( vNewVelocity );
			}
		}
	IPhysicsObject *pPhys = pOther->VPhysicsGetObject();
	if( (pPhys != NULL) && (pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
	{
		CPortal_Player *pHoldingPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pOther );
		pHoldingPlayer->ToggleHeldObjectOnOppositeSideOfPortal();
		if ( pHoldingPlayer->IsHeldObjectOnOppositeSideOfPortal() )
			pHoldingPlayer->SetHeldObjectPortal( this );
		else
			pHoldingPlayer->SetHeldObjectPortal( NULL );
	}
	else if( bPlayer )
	{
		CBaseEntity *pHeldEntity = GetPlayerHeldEntity( pOtherAsPlayer );
		if( pHeldEntity )
		{
			pOtherAsPlayer->ToggleHeldObjectOnOppositeSideOfPortal();
			if( pOtherAsPlayer->IsHeldObjectOnOppositeSideOfPortal() )
			{
				pOtherAsPlayer->SetHeldObjectPortal( m_hLinkedPortal.Get() );
			}
			else
			{
				pOtherAsPlayer->SetHeldObjectPortal( NULL );

				//we need to make sure the held object and player don't interpenetrate when the player's shape changes
				Vector vTargetPosition;
				QAngle qTargetOrientation;
				UpdateGrabControllerTargetPosition( pOtherAsPlayer, &vTargetPosition, &qTargetOrientation );
#ifdef GAME_DLL
				pHeldEntity->Teleport( &vTargetPosition, &qTargetOrientation, 0 );
#else
				pHeldEntity->SetAbsOrigin(vTargetPosition);
				pHeldEntity->SetAbsAngles(qTargetOrientation);

#ifdef CLIENT_DLL
				IPhysicsObject *pHeldPhysics = pHeldEntity->VPhysicsGetObject();
				if ( pHeldPhysics )
				{
					pHeldPhysics->SetPosition( vTargetPosition, qTargetOrientation, true );
				}
#endif

#endif
				FindClosestPassableSpace( pHeldEntity, RemotePortalDataAccess.Placement.vForward );
			}
		}
		
		//we haven't found a good way of fixing the problem of "how do you reorient an AABB". So we just move the player so that they fit
		//m_hLinkedPortal->ForceEntityToFitInPortalWall( pOtherAsPlayer );
	}
	}
	//force the entity to be touching the other portal right this millisecond
	{
		trace_t Trace;
		memset( &Trace, 0, sizeof(trace_t) );
		//UTIL_TraceEntity( pOther, ptNewOrigin, ptNewOrigin, MASK_SOLID, pOther, COLLISION_GROUP_NONE, &Trace ); //fires off some asserts, and we just need a dummy anyways

		pOther->PhysicsMarkEntitiesAsTouching( m_hLinkedPortal.Get(), Trace );
		m_hLinkedPortal.Get()->PhysicsMarkEntitiesAsTouching( pOther, Trace );
	}


#ifdef GAME_DLL
	// Notify the entity that it's being teleported
	// Tell the teleported entity of the portal it has just arrived at
	notify_teleport_params_t paramsTeleport;
	paramsTeleport.prevOrigin		= ptOtherOrigin;
	paramsTeleport.prevAngles		= qOtherAngles;
	paramsTeleport.physicsRotate	= true;
	notify_system_event_params_t eventParams ( &paramsTeleport );
	pOther->NotifySystemEvent( this, NOTIFY_EVENT_TELEPORT, eventParams );
	
	//notify clients of the teleportation
	EntityPortalled( this, pOther, ptNewOrigin, qNewAngles, false );
#endif
#ifdef _DEBUG
	{
		Vector ptTestCenter = pOther->WorldSpaceCenter();

		float fNewDist, fOldDist;
		fNewDist = RemotePortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptTestCenter ) - RemotePortalDataAccess.Placement.PortalPlane.m_Dist;
		fOldDist = LocalPortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptOtherCenter ) - LocalPortalDataAccess.Placement.PortalPlane.m_Dist;
		AssertMsg( fNewDist >= 0.0f, "Entity portalled behind the destination portal." );
	}
#endif

#ifdef GAME_DLL
	pOther->NetworkProp()->NetworkStateForceUpdate();
#endif
	if( bPlayer )
		pOtherAsPlayer->pl.NetworkStateChanged();

	//if( bPlayer )
	//	NDebugOverlay::EntityBounds( pOther, 0, 255, 0, 128, 60.0f );

	Assert( (bPlayer == false) || (pOtherAsPlayer->m_hPortalEnvironment.Get() == m_hLinkedPortal.Get()) );
}


bool CProp_Portal::ShouldTeleportTouchingEntity( CBaseEntity *pOther )
{
	
	if( m_hLinkedPortal.Get() == NULL )
	{
#ifdef GAME_DLL
#if !defined ( DISABLE_DEBUG_HISTORY )
		if ( !IsMarkedForDeletion() )
		{
			ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it has no linked partner portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
		}
#endif
#endif
		if ( sv_portal_debug_touch.GetBool() )
		{
			Msg( "Portal %i not teleporting %s because it has no linked partner portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() );
		}
		return false;
	}

	//can't teleport an entity we don't own
	if( !m_PortalSimulator.OwnsEntity( pOther ) ) 
	{
#ifdef GAME_DLL
#if !defined ( DISABLE_DEBUG_HISTORY )
		if ( !IsMarkedForDeletion() )
		{
			ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it's not simulated by this portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
		}
#endif
#endif
		if ( sv_portal_debug_touch.GetBool() )
		{
#ifdef GAME_DLL
			Msg( "(server)Portal %i not teleporting %s because it's not simulated by this portal. : %f \n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName(), gpGlobals->curtime );
#else
			// This is actually okay because m_hPortalEnvironment is networked.
			Msg( "(client)Portal %i not teleporting %s because it's not simulated by this portal. : %f \n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName(), gpGlobals->curtime );
#endif
		}
		return false;
	}

	if ( !CProp_Portal_Shared::IsEntityTeleportable(pOther) )
	{		
		if ( sv_portal_debug_touch.GetBool() )
			Msg( "Portal %i not teleporting %s because the entity isn't teleportable. : %f \n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName(), gpGlobals->curtime );
		
		return false;
	}
	//Vector ptOtherOrigin = pOther->GetAbsOrigin();
	//IPhysicsObject *pOtherPhysObject = pOther->VPhysicsGetObject();

	Vector ptOtherCenter = pOther->WorldSpaceCenter();

	//Vector vOtherVelocity = Portal_FindUsefulVelocity( pOther );

	// Test for entity's center being past portal plane
	if(m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Normal.Dot( ptOtherCenter ) < m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Dist)
	{
		//entity wants to go further into the plane

		if( m_PortalSimulator.EntityIsInPortalHole( pOther ) )
		{
#ifdef _DEBUG
			static int iAntiRecurse = 0;
			if( pOther->IsPlayer() && (iAntiRecurse == 0) )
			{
				++iAntiRecurse;
				ShouldTeleportTouchingEntity( pOther ); //do it again for debugging
				--iAntiRecurse;
			}
#endif
			return true;
		}
		else
		{
#ifdef GAME_DLL
#if !defined ( DISABLE_DEBUG_HISTORY )
			if ( !IsMarkedForDeletion() )
			{
				ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it was not in the portal hole.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
			}
#endif
#endif
			if ( sv_portal_debug_touch.GetBool() )
			{
				Msg( "Portal %i not teleporting %s because it was not in the portal hole.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() );
			}
		}
	}
	else
	{
		// To spammy
		/*
#ifdef CLIENT_DLL
		if ( sv_portal_debug_touch.GetBool() )
		{
			Msg( "Portal %i not teleporting %s because it was not past the portal plane\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() );
		}
#endif
		*/
	}

	return false;
}

void CProp_Portal::SetupPortalColorSet( void )
{
	if (m_iCustomPortalColorSet && sv_allow_customized_portal_colors.GetBool())
		m_iPortalColorSet = m_iCustomPortalColorSet - 1;
	else
		m_iPortalColorSet = m_iLinkageGroupID;
}


float CProp_Portal::GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	const PS_InternalData_t &RemotePortalDataAccess = m_hLinkedPortal->m_PortalSimulator.GetInternalData();
	
	//velocity hacks
	{
		//minimum floor exit velocity if both portals are on the floor or the player is coming out of the floor
		if (RemotePortalDataAccess.Placement.vForward.z > 0.7071f)
		{
			if (bPlayer)
			{
				return MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER;
			}
			else
			{
				const PS_InternalData_t &LocalPortalDataAccess = m_PortalSimulator.GetInternalData();

				if ( LocalPortalDataAccess.Placement.vForward.z > 0.7071f )
				{
					return MINIMUM_FLOOR_TO_FLOOR_PORTAL_EXIT_VELOCITY;
				}
				else
				{
					return MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY;
				}
			}
		}
	}

	return -FLT_MAX; //default behavior is to not mess with the speed
}

float CProp_Portal::GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	return MAXIMUM_PORTAL_EXIT_VELOCITY;
}


void CProp_Portal::GetExitSpeedRange( CProp_Portal *pEntrancePortal, bool bPlayer, float &fExitMinimum, float &fExitMaximum, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	CProp_Portal *pExitPortal = pEntrancePortal ? pEntrancePortal->m_hLinkedPortal.Get() : NULL;
	if( !pExitPortal )
	{
		fExitMinimum = -FLT_MAX;
		fExitMaximum = FLT_MAX;
		return;
	}
		
	const float COS_PI_OVER_SIX = 0.86602540378443864676372317075294f; // cos( 30 degrees ) in radians
	bool bEntranceOnFloor = pEntrancePortal->m_plane_Origin.z > COS_PI_OVER_SIX;
	bool bExitOnFloor = pExitPortal->m_plane_Origin.z > COS_PI_OVER_SIX;

	fExitMinimum = pExitPortal->GetMinimumExitSpeed( bPlayer, bEntranceOnFloor, bExitOnFloor, vEntityCenterAtExit, pEntity );
	fExitMaximum = pExitPortal->GetMaximumExitSpeed( bPlayer, bEntranceOnFloor, bExitOnFloor, vEntityCenterAtExit, pEntity );
}


void CProp_Portal::UpdatePortalLinkage( void )
{
	if( IsActive() )
	{
		CProp_Portal *pLink = m_hLinkedPortal.Get();

		if( !(pLink && pLink->IsActive()) )
		{
			//no old link, or inactive old link

			if( pLink )
			{
				//we had an old link, must be inactive
				if( pLink->m_hLinkedPortal.Get() != NULL )
					pLink->UpdatePortalLinkage();

				pLink = NULL;
			}
#ifdef GAME_DLL
			int iPortalCount = s_PortalLinkageGroups[m_iLinkageGroupID].Count();
#else
			int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
#endif
			if( iPortalCount != 0 )
			{
#ifdef GAME_DLL
				CProp_Portal **pPortals = s_PortalLinkageGroups[m_iLinkageGroupID].Base();
#else
				CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
#endif
				for( int i = 0; i != iPortalCount; ++i )
				{
					CProp_Portal *pCurrentPortal = pPortals[i];
					if( pCurrentPortal == this )
						continue;
#ifdef CLIENT_DLL
					if ( m_iLinkageGroupID != pCurrentPortal->m_iLinkageGroupID )
						continue;
#endif
					if( pCurrentPortal->IsActive() && pCurrentPortal->m_hLinkedPortal.Get() == NULL )
					{
						pLink = pCurrentPortal;
						pCurrentPortal->m_hLinkedPortal = this;
#ifdef CLIENT_DLL
						pCurrentPortal->m_pLinkedPortal = this;
#endif
						pCurrentPortal->UpdatePortalLinkage();
						break;
					}
				}
			}
		}

		m_hLinkedPortal = pLink;
#ifdef CLIENT_DLL
		m_pLinkedPortal = pLink;
#endif

		if( pLink != NULL )
		{
			CHandle<CProp_Portal> hThis = this;
			CHandle<CProp_Portal> hRemote = pLink;

			this->m_hLinkedPortal = hRemote;
			pLink->m_hLinkedPortal = hThis;
#ifdef CLIENT_DLL
			pLink->m_pLinkedPortal = hThis;
#endif
			m_bIsPortal2 = !m_hLinkedPortal->m_bIsPortal2;
#ifdef GAME_DLL
			CreatePortalMicAndSpeakers();
			UpdatePortalTeleportMatrix();
#else
			UpdateTeleportMatrix();
#endif
		}
		else
		{
			m_PortalSimulator.DetachFromLinked();
			m_PortalSimulator.ReleaseAllEntityOwnership();
		}
		
		m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );

		if( pLink )
			m_PortalSimulator.AttachTo( &pLink->m_PortalSimulator );
#ifndef DISABLE_CLONE_AREA
		if( m_pAttachedCloningArea )
			m_pAttachedCloningArea->UpdatePosition();
#endif
	}
	else
	{
		CProp_Portal *pRemote = m_hLinkedPortal;
		//apparently we've been deactivated
		m_PortalSimulator.DetachFromLinked();
		m_PortalSimulator.ReleaseAllEntityOwnership();
#ifdef GAME_DLL
		PunchAllPenetratingPlayers();
#endif
		m_hLinkedPortal = NULL;
#ifdef CLIENT_DLL
		m_pLinkedPortal = NULL;
#endif
		if( pRemote )
			pRemote->UpdatePortalLinkage();
	}
}