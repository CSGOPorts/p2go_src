//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef PLAYER_H
#define PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "basecombatcharacter.h"
#include "usercmd.h"
#include "playerlocaldata.h"
#include "PlayerState.h"
#include "game/server/iplayerinfo.h"
#include "hintsystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "simtimer.h"
#include "vprof.h"
#include "iclient.h"
#include "input_device.h"
#include "vote_controller.h"

class CLogicPlayerProxy;

// For queuing and processing usercmds
class CCommandContext
{
public:
	CUtlVector< CUserCmd > cmds;

	int				numcmds;
	int				totalcmds;
	int				dropped_packets;
	bool			paused;
};

// Info about last 20 or so updates to the
class CPlayerCmdInfo
{
public:
	CPlayerCmdInfo() : 
	  m_flTime( 0.0f ), m_nNumCmds( 0 ), m_nDroppedPackets( 0 )
	{
	}

	// realtime of sample
	float		m_flTime;
	// # of CUserCmds in this update
	int			m_nNumCmds;
	// # of dropped packets on the link
	int			m_nDroppedPackets;
};

class CPlayerSimInfo
{
public:
	CPlayerSimInfo() : 
	  m_flTime( 0.0f ), m_nNumCmds( 0 ), m_nTicksCorrected( 0 ), m_flFinalSimulationTime( 0.0f ), m_flGameSimulationTime( 0.0f ), m_flServerFrameTime( 0.0f ), m_vecAbsOrigin( 0, 0, 0 )
	{
	}

	// realtime of sample
	float		m_flTime;
	// # of CUserCmds in this update
	int			m_nNumCmds;
	// If clock needed correction, # of ticks added/removed
	int			m_nTicksCorrected; // +ve or -ve
	// player's m_flSimulationTime at end of frame
	float		m_flFinalSimulationTime;
	float		m_flGameSimulationTime;
	// estimate of server perf
	float		m_flServerFrameTime;  
	Vector		m_vecAbsOrigin;
};
//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class CBaseCombatWeapon;
class CBaseViewModel;
class CTeam;
class IPhysicsPlayerController;
class IServerVehicle;
class CUserCmd;
class CFuncLadder;
class CNavArea;
class CHintSystem;
class CAI_Expresser;
class CAI_Node;
class CAI_Link;

class CTonemapTrigger;

// for step sounds
struct surfacedata_t;

// !!!set this bit on guns and stuff that should never respawn.
#define	SF_NORESPAWN	( 1 << 30 )


//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST	4		// max of 4 suit sentences queued up at any time
#define	SUIT_REPEAT_OK		0

#define SUIT_NEXT_IN_30SEC	30
#define SUIT_NEXT_IN_1MIN	60
#define SUIT_NEXT_IN_5MIN	300
#define SUIT_NEXT_IN_10MIN	600
#define SUIT_NEXT_IN_30MIN	1800
#define SUIT_NEXT_IN_1HOUR	3600

#define CSUITNOREPEAT		32

#define TEAM_NAME_LENGTH	16

// constant items
#define ITEM_HEALTHKIT		1
#define ITEM_BATTERY		4

#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669
#define AUTOAIM_20DEGREES 0.3490658503989


enum PlayerConnectedState
{
	PlayerConnected,
	PlayerDisconnecting,
	PlayerDisconnected,
};

enum AimResults
{
	AIMRESULTS_NONE,
	AIMRESULTS_ONTARGET,
	AIMRESULTS_ASSISTED,
};

extern bool gInitHUD;
extern ConVar *sv_cheats;

class CBasePlayer;
class CPlayerInfo : public IBotController, public IPlayerInfo
{
public:
	CPlayerInfo () { m_pParent = NULL; } 
	~CPlayerInfo () {}
	void SetParent( CBasePlayer *parent ) { m_pParent = parent; } 

	// IPlayerInfo interface
	virtual const char *GetName();
	virtual int			GetUserID();
	virtual const char *GetNetworkIDString();
	virtual int			GetTeamIndex();
	virtual void		ChangeTeam( int iTeamNum );
	virtual int			GetFragCount();
	virtual int			GetAssistsCount();
	virtual int			GetDeathCount();
	virtual bool		IsConnected();
	virtual int			GetArmorValue();

	virtual bool IsHLTV();
#if defined( REPLAY_ENABLED )
	virtual bool IsReplay();
#endif
	virtual bool IsPlayer();
	virtual bool IsFakeClient();
	virtual bool IsDead();
	virtual bool IsInAVehicle();
	virtual bool IsObserver();
	virtual const Vector GetAbsOrigin();
	virtual const QAngle GetAbsAngles();
	virtual const Vector GetPlayerMins();
	virtual const Vector GetPlayerMaxs();
	virtual const char *GetWeaponName();
	virtual const char *GetModelName();
	virtual const int GetHealth();
	virtual const int GetMaxHealth();

	// bot specific functions	
	virtual void SetAbsOrigin( Vector & vec );
	virtual void SetAbsAngles( QAngle & ang );
	virtual void RemoveAllItems( bool removeSuit );
	virtual void SetActiveWeapon( const char *WeaponName );
	virtual void SetLocalOrigin( const Vector& origin );
	virtual const Vector GetLocalOrigin( void );
	virtual void SetLocalAngles( const QAngle& angles );
	virtual const QAngle GetLocalAngles( void );
	virtual void PostClientMessagesSent( void );
	virtual bool IsEFlagSet( int nEFlagMask );

	virtual void RunPlayerMove( CBotCmd *ucmd );
	virtual void SetLastUserCommand( const CBotCmd &cmd );

	virtual CBotCmd GetLastUserCommand();

private:
	CBasePlayer *m_pParent; 
};



