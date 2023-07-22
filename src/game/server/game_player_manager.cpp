#include "cbase.h"
#include "game_player_manager.h"

BEGIN_DATADESC(CPlayerManager)

DEFINE_INPUTFUNC( FIELD_VOID, "PlayerJoin", InputPlayerJoin ),
DEFINE_INPUTFUNC( FIELD_VOID, "PlayerSpawn", InputPlayerSpawn ),

DEFINE_INPUTFUNC( FIELD_VOID, "InPlayerCount", InputPlayerCount ),
DEFINE_INPUTFUNC( FIELD_VOID, "InMaxPlayerCount", InputMaxPlayerCount ),

DEFINE_OUTPUT( m_OnPlayerJoin, "OnPlayerJoin" ),
DEFINE_OUTPUT( m_OnPlayerSpawn, "OnPlayerSpawn" ),

DEFINE_OUTPUT( m_MaxPlayers, "OutMaxPlayerCount" ),
DEFINE_OUTPUT( m_PlayerCount, "OutPlayerCount" ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( game_player_manager, CPlayerManager )

void CPlayerManager::Spawn( void )
{
	m_MaxPlayers.Init(0);
	m_PlayerCount.Init(0);
}

void CPlayerManager::InputPlayerCount( inputdata_t &inputdata )
{
	int iPlayerCount = 0;
	
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer )
			++iPlayerCount;
	}

	m_PlayerCount.Set( iPlayerCount, inputdata.pActivator, inputdata.pCaller );
	/*
	variant_t variant;
	variant.Int = iPlayerCount;
	m_PlayerCount.FireOutput( variant, inputdata.pActivator, inputdata.pCaller);
	*/
}

void CPlayerManager::InputMaxPlayerCount( inputdata_t &inputdata )
{
	
	m_MaxPlayers.Set( gpGlobals->maxClients, inputdata.pActivator, inputdata.pCaller );
	/*
	variant_t variant;
	variant.Int = gpGlobals->maxClients;	
	m_MaxPlayers.FireOutput( variant, inputdata.pActivator, inputdata.pCaller);
	*/
}

void CPlayerManager::InputPlayerJoin( inputdata_t &inputdata )
{
	m_OnPlayerJoin.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CPlayerManager::InputPlayerSpawn( inputdata_t &inputdata )
{
	m_OnPlayerSpawn.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CPlayerManager::InputPlayerLeave( inputdata_t &inputdata )
{
	m_OnPlayerSpawn.FireOutput( inputdata.pActivator, inputdata.pCaller );
}