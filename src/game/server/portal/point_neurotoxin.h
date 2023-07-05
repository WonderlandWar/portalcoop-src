//========================================================================//
//
// Purpose: An alternative to the startneurotoxins command for mappers and will effect all players
//
// $NoKeywords: $
//=====================================================================================//

#ifndef POINT_NEUROTOXIN_H
#define POINT_NEUROTOXIN_H

#include "cbase.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "networkvar.h"

/*
float	flNeurotoxinTime;
float	flNeurotoxinTimeLeft;

EHANDLE m_hPointNeurotoxinCaller;

class CNeurotoxin
{
public:
	void	Start();
	void	Stop();
	void	Pause();
	void	Resume();

	void	ThinkTime();

	bool	m_bShouldBeTicking;
	bool	m_bIsTicking;

	bool	m_bTimeEnded;
	bool	m_bShouldDoDamage;
		
	void	OnTimeEnded();
};
*/
class CPointNeurotoxin : public CBaseEntity
{
	DECLARE_CLASS(CPointNeurotoxin, CBaseEntity)
public:

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPointNeurotoxin();

	void	Start();
	void	Stop();
	void	Pause();
	void	Resume();
	
	void	ThinkTimer();
	void	DamagePlayersThink();

	float	GetRemainingTime();

	float	m_flNeurotoxinTime;
	
	CNetworkVar(int, m_flNeurotoxinTimeLeft);
	CNetworkVar(bool, m_bInProgress);

	bool	m_bShouldBeTicking;
	bool	m_bShouldDoDamage;


//	void	FireOutput_TimerEnded();

	// Inputs

	void	InputStart(inputdata_t &inputdata);
	void	InputStop(inputdata_t &inputdata);
	void	InputPause(inputdata_t &inputdata);
	void	InputResume(inputdata_t &inputdata);

	void	InputSetNeurotoxinTimeInSeconds(inputdata_t &inputdata);
	void	InputAddNeurotoxinTimeInSeconds(inputdata_t &inputdata);
	void	InputSubtractNeurotoxinTimeInSeconds(inputdata_t &inputdata);

	void	InputSetNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata);
	void	InputAddNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata);
	void	InputSubtractNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata);

	// Outputs

	COutputEvent m_OnStart;
	COutputEvent m_OnStop;
	COutputEvent m_OnPause;
	COutputEvent m_OnResume;
	COutputEvent m_OnTimerEnded;
};

#endif