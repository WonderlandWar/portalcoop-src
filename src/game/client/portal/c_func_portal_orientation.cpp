//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "c_func_portal_orientation.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_util_shared.h"
#include "portal_placement.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_EntityClassList<C_FuncPortalOrientation> g_FuncPortalOrientationVolumeList;
template <> C_FuncPortalOrientation *C_EntityClassList<C_FuncPortalOrientation>::m_pClassList = NULL;

C_FuncPortalOrientation* GetPortalOrientationVolumeList()
{
	return g_FuncPortalOrientationVolumeList.m_pClassList;
}

//-----------------------------------------------------------------------------
// Purpose: Test for func_orientation_volume ents which could effect the placement angles of a portal.
// Input  : vecCurAngles - Default angles to place (may change)
//			vecCurOrigin - origin of the portal on placement
//			pPortal - The portal attempting to place
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UTIL_TestForOrientationVolumes( QAngle& vecCurAngles, const Vector& vecCurOrigin, const CProp_Portal* pPortal )
{
	if ( !pPortal )
		return false;

	// Walk list of orientation volumes, obb test each with candidate portal
	C_FuncPortalOrientation *pList = g_FuncPortalOrientationVolumeList.m_pClassList;
	while ( pList )
	{
		if ( !pList->IsActive() )
		{
			pList = pList->m_pNext;
			continue;
		}

		if ( IsOBBIntersectingOBB( vecCurOrigin, vecCurAngles, CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, 
			pList->GetAbsOrigin(), pList->GetCollideable()->GetCollisionAngles(), pList->GetCollideable()->OBBMins(), pList->GetCollideable()->OBBMaxs() ) )
		{
			QAngle vecGoalAngles;
			// Ent is marked to match angles of it's linked partner
			if ( pList->m_bMatchLinkedAngles )
			{
				// This feature requires a linked portal on a floor or ceiling. Bail without effecting
				// the placement angles if we fail those requirements.
				CProp_Portal* pLinked = pPortal->m_hLinkedPortal.Get();
				if ( !pLinked || !(AnglesAreEqual( vecCurAngles.x, -90.0f, 0.1f ) || AnglesAreEqual( vecCurAngles.x, 90.0f, 0.1f )) )
					return false;

				vecGoalAngles = pLinked->GetAbsAngles();
				vecCurAngles.y = 0.0f;
				vecCurAngles.z = vecGoalAngles.z;
			}
			// Match the angles loaded in from the map
			else
			{
				vecGoalAngles = pList->m_vecAnglesToFace;
				vecCurAngles = vecGoalAngles;
			}

			return true;
		}
		pList = pList->m_pNext;
	}

	return false;
}

#undef CFuncPortalOrientation

IMPLEMENT_CLIENTCLASS_DT( C_FuncPortalOrientation, DT_FuncPortalOrientation, CFuncPortalOrientation )

RecvPropBool(RECVINFO(m_bDisabled)),
RecvPropBool(RECVINFO(m_bMatchLinkedAngles)),
RecvPropQAngles(RECVINFO(m_vecAnglesToFace)),

END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( func_portal_orientation, C_FuncPortalOrientation ); 

BEGIN_DATADESC( C_FuncPortalOrientation )

	DEFINE_FIELD( m_iListIndex, FIELD_INTEGER ),

	//DEFINE_FIELD ( m_pNext, C_FuncPortalOrientation ),

	DEFINE_FIELD( m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD ( m_bMatchLinkedAngles, FIELD_BOOLEAN ),
	DEFINE_FIELD ( m_vecAnglesToFace, FIELD_VECTOR ),

END_DATADESC()

C_FuncPortalOrientation::C_FuncPortalOrientation()
{
	g_FuncPortalOrientationVolumeList.Insert( this );
}

C_FuncPortalOrientation::~C_FuncPortalOrientation()
{
	g_FuncPortalOrientationVolumeList.Remove( this );
}

void C_FuncPortalOrientation::Spawn()
{
	BaseClass::Spawn();

	// Bind to our model, cause we need the extents for bounds checking
	SetModel( STRING( GetModelName() ) );
	SetRenderMode( kRenderNone );	// Don't draw
	SetSolid( SOLID_VPHYSICS );	// we may want slanted walls, so we'll use OBB
	AddSolidFlags( FSOLID_NOT_SOLID );
}

//-----------------------------------------------------------------------------
// Purpose: Test for portals inside our volume when we switch on, and forcibly rotate them
//-----------------------------------------------------------------------------
void C_FuncPortalOrientation::OnActivate( void )
{
	if ( !GetCollideable() || m_bDisabled )
		return;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( IsOBBIntersectingOBB( pTempPortal->GetAbsOrigin(), pTempPortal->GetAbsAngles(), CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, 
			GetAbsOrigin(), GetCollideable()->GetCollisionAngles(), GetCollideable()->OBBMins(), GetCollideable()->OBBMaxs() ) )
			{
				QAngle angNewAngles;
				if ( m_bMatchLinkedAngles )
				{
					CProp_Portal* pLinked = pTempPortal->m_hLinkedPortal.Get();
					if ( !pLinked )
						return;

					angNewAngles = pTempPortal->m_hLinkedPortal->GetAbsAngles();
				}
				else
				{
					angNewAngles = m_vecAnglesToFace;
				}

				pTempPortal->PlacePortal( pTempPortal->GetAbsOrigin(), angNewAngles, PORTAL_ANALOG_SUCCESS_NO_BUMP );
			}
		}
	}
}
