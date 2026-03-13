#ifndef C_PROJECTED_WALL_ENTITY_H
#define C_PROJECTED_WALL_ENTITY_H

#include "cbase.h"
#include "c_baseprojectedentity.h"

#define CProjectedWallEntity C_ProjectedWallEntity

DECLARE_AUTO_LIST( IProjectedWallEntityAutoList )

class C_ProjectedWallEntity :
#ifdef PORTAL_PAINT
	public CPaintableEntity<C_BaseProjectedEntity>
#else
	public C_BaseProjectedEntity,
#endif
	public IProjectedWallEntityAutoList
{
public:
#ifdef PORTAL_PAINT
	DECLARE_CLASS( C_ProjectedWallEntity, CPaintableEntity<C_BaseProjectedEntity> );
#else
	DECLARE_CLASS( C_ProjectedWallEntity, C_BaseProjectedEntity );
#endif

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	C_BaseEntity *GetEntity() { return this; }
	C_ProjectedWallEntity();
	
	virtual void Spawn();
	virtual void ClientThink();	
	virtual void UpdateOnRemove();
	
	virtual void Touch( C_BaseEntity *pOther );
	
	virtual CollideType_t GetCollideType();
	virtual int DrawModel( int flags ) OVERRIDE;
	virtual bool ShouldDraw();
	virtual void GetRenderBounds( Vector& vecMins, Vector& vecMaxs );
	virtual QAngle const& GetRenderAngles( void );
	
	virtual RenderGroup_t GetRenderGroup(); // If this errors out, use ComputeTranslucencyType instead
	virtual void ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs );
	
	virtual void OnPreDataChanged( DataUpdateType_t datatype );
	virtual void OnDataChanged( DataUpdateType_t datatype );
	virtual void PostDataUpdate( DataUpdateType_t datatype );
	
	virtual bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );
	virtual bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	
	virtual void OnProjected();
	virtual void GetProjectionExtents( Vector &, Vector & );
	virtual void GetExtents( Vector &outMins, Vector &outMaxs, float flWidthScale = 1.0 );
	virtual void ProjectWall();
#ifdef PORTAL_PAINT
	virtual PaintPowerType GetPaintPowerAtPoint( const Vector& worldContactPt ) const;
	virtual void Paint( PaintPowerType type, const Vector& worldContactPt );
	virtual void CleansePaint();
	virtual void SetPaintPower(int ,PaintPowerType );
#endif
	virtual void GetToolRecordingState( KeyValues *msg );
	virtual void RestoreToToolRecordedState( KeyValues * );
	void DisplaceObstructingEntity( CBaseEntity *pEntity, bool bIgnoreStuck );
