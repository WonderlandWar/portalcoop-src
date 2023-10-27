//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD Target ID element
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_portal_player.h"
#include "c_playerresource.h"
#include "vgui_entitypanel.h"
#include "iclientmode.h"
#include "vgui/ILocalize.h"
#include "portal_gamerules.h"
#include "c_weapon_portalgun.h"
#include <string>
#include "view.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PLAYER_HINT_DISTANCE	150
#define PLAYER_HINT_DISTANCE_SQ	(PLAYER_HINT_DISTANCE*PLAYER_HINT_DISTANCE)

static ConVar hud_centerid( "hud_centerid", "1" );
static ConVar hud_showtargetid( "hud_showtargetid", "1" );
ConVar hud_showportalid("hud_showportalid", "0", FCVAR_ARCHIVE );

ConVar hud_showportals( "hud_showportals", "0", FCVAR_ARCHIVE );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTargetID : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTargetID, vgui::Panel );

public:
	CTargetID( const char *pElementName );
	void Init( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );
	void VidInit( void );

private:
	Color			GetColorForPortalgun( int iIndex );
	Color			GetColorForPortal( int iIndex );

	vgui::HFont		m_hFont;
	int				m_iLastEntIndex;
	int				m_iPortalLastEntIndex;
	float			m_flLastChangeTime;
	float			m_flLastPortalChangeTime;
};

