//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"

#include "GameUI/IGameUI.h"
#include "fmtstr.h"
#include "igameevents.h"
#include "gameui/bonusmapsdatabase.h"
#include "portal_gamerules.h"
#ifdef CLIENT_DLL
#include "gameui/bonusmapsdialog.h"
#endif

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI("GameUI");

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
//static CDllDemandLoader g_GameUI( "GameUI" );

#ifndef CLIENT_DLL


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointBonusMapsAccessor : public CPointEntity
{
public:
	DECLARE_CLASS( CPointBonusMapsAccessor, CPointEntity );
	DECLARE_DATADESC();

	virtual void	Activate( void );

	void InputUnlock( inputdata_t& inputdata );
	void InputComplete( inputdata_t& inputdata );
	void InputSave( inputdata_t& inputdata );

private:
	string_t	m_String_tFileName;
	string_t	m_String_tMapName;
//	IGameUI		*m_pGameUI;
};

BEGIN_DATADESC( CPointBonusMapsAccessor )
	DEFINE_KEYFIELD( m_String_tFileName, FIELD_STRING, "filename" ),
	DEFINE_KEYFIELD( m_String_tMapName, FIELD_STRING, "mapname" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Unlock", InputUnlock ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Complete", InputComplete ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Save", InputSave ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( point_bonusmaps_accessor, CPointBonusMapsAccessor );

void CPointBonusMapsAccessor::Activate( void )
{
	BaseClass::Activate();
#if 0
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		m_pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
	}
#endif
}

void CPointBonusMapsAccessor::InputUnlock( inputdata_t& inputdata )
{
#if 0
	if ( m_pGameUI )
		m_pGameUI->BonusMapUnlock( m_String_tFileName.ToCStr(), m_String_tMapName.ToCStr() );	
#else
	
	IGameEvent *event = gameeventmanager->CreateEvent( "bonusmap_unlock" );
	if ( event )
	{
		event->SetString( "mapname", m_String_tMapName.ToCStr() );
		event->SetString( "filename", m_String_tFileName.ToCStr() );
		gameeventmanager->FireEvent( event );
	}

#endif
}

void CPointBonusMapsAccessor::InputComplete( inputdata_t& inputdata )
{	
	IGameEvent *event = gameeventmanager->CreateEvent( "advanced_map_complete" );
	if ( event )
	{
		event->SetString( "mapname", m_String_tMapName.ToCStr() );
		event->SetString( "filename", m_String_tFileName.ToCStr() );
		event->SetInt( "numadvanced", 0 ); //We don't do achievements in portalcoop
		gameeventmanager->FireEvent( event );
	}
}

void CPointBonusMapsAccessor::InputSave( inputdata_t& inputdata )
{
#if 0
	if ( m_pGameUI )
		m_pGameUI->BonusMapDatabaseSave();
#else
	IGameEvent *event = gameeventmanager->CreateEvent( "bonusmap_save" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}
#endif
}

#endif

void BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
#if 0
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			pGameUI->BonusMapChallengeUpdate( pchFileName, pchMapName, pchChallengeName, iBest );

			int piNumMedals[ 3 ];
			pGameUI->BonusMapNumMedals( piNumMedals );

			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_complete" );
			if ( event )
			{
				event->SetInt( "numbronze", piNumMedals[ 0 ] );
				event->SetInt( "numsilver", piNumMedals[ 1 ] );
				event->SetInt( "numgold", piNumMedals[ 2 ] );
				gameeventmanager->FireEvent( event );
			}
		}	
	}
#else


	//CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	//if ( gameUIFactory )
	{
		//IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		//if ( pGameUI )
		{

			
			
			//pGameUI->BonusMapChallengeUpdate( pchFileName, pchMapName, pchChallengeName, iBest );
			if ( !pchFileName || pchFileName[ 0 ] == '\0' || 
				 !pchMapName || pchMapName[ 0 ] == '\0' || 
				 !pchChallengeName || pchChallengeName[ 0 ] == '\0' )
			{
				Warning("BonusMapChallengeUpdate: Something went very wrong here\n");
				Msg("pchFileName: %s\n", pchFileName);
				Msg("pchMapName: %s\n", pchMapName);
				Msg("pchChallengeName: %s\n", pchChallengeName);
				return;
			}
			else
			{
				if ( BonusMapsDatabase()->UpdateChallengeBest( pchFileName, pchMapName, pchChallengeName, iBest ) )
				{
					// The challenge best changed, so write it to the file
					BonusMapsDatabase()->WriteSaveData();
					BonusMapsDatabase()->RefreshMapData();
		#ifdef CLIENT_DLL
					// Update the open dialog
					if ( g_pBonusMapsDialog )
						g_pBonusMapsDialog->RefreshData();
		#endif
				}
			}

			int piNumMedals[ 3 ];
		//	pGameUI->BonusMapNumMedals( piNumMedals );
			BonusMapsDatabase()->NumMedals( piNumMedals );

			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_complete" );
			if ( event )
			{
				event->SetInt( "numbronze", piNumMedals[ 0 ] );
				event->SetInt( "numsilver", piNumMedals[ 1 ] );
				event->SetInt( "numgold", piNumMedals[ 2 ] );
				gameeventmanager->FireEvent( event );
			}
		}	
	}

#endif
}

void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
#ifdef GAME_DLL
	if ( engine->IsDedicatedServer() )
	{
		if ( !pchFileName || !pchMapName || !pchChallengeName )
			return;

		BonusMapsDatabase()->GetCurrentChallengeNames( pchFileName, pchMapName, pchChallengeName );
	}
	else
#endif
	{
		CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
		if ( gameUIFactory )
		{
			IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
			if ( pGameUI )
			{
				pGameUI->BonusMapChallengeNames( pchFileName, pchMapName, pchChallengeName );
			}	
		}
	}
}

void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
#if 0
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( pGameUI )
		{
			pGameUI->BonusMapChallengeObjectives( iBronze, iSilver, iGold );
		}
	}
#else 
	BonusMapsDatabase()->GetCurrentChallengeObjectives( iBronze, iSilver, iGold );
#endif
}
