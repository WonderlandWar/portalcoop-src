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
#ifndef CLIENT_DLL

	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	void	Start();
	void	Stop();
	void	Pause();
	void	Resume();

	void	ThinkTimer();
	void	DamagePlayersThink();

//	bool	ShouldUseMaxTime(); //Is this really necessary?
	bool	ShouldUseMaxTimeLeft();
		
	int		m_iNeurotoxinMaxTimeLeft;
	int		m_iNeurotoxinTime;
	
	bool	m_bShouldBeTicking;
	bool	m_bShouldDoDamage;

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

	int GetControlledMilliseconds() const { return m_flMillisecondsControlled; }
	int GetNeurotoxinTimeLeft() const { return m_iNeurotoxinTimeLeft; }

private:

	bool m_bOldInProgress;

#endif // !CLIENT_DLL

private:
	
	CNetworkVar( bool, m_bInProgress );
	CNetworkVar( float, m_flMillisecondsControlled );
	CNetworkVar( int, m_iNeurotoxinTimeLeft );
};

extern CPointNeurotoxin *g_ActiveNeurotoxin;

#endif