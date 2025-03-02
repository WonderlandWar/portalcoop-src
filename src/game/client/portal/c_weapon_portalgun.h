//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_WEAPON_PORTALGUN_H
#define C_WEAPON_PORTALGUN_H

#ifdef _WIN32
#pragma once
#endif


#include "weapon_portalbasecombatweapon.h"

#include "c_prop_portal.h"
#include "fx_interpvalue.h"
#include "beamdraw.h"
#include "iviewrender_beams.h"

#define CWeaponPortalgun C_WeaponPortalgun

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//  CPhysCannonEffect class
//----------------------------------------------------------------------------------------------------------------------------------------------------------

class CPortalgunEffect
{
public:

	CPortalgunEffect( void ) 
		: m_vecColor( 255, 255, 255 ), 
		  m_bVisibleViewModel( true ), 
		  m_bVisible3rdPerson( true ), 
		  m_nAttachment( -1 )
	{}

	void SetAttachment( int attachment ) { m_nAttachment = attachment; }
	int	GetAttachment( void ) const { return m_nAttachment; }

	void SetVisible( bool visible = true ) { m_bVisibleViewModel = visible; m_bVisible3rdPerson = visible; }

	void SetVisibleViewModel( bool visible = true ) { m_bVisibleViewModel = visible; }
	int IsVisibleViewModel( void ) const { return m_bVisibleViewModel; }

	void SetVisible3rdPerson( bool visible = true ) { m_bVisible3rdPerson = visible; }
	int IsVisible3rdPerson( void ) const { return m_bVisible3rdPerson; }

	void SetColor( const Vector &color ) { m_vecColor = color; }
	const Vector &GetColor( void ) const { return m_vecColor; }

	bool SetMaterial(  const char *materialName )
	{
		m_hMaterial.Init( materialName, TEXTURE_GROUP_CLIENT_EFFECTS );
		return ( m_hMaterial != NULL );
	}

	CMaterialReference &GetMaterial( void ) { return m_hMaterial; }

	CInterpolatedValue &GetAlpha( void ) { return m_Alpha; }
	CInterpolatedValue &GetScale( void ) { return m_Scale; }

	virtual PortalWeaponID GetWeaponID( void ) const { return WEAPON_PORTALGUN; }
	
private:
	CInterpolatedValue	m_Alpha;
	CInterpolatedValue	m_Scale;

	Vector				m_vecColor;
	bool				m_bVisibleViewModel;
	bool				m_bVisible3rdPerson;
	int					m_nAttachment;
	CMaterialReference	m_hMaterial;
};


class CPortalgunEffectBeam
{
public:
	CPortalgunEffectBeam( void );;
	~CPortalgunEffectBeam( void );

	void Release( void );

	void Init( int startAttachment, int endAttachment, CBaseEntity *pEntity, bool firstPerson );

	void SetVisibleViewModel( bool visible = true );
	int IsVisibleViewModel( void ) const;

	void SetVisible3rdPerson( bool visible = true );
	int SetVisible3rdPerson( void ) const;

	void SetBrightness( float fBrightness );

	void DrawBeam( void );

private:
	Beam_t	*m_pBeam;

	float	m_fBrightness;
};

class C_InfoPlacementHelper;

class C_WeaponPortalgun : public CBasePortalCombatWeapon
{

public:
	DECLARE_CLASS( C_WeaponPortalgun, CBasePortalCombatWeapon );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

private:
	CNetworkVar( bool,	m_bCanFirePortal1 );	// Is able to use primary fire
	CNetworkVar( bool,	m_bCanFirePortal2 );	// Is able to use secondary fire
	CNetworkVar( int,	m_iLastFiredPortal );	// Which portal was placed last
	CNetworkVar( bool,	m_bOpenProngs );		// Which portal was placed last
	CNetworkVar( float, m_fCanPlacePortal1OnThisSurfaceNetworked );	// Tells the gun if it can place on the surface it's pointing at
	CNetworkVar( float, m_fCanPlacePortal2OnThisSurfaceNetworked );	// Tells the gun if it can place on the surface it's pointing at
	float				m_fCanPlacePortal1OnThisSurface;	// Tells the gun if it can place on the surface it's pointing at
	float				m_fCanPlacePortal2OnThisSurface;	// Tells the gun if it can place on the surface it's pointing at

