#ifndef C_PROP_GLADOS_CORE_H
#define C_PROP_GLADOS_CORE_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "c_physicsprop.h"

class C_PropGladosCore : public C_PhysicsProp
{
public:
	DECLARE_CLASS( C_PropGladosCore, C_PhysicsProp );
	DECLARE_CLIENTCLASS()
	
	virtual QAngle	PreferredCarryAngles( void ) { return QAngle( 180, -90, 180 ); }
	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return true; }
};
#endif //C_PROP_GLADOS_CORE_H