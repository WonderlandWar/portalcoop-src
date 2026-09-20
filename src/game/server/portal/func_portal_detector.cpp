//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume in which no portal can be placed. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals wholly or partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "func_portal_detector.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_util_shared.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CEntityClassList<CFuncPortalDetector> g_FuncPortalDetectorList;
template <> CFuncPortalDetector *CEntityClassList<CFuncPortalDetector>::m_pClassList = NULL;

CFuncPortalDetector* GetPortalDetectorList()
{
	return g_FuncPortalDetectorList.m_pClassList;
}

// Spawnflags
#define SF_START_INACTIVE			0x01


LINK_ENTITY_TO_CLASS( func_portal_detector, CFuncPortalDetector );

BEGIN_DATADESC( CFuncPortalDetector )

	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_iLinkageGroupID, FIELD_INTEGER, "LinkageGroupID" ),
	DEFINE_KEYFIELD( m_bShouldUseLinkageID, FIELD_BOOLEAN, "ShouldUseLinkageID" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	
	DEFINE_OUTPUT( m_OnStartTouchPortal, "OnStartTouchPortal" ),
	DEFINE_OUTPUT( m_OnStartTouchPortal1, "OnStartTouchPortal1" ),
	DEFINE_OUTPUT( m_OnStartTouchPortal2, "OnStartTouchPortal2" ),
	DEFINE_OUTPUT( m_OnStartTouchLinkedPortal, "OnStartTouchLinkedPortal" ),
	DEFINE_OUTPUT( m_OnStartTouchBothLinkedPortals, "OnStartTouchBothLinkedPortals" ),
	DEFINE_OUTPUT( m_OnEndTouchPortal, "OnEndTouchPortal" ),
	DEFINE_OUTPUT( m_OnEndTouchPortal1, "OnEndTouchPortal1" ),
	DEFINE_OUTPUT( m_OnEndTouchPortal2, "OnEndTouchPortal2" ),
	DEFINE_OUTPUT( m_OnEndTouchLinkedPortal, "OnEndTouchLinkedPortal" ),
	DEFINE_OUTPUT( m_OnEndTouchBothLinkedPortals, "OnEndTouchBothLinkedPortals" ),

	DEFINE_FUNCTION( IsActive ),

END_DATADESC()

CFuncPortalDetector::CFuncPortalDetector()
{
	m_bActive = false;
	m_iLinkageGroupID = 0;
	for ( int i = 0; i < 255; ++i )
	{
		m_phTouchingPortals[i][0] = NULL;
		m_phTouchingPortals[i][1] = NULL;
	}

	m_iTouchingPortalCount = 0;
	m_bShouldUseLinkageID = false;
	g_FuncPortalDetectorList.Insert( this );
}

CFuncPortalDetector::~CFuncPortalDetector()
{
	g_FuncPortalDetectorList.Remove( this );
}

void CFuncPortalDetector::Spawn()
{
	BaseClass::Spawn();

	if ( m_spawnflags & SF_START_INACTIVE )
	{
		m_bActive = false;
	}
	else
	{
		m_bActive = true;
	}

	// Bind to our model, cause we need the extents for bounds checking
	SetModel( STRING( GetModelName() ) );
	SetRenderMode( kRenderNone );	// Don't draw
	SetSolid( SOLID_VPHYSICS );		// we may want slanted walls, so we'll use OBB
	AddSolidFlags( FSOLID_NOT_SOLID );
}

void CFuncPortalDetector::SetActive( bool bActive )
{
	m_bActive = bActive;

	bool bTouchedPortal1 = false;
	bool bTouchedPortal2 = false;

	COutputEvent *pOutputTouchPortal;
	COutputEvent *pOutputTouchPortal1;
	COutputEvent *pOutputTouchPortal2;
	COutputEvent *pOutputTouchLinkedPortal;
	COutputEvent *pOutputTouchBothLinkedPortals;

	if ( bActive )
	{
		pOutputTouchPortal = &m_OnStartTouchPortal;
		pOutputTouchPortal2 = &m_OnStartTouchPortal2;
		pOutputTouchPortal1 = &m_OnStartTouchPortal1;
		pOutputTouchLinkedPortal = &m_OnStartTouchLinkedPortal;
		pOutputTouchBothLinkedPortals = &m_OnStartTouchBothLinkedPortals;
	}
	else
	{
		pOutputTouchPortal = &m_OnEndTouchPortal;
		pOutputTouchPortal2 = &m_OnEndTouchPortal2;
		pOutputTouchPortal1 = &m_OnEndTouchPortal1;
		pOutputTouchLinkedPortal = &m_OnEndTouchLinkedPortal;
		pOutputTouchBothLinkedPortals = &m_OnEndTouchBothLinkedPortals;
	}

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];

			//require that it's active and/or linked?

			if( pTempPortal->GetLinkageGroup() == m_iLinkageGroupID && IsPortalInDetectorSpace( pTempPortal ) )
			{
				pOutputTouchPortal->FireOutput(pTempPortal, this);
				if( pTempPortal->IsPortal2() )
				{
					pOutputTouchPortal2->FireOutput( pTempPortal, this );

					if ( pTempPortal->IsActivedAndLinked() )
					{
						bTouchedPortal2 = true;
						pOutputTouchLinkedPortal->FireOutput( pTempPortal, this );
					}
				}
				else
				{
					pOutputTouchPortal1->FireOutput( pTempPortal, this );

					if ( pTempPortal->IsActivedAndLinked() )
					{
						bTouchedPortal1 = true;
						pOutputTouchLinkedPortal->FireOutput( pTempPortal, this );
					}
				}
			}
		}
	}

	if ( bTouchedPortal1 && bTouchedPortal2 )
	{
		pOutputTouchBothLinkedPortals->FireOutput( this, this );
	}
}

