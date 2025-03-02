//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_prop_portal.h"
#include "portal_shareddefs.h"
#include "clientsideeffects.h"
#include "tier0/vprof.h"
#include "materialsystem/itexture.h"
#include "hud_macros.h"
#include "igamesystem.h"
#include "view.h"						// For MainViewOrigin()
#include "clientleafsystem.h"			// For finding the leaves our portals are in
#include "portal_render_targets.h"		// Access to static references to Portal-specific render textures
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/KeyValues.h"
#include "rendertexture.h"
#include "prop_portal_shared.h"
#include "particles_new.h"
#include "weapon_portalgun_shared.h"
#include "c_te_effect_dispatch.h"
#include "engine/IEngineSound.h"
#include "vcollide_parse.h"
#include "collisionutils.h"
#include "portal_placement.h"

#include "c_portal_player.h"
#include "prediction.h"
#include "recvproxy.h"

#include "c_pixel_visibility.h"

#include "glow_overlay.h"

#include "dlight.h"
#include "iefx.h"

#include "simple_keys.h"

#ifdef _DEBUG
#include "filesystem.h"
#endif

#include "debugoverlay_shared.h"

#include "tier1/callqueue.h"

#define MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY 50.0f
#define MINIMUM_FLOOR_TO_FLOOR_PORTAL_EXIT_VELOCITY 225.0f
#define MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER 300.0f
#define MAXIMUM_PORTAL_EXIT_VELOCITY 1000.0f

CCallQueue *GetPortalCallQueue();

ConVar sv_portal_debug_touch("sv_portal_debug_touch", "0", FCVAR_REPLICATED);
ConVar sv_portal_placement_never_fail("sv_portal_placement_never_fail", "0", FCVAR_REPLICATED | FCVAR_CHEAT);
ConVar sv_portal_new_velocity_check("sv_portal_new_velocity_check", "1", FCVAR_REPLICATED | FCVAR_CHEAT);

extern ConVar sv_allow_customized_portal_colors;

static CUtlVector<C_Prop_Portal *> s_PortalLinkageGroups[256];

IMPLEMENT_CLIENTCLASS_DT( C_Prop_Portal, DT_Prop_Portal, CProp_Portal )

	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropVector( RECVINFO_NAME( m_angNetworkAngles, m_angRotation ) ),
	
	RecvPropVector( RECVINFO( m_ptOrigin ) ),
	RecvPropVector( RECVINFO( m_qAbsAngle ) ),

	RecvPropEHandle( RECVINFO(m_hLinkedPortal) ),
	RecvPropBool( RECVINFO(m_bActivated) ),
	RecvPropBool( RECVINFO(m_bOldActivatedState) ),
	RecvPropBool( RECVINFO(m_bIsPortal2) ),
	RecvPropInt( RECVINFO(m_iLinkageGroupID)),
	
	RecvPropEHandle( RECVINFO( m_hPlacedBy ) ),
	RecvPropEHandle( RECVINFO( m_hFiredByPlayer ) ),
	RecvPropInt( RECVINFO( m_nPlacementAttemptParity ) ),
	RecvPropInt( RECVINFO( m_iCustomPortalColorSet ) ),


	RecvPropDataTable( RECVINFO_DT( m_PortalSimulator ), 0, &REFERENCE_RECV_TABLE(DT_PortalSimulator) )
END_RECV_TABLE()


LINK_ENTITY_TO_CLASS( prop_portal, C_Prop_Portal );

