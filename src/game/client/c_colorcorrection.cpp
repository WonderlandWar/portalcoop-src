//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Color correction entity with simple radial falloff
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"

#include "filesystem.h"
#include "cdll_client_int.h"
#include "colorcorrectionmgr.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "view.h"
#include "debugoverlay_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ConVar mat_colcorrection_disableentities( "mat_colcorrection_disableentities", "0", FCVAR_NONE, "Disable map color-correction entities" );


//------------------------------------------------------------------------------
// Purpose : Color correction entity with radial falloff
//------------------------------------------------------------------------------
class C_ColorCorrection : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ColorCorrection, C_BaseEntity );

	DECLARE_CLIENTCLASS();

	C_ColorCorrection();
	virtual ~C_ColorCorrection();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

	void ClientThink();

	float	m_minFalloff;
	float	m_maxFalloff;

	Vector	m_vecOrigin;

	ClientCCHandle_t CCHandle() const { return m_CCHandle; }

private:

	float	m_flCurWeight;
	char	m_netLookupFilename[MAX_PATH];

	bool	m_bEnabled;

	ClientCCHandle_t m_CCHandle;
};

IMPLEMENT_CLIENTCLASS_DT(C_ColorCorrection, DT_ColorCorrection, CColorCorrection)
	RecvPropVector( RECVINFO(m_vecOrigin) ),
	RecvPropFloat(  RECVINFO(m_minFalloff) ),
	RecvPropFloat(  RECVINFO(m_maxFalloff) ),
	RecvPropFloat(  RECVINFO(m_flCurWeight) ),
	RecvPropString( RECVINFO(m_netLookupFilename) ),
	RecvPropBool(   RECVINFO(m_bEnabled) ),

END_RECV_TABLE()

CUtlVector<C_ColorCorrection*> g_AllColorCorrections;

//------------------------------------------------------------------------------
// Constructor, destructor
//------------------------------------------------------------------------------
C_ColorCorrection::C_ColorCorrection()
{
	m_CCHandle = INVALID_CLIENT_CCHANDLE;
	g_AllColorCorrections.AddToTail( this );
}

C_ColorCorrection::~C_ColorCorrection()
{
	g_pColorCorrectionMgr->RemoveColorCorrection( m_CCHandle );
	g_AllColorCorrections.FindAndRemove( this );
}

struct CCHandleInfo_t
{
	char filename[MAX_PATH];
	ClientCCHandle_t handle;
};

CUtlVector<CCHandleInfo_t> g_AllCCHandles;

void PurgeAndDeleteCCHandles()
{
	g_AllCCHandles.Purge();
}

ClientCCHandle_t GetCachedCCHandleFromFile( const char *filename )
{
	for ( int i = 0; i < g_AllCCHandles.Count(); ++i )
	{
		if ( !stricmp( g_AllCCHandles[i].filename, filename ) )
		{
			return g_AllCCHandles[i].handle;
		}
	}

	return INVALID_CLIENT_CCHANDLE;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_ColorCorrection::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if ( m_CCHandle == INVALID_CLIENT_CCHANDLE )
		{
			ClientCCHandle_t handle = GetCachedCCHandleFromFile( m_netLookupFilename );
			if ( handle != INVALID_CLIENT_CCHANDLE )
			{
				m_CCHandle = handle;
			}
			else
			{
				m_CCHandle = g_pColorCorrectionMgr->AddColorCorrection( m_netLookupFilename );
				
				CCHandleInfo_t info;
				info.handle = m_CCHandle;
				Q_strncpy( info.filename, m_netLookupFilename, MAX_PATH );
				g_AllCCHandles.AddToTail( info );
			}

			SetNextClientThink( ( m_CCHandle != INVALID_CLIENT_CCHANDLE ) ? CLIENT_THINK_ALWAYS : CLIENT_THINK_NEVER );
		}
	}
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool C_ColorCorrection::ShouldDraw()
{
	return false;
}

#ifdef PORTAL


C_ColorCorrection *GetNearestDistBasedColorCorrectionWithHandle( ClientCCHandle_t handle )
{
	float flFinalDistance = FLT_MAX;
	C_ColorCorrection *pFinalCorrection = NULL;

	for (int i = 0; i < g_AllColorCorrections.Count(); i++)
	{
		C_ColorCorrection *pCorrection = g_AllColorCorrections[i];

		if ( !pCorrection )
		{
			continue;
		}

		if ( pCorrection->CCHandle() != handle )
			continue;
		
		bool bUseDist = ( pCorrection->m_minFalloff != -1 ) && ( pCorrection->m_maxFalloff != -1 ) && pCorrection->m_minFalloff != pCorrection->m_maxFalloff;
		if ( !bUseDist )
		{
			continue;
		}
		float flDistance = (pCorrection->m_vecOrigin - MainViewOrigin()).LengthSqr();

		if (flDistance < flFinalDistance)
		{
			pFinalCorrection = pCorrection;
			flFinalDistance = flDistance;
		}
	}

	return pFinalCorrection;
}

#endif

void C_ColorCorrection::ClientThink()
{
	if ( m_CCHandle == INVALID_CLIENT_CCHANDLE )
		return;

	if ( mat_colcorrection_disableentities.GetInt() )
	{
		// Allow the colorcorrectionui panel (or user) to turn off color-correction entities
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, 0.0f );
		return;
	}

	bool bUseDist = ( m_minFalloff != -1 ) && ( m_maxFalloff != -1 ) && m_minFalloff != m_maxFalloff;

	if( bUseDist && GetNearestDistBasedColorCorrectionWithHandle( m_CCHandle ) != this )
		return;

	if( !m_bEnabled && m_flCurWeight == 0.0f )
	{
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, 0.0f );
		return;
	}

	// Debug stuff
	//Msg( "Handle %i\n", (int)m_CCHandle );
	//NDebugOverlay::Box( m_vecOrigin, Vector(-8-8-8), Vector(8,8,8), 255, 0, 0, 100, gpGlobals->frametime );
	
	float weight = 0;
	if ( bUseDist )
	{
		Vector playerOrigin = MainViewOrigin();
		float dist = (playerOrigin - m_vecOrigin).Length();
		weight = (dist-m_minFalloff) / (m_maxFalloff-m_minFalloff);
		if ( weight<0.0f ) weight = 0.0f;	
		if ( weight>1.0f ) weight = 1.0f;	
	}
	
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, m_flCurWeight * ( 1.0 - weight ) );

	BaseClass::ClientThink();
}













