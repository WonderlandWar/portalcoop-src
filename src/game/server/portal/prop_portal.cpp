//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "prop_portal.h"
#include "portal_player.h"
#include "portal/weapon_physcannon.h"
#include "physics_npc_solver.h"
#include "envmicrophone.h"
#include "env_speaker.h"
#include "func_portal_detector.h"
#include "model_types.h"
#include "te_effect_dispatch.h"
#include "collisionutils.h"
#include "physobj.h"
#include "world.h"
#include "hierarchy.h"
#include "physics_saverestore.h"
#include "PhysicsCloneArea.h"
#include "portal_gamestats.h"
#include "prop_portal_shared.h"
#include "weapon_portalgun.h"
#include "portal_placement.h"
#include "physicsshadowclone.h"
#include "particle_parse.h"
#include "rumble_shared.h"
#include "func_portal_orientation.h"
#include "env_debughistory.h"
#include "tier1/callqueue.h"
#include "filters.h"
#include <string>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCallQueue *GetPortalCallQueue();


ConVar sv_portal_debug_touch("sv_portal_debug_touch", "0", FCVAR_REPLICATED );
ConVar sv_portal_placement_never_fail("sv_portal_placement_never_fail", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar sv_portal_new_velocity_check("sv_portal_new_velocity_check", "1", FCVAR_REPLICATED | FCVAR_CHEAT);
extern ConVar use_server_portal_particles;

CUtlVector<CProp_Portal *> s_PortalLinkageGroups[256];

extern ConVar sv_allow_customized_portal_colors;

BEGIN_DATADESC( CProp_Portal )
	//saving
	DEFINE_FIELD( m_hLinkedPortal,		FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iLinkageGroupID,	FIELD_CHARACTER,	"LinkageGroupID" ),
	DEFINE_FIELD( m_matrixThisToLinked, FIELD_VMATRIX ),
	DEFINE_KEYFIELD( m_bActivated,		FIELD_BOOLEAN,		"Activated" ),
	DEFINE_KEYFIELD( m_bOldActivatedState,		FIELD_BOOLEAN,		"OldActivated" ),	
	DEFINE_KEYFIELD( m_bIsPortal2,		FIELD_BOOLEAN,		"PortalTwo" ),
	DEFINE_FIELD( m_vPrevForward,		FIELD_VECTOR ),
	DEFINE_FIELD( m_hFiredByPlayer,		FIELD_EHANDLE ),
#ifndef DONT_USE_MICROPHONESORSPEAKERS
	DEFINE_FIELD( m_hMicrophone,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_hSpeaker,			FIELD_EHANDLE ),
#endif
	DEFINE_SOUNDPATCH( m_pAmbientSound ),

	DEFINE_FIELD( m_vAudioOrigin,		FIELD_VECTOR ),
	DEFINE_FIELD( m_vDelayedPosition,	FIELD_VECTOR ),
	DEFINE_FIELD( m_qDelayedAngles,		FIELD_VECTOR ),
	DEFINE_FIELD( m_iDelayedFailure,	FIELD_INTEGER ),
	DEFINE_FIELD( m_hPlacedBy,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_iCustomPortalColorSet,	FIELD_INTEGER ),

	// DEFINE_FIELD( m_plane_Origin, cplane_t ),
	// DEFINE_FIELD( m_pAttachedCloningArea, CPhysicsCloneArea ),
	// DEFINE_FIELD( m_PortalSimulator, CPortalSimulator ),
	// DEFINE_FIELD( m_pCollisionShape, CPhysCollide ),
	
	DEFINE_FIELD( m_bSharedEnvironmentConfiguration, FIELD_BOOLEAN ),
	DEFINE_ARRAY( m_vPortalCorners, FIELD_POSITION_VECTOR, 4 ),

	// Function Pointers
	DEFINE_THINKFUNC( DelayedPlacementThink ),
	DEFINE_THINKFUNC( TestRestingSurfaceThink ),
	DEFINE_THINKFUNC( FizzleThink ),

	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetActivatedState", InputSetActivatedState ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Fizzle", InputFizzle ),
	DEFINE_INPUTFUNC( FIELD_STRING, "NewLocation", InputNewLocation ),

	DEFINE_OUTPUT( m_OnPlacedSuccessfully, "OnPlacedSuccessfully" ),
	DEFINE_OUTPUT( m_OnEntityTeleportFromMe, "OnEntityTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportFromMe, "OnPlayerTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnEntityTeleportToMe, "OnEntityTeleportToMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportToMe, "OnPlayerTeleportToMe" ),
	
	DEFINE_OUTPUT( m_OnFizzled, "OnFizzled" ),
	DEFINE_OUTPUT( m_OnStolen, "OnStolen" ),

END_DATADESC()

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
extern void SendProxy_Angles( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

IMPLEMENT_SERVERCLASS_ST( CProp_Portal, DT_Prop_Portal )

	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropVector( SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropVector( SENDINFO(m_angRotation), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT, SendProxy_Angles ),
	SendPropInt( SENDINFO(m_iLinkageGroupID)),
	
	//if we're resting on another entity, we still need ultra-precise absolute coords. We should probably downgrade local origin/angles in favor of these
	SendPropVector( SENDINFO(m_ptOrigin), -1,  SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),
	SendPropVector( SENDINFO(m_qAbsAngle), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),

	SendPropEHandle( SENDINFO(m_hLinkedPortal) ),
	SendPropBool( SENDINFO(m_bActivated) ),
	SendPropBool( SENDINFO(m_bOldActivatedState) ),
	SendPropBool( SENDINFO(m_bIsPortal2) ),
	SendPropEHandle( SENDINFO( m_hPlacedBy ) ),
	SendPropEHandle( SENDINFO( m_hFiredByPlayer ) ),
	SendPropInt( SENDINFO( m_nPlacementAttemptParity ), EF_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iCustomPortalColorSet ), EF_PARITY_BITS, SPROP_UNSIGNED ),
	
	SendPropDataTable( SENDINFO_DT( m_PortalSimulator ), &REFERENCE_SEND_TABLE( DT_PortalSimulator ) )
END_SEND_TABLE()


LINK_ENTITY_TO_CLASS( prop_portal, CProp_Portal );






CProp_Portal::CProp_Portal( void )
{
	m_vPrevForward = Vector( 0.0f, 0.0f, 0.0f );
	m_vAudioOrigin = Vector( 0.0f, 0.0f, 0.0f );
	m_PortalSimulator.SetPortalSimulatorCallbacks( this );

	// Init to something safe
	for ( int i = 0; i < 4; ++i )
	{
		m_vPortalCorners[i] = Vector(0,0,0);
	}

	//create the collision shape.... TODO: consider having one shared collideable between all portals
	float fPlanes[6*4];
	fPlanes[(0*4) + 0] = 1.0f;
	fPlanes[(0*4) + 1] = 0.0f;
	fPlanes[(0*4) + 2] = 0.0f;
	fPlanes[(0*4) + 3] = CProp_Portal_Shared::vLocalMaxs.x;

	fPlanes[(1*4) + 0] = -1.0f;
	fPlanes[(1*4) + 1] = 0.0f;
	fPlanes[(1*4) + 2] = 0.0f;
	fPlanes[(1*4) + 3] = -CProp_Portal_Shared::vLocalMins.x;

	fPlanes[(2*4) + 0] = 0.0f;
	fPlanes[(2*4) + 1] = 1.0f;
	fPlanes[(2*4) + 2] = 0.0f;
	fPlanes[(2*4) + 3] = CProp_Portal_Shared::vLocalMaxs.y;

	fPlanes[(3*4) + 0] = 0.0f;
	fPlanes[(3*4) + 1] = -1.0f;
	fPlanes[(3*4) + 2] = 0.0f;
	fPlanes[(3*4) + 3] = -CProp_Portal_Shared::vLocalMins.y;

	fPlanes[(4*4) + 0] = 0.0f;
	fPlanes[(4*4) + 1] = 0.0f;
	fPlanes[(4*4) + 2] = 1.0f;
	fPlanes[(4*4) + 3] = CProp_Portal_Shared::vLocalMaxs.z;

	fPlanes[(5*4) + 0] = 0.0f;
	fPlanes[(5*4) + 1] = 0.0f;
	fPlanes[(5*4) + 2] = -1.0f;
	fPlanes[(5*4) + 3] = -CProp_Portal_Shared::vLocalMins.z;

	CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, 0.00001f, true );
	Assert( pPolyhedron != NULL );
	CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
	pPolyhedron->Release();
	Assert( pConvex != NULL );
	m_pCollisionShape = physcollision->ConvertConvexToCollide( &pConvex, 1 );

	CProp_Portal_Shared::AllPortals.AddToTail( this );
}

CProp_Portal::~CProp_Portal( void )
{
	CProp_Portal_Shared::AllPortals.FindAndRemove( this );
	s_PortalLinkageGroups[m_iLinkageGroupID].FindAndRemove( this );
}


void CProp_Portal::UpdateOnRemove( void )
{
	m_PortalSimulator.ClearEverything();
#ifndef DONT_USE_MICROPHONESORSPEAKERS
	RemovePortalMicAndSpeaker();
#endif
	CProp_Portal *pRemote = m_hLinkedPortal;
	if( pRemote != NULL )
	{
		m_PortalSimulator.DetachFromLinked();
		m_hLinkedPortal = NULL;
		SetActive( false );
		m_bOldActivatedState = false;
		pRemote->UpdatePortalLinkage();
		pRemote->UpdatePortalTeleportMatrix();
	}

	if( m_pAttachedCloningArea )
	{
		UTIL_Remove( m_pAttachedCloningArea );
		m_pAttachedCloningArea = NULL;
	}
	

	BaseClass::UpdateOnRemove();
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

	PrecacheParticleSystem("portal_1_particles");
	PrecacheParticleSystem("portal_2_particles");
	PrecacheParticleSystem("portal_1_edge");
	PrecacheParticleSystem("portal_2_edge");
	PrecacheParticleSystem("portal_1_nofit");
	PrecacheParticleSystem("portal_2_nofit");
	PrecacheParticleSystem("portal_1_overlap");
	PrecacheParticleSystem("portal_2_overlap");
	PrecacheParticleSystem("portal_1_badvolume");
	PrecacheParticleSystem("portal_2_badvolume");
	PrecacheParticleSystem("portal_1_badsurface");
	PrecacheParticleSystem("portal_2_badsurface");
	PrecacheParticleSystem("portal_1_close");
	PrecacheParticleSystem("portal_2_close");
	PrecacheParticleSystem("portal_1_cleanser");
	PrecacheParticleSystem("portal_2_cleanser");
	PrecacheParticleSystem("portal_1_near");
	PrecacheParticleSystem("portal_2_near");
	PrecacheParticleSystem("portal_1_success");
	PrecacheParticleSystem("portal_2_success");
	PrecacheParticleSystem("portal_1_stolen");
	PrecacheParticleSystem("portal_2_stolen");

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

void CProp_Portal::CreateSounds()
{
	if (!m_pAmbientSound)
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pAmbientSound = controller.SoundCreate( filter, entindex(), "Portal.ambient_loop" );
		controller.Play( m_pAmbientSound, 0, 100 );
	}
}

void CProp_Portal::StopLoopingSounds()
{
	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundDestroy( m_pAmbientSound );
		m_pAmbientSound = NULL;
	}

	BaseClass::StopLoopingSounds();
}

