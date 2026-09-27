#include "cbase.h"
#include "c_user_message_register.h"
#include "filesystem.h"
#include "portal_shareddefs.h"
#include "c_portal_radio.h"

#define PROGRESS_DATA_FILE "save/pcoop_progress.txt"
ConVar progress_cv( "progress", "0", FCVAR_USERINFO );

KeyValues *LoadProgressData()
{	
	KeyValues *radios = new KeyValues( "progress_data" );
	if ( !radios->LoadFromFile( g_pFullFileSystem, PROGRESS_DATA_FILE, "GAME" ) )
	{
		radios->SaveToFile( g_pFullFileSystem, PROGRESS_DATA_FILE, "GAME" );
	}

	return radios;
}

void UpdateMapProgress( char iMapNumber )
{
	KeyValues *progress = LoadProgressData();
	const char *pszMapSet = g_MapInfo.GetAssociatedMapSet();
	if ( progress && *pszMapSet )
	{
		char iCurrentMap = progress->GetInt( pszMapSet );
		if ( iCurrentMap < iMapNumber )
		{
			progress->SetInt( pszMapSet, iMapNumber );
			progress->SaveToFile( g_pFullFileSystem, PROGRESS_DATA_FILE, "GAME" );		
		}
	}

	if ( progress )
	{
		progress->deleteThis();
	}
}

void __MsgFunc_UpdateMapProgress( bf_read &msg )
{
	char iMapNumber = msg.ReadChar();
	UpdateMapProgress( iMapNumber );
}
USER_MESSAGE_REGISTER( UpdateMapProgress );

void UpdateProgressConVars()
{
	int iCurrentMap = 0, iFoundRadios = 0;
	const char *pszMapSet = g_MapInfo.GetAssociatedMapSet();

	if ( *pszMapSet )
	{
		KeyValues *progress = LoadProgressData();
		if ( progress )
		{
			iCurrentMap = progress->GetInt( pszMapSet );
			progress->deleteThis();
		}
		
		KeyValues *radios = LoadRadioData();
		if ( radios )
		{
			KeyValues *radios_mapset = radios->FindKey( pszMapSet );
			if ( radios_mapset )
			{
				for ( KeyValues *radio = radios_mapset->GetFirstValue(); radio != NULL; radio = radio->GetNextValue() )
				{
					AssertMsg1( false, "%s ugghhh", radio->GetName() );
					if ( radios_mapset->GetBool( radio->GetName() ) )
					{
						++iFoundRadios;
					}
				}
			}
		}
	}
	
	progress_cv.SetValue( VarArgs( "%i,%i", iCurrentMap, iFoundRadios ) );
}


class CMapProgress : public CAutoGameSystem
{
public:
	virtual void LevelInitPreEntity()
	{
		UpdateProgressConVars();
	}
};

static CMapProgress s_MapProgress;