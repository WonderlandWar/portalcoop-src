//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PORTAL_PHYSICS_COLLISIONEVENT_H
#define PORTAL_PHYSICS_COLLISIONEVENT_H

#ifdef _WIN32
#pragma once
#endif
#ifdef GAME_DLL
#include "physics_collisionevent.h"
#else
#include "physics.h"
#endif

class CPortal_CollisionEvent : public CCollisionEvent
{
public:
	DECLARE_CLASS_GAMEROOT( CPortal_CollisionEvent, CCollisionEvent );

	virtual int ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 );
	virtual void PreCollision( vcollisionevent_t *pEvent );
	virtual void PostCollision( vcollisionevent_t *pEvent );
	virtual int ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt );

	virtual void PostSimulationFrame( void );
	void PortalPostSimulationFrame( void );
#ifdef GAME_DLL
	void AddDamageEvent( CBaseEntity *pEntity, const CTakeDamageInfo &info, IPhysicsObject *pInflictorPhysics, bool bRestoreVelocity, const Vector &savedVel, const AngularImpulse &savedAngVel );
#endif
};

#endif //#ifndef PORTAL_PHYSICS_COLLISIONEVENT_H
