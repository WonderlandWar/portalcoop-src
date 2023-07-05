#ifndef C_NPC_SECURITYCAMERA_H
#define C_NPC_SECURITYCAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "c_ai_basenpc.h"

class C_NPC_SecurityCamera : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS(C_NPC_SecurityCamera, C_AI_BaseNPC)
	DECLARE_CLIENTCLASS();
};

#endif //C_NPC_SECURITYCAMERA_H