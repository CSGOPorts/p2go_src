//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef COMBATWEAPON_SHARED_H
#define COMBATWEAPON_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "sharedInterface.h"
#include "vphysics_interface.h"
#include "predictable_entity.h"
#include "soundflags.h"
#include "weapon_parse.h"
#include "baseviewmodel_shared.h"
#include "weapon_proficiency.h"

#if defined( CLIENT_DLL )
#undef CBaseCombatWeapon
#define CBaseCombatWeapon C_BaseCombatWeapon
#undef CBaseWeaponWorldModel
#define CBaseWeaponWorldModel C_BaseWeaponWorldModel
#endif

#if !defined( CLIENT_DLL )
extern void OnBaseCombatWeaponCreated( CBaseCombatWeapon * );
extern void OnBaseCombatWeaponDestroyed( CBaseCombatWeapon * );

void *SendProxy_SendLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
#endif

class CBasePlayer;
class CBaseCombatCharacter;
class IPhysicsConstraint;
class CUserCmd;

// How many times to display altfire hud hints (per weapon)
#define WEAPON_ALTFIRE_HUD_HINT_COUNT	1
#define WEAPON_RELOAD_HUD_HINT_COUNT	1

//Start with a constraint in place (don't drop to floor)
#define	SF_WEAPON_START_CONSTRAINED	(1<<0)	
#define SF_WEAPON_NO_PLAYER_PICKUP	(1<<1)
#define SF_WEAPON_NO_PHYSCANNON_PUNT (1<<2)

//Percent
#define	CLIP_PERC_THRESHOLD		0.75f	

// Put this in your derived class definition to declare it's activity table
// UNDONE: Cascade these?
#define DECLARE_ACTTABLE()		static acttable_t m_acttable[];\
	acttable_t *ActivityList( void );\
	int ActivityListCount( void );

// You also need to include the activity table itself in your class' implementation:
// e.g.
//	acttable_t	CWeaponStunstick::m_acttable[] = 
//	{
//		{ ACT_MELEE_ATTACK1, ACT_MELEE_ATTACK_SWING, TRUE },
//	};
//
// The stunstick overrides the ACT_MELEE_ATTACK1 activity, replacing it with ACT_MELEE_ATTACK_SWING.
// This animation is required for this weapon's operation.
//

// Put this after your derived class' definition to implement the accessors for the
// activity table.
// UNDONE: Cascade these?
#define IMPLEMENT_ACTTABLE(className) \
	acttable_t *className::ActivityList( void ) { return m_acttable; } \
	int className::ActivityListCount( void ) { return ARRAYSIZE(m_acttable); } \

typedef struct
{
	int			baseAct;
	int			weaponAct;
	bool		required;
} acttable_t;

class CHudTexture;
class Color;

namespace vgui2
{
	typedef unsigned long HFont;
}

#define MWHEEL_UP		 1
#define MWHEEL_DOWN		-1


// -----------------------------------------
//	Vector cones
// -----------------------------------------
// VECTOR_CONE_PRECALCULATED - this resolves to vec3_origin, but adds some
// context indicating that the person writing the code is not allowing
// FireBullets() to modify the direction of the shot because the shot direction
// being passed into the function has already been modified by another piece of
// code and should be fired as specified. See GetActualShotTrajectory(). 

// NOTE: The way these are calculated is that each component == sin (degrees/2)
#define VECTOR_CONE_PRECALCULATED	vec3_origin
#define VECTOR_CONE_1DEGREES		Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES		Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES		Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES		Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES		Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES		Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES		Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES		Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES		Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES		Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES		Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES		Vector( 0.17365, 0.17365, 0.17365 )

//-----------------------------------------------------------------------------
// Purpose: Base weapon class, shared on client and server
//-----------------------------------------------------------------------------

#define BASECOMBATWEAPON_DERIVED_FROM		CBaseAnimating

// temp states for modular weapon body groups
#define MODULAR_BODYGROUPS_DEFAULT_NONE_SET		0
#define MODULAR_BODYGROUPS_NONE_AVAILABLE		1
#define MODULAR_BODYGROUPS_RANDOMIZED			2

