//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for .
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "c_portal_player.h"
#include "view.h"
#include "c_basetempentity.h"
#include "takedamageinfo.h"
#include "in_buttons.h"
#include "iviewrender_beams.h"
#include "r_efx.h"
#include "dlight.h"
#include "PortalRender.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/KeyValues.h"
#include "ScreenSpaceEffects.h"
#include "portal_shareddefs.h"
#include "ivieweffects.h"		// for screenshake
#include "prop_portal_shared.h"
#include "portal_gamerules.h"
#include "collisionutils.h"
#include "c_triggers.h"
#include "prediction.h"
#include "soundenvelope.h"

#include "gameui/BonusMapsDialog.h"

// NVNT for fov updates
#include "haptics/ihaptics.h"

static ConVar cl_pinghudhint("cl_pinghudhint", "1", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Sets if players should see the ping hud hint on spawn.");
ConVar tf_max_separation_force ( "tf_max_separation_force", "256", FCVAR_DEVELOPMENTONLY );

extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;


// Don't alias here
#if defined( CPortal_Player )
#undef CPortal_Player	
#endif


#define REORIENTATION_RATE 120.0f
#define REORIENTATION_ACCELERATION_RATE 400.0f

#define ENABLE_PORTAL_EYE_INTERPOLATION_CODE 1


#define DEATH_CC_LOOKUP_FILENAME "materials/correction/cc_death.raw"
#define DEATH_CC_FADE_SPEED 0.05f


ConVar cl_reorient_in_air("cl_reorient_in_air", "1", FCVAR_ARCHIVE, "Allows the player to only reorient from being upside down while in the air." ); 


// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_Portal_Player *pPlayer = dynamic_cast< C_Portal_Player* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get(), m_nData );
		}	
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
RecvPropEHandle( RECVINFO( m_hPlayer ) ),
RecvPropInt( RECVINFO( m_iEvent ) ),
RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()


//=================================================================================
//
// Ragdoll Entity
//
class C_PortalRagdoll : public C_BaseFlex
{
public:

	DECLARE_CLASS( C_PortalRagdoll, C_BaseFlex );
	DECLARE_CLIENTCLASS();

	C_PortalRagdoll();
	~C_PortalRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;

	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );

private:

	C_PortalRagdoll( const C_PortalRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );
	void CreatePortalRagdoll();

private:

	EHANDLE	m_hPlayer;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );

};

IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_PortalRagdoll, DT_PortalRagdoll, CPortalRagdoll )
RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
RecvPropEHandle( RECVINFO( m_hPlayer ) ),
RecvPropInt( RECVINFO( m_nModelIndex ) ),
RecvPropInt( RECVINFO(m_nForceBone) ),
RecvPropVector( RECVINFO(m_vecForce) ),
RecvPropVector( RECVINFO( m_vecRagdollVelocity ) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::C_PortalRagdoll()
{
	m_hPlayer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::~C_PortalRagdoll()
{
	( this );
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSourceEntity - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;

	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();

	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(), pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_PortalRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	// While we're dying, we want to mimic the facial animation of the player. Once they're dead, we just stay as we are.
	if ( (m_hPlayer && m_hPlayer->IsAlive()) || !m_hPlayer )
	{
		BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
	else if ( m_hPlayer )
	{
		m_hPlayer->SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::CreatePortalRagdoll()
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_Portal_Player *pPlayer = dynamic_cast<C_Portal_Player*>( m_hPlayer.Get() );

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// Move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// This is the local player, so set them in a default
		// pose and slam their velocity, angles and origin
		SetAbsOrigin( /* m_vecRagdollOrigin : */ pPlayer->GetRenderOrigin() );			
		SetAbsAngles( pPlayer->GetRenderAngles() );
		SetAbsVelocity( m_vecRagdollVelocity );

		// Hack! Find a neutral standing pose or use the idle.
		int iSeq = LookupSequence( "ragdoll" );
		if ( iSeq == -1 )
		{
			Assert( false );
			iSeq = 0;
		}			
		SetSequence( iSeq );
		SetCycle( 0.0 );

		Interp_Reset( varMap );

		m_nBody = pPlayer->GetBody();
		SetModelIndex( m_nModelIndex );	
		// Make us a ragdoll..
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );

		InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_PortalRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		CreatePortalRagdoll();
	}
}

IMPLEMENT_CLIENTCLASS_DT(C_Portal_Player, DT_Portal_Player, CPortal_Player)
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropInt( RECVINFO( m_iSpawnInterpCounter ) ),
	RecvPropBool( RECVINFO( m_bPitchReorientation ) ),
	RecvPropEHandle( RECVINFO( m_hPortalEnvironment ) ),
	RecvPropEHandle( RECVINFO( m_hSurroundingLiquidPortal ) ),
	RecvPropBool( RECVINFO( m_bSuppressingCrosshair ) ),
	RecvPropBool( RECVINFO( m_bHasSprintDevice ) ),
	RecvPropBool( RECVINFO( m_bSprintEnabled ) ),
	RecvPropBool( RECVINFO( m_bSilentDropAndPickup ) ),
	RecvPropBool( RECVINFO( m_bIsListenServerHost ) ),
	RecvPropInt( RECVINFO( m_iCustomPortalColorSet ) ),	
	
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( player, C_Portal_Player )

BEGIN_PREDICTION_DATA( C_Portal_Player )


	DEFINE_PRED_FIELD( m_nSkin, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_nBody, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flPlaybackRate, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_ARRAY_TOL( m_flEncodedController, FIELD_FLOAT, MAXSTUDIOBONECTRLS, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE, 0.02f ),
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
		
	DEFINE_PRED_FIELD( m_hPortalEnvironment, FIELD_EHANDLE, FTYPEDESC_NOERRORCHECK ),
	
	//DEFINE_FIELD( m_matLastPortalled, FIELD_VMATRIX_WORLDSPACE ), //Garbage data :(

//	DEFINE_PRED_FIELD( m_iOldModelType, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

	DEFINE_SOUNDPATCH( m_pWooshSound ),
	
	DEFINE_FIELD(m_bHeldObjectOnOppositeSideOfPortal, FIELD_BOOLEAN),
	DEFINE_FIELD(m_pHeldObjectPortal, FIELD_EHANDLE),
	
	DEFINE_FIELD( m_fLatestServerTeleport, FIELD_FLOAT ),
	DEFINE_FIELD( m_matLatestServerTeleportationInverseMatrix, FIELD_VMATRIX ),

END_PREDICTION_DATA()

#define	_WALK_SPEED 150
#define	_NORM_SPEED 190
#define	_SPRINT_SPEED 320

static ConVar cl_playermodel( "cl_playermodel", "none", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Default Player Model");

extern bool g_bUpsideDown;

//EHANDLE g_eKillTarget1;
//EHANDLE g_eKillTarget2;

void SpawnBlood (Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

C_Portal_Player::C_Portal_Player()
: m_iv_angEyeAngles( "C_Portal_Player::m_iv_angEyeAngles" )
{
	m_PlayerAnimState = CreatePortalPlayerAnimState( this );
	SetPredictionEligible(true);

	m_iIDEntIndex = 0;
	m_iIDPortalEntIndex = 0;
	m_iSpawnInterpCounterCache = 0;

	m_hRagdoll.Set( NULL );
	m_flStartLookTime = 0.0f;

	m_bHeldObjectOnOppositeSideOfPortal = false;
	m_pHeldObjectPortal = NULL;

	m_bPitchReorientation = false;
	m_fReorientationRate = 0.0f;

	m_bIntersectingPortalPlane = false;

	m_angEyeAngles.Init();
	
	m_flImplicitVerticalStepSpeed = 0.0f;
	
	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
	m_blinkTimer.Invalidate();
#ifdef CCDEATH
	m_CCDeathHandle = INVALID_CLIENT_CCHANDLE;
	m_flDeathCCWeight = 0.0f;
#endif

	m_bShouldFixAngles = false;
	
	ListenForGameEvent( "bonusmap_unlock" );
	ListenForGameEvent( "advanced_map_complete" );
	ListenForGameEvent( "RefreshBonusData" );
}

C_Portal_Player::~C_Portal_Player( void )
{
	if ( m_PlayerAnimState )
	{
		m_PlayerAnimState->Release();
	}
#ifdef CCDEATH
	g_pColorCorrectionMgr->RemoveColorCorrection( m_CCDeathHandle );
#endif

	StopLoopingSounds();

}

void C_Portal_Player::Spawn( void )
{
	CreateSounds();
}

void C_Portal_Player::CreateSounds( void )
{
	if (prediction->InPrediction() && GetPredictable())

	if (!m_pWooshSound)
	{
		CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter(this);
		filter.UsePredictionRules();

		m_pWooshSound = controller.SoundCreate(filter, entindex(), "PortalPlayer.Woosh");
		controller.Play(m_pWooshSound, 0, 100);
	}
}

void C_Portal_Player::StopLoopingSounds( void )
{
	if (m_pWooshSound)
	{
		CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();

		controller.SoundDestroy(m_pWooshSound);
		m_pWooshSound = NULL;
	}
}
void C_Portal_Player::UpdatePortalPlaneSounds(void)
{
#if 1
	CProp_Portal* pPortal = m_hPortalEnvironment;
	if (pPortal && pPortal->IsActive())
	{
		Vector vVelocity = GetAbsVelocity();
		//GetVelocity(&vVelocity, NULL);

		if (!vVelocity.IsZero())
		{
			Vector vMin, vMax;
			CollisionProp()->WorldSpaceAABB(&vMin, &vMax);

			Vector vEarCenter = (vMax + vMin) / 2.0f;
			Vector vDiagonal = vMax - vMin;

			if (!m_bIntersectingPortalPlane)
			{
				vDiagonal *= 0.25f;

				if (UTIL_IsBoxIntersectingPortal(vEarCenter, vDiagonal, pPortal))
				{

					m_bIntersectingPortalPlane = true;

					CPASAttenuationFilter filter(this);
					CSoundParameters params;
					if (GetParametersForSound("PortalPlayer.EnterPortal", params, NULL))
					{
						EmitSound_t ep(params);
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = min(0.3f + vVelocity.Length() * 0.00075f, 1.0f);

						EmitSound(filter, entindex(), ep);
					}
				}
			}
			else
			{
				vDiagonal *= 0.30f;

				if (!UTIL_IsBoxIntersectingPortal(vEarCenter, vDiagonal, pPortal))
				{
					m_bIntersectingPortalPlane = false;

					CPASAttenuationFilter filter(this);
					CSoundParameters params;
					if (GetParametersForSound("PortalPlayer.ExitPortal", params, NULL))
					{
						EmitSound_t ep(params);
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = min(0.3f + vVelocity.Length() * 0.00075f, 1.0f);

						EmitSound(filter, entindex(), ep);
					}
				}
			}
		}
	}
	else if (m_bIntersectingPortalPlane)
	{
		m_bIntersectingPortalPlane = false;

		CPASAttenuationFilter filter(this);
		CSoundParameters params;
		if (GetParametersForSound("PortalPlayer.ExitPortal", params, NULL))
		{
			EmitSound_t ep(params);
			Vector vVelocity;
			GetVelocity(&vVelocity, NULL);
			ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
			ep.m_flVolume = min(0.3f + vVelocity.Length() * 0.00075f, 1.0f);

			EmitSound(filter, entindex(), ep);
		}
	}
#endif
}

void C_Portal_Player::UpdateWooshSounds(void)
{
	if (m_pWooshSound)
	{
		CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();

		float fWooshVolume = GetAbsVelocity().Length() - MIN_FLING_SPEED;

		if (fWooshVolume < 0.0f)
		{
			controller.SoundChangeVolume(m_pWooshSound, 0.0f, 0.1f);
			return;
		}

		fWooshVolume /= 2000.0f;
		if (fWooshVolume > 1.0f)
			fWooshVolume = 1.0f;

		controller.SoundChangeVolume(m_pWooshSound, fWooshVolume, 0.1f);
		//		controller.SoundChangePitch( m_pWooshSound, fWooshVolume + 0.5f, 0.1f );
	}
}

int C_Portal_Player::GetIDTarget() const
{
	return m_iIDEntIndex;
}

int C_Portal_Player::GetPortalIDTarget() const
{
	return m_iIDPortalEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_Portal_Player::UpdateIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	// don't show IDs in chase spec mode
	if ( GetObserverMode() == OBS_MODE_CHASE || 
		GetObserverMode() == OBS_MODE_DEATHCAM )
		return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 1500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 10,   MainViewForward(), vecStart );
	Ray_t ray;
	ray.Init( vecStart, vecEnd );
	
	UTIL_Portal_TraceRay( ray, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	// Sigh, can't see my own name :(
#if 0
	CProp_Portal *pShootThroughPortal = NULL;
	float fPortalFraction = 2.0f;

	CTraceFilterSimpleList traceFilter(COLLISION_GROUP_NONE);
	traceFilter.SetPassEntity( this ); // Standard pass entity for THIS so that it can be easily removed from the list after passing through a portal
	
	pShootThroughPortal = UTIL_Portal_FirstAlongRay( ray, fPortalFraction );
	if ( !UTIL_Portal_TraceRay_Bullets( pShootThroughPortal, ray, MASK_SHOT, &traceFilter, &tr ) )
	{
		pShootThroughPortal = NULL;
	}
#endif
	//UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;

		if ( pEntity && (pEntity != this) )
		{
			m_iIDEntIndex = pEntity->entindex();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target portal entity
//-----------------------------------------------------------------------------
void C_Portal_Player::UpdatePortalIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

	// Clear old target and find a new one
	m_iIDPortalEntIndex = 0;

	// don't show IDs in chase spec mode
	if ( GetObserverMode() == OBS_MODE_CHASE || 
		GetObserverMode() == OBS_MODE_DEATHCAM )
		return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 1500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 10,   MainViewForward(), vecStart );

	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	
	Ray_t ray;
	ray.Init( tr.startpos, tr.endpos );

	float flMustBeCloserThan = 2.0f;
	CProp_Portal *pPortal = UTIL_Portal_FirstAlongRayAll(ray, flMustBeCloserThan);
	
	if ( pPortal )
	{
		m_iIDPortalEntIndex = pPortal->entindex();
	}
}

void C_Portal_Player::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	Vector vecOrigin = ptr->endpos - vecDir * 4;

	float flDistance = 0.0f;

	if ( info.GetAttacker() )
	{
		flDistance = (ptr->endpos - info.GetAttacker()->GetAbsOrigin()).Length();
	}

	if ( m_takedamage )
	{
		AddMultiDamage( info, this );

		int blood = BloodColor();

		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, vecDir, blood, flDistance );// a little surface blood.
			TraceBleed( flDistance, vecDir, ptr, info.GetDamageType() );
		}
	}
}

void C_Portal_Player::Initialize( void )
{
	m_headYawPoseParam = LookupPoseParameter(  "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	CStudioHdr *hdr = GetModelPtr();
	for ( int i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		SetPoseParameter( hdr, i, 0.0 );
	}
}

CStudioHdr *C_Portal_Player::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();
	
	if (m_PlayerAnimState)
		m_PlayerAnimState->Release();

	m_PlayerAnimState = CreatePortalPlayerAnimState(this);

	Initialize( );

	return hdr;
}

//-----------------------------------------------------------------------------
/**
* Orient head and eyes towards m_lookAt.
*/

#define HL2DM_LOOKAT 0

void C_Portal_Player::UpdateLookAt( void )
{
	//Only I can see me looking at myself
	if (IsLocalPlayer())
	{
		//Don't try to always look at me if I don't have a Portal otherwise it looks weird to other people/players
		//Nope, this completely makes heads snappy
		//if (!m_hPortalEnvironment)
		//	return;

		// head yaw
		if (m_headYawPoseParam < 0 || m_headPitchPoseParam < 0)
			return;

		// This is buggy with dt 0, just skip since there is no work to do.
		if ( gpGlobals->frametime <= 0.0f )
			return;

		// Player looks at themselves through portals. Pick the portal we're turned towards.
		const int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		float *fPortalDot = (float *)stackalloc( sizeof( float ) * iPortalCount );
		float flLowDot = 1.0f;
		int iUsePortal = -1;

		// defaults if no portals are around
		Vector vPlayerForward;
		GetVectors( &vPlayerForward, NULL, NULL );
		Vector vCurLookTarget = EyePosition();

		if ( !IsAlive() )
		{
			m_viewtarget = EyePosition() + vPlayerForward*10.0f;
			return;
		}

		bool bNewTarget = false;
		if ( UTIL_IntersectEntityExtentsWithPortal( this ) != NULL )
		{
			// player is in a portal
			vCurLookTarget = EyePosition() + vPlayerForward*10.0f;
		}
		else if ( pPortals && pPortals[0] )
		{
			// Test through any active portals: This may be a shorter distance to the target
			for( int i = 0; i != iPortalCount; ++i )
			{
				CProp_Portal *pTempPortal = pPortals[i];

				if( pTempPortal && pTempPortal->IsActive() && pTempPortal->m_hLinkedPortal.Get() )
				{
					Vector vEyeForward, vPortalForward;
					EyeVectors( &vEyeForward );
					pTempPortal->GetVectors( &vPortalForward, NULL, NULL );
					fPortalDot[i] = vEyeForward.Dot( vPortalForward );
					if ( fPortalDot[i] < flLowDot )
					{
						flLowDot = fPortalDot[i];
						iUsePortal = i;
					}
				}
			}

			if ( iUsePortal >= 0 )
			{
				C_Prop_Portal* pPortal = pPortals[iUsePortal];
				if ( pPortal )
				{
					vCurLookTarget = pPortal->MatrixThisToLinked()*vCurLookTarget;
					if ( vCurLookTarget != m_vLookAtTarget )
					{
						bNewTarget = true;
					}
				}
			}
		}
		else
		{
			// No other look targets, look straight ahead
			vCurLookTarget += vPlayerForward*10.0f;
		}

		// Figure out where we want to look in world space.
		QAngle desiredAngles;
		Vector to = vCurLookTarget - EyePosition();
		VectorAngles( to, desiredAngles );
		QAngle aheadAngles;
		VectorAngles( vCurLookTarget, aheadAngles );

		// Figure out where our body is facing in world space.
		QAngle bodyAngles( 0, 0, 0 );
		bodyAngles[YAW] = GetLocalAngles()[YAW];

		m_flLastBodyYaw = bodyAngles[YAW];

		// Set the head's yaw.
		float desiredYaw = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
		desiredYaw = clamp( desiredYaw, m_headYawMin, m_headYawMax );

		float desiredPitch = AngleNormalize( desiredAngles[PITCH] );
		desiredPitch = clamp( desiredPitch, m_headPitchMin, m_headPitchMax );

		if ( bNewTarget )
		{
			m_flStartLookTime = gpGlobals->curtime;
		}

		float dt = (gpGlobals->frametime);
		float flSpeed	= 1.0f - ExponentialDecay( 0.7f, 0.033f, dt );

		m_flCurrentHeadYaw = m_flCurrentHeadYaw + flSpeed * ( desiredYaw - m_flCurrentHeadYaw );
		m_flCurrentHeadYaw	= AngleNormalize( m_flCurrentHeadYaw );
		SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );	

		m_flCurrentHeadPitch = m_flCurrentHeadPitch + flSpeed * ( desiredPitch - m_flCurrentHeadPitch );
		m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
		SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );

		// This orients the eyes
		m_viewtarget = m_vLookAtTarget = vCurLookTarget;
	}
#if HL2DM_LOOKAT
	//-------------------------//
	// HL2DM UpdateLookAt code //
	//-------------------------//
	else 
	{
		// head yaw
		if (m_headYawPoseParam < 0 || m_headPitchPoseParam < 0)
			return;

		// This is buggy with dt 0, just skip since there is no work to do.
		if ( gpGlobals->frametime <= 0.0f )
			return;

		// orient eyes
		m_viewtarget = m_vLookAtTarget;

		// blinking
		if (m_blinkTimer.IsElapsed())
		{
			m_blinktoggle = !m_blinktoggle;
			m_blinkTimer.Start( RandomFloat( 1.5f, 4.0f ) );
		}

		// Figure out where we want to look in world space.
		QAngle desiredAngles;
		Vector to = m_vLookAtTarget - EyePosition();
		VectorAngles( to, desiredAngles );

		// Figure out where our body is facing in world space.
		QAngle bodyAngles( 0, 0, 0 );
		bodyAngles[YAW] = GetLocalAngles()[YAW];


		float flBodyYawDiff = bodyAngles[YAW] - m_flLastBodyYaw;
		m_flLastBodyYaw = bodyAngles[YAW];

	

		// Set the head's yaw.
		float desired = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
		desired = clamp( desired, m_headYawMin, m_headYawMax );
		m_flCurrentHeadYaw = ApproachAngle( desired, m_flCurrentHeadYaw, 130 * gpGlobals->frametime );

		// Counterrotate the head from the body rotation so it doesn't rotate past its target.
		m_flCurrentHeadYaw = AngleNormalize( m_flCurrentHeadYaw - flBodyYawDiff );
		desired = clamp( desired, m_headYawMin, m_headYawMax );
	
		SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );

	
		// Set the head's yaw.
		desired = AngleNormalize( desiredAngles[PITCH] );
		desired = clamp( desired, m_headPitchMin, m_headPitchMax );
	
		m_flCurrentHeadPitch = ApproachAngle( desired, m_flCurrentHeadPitch, 130 * gpGlobals->frametime );
		m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
		SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );
	}
