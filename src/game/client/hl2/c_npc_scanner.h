#ifndef C_NPC_SCANNER_H
#define C_NPC_SCANNER_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "c_ai_basenpc.h"

class C_NPC_CScanner : public C_AI_BaseNPC
{
	DECLARE_CLASS(C_NPC_CScanner, C_AI_BaseNPC);
	DECLARE_CLIENTCLASS()
};

class C_NPC_ClawScanner : public C_AI_BaseNPC
{
	DECLARE_CLASS(C_NPC_ClawScanner, C_AI_BaseNPC);
	DECLARE_CLIENTCLASS()
};

#endif //C_NPC_SCANNER_H