enum WeaponHoldsPlayerAnimCapability_t
{
	WEAPON_PLAYER_ANIMS_UNKNOWN = 0,
	WEAPON_PLAYER_ANIMS_AVAILABLE,
	WEAPON_PLAYER_ANIMS_NOT_AVAILABLE
};

enum WeaponModelClassification_t
{
	WEAPON_MODEL_IS_UNCLASSIFIED = 0,
	WEAPON_MODEL_IS_VIEWMODEL,
	WEAPON_MODEL_IS_WORLDMODEL,
	WEAPON_MODEL_IS_DROPPEDMODEL,
	WEAPON_MODEL_IS_UNRECOGNIZED
};

class CBaseWeaponWorldModel : public CBaseAnimatingOverlay
{
	DECLARE_CLASS( CBaseWeaponWorldModel, CBaseAnimatingOverlay );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

public:
	CBaseWeaponWorldModel();
	~CBaseWeaponWorldModel();
	void SetOwningWeapon( CBaseCombatWeapon *pWeaponParent );
	void ShowWorldModel( bool bVisible );
	bool HoldsPlayerAnimations( void );

#ifdef CLIENT_DLL
	void ApplyCustomMaterialsAndStickers( void );
	virtual void	FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual bool	ShouldDraw( void ) OVERRIDE;
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	float * GetRenderClipPlane( void );
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	virtual bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	virtual bool IsFollowingEntity() { return true; } // weapon world models are ALWAYS carried by players

#else
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual int  ShouldTransmit( const CCheckTransmitInfo *pInfo ) OVERRIDE;
	virtual int	UpdateTransmitState() OVERRIDE;
#endif

	virtual bool	IsWeaponWorldModel( void ) const { return true; };

	void ValidateParent( void );

	int GetLeftHandAttachBoneIndex();
	int GetRightHandAttachBoneIndex();
	int GetMuzzleAttachIndex();
	int GetMuzzleBoneIndex();
	void ResetCachedBoneIndices();

	bool HasDormantOwner( void );

	typedef CHandle<CBaseCombatWeapon> CBaseCombatWeaponHandle;
	CNetworkVar( CBaseCombatWeaponHandle, m_hCombatWeaponParent );

private:
	WeaponHoldsPlayerAnimCapability_t m_nHoldsPlayerAnims;
	
	int m_nLeftHandAttachBoneIndex;
	int m_nRightHandAttachBoneIndex;
	int m_nMuzzleAttachIndex;
	int m_nMuzzleBoneIndex;

#ifdef CLIENT_DLL
	bool m_bStickersApplied;
#endif

};


//-----------------------------------------------------------------------------
// Purpose: Client side rep of CBaseTFCombatWeapon 
//-----------------------------------------------------------------------------
// Hacky
class CBaseCombatWeapon : public BASECOMBATWEAPON_DERIVED_FROM
{
public:
	DECLARE_CLASS( CBaseCombatWeapon, BASECOMBATWEAPON_DERIVED_FROM );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

							CBaseCombatWeapon();
	virtual 				~CBaseCombatWeapon();

	// Get unique weapon ID
	// FIXMEL4DTOMAINMERGE
	// We might have to disable this code in main until we refactor all weapons to use this system, as it's a pretty good perf boost
	virtual int GetWeaponID( void ) const		{ return 0; }

	const CEconItemView*	GetEconItemView( void ) const;
	CEconItemView*			GetEconItemView( void );

	virtual bool			IsBaseCombatWeapon( void ) const { return true; }
	virtual CBaseCombatWeapon *MyCombatWeaponPointer( void ) { return this; }

	// A derived weapon class should return true here so that weapon sounds, etc, can
	//  apply the proper filter
	virtual bool			IsPredicted( void ) const { return false; }

	virtual void			Spawn( void );
	virtual void			Precache( void );

