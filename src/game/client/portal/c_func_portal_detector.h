//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which fires an output when a portal is placed in it.
//
// $NoKeywords: $
//======================================================================================//

#ifndef _FUNC_PORTAL_DETECTOR_H_
#define _FUNC_PORTAL_DETECTOR_H_

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

class C_FuncPortalDetector : public C_BaseEntity
{
public:
	DECLARE_CLASS(C_FuncPortalDetector, C_BaseEntity);

	// Overloads from base entity
	virtual void	Spawn( void );
	
	// misc public methods
	bool IsActive( void ) { return m_bActive; }	// is this area currently detecting portals
	bool m_bShouldUseLinkageID;
	int GetLinkageGroupID( void ) { return m_iLinkageGroupID; }

	DECLARE_CLIENTCLASS();
	DECLARE_DATADESC();

private:
	bool	m_bActive;			// are we currently detecting portals
	int		m_iLinkageGroupID;	// what set of portals are we testing for?
	int				m_spawnflags;
	
};

#endif