class CBasePlayer : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CBasePlayer, CBaseCombatCharacter );
protected:
	// HACK FOR BOTS
	friend class CBotManager;
	static edict_t *s_PlayerEdict; // must be set before calling constructor
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
	// script description
	DECLARE_ENT_SCRIPTDESC();
	
	CBasePlayer();
	~CBasePlayer();

	void					StartUserMessageThrottling( char const *pchMessageNames[], int nNumMessageNames );
	void					FinishUserMessageThrottling();

	bool					ShouldThrottleUserMessage( char const *pchMessageName );

	
	// IPlayerInfo passthrough (because we can't do multiple inheritance)
	IPlayerInfo *GetPlayerInfo() { return &m_PlayerInfo; }
	IBotController *GetBotController() { return &m_PlayerInfo; }

	virtual void			SetModel( const char *szModelName );
	void					SetBodyPitch( float flPitch );

	virtual void			UpdateOnRemove( void );

	static CBasePlayer		*CreatePlayer( const char *className, edict_t *ed );

	virtual void			CreateViewModel( int viewmodelindex = 0 );
	CBaseViewModel			*GetViewModel( int viewmodelindex = 0 );
	void					HideViewModels( void );
	void					DestroyViewModels( void );

	CPlayerState			*PlayerData( void ) { return pl.Get(); }
	const CPlayerState		*PlayerData( void ) const { return pl.Get(); }
	
	int						RequiredEdictIndex( void ) { return ENTINDEX(edict()); } 

	void					LockPlayerInPlace( void );
	void					UnlockPlayer( void );

	virtual void			DrawDebugGeometryOverlays(void);
	
	// Networking is about to update this entity, let it override and specify it's own pvs
	virtual void			SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );
	virtual int				UpdateTransmitState();
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual bool			ShouldCheckOcclusion( CBasePlayer *pOtherPlayer ) { return false;  }

	// Returns true if this player wants pPlayer to be moved back in time when this player runs usercmds.
	// Saves a lot of overhead on the server if we can cull out entities that don't need to lag compensate
	// (like team members, entities out of our PVS, etc).
	virtual bool			WantsLagCompensationOnEntity( const CBaseEntity *entity, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const;

	virtual void			Spawn( void );
	virtual void			Activate( void );
	virtual void			SharedSpawn(); // Shared between client and server.
	virtual void			ForceRespawn( void );
	virtual void			PostSpawnPointSelection( void );

	virtual void			InitialSpawn( void );
	virtual void			InitHUD( void ) {}
	virtual void			ShowViewPortPanel( const char * name, bool bShow = true, KeyValues *data = NULL );

	virtual const char *	GetPlayerModelName( void );

	virtual void			PlayerDeathThink( void );
	virtual void			PlayerForceTeamThink( void );



	virtual void			Jump( void );
	virtual void			Duck( void );

	const char				*GetTracerType( void );
	void					MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	void					DoImpactEffect( trace_t &tr, int nDamageType );

	// If a map clean up has been done after a respawn, this function will reassign the entity pointers to those map entities that were deleted.
	void					UpdateMapEntityPointers( void );

#if !defined( NO_ENTITY_PREDICTION )
	void					AddToPlayerSimulationList( CBaseEntity *other );
	void					RemoveFromPlayerSimulationList( CBaseEntity *other );
	void					SimulatePlayerSimulatedEntities( void );
	void					ClearPlayerSimulationList( void );
#endif

	// Physics simulation (player executes it's usercmd's here)
	virtual void			PhysicsSimulate( void );

	// Forces processing of usercmds (e.g., even if game is paused, etc.)
	void					ForceSimulation();

	virtual unsigned int	PhysicsSolidMaskForEntity( void ) const;

	// Move us a bit to ensure we're spawning somewhere we can actually move around.
	void					EnsureValidSpawnLocation();

	virtual void			PreThink( void );
	virtual void			PostThink( void );
	virtual int				TakeHealth( float flHealth, int bitsDamageType );
	virtual void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	bool					ShouldTakeDamageInCommentaryMode( const CTakeDamageInfo &inputInfo );
	virtual int				OnTakeDamage( const CTakeDamageInfo &info );
	virtual void			DamageEffect(float flDamage, int fDamageType);

	virtual void			OnSwitchWeapons( CBaseCombatWeapon* pWeapon ){}
	virtual void			OnDamagedByExplosion( const CTakeDamageInfo &info );

	virtual bool			CanKickFromTeam( int kickTeam ) { return GetTeamNumber() == kickTeam; }

	void					PauseBonusProgress( bool bPause = true );
	void					SetBonusProgress( int iBonusProgress );
	void					SetBonusChallenge( int iBonusChallenge );

	int						GetBonusProgress() const { return m_iBonusProgress; }
	int						GetBonusChallenge() const { return m_iBonusChallenge; }

	virtual Vector			EyePosition( );			// position of eyes
	const QAngle			&EyeAngles( );
	void					EyePositionAndVectors( Vector *pPosition, Vector *pForward, Vector *pRight, Vector *pUp );
	virtual const QAngle	&LocalEyeAngles();		// Direction of eyes
	void					EyeVectors( Vector *pForward, Vector *pRight = NULL, Vector *pUp = NULL );
	void					CacheVehicleView( void );	// Calculate and cache the position of the player in the vehicle

	// Sets the view angles
	void					SnapEyeAngles( const QAngle &viewAngles );

	virtual QAngle			BodyAngles();
	virtual Vector			BodyTarget( const Vector &posSrc, bool bNoisy);
	virtual bool			ShouldFadeOnDeath( void ) { return FALSE; }
	
	virtual const impactdamagetable_t &GetPhysicsImpactDamageTable();
	virtual int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void			Event_Killed( const CTakeDamageInfo &info );
	// Notifier that I've killed some other entity. (called from Victim's Event_Killed).
	virtual void			Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	void					Event_Dying( void );

	bool					IsHLTV( void ) const { return pl.hltv; }
	bool					IsReplay( void ) const { return pl.replay; }
	virtual	bool			IsPlayer( void ) const { return true; }			// Spectators return TRUE for this, use IsObserver to seperate cases
	virtual bool			IsNetClient( void ) const { return true; }		// Bots should return FALSE for this, they can't receive NET messages
																			// Spectators should return TRUE for this

	virtual bool			IsFakeClient( void ) const;

	// Get the client index (entindex-1).
	int						GetClientIndex()	{ return ENTINDEX( edict() ) - 1; }

	// returns the player name
#ifdef _PS3
	const char *			GetPlayerName() const 
							{ 
								if (!strcmp(m_szNetname, ""))
								{
									return "empty";
								}
								else
								{
									return m_szNetname; 
								}
							}
#else
	const char *			GetPlayerName() const { return m_szNetname; }
#endif
		
	void					SetPlayerName( const char *name );

	virtual const char *	GetCharacterDisplayName() { return GetPlayerName(); }

	int						GetUserID() const { return engine->GetPlayerUserId( edict() ); }
	const char *			GetNetworkIDString(); 
	virtual const Vector	GetPlayerMins( void ) const; // uses local player
	virtual const Vector	GetPlayerMaxs( void ) const; // uses local player

	virtual void			UpdateCollisionBounds( void );

	void					VelocityPunch( const Vector &vecForce );
	void					ViewPunch( const QAngle &angleOffset );
	void					ViewPunchReset( float tolerance = 0 );
	void					ShowViewModel( bool bShow );
	void					ShowCrosshair( bool bShow );

	bool					ScriptIsPlayerNoclipping( void );
	virtual void			NoClipStateChanged( void ) { };

	// View model prediction setup
	void					CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );

	// Handle view smoothing when going up stairs
	void					SmoothViewOnStairs( Vector& eyeOrigin );
	virtual float			CalcRoll (const QAngle& angles, const Vector& velocity, float rollangle, float rollspeed);
	void					CalcViewRoll( QAngle& eyeAngles );
	virtual void			CalcViewBob( Vector& eyeOrigin );
	virtual void			CalcAddViewmodelCameraAnimation( Vector& eyeOrigin, QAngle& eyeAngles );

	virtual int				Save( ISave &save );
	virtual int				Restore( IRestore &restore );
	virtual bool			ShouldSavePhysics();
	virtual void			OnRestore( void );

	virtual void			PackDeadPlayerItems( void );
	virtual void			RemoveAllItems( bool removeSuit );
	bool					IsDead() const;
#ifdef CSTRIKE_DLL
	virtual bool			IsRunning( void ) const	{ return false; } // bot support under cstrike (AR)
