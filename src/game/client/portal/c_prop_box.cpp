#include "cbase.h"
#include "c_prop_box.h"

IMPLEMENT_CLIENTCLASS_DT( C_PropBox, DT_PropBox, CPropBox )
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( prop_box, C_PropBox )

IMPLEMENT_CLIENTCLASS_DT( C_PropWeightedCube, DT_PropWeightedCube, CPropWeightedCube )
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( prop_weighted_cube, C_PropWeightedCube )