#ifndef PROP_BOX_H
#define PROP_BOX_H

#include "props.h"

class CTriggerBoxReflector;

class CPropBox : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropBox, CPhysicsProp );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPropBox();

	void Spawn( void );
	void Precache( void );

	void EnergyBallHit( CBaseEntity *pBall );
	void PreDissolve( CBaseEntity *pActivator, CBaseEntity *pCaller );

	CHandle<CTriggerBoxReflector> m_hAttached;

private:

	void InputDissolve( inputdata_t &inputdata );

	COutputEvent m_OnDissolved;
};

#endif // PROP_BOX_H