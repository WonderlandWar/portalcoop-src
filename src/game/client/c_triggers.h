//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef C_TRIGGERS_H
#define C_TRIGGERS_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basetoggle.h"
#include "triggers_shared.h"
#include "clienttouch.h"

class C_BaseTrigger : public C_BaseToggle, CClientTouchable
{
	DECLARE_CLASS( C_BaseTrigger, C_BaseToggle );
	DECLARE_CLIENTCLASS();
	DECLARE_TOUCHABLE();
public:

	C_BaseTrigger( void );

	virtual void Spawn();
	virtual void ClientThink();

	virtual bool TouchCondition( C_BaseEntity *pOther );
	
	virtual bool PassesTriggerFilters(C_BaseEntity *pOther);

	bool m_bDisabled;

public:

	bool	m_bClientSidePredicted;
};

class C_BaseVPhysicsTrigger : public C_BaseTrigger
{
	DECLARE_CLASS( C_BaseVPhysicsTrigger , C_BaseTrigger );
	DECLARE_CLIENTCLASS();

public:
	
	//virtual bool PassesTriggerFilters(C_BaseEntity *pOther);

protected:
	string_t					m_iFilterName;
	//CHandle<class C_BaseFilter>	m_hFilter; //CBaseFilter is not networked yet. Only really care about m_bDisabled for this first pass.
};

#endif // C_TRIGGERS_H
