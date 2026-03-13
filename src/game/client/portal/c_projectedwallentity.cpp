#include "cbase.h"
#include "c_projectedwallentity.h"
#include "c_world.h"
#include "mathlib/polyhedron.h"
#include "portal_player_shared.h"
#include "prediction.h"
#include "c_prop_portal.h"

#undef CProjectedWallEntity

#define PROJECTED_WALL_WIDTH 64.0f
#define PROJECTED_WALL_HEIGHT 0.015625 // 1/64 - thickness of the bridge

static int s_iActiveCollideableFlatFieldOffset = -1;

unsigned char g_nWhite[] =
{
	255,
	255,
	255,
	255
};

extern ConVar sv_thinnerprojectedwalls;
ConVar cl_draw_projected_wall_with_paint( "cl_draw_projected_wall_with_paint", "0", FCVAR_CLIENTDLL ); // 1 by default, but 0 for now
ConVar cl_projected_wall_projection_speed( "cl_projected_wall_projection_speed", "150", FCVAR_CLIENTDLL );

IMPLEMENT_AUTO_LIST( IProjectedWallEntityAutoList )

IMPLEMENT_CLIENTCLASS_DT( C_ProjectedWallEntity, DT_ProjectedWallEntity, CProjectedWallEntity )
	RecvPropFloat( RECVINFO( m_flLength ) ),
	RecvPropFloat( RECVINFO( m_flHeight ) ),
	RecvPropFloat( RECVINFO( m_flWidth ) ),
	RecvPropFloat( RECVINFO( m_flSegmentLength ) ),
	RecvPropFloat( RECVINFO( m_flParticleUpdateTime ) ),
	
	RecvPropBool( RECVINFO( m_bIsHorizontal ) ),
	RecvPropInt( RECVINFO( m_nNumSegments ) ),
	
	RecvPropVector( RECVINFO( m_vWorldSpace_WallMins ) ),
	RecvPropVector( RECVINFO( m_vWorldSpace_WallMaxs ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ProjectedWallEntity )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( projected_wall_entity, C_ProjectedWallEntity )

C_ProjectedWallEntity::C_ProjectedWallEntity()
{
	m_pBodyMaterial = NULL;
	m_pSideRailMaterial = NULL;
	m_pPaintMaterialsMid[0] = NULL;
	m_pPaintMaterialsEnd1[0] = NULL;
	m_pPaintMaterialsEnd2[0] = NULL;
	m_pPaintMaterialsSing[0] = NULL;
	m_pPaintMaterialsMid[1] = NULL;
	m_pPaintMaterialsEnd1[1] = NULL;
	m_pPaintMaterialsEnd2[1] = NULL;
	m_pPaintMaterialsSing[1] = NULL;
	m_pPaintMaterialsMid[2] = NULL;
	m_pPaintMaterialsEnd1[2] = NULL;
	m_pPaintMaterialsEnd2[2] = NULL;
	m_pPaintMaterialsSing[2] = NULL;
	m_pPaintMaterialsMid[3] = NULL;
	m_pPaintMaterialsEnd1[3] = NULL;
	m_pPaintMaterialsEnd2[3] = NULL;
	m_pPaintMaterialsSing[3] = NULL;
	m_pPaintMaterialRBounceLSpeed = NULL;
	m_pPaintMaterialLBounceRSpeed = NULL;
	m_pPaintMaterialBounceRSpeed = NULL;
	m_pPaintMaterialBounceLSpeed = NULL;
	m_pPaintMaterialBounceLRSpeed = NULL;
	m_nNumSegments = NULL;
	m_flCurDisplayLength = 0.0;
	m_flSegmentLength = 0.0;
	m_flParticleUpdateTime = 0.0;
	m_flPrevParticleUpdateTime = 0.0;
}

void C_ProjectedWallEntity::Spawn( void )
{
	SetThink( NULL );
	BaseClass::Spawn();
}

void C_ProjectedWallEntity::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		InitMaterials(); // Might be inaccurate
		SetupWallParticles();
	}

	if ( fabs( m_vLastStartpos.x - m_vecStartPoint.x ) > 0.1 ||
		fabs( m_vLastStartpos.y - m_vecStartPoint.y ) > 0.1 ||
		fabs( m_vLastStartpos.z - m_vecStartPoint.z ) > 0.1 )
	{
		m_flCurDisplayLength = 0.0;
		m_vLastStartpos = m_vecStartPoint;
	}

	CollisionProp()->MarkSurroundingBoundsDirty();
	CollisionProp()->MarkPartitionHandleDirty();
}

