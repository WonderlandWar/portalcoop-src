#ifndef C_PROP_GLADOS_CORE_H
#define C_PROP_GLADOS_CORE_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "c_physicsprop.h"

class C_PropGladosCore : public C_PhysicsProp
{
public:
	DECLARE_CLASS( C_PropGladosCore, C_PhysicsProp );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_PropGladosCore();
	~C_PropGladosCore();
	
	
	virtual void Spawn( void );
	virtual void Precache( void );
	
	virtual void ClientThink();
	
	virtual void OnDataChanged( DataUpdateType_t updateType );

	void	StartPanic ( void );
	void	StartTalking ( float flDelay );

	void	TalkingThink ( void );
	void	PanicThink ( void );

	void	SetupVOList ( void );
		
	typedef enum 
	{
		CORETYPE_CURIOUS,
		CORETYPE_AGGRESSIVE,
		CORETYPE_CRAZY,
		CORETYPE_NONE,
		CORETYPE_TOTAL,

	} CORETYPE;

	// Names of sound scripts for this core's personality
	CUtlVector<string_t> m_speechEvents;
	int m_iSpeechIter;

	string_t	m_iszPanicSoundScriptName;
	string_t	m_iszDeathSoundScriptName;
	
	CNetworkVar(bool, m_bStartTalking);
	CNetworkVar(bool, m_bStartPanic);
	CNetworkVar(float, m_flBetweenVOPadding);		// Spacing (in seconds) between VOs

	bool m_bOldStartTalking;
	bool m_bOldStartPanic;

	bool m_bSuppressTalkingThink;
	bool m_bShouldTalkingThink;
	bool m_bShouldPanicThink;

	int m_iTotalLines;

	
	CORETYPE m_iCoreType;
	
	virtual QAngle	PreferredCarryAngles( void ) { return QAngle( 180, -90, 180 ); }
	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return true; }

	virtual PINGICON GetPingIcon() { return PING_ICON_GLADOS_CORE; }

};
#endif //C_PROP_GLADOS_CORE_H