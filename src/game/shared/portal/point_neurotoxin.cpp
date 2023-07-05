#include "cbase.h"
#include "point_neurotoxin.h"
//#include "neurotoxin_countdown.h"
#ifdef CLIENT_DLL
#include "c_neurotoxin_countdown.h"
#else
#include "neurotoxin_countdown.h"
#endif

int		iNeurotoxinTimeLeft;
int		iMillisecondsControlled;

#ifndef CLIENT_DLL

BEGIN_DATADESC(CPointNeurotoxin)
#ifdef GAME_DLL
	DEFINE_INPUTFUNC(FIELD_VOID, "Start",	InputStart),
	DEFINE_INPUTFUNC(FIELD_VOID, "Stop",	InputStop),
	DEFINE_INPUTFUNC(FIELD_VOID, "Pause",	InputPause),
	DEFINE_INPUTFUNC(FIELD_VOID, "Resume",	InputPause),

	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetNeurotoxinTimeInSeconds", InputSetNeurotoxinTimeInSeconds),

	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetNeurotoxinTimeLeftInSeconds", InputSetNeurotoxinTimeLeftInSeconds),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "AddNeurotoxinTimeLeftInSeconds", InputAddNeurotoxinTimeLeftInSeconds),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "SubtractNeurotoxinTimeLeftInSeconds", InputSubtractNeurotoxinTimeLeftInSeconds),

	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetMaxTimeLeft", InputSetMaxTimeLeft),

	DEFINE_KEYFIELD(m_iNeurotoxinTime, FIELD_INTEGER, "NeurotoxinTime"),
	DEFINE_KEYFIELD(m_iNeurotoxinMaxTimeLeft, FIELD_INTEGER, "NeurotoxinMaxTimeLeft"),

	DEFINE_OUTPUT(m_OnStart, "OnStart"),
	DEFINE_OUTPUT(m_OnStop, "OnStop"),
	DEFINE_OUTPUT(m_OnPause, "OnPause"),
	DEFINE_OUTPUT(m_OnResume, "OnResume"),
	DEFINE_OUTPUT(m_OnTimerEnded, "OnTimerEnded"),
	
	DEFINE_FIELD(m_flMillisecondsControlled,	FIELD_FLOAT),

	DEFINE_FIELD(m_iNeurotoxinTime,				FIELD_INTEGER),
	DEFINE_FIELD(m_iNeurotoxinTimeLeft,			FIELD_INTEGER),

	DEFINE_FIELD(m_bInProgress,					FIELD_BOOLEAN),
	DEFINE_FIELD(m_bShouldBeTicking,			FIELD_BOOLEAN),
	DEFINE_FIELD(m_bShouldDoDamage,				FIELD_BOOLEAN),
	DEFINE_FIELD(m_bShouldUseMaxTimeLeft,		FIELD_BOOLEAN),

	DEFINE_THINKFUNC(ThinkTimer),
	DEFINE_THINKFUNC(DamagePlayersThink),
#endif

END_DATADESC();
/*
IMPLEMENT_NETWORKCLASS_DT( CPointNeurotoxin, DT_Point_Neurotoxin)
#ifdef GAME_DLL
	SendPropInt(SENDINFO(m_iNeurotoxinTime)),
	SendPropInt(SENDINFO(m_iNeurotoxinTimeLeft)),
	SendPropBool(SENDINFO(m_bInProgress)),
	SendPropBool(SENDINFO(m_bShouldBeTicking)),
	SendPropBool(SENDINFO(m_bShouldDoDamage)),
#else
	RecvPropInt(RECVINFO(m_iNeurotoxinTime)),
	RecvPropInt(RECVINFO(m_iNeurotoxinTimeLeft)),
	RecvPropBool(RECVINFO(m_bInProgress)),
	RecvPropBool(RECVINFO(m_bShouldBeTicking)),
	RecvPropBool(RECVINFO(m_bShouldDoDamage)),
#endif
END_NETWORK_TABLE()
*/
LINK_ENTITY_TO_CLASS(point_neurotoxin, CPointNeurotoxin)

CPointNeurotoxin::CPointNeurotoxin()
{
	m_bInProgress = false;
}

void CPointNeurotoxin::Start()
{
	if (m_bInProgress)
		return;
	
	m_flMillisecondsControlled = 0;
	m_bInProgress = true;
	SetThink(&CPointNeurotoxin::ThinkTimer);
	SetNextThink(gpGlobals->curtime);

	m_iNeurotoxinTimeLeft = m_iNeurotoxinTime;

	m_bShouldBeTicking = true;
#ifndef CLIENT_DLL
	m_OnStart.FireOutput(this, this);
#endif
}

