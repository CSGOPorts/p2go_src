//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetypes.h"
#include "hud.h"
#include <string.h>
#include <stdio.h>
#include "voice_status.h"
#include "r_efx.h"
#include <vgui_controls/TextImage.h>
#include <vgui/MouseCode.h>
#include "cdll_client_int.h"
#include "hud_macros.h"
#include "c_playerresource.h"
#include "cliententitylist.h"
#include "c_baseplayer.h"
#include "materialsystem/imesh.h"
#include "view.h"
#include "convar.h"
#include <vgui_controls/Controls.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include "vgui_bitmapimage.h"
#include "materialsystem/imaterial.h"
#include "tier0/dbg.h"
#include "cdll_int.h"
#include <vgui/IPanel.h>
#include "con_nprint.h"

#if defined(PORTAL2)
#include "c_portal_gamestats.h"
#endif

#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
#include "c_cs_player.h"
#include "matchmaking/imatchtitle.h"
#include "matchmaking/iplayer.h"
#include "matchmaking/mm_helpers.h"
#include "matchmaking/imatchframework.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


extern int cam_thirdperson;


#define VOICE_MODEL_INTERVAL		0.3
#define SQUELCHOSCILLATE_PER_SECOND	2.0f

ConVar voice_modenable( "voice_modenable", "1", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Enable/disable voice in this mod." );
ConVar voice_clientdebug( "voice_clientdebug", "0" );
ConVar voice_head_icon_size( "voice_head_icon_size", "6", FCVAR_NONE, "Size of voice icon over player heads in inches" );
ConVar voice_head_icon_height( "voice_head_icon_height", "20", FCVAR_NONE, "Voice icons are this many inches over player eye positions" );
ConVar voice_local_icon( "voice_local_icon", "0", FCVAR_NONE, "Draw local player's voice icon" );
ConVar voice_all_icons( "voice_all_icons", "0", FCVAR_NONE, "Draw all players' voice icons" );
ConVar voice_icons_method( "voice_icons_method", "2", FCVAR_NONE, "0 = classic style, 1 = particles, 2 = integrated into target ID" );

#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
ConVar cl_mute_enemy_team( "cl_mute_enemy_team", "0", FCVAR_ARCHIVE, "Block all communication from players on the enemy team." );
ConVar cl_mute_all_but_friends_and_party( "cl_mute_all_but_friends_and_party", "0", FCVAR_ARCHIVE, "Only allow communication from friends and matchmaking party members. Doesn't apply to competitive matchmaking games." );
#endif

// ---------------------------------------------------------------------- //
// The voice manager for the client.
// ---------------------------------------------------------------------- //
static CVoiceStatus *g_VoiceStatus = NULL;

CVoiceStatus* GetClientVoiceMgr()
{
	if ( !g_VoiceStatus )
	{
		ClientVoiceMgr_Init();
	}

	return g_VoiceStatus;
}

void ClientVoiceMgr_Init()
{
	if ( g_VoiceStatus )
		return;

	g_VoiceStatus = new CVoiceStatus();
}

void ClientVoiceMgr_Shutdown()
{
	delete g_VoiceStatus;
	g_VoiceStatus = NULL;
}

void ClientVoiceMgr_LevelInit()
{
	if ( g_VoiceStatus )
	{
		g_VoiceStatus->LevelInit();
	}
}

void ClientVoiceMgr_LevelShutdown()
{
	if ( g_VoiceStatus )
	{
		g_VoiceStatus->LevelShutdown();
	}
}

// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

static CVoiceStatus *g_pInternalVoiceStatus = NULL;

bool __MsgFunc_VoiceMask(const CUsrMsg_VoiceMask &msg)
{
	if(g_pInternalVoiceStatus)
		return g_pInternalVoiceStatus->HandleVoiceMaskMsg(msg);

	return true;
}

bool __MsgFunc_RequestState(const CUsrMsg_RequestState &msg)
{
	if(g_pInternalVoiceStatus)
		return g_pInternalVoiceStatus->HandleReqStateMsg(msg);

	return true;
}


// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

CVoiceStatus::CVoiceStatus()
{
	m_nControlSize = 0;
	m_bBanMgrInitialized = false;
	m_LastUpdateServerState = 0;

	for ( int k = 0; k < MAX_SPLITSCREEN_CLIENTS; ++ k )
	{
		m_bTalking[k] = false;
		m_bServerAcked[k] = false;
		m_bAboveThreshold[k] = false;

		m_bAboveThresholdTimer[k].Invalidate();
	}

	m_bServerModEnable = -1;

	m_pHeadLabelMaterial = NULL;

	m_bHeadLabelsDisabled = false;
}


CVoiceStatus::~CVoiceStatus()
{
	if ( m_pHeadLabelMaterial )
	{
		m_pHeadLabelMaterial->DecrementReferenceCount();
	}

	g_pInternalVoiceStatus = NULL;			

	const char *pGameDir = engine->GetGameDirectory();
	if( pGameDir )
	{
		if(m_bBanMgrInitialized)
		{
			m_BanMgr.SaveState( pGameDir );
		}
	}
}

int CVoiceStatus::Init(
	IVoiceStatusHelper *pHelper,
	VPANEL pParentPanel)
{
	const char *pGameDir = engine->GetGameDirectory();
	if( pGameDir )
	{
		m_BanMgr.Init( pGameDir );
		m_bBanMgrInitialized = true;
	}

	Assert(!g_pInternalVoiceStatus);
	g_pInternalVoiceStatus = this;


	m_pHeadLabelMaterial = materials->FindMaterial( "voice/icntlk_pl", TEXTURE_GROUP_VGUI );
	m_pHeadLabelMaterial->IncrementReferenceCount();

	m_bInSquelchMode = false;

	m_pHelper = pHelper;
	m_pParentPanel = pParentPanel;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		HOOK_MESSAGE(VoiceMask);
		HOOK_MESSAGE(RequestState);
	}

	return 1;
}


