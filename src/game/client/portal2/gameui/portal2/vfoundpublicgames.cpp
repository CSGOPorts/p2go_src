//========= Copyright � 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//
#include <tier0/platform.h>
#ifdef IS_WINDOWS_PC
#include "windows.h"
#endif

#include "tier1/fmtstr.h"
#include "vgui/ILocalize.h"
#include "engineinterface.h"
#include "vhybridbutton.h"
#include "vdropdownmenu.h"
#include "vflyoutmenu.h"
#include "vgamesettings.h"

#include "vfoundpublicgames.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern ConVar ui_foundgames_spinner_time;

//=============================================================================
static ConVar ui_public_lobby_filter_difficulty2( "ui_public_lobby_filter_difficulty2", "", FCVAR_ARCHIVE, "Filter type for difficulty on the public lobby display" );
ConVar ui_public_lobby_filter_campaign( "ui_public_lobby_filter_campaign", "", FCVAR_ARCHIVE, "Filter type for campaigns on the public lobby display" );
ConVar ui_public_lobby_filter_status( "ui_public_lobby_filter_status", "", FCVAR_ARCHIVE, "Filter type for game status on the public lobby display" );

//=============================================================================
FoundPublicGames::FoundPublicGames( Panel *parent, const char *panelName ) :
	BaseClass( parent, panelName ),
	m_pSearchManager( NULL )
{
	m_drpDifficulty = NULL;
	m_drpGameStatus = NULL;
	m_drpCampaign = NULL;
	
	m_pSupportRequiredDetails = NULL;
	m_pInstallSupportBtn = NULL;

	m_numCurrentPlayers = 0;
}

FoundPublicGames::~FoundPublicGames()
{
	if ( m_pSearchManager )
		m_pSearchManager->Destroy();
	m_pSearchManager = NULL;
}

//=============================================================================
static void SetDropDownMenuVisible( DropDownMenu *btn, bool visible )
{
	if ( btn )
	{
		if ( FlyoutMenu *flyout = btn->GetCurrentFlyout() )
		{
			flyout->CloseMenu( NULL );
		}

		btn->SetVisible( visible );
	}
}

//=============================================================================
static void AdjustButtonY( Panel *btn, int yOffset )
{
	if ( btn )
	{
		int x, y;
		btn->GetPos( x, y );
		btn->SetPos( x, y + yOffset );
	}
}

//=============================================================================

void FoundPublicGames::UpdateFilters( bool newState )
{
}

//=============================================================================

void FoundPublicGames::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_drpDifficulty = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpFilterDifficulty" ) );
	m_drpGameStatus = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpFilterGameStatus" ) );
	m_drpCampaign = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpFilterCampaign" ) );
	m_btnFilters = dynamic_cast< BaseModUI::BaseModHybridButton* >( FindChildByName( "BtnFilters" ) );

	bool bHasDifficulty = GameModeHasDifficulty( m_pDataSettings->GetString( "game/mode", "" ) );
	if ( m_drpDifficulty )
	{
		SetDropDownMenuVisible( m_drpDifficulty, bHasDifficulty );
	}

	m_pSupportRequiredDetails = dynamic_cast< vgui::Label * >( FindChildByName( "LblSupportRequiredDetails" ) );
	m_pInstallSupportBtn = dynamic_cast< BaseModUI::BaseModHybridButton * >( FindChildByName( "BtnInstallSupport" ) );

	Activate();
}

//=============================================================================
void FoundPublicGames::PaintBackground()
{
	const char *gameMode = m_pDataSettings->GetString( "game/mode", "coop" );

	wchar_t finalString[256] = L"";
	if ( m_numCurrentPlayers )
	{
		const wchar_t *subtitleText = g_pVGuiLocalize->Find( CFmtStr( "#L4D360UI_FoundPublicGames_Subtitle_%s", gameMode ) );
		if ( subtitleText )
		{
			wchar_t convertedString[13];
			V_snwprintf( convertedString, ARRAYSIZE( convertedString ), L"%d", m_numCurrentPlayers );
			g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), subtitleText, 1, convertedString );
		}
	}

	BaseClass::DrawDialogBackground( CFmtStr( "#L4D360UI_FoundPublicGames_Title_%s", gameMode ), NULL, NULL, finalString, NULL );
}

