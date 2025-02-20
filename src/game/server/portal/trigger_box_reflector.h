#ifndef TRIGGER_BOX_REFLECTOR_H
#define TRIGGER_BOX_REFLECTOR_H

class CPropBox;

class CTriggerBoxReflector : public CBaseEntity
{
public:
	DECLARE_CLASS( CTriggerBoxReflector, CBaseEntity );
	DECLARE_DATADESC();

	void Spawn( void );
	void Activate( void );
	void Precache( void );
	void UpdateOnRemove( void );
	bool CreateVPhysics();

	void StartTouch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

	void DetachBox( CPropBox *pBox, bool bPush = false );

	void EnergyBallHit( CBaseEntity *pBall );

	// Thinks
	void TemporaryDetachThink( void );

private:

	string_t m_iszAttachToEntity;

	bool m_bTemporary;

	CHandle<CBaseEntity> m_hAttachEnt; // The reference entity the trigger will set the box to
	CHandle<CPropBox> m_hAttachedBox; // The box this trigger attaches to it

	COutputEvent m_OnAttached;
	COutputEvent m_OnDetached;
	COutputEvent m_OnEnergyBallHit;
};

#endif // TRIGGER_BOX_REFLECTOR_H