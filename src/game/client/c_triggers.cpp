//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_triggers.h"
#include "in_buttons.h"
#include "c_prop_vehicle.h"
#include "c_ai_basenpc.h"
//#include "c_func_brush.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_BaseTrigger::C_BaseTrigger()
{
	AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
	m_bOldDisabled = false;
}

void C_BaseTrigger::PreDataUpdate( DataUpdateType_t updatetype )
{
	BaseClass::PreDataUpdate( updatetype );
	m_bOldDisabled = m_bDisabled;
}

void C_BaseTrigger::OnDataChanged( DataUpdateType_t updatetype )
{
	BaseClass::OnDataChanged( updatetype );
	if ( m_bDisabled != m_bOldDisabled )
	{
		if ( VPhysicsGetObject())
		{
			VPhysicsGetObject()->EnableCollisions( !m_bDisabled );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make this trigger touchable for the client
//-----------------------------------------------------------------------------
void C_BaseTrigger::UpdatePartitionListEntry()
{
	SpatialPartitionListMask_t added = PARTITION_CLIENT_TRIGGER_ENTITIES | PARTITION_CLIENT_NON_STATIC_EDICTS;
	if ( m_bDisabled || !IsPredicted() ) // Don't wanna be able to touch disabled triggers
	{
		::partition->Remove( added, 
			CollisionProp()->GetPartitionHandle() );
		return;
	}

	::partition->RemoveAndInsert(
		PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_STATIC_PROPS | PARTITION_CLIENT_SOLID_EDICTS,  // remove
		added,  // add
		CollisionProp()->GetPartitionHandle() );
}


C_BaseVPhysicsTrigger::C_BaseVPhysicsTrigger()
{
	m_bOldDisabled = false;
}

void C_BaseVPhysicsTrigger::PreDataUpdate( DataUpdateType_t updatetype )
{
	BaseClass::PreDataUpdate( updatetype );
	m_bOldDisabled = m_bDisabled;
}

void C_BaseVPhysicsTrigger::OnDataChanged( DataUpdateType_t updatetype )
{
	BaseClass::OnDataChanged( updatetype );
	if ( m_bDisabled != m_bOldDisabled )
	{
		if ( VPhysicsGetObject())
		{
			VPhysicsGetObject()->EnableCollisions( !m_bDisabled );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make this trigger touchable for the client
//-----------------------------------------------------------------------------
void C_BaseVPhysicsTrigger::UpdatePartitionListEntry()
{
	SpatialPartitionListMask_t added = PARTITION_CLIENT_TRIGGER_ENTITIES | PARTITION_CLIENT_NON_STATIC_EDICTS;
	if ( m_bDisabled || !IsPredicted() ) // Don't wanna be able to touch disabled triggers
	{
		::partition->Remove( added, 
			CollisionProp()->GetPartitionHandle() );
		return;
	}

	::partition->RemoveAndInsert(
		PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_STATIC_PROPS | PARTITION_CLIENT_SOLID_EDICTS,  // remove
		added,  // add
		CollisionProp()->GetPartitionHandle() );
}