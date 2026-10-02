//========================================================================//
//
// Purpose: An alternative to the startneurotoxins command for mappers and will effect all players
//
// $NoKeywords: $
//=====================================================================================//


#ifndef POINT_NEUROTOXIN_H
#define POINT_NEUROTOXIN_H

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
#define CPointNeurotoxin C_PointNeurotoxin
#endif

class CPointNeurotoxin : public CBaseEntity
{
	DECLARE_CLASS(CPointNeurotoxin, CBaseEntity)
public:
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
	DECLARE_NETWORKCLASS();
	CPointNeurotoxin();
	~CPointNeurotoxin();

	bool	IsDamaging()
	{
		if ( m_bPaused ) // Can't pause while damaging
		{
			return false;
		}

		return m_MainTimer.HasStarted() && m_MainTimer.IsElapsed();
	}
	bool	InProgress()
	{
		if ( m_bPaused ) // Can't pause while damaging or hasn't started, so must be in progress
		{
			return true;
		}

		return m_MainTimer.HasStarted() && !m_MainTimer.IsElapsed();
	}

	float	GetRemainingTime()
	{
		if ( m_bPaused )
		{
			return m_flPausedTimeLeft;
		}

		return m_MainTimer.GetRemainingTime();
	}

#ifndef CLIENT_DLL

	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	void	Start();
	void	Stop();
	void	Pause();
	void	Resume();

	void	TimerEndThink();
	void	DamagePlayersThink();

//	bool	ShouldUseMaxTime(); //Is this really necessary?
	bool	ShouldUseMaxTimeLeft();

	void	ClampMainTimer();
	
	float	m_flNeurotoxinMaxTimeLeft;
	float	m_flNeurotoxinTime;
	
	// Inputs
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
private:
	COutputEvent m_OnStart;
	COutputEvent m_OnStop;
	COutputEvent m_OnPause;
	COutputEvent m_OnResume;
	COutputEvent m_OnTimerEnded;
#else
public:

	void OnDataChanged( DataUpdateType_t updatetype );

private:

	bool m_bOldInProgress;

#endif // !CLIENT_DLL

private:
	
	CNetworkVarEmbedded( NetworkedCountdownTimer, m_MainTimer );
	CNetworkVar( bool, m_bPaused );
	CNetworkVar( float, m_flPausedTimeLeft );
};

extern CPointNeurotoxin *g_pActiveNeurotoxin;

#endif