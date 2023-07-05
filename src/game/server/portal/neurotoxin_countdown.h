//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the big scary boom-boom machine Antlions fear.
//
//=============================================================================//
#ifndef NEUROTOXIN_COUNTDOWN_H
#define NEUROTOXIN_COUNTDOWN_H
#include "cbase.h"
#include "EnvMessage.h"
#include "fmtstr.h"
#include "vguiscreen.h"
#include "filesystem.h"


struct SlideKeywordList_t
{
	char	szSlideKeyword[64];
};


class CNeurotoxinCountdown : public CBaseEntity
{
public:

	DECLARE_CLASS( CNeurotoxinCountdown, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual ~CNeurotoxinCountdown();

	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	virtual int  UpdateTransmitState();
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void OnRestore( void );
	
	CNetworkVar(int, m_iRemainingTimeCountdown);
	CNetworkVar(int, m_iMilliseconds)

	void		SetRemainingTime(void);
	void	Think();

	void	ScreenVisible( bool bVisible );

	void	Disable( void );
	void	Enable( void );

	void	InputDisable( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );

	int		m_iTickMilliseconds;
	int		GetMilliseconds(void);

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );

private:

	CNetworkVar( bool, m_bEnabled );

	int		m_iScreenWidth;
	int		m_iScreenHeight;

	typedef CHandle<CVGuiScreen>	ScreenHandle_t;
	CUtlVector<ScreenHandle_t>	m_hScreens;
};

#endif // NEUROTOXIN_COUNTDOWN_H