#ifndef PROP_BOX_H
#define PROP_BOX_H

#include "props.h"

enum WeightedCubeType_e
{
	CUBE_STANDARD,
	CUBE_COMPANION,
	CUBE_REFLECTIVE,
	CUBE_SPHERE,
};

class CTriggerBoxReflector;
class CPropCombineBall;

class CPropBox : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropBox, CPhysicsProp );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPropBox();

	void Spawn( void );
	void Precache( void );
	
	virtual	bool	ShouldCollide( int collisionGroup, int contentsMask ) const OVERRIDE;

	void EnergyBallHit( CPropCombineBall *pBall );
	void PreDissolve( CBaseEntity *pActivator, CBaseEntity *pCaller );

	int OnTakeDamage( const CTakeDamageInfo &info ) OVERRIDE;

	CHandle<CTriggerBoxReflector> m_hAttached;

private:

	void InputDissolve( inputdata_t &inputdata );

	COutputEvent m_OnDissolved;
};

class CPropWeightedCube : public CPropBox
{
public:
	DECLARE_CLASS( CPropWeightedCube, CPropBox );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPropWeightedCube();

	void Spawn( void );
	void Precache( void );

	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle	PreferredCarryAngles( void );
	
#ifndef CLIENT_DLL
	#define CREATE_CUBE_AT_POSITION false
	static void CreatePortalWeightedCube( WeightedCubeType_e objectType, bool bAtCursorPosition = true, const Vector &position = vec3_origin );
#endif

	// instead of getting which model it uses, we can just ask this
	WeightedCubeType_e GetCubeType( void ) { return m_nCubeType; }
	
	void SetLaser( CBaseEntity *pLaser );
	CBaseEntity* GetLaser()
	{
		return m_hLaser.Get();
	}
	bool HasLaser( void )
	{
		return m_hLaser.Get() != NULL;
	}

private:
	
	void SetCubeType( void );

	WeightedCubeType_e m_nCubeType;
	EHANDLE					m_hLaser;
};

bool UTIL_IsReflectiveCube( CBaseEntity *pEntity );
bool UTIL_IsWeightedCube( CBaseEntity *pEntity );
bool UTIL_IsValidCubeHolderAttachment( CBaseEntity *pEntity );

#endif // PROP_BOX_H