void CProp_Portal::Spawn( void )
{
	Precache();

	UpdateCollisionShape();

	SetupPortalColorSet();

	Assert( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) == -1 );
	s_PortalLinkageGroups[m_iLinkageGroupID].AddToTail( this );

	SetTransmitState(FL_EDICT_ALWAYS);

	m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	
	AddEffects( EF_NORECEIVESHADOW | EF_NOSHADOW );

	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID | FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	//VPhysicsInitNormal( SOLID_VPHYSICS, FSOLID_TRIGGER, false );
	//CreateVPhysics();
	ResetModel();	
	SetSize( CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs );

	UpdateCorners();

	BaseClass::Spawn();

	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );
}

void CProp_Portal::OnRestore()
{
	m_ptOrigin = GetAbsOrigin();
	m_qAbsAngle = GetAbsAngles();

	UpdateCorners();

	Assert( m_pAttachedCloningArea == NULL );
	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );

	BaseClass::OnRestore();

	SetupPortalColorSet();

	if ( IsActive() && use_server_portal_particles.GetBool() )
	{
		if (m_iPortalColorSet == 1)
		{
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_particles" ) : ( "portal_lightblue_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_edge" ) : ( "portal_lightblue_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
		else if (m_iPortalColorSet == 2)
		{			
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_particles" ) : ( "portal_yellow_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_edge" ) : ( "portal_yellow_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
		else if (m_iPortalColorSet == 3)
		{			
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_particles" ) : ( "portal_green_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_edge" ) : ( "portal_green_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
		else
		{
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_particles" ) : ( "portal_1_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_edge" ) : ( "portal_1_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
	}
}

void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName );
void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );

bool CProp_Portal::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if ( !m_pCollisionShape )
	{
		//HACK: This is a last-gasp type fix for a crash caused by m_pCollisionShape not yet being set up
		// during a restore.
		UpdateCollisionShape();
	}

	physcollision->TraceBox( ray, MASK_ALL, NULL, m_pCollisionShape, GetAbsOrigin(), GetAbsAngles(), &tr );
	return tr.DidHit();
}
//-----------------------------------------------------------------------------
// Purpose: When placed on a surface that could potentially go away (anything but world geo), we test for that condition and fizzle
//-----------------------------------------------------------------------------
void CProp_Portal::TestRestingSurfaceThink( void )
{
	// Make sure there's still a surface behind the portal
	Vector vOrigin = GetAbsOrigin();

	Vector vForward, vRight, vUp;
	GetVectors( &vForward, &vRight, &vUp );

	trace_t tr;
	CTraceFilterSimpleClassnameList baseFilter( NULL, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	baseFilter.AddClassnameToIgnore( "prop_portal" );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );

	int iCornersOnVolatileSurface = 0;

	// Check corners
	for ( int iCorner = 0; iCorner < 4; ++iCorner )
	{
		Vector vCorner = vOrigin;

		if ( iCorner % 2 == 0 )
			vCorner += vRight * ( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS * 1.1f );
		else
			vCorner += -vRight * ( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS * 1.1f );

		if ( iCorner < 2 )
			vCorner += vUp * ( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS * 1.1f );
		else
			vCorner += -vUp * ( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS * 1.1f );

		Ray_t ray;
		ray.Init( vCorner, vCorner - vForward );
		enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilterPortalShot, &tr );

		// This corner isn't on a valid brush (skipping phys converts or physboxes because they frequently go through portals and can't be placed upon).
		if ( tr.fraction == 1.0f && !tr.startsolid && ( !tr.m_pEnt || ( tr.m_pEnt && !FClassnameIs( tr.m_pEnt, "func_physbox" ) && !FClassnameIs( tr.m_pEnt, "simple_physics_brush" ) ) ) )
		{
			DevMsg( "Surface removed from behind portal.\n" );
			Fizzle();
			SetContextThink( NULL, TICK_NEVER_THINK, s_pTestRestingSurfaceContext );
			break;
		}

		if ( !tr.DidHitWorld() )
		{
			iCornersOnVolatileSurface++;
		}
	}

	// Still on a movable or deletable surface
	if ( iCornersOnVolatileSurface > 0 )
	{
		SetContextThink ( &CProp_Portal::TestRestingSurfaceThink, gpGlobals->curtime + 0.1f, s_pTestRestingSurfaceContext );
	}
	else
	{
		// All corners on world, we don't need to test
		SetContextThink( NULL, TICK_NEVER_THINK, s_pTestRestingSurfaceContext );
	}
}

