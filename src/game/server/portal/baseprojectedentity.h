#ifndef BASE_PROJECTED_ENTITY_H
#define BASE_PROJECTED_ENTITY_H


#include "cbase.h"
#include "prop_portal.h"
#include "baseprojector.h"

abstract_class CBaseProjectedEntity : public CBaseEntity
{
public:
	DECLARE_CLASS( CBaseProjectedEntity, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CBaseProjectedEntity();
    ~CBaseProjectedEntity();
    
	virtual void Spawn();
    void FindProjectedEndpoints();
	
    virtual void SetHitPortal( CProp_Portal* pPortal );
    CProp_Portal *GetHitPortal();
	
    virtual void SetSourcePortal( CProp_Portal* pPortal );
    CProp_Portal *GetSourcePortal();
	
    virtual bool DidRedirectionPortalMove( CProp_Portal* pPortal );
	
	Vector &GetStartPoint() { return m_vecStartPoint.GetForModify(); }
	Vector &GetEndPoint() { return m_vecEndPoint.GetForModify(); }
		
    Vector GetLengthVector( void );
	
    virtual void GetProjectionExtents( Vector &outMins, Vector &outMaxs );
    void RecursiveProjection( bool bShouldSpawn, CBaseProjector *pParentProjector, CProp_Portal *pExitPortal, const Vector &vProjectOrigin, const QAngle &qProjectAngles, int iRemainingProjections );
		
    bool IsHittingPortal( Vector* pOutOrigin, QAngle* pOutAngles, CProp_Portal** pOutPortal );
	
    void TestForProjectionChanges();
#ifdef PORTAL_PAINT
    void TestForReflectPaint();
#endif
    virtual void UpdateOnRemove();
    virtual void OnRestore();

	virtual CBaseProjectedEntity *CreateNewProjectedEntity() = 0;

    virtual void OnPreProjected();
    virtual void OnProjected();
	
    virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

    void CheckForSettledReflectorCubes();
		
protected:
		
	CNetworkHandle( CProp_Portal, m_hHitPortal );
	CNetworkHandle( CProp_Portal, m_hSourcePortal );
	
	CNetworkVector( m_vecSourcePortalCenter );
	CNetworkVector( m_vecSourcePortalRemoteCenter );
	CNetworkQAngle( m_vecSourcePortalAngle );
	CNetworkQAngle( m_vecSourcePortalRemoteAngle );
	
	CNetworkVector( m_vecStartPoint );
	CNetworkVector( m_vecEndPoint );
	
	CNetworkHandle( CBaseProjectedEntity, m_hChildSegment );
	
	CNetworkVar( int, m_iMaxRemainingRecursions );
	
};

#endif // BASE_PROJECTED_ENTITY_H