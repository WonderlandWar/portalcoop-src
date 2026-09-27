#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void GetProgressForPlayer( int iPlayer, int *piMapProgress, int *piFoundRadios )
{
	const char *pszPlayerProgress = engine->GetClientConVarValue( iPlayer, "progress" );
	
	char szToken[16];
	const char *psz = nexttoken(szToken, pszPlayerProgress, ',' );
	if ( piMapProgress )
	{
		*piMapProgress = atoi( psz );
	}

	if ( piFoundRadios )
	{
		psz = nexttoken( szToken, psz, ',' ); // Move this out of the check if the "progress" convar has more than 2 values
		*piFoundRadios = atoi( psz );
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

	DEFINE_INPUTFUNC( FIELD_INTEGER, "TestPlayersFoundNumRadios", InputTestPlayersFoundNumRadios ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "TestPlayersReached", InputTestPlayersReached ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "UpdatePlayerProgress", InputUpdatePlayerProgress ),

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