BitmapImage* vgui_LoadMaterial( vgui::VPANEL pParent, const char *pFilename )
{
	return new BitmapImage( pParent, pFilename );
}


void CVoiceStatus::VidInit()
{
}


void CVoiceStatus::LevelInit( void )
{
	for ( int k = 0; k < MAX_SPLITSCREEN_CLIENTS; ++ k )
	{
		m_bTalking[k] = false;
		m_bAboveThreshold[k] = false;
		m_bAboveThresholdTimer[k].Invalidate();
	}

	for ( int k = 0; k < VOICE_MAX_PLAYERS; ++k )
	{
		m_flTalkTime[k] = 0.0f;
		m_flTimeLastUpdate[k] = 0.0f;
	}
}


void CVoiceStatus::LevelShutdown( void )
{
	for ( int k = 0; k < MAX_SPLITSCREEN_CLIENTS; ++ k )
	{
		m_bTalking[k] = false;
		m_bAboveThreshold[k] = false;
		m_bAboveThresholdTimer[k].Invalidate();
	}

	for ( int k = 0; k < VOICE_MAX_PLAYERS; ++k )
	{
		m_flTalkTime[k] = 0.0f;
		m_flTimeLastUpdate[k] = 0.0f;
	}
}


void CVoiceStatus::Frame(double frametime)
{
	// check server banned players once per second
	if (gpGlobals->curtime - m_LastUpdateServerState > 1)
	{
		UpdateServerState(false);
	}
}

float CVoiceStatus::GetHeadLabelOffset( void ) const
{
	return voice_head_icon_height.GetFloat();
}


void CVoiceStatus::DrawHeadLabels()
{
	if ( voice_all_icons.GetBool() )
	{
		for(int i=0; i < VOICE_MAX_PLAYERS; i++)
		{
			IClientNetworkable *pClient = cl_entitylist->GetClientEntity( i+1 );

			// Don't show an icon if the player is not in our PVS.
			if ( !pClient || pClient->IsDormant() )
				continue;

			m_VoicePlayers[i] = voice_all_icons.GetInt() > 0;
		}
	}
	else if ( voice_local_icon.GetBool() )
	{
		C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
		m_VoicePlayers[ localPlayer->entindex() - 1 ] = IsLocalPlayerSpeakingAboveThreshold( localPlayer->GetSplitScreenPlayerSlot() );
	}

	if ( m_bHeadLabelsDisabled )
		return;

	if( !m_pHeadLabelMaterial )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	for ( int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		IClientNetworkable *pClient = cl_entitylist->GetClientEntity( i+1 );

		C_BasePlayer *pPlayer = dynamic_cast< C_BasePlayer* >( pClient );
		if( !pPlayer )
			continue;

		if ( !m_VoicePlayers[i] )
		{
			if ( voice_icons_method.GetInt() > 0 )
			{
				pPlayer->UpdateSpeechVOIP( false );
			}

			continue;
		}

		// Don't show an icon if the player is not in our PVS.
		if ( !pClient || pClient->IsDormant() )
		{
			if ( voice_icons_method.GetInt() > 0 )
			{
				pPlayer->UpdateSpeechVOIP( false );
			}

			continue;
		}
		
		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if( pPlayer->IsPlayerDead() )
		{
			if ( voice_icons_method.GetInt() > 0 )
			{
				pPlayer->UpdateSpeechVOIP( false );
			}

			continue;
		}

		if ( voice_icons_method.GetInt() > 0 )
		{
			pPlayer->UpdateSpeechVOIP( true );
			return;
		}

		// Don't show an icon for players we can't hear
		if ( !IsPlayerAudible( i+1 ) )
			continue;

		// Place it a fixed height above his head.
		Vector vOrigin = pPlayer->EyePosition( );
		vOrigin.z += GetHeadLabelOffset();
		
		// Align it so it never points up or down.
		Vector vUp( 0, 0, 1 );
		Vector vRight = CurrentViewRight();
		if ( fabs( vRight.z ) > 0.95 )	// don't draw it edge-on
			continue;

		vRight.z = 0;
		VectorNormalize( vRight );


		float flSize = voice_head_icon_size.GetFloat();

		pRenderContext->Bind( pPlayer->GetHeadLabelMaterial() );
		IMesh *pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.End();
		pMesh->Draw();
	}
}


