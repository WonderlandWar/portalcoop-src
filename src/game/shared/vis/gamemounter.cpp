//Taken from Open Fortress with all due respect and credit to it's developers.
#include "cbase.h"
#include "gamemounter.h"
#include "filesystem.h"
#include "steam/steam_api.h"
#include "tier1/KeyValues.h"
#include "tier0/icommandline.h"
#ifdef PORTAL
#include "portal_shareddefs.h"
#ifdef CLIENT_DLL
#include "gamestringpool.h"
#endif // CLIENT_DLL
#endif // PORTAL

bool EvaluateExtraConditionals( const char* str )
{
	bool bNot = false; // should we negate this command?
	if ( *str == '!' )
		bNot = true;

	if ( Q_stristr( str, "$DEDICATED" ) )
	{
#ifdef CLIENT_DLL
		return false ^ bNot;
#else
		return engine->IsDedicatedServer() ^ bNot;
#endif
	}

	return false;
}

#ifdef PORTAL
struct FailedMount
{
	FailedMount( const char *pszModFolder, const char *pszPrefix )
	{
		V_strcpy( m_szModFolder, pszModFolder );
		V_strcpy( m_szPrefix, pszPrefix );
	}
	char m_szModFolder[16];
	char m_szPrefix[16];
};

CUtlVector<FailedMount> g_FailedMountChecks;

bool RestrictedMapPrefix( const char *pszPrefix, char *pszMissingMod, const char *pszCheckMod )
{
	for ( int i = 0; i < g_FailedMountChecks.Count(); ++i )
	{
		if ( V_stristr( pszPrefix, g_FailedMountChecks[i].m_szPrefix ) &&
			// It's possible to have multiple map sets using the same prefix while also having different mods
			( !pszCheckMod || !V_stricmp( pszCheckMod, g_FailedMountChecks[i].m_szModFolder ) )
			)
		{
			if ( pszMissingMod )
			{
				V_strcpy( pszMissingMod, g_FailedMountChecks[i].m_szModFolder );
			}
			return true;
		}
	}

	return false;
}

void AddCheckFiles( KeyValues *pGame, KeyValues *pPaths, const char *pModFolder )
{
	// Add the check files
	KeyValues *pCheckFiles = pGame->FindKey( "checkfiles" );
	if ( !pCheckFiles )
		return;

	for ( KeyValues *file = pCheckFiles->GetFirstSubKey(); file; file = file->GetNextKey() )
	{
		if ( !g_pFullFileSystem->FileExists( file->GetString() ) )
		{
			KeyValues *pMapPrefixes = pGame->FindKey( "map_prefixes" );
			if ( !pMapPrefixes )
				return;

			for ( KeyValues *prefix = pMapPrefixes->GetFirstSubKey(); prefix; prefix = prefix->GetNextKey() )
			{
				// No need to add a restricted map prefix if it was already added
				if ( !RestrictedMapPrefix( prefix->GetName(), NULL, pModFolder ) )
				{
					FailedMount failedmount( pModFolder, prefix->GetString() );
					g_FailedMountChecks.AddToTail( failedmount );
				}
			}
		}
	}
}
#endif
// brute forces our search paths, reads the users steam configs
// to determine any additional steam library directories people have
// as there's no other way to currently mount a different game (css) 
// if it's located in a different library without an absolute path
void MountPathLocal( KeyValues* pGame )
{
	const char* szGameName = pGame->GetName();

	if ( !steamapicontext || !steamapicontext->SteamApps() )
	{
		Msg( "Skipping %s, unable to get app install path.\n", szGameName );

		return;
	}

	char szPath[ MAX_PATH * 2 ];
	int ccFolder = steamapicontext->SteamApps()->GetAppInstallDir( pGame->GetUint64( "appid" ), szPath, sizeof( szPath ) );

	if ( ccFolder > 0 )
	{
		ConColorMsg( Color( 90, 240, 90, 255 ), "Mounting %s (local)\n", szGameName );

		KeyValues *pPaths = pGame->FindKey( "paths" );

		if ( !pPaths )
			return;

		for ( KeyValues *pPath = pPaths->GetFirstSubKey(); pPath; pPath = pPath->GetNextKey() )
		{
			if ( !FStrEq( pPath->GetName(), "local" ) )
				continue;

			char szTempPath[ MAX_PATH * 2 ];
			Q_strncpy( szTempPath, szPath, ARRAYSIZE( szTempPath ) );

			V_AppendSlash( szTempPath, ARRAYSIZE( szTempPath ) );
			V_strncat( szTempPath, pPath->GetString(), ARRAYSIZE( szTempPath ) );

			g_pFullFileSystem->AddSearchPath( szTempPath, "GAME" );
#ifdef PORTAL
			AddCheckFiles( pGame, pPaths, pPath->GetString() );
#endif
			ConColorMsg( Color( 144, 238, 144, 255 ), "\tAdding path: %s\n", pPath->GetString() );
		}
	}
	else
	{
		Warning( "%s not found on system. Skipping.\n", szGameName );
	}
}