//=============================================================================
void FoundPublicGames::StartSearching( void )
{
	KeyValues *pKeyValuesSearch = new KeyValues( "Search" );

	char const *szGameMode = m_pDataSettings->GetString( "game/mode", "" );
	if ( szGameMode && *szGameMode )
		pKeyValuesSearch->SetString( "game/mode", szGameMode );

	char const *szCampaign = ui_public_lobby_filter_campaign.GetString();
	if ( szCampaign && *szCampaign )
		pKeyValuesSearch->SetString( "game/missioninfo/builtin", szCampaign );
	
	char const *szDifficulty = ui_public_lobby_filter_difficulty2.GetString();
	if ( szDifficulty && *szDifficulty && GameModeHasDifficulty( szGameMode ) )
		pKeyValuesSearch->SetString( "game/difficulty", szDifficulty );

	char const *szStatus = ui_public_lobby_filter_status.GetString();
	if ( szStatus && *szStatus )
		pKeyValuesSearch->SetString( "game/state", szStatus );

	if ( !m_pSearchManager )
	{
		m_pSearchManager = g_pMatchFramework->GetMatchSystem()->CreateGameSearchManager( pKeyValuesSearch );
	}
	else
	{
		m_pSearchManager->EnableResultsUpdate( true, pKeyValuesSearch );
	}
}

//=============================================================================
bool FoundPublicGames::ShouldShowPublicGame( KeyValues *pGameDetails )
{
	char const *szCampaign = pGameDetails->GetString( "game/campaign", NULL );
	char const *szWebsite = pGameDetails->GetString( "game/missioninfo/website", "" );

	KeyValues *pAllMissions = g_pMatchExt->GetAllMissions();
	KeyValues *pInstalledMission = ( szCampaign && *szCampaign && pAllMissions ) ? pAllMissions->FindKey( szCampaign ) : NULL;

	// if no mission and no website, skip it
	if ( !pInstalledMission && !*szWebsite )
		return false;

	if ( !pInstalledMission &&
		 ( !Q_stricmp( ui_public_lobby_filter_campaign.GetString(), "installedaddon" ) ||
		   !Q_stricmp( ui_public_lobby_filter_campaign.GetString(), "official" ) ) )
		return false;

	return true;
}

static void HandleJoinPublicGame( FoundGameListItem::Info const &fi )
{
	if ( fi.mInfoType != FoundGameListItem::FGT_PUBLICGAME )
		return;

	FoundPublicGames *pWnd = ( FoundPublicGames * ) CBaseModPanel::GetSingleton().GetWindow( WT_FOUNDPUBLICGAMES );
	if ( !pWnd )
		return;

	if ( !pWnd->m_pSearchManager )
		return;

	IMatchSearchResult *pResult = pWnd->m_pSearchManager->GetResultByOnlineId( fi.mFriendXUID );
	if ( !pResult )
		return;

	pResult->Join();
}

//=============================================================================
void FoundPublicGames::AddServersToList( void )
{
	if ( !m_pSearchManager )
		return;

	int numItems = m_pSearchManager->GetNumResults();
	for ( int i = 0; i < numItems; ++ i )
	{
		IMatchSearchResult *item = m_pSearchManager->GetResultByIndex( i );
		KeyValues *pGameDetails = item->GetGameDetails();
		if ( !pGameDetails )
			continue;
		if ( !ShouldShowPublicGame( pGameDetails ) )
			continue;

		FoundGameListItem::Info fi;

		fi.mInfoType = FoundGameListItem::FGT_PUBLICGAME;
		char const *szGameMode = pGameDetails->GetString( "game/mode", "coop" );
		bool bGameModeChapters = GameModeIsSingleChapter( szGameMode );
		Q_strncpy( fi.Name, pGameDetails->GetString( "game/missioninfo/displaytitle", "#L4D360UI_CampaignName_Unknown" ), sizeof( fi.Name ) );

		fi.mIsJoinable = item->IsJoinable();
		fi.mbInGame = true;

		fi.miPing = pGameDetails->GetInt( "server/ping", 0 );
		fi.mPing = fi.GP_HIGH;
		if ( !Q_stricmp( "lan", pGameDetails->GetString( "system/network", "" ) ) )
			fi.mPing = fi.GP_SYSTEMLINK;

		fi.mpGameDetails = pGameDetails->MakeCopy();
		KeyValues::AutoDelete autodelete_fi_mpGameDetails( fi.mpGameDetails );

		// Force the chapter to 1 in case there is no chapter specified
		int iChapter = fi.mpGameDetails->GetInt( "game/chapter", 0 );
		if ( !iChapter )
		{
			fi.mpGameDetails->SetInt( "game/chapter", 1 );
			fi.mpGameDetails->SetBool( "UI/nochapter", 1 );
		}

		if ( IsGameConsole() )
		{
			// For X360 titles do not transmit campaign display title, so client needs to resolve locally
			KeyValues *pMissionInfo = NULL;
			KeyValues *pChapterInfo = GetMapInfoRespectingAnyChapter( fi.mpGameDetails, &pMissionInfo );
			if ( pMissionInfo && pChapterInfo )
			{
				if ( bGameModeChapters )
					Q_strncpy( fi.Name, pChapterInfo->GetString( "displayname", "#L4D360UI_LevelName_Unknown" ), sizeof( fi.Name ) );
				else
					Q_strncpy( fi.Name, pMissionInfo->GetString( "displaytitle", "#L4D360UI_CampaignName_Unknown" ), sizeof( fi.Name ) );
			}
			else
			{
				Q_strncpy( fi.Name, "#L4D360UI_CampaignName_Unknown", sizeof( fi.Name ) );
				fi.mbDLC = true;
			}
		}

		fi.mFriendXUID = item->GetOnlineId();

		// Check if this is actually a non-joinable game
		if ( fi.IsDLC() )
		{
			fi.mIsJoinable = false;
		}
		else if ( fi.IsDownloadable() )
		{
			fi.mIsJoinable = false;
		}
		else if ( fi.mbInGame )
		{
			char const *szHint = fi.GetNonJoinableShortHint();
			if ( !*szHint )
			{
				fi.mIsJoinable = true;
				fi.mpfnJoinGame = HandleJoinPublicGame;
			}
			else
			{
				fi.mIsJoinable = false;
			}
		}

		AddGameFromDetails( fi );
	}
}

