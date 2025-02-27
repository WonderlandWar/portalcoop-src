#include "const.h"
#include <inetmsghandler.h>
#include <client_class.h>
#include <protocol.h>
//#include "netmessages.h"
#include "networkstringtabledefs.h"
#include "bitvec.h"
#include "qlimits.h"
#include "utlstring.h"
#include "mempool.h"
#include "checksum_crc.h"
#include "checksum_md5.h"
#include "irecipientfilter.h"
#include "soundflags.h"
#include "filesystem.h"
#include "inetchannel.h"

#define Bits2Bytes(b) ((b+7)>>3)

// Only send this many requests before timing out.
#define CL_CONNECTION_RETRIES		4 

typedef enum
{
	NA_NULL = 0,
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP,
} netadrtype_t;

typedef struct netadr_s
{
public:
	netadr_s() { SetIP( 0 ); SetPort( 0 ); SetType( NA_IP ); }
	netadr_s( uint unIP, uint16 usPort ) { SetIP( unIP ); SetPort( usPort ); SetType( NA_IP ); }
	netadr_s( const char *pch ) { SetFromString( pch ); }
	void	Clear();	// invalids Address

	void	SetType( netadrtype_t type );
	void	SetPort( unsigned short port );
	bool	SetFromSockadr(const struct sockaddr *s);
	void	SetIP(uint8 b1, uint8 b2, uint8 b3, uint8 b4);
	void	SetIP(uint unIP);									// Sets IP.  unIP is in host order (little-endian)
	void    SetIPAndPort( uint unIP, unsigned short usPort ) { SetIP( unIP ); SetPort( usPort ); }
	bool	SetFromString(const char *pch, bool bUseDNS = false ); // if bUseDNS is true then do a DNS lookup if needed
	
	bool	CompareAdr (const netadr_s &a, bool onlyBase = false) const;
	bool	CompareClassBAdr (const netadr_s &a) const;
	bool	CompareClassCAdr (const netadr_s &a) const;

	netadrtype_t	GetType() const;
	unsigned short	GetPort() const;

	// DON'T CALL THIS
	const char*		ToString( bool onlyBase = false ) const; // returns xxx.xxx.xxx.xxx:ppppp

	void	ToString( char *pchBuffer, uint32 unBufferSize, bool onlyBase = false ) const; // returns xxx.xxx.xxx.xxx:ppppp
	template< size_t maxLenInChars >
	void	ToString_safe( char (&pDest)[maxLenInChars], bool onlyBase = false ) const
	{
		ToString( &pDest[0], maxLenInChars, onlyBase );
	}

	void			ToSockadr(struct sockaddr *s) const;

	// Returns 0xAABBCCDD for AA.BB.CC.DD on all platforms, which is the same format used by SetIP().
	// (So why isn't it just named GetIP()?  Because previously there was a fucntion named GetIP(), and
	// it did NOT return back what you put into SetIP().  So we nuked that guy.)
	unsigned int	GetIPHostByteOrder() const;

	// Returns a number that depends on the platform.  In most cases, this probably should not be used.
	unsigned int	GetIPNetworkByteOrder() const;

	bool	IsLocalhost() const; // true, if this is the localhost IP 
	bool	IsLoopback() const;	// true if engine loopback buffers are used
	bool	IsReservedAdr() const; // true, if this is a private LAN IP
	bool	IsValid() const;	// ip & port != 0
	bool	IsBaseAdrValid() const;	// ip != 0

	void    SetFromSocket( int hSocket );

	bool	Unserialize( bf_read &readBuf );
	bool	Serialize( bf_write &writeBuf );

	bool operator==(const netadr_s &netadr) const {return ( CompareAdr( netadr ) );}
	bool operator!=(const netadr_s &netadr) const {return !( CompareAdr( netadr ) );}
	bool operator<(const netadr_s &netadr) const;

public:	// members are public to avoid to much changes

	netadrtype_t	type;
	unsigned char	ip[4];
	unsigned short	port;
} netadr_t;

class CClockDriftMgr
{
	friend class CBaseClientState;

public:
	CClockDriftMgr();

	// Is clock correction even enabled right now?
	static bool IsClockCorrectionEnabled();

	// Clear our state.
	void Clear();

	// This is called each time a server packet comes in. It is used to correlate
	// where the server is in time compared to us.
	void SetServerTick(int iServerTick);