BEGIN_PREDICTION_DATA(C_Prop_Portal)


	DEFINE_PRED_FIELD (m_bActivated, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bOldActivatedState, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hLinkedPortal, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_ptOrigin, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_qAbsAngle, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iCustomPortalColorSet, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iLinkageGroupID, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	
	//not actually networked fields. But we need them backed up and restored in the same way as the networked ones.	
	DEFINE_FIELD( m_vForward, FIELD_VECTOR ),
	DEFINE_FIELD( m_vRight, FIELD_VECTOR ),
	DEFINE_FIELD( m_vUp, FIELD_VECTOR ),
	DEFINE_FIELD( m_matrixThisToLinked, FIELD_VMATRIX ),
	DEFINE_FIELD( m_plane_Origin, FIELD_QUATERNION ), // Vector4D is the same size as a Quaternion, it's fine
	//DEFINE_FIELD( m_iPortalColorSet, FIELD_INTEGER ),

	DEFINE_PRED_FIELD( m_nPlacementAttemptParity, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	
END_PREDICTION_DATA()

extern ConVar use_server_portal_particles;

static ConVar portal_demohack( "portal_demohack", "0", FCVAR_ARCHIVE, "Do the demo_legacy_rollback setting to help during demo playback of going through portals." );

class C_PortalInitHelper : public CAutoGameSystem
{
	virtual bool Init()
	{
		if ( portal_demohack.GetBool() )
		{
			ConVarRef demo_legacy_rollback_ref( "demo_legacy_rollback" );
			demo_legacy_rollback_ref.SetValue( false ); //Portal demos are wrong if the eyes rollback as far as regular demos
		}
		// However, there are probably bugs with this when jump ducking, etc.
		return true;
	}
};
static C_PortalInitHelper s_PortalInitHelper;

CProp_Portal *CProp_Portal::FindPortal( unsigned char iLinkageGroupID, bool bPortal2, bool bCreateIfNothingFound /*= false*/ )
{
	int iPortalCount = s_PortalLinkageGroups[iLinkageGroupID].Count();

	if( iPortalCount != 0 )
	{
		CProp_Portal *pFoundInactive = NULL;
		CProp_Portal **pPortals = s_PortalLinkageGroups[iLinkageGroupID].Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			if( pPortals[i]->m_bIsPortal2 == bPortal2 )
			{
				if( pPortals[i]->IsActive() )
					return pPortals[i];
				else
					pFoundInactive = pPortals[i];
			}
		}

		if( pFoundInactive )
			return pFoundInactive;
	}

	return NULL;
}


C_Prop_Portal::C_Prop_Portal( void )
{	
	m_PortalSimulator.SetPortalSimulatorCallbacks( this );

	TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	CProp_Portal_Shared::AllPortals.AddToTail( this );

	SetPredictionEligible(true);
}

C_Prop_Portal::~C_Prop_Portal( void )
{
	DestroyAttachedParticles();

	CProp_Portal_Shared::AllPortals.FindAndRemove( this );
	g_pPortalRender->RemovePortal( this );

	for( int i = m_GhostRenderables.Count(); --i >= 0; )
	{
		delete m_GhostRenderables[i];
	}
	m_GhostRenderables.RemoveAll();

	if (m_pCollisionShape)
	{
		physcollision->DestroyCollide(m_pCollisionShape);
		m_pCollisionShape = NULL;
	}
}

void C_Prop_Portal::Spawn( void )
{
//	solid_t tmpSolid;
//	VPhysicsInitNormal(tmpSolid,);

	SetThink( &C_Prop_Portal::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
	m_bDoRenderThink = true;

	m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	m_hEdgeEffect = NULL;
	m_hParticleEffect = NULL;
	BaseClass::Spawn();
#ifndef DISABLE_CLONE_AREA
	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );
#endif
}


void CProp_Portal::Precache( void )
{
	PrecacheScriptSound( "Portal.ambient_loop" );

	PrecacheScriptSound( "Portal.open_blue" );
	PrecacheScriptSound( "Portal.open_red" );
	PrecacheScriptSound( "Portal.close_blue" );
	PrecacheScriptSound( "Portal.close_red" );
	PrecacheScriptSound( "Portal.fizzle_moved" );
	PrecacheScriptSound( "Portal.fizzle_invalid_surface" );

	PrecacheModel( "models/portals/portal1.mdl" );
	PrecacheModel( "models/portals/portal2.mdl" );

	PrecacheParticleSystem( "portal_1_particles" );
	PrecacheParticleSystem( "portal_2_particles" );
	PrecacheParticleSystem( "portal_1_edge" );
	PrecacheParticleSystem( "portal_2_edge" );
	PrecacheParticleSystem( "portal_1_nofit" );
	PrecacheParticleSystem( "portal_2_nofit" );
	PrecacheParticleSystem( "portal_1_overlap" );
	PrecacheParticleSystem( "portal_2_overlap" );
	PrecacheParticleSystem( "portal_1_badvolume" );
	PrecacheParticleSystem( "portal_2_badvolume" );
	PrecacheParticleSystem( "portal_1_badsurface" );
	PrecacheParticleSystem( "portal_2_badsurface" );
	PrecacheParticleSystem( "portal_1_close" );
	PrecacheParticleSystem( "portal_2_close" );
	PrecacheParticleSystem( "portal_1_cleanser" );
	PrecacheParticleSystem( "portal_2_cleanser" );
	PrecacheParticleSystem( "portal_1_near" );
	PrecacheParticleSystem( "portal_2_near" );
	PrecacheParticleSystem( "portal_1_success" );
	PrecacheParticleSystem( "portal_2_success" );
	PrecacheParticleSystem( "portal_1_stolen" );
	PrecacheParticleSystem( "portal_2_stolen" );

	PrecacheParticleSystem("portal_red_particles");
	PrecacheParticleSystem("portal_red_edge");
	PrecacheParticleSystem("portal_red_nofit");
	PrecacheParticleSystem("portal_red_overlap");
	PrecacheParticleSystem("portal_red_badvolume");
	PrecacheParticleSystem("portal_red_badsurface");
	PrecacheParticleSystem("portal_red_close");
	PrecacheParticleSystem("portal_red_cleanser");
	PrecacheParticleSystem("portal_red_near");
	PrecacheParticleSystem("portal_red_success");
	PrecacheParticleSystem("portal_red_stolen");

	PrecacheParticleSystem("portal_yellow_particles");
	PrecacheParticleSystem("portal_yellow_edge");
	PrecacheParticleSystem("portal_yellow_nofit");
	PrecacheParticleSystem("portal_yellow_overlap");
	PrecacheParticleSystem("portal_yellow_badvolume");
	PrecacheParticleSystem("portal_yellow_badsurface");
	PrecacheParticleSystem("portal_yellow_close");
	PrecacheParticleSystem("portal_yellow_cleanser");
	PrecacheParticleSystem("portal_yellow_near");
	PrecacheParticleSystem("portal_yellow_success");
	PrecacheParticleSystem("portal_yellow_stolen");

	PrecacheParticleSystem("portal_lightblue_particles");
	PrecacheParticleSystem("portal_lightblue_edge");
	PrecacheParticleSystem("portal_lightblue_nofit");
	PrecacheParticleSystem("portal_lightblue_overlap");
	PrecacheParticleSystem("portal_lightblue_badvolume");
	PrecacheParticleSystem("portal_lightblue_badsurface");
	PrecacheParticleSystem("portal_lightblue_close");
	PrecacheParticleSystem("portal_lightblue_cleanser");
	PrecacheParticleSystem("portal_lightblue_near");
	PrecacheParticleSystem("portal_lightblue_success");
	PrecacheParticleSystem("portal_lightblue_stolen");

	PrecacheParticleSystem("portal_purple_particles");
	PrecacheParticleSystem("portal_purple_edge");
	PrecacheParticleSystem("portal_purple_nofit");
	PrecacheParticleSystem("portal_purple_overlap");
	PrecacheParticleSystem("portal_purple_badvolume");
	PrecacheParticleSystem("portal_purple_badsurface");
	PrecacheParticleSystem("portal_purple_close");
	PrecacheParticleSystem("portal_purple_cleanser");
	PrecacheParticleSystem("portal_purple_near");
	PrecacheParticleSystem("portal_purple_success");
	PrecacheParticleSystem("portal_purple_stolen");

	PrecacheParticleSystem("portal_green_particles");
	PrecacheParticleSystem("portal_green_edge");
	PrecacheParticleSystem("portal_green_nofit");
	PrecacheParticleSystem("portal_green_overlap");
	PrecacheParticleSystem("portal_green_badvolume");
	PrecacheParticleSystem("portal_green_badsurface");
	PrecacheParticleSystem("portal_green_close");
	PrecacheParticleSystem("portal_green_cleanser");
	PrecacheParticleSystem("portal_green_near");
	PrecacheParticleSystem("portal_green_success");
	PrecacheParticleSystem("portal_green_stolen");

	PrecacheParticleSystem("portal_pink_particles");
	PrecacheParticleSystem("portal_pink_edge");
	PrecacheParticleSystem("portal_pink_nofit");
	PrecacheParticleSystem("portal_pink_overlap");
	PrecacheParticleSystem("portal_pink_badvolume");
	PrecacheParticleSystem("portal_pink_badsurface");
	PrecacheParticleSystem("portal_pink_close");
	PrecacheParticleSystem("portal_pink_cleanser");
	PrecacheParticleSystem("portal_pink_near");
	PrecacheParticleSystem("portal_pink_success");
	PrecacheParticleSystem("portal_pink_stolen");

	BaseClass::Precache();
}

void C_Prop_Portal::UpdatePartitionListEntry()
{
	::partition->RemoveAndInsert( 
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES,  // add
		CollisionProp()->GetPartitionHandle() );
}

bool C_Prop_Portal::TestCollision(const Ray_t &ray, unsigned int fContentsMask, trace_t& tr)
{
	if ( !m_pCollisionShape )
	{
		//HACK: This is a last-gasp type fix for a crash caused by m_pCollisionShape not yet being set up
		// during a restore.
		UpdateCollisionShape();
	}

	physcollision->TraceBox( ray, MASK_ALL, NULL, m_pCollisionShape, m_ptOrigin, m_qAbsAngle, &tr );
	return tr.DidHit();
}

void C_Prop_Portal::Activate( void )
{
#ifndef DISABLE_CLONE_AREA
	if( m_pAttachedCloningArea == NULL )
		m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );
#endif

	//UpdatePortalLinkage();

	if( IsActive() && (m_hLinkedPortal.Get() != NULL) )
	{
		Vector ptCenter = m_ptOrigin;
		QAngle qAngles = m_qAbsAngle;
		m_PortalSimulator.MoveTo( ptCenter, qAngles );

		//resimulate everything we're touching
		touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
		if( root )
		{
			for( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
			{
				CBaseEntity *pOther = link->entityTouched;
				if( CProp_Portal_Shared::IsEntityTeleportable( pOther ) )
				{
					CCollisionProperty *pOtherCollision = pOther->CollisionProp();
					Vector vWorldMins, vWorldMaxs;
					pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
					Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

					if( m_plane_Origin.AsVector3D().Dot( ptOtherCenter ) > m_plane_Origin.w )
					{
						//we should be interacting with this object, add it to our environment
						if( SharedEnvironmentCheck( pOther ) )
						{
							Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
								(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

							CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
							if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
								pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

							m_PortalSimulator.TakeOwnershipOfEntity( pOther );
						}
					}
				}
			}
		}
	}

	BaseClass::Activate();
}

ConVar cl_portal_teleportation_interpolation_fixup_method( "cl_portal_teleportation_interpolation_fixup_method", "1", 0, "0 = transform history only, 1 = insert discontinuity transform" );

void EntityPortalledMessageHandler( C_BaseEntity *pEntity, C_Prop_Portal *pPortal, float fTime, bool bForcedDuck )
{
#if( PLAYERPORTALDEBUGSPEW == 1 )
	Warning( "EntityPortalledMessageHandler() %f -=- %f %i======================\n", fTime, engine->GetLastTimeStamp(), prediction->GetLastAcknowledgedCommandNumber() );
#endif
#if 0
	C_PortalGhostRenderable *pGhost = pPortal->GetGhostRenderableForEntity( pEntity );
	if( !pGhost )
	{
		//high velocity edge case. Entity portalled before it ever created a clone. But will need one for the interpolated origin history
		if( C_PortalGhostRenderable::ShouldCloneEntity( pEntity, pPortal, false ) )
		{
			pGhost = C_PortalGhostRenderable::CreateGhostRenderable( pEntity, pPortal, pEntity == C_BasePlayer::GetLocalPlayer() );
			if( pGhost )
			{
				Assert( !pPortal->m_hGhostingEntities.IsValidIndex( pPortal->m_hGhostingEntities.Find( pEntity ) ) );
				pPortal->m_hGhostingEntities.AddToTail( pEntity );
				Assert( pPortal->m_GhostRenderables.IsValidIndex( pPortal->m_GhostRenderables.Find( pGhost ) ) );
				pGhost->PerFrameUpdate();
			}
		}
	}

	if( pGhost )
	{
		C_PortalGhostRenderable::CreateInversion( pGhost, pPortal, fTime );
	}
#endif
	if( pEntity->IsPlayer() )
	{
		((C_Portal_Player *)pEntity)->PlayerPortalled( pPortal, fTime, bForcedDuck );
		return;
	}	

	pEntity->AddEFlags( EFL_DIRTY_ABSTRANSFORM );

	VMatrix matTransform = pPortal->MatrixThisToLinked();

	CDiscontinuousInterpolatedVar< QAngle > &rotInterp = pEntity->GetRotationInterpolator();
	CDiscontinuousInterpolatedVar< Vector > &posInterp = pEntity->GetOriginInterpolator();


	if( cl_portal_teleportation_interpolation_fixup_method.GetInt() == 0 )
	{
		UTIL_TransformInterpolatedAngle( rotInterp, matTransform.As3x4(), fTime );
		UTIL_TransformInterpolatedPosition( posInterp, matTransform, fTime );
	}
	else
	{
		rotInterp.InsertDiscontinuity( matTransform.As3x4(), fTime );
		posInterp.InsertDiscontinuity( matTransform.As3x4(), fTime );
	}

	if ( pEntity->IsToolRecording() )
	{
		static EntityTeleportedRecordingState_t state;

		KeyValues *msg = new KeyValues( "entity_teleported" );
		msg->SetPtr( "state", &state );
		state.m_bTeleported = true;
		state.m_bViewOverride = false;
		state.m_vecTo = pEntity->GetAbsOrigin();
		state.m_qaTo = pEntity->GetAbsAngles();
		state.m_teleportMatrix = matTransform.As3x4();

		// Post a message back to all IToolSystems
		Assert( (int)pEntity->GetToolHandle() != 0 );
		ToolFramework_PostToolMessage( pEntity->GetToolHandle(), msg );

		msg->deleteThis();
	}
#if 0
	C_Portal_Player* pPlayer = C_Portal_Player::GetLocalPortalPlayer();
	if ( pPlayer && pEntity == pPlayer->GetAttachedObject() )
	{
		C_BaseAnimating *pAnim = pEntity->GetBaseAnimating();	
		if ( pAnim && pAnim->IsUsingRenderOriginOverride() )
		{
			pPlayer->ResetHeldObjectOutOfEyeTransitionDT();
		}
	}
#endif
}


struct PortalTeleportationLogEntry_t
{
	CHandle<C_BaseEntity> hEntity;
	CHandle<C_Prop_Portal> hPortal;
	float fTeleportTime;
	bool bForcedDuck;
};

static CThreadFastMutex s_PortalTeleportationLogMutex;
static CUtlVector<PortalTeleportationLogEntry_t> s_PortalTeleportationLog;

void RecieveEntityPortalledMessage( CHandle<C_BaseEntity> hEntity, CHandle<C_Prop_Portal> hPortal, float fTime, bool bForcedDuck )
{
	PortalTeleportationLogEntry_t temp;
	temp.hEntity = hEntity;
	temp.hPortal = hPortal;
	temp.fTeleportTime = fTime;
	temp.bForcedDuck = bForcedDuck;

	s_PortalTeleportationLogMutex.Lock();
	s_PortalTeleportationLog.AddToTail( temp );
	s_PortalTeleportationLogMutex.Unlock();
}

void ProcessPortalTeleportations( void )
{
	s_PortalTeleportationLogMutex.Lock();
	for( int i = 0; i != s_PortalTeleportationLog.Count(); ++i )
	{
		PortalTeleportationLogEntry_t &entry = s_PortalTeleportationLog[i];

		C_Prop_Portal *pPortal = entry.hPortal;
		if( pPortal == NULL )
			continue;

		//grab other entity's EHANDLE
		C_BaseEntity *pEntity = entry.hEntity;
		if( pEntity == NULL )
			continue;

		EntityPortalledMessageHandler( pEntity, pPortal, entry.fTeleportTime, entry.bForcedDuck );
	}
	s_PortalTeleportationLog.RemoveAll();
	s_PortalTeleportationLogMutex.Unlock();
}

void C_Prop_Portal::ClientThink( void )
{
	SetupPortalColorSet();
#ifndef DISABLE_CLONE_AREA
	if (m_pAttachedCloningArea)
		m_pAttachedCloningArea->ClientThink();
#endif
	if (m_bDoRenderThink)
	{
		bool bDidAnything = false;
	
		if( m_fStaticAmount > 0.0f )
		{
			m_fStaticAmount -= gpGlobals->absoluteframetime;
			if( m_fStaticAmount < 0.0f ) 
				m_fStaticAmount = 0.0f;

			bDidAnything = true;
		}
		if( m_fSecondaryStaticAmount > 0.0f )
		{
			m_fSecondaryStaticAmount -= gpGlobals->absoluteframetime;
			if( m_fSecondaryStaticAmount < 0.0f ) 
				m_fSecondaryStaticAmount = 0.0f;

			bDidAnything = true;
		}

		if( m_fOpenAmount < 1.0f )
		{
			m_fOpenAmount += gpGlobals->absoluteframetime * 2.0f;
			if( m_fOpenAmount > 1.0f ) 
				m_fOpenAmount = 1.0f;

			bDidAnything = true;
		}
	
		if( bDidAnything == false )
		{
			m_bDoRenderThink = false;
			//SetNextClientThink( CLIENT_THINK_NEVER );
		}
	}
}

void C_Prop_Portal::Simulate()
{
	BaseClass::Simulate();
	
	//clear list of ghosted entities from last frame, and clear the clipping planes we put on them
	for( int i = m_hGhostingEntities.Count(); --i >= 0; )
	{
		C_BaseEntity *pEntity = m_hGhostingEntities[i].Get();

		if( pEntity != NULL )
			pEntity->m_bEnableRenderingClipPlane = false;
	}
	m_hGhostingEntities.RemoveAll();


	if( !IsActivedAndLinked() )
	{
		//remove all ghost renderables
		for( int i = m_GhostRenderables.Count(); --i >= 0; )
		{
			delete m_GhostRenderables[i];
		}
		
		m_GhostRenderables.RemoveAll();

		return;
	}



	//Find objects that are intersecting the portal and mark them for later replication on the remote portal's side
	C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPlayer();
	C_BaseViewModel *pLocalPlayerViewModel = pLocalPlayer->GetViewModel();

	C_BaseEntity *pEntsNearPortal[1024];
	int iEntsNearPortal = UTIL_EntitiesInSphere( pEntsNearPortal, 1024, GetNetworkOrigin(), PORTAL_HALF_HEIGHT, 0, PARTITION_CLIENT_NON_STATIC_EDICTS );
		
	if( iEntsNearPortal != 0 )
	{
		float fClipPlane[4];
		fClipPlane[0] = m_plane_Origin.x;
		fClipPlane[1] = m_plane_Origin.y;
		fClipPlane[2] = m_plane_Origin.z;
		fClipPlane[3] = m_plane_Origin.w - 0.3f;
		
		for( int i = 0; i != iEntsNearPortal; ++i )
		{
			C_BaseEntity *pEntity = pEntsNearPortal[i];
			Assert( pEntity != NULL );

			// Don't ghost ghosts.
			//if ( dynamic_cast<C_PortalGhostRenderable*>( pEntity ) != NULL )
			//	continue;		

			bool bIsMovable = false;

			C_BaseEntity *pMoveEntity = pEntity;
			MoveType_t moveType = MOVETYPE_NONE;

			//unmoveables and doors can never get halfway in the portal
			while ( pMoveEntity )
			{
				moveType = pMoveEntity->GetMoveType();

				if ( !( moveType == MOVETYPE_NONE || moveType == MOVETYPE_PUSH ) )
				{
					bIsMovable = true;
					pMoveEntity = NULL;
				}
				else
					pMoveEntity = pMoveEntity->GetMoveParent();
			}

			if ( !bIsMovable )
				continue;

			Assert( dynamic_cast<C_Prop_Portal *>(pEntity) == NULL ); //should have been killed with (pEntity->GetMoveType() == MOVETYPE_NONE) check. Infinite recursion is infinitely bad.

			if( pEntity == pLocalPlayerViewModel )
				continue; //avoid ghosting view models
			
			bool bActivePlayerWeapon = false;

			C_BaseCombatWeapon *pWeapon = dynamic_cast<C_BaseCombatWeapon*>( pEntity );
			if ( pWeapon )
			{
				C_Portal_Player *pPortalPlayer = ToPortalPlayer( pWeapon->GetOwner() );
				if ( pPortalPlayer ) 
				{
					if ( pPortalPlayer->GetActiveWeapon() != pWeapon )
						continue; // don't ghost player owned non selected weapons
					else
						bActivePlayerWeapon = true;
				}
			}

			Vector ptEntCenter = pEntity->WorldSpaceCenter();
			if( bActivePlayerWeapon )
				ptEntCenter = pWeapon->GetOwner()->WorldSpaceCenter();

			if( (m_plane_Origin.AsVector3D().Dot( ptEntCenter ) - m_plane_Origin.w) < -5.0f )
				continue; //entity is behind the portal, most likely behind the wall the portal is placed on

			if( !CProp_Portal_Shared::IsEntityTeleportable( pEntity ) )
				continue;

			if ( bActivePlayerWeapon )
			{
				if( !m_PortalSimulator.EntityHitBoxExtentIsInPortalHole( pWeapon->GetOwner(), false ) && 
					!m_PortalSimulator.EntityHitBoxExtentIsInPortalHole( pWeapon, false ) )
					continue;
			}
			else if( pEntity->IsPlayer() )
			{
				if( !m_PortalSimulator.EntityHitBoxExtentIsInPortalHole( (C_BaseAnimating*)pEntity, false ) )
					continue;
			}
			else
			{
				if( !m_PortalSimulator.EntityIsInPortalHole( pEntity ) )
					continue;
			}

			pEntity->m_bEnableRenderingClipPlane = true;
			memcpy( pEntity->m_fRenderingClipPlane, fClipPlane, sizeof( float ) * 4 );

			EHANDLE hEnt = pEntity;
			m_hGhostingEntities.AddToTail( hEnt );
		}
	}

#if 1
	//now, fix up our list of ghosted renderables.
	{
		//The local player doesn't get a ghost renderable for some reason, hack time!
		
		Vector vPortalMins, vPortalMaxs;
		CollisionProp()->WorldSpaceAABB( &vPortalMins, &vPortalMaxs );

		Vector vPlayerMins, vPlayerMaxs;
		pLocalPlayer->CollisionProp()->WorldSpaceAABB( &vPlayerMins, &vPlayerMaxs );
		if ( IsBoxIntersectingBox( vPortalMins, vPortalMaxs, vPlayerMins, vPlayerMaxs ) )
		{
			m_hGhostingEntities.AddToTail( pLocalPlayer );

			if ( pLocalPlayer->GetActiveWeapon() )
				m_hGhostingEntities.AddToTail( pLocalPlayer->GetActiveWeapon() );

		}

		bool *bStillInUse = (bool *)stackalloc( sizeof( bool ) * (m_GhostRenderables.Count() + m_hGhostingEntities.Count()) );
		memset( bStillInUse, 0, sizeof( bool ) * (m_GhostRenderables.Count() + m_hGhostingEntities.Count()) );

		for( int i = m_hGhostingEntities.Count(); --i >= 0; )
		{
			C_BaseEntity *pRenderable = m_hGhostingEntities[i].Get();

			int j;
			for( j = m_GhostRenderables.Count(); --j >= 0; )
			{
				if( pRenderable == m_GhostRenderables[j]->m_pGhostedRenderable )
				{
					bStillInUse[j] = true;
					m_GhostRenderables[j]->PerFrameUpdate();
					break;
				}
			}
			
			if ( j >= 0 )
				continue;

			//newly added
			C_BaseEntity *pEntity = m_hGhostingEntities[i];

			bool bIsHeldWeapon = false;
			C_BaseCombatWeapon *pWeapon = dynamic_cast<C_BaseCombatWeapon*>( pEntity );
			if ( pWeapon && ToPortalPlayer( pWeapon->GetOwner() ) )
				bIsHeldWeapon = true;

			
			C_PortalGhostRenderable *pNewGhost = new C_PortalGhostRenderable( this,
																				pRenderable, 
																				pEntity->GetRenderGroup(), 
																				m_matrixThisToLinked, 
																				m_fGhostRenderablesClip,
																				(pEntity == pLocalPlayer || bIsHeldWeapon) );
			Assert( pNewGhost );
			
			bStillInUse[ m_GhostRenderables.AddToTail( pNewGhost ) ] = true;
			pNewGhost->PerFrameUpdate();
			
			// HACK - I just copied the CClientTools::OnEntityCreated code here,
			// since the ghosts aren't really entities - they don't have an entindex,
			// they're not in the entitylist, and they get created during Simulate(),
			// which isn't valid for real entities, since it changes the simulate list
			// -jd
			if ( ToolsEnabled() && clienttools->IsInRecordingMode() )
			{
				// Send deletion message to tool interface
				KeyValues *kv = new KeyValues( "created" );
				HTOOLHANDLE h = clienttools->AttachToEntity( pNewGhost );
				ToolFramework_PostToolMessage( h, kv );

				kv->deleteThis();
			}
			
		}

		//remove unused ghosts
		for ( int i = m_GhostRenderables.Count(); --i >= 0; )
		{
			if ( bStillInUse[i] )
				continue;

			// HACK - I just copied the CClientTools::OnEntityDeleted code here,
			// since the ghosts aren't really entities - they don't have an entindex,
			// they're not in the entitylist, and they get created during Simulate(),
			// which isn't valid for real entities, since it changes the simulate list
			// -jd
			C_PortalGhostRenderable *pGhost = m_GhostRenderables[i];
			if ( ToolsEnabled() )
			{
				HTOOLHANDLE handle = pGhost ? pGhost->GetToolHandle() : (HTOOLHANDLE)0;
				if ( handle != (HTOOLHANDLE)0 )
				{
					if ( clienttools->IsInRecordingMode() )
					{
						// Send deletion message to tool interface
						KeyValues *kv = new KeyValues( "deleted" );
						ToolFramework_PostToolMessage( handle, kv );
						kv->deleteThis();
					}

					clienttools->DetachFromEntity( pGhost );
				}
			}

			delete pGhost;
			m_GhostRenderables.FastRemove( i );
		}
	}

#endif

	//ensure the shared clip plane is up to date
	C_Prop_Portal *pLinkedPortal = m_hLinkedPortal.Get();

	m_fGhostRenderablesClip[0] = pLinkedPortal->m_plane_Origin.x;
	m_fGhostRenderablesClip[1] = pLinkedPortal->m_plane_Origin.y;
	m_fGhostRenderablesClip[2] = pLinkedPortal->m_plane_Origin.z;
	m_fGhostRenderablesClip[3] = pLinkedPortal->m_plane_Origin.w - 0.75f;
}


C_PortalGhostRenderable *C_Prop_Portal::GetGhostRenderableForEntity( C_BaseEntity *pEntity )
{
	//Disable this for now
	for( int i = 0; i != m_GhostRenderables.Count(); ++i )
	{
		if( m_GhostRenderables[i]->m_pGhostedRenderable == pEntity )
			return m_GhostRenderables[i];
	}

	return NULL;
}

void C_Prop_Portal::UpdateOnRemove( void )
{
	if( TransformedLighting.m_LightShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->DestroyFlashlight( TransformedLighting.m_LightShadowHandle );
		TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	}

	g_pPortalRender->RemovePortal( this );

	DestroyAttachedParticles();
#ifndef DISABLE_CLONE_AREA
	if( m_pAttachedCloningArea )
	{
		delete m_pAttachedCloningArea;
		m_pAttachedCloningArea = NULL;
	}
#endif
	C_Prop_Portal *pRemote = m_hLinkedPortal;
	if (pRemote != NULL)
	{
		m_PortalSimulator.DetachFromLinked();
		m_hLinkedPortal = NULL;
		SetActive(false);
		m_bOldActivatedState = false;
	}

	BaseClass::UpdateOnRemove();
}

void C_Prop_Portal::OnRestore( void )
{
	BaseClass::OnRestore();
#ifndef DISABLE_CLONE_AREA
	Assert( m_pAttachedCloningArea == NULL );
	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );
#endif
	if (ShouldCreateAttachedParticles())
	{
		CreateAttachedParticles();
	}
}


void C_Prop_Portal::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( Q_stricmp( pszParticleName, "portal_1_overlap" ) == 0 || Q_stricmp( pszParticleName, "portal_2_overlap" ) == 0 )
	{
		float fClosestDistanceSqr = -1.0f;
		Vector vClosestPosition;

		int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
		if( iPortalCount != 0 )
		{
			C_Prop_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
			for( int i = 0; i != iPortalCount; ++i )
			{
				C_Prop_Portal *pTempPortal = pPortals[i];
				if ( pTempPortal != this && pTempPortal->IsActive() )
				{
					Vector vPosition = pTempPortal->GetAbsOrigin();

					float fDistanceSqr = pNewParticleEffect->GetRenderOrigin().DistToSqr( vPosition );

					if ( fClosestDistanceSqr == -1.0f || fClosestDistanceSqr > fDistanceSqr )
					{
						fClosestDistanceSqr = fDistanceSqr;
						vClosestPosition = vPosition;
					}
				}
			}
		}

		if ( fClosestDistanceSqr != -1.0f )
		{
			pNewParticleEffect->SetControlPoint( 1, vClosestPosition );
		}
	}
}