#endif

	bool					HasPhysicsFlag( unsigned int flag ) { return (m_afPhysicsFlags & flag) != 0; }

	virtual	CBaseCombatCharacter *ActivePlayerCombatCharacter( void ) { return this; }

	// Weapon stuff
	virtual Vector			Weapon_ShootPosition( );
	virtual bool			Weapon_CanUse( CBaseCombatWeapon *pWeapon );
	virtual void			Weapon_Equip( CBaseCombatWeapon *pWeapon );
	virtual	void			Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ );
	virtual	bool			Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );		// Switch to given weapon if has ammo (false if failed)
	virtual void			Weapon_SetLast( CBaseCombatWeapon *pWeapon );
	virtual bool			Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon ) { return true; }
	virtual bool			Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon );
	void					Weapon_DropSlot( int weaponSlot );
	CBaseCombatWeapon		*Weapon_GetLast( void ) { return m_hLastWeapon.Get(); }

	virtual bool			HasUnlockableWeapons( int iUnlockedableIndex ) { return false; }
	bool					HasUnlockedWpn( int iIndex ) { return false; }
	bool					HasAnyAmmoOfType( int nAmmoIndex );

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void			UpdateClientData( void );
	virtual void			UpdateBattery( void );
	virtual void			RumbleEffect( unsigned char index, unsigned char rumbleData, unsigned char rumbleFlags );
	
	// Player is moved across the transition by other means
	virtual int				ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void			Precache( void );
	bool					IsOnLadder( void );
	virtual void			ExitLadder() {}
	virtual surfacedata_t	*GetLadderSurface( const Vector &origin );

	#define PLAY_FLASHLIGHT_SOUND true
	virtual void			SetFlashlightEnabled( bool bState ) { };
	virtual int				FlashlightIsOn( void ) { return false; }
	virtual bool			FlashlightTurnOn( bool playSound = false ) { return false; };		// Skip sounds when not necessary
	virtual void			FlashlightTurnOff( bool playSound = false ) { };	// Skip sounds when not necessary
	virtual bool			IsIlluminatedByFlashlight( CBaseEntity *pEntity, float *flReturnDot ) {return false; }
	
	virtual void			UpdatePlayerSound ( void );
	virtual void			UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity );
	virtual void			PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void			GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void			SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking );
	virtual void			DeathSound( const CTakeDamageInfo &info );
	const Vector &			GetMovementCollisionNormal( void ) const;	// return the normal of the surface we last collided with
	const Vector &			GetGroundNormal( void ) const;

	virtual					void SetFogController( CFogController *pFogController );

	// return the entity used for soundscape radius checks
	virtual CBaseEntity		*GetSoundscapeListener();

	Class_T					Classify ( void );
	virtual void			SetAnimation( PLAYER_ANIM playerAnim );
	virtual void			OnMainActivityComplete( Activity newActivity, Activity oldActivity ) {}
	virtual void			OnMainActivityInterrupted( Activity newActivity, Activity oldActivity ) {}
	void					SetWeaponAnimType( const char *szExtention );

	// custom player functions
	virtual void			ImpulseCommands( void );
	virtual void			CheatImpulseCommands( int iImpulse );
	virtual bool			ClientCommand( const CCommand &args );

	static CBasePlayer*		GetPlayerBySteamID( const CSteamID &steamID );

	void					NotifySinglePlayerGameEnding() { m_bSinglePlayerGameEnding = true; }
	bool					IsSinglePlayerGameEnding() { return m_bSinglePlayerGameEnding == true; }

	//bool					HandleVoteCommands( const CCommand &args );
	//IntervalTimer &			GetLastHeldVoteTimer(){ return m_lastHeldVoteTimer; }

	//CVoteController *		GetTeamVoteController( void );	// returns one of the two team vote controllers, g_voteControllerT or g_voteControllerCT
	
	// Observer functions
	virtual bool			StartObserverMode(int mode); // true, if successful
	virtual void			StopObserverMode( void );	// stop spectator mode
	virtual bool			ModeWantsSpectatorGUI( int iMode ) { return true; }
	virtual bool			SetObserverMode(int mode); // sets new observer mode, returns true if successful
	virtual int				GetObserverMode( void ); // returns observer mode or OBS_NONE
	virtual bool			SetObserverTarget(CBaseEntity * target);
	virtual void			ObserverUse( bool bIsPressed ); // observer pressed use
	virtual CBaseEntity		*GetObserverTarget( void ); // returns players targer or NULL
	virtual CBaseEntity		*FindNextObserverTarget( bool bReverse ); // returns next/prev player to follow or NULL
	virtual int				GetNextObserverSearchStartPoint( bool bReverse ); // Where we should start looping the player list in a FindNextObserverTarget call
	virtual bool			PassesObserverFilter( const CBaseEntity *entity );	// returns true if the entity passes the specified filter
	virtual bool			IsValidObserverTarget(CBaseEntity * target); // true, if player is allowed to see this target
	virtual void			CheckObserverSettings(); // checks, if target still valid (didn't die etc)
	virtual void			JumptoPosition(const Vector &origin, const QAngle &angles);
	virtual void			SpecLerptoPosition(const Vector &origin, const QAngle &angles, float flTime);
	virtual void			ForceObserverMode(int mode); // sets a temporary mode, force because of invalid targets
	virtual void			ResetObserverMode(); // resets all observer related settings
	virtual void			ValidateCurrentObserverTarget( void ); // Checks the current observer target, and moves on if it's not valid anymore
	virtual void			AttemptToExitFreezeCam( void );

	virtual bool			StartReplayMode( float fDelay, float fDuration, int iEntity );
	virtual void			StopReplayMode();
	virtual int				GetDelayTicks();
	virtual int				GetReplayEntity();

	CLogicPlayerProxy		*GetPlayerProxy( void );
	void					FirePlayerProxyOutput( const char *pszOutputName, variant_t variant, CBaseEntity *pActivator, CBaseEntity *pCaller );

	virtual void			CreateCorpse( void ) { }
	virtual CBaseEntity		*EntSelectSpawnPoint( void );

	// Vehicles
	virtual bool			IsInAVehicle( void ) const;
			bool			CanEnterVehicle( IServerVehicle *pVehicle, int nRole );
	virtual bool			GetInVehicle( IServerVehicle *pVehicle, int nRole );
	virtual void			LeaveVehicle( const Vector &vecExitPoint = vec3_origin, const QAngle &vecExitAngles = vec3_angle );
	int						GetVehicleAnalogControlBias() { return m_iVehicleAnalogBias; }
	void					SetVehicleAnalogControlBias( int bias ) { m_iVehicleAnalogBias = bias; }
	
	// override these for 
	virtual void			OnVehicleStart() {}
	virtual void			OnVehicleEnd( Vector &playerDestPosition ) {} 
	IServerVehicle			*GetVehicle();
	CBaseEntity				*GetVehicleEntity( void );
	bool					UsingStandardWeaponsInVehicle( void );
	
	void					AddPoints( int score, bool bAllowNegativeScore );
	void					AddPointsToTeam( int score, bool bAllowNegativeScore );
	virtual bool			BumpWeapon( CBaseCombatWeapon *pWeapon );
	bool					RemovePlayerItem( CBaseCombatWeapon *pItem );
	CBaseEntity				*HasNamedPlayerItem( const char *pszItemName );
	bool 					HasWeapons( void );// do I have ANY weapons?
	virtual void			SelectLastItem(void);
	virtual void 			SelectItem( const char *pstr, int iSubType = 0 );
	void					ItemPreFrame( void );
	virtual void			ItemPostFrame( void );

	virtual CBaseEntity		*GiveNamedItem( const char *pchName, int iSubType = 0, CEconItemView *pScriptItem = NULL, bool bForce = false );

	void					EnableControl(bool fControl);
	virtual void			CheckTrainUpdate( void );
	void					AbortReload( void );

	void					SendAmmoUpdate(void);

	void					WaterMove( void );
	float					GetWaterJumpTime() const;
	void					SetWaterJumpTime( float flWaterJumpTime );
	float					GetSwimSoundTime( void ) const;
	void					SetSwimSoundTime( float flSwimSoundTime );

	virtual void			SetPlayerUnderwater( bool state );
	void					UpdateUnderwaterState( void );
	bool					IsPlayerUnderwater( void ) { return m_bPlayerUnderwater; }

	virtual bool			CanBreatheUnderwater() const { return false; }
	virtual bool			CanRecoverCurrentDrowningDamage( void ) const { return true; }// Are we allowed to later recover the drowning damage we are taking right now?  (Not, can we right now recover drowning damage.)
	virtual void			PlayerUse( void );
	virtual void			PlayUseDenySound() {}

	virtual CBaseEntity		*FindUseEntity( void );
	virtual bool			IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps );
	bool					ClearUseEntity();
	CBaseEntity				*DoubleCheckUseNPC( CBaseEntity *pNPC, const Vector &vecSrc, const Vector &vecDir );


	// physics interactions
	// mass/size limit set to zero for none
	static bool				CanPickupObject( CBaseEntity *pObject, float massLimit, float sizeLimit );
	virtual void			PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize = true ) {}
	virtual void			ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldindThis = NULL ) {}
	virtual float			GetHeldObjectMass( IPhysicsObject *pHeldObject );

	void					CheckSuitUpdate();
	void					SetSuitUpdate(char *name, int fgroup, int iNoRepeat);
	virtual void			UpdateGeigerCounter( void );
	void					CheckTimeBasedDamage( void );

	void					ResetAutoaim( void );
	
	virtual Vector			GetAutoaimVector( float flScale );
	virtual Vector			GetAutoaimVector( float flScale, float flMaxDist );
	virtual Vector			GetAutoaimVector( float flScale, float flMaxDist, float flMaxDeflection, AimResults *pAimResults );
	virtual void			GetAutoaimVector( autoaim_params_t &params );

	virtual bool			ShouldAutoaim( void );

	void					SetViewEntity( CBaseEntity *pEntity, bool bShouldDrawPlayer = true );
	CBaseEntity				*GetViewEntity( void ) { return m_hViewEntity; }

	virtual void			ForceClientDllUpdate( void );  // Forces all client .dll specific data to be resent to client.

	void					DeathMessage( CBaseEntity *pKiller );

	virtual void			ProcessUsercmds( CUserCmd *cmds, int numcmds, int totalcmds,
								int dropped_packets, bool paused );
	bool					HasQueuedUsercmds( void ) const;

	void					AvoidPhysicsProps( CUserCmd *pCmd );

	// Run a user command. The default implementation calls ::PlayerRunCommand. In TF, this controls a vehicle if
	// the player is in one.
	virtual void			PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper);
	void					RunNullCommand();

	// Team Handling
	virtual void			ChangeTeam( int iTeamNum ) { ChangeTeam(iTeamNum,false, false); }
	virtual void			ChangeTeam( int iTeamNum, bool bAutoTeam, bool bSilent );

	// say/sayteam allowed?
	virtual bool			CanHearAndReadChatFrom( CBasePlayer *pPlayer ) { return true; }
	virtual bool			CanSpeak( void ) { return true; }

	audioparams_t			&GetAudioParams() { return *m_Local.m_audio.Get(); }

	virtual void 			ModifyOrAppendPlayerCriteria( AI_CriteriaSet& set );

	virtual QAngle			GetViewPunchAngle();
	void					SetViewPunchAngle( const QAngle &punchAngle );
	void					SetViewPunchAngle( int axis, float value );
	virtual QAngle			GetAimPunchAngle();
	void					SetAimPunchAngle( const QAngle &angle );
	void					SetAimPunchAngleVelocity( const QAngle &punchAngleVelocity );

	// TrackIR
	const Vector&		GetEyeOffset() const;
	void				SetEyeOffset( const Vector& v );

	const QAngle &		GetEyeAngleOffset() const;
	void				SetEyeAngleOffset( const QAngle & qa );
	// TrackIR

	const Vector &		GetAimDirection() const;
	void				SetAimDirection( const Vector & v );

	bool				IsCoach( void ) const;
	int					GetCoachingTeam( void ) const;

	int					GetAssociatedTeamNumber( void ) const; // Returns coaching team if player is a coach. Otherwise returns GetTeamNumber.

	bool				IsSpectator( void ) const; // is TEAM_SPECTATOR and is not a coach.

	// Returns the eye or pointer angle plus the punch angle.
	QAngle					GetFinalAimAngle();

	void					PropagatePunchAnglesToObservers( void );

	virtual void			DoMuzzleFlash();

	CNavArea				*GetLastKnownArea( void ) const		{ return m_lastNavArea; }		// return the last nav area the player occupied - NULL if unknown
	const char				*GetLastKnownPlaceName( void ) const	{ return m_szLastPlaceName; }	// return the last nav place name the player occupied

	virtual void			CheckChatText( char *p, int bufsize ) {}

	virtual void			CreateRagdollEntity( void ) { return; }

	virtual void			HandleAnimEvent( animevent_t *pEvent );

	CBaseEntity				*GetTonemapController( void ) const
	{
		return m_hTonemapController.Get();
	}

	virtual bool			ShouldAnnounceAchievement( void ){ return true; }


	bool					IsSplitScreenPartner( CBasePlayer *pPlayer );
	void					SetSplitScreenPlayer( bool bSplitScreenPlayer, CBasePlayer *pOwner );
	bool					IsSplitScreenPlayer() const;
	CBasePlayer				*GetSplitScreenPlayerOwner();
	bool					IsSplitScreenUserOnEdict( edict_t *edict );
	int						GetSplitScreenPlayerSlot();

	void					AddSplitScreenPlayer( CBasePlayer *pOther );
	void					RemoveSplitScreenPlayer( CBasePlayer *pOther );
	CUtlVector< CHandle< CBasePlayer > >& GetSplitScreenPlayers();
	bool					HasAttachedSplitScreenPlayers() const;

	void					AddPictureInPicturePlayer( CBasePlayer *pOther );
	void					RemovePictureInPicturePlayer( CBasePlayer *pOther );
	CUtlVector< CHandle< CBasePlayer > >& GetSplitScreenAndPictureInPicturePlayers();
	CUtlVector< CHandle< CBasePlayer > >& GetPictureInPicturePlayers( void );

	void					SetCrossPlayPlatform( CrossPlayPlatform_t clientPlatform );
	CrossPlayPlatform_t		GetCrossPlayPlatform( void ) const;


	// Returns true if team was changed
	virtual bool			EnsureSplitScreenTeam();

	virtual void			ForceChangeTeam( int iTeamNum ) { }

	void					UpdateFXVolume( void );

	// ===============================================================
	// player's client platform and input device selection
	void					SetPlayerInputDevice( InputDevice_t controller )	{ m_PlayerInputDevice = controller; }
	InputDevice_t			GetPlayerInputDevice( void )						{ return m_PlayerInputDevice; }
	
	void					SetPlayerPlatform( InputDevicePlatform_t platform ) { m_PlayerPlatform = platform; }
	InputDevicePlatform_t	GetPlayerPlatform( void )							{ return m_PlayerPlatform; }

	void					SetLastRequestedClientInfoTime( float newTime )		{ m_lastRequestedClientInfoTime = newTime; }
	float					GetLastRequestedClientInfoTime( void )				{ return m_lastRequestedClientInfoTime; }

