#include "cbase.h"
#include "portal_mapsetdialog.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "gameui/MouseMessageForwardingPanel.h"
#include "ienginevgui.h"
#include "filesystem.h"
#include "portal_shareddefs.h"
#include "clientsteamcontext.h"
#include "c_portal_player.h"
#include "replay/IEngineReplay.h"

static CMapSetDialog *g_pMapSetDialog = NULL;

using namespace vgui;

class CMapSetItemPanel : public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CMapSetItemPanel, EditablePanel );

	CMapSetItemPanel( PanelListPanel *parent, const char *name, const char *titlename, const char *imagename, const char *controlfile ) : BaseClass( parent, name )
	{
		m_pLevelPicBorder = SETUP_PANEL( new ImagePanel( this, "LevelPicBorder" ) );
		m_pLevelPic = SETUP_PANEL( new ImagePanel( this, "LevelPic" ) );
		
		m_pChapterNameLabel = new Label( this, "NameLabel", titlename );

		m_pParent = parent;

		SetPaintBackgroundEnabled( false );

		m_pLevelPic->SetImage( imagename );
		
		LoadControlSettings( controlfile );
				
		CMouseMessageForwardingPanel *panel = new CMouseMessageForwardingPanel(this, NULL);
		panel->SetZPos(2);
		
		int px, py;
		m_pLevelPicBorder->GetPos( px, py );
		SetSize( m_pLevelPicBorder->GetWide(), py + m_pLevelPicBorder->GetTall() );
	}
	
	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			//m_pChapterLabel->SetFgColor( m_SelectedColor );
			m_pLevelPicBorder->SetFillColor( m_SelectedColor );
			m_pChapterNameLabel->SetFgColor( m_SelectedColor );
		}
		else
		{
			//m_pChapterLabel->SetFgColor( m_TextColor );
			m_pLevelPicBorder->SetFillColor( m_FillColor );
			m_pChapterNameLabel->SetFgColor( m_TextColor );
		}

		PostMessage( m_pParent->GetVParent(), new KeyValues("PanelSelected") );
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		m_pParent->SetSelectedPanel( this );
	}
	
	virtual void OnMouseDoublePressed( vgui::MouseCode code )
	{
		// call the panel
		OnMousePressed( code );
		PostMessage( m_pParent->GetParent(), new KeyValues("Command", "command", "play") );
	}
	
	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		m_TextColor = pScheme->GetColor( "NewGame.TextColor", Color(255, 255, 255, 255) );
		m_FillColor = pScheme->GetColor( "NewGame.FillColor", Color(255, 255, 255, 255) );
		m_DisabledColor = pScheme->GetColor( "NewGame.DisabledColor", Color(255, 255, 255, 255) );
		m_SelectedColor = pScheme->GetColor( "NewGame.SelectionColor", Color(255, 255, 255, 255) );

		BaseClass::ApplySchemeSettings( pScheme );
		
		m_pLevelPicBorder->SetFillColor( m_FillColor );
		m_pChapterNameLabel->SetFgColor( m_TextColor );
	}

protected:
	vgui::Label *m_pChapterNameLabel;
	vgui::PanelListPanel *m_pParent;
	
	vgui::ImagePanel *m_pLevelPicBorder;
	vgui::ImagePanel *m_pLevelPic;

	Color m_TextColor;
	Color m_DisabledColor;
	Color m_SelectedColor;
	Color m_FillColor;
};

class CMapSetItemPanelMapSet : public CMapSetItemPanel
{
public:
	DECLARE_CLASS_SIMPLE( CMapSetItemPanelMapSet, CMapSetItemPanel );

	CMapSetItemPanelMapSet( PanelListPanel *parent, const char* name, const char *imagename, const char *titlename, const char *mapsetname, const char *filename ) : BaseClass( parent, name, titlename, imagename, "Resource/MapSetDialogItemPanel.res" )
	{
		m_pMapSetsFile = new KeyValues( "mapsets" );
		m_pMapSetsFile->LoadFromFile( g_pFullFileSystem, filename );
		
		V_strcpy( m_szMapSetName, mapsetname );

		m_pParent = parent;
	}