	// Pass in the frametime you would use, and it will drift it towards the server clock.
	float AdjustFrameTime(float inputFrameTime);

	// Returns how many ticks ahead of the server the client is.
	float GetCurrentClockDifference() const;


private:

	void ShowDebugInfo(float flAdjustment);

	// This scales the offsets so the average produced is equal to the
	// current average + flAmount. This way, as we add corrections,
	// we lower the average accordingly so we don't keep responding
	// as much as we need to after we'd adjusted it a couple times.
	void AdjustAverageDifferenceBy(float flAmountInSeconds);


private:

	enum
	{
		// This controls how much it smoothes out the samples from the server.
		NUM_CLOCKDRIFT_SAMPLES = 16
	};

	// This holds how many ticks the client is ahead each time we get a server tick.
	// We average these together to get our estimate of how far ahead we are.
	float m_ClockOffsets[NUM_CLOCKDRIFT_SAMPLES];
	int m_iCurClockOffset;

	int m_nServerTick;		// Last-received tick from the server.
	int	m_nClientTick;		// The client's own tick counter (specifically, for interpolation during rendering).
	// The server may be on a slightly different tick and the client will drift towards it.
};

// PostDataUpdate calls are stored in a list until all ents have been updated.
class CPostDataUpdateCall
{
public:
	int					m_iEnt;
	DataUpdateType_t	m_UpdateType;
};


#define MAX_CLIENT_FRAMES	128
class CFrameSnapshot;
class CClientFrame
{
public:

	virtual ~CClientFrame();

	// Accessors to snapshots. The data is protected because the snapshots are reference-counted.
	inline CFrameSnapshot*	GetSnapshot() const { return m_pSnapshot; };
	virtual bool		IsMemPoolAllocated() { return true; }

public:

	// State of entities this frame from the POV of the client.
	int					last_entity;	// highest entity index
	int					tick_count;	// server tick of this snapshot

	// Used by server to indicate if the entity was in the player's pvs
	CBitVec<MAX_EDICTS>	transmit_entity; // if bit n is set, entity n will be send to client
	CBitVec<MAX_EDICTS>	*from_baseline;	// if bit n is set, this entity was send as update from baseline
	CBitVec<MAX_EDICTS>	*transmit_always; // if bit is set, don't do PVS checks before sending (HLTV only)

	CClientFrame*		m_pNext;

private:

	// Index of snapshot entry that stores the entities that were active and the serial numbers
	// for the frame number this packed entity corresponds to
	// m_pSnapshot MUST be private to force using SetSnapshot(), see reference counters
	CFrameSnapshot		*m_pSnapshot;
};

enum
{
	ENTITY_SENTINEL = 9999	// larger number than any real entity number
};

class CEntityInfo
{
public:

	CEntityInfo() {
		m_nOldEntity = -1;
		m_nNewEntity = -1;
		m_nHeaderBase = -1;
	}
	virtual	~CEntityInfo() {};
	
	bool			m_bAsDelta;
	CClientFrame	*m_pFrom;
	CClientFrame	*m_pTo;

	UpdateType		m_UpdateType;

	int				m_nOldEntity;	// current entity index in m_pFrom
	int				m_nNewEntity;	// current entity index in m_pTo

	int				m_nHeaderBase;
	int				m_nHeaderCount;

	inline void	NextOldEntity( void ) 
	{
		if ( m_pFrom )
		{
			m_nOldEntity = m_pFrom->transmit_entity.FindNextSetBit( m_nOldEntity+1 );

			if ( m_nOldEntity < 0 )
			{
				// Sentinel/end of list....
				m_nOldEntity = ENTITY_SENTINEL;
			}
		}
		else
		{
			m_nOldEntity = ENTITY_SENTINEL;
		}
	}

	inline void	NextNewEntity( void ) 
	{
		m_nNewEntity = m_pTo->transmit_entity.FindNextSetBit( m_nNewEntity+1 );

		if ( m_nNewEntity < 0 )
		{
			// Sentinel/end of list....
			m_nNewEntity = ENTITY_SENTINEL;
		}
	}
};

// Passed around the read functions.
class CEntityReadInfo : public CEntityInfo
{

public:

	CEntityReadInfo() 
	{	m_nPostDataUpdateCalls = 0;
		m_nLocalPlayerBits = 0;
		m_nOtherPlayerBits = 0;
		m_UpdateType = PreserveEnt;
	}