public:
	// Player Physics Shadow
	void					SetupVPhysicsShadow( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity, CPhysCollide *pStandModel, const char *pStandHullName, CPhysCollide *pCrouchModel, const char *pCrouchHullName );
	IPhysicsPlayerController* GetPhysicsController() { return m_pPhysicsController; }
	virtual void			VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	void					VPhysicsUpdate( IPhysicsObject *pPhysics );
	virtual void			VPhysicsShadowUpdate( IPhysicsObject *pPhysics );
	virtual bool			IsFollowingPhysics( void ) { return false; }
	bool					IsRideablePhysics( IPhysicsObject *pPhysics );
	IPhysicsObject			*GetGroundVPhysics();

	virtual void			Touch( CBaseEntity *pOther );
	void					SetTouchedPhysics( bool bTouch );
	bool					TouchedPhysics( void );
	Vector					GetSmoothedVelocity( void );

	virtual void			InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );
	virtual void			VPhysicsDestroyObject();
	void					SetVCollisionState( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity, int collisionState );
	void					PostThinkVPhysics( void );
	virtual void			UpdatePhysicsShadowToCurrentPosition();
	void					UpdatePhysicsShadowToPosition( const Vector &vecAbsOrigin );
	void					UpdateVPhysicsPosition( const Vector &position, const Vector &velocity, float secondsToArrival );

	// Hint system
	virtual CHintSystem		*Hints( void ) { return NULL; }
	bool					ShouldShowHints( void ) { return Hints() ? Hints()->ShouldShowHints() : false; }
	void					SetShowHints( bool bShowHints ) { if (Hints()) Hints()->SetShowHints( bShowHints ); }
	bool 					HintMessage( int hint, bool bForce = false ) { return Hints() ? Hints()->HintMessage( hint, bForce ) : false; }
	void 					HintMessage( const char *pMessage ) { if (Hints()) Hints()->HintMessage( pMessage ); }
	void					StartHintTimer( int iHintID ) { if (Hints()) Hints()->StartHintTimer( iHintID ); }
	void					StopHintTimer( int iHintID ) { if (Hints()) Hints()->StopHintTimer( iHintID ); }
	void					RemoveHintTimer( int iHintID ) { if (Hints()) Hints()->RemoveHintTimer( iHintID ); }

	// Accessor methods
	int		FragCount() const		{ return m_iFrags; }
	int		AssistsCount() const	{ return m_iAssists; }
	int		DeathCount() const		{ return m_iDeaths;}
	bool	IsConnected() const		{ return m_iConnected != PlayerDisconnected; }
	bool	IsDisconnecting() const	{ return m_iConnected == PlayerDisconnecting; }
	bool	IsSuitEquipped() const	{ return m_Local.m_bWearingSuit; }
	int		ArmorValue() const		{ return m_ArmorValue; }
	bool	HUDNeedsRestart() const { return m_fInitHUD; }
	float	MaxSpeed() const		{ return m_flMaxspeed; }
	Activity GetActivity( ) const	{ return m_Activity; }
	inline void SetActivity( Activity eActivity ) { m_Activity = eActivity; }
	bool	IsPlayerLockedInPlace() const { return m_iPlayerLocked != 0; }
	bool	IsObserver() const		{ return (m_afPhysicsFlags & PFLAG_OBSERVER) != 0; }
	bool	IsOnTarget() const		{ return m_fOnTarget; }
	float	MuzzleFlashTime() const { return m_flFlashTime; }
	float	PlayerDrownTime() const	{ return m_AirFinished; }

	void	SetPlayerLocked( int nLock )					{ m_iPlayerLocked = nLock; }
	int		GetPlayerLocked( void )							{ return m_iPlayerLocked; }

	int		GetObserverMode() const	{ return m_iObserverMode; }
	CBaseEntity *GetObserverTarget() const	{ return m_hObserverTarget; }
	bool	IsActiveCameraMan() const { return m_bActiveCameraMan; }
	void	SetActiveCameraMan( bool bState ) { m_bActiveCameraMan = bState; }

	// Round gamerules
	virtual bool	IsReadyToPlay( void ) { return true; }
	virtual bool	IsReadyToSpawn( void ) { return true; }
	virtual bool	ShouldGainInstantSpawn( void ) { return false; }
	virtual void	ResetPerRoundStats( void ) { return; }
	void			AllowInstantSpawn( void ) { m_bAllowInstantSpawn = true; }

	virtual void	ResetScores( void ) { ResetFragCount(); ResetAssistsCount(); ResetDeathCount(); }
	void			ResetFragCount();
	virtual void	IncrementFragCount( int nCount, int nHeadshots = 0 );
	
	void			ResetAssistsCount();
	virtual void	IncrementAssistsCount( int nCount );

	void			ResetDeathCount();
	virtual void	IncrementDeathCount( int nCount );

	void	SetArmorValue( int value );
	void	IncrementArmorValue( int nCount, int nMaxValue = -1 );

	void	SetConnected( PlayerConnectedState iConnected ) { m_iConnected = iConnected; }
	virtual void EquipSuit( bool bPlayEffects = true );
	virtual void RemoveSuit( void );
	void	SetMaxSpeed( float flMaxSpeed ) { m_flMaxspeed = flMaxSpeed; }

	void	NotifyNearbyRadiationSource( float flRange );

	void	SetAnimationExtension( const char *pExtension );

	void	SetAdditionalPVSOrigin( const Vector &vecOrigin );
	void	SetCameraPVSOrigin( const Vector &vecOrigin );
	void	SetMuzzleFlashTime( float flTime );
	void	SetDropEnabled( bool bEnabled );
	void	SetDuckEnabled( bool bEnabled );
	void	SetUseEntity( CBaseEntity *pUseEntity );
	virtual CBaseEntity* GetUseEntity( void );
	virtual CBaseEntity* GetPotentialUseEntity( void );

	virtual float GetPlayerMaxSpeed();

	// Used to set private physics flags PFLAG_*
	void	SetPhysicsFlag( int nFlag, bool bSet );

	void	AllowImmediateDecalPainting();
	void	PushAwayDecalPaintingTime( float flTime );

	// Suicide...
	virtual void CommitSuicide( bool bExplode = false, bool bForce = false );
	virtual void CommitSuicide( const Vector &vecForce, bool bExplode = false, bool bForce = false );

	// For debugging...
	void	ForceOrigin( const Vector &vecOrigin );

	// Bot accessors...
	void	SetTimeBase( float flTimeBase );
	float	GetTimeBase() const;
	void	SetLastUserCommand( const CUserCmd &cmd );
	const CUserCmd *GetLastUserCommand( void );
	virtual bool IsBot() const;

	bool	IsPredictingWeapons( void ) const; 
	int		CurrentCommandNumber() const;
	const CUserCmd *GetCurrentUserCommand() const;
	int		GetLockViewanglesTickNumber() const { return m_iLockViewanglesTickNumber; }
	QAngle	GetLockViewanglesData() const { return m_qangLockViewangles; }

	int		GetFOV( void );														// Get the current FOV value
	int		GetDefaultFOV( void ) const;										// Default FOV if not specified otherwise
	int		GetFOVForNetworking( void );										// Get the current FOV used for network computations
	bool	SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate = 0.0f, int iZoomStart = 0 );	// Alters the base FOV of the player (must have a valid requester)
	void	SetDefaultFOV( int FOV );											// Sets the base FOV if nothing else is affecting it by zooming
	CBaseEntity *GetFOVOwner( void ) { return m_hZoomOwner; }
	float	GetFOVDistanceAdjustFactor(); // shared between client and server
	float	GetFOVDistanceAdjustFactorForNetworking();

	int		GetImpulse( void ) const { return m_nImpulse; }

	// Movement constraints
	void	ActivateMovementConstraint( CBaseEntity *pEntity, const Vector &vecCenter, float flRadius, float flConstraintWidth, float flSpeedFactor, bool constraintPastRadius = false );
	void	DeactivateMovementConstraint( );

	// talk control
	void	NotePlayerTalked() { m_fLastPlayerTalkTime = gpGlobals->curtime; }
	float	LastTimePlayerTalked() { return m_fLastPlayerTalkTime; }

	void	DisableButtons( int nButtons );
	void	EnableButtons( int nButtons );
	void	ForceButtons( int nButtons );
	void	UnforceButtons( int nButtons );

	//---------------------------------
	// Inputs
	//---------------------------------
	void	InputSetHealth( inputdata_t &inputdata );
	void	InputSetHUDVisibility( inputdata_t &inputdata );

	surfacedata_t *GetSurfaceData( void ) const { return m_pSurfaceData; }
	void SetLadderNormal( Vector vecLadderNormal ) { m_vecLadderNormal = vecLadderNormal; }
	const Vector &GetLadderNormal( void ) const { return m_vecLadderNormal; }
	// If a derived class extends ImpulseCommands() and changes an existing impulse, it'll need to clear out the impulse.
	void ClearImpulse( void ) { m_nImpulse = 0; }

	// Here so that derived classes can use the expresser
	virtual CAI_Expresser *GetExpresser() { return NULL; };

