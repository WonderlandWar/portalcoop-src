//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=====================================================================================//
#include "cbase.h"
#include "c_portal_radio.h"
#include "portal_radio_shared.h"
#include "c_physicsprop.h"
#include "portal_gamerules.h"
#include "igameevents.h"
#include "soundenvelope.h"
#include "beamdraw.h"
#include "filesystem.h"
#include "portal_shareddefs.h"
#include "model_types.h"

KeyValues *LoadRadioData()
{	
	KeyValues *radios = new KeyValues( "radios" );
	if ( !radios->LoadFromFile( g_pFullFileSystem, RADIO_DATA_FILE, NULL ) )
	{
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		radios->RecursiveSaveToFile( buf, 0 );
		g_pFullFileSystem->WriteFile( RADIO_DATA_FILE, "MOD", buf );
		if ( !radios->LoadFromFile( g_pFullFileSystem, RADIO_DATA_FILE, NULL ) )
		{
			radios->deleteThis();
			return NULL;
		}
	}

	return radios;
}

extern const ConVar *sv_cheats;

//#define RADIO_DEBUG_CLIENT

class C_Dinosaur_Signal : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_Dinosaur_Signal, C_BaseEntity );

#ifndef USE_BASIC_RADIOS
	char  m_iszSoundName[128];
#endif
	float m_flInnerRadius;
	float m_flOuterRadius;
	int	  m_nSignalID;
};

IMPLEMENT_CLIENTCLASS_DT( C_Dinosaur_Signal, DT_DinosaurSignal, CDinosaurSignal )
#ifndef USE_BASIC_RADIOS
	RecvPropString( RECVINFO(m_iszSoundName) ), 
#endif
	RecvPropFloat( RECVINFO(m_flOuterRadius) ),
	RecvPropFloat( RECVINFO(m_flInnerRadius) ),
	RecvPropInt( RECVINFO(m_nSignalID) ),
END_RECV_TABLE()

class C_Portal_Dinosaur : public C_PhysicsProp
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_Portal_Dinosaur, C_PhysicsProp );

	C_Portal_Dinosaur( void );
	~C_Portal_Dinosaur( void );

	virtual void Spawn();
	virtual void OnDataChanged( DataUpdateType_t updatetype );
	virtual void ClientThink();
	virtual	int DrawModel( int flags );

	virtual QAngle PreferredCarryAngles( void ) { return QAngle( 0, 180, 0 ); }
	virtual bool HasPreferredCarryAnglesForPlayer( C_BasePlayer *pPlayer ) { return true; }

	const char *GetRadioSongScript( void );
#ifndef USE_BASIC_RADIOS
	void ScanForSounds();
#endif
	void SetupSounds();
#ifndef USE_BASIC_RADIOS
	CHandle<C_Dinosaur_Signal> m_hDinosaur_Signal;
	CHandle<C_Dinosaur_Signal> m_hOldDinosaur_Signal;

	float	m_flOldBlend;
	bool	m_bAlreadyDiscovered;
	bool	m_bDinosaurExtinct;
#endif

	char  m_iszSongName[128];

	// Sound envelopes
	CSoundPatch		*m_pNormalSound;
#ifndef USE_BASIC_RADIOS
	CSoundPatch		*m_pStaticSound;
	CSoundPatch		*m_pSignalSound;
#endif

	RadioMode_t		m_iOldRadioMode;
	RadioMode_t		m_iRadioMode;

	bool			m_bOldShouldPlay;
	bool			m_bShouldPlay;
};

static void RecvProxy_SongName( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	RecvProxy_StringToString( pData, pStruct, pOut );
	
	C_Portal_Dinosaur *pDinosaur = (C_Portal_Dinosaur*)pStruct;
	
	if ( pDinosaur->m_pNormalSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( pDinosaur->m_pNormalSound );
		pDinosaur->m_pNormalSound = NULL;
	}

	pDinosaur->SetupSounds();
}

IMPLEMENT_CLIENTCLASS_DT( C_Portal_Dinosaur, DT_PropDinosaur, CPortal_Dinosaur )
#ifndef USE_BASIC_RADIOS
	RecvPropEHandle( RECVINFO( m_hDinosaur_Signal ) ),
	RecvPropInt( RECVINFO( m_iRadioMode ) ),
	RecvPropBool( RECVINFO( m_bShouldPlay ) ),
	RecvPropString( RECVINFO(m_iszSongName) ), 
#endif
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( prop_radio, C_Portal_Dinosaur );

C_Portal_Dinosaur::C_Portal_Dinosaur()
{
	m_iszSongName[0] = '\0';
	m_iOldRadioMode = m_iRadioMode = RADIO_NUM_MODES;
	m_bShouldPlay = true;
	m_bOldShouldPlay = false;
}

