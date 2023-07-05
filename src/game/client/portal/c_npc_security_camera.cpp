#include "cbase.h"
#include "c_npc_security_camera.h"

IMPLEMENT_CLIENTCLASS_DT(C_NPC_SecurityCamera, DT_NPC_SecurityCamera, CNPC_SecurityCamera)
END_RECV_TABLE()
LINK_ENTITY_TO_CLASS(npc_security_camera, C_NPC_SecurityCamera)