void C_ProjectedWallEntity::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );
	m_flPrevParticleUpdateTime = m_flParticleUpdateTime;
}

void C_ProjectedWallEntity::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );
}

void C_ProjectedWallEntity::OnProjected( void )
{
	BaseClass::OnProjected();
	ProjectWall();
	SetupWallParticles();
}

void C_ProjectedWallEntity::ProjectWall( void )
{
	AddEffects( EF_NOINTERP );
	//CheckForPlayersOnBridge();

	const Vector vStartPoint = GetStartPoint();
	const Vector vEndPoint = GetEndPoint();

	Vector vecForward;
	Vector vecRight;
	Vector vecUp;
	AngleVectors( GetNetworkAngles(), &vecForward, &vecRight, &vecUp );

	float fMaximumTime = fmaxf( prediction->InPrediction() ? prediction->GetSavedTime() : gpGlobals->curtime, engine->GetLastTimeStamp() );

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

	bool bSetLength = true;

	CPhysCollide *pWallCollideable = physcollision->ConvertConvexToCollide( &pTempConvex, 1 );
	if ( pWallCollideable )
	{
		Vector vMaxs = vec3_origin;
		Vector vMins = vec3_origin;
		physcollision->CollideGetAABB(&vMins, &vMaxs, pWallCollideable, vec3_origin, vec3_angle);

		WallCollideableAtTime_t collideable;
		collideable.vStart = GetStartPoint();
		collideable.vEnd = GetEndPoint();
		collideable.vWorldMins = vMins;
		collideable.vWorldMaxs = vMaxs;
		collideable.qAngles = GetNetworkAngles();
		collideable.pCollideable = pWallCollideable;
		collideable.flTime = fMaximumTime;
		m_WallCollideables.AddToTail( collideable );

		// FIXME:
		//m_vWorldSpace_WallMins = vMins;
		//m_vWorldSpace_WallMaxs = vMaxs;

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
				
		if ( bSetLength )
		{
			m_flCurDisplayLength = 0.0;
			SetNextClientThink( CLIENT_THINK_ALWAYS );
		}

		CollisionProp()->MarkSurroundingBoundsDirty();
		CollisionProp()->MarkPartitionHandleDirty();
				
		if ( prediction->InPrediction() )
		{
			CheckForPlayersOnBridge();
			DisplaceObstructingEntities();
		}

		m_nNumSegments = ceil( ( m_flLength / m_flSegmentLength ) );
#ifdef PORTAL_PAINT
		// FIXME
		//m_PaintPowers.SetCount( ceil( ( m_flLength / m_flSegmentLength ) ) );
		CleansePaint();
#endif
	}
}

void C_ProjectedWallEntity::RestoreToToolRecordedState( KeyValues *pKV )
{

}
#ifdef PORTAL_PAINT
void C_ProjectedWallEntity::SetPaintPower( int nSegment, PaintPowerType power )
{

}
#endif
void C_ProjectedWallEntity::UpdateOnRemove( void )
{
	if ( prediction->InPrediction() )
		CheckForPlayersOnBridge();

	StopParticleEffects( this );
	for ( int i = 0; i < m_WallCollideables.Count(); ++i )
	{
		CPhysCollide *pCollideable = m_WallCollideables[i].pCollideable;
		physcollision->DestroyCollide( pCollideable );
	}

	m_WallCollideables.RemoveAll();
	BaseClass::UpdateOnRemove();
}

