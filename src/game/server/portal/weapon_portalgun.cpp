//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"

#include "BasePropDoor.h"
#include "portal_player.h"
#include "te_effect_dispatch.h"
#include "gameinterface.h"
#include "prop_combine_ball.h"
#include "portal_shareddefs.h"
#include "triggers.h"
#include "collisionutils.h"
#include "cbaseanimatingprojectile.h"
#include "weapon_physcannon.h"
#include "prop_portal_shared.h"
#include "portal_placement.h"
#include "weapon_portalgun_shared.h"
#include "physicsshadowclone.h"
#include "particle_parse.h"


#define BLAST_SPEED_NON_PLAYER 1000.0f
#define BLAST_SPEED 3000.0f

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPortalgun, DT_WeaponPortalgun )

BEGIN_NETWORK_TABLE( CWeaponPortalgun, DT_WeaponPortalgun )
	SendPropBool( SENDINFO( m_bCanFirePortal1 ) ),
	SendPropBool( SENDINFO( m_bCanFirePortal2 ) ),
	SendPropInt( SENDINFO( m_iLastFiredPortal ) ),
	SendPropBool( SENDINFO( m_bOpenProngs ) ),
//	SendPropFloat( SENDINFO( m_fCanPlacePortal1OnThisSurface ) ),
//	SendPropFloat( SENDINFO( m_fCanPlacePortal2OnThisSurface ) ),
	SendPropFloat( SENDINFO( m_fCanPlacePortal1OnThisSurfaceNetworked ) ),
	SendPropFloat( SENDINFO( m_fCanPlacePortal2OnThisSurfaceNetworked ) ),
	SendPropFloat( SENDINFO( m_fEffectsMaxSize1 ) ), // HACK HACK! Used to make the gun visually change when going through a cleanser!
	SendPropFloat( SENDINFO( m_fEffectsMaxSize2 ) ),
	SendPropInt( SENDINFO( m_EffectState ) ),
	SendPropInt( SENDINFO( m_iPortalLinkageGroupID ) ),
	SendPropInt( SENDINFO( m_iCustomPortalColorSet ) ),	
	SendPropInt( SENDINFO( m_iPortalColorSet ) ),
	SendPropInt( SENDINFO( m_iValidPlayer ) ),
	SendPropBool( SENDINFO (m_bCanAttack) ),
	SendPropEHandle( SENDINFO (m_hPrimaryPortal) ),
	SendPropEHandle( SENDINFO (m_hSecondaryPortal) )
END_NETWORK_TABLE()