DECLARE_HUDELEMENT( CTargetID );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTargetID::CTargetID( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "TargetID" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_hFont = g_hFontTrebuchet24;
	m_flLastChangeTime = 0;
	m_flLastPortalChangeTime = 0;
	m_iLastEntIndex = 0;
	m_iPortalLastEntIndex = 0;

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: Setup
//-----------------------------------------------------------------------------
void CTargetID::Init( void )
{
};

void CTargetID::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_hFont = scheme->GetFont( "TargetID", IsProportional() );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: clear out string etc between levels
//-----------------------------------------------------------------------------
void CTargetID::VidInit()
{
	CHudElement::VidInit();

	m_flLastChangeTime = 0;
	m_flLastPortalChangeTime = 0;
	m_iLastEntIndex = 0;
	m_iPortalLastEntIndex = 0;
}

Color CTargetID::GetColorForPortalgun( int iIndex )
{
	return g_PR->GetPortalgunColor( iIndex );
} 

Color CTargetID::GetColorForPortal( int iIndex )
{
	return g_PR->GetPortalColor( iIndex );
} 

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CTargetID::Paint()
{
#define MAX_ID_STRING 256
	wchar_t sIDString[ MAX_ID_STRING ];
	sIDString[0] = 0;
	wchar_t sPortalIDString[MAX_ID_STRING];
	sPortalIDString[0] = 0;
	wchar_t sPortalGunIDString[MAX_ID_STRING];
	sPortalGunIDString[0] = 0;

	C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPortalPlayer();

	if ( !pLocalPlayer )
		return;

	Color c = Color(255, 255, 255, 255);
	Color cPortal = Color(255, 255, 255, 255);
	Color cPortalgun = Color(255, 255, 255, 255);

	// Get our target's ent index
	int iEntIndex = pLocalPlayer->GetIDTarget();

	//Nonsensical debugging
#if 0
	CBasePlayer *pPlayerIndex = UTIL_PlayerByIndex(iEntIndex);

	if (pPlayerIndex == pPlayer)
	{
		Msg("Paint is me??\n");
	}
#endif
	// Didn't find one?
	if ( !iEntIndex )
	{
		// Check to see if we should clear our ID
		if ( m_flLastChangeTime && (gpGlobals->curtime > (m_flLastChangeTime + 0.5)) )
		{
			m_flLastChangeTime = 0;
			sIDString[0] = 0;
			m_iLastEntIndex = 0;
		}
		else
		{
			// Keep re-using the old one
			iEntIndex = m_iLastEntIndex;
		}
	}
	else
	{
		m_flLastChangeTime = gpGlobals->curtime;
	}


	C_Prop_Portal *pPortal = pLocalPlayer->GetPortalTarget();

#if 0
	if ( !iPortalEntIndex )
	{
		// Check to see if we should clear our ID
		if ( m_flLastPortalChangeTime && (gpGlobals->curtime > (m_flLastPortalChangeTime + 0.5)) )
		{
			m_flLastPortalChangeTime = 0;
			sPortalIDString[0] = 0;
			m_iPortalLastEntIndex = 0;
		}
		else
		{
			// Keep re-using the old one
			iPortalEntIndex = m_iPortalLastEntIndex;
		}
	}
	else
	{
		m_flLastPortalChangeTime = gpGlobals->curtime;
	}
#endif

	if ( pPortal )
	{
		//if ( pPortal )
		{
			const char *printPortalFormatString = NULL;
			wchar_t wszPortalLinkageID[4];
			bool bShowPortalLinkageID = false;

			int iLinkageGroupIDPortal = pPortal->m_iLinkageGroupID;

			if (hud_showportals.GetBool())
			{
				std::string s = std::to_string(iLinkageGroupIDPortal);
				const char *sLinkageID = (s.c_str());

				g_pVGuiLocalize->ConvertANSIToUnicode( sLinkageID,  wszPortalLinkageID, sizeof(wszPortalLinkageID) );

				cPortal = GetColorForPortal(pPortal->entindex());

				
				if ( hud_showportals.GetBool() )
				{
					bShowPortalLinkageID = true;

					printPortalFormatString = "#Portalid_linkageid";						
				}
				else
				{
						printPortalFormatString = "";
				}
			}
			
			if (printPortalFormatString)
			{
				// For showing portals
			
				if ( bShowPortalLinkageID )
				{
					g_pVGuiLocalize->ConstructString( sPortalIDString, sizeof(sPortalIDString), g_pVGuiLocalize->Find(printPortalFormatString), 1, wszPortalLinkageID );
				}
				else
				{
					g_pVGuiLocalize->ConstructString( sPortalIDString, sizeof(sPortalIDString), g_pVGuiLocalize->Find(printPortalFormatString), 0 );
				}
			}

			
		
			if ( sPortalIDString[0] && hud_showportals.GetBool() && pPortal && bShowPortalLinkageID )
			{
				int wide, tall;
				int ypos = YRES(150);
				int xpos = XRES(10);

				vgui::surface()->GetTextSize( m_hFont, sPortalIDString, wide, tall );

				if( hud_centerid.GetInt() == 0 )
				{
					ypos = YRES(380);
				}
				else
				{
					xpos = (ScreenWidth() - wide) / 2;
				}
			
				vgui::surface()->DrawSetTextFont( m_hFont );
				vgui::surface()->DrawSetTextPos( xpos, ypos );
				vgui::surface()->DrawSetTextColor( cPortal );
				vgui::surface()->DrawPrintText( sPortalIDString, wcslen(sPortalIDString) );
			}
		}
	}

	// Is this an entindex sent by the server?
	if ( iEntIndex )
	{
		C_BasePlayer *pPlayer = ToBasePlayer( ClientEntityList().GetEnt( iEntIndex ) );
		C_WeaponPortalgun *pPortalGunTarget = dynamic_cast<C_WeaponPortalgun*>( ClientEntityList().GetEnt( iEntIndex ) );
		
	//	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		const char *printFormatString = NULL;
		const char *printPortalGunFormatString = NULL;
		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszLinkageID[ 4 ];
		wchar_t wszPortalGunMessage[ 64 ];
		bool bShowPlayerName = false;
		//Portals
		bool bShowLinkageID = false;
		bool bShowPortalgun = false;

		// Some entities we always want to check, cause the text may change
		// even while we're looking at it
				
		if (pPortalGunTarget)
		{
			const char *pszPlayerOnly = NULL;

			C_Portal_Player *pPlayer = static_cast<C_Portal_Player*>( UTIL_PlayerByIndex( pPortalGunTarget->m_iValidPlayer ) );

			if (pPlayer && !pPlayer->IsLocalPlayer())
			{
				bShowPortalgun = true;
				printPortalGunFormatString = "#portalgunid_validpickup";
				pszPlayerOnly = pPlayer->GetPlayerName();

				g_pVGuiLocalize->ConvertANSIToUnicode(pszPlayerOnly, wszPortalGunMessage, sizeof(wszPortalGunMessage));

				UTIL_Portalgun_Color(pPortalGunTarget, cPortalgun);
			}
		}

		C_WeaponPortalgun *pPortalgun = NULL;

		if ( pPlayer && !pPlayer->IsLocalPlayer() )
		{
			bShowPlayerName = true;

			if ( IsPlayerIndex( iEntIndex ) && pPlayer && !pPlayer->IsLocalPlayer() )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(),  wszPlayerName, sizeof(wszPlayerName) );

				pPortalgun = static_cast<C_WeaponPortalgun*>(pPlayer->Weapon_OwnsThisType("weapon_portalgun"));
			}
					
			c = GetColorForPortalgun(pPlayer->entindex());
						
			if ( pPortalgun )
			{
				int iLinkageGroupID = pPortalgun->m_iPortalLinkageGroupID;
				std::string s = std::to_string(iLinkageGroupID);
				const char *sLinkageID = (s.c_str());

				g_pVGuiLocalize->ConvertANSIToUnicode( sLinkageID,  wszLinkageID, sizeof(wszLinkageID) );
								
				if ( hud_showportalid.GetBool() )
				{
					bShowLinkageID = true;
					//if (!pPlayer->IsLocalPlayer())
						printFormatString = "#Playerid_linkageid";
					//else
					//	printFormatString = "#Playerid_linkageid_you";
				}
				else
				{
					//if (!pPlayer->IsLocalPlayer())
						printFormatString = "#Playerid_name";
					//else
					//	printFormatString = "#Playerid_name_you";
				}
			}
			else
			{
				//if (!pPlayer->IsLocalPlayer())
					printFormatString = "#Playerid_name";
					//else
					//	printFormatString = "#Playerid_name_you";
			}
		}

		if ( printFormatString )
		{
			//For showing players

			if ( bShowPlayerName && bShowLinkageID )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 2, wszPlayerName, wszLinkageID );
			}
			else if ( bShowPlayerName )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszPlayerName );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 0 );
			}
		}
				
		if (printPortalGunFormatString)
		{
			// For showing portalgun stuff
			
			if ( bShowPortalgun )
			{
				g_pVGuiLocalize->ConstructString( sPortalGunIDString, sizeof(sPortalGunIDString), g_pVGuiLocalize->Find(printPortalGunFormatString), 1, wszPortalGunMessage );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( sPortalGunIDString, sizeof(sPortalGunIDString), g_pVGuiLocalize->Find(printPortalGunFormatString), 0 );
			}
		}

		if ( sIDString[0] && IsPlayerIndex( iEntIndex ) && pPlayer )
		{
			int wide, tall;
			int ypos = YRES(260);
			int xpos = XRES(10);

			vgui::surface()->GetTextSize( m_hFont, sIDString, wide, tall );

			if( hud_centerid.GetInt() == 0 )
			{
				ypos = YRES(420);
			}
			else
			{
				xpos = (ScreenWidth() - wide) / 2;
			}
			
			vgui::surface()->DrawSetTextFont( m_hFont );
			vgui::surface()->DrawSetTextPos( xpos, ypos );
			vgui::surface()->DrawSetTextColor( c );
			vgui::surface()->DrawPrintText( sIDString, wcslen(sIDString) );
		}

		
		if ( sPortalGunIDString[0] && pPortalGunTarget && bShowPortalgun )
		{
			int wide, tall;
			int ypos = YRES(260);
			int xpos = XRES(10);

			vgui::surface()->GetTextSize( m_hFont, sPortalGunIDString, wide, tall );

			if( hud_centerid.GetInt() == 0 )
			{
				ypos = YRES(420);
			}
			else
			{
				xpos = (ScreenWidth() - wide) / 2;
			}
			
			vgui::surface()->DrawSetTextFont( m_hFont );
			vgui::surface()->DrawSetTextPos( xpos, ypos );
			vgui::surface()->DrawSetTextColor( cPortalgun );
			vgui::surface()->DrawPrintText( sPortalGunIDString, wcslen(sPortalGunIDString) );
		}
	}
}