	bf_read			*m_pBuf;
	int				m_UpdateFlags;	// from the subheader
	bool			m_bIsEntity;

	int				m_nBaseline;	// what baseline index do we use (0/1)
	bool			m_bUpdateBaselines; // update baseline while parsing snaphsot
		
	int				m_nLocalPlayerBits; // profiling data
	int				m_nOtherPlayerBits; // profiling data

	CPostDataUpdateCall	m_PostDataUpdateCalls[MAX_EDICTS];
	int					m_nPostDataUpdateCalls;
};

class PackedEntity;
abstract_class CBaseClientState : public INetChannelHandler, public IConnectionlessPacketHandler, public IServerMessageHandler
{
	
public:
	CBaseClientState();
	virtual ~CBaseClientState();

public: // IConnectionlessPacketHandler interface:
		
	virtual bool ProcessConnectionlessPacket(struct netpacket_s *packet) OVERRIDE;

public: // INetMsgHandler interface:
		
	virtual void ConnectionStart(INetChannel *chan) OVERRIDE;
	virtual void ConnectionClosing( const char *reason ) OVERRIDE;
	virtual void ConnectionCrashed(const char *reason) OVERRIDE;

	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) OVERRIDE {};
	virtual void PacketEnd( void ) OVERRIDE {};

	virtual void FileReceived( const char *fileName, unsigned int transferID ) OVERRIDE;
	virtual void FileRequested( const char *fileName, unsigned int transferID ) OVERRIDE;
	virtual void FileDenied( const char *fileName, unsigned int transferID ) OVERRIDE;
	virtual void FileSent( const char *fileName, unsigned int transferID ) OVERRIDE;

public: // IServerMessageHandlers
	
	PROCESS_NET_MESSAGE( Tick );
	PROCESS_NET_MESSAGE( StringCmd );
	PROCESS_NET_MESSAGE( SetConVar );
	PROCESS_NET_MESSAGE( SignonState );

	PROCESS_SVC_MESSAGE( Print );
	PROCESS_SVC_MESSAGE( ServerInfo );
	PROCESS_SVC_MESSAGE( SendTable );
	PROCESS_SVC_MESSAGE( ClassInfo );
	PROCESS_SVC_MESSAGE( SetPause );
	PROCESS_SVC_MESSAGE( CreateStringTable );
	PROCESS_SVC_MESSAGE( UpdateStringTable );
	PROCESS_SVC_MESSAGE( SetView );
	PROCESS_SVC_MESSAGE( PacketEntities );
	PROCESS_SVC_MESSAGE( Menu );
	PROCESS_SVC_MESSAGE( GameEventList );
	PROCESS_SVC_MESSAGE( GetCvarValue );
	PROCESS_SVC_MESSAGE( CmdKeyValues );

	// Returns dem file protocol version, or, if not playing a demo, just returns PROTOCOL_VERSION
	virtual int GetDemoProtocolVersion() const OVERRIDE;

public: 
	inline	bool IsActive( void ) const { return m_nSignonState == SIGNONSTATE_FULL; };
	inline	bool IsConnected( void ) const { return m_nSignonState >= SIGNONSTATE_CONNECTED; };
	virtual	void Clear( void );
	virtual void FullConnect( netadr_t &adr ); // a connection was established
	virtual void Connect(const char* adr, const char *pszSourceTag); // start a connection challenge
	virtual bool SetSignonState ( int state, int count );
	virtual void Disconnect( const char *pszReason, bool bShowMainMenu );
	virtual void SendConnectPacket (int challengeNr, int authProtocol, uint64 unGSSteamID, bool bGSSecure );
	virtual const char *GetCDKeyHash() { return "123"; }
	virtual void RunFrame ( void );
	virtual void CheckForResend ( void );
	virtual void InstallStringTableCallback( char const *tableName ) { }
	virtual bool HookClientStringTable( char const *tableName ) { return false; }
	virtual bool LinkClasses( void );
	virtual int  GetConnectionRetryNumber() const { return CL_CONNECTION_RETRIES; }
	virtual const char *GetClientName() { return "YOU SHOULD NEVER SEE THIS"; Assert( false ); }
	
	INetworkStringTable *GetStringTable( const char * name ) const;

	virtual void ReadEnterPVS( CEntityReadInfo &u ) = 0;
	virtual void ReadLeavePVS( CEntityReadInfo &u ) = 0;
	virtual void ReadDeltaEnt( CEntityReadInfo &u ) = 0;
	virtual void ReadPreserveEnt( CEntityReadInfo &u ) = 0;
	virtual void ReadDeletions( CEntityReadInfo &u ) = 0;

	static bool ConnectMethodAllowsRedirects( void );