void C_Prop_Portal::OnPreDataChanged( DataUpdateType_t updateType )
{
	//PreDataChanged.m_matrixThisToLinked = m_matrixThisToLinked;
	PreDataChanged.m_bIsPortal2 = m_bIsPortal2;
	PreDataChanged.m_bActivated = m_bActivated;
	PreDataChanged.m_bOldActivatedState = m_bOldActivatedState;
	PreDataChanged.m_vOrigin = m_ptOrigin;
	PreDataChanged.m_qAngles = m_qAbsAngle;
	PreDataChanged.m_hLinkedTo = m_hLinkedPortal.Get();

	BaseClass::OnPreDataChanged( updateType );
}

//ConVar r_portal_light_innerangle( "r_portal_light_innerangle", "90.0", FCVAR_CLIENTDLL );
//ConVar r_portal_light_outerangle( "r_portal_light_outerangle", "90.0", FCVAR_CLIENTDLL );
//ConVar r_portal_light_forward( "r_portal_light_forward", "0.0", FCVAR_CLIENTDLL );

void C_Prop_Portal::HandleNetworkChanges( void )
{
	C_Prop_Portal *pRemote = m_hLinkedPortal;
	m_pLinkedPortal = pRemote;

	//get absolute origin and angles, but cut out interpolation, use the network position and angles as transformed by any move parent
	{		
		ALIGN16 matrix3x4_t finalMatrix;
		AngleMatrix( m_qAbsAngle, finalMatrix );

		MatrixGetColumn( finalMatrix, 0, m_vForward );
		MatrixGetColumn( finalMatrix, 1, m_vRight );
		MatrixGetColumn( finalMatrix, 2, m_vUp );
		m_vRight = -m_vRight;
	}

	const PS_PlacementData_t &placement = m_PortalSimulator.GetInternalData().Placement;

	bool bActivityChanged = PreDataChanged.m_bActivated != IsActive();
	bool bPortalMoved = ((/*(m_ptOrigin != PreDataChanged.m_vOrigin) &&*/ (m_ptOrigin != placement.ptCenter)) || //moved
							(/*(m_qAbsAngle != PreDataChanged.m_qAngles) &&*/ (m_qAbsAngle != placement.qAngles)) ||  //rotated
							(PreDataChanged.m_bIsPortal2 != m_bIsPortal2) ); //swapped portal id
							
	bool bNewLinkage = ( (PreDataChanged.m_hLinkedTo.Get() != m_hLinkedPortal.Get()) );

	if( bNewLinkage )
		m_PortalSimulator.DetachFromLinked(); //detach now so moves are theoretically faster
	
	if( IsActive() )
	{
		//generic stuff we'll need
		g_pPortalRender->AddPortal( this ); //will know if we're already added and avoid adding twice
		 
		if( bPortalMoved || bActivityChanged )
		{
			m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );
		}

		if( pRemote )
			m_PortalSimulator.AttachTo( &pRemote->m_PortalSimulator );
	}
	else
	{
		g_pPortalRender->RemovePortal( this );
		m_PortalSimulator.DetachFromLinked();

		if( bPortalMoved || bActivityChanged )
		{
			m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );
		}
	}

	if( bNewLinkage || bPortalMoved || bActivityChanged )
	{
		//Warning_SpewCallStack( 10, "C_Portal_Base2D::HandleNetworkChanges( %.2f )\n", gpGlobals->curtime );
		PortalMoved(); //updates link matrix and internals
		UpdateTeleportMatrix();

		if ( bPortalMoved )
		{
			OnPortalMoved();
		}

		if( bActivityChanged )
		{
			OnActiveStateChanged();
		}

		if( bNewLinkage )
		{
			OnLinkageChanged( PreDataChanged.m_hLinkedTo.Get() );
		}

		UpdateGhostRenderables();
		if( pRemote )
			pRemote->UpdateGhostRenderables();
	}
}

