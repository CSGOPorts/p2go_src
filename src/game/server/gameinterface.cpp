//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: encapsulates and implements all the accessing of the game dll from external
//			sources (only the engine at the time of writing)
//			This files ONLY contains functions and data necessary to build an interface
//			to external modules
//===========================================================================//

#include "cbase.h"

#if !defined (_GAMECONSOLE)

#endif  // _GAMECONSOLE

#include "gamestringpool.h"
#include "mapentities_shared.h"
#include "game.h"
#include "entityapi.h"
#include "client.h"
#include "saverestore.h"
#include "entitylist.h"
#include "gamerules.h"
#include "soundent.h"
#include "player.h"
#include "server_class.h"
#include "ai_node.h"
#include "ai_link.h"
#include "ai_saverestore.h"
#include "ai_networkmanager.h"
#include "ndebugoverlay.h"
#include "ivoiceserver.h"
#include <stdarg.h>
#include "movehelper_server.h"
#include "networkstringtable_gamedll.h"
#include "filesystem.h"
#include "func_areaportalwindow.h"
#include "igamesystem.h"
#include "init_factory.h"
#include "vstdlib/random.h"
#include "env_wind_shared.h"
#include "engine/IEngineSound.h"
#include "ispatialpartition.h"
#include "textstatsmgr.h"
#include "bitbuf.h"
#include "saverestoretypes.h"
#include "physics_saverestore.h"
#include "tier0/vprof.h"
#include "effect_dispatch_data.h"
#include "engine/IStaticPropMgr.h"
#include "TemplateEntities.h"
#include "ai_speech.h"
#include "soundenvelope.h"
#include "usermessages.h"
#include "physics.h"
#include "igameevents.h"
#include "EventLog.h"
#include "datacache/idatacache.h"
#include "engine/ivdebugoverlay.h"
#include "shareddefs.h"
#include "props.h"
#include "timedeventmgr.h"
#include "gameinterface.h"
#include "eventqueue.h"
#include "hltvdirector.h"
#if defined( REPLAY_ENABLED )
#include "replaydirector.h"
#endif
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "nav_mesh.h"
#include "ai_responsesystem.h"
#include "saverestore_stringtable.h"
#include "util.h"
#include "tier0/icommandline.h"
#include "datacache/imdlcache.h"
#include "engine/iserverplugin.h"
#include "env_debughistory.h"
#include "util_shared.h"
#include "player_voice_listener.h"

#ifdef _WIN32
#include "ienginevgui.h"
#include "vgui_gamedll_int.h"
#include "vgui_controls/AnimationController.h"
#endif

#include "ragdoll_shared.h"
#include "toolframework/iserverenginetools.h"
#include "sceneentity.h"
#include "appframework/IAppSystemGroup.h"
#include "scenefilecache/ISceneFileCache.h"
#include "tier2/tier2.h"
#include "particles/particles.h"
#include "GameStats.h"
#include "ixboxsystem.h"
#include "matchmaking/imatchframework.h"
#include "querycache.h"
#include "particle_parse.h"
#include "steam/steam_gameserver.h"
#include "tier3/tier3.h"
#include "serverbenchmark_base.h"
#include "vscript/ivscript.h"
#include "foundryhelpers_server.h"
#include "entity_tools_server.h"
#include "foundry/iserverfoundry.h"
#include "point_template.h"
#include "engine/iblackbox.h"
#include "vstdlib/jobthread.h"
#include "vscript_server.h"
#include "tier2/tier2_logging.h"
#include "fmtstr.h"
#include "mathlib/aabb.h"
#include "env_cascade_light.h"
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
#include "cstrike15/cs_player.h"
#include "gametypes/igametypes.h"
#include "cs_shareddefs.h"
#endif

#ifdef INFESTED_DLL
#include "missionchooser/iasw_mission_chooser.h"
#include "matchmaking/swarm/imatchext_swarm.h"
#endif

#ifdef CSTRIKE_DLL // BOTPORT: TODO: move these ifdefs out
#include "bot/bot.h"
#endif

#ifdef PORTAL
#include "portal_base2d_shared.h"
#include "portal_player.h"
#endif

#ifdef PORTAL2
#include "info_placement_helper.h"
#include "paint/paint_saverestore.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#endif // PORTAL2

#ifdef DOTA_DLL
#include "dota/dota_bot_commander.h"
#endif

#include "dedicated_server_ugc_manager.h"

#ifdef _WIN32
#include "IGameUIFuncs.h"
#endif

extern IToolFrameworkServer *g_pToolFrameworkServer;
extern IParticleSystemQuery *g_pParticleSystemQuery;

extern ConVar commentary;
extern ConVar game_type;
extern ConVar game_mode;
ConVar occlusion_test_shadow_length( "occlusion_test_shadow_length", "144", FCVAR_RELEASE, "Max length of completely occluded shadow to consider a player for occlusion test. If shadow provably stops at this distance, the player may be considered for occlusion test. For longer shadows, we just don't do occlusion because we are not likely to find full occlusion when one of the boxes is expanded too much." );
//ConVar occlusion_test_margins( "occlusion_test_margins", "36", FCVAR_RELEASE, "Amount by which the player bounding box is expanded for occlusion test. This margin should be large enough to accommodate player movement within a frame or two, and the longest weapon they might hold. Shadow does not take this into account." ); // default: 360 (max speed) / 30 ( give it a couple of frames) + however much the biggest weapon can stick out
ConVar occlusion_test_camera_margins( "occlusion_test_camera_margins", "12", FCVAR_RELEASE, "Amount by which the camera (viewer's eye) is expanded for occlusion test. This should be large enough to accommodate eye's movement within a frame or two" ); // default: 360 (max speed) / 30 ( give it a couple of frames)
ConVar occlusion_test_jump_margin( "occlusion_test_jump_margin", "12", FCVAR_RELEASE, "Amount by which the player bounding box is expanded up for occlusion test to account for jumping. This margin should be large enough to accommodate player movement within a frame or two. Affects both camera box and player box." ); // default: 360 (max speed) / 30 ( give it a couple of frames) + however much the biggest weapon can stick out
//ConVar occlusion_test_shadow_max_distance( "occlusion_test_shadow_max_distance", "1500", FCVAR_RELEASE, "Max distance at which to consider shadows for occlusion computations" );
ConVar pvs_min_player_distance( "pvs_min_player_distance", "1500", FCVAR_RELEASE, "Min distance to player at which PVS is used. At closer distances, PVS assumes we can see a shadow or something else from the player, so it's safer to just always be \"Visible\"" );

bool ShouldHideAllPlayers() { return false; }


// this context is not available on dedicated servers
// WARNING! always check if interfaces are available before using
#if !defined(NO_STEAM)
static CSteamAPIContext s_SteamAPIContext;	
CSteamAPIContext *steamapicontext = &s_SteamAPIContext;

// this context is not available on a pure client connected to a remote server.
// WARNING! always check if interfaces are available before using
static CSteamGameServerAPIContext s_SteamGameServerAPIContext;
CSteamGameServerAPIContext *steamgameserverapicontext = &s_SteamGameServerAPIContext;
#endif


IUploadGameStats *gamestatsuploader = NULL;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_CONSOLE, "Console" );


CTimedEventMgr g_NetworkPropertyEventMgr;

ISaveRestoreBlockHandler *GetEventQueueSaveRestoreBlockHandler();
ISaveRestoreBlockHandler *GetCommentarySaveRestoreBlockHandler();

CUtlLinkedList<CMapEntityRef, unsigned short> g_MapEntityRefs;

// Engine interfaces.
IVEngineServer	*engine = NULL;
IVoiceServer	*g_pVoiceServer = NULL;
#if !defined(_STATIC_LINKED)
IFileSystem		*filesystem = NULL;
#else
extern IFileSystem *filesystem;
#endif
INetworkStringTableContainer *networkstringtable = NULL;
IStaticPropMgrServer *staticpropmgr = NULL;
IUniformRandomStream *random = NULL;
IEngineSound *enginesound = NULL;
ISpatialPartition *partition = NULL;
IVModelInfo *modelinfo = NULL;
IEngineTrace *enginetrace = NULL;
IFileLoggingListener *filelogginglistener = NULL;
IGameEventManager2 *gameeventmanager = NULL;
IDataCache *datacache = NULL;
IVDebugOverlay * debugoverlay = NULL;
ISoundEmitterSystemBase *soundemitterbase = NULL;
IServerPluginHelpers *serverpluginhelpers = NULL;
#ifdef SERVER_USES_VGUI
IEngineVGui *enginevgui = NULL;
#endif // SERVER_USES_VGUI
IServerEngineTools *serverenginetools = NULL;
IServerFoundry *serverfoundry = NULL;
ISceneFileCache *scenefilecache = NULL;
#ifdef SERVER_USES_VGUI
IGameUIFuncs *gameuifuncs = NULL;
#endif // SERVER_USES_VGUI
IXboxSystem *xboxsystem = NULL;	// Xbox 360 only
IScriptManager *scriptmanager = NULL;
IBlackBox *blackboxrecorder = NULL;

#ifdef INFESTED_DLL
IASW_Mission_Chooser *missionchooser = NULL;
IMatchExtSwarm *g_pMatchExtSwarm = NULL;
#endif

IGameSystem *SoundEmitterSystem();
void SoundSystemPreloadSounds( void );

bool ModelSoundsCacheInit();
void ModelSoundsCacheShutdown();

void SceneManager_ClientActive( CBasePlayer *player );

class IMaterialSystem;
class IStudioRender;

#ifdef _DEBUG
static ConVar s_UseNetworkVars( "UseNetworkVars", "1", FCVAR_CHEAT, "For profiling, toggle network vars." );
#endif

extern ConVar sv_noclipduringpause;
ConVar sv_massreport( "sv_massreport", "0" );
ConVar sv_force_transmit_ents( "sv_force_transmit_ents", "0", FCVAR_RELEASE, "Will transmit all entities to client, regardless of PVS conditions (will still skip based on transmit flags, however)." );

ConVar sv_autosave( "sv_autosave", "1", 0, "Set to 1 to autosave game on level transition. Does not affect autosave triggers." );
ConVar *sv_maxreplay = NULL;

ConVar sv_comp_mode_allow_dc( "sv_comp_mode_allow_dc", 
	"0", FCVAR_DEVELOPMENTONLY, 
	"Set this to 1 to allow direct connects in Competitive mode" );

ConVar sv_dc_friends_reqd(
	"sv_dc_friends_reqd",
	"0", FCVAR_RELEASE,
	"Set this to 0 to allow direct connects to a game in progress even if no presents are present" );

static ConVar  *g_pcv_commentary = NULL;
static ConVar *g_pcv_ThreadMode = NULL;

