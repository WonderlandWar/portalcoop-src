#include "cbase.h"
#include "projectedwallentity.h"
#ifdef PORTAL_PAINT
#include "paint/paint_database.h"
#endif
#include "vcollide_parse.h"
#include "portal_player.h"
#include "physicsshadowclone.h"
#include "mathlib/polyhedron.h"
#include "props.h"
#include "collisionutils.h"

#ifndef NO_PROJECTED_WALL

// TODO: Not sure where these should go
#define PROJECTED_WALL_WIDTH 64.0f
#define PROJECTED_WALL_HEIGHT 0.015625 // 1/64 - thickness of the bridge

int CProjectedWallEntity::s_HardLightBridgeSurfaceProps = -1;

extern ConVar sv_thinnerprojectedwalls;

IMPLEMENT_AUTO_LIST( IProjectedWallEntityAutoList )

BEGIN_DATADESC( CProjectedWallEntity )

	DEFINE_FIELD( m_vWorldSpace_WallMins, FIELD_VECTOR ),
	DEFINE_FIELD( m_vWorldSpace_WallMaxs, FIELD_VECTOR ),

	DEFINE_FIELD( m_hColorPortal, FIELD_EHANDLE ),
	
	DEFINE_FIELD( m_flLength, FIELD_FLOAT ),
	DEFINE_FIELD( m_flHeight, FIELD_FLOAT ),
	DEFINE_FIELD( m_flWidth, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSegmentLength, FIELD_FLOAT ),
	DEFINE_FIELD( m_flParticleUpdateTime, FIELD_FLOAT ),
	
	DEFINE_FIELD( m_bIsHorizontal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nNumSegments, FIELD_INTEGER ),
	
	DEFINE_FIELD( m_bWasEverProjected, FIELD_BOOLEAN ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CProjectedWallEntity, DT_ProjectedWallEntity )
	SendPropFloat( SENDINFO( m_flLength ) ),
	SendPropFloat( SENDINFO( m_flHeight ) ),
	SendPropFloat( SENDINFO( m_flWidth ) ),
	SendPropFloat( SENDINFO( m_flSegmentLength ) ),
	SendPropFloat( SENDINFO( m_flParticleUpdateTime ) ),
	
	SendPropBool( SENDINFO( m_bIsHorizontal ) ),
	SendPropInt( SENDINFO( m_nNumSegments ) ),

	SendPropVector( SENDINFO( m_vWorldSpace_WallMins ) ),
	SendPropVector( SENDINFO( m_vWorldSpace_WallMaxs ) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( projected_wall_entity, CProjectedWallEntity )

CProjectedWallEntity::CProjectedWallEntity()
{
	m_bWasEverProjected = false;
}

CProjectedWallEntity::~CProjectedWallEntity()
{
	CleanupWall();
#ifdef PORTAL_PAINT
	PaintDatabase.RemovePaintedWall( this );
#endif
}

void CProjectedWallEntity::Spawn( void )
{
	BaseClass::Spawn();
	Precache();
	CollisionProp()->SetSolid( SOLID_CUSTOM );
	CollisionProp()->SetSolidFlags( FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST | FSOLID_FORCE_WORLD_ALIGNED );
	CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );
	SetMoveType(MOVETYPE_NONE, MOVECOLLIDE_DEFAULT );
	CreateVPhysics();
	SetTransmitState( FL_EDICT_ALWAYS );
	if (CProjectedWallEntity::s_HardLightBridgeSurfaceProps == -1)
		CProjectedWallEntity::s_HardLightBridgeSurfaceProps = physprops->GetSurfaceIndex("hard_light_bridge");
}

void CProjectedWallEntity::Precache( void )
{
	PrecacheParticleSystem("projected_wall_impact");
}

void CProjectedWallEntity::OnRestore( void )
{
	BaseClass::OnRestore();
	SetTransmitState( FL_EDICT_ALWAYS );
}

