#include "cbase.h"
#include "c_prop_glados_core.h"
#include "c_baseentity.h"
#include "c_te_effect_dispatch.h"	// Sprite effect
#include "c_props.h"				// CPhysicsProp base class
#include "saverestore_utlvector.h"
#include "gamestringpool.h"

static const char *s_pTalkingThinkContext = "TalkingThinkContext";

IMPLEMENT_CLIENTCLASS_DT(C_PropGladosCore, DT_PropGladosCore, CPropGladosCore)
RecvPropBool(RECVINFO(m_bStartTalking)),
RecvPropBool(RECVINFO(m_bStartPanic)),
RecvPropFloat(RECVINFO(m_flBetweenVOPadding)),
RecvPropInt(RECVINFO(m_iCoreType)),
END_RECV_TABLE()
LINK_ENTITY_TO_CLASS(prop_glados_core, C_PropGladosCore)

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_PREDICTION_DATA( C_PropGladosCore )

	DEFINE_PRED_FIELD( m_iTotalLines, FIELD_INTEGER, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_iSpeechIter, FIELD_INTEGER, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_iszDeathSoundScriptName, FIELD_STRING, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_iszPanicSoundScriptName, FIELD_STRING, FTYPEDESC_NOERRORCHECK ),
	
	DEFINE_PRED_FIELD( m_bStartTalking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bStartPanic, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flBetweenVOPadding, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iCoreType, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_bOldStartTalking, FIELD_STRING, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_bOldStartPanic, FIELD_STRING, FTYPEDESC_NOERRORCHECK ),

	DEFINE_UTLVECTOR( m_speechEvents, FIELD_STRING ),

	/*
	DEFINE_THINKFUNC( TalkingThink ),
	DEFINE_THINKFUNC( PanicThink ),
	*/
		
END_PREDICTION_DATA()


C_PropGladosCore::C_PropGladosCore()
{
	m_bOldStartTalking = 0;
	m_bOldStartPanic = 0;

	m_iTotalLines = m_iSpeechIter = 0;

	m_iszDeathSoundScriptName = NULL_STRING;
	m_iszPanicSoundScriptName = NULL_STRING;

	m_flBetweenVOPadding = 2.5f;
}

C_PropGladosCore::~C_PropGladosCore()
{
	m_speechEvents.Purge();
}

void C_PropGladosCore::Spawn(void)
{
	Precache();
	BaseClass::Spawn();
}

void C_PropGladosCore::Precache( void )
{
	BaseClass::Precache();

	// Personality VOs -- Curiosity
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_1" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_2" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_3" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_4" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_5" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_6" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_7" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_8" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_9" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_10" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_11" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_12" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_13" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_15" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_16" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_17" );
	PrecacheScriptSound ( "Portal.Glados_core.Curiosity_18" );

	// Aggressive
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_00" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_01" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_02" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_03" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_04" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_05" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_06" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_07" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_08" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_09" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_10" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_11" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_12" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_13" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_14" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_15" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_16" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_17" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_18" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_19" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_20" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_21" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_panic_01" );
	PrecacheScriptSound ( "Portal.Glados_core.Aggressive_panic_02" );

	// Crazy
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_01" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_02" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_03" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_04" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_05" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_06" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_07" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_08" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_09" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_10" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_11" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_12" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_13" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_14" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_15" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_16" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_17" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_18" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_19" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_20" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_21" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_22" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_23" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_24" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_25" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_26" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_27" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_28" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_29" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_30" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_31" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_32" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_33" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_34" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_35" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_36" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_37" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_38" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_39" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_40" );
	PrecacheScriptSound ( "Portal.Glados_core.Crazy_41" );
}

void C_PropGladosCore::ClientThink()
{
#if 0
	
	if (m_bShouldPanicThink)
		Msg("m_bShouldPanicThink = true\n");
	else
		Msg("m_bShouldPanicThink = false\n");

	if (m_bShouldTalkingThink)
		Msg("m_bShouldTalkingThink = true\n");
	else
		Msg("m_bShouldTalkingThink = false\n");

#endif

	bool bShouldNextThink = true;

	if (m_bShouldPanicThink)
	{
		//Msg("PanicThink()\n");
		PanicThink();
		bShouldNextThink = false;
		Assert(0);
	}
	if (m_bShouldTalkingThink)
	{
		//Msg("TalkingThink()\n");
		TalkingThink();
		bShouldNextThink = false;
	}

	// We shouldn't ever stop thinking
	if (bShouldNextThink)
		SetNextClientThink(gpGlobals->curtime + 0.1f);
}

void C_PropGladosCore::OnDataChanged( DataUpdateType_t updateType )
{
	if (m_bStartTalking != m_bOldStartTalking)
	{
		float flTalkingDelay = (CORETYPE_CURIOUS == m_iCoreType) ? (2.0f) : (0.0f);

		if (m_bStartTalking)
		{
			//Msg("if (m_bStartTalking)\n");
			StartTalking(flTalkingDelay);
			SetupVOList();
		}
		m_bOldStartTalking = m_bStartTalking;
	}
	
	if (m_bStartPanic != m_bOldStartPanic)
	{
		if (m_bStartPanic)
		{
			//Msg("if (m_bStartPanic)\n");
			StartPanic();
		}
		m_bOldStartPanic = m_bStartPanic;
	}
}

