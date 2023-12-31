//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CPROPCOMBINEBALL_H_
#define CPROPCOMBINEBALL_H_

#ifdef _WIN32
#pragma once
#endif

class C_PropCombineBall : public C_BaseAnimating
{
	DECLARE_CLASS( C_PropCombineBall, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
public:

	C_PropCombineBall( void );

	virtual RenderGroup_t GetRenderGroup( void );

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual int		DrawModel( int flags );
	
	bool WasWeaponLaunched( void ) const { return m_bWeaponLaunched; }

protected:
	
	bool	m_bWeaponLaunched;		// Means this was fired from the AR2

	void	DrawMotionBlur( void );
	void	DrawFlicker( void );
	virtual bool	InitMaterials( void );

	Vector	m_vecLastOrigin;
	bool	m_bEmit;
	float	m_flRadius;
	bool	m_bHeld;
	bool	m_bLaunched;

	IMaterial	*m_pFlickerMaterial;
	IMaterial	*m_pBodyMaterial;
	IMaterial	*m_pBlurMaterial;
};

// Query function to find out if a physics object is a combine ball (used for collision checks)
bool UTIL_IsCombineBall( CBaseEntity *pEntity );
bool UTIL_IsCombineBallDefinite( CBaseEntity *pEntity );
bool UTIL_IsAR2CombineBall( CBaseEntity *pEntity );


#endif