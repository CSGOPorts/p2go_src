//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "hud_basechat.h"
#include <vgui/ILocalize.h>
#include "c_playerresource.h"
#include "voice_status.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DECLARE_HUDELEMENT_FLAGS( CHudChat, HUDELEMENT_SS_FULLSCREEN_ONLY );

DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, SayText2 );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );

//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	
}

void CHudChat::Init( void )
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, SayText2 );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudChat::MsgFunc_SayText( const CUsrMsg_SayText &msg )
{
	char szString[256];

	//msg.ReadByte(); // client ID // SanyaSho: Used only in ASW
	Q_strncpy( szString, msg.text().c_str(), sizeof( szString ) );
	Printf( CHAT_FILTER_NONE, "%s", szString );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Chat text from the server
//-----------------------------------------------------------------------------
bool CHudChat::MsgFunc_SayText2( const CUsrMsg_SayText2 &msg )
{
	if (CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters())
	{
		if (pParameters->m_bAnonymousPlayerIdentity)
			return true; // cannot print potentially personal details
	}

	int client = msg.ent_idx();
	bool bWantsToChat = msg.chat() != 0;

	wchar_t szBuf[6][256];
	char untranslated_msg_text[256];
	wchar_t *msg_text = ReadLocalizedString(msg.msg_name().c_str(), szBuf[0], sizeof(szBuf[0]), false, untranslated_msg_text, sizeof(untranslated_msg_text));

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString(msg.params(0).c_str(), szBuf[1], sizeof(szBuf[1]));		// player name
	ReadChatTextString(msg.params(1).c_str(), szBuf[2], sizeof(szBuf[2]), true);	// location
	ReadLocalizedString(msg.params(2).c_str(), szBuf[3], sizeof(szBuf[3]), true);	// radio text
	ReadLocalizedString(msg.params(3).c_str(), szBuf[4], sizeof(szBuf[4]), true);	// unused :(

	if (V_strcmp(msg.params(3).c_str(), "auto") != 0 && (GetClientVoiceMgr()->IsPlayerBlocked(client) || GetClientVoiceMgr()->ShouldHideCommunicationFromPlayer(client)))
		bWantsToChat = false;

	g_pVGuiLocalize->ConstructString(szBuf[5], sizeof(szBuf[5]), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);

	char ansiString[512];
	g_pVGuiLocalize->ConvertUnicodeToANSI(ConvertCRtoNL(szBuf[5]), ansiString, sizeof(ansiString));

	if (bWantsToChat)
	{
		int iFilter = CHAT_FILTER_NONE;
		bool playChatSound = true;

		if (client > 0 && g_PR && (g_PR->GetTeam(client) != g_PR->GetTeam(GetLocalPlayerIndex())))
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
			if (!(iFilter & GetFilterFlags()))
			{
				playChatSound = false;
			}
		}

		// print raw chat text
		ChatPrintf(client, iFilter, "%s", ansiString);

		Msg("%s\n", RemoveColorMarkup(ansiString));

		if (playChatSound)
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound(filter, -1, "HudChat.Message"); //SOUND_FROM_LOCAL_PLAYER is defined in iEngineAudio.h afaik and its just pointed to -1 so thats what ill make it here
		}
	}
	else
	{
		if (!GetClientVoiceMgr()->IsPlayerBlocked(client) && !GetClientVoiceMgr()->ShouldHideCommunicationFromPlayer(client))
		{
			// print raw chat text
			ChatPrintf(client, GetFilterForString(untranslated_msg_text), "%s", ansiString);
		}
	}

	return true;
}

// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message 
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next (optional) one to four strings are parameters for that string (which can also be message names if they begin with '#')
bool CHudChat::MsgFunc_TextMsg( const CUsrMsg_TextMsg &msg )
{
	char szString[2048];
	int msg_dest = msg.msg_dst();
	static char szBuf[6][256];

	Q_strncpy( szString, msg.params( 0 ).c_str(), sizeof( szString ) );
	char *msg_text = hudtextmessage->LookupString( szString, &msg_dest );
	Q_strncpy( szBuf[0], msg_text, sizeof( szBuf[0] ) );
	msg_text = szBuf[0];

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	Q_strncpy( szString, msg.params( 1 ).c_str(), sizeof( szString ) );
	char *sstr1 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[1], sstr1, sizeof( szBuf[1] ) );
	sstr1 = szBuf[1];

	StripEndNewlineFromString( sstr1 );  // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
	Q_strncpy( szString, msg.params( 2 ).c_str(), sizeof( szString ) );
	char *sstr2 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[2], sstr2, sizeof( szBuf[2] ) );
	sstr2 = szBuf[2];
	
	StripEndNewlineFromString( sstr2 );
	Q_strncpy( szString, msg.params( 3 ).c_str(), sizeof( szString ) );
	char *sstr3 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[3], sstr3, sizeof( szBuf[3] ) );
	sstr3 = szBuf[3];

	StripEndNewlineFromString( sstr3 );
	Q_strncpy( szString, msg.params( 4 ).c_str(), sizeof( szString ) );
	char *sstr4 = hudtextmessage->LookupString( szString );
	Q_strncpy( szBuf[4], sstr4, sizeof( szBuf[4] ) );
	sstr4 = szBuf[4];
	
	StripEndNewlineFromString( sstr4 );
	char *psz = szBuf[5];

	if ( !cl_showtextmsg.GetInt() )
		return false;

	switch ( msg_dest )
	{
	case HUD_PRINTCENTER:
		Q_snprintf( psz, sizeof( szBuf[5] ), msg_text, sstr1, sstr2, sstr3, sstr4 );
		GetCenterPrint()->Print( ConvertCRtoNL( psz ) );
		break;

	case HUD_PRINTNOTIFY:
		psz[0] = 1;  // mark this message to go into the notify buffer
		Q_snprintf( psz+1, sizeof( szBuf[5] ) - 1, msg_text, sstr1, sstr2, sstr3, sstr4 );
		Msg( "%s", ConvertCRtoNL( psz ) );
		break;

	case HUD_PRINTTALK:
		Q_snprintf( psz, sizeof( szBuf[5] ), msg_text, sstr1, sstr2, sstr3, sstr4 );
		Printf( CHAT_FILTER_NONE, "%s", ConvertCRtoNL( psz ) );
		break;

	case HUD_PRINTCONSOLE:
		Q_snprintf( psz, sizeof( szBuf[5] ), msg_text, sstr1, sstr2, sstr3, sstr4 );
		Msg( "%s", ConvertCRtoNL( psz ) );
		break;
	}

	return true;
}