bool C_ProjectedWallEntity::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	if ( !m_pActiveCollideable )
		return false;

	physcollision->TraceBox( ray, mask, NULL, m_pActiveCollideable, vec3_origin, vec3_angle, &trace );
	return trace.fraction < 1.0 || trace.allsolid || trace.startsolid;
}

bool C_ProjectedWallEntity::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return TestCollision( ray, fContentsMask, tr );
}

const QAngle &C_ProjectedWallEntity::GetRenderAngles( void )
{
	return vec3_angle;
}

void C_ProjectedWallEntity::ClientThink( void )
{
	BaseClass::ClientThink();

	float speed = cl_projected_wall_projection_speed.GetFloat();
	float v1 = 0;
	float v2 = 0;

	if ( speed >= 0.0 )
		v2 = fminf( m_flLength, speed );

	float v4 = v2 + m_flCurDisplayLength;
	if ( v4 >= 0 )
		v1 = fminf( m_flLength, v4 );

	m_flCurDisplayLength = v1;
	if ( m_flParticleUpdateTime != m_flPrevParticleUpdateTime )
	{
		m_flPrevParticleUpdateTime = m_flParticleUpdateTime;
		SetupWallParticles();
	}
	
	SetNextClientThink( gpGlobals->curtime + 0.016 );
}

bool C_ProjectedWallEntity::InitMaterials( void )
{
	if ( !m_pBodyMaterial )
	{
		m_pBodyMaterial = materials->FindMaterial( "effects/projected_wall", NULL, false );
		if ( !m_pBodyMaterial )
			return false;
	}
	
	if ( !m_pSideRailMaterial )
	{
		m_pSideRailMaterial = materials->FindMaterial( "effects/projected_wall_rail", NULL, false );
		if ( !m_pSideRailMaterial )
			return false;
	}

	// UNDONE FUNCTION

	return true;
}

bool C_ProjectedWallEntity::ShouldSpawnParticles( C_Prop_Portal *pPortal )
{
	if ( !pPortal->IsActivedAndLinked() )
		return true;

	if ( !dynamic_cast<C_Prop_Portal*>( pPortal ) )
		return false;

	Vector vPortalUp;
	AngleVectors( pPortal->GetAbsAngles(), NULL, NULL, &vPortalUp );
	
	Vector vWallUp;
	AngleVectors( GetAbsAngles(), NULL, NULL, &vWallUp );

	float dot = DotProduct( vPortalUp, vWallUp );
	if ( dot < -1.0 )
	{
		dot = -1.0;
	}
	else if ( dot > 1.0 )
	{
		dot = 1.0;
	}
	
	return fabs( dot ) > 0.7;
}

void C_ProjectedWallEntity::SetupWallParticles()
{
	StopParticleEffects( this );

	C_Prop_Portal *pSourcePortal = m_hSourcePortal;
	C_Prop_Portal *pHitPortal = m_hHitPortal;

	Vector vWallEndPoint = GetEndPoint();
	Vector vWallStartPoint = GetStartPoint();

	Vector vecForward, vecRight, vecUp;
	QAngle qAngles = GetNetworkAngles();
	AngleVectors( qAngles, &vecForward, &vecRight, &vecUp );

	if ( pSourcePortal && ShouldSpawnParticles( pSourcePortal ) )
	{
		Vector vecPortalPos;
		C_Prop_Portal *pPropPortal = dynamic_cast<C_Prop_Portal*>( pSourcePortal );
		if ( pPropPortal )
		{
			vecPortalPos = pPropPortal->GetAbsOrigin();
		}
		else
		{
			vecPortalPos = vWallStartPoint;
			vecPortalPos.z += 512;
		}

		Vector particleOrg = (vecForward * 3.125) + vWallStartPoint;
		DispatchParticleEffect( "projected_wall_impact", particleOrg, vecPortalPos, qAngles, this );
	}
	
	if ( !pHitPortal || ShouldSpawnParticles( pHitPortal ) )
	{
		Vector vecPortalPos;
		C_Prop_Portal *pPropPortal = dynamic_cast<C_Prop_Portal*>( pHitPortal );
		if ( pPropPortal )
		{
			vecPortalPos = pPropPortal->GetAbsOrigin();
		}
		else
		{
			vecPortalPos = vWallEndPoint;
			vecPortalPos.z += 512;
		}

		Vector particleOrg = vWallEndPoint - (vecForward * 3.125);
		DispatchParticleEffect( "projected_wall_impact", particleOrg, vecPortalPos, qAngles, this );
	}
}

