//Taken from Open Fortress with all due respect and credit to it's developers.
#pragma once

void AddRequiredSearchPaths();
void AddRequiredMapSearchPaths( const char *pMapName );

bool RestrictedMapPrefix( const char *pszPrefix, char *pszMissingMod = NULL, const char *pszCheckMod = NULL );