#endif
}

void C_Portal_Player::ClientThink( void )
{
	//PortalEyeInterpolation.m_bNeedToUpdateEyePosition = true;

	Vector vForward;
	AngleVectors( GetLocalAngles(), &vForward );

	// Allow sprinting
	HandleSpeedChanges();

	FixTeleportationRoll();
		
	//Hacks to get client sided triggers working
#if 0
	C_BaseEntity *pList[1024];

	// PARTITION_ALL_CLIENT_EDICTS
	// PARTITION_CLIENT_TRIGGER_ENTITIES
	int count = UTIL_EntitiesInBox( pList, 1024, GetPlayerMins(), GetPlayerMaxs(), MASK_ALL, PARTITION_ALL_CLIENT_EDICTS );

	for (int i = 1; i <= count; ++i)
	{

		C_BaseEntity *pEntity = pList[i];

		if (pEntity && (pEntity != this))
		{
			C_BaseTrigger *pTrigger = dynamic_cast<C_BaseTrigger*>(pEntity);
			if (pTrigger)
			{
				pTrigger->Touch(this);
			}

			C_BaseCombatWeapon *pWeapon = dynamic_cast<C_BaseCombatWeapon*>(pList[i]);

			if (pWeapon)
			{
				Msg("Touching Classname: %s\n", pWeapon->GetClassname());
			}


		}
	}
#endif

	//QAngle vAbsAngles = EyeAngles();

	// Look at the thing that killed you
	//if ( !IsAlive() )
	//{
	//	C_BaseEntity *pEntity1 = g_eKillTarget1.Get();
	//	C_BaseEntity *pEntity2 = g_eKillTarget2.Get();

	//	if ( pEntity2 && pEntity1 )
	//	{
	//		//engine->GetViewAngles( vAbsAngles );

	//		Vector vLook = pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin();
	//		VectorNormalize( vLook );

	//		QAngle qLook;
	//		VectorAngles( vLook, qLook );

	//		if ( qLook[PITCH] > 180.0f )
	//		{
	//			qLook[PITCH] -= 360.0f;
	//		}

	//		if ( vAbsAngles[YAW] < 0.0f )
	//		{
	//			vAbsAngles[YAW] += 360.0f;
	//		}

	//		if ( vAbsAngles[PITCH] < qLook[PITCH] )
	//		{
	//			vAbsAngles[PITCH] += gpGlobals->frametime * 120.0f;
	//			if ( vAbsAngles[PITCH] > qLook[PITCH] )
	//				vAbsAngles[PITCH] = qLook[PITCH];
	//		}
	//		else if ( vAbsAngles[PITCH] > qLook[PITCH] )
	//		{
	//			vAbsAngles[PITCH] -= gpGlobals->frametime * 120.0f;
	//			if ( vAbsAngles[PITCH] < qLook[PITCH] )
	//				vAbsAngles[PITCH] = qLook[PITCH];
	//		}

	//		if ( vAbsAngles[YAW] < qLook[YAW] )
	//		{
	//			vAbsAngles[YAW] += gpGlobals->frametime * 240.0f;
	//			if ( vAbsAngles[YAW] > qLook[YAW] )
	//				vAbsAngles[YAW] = qLook[YAW];
	//		}
	//		else if ( vAbsAngles[YAW] > qLook[YAW] )
	//		{
	//			vAbsAngles[YAW] -= gpGlobals->frametime * 240.0f;
	//			if ( vAbsAngles[YAW] < qLook[YAW] )
	//				vAbsAngles[YAW] = qLook[YAW];
	//		}

	//		if ( vAbsAngles[YAW] > 180.0f )
	//		{
	//			vAbsAngles[YAW] -= 360.0f;
	//		}

	//		engine->SetViewAngles( vAbsAngles );
	//	}
	//}

#ifdef CCDEATH
	// If dead, fade in death CC lookup
	if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE )
	{
		if ( m_lifeState != LIFE_ALIVE )
		{
			if ( m_flDeathCCWeight < 1.0f )
			{
				m_flDeathCCWeight += DEATH_CC_FADE_SPEED;
				clamp( m_flDeathCCWeight, 0.0f, 1.0f );
			}
		}
		else 
		{
			m_flDeathCCWeight = 0.0f;
		}
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, m_flDeathCCWeight );
	}
