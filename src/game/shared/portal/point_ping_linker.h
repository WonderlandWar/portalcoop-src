#ifndef POINT_PING_LINKER_H
#define POINT_PING_LINKER_H
#ifdef WIN32
#pragma once
#endif

#include "cbase.h"

#ifdef CLIENT_DLL
#define CPointPingLinker C_PointPingLinker
#endif

#ifdef GAME_DLL
class CPointPingLinker : public CBaseEntity
#else
class CPointPingLinker : public C_BaseEntity
#endif
{
#ifdef GAME_DLL
	DECLARE_CLASS( CPointPingLinker, CBaseEntity );
#else
	DECLARE_CLASS( CPointPingLinker, C_BaseEntity );
#endif

public:
	DECLARE_NETWORKCLASS();

	CPointPingLinker();

#ifdef GAME_DLL
	DECLARE_DATADESC();
	

	void PingLinkedEntity( const char *pszName, float flTime, Vector vColor );
	void PingLinkedEntities( float flTime, Vector vColor, CBaseEntity *pOwner, const char* pszSoundName );

	bool HasThisEntity( CBaseAnimating *pAnimating );

private:

	string_t m_iszEntityName1;
	string_t m_iszEntityName2;
	string_t m_iszEntityName3;
	string_t m_iszEntityName4;
	string_t m_iszEntityName5;
	string_t m_iszEntityName6;
	string_t m_iszEntityName7;
	string_t m_iszEntityName8;
	string_t m_iszEntityName9;
	string_t m_iszEntityName10;
#endif
};

#endif //POINT_PING_LINKER_H