void CProjectedWallEntity::UpdateOnRemove( void )
{
	CheckForPlayersOnBridge();
	CheckForSettledReflectorCubes();
	CleanupWall();
	WakeNearbyProps();
	BaseClass::UpdateOnRemove();
}

void CProjectedWallEntity::NotifyPortalEvent( PortalEvent_t nEventType, CProp_Portal *pNotifier )
{
	if (nEventType == PORTALEVENT_LINKED)
	{
		pNotifier->RemovePortalEventListener( this );
	}
}

void CProjectedWallEntity::SetHitPortal( CProp_Portal *pPortal )
{
	if ( pPortal )
	{
		if( !DidRedirectionPortalMove( pPortal ) )
			goto LABEL_11;
	}
	else if ( !GetHitPortal() )
	{
		goto LABEL_11;
	}
	m_flParticleUpdateTime = gpGlobals->curtime + 0.5;

LABEL_11:
	BaseClass::SetHitPortal( pPortal );
}
void CProjectedWallEntity::SetSourcePortal( CProp_Portal *pPortal)
{
	m_flParticleUpdateTime = gpGlobals->curtime + 0.5;
	m_hColorPortal = pPortal;

	BaseClass::SetSourcePortal( pPortal );
}

void CProjectedWallEntity::GetProjectionExtents( Vector &outMins, Vector &outMaxs )
{
	GetExtents( outMins, outMaxs, 0.5 );
}

bool CProjectedWallEntity::ShouldSavePhysics( void )
{
	return false;
}

bool CProjectedWallEntity::CreateVPhysics( void )
{
	ProjectWall();
	return true;
}