#ifdef GAME_DLL
void MountPathDedicated( KeyValues* pGame )
{
	const char* szGameName = pGame->GetName();

	ConColorMsg( Color( 90, 240, 90, 255 ), "Mounting %s (dedicated)\n", szGameName );

	KeyValues *pPaths = pGame->FindKey( "paths" );

	if ( !pPaths )
		return;

	for ( KeyValues *pPath = pPaths->GetFirstSubKey(); pPath; pPath = pPath->GetNextKey() )
	{
		if ( !FStrEq( pPath->GetName(), "dedicated" ) )
			continue;

		const char* szRelativePath = pPath->GetString();
		char gamedir[FILENAME_MAX];
		filesystem->GetCurrentDirectory( gamedir, sizeof( gamedir ) );
		V_StripLastDir( gamedir, sizeof( gamedir ) );
		V_strcat( gamedir, szGameName, sizeof( gamedir ) );
		V_AppendSlash( gamedir, sizeof( gamedir ) );
		V_strcat( gamedir, szRelativePath, sizeof( gamedir ) );
		V_FixSlashes( gamedir );

		g_pFullFileSystem->AddSearchPath( gamedir, "GAME" );
#ifdef PORTAL
		AddCheckFiles( pGame, pPaths, pPath->GetString() );
#endif
		ConColorMsg( Color( 90, 240, 90, 255 ), "\tAdding path: %s\n", gamedir );
	}
}
#endif

