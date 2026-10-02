#include "cbase.h"
#include "point_neurotoxin.h"
//#include "neurotoxin_countdown.h"
#ifdef CLIENT_DLL
#include "c_neurotoxin_countdown.h"
#else
#include "neurotoxin_countdown.h"
#endif

CPointNeurotoxin *g_pActiveNeurotoxin = NULL;
IMPLEMENT_NETWORKCLASS_ALIASED( PointNeurotoxin, DT_PointNeurotoxin )
BEGIN_NETWORK_TABLE( CPointNeurotoxin, DT_PointNeurotoxin )
#ifdef GAME_DLL
	SendPropDataTable( SENDINFO_DT( m_MainTimer ), &REFERENCE_SEND_TABLE( DT_NetworkedCountdownTimer ) ),
	SendPropBool( SENDINFO( m_bPaused ) ),
	SendPropFloat( SENDINFO ( m_flPausedTimeLeft ) ),
#else
	RecvPropDataTable( RECVINFO_DT( m_MainTimer ), 0, &REFERENCE_RECV_TABLE( DT_NetworkedCountdownTimer ) ),
	RecvPropBool( RECVINFO( m_bPaused ) ),
	RecvPropFloat( RECVINFO ( m_flPausedTimeLeft ) ),
#endif
END_NETWORK_TABLE()

#ifndef CLIENT_DLL

static const char *s_pszTimerEndThinkContext = "TimerEndThinkContext";

BEGIN_DATADESC(CPointNeurotoxin)
#ifdef GAME_DLL
	DEFINE_INPUTFUNC(FIELD_VOID, "Start",	InputStart),
	DEFINE_INPUTFUNC(FIELD_VOID, "Stop",	InputStop),
	DEFINE_INPUTFUNC(FIELD_VOID, "Pause",	InputPause),
	DEFINE_INPUTFUNC(FIELD_VOID, "Resume",	InputResume),

	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetNeurotoxinTimeInSeconds", InputSetNeurotoxinTimeInSeconds),

	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetNeurotoxinTimeLeftInSeconds", InputSetNeurotoxinTimeLeftInSeconds),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "AddNeurotoxinTimeLeftInSeconds", InputAddNeurotoxinTimeLeftInSeconds),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SubtractNeurotoxinTimeLeftInSeconds", InputSubtractNeurotoxinTimeLeftInSeconds),

	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetMaxTimeLeft", InputSetMaxTimeLeft),

	DEFINE_KEYFIELD(m_flNeurotoxinTime, FIELD_FLOAT, "NeurotoxinTime"),
	DEFINE_KEYFIELD(m_flNeurotoxinMaxTimeLeft, FIELD_FLOAT, "NeurotoxinMaxTimeLeft"),

	DEFINE_OUTPUT(m_OnStart, "OnStart"),
	DEFINE_OUTPUT(m_OnStop, "OnStop"),
	DEFINE_OUTPUT(m_OnPause, "OnPause"),
	DEFINE_OUTPUT(m_OnResume, "OnResume"),
	DEFINE_OUTPUT(m_OnTimerEnded, "OnTimerEnded"),

	DEFINE_THINKFUNC(TimerEndThink),
	DEFINE_THINKFUNC(DamagePlayersThink),
#endif

END_DATADESC();

LINK_ENTITY_TO_CLASS( point_neurotoxin, CPointNeurotoxin )

#endif // !CLIENT_DLL

CPointNeurotoxin::CPointNeurotoxin()
{
#ifndef CLIENT_DLL
	m_flNeurotoxinMaxTimeLeft = 0;
	m_flNeurotoxinTime = 0;
#endif

	m_bPaused = false;
	m_flPausedTimeLeft = 0;
}

CPointNeurotoxin::~CPointNeurotoxin()
{
	if ( g_pActiveNeurotoxin == this )
	{
		g_pActiveNeurotoxin = NULL;
	}
}
#ifndef CLIENT_DLL
void CPointNeurotoxin::Start()
{
	if ( InProgress() )
		return;
	
	if ( g_pActiveNeurotoxin )
	{
		g_pActiveNeurotoxin->Stop();
	}

	g_pActiveNeurotoxin = this;

	m_MainTimer.Start( m_flNeurotoxinTime );

	m_flPausedTimeLeft = 0;

	SetThink( &CPointNeurotoxin::DamagePlayersThink );
	SetNextThink( gpGlobals->curtime + m_flNeurotoxinTime );
	SetContextThink( &CPointNeurotoxin::TimerEndThink, gpGlobals->curtime + m_flNeurotoxinTime, s_pszTimerEndThinkContext );

	m_OnStart.FireOutput(this, this);
}