#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CSteam3Server s_Steam3Server;
CSteam3Server  &Steam3Server()
{
	return s_Steam3Server;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteam3Server::CSteam3Server() 
{
	m_bInitialized = false;
}
#endif

// String tables
INetworkStringTable *g_pStringTableParticleEffectNames = NULL;
INetworkStringTable *g_pStringTableEffectDispatch = NULL;
INetworkStringTable *g_pStringTableVguiScreen = NULL;
INetworkStringTable *g_pStringTableMaterials = NULL;
INetworkStringTable *g_pStringTableInfoPanel = NULL;
INetworkStringTable *g_pStringTableClientSideChoreoScenes = NULL;
INetworkStringTable *g_pStringTableExtraParticleFiles = NULL;
INetworkStringTable *g_pStringTableMovies = NULL;

CStringTableSaveRestoreOps g_VguiScreenStringOps;

// Holds global variables shared between engine and game.
CGlobalVars *gpGlobals;
static int		g_nCommandClientIndex = -1;

// The chapter number of the current
static int		g_nCurrentChapterIndex = -1;

#ifdef _DEBUG
static ConVar sv_showhitboxes( "sv_showhitboxes", "-1", FCVAR_CHEAT, "Send server-side hitboxes for specified entity to client (NOTE:  this uses lots of bandwidth, use on listen server only)." );
#endif

static ClientPutInServerOverrideFn g_pClientPutInServerOverride = NULL;
static void UpdateChapterRestrictions( const char *mapname );

CSharedEdictChangeInfo *g_pSharedChangeInfo = NULL;

IChangeInfoAccessor *CBaseEdict::GetChangeAccessor()
{
	return engine->GetChangeAccessor( (const edict_t *)this );
}

const IChangeInfoAccessor *CBaseEdict::GetChangeAccessor() const
{
	return engine->GetChangeAccessor( (const edict_t *)this );
}

const char *GetHintTypeDescription( CAI_Hint *pHint );

void ClientPutInServerOverride( ClientPutInServerOverrideFn fn )
{
	g_pClientPutInServerOverride = fn;
}

ConVar ai_post_frame_navigation( "ai_post_frame_navigation", "0" );
class CPostFrameNavigationHook;
extern CPostFrameNavigationHook *PostFrameNavigationSystem( void );

static bool g_bHeadTrackingEnabled = false;

bool IsHeadTrackingEnabled()
{
#if defined( HL2_DLL )
	return g_bHeadTrackingEnabled;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Also in cdll so linked libs can extern it
//-----------------------------------------------------------------------------
bool UTIL_IsDedicatedServer( void )
{
	return engine->IsDedicatedServer();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int UTIL_GetCommandClientIndex( void )
{
	// -1 == unknown,dedicated server console
	// 0  == player 1

	// Convert to 1 based offset
	return (g_nCommandClientIndex+1);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBasePlayer *UTIL_GetCommandClient( void )
{
	int idx = UTIL_GetCommandClientIndex();
	if ( idx > 0 )
	{
		return UTIL_PlayerByIndex( idx );
	}

	// HLDS console issued command
	return NULL;
}

extern void InitializeCvars( void );

float			GetFloorZ(const Vector &origin);
void			UpdateAllClientData( void );
void			DrawMessageEntities();

#include "ai_network.h"

// For now just using one big AI network
extern ConVar think_limit;


#if 0
//-----------------------------------------------------------------------------
// Purpose: Draw output overlays for any measure sections
// Input  : 
//-----------------------------------------------------------------------------
void DrawMeasuredSections(void)
{
	int		row = 1;
	float	rowheight = 0.025;

	CMeasureSection *p = CMeasureSection::GetList();
	while ( p )
	{
		char str[256];
		Q_snprintf(str,sizeof(str),"%s",p->GetName());
		NDebugOverlay::ScreenText( 0.01,0.51+(row*rowheight),str, 255,255,255,255, 0.0 );
		
		Q_snprintf(str,sizeof(str),"%5.2f\n",p->GetTime().GetMillisecondsF());
		//Q_snprintf(str,sizeof(str),"%3.3f\n",p->GetTime().GetSeconds() * 100.0 / Plat_FloatTime());
		NDebugOverlay::ScreenText( 0.28,0.51+(row*rowheight),str, 255,255,255,255, 0.0 );

		Q_snprintf(str,sizeof(str),"%5.2f\n",p->GetMaxTime().GetMillisecondsF());
		//Q_snprintf(str,sizeof(str),"%3.3f\n",p->GetTime().GetSeconds() * 100.0 / Plat_FloatTime());
		NDebugOverlay::ScreenText( 0.34,0.51+(row*rowheight),str, 255,255,255,255, 0.0 );


		row++;

		p = p->GetNext();
	}

	bool sort_reset = false;

	// Time to redo sort?
	if ( measure_resort.GetFloat() > 0.0 &&
		Plat_FloatTime() >= CMeasureSection::m_dNextResort )
	{
		// Redo it
		CMeasureSection::SortSections();
		// Set next time
		CMeasureSection::m_dNextResort = Plat_FloatTime() + measure_resort.GetFloat();
		// Flag to reset sort accumulator, too
		sort_reset = true;
	}

	// Iterate through the sections now
	p = CMeasureSection::GetList();
	while ( p )
	{
		// Update max 
		p->UpdateMax();

		// Reset regular accum.
		p->Reset();
		// Reset sort accum less often
		if ( sort_reset )
		{
			p->SortReset();
		}
		p = p->GetNext();
	}

}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void DrawAllDebugOverlays( void ) 
{
	NDebugOverlay::PurgeTextOverlays();

	// If in debug select mode print the selection entities name or classname
	if (CBaseEntity::m_bInDebugSelect)
	{
		CBasePlayer* pPlayer =  UTIL_PlayerByIndex( CBaseEntity::m_nDebugPlayer );

		if (pPlayer)
		{
			// First try to trace a hull to an entity
			CBaseEntity *pEntity = pPlayer->FindPickerEntity();

			if ( pEntity ) 
			{
				pEntity->DrawDebugTextOverlays();
				pEntity->DrawBBoxOverlay();
				pEntity->SendDebugPivotOverlay();
			}
		}
	}

	// --------------------------------------------------------
	//  Draw debug overlay lines 
	// --------------------------------------------------------
	UTIL_DrawOverlayLines();

	// ------------------------------------------------------------------------
	// If in wc_edit mode draw a box to highlight which node I'm looking at
	// ------------------------------------------------------------------------
	if (engine->IsInEditMode())
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex( CBaseEntity::m_nDebugPlayer );
		if (pPlayer) 
		{
			if (g_pAINetworkManager->GetEditOps()->m_bLinkEditMode)
			{
				CAI_Link* pAILink = pPlayer->FindPickerAILink();
				if (pAILink)
				{
					// For now just using one big AI network
					Vector startPos = g_pBigAINet->GetNode(pAILink->m_iSrcID)->GetPosition(g_pAINetworkManager->GetEditOps()->m_iHullDrawNum);
					Vector endPos	= g_pBigAINet->GetNode(pAILink->m_iDestID)->GetPosition(g_pAINetworkManager->GetEditOps()->m_iHullDrawNum);
					Vector linkDir	= startPos-endPos;
					float linkLen = VectorNormalize( linkDir );
					
					// Draw in green if link that's been turned off
					if (pAILink->m_LinkInfo & bits_LINK_OFF)
					{
						NDebugOverlay::BoxDirection(startPos, Vector(-4,-4,-4), Vector(-linkLen,4,4), linkDir, 0,255,0,40,0);
					}
					// Draw in a pukey yellowish green if link that's "bashable", which is off to everyone but a special door basher
					else if (pAILink->m_LinkInfo & bits_LINK_ASW_BASHABLE)
					{
						NDebugOverlay::BoxDirection(startPos, Vector(-4,-4,-4), Vector(-linkLen,4,4), linkDir, 40,255,0,40,0);
					}
					else
					{
						NDebugOverlay::BoxDirection(startPos, Vector(-4,-4,-4), Vector(-linkLen,4,4), linkDir, 255,0,0,40,0);
					}
				}
			}
			else
			{
				CAI_Node* pAINode;
				if (g_pAINetworkManager->GetEditOps()->m_bAirEditMode)
				{
					pAINode = pPlayer->FindPickerAINode(NODE_AIR);
				}
				else
				{
					pAINode = pPlayer->FindPickerAINode(NODE_GROUND);
				}

				if (pAINode)
				{
					Vector vecPos = pAINode->GetPosition(g_pAINetworkManager->GetEditOps()->m_iHullDrawNum);
					NDebugOverlay::Box( vecPos, Vector(-8,-8,-8), Vector(8,8,8), 255,0,0,40,0);

					if ( pAINode->GetHint() )
					{
						CBaseEntity *pEnt = (CBaseEntity *)pAINode->GetHint();
						if ( pEnt->GetEntityName() != NULL_STRING )
						{
							NDebugOverlay::Text( vecPos + Vector(0,0,6), STRING(pEnt->GetEntityName()), false, 0 );
						}
						NDebugOverlay::Text( vecPos, GetHintTypeDescription( pAINode->GetHint() ), false, 0 );
					}
				}
			}
			// ------------------------------------
			// If in air edit mode draw guide line
			// ------------------------------------
			if (g_pAINetworkManager->GetEditOps()->m_bAirEditMode)
			{
				UTIL_DrawPositioningOverlay(g_pAINetworkManager->GetEditOps()->m_flAirEditDistance);
			}
			else
			{
				NDebugOverlay::DrawGroundCrossHairOverlay();
			}
		}
	}

	// For not just using one big AI Network
	if ( g_pAINetworkManager )
	{
		g_pAINetworkManager->GetEditOps()->DrawAINetworkOverlay();
	}

	// PERFORMANCE: only do this in developer mode
	if ( g_pDeveloper->GetInt() )
	{
		// iterate through all objects for debug overlays
		const CEntInfo *pInfo = gEntList.FirstEntInfo();

		for ( ;pInfo; pInfo = pInfo->m_pNext )
		{
			CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
			// HACKHACK: to flag off these calls
			if ( ent->m_debugOverlays || ent->m_pTimedOverlay )
			{
				MDLCACHE_CRITICAL_SECTION();
				ent->DrawDebugGeometryOverlays();
			}
		}
	}

	if ( sv_massreport.GetInt() )
	{
		// iterate through all objects for debug overlays
		const CEntInfo *pInfo = gEntList.FirstEntInfo();

		for ( ;pInfo; pInfo = pInfo->m_pNext )
		{
			CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
			if (!ent->VPhysicsGetObject())
				continue;

			char tempstr[512];
			Q_snprintf(tempstr, sizeof(tempstr),"%s: Mass: %.2f kg / %.2f lb (%s)", 
				STRING(ent->GetModelName()), ent->VPhysicsGetObject()->GetMass(), 
				kg2lbs(ent->VPhysicsGetObject()->GetMass()), 
				GetMassEquivalent(ent->VPhysicsGetObject()->GetMass()));
			ent->EntityText(0, tempstr, 0);
		}
	}

	// A hack to draw point_message entities w/o developer required
	DrawMessageEntities();
}

// enable threading of init functions on x360
static ConVar sv_threaded_init("sv_threaded_init", IsGameConsole() ? "1" : "0");

static bool InitGameSystems( CreateInterfaceFn appSystemFactory )
{
	// The string system must init first + shutdown last
	IGameSystem::Add( GameStringSystem() );

	// Physics must occur before the sound envelope manager
	IGameSystem::Add( PhysicsGameSystem() );

	// Precache system must be next (requires physics game system)
	IGameSystem::Add( g_pPrecacheRegister );

	// Used to service deferred navigation queries for NPCs
	IGameSystem::Add( (IGameSystem *) PostFrameNavigationSystem() );

	// Add game log system
	IGameSystem::Add( GameLogSystem() );

	// Add HLTV director 
	IGameSystem::Add( HLTVDirectorSystem() );

#ifdef DOTA_DLL
	IGameSystem::Add( DOTAPlayerCommanderSystem() );
#endif

#if defined( REPLAY_ENABLED )
	// Add Replay director
	IGameSystem::Add( ReplayDirectorSystem() );
#endif

	// Add sound emitter
	IGameSystem::Add( SoundEmitterSystem() );

#ifdef SERVER_USES_VGUI
	// Startup vgui
	if ( enginevgui )
	{
		if(!VGui_Startup( appSystemFactory ))
			return false;
	}
#endif // SERVER_USES_VGUI

	// load Mod specific game events ( MUST be before InitAllSystems() so it can pickup the mod specific events)
	gameeventmanager->LoadEventsFromFile("resource/ModEvents.res");

#ifdef CSTRIKE_DLL // BOTPORT: TODO: move these ifdefs out
	InstallBotControl();
#endif

	if ( !IGameSystem::InitAllSystems() )
		return false;

	// Due to dependencies, these are not autogamesystems
	if ( !ModelSoundsCacheInit() )
	{
		return false;
	}

	// Parse the particle manifest file & register the effects within it
//	ParseParticleEffects( false );

	InvalidateQueryCache();

	// create the Navigation Mesh interface
	TheNavMesh = NavMeshFactory();

	// init the gamestatsupload connection
	gamestatsuploader->InitConnection();

	return true;
}

CServerGameDLL g_ServerGameDLL;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerGameDLL, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL, g_ServerGameDLL);

bool CServerGameDLL::DLLInit( CreateInterfaceFn appSystemFactory, 
		CreateInterfaceFn physicsFactory, CreateInterfaceFn fileSystemFactory, 
		CGlobalVars *pGlobals)
{
	COM_TimestampedLog( "ConnectTier1/2/3Libraries - Start" );

	ConnectTier1Libraries( &appSystemFactory, 1 );
	ConnectTier2Libraries( &appSystemFactory, 1 );
	ConnectTier3Libraries( &appSystemFactory, 1 );

	COM_TimestampedLog( "ConnectTier1/2/3Libraries - Finish" );

	// Connected in ConnectTier1Libraries
	if ( cvar == NULL )
		return false;

#if !defined( SWDS ) && !defined(NO_STEAM)
	#ifndef _PS3
	SteamAPI_InitSafe();
	#endif
	s_SteamAPIContext.Init();
#endif
#if !defined(NO_STEAM)
	s_SteamGameServerAPIContext.Init();
#endif

	COM_TimestampedLog( "Factories - Start" );

	// init each (seperated for ease of debugging)
	if ( (engine = (IVEngineServer*)appSystemFactory(INTERFACEVERSION_VENGINESERVER, NULL)) == NULL )
		return false;
 
#if !defined(NO_STEAM)
	#ifndef _PS3
	if( SteamAPI_RestartAppIfNecessary( engine->GetAppID() ) )
	{
		return false;
	}
	#endif
#endif

	if ( (g_pVoiceServer = (IVoiceServer*)appSystemFactory(INTERFACEVERSION_VOICESERVER, NULL)) == NULL )
		return false;
	if ( (networkstringtable = (INetworkStringTableContainer *)appSystemFactory(INTERFACENAME_NETWORKSTRINGTABLESERVER,NULL)) == NULL )
		return false;
	if ( (staticpropmgr = (IStaticPropMgrServer *)appSystemFactory(INTERFACEVERSION_STATICPROPMGR_SERVER,NULL)) == NULL )
		return false;
	if ( (random = (IUniformRandomStream *)appSystemFactory(VENGINE_SERVER_RANDOM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (enginesound = (IEngineSound *)appSystemFactory(IENGINESOUND_SERVER_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (::partition = (ISpatialPartition *)appSystemFactory(INTERFACEVERSION_SPATIALPARTITION, NULL)) == NULL )
		return false;
	if ( (modelinfo = (IVModelInfo *)appSystemFactory(VMODELINFO_SERVER_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (enginetrace = (IEngineTrace *)appSystemFactory(INTERFACEVERSION_ENGINETRACE_SERVER,NULL)) == NULL )
		return false;
	if ( (filelogginglistener = (IFileLoggingListener *)appSystemFactory(FILELOGGINGLISTENER_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (filesystem = (IFileSystem *)fileSystemFactory(FILESYSTEM_INTERFACE_VERSION,NULL)) == NULL )
		return false;

	if ( (gameeventmanager = (IGameEventManager2 *)appSystemFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2,NULL)) == NULL )
		return false;
	if ( (datacache = (IDataCache*)appSystemFactory(DATACACHE_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (soundemitterbase = (ISoundEmitterSystemBase *)appSystemFactory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (gamestatsuploader = (IUploadGameStats *)appSystemFactory( INTERFACEVERSION_UPLOADGAMESTATS, NULL )) == NULL )
		return false;
	if ( !mdlcache )
		return false;
	if ( (serverpluginhelpers = (IServerPluginHelpers *)appSystemFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, NULL)) == NULL )
		return false;
	if ( (scenefilecache = (ISceneFileCache *)appSystemFactory( SCENE_FILE_CACHE_INTERFACE_VERSION, NULL )) == NULL )
		return false;
	if ( (blackboxrecorder = (IBlackBox *)appSystemFactory(BLACKBOX_INTERFACE_VERSION, NULL)) == NULL )
		return false;
	if ( (xboxsystem = (IXboxSystem *)appSystemFactory( XBOXSYSTEM_INTERFACE_VERSION, NULL )) == NULL )
		return false;

	if ( !CommandLine()->CheckParm( "-noscripting") )
	{
		scriptmanager = (IScriptManager *)appSystemFactory( VSCRIPT_INTERFACE_VERSION, NULL );
	}

#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	if ( ( g_pGameTypes = (IGameTypes *)appSystemFactory( VENGINE_GAMETYPES_VERSION, NULL )) == NULL )
		return false;
#endif

#ifdef SERVER_USES_VGUI
	// If not running dedicated, grab the engine vgui interface
	if ( !engine->IsDedicatedServer() )
	{
#ifdef _WIN32
		if ( ( enginevgui = ( IEngineVGui * )appSystemFactory(VENGINE_VGUI_VERSION, NULL)) == NULL )
			return false;
		
		// This interface is optional, and is only valid when running with -tools
		serverenginetools = ( IServerEngineTools * )appSystemFactory( VSERVERENGINETOOLS_INTERFACE_VERSION, NULL );
		
		gameuifuncs = (IGameUIFuncs * )appSystemFactory( VENGINE_GAMEUIFUNCS_VERSION, NULL );
#endif
	}
#endif // SERVER_USES_VGUI

#ifdef INFESTED_DLL
	if ( (missionchooser = (IASW_Mission_Chooser *)appSystemFactory(ASW_MISSION_CHOOSER_VERSION, NULL)) == NULL )
		return false;
	if ( (g_pMatchExtSwarm = (IMatchExtSwarm *)appSystemFactory(IMATCHEXT_SWARM_INTERFACE, NULL)) == NULL )
		return false;
#endif

	if ( !g_pMatchFramework )
		return false;
	if ( IMatchExtensions *pIMatchExtensions = g_pMatchFramework->GetMatchExtensions() )
		pIMatchExtensions->RegisterExtensionInterface(
			INTERFACEVERSION_SERVERGAMEDLL, static_cast< IServerGameDLL * >( this ) );

	COM_TimestampedLog( "Factories - Finish" );

	COM_TimestampedLog( "soundemitterbase->Connect" );

	// Yes, both the client and game .dlls will try to Connect, the soundemittersystem.dll will handle this gracefully
	if ( !soundemitterbase->Connect( appSystemFactory ) )
		return false;

	if ( CommandLine()->FindParm( "-headtracking" ) )
		g_bHeadTrackingEnabled = true;

	// cache the globals
	gpGlobals = pGlobals;

	g_pSharedChangeInfo = engine->GetSharedEdictChangeInfo();
	
	COM_TimestampedLog( "MathLib_Init" );

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// save these in case other system inits need them
	factorylist_t factories;
	factories.engineFactory = appSystemFactory;
	factories.fileSystemFactory = fileSystemFactory;
	factories.physicsFactory = physicsFactory;
	FactoryList_Store( factories );

	COM_TimestampedLog( "gameeventmanager->LoadEventsFromFile" );
	// load used game events  
	gameeventmanager->LoadEventsFromFile("resource/gameevents.res");

	COM_TimestampedLog( "InitializeCvars" );
	// init the cvar list first in case inits want to reference them
	InitializeCvars();
	
	COM_TimestampedLog( "g_pParticleSystemMgr->Init" );
	// Initialize the particle system
	bool bPrecacheParticles = IsPC() && !engine->IsCreatingXboxReslist();
	if ( !g_pParticleSystemMgr->Init( g_pParticleSystemQuery, bPrecacheParticles ) )
	{
		return false;
	}

	sv_cheats = g_pCVar->FindVar( "sv_cheats" );
	if ( !sv_cheats )
		return false;

	g_pcv_commentary = g_pCVar->FindVar( "commentary" );
	g_pcv_ThreadMode = g_pCVar->FindVar( "host_thread_mode" );

	sv_maxreplay = g_pCVar->FindVar( "sv_maxreplay" );

	COM_TimestampedLog( "g_pGameSaveRestoreBlockSet" );

	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetEntitySaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetAISaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetTemplateSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetDefaultResponseSystemSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetCommentarySaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetEventQueueSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetVScriptSaveRestoreBlockHandler() );

#if defined( PORTAL2 )
	g_pGameSaveRestoreBlockSet->AddBlockHandler( GetPaintSaveRestoreBlockHandler() );
#endif

	bool bInitSuccess = false;
	if ( sv_threaded_init.GetBool() )
	{
		CFunctorJob *pGameJob = new CFunctorJob( CreateFunctor( ParseParticleEffects, false ) );
		g_pThreadPool->AddJob( pGameJob );
		bInitSuccess = InitGameSystems( appSystemFactory );

		// FIXME: This method is a bit of a hack.
		// Try to update the screen every .06 seconds while waiting.
		float flLastUpdateTime = -1.0f;

		while( !pGameJob->IsFinished() )
		{
			float flTime = Plat_FloatTime();

			if ( flTime - flLastUpdateTime > .06f )
			{
				flLastUpdateTime = flTime;
				engine->RefreshScreenIfNecessary();
			}

			ThreadSleep( 0 );
		}
		pGameJob->Release();
	}
	else
	{
		COM_TimestampedLog( "ParseParticleEffects" );
		ParseParticleEffects( false );
		COM_TimestampedLog( "InitGameSystems - Start" );
		bInitSuccess = InitGameSystems( appSystemFactory );
		COM_TimestampedLog( "InitGameSystems - Finish" );
	}
	// try to get debug overlay, may be NULL if on HLDS
	debugoverlay = (IVDebugOverlay *)appSystemFactory( VDEBUG_OVERLAY_INTERFACE_VERSION, NULL );

	// init the gamestatsupload connection
	gamestatsuploader->InitConnection();

#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	// Load the game types.
	g_pGameTypes->Initialize();
#endif

	return true;
}

void CServerGameDLL::PostInit()
{
	IGameSystem::PostInitAllSystems();

#ifdef SERVER_USES_VGUI
	if ( !engine->IsDedicatedServer() && enginevgui )
	{
		if ( VGui_PostInit() )
		{
			// all good
		}
	}
#endif // SERVER_USES_VGUI
}

void CServerGameDLL::PostToolsInit()
{
	if ( serverenginetools )
	{
		serverfoundry = ( IServerFoundry * )serverenginetools->QueryInterface( VSERVERFOUNDRY_INTERFACE_VERSION );
	}
}

void CServerGameDLL::DLLShutdown( void )
{

	// Due to dependencies, these are not autogamesystems
	ModelSoundsCacheShutdown();

#ifdef PORTAL2
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetPaintSaveRestoreBlockHandler() );
#endif

	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetVScriptSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetCommentarySaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetEventQueueSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetDefaultResponseSystemSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetTemplateSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetAISaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetPhysSaveRestoreBlockHandler() );
	g_pGameSaveRestoreBlockSet->RemoveBlockHandler( GetEntitySaveRestoreBlockHandler() );

	g_pParticleSystemMgr->Shutdown();

	char *pFilename = g_TextStatsMgr.GetStatsFilename();
	if ( !pFilename || !pFilename[0] )
	{
		g_TextStatsMgr.SetStatsFilename( "stats.txt" );
	}
	g_TextStatsMgr.WriteFile( filesystem );

	IGameSystem::ShutdownAllSystems();

#ifdef SERVER_USES_VGUI
	if ( enginevgui )
	{
		VGui_Shutdown();
	}
#endif // SERVER_USES_VGUI

#ifdef CSTRIKE_DLL // BOTPORT: TODO: move these ifdefs out
	RemoveBotControl();
#endif


	// destroy the Navigation Mesh interface
	if (TheNavMesh)
	{
		delete TheNavMesh;
		TheNavMesh = NULL;
	}

#if !defined(NO_STEAM)
	s_SteamAPIContext.Clear(); // Steam API context shutdown
	s_SteamGameServerAPIContext.Clear();	
	// SteamAPI_Shutdown(); << Steam shutdown is controlled by engine
#endif
	
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	ConVar_Unregister();
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
// Purpose: See shareddefs.h for redefining this.  Don't even think about it, though, for HL2.  Or you will pay.  ywb 9/22/03
// Output : float
//-----------------------------------------------------------------------------
float CServerGameDLL::GetTickInterval( void ) const
{
	float tickinterval = DEFAULT_TICK_INTERVAL;

	if ( engine->IsDedicatedServerForXbox() )
		tickinterval = DEFAULT_TICK_INTERVAL_X360;
	else if ( engine->IsDedicatedServerForPS3() )
		tickinterval = DEFAULT_TICK_INTERVAL_PS3;

// Ignoring this for now, server ops are abusing it
#if !defined( TF_DLL )
	// override if tick rate specified in command line
	if ( CommandLine()->CheckParm( "-tickrate" ) )
	{
		float tickrate = CommandLine()->ParmValue( "-tickrate", 0.0f );
		if ( tickrate > 0 )
		{
			float fDesiredTickInterval = 1.0f / tickrate;

			// quantize tick intervals to the nearest N / 512 fraction
			float fNumerator = floorf(fDesiredTickInterval * 512.0f + 0.5f);

			tickinterval = fNumerator / 512.0f;
		}

		tickinterval = clamp(tickinterval, MINIMUM_TICK_INTERVAL, MAXIMUM_TICK_INTERVAL);
	}
#endif

	return tickinterval;
}

// This is called when a new game is started. (restart, map)
bool CServerGameDLL::GameInit( void )
{
	ResetGlobalState();

	const char* szConfigName = "game.cfg";
	if ( engine->IsDedicatedServerForXbox() || IsX360() )
		szConfigName = "game360.cfg";
	else if ( engine->IsDedicatedServerForPS3() || IsPS3() )
		szConfigName = "gamePS3.cfg";

	char szCommand[64];
	V_snprintf(szCommand, sizeof(szCommand), "exec %s\n", szConfigName);
	engine->ServerCommand( szCommand );

	engine->ServerExecute( );
	CBaseEntity::sm_bAccurateTriggerBboxChecks = true;
	
	IGameEvent *event = gameeventmanager->CreateEvent( "game_init" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}

	return true;
}

// This is called when a game ends (server disconnect, death, restart, load)
// NOT on level transitions within a game
void CServerGameDLL::GameShutdown( void )
{
	ResetGlobalState();
}

static bool g_OneWayTransition = false;
void Game_SetOneWayTransition( void )
{
	g_OneWayTransition = true;
}

static CUtlVector<EHANDLE> g_RestoredEntities;
// just for debugging, assert that this is the only time this function is called
static bool g_InRestore = false;

void AddRestoredEntity( CBaseEntity *pEntity )
{
	Assert(g_InRestore);
	if ( !pEntity )
		return;

	g_RestoredEntities.AddToTail( pEntity );
}

void EndRestoreEntities()
{
	if ( !g_InRestore )
		return;
		
	// The entire hierarchy is restored, so we can call GetAbsOrigin again.
	//CBaseEntity::SetAbsQueriesValid( true );

	// Call all entities' OnRestore handlers
	for ( int i = g_RestoredEntities.Count()-1; i >=0; --i )
	{
		CBaseEntity *pEntity = g_RestoredEntities[i].Get();
		if ( pEntity && !pEntity->IsDormant() )
		{
			MDLCACHE_CRITICAL_SECTION();
			pEntity->OnRestore();
		}
	}

	g_RestoredEntities.Purge();

	IGameSystem::OnRestoreAllSystems();

	g_InRestore = false;
	gEntList.CleanupDeleteList();

	// HACKHACK: UNDONE: We need to redesign the main loop with respect to save/load/server activate
	g_ServerGameDLL.ServerActivate( NULL, 0, 0 );
	CBaseEntity::SetAllowPrecache( false );
}

void BeginRestoreEntities()
{
	if ( g_InRestore )
	{
		DevMsg( "BeginRestoreEntities without previous EndRestoreEntities.\n" );
		gEntList.CleanupDeleteList();
	}
	g_RestoredEntities.Purge();
	g_InRestore = true;

	CBaseEntity::SetAllowPrecache( true );

	// No calls to GetAbsOrigin until the entire hierarchy is restored!
	//CBaseEntity::SetAbsQueriesValid( false );
}

//-----------------------------------------------------------------------------
// Purpose: This prevents sv.tickcount/gpGlobals->tickcount from advancing during restore which
//  would cause a lot of the NPCs to fast forward their think times to the same
//  tick due to some ticks being elapsed during restore where there was no simulation going on
//-----------------------------------------------------------------------------
bool CServerGameDLL::IsRestoring()
{
	return g_InRestore;
}

bool CServerGameDLL::SupportsSaveRestore()
{
#ifdef INFESTED_DLL
	return false;
#endif

	return true;
}

// Called any time a new level is started (after GameInit() also on level transitions within a game)
bool CServerGameDLL::LevelInit( const char *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background )
{
	VPROF("CServerGameDLL::LevelInit");
	ResetWindspeed();
	UpdateChapterRestrictions( pMapName );

	// IGameSystem::LevelInitPreEntityAllSystems() is called when the world is precached
	// That happens either in LoadGameState() or in MapEntity_ParseAllEntities()
	if ( loadGame )
	{
		if ( pOldLevel )
		{
			gpGlobals->eLoadType = MapLoad_Transition;
		}
		else
		{
			gpGlobals->eLoadType = MapLoad_LoadGame;
		}

		BeginRestoreEntities();
		if ( !engine->LoadGameState( pMapName, 1 ) )
		{
			if ( pOldLevel )
			{
				MapEntity_ParseAllEntities( pMapEntities );
			}
			else
			{
				// Regular save load case
				return false;
			}
		}

		if ( pOldLevel )
		{
			engine->LoadAdjacentEnts( pOldLevel, pLandmarkName );
		}

#ifdef PORTAL
		g_OneWayTransition = true;
#endif
		if ( g_OneWayTransition )
		{
			engine->ClearSaveDirAfterClientLoad();
		}

		if ( pOldLevel && sv_autosave.GetBool() == true && gpGlobals->maxClients == 1 )
		{
			// This is a single-player style level transition.
			// Queue up an autosave one second into the level
			CBaseEntity *pAutosave = CBaseEntity::Create( "logic_autosave", vec3_origin, vec3_angle, NULL );
			if ( pAutosave )
			{
				g_EventQueue.AddEvent( pAutosave, "Save", 1.0, NULL, NULL );
				g_EventQueue.AddEvent( pAutosave, "Kill", 1.1, NULL, NULL );
			}
		}
	}
	else
	{
		if ( background )
		{
			gpGlobals->eLoadType = MapLoad_Background;
		}
		else
		{
			gpGlobals->eLoadType = MapLoad_NewGame;
		}

		// Clear out entity references, and parse the entities into it.
		g_MapEntityRefs.Purge();
		CMapLoadEntityFilter filter;
		MapEntity_ParseAllEntities( pMapEntities, &filter );

		g_pServerBenchmark->StartBenchmark();

		// Now call the mod specific parse
		LevelInit_ParseAllEntities( pMapEntities );
	}

	// Check low violence settings for this map
	g_RagdollLVManager.SetLowViolence( pMapName );

	// Now that all of the active entities have been loaded in, precache any entities who need point_template parameters
	//  to be parsed (the above code has loaded all point_template entities)
	PrecachePointTemplates();

	// load MOTD from file into stringtable
	LoadMessageOfTheDay();

	// Sometimes an ent will Remove() itself during its precache, so RemoveImmediate won't happen.
	// This makes sure those ents get cleaned up.
	gEntList.CleanupDeleteList();

	g_AIFriendliesTalkSemaphore.Release();
	g_AIFoesTalkSemaphore.Release();
	g_OneWayTransition = false;

	// clear any pending autosavedangerous
	m_fAutoSaveDangerousTime = 0.0f;
	m_fAutoSaveDangerousMinHealthToCommit = 0.0f;

	// ask for the latest game rules
	GameRules()->UpdateGameplayStatsFromSteam();

	if ( gpGlobals->eLoadType == MapLoad_Transition )
	{
		static ConVarRef map_wants_save_disable( "map_wants_save_disable" );
		map_wants_save_disable.SetValue( 0 );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: called after every level change and load game, iterates through all the
//			active entities and gives them a chance to fix up their state
//-----------------------------------------------------------------------------
#ifdef DEBUG
bool g_bReceivedChainedActivate;
bool g_bCheckForChainedActivate;
#define BeginCheckChainedActivate() if (0) ; else { g_bCheckForChainedActivate = true; g_bReceivedChainedActivate = false; }
#define EndCheckChainedActivate( bCheck )	\
	if (0) ; else \
	{ \
		if ( bCheck ) \
		{ \
			char msg[ 1024 ];	\
			Q_snprintf( msg, sizeof( msg ),  "Entity (%i/%s/%s) failed to call base class Activate()\n", pClass->entindex(), pClass->GetClassname(), STRING( pClass->GetEntityName() ) );	\
			AssertMsg( g_bReceivedChainedActivate == true, msg ); \
		} \
		g_bCheckForChainedActivate = false; \
	}
#else
#define BeginCheckChainedActivate()			((void)0)
#define EndCheckChainedActivate( bCheck )	((void)0)
#endif

void CServerGameDLL::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	// HACKHACK: UNDONE: We need to redesign the main loop with respect to save/load/server activate
	if ( g_InRestore )
		return;

	if ( gEntList.ResetDeleteList() != 0 )
	{
		Msg( "ERROR: Entity delete queue not empty on level start!\n" );
	}

	for ( CBaseEntity *pClass = gEntList.FirstEnt(); pClass != NULL; pClass = gEntList.NextEnt(pClass) )
	{
		if ( pClass && !pClass->IsDormant() )
		{
			MDLCACHE_CRITICAL_SECTION();

			BeginCheckChainedActivate();
			pClass->Activate();
			
			// We don't care if it finished activating if it decided to remove itself.
			EndCheckChainedActivate( !( pClass->GetEFlags() & EFL_KILLME ) ); 
		}
	}

	IGameSystem::LevelInitPostEntityAllSystems();
	// No more precaching after PostEntityAllSystems!!!
	CBaseEntity::SetAllowPrecache( false );

	// only display the think limit when the game is run with "developer" mode set
	if ( !g_pDeveloper->GetInt() )
	{
		think_limit.SetValue( 0 );
	}

#ifndef _XBOX
	// load the Navigation Mesh for this map
	TheNavMesh->Load();
	TheNavMesh->OnServerActivate();
#endif

#if defined( CSTRIKE_DLL ) // BOTPORT: TODO: move these ifdefs out
	TheBots->ServerActivate();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called after the steam API has been activated post-level startup
//-----------------------------------------------------------------------------
void CServerGameDLL::GameServerSteamAPIActivated( bool bActive )
{
	if ( bActive )
	{
#if !defined( NO_STEAM )
		steamgameserverapicontext->Init();
#endif

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
		GCClientSystem()->Activate();
#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	}
	else
	{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
		GCClientSystem()->Shutdown();
#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

#if !defined( NO_STEAM )
		steamgameserverapicontext->Clear();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called at the start of every game frame
//-----------------------------------------------------------------------------
ConVar  trace_report( "trace_report", "0" );

void CServerGameDLL::GameFrame( bool simulating )
{
	VPROF( "CServerGameDLL::GameFrame" );
	SNPROF( "CServerGameDLL::GameFrame" );

	// Don't run frames until fully restored
	if ( g_InRestore )
		return;

#ifndef NO_STEAM
	// All the calls to us from the engine prior to gameframe (like LevelInit & ServerActivate)
	// are done before the engine has got the Steam API connected, so we have to wait until now to connect ourselves.
	if ( Steam3Server().CheckInitialized() )
	{
		GameRules()->UpdateGameplayStatsFromSteam();
	}
#endif

	g_bIsLogging = engine->IsLogEnabled();
	if ( CBaseEntity::IsSimulatingOnAlternateTicks() )
	{
		// only run simulation on even numbered ticks
		if ( gpGlobals->tickcount & 1 )
		{
			UpdateAllClientData();
			return;
		}
		// If we're skipping frames, then the frametime is 2x the normal tick
		gpGlobals->frametime *= 2.0f;
	}

	float oldframetime = gpGlobals->frametime;

#ifdef _DEBUG
	// For profiling.. let them enable/disable the networkvar manual mode stuff.
	g_bUseNetworkVars = s_UseNetworkVars.GetBool();
#endif

	extern void GameStartFrame( void );
	extern void ServiceEventQueue( void );
	extern void Physics_RunThinkFunctions( bool simulating );

	// Delete anything that was marked for deletion
	//  outside of server frameloop (e.g., in response to concommand)
	gEntList.CleanupDeleteList();
	HandleFoundryEntitySpawnRecords();

	{
		MDLCACHE_CRITICAL_SECTION();

		IGameSystem::FrameUpdatePreEntityThinkAllSystems();
		GameStartFrame();


		TheNavMesh->Update();

		{
			VPROF( "gamestatsuploader->UpdateConnection" );
			SNPROF( "gamestatsuploader->UpdateConnection" );
			gamestatsuploader->UpdateConnection();
		}
		{
			VPROF( "UpdateQueryCache" );
			SNPROF( "UpdateQueryCache" );
			UpdateQueryCache();
		}
		
		{
			VPROF( "g_pServerBenchmark->UpdateBenchmark" );
			SNPROF( "g_pServerBenchmark->UpdateBenchmark" );
			g_pServerBenchmark->UpdateBenchmark();
		}
		

		Physics_RunThinkFunctions( simulating );
		
		IGameSystem::FrameUpdatePostEntityThinkAllSystems();

		// UNDONE: Make these systems IGameSystems and move these calls into FrameUpdatePostEntityThink()
		// service event queue, firing off any actions whos time has come
		ServiceEventQueue();
	}

	// free all ents marked in think functions
	gEntList.CleanupDeleteList();

	// FIXME:  Should this only occur on the final tick?
	UpdateAllClientData();

	if ( g_pGameRules )
	{
		VPROF( "g_pGameRules->EndGameFrame" );
		SNPROF( "g_pGameRules->EndGameFrame" );
		g_pGameRules->EndGameFrame();
	}

	if ( trace_report.GetBool() )
	{
		int total = 0, totals[3];
		for ( int i = 0; i < 3; i++ )
		{
			totals[i] = enginetrace->GetStatByIndex( i, true );
			if ( totals[i] > 0 )
			{
				total += totals[i];
			}
		}

		if ( total )
		{
			Msg("Trace: %d, contents %d, enumerate %d\n", totals[0], totals[1], totals[2] );
		}
	}

	// Any entities that detect network state changes on a timer do it here.
	g_NetworkPropertyEventMgr.FireEvents();

	gpGlobals->frametime = oldframetime;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame even if not ticking
// Input  : simulating - 
//-----------------------------------------------------------------------------
void CServerGameDLL::PreClientUpdate( bool simulating )
{
	if ( !simulating )
		return;

	/*
	if (game_speeds.GetInt())
	{
		DrawMeasuredSections();
	}
	*/

//#ifdef _DEBUG  - allow this in release for now
	DrawAllDebugOverlays();
//#endif
	
	IGameSystem::PreClientUpdateAllSystems();

#ifdef _DEBUG
	if ( sv_showhitboxes.GetInt() == -1 )
		return;

	if ( sv_showhitboxes.GetInt() == 0 )
	{
		// assume it's text
		CBaseEntity *pEntity = NULL;

		while (1)
		{
			pEntity = gEntList.FindEntityByName( pEntity, sv_showhitboxes.GetString() );
			if ( !pEntity )
				break;

			CBaseAnimating *anim = dynamic_cast< CBaseAnimating * >( pEntity );

			if (anim)
			{
				anim->DrawServerHitboxes();
			}
		}
		return;
	}

	CBaseAnimating *anim = dynamic_cast< CBaseAnimating * >( CBaseEntity::Instance( INDEXENT( sv_showhitboxes.GetInt() ) ) );
	if ( !anim )
		return;

	anim->DrawServerHitboxes();
#endif
}

void CServerGameDLL::Think( bool finalTick )
{
	if ( m_fAutoSaveDangerousTime != 0.0f && m_fAutoSaveDangerousTime < gpGlobals->curtime )
	{
		// The safety timer for a dangerous auto save has expired
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );

		if ( pPlayer && ( pPlayer->GetDeathTime() == 0.0f || pPlayer->GetDeathTime() > gpGlobals->curtime )
			&& !pPlayer->IsSinglePlayerGameEnding()
			)
		{
			if( pPlayer->GetHealth() >= m_fAutoSaveDangerousMinHealthToCommit )
			{
				// The player isn't dead, so make the dangerous auto save safe
				engine->ServerCommand( "autosavedangerousissafe\n" );
			}
		}

		m_fAutoSaveDangerousTime = 0.0f;
		m_fAutoSaveDangerousMinHealthToCommit = 0.0f;
	}

#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	// TODO: would have liked this to be totally event driven... currently needs a tick.
	if ( engine->IsDedicatedServer() && steamgameserverapicontext->SteamHTTP() )
	{
		DedicatedServerWorkshop().Update();	
	}
#endif
}

void CServerGameDLL::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue )
{
}

// Called when a level is shutdown (including changing levels)
void CServerGameDLL::LevelShutdown( void )
{
	MDLCACHE_CRITICAL_SECTION();
	IGameSystem::LevelShutdownPreEntityAllSystems();

	// YWB:
	// This entity pointer is going away now and is corrupting memory on level transitions/restarts
	CSoundEnt::ShutdownSoundEnt();

	ClearDebugHistory();

	gEntList.Clear();

	InvalidateQueryCache();

	IGameSystem::LevelShutdownPostEntityAllSystems();

	// In case we quit out during initial load
	CBaseEntity::SetAllowPrecache( false );

	// reset the Navigation Mesh
	if ( TheNavMesh )
	{
		TheNavMesh->Reset();
	}

	g_nCurrentChapterIndex = -1;
	CStudioHdr::CActivityToSequenceMapping::ResetMappings();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : ServerClass*
//-----------------------------------------------------------------------------
ServerClass* CServerGameDLL::GetAllServerClasses()
{
	return g_pServerClassHead;
}


const char *CServerGameDLL::GetGameDescription( void )
{
	return ::GetGameDescription();
}

void CServerGameDLL::CreateNetworkStringTables( void )
{
	// Create any shared string tables here (and only here!)
	// E.g.:  xxx = networkstringtable->CreateStringTable( "SceneStrings", 512 );
	g_pStringTableExtraParticleFiles = networkstringtable->CreateStringTable( "ExtraParticleFilesTable", MAX_PARTICLESYSTEMS_STRINGS, 0, 0, NSF_DICTIONARY_ENABLED );
	g_pStringTableParticleEffectNames = networkstringtable->CreateStringTable( "ParticleEffectNames", MAX_PARTICLESYSTEMS_STRINGS, 0, 0, NSF_DICTIONARY_ENABLED );
	g_pStringTableEffectDispatch = networkstringtable->CreateStringTable( "EffectDispatch", MAX_EFFECT_DISPATCH_STRINGS );
	g_pStringTableVguiScreen = networkstringtable->CreateStringTable( "VguiScreen", MAX_VGUI_SCREEN_STRINGS );
	g_pStringTableMaterials = networkstringtable->CreateStringTable( "Materials", MAX_MATERIAL_STRINGS, 0, 0, NSF_DICTIONARY_ENABLED );
	g_pStringTableInfoPanel = networkstringtable->CreateStringTable( "InfoPanel", MAX_INFOPANEL_STRINGS );
	g_pStringTableClientSideChoreoScenes = networkstringtable->CreateStringTable( "Scenes", MAX_CHOREO_SCENES_STRINGS, 0, 0, NSF_DICTIONARY_ENABLED );
	g_pStringTableMovies = networkstringtable->CreateStringTable( "Movies", MAX_MOVIE_STRINGS, 0, 0, NSF_DICTIONARY_ENABLED );

	Assert( g_pStringTableParticleEffectNames &&
			g_pStringTableEffectDispatch &&
			g_pStringTableVguiScreen &&
			g_pStringTableMaterials &&
			g_pStringTableInfoPanel &&
			g_pStringTableClientSideChoreoScenes &&
			g_pStringTableExtraParticleFiles && 
			g_pStringTableMovies );

	// Need this so we have the error material always handy
	PrecacheMaterial( "debug/debugempty" );
	Assert( GetMaterialIndex( "debug/debugempty" ) == 0 );

	PrecacheParticleSystem( "error" );	// ensure error particle system is handy
	Assert( GetParticleSystemIndex( "error" ) == 0 );

	PrecacheEffect( "error" );	// ensure error effect is handy
	Assert( GetEffectIndex( "error" ) == 0 );

	CreateNetworkStringTables_GameRules();

	// Set up save/load utilities for string tables
	g_VguiScreenStringOps.Init( g_pStringTableVguiScreen );
}

CSaveRestoreData *CServerGameDLL::SaveInit( int size )
{
	return ::SaveInit(size);
}

//-----------------------------------------------------------------------------
// Purpose: Saves data from a struct into a saverestore object, to be saved to disk
// Input  : *pSaveData - the saverestore object
//			char *pname - the name of the data to write
//			*pBaseData - the struct into which the data is to be read
//			*pFields - pointer to an array of data field descriptions
//			fieldCount - the size of the array (number of field descriptions)
//-----------------------------------------------------------------------------
void CServerGameDLL::SaveWriteFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, pMap, pFields, fieldCount );
}


//-----------------------------------------------------------------------------
// Purpose: Reads data from a save/restore block into a structure
// Input  : *pSaveData - the saverestore object
//			char *pname - the name of the data to extract from
//			*pBaseData - the struct into which the data is to be restored
//			*pFields - pointer to an array of data field descriptions
//			fieldCount - the size of the array (number of field descriptions)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void CServerGameDLL::SaveReadFields( CSaveRestoreData *pSaveData, const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount )
{
	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pMap, pFields, fieldCount );
}

//-----------------------------------------------------------------------------

void CServerGameDLL::SaveGlobalState( CSaveRestoreData *s )
{
	::SaveGlobalState(s);
}

void CServerGameDLL::RestoreGlobalState(CSaveRestoreData *s)
{
	::RestoreGlobalState(s);
}

void CServerGameDLL::Save( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->Save( &saveHelper );
}

void CServerGameDLL::Restore( CSaveRestoreData *s, bool b)
{
	if ( engine->IsOverrideLoadGameEntsOn() )
		FoundryHelpers_ClearEntityHighlightEffects();

	CRestore restore(s);
	g_pGameSaveRestoreBlockSet->Restore( &restore, b );
	g_pGameSaveRestoreBlockSet->PostRestore();

	if ( serverfoundry && engine->IsOverrideLoadGameEntsOn() )
		serverfoundry->OnFinishedRestoreSavegame();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_type - 
//			*name - 
//			size - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------


CStandardSendProxies* CServerGameDLL::GetStandardSendProxies()
{
	return &g_StandardSendProxies;
}

int	CServerGameDLL::CreateEntityTransitionList( CSaveRestoreData *s, int a)
{
	CRestore restoreHelper( s );
	// save off file base
	int base = restoreHelper.GetReadPos();

	int movedCount = ::CreateEntityTransitionList(s, a);
	if ( movedCount )
	{
		g_pGameSaveRestoreBlockSet->CallBlockHandlerRestore( GetPhysSaveRestoreBlockHandler(), base, &restoreHelper, false );
		g_pGameSaveRestoreBlockSet->CallBlockHandlerRestore( GetAISaveRestoreBlockHandler(), base, &restoreHelper, false );
	}

	GetPhysSaveRestoreBlockHandler()->PostRestore();
	GetAISaveRestoreBlockHandler()->PostRestore();

	return movedCount;
}

void CServerGameDLL::PreSave( CSaveRestoreData *s )
{
	g_pGameSaveRestoreBlockSet->PreSave( s );
}

#include "client_textmessage.h"

// This little hack lets me marry BSP names to messages in titles.txt
typedef struct
{
	char *pBSPName;
	char *pTitleName;
} TITLECOMMENT;

// this list gets searched for the first partial match, so some are out of order
static TITLECOMMENT gTitleComments[] =
{
#ifdef PORTAL
	{ "testchmb_a_00",			"#Portal_Chapter1_Title"  },
	{ "testchmb_a_01",			"#Portal_Chapter1_Title"  },
	{ "testchmb_a_02",			"#Portal_Chapter2_Title"  },
	{ "testchmb_a_03",			"#Portal_Chapter2_Title"  },
	{ "testchmb_a_04",			"#Portal_Chapter3_Title"  },
	{ "testchmb_a_05",			"#Portal_Chapter3_Title"  },
	{ "testchmb_a_06",			"#Portal_Chapter4_Title"  },
	{ "testchmb_a_07",			"#Portal_Chapter4_Title"  },
	{ "testchmb_a_08_advanced",	"#Portal_Chapter5_Title"  },
	{ "testchmb_a_08",			"#Portal_Chapter5_Title"  },
	{ "testchmb_a_09_advanced",	"#Portal_Chapter6_Title"  },
	{ "testchmb_a_09",			"#Portal_Chapter6_Title"  },
	{ "testchmb_a_10_advanced",	"#Portal_Chapter7_Title"  },
	{ "testchmb_a_10",			"#Portal_Chapter7_Title"  },
	{ "testchmb_a_11_advanced",	"#Portal_Chapter8_Title"  },
	{ "testchmb_a_11",			"#Portal_Chapter8_Title"  },
	{ "testchmb_a_13_advanced",	"#Portal_Chapter9_Title"  },
	{ "testchmb_a_13",			"#Portal_Chapter9_Title"  },
	{ "testchmb_a_14_advanced",	"#Portal_Chapter10_Title"  },
	{ "testchmb_a_14",			"#Portal_Chapter10_Title"  },
	{ "testchmb_a_15",			"#Portal_Chapter11_Title"  },
	{ "escape_",				"#Portal_Chapter11_Title"  },
	{ "background2",			"#Portal_Chapter12_Title"  },
#else
	{ "intro", "#HL2_Chapter1_Title" },

	{ "d1_trainstation_05", "#HL2_Chapter2_Title" },
	{ "d1_trainstation_06", "#HL2_Chapter2_Title" },
	
	{ "d1_trainstation_", "#HL2_Chapter1_Title" },

	{ "d1_canals_06", "#HL2_Chapter4_Title" },
	{ "d1_canals_07", "#HL2_Chapter4_Title" },
	{ "d1_canals_08", "#HL2_Chapter4_Title" },
	{ "d1_canals_09", "#HL2_Chapter4_Title" },
	{ "d1_canals_1", "#HL2_Chapter4_Title" },
	
	{ "d1_canals_0", "#HL2_Chapter3_Title" },

	{ "d1_eli_", "#HL2_Chapter5_Title" },

	{ "d1_town_", "#HL2_Chapter6_Title" },

	{ "d2_coast_09", "#HL2_Chapter8_Title" },
	{ "d2_coast_1", "#HL2_Chapter8_Title" },
	{ "d2_prison_01", "#HL2_Chapter8_Title" },

	{ "d2_coast_", "#HL2_Chapter7_Title" },

	{ "d2_prison_06", "#HL2_Chapter9a_Title" },
	{ "d2_prison_07", "#HL2_Chapter9a_Title" },
	{ "d2_prison_08", "#HL2_Chapter9a_Title" },

	{ "d2_prison_", "#HL2_Chapter9_Title" },

	{ "d3_c17_01", "#HL2_Chapter9a_Title" },
	{ "d3_c17_09", "#HL2_Chapter11_Title" },
	{ "d3_c17_1", "#HL2_Chapter11_Title" },

	{ "d3_c17_", "#HL2_Chapter10_Title" },

	{ "d3_citadel_", "#HL2_Chapter12_Title" },

	{ "d3_breen_", "#HL2_Chapter13_Title" },
	{ "credits", "#HL2_Chapter14_Title" },

	{ "ep1_citadel_00", "#episodic_Chapter1_Title" },
	{ "ep1_citadel_01", "#episodic_Chapter1_Title" },
	{ "ep1_citadel_02b", "#episodic_Chapter1_Title" },
	{ "ep1_citadel_02", "#episodic_Chapter1_Title" },
	{ "ep1_citadel_03", "#episodic_Chapter2_Title" },
	{ "ep1_citadel_04", "#episodic_Chapter2_Title" },
	{ "ep1_c17_00a", "#episodic_Chapter3_Title" },
	{ "ep1_c17_00", "#episodic_Chapter3_Title" },
	{ "ep1_c17_01", "#episodic_Chapter4_Title" },
	{ "ep1_c17_02b", "#episodic_Chapter4_Title" },
	{ "ep1_c17_02", "#episodic_Chapter4_Title" },
	{ "ep1_c17_05", "#episodic_Chapter5_Title" },
	{ "ep1_c17_06", "#episodic_Chapter5_Title" },

	{ "ep2_outland_01a", "#ep2_Chapter1_Title" },
	{ "ep2_outland_01", "#ep2_Chapter1_Title" },
	{ "ep2_outland_02", "#ep2_Chapter2_Title" },
	{ "ep2_outland_03", "#ep2_Chapter2_Title" },
	{ "ep2_outland_04", "#ep2_Chapter2_Title" },
	{ "ep2_outland_05", "#ep2_Chapter3_Title" },
	
	{ "ep2_outland_06a", "#ep2_Chapter4_Title" },
	{ "ep2_outland_06", "#ep2_Chapter3_Title" },

	{ "ep2_outland_07", "#ep2_Chapter4_Title" },
	{ "ep2_outland_08", "#ep2_Chapter4_Title" },
	{ "ep2_outland_09", "#ep2_Chapter5_Title" },
	
	{ "ep2_outland_10a", "#ep2_Chapter5_Title" },
	{ "ep2_outland_10", "#ep2_Chapter5_Title" },

	{ "ep2_outland_11a", "#ep2_Chapter6_Title" },
	{ "ep2_outland_11", "#ep2_Chapter6_Title" },
	
	{ "ep2_outland_12a", "#ep2_Chapter7_Title" },
	{ "ep2_outland_12", "#ep2_Chapter6_Title" },
#endif
};

void CServerGameDLL::GetSaveComment( char *text, int maxlength, float flMinutes, float flSeconds, bool bNoTime )
{
	char comment[64];
	const char	*pName;
	int		i;

	char const *mapname = STRING( gpGlobals->mapname );

	pName = NULL;

	// Try to find a matching title comment for this mapname
	for ( i = 0; i < ARRAYSIZE(gTitleComments) && !pName; i++ )
	{
		if ( !Q_strnicmp( mapname, gTitleComments[i].pBSPName, strlen(gTitleComments[i].pBSPName) ) )
		{
			// found one
			int j;

			// Got a message, post-process it to be save name friendly
			Q_strncpy( comment, gTitleComments[i].pTitleName, sizeof( comment ) );
			pName = comment;
			j = 0;
			// Strip out CRs
			while ( j < 64 && comment[j] )
			{
				if ( comment[j] == '\n' || comment[j] == '\r' )
					comment[j] = 0;
				else
					j++;
			}
			break;
		}
	}
	
	// If we didn't get one, use the designer's map name, or the BSP name itself
	if ( !pName )
	{
		pName = mapname;
	}

	if ( bNoTime )
	{
		Q_snprintf( text, maxlength, "%-64.64s", pName );
	}
	else
	{
		int minutes = flMinutes;
		int seconds = flSeconds;

		// Wow, this guy/gal must suck...!
		if ( minutes >= 1000 )
		{
			minutes = 999;
			seconds = 59;
		}

		int minutesAdd = ( seconds / 60 );
		seconds %= 60;

		// add the elapsed time at the end of the comment, for the ui to parse out
		Q_snprintf( text, maxlength, "%-64.64s %03d:%02d", pName, (minutes + minutesAdd), seconds );
	}
}

void CServerGameDLL::WriteSaveHeaders( CSaveRestoreData *s )
{
	CSave saveHelper( s );
	g_pGameSaveRestoreBlockSet->WriteSaveHeaders( &saveHelper );
	g_pGameSaveRestoreBlockSet->PostSave();
}

void CServerGameDLL::ReadRestoreHeaders( CSaveRestoreData *s )
{
	CRestore restoreHelper( s );
	g_pGameSaveRestoreBlockSet->PreRestore();

	g_pGameSaveRestoreBlockSet->ReadRestoreHeaders( &restoreHelper );
}

void CServerGameDLL::PreSaveGameLoaded( char const *pSaveName, bool bInGame )
{
	#ifndef _GAMECONSOLE
	gamestats->Event_PreSaveGameLoaded( pSaveName, bInGame );
	#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the game DLL wants the server not to be made public.
//			Used by commentary system to hide multiplayer commentary servers from the master.
//-----------------------------------------------------------------------------
bool CServerGameDLL::ShouldHideServer( void )
{
	if ( g_pcv_commentary && g_pcv_commentary->GetBool() )
		return true;

	if ( gpGlobals->eLoadType == MapLoad_Background )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CServerGameDLL::InvalidateMdlCache()
{
	CBaseAnimating *pAnimating;
	for ( CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity != NULL; pEntity = gEntList.NextEnt(pEntity) )
	{
		pAnimating = dynamic_cast<CBaseAnimating *>(pEntity);
		if ( pAnimating )
		{
			pAnimating->InvalidateMdlCache();
		}
	}
	CStudioHdr::CActivityToSequenceMapping::ResetMappings();
}

static KeyValues * FindLaunchOptionByValue( KeyValues *pLaunchOptions, char const *szLaunchOption )
{
	if ( !pLaunchOptions || !szLaunchOption || !*szLaunchOption )
		return NULL;

	for ( KeyValues *val = pLaunchOptions->GetFirstSubKey(); val; val = val->GetNextKey() )
	{
		char const *szValue = val->GetString();
		if ( szValue && *szValue && !Q_stricmp( szValue, szLaunchOption ) )
			return val;
	}

	return NULL;
}

bool CServerGameDLL::ShouldPreferSteamAuth()
{
	return true;
}

bool CServerGameDLL::ShouldAllowDirectConnect()
{
	bool bAllowDC = true;
	DevMsg( "Not Valve ds: Direct-connect is allowed\n");
	return bAllowDC;
}

bool CServerGameDLL::FriendsReqdForDirectConnect()
{
	return sv_dc_friends_reqd.GetBool();
}

bool CServerGameDLL::IsLoadTestServer()
{
	if ( CommandLine()->FindParm( "-sv_load_test" ) )
	{
		return true;
	}

	return false;
}

bool CServerGameDLL::IsValveDS()
{
	return IsValveDedicated();
}

KeyValues*	CServerGameDLL::GetExtendedServerInfoForNewClient()
{
	static KeyValues *s_pExtendedServerInfo = NULL;
	return s_pExtendedServerInfo;
}

void CServerGameDLL::UpdateGCInformation()
{
	return;
}

void CServerGameDLL::ReportGCQueuedMatchStart(int32 iReservationStage, uint32* puiConfirmedAccounts, int numConfirmedAccounts)
{
	return;
}

// Last chance validation on connect packet for the client, non-NULL return value
// causes the client connect to be aborted with the provided error
char const* CServerGameDLL::ClientConnectionValidatePreNetChan(bool bGameServer, char const* adr, int nAuthProtocol, uint64 ullSteamID)
{
	/** Removed for partner depot **/
	return NULL;	// allow connections by default
}

//
// Matchmaking game data buffer to set into SteamGameServer()->SetGameData
//
void CServerGameDLL::GetMatchmakingGameData(char* buf, size_t bufSize)
{
	char* const bufBase = buf;
	int len = 0;

	// Put the game key
	Q_snprintf(buf, bufSize, "g:portal2,");
	len = strlen(buf);
	buf += len;
	bufSize -= len;

	// Trim the last comma if anything was written
	if (buf > bufBase)
		buf[-1] = 0;
}

// Network channel notification from engine to game server code
void CServerGameDLL::OnEngineClientNetworkEvent(edict_t* edictClient, uint64 ullSteamID, int nEventType, void* pvParam)
{
	/** Removed for partner depot **/
}

// Game server notifying GC with its sync packet
void CServerGameDLL::EngineGotvSyncPacket(const CEngineGotvSyncPacket* pPkt)
{
	/** Removed for partner depot **/
}

// GOTV client attempt redirect over SDR
bool CServerGameDLL::OnEngineClientProxiedRedirect(uint64 ullClient, const char* adrProxiedRedirect, const char* adrRegular)
{
	/** Removed for partner depot **/
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: A user has had their network id setup and validated 
//-----------------------------------------------------------------------------
void CServerGameClients::NetworkIDValidated(const char* pszUserName, const char* pszNetworkID, CSteamID steamID)
{
	/** Removed for partner depot **/
}

void CServerGameDLL::GetMatchmakingTags( char *buf, size_t bufSize )
{
	// Additional "tags" that matchmaking wants to know about
	// AssertMsg( false, "fixme" );

	buf[0] = 0;

	if ( CommandLine()->FindParm( "-sv_load_test" ) )
	{
		Q_strncat( buf, ",sv_load_test", bufSize);
	}
}

void CServerGameDLL::ServerHibernationUpdate( bool bHibernating )
{
	m_bIsHibernating = bHibernating;
}

//-----------------------------------------------------------------------------
// Purpose: Called during a transition, to build a map adjacency list
//-----------------------------------------------------------------------------
void CServerGameDLL::BuildAdjacentMapList( void )
{
	// retrieve the pointer to the save data
	CSaveRestoreData *pSaveData = gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->levelInfo.connectionCount = BuildChangeList( pSaveData->levelInfo.levelList, MAX_LEVEL_CONNECTIONS );
}

//-----------------------------------------------------------------------------
// Purpose: Sanity-check to verify that a path is a relative path inside the game dir
// Taken From: engine/cmd.cpp
//-----------------------------------------------------------------------------
static bool IsValidPath( const char *pszFilename )
{
	if ( !pszFilename )
	{
		return false;
	}

	if ( Q_strlen( pszFilename ) <= 0    ||
		 Q_IsAbsolutePath( pszFilename ) || // to protect absolute paths
		 Q_strstr( pszFilename, ".." ) )    // to protect relative paths
	{
		return false;
	}

	return true;
}

static void ValidateMOTDFilename( IConVar *pConVar, const char *oldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( !IsValidPath( var.GetString() ) )
	{
		var.SetValue( var.GetDefault() );
	}
}

static ConVar motdfile( "motdfile", "motd.txt", FCVAR_RELEASE, "The MOTD file to load.", ValidateMOTDFilename );
static ConVar hostfile( "hostfile", "host.txt", FCVAR_RELEASE, "The HOST file to load.", ValidateMOTDFilename );
void LoadMOTDFile( const char *stringname, ConVar *pConvarFilename )
{
	char data[2048];

	int length = filesystem->Size( pConvarFilename->GetString(), "GAME" );
	if ( length <= 0 || length >= (sizeof(data)-1) )
	{
		DevMsg("Invalid file size for %s\n", pConvarFilename->GetString() );
		return;
	}

	FileHandle_t hFile = filesystem->Open( pConvarFilename->GetString(), "rb", "GAME" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;

	filesystem->Read( data, length, hFile );
	filesystem->Close( hFile );

	data[length] = 0;

	g_pStringTableInfoPanel->AddString( CBaseEntity::IsServer(), stringname, length+1, data );
}

//extern ConVar sv_server_graphic1;
//extern ConVar sv_server_graphic2;

void LoadServerImageFile( const char *stringname )
{
	char data[ 1 << 14 ];	// MAX_USERDATA_SIZE

	if ( stringname && stringname[0] )
	{
		int length = filesystem->Size( stringname, "GAME" );
		if ( length <= 0 || length >= ( sizeof( data )-1 ) )
		{
			DevMsg( "Invalid file size for %s\n", stringname );
			return;
		}

		FileHandle_t hFile = filesystem->Open( stringname, "rb", "GAME" );
		if ( hFile == FILESYSTEM_INVALID_HANDLE )
			return;

		filesystem->Read( data, length, hFile );
		filesystem->Close( hFile );

		g_pStringTableInfoPanel->AddString( CBaseEntity::IsServer(), stringname, length, data );
	}
}


void CServerGameDLL::LoadMessageOfTheDay()
{
	LoadMOTDFile( "motd", &motdfile );
	LoadMOTDFile( "hostfile", &hostfile );
	//LoadServerImageFile( sv_server_graphic1.GetString() );
	//LoadServerImageFile( sv_server_graphic2.GetString() );
}

PublishedFileId_t CServerGameDLL::GetUGCMapFileID( const char* szMapPath )
{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	return DedicatedServerWorkshop().GetUGCMapPublishedFileID( szMapPath );
#else
	return 0;
#endif
}

bool CServerGameDLL::GetNewestSubscribedFiles( void )
{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	DedicatedServerWorkshop().GetNewestSubscribedFiles();
#endif
	return true;
}

void CServerGameDLL::UpdateUGCMap( PublishedFileId_t id )
{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	DedicatedServerWorkshop().CheckForNewVersion( id );
#endif
}

bool CServerGameDLL::HasPendingMapDownloads( void ) const
{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	return DedicatedServerWorkshop().HasPendingMapDownloads();
#else
	return false;
#endif
}

// Pure server validation failed for the given client, client supplied
// data is included in the payload
void CServerGameDLL::OnPureServerFileValidationFailure(edict_t* edictClient, const char* path, const char* fileName, uint32 crc, int32 hashType, int32 len, int packNumber, int packFileID)
{
	/** Removed for partner depot **/
}

// keeps track of which chapters the user has unlocked
ConVar sv_unlockedchapters( "sv_unlockedchapters", "1", FCVAR_ARCHIVE );

//-----------------------------------------------------------------------------
// Purpose: Updates which chapters are unlocked
//-----------------------------------------------------------------------------
void UpdateChapterRestrictions( const char *mapname )
{
	// look at the chapter for this map
	char chapterTitle[64];
	chapterTitle[0] = 0;
	for ( int i = 0; i < ARRAYSIZE(gTitleComments); i++ )
	{
		if ( !Q_strnicmp( mapname, gTitleComments[i].pBSPName, strlen(gTitleComments[i].pBSPName) ) )
		{
			// found
			Q_strncpy( chapterTitle, gTitleComments[i].pTitleName, sizeof( chapterTitle ) );
			int j = 0;
			while ( j < 64 && chapterTitle[j] )
			{
				if ( chapterTitle[j] == '\n' || chapterTitle[j] == '\r' )
					chapterTitle[j] = 0;
				else
					j++;
			}

			break;
		}
	}

	if ( !chapterTitle[0] )
		return;

	// make sure the specified chapter title is unlocked
	strlwr( chapterTitle );
	
	// Get our active mod directory name
	char modDir[MAX_PATH];
	if ( UTIL_GetModDir( modDir, sizeof(modDir) ) == false )
		return;

	char chapterNumberPrefix[64];
	Q_snprintf(chapterNumberPrefix, sizeof(chapterNumberPrefix), "#%s_chapter", modDir);

	const char *newChapterNumber = strstr( chapterTitle, chapterNumberPrefix );
	if ( newChapterNumber )
	{
		// cut off the front
		newChapterNumber += strlen( chapterNumberPrefix );
		char newChapter[32];
		Q_strncpy( newChapter, newChapterNumber, sizeof(newChapter) );

		// cut off the end
		char *end = strstr( newChapter, "_title" );
		if ( end )
		{
			*end = 0;
		}

		int nNewChapter = atoi( newChapter );

		// HACK: HL2 added a zany chapter "9a" which wreaks
		//       havoc in this stupid atoi-based chapter code.
		if ( !Q_stricmp( modDir, "hl2" ) )
		{
			if ( !Q_stricmp( newChapter, "9a" ) )
			{
				nNewChapter = 10;
			}
			else if ( nNewChapter > 9 )
			{
				nNewChapter++;
			}
		}

		// ok we have the string, see if it's newer
		const char *unlockedChapter = sv_unlockedchapters.GetString();
		int nUnlockedChapter = atoi( unlockedChapter );

		if ( nUnlockedChapter < nNewChapter )
		{
			// ok we're at a higher chapter, unlock
			sv_unlockedchapters.SetValue( nNewChapter );

			// HACK: Call up through a better function than this? 7/23/07 - jdw
			if ( IsGameConsole() )
			{
				engine->ServerCommand( "host_writeconfig\n" );
			}
		}

		g_nCurrentChapterIndex = nNewChapter;
	}
}

//-----------------------------------------------------------------------------
// Precaches a vgui screen overlay material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName )
{
	Assert( pMaterialName && pMaterialName[0] );
	g_pStringTableMaterials->AddString( CBaseEntity::IsServer(), pMaterialName );
}


//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName )
{
	if (pMaterialName)
	{
		int nIndex = g_pStringTableMaterials->FindStringIndex( pMaterialName );
		
		if (nIndex != INVALID_STRING_INDEX )
		{
			return nIndex;
		}
		else
		{
			DevMsg("Warning! GetMaterialIndex: couldn't find material %s\n ", pMaterialName );
			return 0;
		}
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts a previously precached material index into a string
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nMaterialIndex )
{
	return g_pStringTableMaterials->GetString( nMaterialIndex );
}


//-----------------------------------------------------------------------------
// Precaches a vgui screen overlay material
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName )
{
	Assert( pParticleSystemName && pParticleSystemName[0] );
	return g_pStringTableParticleEffectNames->AddString( CBaseEntity::IsServer(), pParticleSystemName );
}

void PrecacheParticleFileAndSystems( const char *pParticleSystemFile )
{
	g_pParticleSystemMgr->ShouldLoadSheets( true );
	g_pParticleSystemMgr->ReadParticleConfigFile( pParticleSystemFile, true, false );
	g_pParticleSystemMgr->DecommitTempMemory();

	Assert( pParticleSystemFile && pParticleSystemFile[0] );
	g_pStringTableExtraParticleFiles->AddString( CBaseEntity::IsServer(), pParticleSystemFile );

	CUtlVector<CUtlString> systems;
	g_pParticleSystemMgr->GetParticleSystemsInFile( pParticleSystemFile, &systems );

	int nCount = systems.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		PrecacheParticleSystem( systems[i] );
	}
}

void CServerGameDLL::PrecacheParticleSystemFile( const char *pParticleSystemFile )
{
	PrecacheParticleFileAndSystems( pParticleSystemFile );
}

// this appears to be DOTA specific and therefore bShouldPreload and bShouldAutocache are insignificant parameters
void PrecacheGameSoundsFile( const char *pSoundFile )
{
	soundemitterbase->AddSoundsFromFile( pSoundFile, true, false );
	SoundSystemPreloadSounds();
}

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetParticleSystemIndex( const char *pParticleSystemName )
{
	if ( pParticleSystemName )
	{
		int nIndex = g_pStringTableParticleEffectNames->FindStringIndex( pParticleSystemName );
		if (nIndex != INVALID_STRING_INDEX )
			return nIndex;

		DevWarning("Server: Missing precache for particle system \"%s\"!\n", pParticleSystemName );
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts a previously precached material index into a string
//-----------------------------------------------------------------------------
const char *GetParticleSystemNameFromIndex( int nMaterialIndex )
{
	if ( nMaterialIndex < g_pStringTableParticleEffectNames->GetMaxStrings() )
		return g_pStringTableParticleEffectNames->GetString( nMaterialIndex );
	return "error";
}


//-----------------------------------------------------------------------------
// Precaches an effect (used by DispatchEffect)
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pEffectName )
{
	Assert( pEffectName && pEffectName[0] );
	g_pStringTableEffectDispatch->AddString( CBaseEntity::IsServer(), pEffectName );
}

//-----------------------------------------------------------------------------
// Converts a previously precached effect into an index
//-----------------------------------------------------------------------------
int GetEffectIndex( const char *pEffectName )
{
	if ( pEffectName )
	{
		int nIndex = g_pStringTableEffectDispatch->FindStringIndex( pEffectName );
		if (nIndex != INVALID_STRING_INDEX )
			return nIndex;

		DevWarning("Server: Missing precache for effect \"%s\"!\n", pEffectName );
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts a previously precached effect index into a string
//-----------------------------------------------------------------------------
const char *GetEffectNameFromIndex( int nEffectIndex )
{
	if ( nEffectIndex < g_pStringTableEffectDispatch->GetMaxStrings() )
		return g_pStringTableEffectDispatch->GetString( nEffectIndex );
	return "error";
}

//-----------------------------------------------------------------------------
// Precaches a movie
//-----------------------------------------------------------------------------
void PrecacheMovie( const char *pMovieName )
{
	Assert ( CBaseEntity::IsPrecacheAllowed() );

	Assert( pMovieName && pMovieName[0] );
	g_pStringTableMovies->AddString( CBaseEntity::IsServer(), pMovieName );
}

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMovieIndex( const char *pMovieName )
{
	if ( pMovieName )
	{
		int nIndex = g_pStringTableMovies->FindStringIndex( pMovieName );
		if ( nIndex != INVALID_STRING_INDEX )
		{
			return nIndex;
		}
		else
		{
			DevMsg( "Warning! GetMovieIndex: couldn't find movie %s\n ", pMovieName );
			return 0;
		}
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts a previously precached movie index into a string
//-----------------------------------------------------------------------------
const char *GetMovieNameFromIndex( int nMovieIndex )
{
	return g_pStringTableMovies->GetString( nMovieIndex );
}

//-----------------------------------------------------------------------------
// Returns true if host_thread_mode is set to non-zero (and engine is running in threaded mode)
//-----------------------------------------------------------------------------
bool IsEngineThreaded()
{
	if ( g_pcv_ThreadMode )
	{
		return g_pcv_ThreadMode->GetBool();
	}
	return false;
}

class CServerGameEnts : public IServerGameEnts
{
public:
	virtual void			MarkEntitiesAsTouching( edict_t *e1, edict_t *e2 );
	virtual void			FreeContainingEntity( edict_t * ); 
	virtual edict_t*		BaseEntityToEdict( CBaseEntity *pEnt );
	virtual CBaseEntity*	EdictToBaseEntity( edict_t *pEdict );
	virtual void			CheckTransmit( CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts );
	virtual void			PrepareForFullUpdate( edict_t *pEdict );
};
EXPOSE_SINGLE_INTERFACE(CServerGameEnts, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);

//-----------------------------------------------------------------------------
// Purpose: Marks entities as touching
// Input  : *e1 - 
//			*e2 - 
//-----------------------------------------------------------------------------
void CServerGameEnts::MarkEntitiesAsTouching( edict_t *e1, edict_t *e2 )
{
	CBaseEntity *entity = GetContainingEntity( e1 );
	CBaseEntity *entityTouched = GetContainingEntity( e2 );
	if ( entity && entityTouched )
	{
		// HACKHACK: UNDONE: Pass in the trace here??!?!?
		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (entity->GetAbsOrigin() + entityTouched->GetAbsOrigin()) * 0.5;
		entity->PhysicsMarkEntitiesAsTouching( entityTouched, tr );
	}
}

void CServerGameEnts::FreeContainingEntity( edict_t *e )
{
	::FreeContainingEntity(e);
}

edict_t* CServerGameEnts::BaseEntityToEdict( CBaseEntity *pEnt )
{
	if ( pEnt )
		return pEnt->edict();
	else
		return NULL;
}

CBaseEntity* CServerGameEnts::EdictToBaseEntity( edict_t *pEdict )
{
	if ( pEdict )
		return CBaseEntity::Instance( pEdict );
	else
		return NULL;
}


/* Yuck.. ideally this would be in CServerNetworkProperty's header, but it requires CBaseEntity and
// inlining it gives a nice speedup.
inline void CServerNetworkProperty::CheckTransmit( CCheckTransmitInfo *pInfo )
{
	// If we have a transmit proxy, let it hook our ShouldTransmit return value.
	if ( m_pTransmitProxy )
	{
		nShouldTransmit = m_pTransmitProxy->ShouldTransmit( pInfo, nShouldTransmit );
	}

	if ( m_pOuter->ShouldTransmit( pInfo ) )
	{
		m_pOuter->SetTransmit( pInfo );
	}
} */

ConVar sv_occlude_players( "sv_occlude_players", "1", FCVAR_RELEASE /*, "Do additional traces on the server to hide players occluded by large brushes from each other - anti-cheat"*/ );

// A bitmap which records whether players were occluded under sv_occlude_players
// rules. This is so other code areas which are interested in player-player
// PVS can also check what the occlusion rules said.
static CBitVec< (MAX_PLAYERS+1)*(MAX_PLAYERS+1) > g_occludePlayersCacheBitVec;

bool WasPlayerOccluded( int fromplayer, int toplayer )
{
	if ( fromplayer > MAX_PLAYERS || toplayer > MAX_PLAYERS )
	{
		Error( "Player indexes too large: %d %d", fromplayer, toplayer );
		return false;
	}

	const int index = fromplayer*( MAX_PLAYERS + 1 )+ toplayer;
	return g_occludePlayersCacheBitVec.IsBitSet(index) ;
}

void SetPlayerOccluded( int index, bool newvalue )
{
	if ( index > (MAX_PLAYERS + 1 ) * ( MAX_PLAYERS + 1 ) )
	{
		Error( "Player indexes too large: %d %d", index % (MAX_PLAYERS+1), index / (MAX_PLAYERS+1) );
		return;
	}

	g_occludePlayersCacheBitVec.Set( index, newvalue );

}


/*
AABB_t GetOcclusionBounds( CBasePlayer *pPlayer )
{
	AABB_t aabb;
	// does server run at 30 fps? can player run faster than 260 (when falling? when climbing ladders?)
	// what to do about falling? (if we always expand down, we'll miss a lot of cases due to looking from under floor)
	Vector vUncertainty = Vector( CS_PLAYER_SPEED_RUN / 30.0f, CS_PLAYER_SPEED_RUN / 30.0f, 0 );
	if ( ( pPlayer->GetFlags() & ( FL_DUCKING | FL_ANIMDUCKING ) ) == ( FL_DUCKING | FL_ANIMDUCKING ) )
	{
		aabb.m_vMinBounds = VEC_DUCK_HULL_MIN - vUncertainty;
		aabb.m_vMaxBounds = VEC_DUCK_HULL_MAX + vUncertainty;
	}
	else
	{
		aabb.m_vMinBounds = VEC_HULL_MIN - vUncertainty;
		aabb.m_vMaxBounds = VEC_HULL_MAX + vUncertainty;
	}
	
	return aabb;
}
*/

AABB_t GetOcclusionBounds( CBaseEntity *pItem )
{
	AABB_t aabb;
	CCollisionProperty *pColl = pItem->CollisionProp();
	pColl->WorldSpaceAABB( &aabb.m_vMinBounds, &aabb.m_vMaxBounds );
	return aabb;
}


AABB_t GetCameraBounds( CBasePlayer *pPlayer )
{
	Assert( !pPlayer->IsDead() && pPlayer->GetObserverMode() != OBS_MODE_DEATHCAM );
	Vector vPlayerOrigin = pPlayer->GetAbsOrigin(); // pPlayer->GetViewOffset() varies between VEC_DUC_VIEW and VEC_VIEW for an alive player
	AABB_t aabb;
	float flCameraMargin = occlusion_test_camera_margins.GetFloat();
	float flJumpMargin = occlusion_test_jump_margin.GetFloat();
	aabb.m_vMinBounds = vPlayerOrigin + VEC_DUCK_VIEW - Vector( flCameraMargin, flCameraMargin, 0 );
	aabb.m_vMaxBounds = vPlayerOrigin + VEC_VIEW + Vector( flCameraMargin, flCameraMargin, flJumpMargin );
	return aabb;
}

void CServerGameEnts::CheckTransmit( CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts )
{
	// NOTE: for speed's sake, this assumes that all networkables are CBaseEntities and that the edict list
	// is consecutive in memory. If either of these things change, then this routine needs to change, but
	// ideally we won't be calling any virtual from this routine. This speedy routine was added as an
	// optimization which would be nice to keep.
	edict_t *pBaseEdict = gpGlobals->pEdicts;

	CBaseEntity *pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
	Assert( pRecipientEntity && pRecipientEntity->IsPlayer() );
	if ( !pRecipientEntity )
		return;
	
	MDLCACHE_CRITICAL_SECTION();
	CBasePlayer *pRecipientPlayer = static_cast<CBasePlayer*>( pRecipientEntity );
	int nRecipientPlayerEntIndexMulNumPlayers = pRecipientPlayer->entindex() * ( MAX_PLAYERS + 1 );

	// If the player is freshly spawned then give it a small window in
	// which it sees all entities so that it gets their basic information.
	const bool bIsFreshlySpawned = pRecipientPlayer->GetInitialSpawnTime()+3.0f > gpGlobals->curtime;
	const int skyBoxArea = pRecipientPlayer->m_Local.m_skybox3d.area;
#ifndef _GAMECONSOLE
	const bool bIsHLTV = pRecipientPlayer->IsHLTV();
#if defined( REPLAY_ENABLED )
	const bool bIsReplay = pRecipientPlayer->IsReplay();
#else
	const bool bIsReplay = false;
#endif

	// m_pTransmitAlways must be set if HLTV client
	Assert( bIsHLTV == ( pInfo->m_pTransmitAlways != NULL) ||
		    bIsReplay == ( pInfo->m_pTransmitAlways != NULL) );
#endif
	IEngineTrace::CAutoSuspendOcclusionTests autoSuspender( enginetrace ); // suspend the async engine traces for the time being
	for ( int i=0; i < nEdicts; i++ )
	{
		int iEdict = pEdictIndices[i];
#ifdef _GAMECONSOLE
		if ( i < nEdicts-1 )
		{
			PREFETCH360(&pBaseEdict[pEdictIndices[i+1]],0);
		}
#endif

		edict_t *pEdict = &pBaseEdict[iEdict];
		int nFlags = pEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);

		// entity needs no transmit
		if ( nFlags & FL_EDICT_DONTSEND )
			continue;
		PREFETCH360(pEdict->GetUnknown(),0);

		// entity is already marked for sending
		if ( pInfo->m_pTransmitEdict->Get( iEdict ) )
			continue;
		
		if ( nFlags & FL_EDICT_ALWAYS )
		{
			// FIXME: Hey! Shouldn't this be using SetTransmit so as 
			// to also force network down dependent entities?
			while ( true )
			{
				// mark entity for sending
				pInfo->m_pTransmitEdict->Set( iEdict );
	
#ifndef _GAMECONSOLE
				if ( bIsHLTV || bIsReplay )
				{
					pInfo->m_pTransmitAlways->Set( iEdict );
				}
#endif	
				CServerNetworkProperty *pEnt = static_cast<CServerNetworkProperty*>( pEdict->GetNetworkable() );
				if ( !pEnt )
					break;

				CServerNetworkProperty *pParent = pEnt->GetNetworkParent();
				if ( !pParent )
					break;

				pEdict = pParent->edict();
				iEdict = pParent->entindex();
			}
			continue;
		}

		// FIXME: Would like to remove all dependencies
		CBaseEntity *pEnt = ( CBaseEntity * )pEdict->GetUnknown();
		Assert( dynamic_cast< CBaseEntity* >( pEdict->GetUnknown() ) == pEnt );

		if ( pEnt->entindex() <= MAX_PLAYERS )
		{
			SetPlayerOccluded( pEnt->entindex() + nRecipientPlayerEntIndexMulNumPlayers, false );
		}

		if ( nFlags == FL_EDICT_FULLCHECK )
		{
			// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
			nFlags = pEnt->ShouldTransmit( pInfo );

			Assert( !(nFlags & FL_EDICT_FULLCHECK) );

			if ( nFlags & FL_EDICT_ALWAYS )
			{
				pEnt->SetTransmit( pInfo, true );
				continue;
			}	
		}

		// don't send this entity
		if ( !( nFlags & FL_EDICT_PVSCHECK ) )
			continue;

		CServerNetworkProperty *netProp = static_cast<CServerNetworkProperty*>( pEdict->GetNetworkable() );

#ifndef _GAMECONSOLE
		if ( bIsHLTV || bIsReplay )
		{
			// for the HLTV/Replay we don't cull against PVS
			if ( netProp->AreaNum() == skyBoxArea )
			{
				pEnt->SetTransmit( pInfo, true );
			}
			else
			{
				pEnt->SetTransmit( pInfo, false );
			}
			continue;
		}
#endif

		// Always send entities in the player's 3d skybox.
		// Sidenote: call of AreaNum() ensures that PVS data is up to date for this entity
		bool bSameAreaAsSky = netProp->AreaNum() == skyBoxArea;
		if ( bSameAreaAsSky )
		{
			pEnt->SetTransmit( pInfo, true );
			continue;
		}

		CBasePlayer *pPlayer = dynamic_cast< CBasePlayer* >( pEnt );

#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
		// Team Lead in gungame should be always visible in counter-strike. Yes, even if he's out of PVS. Yes, even if he's behind the wall.
		if ( pPlayer && static_cast< CCSPlayer* >( pPlayer )->m_isCurrentGunGameTeamLeader )
		{ // do not check PVS or occlusion for a gun game team leader
			pEnt->SetTransmit( pInfo, false );
			continue;
		}
#endif

		bool bInPVS;
		if ( pPlayer && ( pPlayer->GetAbsOrigin() - pRecipientPlayer->GetAbsOrigin() ).LengthSqr() < Sqr( pvs_min_player_distance.GetFloat() ) )
			bInPVS = true;
		else
			bInPVS = netProp->IsInPVS( pInfo );

		CServerNetworkProperty *check = netProp->GetNetworkParent();

		if ( bInPVS && sv_occlude_players.GetBool() )
		{
			if ( pPlayer )
			{
				if ( pRecipientPlayer->ShouldCheckOcclusion( pPlayer ) )
				{
					// if ( IsDebug() && ShouldHideAllPlayers() ) bInPVS = false;

					// the other player is from an opposing team, otherwise the iBaseResult would be "Always Transmit"
					AABB_t playerBounds = GetOcclusionBounds( pPlayer );
					AABB_t recipientBounds  = GetCameraBounds( pRecipientPlayer );

					int nOcclusionKey = pPlayer->entindex() + nRecipientPlayerEntIndexMulNumPlayers;
					if ( enginetrace->IsFullyOccluded( nOcclusionKey, playerBounds, recipientBounds, g_pCascadeLight ? ( g_pCascadeLight->GetShadowDirection() * occlusion_test_shadow_length.GetFloat() ) : vec3_origin ) )
					{
						bInPVS = false; // forget that the player is in PVS. If it's fully occluded, we just act as if he's outside of PVS.
						SetPlayerOccluded( nOcclusionKey, true );
					}

					// if ( dump_occlusion_probes.GetBool() ) Msg( "Occlusion probe %s - %s: %s\n", pRecipientPlayer->GetPlayerName(), pPlayer->GetPlayerName(), bInPVS ? "miss" : "occluded" );
				}
			}
#if defined( CSTRIKE_DLL )
			else if ( CServerNetworkProperty *pNetworkParent = check )
			{
				do
				{
					if ( CBaseEntity *pParent = pNetworkParent->GetBaseEntity() )
					{
						if ( pParent->IsPlayer() )
						{
							// Is the parent-owner-player marked or not market for transmission? If not, let's not transmit its children
							if ( !pInfo->m_pTransmitEdict->Get( pParent->entindex() ) )
							{
								bInPVS = false;
								check = NULL; // don't check anything, just don't transmit this entity
							}
							/*
							if ( CBasePlayer *pParentPlayer = dynamic_cast< CBasePlayer* >( pParent ) )
							{
								if ( pRecipientPlayer->ShouldCheckOcclusion( pParentPlayer ) )
								{
									AABB_t entityBounds = GetOcclusionBounds( pEnt );
									AABB_t recipientBounds = GetCameraBounds( pRecipientPlayer );

									if ( IsFullyOccluded_WithShadow( entityBounds, recipientBounds ) )
									{
										bInPVS = false; // the weapon is fully occluded
									}
									if ( dump_occlusion_probes.GetBool() )
									{
										Msg( "Occlusion probe %s - item %s of %s: %s\n", pRecipientPlayer->GetPlayerName(), pEnt->GetDebugName(), pParentPlayer->GetPlayerName(), bInPVS ? "miss" : "occluded" );
									}
								}
							}
							*/
							
							break;
						}
					}
					pNetworkParent = pNetworkParent->GetNetworkParent();
				} while ( pNetworkParent );
			}
#endif
		}

		// If a player has just been spawned, forcibly send it for a brief
		// window so that everyone gets its basic info.
		const bool bFreshlySpawnedTargetPlayer = pPlayer != NULL && pPlayer->GetInitialSpawnTime()+3.0f > gpGlobals->curtime;
		if ( bInPVS || bIsFreshlySpawned || bFreshlySpawnedTargetPlayer || sv_force_transmit_ents.GetBool() )
		{
			// only send if entity is in PVS
			pEnt->SetTransmit( pInfo, false );
			continue;
		}

		// If the entity is marked "check PVS" but it's in hierarchy, walk up the hierarchy looking for the
		//  for any parent which is also in the PVS.  If none are found, then we don't need to worry about sending ourself
		CBaseEntity *orig = pEnt;

		// BUG BUG:  I think it might be better to build up a list of edict indices which "depend" on other answers and then
		// resolve them in a second pass.  Not sure what happens if an entity has two parents who both request PVS check?
        while ( check )
		{
			int checkIndex = check->entindex();

			// Parent already being sent
			if ( pInfo->m_pTransmitEdict->Get( checkIndex ) )
			{
				orig->SetTransmit( pInfo, true );
				break;
			}

			edict_t *checkEdict = check->edict();
			int checkFlags = checkEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);
			if ( checkFlags & FL_EDICT_DONTSEND )
				break;

			if ( checkFlags & FL_EDICT_ALWAYS )
			{
				orig->SetTransmit( pInfo, true );
				break;
			}

			if ( checkFlags == FL_EDICT_FULLCHECK )
			{
				// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
				CBaseEntity *pCheckEntity = check->GetBaseEntity();
				nFlags = pCheckEntity->ShouldTransmit( pInfo );
				Assert( !(nFlags & FL_EDICT_FULLCHECK) );
				if ( nFlags & FL_EDICT_ALWAYS )
				{
					pCheckEntity->SetTransmit( pInfo, true );
					orig->SetTransmit( pInfo, true );
				}
				break;
			}

			if ( checkFlags & FL_EDICT_PVSCHECK )
			{
				// Check pvs
				check->RecomputePVSInformation();
				bool bMoveParentInPVS = check->IsInPVS( pInfo );
				if ( bMoveParentInPVS )
				{
					orig->SetTransmit( pInfo, true );
					break;
				}
			}

			// Continue up chain just in case the parent itself has a parent that's in the PVS...
			check = check->GetNetworkParent();
		}
	}

//	Msg("A:%i, N:%i, F: %i, P: %i\n", always, dontSend, fullCheck, PVS );
}

//-----------------------------------------------------------------------------
// Purpose: called before a full update, so the server can flush any custom PVS info, etc
//-----------------------------------------------------------------------------
void CServerGameEnts::PrepareForFullUpdate( edict_t *pEdict )
{
	CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );

	Assert( pEntity && pEntity->IsPlayer() );

	if ( !pEntity )
		return;

	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( pEntity );
	pPlayer->PrepareForFullUpdate();
}


CServerGameClients g_ServerGameClients;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerGameClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS, g_ServerGameClients );


//-----------------------------------------------------------------------------
// Purpose: called when a player tries to connect to the server
// Input  : *pEdict - the new player
//			char *pszName - the players name
//			char *pszAddress - the IP address of the player
//			reject - output - fill in with the reason why
//			maxrejectlen -- sizeof output buffer
//			the player was not allowed to connect.
// Output : Returns TRUE if player is allowed to join, FALSE if connection is denied.
//-----------------------------------------------------------------------------
bool CServerGameClients::ClientConnect( edict_t *pEdict, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	if ( !g_pGameRules )
		return false;
	
	return g_pGameRules->ClientConnected( pEdict, pszName, pszAddress, reject, maxrejectlen );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a player is fully active (i.e. ready to receive messages)
// Input  : *pEntity - the player
//-----------------------------------------------------------------------------
void CServerGameClients::ClientActive( edict_t *pEdict, bool bLoadGame )
{
	MDLCACHE_CRITICAL_SECTION();
	
	::ClientActive( pEdict, bLoadGame );

	// If we just loaded from a save file, call OnRestore on valid entities
	EndRestoreEntities();

	if ( gpGlobals->eLoadType != MapLoad_LoadGame )
	{
		// notify all entities that the player is now in the game
		for ( CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity != NULL; pEntity = gEntList.NextEnt(pEntity) )
		{
			pEntity->PostClientActive();
		}
	}

	// Tell the sound controller to check looping sounds
	CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	CSoundEnvelopeController::GetController().CheckLoopingSoundsForPlayer( pPlayer );
	SceneManager_ClientActive( pPlayer );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a player is fully connect ( initial baseline entities have been received )
// Input  : *pEntity - the player
//-----------------------------------------------------------------------------
void CServerGameClients::ClientFullyConnect( edict_t *pEdict )
{
	::ClientFullyConnect( pEdict );
}


//-----------------------------------------------------------------------------
// Purpose: called when a player disconnects from a server
// Input  : *pEdict - the player
//-----------------------------------------------------------------------------
void CServerGameClients::ClientDisconnect( edict_t *pEdict )
{
	extern bool	g_fGameOver;

	CBasePlayer *player = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	if ( player )
	{
		if ( !g_fGameOver )
		{
			player->SetMaxSpeed( 0.0f );

			CSound *pSound;
			pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEdict ) );
			{
				// since this client isn't around to think anymore, reset their sound. 
				if ( pSound )
				{
					pSound->Reset();
				}
			}

		// since the edict doesn't get deleted, fix it so it doesn't interfere.
			player->RemoveFlag( FL_AIMTARGET ); // don't attract autoaim
			player->AddFlag( FL_DONTTOUCH );	// stop it touching anything
			player->AddFlag( FL_NOTARGET );	// stop NPCs noticing it
			player->AddSolidFlags( FSOLID_NOT_SOLID );		// nonsolid

			if ( g_pGameRules )
			{
				g_pGameRules->ClientDisconnected( pEdict );
				#ifndef _GAMECONSOLE
				gamestats->Event_PlayerDisconnected( player );
				#endif
			}
		}

		// Make sure all Untouch()'s are called for this client leaving
		CBaseEntity::PhysicsRemoveTouchedList( player );
		CBaseEntity::PhysicsRemoveGroundList( player );

#if !defined( NO_ENTITY_PREDICTION )
		// Make sure anything we "own" is simulated by the server from now on
		player->ClearPlayerSimulationList();
#endif
	}
}

void CServerGameClients::ClientPutInServer( edict_t *pEntity, const char *playername )
{
	if ( g_pClientPutInServerOverride )
		g_pClientPutInServerOverride( pEntity, playername );
	else
		::ClientPutInServer( pEntity, playername );

	CBasePlayer *pPlayer = ToBasePlayer( GetContainingEntity( pEntity ) );
	if ( pPlayer )
	{
		bool bIsSplitScreenPlayer = engine->IsSplitScreenPlayer( pPlayer->entindex() );
		CBasePlayer *pAttachedTo = NULL;
		if ( bIsSplitScreenPlayer )
		{
			pAttachedTo = (CBasePlayer *)::GetContainingEntity( engine->GetSplitScreenPlayerAttachToEdict( pPlayer->entindex() ) );
		}

		pPlayer->SetSplitScreenPlayer( bIsSplitScreenPlayer, pAttachedTo );

		pPlayer->SetCrossPlayPlatform( engine->GetClientCrossPlayPlatform( pPlayer->entindex() ) );
	}
}

void CServerGameClients::ClientCommand( edict_t *pEntity, const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetContainingEntity( pEntity ) );
	::ClientCommand( pPlayer, args );
}

//-----------------------------------------------------------------------------
// Purpose: called after the player changes userinfo - gives dll a chance to modify 
//			it before it gets sent into the rest of the engine->
// Input  : *pEdict - the player
//			*infobuffer - their infobuffer
//-----------------------------------------------------------------------------
void CServerGameClients::ClientSettingsChanged( edict_t *pEdict )
{
	// Is the client spawned yet?
	if ( !pEdict->GetUnknown() )
		return;

	CBasePlayer *player = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	
	if ( !player )
		return;

	bool bAllowNetworkingClientSettingsChange = g_pGameRules->IsConnectedUserInfoChangeAllowed( player );
	if ( bAllowNetworkingClientSettingsChange )
	{

#define QUICKGETCVARVALUE(v) (engine->GetClientConVarValue( player->entindex(), v ))

	// get network setting for prediction & lag compensation
	
	// Unfortunately, we have to duplicate the code in cdll_bounded_cvars.cpp here because the client
	// doesn't send the virtualized value up (because it has no way to know when the virtualized value
	// changes). Possible todo: put the responsibility on the bounded cvar to notify the engine when
	// its virtualized value has changed.		

	//
	// Most important part in this code is to keep server computations same as client
	// side evaluation of convar string values. That is why the server parses values
	// as floats, then casts them to int to ensure that it yields the same result as
	// when the client sets convar string value and later checks it with GetInt or
	// GetBool accessors that at the moment of fixing this code were parsing original
	// string value as a float and then casting it to an int.
	//
	
	float flUpdateRateValue = Q_atof( QUICKGETCVARVALUE("cl_updaterate") );
	if ( !player->IsHLTV() )
	{
		static const ConVar *pMinUpdateRate = g_pCVar->FindVar( "sv_minupdaterate" );
		static const ConVar *pMaxUpdateRate = g_pCVar->FindVar( "sv_maxupdaterate" );
		if ( pMinUpdateRate && pMaxUpdateRate )
			flUpdateRateValue = clamp( flUpdateRateValue, pMinUpdateRate->GetFloat(), pMaxUpdateRate->GetFloat() );
	}
	player->m_nUpdateRate = ( int ) flUpdateRateValue;

	bool useInterpolation = !player->IsBot() || ( int( Q_atof( QUICKGETCVARVALUE("cl_interpolate") ) ) != 0 );
	if ( useInterpolation )
	{
		float flLerpRatio = Q_atof( QUICKGETCVARVALUE("cl_interp_ratio") );
		if ( flLerpRatio == 0 )
			flLerpRatio = 1.0f;
		float flLerpAmount = Q_atof( QUICKGETCVARVALUE("cl_interp") );

		static const ConVar *pMin = g_pCVar->FindVar( "sv_client_min_interp_ratio" );
		static const ConVar *pMax = g_pCVar->FindVar( "sv_client_max_interp_ratio" );
		if ( pMin && pMax && pMin->GetFloat() != -1 )
		{
			flLerpRatio = clamp( flLerpRatio, pMin->GetFloat(), pMax->GetFloat() );
		}
		else
		{
			if ( flLerpRatio == 0 )
				flLerpRatio = 1.0f;
		}
		// #define FIXME_INTERP_RATIO
		player->m_fLerpTime = MAX( flLerpAmount, flLerpRatio / flUpdateRateValue );
	}
	else
	{
		player->m_fLerpTime = 0.0f;
	}
	
#if !defined( NO_ENTITY_PREDICTION )
	bool usePrediction = ( int( Q_atof( QUICKGETCVARVALUE("cl_predict")) ) != 0 );

	if ( usePrediction )
	{
		player->m_bPredictionEnabled = usePrediction;
		player->m_bPredictWeapons  = ( int( Q_atof( QUICKGETCVARVALUE("cl_predictweapons")) ) != 0 );
		player->m_bLagCompensation = ( int( Q_atof( QUICKGETCVARVALUE("cl_lagcompensation")) ) != 0 );
	}
	else
#endif
	{
		player->m_bPredictionEnabled = false;
		player->m_bPredictWeapons  = false;
		player->m_bLagCompensation = false;
	}


	//HACKHACK: There's a bug in portal 2 splitscreen where the primary player will be set to non-predictive and the secondary player will be set to predictive.
	//	It's probably a better idea in the long run to have CBaseClient::ProcessSetConVar() in engine better handle splitscreen. But for now we're going for a surgical fix
	{
		//stomp attached splitscreen players to have the same prediction settings as the primary player
		CUtlVector< CHandle<CBasePlayer> > &splitscreenPlayers = player->GetSplitScreenPlayers();
		for( int i = 0; i < splitscreenPlayers.Count(); ++i )
		{
			CBasePlayer *pAttachedPlayer = splitscreenPlayers[i];
			if( pAttachedPlayer )
			{
				pAttachedPlayer->m_bPredictionEnabled = player->m_bPredictionEnabled;
				pAttachedPlayer->m_bPredictWeapons = player->m_bPredictWeapons;
				pAttachedPlayer->m_bLagCompensation = player->m_bLagCompensation;
			}
		}
	}
	

#undef QUICKGETCVARVALUE
	}

	g_pGameRules->ClientSettingsChanged( player );
}


#ifdef PORTAL
//-----------------------------------------------------------------------------
// Purpose: Runs CFuncAreaPortalBase::UpdateVisibility on each portal
// Input  : pAreaPortal - The Area portal to test for visibility from portals
// Output : int - 1 if any portal needs this area portal open, 0 otherwise.
//-----------------------------------------------------------------------------
int TestAreaPortalVisibilityThroughPortals ( CFuncAreaPortalBase* pAreaPortal, edict_t *pViewEntity, unsigned char *pvs, int pvssize  )
{
	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return 0;

	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();

	for ( int i = 0; i != iPortalCount; ++i )
	{
		CPortal_Base2D* pLocalPortal = pPortals[ i ];
		if ( pLocalPortal && pLocalPortal->IsActive() )
		{
			CPortal_Base2D* pRemotePortal = pLocalPortal->m_hLinkedPortal.Get();

			// Make sure this portal's linked portal is in the PVS before we add what it can see
			if ( pRemotePortal && pRemotePortal->IsActive() && pRemotePortal->NetworkProp() && 
				pRemotePortal->NetworkProp()->IsInPVS( pViewEntity, pvs, pvssize ) )
			{
				bool bIsOpenOnClient = true;
				float fovDistanceAdjustFactor = 1.0f;
				Vector portalOrg = pLocalPortal->GetAbsOrigin();
				CUtlVector< Vector > orgs;
				orgs.AddToTail( portalOrg );
				int iPortalNeedsThisPortalOpen = pAreaPortal->UpdateVisibility( orgs, fovDistanceAdjustFactor, bIsOpenOnClient );

				// Stop checking on success, this portal needs to be open
				if ( iPortalNeedsThisPortalOpen )
				{
					return iPortalNeedsThisPortalOpen;
				}
			}
		}
	}
	
	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
//  view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
//  entity's origin is used.  Either is offset by the m_vecViewOffset to get the eye position.
// From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
//  override the actual PAS or PVS values, or use a different origin.
// NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
// Input  : *pViewEntity - 
//			*pClient - 
//			**pvs - 
//			**pas - 
//-----------------------------------------------------------------------------
void CServerGameClients::ClientSetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char *pvs, int pvssize )
{
	Vector org;

	// Reset the PVS!!!
	engine->ResetPVS( pvs, pvssize );

	g_pToolFrameworkServer->PreSetupVisibility();

	// Find the client's PVS
	CBaseEntity *pVE = NULL;
	if ( pViewEntity )
	{
		pVE = GetContainingEntity( pViewEntity );
		// If we have a viewentity, it overrides the player's origin
		if ( pVE )
		{
			org = pVE->EyePosition();
			engine->AddOriginToPVS( org );
		}
	}

	float fovDistanceAdjustFactor = 1;

	CUtlVector< Vector > areaPortalOrigins;

	CBasePlayer *pPlayer = ( CBasePlayer * )GetContainingEntity( pClient );
	if ( pPlayer )
	{
		if ( !pVE )
		{
			org = pPlayer->EyePosition();
		}
		pPlayer->SetupVisibility( pVE, pvs, pvssize );
		UTIL_SetClientVisibilityPVS( pClient, pvs, pvssize );
		fovDistanceAdjustFactor = pPlayer->GetFOVDistanceAdjustFactorForNetworking();

		areaPortalOrigins.AddToTail( org );

		// Merge in areaportal "window" states from from split screen players by passing in the extra PVS origins!!!

		CUtlVector< CHandle< CBasePlayer > > &list = pPlayer->GetSplitScreenAndPictureInPicturePlayers();
		for ( int i = 0; i < list.Count(); ++i )
		{
			CBasePlayer *pl = list[ i ];
			if ( !pl )
			{
				continue;
			}

			org = pl->EyePosition();
			areaPortalOrigins.AddToTail( org );
		}
	}
	else
	{
		Warning( "ClientSetupVisibility: No entity for edict!\n" );
		areaPortalOrigins.AddToTail( org );
	}

	unsigned char portalBits[MAX_AREA_PORTAL_STATE_BYTES];
	memset( portalBits, 0, sizeof( portalBits ) );
	
	int portalNums[512];
	int isOpen[512];
	int iOutPortal = 0;

	for( unsigned short i = g_AreaPortals.Head(); i != g_AreaPortals.InvalidIndex(); i = g_AreaPortals.Next(i) )
	{
		CFuncAreaPortalBase *pCur = g_AreaPortals[i];

		bool bIsOpenOnClient = true;
		
		// Update our array of which portals are open and flush it if necessary.		
		portalNums[iOutPortal] = pCur->m_portalNumber;
		isOpen[iOutPortal] = pCur->UpdateVisibility( areaPortalOrigins, fovDistanceAdjustFactor, bIsOpenOnClient );

#ifdef PORTAL
		// If the client doesn't need this open, test if portals might need this area portal open
		if ( isOpen[iOutPortal] == 0 )
		{
			isOpen[iOutPortal] = TestAreaPortalVisibilityThroughPortals( pCur, pViewEntity, pvs, pvssize );
			bIsOpenOnClient |= ( isOpen[iOutPortal] != 0 );
		}
#endif

		++iOutPortal;
		if ( iOutPortal >= ARRAYSIZE( portalNums ) )
		{
			engine->SetAreaPortalStates( portalNums, isOpen, iOutPortal );
			iOutPortal = 0;
		}

		// Version 0 portals (ie: shipping Half-Life 2 era) are always treated as open
		// for purposes of the m_chAreaPortalBits array on the client.
		if ( pCur->m_iPortalVersion == 0 )
			bIsOpenOnClient = true;

		if ( bIsOpenOnClient )
		{
			if ( pCur->m_portalNumber < 0 )
				continue;
			else if ( pCur->m_portalNumber >= sizeof( portalBits ) * 8 )
				Error( "ClientSetupVisibility: portal number (%d) too large", pCur->m_portalNumber );
			else
				portalBits[pCur->m_portalNumber >> 3] |= (1 << (pCur->m_portalNumber & 7));
		}	
	}

	// Flush the remaining areaportal states.
	engine->SetAreaPortalStates( portalNums, isOpen, iOutPortal );

	// Update the area bits that get sent to the client.
	Assert( pPlayer );
	if ( pPlayer )
	{
		pPlayer->m_Local.UpdateAreaBits( pPlayer, portalBits );
	}

#ifdef PORTAL 
	// *After* the player's view has updated its area bits, add on any other areas seen by portals
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( pPlayer );
	if ( pPortalPlayer )
	{
		pPortalPlayer->UpdatePortalViewAreaBits( pvs, pvssize );
	}
#endif //PORTAL
}




//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *player - 
//			*buf - 
//			numcmds - 
//			totalcmds - 
//			dropped_packets - 
//			ignore - 
//			paused - 
// Output : float
//-----------------------------------------------------------------------------
#define CMD_MAXBACKUP 64

static ConVar sv_max_usercmd_move_magnitude( "sv_max_usercmd_move_magnitude", "1000", 0, "Maximum move magnitude that can be requested by client." );

float CServerGameClients::ProcessUsercmds( edict_t *player, bf_read *buf, int numcmds, int totalcmds,
	int dropped_packets, bool ignore, bool paused )
{
	int				i;
	CUserCmd		*from, *to;

	// We track last three command in case we drop some 
	//  packets but get them back.
	CUserCmd cmds[ CMD_MAXBACKUP ];  

	CUserCmd		cmdNull;  // For delta compression
	
	Assert( numcmds >= 0 );
	Assert( ( totalcmds - numcmds ) >= 0 );

	CBasePlayer *pPlayer = NULL;
	CBaseEntity *pEnt = CBaseEntity::Instance(player);
	if ( pEnt && pEnt->IsPlayer() )
	{
		pPlayer = static_cast< CBasePlayer * >( pEnt );
	}
	// Too many commands?
	if ( totalcmds < 0 || totalcmds >= ( CMD_MAXBACKUP - 1 ) || numcmds < 0 || numcmds > totalcmds )
	{
		const char *name = "unknown";
		if ( pPlayer )
		{
			name = pPlayer->GetPlayerName();
		}

		Msg("CBasePlayer::ProcessUsercmds: too many cmds %i sent for player %s\n", totalcmds, name );
		// FIXME:  Need a way to drop the client from here
		//SV_DropClient ( host_client, false, "CMD_MAXBACKUP hit" );
		buf->SetOverflowFlag();
		return 0.0f;
	}

	// Initialize for reading delta compressed usercmds
	cmdNull.Reset();
	from = &cmdNull;
	for ( i = totalcmds - 1; i >= 0; i-- )
	{
		to = &cmds[ i ];
		ReadUsercmd( buf, to, from );
		from = to;

		// Validate UserCmd that we parsed:
		if ( !to->viewangles.IsValid() )
		{
			to->viewangles.Init();
		}

		if ( !IsEntityQAngleReasonable( to->viewangles ) )
		{	// Make sure we don't crash the server even if we ban the dude
			to->viewangles.Init();
		}

		if ( !IsFinite( to->forwardmove ) ||
			!IsFinite( to->sidemove ) ||
			!IsFinite( to->upmove ) )
		{
			to->forwardmove = 0;
			to->sidemove = 0;
			to->upmove = 0;
		}

		if ( ( fabs( to->forwardmove ) > sv_max_usercmd_move_magnitude.GetFloat() ) ||
			( fabs( to->sidemove ) > sv_max_usercmd_move_magnitude.GetFloat() ) ||
			( fabs( to->upmove ) > sv_max_usercmd_move_magnitude.GetFloat() ) )
		{
			to->forwardmove = 0;
			to->sidemove = 0;
			to->upmove = 0;
		}
	}

	// Client not fully connected or server has gone inactive  or is paused, just ignore
	if ( ignore || !pPlayer )
	{
		return 0.0f;
	}

	MDLCACHE_CRITICAL_SECTION();
	pPlayer->ProcessUsercmds( cmds, numcmds, totalcmds, dropped_packets, paused );

	return TICK_INTERVAL;
}


void CServerGameClients::PostClientMessagesSent( void )
{
	VPROF("CServerGameClients::PostClient");
	SNPROF("CServerGameClients::PostClient");
	gEntList.PostClientMessagesSent();
}

// Sets the client index for the client who typed the command into his/her console
void CServerGameClients::SetCommandClient( int index )
{
	g_nCommandClientIndex = index;
}

int	CServerGameClients::GetReplayDelay( edict_t *pEdict, int &entity )
{
	CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );

	if ( !pPlayer )
		return 0;

	entity = pPlayer->GetReplayEntity();

	return pPlayer->GetDelayTicks();
}


//-----------------------------------------------------------------------------
// The client's userinfo data lump has changed
//-----------------------------------------------------------------------------
void CServerGameClients::ClientEarPosition( edict_t *pEdict, Vector *pEarOrigin )
{
	CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	if (pPlayer)
	{
		*pEarOrigin = pPlayer->EarPosition();
	}
	else
	{
		// Shouldn't happen
		Assert(0);
		*pEarOrigin = vec3_origin;
	}
}


bool CServerGameClients::ClientReplayEvent( edict_t *pEdict, const ClientReplayEventParams_t &params )
{
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
	CCSPlayer *pPlayer = ( CCSPlayer * )CBaseEntity::Instance( pEdict );
	if ( pPlayer )
	{
		return pPlayer->StartHltvReplayEvent( params );
	}
	else
#endif
	{
		return 0.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *player - 
// Output : CPlayerState
//-----------------------------------------------------------------------------
CPlayerState *CServerGameClients::GetPlayerState( edict_t *player )
{
	// Is the client spawned yet?
	if ( !player || !player->GetUnknown() )
		return NULL;

	CBasePlayer *pBasePlayer = ( CBasePlayer * )CBaseEntity::Instance( player );
	if ( !pBasePlayer )
		return NULL;

	return &pBasePlayer->pl;
}

//-----------------------------------------------------------------------------
// Purpose: Anything this game .dll wants to add to the bug reporter text (e.g., the entity/model under the picker crosshair)
//  can be added here
// Input  : *buf - 
//			buflen - 
//-----------------------------------------------------------------------------
void CServerGameClients::GetBugReportInfo( char *buf, int buflen )
{
	recentNPCSpeech_t speech[ SPEECH_LIST_MAX_SOUNDS ];
	int  num;
	int  i;

	buf[ 0 ] = 0;

	if ( gpGlobals->maxClients == 1 )
	{
		CBaseEntity *ent = UTIL_PlayerByIndex(1) ? UTIL_PlayerByIndex(1)->FindPickerEntity() : NULL;
		if ( ent )
		{
			Q_snprintf( buf, buflen, "Picker %i/%s - ent %s model %s\n",
				ent->entindex(),
				ent->GetClassname(),
				STRING( ent->GetEntityName() ),
				STRING(ent->GetModelName()) );
		}

		// get any sounds that were spoken by NPCs recently
		num = GetRecentNPCSpeech( speech );
		if ( num > 0 )
		{
			Q_snprintf( buf, buflen, "%sRecent NPC speech:\n", buf );
			for( i = 0; i < num; i++ )
			{
				Q_snprintf( buf, buflen, "%s   time: %6.3f   sound name: %s   scene: %s\n", buf, speech[ i ].time, speech[ i ].name, speech[ i ].sceneName );
			}
			Q_snprintf( buf, buflen, "%sCurrent time: %6.3f\n", buf, gpGlobals->curtime );
		}
	}

#if defined ( PORTAL2 )
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		Q_snprintf( buf, buflen, "%sPortal Locations:\n", buf );
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			Q_snprintf( buf, buflen, "%slinkid:%d loc: %f %f %f\n", buf, pTempPortal->GetLinkageGroup(), XYZ( pTempPortal->GetAbsOrigin() ) );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: A player sent a voice packet
//-----------------------------------------------------------------------------
void CServerGameClients::ClientVoice( edict_t *pEdict )
{
	CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
	if (pPlayer)
	{
		pPlayer->OnVoiceTransmit();
		
		// Notify the voice listener that we've spoken
		PlayerVoiceListener().AddPlayerSpeakTime( pPlayer );
	}
}

int CServerGameClients::GetMaxSplitscreenPlayers()
{
	return MAX_SPLITSCREEN_PLAYERS;
}

int CServerGameClients::GetMaxHumanPlayers()
{
	if ( g_pGameRules )
	{
		return g_pGameRules->GetMaxHumanPlayers();
	}
	return -1;
}

// The client has submitted a keyvalues command
void CServerGameClients::ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues )
{
	if ( !pKeyValues )
		return;

	g_pGameRules->ClientCommandKeyValues( pEntity, pKeyValues );

	char const *szCommand = pKeyValues->GetName();
	if ( FStrEq( szCommand, "OnPlayerAward" ) )
	{
		// Player received an award
		// On server, send to all connected clients
		UTIL_SendClientCommandKVToPlayer( pKeyValues->MakeCopy() );
	}
}

// Server override for supplied client name (implemented in cs_gameinterface)
const char * CServerGameClients::ClientNameHandler( uint64 xuid, const char *pchName )
{
 	return pchName;
}

// Returns which encryption key to use for messages to be encrypted for TV
EncryptedMessageKeyType_t CServerGameDLL::GetMessageEncryptionKey(INetMessage *pMessage)
{
	switch (pMessage->GetType())
	{
	case svc_VoiceData:
	{
		/*
		// check the voice data packets for being from an active caster and add the caster flag and use the public key
		CSVCMsg_VoiceData *pVoiceData = (CSVCMsg_VoiceData *)pMessage;
		CSteamID steamID(static_cast<uint64>(pVoiceData->xuid()));
		if (steamID.GetAccountID())
		{
			for (int j = 0; j < CSGameRules()->m_arrTournamentActiveCasterAccounts.Count(); j++)
			{
				if (steamID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[j])
				{
					pVoiceData->set_caster(true);
					return kEncryptedMessageKeyType_Public;
				}
			}
		}
		*/
		return kEncryptedMessageKeyType_Private;
	}
	return kEncryptedMessageKeyType_Private;

	case svc_UserMessage:
	{
		CSVCMsg_UserMessage *pUsrMessageHeader = (CSVCMsg_UserMessage *)pMessage;
		switch (pUsrMessageHeader->msg_type())
		{
		case UM_SayText:
		{
			CUsrMsg_SayText usrMsg;
			if (usrMsg.ParseFromArray(&pUsrMessageHeader->msg_data().at(0), pUsrMessageHeader->msg_data().size()))
			{
				if (usrMsg.textallchat())
					return kEncryptedMessageKeyType_Public;
			}
		}
		return kEncryptedMessageKeyType_Private;

		case UM_SayText2:
		{
			CUsrMsg_SayText2 usrMsg;
			if (usrMsg.ParseFromArray(&pUsrMessageHeader->msg_data().at(0), pUsrMessageHeader->msg_data().size()))
			{
				if (usrMsg.textallchat())
					return kEncryptedMessageKeyType_Public;
			}
		}
		return kEncryptedMessageKeyType_Private;

		case UM_TextMsg:
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
		case UM_RadioText:
		case UM_RawAudio:
		case UM_SendAudio:
#endif
			return kEncryptedMessageKeyType_Private;

		default:
			return kEncryptedMessageKeyType_None;
		}
	}
	return kEncryptedMessageKeyType_None;

	case svc_EncryptedData:
	default:
		return kEncryptedMessageKeyType_None;
	}
}

bool CServerGameDLL::LogForHTTPListeners(const char* szLogLine)
{
	//return GetServerLogHTTPDispatcher()->LogForHTTPListeners(szLogLine);
	return true;
}

void CServerGameClients::ClientSvcUserMessage(edict_t *pEntity, int nType, int nPassthrough, uint32 cbSize, const void *pvBuffer)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetContainingEntity(pEntity));
	if (!pPlayer)
		return;

	/*switch (nType)
	{
	case UM_PlayerDecalDigitalSignature:
	{
		CUsrMsg_PlayerDecalDigitalSignature msg;
		if (msg.ParseFromArray(pvBuffer, cbSize))
			pPlayer->SprayPaint(msg);
	}
	return;
	}*/
}

// If server game dll needs more time before server process quits then
// it should return true to hold game server reservation from this interface method.
// If this method returns false then the server process will clear the reservation
// and might shutdown to meet uptime or memory limit requirements.
bool CServerGameDLL::ShouldHoldGameServerReservation(float flTimeElapsedWithoutClients)
{
	/** Removed for partner depot ... fuck**/ 
	return false; // let the server get unreserved
}

#if defined( PORTAL2 )
// TODO(SanyaSho): move this to somewhere in portal2
ConVar mp_dev_gamemode( "mp_dev_gamemode", "", FCVAR_DEVELOPMENTONLY );
#endif

//-----------------------------------------------------------------------------
// Purpose: Called to apply lobby settings to a dedicated server
//-----------------------------------------------------------------------------
void CServerGameDLL::ApplyGameSettings( KeyValues *pKV )
{
	if( !pKV )
		return;

#ifdef TERROR
	extern ConVar ZombieDifficulty;
	extern ConVar DirectorMinStartPlayers;
	extern ConVar mp_gamemode;
	extern ConVar mp_roundlimit;
#endif

	// Fix the game settings request when a generic request for
	// map launch comes in (it might be nested under reservation
	// processing)
	bool bAlreadyLoadingMap = false;
	char const *szBspName = NULL;
	if( !Q_stricmp( pKV->GetName(), "::ExecGameTypeCfg" ) )
	{
		if( !engine )
			return;

		char const *szNewMap = pKV->GetString( "map/mapname", "" );
		if( !szNewMap || !*szNewMap )
			return;

		KeyValues *pLaunchOptions = engine->GetLaunchOptions();
		if( !pLaunchOptions )
			return;

		if( FindLaunchOptionByValue( pLaunchOptions, "changelevel" ) ||
			FindLaunchOptionByValue( pLaunchOptions, "changelevel2" ) )
			return;

		if( FindLaunchOptionByValue( pLaunchOptions, "map_background" ) )
		{
#ifdef TERROR
			// Special settings for background map
			mp_gamemode.SetValue( "coop" );
			ZombieDifficulty.SetValue( "normal" );
			DirectorMinStartPlayers.SetValue( 0 );
#endif
#ifdef PORTAL2
			extern ConVar sv_portal_players;
			sv_portal_players.SetValue( 1 );

			static ConVarRef coop_ref( "coop" );
			if( coop_ref.IsValid() )
			{
				coop_ref.SetValue( 0 );
			}

			pKV->SetInt( "members/numSlots", 1 );		// PORTAL2 HACK: fixes maxplayers
#endif
			return;
		}

		bAlreadyLoadingMap = true;

		if( FindLaunchOptionByValue( pLaunchOptions, "reserved" ) )
		{
			// We are already reserved, but we still need to let the engine
			// baseserver know how many human slots to allocate
			int numSlots = 1;
#ifdef TERROR
			numSlots = !CTerrorGameRules::HasPlayerControlledZombies() ? 4 : 8;
#endif
#ifdef INFESTED_DLL
			numSlots = 4;
#endif
#ifdef PORTAL2
			extern ConVar sv_portal_players;
			numSlots = sv_portal_players.GetInt();
#endif
#ifdef DOTA_DLL
			numSlots = 10;
#endif

			pKV->SetInt( "members/numSlots", numSlots );
			return;
		}

#ifdef TERROR
		bool bFound = false;
		for( int i = 0; i < NUM_GAME_MODES; ++i )
		{
			const char *pModeString = GetGameModeString( (GameMode)i );
			if( FindLaunchOptionByValue( pLaunchOptions, pModeString ) )
			{
				pKV->SetString( "game/mode", pModeString );
				pKV->SetInt( "members/numSlots", GetNumPlayersForMode( (GameMode)i ) );
				bFound = true;
				break;
			}
		}

		if( !bFound )
		{
			// set values based on mp_gamemode
			const char *pszGameMode = mp_gamemode.GetString();
			for( int i = 0; i < NUM_GAME_MODES; ++i )
			{
				const char *pModeString = GetGameModeString( (GameMode)i );
				if( !Q_stricmp( pszGameMode, pModeString ) )
				{
					pKV->SetString( "game/mode", pModeString );
					pKV->SetInt( "members/numSlots", GetNumPlayersForMode( (GameMode)i ) );
					break;
				}
			}
		}

		DevMsg( "  Adjusting game mode %s, %d slots.\n", pKV->GetString( "game/mode", "(unknown)" ), pKV->GetInt( "members/numSlots", 0 ) );

		char const *arrDifficulties[4] = { "easy", "normal", "hard", "impossible" };
		for( int k = 0; k < ARRAYSIZE( arrDifficulties ); ++k )
		{
			if( !FindLaunchOptionByValue( pLaunchOptions, arrDifficulties[k] ) )
				continue;

			pKV->SetString( "game/difficulty", arrDifficulties[k] );
			DevMsg( "  Adjusting difficulty %s.\n", arrDifficulties[k] );
			break;
		}

		pKV->SetName( "left4dead2" );
#endif

#ifdef INFESTED_DLL
		pKV->SetName( "swarm" );
#endif

#ifdef PORTAL2
		pKV->SetName( "portal2" );

		// Determine player slots
		{
			int numSlots = 1;
			if( FindLaunchOptionByValue( pLaunchOptions, "*mp" ) )
				numSlots = 2;
			else if( FindLaunchOptionByValue( pLaunchOptions, "*sp" ) )
				numSlots = 1;
			else if( V_stristr( szNewMap, "mp_coop_" ) != NULL )
				numSlots = 2;
			else
				numSlots = 1;

			if( ( numSlots == 2 ) && CommandLine()->FindParm( "-allowspectators" ) )
			{
				numSlots = 3;
			}

			pKV->SetInt( "members/numSlots", numSlots );
			DevMsg( "  Adjusting game players %d.\n", numSlots );
		}
#endif
	}

#ifdef TERROR
	if( Q_stricmp( pKV->GetName(), "left4dead2" ) )
		return;

	KeyValues *pInfoMission = NULL;
	KeyValues *pInfoChapter = g_pMatchExtL4D->GetMapInfo( pKV, &pInfoMission );

	//
	// Game Mode
	//
	char const *szGameMode = pKV->GetString( "game/mode", "" );
	if( szGameMode && *szGameMode )
	{
		mp_gamemode.SetValue( szGameMode );
	}
	DevMsg( "            game mode = %s\n", szGameMode );

	//
	// Difficulty
	//
	if( char const *szDifficulty = pKV->GetString( "game/difficulty", NULL ) )
	{
		DevMsg( "            difficulty = %s\n", szDifficulty );
		ZombieDifficulty.SetValue( szDifficulty );
	}

	//
	// Round Limit
	//
	if( int nRoundLimit = pKV->GetInt( "game/maxrounds" ) )
	{
		DevMsg( "            roundlimit = %d\n", nRoundLimit );
		mp_roundlimit.SetValue( nRoundLimit );
	}

	//
	// Players connecting
	//
	int iNumPlayers = pKV->GetInt( "members/numPlayers", -1 );
	if( iNumPlayers != -1 )
	{
		DevMsg( "            num players = %d\n", iNumPlayers );
		DirectorMinStartPlayers.SetValue( iNumPlayers );
	}

	//
	// Game extras
	//
	if( int iExtra = pKV->GetInt( "GameExtras/bat" ) )
	{
		DevMsg( "            extras bat = %s\n", ( iExtra > 0 ) ? "enabled" : "disabled" );

		if( TheDirector )
		{
			CDirectorItemManager *pItemManager = TheDirector->GetItemManager();
			if( pItemManager )
			{
				pItemManager->SetWeaponAllowed_Bat( !!( iExtra > 0 ) );
			}
		}
	}
	if( int iExtra = pKV->GetInt( "GameExtras/csguns" ) )
	{
		DevMsg( "            extras csguns = %s\n", ( iExtra > 0 ) ? "enabled" : "disabled" );

		bool bAllowed = !!( iExtra > 0 );
		CSWeaponAllowed weapon = CS_WEAPON_ALLOWED_NONE;

		// 50% chance to select to spawn one of the german weapons.
		if( bAllowed && random->RandomInt( 0, 1 ) )
		{
			// this being set to non-0 means the knife is allowed to spawn
			weapon = (CSWeaponAllowed)random->RandomInt( CS_WEAPON_ALLOWED_SMG_MP5, CS_WEAPON_ALLOWED_SNIPER_AWP );
		}
		else
		{
			bAllowed = false;
		}

		if( TheDirector )
		{
			CDirectorItemManager *pItemManager = TheDirector->GetItemManager();
			if( pItemManager )
			{
				pItemManager->SetWeaponAllowed_CSWeapon( bAllowed, weapon );
			}
		}
	}

	// If we determined the chapter, set the bspname to load
	szBspName = pInfoChapter->GetString( "map" );
#endif

#ifdef INFESTED_DLL
	if( Q_stricmp( pKV->GetName(), "swarm" ) )
		return;

	KeyValues *pInfoMission = NULL;
	KeyValues *pInfoChapter = g_pMatchExtSwarm->GetMapInfo( pKV, &pInfoMission );

	szBspName = pInfoChapter->GetString( "map" );
#endif

#ifdef PORTAL2
	if( Q_stricmp( pKV->GetName(), "portal2" ) )
		return;

	if( int numSlots = pKV->GetInt( "members/numSlots" ) )
	{
		extern ConVar sv_portal_players;
		sv_portal_players.SetValue( numSlots );

		static ConVarRef coop_ref( "coop" );
		if( coop_ref.IsValid() )
		{
			if( numSlots <= 1 )
			{
				coop_ref.SetValue( 0 );
			}
			else
			{
				coop_ref.SetValue( 1 );
			}
		}

		DevMsg( "            game players = %d\n", numSlots );
	}

	//
	// Game extras
	//
	if( int iExtra = pKV->GetInt( "gameextras/skins" ) )
	{
		DevMsg( "            extras skins = %s\n", ( iExtra > 0 ) ? "enabled" : "disabled" );

		if( iExtra > 0 )
		{
			g_nPortal2PromoFlags |= PORTAL2_PROMO_SKINS;
		}
		else
		{
			g_nPortal2PromoFlags &= ~PORTAL2_PROMO_SKINS;
		}
	}
	if( int iExtra = pKV->GetInt( "gameextras/helmets" ) )
	{
		DevMsg( "            extras helmets = %s\n", ( iExtra > 0 ) ? "enabled" : "disabled" );

		if( iExtra > 0 )
		{
			g_nPortal2PromoFlags |= PORTAL2_PROMO_HELMETS;
		}
		else
		{
			g_nPortal2PromoFlags &= ~PORTAL2_PROMO_HELMETS;
		}
	}
	if( int iExtra = pKV->GetInt( "gameextras/antenna" ) )
	{
		DevMsg( "            extras antenna = %s\n", ( iExtra > 0 ) ? "enabled" : "disabled" );

		if( iExtra > 0 )
		{
			g_nPortal2PromoFlags |= PORTAL2_PROMO_ANTENNA;
		}
		else
		{
			g_nPortal2PromoFlags &= ~PORTAL2_PROMO_ANTENNA;
		}
	}

	// Force promo flags for debugging
	//g_nPortal2PromoFlags |= PORTAL2_PROMO_SKINS;
	//g_nPortal2PromoFlags |= PORTAL2_PROMO_HELMETS;
	//g_nPortal2PromoFlags |= PORTAL2_PROMO_ANTENNA;

	static ConVarRef sv_bonus_challenge( "sv_bonus_challenge" );
	if( sv_bonus_challenge.IsValid() )
	{
		char const *szGameMode = pKV->GetString( "game/mode", "" );
		if( !szGameMode[0] )
		{
			szGameMode = mp_dev_gamemode.GetString();
		}

		if( !Q_stricmp( szGameMode, "sp" ) )
		{
			sv_bonus_challenge.SetValue( !Q_stricmp( "challenge", pKV->GetString( "options/play", "" ) ) );
		}
		else
		{
			sv_bonus_challenge.SetValue( !Q_stricmp( szGameMode, "coop_challenge" ) );
		}
	}

	szBspName = pKV->GetString( "game/map" );
#endif

#ifdef DOTA_DLL
	if( Q_stricmp( pKV->GetName(), "DotA" ) )
		return;

	szBspName = pKV->GetString( "game/bspname" );
#endif

	//
	// Change map as the last command
	//
	if( szBspName && szBspName[0] && !bAlreadyLoadingMap )
	{
		// Vitaliy: Disable cheats as part of reservation in case they were enabled (unless we are on Steam Beta)
		if( sv_force_transmit_ents.IsFlagSet( FCVAR_DEVELOPMENTONLY ) &&	// any convar with FCVAR_DEVELOPMENTONLY flag will be sufficient here
			sv_cheats && sv_cheats->GetBool() )
		{
			sv_cheats->SetValue( 0 );
		}

		static ConVarRef map_wants_save_disable( "map_wants_save_disable" );
		map_wants_save_disable.SetValue( 0 );

		if( char const *szSaveGame = pKV->GetString( "game/save", NULL ) )
		{
			char const *szMapCommand = IsX360() ? "xload" : "load";
			DevMsg( "            loading savegame: %s %s (map %s)...\n", szMapCommand, szSaveGame, szBspName );
			engine->ServerCommand( "maxplayers 1\n" ); // need to set maxplayers 1, otherwise server might remain in multiplayer mode and reject load command
			engine->ServerCommand( CFmtStr( "%s %s\n",
				szMapCommand,
				szSaveGame ) );
			engine->ServerExecute(); // trigger a load right away because we might be requesting a load while
			// already playing a single player session and we cannot let the engine fall out into main menu mode
		}
		else
		{
			char const *szMapCommand = pKV->GetString( "map/mapcommand", "map" );
			DevMsg( "            starting: %s %s...\n", szMapCommand, szBspName );
			engine->ServerCommand( CFmtStr( "%s %s reserved\n",
				szMapCommand,
				szBspName ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bf_write *g_pMsgBuffer = NULL;

void EntityMessageBegin( CBaseEntity * entity, bool reliable /*= false*/ ) 
{
	Assert( !g_pMsgBuffer );

	Assert ( entity );

	g_pMsgBuffer = engine->EntityMessageBegin( entity->entindex(), entity->GetServerClass(), reliable );
}

void MessageEnd( void )
{
	Assert( g_pMsgBuffer );

	engine->MessageEnd();

	g_pMsgBuffer = NULL;
}

void MessageWriteByte( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WRITE_BYTE called with no active message\n" );

	g_pMsgBuffer->WriteByte( iValue );
}

void MessageWriteChar( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WRITE_CHAR called with no active message\n" );

	g_pMsgBuffer->WriteChar( iValue );
}

void MessageWriteShort( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WRITE_SHORT called with no active message\n" );

	g_pMsgBuffer->WriteShort( iValue );
}

void MessageWriteWord( int iValue )
{
	if (!g_pMsgBuffer)
		Error( "WRITE_WORD called with no active message\n" );

	g_pMsgBuffer->WriteWord( iValue );
}

void MessageWriteLong( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteLong called with no active message\n" );

	g_pMsgBuffer->WriteLong( iValue );
}

void MessageWriteFloat( float flValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteFloat called with no active message\n" );

	g_pMsgBuffer->WriteFloat( flValue );
}

void MessageWriteAngle( float flValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteAngle called with no active message\n" );

	g_pMsgBuffer->WriteBitAngle( flValue, 8 );
}

void MessageWriteCoord( float flValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteCoord called with no active message\n" );

	g_pMsgBuffer->WriteBitCoord( flValue );
}

void MessageWriteVec3Coord( const Vector& rgflValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteVec3Coord called with no active message\n" );

	g_pMsgBuffer->WriteBitVec3Coord( rgflValue );
}

void MessageWriteVec3Normal( const Vector& rgflValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteVec3Normal called with no active message\n" );

	g_pMsgBuffer->WriteBitVec3Normal( rgflValue );
}

void MessageWriteBitVecIntegral( const Vector& vecValue )
{
	if (!g_pMsgBuffer)
		Error( "MessageWriteBitVecIntegral called with no active message\n" );

	for ( int i = 0; i < 3; ++i )
	{
		g_pMsgBuffer->WriteBitCoordMP( vecValue[ i ], kCW_Integral );
	}
}

void MessageWriteAngles( const QAngle& rgflValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteVec3Normal called with no active message\n" );

	g_pMsgBuffer->WriteBitAngles( rgflValue );
}

void MessageWriteString( const char *sz )
{
	if (!g_pMsgBuffer)
		Error( "WriteString called with no active message\n" );

	g_pMsgBuffer->WriteString( sz );
}

void MessageWriteEntity( int iValue)
{
	if (!g_pMsgBuffer)
		Error( "WriteEntity called with no active message\n" );

	g_pMsgBuffer->WriteShort( iValue );
}

void MessageWriteEHandle( CBaseEntity *pEntity )
{
	if (!g_pMsgBuffer)
		Error( "WriteEHandle called with no active message\n" );

	long iEncodedEHandle;
	
	if( pEntity )
	{
		EHANDLE hEnt = pEntity;

		int iSerialNum = hEnt.GetSerialNumber() & (1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1;
		iEncodedEHandle = hEnt.GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS);
	}
	else
	{
		iEncodedEHandle = INVALID_NETWORKED_EHANDLE_VALUE;
	}
	
	g_pMsgBuffer->WriteLong( iEncodedEHandle );
}

// bitwise
void MessageWriteBool( bool bValue )
{
	if (!g_pMsgBuffer)
		Error( "WriteBool called with no active message\n" );

	g_pMsgBuffer->WriteOneBit( bValue ? 1 : 0 );
}

void MessageWriteUBitLong( unsigned int data, int numbits )
{
	if (!g_pMsgBuffer)
		Error( "WriteUBitLong called with no active message\n" );

	g_pMsgBuffer->WriteUBitLong( data, numbits );
}

void MessageWriteSBitLong( int data, int numbits )
{
	if (!g_pMsgBuffer)
		Error( "WriteSBitLong called with no active message\n" );

	g_pMsgBuffer->WriteSBitLong( data, numbits );
}

void MessageWriteBits( const void *pIn, int nBits )
{
	if (!g_pMsgBuffer)
		Error( "WriteBits called with no active message\n" );

	g_pMsgBuffer->WriteBits( pIn, nBits );
}

//-----------------------------------------------------------------------------
void SendUserMessage( IRecipientFilter& filter, int message, const ::google::protobuf::Message &msg )
{
	engine->SendUserMessage( filter, message, msg );
}

class CServerDLLSharedAppSystems : public IServerDLLSharedAppSystems
{
public:
	CServerDLLSharedAppSystems()
	{
		AddAppSystem( "soundemittersystem", SOUNDEMITTERSYSTEM_INTERFACE_VERSION );
		AddAppSystem( "scenefilecache", SCENE_FILE_CACHE_INTERFACE_VERSION );
#ifdef INFESTED_DLL
		AddAppSystem( "missionchooser", ASW_MISSION_CHOOSER_VERSION );
#endif
	}

	virtual int	Count()
	{
		return m_Systems.Count();
	}
	virtual char const *GetDllName( int idx )
	{
		return m_Systems[ idx ].m_pModuleName;
	}
	virtual char const *GetInterfaceName( int idx )
	{
		return m_Systems[ idx ].m_pInterfaceName;
	}
private:
	void AddAppSystem( char const *moduleName, char const *interfaceName )
	{
		AppSystemInfo_t sys;
		sys.m_pModuleName = moduleName;
		sys.m_pInterfaceName = interfaceName;
		m_Systems.AddToTail( sys );
	}

	CUtlVector< AppSystemInfo_t >	m_Systems;
};

EXPOSE_SINGLE_INTERFACE( CServerDLLSharedAppSystems, IServerDLLSharedAppSystems, SERVER_DLL_SHARED_APPSYSTEMS );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CServerGameTags::GetTaggedConVarList( KeyValues *pCvarTagList )
{
	if ( pCvarTagList && g_pGameRules )
	{
		g_pGameRules->GetTaggedConVarList( pCvarTagList );
	}
}

#if defined(PORTAL2)
CON_COMMAND_F( give_promo_helmet, "Gives the gamestop promo helmets for coop bots. Requires a respawn or changelevel to start showing.", FCVAR_DEVELOPMENTONLY )
{
	g_nPortal2PromoFlags |= PORTAL2_PROMO_HELMETS;
}
#endif