#endif

	//HL2DM LOOKAT CODE
#if HL2DM_LOOKAT

	if (!IsLocalPlayer())
	{		
		bool bFoundViewTarget = false;
	
		Vector vForward;
		AngleVectors( GetLocalAngles(), &vForward );

		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if( !pPlayer )
				continue;

			if ( pPlayer->entindex() == entindex() )
				continue;

			if ( pPlayer == this )
				continue;

			Vector vTargetOrigin = pPlayer->GetAbsOrigin();
			Vector vMyOrigin =  GetAbsOrigin();

			Vector vDir = vTargetOrigin - vMyOrigin;
		
			if ( vDir.Length() > 128 ) 
				continue;

			VectorNormalize( vDir );

			if ( DotProduct( vForward, vDir ) < 0.0f )
				 continue;

			m_vLookAtTarget = pPlayer->EyePosition();
			bFoundViewTarget = true;
			break;
		}

		if ( bFoundViewTarget == false )
		{
			m_vLookAtTarget = GetAbsOrigin() + vForward * 512;
		}

	}

#endif

	UpdateIDTarget();
	UpdatePortalIDTarget();
	
	if (IsLocalPlayer())
	{
		UpdatePortalPlaneSounds();
	}

	if (prediction->InPrediction() && GetPredictable())
	{
		UpdateWooshSounds();
	}
}

void C_Portal_Player::FixTeleportationRoll( void )
{
	if( IsInAVehicle() ) //HL2 compatibility fix. do absolutely nothing to the view in vehicles
		return;

	if( !IsLocalPlayer() )
		return;

	// Normalize roll from odd portal transitions
	QAngle vAbsAngles = EyeAngles();


	Vector vCurrentForward, vCurrentRight, vCurrentUp;
	AngleVectors( vAbsAngles, &vCurrentForward, &vCurrentRight, &vCurrentUp );

	if ( vAbsAngles[ROLL] == 0.0f )
	{
		m_fReorientationRate = 0.0f;
		g_bUpsideDown = ( vCurrentUp.z < 0.0f );
		return;
	}

	bool bForcePitchReorient = ( vAbsAngles[ROLL] > 175.0f && vCurrentForward.z > 0.99f );
	bool bOnGround = ( GetGroundEntity() != NULL );

	if ( bForcePitchReorient )
	{
		m_fReorientationRate = REORIENTATION_RATE * ( ( bOnGround ) ? ( 2.0f ) : ( 1.0f ) );
	}
	else
	{
		// Don't reorient in air if they don't want to
		if ( !cl_reorient_in_air.GetBool() && !bOnGround )
		{
			g_bUpsideDown = ( vCurrentUp.z < 0.0f );
			return;
		}
	}

	if ( vCurrentUp.z < 0.75f )
	{
		m_fReorientationRate += gpGlobals->frametime * REORIENTATION_ACCELERATION_RATE;

		// Upright faster if on the ground
		float fMaxReorientationRate = REORIENTATION_RATE * ( ( bOnGround ) ? ( 2.0f ) : ( 1.0f ) );
		if ( m_fReorientationRate > fMaxReorientationRate )
			m_fReorientationRate = fMaxReorientationRate;
	}
	else
	{
		if ( m_fReorientationRate > REORIENTATION_RATE * 0.5f )
		{
			m_fReorientationRate -= gpGlobals->frametime * REORIENTATION_ACCELERATION_RATE;
			if ( m_fReorientationRate < REORIENTATION_RATE * 0.5f )
				m_fReorientationRate = REORIENTATION_RATE * 0.5f;
		}
		else if ( m_fReorientationRate < REORIENTATION_RATE * 0.5f )
		{
			m_fReorientationRate += gpGlobals->frametime * REORIENTATION_ACCELERATION_RATE;
			if ( m_fReorientationRate > REORIENTATION_RATE * 0.5f )
				m_fReorientationRate = REORIENTATION_RATE * 0.5f;
		}
	}

	if ( !m_bPitchReorientation && !bForcePitchReorient )
	{
		// Randomize which way we roll if we're completely upside down
		if ( vAbsAngles[ROLL] == 180.0f && RandomInt( 0, 1 ) == 1 )
		{
			vAbsAngles[ROLL] = -180.0f;
		}

		if ( vAbsAngles[ROLL] < 0.0f )
		{
			vAbsAngles[ROLL] += gpGlobals->frametime * m_fReorientationRate;
			if ( vAbsAngles[ROLL] > 0.0f )
				vAbsAngles[ROLL] = 0.0f;
			engine->SetViewAngles( vAbsAngles );
		}
		else if ( vAbsAngles[ROLL] > 0.0f )
		{
			vAbsAngles[ROLL] -= gpGlobals->frametime * m_fReorientationRate;
			if ( vAbsAngles[ROLL] < 0.0f )
				vAbsAngles[ROLL] = 0.0f;
			engine->SetViewAngles( vAbsAngles );
			m_angEyeAngles = vAbsAngles;
			m_iv_angEyeAngles.Reset( gpGlobals->curtime );
		}
	}
	else
	{
		if ( vAbsAngles[ROLL] != 0.0f )
		{
			if ( vCurrentUp.z < 0.2f )
			{
				float fDegrees = gpGlobals->frametime * m_fReorientationRate;
				if ( vCurrentForward.z > 0.0f )
				{
					fDegrees = -fDegrees;
				}

				// Rotate around the right axis
				VMatrix mAxisAngleRot = SetupMatrixAxisRot( vCurrentRight, fDegrees );

				vCurrentUp = mAxisAngleRot.VMul3x3( vCurrentUp );
				vCurrentForward = mAxisAngleRot.VMul3x3( vCurrentForward );

				VectorAngles( vCurrentForward, vCurrentUp, vAbsAngles );

				engine->SetViewAngles( vAbsAngles );
				m_angEyeAngles = vAbsAngles;
				m_iv_angEyeAngles.Reset( gpGlobals->curtime );
			}
			else
			{
				if ( vAbsAngles[ROLL] < 0.0f )
				{
					vAbsAngles[ROLL] += gpGlobals->frametime * m_fReorientationRate;
					if ( vAbsAngles[ROLL] > 0.0f )
						vAbsAngles[ROLL] = 0.0f;
					engine->SetViewAngles( vAbsAngles );
					m_angEyeAngles = vAbsAngles;
					m_iv_angEyeAngles.Reset( gpGlobals->curtime );
				}
				else if ( vAbsAngles[ROLL] > 0.0f )
				{
					vAbsAngles[ROLL] -= gpGlobals->frametime * m_fReorientationRate;
					if ( vAbsAngles[ROLL] < 0.0f )
						vAbsAngles[ROLL] = 0.0f;
					engine->SetViewAngles( vAbsAngles );
					m_angEyeAngles = vAbsAngles;
					m_iv_angEyeAngles.Reset( gpGlobals->curtime );
				}
			}
		}
	}

	// Keep track of if we're upside down for look control
	vAbsAngles = EyeAngles();
	AngleVectors( vAbsAngles, NULL, NULL, &vCurrentUp );

	if ( bForcePitchReorient )
		g_bUpsideDown = ( vCurrentUp.z < 0.0f );
	else
		g_bUpsideDown = false;
}

const QAngle& C_Portal_Player::GetRenderAngles()
{
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}
	else
	{
		return m_PlayerAnimState->GetRenderAngles();
	}
}

void C_Portal_Player::UpdateClientSideAnimation( void )
{

	/*
	UpdateLookAt();

	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( this == C_Portal_Player::GetLocalPortalPlayer() )
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	else
		m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
	*/
	

	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( C_BasePlayer::IsLocalPlayer() )
	{
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	}
	else
	{
		QAngle qEffectiveAngles;
		
		if( m_iv_angEyeAngles.GetInterpolatedTime( GetEffectiveInterpolationCurTime( gpGlobals->curtime ) ) < m_fLatestServerTeleport )
		{
			qEffectiveAngles = TransformAnglesToLocalSpace( m_angEyeAngles, m_matLatestServerTeleportationInverseMatrix.As3x4() );
		}
		else
		{
			qEffectiveAngles = m_angEyeAngles;
		}

		m_PlayerAnimState->Update( qEffectiveAngles[YAW], qEffectiveAngles[PITCH] );
	}

	BaseClass::UpdateClientSideAnimation();
}