private:

public:
	// Connection to server.			
	int				m_Socket;		// network socket 
	INetChannel		*m_NetChannel;		// Our sequenced channel to the remote server.
	unsigned int	m_nChallengeNr;	// connection challenge number
	double			m_flConnectTime;	// If gap of connect_time to net_time > 3000, then resend connect packet
	int				m_nRetryNumber;	// number of retry connection attemps
	char			m_szRetryAddress[ MAX_OSPATH ];
	CUtlString		m_sRetrySourceTag; // string that describes why we decided to connect to this server (empty for command line, "serverbrowser", "quickplay", etc)
	int				m_retryChallenge; // challenge we sent to the server
	int				m_nSignonState;    // see SIGNONSTATE_* definitions
	double			m_flNextCmdTime; // When can we send the next command packet?
	int				m_nServerCount;	// server identification for prespawns, must match the svs.spawncount which
									// is incremented on server spawning.  This supercedes svs.spawn_issued, in that
									// we can now spend a fair amount of time sitting connected to the server
									// but downloading models, sounds, etc.  So much time that it is possible that the
									// server might change levels again and, if so, we need to know that.
	uint64			m_ulGameServerSteamID; // Steam ID of the game server we are trying to connect to, or are connected to.  Zero if unknown
	int			m_nCurrentSequence;	// this is the sequence number of the current incoming packet	

	CClockDriftMgr m_ClockDriftMgr;

	int			m_nDeltaTick;		//	last valid received snapshot (server) tick
	bool		m_bPaused;			// send over by server
	int			m_nViewEntity;		// cl_entitites[cl.viewentity] == player point of view

	int			m_nPlayerSlot;		// own player entity index-1. skips world. Add 1 to get cl_entitites index;

	char		m_szLevelFileName[ 128 ];	// for display on solo scoreboard
	char		m_szLevelBaseName[ 128 ]; // removes maps/ and .bsp extension
	//char		m_szLevelName[40];	// for display on solo scoreboard
	//char		m_szLevelNameShort[40]; // removes maps/ and .bsp extension

	int			m_nMaxClients;		// max clients on server

	PackedEntity	*m_pEntityBaselines[2][MAX_EDICTS];	// storing entity baselines
		
	// This stuff manages the receiving of data tables and instantiating of client versions
	// of server-side classes.
	void	*m_pServerClasses;
	int					m_nServerClasses;
	int					m_nServerClassBits;
	char				m_szEncrytionKey[STEAM_KEYSIZE];
	unsigned int		m_iEncryptionKeySize;

	void *m_StringTableContainer;
	
	bool m_bRestrictServerCommands;	// If true, then the server is only allowed to execute commands marked with FCVAR_SERVER_CAN_EXECUTE on the client.
	bool m_bRestrictClientCommands;	// If true, then IVEngineClient::ClientCmd is only allowed to execute commands marked with FCVAR_CLIENTCMD_CAN_EXECUTE on the client.
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEngineRecipientFilter : public IRecipientFilter
{
public:	// IRecipientFilter interface:
	
					CEngineRecipientFilter();
	virtual int		GetRecipientCount( void ) const {};
	virtual int		GetRecipientIndex( int slot ) const {};
	virtual bool	IsReliable( void ) const { return m_bReliable; };
	virtual bool	IsInitMessage( void )  const { return m_bInit; };

public:

	void			AddPlayersFromFilter( const IRecipientFilter *filter );
	
private:

	bool				m_bInit;
	bool				m_bReliable;
	CUtlVector< int >	m_Recipients;
};

class SendTable;
class ClientClass;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEventInfo
{
public:
	enum
	{
		EVENT_INDEX_BITS = 8,
		EVENT_DATA_LEN_BITS = 11,
		MAX_EVENT_DATA = 192,  // ( 1<<8 bits == 256, but only using 192 below )
	};

	inline CEventInfo()
	{
		classID = 0;
		fire_delay = 0.0f;
		bits = 0;
		flags = 0;
		pSendTable = NULL;
		pClientClass = NULL;
		pData = NULL;
	}

