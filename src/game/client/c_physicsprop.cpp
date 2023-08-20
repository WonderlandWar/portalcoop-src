//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "model_types.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "view.h"
#include "clienteffectprecachesystem.h"
#include "c_physicsprop.h"
#include "tier0/vprof.h"
#include "ivrenderview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_PhysicsProp, DT_PhysicsProp, CPhysicsProp)
	RecvPropBool( RECVINFO( m_bAwake ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_PhysicsPropRespawnable, DT_PhysicsPropRespawnable, CPhysicsPropRespawnable)
	RecvPropBool( RECVINFO( m_bAwake ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_PhysicsPropOverride, DT_PhysicsPropOverride, CPhysicsPropOverride)
	RecvPropBool( RECVINFO( m_bAwake ) ),
END_RECV_TABLE()

#if 1
LINK_ENTITY_TO_CLASS(prop_physics, C_PhysicsProp)
LINK_ENTITY_TO_CLASS(prop_physics_respawnable, C_PhysicsPropRespawnable)
LINK_ENTITY_TO_CLASS(prop_physics_override, C_PhysicsPropOverride)
#endif

ConVar r_PhysPropStaticLighting( "r_PhysPropStaticLighting", "1" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PhysicsProp::C_PhysicsProp( void )
{
	m_pPhysicsObject = NULL;
	m_takedamage = DAMAGE_YES;

	// default true so static lighting will get recomputed when we go to sleep
	m_bAwakeLastTime = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PhysicsProp::~C_PhysicsProp( void )
{
}


// @MULTICORE (toml 9/18/2006): this visualization will need to be implemented elsewhere
ConVar r_visualizeproplightcaching( "r_visualizeproplightcaching", "0" );

//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
bool C_PhysicsProp::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	CreateModelInstance();

	if ( r_PhysPropStaticLighting.GetBool() && m_bAwakeLastTime != m_bAwake )
	{
		if ( m_bAwakeLastTime && !m_bAwake )
		{
			// transition to sleep, bake lighting now, once
			if ( !modelrender->RecomputeStaticLighting( GetModelInstance() ) )
			{
				// not valid for drawing
				return false;
			}

			if ( r_visualizeproplightcaching.GetBool() )
			{
				float color[] = { 0.0f, 1.0f, 0.0f, 1.0f };
				render->SetColorModulation( color );
			}
		}
		else if ( r_visualizeproplightcaching.GetBool() )
		{
			float color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
			render->SetColorModulation( color );
		}
	}

	if ( !m_bAwake && r_PhysPropStaticLighting.GetBool() )
	{
		// going to sleep, have static lighting
		pInfo->flags |= STUDIO_STATIC_LIGHTING;
	}
	
	// track state
	m_bAwakeLastTime = m_bAwake;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get the specified key's angles for this prop from the QC's physgun_interactions
//-----------------------------------------------------------------------------
bool C_PhysicsProp::GetPropDataAngles( const char *pKeyName, QAngle &vecAngles )
{
	KeyValues *modelKeyValues = new KeyValues("");
	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
	{
		KeyValues *pkvPropData = modelKeyValues->FindKey( "physgun_interactions" );
		if ( pkvPropData )
		{
			char const *pszBase = pkvPropData->GetString( pKeyName );
			if ( pszBase && pszBase[0] )
			{
				UTIL_StringToVector( vecAngles.Base(), pszBase );
				modelKeyValues->deleteThis();
				return true;
			}
		}
	}

	modelKeyValues->deleteThis();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float C_PhysicsProp::GetCarryDistanceOffset( void )
{
	KeyValues *modelKeyValues = new KeyValues("");
	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
	{
		KeyValues *pkvPropData = modelKeyValues->FindKey( "physgun_interactions" );
		if ( pkvPropData )
		{
			float flDistance = pkvPropData->GetFloat( "carry_distance_offset", 0 );
			modelKeyValues->deleteThis();
			return flDistance;
		}
	}

	modelKeyValues->deleteThis();
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void C_PhysicsProp::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( pPlayer )
	{
		pPlayer->PickupObject( this );
	}
}