	CNetworkVar( float,	m_fEffectsMaxSize1 );
	CNetworkVar( float,	m_fEffectsMaxSize2 );

public:
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_10DEGREES;
		return cone;
	}
		
	void Precache ( void );

	virtual void OnRestore( void );
	virtual void UpdateOnRemove(void);
	void DoEffectBlast(CBaseEntity *pOwner, bool bPortal2, int iPlacedBy, const Vector &ptStart, const Vector &ptFinalPos, const QAngle &qStartAngles, float fDelay, int iPortalLinkageGroup);
	void Spawn( void );
	void DoEffectCreate( Vector &vDir, Vector &ptStart, Vector &ptEnd, bool bPortal1, bool bPlayer );

	virtual bool ShouldDrawCrosshair( void );
	float GetPortal1Placablity( void );
	float GetPortal2Placablity( void );
	int GetLastFiredPortal( void ) { return m_iLastFiredPortal; }
	bool IsHoldingObject( void ) { return m_bOpenProngs; }

	bool Reload( void );
	void FillClip( void );
	void CheckHolsterReload( void );
	void ItemHolsterFrame( void );
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	bool Deploy( void );

	void SetCanFirePortal1( bool bCanFire = true );
	void SetCanFirePortal2( bool bCanFire = true );
	bool CanFirePortal1( void ) { return m_bCanFirePortal1; }
	bool CanFirePortal2( void ) { return m_bCanFirePortal2; }

	void PrimaryAttack( void );
	void SecondaryAttack( void );

	void DelayAttack( float fDelay );
	
	virtual bool PreThink( void );
	virtual void Think( void );

	void DryFire( void );
	virtual float GetFireRate( void ) { return 0.7; };
	void WeaponIdle( void );
	
	void FirePortal1( );
	void FirePortal2( );

	CProp_Portal *GetAssociatedPortal(bool bPortal2);

	void SetLinkageGroupID( int iPortalLinkageGroupID) { iPortalLinkageGroupID = m_iPortalLinkageGroupID; }
	
	float TraceFirePortal( bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy, C_InfoPlacementHelper **pPlacementHelper, bool bTest = false );
	float FirePortal( bool bPortal2, Vector *pVector = 0, bool bTest = false );
	C_InfoPlacementHelper *AttemptSnapToPlacementHelper( bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy, bool bTest );
	bool PortalTraceClippedByBlockers( bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy, bool bTest );
	bool ShouldStealCoopPortal(CProp_Portal *pHitPortal, float fPlacementSuccess);

	unsigned char m_iOldPortalLinkageGroupID; 
	unsigned char m_iPortalLinkageGroupID; //which portal linkage group this gun is tied to, usually set by mapper, or inherited from owning player's index

	Vector m_vFirstPredictedShotPos;

	int m_iCustomPortalColorSet;
	int	m_iOldPortalColorSet;
	int m_iPortalColorSet;
	int m_iValidPlayer;
	bool m_bCanAttack;

	CHandle<CProp_Portal> m_hPrimaryPortal;
	CHandle<CProp_Portal> m_hSecondaryPortal;

	const char *GetPlacementSuccess(float fPlacementSuccess);

