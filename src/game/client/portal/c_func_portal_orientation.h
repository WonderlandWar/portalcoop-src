//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Volume entity which overrides the placement angles of a portal placed within its bounds.
//
// $NoKeywords: $
//======================================================================================//

#ifndef _FUNC_PORTAL_ORIENTATION_H_
#define _FUNC_PORTAL_ORIENTATION_H_

#define CFuncPortalOrientation C_FuncPortalOrientation

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

class C_FuncPortalOrientation : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncPortalOrientation, C_BaseEntity );
	DECLARE_DATADESC();
	DECLARE_CLIENTCLASS();

	C_FuncPortalOrientation();
	~C_FuncPortalOrientation();

	// Overloads from base entity
	virtual void	Spawn( void );
	
	void			OnActivate ( void );
	
	bool IsActive() { return !m_bDisabled; }	// is this area causing portals to lock orientation

	bool					m_bMatchLinkedAngles;
	QAngle					m_vecAnglesToFace;

	C_FuncPortalOrientation		*m_pNext;			// Needed for the template list
	unsigned int				m_iListIndex;

	bool					m_bDisabled;				// are we currently locking portal orientations
};

C_FuncPortalOrientation* GetPortalOrientationVolumeList();

// Upon portal placement, test for orientation changing volumes
bool UTIL_TestForOrientationVolumes( QAngle& vecCurAngles, const Vector& vecCurOrigin, const CProp_Portal* pPortal );

#endif //_FUNC_PORTAL_ORIENTATION_H_