//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: insulates client DLL from dependencies on vphysics
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_H
#define PHYSICS_H
#ifdef _WIN32
#pragma once
#endif


#include "interface.h"
#include "physics_shared.h"
#include "positionwatcher.h"

class CCollisionEvent : public IPhysicsCollisionEvent, public IPhysicsCollisionSolver, public IPhysicsObjectEvent
{
public:
	CCollisionEvent( void );

	void	ObjectSound( int index, vcollisionevent_t *pEvent );
	void	PreCollision( vcollisionevent_t *pEvent ) {}
	void	PostCollision( vcollisionevent_t *pEvent );
	void	Friction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit, IPhysicsCollisionData *pData );

	void	BufferTouchEvents( bool enable ) { m_bBufferTouchEvents = enable; }

	void	StartTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData );
	void	EndTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData );

	void	FluidStartTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid );
	void	FluidEndTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid );
	void	PostSimulationFrame() {}

	virtual void ObjectEnterTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject ) {}
	virtual void ObjectLeaveTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject ) {}

	float	DeltaTimeSinceLastFluid( CBaseEntity *pEntity );
	void	FrameUpdate( void );

	void	UpdateFluidEvents( void );
	void	UpdateTouchEvents( void );

	// IPhysicsCollisionSolver
	int		ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 );
#if _DEBUG
	int		ShouldCollide_2( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 );
#endif
	// debugging collision problem in TF2
	int		ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt );
	bool	ShouldFreezeObject( IPhysicsObject *pObject ) { return true; }
	int		AdditionalCollisionChecksThisTick( int currentChecksDone ) { return 0; }
	bool ShouldFreezeContacts( IPhysicsObject **pObjectList, int objectCount )  { return true; }

	// IPhysicsObjectEvent
	virtual void ObjectWake( IPhysicsObject *pObject )
	{
		C_BaseEntity *pEntity = static_cast<C_BaseEntity *>(pObject->GetGameData());
		if (pEntity && pEntity->HasDataObjectType(VPHYSICSWATCHER))
		{
			ReportVPhysicsStateChanged( pObject, pEntity, true );
		}
	}

	virtual void ObjectSleep( IPhysicsObject *pObject )
	{
		C_BaseEntity *pEntity = static_cast<C_BaseEntity *>(pObject->GetGameData());
		if ( pEntity && pEntity->HasDataObjectType( VPHYSICSWATCHER ) )
		{
			ReportVPhysicsStateChanged( pObject, pEntity, false );
		}
	}


	friction_t *FindFriction( CBaseEntity *pObject );
	void ShutdownFriction( friction_t &friction );
	void UpdateFrictionSounds();
	bool IsInCallback() { return m_inCallback > 0 ? true : false; }

private:
	class CallbackContext
	{
	public:
		CallbackContext(CCollisionEvent *pOuter)
		{
			m_pOuter = pOuter;
			m_pOuter->m_inCallback++;
		}
		~CallbackContext()
		{
			m_pOuter->m_inCallback--;
		}
	private:
		CCollisionEvent *m_pOuter;
	};
	friend class CallbackContext;
	
	void	AddTouchEvent( C_BaseEntity *pEntity0, C_BaseEntity *pEntity1, int touchType, const Vector &point, const Vector &normal );
	void	DispatchStartTouch( C_BaseEntity *pEntity0, C_BaseEntity *pEntity1, const Vector &point, const Vector &normal );
	void	DispatchEndTouch( C_BaseEntity *pEntity0, C_BaseEntity *pEntity1 );

	friction_t					m_current[8];
	CUtlVector<fluidevent_t>	m_fluidEvents;
	CUtlVector<touchevent_t>	m_touchEvents;
	int							m_inCallback;
	bool						m_bBufferTouchEvents;
};

struct objectparams_t;
struct solid_t;

// HACKHACK: Make this part of IClientSystem somehow???
extern bool PhysicsDLLInit( CreateInterfaceFn physicsFactory );
extern void PhysicsReset();
extern void PhysicsSimulate();
extern void PortalPhysicsSimulate();
extern float PhysGetSyncCreateTime();

#endif // PHYSICS_H
