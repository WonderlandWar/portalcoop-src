#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "vgui_entitypanel.h"
#include "iclientmode.h"
#include "vgui/ILocalize.h"
#include "portal_shareddefs.h"

#include "portal_gamerules.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWaitingForPlayersText : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CWaitingForPlayersText, vgui::Panel );

public:
	CWaitingForPlayersText( const char *pElementName );
	bool ShouldDraw( void ) OVERRIDE;
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );

private:

	vgui::HFont		m_hFont;
};

DECLARE_HUDELEMENT( CWaitingForPlayersText );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWaitingForPlayersText::CWaitingForPlayersText( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "WaitingForPlayersText" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_hFont = g_hFontTrebuchet24;
}

bool CWaitingForPlayersText::ShouldDraw( void )
{
	if ( !pcoop_require_all_players.GetBool() )
		return false;

	if ( !PortalGameRules() )
		return false;
	
	if ( !PortalGameRules()->IsGamePaused() )
		return false;

	return CHudElement::ShouldDraw();
}

void CWaitingForPlayersText::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_hFont = scheme->GetFont( "TargetID", IsProportional() );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CWaitingForPlayersText::Paint()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->IsObserver() )
	{
		wchar_t wszObserverText[64];
		g_pVGuiLocalize->ConstructString( wszObserverText, sizeof(wszObserverText), g_pVGuiLocalize->Find("#portalcoop_waiting_for_players_spectator"), 0 );

		int ypos = YRES(280);
		int xpos = XRES(175);

		if ( wszObserverText[0] )
		{
			int wide, tall;
			vgui::surface()->GetTextSize( m_hFont, wszObserverText, wide, tall );
					
			vgui::surface()->DrawSetTextFont( m_hFont );
			vgui::surface()->DrawSetTextPos( xpos, ypos );
		
			Color c = COLOR_YELLOW;

			vgui::surface()->DrawSetTextColor( c );
			vgui::surface()->DrawPrintText( wszObserverText, wcslen(wszObserverText) );
		}
	}
	wchar_t wszWaitingText[32];
	g_pVGuiLocalize->ConstructString( wszWaitingText, sizeof(wszWaitingText), g_pVGuiLocalize->Find("#portalcoop_waiting_for_players"), 0 );
	
	int ypos = YRES(260);
	int xpos = XRES(250);

	if ( wszWaitingText[0] )
	{
		int wide, tall;
		vgui::surface()->GetTextSize( m_hFont, wszWaitingText, wide, tall );
					
		vgui::surface()->DrawSetTextFont( m_hFont );
		vgui::surface()->DrawSetTextPos( xpos, ypos );
		
		Color c = COLOR_YELLOW;

		vgui::surface()->DrawSetTextColor( c );
		vgui::surface()->DrawPrintText( wszWaitingText, wcslen(wszWaitingText) );
	}
}
