//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICSCLONEAREA_H
#define PHYSICSCLONEAREA_H

#ifdef _WIN32
#pragma once
#endif

#include "baseentity_shared.h"
#ifdef CLIENT_DLL
#include "c_prop_portal.h"

//#define CPhysicsCloneArea C_PhysicsCloneArea
#endif
#ifdef GAME_DLL
class CProp_Portal;
#else
class C_Prop_Portal;
#endif
class CPortalSimulator;

#ifdef GAME_DLL
class CPhysicsCloneArea : public CBaseEntity
#else
class CPhysicsCloneArea : public C_BaseEntity
#endif
{
public:
	DECLARE_CLASS( CPhysicsCloneArea, CBaseEntity );
		
#ifdef CLIENT_DLL
	CPhysicsCloneArea();
	~CPhysicsCloneArea();

#endif

	static const Vector		vLocalMins;
	static const Vector		vLocalMaxs;

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );

	virtual void			Spawn( void );
	virtual void			Activate( void );

	virtual int				ObjectCaps( void );
	void					UpdatePosition( void );

	void					CloneTouchingEntities( void );
	void					CloneNearbyEntities( void );
#ifdef GAME_DLL
	static CPhysicsCloneArea *CreatePhysicsCloneArea( CProp_Portal *pFollowPortal );	
#else
	static CPhysicsCloneArea *CreatePhysicsCloneArea( C_Prop_Portal *pFollowPortal );	
#endif
private:

	CProp_Portal			*m_pAttachedPortal;
	CHandle<CProp_Portal>			m_hAttachedPortal;

	CPortalSimulator		*m_pAttachedSimulator;
	bool					m_bActive;


};

#endif //#ifndef PHYSICSCLONEAREA_H