void C_Portal_Player::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Portal_Player::DrawModel( int flags )
{
	if ( !m_bReadyToDraw )
		return 0;

	if( IsLocalPlayer() )
	{
		if ( !C_BasePlayer::ShouldDrawThisPlayer() )
		{
			if ( !g_pPortalRender->IsRenderingPortal() )
				return 0;

			if( (g_pPortalRender->GetViewRecursionLevel() == 1) && (m_iForceNoDrawInPortalSurface != -1) ) //CPortalRender::s_iRenderingPortalView )
				return 0;
		}
	}

	return BaseClass::DrawModel(flags);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Portal_Player::Simulate( void )
{
	BaseClass::Simulate();

	QAngle vTempAngles = GetLocalAngles();
	vTempAngles[PITCH] = m_angEyeAngles[PITCH];

	SetLocalAngles( vTempAngles );

	// Zero out model pitch, blending takes care of all of it.
	SetLocalAnglesDim( X_INDEX, 0 );
}


extern ConVar pcoop_avoidplayers;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_Portal_Player::ShouldCollide(int collisionGroup, int contentsMask) const
{
	if ( ( ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ) && pcoop_avoidplayers.GetBool() ) )
		return false;

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

#if USEMOVEMENTFORPORTALLING

void C_Portal_Player::ApplyTransformToInterpolators( const VMatrix &matTransform, float fUpToTime, bool bIsRevertingPreviousTransform, bool bDuckForced )
{	
	Vector vOriginToCenter = (GetPlayerMins() + GetPlayerMins()) * 0.5f;
	Vector vCenterToOrigin = -vOriginToCenter;
	Vector vViewOffset = vec3_origin;
	VMatrix matCenterTransform = matTransform, matEyeTransform;
	Vector vOldEye = GetViewOffset();
	Vector vNewEye = GetViewOffset();
	
	if( bDuckForced )
	{
		// Going to be standing up
		if( bIsRevertingPreviousTransform )
		{
			vNewEye = VEC_VIEW;
			vViewOffset = VEC_VIEW - VEC_DUCK_VIEW;
			vOriginToCenter = (VEC_DUCK_HULL_MIN_SCALED(this) + VEC_DUCK_HULL_MAX_SCALED(this)) * 0.5f;
			vCenterToOrigin = -(VEC_HULL_MIN_SCALED(this) + VEC_HULL_MAX_SCALED(this)) * 0.5f;
		}
		// Going to be crouching
		else
		{
			vNewEye = VEC_DUCK_VIEW;
			vViewOffset = VEC_DUCK_VIEW - VEC_VIEW;
			vOriginToCenter = (VEC_HULL_MIN_SCALED(this) + VEC_HULL_MAX_SCALED(this)) * 0.5f;
			vCenterToOrigin = -(VEC_DUCK_HULL_MIN_SCALED(this) + VEC_DUCK_HULL_MAX_SCALED(this)) * 0.5f;
		}

		vOldEye = matTransform.ApplyRotation( vOldEye );
		Vector vEyeOffset = vOldEye - vNewEye - vCenterToOrigin;
		matEyeTransform = SetupMatrixTranslation(vEyeOffset);
	}
	else
	{
		vOldEye -= vOriginToCenter;
		vOldEye = matTransform.ApplyRotation( vOldEye );
		vOldEye += vOriginToCenter;

		Vector vEyeOffset = vOldEye - vNewEye;
		matEyeTransform = SetupMatrixTranslation(vEyeOffset);
	}

	// There's a 1-frame pop in multiplayer with lag when forced to duck.  WHAT THE FUCKKKKKKKKKKKKKKK

	// Translate origin to center
	matCenterTransform = matCenterTransform * SetupMatrixTranslation(vOriginToCenter);
	// Translate center to origin
	matCenterTransform = SetupMatrixTranslation( vCenterToOrigin ) * matCenterTransform;

	VMatrix matViewOffset = SetupMatrixTranslation( vViewOffset );

	if( bIsRevertingPreviousTransform )
	{
		GetOriginInterpolator().RemoveDiscontinuity( fUpToTime, &matCenterTransform.As3x4() );
		GetRotationInterpolator().RemoveDiscontinuity( fUpToTime, &matCenterTransform.As3x4() );
		m_iv_angEyeAngles.RemoveDiscontinuity( fUpToTime, &matCenterTransform.As3x4() );
		m_iv_vecViewOffset.RemoveDiscontinuity( fUpToTime, &matViewOffset.As3x4() );
		m_iv_vEyeOffset.RemoveDiscontinuity( fUpToTime, &matEyeTransform.As3x4() );
	}
	else
	{
		GetOriginInterpolator().InsertDiscontinuity( matCenterTransform.As3x4(), fUpToTime );
		GetRotationInterpolator().InsertDiscontinuity( matCenterTransform.As3x4(), fUpToTime );
		m_iv_angEyeAngles.InsertDiscontinuity( matCenterTransform.As3x4(), fUpToTime );
		m_iv_vecViewOffset.InsertDiscontinuity( matViewOffset.As3x4(), fUpToTime );
		m_iv_vEyeOffset.InsertDiscontinuity( matEyeTransform.As3x4(), fUpToTime );
	}	

	m_PlayerAnimState->TransformYAWs( matCenterTransform.As3x4() );
	
	AddEFlags( EFL_DIRTY_ABSTRANSFORM );
}

void C_Portal_Player::ApplyUnpredictedPortalTeleportation( const CProp_Portal *pEnteredPortal, float flTeleportationTime, bool bForcedDuck )
{
	// This is causing too many issues in multiplayer
#if 0
	ApplyTransformToInterpolators( pEnteredPortal->m_matrixThisToLinked, flTeleportationTime, false, bForcedDuck );

	//Warning( "Applying teleportation view angle change %d, %f\n", m_PredictedPortalTeleportations.Count(), gpGlobals->curtime );

	if( IsLocalPlayer() )
	{
		//Warning( "C_Portal_Player::ApplyUnpredictedPortalTeleportation() ent:%i slot:%i\n", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
		matrix3x4_t matAngleTransformIn, matAngleTransformOut; //temps for angle transformation
		{
			QAngle qEngineAngles;
			engine->GetViewAngles( qEngineAngles );
			AngleMatrix( qEngineAngles, matAngleTransformIn );
			ConcatTransforms( pEnteredPortal->m_matrixThisToLinked.As3x4(), matAngleTransformIn, matAngleTransformOut );
			MatrixAngles( matAngleTransformOut, qEngineAngles );
			engine->SetViewAngles( qEngineAngles );
			pl.v_angle = qEngineAngles;
		}
	}

	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *pViewModel = GetViewModel( i );
		if ( !pViewModel )
			continue;

		pViewModel->m_vecLastFacing = pEnteredPortal->m_matrixThisToLinked.ApplyRotation( pViewModel->m_vecLastFacing );
	}

#if ( PLAYERPORTALDEBUGSPEW == 1 )
	Warning( "C_Portal_Player::ApplyUnpredictedPortalTeleportation( %f )\n", flTeleportationTime/*gpGlobals->curtime*/ );
#endif

	m_bFixEyeAnglesFromPortalling = true;
	PostTeleportationCameraFixup( pEnteredPortal );

	if( IsToolRecording() )
	{		
		KeyValues *msg = new KeyValues( "entity_nointerp" );

		// Post a message back to all IToolSystems
		Assert( (int)GetToolHandle() != 0 );
		ToolFramework_PostToolMessage( GetToolHandle(), msg );

		msg->deleteThis();
	}
	
	SetOldPlayerZ( GetNetworkOrigin().z );
#endif
}

void C_Portal_Player::ApplyPredictedPortalTeleportation( CProp_Portal *pEnteredPortal, CMoveData *pMove, bool bForcedDuck )
{
	if( pEnteredPortal->m_hLinkedPortal.Get() != NULL )
	{
		m_matLatestServerTeleportationInverseMatrix = pEnteredPortal->m_hLinkedPortal->MatrixThisToLinked();
	}
	else
	{
		m_matLatestServerTeleportationInverseMatrix.Identity();
	}

	m_fLatestServerTeleport = gpGlobals->curtime;

#if 0
	C_PortalGhostRenderable *pGhost = pEnteredPortal->GetGhostRenderableForEntity( this );
	if( !pGhost )
	{
		//high velocity edge case. Entity portalled before it ever created a clone. But will need one for the interpolated origin history
		if( C_PortalGhostRenderable::ShouldCloneEntity( this, pEnteredPortal, false ) )
		{
			pGhost = C_PortalGhostRenderable::CreateGhostRenderable( this, pEnteredPortal );
			Assert( !pEnteredPortal->m_hGhostingEntities.IsValidIndex( pEnteredPortal->m_hGhostingEntities.Find( this ) ) );
			pEnteredPortal->m_hGhostingEntities.AddToTail( this );
			Assert( pEnteredPortal->m_GhostRenderables.IsValidIndex( pEnteredPortal->m_GhostRenderables.Find( pGhost ) ) );
			pGhost->PerFrameUpdate();
		}
	}

	if( pGhost )
	{
	//	C_PortalGhostRenderable::CreateInversion( pGhost, pEnteredPortal, gpGlobals->curtime );
	}
#endif

	//Warning( "C_Portal_Player::ApplyPredictedPortalTeleportation() ent:%i slot:%i\n", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
	ApplyTransformToInterpolators( pEnteredPortal->m_matrixThisToLinked, gpGlobals->curtime, false, bForcedDuck );
	
	PostTeleportationCameraFixup( pEnteredPortal );

	if( prediction->IsFirstTimePredicted() && IsToolRecording() )
	{
		KeyValues *msg = new KeyValues( "entity_nointerp" );
		
		// Post a message back to all IToolSystems
		Assert( (int)GetToolHandle() != 0 );
		ToolFramework_PostToolMessage( GetToolHandle(), msg );

		msg->deleteThis();
	}

	// This fixes viewmodel popping once and for all! - Wonderland_War
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *pViewModel = GetViewModel( i );
		if ( !pViewModel )
			continue;

		pViewModel->m_vecLastFacing = pEnteredPortal->m_matrixThisToLinked.ApplyRotation( pViewModel->m_vecLastFacing );
	}
	if ( pMove )
		SetOldPlayerZ( pMove->GetAbsOrigin().z );
}

void C_Portal_Player::UndoPredictedPortalTeleportation( const C_Prop_Portal *pEnteredPortal, float fOriginallyAppliedTime, const VMatrix &matUndo, bool bForcedDuck )
{
	ApplyTransformToInterpolators( matUndo, fOriginallyAppliedTime, true, bForcedDuck );

	// Don't use the expanded box to search for stick surfaces, since the player hasn't teleported yet.
	//m_flUsePostTeleportationBoxTime = 0.0f;

	SetOldPlayerZ( GetNetworkOrigin().z );
}

void C_Portal_Player::UnrollPredictedTeleportations( int iCommandNumber )
{
	//roll back changes that aren't automatically restored when rolling back prediction time
	//ACTIVE_SPLITSCREEN_PLAYER_GUARD( this );

	if( (m_PredictedPortalTeleportations.Count() != 0) && (iCommandNumber <= m_PredictedPortalTeleportations.Tail().iCommandNumber) )
	{
		matrix3x4_t matAngleTransformIn, matAngleTransformOut; //temps for angle transformation

		QAngle qEngineViewAngles;
		engine->GetViewAngles( qEngineViewAngles );
		//QAngle qVAngles = player->pl.v_angle;

		//crap, re-predicting teleportations. This is fine for the CMoveData, but CUserCmd/engine view angles are temporally sensitive.
		for( int i = m_PredictedPortalTeleportations.Count(); --i >= 0; )
		{
			if( iCommandNumber <= m_PredictedPortalTeleportations[i].iCommandNumber )
			{
				const VMatrix &matTransform = m_PredictedPortalTeleportations[i].matUnroll;
				//undo the view transformation this previous (but future) teleportation applied to the view angles.
				{
					AngleMatrix( qEngineViewAngles, matAngleTransformIn );
					ConcatTransforms( matTransform.As3x4(), matAngleTransformIn, matAngleTransformOut );
					MatrixAngles( matAngleTransformOut, qEngineViewAngles );
				}

				/*{
				AngleMatrix( qVAngles, matAngleTransformIn );
				ConcatTransforms( matTransform.As3x4(), matAngleTransformIn, matAngleTransformOut );
				MatrixAngles( matAngleTransformOut, qVAngles );
				}*/

				UndoPredictedPortalTeleportation( m_PredictedPortalTeleportations[i].pEnteredPortal, m_PredictedPortalTeleportations[i].flTime, matTransform, m_PredictedPortalTeleportations[i].bDuckForced );

				// Hack? Squash camera values back to neutral.
//				m_PortalLocal.m_Up = Vector(0,0,1);
//				m_PortalLocal.m_qQuaternionPunch.Init(0,0,0);
				m_vEyeOffset = vec3_origin;

#if ( PLAYERPORTALDEBUGSPEW == 1 )
				Warning( "<--Rolling back predicted teleportation %d, %f %i %i\n", m_PredictedPortalTeleportations.Count(), m_PredictedPortalTeleportations[i].flTime, m_PredictedPortalTeleportations[i].iCommandNumber, iCommandNumber );
#endif
				m_PredictedPortalTeleportations.FastRemove( i );
			}
			else
			{
				break;
			}
		}

		if( IsLocalPlayer() )
		{
			engine->SetViewAngles( qEngineViewAngles );
			//Warning( "C_Portal_Player::UnrollPredictedTeleportations() ent:%i slot:%i\n", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
		}
		//player->pl.v_angle = qVAngles;
	}
}
#endif // USEMOVEMENTFORPORTALLING
void C_Portal_Player::ForceDropOfCarriedPhysObjects(CBaseEntity* pOnlyIfHoldingThis)
{
	m_bHeldObjectOnOppositeSideOfPortal = false;
	BaseClass::ForceDropOfCarriedPhysObjects(pOnlyIfHoldingThis);
}