void CFuncPortalDetector::InputDisable( inputdata_t &inputdata )
{
	SetActive(false);
}

void CFuncPortalDetector::InputEnable( inputdata_t &inputdata )
{
	SetActive(true);
}

void CFuncPortalDetector::InputToggle( inputdata_t &inputdata )
{
	m_bActive = !m_bActive;

	SetActive(m_bActive);
}

void CFuncPortalDetector::NotifyPortalEvent( PortalEvent_t nEventType, CProp_Portal *pNotifier )
{
	if (nEventType == PORTALEVENT_FIZZLE)
	{
		CProp_Portal *pPropPortal = dynamic_cast<CProp_Portal*>( pNotifier );

		UpdateOnPortalMoved( pPropPortal );
	}
}

void CFuncPortalDetector::UpdateOnPortalMoved( CProp_Portal *pPortal )
{
	if ( m_bActive )
	{
		m_iLinkageGroupID = pPortal->GetLinkageGroup();

		bool bWasTouchingPortalDetector = IsPortalTouchingDetector( pPortal );

		bool bIsTouchingPortalDetector = true;
		if ( GetLinkageGroupID() != pPortal->GetLinkageGroup() && m_bShouldUseLinkageID )
		{
			bIsTouchingPortalDetector = false;
		}
		else
		{
			if ( !IsPortalInDetectorSpace( pPortal ) )
			{
				//Msg( "Portal %i (%i) is not in the detector space\n", pPortal->m_bIsPortal2 ? 2 : 1, pPortal->GetLinkageGroup() );
				bIsTouchingPortalDetector = false;
			}
		}

		if ( ( bWasTouchingPortalDetector && !pPortal->IsActive() ) 
			|| ( bWasTouchingPortalDetector && !bIsTouchingPortalDetector ) )
		{
			m_phTouchingPortals[pPortal->GetLinkageGroup()][pPortal->m_bIsPortal2] = NULL;
			
			--m_iTouchingPortalCount;
			PortalRemovedFromInsideBounds( pPortal );
		}

		if ( bIsTouchingPortalDetector && !bWasTouchingPortalDetector )
		{
			m_phTouchingPortals[pPortal->GetLinkageGroup()][pPortal->m_bIsPortal2] = pPortal;
			++m_iTouchingPortalCount;
			PortalPlacedInsideBounds( pPortal );
		}
	}
}

