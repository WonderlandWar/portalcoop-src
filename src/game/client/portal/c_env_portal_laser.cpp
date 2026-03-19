#include "cbase.h"
#include "portalsimulation.h"
#include "c_portal_beam_helper.h"
#include "portal_util_shared.h"
#include "util_shared.h"
//#include "c_prop_weightedcube.h"
#include "debugoverlay_shared.h"
#include "model_types.h"

#define	MASK_PORTAL_LASER (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_DEBRIS|CONTENTS_HITBOX)

class C_PortalLaser : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_PortalLaser, C_BaseAnimating );	
	DECLARE_CLIENTCLASS();
    
    C_PortalLaser();
    void Precache();
    void Spawn();
    void UpdateOnRemove();
    void OnDataChanged( DataUpdateType_t updateType );
    void ClientThink();
    ~C_PortalLaser();
	
	CBaseAnimating *GetReflector() { return m_hReflector; }

	bool m_bDidFirstTrace;

private:
	
    C_PortalBeamHelper m_beamHelper;
    
	CHandle<CBaseAnimating> m_hReflector;

    Vector m_vStartPoint;
    Vector m_vEndPoint;
    bool m_bLaserOn;
    bool m_bIsLethal;
    bool m_bIsAutoAiming;
    bool m_bUseParentDir;
    QAngle m_angParentAngles;
};

IMPLEMENT_CLIENTCLASS_DT( C_PortalLaser, DT_PortalLaser, CPortalLaser )

RecvPropBool( RECVINFO( m_bLaserOn ) ),
RecvPropBool( RECVINFO( m_bIsAutoAiming ) ),
RecvPropBool( RECVINFO( m_bIsLethal ) ),
RecvPropBool( RECVINFO( m_bUseParentDir ) ),
RecvPropEHandle( RECVINFO( m_hReflector ) ),
RecvPropVector( RECVINFO( m_vStartPoint ) ),
RecvPropVector( RECVINFO( m_vEndPoint ) ),
RecvPropQAngles( RECVINFO( m_angParentAngles ) ),

END_RECV_TABLE();

LINK_ENTITY_TO_CLASS( env_portal_laser, C_PortalLaser );

C_PortalLaser::C_PortalLaser( void )
{
	m_bLaserOn = false;
	m_bDidFirstTrace = false;

	m_vStartPoint = vec3_origin;
	m_vEndPoint = vec3_origin;
}

C_PortalLaser::~C_PortalLaser( void )
{
}
void C_PortalLaser::Precache( void )
{
	 BaseClass::Precache();
}

void C_PortalLaser::Spawn( void )
{
	C_Beam *pNewBeam; // eax

	Precache();
	if ( m_bIsLethal )
	{
		pNewBeam = C_Beam::BeamCreate("sprites/laserbeam.vmt", 2.0);
		pNewBeam->SetRenderColor( 100, 255, 100 );
	}
	else
	{
		pNewBeam = C_Beam::BeamCreate("sprites/purplelaser1.vmt", 32.0);
	}
	
	m_beamHelper.Init( pNewBeam );
	BaseClass::Spawn();
}

void C_PortalLaser::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
}

void C_PortalLaser::OnDataChanged( DataUpdateType_t updateType )
{
	if (m_bLaserOn)
	{
		SetNextClientThink(CLIENT_THINK_ALWAYS);
		m_beamHelper.TurnOn();
	}
	else
	{
		SetNextClientThink(CLIENT_THINK_NEVER);
		m_beamHelper.TurnOff();
	}

	BaseClass::OnDataChanged( updateType );
}


class CTraceFilterLaser : public CTraceFilterSimpleClassnameList
{
public:
	CTraceFilterLaser( const IHandleEntity *passentity, int collisionGroup );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

	CHandle<C_PortalLaser> m_hPortalLaser;
};

//-----------------------------------------------------------------------------
// Trace filter that can take a list of entities to ignore
//-----------------------------------------------------------------------------
CTraceFilterLaser::CTraceFilterLaser( const IHandleEntity *passentity, int collisionGroup ) :
CTraceFilterSimpleClassnameList( passentity, collisionGroup )
{
	m_hPortalLaser = NULL;
}

bool Laser_CanHitTransparentEntity( CBaseEntity *pEntity )
{
	if ( !pEntity || pEntity->IsWorld() )
	{
		return false;
	}

	if ( !pEntity->GetModel() || modelinfo->GetModelType( pEntity->GetModel() ) == mod_brush )
	{
		return false;
	}

	return true;
}