	~CMapSetItemPanelMapSet()
	{
		if ( m_pMapSetsFile )
		{
			m_pMapSetsFile->deleteThis();
			m_pMapSetsFile = NULL;
		}
	}
	
	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			//m_pChapterLabel->SetFgColor( m_SelectedColor );
			m_pLevelPicBorder->SetFillColor( m_SelectedColor );
			m_pChapterNameLabel->SetFgColor( m_SelectedColor );

			CMapSetDialog *pMapSetDialog = dynamic_cast<CMapSetDialog*>( m_pParent->GetParent() );
			if ( pMapSetDialog )
			{
				pMapSetDialog->SetupMapList( this );
				vgui::Button *pStartButton = (vgui::Button *)pMapSetDialog->FindChildByName( "Play" );
				if ( pStartButton )
				{
					pStartButton->SetEnabled( false );
				}
			}
		}
		else
		{
			//m_pChapterLabel->SetFgColor( m_TextColor );
			m_pLevelPicBorder->SetFillColor( m_FillColor );
			m_pChapterNameLabel->SetFgColor( m_TextColor );
		}

		PostMessage( m_pParent->GetVParent(), new KeyValues("PanelSelected") );
	}
	
	virtual void OnMouseDoublePressed( vgui::MouseCode code )
	{
		// call the panel
		OnMousePressed( code );
		//PostMessage( m_pParent->GetParent(), new KeyValues("Command", "command", "play") );
	}

	int GetRequiredPlayers() { return GetMapSet()->GetInt("required_players"); }

	KeyValues *GetMapSet()
	{
		for ( KeyValues *mapset = m_pMapSetsFile->GetFirstSubKey(); mapset != NULL; mapset = mapset->GetNextKey() )
		{
			if ( !V_strcmp( m_szMapSetName, mapset->GetName() ) )
			{
				return mapset;
			}
		}
		return NULL;
	}

	KeyValues *m_pMapSetsFile;
	char m_szMapSetName[32];
};

class CMapSetItemPanelMap : public CMapSetItemPanel
{
public:
	DECLARE_CLASS_SIMPLE( CMapSetItemPanelMap, CMapSetItemPanel );
		
	CMapSetItemPanelMap( PanelListPanel *parent, const char *name, const char *titlename, const char *imagename, const char *map, int nRequiredPlayers ) : BaseClass( parent, name, titlename, imagename, "Resource/MapSetDialogItemPanelMap.res" )
	{
		V_strcpy( m_szMap, map );
		m_nRequiredPlayers = nRequiredPlayers;
	}
	
	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			//m_pChapterLabel->SetFgColor( m_SelectedColor );
			m_pLevelPicBorder->SetFillColor( m_SelectedColor );
			m_pChapterNameLabel->SetFgColor( m_SelectedColor );

			CMapSetDialog *pMapSetDialog = dynamic_cast<CMapSetDialog*>( m_pParent->GetParent() );
			if ( pMapSetDialog )
			{
				V_strcpy( pMapSetDialog->m_szMap, m_szMap );
				pMapSetDialog->m_nRequiredPlayers = m_nRequiredPlayers;
				vgui::Button *pStartButton = (vgui::Button *)pMapSetDialog->FindChildByName( "Play" );
				if ( pStartButton )
				{
					pStartButton->SetEnabled( true );
				}
			}
		}
		else
		{
			//m_pChapterLabel->SetFgColor( m_TextColor );
			m_pLevelPicBorder->SetFillColor( m_FillColor );
			m_pChapterNameLabel->SetFgColor( m_TextColor );
		}

		PostMessage( m_pParent->GetVParent(), new KeyValues("PanelSelected") );
	}

	char m_szMap[32];
	int m_nRequiredPlayers;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMapSetDialog::CMapSetDialog(Panel *parent, const char *panelName) : Frame(parent, panelName)
{
	SetDeleteSelfOnClose( true );
	SetMoveable( true );
	SetSizeable( false );
	m_pMapSetList = NULL;
	m_pMapList = NULL;
	
	ConVarRef sv_use_steam_networking( "sv_use_steam_networking" );

	char sHostname[64];
	V_sprintf_safe( sHostname, "%s's Server", steamapicontext->SteamFriends()->GetPersonaName() );

	m_pHostnameTextEntry = new vgui::TextEntry( this, "HostNameTextEntry" );
	m_pHostnameTextEntry->SetText( sHostname );

	m_pPasswordTextEntry = new vgui::TextEntry(this, "PasswordTextEntry");
	m_pPasswordTextEntry->SetText( "" );
	
	m_pSteamNetworkingCheck = new vgui::CheckButton(this, "SteamNetworkingCheck", "#Start_Server_SteamNetworking");
	m_pSteamNetworkingCheck->SetSelected( sv_use_steam_networking.GetBool() );

	LoadControlSettings( "Resource/MapSetDialog.res" );
}