BEGIN_DATADESC( CWeaponPortalgun )

	DEFINE_KEYFIELD( m_bCanFirePortal1, FIELD_BOOLEAN, "CanFirePortal1" ),
	DEFINE_KEYFIELD( m_bCanFirePortal2, FIELD_BOOLEAN, "CanFirePortal2" ),
	DEFINE_KEYFIELD( m_iPortalLinkageGroupID, FIELD_CHARACTER, "PortalLinkageGroupID" ),
	DEFINE_KEYFIELD( m_bForceAlwaysUseSetID, FIELD_BOOLEAN, "ForceAlwaysUseSetID" ),
	DEFINE_KEYFIELD( m_iValidPlayer, FIELD_INTEGER, "ValidPlayer" ),
	DEFINE_FIELD( m_iLastFiredPortal, FIELD_INTEGER ),
	DEFINE_FIELD( m_bOpenProngs, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fCanPlacePortal1OnThisSurface, FIELD_FLOAT ),
	DEFINE_FIELD( m_fCanPlacePortal2OnThisSurface, FIELD_FLOAT ),
	DEFINE_FIELD( m_fCanPlacePortal1OnThisSurfaceNetworked, FIELD_FLOAT ),
	DEFINE_FIELD( m_fCanPlacePortal2OnThisSurfaceNetworked, FIELD_FLOAT ),
	DEFINE_FIELD( m_fEffectsMaxSize1, FIELD_FLOAT ),
	DEFINE_FIELD( m_fEffectsMaxSize2, FIELD_FLOAT ),
	DEFINE_FIELD( m_EffectState, FIELD_INTEGER ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ChargePortal1", InputChargePortal1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ChargePortal2", InputChargePortal2 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "FirePortal1", InputFirePortal1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "FirePortal2", InputFirePortal2 ),
	DEFINE_INPUTFUNC( FIELD_VECTOR, "FirePortalDirection1", FirePortalDirection1 ),
	DEFINE_INPUTFUNC( FIELD_VECTOR, "FirePortalDirection2", FirePortalDirection2 ),

	DEFINE_SOUNDPATCH( m_pMiniGravHoldSound ),

	DEFINE_OUTPUT ( m_OnFiredPortal1, "OnFiredPortal1" ),
	DEFINE_OUTPUT ( m_OnFiredPortal2, "OnFiredPortal2" ),

	DEFINE_FUNCTION( Think ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( weapon_portalgun, CWeaponPortalgun );
PRECACHE_WEAPON_REGISTER(weapon_portalgun);


void CWeaponPortalgun::Spawn( void )
{
	Precache();

	BaseClass::Spawn();

	m_bCanAttack = true;
	
	SetThink( &CWeaponPortalgun::Think );
	SetNextThink( gpGlobals->curtime + 0.1 );
	
	if (!m_bForceAlwaysUseSetID)
	{
		if( GameRules()->IsMultiplayer() || !GetOwner()->IsPlayer() )
		{
			CBaseEntity *pOwner = GetOwner();
			if( pOwner )
			{
				m_iPortalLinkageGroupID = pOwner->entindex();
			}

			Assert( (m_iPortalLinkageGroupID >= 0) && (m_iPortalLinkageGroupID < 256) );
		}	
	}

	m_iPortalColorSet = m_iPortalLinkageGroupID;

	CPortal_Player *pPlayer = ToPortalPlayer(GetOwner());

	if (pPlayer)
		m_iCustomPortalColorSet = pPlayer->m_iCustomPortalColorSet;

	m_hPrimaryPortal = CProp_Portal::FindPortal(m_iPortalLinkageGroupID, false, true);
	m_hSecondaryPortal = CProp_Portal::FindPortal(m_iPortalLinkageGroupID, true, true);
}

void CWeaponPortalgun::Activate()
{
	BaseClass::Activate();

	CreateSounds();

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( pPlayer )
	{
		CBaseEntity *pHeldObject = GetPlayerHeldEntity( pPlayer );
		OpenProngs( ( pHeldObject ) ? ( false ) : ( true ) );
		OpenProngs( ( pHeldObject ) ? ( true ) : ( false ) );
		if (!m_bForceAlwaysUseSetID)
		{
			if( GameRules()->IsMultiplayer() || !GetOwner()->IsPlayer() )
			{
				m_iPortalLinkageGroupID = GetOwner()->entindex();
			}
		}

		CPortal_Player *pPortalPlayer = ToPortalPlayer(GetOwner());

		if (pPortalPlayer)
			m_iCustomPortalColorSet = pPortalPlayer->m_iCustomPortalColorSet;
		
		m_hPrimaryPortal = CProp_Portal::FindPortal(m_iPortalLinkageGroupID, false, true);
		m_hSecondaryPortal = CProp_Portal::FindPortal(m_iPortalLinkageGroupID, true, true);
		Assert( (m_iPortalLinkageGroupID >= 0) && (m_iPortalLinkageGroupID < 256) );
	}

	// HACK HACK! Used to make the gun visually change when going through a cleanser!
	m_fEffectsMaxSize1 = 4.0f;
	m_fEffectsMaxSize2 = 4.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::Drop(const Vector &vecVelocity)
{
	UTIL_Remove(this);
}

void CWeaponPortalgun::FizzleOwnedPortals()
{
	for (int i = 0; i <= 1; ++i)
	{
		if (i >= 2)
			break;

		bool bPortal2 = i == 1;

		if ( bPortal2 && !CanFirePortal2() )
			continue;

		if ( !bPortal2 && !CanFirePortal1() )
			continue;
		
		CProp_Portal *pPortal = CProp_Portal::FindPortal(m_iPortalLinkageGroupID, bPortal2 );
		if ( pPortal )
		{
			pPortal->Fizzle();

			if ( pPortal->GetNextThink( s_pDelayedPlacementContext ) > gpGlobals->curtime )
			{
				pPortal->SetContextThink( NULL, gpGlobals->curtime, s_pDelayedPlacementContext ); 
			}
		}
	}
}

void CWeaponPortalgun::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	if(!m_bForceAlwaysUseSetID)
	{
		if (GameRules()->IsMultiplayer() || !pNewOwner->IsPlayer())
		{
		//	if( pNewOwner && pNewOwner->IsPlayer() )
			{
				m_iPortalLinkageGroupID = pNewOwner->entindex();
			}
			Assert( (m_iPortalLinkageGroupID >= 0) && (m_iPortalLinkageGroupID < 256) );
		}
	}

	CPortal_Player *pPortalPlayer = ToPortalPlayer(GetOwner());

	if (pPortalPlayer)
		m_iCustomPortalColorSet = pPortalPlayer->m_iCustomPortalColorSet;

	m_hPrimaryPortal = CProp_Portal::FindPortal(m_iPortalLinkageGroupID, false, true);
	m_hSecondaryPortal = CProp_Portal::FindPortal(m_iPortalLinkageGroupID, true, true);

	BaseClass::OnPickedUp( pNewOwner );		
}

void CWeaponPortalgun::CreateSounds()
{
	if (!m_pMiniGravHoldSound)
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pMiniGravHoldSound = controller.SoundCreate( filter, entindex(), "Weapon_Portalgun.HoldSound" );
		controller.Play( m_pMiniGravHoldSound, 0, 100 );
	}
}