void CVoiceStatus::UpdateSpeakerStatus(int entindex, int iSsSlot, bool bTalking)
{
	if( !m_pParentPanel )
		return;

	if( voice_clientdebug.GetInt() == 1 )
	{
		Msg( "CVoiceStatus::UpdateSpeakerStatus: ent %d ss[%d] talking = %d\n",
			entindex, iSsSlot, bTalking );
	}
	else if ( voice_clientdebug.GetInt() == 2 )
	{
		con_nprint_t np;
		np.index = 0;
		np.color[0] = 1.0f;
		np.color[1] = 1.0f;
		np.color[2] = 1.0f;
		np.time_to_live = 2.0f;
		np.fixed_width_font = true;

		int numActiveChannels = VOICE_MAX_PLAYERS;
		engine->Con_NXPrintf ( &np, "Total Players: %i", numActiveChannels);

		for ( int i = 1; i <= numActiveChannels; i++ )
		{
			np.index++;
			np.color[0] = np.color[1] = np.color[2] = ( i % 2 == 0 ? 0.9f : 0.7f );

			if ( !IsPlayerBlocked( i ) && IsPlayerAudible( i ) && !ShouldHideCommunicationFromPlayer( i ) && IsPlayerSpeaking( i ) )
			{
				np.color[0] = 0.0f;
				np.color[1] = 1.0f;
				np.color[2] = 0.0f;
			}

			engine->Con_NXPrintf ( &np, "%02i enabled(%s) blocked(%s) audible(%s) speaking(%s)", 
				i, 
				m_VoiceEnabledPlayers[ i - 1 ] != 0 ? "YES" : " NO",
				IsPlayerBlocked( i ) ? "YES" : " NO", 
				IsPlayerAudible( i ) ? "YES" : " NO", 
				IsPlayerSpeaking( i ) ? "YES" : " NO" );
		}

		np.color[0] = 1.0f;
		np.color[1] = 1.0f;
		np.color[2] = 1.0f;

		np.index += 2;
		numActiveChannels = MAX_SPLITSCREEN_CLIENTS;
		engine->Con_NXPrintf ( &np, "Local Players: %i", numActiveChannels);

		for ( int i = 0; i < numActiveChannels; i++ )
		{
			np.index++;
			np.color[0] = np.color[1] = np.color[2] = ( i % 2 == 0 ? 0.9f : 0.7f );

			if ( IsLocalPlayerSpeaking( i ) && IsLocalPlayerSpeakingAboveThreshold( i ) )
			{
				np.color[0] = 0.0f;
				np.color[1] = 1.0f;
				np.color[2] = 0.0f;
			}

			engine->Con_NXPrintf ( &np, "%02i speaking(%s) above_threshold(%s)", 
				i, 
				IsLocalPlayerSpeaking( i ) ? "YES" : " NO", 
				IsLocalPlayerSpeakingAboveThreshold( i ) ? "YES" : " NO" );
		}
	}

	// Is it the local player talking?
	if( entindex == -1 && iSsSlot >= 0 )
	{
		m_bTalking[ iSsSlot ] = !!bTalking;
#if !( defined( CSTRIKE15 ) && defined( CSTRIKE_DLL ) )
		if( bTalking )
		{
			// Enable voice for them automatically if they try to talk.
			char chClientCmd[0xFF];
			Q_snprintf( chClientCmd, sizeof( chClientCmd ),
				"cmd%d voice_modenable 1", iSsSlot + 1 );
			engine->ClientCmd( chClientCmd );
		}
#endif
	}
	
	if( entindex == -2 && iSsSlot >= 0 )
	{
		m_bServerAcked[ iSsSlot ] = !!bTalking;
	}
	
	if ( entindex == -3 && iSsSlot >= 0 )
	{
		m_bAboveThreshold[ iSsSlot ] = !!bTalking;

		if ( bTalking )
		{
			const float AboveThresholdMinDuration = 0.5f;
			m_bAboveThresholdTimer[ iSsSlot ].Start( AboveThresholdMinDuration );
		}
	}
	
	if( entindex > 0 && entindex <= VOICE_MAX_PLAYERS )
	{
		int iClient = entindex - 1;
		if(iClient < 0)
			return;

		if(bTalking)
		{
			m_VoicePlayers[iClient] = true;
			m_VoiceEnabledPlayers[iClient] = true;
		}
		else
		{
			m_VoicePlayers[iClient] = false;
		}

		if ( bTalking && m_bAboveThresholdTimer[ iClient ].IsElapsed() )
		{
			m_flTalkTime[ iClient ] +=  (gpGlobals->curtime - m_flTimeLastUpdate[ iClient ]);
		}
		else 
		{
#if defined( PORTAL2_REAL ) && !defined( _GAMECONSOLE )
			if ( m_flTalkTime[ iClient ] > 0.0f )
			{
				g_PortalGameStats.Event_MicUsage( entindex, m_flTalkTime[ iClient ], gpGlobals->curtime - m_flTalkTime[ iClient ] );
			}
#endif //!defined( _GAMECONSOLE )
			m_flTalkTime[ iClient ] = 0.0f;
		}
		m_flTimeLastUpdate[ iClient ] = gpGlobals->curtime;
	}
}


