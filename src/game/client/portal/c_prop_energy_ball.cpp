//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	c_prop_energy_ball.cpp
// 
// Purpose: Portal version of the combine ball. This client code is needed to provide a different
//			look when the energy ball has infinite life and to have modified client effects.
//
//=====================================================================================//


#include "cbase.h"							// precompiled headers
#include "c_prop_combine_ball.h"			// Our parent class
#include "clienteffectprecachesystem.h"		// To precache our new material
#include "portal_shareddefs.h"
#include "c_te_effect_dispatch.h"
#include "fx_quad.h"
#include "fx.h"

ConVar cl_energy_ball_start_fade_time ( "cl_energy_ball_start_fade_time", "8", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Portal version of a combine ball
//-----------------------------------------------------------------------------
class C_PropEnergyBall : public C_PropCombineBall
{
public:
	DECLARE_CLASS( C_PropEnergyBall, C_PropCombineBall );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_PropEnergyBall();

	virtual void	OnDataChanged( DataUpdateType_t updateType );

protected:
	bool InitMaterials();

	bool	m_bIsInfiniteLife;			// if this energy ball is an infinite life variety
	float	m_fTimeTillDeath;			// If this is a finite life energy ball, the time remaining until detonation
	float	m_fCurAlpha;				// The amount of alpha to apply at DrawModel, to simulate a decaying energy ball
};

// precache our different materials for the infinite life energy balls
CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectEnergyBall )

	CLIENTEFFECT_MATERIAL( "effects/eball_infinite_life_rex" )
	CLIENTEFFECT_MATERIAL( "effects/eball_infinite_life" )
	CLIENTEFFECT_MATERIAL( "effects/eball_finite_life" )

CLIENTEFFECT_REGISTER_END()


IMPLEMENT_CLIENTCLASS_DT( C_PropEnergyBall, DT_PropEnergyBall, CPropEnergyBall )

	RecvPropBool( RECVINFO( m_bIsInfiniteLife ) ),
	RecvPropFloat( RECVINFO( m_fTimeTillDeath ) ),

END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( prop_energy_ball, C_PropEnergyBall );


BEGIN_PREDICTION_DATA( C_PropEnergyBall )

	DEFINE_PRED_FIELD( m_bIsInfiniteLife, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fTimeTillDeath, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PropEnergyBall::C_PropEnergyBall(): m_bIsInfiniteLife(false), m_fTimeTillDeath(-1), m_fCurAlpha ( 1.0f )
{
}

//-----------------------------------------------------------------------------
// Purpose: Flag our data as new this frame
// Input  : DataUpdateType_t, either created or updated
// Output : void
//-----------------------------------------------------------------------------
void C_PropEnergyBall::OnDataChanged(DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// If our data changed this frame, then operate based on it next think
	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		float fStartFadeTime = cl_energy_ball_start_fade_time.GetFloat();

		if ( fStartFadeTime < 1.0f )
		{ 
			fStartFadeTime = 1.0f;
		}

		// The last x seconds of life, fade
		if ( (m_fTimeTillDeath > 0.0f) )
		{
			float fNewAlpha = m_fTimeTillDeath / fStartFadeTime;
			clamp( fNewAlpha, 0.0f, 1.0f );
			m_fCurAlpha = fNewAlpha;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use our custom body materials for energy ball, but otherwise use the base class materials (base being C_PropCombineBall)
// Output : bool
//-----------------------------------------------------------------------------
bool C_PropEnergyBall::InitMaterials()
{
	// Use the same materials as a combine ball
	bool bRetVal = BaseClass::InitMaterials();

	// If we're an infinite life combine ball, swap out the body material (and the base implementation didnt fail)
	IMaterial* pBodyMat;
	if ( m_bIsInfiniteLife )
	{
		if ( sv_portal_game.GetInt() == PORTAL_GAME_REXAURA )
		{
			pBodyMat = materials->FindMaterial( "effects/eball_infinite_life_rex", NULL, false );
		}
		else
		{
			pBodyMat = materials->FindMaterial( "effects/eball_infinite_life", NULL, false );
		}
	}
	else
	{
		pBodyMat = materials->FindMaterial( "effects/eball_finite_life", NULL, false );
	}

	// If we can find our custom material, use it.
	if ( pBodyMat == NULL )
	{
		bRetVal = false;
	}
	else
	{
		m_pBodyMaterial = pBodyMat;
		m_pBodyMaterial->AlphaModulate( m_fCurAlpha );
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void InfiniteEnergyBallImpactCallback( const CEffectData &data )
{
	// Quick flash
	FX_AddQuad( data.m_vOrigin,
				data.m_vNormal,
				data.m_flRadius * 10.0f,
				0,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 0.7f, 1.0f, 0.7f ), 
				0.25f, 
				"effects/combinemuzzle1_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );

	// Lingering burn
	FX_AddQuad( data.m_vOrigin,
				data.m_vNormal, 
				data.m_flRadius * 2.0f,
				data.m_flRadius * 4.0f,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 0.6f, 1.0f, 0.6f ), 
				0.5f, 
				"effects/combinemuzzle2_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );

	// Throw sparks
	FX_ElectricSpark( data.m_vOrigin, 2, 1, &data.m_vNormal );
}

DECLARE_CLIENT_EFFECT( "cball_bounce_inf", InfiniteEnergyBallImpactCallback );