	void					MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );

	// Subtypes are used to manage multiple weapons of the same type on the player.
	virtual int				GetSubType( void ) { return m_iSubType; }
	virtual void			SetSubType( int iType ) { m_iSubType = iType; }

	virtual void			Equip( CBaseCombatCharacter *pOwner );
	virtual void			Drop( const Vector &vecVelocity );

	virtual	int				UpdateClientData( CBasePlayer *pPlayer );

	virtual bool			IsAllowedToSwitch( void );
	virtual bool			CanBeSelected( void );
	virtual bool			VisibleInWeaponSelection( void );
	virtual bool			HasAmmo( void );

	// Weapon Pickup For Player
	virtual void			SetPickupTouch( void );
	virtual void 			DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	virtual void			GiveTo( CBaseEntity *pOther );

	// HUD Hints
	virtual bool			ShouldDisplayAltFireHUDHint();
	virtual void			DisplayAltFireHudHint();	
	virtual void			RescindAltFireHudHint(); ///< undisplay the hud hint and pretend it never showed.

	virtual bool			ShouldDisplayReloadHUDHint();
	virtual void			DisplayReloadHudHint();
	virtual void			RescindReloadHudHint();

	// Weapon client handling
	virtual void			SetViewModelIndex( int index = 0 );
	virtual bool			SendWeaponAnim( int iActivity );
	virtual void			SendViewModelAnim( int nSequence );
	float					GetViewModelSequenceDuration();	// Return how long the current view model sequence is.
	bool					IsViewModelSequenceFinished( void ); // Returns if the viewmodel's current animation is finished

	virtual void			SetViewModel();

	virtual bool			HasWeaponIdleTimeElapsed( void );
	virtual void			SetWeaponIdleTime( float time );
	virtual float			GetWeaponIdleTime( void );

	// Weapon selection
	virtual bool			HasAnyAmmo( void );							// Returns true is weapon has ammo
	virtual bool			HasPrimaryAmmo( void );						// Returns true is weapon has ammo
	virtual bool			HasSecondaryAmmo( void );					// Returns true is weapon has ammo
	bool					UsesPrimaryAmmo( void );					// returns true if the weapon actually uses primary ammo
	bool					UsesSecondaryAmmo( void );					// returns true if the weapon actually uses secondary ammo
	void					GiveDefaultAmmo( void );
	
	virtual bool			CanHolster( void ) { return TRUE; };		// returns true if the weapon can be holstered
	virtual bool			DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );
	virtual bool			CanDeploy( void ) { return true; }			// return true if the weapon's allowed to deploy
	virtual bool			Deploy( void );								// returns true is deploy was successful
	virtual bool			Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual CBaseCombatWeapon *GetLastWeapon( void ) { return this; }
	virtual void			SetWeaponVisible( bool visible );
	virtual bool			IsWeaponVisible( void );
	virtual bool			ReloadOrSwitchWeapons( void );
	virtual bool			HolsterOnDetach() { return false; }
	virtual bool			IsHolstered(){ return false; }

	// Weapon behaviour
	virtual void			ItemPreFrame( void );					// called each frame by the player PreThink
	virtual void			ItemPostFrame( void );					// called each frame by the player PostThink
	virtual void			ItemBusyFrame( void );					// called each frame by the player PostThink, if the player's not ready to attack yet
	virtual void			ItemHolsterFrame( void ) {};			// called each frame by the player PreThink, if the weapon is holstered
	virtual void			WeaponIdle( void );						// called when no buttons pressed
	virtual void			HandleFireOnEmpty();					// Called when they have the attack button down
																	// but they are out of ammo. The default implementation
																	// either reloads, switches weapons, or plays an empty sound.

	virtual bool			ShouldBlockPrimaryFire() { return true; }

#ifdef CLIENT_DLL
	virtual void			CreateMove( float flInputSampleTime, CUserCmd *pCmd, const QAngle &vecOldViewAngles ) {}


