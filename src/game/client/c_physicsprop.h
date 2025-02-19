//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_PHYSICSPROP_H
#define C_PHYSICSPROP_H
#ifdef _WIN32
#pragma once
#endif

#include "c_breakableprop.h"
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_PhysicsProp : public C_BreakableProp
{
	typedef C_BreakableProp BaseClass;
public:
	DECLARE_CLIENTCLASS();

	C_PhysicsProp();
	~C_PhysicsProp();

	virtual bool OnInternalDrawModel( ClientModelRenderInfo_t *pInfo );
	bool GetPropDataAngles( const char *pKeyName, QAngle &vecAngles );
	float GetCarryDistanceOffset( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void			SetInteraction( propdata_interactions_t Interaction ) { m_iInteractions |= (1 << Interaction); }
	void			RemoveInteraction( propdata_interactions_t Interaction ) { m_iInteractions &= ~(1 << Interaction); }
	bool			HasInteraction( propdata_interactions_t Interaction ) { return ( m_iInteractions & (1 << Interaction) ) != 0; }

protected:
	// Networked vars.
	bool m_bAwake;
	bool m_bAwakeLastTime;

	int				m_iInteractions;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_PhysicsPropRespawnable : public C_PhysicsProp
{
	typedef C_PhysicsProp BaseClass;
public:
	DECLARE_CLIENTCLASS();
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_PhysicsPropOverride : public C_PhysicsProp
{
	typedef C_PhysicsProp BaseClass;
public:
	DECLARE_CLIENTCLASS();
};

#endif // C_PHYSICSPROP_H 