void CProjectedWallEntity::ProjectWall( void )
{
	CleanupWall();
	AddEffects( EF_NOINTERP );
	CheckForPlayersOnBridge();

	const Vector vStartPoint = GetStartPoint();
	const Vector vEndPoint = GetEndPoint();

	Vector vecForward;
	Vector vecRight;
	Vector vecUp;
	GetVectors( &vecForward, &vecRight, &vecUp );

	CPhysConvex *pTempConvex;

	if (sv_thinnerprojectedwalls.GetInt())
	{
		// Generates an infinitely thin light bridge out of 4 vertices

		Vector vBackRight = vStartPoint + (vecRight * (PROJECTED_WALL_WIDTH / 2));
		Vector vBackLeft = vStartPoint - (vecRight * (PROJECTED_WALL_WIDTH / 2));
		Vector vFrontRight = vEndPoint + (vecRight * (PROJECTED_WALL_WIDTH / 2));
		Vector vFrontLeft = vEndPoint - (vecRight * (PROJECTED_WALL_WIDTH / 2));

		Vector *vVerts[4];

		vVerts[0] = &vBackRight;
		vVerts[1] = &vBackLeft;
		vVerts[2] = &vFrontRight;
		vVerts[3] = &vFrontLeft;
		
		pTempConvex = physcollision->ConvexFromVerts( vVerts, 4 );
	}
	else
	{
		// Generates a 1/64 unit thick light bridge out of 6 planes
		// Based on how Valve uses GeneratePolyhedronFromPlanes elsewhere they probably just did everything in-line like this

		float fPlanes[6 * 4];

		// Forward plane
		fPlanes[(0 * 4) + 0] = vecForward.x;
		fPlanes[(0 * 4) + 1] = vecForward.y;
		fPlanes[(0 * 4) + 2] = vecForward.z;
		fPlanes[(0 * 4) + 3] = vecForward.Dot(vEndPoint);

		// Back plane
		fPlanes[(1 * 4) + 0] = -vecForward.x;
		fPlanes[(1 * 4) + 1] = -vecForward.y;
		fPlanes[(1 * 4) + 2] = -vecForward.z;
		fPlanes[(1 * 4) + 3] = -vecForward.Dot(vStartPoint);

		// Right plane
		fPlanes[(4 * 4) + 0] = vecRight.x;
		fPlanes[(4 * 4) + 1] = vecRight.y;
		fPlanes[(4 * 4) + 2] = vecRight.z;
		fPlanes[(4 * 4) + 3] = vecRight.Dot( vStartPoint + (vecRight * PROJECTED_WALL_WIDTH / 2) );

		// Left plane
		fPlanes[(5 * 4) + 0] = -vecRight.x;
		fPlanes[(5 * 4) + 1] = -vecRight.y;
		fPlanes[(5 * 4) + 2] = -vecRight.z;
		fPlanes[(5 * 4) + 3] = -vecRight.Dot( vStartPoint - (vecRight * PROJECTED_WALL_WIDTH / 2) );

		// Up plane
		fPlanes[(2 * 4) + 0] = vecUp.x;
		fPlanes[(2 * 4) + 1] = vecUp.y;
		fPlanes[(2 * 4) + 2] = vecUp.z;
		fPlanes[(2 * 4) + 3] = vecUp.Dot( vStartPoint + (vecUp * PROJECTED_WALL_HEIGHT / 2) );

		// Down plane
		fPlanes[(3 * 4) + 0] = -vecUp.x;
		fPlanes[(3 * 4) + 1] = -vecUp.y;
		fPlanes[(3 * 4) + 2] = -vecUp.z;
		fPlanes[(3 * 4) + 3] = -vecUp.Dot( vStartPoint - (vecUp * PROJECTED_WALL_HEIGHT / 2) );

		CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, 0.0 );
		if (!pPolyhedron)
		{
			Warning( "CProjectedWallEntity: GeneratePolyhedronFromPlanes failed! Get a save game for me!.\n" );
			return;
		}
		pTempConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
		pPolyhedron->Release();
	}

	if (!pTempConvex)
		return;

	m_pWallCollideable = physcollision->ConvertConvexToCollide( &pTempConvex, 1 );
	if (m_pWallCollideable)
	{
		solid_t solid;
		V_strncpy( solid.surfaceprop, "hard_light_bridge", sizeof( solid.surfaceprop ) );
		solid.params.massCenterOverride = g_PhysDefaultObjectParams.massCenterOverride;
		solid.params.pGameData = this;
		solid.params.mass = g_PhysDefaultObjectParams.mass;
		solid.params.inertia = g_PhysDefaultObjectParams.inertia;
		solid.params.damping = g_PhysDefaultObjectParams.damping;
		solid.params.rotdamping = g_PhysDefaultObjectParams.rotdamping;
		solid.params.rotInertiaLimit = g_PhysDefaultObjectParams.rotInertiaLimit;
		solid.params.pName = g_PhysDefaultObjectParams.pName;
		solid.params.volume = g_PhysDefaultObjectParams.volume;
		solid.params.dragCoefficient = g_PhysDefaultObjectParams.dragCoefficient;
		solid.params.enableCollisions = g_PhysDefaultObjectParams.enableCollisions;

		// create physics object
		IPhysicsObject *physModel = PhysModelCreateCustom( this, m_pWallCollideable, vec3_origin, vec3_angle, "hard_light_bridge", true, &solid );
		if (physModel)
		{
			if ( VPhysicsGetObject() )
				VPhysicsDestroyObject();
			VPhysicsSetObject( physModel );
			physModel->RecheckContactPoints();

			if ( physModel->GetCollide() )
			{
				Vector vMaxs = vec3_origin;
				Vector vMins = vec3_origin;
				physcollision->CollideGetAABB(&vMins, &vMaxs, physModel->GetCollide(), vec3_origin, vec3_angle);
				m_vWorldSpace_WallMins = vMins;
				m_vWorldSpace_WallMaxs = vMaxs;

				// set entity size
				Vector vLocalMins = vMins - vStartPoint;
				Vector vLocalMaxs = vMaxs - vStartPoint;
				SetSize( vLocalMins, vLocalMaxs );

				// Unsure if they actually used this function or not...original decompiled code below
				m_flLength = vStartPoint.DistTo(vEndPoint);
				//m_flLength = sqrt(
				//	(((vStartPoint.x - vEndPoint.x) * (vStartPoint.x - vEndPoint.x))
				//	+ ((vStartPoint.y - vEndPoint.y) * (vStartPoint.y - vEndPoint.y)))
				//	+ ((vStartPoint.z - vEndPoint.z) * (vStartPoint.z - vEndPoint.z)));

				// How useless.
				m_flWidth = PROJECTED_WALL_WIDTH;
				m_flHeight = PROJECTED_WALL_HEIGHT;

				CollisionProp()->MarkSurroundingBoundsDirty();
				CollisionProp()->MarkPartitionHandleDirty();
				CollisionProp()->UpdatePartition();

				Vector vRight;
				Vector vUp;
				AngleVectors( GetAbsAngles(), NULL, &vRight, &vUp);
				m_bIsHorizontal = (vUp.z > 0.7 || vUp.z < -0.7) && vRight.z > -0.7 && vRight.z < 0.7;
				DisplaceObstructingEntities();

				m_nNumSegments = ceil( ( m_flLength / m_flSegmentLength ) );
#ifdef PORTAL_PAINT
				// FIXME
				//m_PaintPowers.SetCount( ceil( ( m_flLength / m_flSegmentLength ) ) );
				CleansePaint();
#endif
			}
		}
	}
}

