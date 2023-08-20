//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the big scary boom-boom machine Antlions fear.
//
//=============================================================================//

#include "cbase.h"
#include "baseanimating.h"
#include "portal_player.h"
#include "EnvMessage.h"
#include "fmtstr.h"
#include "vguiscreen.h"
#include "point_bonusmaps_accessor.h"
#include "portal_shareddefs.h"
#include "portal_gamerules.h"
#include "props.h"
#include "gameui/bonusmapsdatabase.h"

#define PORTAL_STATS_DISPLAY_MODEL_NAME "models/props/Round_elevator_body.mdl"


class CPropPortalStatsDisplay : public CDynamicProp
{
public:

	DECLARE_CLASS( CPropPortalStatsDisplay, CDynamicProp );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual ~CPropPortalStatsDisplay();

	virtual int  UpdateTransmitState();
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void OnRestore( void );

	void	ScreenVisible( bool bVisible );

	void	Disable( void );
	void	Enable( void );

	void	InputDisable( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );

	void	InputUpdateStats( inputdata_t &inputdata );
	void	InputResetPlayerStats( inputdata_t &inputdata );
	void	InputTestChallengeMode( inputdata_t &inputdata );

private:

	CNetworkVar( bool, m_bEnabled );

	CNetworkVar( int, m_iNumPortalsPlaced );
	CNetworkVar( int, m_iNumStepsTaken );
	CNetworkVar( float, m_fNumSecondsTaken );
	
	CNetworkVar( int, m_iBronzeObjective );
	CNetworkVar( int, m_iSilverObjective );
	CNetworkVar( int, m_iGoldObjective );

	CNetworkString( szChallengeFileName, 128 );
	CNetworkString( szChallengeMapName, 32 );
	CNetworkString( szChallengeName, 32 );

	CNetworkVar( int, m_iDisplayObjective );
	
	COutputEvent m_OnMetBronzeObjective;
	COutputEvent m_OnMetSilverObjective;
	COutputEvent m_OnMetGoldObjective;
	COutputEvent m_OnFailedAllObjectives;

	COutputEvent m_IfIsChallengeMode;
	COutputEvent m_IfIsNotChallengeMode;

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );

	typedef CHandle<CVGuiScreen>	ScreenHandle_t;
	CUtlVector<ScreenHandle_t>	m_hScreens;

};


LINK_ENTITY_TO_CLASS( prop_portal_stats_display, CPropPortalStatsDisplay );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CPropPortalStatsDisplay )
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_iNumPortalsPlaced, FIELD_INTEGER ),
	DEFINE_FIELD( m_iNumStepsTaken, FIELD_INTEGER ),
	DEFINE_FIELD( m_fNumSecondsTaken, FIELD_FLOAT ),

#if 1
	DEFINE_FIELD( m_iBronzeObjective, FIELD_INTEGER ),
	DEFINE_FIELD( m_iSilverObjective, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGoldObjective, FIELD_INTEGER ),
