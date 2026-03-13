#ifndef PROP_WALL_PROJECTOR_H
#define PROP_WALL_PROJECTOR_H

#include "baseprojector.h"
#include "projectedwallentity.h"

class CPropWallProjector : public CBaseProjector
{
public:
	DECLARE_CLASS( CPropWallProjector, CBaseProjector );
	DECLARE_DATADESC();
    CPropWallProjector();
    ~CPropWallProjector();
    void Spawn();
    void Precache();
    void Project();
    void Shutdown();

protected:
    CBaseProjectedEntity *CreateNewProjectedEntity();
};

#endif // PROP_WALL_PROJECTOR_H