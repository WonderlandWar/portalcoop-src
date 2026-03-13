#ifndef C_BASEPROJECTEDENTITY_H
#define C_BASEPROJECTEDENTITY_H

#include "cbase.h"
#ifdef PORTAL_PAINT
#include "paint/paintable_entity.h"
#endif

class C_BaseProjector;

class C_BaseProjectedEntity : public C_BaseEntity
{
public:
	
	DECLARE_CLASS( C_BaseProjectedEntity, C_BaseEntity )
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	
	C_BaseProjectedEntity();
	~C_BaseProjectedEntity();
	Vector GetStartPoint() { return m_vecStartPoint; }
	Vector GetEndPoint() { return m_vecEndPoint; }
	Vector GetLengthVector();
    void RecursiveProjection( bool bShouldSpawn, C_BaseProjector *pParentProjector, C_Prop_Portal *pExitPortal, const Vector &vProjectOrigin, const QAngle &qProjectAngles, int iRemainingProjections );
	void TestForProjectionChanges();
	
    bool IsHittingPortal( Vector* pOutOrigin, QAngle* pOutAngles, C_Prop_Portal** pOutPortal );
    bool DidRedirectionPortalMove( C_Prop_Portal* pPortal );

	void FindProjectedEndpoints();
    virtual void GetProjectionExtents( Vector &outMins, Vector &outMaxs );
	virtual void OnProjected();
	virtual void HandleDataChange();
	virtual void HandlePredictionError(bool bErrorInThisEntity);
	virtual void OnPreDataChanged( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	C_BasePlayer *GetPredictionOwner();
	bool ShouldPredict();
	static void TestAllForProjectionChanges();
	
	struct BaseProjectedEntity_PreDataChanged
	{
		Vector vStartPoint;
		Vector vEndPoint;
		QAngle qAngles;
		
		BaseProjectedEntity_PreDataChanged()
		{
			vStartPoint = vec3_origin;
			vEndPoint = vec3_origin;
			qAngles = vec3_angle;
		}
    };
	
	
    void SetHitPortal( C_Prop_Portal* pPortal );
    void SetSourcePortal( C_Prop_Portal* pPortal );
	C_Prop_Portal *GetHitPortal();
	C_Prop_Portal *GetSourcePortal();
	
	BaseProjectedEntity_PreDataChanged PreDataChanged;

	Vector m_vecSourcePortalCenter;
	Vector m_vecSourcePortalRemoteCenter;
	QAngle m_vecSourcePortalAngle;
	QAngle m_vecSourcePortalRemoteAngle;
	Vector m_vecStartPoint;
	Vector m_vecEndPoint;
	int m_iMaxRemainingRecursions;

	CHandle<C_Prop_Portal> m_hHitPortal;
	CHandle<C_Prop_Portal> m_hSourcePortal;
	CHandle<C_BaseProjectedEntity> m_hChildSegment;

};

#endif // C_BASEPROJECTEDENTITY_H