bool FoundPublicGames::IsADuplicateServer( FoundGameListItem *item, FoundGameListItem::Info const &fi )
{
	// Only check UID
	FoundGameListItem::Info const &ii = item->GetFullInfo();
	if ( ii.mFriendXUID == fi.mFriendXUID )
		return true;
	else
		return false;
}

//=============================================================================
static int __cdecl FoundPublicGamesSortFunc( vgui::Panel* const *a, vgui::Panel* const *b)
{
	FoundGameListItem *fA	= dynamic_cast< FoundGameListItem* >(*a);
	FoundGameListItem *fB	= dynamic_cast< FoundGameListItem* >(*b);

	const BaseModUI::FoundGameListItem::Info &ia = fA->GetFullInfo();
	const BaseModUI::FoundGameListItem::Info &ib = fB->GetFullInfo();

	// built in first
	bool builtInA = ia.mpGameDetails->GetInt( "game/missioninfo/builtin", 0 ) != 0;
	bool builtInB = ib.mpGameDetails->GetInt( "game/missioninfo/builtin", 0 ) != 0;
	if ( builtInA != builtInB )
	{
		return ( builtInA ? -1 : 1 );
	}

	// now by campaign
	const char *campaignNameA = ia.mpGameDetails->GetString( "game/missioninfo/displaytitle", "" );
	const char *campaignNameB = ib.mpGameDetails->GetString( "game/missioninfo/displaytitle", "" );
	if ( int iResult = Q_stricmp( campaignNameA, campaignNameB ) )
	{
		return iResult;
	}

	// difficulty
	const char *diffA = ia.mpGameDetails->GetString( "game/difficulty", "" );
	const char *diffB = ib.mpGameDetails->GetString( "game/difficulty", "" );
	if ( int iResult = Q_stricmp( diffA, diffB ) )
	{
		return iResult;
	}

	// Sort by name in the end
	return Q_stricmp( ia.Name, ib.Name );
}

//=============================================================================
void FoundPublicGames::SortListItems()
{
	m_GplGames->SortPanelItems( FoundPublicGamesSortFunc );
}