#endif

	DEFINE_AUTO_ARRAY( szChallengeFileName, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( szChallengeMapName, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( szChallengeName, FIELD_CHARACTER ),

	DEFINE_FIELD( m_iDisplayObjective, FIELD_INTEGER ),

	//DEFINE_UTLVECTOR( m_hScreens, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UpdateStats", InputUpdateStats ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetPlayerStats", InputResetPlayerStats ),
	
	DEFINE_INPUTFUNC( FIELD_VOID, "TestChallengeMode", InputTestChallengeMode ),

	DEFINE_OUTPUT ( m_OnMetBronzeObjective, "OnMetBronzeObjective" ),
	DEFINE_OUTPUT ( m_OnMetSilverObjective, "OnMetSilverObjective" ),
	DEFINE_OUTPUT ( m_OnMetGoldObjective, "OnMetGoldObjective" ),
	DEFINE_OUTPUT ( m_OnFailedAllObjectives, "OnFailedAllObjectives" ),
	
	DEFINE_OUTPUT ( m_IfIsChallengeMode, "IfIsChallengeMode" ),	
	DEFINE_OUTPUT ( m_IfIsNotChallengeMode, "IfIsNotChallengeMode" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPropPortalStatsDisplay, DT_PropPortalStatsDisplay )
	SendPropBool( SENDINFO(m_bEnabled) ),

	SendPropInt( SENDINFO(m_iNumPortalsPlaced) ),
	SendPropInt( SENDINFO(m_iNumStepsTaken) ),
	SendPropFloat( SENDINFO(m_fNumSecondsTaken) ),
#if 1
	SendPropInt( SENDINFO(m_iBronzeObjective) ),
	SendPropInt( SENDINFO(m_iSilverObjective) ),
	SendPropInt( SENDINFO(m_iGoldObjective) ),
#endif
	SendPropString( SENDINFO( szChallengeFileName ) ),
	SendPropString( SENDINFO( szChallengeMapName ) ),
	SendPropString( SENDINFO( szChallengeName ) ),

	SendPropInt( SENDINFO(m_iDisplayObjective) ),
END_SEND_TABLE()


CPropPortalStatsDisplay::~CPropPortalStatsDisplay()
{
	int i;
	// Kill the control panels
	for ( i = m_hScreens.Count(); --i >= 0; )
	{
		DestroyVGuiScreen( m_hScreens[i].Get() );
	}
	m_hScreens.RemoveAll();
}

int CPropPortalStatsDisplay::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CPropPortalStatsDisplay::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	// Force our screens to be sent too.
	for ( int i=0; i < m_hScreens.Count(); i++ )
	{
		CVGuiScreen *pScreen = m_hScreens[i].Get();
		pScreen->SetTransmit( pInfo, bAlways );
	}
}

void CPropPortalStatsDisplay::Spawn( void )
{
	char *szModel = (char *)STRING( GetModelName() );
	if (!szModel || !*szModel)
	{
		szModel = PORTAL_STATS_DISPLAY_MODEL_NAME;
		SetModelName( AllocPooledString(szModel) );
	}

	Precache();
	SetModel( szModel );

	SetSolid( SOLID_VPHYSICS );
	//VPhysicsInitStatic();

	BaseClass::Spawn();

	int iBronze, iSilver, iGold;

	BonusMapChallengeObjectives( iBronze, iSilver, iGold );

	m_iBronzeObjective = iBronze;
	m_iSilverObjective = iSilver;
	m_iGoldObjective = iGold;

	
	BonusMapsDatabase()->GetCurrentChallengeNames( szChallengeFileName.GetForModify(), szChallengeMapName.GetForModify(), szChallengeName.GetForModify() );

	m_bEnabled = false;

	int iSequence = SelectHeaviestSequence ( ACT_IDLE );

	if ( iSequence != ACT_INVALID )
	{
		 SetSequence( iSequence );
		 ResetSequenceInfo();

		 //Do this so we get the nice ramp-up effect.
		 m_flPlaybackRate = random->RandomFloat( 0.0f, 1.0f );
	}

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );
}

void CPropPortalStatsDisplay::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( STRING( GetModelName() ) );

	PrecacheVGuiScreen( "portal_stats_display_screen" );
}

void CPropPortalStatsDisplay::OnRestore( void )
{
	BaseClass::OnRestore();

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

void CPropPortalStatsDisplay::ScreenVisible( bool bVisible )
{
	for ( int iScreen = 0; iScreen < m_hScreens.Count(); ++iScreen )
	{
		CVGuiScreen *pScreen = m_hScreens[ iScreen ].Get();
		if ( bVisible )
			pScreen->RemoveEffects( EF_NODRAW );
		else
			pScreen->AddEffects( EF_NODRAW );
	}
}

void CPropPortalStatsDisplay::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	ScreenVisible( false );
}

void CPropPortalStatsDisplay::Enable( void )
{
	if ( m_bEnabled )
		return;

	// Don't show stats display in non challenge mode!
	if ( PortalGameRules()->GetBonusChallenge() == 0 )
		return;
	
	IGameEvent *event = gameeventmanager->CreateEvent( "bonusmap_save" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}

	m_bEnabled = true;

	m_iDisplayObjective = PortalGameRules()->GetBonusChallenge() - 1;


#if 1 // This sadly won't be supported in portalcoop
	// Check if they beat objectives
	if ( PortalGameRules()->GetBonusProgress() <= m_iBronzeObjective )
		m_OnMetBronzeObjective.FireOutput( this, this );
	else if ( PortalGameRules()->GetBonusProgress() <= m_iSilverObjective )
		m_OnMetSilverObjective.FireOutput( this, this );
	else if ( PortalGameRules()->GetBonusProgress() <= m_iGoldObjective )
		m_OnMetGoldObjective.FireOutput( this, this );
	else
		m_OnFailedAllObjectives.FireOutput( this, this );
#endif

	ScreenVisible( true );
}

void CPropPortalStatsDisplay::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

void CPropPortalStatsDisplay::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

void CPropPortalStatsDisplay::InputUpdateStats( inputdata_t &inputdata )
{
	{
		m_iNumPortalsPlaced = PortalGameRules()->NumPortalsPlaced();
		m_iNumStepsTaken = PortalGameRules()->NumStepsTaken();
		m_fNumSecondsTaken = PortalGameRules()->NumSecondsTaken();

		// Now that we've recorded it, don't let it change
		PortalGameRules()->PauseBonusProgress();
	}
}

