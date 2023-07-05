#include "cbase.h"
#include "c_npc_turret_floor.h"


IMPLEMENT_CLIENTCLASS_DT(C_NPC_FloorTurret, DT_NPC_FloorTurret, CNPC_FloorTurret)
END_RECV_TABLE()
LINK_ENTITY_TO_CLASS(npc_turret_floor, C_NPC_FloorTurret)