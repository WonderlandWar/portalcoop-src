#include "baseprojector.h"
#include "citadel_effects_shared.h"

class ProjectedEntityAmbientSoundProxy : public CBaseEntity
{
public:
	
	DECLARE_CLASS( ProjectedEntityAmbientSoundProxy, CBaseEntity );
	DECLARE_SERVERCLASS();
	ProjectedEntityAmbientSoundProxy();
	~ProjectedEntityAmbientSoundProxy();
	
	static ProjectedEntityAmbientSoundProxy *Create( CBaseEntity *pAttachTo );
    
	int UpdateTransmitState();
	void AttachToEntity( CBaseEntity *pAttachTo );
};

class CPropTractorBeamProjector : public CBaseProjector
{
public:

	DECLARE_CLASS( CPropTractorBeamProjector, CBaseProjector );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPropTractorBeamProjector();
    ~CPropTractorBeamProjector();
    void Spawn();
    void Precache();
    void Project();
	
	float GetLinearForce() { return m_flLinearForce; }
    
	bool IsReversed();
	
protected:
	
    virtual CBaseProjectedEntity *CreateNewProjectedEntity();
    
	void InputSetLinearForce( inputdata_t &inputdata );
    
	void Shutdown();
		
	CNetworkVar( float, m_flLinearForce );
	
	CNetworkVector( m_vEndPos );
	
	CNetworkVar( bool, m_bNoEmitterParticles );
	CNetworkVar( bool, bDisableAutoReprojection );
	
    bool m_bUse128Model;
	
private:

	CHandle<CCitadelEnergyCore> m_hCoreEffect;

    CSoundPatch *m_sndMechanical;
    CSoundPatch *m_sndAmbientSound;
    
	CHandle<ProjectedEntityAmbientSoundProxy> m_hAmbientSoundProxy;
};