bool CProp_Portal::IsActivedAndLinked( void ) const
{
	return ( IsActive() && m_hLinkedPortal.Get() != NULL );
}

void CProp_Portal::ResetModel( void )
{
	if( !m_bIsPortal2 )
		SetModel( "models/portals/portal1.mdl" );
	else
		SetModel( "models/portals/portal2.mdl" );

	SetSize( CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs );

	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID | FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST );
}

void CProp_Portal::DoFizzleEffect( int iEffect, int iLinkageGroupID, bool bDelayedPos /*= true*/ )
{
	m_vAudioOrigin = ( ( bDelayedPos ) ? ( m_vDelayedPosition ) : ( GetAbsOrigin() ) );

	CEffectData	fxData;

	fxData.m_vAngles = ( ( bDelayedPos ) ? ( m_qDelayedAngles ) : ( GetAbsAngles() ) );

	Vector vForward, vUp;
	AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
	fxData.m_vOrigin = m_vAudioOrigin + vForward * 1.0f;

	fxData.m_nColor = ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) );

	EmitSound_t ep;
	CPASAttenuationFilter filter( m_vDelayedPosition );

	ep.m_nChannel = CHAN_STATIC;
	ep.m_flVolume = 1.0f;
	ep.m_pOrigin = &m_vAudioOrigin;

	// Rumble effects on the firing player (if one exists)
	CWeaponPortalgun *pPortalGun = ( m_hPlacedBy.Get() );

	if ( pPortalGun && (iEffect != PORTAL_FIZZLE_CLOSE ) 
				    && (iEffect != PORTAL_FIZZLE_SUCCESS )
				    && (iEffect != PORTAL_FIZZLE_NONE )		)
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pPortalGun->GetOwner();
		if ( pPlayer )
		{
			pPlayer->RumbleEffect( RUMBLE_PORTAL_PLACEMENT_FAILURE, 0, RUMBLE_FLAGS_NONE );
		}
	}

	// Just a little hack to always prevent particle inconsistencies.
	SetupPortalColorSet();

	bool bFilterUsePredictionRules = true;
		
	// Pick a fizzle effect
	switch ( iEffect )
	{
		case PORTAL_FIZZLE_CANT_FIT:
		{
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
		}
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
		{
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
		}
		case PORTAL_FIZZLE_BAD_SURFACE:
		{
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
		}
		case PORTAL_FIZZLE_KILLED:
		{
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

			bFilterUsePredictionRules = false;
			break;
		}
		case PORTAL_FIZZLE_CLEANSER:
		{
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
		}
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

	if ( bFilterUsePredictionRules )
	{
		filter.UsePredictionRules();
	}

	EmitSound( filter, SOUND_FROM_WORLD, ep );
}

//-----------------------------------------------------------------------------
// Purpose: Fizzle the portal
//-----------------------------------------------------------------------------
void CProp_Portal::FizzleThink( void )
{
	CProp_Portal *pRemotePortal = m_hLinkedPortal;
#ifndef DONT_USE_MICROPHONESORSPEAKERS
	RemovePortalMicAndSpeaker();
#endif
	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pAmbientSound, 0.0, 0.0 );
	}

	StopParticleEffects( this );

	SetActive(false);
	m_hLinkedPortal = NULL;
	m_PortalSimulator.DetachFromLinked();
	m_PortalSimulator.ReleaseAllEntityOwnership();

	if( pRemotePortal )
	{
		//pRemotePortal->m_hLinkedPortal = NULL;
		pRemotePortal->UpdatePortalLinkage();
	}

	SetContextThink( NULL, TICK_NEVER_THINK, s_pFizzleThink );
}


//-----------------------------------------------------------------------------
// Purpose: Portal will fizzle next time we get to think
//-----------------------------------------------------------------------------
void CProp_Portal::Fizzle( void )
{
	CProp_Portal *pRemotePortal = m_hLinkedPortal;
#ifndef DONT_USE_MICROPHONESORSPEAKERS
	RemovePortalMicAndSpeaker();
#endif
	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pAmbientSound, 0.0, 0.0 );
	}

	StopParticleEffects( this );

	SetActive(false);
	m_hLinkedPortal = NULL;
	m_PortalSimulator.DetachFromLinked();
	m_PortalSimulator.ReleaseAllEntityOwnership();

	if( pRemotePortal )
	{
		//pRemotePortal->m_hLinkedPortal = NULL;
		pRemotePortal->UpdatePortalLinkage();
	}

	//SetContextThink( &CProp_Portal::FizzleThink, gpGlobals->curtime, s_pFizzleThink );
}

//-----------------------------------------------------------------------------
// Purpose: Removes the portal microphone and speakers. This is done in two places
//			(fizzle and UpdateOnRemove) so the code is consolidated here.
// Input  :  - 
//-----------------------------------------------------------------------------
#ifndef DONT_USE_MICROPHONESORSPEAKERS
void CProp_Portal::RemovePortalMicAndSpeaker()
{

	// Shut down microphone/speaker if they exist
	if ( m_hMicrophone )
	{
		CEnvMicrophone *pMicrophone = (CEnvMicrophone*)(m_hMicrophone.Get());
		if ( pMicrophone )
		{
			inputdata_t in;
			pMicrophone->InputDisable( in );
			UTIL_Remove( pMicrophone );
		}
		m_hMicrophone = 0;
	}

	if ( m_hSpeaker )
	{
		CSpeaker *pSpeaker = (CSpeaker *)(m_hSpeaker.Get());
		if ( pSpeaker )
		{
			// Remove the remote portal's microphone, as it references the speaker we're about to remove.
			if ( m_hLinkedPortal.Get() )
			{
				CProp_Portal* pRemotePortal =  m_hLinkedPortal.Get();
				if ( pRemotePortal->m_hMicrophone )
				{
					inputdata_t inputdata;
					inputdata.pActivator = this;
					inputdata.pCaller = this;
					CEnvMicrophone* pRemotePortalMic = dynamic_cast<CEnvMicrophone*>(pRemotePortal->m_hMicrophone.Get());
					if ( pRemotePortalMic )
					{
						pRemotePortalMic->Remove();
					}
				}
			}
			inputdata_t in;
			pSpeaker->InputTurnOff( in );
			UTIL_Remove( pSpeaker );
		}
		m_hSpeaker = 0;
	}
}
#endif
void CProp_Portal::PunchPenetratingPlayer( CBaseEntity *pPlayer )
{
#if 1
	if( m_PortalSimulator.IsReadyToSimulate() )
	{
		ICollideable *pCollideable = pPlayer->GetCollideable();
		if ( pCollideable )
		{
			Vector vMin, vMax;

			pCollideable->WorldSpaceSurroundingBounds( &vMin, &vMax );

			if ( UTIL_IsBoxIntersectingPortal( ( vMin + vMax ) / 2.0f, ( vMax - vMin ) / 2.0f, this ) )
			{
				Vector vForward;
				GetVectors( &vForward, 0, 0 );
				vForward *= 100.0f;
				pPlayer->VelocityPunch( vForward );
			}
		}
	}
#endif
}