#if !defined(NO_STEAM)
	//----------------------------
	// Steam handling
	bool		GetSteamID( CSteamID *pID, bool bRequireFullyAuthenticated = false );
	uint64		GetSteamIDAsUInt64( void );
#endif

	void				IncrementEFNoInterpParity();
	int					GetEFNoInterpParity() const;

private:
	
	// For queueing up CUserCmds and running them from PhysicsSimulate
	int					GetCommandContextCount( void ) const;
	CCommandContext		*GetCommandContext( int index );
	CCommandContext		*AllocCommandContext( void );
	void				RemoveCommandContext( int index );
	void				RemoveAllCommandContexts( void );
	CCommandContext		*RemoveAllCommandContextsExceptNewest( void );
	void				ReplaceContextCommands( CCommandContext *ctx, CUserCmd *pCommands, int nCommands );

	int					DetermineSimulationTicks( void );
	void				AdjustPlayerTimeBase( int simulation_ticks );
	void				UpdateSplitScreenAndPictureInPicturePlayerList();

public:
	
	// Used by gamemovement to check if the entity is stuck.
	int m_StuckLast;
	
	// FIXME: Make these protected or private!
	

	CNetworkVar( float, m_flDuckAmount );
	CNetworkVar( float, m_flDuckSpeed );
	Vector2D m_vecLastPositionAtFullCrouchSpeed;

	int m_nSuicides;

	// This player's data that should only be replicated to 
	//  the player and not to other players.
	CNetworkVarEmbedded( CPlayerLocalData, m_Local );

	CNetworkVarEmbedded( fogplayerparams_t, m_PlayerFog );
	void InitFogController( void );
	void InputSetFogController( inputdata_t &inputdata );

	void OnTonemapTriggerStartTouch( CTonemapTrigger *pTonemapTrigger );
	void OnTonemapTriggerEndTouch( CTonemapTrigger *pTonemapTrigger );
	CUtlVector< CHandle< CTonemapTrigger > > m_hTriggerTonemapList;

	CNetworkHandle( CPostProcessController, m_hPostProcessCtrl );	// active postprocessing controller
	CNetworkHandle( CColorCorrection, m_hColorCorrectionCtrl );		// active FXVolume color correction
	void InitPostProcessController( void );
	void InputSetPostProcessController( inputdata_t &inputdata );
	void InitColorCorrectionController( void );
	void InputSetColorCorrectionController( inputdata_t &inputdata );

	// Used by env_soundscape_triggerable to manage when the player is touching multiple
	// soundscape triggers simultaneously.
	// The one at the HEAD of the list is always the current soundscape for the player.
	CUtlVector<EHANDLE> m_hTriggerSoundscapeList;

	// Player data that's sometimes needed by the engine
	CNetworkVarEmbedded( CPlayerState, pl );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecViewOffset );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_flFriction );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iAmmo );
	
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_hGroundEntity );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_lifeState );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecBaseVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nNextThinkTick );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nWaterLevel );

	CNetworkVar( int, m_iCoachingTeam );	// When on TEAM_SPECTATOR, is this player restricted to a team, aka 'coaching' a team
	
	int						m_nButtons;
	int						m_afButtonPressed;
	int						m_afButtonReleased;
	int						m_afButtonLast;
	int						m_afButtonDisabled;	// A mask of input flags that are cleared automatically
	int						m_afButtonForced;	// These are forced onto the player's inputs

	CNetworkVar( bool, m_fOnTarget );		//Is the crosshair on a target?

	char					m_szAnimExtension[32];

	int						m_nUpdateRate;		// user snapshot rate cl_updaterate
	float					m_fLerpTime;		// users cl_interp
	bool					m_bLagCompensation;	// user wants lag compenstation
	bool					m_bPredictWeapons; //  user has client side predicted weapons
	bool					m_bPredictionEnabled; // user has prediction enabled
	
	float		GetDeathTime( void ) { return m_flDeathTime; }

	void		ClearZoomOwner( void );

	void		SetPreviouslyPredictedOrigin( const Vector &vecAbsOrigin );
	const Vector &GetPreviouslyPredictedOrigin() const;
	float		GetFOVTime( void ){ return m_flFOVTime; }

	void		AdjustDrownDmg( int nAmount );

private:

	Activity				m_Activity;