void CPropPortalStatsDisplay::InputResetPlayerStats( inputdata_t &inputdata )
{
	PortalGameRules()->ResetThisLevelStats();
}

void CPropPortalStatsDisplay::InputTestChallengeMode( inputdata_t &inputdata )
{
	if ( g_pGameRules->GetBonusChallenge() )
		m_IfIsChallengeMode.FireOutput( inputdata.pActivator, inputdata.pCaller );
	else
		m_IfIsNotChallengeMode.FireOutput( inputdata.pActivator, inputdata.pCaller );
}
void CPropPortalStatsDisplay::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "portal_stats_display_screen";
}

void CPropPortalStatsDisplay::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CPropPortalStatsDisplay::SpawnControlPanels()
{
	char buf[64];

	// FIXME: Deal with dynamically resizing control panels?

	// If we're attached to an entity, spawn control panels on it instead of use
	CBaseAnimating *pEntityToSpawnOn = this;
	char *pOrgLL = "statPanel%d_bl";
	char *pOrgUR = "statPanel%d_tr";
	char *pAttachmentNameLL = pOrgLL;
	char *pAttachmentNameUR = pOrgUR;

	Assert( pEntityToSpawnOn );

	// Lookup the attachment point...
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		Q_snprintf( buf, sizeof( buf ), pAttachmentNameLL, nPanel );
		int nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nLLAttachmentIndex <= 0)
		{
			// Try and use my panels then
			pEntityToSpawnOn = this;
			Q_snprintf( buf, sizeof( buf ), pOrgLL, nPanel );
			nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nLLAttachmentIndex <= 0)
				return;
		}

		Q_snprintf( buf, sizeof( buf ), pAttachmentNameUR, nPanel );
		int nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nURAttachmentIndex <= 0)
		{
			// Try and use my panels then
			Q_snprintf( buf, sizeof( buf ), pOrgUR, nPanel );
			nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nURAttachmentIndex <= 0)
				return;
		}

		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		// Compute the screen size from the attachment points...
		matrix3x4_t	panelToWorld;
		pEntityToSpawnOn->GetAttachment( nLLAttachmentIndex, panelToWorld );

		matrix3x4_t	worldToPanel;
		MatrixInvert( panelToWorld, worldToPanel );

		// Now get the lower right position + transform into panel space
		Vector lr, lrlocal;
		pEntityToSpawnOn->GetAttachment( nURAttachmentIndex, panelToWorld );
		MatrixGetColumn( panelToWorld, 3, lr );
		VectorTransform( lr, worldToPanel, lrlocal );

		float flWidth = lrlocal.x;
		float flHeight = lrlocal.y;

		CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, pEntityToSpawnOn, this, nLLAttachmentIndex );
		pScreen->ChangeTeam( GetTeamNumber() );
		pScreen->SetActualSize( flWidth, flHeight );
		pScreen->SetActive( true );
		pScreen->MakeVisibleOnlyToTeammates( false );
		pScreen->SetTransparency( true );
		int nScreen = m_hScreens.AddToTail( );
		m_hScreens[nScreen].Set( pScreen );	
	}
}

void CPropPortalStatsDisplay::RestoreControlPanels( void )
{
	char buf[64];

	// FIXME: Deal with dynamically resizing control panels?

	// If we're attached to an entity, spawn control panels on it instead of use
	CBaseAnimating *pEntityToSpawnOn = this;
	char *pOrgLL = "statPanel%d_bl";
	char *pOrgUR = "statPanel%d_tr";
	char *pAttachmentNameLL = pOrgLL;
	char *pAttachmentNameUR = pOrgUR;

	Assert( pEntityToSpawnOn );

	// Lookup the attachment point...
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		Q_snprintf( buf, sizeof( buf ), pAttachmentNameLL, nPanel );
		int nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nLLAttachmentIndex <= 0)
		{
			// Try and use my panels then
			pEntityToSpawnOn = this;
			Q_snprintf( buf, sizeof( buf ), pOrgLL, nPanel );
			nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nLLAttachmentIndex <= 0)
				return;
		}

		Q_snprintf( buf, sizeof( buf ), pAttachmentNameUR, nPanel );
		int nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nURAttachmentIndex <= 0)
		{
			// Try and use my panels then
			Q_snprintf( buf, sizeof( buf ), pOrgUR, nPanel );
			nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nURAttachmentIndex <= 0)
				return;
		}

		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		CVGuiScreen *pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( NULL, pScreenClassname );

		while ( pScreen && pScreen->GetOwnerEntity() != this && Q_strcmp( pScreen->GetPanelName(), pScreenName ) == 0 )
		{
			pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( pScreen, pScreenClassname );
		}

		if ( pScreen )
		{
			int nScreen = m_hScreens.AddToTail( );
			m_hScreens[nScreen].Set( pScreen );	
		}
	}
}