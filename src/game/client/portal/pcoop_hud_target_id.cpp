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

	C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPortalPlayer();

	if ( !pLocalPlayer )
		return;

	Color c = Color(255, 255, 255, 255);
	Color cPortal = Color(255, 255, 255, 255);

	// Get our target's ent index
	int iEntIndex = pLocalPlayer->GetIDTarget();
	int iPortalEntIndex = pLocalPlayer->GetPortalIDTarget();

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
	
	C_WeaponPortalgun *pPortalgun = NULL;

	// Is this an entindex sent by the server?
	if ( iEntIndex || iPortalEntIndex)
	{
		C_BasePlayer *pPlayer = static_cast<C_BasePlayer*>(ClientEntityList().GetEnt(iEntIndex));
		C_Prop_Portal *pPortal = static_cast<C_Prop_Portal*>(ClientEntityList().GetEnt(iPortalEntIndex));

		trace_t tr;
		Vector vecStart, vecEnd;
		VectorMA(MainViewOrigin(), 1500, MainViewForward(), vecEnd);
		VectorMA(MainViewOrigin(), 10, MainViewForward(), vecStart);

		UTIL_TraceLine(vecStart, vecEnd, MASK_SOLID, pLocalPlayer, COLLISION_GROUP_NONE, &tr);

		Ray_t ray;
		ray.Init(tr.startpos, tr.endpos);

		
		float flMustBeCloserThan = 2.0f;
		CProp_Portal *pNewPortal = UTIL_Portal_FirstAlongRayAll(ray, flMustBeCloserThan);
		
	//	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		const char *printFormatString = NULL;
		const char *printPortalFormatString = NULL;
		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszLinkageID[ 4 ];
		wchar_t wszPortalLinkageID[ 4 ];
		bool bShowPlayerName = false;
		//Portals
		bool bShowLinkageID = false;
		bool bShowPortalLinkageID = false;

		// Some entities we always want to check, cause the text may change
		// even while we're looking at it
		// Is it a player?
		if ( ( IsPlayerIndex( iEntIndex ) && pPlayer && !pPlayer->IsLocalPlayer() ) || pPortal )
		{
			bShowPlayerName = true;
			if ( IsPlayerIndex( iEntIndex ) && pPlayer && !pPlayer->IsLocalPlayer() )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(),  wszPlayerName, sizeof(wszPlayerName) );

				pPortalgun = dynamic_cast<C_WeaponPortalgun*>(pPlayer->Weapon_OwnsThisType("weapon_portalgun"));
			}
		
			int iLinkageGroupIDPortal = pPortal->m_iLinkageGroupID;
						
			if ( pPortalgun )
			{
				int iLinkageGroupID = pPortalgun->m_iPortalLinkageGroupID;
				std::string s = std::to_string(iLinkageGroupID);
				const char *sLinkageID = (s.c_str());

				g_pVGuiLocalize->ConvertANSIToUnicode( sLinkageID,  wszLinkageID, sizeof(wszLinkageID) );
				
				if (pPlayer->entindex())
					c = GetColorForPortalgun(pPlayer->entindex());
				
				if ( hud_showportalid.GetBool() )
				{
					bShowLinkageID = true;
					if (!pPlayer->IsLocalPlayer())
					{
						printFormatString = "#Playerid_linkageid";
					}
					else
					{
						printFormatString = "#Playerid_linkageid_you";
					}
				}
				else
				{
					if (!pPlayer->IsLocalPlayer())
						printFormatString = "#Playerid_name";
					else
					{
						printFormatString = "#Playerid_name_you";
					}
				}
			}
			else if (!pPortalgun && pPlayer)
			{
				c = COLOR_GREY;

				if (!pPlayer->IsLocalPlayer())
					printFormatString = "#Playerid_name";
				else
				{
					printFormatString = "#Playerid_name_you";
				}
			}
			if (pPortal)
			{
				if (hud_showportals.GetBool())
				{
					std::string s = std::to_string(iLinkageGroupIDPortal);
					const char *sLinkageID = (s.c_str());

					g_pVGuiLocalize->ConvertANSIToUnicode( sLinkageID,  wszPortalLinkageID, sizeof(wszPortalLinkageID) );

					if (pPortal->entindex())
						cPortal = GetColorForPortal(pPortal->entindex());

				
					if ( hud_showportals.GetBool() )
					{
						bShowPortalLinkageID = true;

						if (!pPlayer->IsLocalPlayer())
						{
							printPortalFormatString = "#Portalid_linkageid";
						}
						else
						{
							printPortalFormatString = "#Portalid_linkageid";
						}
					}
					else
					{
						printPortalFormatString = "";
					}
				}
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
		
		if ( sPortalIDString[0] && hud_showportals.GetBool() && pPortal && pPortal == pNewPortal && bShowPortalLinkageID )
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
