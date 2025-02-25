#ifndef TRIGGER_BOX_REFLECTOR_H
#define TRIGGER_BOX_REFLECTOR_H

class CPropBox;

class CTriggerBoxReflector : public CBaseEntity
{
public:
	DECLARE_CLASS( CTriggerBoxReflector, CBaseEntity );
	DECLARE_DATADESC();

	CTriggerBoxReflector();

	void Spawn( void );
	void Activate( void );
	void Precache( void );
	void UpdateOnRemove( void );
	bool CreateVPhysics();

	void Touch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

	void DetachBox( CPropBox *pBox, bool bPush = false );

	void EnergyBallHit( CBaseEntity *pBall );
	
	void SetSpecificBeamBrightness( const char *name, float flBrightness );
	void SetBeamBrightness( float flBrightness );
		
	// Thinks
	void TemporaryDetachThink( void );
	void BeamUpdateThink( void );

	// Accessors
	CPropBox *GetBox() const { return m_hAttachedBox; }

private:

	string_t m_iszBeamSetName1;
	string_t m_iszBeamSetName2;
	string_t m_iszBeamSetName3;
	string_t m_iszBeamSetName4;
	string_t m_iszBeamSetName5;

	string_t m_iszAttachToEntity;

	bool m_bTemporary;
	float m_flTemporaryDetachTime;
	float m_flTemporaryEndTime;
	float m_flBeamBrightness;

	CHandle<CBaseEntity> m_hAttachEnt; // The reference entity the trigger will set the box to
	CHandle<CPropBox> m_hAttachedBox; // The box this trigger attaches to it

	COutputEvent m_OnAttached;
	COutputEvent m_OnDetached;
	COutputEvent m_OnEnergyBallHit;
};

class CPropCombineBall;
class CFuncBoxReflectorShield : public CBaseEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncBoxReflectorShield, CBaseEntity );

public:
	void Spawn();
	void Activate();
	bool CreateVPhysics( void );

	bool ForceVPhysicsCollide( CBaseEntity *pEntity );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	void EnergyBallHit( CPropCombineBall *pBall );

private:

	string_t						m_iszBoxReflector;
	CHandle<CTriggerBoxReflector>	m_hBoxReflector;
	bool							m_bDisabled;
};

#endif // TRIGGER_BOX_REFLECTOR_H