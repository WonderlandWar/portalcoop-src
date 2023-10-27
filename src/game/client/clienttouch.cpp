#include "cbase.h"
#include "clienttouch.h"
#include "collisionutils.h"
#include "cliententitylist.h"

CUtlVector<CClientTouchable*> g_AllTouchables;

CON_COMMAND(getallclienttouchingentities, "")
{
	for (int i = 0; i < g_AllTouchables.Count(); ++i)
	{
		CClientTouchable *pTouchable = g_AllTouchables[i];
		
		if (pTouchable)
		{

			for (int i = 0; i < pTouchable->m_TouchingEntities.Count(); ++i)
			{
				if (!pTouchable->m_TouchingEntities[i])
					continue;

				Msg("touching ents %s\n", pTouchable->m_TouchingEntities[i]->GetClassname());

			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientTouchable::CClientTouchable()
{
	g_AllTouchables.AddToTail(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientTouchable::~CClientTouchable()
{
	g_AllTouchables.FindAndRemove(this);
}

//-----------------------------------------------------------------------------
// Purpose: Emulates touching the same way the server entities touch
//-----------------------------------------------------------------------------
void CClientTouchable::HandleFakeTouch( void )
{
	C_BaseEntity *pThisEntity = GetTouchableBaseEntity();

	Assert( pThisEntity );

	Vector vMins;
	Vector vMaxs;
	pThisEntity->CollisionProp()->WorldSpaceAABB( &vMins, &vMaxs );
		
	C_BaseEntity *pEntsInBounds[1024];

	int count = UTIL_EntitiesInBox( pEntsInBounds, 1024, vMins, vMaxs, 0, PARTITION_CLIENT_NON_STATIC_EDICTS );

	//int count = UTIL_EntitiesInSphere( pEntsInBounds, 1024, GetAbsOrigin(), m_flFakeTouchRadius, 0, PARTITION_CLIENT_NON_STATIC_EDICTS );
	
	// HACK: Manually add the local player because UTIL_EntitiesInBox doesn't add the local player!
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		if ( EntityIsInBounds( pLocalPlayer ) )
		{
			pEntsInBounds[count] = pLocalPlayer;
			++count;
		}
	}


	for( int i = 0; i < count; ++i )
	{
		C_BaseEntity *pEntity = pEntsInBounds[i];

		if (!pEntity)
		{
			//Warning("No entity, continue.\n");
			continue;
		}
		
		if ( pEntity == pThisEntity )
			continue; // Don't touch myself

		if ( !TouchCondition( pEntity ) )
			continue;

		if ( EntityIsInBounds( pEntity ) )
		{
			if ( !IsTouchingEntity( pEntity ) )
			{
				pThisEntity->StartTouch( pEntity );
				m_TouchingEntities.AddToTail( pEntity );
				//Msg("Start Touch %s\n", pEntity->GetClassname());
			}

			pThisEntity->Touch( pEntity );
			//Msg("Touch %s\n", pEntity->GetClassname());
		}
	}

	TestEndTouch();
}

void CClientTouchable::TestEndTouch( void )
{
	C_BaseEntity *pThisEntity = GetTouchableBaseEntity();

	for (int i = 0; i < m_TouchingEntities.Count(); ++i)
	{
		C_BaseEntity *pEntity = m_TouchingEntities[i].Get();
		
		if ( !pEntity )
		{
			m_TouchingEntities[i] = NULL;
			continue;
		}

		if ( !EntityIsInBounds( pEntity ) )
		{
			m_TouchingEntities.FindAndFastRemove( pEntity );
			pThisEntity->EndTouch( pEntity );
			//Msg("End Touch %s\n", pEntity->GetClassname());
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds physics objects to touch ( DON'T USE )
//-----------------------------------------------------------------------------
void CClientTouchable::HandleFakePhysicsTouch( void )
{
	C_BaseEntity *pThisEntity = GetTouchableBaseEntity();
	
	IPhysicsObject *pPhysObjects[1024];
	
	Vector vMins;
	Vector vMaxs;

	int count = UTIL_PhysicsObjectsInBox( pPhysObjects, 1024, vMins, vMaxs );
	
	for( int i = 0; i < count; ++i )
	{
		IPhysicsObject *pPhys = pPhysObjects[i];

		if (!pPhys)
		{
			//Warning("No entity, continue.\n");
			continue;
		}
		
		if ( pPhys == pThisEntity->VPhysicsGetObject() )
			continue; // Don't touch myself

		C_BaseEntity *pEntity = dynamic_cast<C_BaseEntity*>( pPhys );

		if ( !TouchCondition( pEntity ) )
			continue;

		if ( EntityIsInBounds( pEntity ) )
		{
			if ( !IsTouchingEntity( pEntity ) )
			{
				pThisEntity->StartTouch( pEntity );
				m_TouchingEntities.AddToTail( pEntity );
				//Msg("Start Touch %s\n", pEntity->GetClassname());
			}

			pThisEntity->Touch( pEntity );
			//Msg("Touch %s\n", pEntity->GetClassname());
		}
		else
		{
			if ( IsTouchingEntity( pEntity ) )
			{
				m_TouchingEntities.FindAndFastRemove( pEntity );
				pThisEntity->EndTouch( pEntity );
				//Msg("End Touch %s\n", pEntity->GetClassname());
			}
		}
	}

}

bool CClientTouchable::EntityIsInBounds( C_BaseEntity *pEntity )
{
	Vector vMins;
	Vector vMaxs;	
	GetTouchableBaseEntity()->CollisionProp()->WorldSpaceAABB( &vMins, &vMaxs );
	
	Vector vEntityMins;
	Vector vEntityMaxs;
	pEntity->CollisionProp()->WorldSpaceAABB( &vEntityMins, &vEntityMaxs );


	if ( IsBoxIntersectingBox( vMins, vMaxs, vEntityMins, vEntityMaxs ) )
	{
		return true;
	}

	return false;
}

bool CClientTouchable::IsTouchingEntity( C_BaseEntity *pEntity )
{
	return m_TouchingEntities.HasElement( pEntity );
}