void C_Portal_Dinosaur::Spawn()
{
	Precache();
#ifndef USE_BASIC_RADIOS
	m_flOldBlend = 0.0f;
#endif
	BaseClass::Spawn();
	SetThink( &C_Portal_Dinosaur::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_Portal_Dinosaur::OnDataChanged( DataUpdateType_t updatetype )
{
	BaseClass::OnDataChanged( updatetype );
#ifndef USE_BASIC_RADIOS
	KeyValues *radios = LoadRadioData();
	if ( radios )
	{
		if ( m_hDinosaur_Signal != m_hOldDinosaur_Signal && m_hDinosaur_Signal != NULL )
		{
			m_hOldDinosaur_Signal = m_hDinosaur_Signal;
			
			const char *pszMapSet = g_MapInfo.GetAssociatedMapSet();
			if ( *pszMapSet )
			{
				KeyValues *mapset = radios->FindKey( pszMapSet );
				if ( mapset )
				{
					bool bFound = mapset->GetBool( VarArgs( "%i", m_hDinosaur_Signal.Get()->m_nSignalID ) );
					if ( bFound )
					{
						m_bAlreadyDiscovered = true;
					}
				}
			}
		}
		radios->deleteThis();
	}

	if ( m_iRadioMode != m_iOldRadioMode )
	{
		SetupSounds();
		m_iOldRadioMode = m_iRadioMode;
	}

	if ( m_bOldShouldPlay != m_bShouldPlay )
	{
		SetupSounds();
		m_bOldShouldPlay = m_bShouldPlay;
	}
#endif
}

C_Portal_Dinosaur::~C_Portal_Dinosaur( void )
{
	if ( m_pNormalSound != NULL )
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pNormalSound );
	}
#ifndef USE_BASIC_RADIOS
	if ( m_pStaticSound != NULL )
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pStaticSound );
	}

	if ( m_pSignalSound != NULL )
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pSignalSound );
	}
#endif
}

const char *C_Portal_Dinosaur::GetRadioSongScript( void )
{
	if ( m_iszSongName[0] )
	{
		return m_iszSongName;
	}

	if ( sv_portal_game.GetInt() == PORTAL_GAME_REXAURA )
	{
		return "";
	}

	return DEFAULT_RADIO_SONG;
}

void C_Portal_Dinosaur::SetupSounds()
{
	CPASAttenuationFilter filter( this );

	if ( m_iOldRadioMode != m_iRadioMode )
	{
		if ( m_pNormalSound )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pNormalSound );
			m_pNormalSound = NULL;
		}
	}

	if ( !m_bShouldPlay )
	{
		if ( m_pNormalSound )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pNormalSound );
			m_pNormalSound = NULL;
		}

		return;
	}

	if ( m_pNormalSound == NULL )
	{
		m_pNormalSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), GetRadioSongScript() );
#ifdef USE_BASIC_RADIOS // Basic radios play the full sound
		CSoundEnvelopeController::GetController().Play( m_pNormalSound, VOL_NORM, PITCH_NORM );
#else
		if ( m_iRadioMode == RADIO_NORMAL )
		{
			CSoundEnvelopeController::GetController().Play( m_pNormalSound, VOL_NORM, PITCH_NORM );
		}
		else
		{
			CSoundEnvelopeController::GetController().Play( m_pNormalSound, 0.0, PITCH_NORM );
		}
#endif
	}
#ifndef USE_BASIC_RADIOS
	if ( m_iRadioMode == RADIO_DINOSAUR_DEFAULT )
	{
		if ( m_pStaticSound == NULL )
		{
			m_pStaticSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "UpdateItem.Static" );
			CSoundEnvelopeController::GetController().Play( m_pStaticSound, 0.0, PITCH_NORM );
		}

		if ( m_pSignalSound == NULL && 	m_hDinosaur_Signal.Get() != NULL )
		{
			m_pSignalSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), m_hDinosaur_Signal->m_iszSoundName );
			//m_pSignalSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "UpdateItem.Signal" );
			CSoundEnvelopeController::GetController().Play( m_pSignalSound, 0.0, PITCH_NORM );
		}
	}
	else
	{
		if ( m_pStaticSound )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pStaticSound );
			m_pStaticSound = NULL;
		}
		
		if ( m_pSignalSound )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pSignalSound );
			m_pSignalSound = NULL;
		}
	}
#endif
}