void C_Prop_Portal::OnDataChanged( DataUpdateType_t updateType )
{	
	BaseClass::OnDataChanged( updateType );
	
	if ( GetPredictable() && !ShouldPredict() )
		ShutdownPredictable();

	bool bActivityChanged = PreDataChanged.m_bActivated != IsActive();

	HandleNetworkChanges();

	if ( bActivityChanged )
	{
		UpdateVisibility();
	}

	if (m_iPortalColorSet != m_iOldPortalColorSet)
	{
		DestroyAttachedParticles();

		if (ShouldCreateAttachedParticles())
		{
			CreateAttachedParticles();
		}

		m_iOldPortalColorSet = m_iPortalColorSet;
	}
}

void C_Prop_Portal::UpdateGhostRenderables( void )
{
	//lastly, update all ghost renderables
	for( int i = m_GhostRenderables.Count(); --i >= 0; )
	{
		m_GhostRenderables[i]->m_matGhostTransform = m_matrixThisToLinked;;
	}
}

extern ConVar building_cubemaps;

int C_Prop_Portal::DrawModel( int flags )
{
	// Don't draw in cube maps because it makes an ugly colored splotch
	if( IsActive() == false || building_cubemaps.GetBool() )
		return 0;

	int iRetVal = 0;

	C_Prop_Portal *pLinkedPortal = m_hLinkedPortal.Get();

	if ( pLinkedPortal == NULL )
	{
		//SetNextClientThink( CLIENT_THINK_ALWAYS ); // we need this to help fade out
		m_bDoRenderThink = true;
	}
	
	SetupPortalColorSet();
	
	if ( !g_pPortalRender->ShouldUseStencilsToRenderPortals() )
	{				
		DrawPortal();
	}


	if( WillUseDepthDoublerThisDraw() )
		m_fSecondaryStaticAmount = 0.0f;

	iRetVal = BaseClass::DrawModel( flags );
	
	return iRetVal;
}