void CPointNeurotoxin::Stop()
{
	if (!m_bInProgress)
		return;
	m_bInProgress = false;
	SetThink(NULL);
	m_iNeurotoxinTimeLeft = m_iNeurotoxinTime;
	m_bShouldBeTicking = false;
	m_bShouldDoDamage = false;

#ifndef CLIENT_DLL
	m_OnStop.FireOutput(this, this);
#endif
}
void CPointNeurotoxin::Pause()
{
	if (!m_bInProgress)
		return;
	if (m_bShouldDoDamage)
		return;
	m_bShouldBeTicking = false;

#ifndef CLIENT_DLL
	m_OnPause.FireOutput(this, this);
#endif
}

void CPointNeurotoxin::Resume()
{
	if (!m_bInProgress)
		return;
	if (m_bShouldDoDamage)
		return;

	m_bShouldBeTicking = true;

#ifndef CLIENT_DLL
	m_OnResume.FireOutput(this, this);
#endif
}

#define MILSECSADDED 1.5
#define NEXTTHINK 0.015

void CPointNeurotoxin::ThinkTimer()
{
	if (m_bShouldBeTicking == true)
	{
		m_flMillisecondsControlled = m_flMillisecondsControlled - MILSECSADDED;
	}
		if (0 > m_flMillisecondsControlled)
			m_flMillisecondsControlled = 0;
	
//	Msg("m_flMillisecondsControlled = %f\n", m_flMillisecondsControlled);

	if (0 >= m_flMillisecondsControlled)
	{

		if (m_iNeurotoxinTimeLeft <= 0 && m_flMillisecondsControlled <= 0)
		{
				m_flMillisecondsControlled = 0;
				m_iNeurotoxinTimeLeft = 0;
				m_bShouldDoDamage = true;
				m_bShouldBeTicking = false;
				m_bInProgress = false;
		}

		if (m_bShouldBeTicking)
		{
			m_iNeurotoxinTimeLeft = m_iNeurotoxinTimeLeft - 1;
			m_flMillisecondsControlled = 99;
		}
		else
		{
			m_flMillisecondsControlled = 0;
		}


		if (m_bShouldDoDamage)
		{
			m_OnTimerEnded.FireOutput(this, this);
			SetThink(&CPointNeurotoxin::DamagePlayersThink);
		}
	}
	

	iMillisecondsControlled = m_flMillisecondsControlled;
	iNeurotoxinTimeLeft = m_iNeurotoxinTimeLeft;
	
	SetNextThink(gpGlobals->curtime + NEXTTHINK);
}

void CPointNeurotoxin::DamagePlayersThink()
{
	if (m_iNeurotoxinTimeLeft >= 1)
	{
		SetThink(&CPointNeurotoxin::ThinkTimer);
		m_bShouldDoDamage = false;
		m_bShouldBeTicking = true;
		m_bInProgress = true;
	}

#ifndef CLIENT_DLL
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CPortal_Player* pPlayer = (CPortal_Player *)UTIL_PlayerByIndex(i);

		if (pPlayer)
		{
			CTakeDamageInfo info;
			info.SetDamage(gpGlobals->frametime * 50.0f);
			info.SetDamageType(DMG_NERVEGAS);
			pPlayer->TakeDamage(info);

		}
	
	}
#endif
	SetNextThink(gpGlobals->curtime);
}

bool CPointNeurotoxin::ShouldUseMaxTimeLeft()
{
	if (m_iNeurotoxinMaxTimeLeft <= 0)
		return false;
	else
		return true;
}

void CPointNeurotoxin::InputStart(inputdata_t &inputdata)
{
	Start();
}
void CPointNeurotoxin::InputStop(inputdata_t &inputdata)
{
	Stop();
}
void CPointNeurotoxin::InputPause(inputdata_t &inputdata)
{
	Pause();
}
void CPointNeurotoxin::InputResume(inputdata_t &inputdata)
{
	Resume();
}

#ifndef CLIENT_DLL
void CPointNeurotoxin::InputSetNeurotoxinTimeInSeconds(inputdata_t &inputdata)
{
	m_iNeurotoxinTime = inputdata.value.Int();

	if (m_iNeurotoxinTime < 0)
		m_iNeurotoxinTime = 0;
}