void C_ProjectedWallEntity::CheckForPlayersOnBridge()
{

}

RenderGroup_t C_ProjectedWallEntity::GetRenderGroup( void )
{
	return RENDER_GROUP_TRANSLUCENT_ENTITY;
}

int C_ProjectedWallEntity::DrawModel( int flags )
{
	if ( false ) // Unknown condition
		return 0;

	if ( cl_draw_projected_wall_with_paint.GetBool() &&
		m_PaintPowers.NumAllocated() == m_nNumSegments ) // FIXME: Compare m_nNumSegments to m_PaintPowers.m_Memory.m_nGrowSize instead
	{
		// Undone
	}
	else
	{
		Vector vForward, vRight, vUp;
		AngleVectors( GetNetworkAngles(), &vForward, &vRight, &vUp );

		Vector vOrigin = GetStartPoint() + ( ( vForward * m_flCurDisplayLength ) * 0.5 );

		CMatRenderContextPtr pRenderContext( materials );
		IMesh *pBodyMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, m_pBodyMaterial );

		CMeshBuilder meshBuilder;
		if ( g_pGameRules->IsMultiplayer() )
		{
			// Nothing, probably leftover code
		}

		float flLengthTexRate = m_flCurDisplayLength * 0.0078125;

		Vector vHalfLength = vForward * ( ( m_flCurDisplayLength * 0.5 ) + 0.1 );
		Vector vHalfWidth = ( vRight * m_flWidth ) * 0.5;
		Vector vHalfHeight = ( vUp * m_flHeight ) * 0.5;

		meshBuilder.Begin( pBodyMesh, MATERIAL_QUADS, 50 );
		Vector vStart = vHalfHeight + vOrigin;
		DrawQuadHelper<false>( &meshBuilder, vStart, vHalfLength, vHalfWidth, 1.0, flLengthTexRate );
		meshBuilder.End();
		pBodyMesh->Draw();

		IMesh *pRailMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, m_pSideRailMaterial );
		meshBuilder.Begin( pRailMesh, MATERIAL_QUADS, 20 );

		vStart = vHalfWidth + vOrigin;
		vHalfHeight.z = vUp.z * 6.0;
		DrawQuadHelper<false>( &meshBuilder, vStart, vHalfLength, vHalfHeight, 1.0, flLengthTexRate );
		vStart = vOrigin - vHalfWidth;
		DrawQuadHelper<false>( &meshBuilder, vStart, vHalfLength, vHalfHeight, 1.0, flLengthTexRate );		

		meshBuilder.End();
		pRailMesh->Draw();
	}

	return 1;
}

void C_ProjectedWallEntity::ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs )
{
	*pWorldMins = m_vWorldSpace_WallMins;
	*pWorldMaxs = m_vWorldSpace_WallMaxs;
}

void C_ProjectedWallEntity::GetRenderBounds( Vector& vecMins, Vector& vecMaxs )
{
	vecMins = m_vWorldSpace_WallMins - GetRenderOrigin();
	vecMaxs = m_vWorldSpace_WallMaxs - GetRenderOrigin();
}

void C_ProjectedWallEntity::GetProjectionExtents( Vector &outMins, Vector& outMaxs )
{
	GetExtents( outMins, outMaxs, 0.5 );
}

bool C_ProjectedWallEntity::ShouldDraw( void )
{
	return true;
}

CollideType_t C_ProjectedWallEntity::GetCollideType( void )
{
	return ENTITY_SHOULD_COLLIDE;
}

void C_ProjectedWallEntity::GetToolRecordingState( KeyValues *msg )
{
	BaseClass::GetToolRecordingState( msg );
}