CMapSetDialog::~CMapSetDialog()
{
	g_pMapSetDialog = NULL;
	m_pMapList->DeleteAllItems();
	m_pMapSetList->DeleteAllItems();
}

void ParseCustomMapSet( const char *pFilename )
{
	if ( !g_pMapSetDialog )
		return;
		
	char szFullDirectory[_MAX_PATH];
	Q_snprintf( szFullDirectory, sizeof( szFullDirectory ), "%s/mapsets.txt", pFilename );

	KeyValues *mapsets = new KeyValues( "mapsets" );
	mapsets->LoadFromFile( g_pFullFileSystem, szFullDirectory );
	g_pMapSetDialog->ParseMapSetKeyValues( mapsets, szFullDirectory );
	mapsets->deleteThis();
}

void CMapSetDialog::SetupMapSetList( void )
{
	KeyValues *mapsets = new KeyValues( "mapsets" );
	const char *official_filename = "scripts/mapsets/mapsets_official.txt";
	mapsets->LoadFromFile( g_pFullFileSystem, official_filename );
	// Parse the official mapsets first
	ParseMapSetKeyValues( mapsets, official_filename );
	mapsets->deleteThis();

	ExecuteLoadingMapSetFunction( ParseCustomMapSet );
}

void CMapSetDialog::SetupMapList( CMapSetItemPanelMapSet *pMapPanel )
{
	m_pMapList->DeleteAllItems();
	memset( m_szMap, 0, sizeof( m_szMap ) );

	// add chapters to combobox
	KeyValues *maps = NULL;
	for ( KeyValues *subkey = pMapPanel->GetMapSet()->GetFirstSubKey(); subkey != NULL; subkey = subkey->GetNextKey() )
	{
		if ( !V_strcmp( subkey->GetName(), "maps" ) )
		{
			maps = subkey;
			break;
		}
	}

	if ( !maps )
		return;
	
	for ( KeyValues *map = maps->GetFirstSubKey(); map != NULL; map = map->GetNextKey() )
	{
		char szImage[64];
		V_snprintf( szImage, sizeof( szImage ), "maps/menu_thumb_%s", map->GetName() );
		int nRequiredPlayers = pMapPanel->GetMapSet()->GetInt( "required_players" );
		CMapSetItemPanelMap *chapterPanel = SETUP_PANEL( new CMapSetItemPanelMap( m_pMapSetList, NULL, map->GetString(), szImage, map->GetName(), nRequiredPlayers ) );
		chapterPanel->SetVisible( true );
		chapterPanel->InvalidateLayout( true );

		m_pMapList->AddItem( NULL, chapterPanel );
	}
}

void CMapSetDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pMapSetList = dynamic_cast<vgui::PanelListPanel*>( FindChildByName( "MapSetList" ) );
	m_pMapList = dynamic_cast<vgui::PanelListPanel*>( FindChildByName( "MapList" ) );

	m_pMapSetList->SetFirstColumnWidth( 10 );
	m_pMapSetList->SetVerticalBufferPixels( 0 );
	
	m_pMapList->SetFirstColumnWidth( 25 );
	m_pMapList->SetVerticalBufferPixels( 0 );
	
	SetupMapSetList();
}