private:
	virtual bool InitMaterials();
	void DisplaceObstructingEntity( CBaseEntity *pEntity, const Vector &vOrigin, const Vector &vWallUp, const Vector &vWallRight, bool bIgnoreStuck );
	void DisplaceObstructingEntities();
	void ColorWallByPortal( IMaterial * );
	bool ShouldSpawnParticles( C_Prop_Portal *pPortal );
	void SetupWallParticles();
	void CheckForPlayersOnBridge();
	void PaintWallSideRails( CMeshBuilder & , Vector & , Vector & , Vector & , Vector & ,float ,float );
	
	IMaterial *m_pBodyMaterial;
	IMaterial *m_pPaintMaterialsMid[4];
	IMaterial *m_pPaintMaterialsEnd1[4];
	IMaterial *m_pPaintMaterialsEnd2[4];
	IMaterial *m_pPaintMaterialsSing[4];
	IMaterial *m_pPaintMaterialBounceLSpeed;
	IMaterial *m_pPaintMaterialBounceRSpeed;
	IMaterial *m_pPaintMaterialBounceLRSpeed;
	IMaterial *m_pPaintMaterialRBounceLSpeed;
	IMaterial *m_pPaintMaterialLBounceRSpeed;
	IMaterialVar *m_pPaintColorMid;
	IMaterialVar *m_pPaintColorEnd1;
	IMaterialVar *m_pPaintColorEnd2;
	IMaterialVar *m_pPaintColorSing;
	IMaterial *m_pSideRailMaterial;
	Vector m_vWorldSpace_WallMins;
	Vector m_vWorldSpace_WallMaxs;
	struct WallCollideableAtTime_t
	{
		Vector vStart;
		Vector vEnd;
		Vector vWorldMins;
		Vector vWorldMaxs;
		QAngle qAngles;
		float flTime;
		CPhysCollide *pCollideable;
	};
	CUtlVector<WallCollideableAtTime_t> m_WallCollideables;
	CPhysCollide *m_pActiveCollideable;
	float m_flLength;
	float m_flWidth;
	float m_flHeight;
	bool m_bIsHorizontal;
	CHandle<C_BaseEntity> m_hColorPortal;
	float m_flCurDisplayLength;
	Vector m_vLastStartpos;
	bool SetPaintSurfaceColor(enum PaintPowerType & );
	int m_nNumSegments;
	float m_flSegmentLength;
	CUtlVector<PaintPowerType> m_PaintPowers;
	float m_flParticleUpdateTime;
	float m_flPrevParticleUpdateTime;
	int ComputeSegmentIndex( const Vector& vWorldPositionOnWall ) const;
	
	template< bool bPaint >
	void DrawQuadHelper( CMeshBuilder *meshBuilder, Vector &vOrigin, Vector &vRight, Vector &vUp, float flTextureScaleU, float flTextureScaleV )
	{
		if ( bPaint ) // We can do this later...
		{
			Vector vert = (vUp + vOrigin) - vRight;
			Vector vNormal = (vRight * vUp) - (vRight * vUp);
			VectorNormalize( vNormal );
	
			// Vert 1
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, flTextureScaleV, 0 );
			meshBuilder->Position3fv( vert.Base() );
			meshBuilder->AdvanceVertex();
	
			// Vert 2
			Vector vert2 = vRight + vUp + vOrigin;
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, 0, 0 );
			meshBuilder->Position3fv( vert2.Base() );
			meshBuilder->AdvanceVertex();
	
			// Vert 3
			Vector vert3 = vRight + vOrigin + vUp;
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, 0, flTextureScaleV );
			meshBuilder->Position3fv( vert3.Base() );
			meshBuilder->AdvanceVertex();
	
			// Vert 4
			Vector vert4 = ( vOrigin - vUp ) - vRight;
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, flTextureScaleV, flTextureScaleU );
			meshBuilder->Position3fv( vert4.Base() );
			meshBuilder->AdvanceVertex();
		}
		else
		{
			Vector vert = (vUp + vOrigin) - vRight;
			Vector vNormal = (vRight * vUp) - (vRight * vUp);
			VectorNormalize( vNormal );
	
			// Vert 1
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, 0, flTextureScaleV );
			meshBuilder->Position3fv( vert.Base() );
			meshBuilder->AdvanceVertex();
	
			// Vert 2
			Vector vert2 = vRight + vUp + vOrigin;
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, 0, 0 );
			meshBuilder->Position3fv( vert2.Base() );
			meshBuilder->AdvanceVertex();
	
			// Vert 3
			Vector vert3 = vRight + (vOrigin - vUp);
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, flTextureScaleU, 0 );
			meshBuilder->Position3fv( vert3.Base() );
			meshBuilder->AdvanceVertex();
	
			// Vert 4
			Vector vert4 = ( vOrigin - vUp ) - vRight;
			meshBuilder->Color4ubv( g_nWhite );
			meshBuilder->TexCoord2f( 0, flTextureScaleU, flTextureScaleV );
			meshBuilder->Position3fv( vert4.Base() );
			meshBuilder->AdvanceVertex();
		}
	}
};

#endif // C_PROJECTED_WALL_ENTITY_H