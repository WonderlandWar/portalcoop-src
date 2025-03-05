//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_portalgun_shared.h"
#include "portal_gamerules.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "rumble_shared.h"
#include "particle_parse.h"
#include "collisionutils.h"
#include "portal_placement.h"
#include "prop_portal_shared.h"
#include "debugoverlay_shared.h"
#ifdef GAME_DLL
	#include "physicsshadowclone.h"
	#include "te_effect_dispatch.h"
	#include "trigger_portal_cleanser.h"
	#include "info_placement_helper.h"
#else
#define CInfoPlacementHelper C_InfoPlacementHelper
	#include "c_info_placement_helper.h"
	#include "c_te_effect_dispatch.h"
	#include "c_triggers.h"
	#include "c_trigger_portal_cleanser.h"
	#include "prediction.h"
	#include "view.h"
#endif

ConVar use_server_portal_particles( "use_server_portal_particles", "0", FCVAR_REPLICATED );
ConVar use_server_portal_crosshair_test("use_server_portal_crosshair_test", "0", FCVAR_REPLICATED, "Changes if the crosshair placement indicator should be predicted or use the server");
extern ConVar sv_allow_customized_portal_colors;

#ifdef CLIENT_DLL
	#define CWeaponPortalgun C_WeaponPortalgun
	#define CTriggerPortalCleanser C_TriggerPortalCleanser
#endif //#ifdef CLIENT_DLL


acttable_t	CWeaponPortalgun::m_acttable[] = 
{
	{ ACT_MP_STAND_IDLE,				ACT_MP_STAND_PRIMARY,					false },
	{ ACT_MP_RUN,						ACT_MP_RUN_PRIMARY,						false },
	{ ACT_MP_CROUCH_IDLE,				ACT_MP_CROUCH_PRIMARY,					false },
	{ ACT_MP_CROUCHWALK,				ACT_MP_CROUCHWALK_PRIMARY,				false },
	{ ACT_MP_JUMP_START,				ACT_MP_JUMP_START_PRIMARY,				false },
	{ ACT_MP_JUMP_FLOAT,				ACT_MP_JUMP_FLOAT_PRIMARY,				false },
	{ ACT_MP_JUMP_LAND,					ACT_MP_JUMP_LAND_PRIMARY,				false },
	{ ACT_MP_AIRWALK,					ACT_MP_AIRWALK_PRIMARY,					false },
};

IMPLEMENT_ACTTABLE(CWeaponPortalgun);


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponPortalgun::CWeaponPortalgun( void )
{
	m_bReloadsSingly = true;

	// TODO: specify these in hammer instead of assuming every gun has blue chip
	m_bCanFirePortal1 = true;
	m_bCanFirePortal2 = false;

	m_iLastFiredPortal = 0;
	m_fCanPlacePortal1OnThisSurface = 1.0f;
	m_fCanPlacePortal2OnThisSurface = 1.0f;
#ifdef GAME_DLL
	m_fCanPlacePortal1OnThisSurfaceNetworked = m_fCanPlacePortal1OnThisSurface;
	m_fCanPlacePortal2OnThisSurfaceNetworked = m_fCanPlacePortal2OnThisSurface;
	
	m_iValidPlayer = 0;

	//m_iValidPlayer = 0;

#endif

#ifdef CLIENT_DLL
	m_iOldPortalLinkageGroupID = 255; // This has to be done so that m_iOldPortalLinkageGroupID != m_iPortalLinkageGroupID for OnDataChanged
	m_iOldPortalColorSet = 255; // This has to be done so that m_iOldPortalColorSet != m_iPortalColorSet for OnDataChanged
#endif

	m_fMinRange1	= 0.0f;
	m_fMaxRange1	= MAX_TRACE_LENGTH;
	m_fMinRange2	= 0.0f;
	m_fMaxRange2	= MAX_TRACE_LENGTH;

	m_EffectState	= (int)EFFECT_NONE;
}

void CWeaponPortalgun::Precache()
{
	BaseClass::Precache();

	PrecacheModel( PORTALGUN_BEAM_SPRITE );
	PrecacheModel( PORTALGUN_BEAM_SPRITE_NOZ );

	PrecacheModel( "models/portals/portal1.mdl" );
	PrecacheModel( "models/portals/portal2.mdl" );

	PrecacheScriptSound( "Portal.ambient_loop" );

	PrecacheScriptSound( "Portal.open_blue" );
	PrecacheScriptSound( "Portal.open_red" );
	PrecacheScriptSound( "Portal.close_blue" );
	PrecacheScriptSound( "Portal.close_red" );
	PrecacheScriptSound( "Portal.fizzle_moved" );
	PrecacheScriptSound( "Portal.fizzle_invalid_surface" );
	PrecacheScriptSound( "Weapon_Portalgun.powerup" );
	PrecacheScriptSound( "Weapon_PhysCannon.HoldSound" );

	PrecacheParticleSystem( "portal_1_projectile_stream" );
	PrecacheParticleSystem( "portal_1_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_2_projectile_stream" );
	PrecacheParticleSystem( "portal_2_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_red_projectile_stream" );
	PrecacheParticleSystem( "portal_red_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_yellow_projectile_stream" );
	PrecacheParticleSystem( "portal_yellow_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_purple_projectile_stream" );
	PrecacheParticleSystem( "portal_purple_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_lightblue_projectile_stream" );
	PrecacheParticleSystem( "portal_lightblue_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_pink_projectile_stream" );
	PrecacheParticleSystem( "portal_pink_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_green_projectile_stream" );
	PrecacheParticleSystem( "portal_green_projectile_stream_pedestal" );
#ifndef CLIENT_DLL
	PrecacheParticleSystem( "portal_1_charge" );
	PrecacheParticleSystem( "portal_2_charge" );
	
	PrecacheParticleSystem( "portal_red_charge" );
	PrecacheParticleSystem( "portal_yellow_charge" );
	PrecacheParticleSystem( "portal_purple_charge" );
	PrecacheParticleSystem( "portal_lightblue_charge" );
	PrecacheParticleSystem( "portal_pink_charge" );
	PrecacheParticleSystem( "portal_green_charge" );
#endif
}

