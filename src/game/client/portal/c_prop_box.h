#ifndef C_PROP_BOX_H
#define C_PROP_BOX_H

#include "c_physicsprop.h"

class C_PropBox : public C_PhysicsProp
{
public:
	DECLARE_CLASS( C_PropBox, C_PhysicsProp );
	DECLARE_CLIENTCLASS();
};

class C_PropWeightedCube : public C_PropBox
{
public:
	DECLARE_CLASS( C_PropWeightedCube, C_PropBox );
	DECLARE_CLIENTCLASS();
};


#endif // C_PROP_BOX_H