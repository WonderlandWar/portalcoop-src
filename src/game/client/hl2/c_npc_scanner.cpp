#include "cbase.h"
#include "c_ai_basenpc.h"
#include "c_npc_scanner.h"

IMPLEMENT_CLIENTCLASS_DT(C_NPC_CScanner, DT_NPC_CScanner, CNPC_CScanner)
END_RECV_TABLE()
LINK_ENTITY_TO_CLASS(npc_cscanner, C_NPC_CScanner)

IMPLEMENT_CLIENTCLASS_DT(C_NPC_ClawScanner, DT_NPC_ClawScanner, CNPC_ClawScanner)
END_RECV_TABLE()
LINK_ENTITY_TO_CLASS(npc_clawscanner, C_NPC_ClawScanner)