void CWeaponPortalgun::StopLoopingSounds()
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	controller.SoundDestroy( m_pMiniGravHoldSound );
	m_pMiniGravHoldSound = NULL;

	BaseClass::StopLoopingSounds();
}

void CWeaponPortalgun::OpenProngs( bool bOpenProngs )
{
	if ( m_bOpenProngs == bOpenProngs )
	{
		return;
	}

	m_bOpenProngs = bOpenProngs;

	DoEffect( ( m_bOpenProngs ) ? ( EFFECT_HOLDING ) : ( EFFECT_READY ) );

	SendWeaponAnim( ( m_bOpenProngs ) ? ( ACT_VM_PICKUP ) : ( ACT_VM_RELEASE ) );
}

void CWeaponPortalgun::InputChargePortal1( inputdata_t &inputdata )
{
	if (m_iPortalLinkageGroupID == 1)
		DispatchParticleEffect( "portal_lightblue_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
	else if (m_iPortalLinkageGroupID == 2)
		DispatchParticleEffect( "portal_yellow_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
	else if (m_iPortalLinkageGroupID == 3)
		DispatchParticleEffect( "portal_green_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
	else
		DispatchParticleEffect( "portal_1_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
}

void CWeaponPortalgun::InputChargePortal2( inputdata_t &inputdata )
{
	if (m_iPortalLinkageGroupID == 1)
		DispatchParticleEffect( "portal_purple_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
	else if (m_iPortalLinkageGroupID == 2)
		DispatchParticleEffect( "portal_red_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
	else if (m_iPortalLinkageGroupID == 3)
		DispatchParticleEffect( "portal_pink_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
	else
		DispatchParticleEffect( "portal_2_charge", PATTACH_POINT_FOLLOW, this, "muzzle" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::StartEffects( void )
{
}

void CWeaponPortalgun::DestroyEffects( void )
{
	// Stop everything
	StopEffects();
}

//-----------------------------------------------------------------------------
// Purpose: Ready effects
//-----------------------------------------------------------------------------
void CWeaponPortalgun::DoEffectReady( void )
{
	if ( m_pMiniGravHoldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pMiniGravHoldSound, 0.0, 0.1 );
	}
}


//-----------------------------------------------------------------------------
// Holding effects
//-----------------------------------------------------------------------------
void CWeaponPortalgun::DoEffectHolding( void )
{
	if ( m_pMiniGravHoldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pMiniGravHoldSound, 1.0, 0.1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown for the weapon when it's holstered
//-----------------------------------------------------------------------------
void CWeaponPortalgun::DoEffectNone( void )
{
	if ( m_pMiniGravHoldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pMiniGravHoldSound, 0.0, 0.1 );
	}
}

void CC_UpgradePortalGun( void )
{
	CPortal_Player *pPlayer = ToPortalPlayer( UTIL_GetCommandClient() );

	CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun*>( pPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );
	if ( pPortalGun )
	{
		pPortalGun->SetCanFirePortal1();
		pPortalGun->SetCanFirePortal2();
	}
}

static ConCommand upgrade_portal("upgrade_portalgun", CC_UpgradePortalGun, "Equips the player with a single portal portalgun. Use twice for a dual portal portalgun.\n\tArguments:   	none ", FCVAR_CHEAT);




static void change_portalgun_linkage_id_f( const CCommand &args )
{
	if( sv_cheats->GetBool() == false ) //heavy handed version since setting the concommand with FCVAR_CHEATS isn't working like I thought
		return;

	if( args.ArgC() < 2 )
		return;

	unsigned char iNewID = (unsigned char)atoi( args[1] );

	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetCommandClient();

	int iWeaponCount = pPlayer->WeaponCount();
	for( int i = 0; i != iWeaponCount; ++i )
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if( pWeapon == NULL )
			continue;

		if( dynamic_cast<CWeaponPortalgun *>(pWeapon) != NULL )
		{
			CWeaponPortalgun *pPortalGun = (CWeaponPortalgun *)pWeapon;
			pPortalGun->m_iPortalLinkageGroupID = iNewID;
			pPortalGun->m_bForceAlwaysUseSetID = true; // HACK! Don't change linkage group id's when deploying
			pPortalGun->m_hPrimaryPortal = CProp_Portal::FindPortal(pPortalGun->m_iPortalLinkageGroupID, false, true);
			pPortalGun->m_hSecondaryPortal = CProp_Portal::FindPortal(pPortalGun->m_iPortalLinkageGroupID, true, true);
			break;
		}
	}
}

void CWeaponPortalgun::InputFirePortal1( inputdata_t &inputdata )
{
	FirePortal1();
}

void CWeaponPortalgun::InputFirePortal2( inputdata_t &inputdata )
{
	FirePortal2();
}
void CWeaponPortalgun::FirePortalDirection1( inputdata_t &inputdata )
{
	Vector vDirection;
	inputdata.value.Vector3D( vDirection );
	FirePortal( false, &vDirection );
	m_iLastFiredPortal = 1;
	
	CBaseCombatCharacter *pOwner = GetOwner();

	if( pOwner && pOwner->IsPlayer() )
	{
		WeaponSound( SINGLE );
	}
	else
	{
		WeaponSound( SINGLE_NPC );
	}
}

void CWeaponPortalgun::FirePortalDirection2( inputdata_t &inputdata )
{
	Vector vDirection;
	inputdata.value.Vector3D( vDirection );
	FirePortal( true, &vDirection );
	m_iLastFiredPortal = 2;
	
	CBaseCombatCharacter *pOwner = GetOwner();

	if( pOwner && pOwner->IsPlayer() )
	{
		WeaponSound( WPN_DOUBLE );
	}
	else
	{
		WeaponSound( DOUBLE_NPC );
	}
}

bool CWeaponPortalgun::CanPlayerPickupMe( CBasePlayer *pPlayer )
{
	if (!m_bAllowPlayerEquip)
		return false;

	CWeaponPortalgun *pPortalgun = dynamic_cast<CWeaponPortalgun*>(pPlayer->Weapon_OwnsThisType("weapon_portalgun"));

	if ( pPortalgun )
	{
		if ( pPortalgun->m_iPortalLinkageGroupID != m_iPortalLinkageGroupID && m_bForceAlwaysUseSetID )
			return false;
	}
	
	if ( m_iValidPlayer && m_iValidPlayer != pPlayer->entindex() )
		return false;

	return true;
}

void CWeaponPortalgun::DefaultTouch( CBaseEntity *pOther )
{
	CBasePlayer *pPlayer = ToBasePlayer(pOther);

	if (pPlayer && CanPlayerPickupMe( pPlayer ) )
	{
		BaseClass::DefaultTouch( pOther );
	}
	else if ( !pPlayer )
		BaseClass::DefaultTouch( pOther );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	
	if ( pPlayer )
	{
		m_OnPlayerUse.FireOutput(pActivator, pCaller);

		if ( !CanPlayerPickupMe( pPlayer ) )
			return;

	}

	BaseClass::Use( pActivator, pCaller, useType, value );
}

ConCommand change_portalgun_linkage_id( "change_portalgun_linkage_id", change_portalgun_linkage_id_f, "Changes the portal linkage ID for the portal gun held by the commanding player.", FCVAR_CHEAT );
