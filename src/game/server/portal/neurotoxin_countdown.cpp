//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the big scary boom-boom machine Antlions fear.
//
//=============================================================================//

#include "cbase.h"
#include "EnvMessage.h"
#include "fmtstr.h"
#include "vguiscreen.h"
#include "filesystem.h"
#include "neurotoxin_countdown.h"

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CNeurotoxinCountdown )
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_iScreenWidth, FIELD_INTEGER, "width" ),
	DEFINE_KEYFIELD( m_iScreenHeight, FIELD_INTEGER, "height" ),

	//DEFINE_UTLVECTOR( m_hScreens, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),

	DEFINE_THINKFUNC(Think),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CNeurotoxinCountdown, DT_NeurotoxinCountdown )
	SendPropBool( SENDINFO(m_bEnabled) ),
	SendPropInt( SENDINFO(m_iRemainingTimeCountdown) ),
	SendPropInt( SENDINFO(m_iMilliseconds) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( vgui_neurotoxin_countdown, CNeurotoxinCountdown );

CNeurotoxinCountdown::~CNeurotoxinCountdown()
{
	int i;
	// Kill the control panels
	for ( i = m_hScreens.Count(); --i >= 0; )
	{
		DestroyVGuiScreen( m_hScreens[i].Get() );
	}
	m_hScreens.RemoveAll();
}

//-----------------------------------------------------------------------------
// Read in worldcraft data...
//-----------------------------------------------------------------------------
bool CNeurotoxinCountdown::KeyValue( const char *szKeyName, const char *szValue ) 
{
	//!! temp hack, until worldcraft is fixed
	// strip the # tokens from (duplicate) key names
	char *s = (char *)strchr( szKeyName, '#' );
	if ( s )
	{
		*s = '\0';
	}

	// NOTE: Have to do these separate because they set two values instead of one
	if( FStrEq( szKeyName, "angles" ) )
	{
		Assert( GetMoveParent() == NULL );
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );

		// Because the vgui screen basis is strange (z is front, y is up, x is right)
		// we need to rotate the typical basis before applying it
		VMatrix mat, rotation, tmp;
		MatrixFromAngles( angles, mat );
		MatrixBuildRotationAboutAxis( rotation, Vector( 0, 1, 0 ), 90 );
		MatrixMultiply( mat, rotation, tmp );
		MatrixBuildRotateZ( rotation, 90 );
		MatrixMultiply( tmp, rotation, mat );
		MatrixToAngles( mat, angles );
		SetAbsAngles( angles );

		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

int CNeurotoxinCountdown::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CNeurotoxinCountdown::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
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

void CNeurotoxinCountdown::Spawn( void )
{
	Precache();

	BaseClass::Spawn();

	m_bEnabled = false;

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );
}

void CNeurotoxinCountdown::Precache( void )
{
	BaseClass::Precache();

	PrecacheVGuiScreen( "neurotoxin_countdown_screen" );
}

void CNeurotoxinCountdown::OnRestore( void )
{
	BaseClass::OnRestore();

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

void CNeurotoxinCountdown::ScreenVisible( bool bVisible )
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

void CNeurotoxinCountdown::Disable( void )
{
	if ( !m_bEnabled )
		return;

	SetThink(NULL);

	m_bEnabled = false;

	ScreenVisible( false );
}

void CNeurotoxinCountdown::Enable( void )
{
	if ( m_bEnabled )
		return;

	SetThink(&CNeurotoxinCountdown::Think);
	
	m_bEnabled = true;

	ScreenVisible( true );

	SetNextThink(gpGlobals->curtime);
}


void CNeurotoxinCountdown::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

void CNeurotoxinCountdown::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

void CNeurotoxinCountdown::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "neurotoxin_countdown_screen";
}

void CNeurotoxinCountdown::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CNeurotoxinCountdown::SpawnControlPanels()
{
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		float flWidth = m_iScreenWidth;
		float flHeight = m_iScreenHeight;

		CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, this, this, -1 );
		pScreen->ChangeTeam( GetTeamNumber() );
		pScreen->SetActualSize( flWidth, flHeight );
		pScreen->SetActive( true );
		pScreen->MakeVisibleOnlyToTeammates( false );
		pScreen->SetTransparency( true );
		int nScreen = m_hScreens.AddToTail( );
		m_hScreens[nScreen].Set( pScreen );

		return;
	}
}

void CNeurotoxinCountdown::RestoreControlPanels( void )
{
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		CVGuiScreen *pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( NULL, pScreenClassname );

		while ( ( pScreen && pScreen->GetOwnerEntity() != this ) || Q_strcmp( pScreen->GetPanelName(), pScreenName ) != 0 )
		{
			pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( pScreen, pScreenClassname );
		}

		if ( pScreen )
		{
			int nScreen = m_hScreens.AddToTail( );
			m_hScreens[nScreen].Set( pScreen );
			pScreen->SetActive( true );
		}

		return;
	}
}