	~CEventInfo()
	{
		if ( pData )
		{
			delete pData;
		}
	}

	CEventInfo( const CEventInfo& src )
	{
		classID = src.classID;
		fire_delay = src.fire_delay;
		bits = src.bits;
		flags = src.flags;
		pSendTable = src.pSendTable;
		pClientClass = src.pClientClass;
		filter.AddPlayersFromFilter( &src.filter );
				
		if ( src.pData )
		{
			int size = Bits2Bytes( src.bits );
			pData = new byte[size];
			Q_memcpy( pData, src.pData, size );
		}
		else
		{
			pData = NULL;
		}

	}

	// 0 implies not in use
	short classID;
	
	// If non-zero, the delay time when the event should be fired ( fixed up on the client )
	float fire_delay;

	// send table pointer or NULL if send as full update
	const SendTable *pSendTable;
	const ClientClass *pClientClass;
	
	// Length of data bits
	int		bits;
	// Raw event data
	byte	*pData;
	// CLIENT ONLY Reliable or not, etc.
	int		flags;
	
	// clients that see that event
	CEngineRecipientFilter filter;
};

#define	MAX_DEMOS		32

struct AddAngle
{
	float total;
	float starttime;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPrecacheItem
{
public:

private:
	enum
	{
		TYPE_UNK = 0,
		TYPE_MODEL,
		TYPE_SOUND,
		TYPE_GENERIC,
		TYPE_DECAL
	};

	unsigned int	m_nType : 3;
	unsigned int	m_uiRefcount : 29;
	union precacheptr
	{
		model_t		*model;
		char const	*generic;
		void		*sound;
		char const	*name;
	} u;
};

// TODO substitute CClientFrameManager with an intelligent structure (Tree, hash, cache, etc)
class CClientFrameManager
{
public:
	CClientFrameManager(void);
	virtual ~CClientFrameManager(void);

private:

	CClientFrame	*m_Frames;		// updates can be delta'ed from here
	CClientFrame	*m_LastFrame;
	int				m_nFrames;
	CClassMemoryPool< CClientFrame >	m_ClientFramePool;
};

//-----------------------------------------------------------------------------
// Purpose: CClientState should hold all pieces of the client state
//   The client_state_t structure is wiped completely at every server signon
//-----------------------------------------------------------------------------
class CClientState : public CBaseClientState, public CClientFrameManager
{
	typedef struct CustomFile_s
	{
		CRC32_t			crc;	//file CRC
		unsigned int	reqID;	// download request ID
	} CustomFile_t;

public:
	CClientState();
	~CClientState();

public: // IConnectionlessPacketHandler interface:
		
	bool ProcessConnectionlessPacket(struct netpacket_s *packet) OVERRIDE;

public: // CBaseClientState overrides:
	void Disconnect( const char *pszReason, bool bShowMainMenu ) OVERRIDE;
	void FullConnect( netadr_t &adr ) OVERRIDE;
	bool SetSignonState ( int state, int count ) OVERRIDE;
	void PacketStart(int incoming_sequence, int outgoing_acknowledged) OVERRIDE;
	void PacketEnd( void ) OVERRIDE;
	void FileReceived( const char *fileName, unsigned int transferID ) OVERRIDE;
	void FileRequested(const char *fileName, unsigned int transferID ) OVERRIDE;
	void FileDenied(const char *fileName, unsigned int transferID ) OVERRIDE;
	void FileSent( const char *fileName, unsigned int transferID ) OVERRIDE;
	void ConnectionCrashed( const char * reason ) OVERRIDE;
	void ConnectionClosing( const char * reason ) OVERRIDE;
	const char *GetCDKeyHash( void ) OVERRIDE;
	void InstallStringTableCallback( char const *tableName ) OVERRIDE;
	bool HookClientStringTable( char const *tableName ) OVERRIDE;
	void RunFrame() OVERRIDE;

public: // IServerMessageHandlers
	
	PROCESS_NET_MESSAGE( Tick );
	
