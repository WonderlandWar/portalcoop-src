//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which bumps portal placement. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "c_func_portal_bumper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( C_FuncPortalBumper )

	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),
	
END_DATADESC()

IMPLEMENT_CLIENTCLASS_DT(C_FuncPortalBumper, DT_FuncPortalBumper, CFuncPortalBumper)

	RecvPropBool(RECVINFO(m_bActive)),

END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( func_portal_bumper, C_FuncPortalBumper );

C_FuncPortalBumper::C_FuncPortalBumper()
{
	m_bActive = true;
}

void C_FuncPortalBumper::Spawn()
{
	BaseClass::Spawn();

	// Bind to our model, cause we need the extents for bounds checking
	SetModel( STRING( GetModelName() ) );
	SetRenderMode( kRenderNone );	// Don't draw
	SetSolid( SOLID_VPHYSICS );	// we may want slanted walls, so we'll use OBB
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
}

void C_FuncPortalBumper::UpdatePartitionListEntry( void )
{
	BaseClass::UpdatePartitionListEntry();
	return;
	::partition->RemoveAndInsert( 
		PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_STATIC_PROPS | PARTITION_CLIENT_TRIGGER_ENTITIES | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES | PARTITION_CLIENT_SOLID_EDICTS,  // add
		CollisionProp()->GetPartitionHandle() );
}