PRECACHE_WEAPON_REGISTER(weapon_portalgun);

bool CWeaponPortalgun::ShouldDrawCrosshair( void )
{
	return true;//( m_fCanPlacePortal1OnThisSurface > 0.5f || m_fCanPlacePortal2OnThisSurface > 0.5f );
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::Reload( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponPortalgun::FillClip( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	// Add them to the clip
	if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		if ( Clip1() < GetMaxClip1() )
		{
			m_iClip1++;
			pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponPortalgun::DryFire( void )
{
	WeaponSound(EMPTY);
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

void CWeaponPortalgun::SetCanFirePortal1( bool bCanFire /*= true*/ )
{
	m_bCanFirePortal1 = bCanFire;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	if ( !m_bOpenProngs )
	{
		DoEffect( EFFECT_HOLDING );
		DoEffect( EFFECT_READY );
	}

	// TODO: Remove muzzle flash when there's an upgrade animation
	pOwner->DoMuzzleFlash();

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.25f;

	// player "shoot" animation
	pOwner->SetAnimation( PLAYER_ATTACK1 );
	pOwner->ViewPunch( QAngle( random->RandomFloat( -1, -0.5f ), random->RandomFloat( -1, 1 ), 0 ) );
	EmitSound( "Weapon_Portalgun.powerup" );
}

void CWeaponPortalgun::SetCanFirePortal2( bool bCanFire /*= true*/ )
{
	m_bCanFirePortal2 = bCanFire;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	if ( !m_bOpenProngs )
	{
		DoEffect( EFFECT_HOLDING );
		DoEffect( EFFECT_READY );
	}

	// TODO: Remove muzzle flash when there's an upgrade animation
	pOwner->DoMuzzleFlash();

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;

	// player "shoot" animation
	pOwner->SetAnimation( PLAYER_ATTACK1 );

	pOwner->ViewPunch( QAngle( random->RandomFloat( -1, -0.5f ), random->RandomFloat( -1, 1 ), 0 ) );

	EmitSound( "Weapon_Portalgun.powerup" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponPortalgun::PrimaryAttack( void )
{
	if (!m_bCanAttack)
		return;
	if (m_bHolstered)
		return;
	if ( !CanFirePortal1() && !CanFirePortal2() )
		return;

	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	if (CanFirePortal1())
	{
		FirePortal1();
	}
	else if (CanFirePortal2())
	{
		FirePortal2();
	}
	
#ifndef CLIENT_DLL
	m_OnFiredPortal1.FireOutput( pPlayer, this );
	pPlayer->RumbleEffect( RUMBLE_PORTALGUN_LEFT, 0, RUMBLE_FLAGS_NONE );
#endif

	pPlayer->DoMuzzleFlash();

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;//SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;//SequenceDuration();
	
	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
	float punchPitch = SharedRandomFloat( "portalgun_punchpitch", -1, -0.5f );
	float punchYaw = SharedRandomFloat( "portalgun_punchyaw", -1, 1 );

	pPlayer->ViewPunch( QAngle( punchPitch, punchYaw, 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponPortalgun::SecondaryAttack( void )
{
	if (!m_bCanAttack)
		return;
	if (m_bHolstered)
		return;
	if ( !CanFirePortal2() || !CanFirePortal1() )
		return;

	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

		FirePortal2();

#ifndef CLIENT_DLL
	m_OnFiredPortal2.FireOutput( pPlayer, this );
	pPlayer->RumbleEffect( RUMBLE_PORTALGUN_RIGHT, 0, RUMBLE_FLAGS_NONE );
#endif

	pPlayer->DoMuzzleFlash();

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;//SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;//SequenceDuration();

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
	float punchPitch = SharedRandomFloat( "portalgun_punchpitch", -1, -0.5f );
	float punchYaw = SharedRandomFloat( "portalgun_punchyaw", -1, 1 );

	pPlayer->ViewPunch( QAngle( punchPitch, punchYaw, 0 ) );
}

void CWeaponPortalgun::DelayAttack( float fDelay )
{
	m_flNextPrimaryAttack = gpGlobals->curtime + fDelay;
	m_flNextSecondaryAttack = gpGlobals->curtime + fDelay;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::ItemHolsterFrame( void )
{
	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than three seconds, reload
	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() )
	{
		// Reset the timer
		m_flHolsterTime = gpGlobals->curtime;
	
		if ( GetOwner() == NULL )
			return;

		if ( m_iClip1 == GetMaxClip1() )
			return;

		// Just load the clip with no animations
		int ammoFill = MIN( (GetMaxClip1() - m_iClip1), GetOwner()->GetAmmoCount( GetPrimaryAmmoType() ) );
		
		GetOwner()->RemoveAmmo( ammoFill, GetPrimaryAmmoType() );
		m_iClip1 += ammoFill;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	DestroyEffects();

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::Deploy( void )
{
	DoEffect( EFFECT_READY );

	bool bReturn = BaseClass::Deploy();

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner )
	{
		pOwner->SetNextAttack( gpGlobals->curtime );

#ifndef CLIENT_DLL
		if (!m_bForceAlwaysUseSetID)
		{
			if( GameRules()->IsMultiplayer() || !GetOwner()->IsPlayer())
			{
				m_iPortalLinkageGroupID = pOwner->entindex();
			
				Assert( (m_iPortalLinkageGroupID >= 0) && (m_iPortalLinkageGroupID < 256) );
			}
		}
		
		CPortal_Player *pPlayer = ToPortalPlayer( pOwner );

		if (pPlayer)
			m_iCustomPortalColorSet = pPlayer->m_iCustomPortalColorSet;

		m_hPrimaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, false, true );
		m_hSecondaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, true, true );
#endif
	}

	return bReturn;
}

void CWeaponPortalgun::WeaponIdle( void )
{
	//See if we should idle high or low
	if ( WeaponShouldBeLowered() )
	{
		// Move to lowered position if we're not there yet
		if ( GetActivity() != ACT_VM_IDLE_LOWERED && GetActivity() != ACT_VM_IDLE_TO_LOWERED 
			&& GetActivity() != ACT_TRANSITION )
		{
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			// Keep idling low
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
	}
	else
	{
		// See if we need to raise immediately
		if ( m_flRaiseTime < gpGlobals->curtime && GetActivity() == ACT_VM_IDLE_LOWERED ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
		else if ( HasWeaponIdleTimeElapsed() ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::StopEffects( bool stopSound )
{
	// Turn off our effect state
	DoEffect( EFFECT_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : effectType - 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::DoEffect( int effectType, Vector *pos )
{
	m_EffectState = effectType;

#ifdef CLIENT_DLL
	// Save predicted state
	m_nOldEffectState = m_EffectState;
#endif

	switch( effectType )
	{
	case EFFECT_READY:
		DoEffectReady();
		break;

	case EFFECT_HOLDING:
		DoEffectHolding();
		break;

	default:
	case EFFECT_NONE:
		DoEffectNone();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Restore
//-----------------------------------------------------------------------------
void CWeaponPortalgun::OnRestore()
{
	BaseClass::OnRestore();

	// Portalgun effects disappear through level transition, so
	//  just recreate any effects here
	if ( m_EffectState != EFFECT_NONE )
	{
		DoEffect( m_EffectState, NULL );
	}
}


//-----------------------------------------------------------------------------
// On Remove
//-----------------------------------------------------------------------------
void CWeaponPortalgun::UpdateOnRemove(void)
{
#ifdef GAME_DLL
	FizzleOwnedPortals();
#endif

	DestroyEffects();
	BaseClass::UpdateOnRemove();
}

void CWeaponPortalgun::DoEffectBlast(CBaseEntity *pOwner, bool bPortal2, int iPlacedBy, const Vector &ptStart, const Vector &ptFinalPos, const QAngle &qStartAngles, float fDelay, int iPortalLinkageGroup)
{
#ifdef CLIENT_DLL
	if ( !prediction->IsFirstTimePredicted() )
		return;
#endif
	if (use_server_portal_particles.GetBool())
	{
		IPredictionSystem::SuppressHostEvents( this );

#ifdef CLIENT_DLL
		return;
#endif
	}
	
	CEffectData	fxData;
	fxData.m_vOrigin = ptStart;
	fxData.m_vStart = ptFinalPos;
	fxData.m_flScale = gpGlobals->curtime + fDelay;
	fxData.m_vAngles = qStartAngles;
	fxData.m_nColor = ( ( bPortal2 ) ? ( 2 ) : ( 1 ) );
	fxData.m_nDamageType = iPlacedBy;
	fxData.m_nHitBox = iPortalLinkageGroup; //Use m_nHitBox as a dummy var
#ifdef CLIENT_DLL
	AssertMsg( GetOwner() == C_BasePlayer::GetLocalPlayer(), "This should only run in prediction, and the owner should be the local player!" );
	fxData.m_hEntity = GetOwner();
#else
	fxData.m_nEntIndex = pOwner ? pOwner->entindex() : entindex();
#endif
	
#ifdef CLIENT_DLL
	extern void PortalBlastCallback( const CEffectData & data );
	PortalBlastCallback( fxData );
#else
	DispatchEffect( "PortalBlast", fxData );
#endif
}

#ifdef CLIENT_DLL
#define CBaseTrigger C_BaseTrigger
#endif

float CWeaponPortalgun::FirePortal( bool bPortal2, Vector *pVector /*= 0*/, bool bTest /*= false*/ )
{
	Vector vEye;
	Vector vDirection;
	Vector vTracerOrigin;
	
	CBaseEntity *pOwner = GetOwner();
	CPortal_Player *pPlayer = ToPortalPlayer(pOwner);
#ifdef CLIENT_DLL
	bool bIsFirstTimePredicted = prediction->IsFirstTimePredicted();
#endif

	if( pPlayer )
	{
#ifdef CLIENT_DLL
		if ( bIsFirstTimePredicted )
#endif
		if ( !bTest )
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );
		}

		Vector forward, right, up;
		

		AngleVectors( pPlayer->EyeAngles(), &forward, &right, &up );


		pPlayer->EyeVectors( &vDirection, NULL, NULL );
#ifdef CLIENT_DLL
		if ( !bTest )
		{
			if ( bIsFirstTimePredicted )
			{
				m_vFirstPredictedShotPos = pPlayer->EyePosition();
			}

			vEye = m_vFirstPredictedShotPos;
		}
		else
#endif
		{
			vEye = pPlayer->EyePosition();
		}

		// Check if the players eye is behind the portal they're in and translate it
		VMatrix matThisToLinked;
		CProp_Portal *pPlayerPortal = pPlayer->m_hPortalEnvironment;

		if ( pPlayerPortal )
		{
			Vector vPortalForward;
			pPlayerPortal->GetVectors( &vPortalForward, NULL, NULL );

			Vector vEyeToPortalCenter = pPlayerPortal->m_ptOrigin - vEye;

			float fPortalDist = vPortalForward.Dot( vEyeToPortalCenter );
			if( fPortalDist > 0.0f )
			{
				// Eye is behind the portal
				matThisToLinked = pPlayerPortal->MatrixThisToLinked();
			}
			else
			{
				pPlayerPortal = NULL;
			}
		}

		if ( pPlayerPortal )
		{
			UTIL_Portal_VectorTransform( matThisToLinked, forward, forward );
			UTIL_Portal_VectorTransform( matThisToLinked, right, right );
			UTIL_Portal_VectorTransform( matThisToLinked, up, up );
			UTIL_Portal_VectorTransform( matThisToLinked, vDirection, vDirection );
			UTIL_Portal_PointTransform( matThisToLinked, vEye, vEye );

			if ( pVector )
			{
				UTIL_Portal_VectorTransform( matThisToLinked, *pVector, *pVector );
			}
		}

		vTracerOrigin = vEye
			+ forward * 30.0f
			+ right * 4.0f
			+ up * (-5.0f);
		
	}
	else
	{
		// This portalgun is not held by the player-- Fire using the muzzle attachment
		Vector vecShootOrigin;
		QAngle angShootDir;
		GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
		vEye = vecShootOrigin;
		vTracerOrigin = vecShootOrigin;
		AngleVectors( angShootDir, &vDirection, NULL, NULL );
	}

	if ( !bTest )
	{
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}

	if ( pVector )
	{
		vDirection = *pVector;
	}

	CProp_Portal *pPortal  = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();
	Assert( pPortal );
	if (!pPortal)
		return PORTAL_ANALOG_SUCCESS_INVALID_VOLUME;
	if (pPortal)
	{
		pPortal->SetFiredByPlayer(pPlayer);
		pPortal->m_nPlacementAttemptParity = (pPortal->m_nPlacementAttemptParity + 1) & EF_PARITY_MASK; //no matter what, prod the network state so we can detect prediction errors
	}

	Vector vTraceStart = vEye + (vDirection * m_fMinRange1);

	Vector vFinalPosition;
	QAngle qFinalAngles;

	PortalPlacedByType ePlacedBy = ( pPlayer ) ? ( PORTAL_PLACED_BY_PLAYER ) : ( PORTAL_PLACED_BY_PEDESTAL );

	CInfoPlacementHelper *pPlacementHelper = NULL;

	trace_t tr;
	float fPlacementSuccess = TraceFirePortal( bPortal2, vTraceStart, vDirection, tr, vFinalPosition, qFinalAngles, ePlacedBy, &pPlacementHelper, bTest );
		
	if ( sv_portal_placement_never_fail.GetBool() )
	{
		fPlacementSuccess = 1.0f;
	}
	
	Ray_t ray;

	ray.Init(vTraceStart, tr.endpos);

	CProp_Portal *pHitPortal = NULL;
	
	float fraction = 2.0f;
	pHitPortal = UTIL_Portal_FirstAlongRayAll( ray, fraction );
		
	if ( bTest && !pHitPortal && tr.DidHit() && &tr.surface != NULL && tr.surface.name != NULL && !tr.surface.name != NULL ) // We need to do this because a normal trace isn't going to work.
	{
		if ( !IsNoPortalMaterial(tr.surface))
		{
			QAngle qNormal;
			VectorAngles( tr.plane.normal, qNormal );
			pHitPortal = GetTheoreticalOverlappedPartnerPortal( m_iPortalLinkageGroupID, tr.endpos, qNormal );
		}
	}
	

	// Test for portal steal in coop after we've collided against likely targets
	//(const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, bool bTest = false);
	if ( ShouldStealCoopPortal(pHitPortal, fPlacementSuccess))
	{
		fPlacementSuccess = PORTAL_ANALOG_SUCCESS_STEAL;
	}
	
	if ( !bTest )
	{
		if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_STEAL)
		{
			vFinalPosition = pHitPortal->GetAbsOrigin();
			qFinalAngles = pHitPortal->GetAbsAngles();
			pPortal->m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;
		}
		
		// If it was a failure, put the effect at exactly where the player shot instead of where the portal bumped to
		if ( fPlacementSuccess < 0.5f ) //0.5f or greater is a success
			vFinalPosition = tr.endpos;
		
		QAngle qFireAngles;
		VectorAngles( vDirection, qFireAngles );
		
		float fDelay;
		bool bInstantPlacement = pPlayer != NULL;  // Do delayed placement for pedestal portalguns, but not the player
		if ( !bInstantPlacement )
		{
			fDelay = vTracerOrigin.DistTo( tr.endpos ) / ( ( pPlayer ) ? ( BLAST_SPEED ) : ( BLAST_SPEED_NON_PLAYER ) );
		}
		else
		{
			fDelay = 0.1;
		}

		DoEffectBlast(pOwner, pPortal->m_bIsPortal2, ePlacedBy, vTracerOrigin, vFinalPosition, qFireAngles, fDelay, m_iPortalColorSet );
		if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_NEAR && pHitPortal)
		{
			if (bPortal2)
				pHitPortal->DoFizzleEffect( PORTAL_FIZZLE_NEAR_RED, m_iPortalColorSet, false );
			else
				pHitPortal->DoFizzleEffect( PORTAL_FIZZLE_NEAR_BLUE, m_iPortalColorSet, false );
		}

		else
		{
			pPortal->PlacePortal(vFinalPosition, qFinalAngles, fPlacementSuccess, true);

			pPortal->m_vDelayedPosition = vFinalPosition;

			pPortal->m_hPlacedBy = this;

#ifdef GAME_DLL
			if ( !bInstantPlacement )
			{
				pPortal->SetContextThink( &CProp_Portal::DelayedPlacementThink, gpGlobals->curtime + fDelay, s_pDelayedPlacementContext ); 
			}
			else
#endif
			{
				pPortal->DelayedPlacementThink();
			}
		}
	}

	return fPlacementSuccess;
}

extern int AllEdictsAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask );

float CWeaponPortalgun::TraceFirePortal( bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy, CInfoPlacementHelper **pPlacementHelper, bool bTest /*= false*/ )
{
	
#ifdef GAME_DLL
	CTraceFilterSimpleClassnameList baseFilter( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );
#else
	CTraceFilterSimpleClassnameList traceFilterPortalShot( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &traceFilterPortalShot );
#endif

	Ray_t rayEyeArea;
	rayEyeArea.Init( vTraceStart + vDirection * 24.0f, vTraceStart + vDirection * -24.0f );

	float fMustBeCloserThan = 2.0f;
	
	CProp_Portal *pNearPortal = UTIL_Portal_FirstAlongRay( rayEyeArea, fMustBeCloserThan );

	if ( !pNearPortal )
	{
		// Check for portal near and infront of you
		rayEyeArea.Init( vTraceStart + vDirection * -24.0f, vTraceStart + vDirection * 48.0f );

		fMustBeCloserThan = 2.0f;

		pNearPortal = UTIL_Portal_FirstAlongRay( rayEyeArea, fMustBeCloserThan );
	}

	if ( pNearPortal && pNearPortal->IsActivedAndLinked() )
	{
		iPlacedBy = PORTAL_PLACED_BY_PEDESTAL;

		Vector vPortalForward;
		pNearPortal->GetVectors( &vPortalForward, 0, 0 );

		if ( vDirection.Dot( vPortalForward ) < 0.01f )
		{
			// If shooting out of the world, fizzle
			if ( !bTest )
			{
				CProp_Portal *pPortal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();

				pPortal->m_iDelayedFailure = ( ( pNearPortal->m_bIsPortal2 ) ? ( PORTAL_FIZZLE_NEAR_RED ) : ( PORTAL_FIZZLE_NEAR_BLUE ) );
				VectorAngles( vPortalForward, pPortal->m_qDelayedAngles );
				pPortal->m_vDelayedPosition = pNearPortal->GetAbsOrigin();

				vFinalPosition = pPortal->m_vDelayedPosition;
				qFinalAngles = pPortal->m_qDelayedAngles;

			}
			UTIL_TraceLine( vTraceStart - vDirection * 16.0f, vTraceStart + (vDirection * m_fMaxRange1), MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );
			return PORTAL_ANALOG_SUCCESS_NEAR;
			
			//UTIL_TraceLine( vTraceStart - vDirection * 16.0f, vTraceStart + (vDirection * m_fMaxRange1), MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );
			//return PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED;
		}
	}

	// Trace to see where the portal hit

	UTIL_TraceLine( vTraceStart, vTraceStart + (vDirection * m_fMaxRange1), MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

	if ( !tr.DidHit() || tr.startsolid )
	{
		// If it didn't hit anything, fizzle
		if ( !bTest )
		{
			CProp_Portal *pPortal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();

			pPortal->m_iDelayedFailure = PORTAL_FIZZLE_NONE;
			VectorAngles( -vDirection, pPortal->m_qDelayedAngles );
			pPortal->m_vDelayedPosition = tr.endpos;

			vFinalPosition = pPortal->m_vDelayedPosition;
			qFinalAngles = pPortal->m_qDelayedAngles;
		}
		return PORTAL_ANALOG_SUCCESS_PASSTHROUGH_SURFACE;
	}
	
	// Clip this to any number of entities that can block us
	//(bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy)
	if (PortalTraceClippedByBlockers(bPortal2, vTraceStart, vDirection, tr, vFinalPosition, qFinalAngles, iPlacedBy, bTest) )
		return PORTAL_ANALOG_SUCCESS_CLEANSER;
	

	Vector vUp( 0.0f, 0.0f, 1.0f );
	if( ( tr.plane.normal.x > -0.001f && tr.plane.normal.x < 0.001f ) && ( tr.plane.normal.y > -0.001f && tr.plane.normal.y < 0.001f ) )
	{
		//plane is a level floor/ceiling
		vUp = vDirection;
	}

	// Check that the placement succeed
	VectorAngles( tr.plane.normal, vUp, qFinalAngles );
	
	// Hit any placement helpers at this point
	*pPlacementHelper = AttemptSnapToPlacementHelper( bPortal2, vTraceStart, vDirection, tr, vFinalPosition, qFinalAngles, iPlacedBy, bTest );
	if ( *pPlacementHelper )
		return PORTAL_ANALOG_SUCCESS_NO_BUMP;

	vFinalPosition = tr.endpos;

	return VerifyPortalPlacementAndFizzleBlockingPortals(bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get() , vFinalPosition, qFinalAngles, iPlacedBy, bTest);
}

//-----------------------------------------------------------------------------
// Purpose: Try to fit a portal using placement helpers
//-----------------------------------------------------------------------------
CInfoPlacementHelper *CWeaponPortalgun::AttemptSnapToPlacementHelper( bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy, bool bTest )
{
	CBaseEntity *pOwner = GetOwner();
	// First, find a helper in the general area we hit
	CInfoPlacementHelper *pHelper = UTIL_FindPlacementHelper( vFinalPosition, (pOwner && pOwner->IsPlayer()) ? (CBasePlayer *)pOwner : NULL );
	if ( pHelper == NULL )
		return NULL;

#if defined( GAME_DLL )
	if ( sv_portal_placement_debug.GetBool() )
	{
		Msg("PortalPlacement: Found placement helper centered at %f, %f, %f. Radius %f\n", XYZ(pHelper->GetAbsOrigin()), pHelper->GetTargetRadius() );
	}
#endif

	Assert( !tr.plane.normal.IsZero() );

	// Setup a trace filter that ignore / hits everything we care about
#if defined( GAME_DLL )
	CTraceFilterSimpleClassnameList baseFilter( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );
#else
	CTraceFilterSimpleClassnameList traceFilterPortalShot( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &traceFilterPortalShot );
#endif

	// re-hit the area near the center of the placement helper. Very small trace is fine
	Vector vecStartPos = tr.plane.normal + pHelper->GetAbsOrigin();
	Vector vecDir = -tr.plane.normal;
	VectorNormalize( vecDir );
	trace_t trHelper;
	UTIL_TraceLine( vecStartPos, vecStartPos + vecDir*m_fMaxRange1, MASK_SHOT_PORTAL, &traceFilterPortalShot, &trHelper );
	Assert ( trHelper.DidHit() );

	// Use the helper angles, if specified
	QAngle qHelperAngles = ( pHelper->ShouldUseHelperAngles() ) ? ( pHelper->GetTargetAngles() ) : qFinalAngles;

	if ( sv_portal_placement_debug.GetBool() )
	{
		Msg("PortalPlacement: Using placement helper angles %f %f %f\n", XYZ(pHelper->GetTargetAngles()));
	}

	Vector vHelperFinalPos = trHelper.endpos;

	bool bPlacementOnHelperValid = true;

	// make sure the normals match
	if ( VectorsAreEqual( trHelper.plane.normal, tr.plane.normal, FLT_EPSILON ) == false )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			Msg("PortalPlacement: Not using placement helper because the surface normal of the portal's resting surface and the placement helper's intended surface do not match\n" );
		}
		bPlacementOnHelperValid = false;
	}

	//make sure distance is a sane amount
	Vector vecHelperToHitPoint = tr.endpos - trHelper.endpos;
	float flLenSq = vecHelperToHitPoint.LengthSqr();
	if ( flLenSq > (pHelper->GetTargetRadius()*pHelper->GetTargetRadius()) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			Msg("PortalPlacement:Not using placement helper because the Portal's final position was outside the helper's radius!\n" );
		}
		bPlacementOnHelperValid = false;
	}

	if( bPlacementOnHelperValid )
	{
		// Find the portal we're moving
		CProp_Portal *pPortal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get(); //CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bPortal2 );
		float fPlacementSuccess = VerifyPortalPlacement( pPortal, vHelperFinalPos, qHelperAngles, iPlacedBy, bTest );
		
		// run normal placement validity checks
		if ( fPlacementSuccess < 0.5f )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				Msg("PortalPlacement: Not using placement helper because portal could not fit in a valid spot at it's origin and angles\n" );
			}

			bPlacementOnHelperValid = false;
		}
	}

	if ( bPlacementOnHelperValid )
	{
		vFinalPosition = vHelperFinalPos;
		qFinalAngles = qHelperAngles;

		return pHelper;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Clips a complex trace segment against a variety of entities that
//			we'd like to collide with.
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::PortalTraceClippedByBlockers(bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy, bool bTest )
{
	// Deal with this segment of the trace
//	tr = pTraceResults[nNumResultSegments - 1].trSegment;

	//tr.resu

//	for ( int iTraceSegment = 0; iTraceSegment != nNumResultSegments; ++iTraceSegment ) //loop over all segments
	{
//		ComplexPortalTrace_t &currentSegment = pTraceResults[iTraceSegment];

		// Trace to the surface to see if there's a rotating door in the way
		CBaseEntity *list[1024];
		CUtlVector<CTriggerPortalCleanser*> vFizzlersAlongRay;

		Ray_t ray;
		ray.Init( tr.startpos, tr.endpos );

		int nCount = AllEdictsAlongRay( list, 1024, ray, 0 );

		// Loop through all entities along the ray between the gun and the surface
		for ( int i = 0; i < nCount; i++ )
		{
#if 0
#if defined( GAME_DLL )
			Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(server) : %s\n", list[i]->m_iClassname );
#else
			Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(client) : %s\n", list[i]->GetClassname() );
#endif
#endif
			if( dynamic_cast<CTriggerPortalCleanser*>( list[i] ) != NULL )
			{
				CTriggerPortalCleanser *pTrigger = static_cast<CTriggerPortalCleanser*>( list[i] );

				if ( pTrigger && !pTrigger->m_bDisabled )

				{
#if 0
#if defined( GAME_DLL )
					Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(server) : CLEANSER!!!!!\n" );
#else
					Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(client) : CLEANSER!!!!!\n" );
#endif
#endif
					vFizzlersAlongRay.AddToTail( pTrigger );
				}
			}
		}

		CTriggerPortalCleanser *pHitFizzler = NULL;
		trace_t nearestFizzlerTrace;
		float flNearestFizzler = FLT_MAX;
		bool bFizzlerHit = false;

		for ( int n = 0; n < vFizzlersAlongRay.Count(); ++n )
		{
			Vector vMin;
			Vector vMax;
			vFizzlersAlongRay[n]->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );

			trace_t fizzlerTrace;
			IntersectRayWithBox( tr.startpos, tr.endpos - tr.startpos, vMin, vMax, 0.0f, &fizzlerTrace );

			float flDist = ( fizzlerTrace.endpos - fizzlerTrace.startpos ).LengthSqr();

			if ( flDist < flNearestFizzler )
			{
				flNearestFizzler = flDist;
				pHitFizzler = vFizzlersAlongRay[n];
				nearestFizzlerTrace = fizzlerTrace;
				bFizzlerHit = true;
			}
		}

		if ( bFizzlerHit )
		{
			tr = nearestFizzlerTrace;

			tr.plane.normal = -vDirection;

#if defined( GAME_DLL )
//			pHitFizzler->SetPortalShot();
#endif

			CProp_Portal *pPortal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();//CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bIsSecondPortal, true );
			if( pPortal && !bTest )
			{
				pPortal->m_iDelayedFailure = PORTAL_FIZZLE_CLEANSER;
				VectorAngles( tr.plane.normal, pPortal->m_qDelayedAngles );
				pPortal->m_vDelayedPosition = tr.endpos;

				vFinalPosition	= pPortal->m_vDelayedPosition;
				qFinalAngles	= pPortal->m_qDelayedAngles;
				//placementInfo.ePlacementResult	= PORTAL_PLACEMENT_CLEANSER;
			}
			else
			{
				vFinalPosition = tr.endpos;
				VectorAngles( tr.plane.normal, qFinalAngles );
				//placementInfo.ePlacementResult = PORTAL_PLACEMENT_CLEANSER;
			}
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: In a coop game we'd like to steal our partner's portals if we try 
//			to place right on top of their's
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::ShouldStealCoopPortal(CProp_Portal *pHitPortal, float fPlacementSuccess)
{
	if (!pHitPortal)
		return false;
	// Don't steal if we're near a Portal
	if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_NEAR)
		return false;

	if (pHitPortal->GetLinkageGroup() != m_iPortalLinkageGroupID)
		return true;

	return false;
}

void CWeaponPortalgun::FirePortal1( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	CPortal_Player *pPlayer = ToPortalPlayer( pOwner );


#ifdef GAME_DLL
	if( m_hPrimaryPortal.Get() == NULL )
	{
		m_hPrimaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, false, true );
	}
#else
	/*
	if( m_hPrimaryPortal )
	{
		if( !m_hPrimaryPortal->GetPredictable() || !m_hSecondaryPortal.Get() || !m_hSecondaryPortal->GetPredictable() )
			return;

		if( m_hPrimaryPortal->m_hLinkedPortal.Get() == NULL )
		{
			CProp_Portal *pPortal = m_hPrimaryPortal.Get();
			CProp_Portal *pOtherPortal = m_hSecondaryPortal.Get();
			if( pOtherPortal && pOtherPortal->IsActive() )
			{
				pPortal->m_hLinkedPortal = pOtherPortal;
				pOtherPortal->m_hLinkedPortal = pPortal;
			}
		}
		else
		{
			if( !m_hPrimaryPortal->m_hLinkedPortal->GetPredictable() )
				return;
		}
	}
	*/
#endif
	
	FirePortal( false );
	m_iLastFiredPortal = 1;
	
	if( pPlayer )
	{
		WeaponSound( SINGLE );
	}
	else
	{
		WeaponSound( SINGLE_NPC );
	}
}

void CWeaponPortalgun::FirePortal2( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	CPortal_Player *pPlayer = ToPortalPlayer( pOwner );

#if defined( GAME_DLL )
	if( m_hSecondaryPortal.Get() == NULL )
	{
			m_hSecondaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, true, true );
	}
#else
	/*
	if( m_hSecondaryPortal )
	{
		if( !m_hSecondaryPortal->GetPredictable() || !m_hPrimaryPortal.Get() || !m_hPrimaryPortal->GetPredictable() )
			return;

		if( m_hSecondaryPortal->m_hLinkedPortal.Get() == NULL )
		{
			CProp_Portal *pPortal = m_hSecondaryPortal.Get();
			CProp_Portal *pOtherPortal = m_hPrimaryPortal.Get();
			if( pOtherPortal && pOtherPortal->IsActive() )
			{
				pPortal->m_hLinkedPortal = pOtherPortal;
				pOtherPortal->m_hLinkedPortal = pPortal;
			}
		}
		else
		{
			if( !m_hSecondaryPortal->m_hLinkedPortal->GetPredictable() )
				return;
		}
	}
	*/
#endif

	FirePortal( true );
	m_iLastFiredPortal = 2;
	
	if (pPlayer)
	{
		WeaponSound( WPN_DOUBLE );
	}
	else
	{
		WeaponSound( DOUBLE_NPC );
	}
}

CProp_Portal *CWeaponPortalgun::GetAssociatedPortal( bool bPortal2 )
{
	CProp_Portal *pRetVal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();

#if defined( GAME_DLL )
	if( pRetVal == NULL )
	{
		pRetVal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bPortal2, true );

		if( pRetVal != NULL )
		{
			if( bPortal2 )
			{
				m_hSecondaryPortal = pRetVal;
			}
			else
			{
				m_hPrimaryPortal = pRetVal;
			}
		}
	}
#endif

	return pRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: Allows a generic think function before the others are called
// Input  : state - which state the turret is currently in
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::PreThink( void )
{
	//Animate
	StudioFrameAdvance();

	//Do not interrupt current think function
	return false;
}

void CWeaponPortalgun::Think( void )
{
	//Allow descended classes a chance to do something before the think function
	if ( PreThink() )
		return;

	SetNextThink( gpGlobals->curtime + 0.1f );

	CPortal_Player *pPlayer = ToPortalPlayer( GetOwner() );

	if (pPlayer)
	{
		m_iCustomPortalColorSet = pPlayer->m_iCustomPortalColorSet;
	
		if (m_iCustomPortalColorSet && sv_allow_customized_portal_colors.GetBool())
		{
			m_iPortalColorSet = m_iCustomPortalColorSet - 1;
		}
		else
			m_iPortalColorSet = m_iPortalLinkageGroupID;

	}

	if ( !pPlayer || pPlayer->GetActiveWeapon() != this )
	{
#ifdef GAME_DLL
		if (use_server_portal_crosshair_test.GetBool())
		{
			m_fCanPlacePortal1OnThisSurface = 1.0f;
			m_fCanPlacePortal2OnThisSurface = 1.0f;
			m_fCanPlacePortal1OnThisSurfaceNetworked = m_fCanPlacePortal1OnThisSurface;
			m_fCanPlacePortal2OnThisSurfaceNetworked = m_fCanPlacePortal2OnThisSurface;
		}
#else
		if (!use_server_portal_crosshair_test.GetBool() && prediction->InPrediction() )
		{
			m_fCanPlacePortal1OnThisSurface = 1.0f;
			m_fCanPlacePortal2OnThisSurface = 1.0f;
		}
		else
		{
			m_fCanPlacePortal1OnThisSurface = m_fCanPlacePortal1OnThisSurfaceNetworked;
			m_fCanPlacePortal2OnThisSurface = m_fCanPlacePortal2OnThisSurfaceNetworked;
		}
#endif
		return;
	}

	// Test portal placement
#ifdef GAME_DLL

	// Doing this somehow makes portals more responsive...
	if (m_bCanFirePortal1)
		FirePortal(false, 0, 1);
	if (m_bCanFirePortal2)
		FirePortal(true, 0, 2);


	if (use_server_portal_crosshair_test.GetBool())
	{
		m_fCanPlacePortal1OnThisSurface = ( ( m_bCanFirePortal1 ) ? ( FirePortal( false, 0, 1 ) ) : ( 0.0f ) );
		m_fCanPlacePortal2OnThisSurface = ( ( m_bCanFirePortal2 ) ? ( FirePortal( true, 0, 2 ) ) : ( 0.0f ) );
		m_fCanPlacePortal1OnThisSurfaceNetworked = m_fCanPlacePortal1OnThisSurface;
		m_fCanPlacePortal2OnThisSurfaceNetworked = m_fCanPlacePortal2OnThisSurface;
	}
#else
	if (!use_server_portal_crosshair_test.GetBool() && prediction->InPrediction() ) // Use client crosshair test
	{
		m_fCanPlacePortal1OnThisSurface = ( ( m_bCanFirePortal1 ) ? ( FirePortal( false, 0, 1 ) ) : ( 0.0f ) );
		m_fCanPlacePortal2OnThisSurface = ( ( m_bCanFirePortal2 ) ? ( FirePortal( true, 0, 2 ) ) : ( 0.0f ) );
	}
	else
	{
		m_fCanPlacePortal1OnThisSurface = m_fCanPlacePortal1OnThisSurfaceNetworked;
		m_fCanPlacePortal2OnThisSurface = m_fCanPlacePortal2OnThisSurfaceNetworked;
	}
#endif

	// Draw obtained portal color chips
	int iSlot1State = ( ( m_bCanFirePortal1 ) ? ( 0 ) : ( 1 ) ); // FIXME: Portal gun might have only red but not blue;
	int iSlot2State = ( ( m_bCanFirePortal2 ) ? ( 0 ) : ( 1 ) );

	SetBodygroup( 1, iSlot1State );
	SetBodygroup( 2, iSlot2State );

	if ( pPlayer->GetViewModel() )
	{
		pPlayer->GetViewModel()->SetBodygroup( 1, iSlot1State );
		pPlayer->GetViewModel()->SetBodygroup( 2, iSlot2State );
	}

	// HACK HACK! Used to make the gun visually change when going through a cleanser!
	if ( m_fEffectsMaxSize1 > 4.0f )
	{
		m_fEffectsMaxSize1 -= gpGlobals->frametime * 400.0f;
		if ( m_fEffectsMaxSize1 < 4.0f )
			m_fEffectsMaxSize1 = 4.0f;
	}

	if ( m_fEffectsMaxSize2 > 4.0f )
	{
		m_fEffectsMaxSize2 -= gpGlobals->frametime * 400.0f;
		if ( m_fEffectsMaxSize2 < 4.0f )
			m_fEffectsMaxSize2 = 4.0f;
	}
}

float CWeaponPortalgun::GetPortal1Placablity(void)
{
#ifdef CLIENT_DLL
	if (use_server_portal_crosshair_test.GetBool())
		m_fCanPlacePortal1OnThisSurface = m_fCanPlacePortal1OnThisSurfaceNetworked;
#endif
	return m_fCanPlacePortal1OnThisSurface;
}

float CWeaponPortalgun::GetPortal2Placablity(void)
{
#ifdef CLIENT_DLL
	if (use_server_portal_crosshair_test.GetBool())
		m_fCanPlacePortal2OnThisSurface = m_fCanPlacePortal2OnThisSurfaceNetworked;
#endif
	return m_fCanPlacePortal2OnThisSurface;
}

const char *CWeaponPortalgun::GetPlacementSuccess(float fPlacementSuccess)
{
	const char *sPlacementSuccess = "";

	if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_NO_BUMP)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_NO_BUMP";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_STEAL)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_STEAL";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_BUMPED)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_BUMPED";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_CANT_FIT)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_CANT_FIT";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_CLEANSER)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_CLEANSER";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_NEAR)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_NEAR";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_INVALID_VOLUME)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_INVALID_VOLUME";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_INVALID_SURFACE)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_INVALID_SURFACE";
	else if (fPlacementSuccess == PORTAL_ANALOG_SUCCESS_PASSTHROUGH_SURFACE)
		sPlacementSuccess = "PORTAL_ANALOG_SUCCESS_PASSTHROUGH_SURFACE";
	else
		sPlacementSuccess = "Couldn't get fPlacementSuccess value";

	return sPlacementSuccess;
}