protected:

	void	StartEffects( void );	// Initialize all sprites and beams
	void	StopEffects( bool stopSound = true );	// Hide all effects temporarily
	void	DestroyEffects( void );	// Destroy all sprites and beams

	// Portalgun effects
	void	DoEffect( int effectType, Vector *pos = NULL );

	void	DoEffectClosed( void );
	void	DoEffectReady( void );
	void	DoEffectHolding( void );
	void	DoEffectNone( void );

	enum EffectType_t
	{
		PORTALGUN_GRAVLIGHT = 0,
		PORTALGUN_GRAVLIGHT_WORLD,
		PORTALGUN_PORTAL1LIGHT,
		PORTALGUN_PORTAL1LIGHT_WORLD,
		PORTALGUN_PORTAL2LIGHT,
		PORTALGUN_PORTAL2LIGHT_WORLD,

		PORTALGUN_GLOW1,	// Must be in order!
		PORTALGUN_GLOW2,
		PORTALGUN_GLOW3,
		PORTALGUN_GLOW4,
		PORTALGUN_GLOW5,
		PORTALGUN_GLOW6,

		PORTALGUN_GLOW1_WORLD,
		PORTALGUN_GLOW2_WORLD,
		PORTALGUN_GLOW3_WORLD,
		PORTALGUN_GLOW4_WORLD,
		PORTALGUN_GLOW5_WORLD,
		PORTALGUN_GLOW6_WORLD,

		PORTALGUN_ENDCAP1,	// Must be in order!
		PORTALGUN_ENDCAP2,
		PORTALGUN_ENDCAP3,

		PORTALGUN_ENDCAP1_WORLD,
		PORTALGUN_ENDCAP2_WORLD,
		PORTALGUN_ENDCAP3_WORLD,

		PORTALGUN_MUZZLE_GLOW,

		PORTALGUN_MUZZLE_GLOW_WORLD,

		PORTALGUN_TUBE_BEAM1,
		PORTALGUN_TUBE_BEAM2,
		PORTALGUN_TUBE_BEAM3,
		PORTALGUN_TUBE_BEAM4,
		PORTALGUN_TUBE_BEAM5,

		PORTALGUN_TUBE_BEAM1_WORLD,
		PORTALGUN_TUBE_BEAM2_WORLD,
		PORTALGUN_TUBE_BEAM3_WORLD,
		PORTALGUN_TUBE_BEAM4_WORLD,
		PORTALGUN_TUBE_BEAM5_WORLD,

		NUM_PORTALGUN_PARAMETERS	// Must be last!
	};

	#define	NUM_GLOW_SPRITES ((C_WeaponPortalgun::PORTALGUN_GLOW6-C_WeaponPortalgun::PORTALGUN_GLOW1)+1)
	#define	NUM_GLOW_SPRITES_WORLD ((C_WeaponPortalgun::PORTALGUN_GLOW6_WORLD-C_WeaponPortalgun::PORTALGUN_GLOW1_WORLD)+1)
	#define NUM_ENDCAP_SPRITES ((C_WeaponPortalgun::PORTALGUN_ENDCAP3-C_WeaponPortalgun::PORTALGUN_ENDCAP1)+1)
	#define NUM_ENDCAP_SPRITES_WORLD ((C_WeaponPortalgun::PORTALGUN_ENDCAP3_WORLD-C_WeaponPortalgun::PORTALGUN_ENDCAP1_WORLD)+1)
	#define NUM_TUBE_BEAM_SPRITES ((C_WeaponPortalgun::PORTALGUN_TUBE_BEAM5-C_WeaponPortalgun::PORTALGUN_TUBE_BEAM1)+1)
	#define NUM_TUBE_BEAM_SPRITES_WORLD ((C_WeaponPortalgun::PORTALGUN_TUBE_BEAM5_WORLD-C_WeaponPortalgun::PORTALGUN_TUBE_BEAM1_WORLD)+1)

	#define	NUM_PORTALGUN_BEAMS	6

	void			DrawEffects( bool b3rdPerson );
	Vector			GetEffectColor( int iPalletIndex );
	void			GetEffectParameters( EffectType_t effectID, color32 &color, float &scale, IMaterial **pMaterial, Vector &vecAttachment, bool b3rdPerson );
	void			DrawEffectSprite( EffectType_t effectID, bool b3rdPerson );
	inline bool		IsEffectVisible( EffectType_t effectID, bool b3rdPerson );
	void			UpdateElementPosition( void );

	CPortalgunEffect		m_Parameters[NUM_PORTALGUN_PARAMETERS];	// Interpolated parameters for the effects
	CPortalgunEffectBeam	m_Beams[NUM_PORTALGUN_BEAMS];				// Beams

	int				m_nOldEffectState;	// Used for parity checks
	bool			m_bOldCanFirePortal1;
	bool			m_bOldCanFirePortal2;

	bool			m_bPulseUp;
	float			m_fPulse;

	CNetworkVar( int,	m_EffectState );		// Current state of the effects on the gun

public:

	virtual int		DrawModel( int flags );
	virtual void	ViewModelDrawn( C_BaseViewModel *pBaseViewModel );
	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

	void DoEffectIdle( void );
	void OpenProngs( bool bOpenProngs );

public:

	DECLARE_ACTTABLE();

	C_WeaponPortalgun(void);

private:
	C_WeaponPortalgun( const C_WeaponPortalgun & );

};


#endif // C_WEAPON_PORTALGUN_H