bool Laser_CanHitReflectorNoCast( C_PortalLaser *pLaser, IHandleEntity *pReflector )
{
	if ( pLaser && !pLaser->m_bDidFirstTrace && (pLaser->GetReflector() && pLaser->GetReflector() == pReflector) )
	{
		return false;
	}

	return true;
}

bool Laser_CanHitReflector( C_BaseEntity* pLaserEntity, IHandleEntity *pReflector )
{
	return Laser_CanHitReflectorNoCast( dynamic_cast<C_PortalLaser*>( pLaserEntity ), pReflector );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterLaser::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	C_PortalLaser *pLaser = m_hPortalLaser;
	if ( !Laser_CanHitReflectorNoCast( pLaser, pHandleEntity ) )
	{
		return false;
	}

	return CTraceFilterSimpleClassnameList::ShouldHitEntity( pHandleEntity, contentsMask );
}

void C_PortalLaser::ClientThink( void )
{
	m_bDidFirstTrace = false;
	trace_t tr;
	
	CTraceFilterLaser traceFilter( NULL, COLLISION_GROUP_NONE );
	traceFilter.m_hPortalLaser = this;
	traceFilter.AddClassnameToIgnore( "projected_wall_entity" );
	traceFilter.AddClassnameToIgnore( "player" );
	traceFilter.AddClassnameToIgnore( "point_laser_target" );
#if 1
	C_BaseEntity *pReflector = m_hReflector.Get();

	if ( pReflector )
	{
		Vector vDir;
		if (m_bUseParentDir)
		{
			AngleVectors( m_angParentAngles, &vDir);
		}
		else
		{
			AngleVectors( pReflector->GetAbsAngles(), &vDir );
		}
		
		Vector vStart = /*(vDir * 22.0) +*/ pReflector->WorldSpaceCenter();
#if 1
		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pReflector );
		if (pSimulator)
		{
			if (pSimulator->EntityIsInPortalHole( pReflector )
				&& ((((vStart.x * pSimulator->GetInternalData().Placement.PortalPlane.m_Normal.x)
				+ (vStart.y * pSimulator->GetInternalData().Placement.PortalPlane.m_Normal.y))
				+ (vStart.z * pSimulator->GetInternalData().Placement.PortalPlane.m_Normal.z))
				- pSimulator->GetInternalData().Placement.PortalPlane.m_Dist) < 0.0)
			{
				UTIL_Portal_VectorTransform( pSimulator->GetInternalData().Placement.matThisToLinked, vDir, vDir );
				UTIL_Portal_PointTransform( pSimulator->GetInternalData().Placement.matThisToLinked, vStart, vStart );
			}
		}
#endif
		if (m_bIsAutoAiming)
		{
			vDir = (m_vEndPoint - vStart);
			
			VectorNormalize( vDir );
		}
		else
		{
			UTIL_Portal_Laser_Prevent_Tilting(vDir);
		}
#if 0
		// First, do the trace that can hit glass (for CBaseAnimatings)
		trace_t tempTrace;

		Ray_t ray;
		ray.Init( vStart, (vDir * MAX_TRACE_LENGTH) + vStart );

		enginetrace->TraceRay( ray, MASK_PORTAL_LASER | CONTENTS_WINDOW, &traceFilter, &tempTrace );

		if ( !tempTrace.m_pEnt || tempTrace.m_pEnt->GetBaseAnimating() ) // if it was a base animating, then use the same trace
		{
			m_beamHelper.UpdatePointDirection( vStart, vDir, MASK_PORTAL_LASER | CONTENTS_WINDOW, &traceFilter, &tr );
		}
		else
#endif
		{
			m_beamHelper.UpdatePointDirection( this, vStart, vDir, MASK_PORTAL_LASER, &traceFilter, &tr );
		}
	}
	else
#endif
	{
		m_beamHelper.UpdatePoints( this, m_vStartPoint, m_vEndPoint, MASK_PORTAL_LASER, &traceFilter, &tr );
	}
}

// This code sucks
void BeamHelper_Laser_OnTrace( CBaseEntity *pEntity )
{
	C_PortalLaser *pLaser = assert_cast<C_PortalLaser*>( pEntity );
	//if ( pLaser )
	{
		pLaser->m_bDidFirstTrace = true;
	}
}