// To-Do: This Should really needs to be re-worked
// basically is the same MountPathLocal but don't check for required
// there should be a proper way to get the sourcemods folder
void MountSourceMod( KeyValues* pGame )
{
	const char* szGameName = pGame->GetName();

	const char *defaultpath = NULL;
	const char *szPath = CommandLine()->ParmValue( "-game", defaultpath );

	if ( szPath )
	{
		ConColorMsg( Color( 90, 240, 90, 255 ), "Mounting %s (sourcemod)\n", szGameName );

		KeyValues *pPaths = pGame->FindKey( "paths" );
		if ( !pPaths )
			return;

		for ( KeyValues *folder = pPaths->GetFirstSubKey(); folder; folder = folder->GetNextKey() )
		{
			char szTempPath[ MAX_PATH * 2 ];
			Q_snprintf( szTempPath, sizeof( szTempPath ), "%s/../%s", szPath, folder->GetString() );
			
			V_AppendSlash( szTempPath, ARRAYSIZE( szTempPath ) );

			g_pFullFileSystem->AddSearchPath( szTempPath, "GAME" );
#ifdef PORTAL
			AddCheckFiles( pGame, pPaths, folder->GetString() );
#endif
			ConColorMsg( Color( 90, 240, 90, 255 ), "\tAdding sourcemod path: %s\n", szTempPath );
		}
	}
}
#ifdef PORTAL
void MountGamesForMapSets( const char *pFilename )
{
	KeyValues *pMountFile = new KeyValues( "gamemounting.txt" );
	char szFileName[MAX_PATH];
	Q_snprintf( szFileName, sizeof( szFileName ), "%s/%s", pFilename, "gamemounting.txt" );
	if ( pMountFile->LoadFromFile( g_pFullFileSystem, szFileName, "MOD" ) )
	{
		for( KeyValues *pGame = pMountFile->GetFirstTrueSubKey(); pGame; pGame = pGame->GetNextTrueSubKey() )
		{

	#ifndef CLIENT_DLL
			if ( engine->IsDedicatedServer() )
				MountPathDedicated( pGame );
			else
				MountPathLocal( pGame );

	#else
			MountPathLocal( pGame ); // Client only mounts locally...
	#endif

		}
	}

	pMountFile->deleteThis();
	
	KeyValues *pMountModFile = new KeyValues( "SourceMods" );
	memset( szFileName, 0, sizeof( szFileName ) );
	Q_snprintf( szFileName, sizeof( szFileName ), "%s/%s", pFilename, "sourcemounting.txt" );
	if ( pMountModFile->LoadFromFile( g_pFullFileSystem, szFileName, "MOD" ) )
	{
		for( KeyValues *pGame = pMountModFile->GetFirstTrueSubKey(); pGame; pGame = pGame->GetNextTrueSubKey() )
			MountSourceMod( pGame );
	}

	pMountModFile->deleteThis();
}

#endif
void AddRequiredSearchPaths()
{
	SetExtraConditionalFunc( &EvaluateExtraConditionals ); //To-Do: Move this

#ifdef PORTAL
	ExecuteLoadingMapSetFunction( MountGamesForMapSets );
#else
	KeyValues *pMountFile = new KeyValues( "gamemounting.txt" );
	pMountFile->LoadFromFile( g_pFullFileSystem, "gamemounting.txt", "MOD" );

	for( KeyValues *pGame = pMountFile->GetFirstTrueSubKey(); pGame; pGame = pGame->GetNextTrueSubKey() )
	{

#ifndef CLIENT_DLL
		if ( engine->IsDedicatedServer() )
			MountPathDedicated( pGame );
		else
			MountPathLocal( pGame );

#else
		MountPathLocal( pGame ); // Client only mounts locally...
#endif

	}

	pMountFile->deleteThis();

	KeyValues *pMountModFile = new KeyValues( "SourceMods" );
	pMountModFile->LoadFromFile( g_pFullFileSystem, "sourcemounting.txt", "MOD" );

	for( KeyValues *pGame = pMountModFile->GetFirstTrueSubKey(); pGame; pGame = pGame->GetNextTrueSubKey() )
		MountSourceMod( pGame );

	pMountModFile->deleteThis();
#endif
}

void AddRequiredMapSearchPaths( const char *pMapName )
{
	SetExtraConditionalFunc( &EvaluateExtraConditionals );

	char szMapFilename[MAX_PATH];
	szMapFilename[0] = NULL;

	if ( pMapName && *pMapName )
	{
		V_snprintf( szMapFilename, sizeof( szMapFilename ), "maps/%s_mounting.txt", pMapName );
	}

	KeyValues *pMountFile = new KeyValues( szMapFilename );
	pMountFile->LoadFromFile( g_pFullFileSystem, szMapFilename, "MOD" );

	for( KeyValues *pGame = pMountFile->GetFirstTrueSubKey(); pGame; pGame = pGame->GetNextTrueSubKey() )
	{

#ifndef CLIENT_DLL
		if ( engine->IsDedicatedServer() )
			MountPathDedicated( pGame );
		else
			MountPathLocal( pGame );

#else
		MountPathLocal( pGame ); // Client only mounts locally...
#endif

	}

	pMountFile->deleteThis();
}