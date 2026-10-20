//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Fizzle effects for portal.
//
//=============================================================================//

#include "cbase.h"
#include "clienteffectprecachesystem.h"
#include "fx.h"
#include "fx_sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "particles_ez.h"
#include "decals.h"
#include "engine/IEngineSound.h"
#include "fx_quad.h"
#include "engine/ivdebugoverlay.h"
#include "shareddefs.h"
#include "portal_shareddefs.h"
#include "effect_color_tables.h"
#include "prediction.h"
#include "portal_shareddefs.h"
#include "vprof.h"
#include "particles_localspace.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
extern float g_flGameCurTime;

static void MuzzleFlash_Portalgun_Thirdperson( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	VPROF_BUDGET( "MuzzleFlash_Portalgun_Thirdperson", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	CSmartPtr<CLocalSpaceEmitter> pSimple = CLocalSpaceEmitter::Create( "MuzzleFlash", hEntity, attachmentIndex );

	SimpleParticle *pParticle;
	Vector			forward(1,0,0), offset; //NOTENOTE: All coords are in local space

	float flScale = random->RandomFloat( 1.1f, 1.15f );

	
	PMaterialHandle pMuzzle[2];
	pMuzzle[0] = pSimple->GetPMaterial( "effects/combinemuzzle1" );
	pMuzzle[1] = pSimple->GetPMaterial( "effects/combinemuzzle2" );

	// Flash
	for ( int i = 1; i < 6; i++ )
	{
		offset = (forward * (i*5.0f*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pMuzzle[random->RandomInt(0,1)], offset );
			
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.025f;

		pParticle->m_vecVelocity.Init();

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 200+random->RandomInt(0,55);

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 255;

		pParticle->m_uchStartSize	= ( (random->RandomFloat( 6.0f, 8.0f ) * (16-(i))/16) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	// Tack on the smoke
	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pMuzzle[random->RandomInt(0,1)], vec3_origin );
		
	if ( pParticle == NULL )
		return;

	pParticle->m_flLifetime		= 0.0f;
	pParticle->m_flDieTime		= 0.025f;

	pParticle->m_vecVelocity.Init();

	pParticle->m_uchColor[0]	= 255;
	pParticle->m_uchColor[1]	= 255;
	pParticle->m_uchColor[2]	= 255;

	pParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
	pParticle->m_uchEndAlpha	= 32;

	pParticle->m_uchStartSize	= random->RandomFloat( 10.0f, 16.0f );
	pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
	
	pParticle->m_flRoll			= random->RandomInt( 0, 360 );
	pParticle->m_flRollDelta	= 0.0f;
}

class C_PortalBlast : public C_BaseEntity
{
	DECLARE_CLASS( C_PortalBlast, C_BaseEntity );

public:

	static void		Create( bool bIsPortal2, PortalPlacedByType ePlacedBy, const Vector &vStart, const Vector &vEnd, const QAngle &qAngles, float fDeathTime, PortalColorSet_t iPortalColorSet, EHANDLE hEntity );

	void			Init( bool bIsPortal2, PortalPlacedByType ePlacedBy, const Vector &vStart, const Vector &vEnd, const QAngle &qAngles, float fDeathTime, PortalColorSet_t iPortalColorSet, EHANDLE hEntity );

	virtual void	ClientThink( void );

private:

	Vector	m_ptCreationPoint;
	Vector	m_ptDeathPoint;
	Vector	m_ptAimPoint;

	float	m_fCreationTime;
	float	m_fDeathTime;

	PortalPlacedByType m_ePlacedBy;
};


void C_PortalBlast::Create( bool bIsPortal2, PortalPlacedByType ePlacedBy, const Vector &vStart, const Vector &vEnd, const QAngle &qAngles, float fDeathTime, PortalColorSet_t iPortalColorSet, EHANDLE hEntity )
{
	C_PortalBlast *pPortalBlast = new C_PortalBlast;
	pPortalBlast->Init( bIsPortal2, ePlacedBy, vStart, vEnd, qAngles, fDeathTime, iPortalColorSet, hEntity );
}


void C_PortalBlast::Init( bool bIsPortal2, PortalPlacedByType ePlacedBy, const Vector &vStart, const Vector &vEnd, const QAngle &qAngles, float fDeathTime, PortalColorSet_t iPortalColorSet, EHANDLE hEntity )
{
	ClientEntityList().AddNonNetworkableEntity( this );
	ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );

	AddToLeafSystem( RENDER_GROUP_OPAQUE_ENTITY );

	SetThink( &C_PortalBlast::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_ptCreationPoint = vStart;
	m_ptDeathPoint = vEnd;

	m_ePlacedBy = ePlacedBy;

	SetAbsOrigin( m_ptCreationPoint );
	
	m_fCreationTime = g_flGameCurTime;

	m_fDeathTime = fDeathTime;

	if ( m_fDeathTime - 0.1f < m_fCreationTime )
	{
		m_fDeathTime = m_fCreationTime + 0.1f;
	}

	Vector vForward;
	AngleVectors( qAngles, &vForward );

	m_ptAimPoint = m_ptCreationPoint + vForward * m_ptCreationPoint.DistTo( m_ptDeathPoint );

	C_BaseCombatWeapon *pWeapon = hEntity ? hEntity->MyCombatWeaponPointer() : NULL;

	C_BasePlayer *pPlayer = ToBasePlayer( hEntity.Get() );
	
	//Probably unnecessary
	if ( pWeapon && pWeapon->GetOwner() == C_BasePlayer::GetLocalPlayer() )
	{
		ePlacedBy = PORTAL_PLACED_BY_PLAYER;
	}
	else
	{
		if ( pWeapon && pWeapon->GetOwner() )
		{
			int attachment = pWeapon->LookupAttachment( "muzzle" );
			MuzzleFlash_Portalgun_Thirdperson( hEntity, attachment );
		}
		ePlacedBy = PORTAL_PLACED_BY_PEDESTAL;
	}


	if ( ePlacedBy == PORTAL_PLACED_BY_PLAYER )
	{
		if (iPortalColorSet == PORTAL_COLOR_SET_LIGHTBLUE_PURPLE)
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_purple_projectile_stream" ) : ( "portal_lightblue_projectile_stream" ) ), PATTACH_ABSORIGIN_FOLLOW );		
		}
		else if (iPortalColorSet == PORTAL_COLOR_SET_YELLOW_RED)
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_red_projectile_stream" ) : ( "portal_yellow_projectile_stream" ) ), PATTACH_ABSORIGIN_FOLLOW );		
		}
		else if (iPortalColorSet == PORTAL_COLOR_SET_GREEN_PINK)
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_pink_projectile_stream" ) : ( "portal_green_projectile_stream" ) ), PATTACH_ABSORIGIN_FOLLOW );		
		}
		else
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_2_projectile_stream" ) : ( "portal_1_projectile_stream" ) ), PATTACH_ABSORIGIN_FOLLOW );
		}
	}
	else
	{
		if (iPortalColorSet == PORTAL_COLOR_SET_LIGHTBLUE_PURPLE)
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_purple_projectile_stream_pedestal" ) : ( "portal_lightblue_projectile_stream_pedestal" ) ), PATTACH_ABSORIGIN_FOLLOW );		
		}
		else if (iPortalColorSet == PORTAL_COLOR_SET_YELLOW_RED)
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_red_projectile_stream_pedestal" ) : ( "portal_yellow_projectile_stream_pedestal" ) ), PATTACH_ABSORIGIN_FOLLOW );		
		}
		else if (iPortalColorSet == PORTAL_COLOR_SET_GREEN_PINK)
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_pink_projectile_stream_pedestal" ) : ( "portal_green_projectile_stream_pedestal" ) ), PATTACH_ABSORIGIN_FOLLOW );		
		}
		else
		{
			ParticleProp()->Create( ( ( bIsPortal2 ) ? ( "portal_2_projectile_stream_pedestal" ) : ( "portal_1_projectile_stream_pedestal" ) ), PATTACH_ABSORIGIN_FOLLOW );
		}
	}
}