//-----------------------------------------------------------------------------
// Handle recording for the SFM
//-----------------------------------------------------------------------------
void C_Prop_Portal::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "C_Prop_Portal::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );
	BaseClass::GetToolRecordingState( m_bActivated, msg );

	if ( !IsActive() )
	{
		BaseEntityRecordingState_t *pBaseEntity = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );
		pBaseEntity->m_bVisible = false;
	}
}

void C_Prop_Portal::UpdateTeleportMatrix( void )
{
	//setup our origin plane
	m_plane_Origin.x = m_vForward.x;
	m_plane_Origin.y = m_vForward.y;
	m_plane_Origin.z = m_vForward.z;
	//GetVectors( &m_plane_Origin.AsVector3D(), NULL, NULL );
	m_plane_Origin.w = m_plane_Origin.AsVector3D().Dot( m_ptOrigin );

	UTIL_Portal_ComputeMatrix( this, m_pLinkedPortal );
}

void C_Prop_Portal::SetIsPortal2( bool bValue )
{
	m_bIsPortal2 = bValue;
}

bool C_Prop_Portal::IsActivedAndLinked( void ) const
{
	if (IsActive() && m_hLinkedPortal.Get() != NULL)
		return ( true );
	
	return false;
}



void C_Prop_Portal::Touch(C_BaseEntity *pOther)
{
	if ( pOther->IsPlayer() )
	{
		return;
	}

	BaseClass::Touch( pOther );
	pOther->Touch( this );

	// Don't do anything on touch if it's not active
	if( !IsActive() || (m_hLinkedPortal.Get() == NULL) )
	{
		Assert( !m_PortalSimulator.OwnsEntity( pOther ) );
		Assert( !pOther->IsPlayer() || (((CPortal_Player *)pOther)->m_hPortalEnvironment.Get() != this) );
		
		//I'd really like to fix the root cause, but this will keep the game going
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
		return;
	}

	//see if we should even be interacting with this object, this is a bugfix where some objects get added to physics environments through walls
	if( !m_PortalSimulator.OwnsEntity( pOther ) )
	{
		//hmm, not in our environment, plane tests, sharing tests
		if( SharedEnvironmentCheck( pOther ) )
		{
			bool bObjectCenterInFrontOfPortal	= (m_plane_Origin.AsVector3D().Dot( pOther->WorldSpaceCenter() ) > m_plane_Origin.w);
			bool bIsStuckPlayer					= ( pOther->IsPlayer() )? ( !UTIL_IsSpaceEmpty( pOther, pOther->WorldAlignMins(), pOther->WorldAlignMaxs() ) ) : ( false );

			if ( bIsStuckPlayer )
			{
				Assert ( !"Player stuck" );
				DevMsg( "Player in solid behind behind portal %i's plane, Adding to it's environment to run find closest passable space.\n", ((m_bIsPortal2)?(2):(1)) );
			}

			if ( bObjectCenterInFrontOfPortal || bIsStuckPlayer )
			{
				if( sv_portal_debug_touch.GetBool() )
				{
					DevMsg( "Portal %i took control of shared object: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() );
				}

				//we should be interacting with this object, add it to our environment
				CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
				if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
					pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

				m_PortalSimulator.TakeOwnershipOfEntity( pOther );
			}
		}
		else
		{
			return; //we shouldn't interact with this object
		}
	}

	if( ShouldTeleportTouchingEntity( pOther ) )
		TeleportTouchingEntity( pOther );
}

void C_Prop_Portal::StartTouch(C_BaseEntity *pOther)
{
	if ( pOther->IsPlayer() )
	{
		return;
	}

	BaseClass::StartTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back start touch, so I'm forcing it
	pOther->StartTouch( this );
	
	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "(client)Portal %i StartTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime );
	}

	if( (m_hLinkedPortal == NULL) || (IsActive() == false) )
		return;

	if( CProp_Portal_Shared::IsEntityTeleportable( pOther ) )
	{
		CCollisionProperty *pOtherCollision = pOther->CollisionProp();
		Vector vWorldMins, vWorldMaxs;
		pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
		Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

		if( m_plane_Origin.AsVector3D().Dot( ptOtherCenter ) > m_plane_Origin.w )
		{
			//we should be interacting with this object, add it to our environment
			if( SharedEnvironmentCheck( pOther ) )
			{
				Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
					(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

				CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
				if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
					pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

				m_PortalSimulator.TakeOwnershipOfEntity( pOther );
			}
		}
	}
}

void C_Prop_Portal::EndTouch(C_BaseEntity *pOther)
{
	if ( pOther->IsPlayer() )
	{
		return;
	}

	BaseClass::EndTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back end touch, so I'm forcing it
	pOther->EndTouch( this );

	// Don't do anything on end touch if it's not active
	if( !IsActive() || ((m_hLinkedPortal.Get() != NULL) )  )
	{
		return;
	}
	
	if( ShouldTeleportTouchingEntity( pOther ) ) //an object passed through the plane and all the way out of the touch box
		TeleportTouchingEntity( pOther );
	else if( pOther->IsPlayer() && //player
			(m_PortalSimulator.GetInternalData().Placement.vForward.z < -0.7071f) && //most likely falling out of the portal
			(m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Normal.Dot( pOther->WorldSpaceCenter() ) < m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Dist) && //but behind the portal plane
			(((CPortal_Player *)pOther)->m_Local.m_bInDuckJump) ) //while ducking
	{
		//TeleportTouchingEntity( pOther );
	}
	else
	{
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
	}
}

bool C_Prop_Portal::SharedEnvironmentCheck(C_BaseEntity *pEntity)
{
	Assert(((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) ||
		(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator)); //make sure this entity is linked to the same portal as our simulator

	CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity(pEntity);
	if ((pOwningSimulator == NULL) || (pOwningSimulator == &m_PortalSimulator))
	{
		//nobody else is claiming ownership
		return true;
	}

	Vector ptCenter = pEntity->WorldSpaceCenter();
	if ((ptCenter - m_PortalSimulator.GetInternalData().Placement.ptCenter).LengthSqr() < (ptCenter - pOwningSimulator->GetInternalData().Placement.ptCenter).LengthSqr())
		return true;

	/*if( !m_hLinkedPortal->m_PortalSimulator.EntityIsInPortalHole( pEntity ) )
	{
		Vector vOtherVelocity;
		pEntity->GetVelocity( &vOtherVelocity );

		if( vOtherVelocity.Dot( m_PortalSimulator.GetInternalData().Placement.vForward ) < vOtherVelocity.Dot( m_hLinkedPortal->m_PortalSimulator.GetInternalData().Placement.vForward ) )
			return true; //entity is going towards this portal more than the other
	}*/
	return false;

	//we're in the shared configuration, and the other portal already owns the object, see if we'd be a better caretaker (distance check
	/*CCollisionProperty *pEntityCollision = pEntity->CollisionProp();
	Vector vWorldMins, vWorldMaxs;
	pEntityCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
	Vector ptEntityCenter = (vWorldMins + vWorldMaxs) / 2.0f;

	Vector vEntToThis = GetAbsOrigin() - ptEntityCenter;
	Vector vEntToRemote = m_hLinkedPortal->GetAbsOrigin() - ptEntityCenter;

	return ( vEntToThis.LengthSqr() < vEntToRemote.LengthSqr() );*/
}


bool C_Prop_Portal::ShouldCreateAttachedParticles(void)
{	
	if (IsActive())
	{
		if ( !m_hEdgeEffect && !m_hEdgeEffect.IsValid() )
		{
			return true;
		}
		if ( !m_hParticleEffect && !m_hParticleEffect.IsValid())
		{
			return true;
		}
	}
	return false;
}

void C_Prop_Portal::NewLocation( const Vector &vOrigin, const QAngle &qAngles )
{
	// Don't fizzle me if I moved from a location another portalgun shot
//	if (m_pPortalReplacingMe)
//		m_pPortalReplacingMe->m_pHitPortal = NULL;
	
	if( IsActive() )
	{
		CProp_Portal *pLink = m_hLinkedPortal.Get();

		if( !(pLink && pLink->IsActive()) )
		{
			//no old link, or inactive old link
			
			if( pLink )
			{
				Vector vLinkOrigin = pLink->m_ptOrigin;
				QAngle qLinkAngles = pLink->m_qAbsAngle;

				//we had an old link, must be inactive
				if( pLink->m_hLinkedPortal.Get() != NULL )
					pLink->NewLocation( vLinkOrigin, qLinkAngles );

				pLink = NULL;
			}

			int iPortalCount = s_PortalLinkageGroups[m_iLinkageGroupID].Count();

			if( iPortalCount != 0 )
			{
				CProp_Portal **pPortals = s_PortalLinkageGroups[m_iLinkageGroupID].Base();
				for( int i = 0; i != iPortalCount; ++i )
				{
					CProp_Portal *pCurrentPortal = pPortals[i];
					if( pCurrentPortal == this )
						continue;
					if( pCurrentPortal->IsActive() && pCurrentPortal->m_hLinkedPortal.Get() == NULL )
					{
						Vector vCurrentOrigin = pCurrentPortal->m_ptOrigin;
						QAngle qCurrentAngles = pCurrentPortal->m_qAbsAngle;

						pLink = pCurrentPortal;
						pCurrentPortal->m_hLinkedPortal = this;
						pCurrentPortal->NewLocation( vCurrentOrigin, qCurrentAngles );
						break;
					}
				}
			}
		}
	

		m_hLinkedPortal = pLink;

		if( pLink != NULL )
		{
			CHandle<CProp_Portal> hThis = this;
			CHandle<CProp_Portal> hRemote = pLink;

			this->m_hLinkedPortal = hRemote;
			pLink->m_hLinkedPortal = hThis;
			m_bIsPortal2 = !m_hLinkedPortal->m_bIsPortal2;
		}
	}
	else
	{
		m_hLinkedPortal = NULL;
	}

	//Warning( "C_Portal_Base2D::NewLocation(client) %f     %.2f %.2f %.2f\n", gpGlobals->curtime, XYZ( vOrigin ) );

	//get absolute origin and angles, but cut out interpolation, use the network position and angles as transformed by any move parent
	{
		ALIGN16 matrix3x4_t finalMatrix;
		AngleMatrix( qAngles, finalMatrix );

		MatrixGetColumn( finalMatrix, 0, m_vForward );
		MatrixGetColumn( finalMatrix, 1, m_vRight );
		MatrixGetColumn( finalMatrix, 2, m_vUp );
		m_vRight = -m_vRight;

		m_ptOrigin = vOrigin;
		m_qAbsAngle = qAngles;
	}

	AddEffects( EF_NOINTERP );
	//PredictClearNoInterpEffect();
	
	// The Abs origin must be set for the sounds, otherwise they play in the wrong spot
	SetAbsOrigin( vOrigin );

	if( GetMoveParent() )
	{
		SetAbsAngles( qAngles );
	}
	else
	{
		SetNetworkOrigin( vOrigin );
		SetNetworkAngles( qAngles );
	}
	GetOriginInterpolator().ClearHistory();
	GetRotationInterpolator().ClearHistory();
	
	if( IsActive() == false )
	{
		SetActive( true );
		OnActiveStateChanged();
	}

	m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );

	m_pLinkedPortal = m_hLinkedPortal;
	
	PortalMoved(); //updates link matrix and internals
	UpdatePortalLinkage();
	OnPortalMoved();
	UpdateTeleportMatrix();

	UpdateGhostRenderables();

	C_Prop_Portal *pRemote = m_hLinkedPortal.Get();
	if( pRemote )
		pRemote->UpdateGhostRenderables();

	g_pPortalRender->AddPortal( this ); //will know if we're already added and avoid adding twice
	
	if ( prediction->IsFirstTimePredicted() )
	{
		if ( m_bIsPortal2 )
		{
			EmitSound( "Portal.open_red" );
		}
		else
		{
			EmitSound( "Portal.open_blue" );
		}
	}
}

void C_Prop_Portal::DoFizzleEffect( int iEffect, int iLinkageGroupID, bool bDelayedPos /*= true*/ )
{	
	Assert( prediction->InPrediction() );
	if ( !prediction->IsFirstTimePredicted() )
		return;
	if ( use_server_portal_particles.GetBool() )
		return;

	Vector m_vAudioOrigin = ( ( bDelayedPos ) ? ( m_vDelayedPosition ) : ( GetAbsOrigin() ) );


	Vector vecOrigin = ( ( bDelayedPos ) ? ( m_vDelayedPosition ) : ( GetAbsOrigin() ) );
	QAngle qAngles = ( ( bDelayedPos ) ? ( m_qDelayedAngles ) : ( GetAbsAngles() ) );

	
	EmitSound_t ep;
	CPASAttenuationFilter filter( m_vDelayedPosition );

	ep.m_nChannel = CHAN_STATIC;
	ep.m_flVolume = 1.0f;
	ep.m_pOrigin = &m_vAudioOrigin;
	
	CEffectData	fxData;

	fxData.m_vAngles = ( ( bDelayedPos ) ? ( m_qDelayedAngles ) : ( GetAbsAngles() ) );

	Vector vForward, vUp;
	AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
	fxData.m_vOrigin = vecOrigin + vForward * 1.0f;

	fxData.m_nColor = ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) );
		