void C_Portal_Player::AvoidPlayers( CUserCmd *pCmd )
{
	// Turn off the avoid player code.
	if ( !pcoop_avoidplayers.GetBool() )
		return;

	// Don't test if the player doesn't exist or is dead.
	if ( IsAlive() == false )
		return;
	
	// Up vector.
	static Vector vecUp( 0.0f, 0.0f, 1.0f );

	Vector vecTFPlayerCenter = GetAbsOrigin();
	Vector vecTFPlayerMin = GetPlayerMins();
	Vector vecTFPlayerMax = GetPlayerMaxs();
	float flZHeight = vecTFPlayerMax.z - vecTFPlayerMin.z;
	vecTFPlayerCenter.z += 0.5f * flZHeight;
	VectorAdd( vecTFPlayerMin, vecTFPlayerCenter, vecTFPlayerMin );
	VectorAdd( vecTFPlayerMax, vecTFPlayerCenter, vecTFPlayerMax );

	// Find an intersecting player or object.
	int nAvoidPlayerCount = 0;
	C_Portal_Player *pAvoidPlayerList[MAX_PLAYERS];

	C_Portal_Player *pIntersectPlayer = NULL;
	float flAvoidRadius = 0.0f;

	Vector vecAvoidCenter, vecAvoidMin, vecAvoidMax;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_Portal_Player *pAvoidPlayer = static_cast< C_Portal_Player * >(UTIL_PlayerByIndex(i));
		if ( pAvoidPlayer == NULL )
			continue;
		// Is the avoid player me?
		if ( pAvoidPlayer == this )
			continue;

		// Save as list to check against for objects.
		pAvoidPlayerList[nAvoidPlayerCount] = pAvoidPlayer;
		++nAvoidPlayerCount;

		// Check to see if the avoid player is dormant.
		if ( pAvoidPlayer->IsDormant() )
			continue;

		// Is the avoid player solid?
		if ( pAvoidPlayer->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
			continue;

		Vector t1, t2;

		vecAvoidCenter = pAvoidPlayer->GetAbsOrigin();
		vecAvoidMin = pAvoidPlayer->GetPlayerMins();
		vecAvoidMax = pAvoidPlayer->GetPlayerMaxs();
		flZHeight = vecAvoidMax.z - vecAvoidMin.z;
		vecAvoidCenter.z += 0.5f * flZHeight;
		VectorAdd( vecAvoidMin, vecAvoidCenter, vecAvoidMin );
		VectorAdd( vecAvoidMax, vecAvoidCenter, vecAvoidMax );

		if ( IsBoxIntersectingBox( vecTFPlayerMin, vecTFPlayerMax, vecAvoidMin, vecAvoidMax ) )
		{
			// Need to avoid this player.
			if ( !pIntersectPlayer )
			{
				pIntersectPlayer = pAvoidPlayer;
				break;
			}
		}
	}
	
	// Anything to avoid?
	if ( !pIntersectPlayer)
	{
		SetSeparation( false );
		SetSeparationVelocity( vec3_origin );
		return;
	}

	// Calculate the push strength and direction.
	Vector vecDelta;

	// Avoid a player - they have precedence.
	if ( pIntersectPlayer )
	{
		VectorSubtract( pIntersectPlayer->WorldSpaceCenter(), vecTFPlayerCenter, vecDelta );

		Vector vRad = pIntersectPlayer->WorldAlignMaxs() - pIntersectPlayer->WorldAlignMins();
		vRad.z = 0;

		flAvoidRadius = vRad.Length();
	}

	float flPushStrength = RemapValClamped( vecDelta.Length(), flAvoidRadius, 0, 0, tf_max_separation_force.GetInt() ); //flPushScale;

	//Msg( "PushScale = %f\n", flPushStrength );

	// Check to see if we have enough push strength to make a difference.
	if ( flPushStrength < 0.01f )
		return;

	Vector vecPush;
	if ( GetAbsVelocity().Length2DSqr() > 0.1f )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity.z = 0.0f;
		CrossProduct( vecUp, vecVelocity, vecPush );
		VectorNormalize( vecPush );
	}
	else
	{
		// We are not moving, but we're still intersecting.
		QAngle angView = pCmd->viewangles;
		angView.x = 0.0f;
		AngleVectors( angView, NULL, &vecPush, NULL );
	}

	// Move away from the other player/object.
	Vector vecSeparationVelocity;
	if ( vecDelta.Dot( vecPush ) < 0 )
	{
		vecSeparationVelocity = vecPush * flPushStrength;
	}
	else
	{
		vecSeparationVelocity = vecPush * -flPushStrength;
	}

	// Don't allow the max push speed to be greater than the max player speed.
	float flMaxPlayerSpeed = MaxSpeed();
	float flCropFraction = 1.33333333f;

	if ( ( GetFlags() & FL_DUCKING ) && ( GetGroundEntity() != NULL ) )
	{	
		flMaxPlayerSpeed *= flCropFraction;
	}	

	float flMaxPlayerSpeedSqr = flMaxPlayerSpeed * flMaxPlayerSpeed;

	if ( vecSeparationVelocity.LengthSqr() > flMaxPlayerSpeedSqr )
	{
		vecSeparationVelocity.NormalizeInPlace();
		VectorScale( vecSeparationVelocity, flMaxPlayerSpeed, vecSeparationVelocity );
	}

	QAngle vAngles = pCmd->viewangles;
	vAngles.x = 0;
	Vector currentdir;
	Vector rightdir;

	AngleVectors( vAngles, &currentdir, &rightdir, NULL );

	Vector vDirection = vecSeparationVelocity;

	VectorNormalize( vDirection );

	float fwd = currentdir.Dot( vDirection );
	float rt = rightdir.Dot( vDirection );

	float forward = fwd * flPushStrength;
	float side = rt * flPushStrength;

	//Msg( "fwd: %f - rt: %f - forward: %f - side: %f\n", fwd, rt, forward, side );

	SetSeparation( true );
	SetSeparationVelocity( vecSeparationVelocity );

	pCmd->forwardmove	+= forward;
	pCmd->sidemove		+= side;

	// Clamp the move to within legal limits, preserving direction. This is a little
	// complicated because we have different limits for forward, back, and side

	//Msg( "PRECLAMP: forwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );

	float flForwardScale = 1.0f;
	if ( pCmd->forwardmove > fabs( cl_forwardspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_forwardspeed.GetFloat() ) / pCmd->forwardmove;
	}
	else if ( pCmd->forwardmove < -fabs( cl_backspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_backspeed.GetFloat() ) / fabs( pCmd->forwardmove );
	}

	float flSideScale = 1.0f;
	if ( fabs( pCmd->sidemove ) > fabs( cl_sidespeed.GetFloat() ) )
	{
		flSideScale = fabs( cl_sidespeed.GetFloat() ) / fabs( pCmd->sidemove );
	}

	float flScale = min( flForwardScale, flSideScale );
	pCmd->forwardmove *= flScale;
	pCmd->sidemove *= flScale;

	//Msg( "Pforwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );
}

ConVar cl_use_portalling_angle_fix("cl_use_portalling_angle_fix", "0", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_ARCHIVE, "Attempts to make portal teleportations seem less laggy");

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInputSampleTime - 
//			*pCmd - 
//-----------------------------------------------------------------------------
bool C_Portal_Player::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{	
	bool bFakeLag = false;
	ConVarRef net_fakelag("net_fakelag");
	if (net_fakelag.IsValid())
	{
		bFakeLag = (net_fakelag.GetInt() != 0);
	}

	if ( m_hPortalEnvironment.Get() )
		DetectAndHandlePortalTeleportation( m_hPortalEnvironment.Get() );

	// This works GREAT for everyone! Well, except for the listen server host.
	// What happens is that it the stored view angles are set before the listen server host actually teleports, and portal angles have never been a problem for the listen server host.
	if ( (m_bShouldFixAngles && !m_bIsListenServerHost ) || ( m_bShouldFixAngles && m_bIsListenServerHost && bFakeLag ) )
	{
		m_bShouldFixAngles = false;
		// This is just causing way too many problems, it could've worked, but it doesn't very well. I'll make it optional though.
		if (cl_use_portalling_angle_fix.GetBool())
		{
			pCmd->viewangles = m_qPrePortalledStoredViewAngles;
			FixEyeAnglesFromPortalling(); 
		}
	}

	AvoidPlayers( pCmd );

	return BaseClass::CreateMove(flInputSampleTime, pCmd);
}

void C_Portal_Player::SetupMove( CUserCmd *ucmd, IMoveHelper *pHelper )
{
#if 0
	if (m_bFixEyeAnglesFromPortalling)
	{
		//the idea here is to handle the notion that the player has portalled, but they sent us an angle update before receiving that message.
		//If we don't handle this here, we end up sending back their old angles which makes them hiccup their angles for a frame
		float fOldAngleDiff = fabs(AngleDistance(ucmd->viewangles.x, m_qPrePortalledViewAngles.x));
		fOldAngleDiff += fabs(AngleDistance(ucmd->viewangles.y, m_qPrePortalledViewAngles.y));
		fOldAngleDiff += fabs(AngleDistance(ucmd->viewangles.z, m_qPrePortalledViewAngles.z));

		float fCurrentAngleDiff = fabs(AngleDistance(ucmd->viewangles.x, pl.v_angle.x));
		fCurrentAngleDiff += fabs(AngleDistance(ucmd->viewangles.y, pl.v_angle.y));
		fCurrentAngleDiff += fabs(AngleDistance(ucmd->viewangles.z, pl.v_angle.z));

		if (fCurrentAngleDiff > fOldAngleDiff)
			ucmd->viewangles = TransformAnglesToWorldSpace(ucmd->viewangles, m_matLastPortalled.As3x4());

		m_bFixEyeAnglesFromPortalling = false;
	}
#endif
}

//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_Portal_Player::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( IsEffectActive( EF_NODRAW ) )
		return false;

	if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		return true;
	}

	return BaseClass::ShouldReceiveProjectedTextures( flags );
}

void C_Portal_Player::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->DoImpactEffect( tr, nDamageType );
		return;
	}

	BaseClass::DoImpactEffect( tr, nDamageType );
}