void CVoiceStatus::UpdateServerState(bool bForce)
{
	// Can't do anything when we're not in a level.
	if( !g_bLevelInitialized )
	{
		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: g_bLevelInitialized\n" );
		}

		return;
	}
	
	int bCVarModEnable = !!voice_modenable.GetInt();
	if(bForce || m_bServerModEnable != bCVarModEnable)
	{
		m_bServerModEnable = bCVarModEnable;

		char str[256];
		Q_snprintf(str, sizeof(str), "VModEnable %d", m_bServerModEnable);
		
		{
			HACK_GETLOCALPLAYER_GUARD( "CVoiceStatus::UpdateServerState" );
			engine->ServerCmd(str);
		}

		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: Sending '%s'\n", str );
		}
	}

	char str[2048];
	Q_strncpy(str,"vban",sizeof(str));
	bool bChange = false;

	for(unsigned long dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{	
		unsigned long serverBanMask = 0;
		unsigned long banMask = 0;
		for(unsigned long i=0; i < 32; i++)
		{
			int playerIndex = ( dw * 32 + i );
			if ( playerIndex >= MAX_PLAYERS )
				break;

			player_info_t pi;

			if ( !engine->GetPlayerInfo( i+1, &pi ) )
				continue;

			if ( m_BanMgr.GetPlayerBan( pi.guid ) || ShouldHideCommunicationFromPlayer( i + 1 ) )
			{
				banMask |= 1 << i;
			}

			if ( m_ServerBannedPlayers[playerIndex] )
			{
				serverBanMask |= 1 << i;
			}
		}

		if ( serverBanMask != banMask )
		{
			bChange = true;
		}

		// Ok, the server needs to be updated.
		char numStr[512];
		Q_snprintf(numStr,sizeof(numStr), " %lx", banMask);
		Q_strncat(str, numStr, sizeof(str), COPY_ALL_CHARACTERS);
	}

	if(bChange || bForce)
	{
		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: Sending '%s'\n", str );
		}

		engine->ServerCmd( str, false );	// Tell the server..
	}
	else
	{
		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: no change\n" );
		}
	}
	
	m_LastUpdateServerState = gpGlobals->curtime;
}

bool CVoiceStatus::HandleVoiceMaskMsg(const CUsrMsg_VoiceMask &msg)
{
	unsigned long dw;
	for(dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{
		m_AudiblePlayers.SetDWord(dw, (unsigned long)msg.player_masks(dw).game_rules_mask());
		m_ServerBannedPlayers.SetDWord(dw, (unsigned long)msg.player_masks(dw).ban_masks());

		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg("CVoiceStatus::HandleVoiceMaskMsg\n");
			Msg("    - m_AudiblePlayers[%lu] = %u\n", dw, m_AudiblePlayers.GetDWord(dw));
			Msg("    - m_ServerBannedPlayers[%lu] = %u\n", dw, m_ServerBannedPlayers.GetDWord(dw));
		}
	}

	m_bServerModEnable = msg.player_mod_enable();

	return true;
}