void CFuncPortalDetector::PortalPlacedInsideBounds( CProp_Portal *pPortal )
{
	m_OnStartTouchPortal.FireOutput( pPortal, this );
	if ( pPortal->m_bIsPortal2 )
	{
		m_OnStartTouchPortal2.FireOutput( pPortal, this );
	}
	else
	{
		m_OnStartTouchPortal1.FireOutput( pPortal, this );
	}
	
	if ( pPortal->IsActivedAndLinked() )
	{
		m_OnStartTouchLinkedPortal.FireOutput( pPortal, this );
		if ( m_iTouchingPortalCount == 2 )
		{
			m_OnStartTouchBothLinkedPortals.FireOutput( pPortal, this );
		}
	}
	else
	{
		pPortal->AddPortalEventListener( this );
	}
}

bool CFuncPortalDetector::IsPortalInDetectorSpace( CProp_Portal *pPortal )
{
	// PCOOP: It would be a lot more ideal to check if the actual brushwork is being touched rather than using bounds
#if 1
	Vector vMin, vMax;
	CollisionProp()->WorldSpaceAABB( &vMin, &vMax );
	Vector vBoxCenter = ( vMax + vMin ) * 0.5;
	Vector vBoxExtents = ( vMax - vMin ) * 0.5;

	return UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, pPortal );
#else
	trace_t tr;

	const Vector &vecCollisionOrigin = CollisionProp()->GetCollisionOrigin();
	const QAngle &vecCollisionAngle = CollisionProp()->GetCollisionAngles();

	const Vector &vecPortalOrigin = pPortal->CollisionProp()->GetCollisionOrigin();
	const QAngle &vecPortalAngle = pPortal->CollisionProp()->GetCollisionAngles();

	vcollide_t *vcollide = modelinfo->GetVCollide( GetModelIndex() );
	for ( int i = 0; i < vcollide->solidCount; ++i )
	{
		physcollision->TraceCollide( vecCollisionOrigin, vecCollisionOrigin, vcollide->solids[i], vecCollisionAngle, pPortal->GetCollisionShape(), vecPortalOrigin, vecPortalAngle, &tr );
	}

	return tr.DidHitNonWorldEntity();
#endif
}

bool CFuncPortalDetector::IsPortalTouchingDetector( const CProp_Portal *pPortal )
{
	if ( !pPortal )
		return false;

	for ( int i = 0; i < 2; i++ )
	{
		if ( m_phTouchingPortals[pPortal->GetLinkageGroup()][i].Get() == pPortal )
			return true;
	}

	return false;
}

void CFuncPortalDetector::UpdateOnPortalActivated( CProp_Portal *pPortal )
{
	if ( IsPortalTouchingDetector( pPortal ) )
	{
		m_OnStartTouchLinkedPortal.FireOutput( pPortal, this );
		if ( m_iTouchingPortalCount == 2 )
			m_OnStartTouchBothLinkedPortals.FireOutput( pPortal, this );
	}
}

void CFuncPortalDetector::UpdateOnPortalDeactivated( CProp_Portal *pPortal )
{
	if ( IsPortalTouchingDetector( pPortal ) )
	{
		PortalRemovedFromInsideBounds( pPortal );
	}
}

void CFuncPortalDetector::PortalRemovedFromInsideBounds( CProp_Portal *pPortal )
{
	m_OnEndTouchPortal.FireOutput( pPortal, this );
	if ( pPortal->m_bIsPortal2 )
	{
		m_OnEndTouchPortal2.FireOutput( pPortal, this );
	}
	else
	{
		m_OnEndTouchPortal1.FireOutput( pPortal, this );
	}
	
	if ( pPortal->IsActivedAndLinked() )
	{
		m_OnEndTouchLinkedPortal.FireOutput( pPortal, this );
		if ( m_iTouchingPortalCount == 0 )
			m_OnEndTouchBothLinkedPortals.FireOutput( pPortal, this );
	}
	else
	{
		pPortal->RemovePortalEventListener( this );
	}
}