#endif

	virtual bool			IsZoomed() const { return false; }		// Is this weapon in its 'zoomed in' mode?

	// Reloading
	virtual	void			CheckReload( void );
	virtual void			FinishReload( void );
	virtual void			AbortReload( void );
	virtual bool			Reload( void );
	bool					DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

	// Weapon firing
	virtual void			PrimaryAttack( void );						// do "+ATTACK"
	virtual void			SecondaryAttack( void ) { return; }			// do "+ATTACK2"
	virtual void			BaseForceFire( CBaseCombatCharacter *pOperator, CBaseEntity *pTarget = NULL );

	// Firing animations
	virtual Activity		GetPrimaryAttackActivity( void );
	virtual Activity		GetSecondaryAttackActivity( void );
	virtual Activity		GetDrawActivity( void );
	virtual float			GetDefaultAnimSpeed( void ) { return 1.0; }

	// Bullet launch information
	virtual int				GetBulletType( void );
	virtual const Vector&	GetBulletSpread( void );
	virtual Vector			GetBulletSpread( WeaponProficiency_t proficiency )		{ return GetBulletSpread(); }
	virtual float			GetSpreadBias( WeaponProficiency_t proficiency )			{ return 1.0; }
	virtual float			GetAccuracyFishtail() const { return 0.0f; }
	virtual float			GetFireRate( void );
	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 1; }
	virtual float			GetMinRestTime() { return 0.3; }
	virtual float			GetMaxRestTime() { return 0.6; }
	virtual int				GetRandomBurst() { return random->RandomInt( GetMinBurst(), GetMaxBurst() ); }
	virtual void			WeaponSound( WeaponSound_t sound_type, float soundtime = 0.0f );
	virtual void			StopWeaponSound( WeaponSound_t sound_type );
	virtual const WeaponProficiencyInfo_t *GetProficiencyValues();

	// Autoaim
	virtual float			GetMaxAutoAimDeflection() { return 0.99f; }
	virtual float			WeaponAutoAimScale() { return 1.0f; } // allows a weapon to influence the perceived size of the target's autoaim radius.

	// TF Sprinting functions
	virtual bool			StartSprinting( void ) { return false; };
	virtual bool			StopSprinting( void ) { return false; };

	// TF Injury functions
	virtual float			GetDamage( float flDistance, int iLocation ) { return 0.0; };

	virtual void			SetActivity( Activity act, float duration );
	inline void				SetActivity( Activity eActivity ) { m_Activity = eActivity; }
	inline Activity			GetActivity( void ) { return m_Activity; }

	virtual void			AddViewKick( void );	// Add in the view kick for the weapon

	virtual char			*GetDeathNoticeName( void );	// Get the string to print death notices with

	CBaseCombatCharacter	*GetOwner() const;
	void					SetOwner( CBaseCombatCharacter *owner );
	virtual void			OnPickedUp( CBaseCombatCharacter *pNewOwner );

	virtual void			AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles ) {};
	virtual float			CalcViewmodelBob( void ) { return 0.0f; };

	// Returns information about the various control panels
	virtual void 			GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void			GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );

	virtual bool			ShouldShowControlPanels( void ) { return true; }

	void					Lock( float lockTime, CBaseEntity *pLocker );
	bool					IsLocked( CBaseEntity *pAsker );

	//All weapons can be picked up by NPCs by default
	virtual bool			CanBePickedUpByNPCs( void ) { return true;	}