void C_Portal_Player::PreThink( void )
{
	QAngle vTempAngles = GetLocalAngles();

	if ( IsLocalPlayer() )
	{
		vTempAngles[PITCH] = EyeAngles()[PITCH];
	}
	else
	{
		vTempAngles[PITCH] = m_angEyeAngles[PITCH];
	}

	if ( vTempAngles[YAW] < 0.0f )
	{
		vTempAngles[YAW] += 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();
	
	HandleSpeedChanges();

	FixPortalEnvironmentOwnership();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Portal_Player::AddEntity( void )
{
	BaseClass::AddEntity();

	QAngle vTempAngles = GetLocalAngles();
	vTempAngles[PITCH] = m_angEyeAngles[PITCH];

	SetLocalAngles( vTempAngles );

	UpdateLookAt();
	
#if 0
	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( IsLocalPlayer() )
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	else
		m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
#endif

	// Zero out model pitch, blending takes care of all of it.
	SetLocalAnglesDim( X_INDEX, 0 );

	if( this != C_BasePlayer::GetLocalPlayer() )
	{
		if ( IsEffectActive( EF_DIMLIGHT ) )
		{
			int iAttachment = LookupAttachment( "anim_attachment_RH" );

			if ( iAttachment < 0 )
				return;

			Vector vecOrigin;
			QAngle eyeAngles = m_angEyeAngles;

			GetAttachment( iAttachment, vecOrigin, eyeAngles );

			Vector vForward;
			AngleVectors( eyeAngles, &vForward );

			trace_t tr;
			UTIL_TraceLine( vecOrigin, vecOrigin + (vForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		}
	}
}

ShadowType_t C_Portal_Player::ShadowCastType( void ) 
{
	// Drawing player shadows looks bad in first person when they get close to walls
	// It doesn't make sense to have shadows in the portal view, but not in the main view
	// So no shadows for the player
	return SHADOWS_NONE;
}

bool C_Portal_Player::ShouldDraw( void )
{
	if ( !IsAlive() )
		return false;

	//return true;

	//	if( GetTeamNumber() == TEAM_SPECTATOR )
	//		return false;

	if( IsLocalPlayer() && IsRagdoll() )
		return true;

	if ( IsRagdoll() )
		return false;

	return true;

	return BaseClass::ShouldDraw();
}

const QAngle& C_Portal_Player::EyeAngles()
{
	if ( IsLocalPlayer() && g_nKillCamMode == OBS_MODE_NONE )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		//C_BaseEntity *pEntity1 = g_eKillTarget1.Get();
		//C_BaseEntity *pEntity2 = g_eKillTarget2.Get();

		//Vector vLook = Vector( 0.0f, 0.0f, 0.0f );

		//if ( pEntity2 )
		//{
		//	vLook = pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin();
		//	VectorNormalize( vLook );
		//}
		//else if ( pEntity1 )
		//{
		//	return BaseClass::EyeAngles();
		//	//vLook =  - pEntity1->GetAbsOrigin();
		//}

		//if ( vLook != Vector( 0.0f, 0.0f, 0.0f ) )
		//{
		//	VectorAngles( vLook, m_angEyeAngles );
		//}

		return m_angEyeAngles;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_Portal_Player::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_PortalRagdoll *pRagdoll = static_cast<C_PortalRagdoll*>( m_hRagdoll.Get() );
		if ( !pRagdoll )
			return NULL;

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}

#if 0
CON_COMMAND(setviewangletesting, "Sets angles to 0 90 0")
{	
	C_Portal_Player *pPlayer = C_Portal_Player::GetLocalPortalPlayer();

	QAngle qTestAngle = QAngle(0, 90, 0);

	pPlayer->SetViewAngles( qTestAngle );

	//engine view angles (for mouse input smoothness)
	{
		engine->SetViewAngles( qTestAngle );
	}

	//predicted view angles
	{
		prediction->SetViewAngles( qTestAngle );
	}
}
#endif

void C_Portal_Player::PrePlayerPortalled( void )
{	
	m_qPrePortalledStoredViewAngles = MainViewAngles();
	m_qPrePortalledStoredAngles = GetLocalAngles();
}

void C_Portal_Player::PlayerPortalled( C_Prop_Portal *pEnteredPortal, float fTime, bool bForcedDuck )
{
#if USEMOVEMENTFORPORTALLING
	
	//Warning( "C_Portal_Player::PlayerPortalled( %s ) ent:%i slot:%i\n", IsLocalPlayer() ? "local" : "nonlocal", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
#if ( PLAYERPORTALDEBUGSPEW == 1 )
	Warning( "C_Portal_Player::PlayerPortalled( %f %f %f %i ) %i\n", fTime, engine->GetLastTimeStamp(), GetTimeBase(), prediction->GetLastAcknowledgedCommandNumber(), m_PredictedPortalTeleportations.Count() );
#endif

	/*{
		QAngle qEngineView;
		engine->GetViewAngles( qEngineView );
		Warning( "Client player portalled %f   %f %f %f\n\t%f %f %f   %f %f %f\n", gpGlobals->curtime, XYZ( GetNetworkOrigin() ), XYZ( pl.v_angle ), XYZ( qEngineView ) ); 
	}*/

	if ( pEnteredPortal )
	{
		C_Prop_Portal *pRemotePortal = pEnteredPortal->m_hLinkedPortal;

		m_bPortalledMessagePending = true;
		m_PendingPortalMatrix = pEnteredPortal->MatrixThisToLinked();

		
		bool bFakeLag = false;
		ConVarRef net_fakelag("net_fakelag");
		if (net_fakelag.IsValid())
		{
			bFakeLag = (net_fakelag.GetInt() != 0);
		}

		if (!m_bIsListenServerHost || bFakeLag)
			m_bShouldFixAngles = true;

		if( IsLocalPlayer( ) && pRemotePortal )
		{
			g_pPortalRender->EnteredPortal( pEnteredPortal );
		}

		if( !GetPredictable() )
		{
			//non-predicted case
			ApplyUnpredictedPortalTeleportation( pEnteredPortal, fTime, bForcedDuck );
		}
		else
		{
			if( m_PredictedPortalTeleportations.Count() == 0 )
			{
				//surprise teleportation
#if ( PLAYERPORTALDEBUGSPEW == 1 )
				Warning( "C_Portal_Player::PlayerPortalled()  No predicted teleportations %f %f\n", gpGlobals->curtime, fTime );
#endif
				ApplyUnpredictedPortalTeleportation( pEnteredPortal, fTime, bForcedDuck );

			}
			else
			{				
				PredictedPortalTeleportation_t shouldBeThisTeleport = m_PredictedPortalTeleportations.Head();

				if( pEnteredPortal != shouldBeThisTeleport.pEnteredPortal )
				{
					AssertMsg( false, "predicted teleportation through the wrong portal." ); //we don't have any test cases for this happening. So the logic is accordingly untested.
					Warning( "C_Portal_Player::PlayerPortalled()  Mismatched head teleportation %f, %f %f\n", gpGlobals->curtime, shouldBeThisTeleport.flTime, fTime );
					UnrollPredictedTeleportations( shouldBeThisTeleport.iCommandNumber );
					ApplyUnpredictedPortalTeleportation( pEnteredPortal, fTime, bForcedDuck );
				}
				else
				{
#if ( PLAYERPORTALDEBUGSPEW == 1 )
					Warning( "C_Portal_Player::PlayerPortalled()  Existing teleportation at %f correct, %f %f\n", m_PredictedPortalTeleportations[0].flTime, gpGlobals->curtime, fTime );
#endif
					m_PredictedPortalTeleportations.Remove( 0 );
				}
			}
		}

		if( pRemotePortal != NULL )
		{
			m_matLatestServerTeleportationInverseMatrix = pRemotePortal->MatrixThisToLinked();
		}
		else
		{
			m_matLatestServerTeleportationInverseMatrix.Identity();
		}
	}

	m_fLatestServerTeleport = fTime;

#else
	if( pEnteredPortal )
	{
		m_bPortalledMessagePending = true;
		m_PendingPortalMatrix = pEnteredPortal->MatrixThisToLinked();

		if( IsLocalPlayer() )
			g_pPortalRender->EnteredPortal( pEnteredPortal );
	}
#endif
}

void C_Portal_Player::FixEyeAnglesFromPortalling( void )
{	
	
	SetViewAngles( m_qPrePortalledStoredAngles );

	// Unbeknownst to me, this was a terrible idea as it causes angles to improperly snap
#if 0
	//engine view angles (for mouse input smoothness)
	{
		engine->SetViewAngles( m_qPrePortalledStoredViewAngles );
	}

	//predicted view angles
	{
		prediction->SetViewAngles( m_qPrePortalledStoredViewAngles );
	}
#endif

	// Unnecessary
#if 0
	SetLocalAngles(m_qPrePortalledStoredAngles);
	SetAbsAngles(m_qPrePortalledStoredAngles);
	SetNetworkAngles(m_qPrePortalledStoredAngles);
#endif

}

void C_Portal_Player::CheckPlayerAboutToTouchPortal( void )
{
//This doesn't seem to be useful...
#if 0
	// don't run this code unless we are in MP and are using the robots
	if ( gpGlobals->maxClients == 1 )
		return;
		
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return;

	float fFlingTrail = GetAbsVelocity().Length() - MIN_FLING_SPEED;
	// if we aren't going at least fling speed, don't both with the code below
	if ( fFlingTrail <= 0 )
		return;

	Vector vecVelocity = GetAbsVelocity();

	Vector vMin, vMax;
	CollisionProp()->WorldSpaceAABB( &vMin, &vMax );
	Vector ptCenter = ( vMin + vMax ) * 0.5f;
	Vector vExtents = ( vMax - vMin ) * 0.5f;

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	for( int i = 0; i != iPortalCount; ++i )
	{
		CProp_Portal *pTempPortal = pPortals[i];
		if( pTempPortal->IsActive() && 
			(pTempPortal->m_hLinkedPortal.Get() != NULL) &&
			UTIL_IsBoxIntersectingPortal( ptCenter, vExtents, pTempPortal )	)
		{
			Vector vecDirToPortal = ptCenter - pTempPortal->GetAbsOrigin();
			VectorNormalize(vecDirToPortal);
			Vector vecDirMotion = vecVelocity;
			VectorNormalize(vecDirMotion);
			float dot = DotProduct( vecDirToPortal, vecDirMotion );

			// If the portal is behind our direction of movement, then we probably just came out of it
			// IGNORE
			if ( dot > 0.0f )
				continue;
			
			// if we're flinging and we touched a portal
			if ( m_FlingTrailEffect && !m_bFlingTrailPrePortalled && !m_bFlingTrailJustPortalled )
			{
				// stop the effect linger effect if it exists
				m_FlingTrailEffect->SetOwner( NULL );
				ParticleProp()->StopEmission( m_FlingTrailEffect, false, true, false );
				m_FlingTrailEffect = NULL;
				m_bFlingTrailActive = false;
				m_bFlingTrailPrePortalled = true;
				return;
			}
		}
	}
#endif
}

void C_Portal_Player::OnPreDataChanged( DataUpdateType_t type )
{
	BaseClass::OnPreDataChanged( type );
}

void C_Portal_Player::PreDataUpdate( DataUpdateType_t updateType )
{
	//Assert( m_pPortalEnvironment_LastCalcView == m_hPortalEnvironment.Get() );
	PreDataChanged_Backup.m_hPortalEnvironment = m_hPortalEnvironment;
	PreDataChanged_Backup.m_hSurroundingLiquidPortal = m_hSurroundingLiquidPortal;
	PreDataChanged_Backup.m_qEyeAngles = m_iv_angEyeAngles.GetCurrent();

	BaseClass::PreDataUpdate( updateType );
}

void C_Portal_Player::FixPortalEnvironmentOwnership(void)
{
	CPortalSimulator *pExistingSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity(this);
	C_Prop_Portal *pPortalEnvironment = m_hPortalEnvironment;
	CPortalSimulator *pNewSimulator = pPortalEnvironment ? &pPortalEnvironment->m_PortalSimulator : NULL;
	if (pExistingSimulator != pNewSimulator)
	{
		if (pExistingSimulator)
		{
			pExistingSimulator->ReleaseOwnershipOfEntity(this);
		}

		if (pNewSimulator)
		{
			pNewSimulator->TakeOwnershipOfEntity(this);
		}
	}
}

void C_Portal_Player::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	FixPortalEnvironmentOwnership();

	if( m_hSurroundingLiquidPortal != PreDataChanged_Backup.m_hSurroundingLiquidPortal )
	{
		CLiquidPortal_InnerLiquidEffect *pLiquidEffect = (CLiquidPortal_InnerLiquidEffect *)g_pScreenSpaceEffects->GetScreenSpaceEffect( "LiquidPortal_InnerLiquid" );
		if( pLiquidEffect )
		{
			C_Func_LiquidPortal *pSurroundingPortal = m_hSurroundingLiquidPortal.Get();
			if( pSurroundingPortal != NULL )
			{
				C_Func_LiquidPortal *pOldSurroundingPortal = PreDataChanged_Backup.m_hSurroundingLiquidPortal.Get();
				if( pOldSurroundingPortal != pSurroundingPortal->m_hLinkedPortal.Get() )
				{
					pLiquidEffect->m_pImmersionPortal = pSurroundingPortal;
					pLiquidEffect->m_bFadeBackToReality = false;
				}
				else
				{
					pLiquidEffect->m_bFadeBackToReality = true;
					pLiquidEffect->m_fFadeBackTimeLeft = pLiquidEffect->s_fFadeBackEffectTime;
				}
			}
			else
			{
				pLiquidEffect->m_pImmersionPortal = NULL;
				pLiquidEffect->m_bFadeBackToReality = false;
			}
		}		
	}

	CProp_Portal *pPortal = m_hPortalEnvironment.Get();
	
	if (pPortal)
		DetectAndHandlePortalTeleportation(pPortal);

	if ( type == DATA_UPDATE_CREATED )
	{
#ifdef CCDEATH
		// Load color correction lookup for the death effect
		m_CCDeathHandle = g_pColorCorrectionMgr->AddColorCorrection( DEATH_CC_LOOKUP_FILENAME );
		if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE )
		{
			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, 0.0f );
		}
#endif
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateVisibility();
}

//CalcView() gets called between OnPreDataChanged() and OnDataChanged(), and these changes need to be known about in both before CalcView() gets called, and if CalcView() doesn't get called
bool C_Portal_Player::DetectAndHandlePortalTeleportation( CProp_Portal *pPortal )
{	
	if( m_bPortalledMessagePending )
	{
		m_bPortalledMessagePending = false;

		//C_Prop_Portal *pOldPortal = PreDataChanged_Backup.m_hPortalEnvironment.Get();
		//Assert( pOldPortal );
		//if( pOldPortal )
		{
			Vector ptNewPosition = GetNetworkOrigin();

			UTIL_Portal_PointTransform( m_PendingPortalMatrix, PortalEyeInterpolation.m_vEyePosition_Interpolated, PortalEyeInterpolation.m_vEyePosition_Interpolated );
			UTIL_Portal_PointTransform( m_PendingPortalMatrix, PortalEyeInterpolation.m_vEyePosition_Uninterpolated, PortalEyeInterpolation.m_vEyePosition_Uninterpolated );

			PortalEyeInterpolation.m_bEyePositionIsInterpolating = true;

			UTIL_Portal_AngleTransform( m_PendingPortalMatrix, m_qEyeAngles_LastCalcView, m_angEyeAngles );
			m_angEyeAngles.x = AngleNormalize( m_angEyeAngles.x );
			m_angEyeAngles.y = AngleNormalize( m_angEyeAngles.y );
			m_angEyeAngles.z = AngleNormalize( m_angEyeAngles.z );
			m_iv_angEyeAngles.Reset( gpGlobals->curtime ); //copies from m_angEyeAngles

			if( engine->IsPlayingDemo() )
			{				
				pl.v_angle = m_angEyeAngles;		
				engine->SetViewAngles( pl.v_angle );
			}

			engine->ResetDemoInterpolation();
			if( IsLocalPlayer() ) 
			{
				//DevMsg( "FPT: %.2f %.2f %.2f\n", m_angEyeAngles.x, m_angEyeAngles.y, m_angEyeAngles.z );
				SetLocalAngles( m_angEyeAngles );
			}

			m_PlayerAnimState->Teleport ( &ptNewPosition, &GetNetworkAngles(), this );

			// Reorient last facing direction to fix pops in view model lag
			for ( int i = 0; i < MAX_VIEWMODELS; i++ )
			{
				CBaseViewModel *vm = GetViewModel( i );
				if ( !vm )
					continue;

				UTIL_Portal_VectorTransform( m_PendingPortalMatrix, vm->m_vecLastFacing, vm->m_vecLastFacing );
			}
		}
		m_bPortalledMessagePending = false;
	}

	return false;
}

/*bool C_Portal_Player::ShouldInterpolate( void )
{
if( !IsInterpolationEnabled() )
return false;

return BaseClass::ShouldInterpolate();
}*/


void C_Portal_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate(updateType);

	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );

	if ( m_iSpawnInterpCounter != m_iSpawnInterpCounterCache )
	{
		MoveToLastReceivedPosition( true );
		ResetLatched();
		m_iSpawnInterpCounterCache = m_iSpawnInterpCounter;
	}

	FixPortalEnvironmentOwnership();
}

