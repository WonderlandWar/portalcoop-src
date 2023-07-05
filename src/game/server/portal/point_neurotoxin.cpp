#include "cbase.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "point_neurotoxin.h"
#include "portal_player.h"
#include "networkvar.h"
#include "util.h"


// Useless, old, and rubbish code

/*
void CNeurotoxin::Start()
{
	flNeurotoxinTime = flNeurotoxinTimeLeft;
	m_bShouldBeTicking = true;
}

void CNeurotoxin::Stop()
{
	m_bShouldBeTicking = false;
	m_bShouldDoDamage = false;
}
void CNeurotoxin::Pause()
{
	m_bShouldBeTicking = false;
}

void CNeurotoxin::Resume()
{
	m_bShouldBeTicking = true;
}

void CNeurotoxin::ThinkTime()
{
	if (m_bShouldBeTicking)
	{
		flNeurotoxinTimeLeft = flNeurotoxinTimeLeft - gpGlobals->curtime;
	}
	if (flNeurotoxinTimeLeft <= 0)
	{
		m_bShouldDoDamage = true;
	}

}
void CNeurotoxin::OnTimeEnded()
{
	CPointNeurotoxin* pPointNeuro;
	if (m_hPointNeurotoxinCaller = pPointNeuro)
	{

	}
	m_bShouldDoDamage = true;
}

*/


BEGIN_DATADESC(CPointNeurotoxin)

	DEFINE_INPUTFUNC(FIELD_VOID, "Start",	InputStart),
	DEFINE_INPUTFUNC(FIELD_VOID, "Stop",	InputStop),
	DEFINE_INPUTFUNC(FIELD_VOID, "Pause",	InputPause),
	DEFINE_INPUTFUNC(FIELD_VOID, "Resume",	InputPause),

	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetNeurotoxinTimeInSeconds", InputSetNeurotoxinTimeInSeconds),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "AddNeurotoxinTimeInSeconds", InputAddNeurotoxinTimeInSeconds),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SubtractNeurotoxinTimeInSeconds", InputSubtractNeurotoxinTimeInSeconds),

	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetNeurotoxinTimeLeftInSeconds", InputSetNeurotoxinTimeLeftInSeconds),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "AddNeurotoxinTimeLeftInSeconds", InputAddNeurotoxinTimeLeftInSeconds),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SubtractNeurotoxinTimeLeftInSeconds", InputSubtractNeurotoxinTimeLeftInSeconds),

	DEFINE_THINKFUNC(ThinkTimer),
	DEFINE_THINKFUNC(DamagePlayersThink),

	DEFINE_KEYFIELD(m_flNeurotoxinTime, FIELD_FLOAT, "NeurotoxinTime"),

	DEFINE_OUTPUT(m_OnStart, "OnStart"),
	DEFINE_OUTPUT(m_OnStop, "OnStop"),
	DEFINE_OUTPUT(m_OnPause, "OnPause"),
	DEFINE_OUTPUT(m_OnResume, "OnResume"),
	DEFINE_OUTPUT(m_OnTimerEnded, "OnTimerEnded"),

END_DATADESC();

IMPLEMENT_SERVERCLASS_ST(CPointNeurotoxin, DT_Point_Neurotoxin)

	SendPropInt(SENDINFO(m_flNeurotoxinTimeLeft)),

END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(point_neurotoxin, CPointNeurotoxin)

CPointNeurotoxin::CPointNeurotoxin()
{
	m_bInProgress = false;
}

void CPointNeurotoxin::Start()
{
	if (m_bInProgress)
		return;
	m_bInProgress = true;
	SetThink(&CPointNeurotoxin::ThinkTimer);
	SetNextThink(gpGlobals->curtime);

	m_flNeurotoxinTimeLeft = m_flNeurotoxinTime;
	m_bShouldBeTicking = true;

	m_OnStart.FireOutput(this, this);
}