	PROCESS_NET_MESSAGE( StringCmd );
	PROCESS_SVC_MESSAGE( ServerInfo );
	PROCESS_SVC_MESSAGE( ClassInfo );
	PROCESS_SVC_MESSAGE( SetPause );
	PROCESS_SVC_MESSAGE( VoiceInit );
	PROCESS_SVC_MESSAGE( VoiceData );
	PROCESS_SVC_MESSAGE( Sounds );
	PROCESS_SVC_MESSAGE( FixAngle );
	PROCESS_SVC_MESSAGE( CrosshairAngle );
	PROCESS_SVC_MESSAGE( BSPDecal );
	PROCESS_SVC_MESSAGE( GameEvent );
	PROCESS_SVC_MESSAGE( UserMessage );
	PROCESS_SVC_MESSAGE( EntityMessage );
	PROCESS_SVC_MESSAGE( PacketEntities );
	PROCESS_SVC_MESSAGE( TempEntities );
	PROCESS_SVC_MESSAGE( Prefetch );
	
public:

	float		m_flLastServerTickTime;		// the timestamp of last message
	bool		insimulation;

	int			oldtickcount;		// previous tick
	float		m_tickRemainder;	// client copy of tick remainder
	float		m_frameTime;		// dt of the current frame

	int			lastoutgoingcommand;// Sequence number of last outgoing command
	int			chokedcommands;		// number of choked commands
	int			last_command_ack;	// last command sequence number acknowledged by server
	int			command_ack;		// current command sequence acknowledged by server
	int			m_nSoundSequence;	// current processed reliable sound sequence number
	
	//
	// information that is static for the entire time connected to a server
	//
	bool		ishltv;			// true if HLTV server/demo
#if defined( REPLAY_ENABLED )
	bool		isreplay;		// true if Replay server/demo
#endif

	MD5Value_t	serverMD5;              // To determine if client is playing hacked .map. (entities lump is skipped)
	
	unsigned char	m_chAreaBits[MAX_AREA_STATE_BYTES];
	unsigned char	m_chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES];
	bool			m_bAreaBitsValid; // Have the area bits been set for this level yet?
	
// refresh related state
	QAngle		viewangles;
	CUtlVector< AddAngle >	addangle;
	float		addangletotal;
	float		prevaddangletotal;
	int			cdtrack;			// cd audio

	CustomFile_t	m_nCustomFiles[MAX_CUSTOM_FILES]; // own custom files CRCs

	uint		m_nFriendsID;
	char		m_FriendsName[MAX_PLAYER_NAME_LENGTH];


	CUtlFixedLinkedList< CEventInfo > events;	// list of received events

// demo loop control
	int			demonum;		                  // -1 = don't play demos
	CUtlString	demos[MAX_DEMOS];				  // when not playing

public:
		
	void				Clear( void ) OVERRIDE;


	INetworkStringTable *m_pModelPrecacheTable;	
	INetworkStringTable *m_pGenericPrecacheTable;	
	INetworkStringTable *m_pSoundPrecacheTable;
	INetworkStringTable *m_pDecalPrecacheTable;
	INetworkStringTable *m_pInstanceBaselineTable;
	INetworkStringTable *m_pLightStyleTable;
	INetworkStringTable *m_pUserInfoTable;
	INetworkStringTable *m_pServerStartupTable;
	INetworkStringTable *m_pDownloadableFileTable;
	INetworkStringTable *m_pDynamicModelsTable;
	
	CPrecacheItem		model_precache[ MAX_MODELS ];
	CPrecacheItem		generic_precache[ MAX_GENERIC ];
	CPrecacheItem		sound_precache[ MAX_SOUNDS ];
	CPrecacheItem		decal_precache[ MAX_BASE_DECALS ];

	WaitForResourcesHandle_t m_hWaitForResourcesHandle;
	bool m_bUpdateSteamResources;
	bool m_bShownSteamResourceUpdateProgress;
	bool m_bDownloadResources;
	bool m_bPrepareClientDLL;
	bool m_bCheckCRCsWithServer;
	float m_flLastCRCBatchTime;

	// This is only kept around to print out the whitelist info if sv_pure is used.
	void *m_pPureServerWhitelist;

	IFileList *m_pPendingPureFileReloads;

public:
	
	// Note: This is only here for backwards compatibility. If it is set to something other than NULL,
	// then we'll copy its contents into m_chAreaBits in UpdateAreaBits_BackwardsCompatible.
	byte		*m_pAreaBits;
	
	// Set to false when we first connect to a server and true later on before we
	// respond to a new whitelist.
	bool		m_bMarkedCRCsUnverified;

	int crashint1;
	int crashint2;
	int crashint3;
	int crashint4;
	int crashint5;

	int crashints[100];
};  //CClientState