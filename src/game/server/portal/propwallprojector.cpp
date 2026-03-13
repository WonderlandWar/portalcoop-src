#include "cbase.h"
#include "soundenvelope.h"
#include "propwallprojector.h"
#include "projectedwallentity.h"

#if !defined ( NO_PROJECTED_WALL )

BEGIN_DATADESC( CPropWallProjector )
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_wall_projector, CPropWallProjector );

CPropWallProjector::CPropWallProjector()
{

}

CPropWallProjector::~CPropWallProjector()
{

}

void CPropWallProjector::Spawn( void )
{
	BaseClass::Spawn();
	Precache();
	SetModel( "models/props/wall_emitter.mdl" );
	SetSolid( SOLID_VPHYSICS );
	//AddEffects( EF_NOFLASHLIGHT );
}

void CPropWallProjector::Precache( void )
{
	PrecacheModel("models/props/wall_emitter.mdl");
	PrecacheScriptSound("VFX.BridgeGlow");
}

void CPropWallProjector::Project( void )
{
	BaseClass::Project();
}

void CPropWallProjector::Shutdown( void )
{
	BaseClass::Shutdown();
}

CBaseProjectedEntity *CPropWallProjector::CreateNewProjectedEntity( void )
{
	 return CProjectedWallEntity::CreateNewInstance();
}

#endif // NO_PROJECTED_WALL