public:

	// Weapon info accessors for data in the weapon's data file
	const FileWeaponInfo_t	&GetWpnData( void ) const;
	virtual const char		*GetViewModel( int viewmodelindex = 0 ) const;
	virtual const char		*GetWorldModel( void ) const;
	virtual const char		*GetWorldDroppedModel( void ) const;
	virtual const char		*GetAnimPrefix( void ) const;
	virtual int				GetMaxClip1( void ) const { return GetWpnData().GetPrimaryClipSize( GetEconItemView() ); }
	virtual int				GetMaxClip2( void ) const { return GetWpnData().GetSecondaryClipSize( GetEconItemView() ); }
	virtual int				GetDefaultClip1( void ) const { return GetWpnData().GetDefaultPrimaryClipSize( GetEconItemView() ); }
	virtual int				GetDefaultClip2( void ) const { return GetWpnData().GetDefaultSecondaryClipSize( GetEconItemView() ); }
	virtual int				GetReserveAmmoMax( AmmoPosition_t nAmmoPos ) const;
	virtual int				GetWeight( void ) const;
	virtual bool			AllowsAutoSwitchTo( void ) const;
	virtual bool			AllowsAutoSwitchFrom( void ) const;
	virtual int				GetWeaponFlags( void ) const;
	virtual int				GetSlot( void ) const;
	virtual int				GetPosition( void ) const;
	virtual char const		*GetName( void ) const;
	virtual char const		*GetPrintName( void ) const;
	virtual char const		*GetShootSound( int iIndex ) const;
	virtual int				GetRumbleEffect() const;
	virtual bool			UsesClipsForAmmo1( void ) const;
	virtual bool			UsesClipsForAmmo2( void ) const;
	bool					IsMeleeWeapon() const;

	virtual	void			OnMouseWheel( int nDirection ) {}

	// derive this function if you mod uses encrypted weapon info files
	virtual const unsigned char *GetEncryptionKey( void );

	virtual int				GetPrimaryAmmoType( void )  const { return m_iPrimaryAmmoType; }
	virtual int				GetSecondaryAmmoType( void )  const { return m_iSecondaryAmmoType; }
	int						Clip1() const { return m_iClip1; }
	int						Clip2() const { return m_iClip2; }

	// Ammo quantity queries for weapons that do not use clips. These are only
	// used to determine how much ammo is in a weapon that does not have an owner.
	// That is, a weapon that's on the ground for the player to get ammo out of.
	int GetPrimaryAmmoCount() { return m_iPrimaryAmmoCount; }
	void SetPrimaryAmmoCount( int count ) { m_iPrimaryAmmoCount = count; }

	int GetSecondaryAmmoCount() { return m_iSecondaryAmmoCount; }
	void SetSecondaryAmmoCount( int count ) { m_iSecondaryAmmoCount = count; }

	int GetReserveAmmoCount( AmmoPosition_t nAmmoPosition, CBaseCombatCharacter * pForcedOwner = NULL  );
	int SetReserveAmmoCount( AmmoPosition_t nAmmoPosition, int nCount, bool bSuppressSound = false, CBaseCombatCharacter * pOwner = NULL );
	int GiveReserveAmmo( AmmoPosition_t nAmmoPosition, int nCount, bool bSuppressSound = false, CBaseCombatCharacter * pOwner = NULL );

	virtual CHudTexture const	*GetSpriteActive( void ) const;
	virtual CHudTexture const	*GetSpriteInactive( void ) const;
	virtual CHudTexture const	*GetSpriteAmmo( void ) const;
	virtual CHudTexture const	*GetSpriteAmmo2( void ) const;
	virtual CHudTexture const	*GetSpriteCrosshair( void ) const;
	virtual CHudTexture const	*GetSpriteAutoaim( void ) const;
	virtual CHudTexture const	*GetSpriteZoomedCrosshair( void ) const;
	virtual CHudTexture const	*GetSpriteZoomedAutoaim( void ) const;

	virtual Activity		ActivityOverride( Activity baseAct, bool *pRequired );
	virtual	acttable_t*		ActivityList( void ) { return NULL; }
	virtual	int				ActivityListCount( void ) { return 0; }

	virtual void			Activate( void );

public:


// Server Only Methods
#if !defined( CLIENT_DLL )

	CBaseWeaponWorldModel*	CreateWeaponWorldModel( void );

	DECLARE_DATADESC();
	virtual void			FallInit( void );						// prepare to fall to the ground
	virtual void			FallThink( void );						// make the weapon fall to the ground after spawning

	// Weapon spawning
	bool					IsConstrained() { return m_pConstraint != NULL; }
	bool					IsInBadPosition ( void );				// Is weapon in bad position to pickup?
	bool					RepositionWeapon ( void );				// Attempts to reposition the weapon in a location where it can be
	virtual void			Materialize( void );					// make a weapon visible and tangible
	void					AttemptToMaterialize( void );			// see if the game rules will let the weapon become visible and tangible
	virtual void			CheckRespawn( void );					// see if this weapon should respawn after being picked up
	CBaseEntity				*Respawn ( void );						// copy a weapon

	static int				GetAvailableWeaponsInBox( CBaseCombatWeapon **pList, int listMax, const Vector &mins, const Vector &maxs );

	// Weapon dropping / destruction
	virtual void			Delete( void );
	void					DestroyItem( void );
	virtual void			Kill( void );

	virtual int				CapabilitiesGet( void ) { return 0; }
	virtual	int				ObjectCaps( void );

	bool					IsRemoveable() { return m_bRemoveable; }
	void					SetRemoveable( bool bRemoveable ) { m_bRemoveable = bRemoveable; }
	
	// Returns bits for	weapon conditions
	virtual bool			WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );	
	virtual	int				WeaponRangeAttack1Condition( float flDot, float flDist );
	virtual	int				WeaponRangeAttack2Condition( float flDot, float flDist );
	virtual	int				WeaponMeleeAttack1Condition( float flDot, float flDist );
	virtual	int				WeaponMeleeAttack2Condition( float flDot, float flDist );

	virtual void			Operator_FrameUpdate( CBaseCombatCharacter  *pOperator );
	virtual void			Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	virtual void			Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary, CBaseEntity *pTarget = NULL ) { return; }
	// NOTE: This should never be called when a character is operating the weapon.  Animation events should be
	// routed through the character, and then back into CharacterAnimEvent() 
	void					HandleAnimEvent( animevent_t *pEvent );

	virtual int				UpdateTransmitState( void );

	void					InputHideWeapon( inputdata_t &inputdata );
	void					Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual void			MakeWeaponNameFromEntity( CBaseEntity *pOther );

	//weapon module body groups
	virtual void			SetWeaponModules( void );

// Client only methods
#else

	virtual void			UpdateVisibility( void );

	virtual void			BoneMergeFastCullBloat( Vector &localMins, Vector &localMaxs, const Vector &thisEntityMins, const Vector &thisEntityMaxs  ) const;
	virtual bool			OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options ) { return false; }

	// Should this object cast shadows?
	virtual ShadowType_t	ShadowCastType();
	virtual void			SetDormant( bool bDormant );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual void			OnRestore();
	virtual void			UpdateOnRemove( void );

	virtual void			Redraw(void);
	virtual void			ViewModelDrawn( int nFlags, CBaseViewModel *pViewModel );
	// Get the position that bullets are seen coming out. Note: the returned values are different
	// for first person and third person.
	bool					GetShootPosition( Vector &vOrigin, QAngle &vAngles );
	virtual void			DrawCrosshair( void );
	virtual bool			ShouldDrawCrosshair( void ) { return true; }
	
	// Weapon state checking
	virtual bool			IsCarriedByLocalPlayer( void );
	virtual bool			IsActiveByLocalPlayer( void );

	bool					IsBeingCarried() const;

	// Returns the aiment render origin + angles
	virtual int				DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool			ShouldDraw( void );
	virtual bool			ShouldSuppressForSplitScreenPlayer( int nSlot );
	virtual bool			ShouldDrawPickup( void );
	virtual void			HandleInput( void ) { return; };
	virtual void			OverrideMouseInput( float *x, float *y ) { return; };
	virtual int				KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding ) { return 1; }
	virtual bool			AddLookShift( void ) { return true; };

	virtual void			GetViewmodelBoneControllers(C_BaseViewModel *pViewModel, float controllers[MAXSTUDIOBONECTRLS]) { return; }

	virtual void			NotifyShouldTransmit( ShouldTransmitState_t state );

	// SanyaSho: get rid of this ASAP
	virtual int				GetWorldModelIndex(void);

	virtual void			GetToolRecordingState( KeyValues *msg );
	bool					IsFirstPersonSpectated( void ); //true if the weapon is held by someone we're spectating in first person

	virtual void			GetWeaponCrosshairScale( float &flScale ) { flScale = 1.f; }

	virtual void			GetToolViewModelState( KeyValues *msg ) {} // this is just a stub for viewmodels to request recording of weapon-specific effects, etc

	// Viewmodel overriding
	virtual bool			IsOverridingViewmodel( void ) { return false; };
	virtual int				DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags, const RenderableInstance_t &instance ) { return 0; };
	bool					WantsToOverrideViewmodelAttachments( void ) { return false; }

	virtual IClientModelRenderable*	GetClientModelRenderable();

	static CUtlLinkedList< CBaseCombatWeapon * >& GetWeaponList( void );


	void					ApplyThirdPersonStickers( C_BaseAnimating *pWeaponModelTargetOverride = NULL );