void CProp_Portal::PunchAllPenetratingPlayers( void )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if( pPlayer )
			PunchPenetratingPlayer( pPlayer );
	}
}

void CProp_Portal::Activate( void )
{
	UpdateCollisionShape();

	if( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) == -1 )
		s_PortalLinkageGroups[m_iLinkageGroupID].AddToTail( this );

	if( m_pAttachedCloningArea == NULL )
		m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );

	UpdatePortalTeleportMatrix();
	
	UpdatePortalLinkage();

	BaseClass::Activate();

	CreateSounds();

	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW );

	if( IsActive() && (m_hLinkedPortal.Get() != NULL) )
	{
		Vector ptCenter = GetAbsOrigin();
		QAngle qAngles = GetAbsAngles();
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
}

void CProp_Portal::Touch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() )
		return;
	BaseClass::Touch( pOther );
	pOther->Touch( this );

	// Don't do anything on touch if it's not active
	if( !m_bActivated || (m_hLinkedPortal.Get() == NULL) )
	{
		Assert( !m_PortalSimulator.OwnsEntity( pOther ) );
		Assert( !pOther->IsPlayer() || (((CPortal_Player *)pOther)->m_hPortalEnvironment.Get() != this) );
		
		//I'd really like to fix the root cause, but this will keep the game going
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
		return;
	}

	Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
		(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

	// Fizzle portal with any moving brush
	Vector vVelocityCheck;
	AngularImpulse vAngularImpulseCheck;
	pOther->GetVelocity( &vVelocityCheck, &vAngularImpulseCheck );

	if( vVelocityCheck != vec3_origin || vAngularImpulseCheck != vec3_origin )
	{
		if ( modelinfo->GetModelType( pOther->GetModel() ) == mod_brush )
		{
			if ( !FClassnameIs( pOther, "func_physbox" ) && !FClassnameIs( pOther, "simple_physics_brush" ) )	// except CPhysBox
			{
				Vector vForward;
				GetVectors( &vForward, NULL, NULL );

				Vector vMin, vMax;
				pOther->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );

				if ( UTIL_IsBoxIntersectingPortal( ( vMin + vMax ) / 2.0f, ( vMax - vMin ) / 2.0f - Vector( 2.0f, 2.0f, 2.0f ), this, 0.0f ) )
				{
					DevMsg( "Moving brush intersected portal plane.\n" );

					DoFizzleEffect( PORTAL_FIZZLE_KILLED, GetLinkageGroup(), false );
					Fizzle();
				}
				else
				{
					Vector vOrigin = GetAbsOrigin();

					trace_t tr;

					UTIL_TraceLine( vOrigin, vOrigin - vForward * PORTAL_HALF_DEPTH, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

					// Something went wrong
					if ( tr.fraction == 1.0f && !tr.startsolid )
					{
						DevMsg( "Surface removed from behind portal.\n" );

						DoFizzleEffect( PORTAL_FIZZLE_KILLED, GetLinkageGroup(), false );
						Fizzle();
					}
					else if ( tr.m_pEnt && tr.m_pEnt->IsMoving() )
					{
						DevMsg( "Surface behind portal is moving.\n" );

						DoFizzleEffect( PORTAL_FIZZLE_KILLED, GetLinkageGroup(), false );
						Fizzle();
					}
				}
			}
		}
	}
	
	if( m_hLinkedPortal == NULL )
		return;

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
#if !defined ( DISABLE_DEBUG_HISTORY )
				if ( !IsMarkedForDeletion() )
				{
					ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i took control of shared object: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() ) );
				}
#endif

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

void CProp_Portal::StartTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() )
		return;
	BaseClass::StartTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back start touch, so I'm forcing it
	pOther->StartTouch( this );

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "Portal %i StartTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime );
	}
#if !defined ( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i StartTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime ) );
	}
#endif

	if( (m_hLinkedPortal == NULL) || (m_bActivated == false) )
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

void CProp_Portal::EndTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() )
		return;
	BaseClass::EndTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back end touch, so I'm forcing it
	pOther->EndTouch( this );

	// Don't do anything on end touch if it's not active
	if( !m_bActivated )
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
		//player has pulled their feet up (moving their center instantaneously) while falling downward out of the portal, send them back (probably only for a frame)
		
		DevMsg( "Player pulled feet above the portal they fell out of, postponing Releasing ownership\n" );
		//TeleportTouchingEntity( pOther );
	}
	else
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "Portal %i EndTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime );
	}

#if !defined( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i EndTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime ) );
	}
#endif
}