bool CVoiceStatus::HandleReqStateMsg(const CUsrMsg_RequestState &msg)
{
	if( voice_clientdebug.GetInt() == 1 )
	{
		Msg("CVoiceStatus::HandleReqStateMsg\n");
	}

	UpdateServerState(true);

	return true;
}

void CVoiceStatus::StartSquelchMode()
{
	if(m_bInSquelchMode)
		return;

	m_bInSquelchMode = true;
	m_pHelper->UpdateCursorState();
}

void CVoiceStatus::StopSquelchMode()
{
	m_bInSquelchMode = false;
	m_pHelper->UpdateCursorState();
}

bool CVoiceStatus::IsInSquelchMode()
{
	return m_bInSquelchMode;
}

void SetOrUpdateBounds( 
	vgui::Panel *pPanel, 
	int left, int top, int wide, int tall, 
	bool bOnlyUpdateBounds, int &topCoord, int &bottomCoord )
{
	if ( bOnlyUpdateBounds )
	{
		if ( top < topCoord )
			topCoord = top;

		if ( (top+tall) >= bottomCoord )
			bottomCoord = top+tall;
	}
	else
	{
		pPanel->SetBounds( left, top, wide, tall );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the target client has been banned
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerBlocked(int iPlayer)
{
	player_info_t pi;

	if ( !engine->GetPlayerInfo( iPlayer, &pi ) )
		return false;

	return m_BanMgr.GetPlayerBan( pi.guid );
}

bool CVoiceStatus::ShouldHideCommunicationFromPlayer( int iPlayerIndex )
{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	C_CSPlayer* pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer && pLocalPlayer->entindex() == iPlayerIndex )
		return false;

	if ( cl_mute_enemy_team.GetBool() )
	{
		if ( pLocalPlayer && pLocalPlayer->IsOtherEnemy( iPlayerIndex ) && 
			( pLocalPlayer->GetTeamNumber() == TEAM_CT || pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST ) )
			return true;
	}
	if ( cl_mute_all_but_friends_and_party.GetBool() && CSGameRules() && !CSGameRules()->IsQueuedMatchmaking() )
	{
		// This type of muting doesn't make sense when playing back a demo
		if ( engine->IsPlayingDemo() )
			return false;
		C_CSPlayer * pOther = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		CSteamID otherID;
		bool bIsInParty = false;
		if ( pOther && pOther->GetSteamID( &otherID ) )
		{
			if ( IsPartyMember( otherID.ConvertToUint64() ) )
				bIsInParty = true;
		}

		if ( pLocalPlayer && !pLocalPlayer->HasPlayerAsFriend( pOther ) && !bIsInParty )
			return true;
	}

#endif 
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: blocks/unblocks the target client from being heard
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVoiceStatus::SetPlayerBlockedState( int iPlayer )
{
	if ( voice_clientdebug.GetInt() == 1 )
	{
		Msg( "CVoiceStatus::SetPlayerBlockedState part 1\n" );
	}

	player_info_t pi;
	if ( !engine->GetPlayerInfo( iPlayer, &pi ) )
		return;

	if ( voice_clientdebug.GetInt() == 1 )
	{
		Msg( "CVoiceStatus::SetPlayerBlockedState part 2\n" );
	}

	// Squelch or (try to) unsquelch this player.
	if ( voice_clientdebug.GetInt() == 1 )
	{
		Msg("CVoiceStatus::SetPlayerBlockedState: setting player %d ban to %d\n", iPlayer, !m_BanMgr.GetPlayerBan(pi.guid));
	}

	m_BanMgr.SetPlayerBan(pi.guid, !m_BanMgr.GetPlayerBan(pi.guid));
	UpdateServerState(false);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVoiceStatus::SetHeadLabelMaterial( const char *pszMaterial )
{
	if ( m_pHeadLabelMaterial )
	{
		m_pHeadLabelMaterial->DecrementReferenceCount();
		m_pHeadLabelMaterial = NULL;
	}

	m_pHeadLabelMaterial = materials->FindMaterial( pszMaterial, TEXTURE_GROUP_VGUI );
	m_pHeadLabelMaterial->IncrementReferenceCount();
}