void C_Portal_Dinosaur::ClientThink()
{
	if ( m_iRadioMode != RADIO_DINOSAUR_DEFAULT )
	{
		return;
	}

	SetupSounds();
#ifndef USE_BASIC_RADIOS
	//if ( V_stristr( engine->GetLevelName(), "testchmb_a_00" ) != 0 )
	if ( 0 )//m_hDinosaur_Signal.Get() && m_hDinosaur_Signal.Get()->m_nSignalID == 0 )
	{
		// TODO:Play either morse code or special sstv content based on 
		// game account client state given from gc.
	}
	else
	{
		// On any other map, we scan for the partnered Dinosaur_Signal signal
		ScanForSounds();
	}

	if ( m_bDinosaurExtinct == false && m_bDinosaurExtinct != m_bAlreadyDiscovered )
		m_bDinosaurExtinct = m_bAlreadyDiscovered;
#endif
}
#ifndef USE_BASIC_RADIOS
void C_Portal_Dinosaur::ScanForSounds()
{
	C_Dinosaur_Signal *pSignal = m_hDinosaur_Signal;
	if ( pSignal == NULL )
	{
		AssertMsgOnce( 0, "Unpaired radio exists on this map." );
		return;
	}

	// How much of the real signal to blend in
	// 1.0 at > outerrad, 0.0 at < inner rad, blend when in between the two.
	float flRadiusDelta = fabs( pSignal->m_flOuterRadius - pSignal->m_flInnerRadius );
	float flDist = pSignal->GetAbsOrigin().DistTo( GetAbsOrigin() );

	float flOuterBlend = RemapValClamped( flDist, pSignal->m_flOuterRadius, pSignal->m_flOuterRadius-(flRadiusDelta*0.5f), 1.0f, 0.0f );
	float flInnerBlend = RemapValClamped( flDist, pSignal->m_flOuterRadius-(flRadiusDelta*0.5f), pSignal->m_flInnerRadius, 0.0f, 1.0f );
	flInnerBlend = Bias( flInnerBlend, 0.1f );

	if ( m_pNormalSound )
	{
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pNormalSound, flOuterBlend, 0.1f );
	}

	float flMidBlend = 0.0f;
	if ( m_pStaticSound )
	{
		if ( flOuterBlend <= 0.0f )
		{
			flMidBlend = RemapValClamped( flDist, pSignal->m_flOuterRadius-(flRadiusDelta*0.5f), pSignal->m_flInnerRadius, 1.0f, 0.0f );
		}
		else
		{
			flMidBlend = RemapValClamped( flDist, pSignal->m_flOuterRadius, pSignal->m_flOuterRadius-(flRadiusDelta*0.5f), 0.0f, 1.0f );
			flMidBlend = Bias( flMidBlend, 0.9f );
		}
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pStaticSound, flMidBlend, 0.1f );
	}

	if ( m_pSignalSound )
	{
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pSignalSound, flInnerBlend, 0.1f );
	}

#if defined ( RADIO_DEBUG_CLIENT )
	// Msg( "Dinosaur(%d) listening for signal... dist: %f, blend amt: %f\n", entindex(), flDist, flMidBlend );
#endif
	
	// If we've fully heard this signal, mark the achievement
	if ( flInnerBlend >= 1.0f && m_flOldBlend < 1.0f )
	{
		int id = pSignal->m_nSignalID;
		if ( !engine->IsPlayingDemo() && // Don't save any data if we're running a demo file
			!sv_cheats->GetBool() // Don't save if cheats were enabled 
			) 
		{
			KeyValues *radios = LoadRadioData();
			if ( radios )
			{
				const char *pszMapSet = g_MapInfo.GetAssociatedMapSet();
				if ( *pszMapSet )
				{
					KeyValues *mapset = radios->FindKey( pszMapSet, true );
					if ( mapset )
					{
						const char *pszID = VarArgs( "%i", id );
						KeyValues *radioid = mapset->FindKey( pszID, true );
						radioid->SetStringValue( "1" );
						radios->SaveToFile( g_pFullFileSystem, RADIO_DATA_FILE, "GAME" );
					}

					radios->deleteThis();
				}
			}
		}

		m_bDinosaurExtinct = true;

		IGameEvent *event = gameeventmanager->CreateEvent( "dinosaur_signal_found" );
		if ( event )
		{
			event->SetInt( "id", id );
			gameeventmanager->FireEventClientSide( event );
		}
	}

	m_flOldBlend = flInnerBlend;
}
#endif
int C_Portal_Dinosaur::DrawModel( int flags )
{
	int nRet = BaseClass::DrawModel( flags );

	if ( flags & STUDIO_GLOWING )
	{
		return nRet;
	}

#ifndef USE_BASIC_RADIOS
	if ( m_iRadioMode == RADIO_DINOSAUR_DEFAULT )
	{
		CMaterialReference	hMaterial;
		hMaterial.Init( "sprites/grav_light", TEXTURE_GROUP_CLIENT_EFFECTS );

		// Draw the sprite
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( hMaterial, this );
		color32 color;
		color.r = ( m_bDinosaurExtinct ) ? 0 : 255;
		color.g = ( m_bDinosaurExtinct ) ? 255 : 0;
		color.b = 0;
		color.a = 128;

		Vector vForward, vRight, vUp;
		GetVectors( &vForward, &vRight, &vUp );
		Vector vOffset = GetAbsOrigin() + ( vForward * 4.0f ) + ( vUp * 1.85f );
		DrawSprite( vOffset, 6.0f, 6.0f, color );
	}
#endif
	return nRet;
}
