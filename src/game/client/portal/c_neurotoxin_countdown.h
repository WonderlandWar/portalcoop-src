//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef C_NEUROTOXIN_COUNTDOWN_H
#define C_NEUROTOXIN_COUNTDOWN_H

#include "cbase.h"
#include "utlvector.h"

class C_NeurotoxinCountdown : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_NeurotoxinCountdown, C_BaseEntity );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_NeurotoxinCountdown();
	virtual ~C_NeurotoxinCountdown();

	bool IsEnabled( void ) { return m_bEnabled; }

	CNetworkVar(int, m_iRemainingTimeCountdown);
	CNetworkVar(int, m_iMilliseconds)
	

	int m_iMinutes;
	int m_iSeconds;
	
	int GetMinutes( void );
	int GetSeconds( void );
	int GetMilliseconds( void );

private:

	bool	m_bEnabled;
};


extern CUtlVector< C_NeurotoxinCountdown* > g_NeurotoxinCountdowns;


#endif //C_NEUROTOXIN_COUNTDOWN_H