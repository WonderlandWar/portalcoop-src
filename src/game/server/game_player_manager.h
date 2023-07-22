#ifndef GAME_PLAYER_MANAGER_H
#define GAME_PLAYER_MANAGER_H
#ifdef WIN32
#pragma once
#endif

#include "cbase.h"


class CPlayerManager : public CBaseEntity
{
public:
	DECLARE_CLASS(CPlayerManager, CBaseEntity)
	DECLARE_DATADESC();


	void Spawn(void);
	
	void InputPlayerCount( inputdata_t &inputdata );
	void InputMaxPlayerCount( inputdata_t &inputdata );
	void InputPlayerSpawn( inputdata_t &inputdata ); // !FGD
	void InputPlayerJoin( inputdata_t &inputdata ); // !FGD
	void InputPlayerLeave( inputdata_t &inputdata ); // !FGD

	COutputInt m_PlayerCount;
	COutputInt m_MaxPlayers;
	COutputEvent m_OnPlayerSpawn;
	COutputEvent m_OnPlayerJoin;
};

#endif //GAME_PLAYER_MANAGER_H