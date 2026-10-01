#include "cbase.h"
#include "pcoop_progress.h"
#include "portal_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar pcoop_progress_bots_are_maxed( "pcoop_progress_bots_are_maxed", "0", FCVAR_CHEAT, "If this is set to 1, the game will treat bots as though they have max progress" );

void Progress_GetPlayerProgress( int iPlayer, int *piMapProgress, int *piFoundRadios )
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayer );
	if ( pPlayer )
	{
		extern bool g_bCreatingTempBot;
		if ( ( g_bCreatingTempBot || pPlayer->IsBot() ) && pcoop_progress_bots_are_maxed.GetBool() )
		{
			if ( piMapProgress )
			{
				*piMapProgress = 255;
			}

			if ( piFoundRadios )
			{
				*piFoundRadios = 255;
			}
			return;
		}
	}

	const char *pszPlayerProgress = engine->GetClientConVarValue( iPlayer, "progress" );

	if ( !pszPlayerProgress || !*pszPlayerProgress )
	{
		if ( piMapProgress )
		{
			*piMapProgress = 0;
		}

		if ( piFoundRadios )
		{
			*piFoundRadios = 0;
		}
		return;
	}
	
	char szToken[16];
	const char *psz = nexttoken(szToken, pszPlayerProgress, ',' );
	if ( piMapProgress )
	{
		*piMapProgress = MAX( atoi( szToken ), 0 );
	}

	if ( piFoundRadios )
	{
		psz = nexttoken( szToken, psz, ',' ); // Move this out of the check if the "progress" convar has more than 2 values
		*piFoundRadios = MAX( atoi( szToken ), 0 );
	}
}

bool Progress_HasPlayerReachedNumber( int iPlayer, int iMapNumber )
{
	int iMapProgress = 0;
	Progress_GetPlayerProgress( iPlayer, &iMapProgress, NULL );

	if ( iMapProgress >= iMapNumber )
	{
		return true;
	}

	return false;
}

bool Progress_HasPlayerFoundRadios( int iPlayer, int iNumRadios )
{
	int iFoundRadios = 0;
	Progress_GetPlayerProgress( iPlayer, &iFoundRadios, NULL );

	if ( iFoundRadios >= iNumRadios )
	{
		return true;
	}

	return false;
}

void UpdatePlayerProgress( char iMapNumber )
{
	CBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "UpdateMapProgress" );
	WRITE_CHAR( iMapNumber );
	MessageEnd();
}

class CProgressManager : public CServerOnlyPointEntity 
{
public:
	DECLARE_CLASS( CProgressManager, CServerOnlyPointEntity );
	DECLARE_DATADESC();

	void InputUpdatePlayerProgress( inputdata_t &inputdata );
	void InputTestPlayersReached( inputdata_t &inputdata );
	void InputTestPlayersFoundNumRadios( inputdata_t &inputdata );

private:
	COutputEvent m_IfAllPlayersReached;
	COutputEvent m_IfAnyPlayersReached;
	COutputEvent m_IfAllPlayersFoundNumRadios;
	COutputEvent m_IfAnyPlayersFoundNumRadios;
};

BEGIN_DATADESC( CProgressManager )

	DEFINE_INPUTFUNC( FIELD_INTEGER, "UpdatePlayerProgress", InputUpdatePlayerProgress ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "TestPlayersReached", InputTestPlayersReached ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "TestPlayersFoundNumRadios", InputTestPlayersFoundNumRadios ),

	DEFINE_OUTPUT( m_IfAllPlayersReached, "IfAllPlayersReached" ),
	DEFINE_OUTPUT( m_IfAnyPlayersReached, "IfAnyPlayersReached" ),
	DEFINE_OUTPUT( m_IfAllPlayersFoundNumRadios, "IfAllPlayersFoundNumRadios" ),
	DEFINE_OUTPUT( m_IfAnyPlayersFoundNumRadios, "IfAnyPlayersFoundNumRadios" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( game_progress_manager, CProgressManager );

void CProgressManager::InputUpdatePlayerProgress( inputdata_t &inputdata )
{
	int iMapNumber = inputdata.value.Int();
	UpdatePlayerProgress( iMapNumber );
}

void CProgressManager::InputTestPlayersReached( inputdata_t &inputdata )
{
	int iMapNumber = inputdata.value.Int();

	bool bAnyPlayerReached = false;
	bool bFoundUnreachedPlayer = false;

	for ( int i = 1; i <= GetRequiredPlayers(); ++i )
	{
		if ( Progress_HasPlayerReachedNumber( i, iMapNumber ) )
		{
			bAnyPlayerReached = true;
		}
		else
		{
			bFoundUnreachedPlayer = true;
		}
	}

	if ( bAnyPlayerReached )
	{
		if ( !bFoundUnreachedPlayer )
		{
			m_IfAllPlayersReached.FireOutput( inputdata.pActivator, this );
		}
		m_IfAnyPlayersReached.FireOutput( inputdata.pActivator, this );
	}
}

void CProgressManager::InputTestPlayersFoundNumRadios( inputdata_t &inputdata )
{
	int iNumRadios = inputdata.value.Int();

	bool bAnyPlayerReached = false;
	bool bFoundUnreachedPlayer = false;

	for ( int i = 1; i <= GetRequiredPlayers(); ++i )
	{
		if ( Progress_HasPlayerFoundRadios( i, iNumRadios ) )
		{
			bAnyPlayerReached = true;
		}
		else
		{
			bFoundUnreachedPlayer = true;
		}
	}

	if ( bAnyPlayerReached )
	{
		if ( !bFoundUnreachedPlayer )
		{
			m_IfAllPlayersFoundNumRadios.FireOutput( inputdata.pActivator, this );
		}
		m_IfAnyPlayersFoundNumRadios.FireOutput( inputdata.pActivator, this );
	}
}