#endif // End client-only methods

// 	virtual float GetAttributeFloat( const char* szAttribClassName ) const;
// 	virtual bool GetAttributeBool( const char* szAttribClassName ) const;
// 	virtual int GetAttributeInt( const char* szAttribClassName ) const;

	WEAPON_FILE_INFO_HANDLE	GetWeaponFileInfoHandle() const { return m_hWeaponFileInfo; }

	// Is the carrier alive?
	bool					IsCarrierAlive() const;
	virtual bool			IsAlwaysActive( void ) { return false; }

	virtual bool			CanLower( void ) { return false; }
	virtual bool			Ready( void ) { return false; }
	virtual bool			Lower( void ) { return false; }

	virtual void			HideThink( void );
	virtual bool			CanReload( void ){ return true; }

	virtual bool			IsSilentPickupThirdperson( CBaseCombatCharacter *pNewOwner ) { return false; }

// FTYPEDESC_INSENDTABLE STUFF
private:
	typedef CHandle< CBaseCombatCharacter > CBaseCombatCharacterHandle;
	CNetworkVar( CBaseCombatCharacterHandle, m_hOwner );				// Player carrying this weapon
public:
	// Networked fields
	CNetworkVar( int, m_nViewModelIndex );
	// Weapon firing
	CNetworkVar( float, m_flNextPrimaryAttack );						// soonest time ItemPostFrame will call PrimaryAttack
	CNetworkVar( float, m_flNextSecondaryAttack );						// soonest time ItemPostFrame will call SecondaryAttack

	// Weapon art
	CNetworkVar( int, m_iViewModelIndex );
	CNetworkVar( int, m_iWorldModelIndex );

	CNetworkVar( int, m_iWorldDroppedModelIndex );

	CNetworkVar( int, m_iWeaponModule );

	CNetworkVar( int, m_iNumEmptyAttacks );

	typedef CHandle<CBaseWeaponWorldModel> CBaseWeaponWorldModelHandle;
	CNetworkVar( CBaseWeaponWorldModelHandle, m_hWeaponWorldModel );

	CBaseWeaponWorldModel* GetWeaponWorldModel( void ) { return m_hWeaponWorldModel->Get(); }

#ifndef CLIENT_DLL
	void ShowWeaponWorldModel( bool bVisible );
#endif
			
public:
	// Weapon data
	CNetworkVar( int, m_iState );				// See WEAPON_* definition
	CNetworkVar( int, m_iPrimaryAmmoType );		// "primary" ammo index into the ammo info array 
	CNetworkVar( int, m_iSecondaryAmmoType );	// "secondary" ammo index into the ammo info array
	CNetworkVar( int, m_iClip1 );				// number of shots left in the primary weapon clip, -1 it not used
	CNetworkVar( int, m_iClip2 );				// number of shots left in the secondary weapon clip, -1 it not used

	CNetworkVar( int, m_iPrimaryReserveAmmoCount);	// amount of reserve ammo. This used to be on the player ( m_iAmmo ) but we're moving it to the weapon.
	CNetworkVar( int, m_iSecondaryReserveAmmoCount);	// amount of reserve ammo. This used to be on the player ( m_iAmmo ) but we're moving it to the weapon.

public:

#ifndef CLIENT_DLL
	float					m_flLastTimeInAir;
	virtual void			PhysicsSimulate( void );
#endif