void CPointNeurotoxin::Stop()
{
	if (!m_bInProgress)
		return;
	m_bInProgress = false;
	SetThink(NULL);
	m_bShouldBeTicking = false;
	m_bShouldDoDamage = false;

	m_OnStop.FireOutput(this, this);
}
void CPointNeurotoxin::Pause()
{
	if (!m_bInProgress)
		return;
	m_bShouldBeTicking = false;

	m_OnPause.FireOutput(this, this);
}

void CPointNeurotoxin::Resume()
{
	if (!m_bInProgress)
		return;
	m_bShouldBeTicking = true;

	m_OnResume.FireOutput(this, this);
}

void CPointNeurotoxin::ThinkTimer()
{
	if (m_bShouldBeTicking)
	{
		m_flNeurotoxinTimeLeft = m_flNeurotoxinTimeLeft - 1;
	}
	if (m_flNeurotoxinTimeLeft <= 0)
	{
		m_flNeurotoxinTimeLeft = 0;
		m_bShouldDoDamage = true;
		m_bShouldBeTicking = false;
	}

	if (m_bShouldDoDamage)
	{
		SetThink(&CPointNeurotoxin::DamagePlayersThink);
	}

	SetNextThink(gpGlobals->curtime + 1);
}

void CPointNeurotoxin::DamagePlayersThink()
{
	if (m_flNeurotoxinTimeLeft >= 1)
	{
		SetThink(&CPointNeurotoxin::ThinkTimer);
		m_bShouldDoDamage = false;
		m_bShouldBeTicking = true;
	}

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

	SetNextThink(gpGlobals->curtime);
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

void CPointNeurotoxin::InputSetNeurotoxinTimeInSeconds(inputdata_t &inputdata)
{
	m_flNeurotoxinTime	= inputdata.value.Float();

	if (m_flNeurotoxinTime < 0)
			m_flNeurotoxinTime = 0;
}

void CPointNeurotoxin::InputAddNeurotoxinTimeInSeconds(inputdata_t &inputdata)
{
	m_flNeurotoxinTime = m_flNeurotoxinTime + inputdata.value.Float();
	if (m_flNeurotoxinTime < 0)
			m_flNeurotoxinTime = 0;
}


void CPointNeurotoxin::InputSubtractNeurotoxinTimeInSeconds(inputdata_t &inputdata)
{
	m_flNeurotoxinTime = m_flNeurotoxinTime - inputdata.value.Float();

	if (m_flNeurotoxinTime < 0)
			m_flNeurotoxinTime = 0;
}

void CPointNeurotoxin::InputSetNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	m_flNeurotoxinTimeLeft = inputdata.value.Float();

	if (m_flNeurotoxinTimeLeft < 0)
			m_flNeurotoxinTimeLeft = 0;
}

void CPointNeurotoxin::InputAddNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	m_flNeurotoxinTimeLeft = m_flNeurotoxinTimeLeft + inputdata.value.Float();
	if (m_flNeurotoxinTimeLeft < 0)
			m_flNeurotoxinTimeLeft = 0;
}


void CPointNeurotoxin::InputSubtractNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	m_flNeurotoxinTimeLeft = m_flNeurotoxinTimeLeft - inputdata.value.Float();

	if (m_flNeurotoxinTimeLeft < 0)
			m_flNeurotoxinTimeLeft = 0;
}

float CPointNeurotoxin::GetRemainingTime()
{
	return	m_flNeurotoxinTimeLeft;
}

// This is also useless, old and rubbish code...
/*
void CPointNeurotoxin::FireOutput_TimerEnded()
{
	m_OnTimerEnded.FireOutput(this, this);
}

void CPointNeurotoxin::SetPointNeurotoxinCaller(CPointNeurotoxin* caller)
{
	m_hPointNeurotoxinCaller = caller;
}
void GetNeurotoxinCaller()
{
	return m_hPointNeurotoxinCaller;
}
*/