void C_PortalBlast::ClientThink( void )
{
	if ( m_fCreationTime == 0.0f && m_fDeathTime == 0.0f )
	{
		// Die!
		Remove();
		return;
	}
	
	float fT = ( gpGlobals->curtime - m_fCreationTime ) / ( m_fDeathTime - m_fCreationTime );

	if ( fT >= 1.0f )
	{
		// Ready to die! But we want one more frame in the final position
		SetAbsOrigin( m_ptDeathPoint );

		m_fCreationTime = 0.0f;
		m_fDeathTime = 0.0f;

		return;
	}

	// Set the interpolated position
	Vector vTarget = m_ptAimPoint * ( 1.0f - fT ) + m_ptDeathPoint * fT;
	SetAbsOrigin( m_ptCreationPoint * ( 1.0f - fT ) + vTarget * fT );
}


void PortalBlastCallback( const CEffectData & data )
{
	C_PortalBlast::Create( ( data.m_nColor == 1 ) ? ( false ) : ( true ), (PortalPlacedByType)data.m_nDamageType, data.m_vOrigin, data.m_vStart, data.m_vAngles, data.m_flScale, (PortalColorSet_t)data.m_nHitBox, cl_entitylist->GetBaseEntityFromHandle( data.m_hEntity ) );
}

DECLARE_CLIENT_EFFECT( "PortalBlast", PortalBlastCallback );