float C_Portal_Player::GetFOV( void )
{
	//Find our FOV with offset zoom value
	float flFOVOffset = C_BasePlayer::GetFOV() + GetZoom();

	// Clamp FOV in MP
	int min_fov = GetMinFOV();

	// Don't let it go too low
	flFOVOffset = MAX( min_fov, flFOVOffset );

	return flFOVOffset;
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector C_Portal_Player::GetAutoaimVector( float flDelta )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( EyeAngles() + m_Local.m_vecPunchAngle, &forward );
	return	forward;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we are allowed to sprint now.
//-----------------------------------------------------------------------------
bool C_Portal_Player::CanSprint( void )
{
	if (!m_bHasSprintDevice || !m_bSprintEnabled)
	{
		return false;
	}

	return ( (!m_Local.m_bDucked && !m_Local.m_bDucking) && (GetWaterLevel() != 3) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_Portal_Player::StartSprinting( void )
{
	//if( m_HL2Local.m_flSuitPower < 10 )
	//{
	//	// Don't sprint unless there's a reasonable
	//	// amount of suit power.
	//	CPASAttenuationFilter filter( this );
	//	filter.UsePredictionRules();
	//	EmitSound( filter, entindex(), "Player.SprintNoPower" );
	//	return;
	//}

	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "Player.SprintStart" );

	SetMaxSpeed( _SPRINT_SPEED );
	m_fIsSprinting = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_Portal_Player::StopSprinting( void )
{
	SetMaxSpeed( _NORM_SPEED );
	m_fIsSprinting = false;
}

void C_Portal_Player::HandleSpeedChanges( void )
{
	int buttonsChanged = m_afButtonPressed | m_afButtonReleased;

	if( buttonsChanged & IN_SPEED )
	{
		if ( !(m_afButtonPressed & IN_SPEED)  && IsSprinting() )
		{
			StopSprinting();
		}
		else if ( (m_afButtonPressed & IN_SPEED) && !IsSprinting() )
		{
			if ( CanSprint() )
			{
				StartSprinting();
			}
			else
			{
				// Reset key, so it will be activated post whatever is suppressing it.
				m_nButtons &= ~IN_SPEED;
			}
		}
	}
}

void C_Portal_Player::ItemPreFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		return;

	// Disallow shooting while zooming
	if ( m_nButtons & IN_ZOOM )
	{
		//FIXME: Held weapons like the grenade get sad when this happens
		m_nButtons &= ~(IN_ATTACK|IN_ATTACK2);
	}

	BaseClass::ItemPreFrame();

}

void C_Portal_Player::ItemPostFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		return;

	BaseClass::ItemPostFrame();
}

C_BaseAnimating *C_Portal_Player::BecomeRagdollOnClient()
{
	// Let the C_CSRagdoll entity do this.
	// m_builtRagdoll = true;
	return NULL;
}

#include "GameUI/IGameUI.h"


static CDllDemandLoader g_GameUI( "GameUI" );

extern CBonusMapsDialog *g_pBonusMapsDialog;

void C_Portal_Player::FireGameEvent( IGameEvent *event )
{

	// GameUI Interface
	IGameUI *pGameUI = NULL;

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if (gameUIFactory)
	{
		pGameUI = (IGameUI *)gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL);
	}


	if ( FStrEq( event->GetName(), "bonusmap_unlock" ) )
	{
		const char* pszMapname = event->GetString("mapname");
		const char* pszFilename = event->GetString("filename");
		
#if 0
		if ( pGameUI )
		{
			pGameUI->BonusMapUnlock( pszFilename, pszMapname );	

			g_pBonusMapsDialog->SetSelectedBooleanStatus("lock", false);
		}
#else
		if ( BonusMapsDatabase()->SetBooleanStatus( "lock", pszFilename, pszMapname, false ) )
		{
			BonusMapsDatabase()->RefreshMapData();

			if ( !g_pBonusMapsDialog )
			{
#if 0
				// It unlocked without the bonus maps menu open, so flash the menu item
				CBasePanel *pBasePanel = BasePanel();
				if ( pBasePanel )
				{
					if ( GameUI().IsConsoleUI() )
					{
						if ( Q_stricmp( pchFileName, "scripts/advanced_chambers" ) == 0 )
						{
							pBasePanel->SetMenuItemBlinkingState( "OpenNewGameDialog", true );
						}
					}
					else
					{
						pBasePanel->SetMenuItemBlinkingState( "OpenBonusMapsDialog", true );
					}
				}

				BonusMapsDatabase()->SetBlink( true );
#endif
			}
			else
				g_pBonusMapsDialog->RefreshData();	// Update the open dialog
		}
#endif
	}
	else if ( FStrEq( event->GetName(), "advanced_map_complete" ) )
	{
		const char* pszMapname = event->GetString("mapname");
		const char* pszFilename = event->GetString("filename");
											
		if ( pGameUI )
		{
			pGameUI->BonusMapComplete( pszFilename, pszMapname );
			pGameUI->BonusMapNumAdvancedCompleted();
		}
		
	}
	else if ( FStrEq( event->GetName(), "bonusmap_save" ) )
	{
		BonusMapsDatabase()->WriteSaveData();		
	}
	else if ( FStrEq( event->GetName(), "RefreshBonusData" ) )
	{
		// Update the open dialog
		if ( g_pBonusMapsDialog )
			g_pBonusMapsDialog->RefreshData();
		
	}	


	BaseClass::FireGameEvent( event );
}

void C_Portal_Player::UpdatePortalEyeInterpolation( void )
{
#ifdef ENABLE_PORTAL_EYE_INTERPOLATION_CODE
	//PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;
	if( PortalEyeInterpolation.m_bUpdatePosition_FreeMove )
	{
		PortalEyeInterpolation.m_bUpdatePosition_FreeMove = false;

		C_Prop_Portal *pOldPortal = PreDataChanged_Backup.m_hPortalEnvironment.Get();
		if( pOldPortal )
		{
			UTIL_Portal_PointTransform( pOldPortal->MatrixThisToLinked(), PortalEyeInterpolation.m_vEyePosition_Interpolated, PortalEyeInterpolation.m_vEyePosition_Interpolated );
			//PortalEyeInterpolation.m_vEyePosition_Interpolated = pOldPortal->m_matrixThisToLinked * PortalEyeInterpolation.m_vEyePosition_Interpolated;

			//Vector vForward;
			//m_hPortalEnvironment.Get()->GetVectors( &vForward, NULL, NULL );

			PortalEyeInterpolation.m_vEyePosition_Interpolated = EyeFootPosition();

			PortalEyeInterpolation.m_bEyePositionIsInterpolating = true;
		}
	}

	if( IsInAVehicle() )
		PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;

	if( !PortalEyeInterpolation.m_bEyePositionIsInterpolating )
	{
		PortalEyeInterpolation.m_vEyePosition_Uninterpolated = EyeFootPosition();
		PortalEyeInterpolation.m_vEyePosition_Interpolated = PortalEyeInterpolation.m_vEyePosition_Uninterpolated;
		return;
	}

	Vector vThisFrameUninterpolatedPosition = EyeFootPosition();

	//find offset between this and last frame's uninterpolated movement, and apply this as freebie movement to the interpolated position
	PortalEyeInterpolation.m_vEyePosition_Interpolated += (vThisFrameUninterpolatedPosition - PortalEyeInterpolation.m_vEyePosition_Uninterpolated);
	PortalEyeInterpolation.m_vEyePosition_Uninterpolated = vThisFrameUninterpolatedPosition;

	Vector vDiff = vThisFrameUninterpolatedPosition - PortalEyeInterpolation.m_vEyePosition_Interpolated;
	float fLength = vDiff.Length();
	float fFollowSpeed = gpGlobals->frametime * 100.0f;
	const float fMaxDiff = 150.0f;
	if( fLength > fMaxDiff )
	{
		//camera lagging too far behind, give it a speed boost to bring it within maximum range
		fFollowSpeed = fLength - fMaxDiff;
	}
	else if( fLength < fFollowSpeed )
	{
		//final move
		PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;
		PortalEyeInterpolation.m_vEyePosition_Interpolated = vThisFrameUninterpolatedPosition;
		return;
	}

	if ( fLength > 0.001f )
	{
		vDiff *= (fFollowSpeed/fLength);
		PortalEyeInterpolation.m_vEyePosition_Interpolated += vDiff;
	}
	else
	{
		PortalEyeInterpolation.m_vEyePosition_Interpolated = vThisFrameUninterpolatedPosition;
	}



#else
	PortalEyeInterpolation.m_vEyePosition_Interpolated = BaseClass::EyePosition();
#endif
}

Vector C_Portal_Player::EyePosition()
{
	return PortalEyeInterpolation.m_vEyePosition_Interpolated;  
}

Vector C_Portal_Player::EyeFootPosition( const QAngle &qEyeAngles )
{
#if 0
	static int iPrintCounter = 0;
	++iPrintCounter;
	if( iPrintCounter == 50 )
	{
		QAngle vAbsAngles = qEyeAngles;
		DevMsg( "Eye Angles: %f %f %f\n", vAbsAngles.x, vAbsAngles.y, vAbsAngles.z );
		iPrintCounter = 0;
	}
#endif

	//interpolate between feet and normal eye position based on view roll (gets us wall/ceiling & ceiling/ceiling teleportations without an eye position pop)
	float fFootInterp = fabs(qEyeAngles[ROLL]) * ((1.0f/180.0f) * 0.75f); //0 when facing straight up, 0.75 when facing straight down
	return (BaseClass::EyePosition() - (fFootInterp * m_vecViewOffset)); //TODO: Find a good Up vector for this rolled player and interpolate along actual eye/foot axis
}