void CPointNeurotoxin::Stop()
{
	if ( !InProgress() )
		return;

	if ( g_pActiveNeurotoxin == this )
	{
		g_pActiveNeurotoxin = NULL;
	}

	m_flPausedTimeLeft = 0;

	SetThink(NULL);
	SetContextThink( NULL, 0, s_pszTimerEndThinkContext );

	m_MainTimer.Invalidate();

#ifndef CLIENT_DLL
	m_OnStop.FireOutput(this, this);
#endif
}
void CPointNeurotoxin::Pause()
{
	if ( m_bPaused )
		return;

	if ( !InProgress() )
		return;

	if ( IsDamaging() )
		return;

	m_bPaused = true;
	
	m_flPausedTimeLeft = m_MainTimer.GetRemainingTime();

	SetThink( NULL );
	SetContextThink( NULL, 0, s_pszTimerEndThinkContext );

	m_OnPause.FireOutput(this, this);
}

void CPointNeurotoxin::Resume()
{
	if ( !m_bPaused )
		return;

	if ( !InProgress() )
		return;

	if ( IsDamaging() )
		return;

	m_bPaused = false;
	
	m_MainTimer.Start( m_flPausedTimeLeft );
	
	SetThink( &CPointNeurotoxin::DamagePlayersThink );
	SetNextThink( gpGlobals->curtime + m_flPausedTimeLeft );
	SetContextThink( &CPointNeurotoxin::TimerEndThink, gpGlobals->curtime + m_flNeurotoxinTime, s_pszTimerEndThinkContext );

	m_flPausedTimeLeft = 0;

	m_OnResume.FireOutput(this, this);
}

void CPointNeurotoxin::TimerEndThink()
{
	m_OnTimerEnded.FireOutput( this, this );
}

void CPointNeurotoxin::DamagePlayersThink()
{
	if ( !IsDamaging() )
	{
		SetNextThink( gpGlobals->curtime + m_MainTimer.GetRemainingTime() );
		return;
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
	if (m_flNeurotoxinMaxTimeLeft <= 0)
		return false;
	else
		return true;
}

void CPointNeurotoxin::ClampMainTimer()
{
	float elapsed;
	if ( ShouldUseMaxTimeLeft() )
	{
		elapsed = m_MainTimer.GetRemainingTime();
		elapsed = clamp( elapsed, 0.0, m_flNeurotoxinMaxTimeLeft );
		m_MainTimer.Start( elapsed );
	
		if ( m_bPaused )
		{
			m_flPausedTimeLeft = clamp( m_flPausedTimeLeft, 0.0, m_flNeurotoxinMaxTimeLeft );
		}
	}
	else
	{
		elapsed = MAX( 0.0, m_MainTimer.GetRemainingTime() );
		m_MainTimer.Start( elapsed );
	
		if ( m_bPaused )
		{
			m_flPausedTimeLeft = MAX( 0.0, m_flPausedTimeLeft );
		}
	}
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
	m_flNeurotoxinTime = MAX( 0, inputdata.value.Float() );
}

void CPointNeurotoxin::InputSetNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	if ( m_bPaused )
	{
		m_flPausedTimeLeft = inputdata.value.Float();
	}
	else
	{
		m_MainTimer.Start( inputdata.value.Float() );
	}
	ClampMainTimer();
}

void CPointNeurotoxin::InputAddNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	if ( m_bPaused )
	{
		m_flPausedTimeLeft += inputdata.value.Float();
	}
	else
	{
		m_MainTimer.Start( m_MainTimer.GetRemainingTime() + inputdata.value.Float() );
	}
	ClampMainTimer();
}


void CPointNeurotoxin::InputSubtractNeurotoxinTimeLeftInSeconds(inputdata_t &inputdata)
{
	if ( m_bPaused )
	{
		m_flPausedTimeLeft -= inputdata.value.Float();
	}
	else
	{
		m_MainTimer.Start( m_MainTimer.GetRemainingTime() - inputdata.value.Float() );
	}
	ClampMainTimer();
}

void CPointNeurotoxin::InputSetMaxTimeLeft(inputdata_t &inputdata)
{
	m_flNeurotoxinMaxTimeLeft = inputdata.value.Float();
}

#else


void CPointNeurotoxin::OnDataChanged( DataUpdateType_t updatetype )
{
	BaseClass::OnDataChanged( updatetype );
	if ( m_bOldInProgress != InProgress() )
	{
		if ( InProgress() )
		{
			g_pActiveNeurotoxin = this;
		}
		else
		{
			if ( g_pActiveNeurotoxin == this )
			{
				g_pActiveNeurotoxin = NULL;
			}
		}
		m_bOldInProgress = InProgress();
	}
}

#endif // !CLIENT_DLL