protected:

	virtual void			CalcPlayerView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void					CalcVehicleView( IServerVehicle *pVehicle, Vector& eyeOrigin, QAngle& eyeAngles, 	
								float& zNear, float& zFar, float& fov );
	void					CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void					CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles);

	// FIXME: Make these private! (tf_player uses them)

	// Secondary point to derive PVS from when zoomed in with binoculars/sniper rifle.  The PVS is 
	//  a merge of the standing origin and this additional origin
	Vector					m_vecAdditionalPVSOrigin; 
	// Extra PVS origin if we are using a camera object
	Vector					m_vecCameraPVSOrigin;

	bool					m_bIsSpecLerping;
	float					m_flSpecLerpTime;
	float					m_flSpecLerpEndTime;
	Vector					m_vecSpecLerpIdealPos;
	QAngle					m_angSpecLerpIdealAng;
	Vector					m_vecSpecLerpOldPos;
	QAngle					m_angSpecLerpOldAng;

	bool					m_bDropEnabled;
	bool					m_bDuckEnabled;
	CNetworkHandle( CBaseEntity, m_hUseEntity );			// the player is currently controlling this entity because of +USE latched, NULL if no entity

	int						m_iTrain;				// Train control position

	float					m_iRespawnFrames;	// used in PlayerDeathThink() to make sure players can always respawn
 	CNetworkVar( unsigned int, m_afPhysicsFlags );	// physics flags - set when 'normal' physics should be revisited or overriden
	
	// Vehicles
	CNetworkHandle( CBaseEntity, m_hVehicle );

	int						m_iVehicleAnalogBias;

	void					UpdateButtonState( int nUserCmdButtonMask );

	bool	m_bPauseBonusProgress;
	CNetworkVar( int, m_iBonusProgress );
	CNetworkVar( int, m_iBonusChallenge );

	float m_flTimeLastTouchedGround;

	int						m_lastDamageAmount;		// Last damage taken
	float					m_fTimeLastHurt;

	Vector					m_DmgOrigin;
	float					m_DmgTake;
	float					m_DmgSave;
	int						m_bitsDamageType;	// what types of damage has player taken
	int						m_bitsHUDDamage;	// Damage bits for the current fame. These get sent to the hud via gmsgDamage

	CNetworkVar( float, m_flDeathTime );		// the time at which the player died  (used in PlayerDeathThink())
	float					m_flDeathAnimTime;	// the time at which the player finished their death anim (used in PlayerDeathThink() and ShouldTransmit())

	CNetworkVar( float, m_fForceTeam );			// the time at which the player will be forced onto a team ( use in PlayerForceTeamThink())

	CNetworkVar( int, m_iObserverMode );	// if in spectator mode != 0
	CNetworkVar( bool, m_bActiveCameraMan );		// the player is an active cameraman for gotv viewers.
	CNetworkVar( bool, m_bCameraManXRay );			// XRay state for cameraman
	CNetworkVar( bool, m_bCameraManOverview );		// Overview state for cameraman
	CNetworkVar( bool, m_bCameraManScoreBoard );	// ScoreBoard state for cameraman
	CNetworkVar( int, m_uCameraManGraphs );		// Graphs state for cameraman
	CNetworkVar( int,	m_iFOV );			// field of view
	CNetworkVar( int,	m_iDefaultFOV );	// default field of view
	CNetworkVar( int,	m_iFOVStart );		// What our FOV started at
	CNetworkVar( float,	m_flFOVTime );		// Time our FOV change started
	
	int						m_iObserverLastMode; // last used observer mode
	CNetworkHandle( CBaseEntity, m_hObserverTarget );	// entity handle to m_iObserverTarget
	bool					m_bForcedObserverMode; // true, player was forced by invalid targets to switch mode
	
	CNetworkHandle( CBaseEntity, m_hZoomOwner );	//This is a pointer to the entity currently controlling the player's zoom
													//Only this entity can change the zoom state once it has ownership

	float					m_tbdPrev;				// Time-based damage timer
	int						m_idrowndmg;			// track drowning damage taken
	int						m_idrownrestored;		// track drowning damage restored
	int						m_nPoisonDmg;			// track recoverable poison damage taken
	int						m_nPoisonRestored;		// track poison damage restored
	// NOTE: bits damage type appears to only be used for time-based damage
	BYTE					m_rgbTimeBasedDamage[CDMG_TIMEBASED];

	// Player Physics Shadow
	CNetworkVar( int, m_vphysicsCollisionState );

	virtual int SpawnArmorValue( void ) const { return 0; }

	float					m_fNextSuicideTime; // the time after which the player can next use the suicide command
	int						m_iSuicideCustomKillFlags;

	// Replay mode	
	float					m_fDelay;			// replay delay in seconds
	float					m_fReplayEnd;		// time to stop replay mode
	int						m_iReplayEntity;	// follow this entity in replay

	virtual void UpdateTonemapController( void );
	CNetworkHandle( CBaseEntity, m_hTonemapController );

	bool m_bKilledByHeadshot;

public:
	CNetworkVar( int, m_iDeathPostEffect );		// which deathcam post effect to use


private:
	void HandleFuncTrain();

// DATA
private:
	CUtlVector< CCommandContext > m_CommandContext;
	// Player Physics Shadow

protected: //used to be private, but need access for portal mod (Dave Kircher)
	IPhysicsPlayerController	*m_pPhysicsController;
	IPhysicsObject				*m_pShadowStand;
	IPhysicsObject				*m_pShadowCrouch;
	Vector						m_oldOrigin;
	Vector						m_vecSmoothedVelocity;
	bool						m_bTouchedPhysObject;
	bool						m_bPhysicsWasFrozen;
	CNetworkVar( float, m_flNextDecalTime );	// next time this player can spray a decal
	bool			m_bNextDecalTimeExpedited;	// whether decal time was shortened already

private:

	int						m_iPlayerSound;// the index of the sound list slot reserved for this player
	int						m_iTargetVolume;// ideal sound volume. 
	
	int						m_rgItems[MAX_ITEMS];

	// Voting info
	IntervalTimer 			m_lastHeldVoteTimer;	///< How long since we last created a vote.  Prevents vote spam.

	// these are time-sensitive things that we keep track of
	float					m_flSwimTime;		// how long player has been underwater
	float					m_flDuckTime;		// how long we've been ducking
	float					m_flDuckJumpTime;	

	float					m_flSuitUpdate;					// when to play next suit update
	int						m_rgSuitPlayList[CSUITPLAYLIST];// next sentencenum to play for suit update
	int						m_iSuitPlayNext;				// next sentence slot for queue storage;
	int						m_rgiSuitNoRepeat[CSUITNOREPEAT];		// suit sentence no repeat list
	float					m_rgflSuitNoRepeatTime[CSUITNOREPEAT];	// how long to wait before allowing repeat

	float					m_flgeigerRange;		// range to nearest radiation source
	float					m_flgeigerDelay;		// delay per update of range msg to client
	int						m_igeigerRangePrev;

	bool					m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	bool					m_fGameHUDInitialized;
	bool					m_fWeapon;				// Set this to FALSE to force a reset of the current weapon HUD info

	int						m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int						m_iClientBattery;	// the Battery currently known by the client.  If this changes, send a new

	int						m_lastx, m_lasty;	// These are the previous update's crosshair angles, DON"T SAVE/RESTORE

	// Autoaim data
	QAngle					m_vecAutoAim;

	// Team Handling
	// char					m_szTeamName[TEAM_NAME_LENGTH];

	// Multiplayer handling
	PlayerConnectedState	m_iConnected;

	// from edict_t
	// CBasePlayer doesn't send this but CCSPlayer does.
	CNetworkVarForDerived( int, m_ArmorValue );
	float					m_AirFinished;
	float					m_PainFinished;

	// player locking
	int						m_iPlayerLocked;

	CSimpleSimTimer			m_AutoaimTimer;

protected:
	int						m_iFrags;
	int						m_iAssists;
	int						m_iDeaths;

	// the player's personal view model
	typedef CHandle<CBaseViewModel> CBaseViewModelHandle;
	CNetworkArray( CBaseViewModelHandle, m_hViewModel, MAX_VIEWMODELS );

	// Last received usercmd (in case we drop a lot of packets )
	CUserCmd				m_LastCmd;
	CUserCmd				*m_pCurrentCommand;
	int						m_iLockViewanglesTickNumber;
	QAngle					m_qangLockViewangles;

	float					m_flStepSoundTime;	// time to check for next footstep sound

	bool					m_bAllowInstantSpawn;

	// Input device info
	InputDevice_t			m_PlayerInputDevice;
	InputDevicePlatform_t	m_PlayerPlatform;
	float					m_lastRequestedClientInfoTime;