void CProjectedWallEntity::CheckForPlayersOnBridge( void )
{
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CPortal_Player *pPlayer = (CPortal_Player *)UTIL_PlayerByIndex(i);
		if (pPlayer && pPlayer->GetGroundEntity() == this)
		{
			SetGroundEntity( NULL );
			//pPlayer->BridgeRemovedFromUnder();
		}
	}
}

bool CProjectedWallEntity::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if ( !m_pWallCollideable )
		return false;

	physcollision->TraceBox( ray, fContentsMask, 0, m_pWallCollideable, vec3_origin, vec3_angle, &tr );
	
	tr.surface.name = "hard_light_bridge";
	tr.surface.flags = 0;
	tr.surface.surfaceProps = CProjectedWallEntity::s_HardLightBridgeSurfaceProps;
	if ( !(tr.fraction >= 1.0) || tr.allsolid || tr.startsolid )
		return true;

	return false;
}

void CProjectedWallEntity::WakeNearbyProps( void )
{
	//CBaseEntity *pList[64];
	//memset( pList, 0, sizeof( pList ) );
	Vector right, up;
	CProp_Portal *pPortal = GetSourcePortal();
	CBaseProjectedEntity *pOwner = dynamic_cast<CBaseProjectedEntity *>( GetOwnerEntity() );
	if ( pPortal && ( pOwner && pOwner->GetHitPortal() ) )
	{
		//pPortal->GetVectors( NULL, &right, &up );
		AngleVectors( pPortal->GetLastAngles(), NULL, &right, &up );
	}
	else
	{
		GetVectors( NULL, &right, &up );
	}
	
	Vector vMins = m_vecStartPoint + ( 40 * -right ) + ( 12 * -up );
	Vector vMaxs = m_vecEndPoint + ( 40 * right ) + ( 12 * up );
	//NDebugOverlay::Box( vec3_origin, vMins, vMaxs, 255, 0, 0, 192, 5 );
	
	//int count = UTIL_EntitiesInBox( pList, 64, vMins, vMaxs, MASK_SOLID );

	int count = IBasePropAutoList::AutoList().Count();

	for ( int i = 0; i < count; ++i )
	{
		CBaseProp *pEntity = (CBaseProp *)IBasePropAutoList::AutoList().Element( i );

		Vector vEntityMins;
		Vector vEntityMaxs;
		pEntity->CollisionProp()->WorldSpaceAABB( &vEntityMins, &vEntityMaxs );

		if ( !IsBoxIntersectingBox( vMins, vMaxs, vEntityMins, vEntityMaxs ) )
		{
			continue;
		}

		IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
		if ( pPhys )
		{
			pPhys->Wake();
		}
	}
}

