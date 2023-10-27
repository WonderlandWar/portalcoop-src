//========================================================================//
//
// Purpose: An alternative to the startneurotoxins command for mappers and will effect all players
//
// $NoKeywords: $
//=====================================================================================//


#ifndef POINT_NEUROTOXIN_H
#define POINT_NEUROTOXIN_H

#ifdef CLIENT_DLL
//#define CPointNeurotoxin C_PointNeurotoxin
#endif

#include "cbase.h"
#include "networkvar.h"

#ifdef GAME_DLL
#include "entityinput.h"
#include "entityoutput.h"
#include "portal_player.h"
#include "util.h"
#else
#include "c_portal_player.h"
#include "cdll_util.h"
#endif
#ifndef CLIENT_DLL

class CPointNeurotoxin : public CBaseEntity
{
	DECLARE_CLASS(CPointNeurotoxin, CBaseEntity)
public:

	DECLARE_DATADESC();
//	DECLARE_NETWORKCLASS();

	CPointNeurotoxin();

	void	Start();
	void	Stop();
	void	Pause();
	void	Resume();

	void	ThinkTimer();
	void	DamagePlayersThink();

//	bool	ShouldUseMaxTime(); //Is this really necessary?
	bool	ShouldUseMaxTimeLeft();
		
	int		m_iNeurotoxinMaxTimeLeft;
	/*
	CNetworkVar(int, m_iNeurotoxinTime);
	CNetworkVar(int, m_iNeurotoxinTimeLeft);

	CNetworkVar(bool, m_bInProgress);
	CNetworkVar(bool, m_bShouldBeTicking);
	CNetworkVar(bool, m_bShouldDoDamage);
	*/

	float		m_flMillisecondsControlled;

	int		m_iNeurotoxinTime;
	int		m_iNeurotoxinTimeLeft;
	
	bool	m_bInProgress;
	bool	m_bShouldBeTicking;
	bool	m_bShouldDoDamage;

	// Inputs

#ifndef CLIENT_DLL
	void	InputStart(inputdata_t &inputdata);
	void	InputStop(inputdata_t &inputdata);
	void	InputPause(inputdata_t &inputdata);
	void	InputResume(inputdata_t &inputdata);

	void	InputSetNeurotoxinTimeInSeconds(inputdata_t &inputdata);

	void	InputSetNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata);
	void	InputAddNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata);
	void	InputSubtractNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata);

	void	InputSetMaxTimeLeft(inputdata_t &inputdata);

	// Outputs

	COutputEvent m_OnStart;
	COutputEvent m_OnStop;
	COutputEvent m_OnPause;
	COutputEvent m_OnResume;
	COutputEvent m_OnTimerEnded;
#endif
};
#endif
#endif