private:

// Replicated to all clients
	CNetworkVar( float, m_flMaxspeed );
	CNetworkVar( int, m_ladderSurfaceProps );
	CNetworkVector( m_vecLadderNormal );	// Clients may need this for climbing anims
	
protected:
// Not transmitted
	float					m_flWaterJumpTime;  // used to be called teleport_time
	Vector					m_vecWaterJumpVel;
	int						m_nImpulse;
	float					m_flSwimSoundTime;
	float					m_ignoreLadderJumpTime;
	bool					m_bHasWalkMovedSinceLastJump;
	
private:
	float					m_flFlashTime;
	int						m_nDrownDmgRate;		// Drowning damage in points per second without air.

	int						m_nNumCrouches;			// Number of times we've crouched (for hinting)
	bool					m_bDuckToggled;		// If true, the player is crouching via a toggle

public:
	bool					GetToggledDuckState( void ) { return m_bDuckToggled; }
	void					ToggleDuck( void );
	float					GetStickDist( void );

	float					m_flForwardMove;
	float					m_flSideMove;
	int						m_nNumCrateHudHints;

private:

	// Used in test code to teleport the player to random locations in the map.
	Vector					m_vForcedOrigin;
	bool					m_bForceOrigin;	

	// Clients try to run on their own realtime clock, this is this client's clock
	CNetworkVar( int, m_nTickBase );

	bool					m_bGamePaused;
	float					m_fLastPlayerTalkTime;
	
	CNetworkVar( CBaseCombatWeaponHandle, m_hLastWeapon );

#if !defined( NO_ENTITY_PREDICTION )
	CUtlVector< CHandle< CBaseEntity > > m_SimulatedByThisPlayer;
#endif

	float					m_flOldPlayerZ;
	float					m_flOldPlayerViewOffsetZ;

	bool					m_bPlayerUnderwater;

	CNetworkHandle( CBaseEntity, m_hViewEntity );
	CNetworkVar( bool, m_bShouldDrawPlayerWhileUsingViewEntity );

	// Movement constraints
	CNetworkHandle( CBaseEntity, m_hConstraintEntity );
	CNetworkVector( m_vecConstraintCenter );
	CNetworkVar( float, m_flConstraintRadius );
	CNetworkVar( float, m_flConstraintWidth );
	CNetworkVar( float, m_flConstraintSpeedFactor );
	CNetworkVar( bool, m_bConstraintPastRadius );

	friend class CPlayerMove;
	friend class CPlayerClass;
	friend class CASW_PlayerMove;
	friend class CDOTAPlayerMove;
	friend class CPaintPlayerMove;

	// Player name
	char					m_szNetname[MAX_PLAYER_NAME_LENGTH];

protected:
	// HACK FOR TF2 Prediction
	friend class CTFGameMovementRecon;
	friend class CGameMovement;
	friend class CTFGameMovement;
	friend class CHL1GameMovement;
	friend class CCSGameMovement;	
	friend class CHL2GameMovement;
	friend class CPortalGameMovement;
	friend class CASW_MarineGameMovement;
	
	// Accessors for gamemovement
	bool IsDucked( void ) const { return m_Local.m_bDucked; }
	bool IsDucking( void ) const { return m_Local.m_bDucking; }
	float GetStepSize( void ) const { return m_Local.m_flStepSize; }

	CNetworkVar( float,  m_flLaggedMovementValue );

	// These are generated while running usercmds, then given to UpdateVPhysicsPosition after running all queued commands.
#if defined( DEBUG_MOTION_CONTROLLERS )
	CNetworkVector( m_vNewVPhysicsPosition );
	CNetworkVector( m_vNewVPhysicsVelocity );
#else
	Vector m_vNewVPhysicsPosition;
	Vector m_vNewVPhysicsVelocity;
#endif
	
	Vector	m_vecVehicleViewOrigin;		// Used to store the calculated view of the player while riding in a vehicle
	QAngle	m_vecVehicleViewAngles;		// Vehicle angles
	float	m_flVehicleViewFOV;			// FOV of the vehicle driver
	int		m_nVehicleViewSavedFrame;	// Used to mark which frame was the last one the view was calculated for

	Vector m_vecPreviouslyPredictedOrigin; // Used to determine if non-gamemovement game code has teleported, or tweaked the player's origin
	int		m_nBodyPitchPoseParam;

	CNetworkString( m_szLastPlaceName, MAX_PLACE_NAME_LENGTH );
	unsigned int    m_nTicksSinceLastPlaceUpdate;

	char m_szNetworkIDString[MAX_NETWORKID_LENGTH];
	CPlayerInfo m_PlayerInfo;

	// Texture names and surface data, used by CGameMovement
	int				m_surfaceProps;
	surfacedata_t*	m_pSurfaceData;
	float			m_surfaceFriction;
	char			m_chTextureType;
	char			m_chPreviousTextureType;	// Separate from m_chTextureType. This is cleared if the player's not on the ground.

	bool			m_bSinglePlayerGameEnding;

	CNetworkVar( int, m_ubEFNoInterpParity );

	EHANDLE			m_hPlayerProxy;	// Handle to a player proxy entity for quicker reference

public:

	float  GetLaggedMovementValue( void ){ return m_flLaggedMovementValue;	}
	void   SetLaggedMovementValue( float flValue ) { m_flLaggedMovementValue = flValue;	}

	inline bool IsAutoKickDisabled( void ) const;
	inline void DisableAutoKick( bool disabled );

	void	DumpPerfToRecipient( CBasePlayer *pRecipient, int nMaxRecords );

	// picker debug utility functions
	virtual CBaseEntity*	FindEntityClassForward( char *classname );
	virtual CBaseEntity*	FindEntityForward( bool fHull );
	virtual CBaseEntity*	FindPickerEntityClass( char *classname );
	virtual CBaseEntity*	FindPickerEntity();
	virtual CAI_Node*		FindPickerAINode( int nNodeType );
	virtual CAI_Link*		FindPickerAILink();


	void PrepareForFullUpdate( void );

	virtual void OnSpeak( CBasePlayer *actor, const char *sound, float duration ) {}

	// A voice packet from this client was received by the server
	virtual void OnVoiceTransmit( void ) {}

	float GetRemainingMovementTimeForUserCmdProcessing() const { return m_flMovementTimeForUserCmdProcessingRemaining; }
	float ConsumeMovementTimeForUserCmdProcessing( float flTimeNeeded )
	{
		if ( m_flMovementTimeForUserCmdProcessingRemaining <= 0.0f )
		{
			return 0.0f;
		}
		else if ( flTimeNeeded > m_flMovementTimeForUserCmdProcessingRemaining + FLT_EPSILON )
		{
			float flResult = m_flMovementTimeForUserCmdProcessingRemaining;
			m_flMovementTimeForUserCmdProcessingRemaining = 0.0f;
			return flResult;
		}
		else
		{
			m_flMovementTimeForUserCmdProcessingRemaining -= flTimeNeeded;
			if ( m_flMovementTimeForUserCmdProcessingRemaining < 0.0f )
				m_flMovementTimeForUserCmdProcessingRemaining = 0.0f;
			return flTimeNeeded;
		}
	}

private:
	// How much of a movement time buffer can we process from this user?
	float				m_flMovementTimeForUserCmdProcessingRemaining;

public:
	float				GetInitialSpawnTime() const { return m_flInitialSpawnTime; }
private:
	float				m_flInitialSpawnTime;

	bool m_autoKickDisabled;

	struct StepSoundCache_t
	{
		StepSoundCache_t() : m_usSoundNameIndex( 0 ) {}
		CSoundParameters	m_SoundParameters;
		unsigned short		m_usSoundNameIndex;
	};
	// One for left and one for right side of step
	StepSoundCache_t		m_StepSoundCache[ 2 ];

	CUtlLinkedList< CPlayerSimInfo >  m_vecPlayerSimInfo;
	CUtlLinkedList< CPlayerCmdInfo >  m_vecPlayerCmdInfo;

	IntervalTimer m_weaponFiredTimer;

	friend class CMoveHelperServer;
	Vector m_movementCollisionNormal;
	Vector m_groundNormal;
	CHandle< CBaseCombatCharacter > m_stuckCharacter;

	// If true, the m_hSplitOwner points to who owns us
	bool			m_bSplitScreenPlayer;
	CHandle< CBasePlayer > m_hSplitOwner;
	// If we have any attached split users, this is the list of them
	CUtlVector< CHandle< CBasePlayer > > m_hSplitScreenPlayers;
	CUtlVector< CHandle< CBasePlayer > > m_hSplitScreenAndPipPlayers;
	CUtlVector< CHandle< CBasePlayer > > m_hPipPlayers;

	CrossPlayPlatform_t m_ClientPlatform;