bool CProp_Portal::SharedEnvironmentCheck( CBaseEntity *pEntity )
{
	Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
		(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

	CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
	if( (pOwningSimulator == NULL) || (pOwningSimulator == &m_PortalSimulator) )
	{
		//nobody else is claiming ownership
		return true;
	}

	Vector ptCenter = pEntity->WorldSpaceCenter();
	if( (ptCenter - m_PortalSimulator.GetInternalData().Placement.ptCenter).LengthSqr() < (ptCenter - pOwningSimulator->GetInternalData().Placement.ptCenter).LengthSqr() )
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

void CProp_Portal::WakeNearbyEntities( void )
{
	CBaseEntity*	pList[ 1024 ];

	Vector vForward, vUp, vRight;
	GetVectors( &vForward, &vRight, &vUp );

	Vector ptOrigin = GetAbsOrigin();
	QAngle qAngles = GetAbsAngles();

	Vector ptOBBStart = ptOrigin;
	ptOBBStart += vForward * CProp_Portal_Shared::vLocalMins.x;
	ptOBBStart += vRight * CProp_Portal_Shared::vLocalMins.y;
	ptOBBStart += vUp * CProp_Portal_Shared::vLocalMins.z;


	vForward *= CProp_Portal_Shared::vLocalMaxs.x - CProp_Portal_Shared::vLocalMins.x;
	vRight *= CProp_Portal_Shared::vLocalMaxs.y - CProp_Portal_Shared::vLocalMins.y;
	vUp *= CProp_Portal_Shared::vLocalMaxs.z - CProp_Portal_Shared::vLocalMins.z;


	Vector vAABBMins, vAABBMaxs;
	vAABBMins = vAABBMaxs = ptOBBStart;

	for( int i = 1; i != 8; ++i )
	{
		Vector ptTest = ptOBBStart;
		if( i & (1 << 0) ) ptTest += vForward;
		if( i & (1 << 1) ) ptTest += vRight;
		if( i & (1 << 2) ) ptTest += vUp;

		if( ptTest.x < vAABBMins.x ) vAABBMins.x = ptTest.x;
		if( ptTest.y < vAABBMins.y ) vAABBMins.y = ptTest.y;
		if( ptTest.z < vAABBMins.z ) vAABBMins.z = ptTest.z;
		if( ptTest.x > vAABBMaxs.x ) vAABBMaxs.x = ptTest.x;
		if( ptTest.y > vAABBMaxs.y ) vAABBMaxs.y = ptTest.y;
		if( ptTest.z > vAABBMaxs.z ) vAABBMaxs.z = ptTest.z;
	}
	
	int count = UTIL_EntitiesInBox( pList, 1024, vAABBMins, vAABBMaxs, 0 );

	//Iterate over all the possible targets
	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pEntity = pList[i];

		if ( pEntity && (pEntity != this) )
		{
			CCollisionProperty *pEntCollision = pEntity->CollisionProp();
			Vector ptEntityCenter = pEntCollision->GetCollisionOrigin();

			//double check intersection at the OBB vs OBB level, we don't want to affect large piles of physics objects if we don't have to. It gets slow
			if( IsOBBIntersectingOBB( ptOrigin, qAngles, CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, 
				ptEntityCenter, pEntCollision->GetCollisionAngles(), pEntCollision->OBBMins(), pEntCollision->OBBMaxs() ) )
			{
				if( FClassnameIs( pEntity, "func_portal_detector" ) )
				{
					// It's a portal detector
					CFuncPortalDetector *pPortalDetector = static_cast<CFuncPortalDetector*>( pEntity );

					if (pPortalDetector->m_bShouldUseLinkageID)
					{
						if ( pPortalDetector->IsActive() && pPortalDetector->GetLinkageGroupID() == m_iLinkageGroupID )
						{
							// It's detecting this portal's group
							Vector vMin, vMax;
							pPortalDetector->CollisionProp()->WorldSpaceAABB( &vMin, &vMax );

							Vector vBoxCenter = ( vMin + vMax ) * 0.5f;
							Vector vBoxExtents = ( vMax - vMin ) * 0.5f;

							if ( UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, this ) )
							{
								// It's intersecting this portal
								if ( m_bIsPortal2 )
									pPortalDetector->m_OnStartTouchPortal2.FireOutput( this, pPortalDetector );
								else
									pPortalDetector->m_OnStartTouchPortal1.FireOutput( this, pPortalDetector );

								if ( IsActivedAndLinked() )
								{
									pPortalDetector->m_OnStartTouchLinkedPortal.FireOutput( this, pPortalDetector );

									if ( UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, m_hLinkedPortal ) )
									{
										pPortalDetector->m_OnStartTouchBothLinkedPortals.FireOutput( this, pPortalDetector );
									}
								}
							}
						}
					}
					else //if (!m_bShouldUseLinkageID)
					{
						if (pPortalDetector->IsActive())
						{
							// It's detecting this portal's group
							Vector vMin, vMax;
							pPortalDetector->CollisionProp()->WorldSpaceAABB(&vMin, &vMax);

							Vector vBoxCenter = (vMin + vMax) * 0.5f;
							Vector vBoxExtents = (vMax - vMin) * 0.5f;

							if (UTIL_IsBoxIntersectingPortal(vBoxCenter, vBoxExtents, this))
							{
								// It's intersecting this portal
								if (m_bIsPortal2)
									pPortalDetector->m_OnStartTouchPortal2.FireOutput(this, pPortalDetector);
								else
									pPortalDetector->m_OnStartTouchPortal1.FireOutput(this, pPortalDetector);

								if (IsActivedAndLinked())
								{
									pPortalDetector->m_OnStartTouchLinkedPortal.FireOutput(this, pPortalDetector);

									if (UTIL_IsBoxIntersectingPortal(vBoxCenter, vBoxExtents, m_hLinkedPortal))
									{
										pPortalDetector->m_OnStartTouchBothLinkedPortals.FireOutput(this, pPortalDetector);
									}
								}
							}
						}
					}

				}

				pEntity->WakeRestingObjects();
				//pEntity->SetGroundEntity( NULL );

				if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
				{
					IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

					if ( pPhysicsObject && pPhysicsObject->IsMoveable() )
					{
						pPhysicsObject->Wake();

						// If the target is debris, convert it to non-debris
						if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
						{
							// Interactive debris converts back to debris when it comes to rest
							pEntity->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
						}
					}
				}
			}
		}
	}
}

void CProp_Portal::ForceEntityToFitInPortalWall( CBaseEntity *pEntity )
{
	CCollisionProperty *pCollision = pEntity->CollisionProp();
	Vector vWorldMins, vWorldMaxs;
	pCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
	Vector ptCenter = pEntity->WorldSpaceCenter(); //(vWorldMins + vWorldMaxs) / 2.0f;
	Vector ptOrigin = pEntity->GetAbsOrigin();
	Vector vEntityCenterToOrigin = ptOrigin - ptCenter;


	Vector ptPortalCenter = GetAbsOrigin();
	Vector vPortalCenterToEntityCenter = ptCenter - ptPortalCenter;
	Vector vPortalForward;
	GetVectors( &vPortalForward, NULL, NULL );
	Vector ptProjectedEntityCenter = ptPortalCenter + ( vPortalForward * vPortalCenterToEntityCenter.Dot( vPortalForward ) );

	Vector ptDest;

	if ( m_PortalSimulator.IsReadyToSimulate() )
	{
		Ray_t ray;
		ray.Init( ptProjectedEntityCenter, ptCenter, vWorldMins - ptCenter, vWorldMaxs - ptCenter );

		trace_t ShortestTrace;
		ShortestTrace.fraction = 2.0f;

		if( m_PortalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes.pCollideable )
		{
			physcollision->TraceBox( ray, m_PortalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes.pCollideable, vec3_origin, vec3_angle, &ShortestTrace );
		}

		/*if( pEnvironment->LocalCollide.pWorldCollide )
		{
			trace_t TempTrace;
			physcollision->TraceBox( ray, pEnvironment->LocalCollide.pWorldCollide, vec3_origin, vec3_angle, &TempTrace );
			if( TempTrace.fraction < ShortestTrace.fraction )
				ShortestTrace = TempTrace;
		}

		if( pEnvironment->LocalCollide.pWallShellCollide )
		{
			trace_t TempTrace;
			physcollision->TraceBox( ray, pEnvironment->LocalCollide.pWallShellCollide, vec3_origin, vec3_angle, &TempTrace );
			if( TempTrace.fraction < ShortestTrace.fraction )
				ShortestTrace = TempTrace;
		}

		if( pEnvironment->LocalCollide.pRemoteWorldWallCollide )
		{
			trace_t TempTrace;
			physcollision->TraceBox( ray, pEnvironment->LocalCollide.pRemoteWorldWallCollide, vec3_origin, vec3_angle, &TempTrace );
			if( TempTrace.fraction < ShortestTrace.fraction )
				ShortestTrace = TempTrace;
		}

		//Add displacement checks here too?

		*/

		if( ShortestTrace.fraction < 2.0f )
		{
			Vector ptNewPos = ShortestTrace.endpos + vEntityCenterToOrigin;
			pEntity->Teleport( &ptNewPos, NULL, NULL );
			pEntity->IncrementInterpolationFrame();
#if !defined ( DISABLE_DEBUG_HISTORY )
			if ( !IsMarkedForDeletion() )
			{
				ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Teleporting %s inside 'ForceEntityToFitInPortalWall'\n", pEntity->GetDebugName() ) );
			}
#endif
			if( sv_portal_debug_touch.GetBool() )
			{
				DevMsg( "Teleporting %s inside 'ForceEntityToFitInPortalWall'\n", pEntity->GetDebugName() );
			}
			//pEntity->SetAbsOrigin( ShortestTrace.endpos + vEntityCenterToOrigin );
		}
	}	
}