//=============================================================================
void FoundPublicGames::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "StartCustomMatchSearch" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfNotSignedInToLive( this ) )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_DENY );
			return;
		}

		KeyValues *pSettings = NULL;
		bool bDefaultSettings = true;
		if ( FoundGameListItem* gameListItem = static_cast<FoundGameListItem*>( m_GplGames->GetSelectedPanelItem() ) )
		{
			pSettings = gameListItem->GetFullInfo().mpGameDetails;
		}

		if ( !pSettings )
		{
			pSettings = new KeyValues( "settings" );
		}
		else
		{
			pSettings = pSettings->MakeCopy();
			bDefaultSettings = false;
		}

		// Take ownership of allocated/copied keyvalues
		KeyValues::AutoDelete autodelete( pSettings );
		char const *szGameMode = m_pDataSettings->GetString( "game/mode", "coop" );

		pSettings->SetString( "system/network", "LIVE" );
		pSettings->SetString( "system/access", "public" );
		pSettings->SetString( "game/mode", szGameMode );
		pSettings->SetString( "options/action", "custommatch" );

		// TCR: We need to respect the default difficulty
		if ( bDefaultSettings && GameModeHasDifficulty( szGameMode ) )
			pSettings->SetString( "game/difficulty", GameModeGetDefaultDifficulty( szGameMode ) );

		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		CBaseModPanel::GetSingleton().CloseAllWindows();
		CBaseModPanel::GetSingleton().OpenWindow( WT_GAMESETTINGS, NULL, true, pSettings );
	}
	else if ( char const *filterDifficulty = StringAfterPrefix( command, "filter_difficulty_" ) )
	{
		ui_public_lobby_filter_difficulty2.SetValue( filterDifficulty );
		StartSearching();
	}
	else if ( char const *filterCampaign = StringAfterPrefix( command, "filter_campaign_" ) )
	{
		ui_public_lobby_filter_campaign.SetValue( filterCampaign );
		StartSearching();
	}
	else if ( char const *filterGamestatus = StringAfterPrefix( command, "filter_status_" ) )
	{
		ui_public_lobby_filter_status.SetValue( filterGamestatus );
		StartSearching();
	}
	else if ( !Q_stricmp( command, "InstallSupport" ) )
	{
		// install the add-on support package
#ifdef IS_WINDOWS_PC
		// App ID for the legacy addon data is 564
		ShellExecute ( 0, "open", "steam://install/564", NULL, 0, SW_SHOW );
#endif		
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//=============================================================================
void FoundPublicGames::Activate()
{
	BaseClass::Activate();

	if ( BaseModHybridButton *pWndCreateGame = dynamic_cast< BaseModHybridButton * >( FindChildByName( "DrpCreateGame" ) ) )
	{
		pWndCreateGame->SetText( CFmtStr( "#L4D360UI_FoudGames_CustomMatch_%s", m_pDataSettings->GetString( "game/mode", "" ) ) );
	}

	if ( m_drpDifficulty )
	{
		m_drpDifficulty->SetCurrentSelection( CFmtStr( "filter_difficulty_%s", ui_public_lobby_filter_difficulty2.GetString() ) );
	}

	if ( m_drpGameStatus )
	{
		m_drpGameStatus->SetCurrentSelection( CFmtStr( "filter_status_%s", ui_public_lobby_filter_status.GetString() ) );
	}

	if ( m_drpCampaign )
	{
		m_drpCampaign->SetCurrentSelection( CFmtStr( "filter_campaign_%s", ui_public_lobby_filter_campaign.GetString() ) );
	}

#if !defined( NO_STEAM )
	if ( steamapicontext )
	{
		if ( ISteamUserStats *pSteamUserStats = steamapicontext->SteamUserStats() )
		{
			SteamAPICall_t hSteamAPICall = pSteamUserStats->GetNumberOfCurrentPlayers();
			m_callbackNumberOfCurrentPlayers.Set( hSteamAPICall, this, &FoundPublicGames::Steam_OnNumberOfCurrentPlayers );
		}
	}
#endif
}

void FoundPublicGames::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_X:
		OnCommand( "StartCustomMatchSearch" );
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void FoundPublicGames::OnEvent( KeyValues *pEvent )
{
	char const *szName = pEvent->GetName();

	if ( !Q_stricmp( "OnMatchSearchMgrUpdate", szName ) )
	{
		if ( !m_pSearchManager || pEvent->GetPtr( "mgr" ) != m_pSearchManager )
			return;

		char const *szUpdate = pEvent->GetString( "update", "" );
		if ( !Q_stricmp( "searchstarted", szUpdate ) )
		{
			m_flSearchStartedTime = Plat_FloatTime();
			m_flSearchEndTime = m_flSearchStartedTime + ui_foundgames_spinner_time.GetFloat();
			OnThink();
		}
		else if ( !Q_stricmp( "searchfinished", szUpdate ) )
		{
			m_flSearchStartedTime = 0.0f;
			UpdateGameDetails();
		}
		else if ( !Q_stricmp( "result", szUpdate ) )
		{
			// Search result details have been updated
			UpdateGameDetails();
		}
	}
}

//=============================================================================
char const * FoundPublicGames::GetListHeaderText()
{
	bool bHasChapters = GameModeIsSingleChapter( m_pDataSettings->GetString( "game/mode", "coop" ) );

	if ( bHasChapters )
		return "#L4D360UI_FoundPublicGames_Header_Survival";
	else
		return "#L4D360UI_FoundPublicGames_Header";
}

//=============================================================================
#if !defined( NO_STEAM )
void FoundPublicGames::Steam_OnNumberOfCurrentPlayers( NumberOfCurrentPlayers_t *pResult, bool bError )
{
	if ( bError || !pResult->m_bSuccess )
	{
		m_numCurrentPlayers = 0;
	}
	else
	{
		m_numCurrentPlayers = pResult->m_cPlayers;
	}
}
#endif