void C_Portal_Player::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	C_Prop_Portal *pPortalBackup = m_hPortalEnvironment.Get();
	if (pPortalBackup)
		DetectAndHandlePortalTeleportation( pPortalBackup );
	//if( DetectAndHandlePortalTeleportation() )
	//	DevMsg( "Teleported within OnDataChanged\n" );

	m_iForceNoDrawInPortalSurface = -1;
	bool bEyeTransform_Backup = m_bEyePositionIsTransformedByPortal;
	m_bEyePositionIsTransformedByPortal = false; //assume it's not transformed until it provably is
	UpdatePortalEyeInterpolation();

	QAngle qEyeAngleBackup = EyeAngles();
	Vector ptEyePositionBackup = EyePosition();

	if ( m_lifeState != LIFE_ALIVE )
	{
		if ( g_nKillCamMode != 0 )
		{
			return;
		}

		Vector origin = EyePosition();

		C_BaseEntity* pRagdoll = m_hRagdoll.Get();

		if ( pRagdoll )
		{
			origin = pRagdoll->GetAbsOrigin();
#if !PORTAL_HIDE_PLAYER_RAGDOLL
			origin.z += VEC_DEAD_VIEWHEIGHT_SCALED( this ).z; // look over ragdoll, not through
#endif //PORTAL_HIDE_PLAYER_RAGDOLL
		}

		BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );

		eyeOrigin = origin;

		Vector vForward; 
		AngleVectors( eyeAngles, &vForward );

		VectorNormalize( vForward );
#if !PORTAL_HIDE_PLAYER_RAGDOLL
		VectorMA( origin, -CHASE_CAM_DISTANCE_MAX, vForward, eyeOrigin );
#endif //PORTAL_HIDE_PLAYER_RAGDOLL

		Vector WALL_MIN( -WALL_OFFSET, -WALL_OFFSET, -WALL_OFFSET );
		Vector WALL_MAX( WALL_OFFSET, WALL_OFFSET, WALL_OFFSET );

		trace_t trace; // clip against world
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();

		if (trace.fraction < 1.0)
		{
			eyeOrigin = trace.endpos;
		}
		
	// in multiplayer we want to make sure we get the screenshakes and stuff
	//	if ( gpGlobals->maxClients >> 1 )
	//		CalcPortalView( eyeOrigin, eyeAngles );

	}
	else
	{
		IClientVehicle *pVehicle; 
		pVehicle = GetVehicle();

		if ( !pVehicle )
		{
			if ( IsObserver() )
			{
				CalcObserverView( eyeOrigin, eyeAngles, fov );
			}
			else
			{
				CalcPlayerView( eyeOrigin, eyeAngles, fov );
				if( m_hPortalEnvironment.Get() != NULL )
				{
					//time for hax
					m_bEyePositionIsTransformedByPortal = bEyeTransform_Backup;
					CalcPortalView( eyeOrigin, eyeAngles );
				}
			}
		}
		else
		{
			CalcVehicleView( pVehicle, eyeOrigin, eyeAngles, zNear, zFar, fov );
		}
	}

	m_qEyeAngles_LastCalcView = qEyeAngleBackup;
	m_ptEyePosition_LastCalcView = ptEyePositionBackup;
	m_pPortalEnvironment_LastCalcView = pPortalBackup;

#ifdef WIN32s
	// NVNT Inform haptics module of fov
	if(IsLocalPlayer())
		haptics->UpdatePlayerFOV(fov);
#endif
}

void C_Portal_Player::SetLocalViewAngles( const QAngle &viewAngles )
{
	// Nothing
	if ( engine->IsPlayingDemo() )
		return;
	BaseClass::SetLocalViewAngles( viewAngles );
}

void C_Portal_Player::SetViewAngles( const QAngle& ang )
{
	BaseClass::SetViewAngles( ang );

	if ( engine->IsPlayingDemo() )
	{
		pl.v_angle = ang;
	}
}

void C_Portal_Player::CalcPortalView( Vector &eyeOrigin, QAngle &eyeAngles )
{
	if ( !prediction->InPrediction() )
	{
		// FIXME: Move into prediction
		view->DriftPitch();
	}

	//although we already ran CalcPlayerView which already did these copies, they also fudge these numbers in ways we don't like, so recopy
	VectorCopy( EyePosition(), eyeOrigin );
	VectorCopy( EyeAngles(), eyeAngles );
	
	if ( !prediction->InPrediction() )
	{
		SmoothViewOnStairs( eyeOrigin );
	}

	// Apply punch angle
	VectorAdd( eyeAngles, m_Local.m_vecPunchAngle, eyeAngles );

	//Re-apply the screenshake (we just stomped it)
	vieweffects->ApplyShake( eyeOrigin, eyeAngles, 1.0 );

#if USEMOVEMENTFORPORTALLING
	// Apply a smoothing offset to smooth out prediction errors.

	if ( sv_portal_with_gamemovement.GetBool() )
	{
		Vector vSmoothOffset;
		GetPredictionErrorSmoothingVector( vSmoothOffset );
		eyeOrigin += vSmoothOffset;
	}
#endif

	C_Prop_Portal *pPortal = m_hPortalEnvironment.Get();

	if (!pPortal)
		return;

	C_Prop_Portal *pRemotePortal = NULL;
	assert( pPortal );
	if (pPortal)
	{
		pRemotePortal = pPortal->m_hLinkedPortal.Get();
		if( !pRemotePortal )
		{
			return; //no hacks possible/necessary
		}
	}
	Vector ptPortalCenter;
	Vector vPortalForward;

	//ptPortalCenter = pPortal->GetNetworkOrigin();
	ptPortalCenter = pPortal->m_ptOrigin;
	pPortal->GetVectors( &vPortalForward, NULL, NULL );
	float fPortalPlaneDist = vPortalForward.Dot( ptPortalCenter );

	bool bOverrideSpecialEffects = false; //sometimes to get the best effect we need to kill other effects that are simply for cleanliness

	float fEyeDist = vPortalForward.Dot( eyeOrigin ) - fPortalPlaneDist;
	bool bTransformEye = false;
	if( fEyeDist < 0.0f ) //eye behind portal
	{
		if( pPortal->m_PortalSimulator.EntityIsInPortalHole( this ) ) //player standing in portal
		{
			bTransformEye = true;
		}
		else if( vPortalForward.z < -0.01f ) //there's a weird case where the player is ducking below a ceiling portal. As they unduck their eye moves beyond the portal before the code detects that they're in the portal hole.
		{
			Vector ptPlayerOrigin = GetAbsOrigin();
			float fOriginDist = vPortalForward.Dot( ptPlayerOrigin ) - fPortalPlaneDist;

			if( fOriginDist > 0.0f )
			{
				float fInvTotalDist = 1.0f / (fOriginDist - fEyeDist); //fEyeDist is negative
				Vector ptPlaneIntersection = (eyeOrigin * fOriginDist * fInvTotalDist) - (ptPlayerOrigin * fEyeDist * fInvTotalDist);
				Assert( fabs( vPortalForward.Dot( ptPlaneIntersection ) - fPortalPlaneDist ) < 0.01f );

				Vector vIntersectionTest = ptPlaneIntersection - ptPortalCenter;

				Vector vPortalRight, vPortalUp;
				pPortal->GetVectors( NULL, &vPortalRight, &vPortalUp );

				if( (vIntersectionTest.Dot( vPortalRight ) <= PORTAL_HALF_WIDTH) &&
					(vIntersectionTest.Dot( vPortalUp ) <= PORTAL_HALF_HEIGHT) )
				{
					bTransformEye = true;
				}
			}
		}		
	}

	if( bTransformEye )
	{
		m_bEyePositionIsTransformedByPortal = true;

		//DevMsg( 2, "transforming portal view from <%f %f %f> <%f %f %f>\n", eyeOrigin.x, eyeOrigin.y, eyeOrigin.z, eyeAngles.x, eyeAngles.y, eyeAngles.z );

		VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
		UTIL_Portal_PointTransform( matThisToLinked, eyeOrigin, eyeOrigin );
		UTIL_Portal_AngleTransform( matThisToLinked, eyeAngles, eyeAngles );

		//DevMsg( 2, "transforming portal view to   <%f %f %f> <%f %f %f>\n", eyeOrigin.x, eyeOrigin.y, eyeOrigin.z, eyeAngles.x, eyeAngles.y, eyeAngles.z );

		if ( IsToolRecording() )
		{
			static EntityTeleportedRecordingState_t state;

			KeyValues *msg = new KeyValues( "entity_teleported" );
			msg->SetPtr( "state", &state );
			state.m_bTeleported = false;
			state.m_bViewOverride = true;
			state.m_vecTo = eyeOrigin;
			state.m_qaTo = eyeAngles;
			MatrixInvert( matThisToLinked.As3x4(), state.m_teleportMatrix );

			// Post a message back to all IToolSystems
			Assert( (int)GetToolHandle() != 0 );
			ToolFramework_PostToolMessage( GetToolHandle(), msg );

			msg->deleteThis();
		}

		bOverrideSpecialEffects = true;
	}
	else
	{
		m_bEyePositionIsTransformedByPortal = false;
	}

	if( bOverrideSpecialEffects && pRemotePortal )
	{		
		m_iForceNoDrawInPortalSurface = ((pRemotePortal->m_bIsPortal2)?(2):(1));
		pRemotePortal->m_fStaticAmount = 0.0f;
	}
}

void C_Portal_Player::GetToolRecordingState( KeyValues *msg )
{
	BaseClass::GetToolRecordingState( msg );

	if( m_bToolMode_EyeHasPortalled_LastRecord != m_bEyePositionIsTransformedByPortal )
	{
		BaseEntityRecordingState_t dummyState;
		BaseEntityRecordingState_t *pState = (BaseEntityRecordingState_t *)msg->GetPtr( "baseentity", &dummyState );
		pState->m_nEffects |= EF_NOINTERP; //If we interpolate, we'll be traversing an arbitrary line through the level at an undefined speed. That would be bad
	}

	m_bToolMode_EyeHasPortalled_LastRecord = m_bEyePositionIsTransformedByPortal;

	//record if the eye is on the opposite side of the portal from the body
	{
		CameraRecordingState_t dummyState;
		CameraRecordingState_t *pState = (CameraRecordingState_t *)msg->GetPtr( "camera", &dummyState );
		pState->m_bPlayerEyeIsPortalled = m_bEyePositionIsTransformedByPortal;
	}
}

extern float g_fMaxViewModelLag;
void C_Portal_Player::CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles)
{
	// HACK: Manually adjusting the eye position that view model looking up and down are similar
	// (solves view model "pop" on floor to floor transitions)
	Vector vInterpEyeOrigin = eyeOrigin;

	Vector vForward;
	Vector vRight;
	Vector vUp;
	AngleVectors( eyeAngles, &vForward, &vRight, &vUp );

	if ( vForward.z < 0.0f )
	{
		float fT = vForward.z * vForward.z;
		vInterpEyeOrigin += vRight * ( fT * 4.7f ) + vForward * ( fT * 5.0f ) + vUp * ( fT * 4.0f );
	}
	
	if ( UTIL_EntityIsIntersectingPortalWithLinkedAlsoBeingFloorOrCeiling( this ) )
		g_fMaxViewModelLag = 0.0f;
	else
		g_fMaxViewModelLag = 1.5f;

	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;

		vm->CalcViewModelView( this, vInterpEyeOrigin, eyeAngles );
	}
}

bool LocalPlayerIsCloseToPortal( void )
{
	return C_Portal_Player::GetLocalPlayer()->IsCloseToPortal();
}


//-----------------------------------------------------------------------------
// Purpose: Called to disable and enable sprint due to temporary circumstances:
//			- Carrying a heavy object with the physcannon
//-----------------------------------------------------------------------------
void C_Portal_Player::EnableSprint( bool bEnable )
{
	if ( !bEnable && IsSprinting() )
	{
		StopSprinting();
	}

	m_bSprintEnabled = bEnable;
}