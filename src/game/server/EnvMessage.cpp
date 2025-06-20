//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "EnvMessage.h"
#include "engine/IEngineSound.h"
#include "keyvalues.h"
#include "filesystem.h"
#include "color.h"
#include "GameStats.h"
#include "portal2_usermessages.pb.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_message, CMessage );

BEGIN_DATADESC( CMessage )

	DEFINE_KEYFIELD( m_iszMessage, FIELD_STRING, "message" ),
	DEFINE_KEYFIELD( m_sNoise, FIELD_SOUNDNAME, "messagesound" ),
	DEFINE_KEYFIELD( m_MessageAttenuation, FIELD_INTEGER, "messageattenuation" ),
	DEFINE_KEYFIELD( m_MessageVolume, FIELD_FLOAT, "messagevolume" ),

	DEFINE_FIELD( m_Radius, FIELD_FLOAT ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ShowMessage", InputShowMessage ),

	DEFINE_OUTPUT(m_OnShowMessage, "OnShowMessage"),

END_DATADESC()



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessage::Spawn( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	switch( m_MessageAttenuation )
	{
	case 1: // Medium radius
		m_Radius = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		m_Radius = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		m_Radius = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		m_Radius = SNDLVL_IDLE;
		break;
	}
	m_MessageAttenuation = 0;

	// Remap volume from [0,10] to [0,1].
	m_MessageVolume *= 0.1;

	// No volume, use normal
	if ( m_MessageVolume <= 0 )
	{
		m_MessageVolume = 1.0;
	}
}


void CMessage::Precache( void )
{
	if ( m_sNoise != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_sNoise) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CMessage::InputShowMessage( inputdata_t &inputdata )
{
	CBaseEntity *pPlayer = NULL;

	if ( m_spawnflags & SF_MESSAGE_ALL )
	{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
		UTIL_ClientPrintAll( HUD_PRINTCENTER, STRING( m_iszMessage ) );
#else
		UTIL_ShowMessageAll( STRING( m_iszMessage ) );
#endif
	}
	else
	{
		if ( inputdata.pActivator && inputdata.pActivator->IsPlayer() )
		{
			pPlayer = inputdata.pActivator;
		}
		else
		{
			pPlayer = (gpGlobals->maxClients > 1) ? NULL : UTIL_GetLocalPlayer();
		}

		if ( pPlayer && pPlayer->IsPlayer() )
		{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
			CRecipientFilter filterPlayer;
			filterPlayer.MakeReliable();
			filterPlayer.AddRecipient( ToBasePlayer( pPlayer ) );
			UTIL_ClientPrintFilter( filterPlayer, HUD_PRINTCENTER, STRING( m_iszMessage ) );
			filterPlayer.RemoveAllRecipients();
#else
			UTIL_ShowMessage( STRING( m_iszMessage ), ToBasePlayer( pPlayer ) );
#endif
		}
	}

	if ( m_sNoise != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );
		
		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_sNoise);
		ep.m_flVolume = m_MessageVolume;
		ep.m_SoundLevel = ATTN_TO_SNDLVL( m_Radius );

		EmitSound( filter, entindex(), ep );
	}

	if ( m_spawnflags & SF_MESSAGE_ONCE )
	{
		UTIL_Remove( this );
	}

	m_OnShowMessage.FireOutput( inputdata.pActivator, this );
}


void CMessage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	inputdata_t inputdata;

	inputdata.pActivator	= NULL;
	inputdata.pCaller		= NULL;

	InputShowMessage( inputdata );
}


class CCredits : public CPointEntity
{
public:
	DECLARE_CLASS( CMessage, CPointEntity );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	InputRollCredits( inputdata_t &inputdata );
	void	InputRollOutroCredits( inputdata_t &inputdata );
	void	InputShowLogo( inputdata_t &inputdata );
	void	InputSetLogoLength( inputdata_t &inputdata );

	COutputEvent m_OnCreditsDone;

	virtual void OnRestore();
private:

	void		RollOutroCredits();

	bool		m_bRolledOutroCredits;
	float		m_flLogoLength;
};

LINK_ENTITY_TO_CLASS( env_credits, CCredits );

