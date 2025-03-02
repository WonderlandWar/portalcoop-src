#include "cbase.h"
#include "prop_box.h"
#include "datacache/imdlcache.h"
#include "trigger_portal_cleanser.h"
#include "trigger_box_reflector.h"

#define BOX_MODEL "models/props/metal_box.mdl"

BEGIN_DATADESC( CPropBox )

	DEFINE_FIELD( m_hAttached, FIELD_EHANDLE ),

	DEFINE_OUTPUT( m_OnDissolved, "OnDissolved" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Dissolve", InputDissolve ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPropBox, DT_PropBox )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( prop_box, CPropBox );

CPropBox::CPropBox()
{
	m_spawnflags |= ( SF_PHYSPROP_DONT_TAKE_PHYSICS_DAMAGE | SF_PHYSPROP_ENABLE_ON_PHYSCANNON | SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );
}

void CPropBox::Spawn( void )
{
	m_hAttached = NULL;

	SetModelName( castable_string_t( BOX_MODEL ) );

	BaseClass::Spawn();

	//SetModel( BOX_MODEL );

	SetPingIcon( PING_ICON_BOX );
}

void CPropBox::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( BOX_MODEL );

	PrecacheScriptSound( "Rexaura.BoxDissolve" );
}

void CPropBox::EnergyBallHit( CBaseEntity *pBall )
{
	CTriggerBoxReflector *pAttached = m_hAttached;
	if ( pAttached )
	{
		pAttached->EnergyBallHit( pBall );
	}

	// Only fizzle energy balls in Rexaura
	if ( sv_portal_game.GetInt() == PORTAL_GAME_REXAURA )
	{
		CTriggerPortalCleanser::FizzleBaseAnimating( this, NULL );
	}
}

void CPropBox::PreDissolve( CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	if ( sv_portal_game.GetInt() == PORTAL_GAME_REXAURA )
	{
		EmitSound( "Rexaura.BoxDissolve" );
	}
	m_OnDissolved.FireOutput( pActivator, pCaller );
}

void CPropBox::InputDissolve( inputdata_t &inputdata )
{
	CTriggerPortalCleanser::FizzleBaseAnimating( this, NULL );
}

#define PORTAL_WEIGHT_BOX_MODEL_NAME "models/props/metal_box.mdl"

// Create the box used for portal puzzles, named 'box'. Used for easy debugging of portal puzzles.
void CC_Create_PortalWeightBoxNew( void )
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	// Try to create entity
	CBaseEntity *entity = CreateEntityByName("prop_box");
	if (entity)
	{
		entity->PrecacheModel( PORTAL_WEIGHT_BOX_MODEL_NAME );
		entity->SetModel( PORTAL_WEIGHT_BOX_MODEL_NAME );
		entity->SetName( MAKE_STRING("box_model_1") );
		entity->AddSpawnFlags( SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );
		entity->m_iPingIcon = PING_ICON_BOX;
		entity->Precache();
		DispatchSpawn(entity);

		// Now attempt to drop into the world
		CBasePlayer* pPlayer = UTIL_GetCommandClient();
		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine(pPlayer->EyePosition(),
			pPlayer->EyePosition() + forward * MAX_TRACE_LENGTH,MASK_SOLID, 
			pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 )
		{
			tr.endpos.z += 12;
			entity->Teleport( &tr.endpos, NULL, NULL );
			UTIL_DropToFloor( entity, MASK_SOLID );
		}
	}
	CBaseEntity::SetAllowPrecache( allowPrecache );
}

static ConCommand ent_create_portal_weight_box_new("ent_create_portal_weight_box_new", CC_Create_PortalWeightBoxNew, "Creates a weight box used in portal puzzles at the location the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);