void CProp_Portal::UpdatePortalTeleportMatrix( void )
{
	//copied from client to ensure the numbers match as closely as possible.
	{		
		ALIGN16 matrix3x4_t finalMatrix;
		if( GetMoveParent() )
		{
			// Construct the entity-to-world matrix
			// Start with making an entity-to-parent matrix
			ALIGN16 matrix3x4_t matEntityToParent;
			AngleMatrix( GetLocalAngles(), matEntityToParent );
			MatrixSetColumn( GetLocalOrigin(), 3, matEntityToParent );

			// concatenate with our parent's transform
			ALIGN16 matrix3x4_t scratchMatrix;
			ConcatTransforms( GetParentToWorldTransform( scratchMatrix ), matEntityToParent, finalMatrix );

			MatrixGetColumn( finalMatrix, 0, m_vForward );
			MatrixGetColumn( finalMatrix, 1, m_vRight );
			MatrixGetColumn( finalMatrix, 2, m_vUp );
			Vector vTempOrigin;
			MatrixGetColumn( finalMatrix, 3, vTempOrigin );
			m_ptOrigin = vTempOrigin;
			m_vRight = -m_vRight;

			QAngle qTempAngle;
			MatrixAngles( finalMatrix, qTempAngle );
			m_qAbsAngle = qTempAngle;
		}
		else
		{
			AngleMatrix( m_qAbsAngle, finalMatrix );
			MatrixGetColumn( finalMatrix, 0, m_vForward );
			MatrixGetColumn( finalMatrix, 1, m_vRight );
			MatrixGetColumn( finalMatrix, 2, m_vUp );
			m_vRight = -m_vRight;
		}		
	}

	ResetModel();

	//setup our origin plane
	GetVectors( &m_plane_Origin.AsVector3D(), NULL, NULL );
	m_plane_Origin.w = m_plane_Origin.AsVector3D().Dot( m_ptOrigin );
	
	if ( m_hLinkedPortal != NULL )
	{
		CProp_Portal_Shared::UpdatePortalTransformationMatrix( EntityToWorldTransform(), m_hLinkedPortal->EntityToWorldTransform(), &m_matrixThisToLinked );

		m_hLinkedPortal->ResetModel();
		//update the remote portal
		MatrixInverseTR( m_matrixThisToLinked, m_hLinkedPortal->m_matrixThisToLinked );
	}
	else
	{
		m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	}
}

void CProp_Portal::CreatePortalMicAndSpeakers( void )
{
	// Don't use microphones in Rexaura! Many of the button sounds, timers, etc... are louder so both players can hear them, but they're way too loud for Rexaura
	bool bUseMicrophones = sv_portal_game.GetInt() != PORTAL_GAME_REXAURA;

	if ( bUseMicrophones )
	{
		// Initialize mics/speakers

		int iLinkageGroupID = m_iLinkageGroupID;
			
		char tspeakername1[64];
		char tspeakername2[64];
		char tmicname1[64];
		char tmicname2[64];

		Q_snprintf( tspeakername1, sizeof(tspeakername1), "PortalSpeaker%i_1", iLinkageGroupID );
		Q_snprintf( tspeakername2, sizeof(tspeakername2), "PortalSpeaker%i_2", iLinkageGroupID );
		Q_snprintf( tmicname1, sizeof(tmicname1), "PortalMic%i_1", iLinkageGroupID );
		Q_snprintf( tmicname2, sizeof(tmicname2), "PortalMic%i_2", iLinkageGroupID );
			
		string_t iszSpeakerName1 = AllocPooledString( tspeakername1 );
		string_t iszSpeakerName2 = AllocPooledString( tspeakername2 );
		string_t iszMicName1 = AllocPooledString( tmicname1 );
		string_t iszMicName2 = AllocPooledString( tmicname2 );			

		if( m_hMicrophone == NULL )
		{
			inputdata_t inputdata;

			m_hMicrophone = CreateEntityByName( "env_microphone" );
			CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
			pMicrophone->SetOwnerEntity( this );
			pMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
			pMicrophone->AddSpawnFlags( SF_MICROPHONE_SOUND_COMBAT | SF_MICROPHONE_SOUND_WORLD | SF_MICROPHONE_SOUND_PLAYER | SF_MICROPHONE_SOUND_BULLET_IMPACT | SF_MICROPHONE_SOUND_EXPLOSION );
		//	pMicrophone->KeyValue("ListenFilter", "weapon_portalgun_filter_disallow_in_code");
			DispatchSpawn( pMicrophone );

			m_hSpeaker = CreateEntityByName( "env_speaker" );
			CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );

			if( !m_bIsPortal2 )
			{
				pSpeaker->SetName( iszSpeakerName1 );
				pMicrophone->SetName( iszMicName1 );
				pMicrophone->Activate();
				pMicrophone->SetSpeakerName( iszSpeakerName2 );
				pMicrophone->SetSensitivity( 10.0f );
			}
			else
			{
				pSpeaker->SetName( iszSpeakerName2 );
				pMicrophone->SetName( iszMicName2 );
				pMicrophone->Activate();
				pMicrophone->SetSpeakerName( iszSpeakerName1 );
				pMicrophone->SetSensitivity( 10.0f );
			}
		}

		if ( m_hLinkedPortal->m_hMicrophone == 0 )
		{
			inputdata_t inputdata;

			m_hLinkedPortal->m_hMicrophone = CreateEntityByName( "env_microphone" );
			CEnvMicrophone *pLinkedMicrophone = static_cast<CEnvMicrophone*>( m_hLinkedPortal->m_hMicrophone.Get() );
			pLinkedMicrophone->SetOwnerEntity( m_hLinkedPortal );
			pLinkedMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
			pLinkedMicrophone->AddSpawnFlags( SF_MICROPHONE_SOUND_COMBAT | SF_MICROPHONE_SOUND_WORLD | SF_MICROPHONE_SOUND_PLAYER | SF_MICROPHONE_SOUND_BULLET_IMPACT | SF_MICROPHONE_SOUND_EXPLOSION );
		//	pLinkedMicrophone->KeyValue("ListenFilter", "weapon_portalgun_filter_disallow_in_code");
			DispatchSpawn( pLinkedMicrophone );

			m_hLinkedPortal->m_hSpeaker = CreateEntityByName( "env_speaker" );
			CSpeaker *pLinkedSpeaker = static_cast<CSpeaker*>( m_hLinkedPortal->m_hSpeaker.Get() );

			if ( !m_bIsPortal2 )
			{
				pLinkedSpeaker->SetName( iszSpeakerName2 );
				pLinkedMicrophone->SetName( iszMicName2 );
				pLinkedMicrophone->Activate();
				pLinkedMicrophone->SetSpeakerName( iszSpeakerName1 );
			}
			else
			{
				pLinkedSpeaker->SetName( iszSpeakerName1 );
				pLinkedMicrophone->SetName( iszMicName1 );
				pLinkedMicrophone->Activate();
				pLinkedMicrophone->SetSpeakerName( iszSpeakerName2 );
			}

			pLinkedMicrophone->SetSensitivity( 10.0f );
		}
		// Set microphone/speaker positions
		Vector vZero( 0.0f, 0.0f, 0.0f );
		CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
		pMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
		pMicrophone->Teleport( &GetAbsOrigin(), &GetAbsAngles(), &vZero );
		inputdata_t in;
		pMicrophone->InputEnable( in );

		CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );
		pSpeaker->Teleport( &GetAbsOrigin(), &GetAbsAngles(), &vZero );
		pSpeaker->InputTurnOn( in );
	}
}