int CProjectedWallEntity::ObjectCaps( void )
{	
	return BaseClass::ObjectCaps() & 1;
}

void CProjectedWallEntity::ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs )
{
	*pWorldMins = m_vWorldSpace_WallMins;
	*pWorldMaxs = m_vWorldSpace_WallMaxs;
}

void CProjectedWallEntity::OnPreProjected( void )
{
	if ( m_bWasEverProjected )
	{
		WakeNearbyProps();
	}
	CheckForSettledReflectorCubes();

	m_bWasEverProjected = true;
}

void CProjectedWallEntity::OnProjected( void )
{
	BaseClass::OnProjected();

	ProjectWall();

	m_flParticleUpdateTime = gpGlobals->curtime + 0.5;
}

void CProjectedWallEntity::CleanupWall( void )
{
	if ( VPhysicsGetObject() )
	{
		CPhysicsShadowClone::NotifyDestroy( VPhysicsGetObject(), this );
		VPhysicsDestroyObject();
	}
	if (m_pWallCollideable)
	{
		CPhysicsShadowClone::NotifyDestroy( m_pWallCollideable, this );
		physcollision->DestroyCollide( m_pWallCollideable );
		m_pWallCollideable = NULL;
	}

	m_vWorldSpace_WallMins = m_vWorldSpace_WallMaxs = vec3_origin;
	m_flHeight = m_flWidth = m_flLength = 0.0;
	m_hHitPortal = NULL;
}

float CProjectedWallEntity::GetSegmentLength( void )
{
	return m_flSegmentLength;
}

int CProjectedWallEntity::GetNumSegments( void )
{
	return m_nNumSegments;
}

bool CProjectedWallEntity::IsWallPainted( const Vector &vecPosition )
{
#ifdef PORTAL_PAINT
	return CProjectedWallEntity::GetPaintPowerAtPoint(vecPosition) != NO_POWER;
#else
	return false;
#endif
}
#ifdef PORTAL_PAINT
PaintPowerType CProjectedWallEntity::GetPaintPowerAtSegment( int i )
{
	return m_PaintPowers[i];
}
#endif
CBaseProjectedEntity *CProjectedWallEntity::CreateNewProjectedEntity( void )
{
	return CProjectedWallEntity::CreateNewInstance();
}

CProjectedWallEntity *CProjectedWallEntity::CreateNewInstance(void)
{
	return (CProjectedWallEntity *)CreateEntityByName( "projected_wall_entity" );
}

void WallPainted( int colorIndex, int nSegment, CBaseEntity *pWall )
{
	CBaseEntity *pEntity = g_TEWallPaintedEvent.m_hEntity;

	CRecipientFilter filter;
	filter.AddAllPlayers();

	if ( pEntity != pWall )
	{
		if ( pWall )
			g_TEWallPaintedEvent.m_hEntity = pWall;
		else
			g_TEWallPaintedEvent.m_hEntity = NULL;
	}

	g_TEWallPaintedEvent.m_colorIndex = colorIndex;
	g_TEWallPaintedEvent.m_nSegment = nSegment;

	g_TEWallPaintedEvent.Create( filter, 0.0 );
}

#ifndef PROJECTED_WALL_EVENT_SERVERONLY
IMPLEMENT_SERVERCLASS_ST( CTEWallPaintedEvent, DT_TEWallPaintedEvent )

	SendPropEHandle( SENDINFO( m_hEntity ) ),
	SendPropInt( SENDINFO( m_colorIndex ) ),
	SendPropInt( SENDINFO( m_nSegment ) ),

END_SEND_TABLE();
#endif

CTEWallPaintedEvent::~CTEWallPaintedEvent()
{
	m_hEntity = NULL;
}


CTEWallPaintedEvent g_TEWallPaintedEvent("WallPaintedEvent");

#endif // NO_PROJECTED_WALL