//	if ( iEffect != PORTAL_FIZZLE_SUCCESS && iEffect != PORTAL_FIZZLE_CLOSE )
//		iEffect = PORTAL_FIZZLE_BAD_SURFACE;

	SetupPortalColorSet();
	
	// Pick a fizzle effect
	switch ( iEffect )
	{
		case PORTAL_FIZZLE_CANT_FIT:
			//DispatchEffect( "PortalFizzleCantFit", fxData );
			//ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_nofit" ) : ( "portal_lightblue_nofit" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_nofit" ) : ( "portal_yellow_nofit" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_nofit" ) : ( "portal_green_nofit" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_nofit" ) : ( "portal_1_nofit" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			break;

		case PORTAL_FIZZLE_OVERLAPPED_LINKED:
		{
			/*CProp_Portal *pLinkedPortal = m_hLinkedPortal;
			if ( pLinkedPortal )
			{
				Vector vLinkedForward;
				pLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
				fxData.m_vStart = pLink3edPortal->GetAbsOrigin() + vLinkedForward * 5.0f;
			}*/

			//DispatchEffect( "PortalFizzleOverlappedLinked", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_overlap" ) : ( "portal_lightblue_overlap" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_overlap" ) : ( "portal_yellow_overlap" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_overlap" ) : ( "portal_green_overlap" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_overlap" ) : ( "portal_1_overlap" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_BAD_VOLUME:
			//DispatchEffect( "PortalFizzleBadVolume", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_badvolume" ) : ( "portal_lightblue_badvolume" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_badvolume" ) : ( "portal_yellow_badvolume" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_badvolume" ) : ( "portal_green_badvolume" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_badvolume" ) : ( "portal_1_badvolume" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_BAD_SURFACE:
			//DispatchEffect( "PortalFizzleBadSurface", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_badsurface" ) : ( "portal_lightblue_badsurface" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_badsurface" ) : ( "portal_yellow_badsurface" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_badsurface" ) : ( "portal_green_badsurface" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_badsurface" ) : ( "portal_1_badsurface" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
			
		case PORTAL_FIZZLE_KILLED:
			//DispatchEffect( "PortalFizzleKilled", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_close" ) : ( "portal_lightblue_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_close" ) : ( "portal_yellow_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_close" ) : ( "portal_green_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_close" ) : ( "portal_1_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_moved";
			break;

		case PORTAL_FIZZLE_CLEANSER:
			//DispatchEffect( "PortalFizzleCleanser", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_cleanser" ) : ( "portal_lightblue_cleanser" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_cleanser" ) : ( "portal_yellow_cleanser" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_cleanser" ) : ( "portal_green_cleanser" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_cleanser" ) : ( "portal_1_cleanser" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_CLOSE:
			//DispatchEffect( "PortalFizzleKilled", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_close" ) : ( "portal_lightblue_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_close" ) : ( "portal_yellow_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_close" ) : ( "portal_green_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_close" ) : ( "portal_1_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = ( ( m_bIsPortal2 ) ? ( "Portal.close_red" ) : ( "Portal.close_blue" ) );
			break;

		case PORTAL_FIZZLE_NEAR_BLUE:
		{
			Vector vLinkedForward;
			m_hLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
			fxData.m_vOrigin = m_hLinkedPortal->GetAbsOrigin() + vLinkedForward * 16.0f;
			fxData.m_vAngles = m_hLinkedPortal->GetAbsAngles();
			

			//DispatchEffect( "PortalFizzleNear", fxData );
			AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			
			if (iLinkageGroupID == 1)
				DispatchParticleEffect( ( ( "portal_lightblue_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (iLinkageGroupID == 2)
				DispatchParticleEffect( ( ( "portal_yellow_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (iLinkageGroupID == 3)
				DispatchParticleEffect( ( ( "portal_green_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else
				DispatchParticleEffect( ( ( "portal_1_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_NEAR_RED:
		{
			
			Vector vLinkedForward;
			m_hLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
			fxData.m_vOrigin = m_hLinkedPortal->GetAbsOrigin() + vLinkedForward * 16.0f;
			fxData.m_vAngles = m_hLinkedPortal->GetAbsAngles();
			

			//DispatchEffect( "PortalFizzleNear", fxData );
			AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (iLinkageGroupID == 1)
				DispatchParticleEffect( ( ( "portal_purple_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (iLinkageGroupID == 2)
				DispatchParticleEffect( ( ( "portal_red_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (iLinkageGroupID == 3)
				DispatchParticleEffect( ( ( "portal_pink_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else
				DispatchParticleEffect( ( ( "portal_2_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_SUCCESS:
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_success" ) : ( "portal_lightblue_success" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 2)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_success" ) : ( "portal_yellow_success" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 3)
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_success" ) : ( "portal_green_success" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_success" ) : ( "portal_1_success" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			// Don't make a sound!
			return;
			
		case PORTAL_FIZZLE_STOLEN:
			//DispatchEffect( "PortalFizzleBadSurface", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			if (m_iPortalColorSet == 1) // Purple / Light Blue
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_stolen" ) : ( "portal_lightblue_stolen" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 2) // Red / Yellow
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_stolen" ) : ( "portal_yellow_stolen" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else if (m_iPortalColorSet == 3) // Pink / Green
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_stolen" ) : ( "portal_green_stolen" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			else // Default colors
				DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_stolen" ) : ( "portal_1_stolen" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_NONE:
			// Don't do anything!
			return;
	}
	
	EmitSound( filter, SOUND_FROM_WORLD, ep );

	VectorAngles( vUp, vForward, qAngles );
}

void C_Prop_Portal::Fizzle( void )
{
	CProp_Portal *pRemotePortal = m_hLinkedPortal;

	DestroyAttachedParticles();
	//StopParticleEffects( this );

	SetActive(false);
	m_hLinkedPortal = NULL;
	m_PortalSimulator.DetachFromLinked();
	m_PortalSimulator.ReleaseAllEntityOwnership();

	if( pRemotePortal )
	{
		//pRemotePortal->m_hLinkedPortal = NULL;
		pRemotePortal->OnLinkageChanged(pRemotePortal);
	}
}


bool C_Prop_Portal::ShouldPredict( void )
{
	CBasePlayer *pOwner = GetPredictionOwner();
	if ( pOwner && pOwner->IsLocalPlayer() )
		return true;

	return BaseClass::ShouldPredict();
}

C_BasePlayer *C_Prop_Portal::GetPredictionOwner( void )
{
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer)
			continue;
		
		C_WeaponPortalgun *pPortalgun = assert_cast<C_WeaponPortalgun*>( pPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );
		if (!pPortalgun)
			continue;
		
		if ( pPortalgun->m_iPortalLinkageGroupID != m_iLinkageGroupID )
		{
			continue;
		}

		// FIXME:
		// This is needed to prevent unowned portals from just "popping" without an opening animation
		// but enabling this code breaks predicted portal views with unpredicted portals!
		/*if ( m_bIsPortal2 )
		{
			if ( !pPortalgun->CanFirePortal2() )
				continue;
		}
		else
		{
			if ( !pPortalgun->CanFirePortal1() )
				continue;
		}*/

		return pPlayer;
	}

	return NULL;
}

bool C_Prop_Portal::LocalPlayerCanPlace( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;
	
	C_WeaponPortalgun *pPortalgun = assert_cast<C_WeaponPortalgun*>( pPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );
	if ( !pPortalgun )
		return false;

	if ( m_iLinkageGroupID != pPortalgun->m_iPortalLinkageGroupID )
		return false;

	if ( m_bIsPortal2 )
	{
		if ( pPortalgun->CanFirePortal2() )
			return true;
	}
	else
	{
		if ( pPortalgun->CanFirePortal1() )
			return true;
	}

	return false;
}

void C_Prop_Portal::OnPortalMoved( void )
{
	if( IsActive() )
	{
		if( (!GetPredictable() || !LocalPlayerCanPlace()) || (prediction->InPrediction() && prediction->IsFirstTimePredicted()) )
		{
			m_fOpenAmount = 0.0f;
			//SetNextClientThink( CLIENT_THINK_ALWAYS ); //we need this to help open up
			m_bDoRenderThink = true;
				
			C_Prop_Portal *pRemote = (C_Prop_Portal *)m_hLinkedPortal.Get();
			//add static to the remote
			if( pRemote )
			{
				pRemote->m_fStaticAmount = 1.0f; // This will cause the other portal to show the static effect
				//pRemote->SetNextClientThink( CLIENT_THINK_ALWAYS );
				pRemote->m_bDoRenderThink = true;
			}
		}
	}
}

void C_Prop_Portal::OnActiveStateChanged( void )
{
	if( IsActive() )
	{
		CreateAttachedParticles();
		/*
		Msg("Portal %i ID: %i is now active.\n", m_bIsPortal2 ? 2 : 1, m_iLinkageGroupID );

		IsPortalOverlappingOtherPortals( this, GetAbsOrigin(), GetAbsAngles(), true );
		*/

		// UpdateTransformedLighting();
		if( (!GetPredictable() || !LocalPlayerCanPlace()) || (prediction->InPrediction() && prediction->IsFirstTimePredicted()) )
		{
			m_fOpenAmount = 0.0f;
			m_fStaticAmount = 1.0f;
			//SetNextClientThink( CLIENT_THINK_ALWAYS ); //we need this to help open up
			m_bDoRenderThink = true;

			C_Prop_Portal *pRemote = (C_Prop_Portal *)m_hLinkedPortal.Get();
			//add static to the remote
			if( pRemote )
			{
				pRemote->m_fStaticAmount = 1.0f; // This will cause the other portal to show the static effect
				//pRemote->SetNextClientThink( CLIENT_THINK_ALWAYS );
				pRemote->m_bDoRenderThink = true;
			}			
		}
	}
	else
	{	
		if( TransformedLighting.m_pEntityLight )
		{
			TransformedLighting.m_pEntityLight->die = gpGlobals->curtime;
			TransformedLighting.m_pEntityLight = NULL;
		}

		// the portal closed
		DestroyAttachedParticles();

		/*
		if( TransformedLighting.m_LightShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
		{
			g_pClientShadowMgr->DestroyFlashlight( TransformedLighting.m_LightShadowHandle );
			TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
		}
		*/
	}
}


void C_Prop_Portal::OnLinkageChanged( C_Prop_Portal *pOldLinkage )
{
	if ( IsActive() )
	{
		CreateAttachedParticles();
	}

	if( m_hLinkedPortal.Get() != NULL )
	{
	//	UpdateTransformedLighting();
	}
	/*
	else
	{
		if( TransformedLighting.m_pEntityLight )
		{
			TransformedLighting.m_pEntityLight->die = gpGlobals->curtime;
			TransformedLighting.m_pEntityLight = NULL;
		}

		if( TransformedLighting.m_LightShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
		{
			g_pClientShadowMgr->DestroyFlashlight( TransformedLighting.m_LightShadowHandle );
			TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
		}
	}
	*/
}


void C_Prop_Portal::CreateAttachedParticles( void )
{
	DestroyAttachedParticles();

	SetupPortalColorSet();

	if ( use_server_portal_particles.GetBool() )
		return;

	// create a new effect for this portal
	mdlcache->BeginLock();
	if (m_iPortalColorSet == 1)
		m_hEdgeEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_purple_edge"  :  "portal_lightblue_edge"  , PATTACH_POINT_FOLLOW, "particlespin" );
	else if (m_iPortalColorSet == 2)
		m_hEdgeEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_red_edge"  :  "portal_yellow_edge"  , PATTACH_POINT_FOLLOW, "particlespin" );
	else if (m_iPortalColorSet == 3)
		m_hEdgeEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_pink_edge"  :  "portal_green_edge"  , PATTACH_POINT_FOLLOW, "particlespin" );
	else
		m_hEdgeEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_2_edge"  :  "portal_1_edge"  , PATTACH_POINT_FOLLOW, "particlespin" );
			
	if (m_iPortalColorSet == 1)
		m_hParticleEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_purple_particles"  :  "portal_lightblue_particles"  , PATTACH_POINT_FOLLOW, "particles_2" );
	else if (m_iPortalColorSet == 2)
		m_hParticleEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_red_particles"  :  "portal_yellow_particles"  , PATTACH_POINT_FOLLOW, "particles_2" );
	else if (m_iPortalColorSet == 3)
		m_hParticleEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_pink_particles"  :  "portal_green_particles"  , PATTACH_POINT_FOLLOW, "particles_2" );
	else
		m_hParticleEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_2_particles" : "portal_1_particles", PATTACH_POINT_FOLLOW, "particles_2" );
	
	mdlcache->EndLock();
}

void C_Prop_Portal::DestroyAttachedParticles( void )
{
	// If there's already a different stream particle effect, get rid of it.
	// Shut down our effect if we have it
	if ( m_hEdgeEffect && m_hEdgeEffect.IsValid() )
	{
		ParticleProp()->StopEmission( m_hEdgeEffect, false, true);
		m_hEdgeEffect = NULL;
	}
	if (m_hParticleEffect && m_hParticleEffect.IsValid())
	{
		ParticleProp()->StopEmission(m_hParticleEffect, false, true);
		m_hParticleEffect = NULL;
	}
}

void C_Prop_Portal::HandlePredictionError( bool bErrorInThisEntity )
{		
	if( bErrorInThisEntity 
		|| !LocalPlayerCanPlace() // Pretty bad but it'll have to do
		) 
	{
		HandleNetworkChanges();
	}

	BaseClass::HandlePredictionError( bErrorInThisEntity );
	if( bErrorInThisEntity )
	{
		if( IsActive() )
		{
			if ( ShouldCreateAttachedParticles() )
			{
				CreateAttachedParticles();
			}
		}
		else
		{
			DestroyAttachedParticles();
		}
	}
}