BEGIN_DATADESC( CCredits )
	DEFINE_INPUTFUNC( FIELD_VOID, "RollCredits", InputRollCredits ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RollOutroCredits", InputRollOutroCredits ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ShowLogo", InputShowLogo ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetLogoLength", InputSetLogoLength ),
	DEFINE_OUTPUT( m_OnCreditsDone, "OnCreditsDone"),

	DEFINE_FIELD( m_bRolledOutroCredits, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLogoLength, FIELD_FLOAT )
END_DATADESC()

void CCredits::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

#ifndef PORTAL

static void CreditsDone_f( void )
{
	CCredits *pCredits = (CCredits*)gEntList.FindEntityByClassname( NULL, "env_credits" );

	if ( pCredits )
	{
		pCredits->m_OnCreditsDone.FireOutput( pCredits, pCredits );
	}
}

static ConCommand creditsdone("creditsdone", CreditsDone_f );

#endif // PORTAL

extern ConVar sv_unlockedchapters;

void CCredits::OnRestore()
{
	BaseClass::OnRestore();

	if ( m_bRolledOutroCredits )
	{
		// Roll them again so that the client .dll will send the "creditsdone" message and we'll
		//  actually get back to the main menu
		RollOutroCredits();
	}
}

void CCredits::RollOutroCredits()
{
	sv_unlockedchapters.SetValue( "15" );

	// Gurjeets - Not used in CSGO
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	//UserMessageBegin( user, "CreditsMsg" );
	//	WRITE_BYTE( 3 );
	//MessageEnd();

	CUsrMsg_CreditsMsg msg;
	msg.set_creditstype(3);
	SendUserMessage(user, UM_CreditsMsg, msg);
}

void CCredits::InputRollOutroCredits( inputdata_t &inputdata )
{
	RollOutroCredits();

	// In case we save restore
	m_bRolledOutroCredits = true;

	#ifndef _GAMECONSOLE
	gamestats->Event_Credits();
	#endif
}

void CCredits::InputShowLogo( inputdata_t &inputdata )
{
	//ASSERT(0);

	// Gurjeets - Not used in CSGO
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	if ( m_flLogoLength )
	{
	//	UserMessageBegin( user, "LogoTimeMsg" );
	//		WRITE_FLOAT( m_flLogoLength );
	//	MessageEnd();
		CUsrMsg_LogoTimeMsg msg;
		msg.set_logotime(m_flLogoLength);
		SendUserMessage(user, UM_LogoTimeMsg, msg);
	}
	else
	{
	//	UserMessageBegin( user, "CreditsMsg" );
	//		WRITE_BYTE( 1 );
	//	MessageEnd();
		CUsrMsg_CreditsMsg msg;
		msg.set_creditstype(1);
		SendUserMessage(user, UM_CreditsMsg, msg);
	}
}

void CCredits::InputSetLogoLength( inputdata_t &inputdata )
{
	m_flLogoLength = inputdata.value.Float();
}

void CCredits::InputRollCredits( inputdata_t &inputdata )
{
	// Gurjeets - Not used in CSGO
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	//UserMessageBegin( user, "CreditsMsg" );
	//	WRITE_BYTE( 2 );
	//MessageEnd();
	CUsrMsg_CreditsMsg msg;
	msg.set_creditstype(2);
	SendUserMessage(user, UM_CreditsMsg, msg);
}

//-----------------------------------------------------------------------------
// Purpose: Play the outtro stats at the end of the campaign
//-----------------------------------------------------------------------------
class COuttroStats : public CPointEntity
{
public:
	DECLARE_CLASS( COuttroStats, CPointEntity );
	DECLARE_DATADESC();

	void Spawn( void );
	void InputRollCredits( inputdata_t &inputdata );
	void InputRollStatsCrawl( inputdata_t &inputdata );
	void InputSkipStateChanged( inputdata_t &inputdata );

	void SkipThink( void );
	void CalcSkipState( int &skippingPlayers, int &totalPlayers );

	COutputEvent m_OnOuttroStatsDone;
};

//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( env_outtro_stats, COuttroStats );

BEGIN_DATADESC( COuttroStats )
	DEFINE_INPUTFUNC( FIELD_VOID, "RollStatsCrawl", InputRollStatsCrawl ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RollCredits", InputRollCredits ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SkipStateChanged", InputSkipStateChanged ),
	DEFINE_OUTPUT( m_OnOuttroStatsDone, "OnOuttroStatsDone"),
END_DATADESC()

//-----------------------------------------------------------------------------
void COuttroStats::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
void COuttroStats::InputRollStatsCrawl( inputdata_t &inputdata )
{
	// Gurjeets - Not used in CSGO
	//CReliableBroadcastRecipientFilter players;
	//UserMessageBegin( players, "StatsCrawlMsg" );
	//MessageEnd();

	SetThink( &COuttroStats::SkipThink );
	SetNextThink( gpGlobals->curtime + 1.0 );
}

//-----------------------------------------------------------------------------
void COuttroStats::InputRollCredits( inputdata_t &inputdata )
{
	ASSERT(0);

	// Gurjeets - Not used in CSGO

	//CReliableBroadcastRecipientFilter players;
	//UserMessageBegin( players, "creditsMsg" );
	//MessageEnd();
}

void COuttroStats::SkipThink( void )
{
	// if all valid players are skipping, then end
	int iNumSkips = 0;
	int iNumPlayers = 0;
	CalcSkipState( iNumSkips, iNumPlayers );

	if ( iNumSkips >= iNumPlayers )
	{
//		TheDirector->StartScenarioExit();
	}
	else
		SetNextThink( gpGlobals->curtime + 1.0 );
}

void COuttroStats::CalcSkipState( int &skippingPlayers, int &totalPlayers )
{
	// calc skip state
	skippingPlayers = 0;
	totalPlayers = 0;
}

void COuttroStats::InputSkipStateChanged( inputdata_t &inputdata )
{
	int iNumSkips = 0;
	int iNumPlayers = 0;
	CalcSkipState( iNumSkips, iNumPlayers );

	DevMsg( "COuttroStats: Skip state changed. %d players, %d skips\n", iNumPlayers, iNumSkips );
	
	// Gurjeets - Not used in CSGO
	//// Don't send to players in singleplayer
	//if ( iNumPlayers > 1 )
	//{
	//	CReliableBroadcastRecipientFilter players;
	//	UserMessageBegin( players, "StatsSkipState" );
	//		WRITE_BYTE( iNumSkips );
	//		WRITE_BYTE( iNumPlayers );
	//	MessageEnd();
	//}
}

void CC_Test_Outtro_Stats( const CCommand& args )
{
	CBaseEntity *pOuttro = gEntList.FindEntityByClassname( NULL, "env_outtro_stats" );
	if ( pOuttro )
	{
		variant_t emptyVariant;
		pOuttro->AcceptInput( "RollStatsCrawl", NULL, NULL, emptyVariant, 0 );
	}
}
static ConCommand test_outtro_stats("test_outtro_stats", CC_Test_Outtro_Stats, 0, FCVAR_CHEAT);