void CProp_Portal::NewLocation( const Vector &vOrigin, const QAngle &qAngles )
{
	// Tell our physics environment to stop simulating it's entities.
	// Fast moving objects can pass through the hole this frame while it's in the old location.

	// Don't fizzle me if I moved from a location another portalgun shot
//	if (m_pPortalReplacingMe)
//		m_pPortalReplacingMe->m_pHitPortal = NULL;

	m_PortalSimulator.ReleaseAllEntityOwnership();
	Vector vOldForward;
	GetVectors( &vOldForward, 0, 0 );

	m_vPrevForward = vOldForward;

	WakeNearbyEntities();

	Teleport( &vOrigin, &qAngles, 0 );
	
	m_ptOrigin = vOrigin;
	m_qAbsAngle = qAngles;

#ifndef DONT_USE_MICROPHONESORSPEAKERS
	if ( m_hMicrophone )
	{
		CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
		pMicrophone->Teleport( &vOrigin, &qAngles, 0 );
		inputdata_t in;
		pMicrophone->InputEnable( in );
	}

	if ( m_hSpeaker )
	{
		CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );
		pSpeaker->Teleport( &vOrigin, &qAngles, 0 );
		inputdata_t in;
		pSpeaker->InputTurnOn( in );
	}
#endif
	CreateSounds();

	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangeVolume( m_pAmbientSound, 0.4, 0.1 );
	}
	if (use_server_portal_particles.GetBool())
	{
		if (m_iPortalColorSet == 1)
		{
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_particles" ) : ( "portal_lightblue_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_edge" ) : ( "portal_lightblue_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
		else if (m_iPortalColorSet == 2)
		{			
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_particles" ) : ( "portal_yellow_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_edge" ) : ( "portal_yellow_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
		else if (m_iPortalColorSet == 3)
		{			
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_particles" ) : ( "portal_green_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_edge" ) : ( "portal_green_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
		else
		{
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_particles" ) : ( "portal_1_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_edge" ) : ( "portal_1_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
		}
	}
	//if the other portal should be static, let's not punch stuff resting on it
	bool bOtherShouldBeStatic = false;
	if( !m_hLinkedPortal )
		bOtherShouldBeStatic = true;

	SetActive(true);

	UpdatePortalLinkage();
	UpdatePortalTeleportMatrix();

	// Update the four corners of this portal for faster reference
	UpdateCorners();

	WakeNearbyEntities();

	if ( m_hLinkedPortal )
	{
		m_hLinkedPortal->WakeNearbyEntities();
		if( !bOtherShouldBeStatic ) 
		{
			m_hLinkedPortal->PunchAllPenetratingPlayers();
		}
	}


	/*bool bOldPlaySound = false;
	CWeaponPortalgun *pPortalgun = m_hPlacedBy;
	if ( !pPortalgun || !pPortalgun->GetOwnerEntity() ) // a portalgun without an owner
		bOldPlaySound = true;

	if ( bOldPlaySound )
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
	else*/
	{
		CRecipientFilter filter;
		filter.UsePredictionRules();
		filter.AddRecipientsByPAS( GetAbsOrigin() );
		if ( m_bIsPortal2 )
		{
			EmitSound( filter, entindex(), "Portal.open_red" );
		}
		else
		{
			EmitSound( filter, entindex(), "Portal.open_blue" );
		}
	}
}

#define PORTALBROADCAST 0

//-----------------------------------------------------------------------------
// Purpose: Notify this the supplied entity has teleported to this portal
//-----------------------------------------------------------------------------
void CProp_Portal::OnEntityTeleportedToPortal( CBaseEntity *pEntity )
{
	m_OnEntityTeleportToMe.FireOutput( this, this );
	BroadcastPortalEvent( PORTALEVENT_ENTITY_TELEPORTED_TO );

	if ( pEntity->IsPlayer() )
	{
		m_OnPlayerTeleportToMe.FireOutput( this, this );
		BroadcastPortalEvent( PORTALEVENT_PLAYER_TELEPORTED_TO );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notify this the supplied entity has teleported from this portal
//-----------------------------------------------------------------------------
void CProp_Portal::OnEntityTeleportedFromPortal( CBaseEntity *pEntity )
{
	m_OnEntityTeleportFromMe.FireOutput( this, this );
	BroadcastPortalEvent( PORTALEVENT_ENTITY_TELEPORTED_FROM );

	if ( pEntity->IsPlayer() )
	{
		m_OnPlayerTeleportFromMe.FireOutput( this, this );
		BroadcastPortalEvent( PORTALEVENT_PLAYER_TELEPORTED_FROM );
	}
}

void CProp_Portal::OnStolen( CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	m_OnStolen.FireOutput( pActivator, pCaller );
}

void CProp_Portal::PreTeleportTouchingEntity( CBaseEntity *pOther )
{
	if( m_NotifyOnPortalled )
		m_NotifyOnPortalled->OnPrePortalled( pOther, true );

	CProp_Portal *pLinked = (CProp_Portal *)m_hLinkedPortal.Get();
	if( pLinked->m_NotifyOnPortalled )
		pLinked->m_NotifyOnPortalled->OnPrePortalled( pOther, false );
}

void CProp_Portal::PostTeleportTouchingEntity( CBaseEntity *pOther )
{
	if( m_NotifyOnPortalled )
		m_NotifyOnPortalled->OnPostPortalled( pOther, true );

	CProp_Portal *pLinked = (CProp_Portal *)m_hLinkedPortal.Get();
	if( pLinked->m_NotifyOnPortalled )
		pLinked->m_NotifyOnPortalled->OnPostPortalled( pOther, false );
}

void CProp_Portal::InputSetActivatedState( inputdata_t &inputdata )
{
	SetActive(inputdata.value.Bool());
	m_hPlacedBy = NULL;

	if ( IsActive() )
	{
		Vector vOrigin = GetAbsOrigin();

		Vector vForward, vUp;
		GetVectors( &vForward, 0, &vUp );

		CTraceFilterSimpleClassnameList baseFilter( this, COLLISION_GROUP_NONE );
		UTIL_Portal_Trace_Filter( &baseFilter );
		CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );

		trace_t tr;
		UTIL_TraceLine( vOrigin + vForward, vOrigin + vForward * -8.0f, MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

		QAngle qAngles;
		VectorAngles( tr.plane.normal, vUp, qAngles );

		float fPlacementSuccess = VerifyPortalPlacementAndFizzleBlockingPortals(this, tr.endpos, qAngles, PORTAL_PLACED_BY_FIXED);
		PlacePortal( tr.endpos, qAngles, fPlacementSuccess );

		// If the fixed portal is overlapping a portal that was placed before it... kill it!
		if ( fPlacementSuccess )
		{
			IsPortalOverlappingOtherPortals( this, vOrigin, GetAbsAngles(), true );

			CreateSounds();

			if ( m_pAmbientSound )
			{
				CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

				controller.SoundChangeVolume( m_pAmbientSound, 0.4, 0.1 );
			}
			if (use_server_portal_particles.GetBool())
			{
				if (m_iPortalColorSet == 1)
				{
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_particles" ) : ( "portal_lightblue_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_purple_edge" ) : ( "portal_lightblue_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
				}
				else if (m_iPortalColorSet == 2)
				{			
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_particles" ) : ( "portal_yellow_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_red_edge" ) : ( "portal_yellow_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
				}
				else if (m_iPortalColorSet == 3)
				{			
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_particles" ) : ( "portal_green_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_pink_edge" ) : ( "portal_green_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
				}
				else
				{
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_particles" ) : ( "portal_1_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
					DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_edge" ) : ( "portal_1_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
				}
			}
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
	else
	{
		if ( m_pAmbientSound )
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

			controller.SoundChangeVolume( m_pAmbientSound, 0.0, 0.0 );
		}

		StopParticleEffects( this );
	}

	UpdatePortalTeleportMatrix();

	UpdatePortalLinkage();
}

void CProp_Portal::InputFizzle( inputdata_t &inputdata )
{
	DoFizzleEffect( PORTAL_FIZZLE_KILLED, m_iPortalColorSet, false );
	Fizzle();
}

//-----------------------------------------------------------------------------
// Purpose: Map can call new location, so far it's only for debugging purposes so it's not made to be very robust.
// Input  : &inputdata - String with 6 float entries with space delimiters, location and orientation
//-----------------------------------------------------------------------------
void CProp_Portal::InputNewLocation( inputdata_t &inputdata )
{
	char sLocationStats[MAX_PATH];
	Q_strncpy( sLocationStats, inputdata.value.String(), sizeof(sLocationStats) );

	// first 3 are location of new origin
	Vector vNewOrigin;
	char* pTok = strtok( sLocationStats, " " ); 
	vNewOrigin.x = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewOrigin.y = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewOrigin.z = atof(pTok);

	// Next 3 entries are new angles
	QAngle vNewAngles;
	pTok = strtok( NULL, " " );
	vNewAngles.x = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewAngles.y = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewAngles.z = atof(pTok);

	// Call main placement function (skipping placement rules)
	NewLocation( vNewOrigin, vNewAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Tell all listeners about an event that just occurred 
//-----------------------------------------------------------------------------
void CProp_Portal::BroadcastPortalEvent( PortalEvent_t nEventType )
{
	/*
	switch( nEventType )
	{
	case PORTALEVENT_MOVED:
		Msg("[ Portal moved ]\n");
		break;
	
	case PORTALEVENT_FIZZLE:
		Msg("[ Portal fizzled ]\n");
		break;
	
	case PORTALEVENT_LINKED:
		Msg("[ Portal linked ]\n");
		break;
	}
	*/

	// We need to walk the list backwards because callers can remove themselves from our list as they're notified
	for ( int i = m_PortalEventListeners.Count()-1; i >= 0; i-- )
	{
		if ( m_PortalEventListeners[i] == NULL )
			continue;

		m_PortalEventListeners[i]->NotifyPortalEvent( nEventType, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a listener to our collection
//-----------------------------------------------------------------------------
void CProp_Portal::AddPortalEventListener( EHANDLE hListener )
{
	// Don't multiply add
	if ( m_PortalEventListeners.Find( hListener ) != m_PortalEventListeners.InvalidIndex() )
		return;

	m_PortalEventListeners.AddToTail( hListener );
}

//-----------------------------------------------------------------------------
// Purpose: Remove a listener to our collection
//-----------------------------------------------------------------------------
void CProp_Portal::RemovePortalEventListener( EHANDLE hListener )
{
	m_PortalEventListeners.FindAndFastRemove( hListener );
}

void CProp_Portal::UpdateCorners()
{
	Vector vOrigin = m_ptOrigin;
	Vector vUp, vRight;
	GetVectors( NULL, &vRight, &vUp );

	for ( int i = 0; i < 4; ++i )
	{
		Vector vAddPoint = vOrigin;

		vAddPoint += vRight * ((i & (1<<0))?(PORTAL_HALF_WIDTH):(-PORTAL_HALF_WIDTH));
		vAddPoint += vUp * ((i & (1<<1))?(PORTAL_HALF_HEIGHT):(-PORTAL_HALF_HEIGHT));

		m_vPortalCorners[i] = vAddPoint;
	}
}




void CProp_Portal::ChangeLinkageGroup( unsigned char iLinkageGroupID )
{
	Assert( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) != -1 );
	s_PortalLinkageGroups[m_iLinkageGroupID].FindAndRemove( this );
	s_PortalLinkageGroups[iLinkageGroupID].AddToTail( this );
	m_iLinkageGroupID = iLinkageGroupID;
}



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

	if( bCreateIfNothingFound )
	{
		CProp_Portal *pPortal = (CProp_Portal *)CreateEntityByName( "prop_portal" );
		pPortal->m_iLinkageGroupID = iLinkageGroupID;
		pPortal->m_bIsPortal2 = bPortal2;
		DispatchSpawn( pPortal );
		return pPortal;
	}

	return NULL;
}

const CUtlVector<CProp_Portal *> *CProp_Portal::GetPortalLinkageGroup( unsigned char iLinkageGroupID )
{
	return &s_PortalLinkageGroups[iLinkageGroupID];
}

//===================================================//
// CFunc_Portalled Code
//===================================================//

LINK_ENTITY_TO_CLASS( func_portalled, CFunc_Portalled )

BEGIN_DATADESC( CFunc_Portalled )

DEFINE_FIELD( m_bFireOnDeparture, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bFireOnArrival, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bFireOnPlayer, FIELD_BOOLEAN ),

DEFINE_OUTPUT( m_OnEntityPrePortalled, "OnEntityPrePortalled" ),
DEFINE_OUTPUT( m_OnEntityPostPortalled, "OnEntityPrePortalled" ),

END_DATADESC()

void CFunc_Portalled::OnPrePortalled( CBaseEntity *pEntity, bool m_bFireType )
{
	char sFireType;

	if (m_bFireType)
		sFireType = m_bFireOnDeparture;
	else
		sFireType = m_bFireOnArrival;

	if (sFireType != NULL && m_bFireOnPlayer)
		m_OnEntityPrePortalled.FireOutput( pEntity, this );
}

void CFunc_Portalled::OnPostPortalled( CBaseEntity *pEntity, bool m_bFireType )
{
	char sFireType;

	if (m_bFireType)
		sFireType = m_bFireOnDeparture;
	else
		sFireType = m_bFireOnArrival;

	if (sFireType != NULL && m_bFireOnPlayer)
		m_OnEntityPostPortalled.FireOutput( pEntity, this );
}

void EntityPortalled( CProp_Portal *pPortal, CBaseEntity *pOther, const Vector &vNewOrigin, const QAngle &qNewAngles, bool bForcedDuck )
{
	/*if( pOther->IsPlayer() )
	{
		Warning( "Server player portalled %f   %f %f %f   %f %f %f\n", gpGlobals->curtime, XYZ( vNewOrigin ), XYZ( ((CPortal_Player *)pOther)->pl.v_angle ) ); 
	}*/

	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayer = (CPortal_Player *)UTIL_PlayerByIndex( i );
		if( pPlayer )
		{
			pPlayer->NetworkPortalTeleportation( pOther, pPortal, gpGlobals->curtime, bForcedDuck );
		}
	}
}