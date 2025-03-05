//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal.
//
//=============================================================================//

#ifdef PORTAL_MP



#include "portal_mp_gamerules.h" //redirect to multiplayer gamerules in multiplayer builds



#else

#ifndef PORTAL_GAMERULES_H
#define PORTAL_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "hl2_gamerules.h"

#ifdef CLIENT_DLL
	#define CPortalGameRules C_PortalGameRules
	#define CPortalGameRulesProxy C_PortalGameRulesProxy
#endif

#if defined ( CLIENT_DLL )
#include "steam/steam_api.h"
#endif

extern ConVar	tf_avoidteammates;


struct PortalPlayerStatistics_t
{
	int iNumPortalsPlaced;
	int iNumStepsTaken;
	float fNumSecondsTaken;
};

class CPortalGameRulesProxy : public CHalfLife2Proxy
{
public:
	CPortalGameRulesProxy();

	DECLARE_CLASS( CPortalGameRulesProxy, CHalfLife2Proxy );
	DECLARE_NETWORKCLASS();
#ifdef GAME_DLL
	DECLARE_DATADESC();
	
	void InputSuspendRespawning( inputdata_t &inputdata );
	void InputRespawnAllPlayers( inputdata_t &inputdata );
	void InputResetDetachedCameras( inputdata_t &inputdata );
	//void InputDisableGamePause( inputdata_t &inputdata );
	
	bool m_bSuspendRespawn;

	COutputEvent m_OnPlayerConnected;
#endif
};


class CPortalGameRules : public CHalfLife2
{
public:
	DECLARE_CLASS( CPortalGameRules, CSingleplayRules );

	virtual bool IsMultiplayer( void );

	virtual bool	Init();
	
	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool	ShouldUseRobustRadiusDamage(CBaseEntity *pEntity);
	virtual void	ClientSettingsChanged( CBasePlayer *pPlayer );
	virtual void	GoToIntermission( void );
#ifndef CLIENT_DLL
	virtual bool	ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual float	GetAutoAimScale( CBasePlayer *pPlayer );

	virtual void	LevelShutdown( void );

	virtual bool	ServerIsFull( void );

	float m_flPreStartTime;
	bool m_bReadyToCountProgress;
	bool m_bShouldSetPreStartTime;

#endif

#ifdef CLIENT_DLL
	virtual bool IsBonusChallengeTimeBased( void );
#endif


	bool	MegaPhyscannonActive(void) { return m_bMegaPhysgun; }
	
	PortalPlayerStatistics_t m_StatsThisLevel;
	
#ifdef GAME_DLL
	void IncrementPortalsPlaced( void );
	void IncrementStepsTaken( void );
	void UpdateSecondsTaken( void );

	void ResetThisLevelStats( void );
	
	float m_fTimeLastNumSecondsUpdate;

	bool m_bOldAllowPortalCustomization;

#endif
	int NumPortalsPlaced( void ) const { return m_StatsThisLevel.iNumPortalsPlaced; }
	int NumStepsTaken( void ) const { return m_StatsThisLevel.iNumStepsTaken; }
	float NumSecondsTaken( void ) const { return m_StatsThisLevel.fNumSecondsTaken; }

private:
	// Rules change for the mega physgun
	CNetworkVar( bool, m_bMegaPhysgun );

	DECLARE_SIMPLE_DATADESC();

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CPortalGameRules();
	virtual ~CPortalGameRules() {}

	virtual void			Think( void );

#ifdef GAME_DLL
	virtual void ClientDisconnected( edict_t *pClient );
#endif

	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			PlayerSpawn( CBasePlayer *pPlayer );
	virtual CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );// Place this player on their spawnspot and face them the proper direction.
	virtual bool			IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer );

	virtual void			InitDefaultAIRelationships( void );
	virtual const char*		AIClassText(int classType);
	virtual const char *GetGameDescription( void );

	// Ammo
	virtual void			PlayerThink( CBasePlayer *pPlayer );
	virtual float			GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );

	virtual bool			ShouldBurningPropsEmitLight();

	bool ShouldRemoveRadio( void );
	
public:

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

private:

	int						DefaultFOV( void ) { return 75; }
#endif
};


//-----------------------------------------------------------------------------
// Gets us at the Half-Life 2 game rules
//-----------------------------------------------------------------------------
inline CPortalGameRules* PortalGameRules()
{
	return static_cast<CPortalGameRules*>(g_pGameRules);
}

#endif // PORTAL_GAMERULES_H
#endif