void CPointNeurotoxin::InputSetNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	m_iNeurotoxinTimeLeft = inputdata.value.Int();
	m_flMillisecondsControlled = 0;

	if (m_iNeurotoxinTimeLeft < 0)
			m_iNeurotoxinTimeLeft = 0;

	if (ShouldUseMaxTimeLeft() == true)
	{
		if (m_iNeurotoxinTimeLeft > m_iNeurotoxinMaxTimeLeft)
		{
			m_iNeurotoxinTimeLeft = m_iNeurotoxinMaxTimeLeft;
		}
	}

}

void CPointNeurotoxin::InputAddNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	m_iNeurotoxinTimeLeft = m_iNeurotoxinTimeLeft + inputdata.value.Int();
	if (m_iNeurotoxinTimeLeft < 0)
			m_iNeurotoxinTimeLeft = 0;

	if (ShouldUseMaxTimeLeft() == true)
	{
		if (m_iNeurotoxinTimeLeft > m_iNeurotoxinMaxTimeLeft)
		{
			m_iNeurotoxinTimeLeft = m_iNeurotoxinMaxTimeLeft;
			m_flMillisecondsControlled = 0;
		}
	}

}


void CPointNeurotoxin::InputSubtractNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	m_iNeurotoxinTimeLeft = m_iNeurotoxinTimeLeft - inputdata.value.Int();
	if (m_iNeurotoxinTimeLeft < 0)
			m_iNeurotoxinTimeLeft = 0;

	if (ShouldUseMaxTimeLeft() == true)
	{
		if (m_iNeurotoxinTimeLeft > m_iNeurotoxinMaxTimeLeft)
		{
			m_flMillisecondsControlled = 0;
			m_iNeurotoxinTimeLeft = m_iNeurotoxinMaxTimeLeft;
		}
	}

}

void CPointNeurotoxin::InputSetMaxTimeLeft(inputdata_t &inputdata)
{
	m_iNeurotoxinMaxTimeLeft = inputdata.value.Int();
}

#endif

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Set the remaining time
//-----------------------------------------------------------------------------
void CNeurotoxinCountdown::SetRemainingTime()
{
	m_iRemainingTimeCountdown = iNeurotoxinTimeLeft;
	m_iMilliseconds = iMillisecondsControlled;
}

//-----------------------------------------------------------------------------
// Constantly update the remaining time to C_NeurotoxinCountdown
//-----------------------------------------------------------------------------
void CNeurotoxinCountdown::Think()
{
	SetRemainingTime();
	
	SetNextThink(gpGlobals->curtime);
}
#endif

#endif


#ifdef CLIENT_DLL

IMPLEMENT_CLIENTCLASS_DT(C_NeurotoxinCountdown, DT_NeurotoxinCountdown, CNeurotoxinCountdown)
	RecvPropBool( RECVINFO(m_bEnabled) ),
	RecvPropInt( RECVINFO(m_iRemainingTimeCountdown) ),
	RecvPropInt( RECVINFO(m_iMilliseconds) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA(C_NeurotoxinCountdown)

	DEFINE_PRED_FIELD(m_iRemainingTimeCountdown, FIELD_INTEGER ,FTYPEDESC_OVERRIDE),
	DEFINE_PRED_FIELD(m_iMilliseconds, FIELD_INTEGER, FTYPEDESC_OVERRIDE),

END_PREDICTION_DATA()


#endif

//NEUROTOXIN COUNTDOWN CODE
#ifdef CLIENT_DLL
CUtlVector< C_NeurotoxinCountdown* > g_NeurotoxinCountdowns;

C_NeurotoxinCountdown::C_NeurotoxinCountdown()
{
	g_NeurotoxinCountdowns.AddToTail(this);
}

C_NeurotoxinCountdown::~C_NeurotoxinCountdown()
{
	g_NeurotoxinCountdowns.FindAndRemove(this);
}

int C_NeurotoxinCountdown::GetMinutes(void)
{
	
	int m_iMinutes = m_iRemainingTimeCountdown / 60;
	return m_iMinutes;	
}

int C_NeurotoxinCountdown::GetSeconds(void)
{
	int m_iSeconds = m_iRemainingTimeCountdown % 60;
	return m_iSeconds;
}

int C_NeurotoxinCountdown::GetMilliseconds(void)
{
	return m_iMilliseconds;

	//return static_cast<int>(gpGlobals->curtime * 100.0f) % 100;;
}
#endif