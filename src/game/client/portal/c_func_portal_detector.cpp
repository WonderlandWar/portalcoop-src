//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume in which no portal can be placed. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals wholly or partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "c_func_portal_detector.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_util_shared.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Spawnflags
#define SF_START_INACTIVE			0x01


LINK_ENTITY_TO_CLASS( func_portal_detector, C_FuncPortalDetector );

BEGIN_DATADESC( C_FuncPortalDetector )

	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),
	
END_DATADESC()

IMPLEMENT_CLIENTCLASS_DT(C_FuncPortalDetector, DT_FuncPortalDetector, CFuncPortalDetector)

	RecvPropBool(RECVINFO(m_bActive)),
	RecvPropBool(RECVINFO(m_bShouldUseLinkageID)),
	RecvPropInt(RECVINFO(m_iLinkageGroupID)),
	RecvPropInt(RECVINFO(m_spawnflags)),

END_NETWORK_TABLE()

void C_FuncPortalDetector::Spawn()
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
}