// Non-networked prediction fields
	CNetworkVar( float, m_flTimeWeaponIdle );							// soonest time ItemPostFrame will call WeaponIdle
	// Sounds
	float					m_flNextEmptySoundTime;				// delay on empty sound playing
	float					m_fMinRange1;			// What's the closest this weapon can be used?
	float					m_fMinRange2;			// What's the closest this weapon can be used?
	float					m_fMaxRange1;			// What's the furthest this weapon can be used?
	float					m_fMaxRange2;			// What's the furthest this weapon can be used?
	float					m_fFireDuration;		// The amount of time that the weapon has sustained firing
	int						m_iReloadActivityIndex;
	
private:
	Activity				m_Activity;
	int						m_iPrimaryAmmoCount;
	int						m_iSecondaryAmmoCount;

public:
	string_t				m_iszName;				// Classname of this weapon.

private:
	bool					m_bRemoveable;
public:
	// Weapon state
	CNetworkVar( bool, m_bInReload );									// Are we in the middle of a reload;
	bool					m_bFireOnEmpty;												// True when the gun is empty and the player is still holding down the attack key(s)
	bool					m_bFiresUnderwater;		// true if this weapon can fire underwater
	bool					m_bAltFiresUnderwater;		// true if this weapon can fire underwater
	bool					m_bReloadsSingly;		// Tryue if this weapon reloads 1 round at a time

	float					m_flWeaponTauntHideTimeout;

// FTYPEDESC_INSENDTABLE STUFF (end)
public:
	Activity				GetIdealActivity( void ) { return m_IdealActivity; }
	int						GetIdealSequence( void ) { return m_nIdealSequence; }

	bool					SetIdealActivity( Activity ideal );
	void					MaintainIdealActivity( void );

private:
	int						m_nIdealSequence;
	Activity				m_IdealActivity;

public:

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nNextThinkTick );

	int						WeaponState() const { return m_iState; }

	int						m_iSubType;

	float					m_flUnlockTime;
	EHANDLE					m_hLocker;				// Who locked this weapon.

	CNetworkVar( bool, m_bFlipViewModel );
	
	CNetworkVar( int, m_iWeaponOrigin );			// How the player acquired the weapon

	IPhysicsConstraint		*GetConstraint() { return m_pConstraint; }

	virtual CStudioHdr			*OnNewModel() OVERRIDE;
	void						ClassifyWeaponModel( void );
	WeaponModelClassification_t	GetWeaponModelClassification( void );
	void						VerifyAndSetContextSensitiveWeaponModel( void );

private:
	WeaponModelClassification_t m_WeaponModelClassification;

	WEAPON_FILE_INFO_HANDLE	m_hWeaponFileInfo;
	IPhysicsConstraint		*m_pConstraint;

	int						m_iAltFireHudHintCount;		// How many times has this weapon displayed its alt-fire HUD hint?
	int						m_iReloadHudHintCount;		// How many times has this weapon displayed its reload HUD hint?
	bool					m_bAltFireHudHintDisplayed;	// Have we displayed an alt-fire HUD hint since this weapon was deployed?
	bool					m_bReloadHudHintDisplayed;	// Have we displayed a reload HUD hint since this weapon was deployed?
	float					m_flHudHintPollTime;	// When to poll the weapon again for whether it should display a hud hint.
	float					m_flHudHintMinDisplayTime; // if the hint is squelched before this, reset my counter so we'll display it again.
	
	// Server only
#if !defined( CLIENT_DLL )

	// Outputs
protected:
	COutputEvent			m_OnPlayerUse;		// Fired when the player uses the weapon.
	COutputEvent			m_OnPlayerPickup;	// Fired when the player picks up the weapon.
	COutputEvent			m_OnNPCPickup;		// Fired when an NPC picks up the weapon.
	COutputEvent			m_OnCacheInteraction;	// For awarding lambda cache achievements in HL2 on 360. See .FGD file for details 

#else // Client .dll only
	bool					m_bJustRestored;
	
	// Allow weapons resource to access m_hWeaponFileInfo directly
	friend class			WeaponsResource;


protected:	
	int						m_iOldState;

#endif // End Client .dll only
};

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CBaseCombatWeapon *ToBaseCombatWeapon( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return NULL;
	return pEntity->MyCombatWeaponPointer();
}


#endif // COMBATWEAPON_SHARED_H
