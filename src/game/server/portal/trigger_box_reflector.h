#ifndef TRIGGER_BOX_REFLECTOR_H
#define TRIGGER_BOX_REFLECTOR_H

class CPropBox;
class CPropCombineBall;

#define CUBE_HOLDER_NUM_BEAMS 5

class CTriggerBoxReflector : public CBaseEntity
{
public:
	DECLARE_CLASS( CTriggerBoxReflector, CBaseEntity );
	DECLARE_DATADESC();

	CTriggerBoxReflector();
	~CTriggerBoxReflector();

	void Spawn( void );
	void Activate( void );
	void Precache( void );
	void UpdateOnRemove( void );
	bool CreateVPhysics();

	void Touch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

	void DetachBox( CPropBox *pBox, bool bPush = false );

	void EnergyBallHit( CPropCombineBall *pBall );
	
	void SetSpecificBeamBrightness( const char *name, float flBrightness );
	void SetBeamBrightness( float flBrightness );
		
	// Thinks
	void TemporaryDetachThink( void );
	void BeamUpdateThink( void );

	// Accessors
	CPropBox *GetBox() const { return m_hAttachedBox; }
	
	virtual void OnUnPause( float flAddedTime );

private:

	string_t m_iszBeamSetName[CUBE_HOLDER_NUM_BEAMS];

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