void C_PropGladosCore::StartPanic( void )
{
	//Without this check, the crazy/cake core will skip to the next line without finishing its current line
	if (m_speechEvents.Count() <= 0 || !m_speechEvents.IsValidIndex(m_iSpeechIter) || m_iszPanicSoundScriptName == NULL_STRING)
	{
		m_bShouldPanicThink = false;
		m_bShouldTalkingThink = true;
		return;
	}

	m_bShouldTalkingThink = false;
	m_bShouldPanicThink = true;
	m_bSuppressTalkingThink = true;
		
	SetNextClientThink( gpGlobals->curtime + 0.1f );
}

void C_PropGladosCore::StartTalking( float flDelay )
{
	if ( m_speechEvents.IsValidIndex( m_iSpeechIter ) &&  m_speechEvents.Count() > 0 )
	{
		StopSound( m_speechEvents[m_iSpeechIter] );
	}

	m_iSpeechIter = 0;

	m_bShouldTalkingThink = true;
	m_bShouldPanicThink = false;

	float flThinkTime = gpGlobals->curtime + m_flBetweenVOPadding + flDelay;
		
	SetNextClientThink( flThinkTime );
}

//-----------------------------------------------------------------------------
// Purpose: Play panic vo and animations, then return to talking
// Output :
//-----------------------------------------------------------------------------
void C_PropGladosCore::PanicThink ( void )
{
	if ( m_speechEvents.Count() <= 0 || !m_speechEvents.IsValidIndex( m_iSpeechIter ) || m_iszPanicSoundScriptName == NULL_STRING )
	{
		m_bShouldPanicThink = false;
		m_bShouldTalkingThink = true;
		//TalkingThink();
		//SetNextClientThink( gpGlobals->curtime );
		return;
	}
	
	StopSound( m_speechEvents[m_iSpeechIter] );
	EmitSound( m_iszPanicSoundScriptName );

	float flCurDuration = GetSoundDuration(  m_iszPanicSoundScriptName, GetModelName() );

	float flThinkTime = gpGlobals->curtime + m_flBetweenVOPadding + flCurDuration;

	if (!m_bSuppressTalkingThink)
	{
		m_bShouldTalkingThink = true;
		m_bShouldPanicThink = false;
	}

	m_bSuppressTalkingThink = false;


	SetNextClientThink( flThinkTime );
}


//-----------------------------------------------------------------------------
// Purpose: Start playing personality VO list
//-----------------------------------------------------------------------------
void C_PropGladosCore::TalkingThink( void )
{
	if ( m_speechEvents.Count() <= 0 || !m_speechEvents.IsValidIndex( m_iSpeechIter ) )
	{
		m_bShouldTalkingThink = false;
		SetNextClientThink( gpGlobals->curtime );
		return;
	}

	int iPrevIter = m_iSpeechIter-1;
	if ( iPrevIter < 0 )
		iPrevIter = 0;
	
	StopSound( m_speechEvents[iPrevIter] );

	float flCurDuration = GetSoundDuration( m_speechEvents[m_iSpeechIter], GetModelName() );

	EmitSound( m_speechEvents[m_iSpeechIter] );
	SetNextClientThink( gpGlobals->curtime + m_flBetweenVOPadding + flCurDuration );


	// wrap if we hit the end of the list
	m_iSpeechIter = (m_iSpeechIter+1)%m_speechEvents.Count();

}


//-----------------------------------------------------------------------------
// Purpose: Setup list of lines based on core personality
//-----------------------------------------------------------------------------
void C_PropGladosCore::SetupVOList( void )
{
	m_speechEvents.RemoveAll();

	switch ( m_iCoreType )
	{
	case CORETYPE_CURIOUS:
		{
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_1" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_2" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_3" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_4" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_5" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_6" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_7" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_8" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_9" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_10" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_11" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_12" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_13" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_16" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Curiosity_17" ) );
			m_iszPanicSoundScriptName =  AllocPooledString( "Portal.Glados_core.Curiosity_15" );
			
		}
		break;
	case CORETYPE_AGGRESSIVE:
		{
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_01" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_02" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_03" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_04" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_05" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_06" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_07" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_08" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_09" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_10" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_11" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_12" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_13" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_14" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_15" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_16" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_17" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_18" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_19" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_20" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Aggressive_21" ) );
			m_iszPanicSoundScriptName = AllocPooledString( "Portal.Glados_core.Aggressive_panic_01" );
		}
		break;
	case CORETYPE_CRAZY:
		{
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_01" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_02" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_03" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_04" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_05" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_06" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_07" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_08" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_09" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_10" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_11" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_12" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_13" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_14" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_15" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_16" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_17" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_18" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_19" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_20" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_21" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_22" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_23" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_24" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_25" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_26" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_27" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_28" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_29" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_30" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_31" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_32" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_33" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_34" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_35" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_36" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_37" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_38" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_39" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_40" ) );
			m_speechEvents.AddToTail( AllocPooledString( "Portal.Glados_core.Crazy_41" ) );
		}
		break;
	};

	m_iszDeathSoundScriptName =  AllocPooledString( "Portal.Glados_core.Death" );
	m_iTotalLines = m_speechEvents.Count();
	m_iSpeechIter = 0;
}