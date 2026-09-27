#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar pcoop_progress_bots_are_maxed( "pcoop_progress_bots_are_maxed", "0", FCVAR_CHEAT, "If this is set to 1, the game will treat bots as though they have max progress" );

void GetProgressForPlayer( int iPlayer, int *piMapProgress, int *piFoundRadios )
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
	
	char szToken[16];
	const char *psz = nexttoken(szToken, pszPlayerProgress, ',' );
	if ( piMapProgress )
	{
		*piMapProgress = atoi( szToken );
	}

	if ( piFoundRadios )
	{
		psz = nexttoken( szToken, psz, ',' ); // Move this out of the check if the "progress" convar has more than 2 values
		*piFoundRadios = atoi( szToken );
	}
}

void UpdatePlayerProgress( char iMapNumber )
{
	CBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "UpdateMapProgress" );
	WRITE_CHAR( iMapNumber );
	MessageEnd();
}

bool Progress_HasPlayer()
{
	return true;
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

}

void CProgressManager::InputTestPlayersFoundNumRadios( inputdata_t &inputdata )
{

}