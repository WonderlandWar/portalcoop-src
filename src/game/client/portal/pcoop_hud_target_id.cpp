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
	virtual bool	ShouldDraw( void ) OVERRIDE;
	virtual void	Paint( void );
	void VidInit( void );

private:

	vgui::HFont		m_hFont;
	EHANDLE			m_hLastEnt;
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
	m_hLastEnt = NULL;

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
	m_hLastEnt = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
bool CTargetID::ShouldDraw()
{
	if ( PortalGameRules() && PortalGameRules()->IsGamePaused() )
	{
		return false;
	}

	if ( !hud_showtargetid.GetBool() )
	{
		return false;
	}

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CTargetID::Paint()
{
#define MAX_ID_STRING 256
	wchar_t sIDString[ MAX_ID_STRING ];
	sIDString[0] = 0;

	C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPortalPlayer();

	if ( !pLocalPlayer )
		return;

	Color c = Color(255, 255, 255, 255);

	// Get our target's ent index
	CBaseEntity *pEnt = pLocalPlayer->GetTargetIDEnt();

	// Didn't find one?
	if ( !pEnt )
	{
		// Check to see if we should clear our ID
		if ( m_flLastChangeTime && (gpGlobals->curtime > (m_flLastChangeTime + 0.5)) )
		{
			m_flLastChangeTime = 0;
			sIDString[0] = 0;
			m_hLastEnt = NULL;
		}
		else
		{
			// Keep re-using the old one
			pEnt = m_hLastEnt;
		}
	}
	else
	{
		m_flLastChangeTime = gpGlobals->curtime;
	}

	// Is this an entindex sent by the server?
	if ( pEnt )
	{
		C_Prop_Portal *pPortal = ( pEnt && FClassnameIs( pEnt, "prop_portal" ) ) ? static_cast<C_Prop_Portal*>( pEnt ) : NULL;

		C_BasePlayer *pTargetPlayer = ToBasePlayer( pEnt );
		C_WeaponPortalgun *pPortalGunTarget = ( pEnt && FClassnameIs( pEnt, "weapon_portalgun" ) ) ? static_cast<C_WeaponPortalgun*>( pEnt ) : NULL;
		
		const char *printFormatString = NULL;
		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH + 32 ];
		wchar_t wszLinkageID[ 4 ];
		wchar_t wszPortalOwner[MAX_PLAYER_NAME_LENGTH];
		bool bShowPlayerName = false;
		//Portals
		bool bShowOtherPortalgun = false;
		//bool bShowMyPortalgun = false;
		bool bShowOtherPortal = false;
		//bool bShowMyPortal = false;

		// Some entities we always want to check, cause the text may change
		// even while we're looking at it
				
		if (pPortalGunTarget)
		{
			C_Portal_Player *pPlayer = static_cast<C_Portal_Player*>( UTIL_PlayerByIndex( pPortalGunTarget->m_iValidPlayer ) );

			if (pPlayer )
			{
				if ( !pPlayer->IsLocalPlayer() )
				{
					printFormatString = "#portalgunid_validpickup";
					const char *pszPlayerOnly = pPlayer->GetPlayerName();
					g_pVGuiLocalize->ConvertANSIToUnicode(pszPlayerOnly, wszPlayerName, sizeof(wszPlayerName));
					bShowOtherPortalgun = true;
				}
				else
				{
					printFormatString = "#portalgunid_yours";
					g_pVGuiLocalize->ConvertANSIToUnicode("", wszPlayerName, sizeof(wszPlayerName));
					//bShowMyPortalgun = true;
				}

				UTIL_Portal_ColorSet_Color( GetColorSetForPlayer( pPortalGunTarget->m_iValidPlayer ), c );
			}
		}
		else if ( pPortal && hud_showportals.GetBool() )
		{
			C_Portal_Player *pPortalOwner = NULL;
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				C_Portal_Player *pPlayer = (C_Portal_Player*)UTIL_PlayerByIndex( i );
				if ( !pPlayer )
					continue;

				C_WeaponPortalgun *pPortalgun = (C_WeaponPortalgun *)pPlayer->Weapon_OwnsThisType( "weapon_portalgun" );
				if ( !pPortalgun || pPortalgun->m_iPortalLinkageGroupID != pPortal->m_iLinkageGroupID )
					continue;

				pPortalOwner = pPlayer;
				break;
			}

			if ( pPortalOwner )
			{
				if ( !pPortalOwner->IsLocalPlayer() )
				{
					g_pVGuiLocalize->ConvertANSIToUnicode( pPortalOwner->GetPlayerName(), wszPortalOwner, sizeof(wszPortalOwner));
					bShowOtherPortal = true;
					printFormatString = "#Portalid_owner";
				}
				else
				{
					g_pVGuiLocalize->ConvertANSIToUnicode( "", wszPortalOwner, sizeof(wszPortalOwner));
					//bShowMyPortal = true;
					printFormatString = "#portalid_yours";
				}

				c = UTIL_Portal_Color( pPortal->m_bIsPortal2 ? 2 : 1, ConvertLinkageIDToColorSet( pPortal->m_iLinkageGroupID ) );
			}
		}
		else if ( pTargetPlayer && !pTargetPlayer->IsLocalPlayer() )
		{
			bShowPlayerName = true;
			g_pVGuiLocalize->ConvertANSIToUnicode( pTargetPlayer->GetPlayerName(),  wszPlayerName, sizeof(wszPlayerName) );
			UTIL_Portal_ColorSet_Color( GetColorSetForPlayer( pTargetPlayer->entindex() ), c );
			//if (!pTargetPlayer->IsLocalPlayer())
				printFormatString = "#Playerid_name";
				//else
				//	printFormatString = "#Playerid_name_you";
		}

		if ( printFormatString )
		{
			//For showing players
			if ( bShowPlayerName )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszPlayerName );
			}			
			else if ( bShowOtherPortal )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszPortalOwner );
			}
			else if ( bShowOtherPortalgun )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszPlayerName );
			}
			else // if ( bShowMyPortalgun || bShowMyPortal )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 0 );
			}
		}

		if ( sIDString[0] )
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
	}
}