public:
	float GetAirTime( void );

private:
	float	GetAutoaimScore( const Vector &eyePosition, const Vector &viewDir, const Vector &vecTarget, CBaseEntity *pTarget, float fScale, CBaseCombatWeapon *pActiveWeapon );
	QAngle	AutoaimDeflection( Vector &vecSrc, autoaim_params_t &params );

public:
	virtual unsigned int PlayerSolidMask( bool brushOnly = false ) const;	// returns the solid mask for the given player, so bots can have a more-restrictive set
#if defined( DEBUG_MOTION_CONTROLLERS )
	CNetworkVector( m_Debug_vPhysPosition );
	CNetworkVector( m_Debug_vPhysVelocity );
	CNetworkVector( m_Debug_LinearAccel );
#endif

private:
	// Eye/angle offsets - used for TrackIR and motion controllers
	Vector							m_vecEyeOffset;
	QAngle							m_EyeAngleOffset;
	Vector							m_AimDirection;
};

typedef CHandle<CBasePlayer> CBasePlayerHandle;

EXTERN_SEND_TABLE(DT_BasePlayer)



//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline const Vector &CBasePlayer::GetMovementCollisionNormal( void ) const
{
	return m_movementCollisionNormal;
}

inline const Vector &CBasePlayer::GetGroundNormal( void ) const
{
	return m_groundNormal;
}

inline bool CBasePlayer::IsAutoKickDisabled( void ) const
{
	return m_autoKickDisabled;
}

inline void CBasePlayer::DisableAutoKick( bool disabled )
{
	m_autoKickDisabled = disabled;
}

inline void CBasePlayer::SetAdditionalPVSOrigin( const Vector &vecOrigin ) 
{ 
	m_vecAdditionalPVSOrigin = vecOrigin; 
}

inline void CBasePlayer::SetCameraPVSOrigin( const Vector &vecOrigin ) 
{ 
	m_vecCameraPVSOrigin = vecOrigin; 
}

inline void CBasePlayer::SetMuzzleFlashTime( float flTime ) 
{ 
	m_flFlashTime = flTime; 
}

inline void CBasePlayer::SetDropEnabled( bool bEnabled ) 
{ 
	m_bDropEnabled = bEnabled; 
}

inline void CBasePlayer::SetDuckEnabled( bool bEnabled ) 
{ 
	m_bDuckEnabled = bEnabled; 
}

// Bot accessors...
inline void CBasePlayer::SetTimeBase( float flTimeBase ) 
{ 
	m_nTickBase = TIME_TO_TICKS( flTimeBase ); 
}

inline void CBasePlayer::SetLastUserCommand( const CUserCmd &cmd ) 
{ 
	m_LastCmd = cmd; 
}

inline CUserCmd const *CBasePlayer::GetLastUserCommand( void )
{
	return &m_LastCmd;
}

inline bool CBasePlayer::IsPredictingWeapons( void ) const 
{
	return m_bPredictWeapons;
}

inline int CBasePlayer::CurrentCommandNumber() const
{
	Assert( m_pCurrentCommand );
	if ( !m_pCurrentCommand )
		return 0;
	return m_pCurrentCommand->command_number;
}

inline const CUserCmd *CBasePlayer::GetCurrentUserCommand() const
{
	Assert( m_pCurrentCommand );
	return m_pCurrentCommand;
}

inline IServerVehicle *CBasePlayer::GetVehicle() 
{ 
	CBaseEntity *pVehicleEnt = m_hVehicle.Get();
	return pVehicleEnt ? pVehicleEnt->GetServerVehicle() : NULL;
}

inline CBaseEntity *CBasePlayer::GetVehicleEntity() 
{ 
	return m_hVehicle.Get();
}

inline bool CBasePlayer::IsInAVehicle( void ) const 
{ 
	return ( NULL != m_hVehicle.Get() ) ? true : false; 
}

inline void CBasePlayer::SetTouchedPhysics( bool bTouch ) 
{ 
	m_bTouchedPhysObject = bTouch; 
}

inline bool CBasePlayer::TouchedPhysics( void )			
{ 
	return m_bTouchedPhysObject; 
}

//-----------------------------------------------------------------------------
// Converts an entity to a player
//-----------------------------------------------------------------------------
inline CBasePlayer *ToBasePlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#if _DEBUG
	Assert( static_cast< CBasePlayer* >( pEntity ) == dynamic_cast< CBasePlayer* >( pEntity ) );
#endif

	return static_cast<CBasePlayer *>( pEntity );
}

inline const CBasePlayer *ToBasePlayer( const CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#if _DEBUG
	Assert( static_cast< const CBasePlayer* >( pEntity ) == dynamic_cast< const CBasePlayer* >( pEntity ) );
#endif

	return static_cast< const CBasePlayer * >( pEntity );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * DEPRECATED: Use CollectPlayers() instead.
 * Iterate over all active players in the game, invoking functor on each.
 * If functor returns false, stop iteration and return false.
 */
template < typename Functor >
bool ForEachPlayer( Functor &func )
{
	VPROF("ForEachPlayer");
	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		if (FNullEnt( player->edict() ))
			continue;

		if (!player->IsPlayer())
			continue;

		if( !player->IsConnected() )
			continue;

		if (func( player ) == false)
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------------------------
/**
 * The interface for an iterative player functor
 */
class IPlayerFunctor
{
public:
	virtual void OnBeginIteration( void )						{ }		// invoked once before iteration begins
	
	virtual bool operator() ( CBasePlayer *player ) = 0;
	
	virtual void OnEndIteration( bool allElementsIterated )		{ }		// invoked once after iteration is complete whether successful or not
};


//--------------------------------------------------------------------------------------------------------------
/**
 * DEPRECATED: Use CollectPlayers() instead.
 * Specialization of ForEachPlayer template for IPlayerFunctors
 */
template <>
inline bool ForEachPlayer( IPlayerFunctor &func )
{
	VPROF("ForEachPlayer");
	func.OnBeginIteration();
	
	bool isComplete = true;
	
	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		if (FNullEnt( player->edict() ))
			continue;

		if (!player->IsPlayer())
			continue;

		if( !player->IsConnected() )
			continue;

		if (func( player ) == false)
		{
			isComplete = false;
			break;
		}
	}
	
	func.OnEndIteration( isComplete );

	return isComplete;
}



//--------------------------------------------------------------------------------------------------------------
//
// Collect all valid, connected players into given vector.
// Returns number of players collected.
//
#define COLLECT_ONLY_LIVING_PLAYERS true
#define APPEND_PLAYERS true
template < typename T >
int CollectPlayers( CUtlVector< T * > *playerVector, int team = TEAM_ANY, bool isAlive = false, bool shouldAppend = false )
{
	if ( !shouldAppend )
	{
		playerVector->RemoveAll();
	}

	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		T *player = static_cast< T * >( UTIL_PlayerByIndex( i ) );

		if ( player == NULL )
			continue;

		if ( FNullEnt( player->edict() ) )
			continue;

		if ( !player->IsPlayer() )
			continue;

		if ( !player->IsConnected() )
			continue;

		if ( team != TEAM_ANY && player->GetTeamNumber() != team )
			continue;

		if ( isAlive && !player->IsAlive() )
			continue;

		playerVector->AddToTail( player );
	}

	return playerVector->Count();
}


enum
{
	VEHICLE_ANALOG_BIAS_NONE = 0,
	VEHICLE_ANALOG_BIAS_FORWARD,
	VEHICLE_ANALOG_BIAS_REVERSE,
};

// Used on player entities - only sends the data to the local player (objectID-1).
void* SendProxy_SendLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
void* SendProxy_SendNonLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );

// Helper class
class CAutoUserMessageThrottle
{
public:
	CAutoUserMessageThrottle( CBasePlayer *pPlayer, char const *pchMessages[], int nNumMessage ) :
	  m_pPlayer( pPlayer )
	  {
		  if ( m_pPlayer )
		  {
			  m_pPlayer->StartUserMessageThrottling( pchMessages, nNumMessage );
		  }
	  }

	  ~CAutoUserMessageThrottle()
	  {
		  if ( m_pPlayer )
		  {
			  m_pPlayer->FinishUserMessageThrottling();
		  }
	  }
private:

	CBasePlayer	*m_pPlayer;
};

#endif // PLAYER_H
