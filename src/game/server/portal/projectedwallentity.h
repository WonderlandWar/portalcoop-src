#ifndef PROJECTED_WALL_ENTITY_H
#define PROJECTED_WALL_ENTITY_H

#include "baseprojectedentity.h"
#ifdef PORTAL_PAINT
#include "paint/paintable_entity.h"
#endif
#include "basetempentity.h"

//#define NO_PROJECTED_WALL
#ifndef NO_PROJECTED_WALL

//#define PROJECTED_WALL_SERVERONLY

DECLARE_AUTO_LIST( IProjectedWallEntityAutoList )

class CProjectedWallEntity :
#ifdef PORTAL_PAINT
	public CPaintableEntity<CBaseProjectedEntity>,
#else
	public CBaseProjectedEntity,
#endif
	public IProjectedWallEntityAutoList
{
#ifdef PORTAL_PAINT
	DECLARE_CLASS( CProjectedWallEntity, CPaintableEntity<CBaseProjectedEntity> );
#else
	DECLARE_CLASS( CProjectedWallEntity, CBaseProjectedEntity );
#endif
public:
    CProjectedWallEntity();
    ~CProjectedWallEntity();
	
	DECLARE_DATADESC();	
#ifndef PROJECTED_WALL_SERVERONLY
	DECLARE_SERVERCLASS();
#endif

    virtual void Spawn();
    virtual void Precache();
    virtual void OnRestore();
	virtual void UpdateOnRemove();
    virtual void SetSourcePortal( CProp_Portal *pPortal );
    virtual void SetHitPortal( CProp_Portal *pPortal );
    virtual void Touch( CBaseEntity *pOther );
    virtual void GetProjectionExtents( Vector &, Vector & );
    virtual void GetExtents( Vector &outMins, Vector &outMaxs, float flWidthScale = 1.0 );
#ifdef PORTAL_PAINT
    virtual void Paint( PaintPowerType type, const Vector& worldContactPt );
    virtual void CleansePaint();
#endif
    virtual void OnPreProjected();
    virtual void OnProjected();
	
    virtual bool CreateVPhysics();
	virtual bool ShouldSavePhysics();
	virtual bool TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	void WakeNearbyProps( void );
	
    void ProjectWall();
    void ComputeWorldSpaceSurroundingBox( Vector *, Vector * );
    void NotifyPortalEvent( PortalEvent_t, CProp_Portal * );
    void DisplaceObstructingEntity( CBaseEntity *pEntity, bool bIgnoreStuck ); // DisplaceObstructingEntity--1
	
    bool IsWallPainted( const Vector &vecPosition );
    bool IsHorizontal();
	
    virtual int ObjectCaps();
    int GetNumSegments();
	float GetSegmentLength();
#ifdef PORTAL_PAINT
    virtual PaintPowerType GetPaintPowerAtSegment( int i );
    virtual PaintPowerType GetPaintPowerAtPoint( const Vector& worldContactPt ) const;
#endif
    static CProjectedWallEntity *CreateNewInstance();
    virtual CBaseProjectedEntity *CreateNewProjectedEntity();
	
	virtual CBaseEntity *GetEntity() { return this; }
		
private:
    	
	void DisplaceObstructingEntity( CBaseEntity *pEntity, const Vector &vOrigin, const Vector &vWallUp, const Vector &vWallRight, bool bIgnoreStuck ); // DisplaceObstructingEntity--2
	
    void CleanupWall();
    void DisplaceObstructingEntities();
    void TraceForWallEndpoint( Vector * );
    void DrawWallBoundsHighlight();
    CPhysCollide *m_pWallCollideable;
    void CheckForPlayersOnBridge();
	
	CNetworkVector( m_vWorldSpace_WallMins );
	CNetworkVector( m_vWorldSpace_WallMaxs );
	
	CNetworkHandle( CProp_Portal, m_hColorPortal );
	
	CNetworkVar( float, m_flLength );
	CNetworkVar( float, m_flHeight );
	CNetworkVar( float, m_flWidth );
	
	CNetworkVar( bool, m_bIsHorizontal );
	
	CNetworkVar( int, m_nNumSegments );
	
	CNetworkVar( float, m_flSegmentLength );
	CNetworkVar( float, m_flParticleUpdateTime );
#ifdef PORTAL_PAINT
    CUtlVector<PaintPowerType,CUtlMemory<PaintPowerType,int> > m_PaintPowers;
#endif
	
	bool m_bWasEverProjected;

	int ComputeSegmentIndex( const Vector& vWorldPositionOnWall ) const;
    static int s_HardLightBridgeSurfaceProps;
};

#define PROJECTED_WALL_EVENT_SERVERONLY

class CTEWallPaintedEvent : public CBaseTempEntity
{
public:

	DECLARE_CLASS( CTEWallPaintedEvent, CBaseTempEntity );
#ifndef PROJECTED_WALL_EVENT_SERVERONLY
	DECLARE_SERVERCLASS();
#endif
	CTEWallPaintedEvent( const char *name ) : CBaseTempEntity( name )
	{

	}
	~CTEWallPaintedEvent();
#if 1
	CNetworkHandle( CBaseEntity, m_hEntity );

	CNetworkVar( int, m_colorIndex );
	CNetworkVar( int, m_nSegment );
#else
	EHANDLE m_hEntity;
	int m_colorIndex;
	int m_nSegment;
#endif
};

extern CTEWallPaintedEvent g_TEWallPaintedEvent;

#endif // NO_PROJECTED_WALL

#endif // PROJECTED_WALL_ENTITY_H