void CMapSetDialog::OnCommand( const char *command )
{
	if ( !V_stricmp( command, "play" ) )
	{
		if ( m_szMap[0] != 0 )
		{
			char szBuffer[64];
			ConVarRef hostname( "hostname" );
			m_pHostnameTextEntry->GetText( szBuffer, sizeof( szBuffer ) );
			hostname.SetValue( szBuffer );
		
			ConVarRef sv_password( "sv_password" );
			m_pPasswordTextEntry->GetText( szBuffer, sizeof( szBuffer ) );
			sv_password.SetValue( szBuffer );
		
			ConVarRef sv_use_steam_networking( "sv_use_steam_networking" );
			sv_use_steam_networking.SetValue( m_pSteamNetworkingCheck->IsSelected() );
		
			// Set commentary
			ConVarRef commentary( "commentary" );
			commentary.SetValue( false );

			ConVarRef sv_cheats( "sv_cheats" );
			sv_cheats.SetValue( false );
		
			// Also set certain convars that are necessary for a vanilla experience
			{
				ConVarRef pcoop_spectate_after_past_required_players( "pcoop_spectate_after_past_required_players" );
				pcoop_spectate_after_past_required_players.SetValue( true );
		
				ConVarRef pcoop_require_all_players( "pcoop_require_all_players" );
				pcoop_require_all_players.SetValue( true );
		
				ConVarRef pcoop_require_all_players_force_amount( "pcoop_require_all_players_force_amount" );
				pcoop_require_all_players_force_amount.SetValue( -2 );
				
				ConVarRef pcoop_ignore_installed_games_check( "pcoop_ignore_installed_games_check" );
				pcoop_ignore_installed_games_check.SetValue( 0 );
				
				ConVarRef sv_require_game_install_necessary_for_map( "sv_require_game_install_necessary_for_map" );
				sv_require_game_install_necessary_for_map.SetValue( 1 );				
			}

			bool bCanChangeLevel = g_pEngineClientReplay->IsListenServer();

			char szCommand[256];
			const char *pszLevelName = engine->GetLevelName();
			if ( ( !pszLevelName || pszLevelName[0] == '\0' ) || (gpGlobals->maxClients < m_nRequiredPlayers) || !bCanChangeLevel ) // Create a new server if the maxclients is smaller than the required players
			{
				ConVarRef sv_allow_wait_command( "sv_allow_wait_command" );
				bool bOldWait = sv_allow_wait_command.GetBool();
				sv_allow_wait_command.SetValue( true );

				engine->ExecuteClientCmd(""); // This will allow the wait command instantly

				V_snprintf( szCommand, sizeof( szCommand ), "disconnect\nwait\nwait\nmaxplayers %i\nmap %s\n", m_nRequiredPlayers, m_szMap );
				engine->ClientCmd_Unrestricted( szCommand );

				sv_allow_wait_command.SetValue( bOldWait );
			}
			else // Otherwise, if the maxclients is big enough to do a level change, then do that instead.
			{
				V_snprintf( szCommand, sizeof( szCommand ), "changelevel %s\n", m_szMap );
				engine->ClientCmd_Unrestricted( szCommand );
			}
		}
		
		return;
	}
	else if ( !stricmp( command, "OpenLegacyServerCreator" ) )
	{
		engine->ClientCmd_Unrestricted( "gamemenucommand opencreatemultiplayergamedialog" );
		OnCommand( "Close" );

		return;
	}
	BaseClass::OnCommand( command );
}

void CMapSetDialog::ParseMapSetKeyValues( KeyValues *mapsets, const char *filename )
{
	// add chapters to combobox
	for ( KeyValues *mapset = mapsets->GetFirstSubKey(); mapset != NULL; mapset = mapset->GetNextKey() )
	{
		char szImage[32];
		V_snprintf( szImage, sizeof( szImage ), "mapsets/%s", mapset->GetName() );
		CMapSetItemPanelMapSet *chapterPanel = SETUP_PANEL( new CMapSetItemPanelMapSet( m_pMapSetList, NULL, szImage, mapset->GetString( "name" ), mapset->GetName(), filename ) );
		chapterPanel->SetVisible( true );
		chapterPanel->InvalidateLayout( true );

		m_pMapSetList->AddItem( NULL, chapterPanel );
	}
}

static void CC_OpenMapSetDialog(const CCommand &args)
{
	vgui::Panel *parent = (vgui::Panel *)NULL;

	if (!g_pMapSetDialog)
	{
		g_pMapSetDialog = new CMapSetDialog(parent, "MapSetDialog");
		SETUP_PANEL( g_pMapSetDialog );
	}
	if (!g_pMapSetDialog)
		return;

	vgui::VPANEL GameUiDll = enginevgui->GetPanel(PANEL_GAMEUIDLL);
	//SetTitle("#GameUI_Options", true);
	g_pMapSetDialog->MoveToCenterOfScreen();
	g_pMapSetDialog->Activate();
	g_pMapSetDialog->SetParent(GameUiDll);
	
}

static ConCommand OpenMapSetDialog( "OpenMapSetDialog", CC_OpenMapSetDialog, "Open map set dialog" );