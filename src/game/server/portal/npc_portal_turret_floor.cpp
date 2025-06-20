//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "npc_portal_turret_floor.h"
#include "portal_player.h"
#include "portal_grabcontroller_shared.h"
#include "basehlcombatweapon_shared.h"
#include "ammodef.h"
#include "ai_senses.h"
#include "ai_memory.h"
#include "rope.h"
#include "rope_shared.h"
#include "prop_portal_shared.h"
#include "Sprite.h"
#include "particle_parse.h"

ConVar sv_portal_turret_fire_cone_z_tolerance("sv_portal_turret_fire_cone_z_tolerance", "45.0", FCVAR_CHEAT, "The max height of the turrets firing view cone (in degrees)", true, 0.0f, true, 180.0f);
ConVar sv_portal_turret_burn_time_min("sv_portal_turret_burn_time_min", "1.0f", FCVAR_CHEAT, "The min time that the turret will burn for.");
ConVar sv_portal_turret_burn_time_max("sv_portal_turret_burn_time_max", "1.5f", FCVAR_CHEAT, "The max time that the turret will burn for.");
ConVar sv_portal_turret_shoot_at_death("sv_portal_turret_shoot_at_death", "1", FCVAR_CHEAT, "If the turrets should shoot after they die.");
ConVar sv_portal_turret_shoot_through_portals_proximity("sv_portal_turret_shoot_through_portals_proximity", "36864", FCVAR_CHEAT, "Only allow turrets to shoot through portals at players this close to portals (in square units)");

extern ConVar player_held_object_collide_with_player;

int ACT_FLOOR_TURRET_FIRE2;
int ACT_FLOOR_TURRET_DIE;
int ACT_FLOOR_TURRET_DIE_IDLE;

const char *g_TalkNames[] = 
{
	"NPC_FloorTurret.TalkSearch",
	"NPC_FloorTurret.TalkAutosearch",
	"NPC_FloorTurret.TalkActive",
	"NPC_FloorTurret.TalkSupress",
	"NPC_FloorTurret.TalkDeploy",
	"NPC_FloorTurret.TalkRetire",
	"NPC_FloorTurret.TalkTipped",
	NULL							// Must have NULL at end of list in case more states are added to base turret!
};

const char *g_PortalTalkNames[ PORTAL_TURRET_STATE_TOTAL - TURRET_STATE_TOTAL ] = 
{
	"NPC_FloorTurret.TalkDisabled",
	"NPC_FloorTurret.TalkCollide",
	"NPC_FloorTurret.TalkPickup",
	"NPC_FloorTurret.TalkShotAt",
	"NPC_FloorTurret.TalkDissolved",
	"NPC_FloorTurret.TalkFlung",
	"NPC_FloorTurret.TalkStartBurning",
	"NPC_FloorTurret.TalkBurned"
};

const char* GetTurretTalkName( int iState )
{
	if ( iState < TURRET_STATE_TOTAL )
		return g_TalkNames[ iState ];
	
	return g_PortalTalkNames[ iState - TURRET_STATE_TOTAL ];
}

LINK_ENTITY_TO_CLASS( npc_portal_turret_floor, CNPC_Portal_FloorTurret );

//Datatable
BEGIN_DATADESC( CNPC_Portal_FloorTurret )

	DEFINE_FIELD( m_bOutOfAmmo, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLaserOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsFiring, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_sLaserHaloSprite, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDistToEnemy, FIELD_FLOAT ),

//	DEFINE_ARRAY( m_iBarrelAttachments, FIELD_INTEGER, 4 ),
	DEFINE_FIELD( m_bShootWithBottomBarrels, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fSearchSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_fMovingTargetThreashold, FIELD_FLOAT ),
	DEFINE_FIELD( m_iLastState, FIELD_INTEGER ),
	DEFINE_FIELD( m_fNextTalk, FIELD_FLOAT ),
	DEFINE_FIELD( m_bDelayTippedTalk, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsDead, FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_nModelIndex, FIELD_INTEGER, "ModelIndex" ),
	DEFINE_KEYFIELD( m_bDamageForce, FIELD_BOOLEAN, "DamageForce" ),
	DEFINE_KEYFIELD( m_bGagged, FIELD_BOOLEAN, "Gagged" ),
	DEFINE_KEYFIELD( m_bUsedAsActor, FIELD_BOOLEAN, "UsedAsActor" ),
	DEFINE_KEYFIELD( m_bPickupEnabled, FIELD_BOOLEAN, "PickupEnabled" ),
	DEFINE_KEYFIELD( m_bDisableMotion, FIELD_BOOLEAN, "DisableMotion" ),
	DEFINE_KEYFIELD( m_flTurretRange, FIELD_FLOAT, "TurretRange" ),
	DEFINE_KEYFIELD( m_nCollisionType, FIELD_INTEGER, "CollisionType" ),
	DEFINE_KEYFIELD( m_bUseSuperDamageScale, FIELD_BOOLEAN, "UseSuperDamageScale" ),
	DEFINE_KEYFIELD( m_bLoadAlternativeModels, FIELD_BOOLEAN, "LoadAlternativeModels" ),

	DEFINE_THINKFUNC( Retire ),
	DEFINE_THINKFUNC( Deploy ),
	DEFINE_THINKFUNC( ActiveThink ),
	DEFINE_THINKFUNC( SearchThink ),
	DEFINE_THINKFUNC( AutoSearchThink ),
	DEFINE_THINKFUNC( TippedThink ),
	DEFINE_THINKFUNC( HeldThink ),
	DEFINE_THINKFUNC( InactiveThink ),
	DEFINE_THINKFUNC( DieThink ),
	DEFINE_THINKFUNC( SuppressThink ),
	DEFINE_THINKFUNC( DisabledThink ),
	DEFINE_THINKFUNC( BreakThink ),
	DEFINE_THINKFUNC( BurnThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "FireBullet", InputFireBullet ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnableGagging", InputEnableGagging),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableGagging", InputDisableGagging),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnablePickup", InputEnablePickup),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisablePickup", InputDisablePickup),
	DEFINE_INPUTFUNC( FIELD_VOID, "SelfDestructImmediately", InputSelfDestructImmediately),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetModel", InputSetModel),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CNPC_Portal_FloorTurret, DT_NPC_Portal_FloorTurret)

	SendPropBool( SENDINFO( m_bOutOfAmmo ) ),
	SendPropBool( SENDINFO( m_bLaserOn ) ),
	SendPropInt( SENDINFO( m_sLaserHaloSprite ) ),

END_SEND_TABLE()

bool IsPlayerNearTargetPortal( CPortal_Base2D *pPortal ) // Line 123
{
	if( sv_portal_turret_shoot_through_portals_proximity.GetFloat() >= 0.f ) // bug?
	{
		for( int i = 1; i < gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if( pPlayer )
			{
				Vector vecPlayerOrigin = pPlayer->GetAbsOrigin();
				Vector vecPortalOrigin = pPortal->GetAbsOrigin();
				return sv_portal_turret_shoot_through_portals_proximity.GetFloat() > ( (
					( ( vecPlayerOrigin.x - vecPortalOrigin.x ) * ( vecPlayerOrigin.x - vecPortalOrigin.x ) ) +
					( ( vecPlayerOrigin.y - vecPortalOrigin.y ) * ( vecPlayerOrigin.y - vecPortalOrigin.y ) ) ) +
					( ( vecPlayerOrigin.z - vecPortalOrigin.z ) * ( vecPlayerOrigin.z - vecPortalOrigin.z ) ) ); // TODO: replace with fabs()
			}
		}

		return false;
	}

	return true;
}

CNPC_Portal_FloorTurret::CNPC_Portal_FloorTurret( void )
{
	CNPC_FloorTurret::fMaxTipControllerVelocity = 100.0f * 100.0f;
	CNPC_FloorTurret::fMaxTipControllerAngularVelocity = 30.0f * 30.0f;

	m_nModelIndex = 0;
	m_bUseSuperDamageScale = false;
	m_flPreviousVelocity = 0.0f;
	m_bDamageForce = true;
	m_bPickupEnabled = true;
}

void CNPC_Portal_FloorTurret::Precache( void )
{
	if (this->m_bLoadAlternativeModels)
	{
		PrecacheModel(BOX_TURRET_MODEL);
		PrecacheModel(BACKWARDS_TURRET_MODEL);
		PrecacheModel(DEFECTIVE_TURRET_MODEL);
	}
	PrecacheGibsForModel(PrecacheModel(FLOOR_TURRET_PORTAL_MODEL));
	BaseClass::Precache();

	ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_FIRE2 );
	ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_DIE);
	ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_DIE_IDLE);

	PrecacheParticleSystem("turret_coop_explosion");
	CRopeKeyframe::PrecacheShakeRopes();
	m_sLaserHaloSprite = PrecacheModel( "sprites/redlaserglow.vmt" );
	PrecacheModel("effects/redlaser1.vmt");
	PrecacheModel("models/npcs/turret/turret_fx_fizzler.mdl");
	PrecacheEffect("AR2Tracer");
	PrecacheParticleSystem("burning_character");
	PrecacheEffect("ShakeRopes");

	for ( int iTalkScript = 0; iTalkScript < PORTAL_TURRET_STATE_TOTAL; ++iTalkScript )
	{
		if ( iTalkScript < TURRET_STATE_TOTAL )
		{
			if ( g_TalkNames[ iTalkScript ] )
				PrecacheScriptSound( g_TalkNames[ iTalkScript ] );
			else
				iTalkScript = TURRET_STATE_TOTAL - 1;	// We hit the last script item, so jump to the portal only states
		}
		else
		{
			PrecacheScriptSound( g_PortalTalkNames[ iTalkScript - TURRET_STATE_TOTAL ] );
		}
	}
	PrecacheModel("cable/cable.vmt");
	PrecacheMaterial("cable/rope_shadowdepth");
	SetTurretType(GetModelIndex());

}

//-----------------------------------------------------------------------------
// Purpose: Spawn the entity
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::Spawn( void )
{ 
	BaseClass::Spawn();

	CAmmoDef* pAmmoDef = GetAmmoDef();
	m_iAmmoType = pAmmoDef->Index("PortalTurretBullet");
	AddFlag(FL_NPC);

	m_iBarrelAttachments[ 0 ] = LookupAttachment( "LFT_Gun1_Muzzle" );
	m_iBarrelAttachments[ 1 ] = LookupAttachment( "RT_Gun1_Muzzle" );
	m_iBarrelAttachments[ 2 ] = LookupAttachment( "LFT_Gun2_Muzzle" );
	m_iBarrelAttachments[ 3 ] = LookupAttachment( "RT_Gun2_Muzzle" );

	m_fSearchSpeed = RandomFloat( 1.0f, 1.4f );
	m_fMovingTargetThreashold = 20.0f;

	m_bNoAlarmSounds = true;

	m_bIsFiring = false;

	m_bOutOfAmmo = ( m_spawnflags & SF_FLOOR_TURRET_OUT_OF_AMMO ) != 0;

	if (m_flTurretRange == 0.0f)
		m_flTurretRange = 1024.0f;

	SetMaxHealth(10);

	m_bIsDead = false;

	if (m_bDisableMotion)
		if (m_pPhysicsObject)
			m_pPhysicsObject->EnableMotion(false);

	if (m_nCollisionType == COLLISION_GROUP_DEBRIS)
		SetCollisionGroup(COLLISION_GROUP_DEBRIS_TRIGGER);

	m_nSkin = m_iKeySkin;

	AddEffects(EF_NOFLASHLIGHT);
	SetFadeDistance(-1.0f, 0.0f);
	SetGlobalFadeScale(0.0f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::Activate( void )
{
	BaseClass::Activate();

	// Find all nearby physics objects and add them to the list of objects we will sense
	CBaseEntity *pObject = gEntList.FindEntityByClassname( NULL, "prop_physics" );
	while ( pObject )
	{
		// Tell the AI sensing list that we want to consider this
		g_AI_SensedObjectsManager.AddEntity( pObject );

		pObject = gEntList.FindEntityByClassname( pObject, "prop_physics" );
	}

	pObject = gEntList.FindEntityByClassname( NULL, "func_physbox" );
	while ( pObject )
	{
		// Tell the AI sensing list that we want to consider this
		g_AI_SensedObjectsManager.AddEntity( pObject );

		pObject = gEntList.FindEntityByClassname( pObject, "func_physbox" );
	}

	m_iLastState = TURRET_AUTO_SEARCHING;

	m_iBarrelAttachments[ 0 ] = LookupAttachment( "LFT_Gun1_Muzzle" );
	m_iBarrelAttachments[ 1 ] = LookupAttachment( "RT_Gun1_Muzzle" );
	m_iBarrelAttachments[ 2 ] = LookupAttachment( "LFT_Gun2_Muzzle" );
	m_iBarrelAttachments[ 3 ] = LookupAttachment( "RT_Gun2_Muzzle" );
}

void CNPC_Portal_FloorTurret::OnRestore(void)
{
	BaseClass::OnRestore();
	if (m_pPhysicsObject)
	{
		if (m_pPhysicsObject->IsAsleep())
		m_pPhysicsObject->Wake();
	}
}

void CNPC_Portal_FloorTurret::UpdateOnRemove( void )
{
	if ( IsDissolving() && !this->m_bGagged )
		EmitSound( GetTurretTalkName( PORTAL_TURRET_DISSOLVED ) );

	LaserOff();
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
int CNPC_Portal_FloorTurret::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( m_lifeState == LIFE_ALIVE && ( info.GetDamageType() & DMG_BULLET ) && !info.GetAttacker()->IsPlayer() )
	{
		if ( gpGlobals->curtime > m_fNextTalk )
		{
			if (!this->m_bGagged)
				EmitSound( GetTurretTalkName( PORTAL_TURRET_SHOTAT ) );
			m_fNextTalk = gpGlobals->curtime + 3.0f;
		}
	}

	return BaseClass::OnTakeDamage( info );
}

bool CNPC_Portal_FloorTurret::ShouldAttractAutoAim( CBaseEntity *pAimingEnt )
{
	return ( m_lifeState == LIFE_ALIVE );
}

float CNPC_Portal_FloorTurret::GetAutoAimRadius()
{
	return 64.0f;
}

Vector CNPC_Portal_FloorTurret::GetAutoAimCenter()
{
	// We want to shoot portals right under them
	return WorldSpaceCenter() + Vector( 0.0f, 0.0f, -18.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	if ( m_lifeState == LIFE_ALIVE && m_bEnabled )
	{
		m_bActive = true;
		SetThink( &CNPC_Portal_FloorTurret::HeldThink );
	}

	BaseClass::OnPhysGunPickup( pPhysGunUser, reason );
	EnableTipController(false);
}

void CNPC_Portal_FloorTurret::NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	BaseClass::NotifySystemEvent( pNotify, eventType, params );
}

//-----------------------------------------------------------------------------
// Purpose: Allows a generic think function before the others are called
// Input  : state - which state the turret is currently in
//-----------------------------------------------------------------------------
bool CNPC_Portal_FloorTurret::PreThink( turretState_e state )
{
	if (IsOnFire())
	{
		m_flBurnExplodeTime = random->RandomFloat(sv_portal_turret_burn_time_min.GetFloat(), sv_portal_turret_burn_time_max.GetFloat());
		
		if (!random->RandomInt(0, 3))
		{
			if (!m_bGagged)
				EmitSound(GetTurretTalkName(PORTAL_TURRET_STARTBURNING));
			m_fNextTalk = gpGlobals->curtime + 1.0;
		}
		SetThink(&CNPC_Portal_FloorTurret::BurnThink);
		SetNextThink(gpGlobals->curtime + 0.1f);
		return false;
	}

	m_bIsFiring = state == TURRET_ACTIVE;

	if (IsMovingSuddenly())
		if (!this->m_bGagged)
			EmitSound(GetTurretTalkName(PORTAL_TURRET_FLUNG));
		m_fNextTalk = gpGlobals->curtime + 1.0;

	// Working 2 enums into one integer
	int iNewState = state;

	// If the turret is dissolving go to a special state
	if ( IsDissolving() )
		iNewState = PORTAL_TURRET_DISSOLVED;

	// Need to play these sounds immediately
	if ( m_iLastState != iNewState && ( ( iNewState == TURRET_TIPPED && !m_bDelayTippedTalk ) || 
										iNewState == TURRET_RETIRING || 
										iNewState == PORTAL_TURRET_DISSOLVED || 
										iNewState == PORTAL_TURRET_FLUNG ||
										iNewState == PORTAL_TURRET_STARTBURNING ||
										iNewState == PORTAL_TURRET_BURNED ||
										iNewState == PORTAL_TURRET_PICKUP ) )
	{
		m_fNextTalk = gpGlobals->curtime -1.0f;
	}

	// If we've changed states or are in a state with a repeating message
	if ( gpGlobals->curtime > m_fNextTalk && m_iLastState != iNewState )
	{
		m_iLastState = (turretState_e)iNewState;

		const char *pchScriptName = GetTurretTalkName( m_iLastState );

		switch ( iNewState )
		{
			case TURRET_SEARCHING:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 1.75f;
				break;

			case TURRET_AUTO_SEARCHING:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 1.75f;
				break;

			/*case TURRET_ACTIVE:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 2.5f;
				break;*/

			// Suppress is too fleeting for a message
			/*case TURRET_SUPPRESSING:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 1.5f;
				break;*/

			case TURRET_DEPLOYING:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 1.75f;
				break;

			case TURRET_RETIRING:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 3.5f;
				break;

			case TURRET_TIPPED:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 1.15f;
				break;

			case PORTAL_TURRET_PICKUP:
				EmitSound( GetTurretTalkName( PORTAL_TURRET_PICKUP ) );
				m_fNextTalk = gpGlobals->curtime + 2.25f;
				break;

			case PORTAL_TURRET_DISSOLVED:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 10.0f;	// Never going to talk again
				break;

			case PORTAL_TURRET_STARTBURNING:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 1.0f;
				break;
			case PORTAL_TURRET_BURNED:
				EmitSound( pchScriptName );
				m_fNextTalk = gpGlobals->curtime + 1.0f;
				break;
		}
	}

	// New states are not supported by old turret code
	if (!m_bUsedAsActor &&
		iNewState != TURRET_TIPPED &&
		iNewState < TURRET_STATE_TOTAL )
		return BaseClass::PreThink( (turretState_e)iNewState );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Fire!
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy, bool bStrict )
{
	FireBulletsInfo_t info;

	//if ( !bStrict && GetEnemy() == UTIL_PlayerByIndex( 1 ) )
	CBaseEntity *pEnemy = GetEnemy();
	if( !bStrict && (pEnemy && pEnemy->IsPlayer()) )
	{
		Vector vecDir = GetActualShootTrajectory( vecSrc );

		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDir;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = GetAttackSpread( NULL, GetEnemy() );
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
	}
	else
	{
		// Just shoot where you're facing!
		Vector vecMuzzle, vecMuzzleDir;

		GetAttachment( m_iMuzzleAttachment, vecMuzzle, &vecMuzzleDir );

		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecMuzzleDir;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = GetAttackSpread( NULL, GetEnemy() );
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
	}

	if (m_nModelIndex == 3)
		info.m_vecDirShooting = info.m_vecDirShooting * Vector(-1.0f, -1.0f, -1.0f);
		m_iNextShootingBarrel = 0;

	info.m_flDamageForceScale = ( ( !m_bDamageForce ) ? ( 0.0f ) : ( TURRET_FLOOR_BULLET_FORCE_MULTIPLIER ) );

	int iBarrelIndex = ( m_bShootWithBottomBarrels ) ? ( 2 ) : ( 0 );
	QAngle angBarrelDir;

	// Shoot out of the left barrel if there's nothing solid between the turret's center and the muzzle
	trace_t tr;
	GetAttachment( m_iBarrelAttachments[ iBarrelIndex ], info.m_vecSrc, angBarrelDir );
	Vector vecCenter = GetAbsOrigin();
	UTIL_TraceLine( vecCenter, info.m_vecSrc, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	if ( !tr.m_pEnt || !tr.m_pEnt->IsWorld() )
	{
		FireBullets( info );
	}

	// Shoot out of the right barrel if there's nothing solid between the turret's center and the muzzle
	GetAttachment( m_iBarrelAttachments[ iBarrelIndex + 1 ], info.m_vecSrc, angBarrelDir );
	vecCenter = GetAbsOrigin();
	UTIL_TraceLine( vecCenter, info.m_vecSrc, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	if ( !tr.m_pEnt || !tr.m_pEnt->IsWorld() )
	{
		FireBullets( info );
	}

	// Flip shooting from the top or bottom
	m_bShootWithBottomBarrels = !m_bShootWithBottomBarrels;

	EmitSound( "NPC_FloorTurret.ShotSounds" );
	DoMuzzleFlash();
	/*
	// Make ropes shake if they exist
	for ( int iRope = 0; iRope < PORTAL_FLOOR_TURRET_NUM_ROPES; ++iRope )
	{
		if ( m_hRopes[ iRope ] )
		{
			m_hRopes[ iRope ]->ShakeRopes( vecSrc, 32.0f, 5.0f );
		}
	}
	*/
	// If a turret is partially tipped the recoil with each shot so that it can knock itself over
	Vector	up;
	GetVectors( NULL, NULL, &up );

	if ( up.z < 0.9f )
	{
		m_pMotionController->Suspend( 2.0f );

		IPhysicsObject *pTurretPhys = VPhysicsGetObject();
		Vector vVelocityImpulse = info.m_vecDirShooting * -35.0f;
		pTurretPhys->AddVelocity( &vVelocityImpulse, &vVelocityImpulse );
	}

	if ( m_iLastState == TURRET_ACTIVE && gpGlobals->curtime > m_fNextTalk )
	{
		if (!this->m_bGagged)
			EmitSound( GetTurretTalkName( m_iLastState ) );
		m_fNextTalk = gpGlobals->curtime + 2.5f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the state of the glowing eye attached to the turret
// Input  : state - state the eye should be in
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::SetEyeState( eyeState_t state )
{
	// Must have a valid eye to affect
	if ( !m_hEyeGlow )
	{
		// Create our eye sprite
		m_hEyeGlow = CSprite::SpriteCreate( FLOOR_TURRET_GLOW_SPRITE, GetLocalOrigin(), false );
		if ( !m_hEyeGlow )
			return;

		m_hEyeGlow->SetTransparency( kRenderWorldGlow, 255, 0, 0, 128, kRenderFxNoDissipation );
		m_hEyeGlow->SetAttachment( this, m_iEyeAttachment );
	}

//	bool bNewState = ( m_iEyeState != state );

	m_iEyeState = state;

	//Set the state
	switch( state )
	{
	default:
	case TURRET_EYE_SEE_TARGET: //Fade in and scale up
		m_hEyeGlow->SetColor( 255, 0, 0 );
		m_hEyeGlow->SetBrightness( 255, 0.1f );
		m_hEyeGlow->SetScale( 0.4f, 0.1f );
		break;

	case TURRET_EYE_SEEKING_TARGET: //Ping-pongs

		//Toggle our state
		m_bBlinkState = !m_bBlinkState;
		m_hEyeGlow->SetColor( 255, 0, 0 );

		if ( m_bBlinkState )
		{
			//Fade up and scale up
			m_hEyeGlow->SetScale( 0.3f, 0.1f );
			m_hEyeGlow->SetBrightness( 224, 0.1f );
		}
		else
		{
			//Fade down and scale down
			m_hEyeGlow->SetScale( 0.2f, 0.1f );
			m_hEyeGlow->SetBrightness( 192, 0.1f );
		}

		break;

	case TURRET_EYE_DORMANT: //Fade out and scale down
		m_hEyeGlow->SetColor( 255, 0, 0 );
		m_hEyeGlow->SetScale( 0.2f, 0.5f );
		m_hEyeGlow->SetBrightness( 192, 0.5f );
		break;

	case TURRET_EYE_DEAD: //Fade out slowly
		m_hEyeGlow->SetColor( 255, 0, 0 );
		m_hEyeGlow->SetScale( 0.1f, 3.0f );
		m_hEyeGlow->SetBrightness( 0, 3.0f );
		break;

	case TURRET_EYE_DISABLED:
		m_hEyeGlow->SetColor( 255, 0, 0 );
		m_hEyeGlow->SetScale( 0.1f, 1.0f );
		m_hEyeGlow->SetBrightness( 0, 1.0f );
		break;
	}
}

void CNPC_Portal_FloorTurret::TryEmitSound(const char* soundname)
{
	if (!this->m_bGagged)
		CBaseEntity::EmitSound(soundname);
}

void CNPC_Portal_FloorTurret::SetTurretType(int nType)
{
	m_nModelIndex = nType;
	switch (nType)
	{
	case 0:
		SetModelName(AllocPooledString(FLOOR_TURRET_PORTAL_MODEL));
		SetModel(FLOOR_TURRET_PORTAL_MODEL);
		break;
	case 2:
		SetModelName(AllocPooledString(BOX_TURRET_MODEL));
		SetModel(BOX_TURRET_MODEL);
		break;
	case 3:
		SetModelName(AllocPooledString(BACKWARDS_TURRET_MODEL));
		SetModel(BACKWARDS_TURRET_MODEL);
		break;
	case 4:
		SetModelName(AllocPooledString(DEFECTIVE_TURRET_MODEL));
		SetModel(DEFECTIVE_TURRET_MODEL);
		break;
	default:
		return;
	}
}

inline bool CNPC_Portal_FloorTurret::OnSide( void )
{
	if ( GetWaterLevel() > 0 )
		return true;

	Vector	up;
	GetVectors( NULL, NULL, &up );

	return ( DotProduct( up, Vector(0,0,1) ) < 0.5f );
}

void CNPC_Portal_FloorTurret::EnableTipController(bool bEnabled)
{
	m_pMotionController->Enable(bEnabled);
}

float CNPC_Portal_FloorTurret::GetAttackDamageScale( CBaseEntity *pVictim )
{
	CBaseCombatCharacter *pBCC = pVictim->MyCombatCharacterPointer();

	// Do extra damage to antlions & combine
	if ( pBCC )
	{
		if ( pBCC->Classify() == CLASS_PLAYER )
		{
			// Does normal damage when thrashing
			if ( OnSide() )
				return 1.0f;

			return TURRET_FLOOR_DAMAGE_MULTIPLIER;
		}
	}

	if (this->m_bUseSuperDamageScale)
		return TURRET_FLOOR_DAMAGE_MULTIPLIER * 4.0;

	return BaseClass::GetAttackDamageScale( pVictim );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CNPC_Portal_FloorTurret::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget ) 
{
	WeaponProficiency_t weaponProficiency = WEAPON_PROFICIENCY_AVERAGE;

	// Switch our weapon proficiency based upon our target
	if ( pTarget )
	{
		if ( pTarget->Classify() == CLASS_PLAYER || pTarget->Classify() == CLASS_ANTLION || pTarget->Classify() == CLASS_ZOMBIE )
		{
			// Make me much more accurate
			weaponProficiency = WEAPON_PROFICIENCY_PERFECT;
		}
		else if ( pTarget->Classify() == CLASS_COMBINE )
		{
			// Make me more accurate
			weaponProficiency = WEAPON_PROFICIENCY_VERY_GOOD;
		}
	}

	return VECTOR_CONE_4DEGREES * ((CBaseHLCombatWeapon::GetDefaultProficiencyValues())[ weaponProficiency ].spreadscale);
}

void CNPC_Portal_FloorTurret::Retire( void )
{
	LaserOn();

	BaseClass::Retire();
}

void CNPC_Portal_FloorTurret::Deploy( void )
{
	LaserOn();

	BaseClass::Deploy();
}

void CNPC_Portal_FloorTurret::ActiveThink( void )
{
	LaserOn();

	//Allow descended classes a chance to do something before the think function
	if ( PreThink( TURRET_ACTIVE ) )
		return;

	HackFindEnemy();

	//Update our think time
	SetNextThink( gpGlobals->curtime + 0.1f );

	CBaseEntity *pEnemy = GetEnemy();

	//If we've become inactive, go back to searching
	if ( ( m_bActive == false ) || ( pEnemy == NULL ) )
	{
		SetEnemy( NULL );
		m_flLastSight = gpGlobals->curtime + FLOOR_TURRET_MAX_WAIT;
		SetThink( &CNPC_FloorTurret::SearchThink );
		m_vecGoalAngles = GetAbsAngles();
		return;
	}

	//Get our shot positions
	Vector vecMid = EyePosition();
	Vector vecMidEnemy = pEnemy->BodyTarget( vecMid );

	// Store off our last seen location so we can suppress it later
	m_vecEnemyLKP = vecMidEnemy;

	//Look for our current enemy
	bool bEnemyInFOV = FInViewCone( pEnemy );
	bool bEnemyVisible = FVisible( pEnemy ) && pEnemy->IsAlive();

	//Calculate dir and dist to enemy
	Vector	vecDirToEnemy = vecMidEnemy - vecMid;	
	m_flDistToEnemy = VectorNormalize( vecDirToEnemy );

	// If the enemy isn't in the normal fov, check the fov through portals
	CPortal_Base2D *pPortal = NULL;
	if ( pEnemy->IsAlive() )
	{
		pPortal = FInViewConeThroughPortal( pEnemy );

		if ( pPortal && FVisibleThroughPortal( pPortal, pEnemy ) )
		{
			// Translate our target across the portal
			Vector vecMidEnemyTransformed;
			UTIL_Portal_PointTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecMidEnemy, vecMidEnemyTransformed );

			//Calculate dir and dist to enemy
			Vector	vecDirToEnemyTransformed = vecMidEnemyTransformed - vecMid;	
			float	flDistToEnemyTransformed = VectorNormalize( vecDirToEnemyTransformed );

			// If it's not visible through normal means or the enemy is closer through the portal, use the translated info
			if ( !bEnemyInFOV || !bEnemyVisible || flDistToEnemyTransformed < m_flDistToEnemy )
			{
				bEnemyInFOV = true;
				bEnemyVisible = true;
				vecMidEnemy = vecMidEnemyTransformed;
				vecDirToEnemy = vecDirToEnemyTransformed;
				m_flDistToEnemy = flDistToEnemyTransformed;
			}
			else
			{
				pPortal = NULL;
			}
		}
		else
		{
			pPortal = NULL;
		}
	}

	//Draw debug info
	if ( g_debug_turret.GetBool() )
	{
		NDebugOverlay::Cross3D( vecMid, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Cross3D( GetEnemy()->WorldSpaceCenter(), -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Line( vecMid, GetEnemy()->WorldSpaceCenter(), 0, 255, 0, false, 0.05 );

		NDebugOverlay::Cross3D( vecMid, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Cross3D( vecMidEnemy, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, false, 0.05 );
		NDebugOverlay::Line( vecMid, vecMidEnemy, 0, 255, 0, false, 0.05f );
	}

	//See if they're past our FOV of attack
	if ( bEnemyInFOV == false )
	{
		// Should we look for a new target?
		ClearEnemyMemory();
		SetEnemy( NULL );

		if ( m_spawnflags & SF_FLOOR_TURRET_FASTRETIRE )
		{
			// Retire quickly in this case. (The case where we saw the player, but he hid again).
			m_flLastSight = gpGlobals->curtime + FLOOR_TURRET_SHORT_WAIT;
		}
		else
		{
			m_flLastSight = gpGlobals->curtime + FLOOR_TURRET_MAX_WAIT;
		}

		SetThink( &CNPC_FloorTurret::SearchThink );
		m_vecGoalAngles = GetAbsAngles();

		SpinDown();

		return;
	}

	//Current enemy is not visible
	if ( ( bEnemyVisible == false ) || ( m_flDistToEnemy > m_flTurretRange ))
	{
		m_flLastSight = gpGlobals->curtime + 2.0f;

		ClearEnemyMemory();
		SetEnemy( NULL );
		SetThink( &CNPC_FloorTurret::SuppressThink );

		return;
	}

	if ( g_debug_turret.GetBool() )
	{
		Vector vecMuzzle, vecMuzzleDir;

		UpdateMuzzleMatrix();
		MatrixGetColumn( m_muzzleToWorld, 0, vecMuzzleDir );
		MatrixGetColumn( m_muzzleToWorld, 3, vecMuzzle );

		// Visualize vertical firing ranges
		for ( int i = 0; i < 4; i++ )
		{
			QAngle angMaxDownPitch = GetAbsAngles();

			switch( i )
			{
			case 0:	angMaxDownPitch.x -= 15; break;
			case 1:	angMaxDownPitch.x += 15; break;
			case 2:	angMaxDownPitch.x -= 25; break;
			case 3:	angMaxDownPitch.x += 25; break;
			default:
				break;
			}

			Vector vecMaxDownPitch;
			AngleVectors( angMaxDownPitch, &vecMaxDownPitch );
			NDebugOverlay::Line( vecMuzzle, vecMuzzle + (vecMaxDownPitch*256), 255, 255, 255, false, 0.1 );
		}
	}

	if ( m_flShotTime < gpGlobals->curtime )
	{
		Vector vecMuzzle, vecMuzzleDir;

		UpdateMuzzleMatrix();
		MatrixGetColumn( m_muzzleToWorld, 0, vecMuzzleDir );
		MatrixGetColumn( m_muzzleToWorld, 3, vecMuzzle );

		Vector2D vecDirToEnemy2D = vecDirToEnemy.AsVector2D();
		Vector2D vecMuzzleDir2D = vecMuzzleDir.AsVector2D();

		bool bCanShoot = true;
		float minCos3d = DOT_10DEGREE; // 10 degrees slop

		if ( m_flDistToEnemy < 60.0 )
		{
			vecDirToEnemy2D.NormalizeInPlace();
			vecMuzzleDir2D.NormalizeInPlace();

			bCanShoot = ( vecDirToEnemy2D.Dot(vecMuzzleDir2D) >= DOT_10DEGREE );
			minCos3d = 0.7071; // 45 degrees
		}

		//Fire the gun
		if ( IsEnemyBehindGlass(pPortal, pEnemy, vecMuzzle, vecDirToEnemy, m_flDistToEnemy ) || bCanShoot ) // 10 degree slop XY
		{
			float dot3d = DotProduct( vecDirToEnemy, vecMuzzleDir );

			if( m_bOutOfAmmo )
			{
				DryFire();
			}
			else
			{
				if ( dot3d >= minCos3d ) 
				{
					SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );
					SetActivity( (Activity)( ( m_bShootWithBottomBarrels ) ? ( ACT_FLOOR_TURRET_FIRE2 ) : ( ACT_FLOOR_TURRET_FIRE ) ) );

					//Fire the weapon
#if !DISABLE_SHOT
					Shoot( vecMuzzle, vecMuzzleDir, (dot3d < DOT_10DEGREE) );
#endif
				}
			}
		} 
	}
	else
	{
		SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );
	}

	//If we can see our enemy, face it
	if ( bEnemyVisible )
	{
		//We want to look at the enemy's eyes so we don't jitter
		Vector vEnemyWorldSpaceCenter = pEnemy->WorldSpaceCenter();
		if ( pPortal && pPortal->IsActivedAndLinked() )
		{
			// Translate our target across the portal
			UTIL_Portal_PointTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vEnemyWorldSpaceCenter, vEnemyWorldSpaceCenter );
		}

		Vector	vecDirToEnemyEyes = vEnemyWorldSpaceCenter - vecMid;
		VectorNormalize( vecDirToEnemyEyes );

		QAngle vecAnglesToEnemy;
		VectorAngles( vecDirToEnemyEyes, vecAnglesToEnemy );

		m_vecGoalAngles.y = vecAnglesToEnemy.y;
		m_vecGoalAngles.x = vecAnglesToEnemy.x;
	}

	//Turn to face
	UpdateFacing();
}

//-----------------------------------------------------------------------------
// Purpose: Target doesn't exist or has eluded us, so search for one
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::SearchThink( void )
{
	//Allow descended classes a chance to do something before the think function
	if ( PreThink( TURRET_SEARCHING ) )
		return;

	SetNextThink( gpGlobals->curtime + 0.05f );

	SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );

	//If our enemy has died, pick a new enemy
	if ( ( GetEnemy() != NULL ) && ( GetEnemy()->IsAlive() == false ) )
	{
		SetEnemy( NULL );
	}

	//Acquire the target
	if ( GetEnemy() == NULL )
	{
		HackFindEnemy();
	}

	LaserOn();

	CBaseEntity *pEnemy = GetEnemy();

	//If we've found a target, spin up the barrel and start to attack
	if ( pEnemy != NULL )
	{
		//Get our shot positions
		Vector vecMid = EyePosition();
		Vector vecMidEnemy = pEnemy->BodyTarget( vecMid );

		//Look for our current enemy
		bool bEnemyInFOV = FInViewCone( pEnemy );
		bool bEnemyVisible = FVisible( pEnemy );

		//Calculate dir and dist to enemy
		Vector	vecDirToEnemy = vecMidEnemy - vecMid;	
		m_flDistToEnemy = VectorNormalize( vecDirToEnemy );

		// If the enemy isn't in the normal fov, check the fov through portals
		CPortal_Base2D *pPortal = NULL;
		pPortal = FInViewConeThroughPortal( pEnemy );

		if ( pPortal && FVisibleThroughPortal( pPortal, pEnemy ) )
		{
			// Translate our target across the portal
			Vector vecMidEnemyTransformed;
			UTIL_Portal_PointTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecMidEnemy, vecMidEnemyTransformed );

			//Calculate dir and dist to enemy
			Vector	vecDirToEnemyTransformed = vecMidEnemyTransformed - vecMid;	
			float	flDistToEnemyTransformed = VectorNormalize( vecDirToEnemyTransformed );

			// If it's not visible through normal means or the enemy is closer through the portal, use the translated info
			if ( !bEnemyInFOV || !bEnemyVisible || flDistToEnemyTransformed < m_flDistToEnemy )
			{
				bEnemyInFOV = true;
				bEnemyVisible = true;
				vecMidEnemy = vecMidEnemyTransformed;
				vecDirToEnemy = vecDirToEnemyTransformed;
				m_flDistToEnemy = flDistToEnemyTransformed;
			}
		}

		// Give enemies that are farther away a longer grace period
		float fDistanceRatio = m_flDistToEnemy / m_flTurretRange;
		m_flShotTime = gpGlobals->curtime + fDistanceRatio * fDistanceRatio * PORTAL_FLOOR_TURRET_MAX_SHOT_DELAY;

		m_flLastSight = 0;
		SetThink( &CNPC_FloorTurret::ActiveThink );
		SetEyeState( TURRET_EYE_SEE_TARGET );

		SpinUp();

		if ( gpGlobals->curtime > m_flNextActivateSoundTime )
		{
			if (!this->m_bGagged)
				EmitSound( "NPC_FloorTurret.Activate" );
			m_flNextActivateSoundTime = gpGlobals->curtime + 3.0;
		}
		return;
	}

	//Are we out of time and need to retract?
	if ( gpGlobals->curtime > m_flLastSight )
	{
		//Before we retrace, make sure that we are spun down.
		m_flLastSight = 0;
		SetThink( &CNPC_FloorTurret::Retire );
		return;
	}

	//Display that we're scanning
	m_vecGoalAngles.x = GetAbsAngles().x + ( sin( ( m_flLastSight + gpGlobals->curtime * m_fSearchSpeed ) * 1.5f ) * 20.0f );
	m_vecGoalAngles.y = GetAbsAngles().y + ( sin( ( m_flLastSight + gpGlobals->curtime * m_fSearchSpeed ) * 2.5f ) * 20.0f );

	//Turn and ping
	UpdateFacing();
	Ping();
}

void CNPC_Portal_FloorTurret::AutoSearchThink( void )
{
	LaserOn();

	BaseClass::AutoSearchThink();
}

//-----------------------------------------------------------------------------
// Purpose: The turret has been tipped over and will thrash for awhile
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::TippedThink( void )
{
	PreThink( TURRET_TIPPED );

	SetNextThink( gpGlobals->curtime + 0.05f );
	SetEnemy( NULL );

	StudioFrameAdvance();
	// If we're not on side anymore, stop thrashing
	if ( !OnSide() && VPhysicsGetObject()->GetContactPoint( NULL, NULL ) )
	{
		ReturnToLife();
		return;
	}

	LaserOn();

	//See if we should continue to thrash
	if ( gpGlobals->curtime < m_flThrashTime && !IsDissolving() )
	{
		if ( m_flShotTime < gpGlobals->curtime )
		{
			if( m_bOutOfAmmo )
			{
				SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );
				DryFire();
			}
			else if (sv_portal_turret_shoot_at_death.GetInt())
			{
				Vector vecMuzzle, vecMuzzleDir;
				GetAttachment( m_iMuzzleAttachment, vecMuzzle, &vecMuzzleDir );

				SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );
				SetActivity( (Activity)( ( m_bShootWithBottomBarrels ) ? ( ACT_FLOOR_TURRET_FIRE2 ) : ( ACT_FLOOR_TURRET_FIRE ) ) );

#if !DISABLE_SHOT
				Shoot( vecMuzzle, vecMuzzleDir );
#endif
			}

			m_flShotTime = gpGlobals->curtime + 0.05f;
		}

		m_vecGoalAngles.x = GetAbsAngles().x + random->RandomFloat( -60, 60 );
		m_vecGoalAngles.y = GetAbsAngles().y + random->RandomFloat( -60, 60 );

		UpdateFacing();
	}
	else
	{
		//Face forward
		m_vecGoalAngles = GetAbsAngles();

		//Set ourselves to close
		if ( GetActivity() != ACT_FLOOR_TURRET_CLOSE )
		{
			SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );

			//If we're done moving to our desired facing, close up
			if ( UpdateFacing() == false )
			{
				if (!this->m_bGagged)
				{
					EmitSound("NPC_FloorTurret.Die");
				}
				if (!this->m_bGagged)
				{
					EmitSound(GetTurretTalkName(PORTAL_TURRET_DISABLED));
				}
				SpinDown();

				SetActivity( (Activity) ACT_FLOOR_TURRET_CLOSE );
				if (!this->m_bGagged)
					EmitSound( "NPC_FloorTurret.Retract" );

				CTakeDamageInfo	info;
				info.SetDamage( 1 );
				info.SetDamageType( DMG_CRUSH );
				Event_Killed( info );
			}
		}
		else if ( IsActivityFinished() )
		{	
			m_bActive		= false;
			m_flLastSight	= 0;

			SetActivity( (Activity) ACT_FLOOR_TURRET_CLOSED_IDLE );

			// Don't need to store last NPC anymore, because I've been knocked over
			if ( m_hLastNPCToKickMe )
			{
				m_hLastNPCToKickMe = NULL;
				m_flKnockOverFailedTime = 0;
			}

			//Try to look straight
			if ( UpdateFacing() == false )
			{
				m_OnTipped.FireOutput( this, this );
				SetEyeState( TURRET_EYE_DEAD );
				//SetCollisionGroup( COLLISION_GROUP_DEBRIS_TRIGGER );

				// Start thinking slowly to see if we're ever set upright somehow
				SetThink( &CNPC_FloorTurret::InactiveThink );
				SetNextThink( gpGlobals->curtime + 1.0f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The turret has been tipped over and will thrash for awhile
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::HeldThink( void )
{
	PreThink( (turretState_e)PORTAL_TURRET_PICKUP );

	SetNextThink( gpGlobals->curtime + 0.05f );
	SetEnemy( NULL );

	StudioFrameAdvance();

	IPhysicsObject *pTurretPhys = VPhysicsGetObject();

	// If we're not held anymore, stop thrashing
	if ( !(pTurretPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
	{
		m_fNextTalk = gpGlobals->curtime + 1.25f;

		if ( m_lifeState == LIFE_ALIVE )
			SetThink( &CNPC_FloorTurret::ActiveThink );
		else
			SetThink( &CNPC_FloorTurret::InactiveThink );
	}

	LaserOn();

	//See if we should continue to thrash
	if ( !IsDissolving() )
	{
		if ( m_flShotTime < gpGlobals->curtime )
		{
			SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );

			DryFire();

			m_flShotTime = gpGlobals->curtime + RandomFloat( 0.25f, 0.75f );

			m_vecGoalAngles.x = GetAbsAngles().x + RandomFloat( -15, 15 );
			m_vecGoalAngles.y = GetAbsAngles().y + RandomFloat( -40, 40 );
		}

		UpdateFacing();
	}
}

void CNPC_Portal_FloorTurret::InactiveThink( void )
{
	LaserOff();

	// Update our PVS state
	CheckPVSCondition();

	SetNextThink( gpGlobals->curtime + 1.0f );

	// Wake up if we're not on our side
	if ( !OnSide() && VPhysicsGetObject()->GetContactPoint( NULL, NULL ) && m_bEnabled )
	{
		// Never return to life!
		MakeSolid();
		//ReturnToLife();
	}

	if (IsOnFire())
	{
		MakeSolid();
		ReturnToLife();
		LaserOn();
		PreThink( (turretState_e)PORTAL_TURRET_STARTBURNING );
	}

	if (m_pPhysicsObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
	{
		if (player_held_object_collide_with_player.GetBool())
		{
			SetCollisionGroup(COLLISION_GROUP_NONE);
		}
		SetCollisionGroup(COLLISION_GROUP_PLAYER_HELD);
	}

	if (m_pPhysicsObject->IsAsleep())
	{
		SetCollisionGroup(COLLISION_GROUP_DEBRIS_TRIGGER);
	}
}

void CNPC_Portal_FloorTurret::DieThink()
{
	LaserOff();

	StudioFrameAdvance();
	SetNextThink(gpGlobals->curtime + 0.1f);
	m_bIsDead = true;
	if (GetActivity() != ACT_FLOOR_TURRET_DIE)
	{
		if (IsActivityFinished())
		{
			SetActivity((Activity)ACT_FLOOR_TURRET_DIE_IDLE);
			SetThink(&CNPC_Portal_FloorTurret::InactiveThink);
			SetNextThink(gpGlobals->curtime + 1.0f);
		}
	}
	else
	{
		SetActivity((Activity)ACT_FLOOR_TURRET_DIE);
	}
}

void CNPC_Portal_FloorTurret::SuppressThink( void )
{
	LaserOn();

	BaseClass::SuppressThink();
}

//-----------------------------------------------------------------------------
// Purpose: The turret is not doing anything at all
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::DisabledThink(void)
{
	float thinkTime;

	LaserOff();

	if (this->m_bUsedAsActor)
	{
		NPCThink();
		thinkTime = gpGlobals->curtime + 0.1;
	}
	else
	{
		thinkTime = gpGlobals->curtime + 0.5;
	}

	SetNextThink(thinkTime);
	if ( OnSide() )
	{
		m_OnTipped.FireOutput( this, this );
		SetEyeState( TURRET_EYE_DEAD );
		SetThink( NULL );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::BreakThink(void)
{
	Vector vecUp;
	GetVectors(NULL, NULL, &vecUp);
	Vector vecOrigin = WorldSpaceCenter() + (vecUp * 12.0f);

	// K-boom
	RadiusDamage(CTakeDamageInfo(this, this, 15.0f, DMG_BLAST), vecOrigin, (10 * 12), CLASS_NONE, this);

	EmitSound("NPC_FloorTurret.Destruct");

	if (g_pGameRules->IsMultiplayer())
	{
		DispatchParticleEffect("turret_coop_explosion", vecOrigin, GetAbsAngles(), PATTACH_ABSORIGIN, NULL);
	}
	else
	{
		breakablepropparams_t params(GetAbsOrigin(), GetAbsAngles(), vec3_origin, RandomAngularImpulse(-800.0f, 800.0f));
		params.impactEnergyScale = 1.0f;
		params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;

		// no damage/damage force? set a burst of 100 for some movement
		params.defBurstScale = 100;
		PropBreakableCreateAll(GetModelIndex(), VPhysicsGetObject(), params, this, -1, true);

		// Throw out some small chunks too obscure the explosion even more
		CPVSFilter filter(vecOrigin);
		Vector gibVelocity = RandomVector(-100, 100);
		int iModelIndex = modelinfo->GetModelIndex(g_PropDataSystem.GetRandomChunkModel("MetalChunks"));
		te->BreakModel(filter, 0.0, vecOrigin, GetAbsAngles(), Vector(40, 40, 40), gibVelocity, iModelIndex, 150, 4, 2.5, BREAK_METAL);
	}
	// We're done!
	UTIL_Remove(this);
}

void CNPC_Portal_FloorTurret::BurnThink(void)
{
	if (gpGlobals->curtime <= this->m_flBurnExplodeTime)
	{
		if (gpGlobals->curtime > m_fNextTalk)
		{
			if (!this->m_bGagged)
				EmitSound(GetTurretTalkName(PORTAL_TURRET_BURNED ) );
			m_fNextTalk = random->RandomFloat(0.5f, 0.75f);
		}
		SetThink(&CNPC_Portal_FloorTurret::BurnThink);
		SetNextThink(gpGlobals->curtime + 0.1f);
	}
	else
	{
		ExplosionCreate(WorldSpaceCenter(), GetAbsAngles(), this, 1000, 500, 1385);
		UTIL_ScreenShake(WorldSpaceCenter(), 20.0f, 150.0f, 0.75f, 750.0f, SHAKE_START, false);
		SetThink(&CNPC_Portal_FloorTurret::BreakThink);
		SetNextThink(gpGlobals->curtime);
		StopSound(GetTurretTalkName(PORTAL_TURRET_BURNED));
	}
}

//-----------------------------------------------------------------------------
// Purpose: The turret doesn't run base AI properly, which is a bad decision.
//			As a result, it has to manually find enemies.
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::HackFindEnemy( void )
{
	// We have to refresh our memories before finding enemies, so
	// dead enemies are cleared out before new ones are added.
	GetEnemies()->RefreshMemories();

	GetSenses()->Look( m_flTurretRange );
	SetEnemy(BestEnemy());
}

void CNPC_Portal_FloorTurret::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	IPhysicsObject *pOtherPhys = pOther->VPhysicsGetObject();

	if ( !pOtherPhys )
		return;

	if ( !m_pMotionController )
		return;

	if ( m_pMotionController->Enabled() )
	{
		m_pMotionController->Suspend( 2.0f );

		IPhysicsObject *pTurretPhys = VPhysicsGetObject();

		if ( !pOther->IsPlayer() && pOther->GetMoveType() == MOVETYPE_VPHYSICS && !(pTurretPhys && ((pTurretPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD) != 0)) )
		{
			// Get a lateral impulse
			Vector vVelocityImpulse = GetAbsOrigin() - pOther->GetAbsOrigin();
			vVelocityImpulse.z = 0.0f;
			if ( vVelocityImpulse.IsZero() )
			{
				vVelocityImpulse.x = 1.0f;
				vVelocityImpulse.y = 1.0f;
			}
			VectorNormalize( vVelocityImpulse );

			// If impulse is too much along the forward or back axis, skew it
			Vector vTurretForward, vTurretRight;
			GetVectors( &vTurretForward, &vTurretRight, NULL );
			float fForwardDotImpulse = vTurretForward.Dot( vVelocityImpulse );
			if ( fForwardDotImpulse > 0.7f || fForwardDotImpulse < -0.7f )
			{
				vVelocityImpulse += vTurretRight;
				VectorNormalize( vVelocityImpulse );
			}

			Vector vAngleImpulse( ( vTurretRight.Dot( vVelocityImpulse ) < 0.0f ) ? ( -1.6f ) : ( 1.6f ), RandomFloat( -0.5f, 0.5f ), RandomFloat( -0.5f, 0.5f ) );

			vVelocityImpulse *= TURRET_FLOOR_PHYSICAL_FORCE_MULTIPLIER;
			vAngleImpulse *= TURRET_FLOOR_PHYSICAL_FORCE_MULTIPLIER;
			pTurretPhys->AddVelocity( &vVelocityImpulse, &vAngleImpulse );

			// Check if another turret is hitting us
			CNPC_Portal_FloorTurret *pPortalFloor = dynamic_cast<CNPC_Portal_FloorTurret*>( pOther );
			if ( pPortalFloor && pPortalFloor->m_lifeState == LIFE_ALIVE )
			{
				Vector vTurretVelocity, vOtherVelocity;
				pTurretPhys->GetVelocity( &vTurretVelocity, NULL );
				pOtherPhys->GetVelocity( &vOtherVelocity, NULL );

				// If it's moving faster
				if ( vOtherVelocity.LengthSqr() > vTurretVelocity.LengthSqr() )
				{
					// Make the turret falling onto this one talk
					pPortalFloor->EmitSound( GetTurretTalkName( PORTAL_TURRET_COLLIDE ) );
					pPortalFloor->m_fNextTalk = gpGlobals->curtime + 1.2f;
					pPortalFloor->m_bDelayTippedTalk = true;

					// Delay out potential tipped talking so we can here the other turret talk
					m_fNextTalk = gpGlobals->curtime + 0.6f;
					m_bDelayTippedTalk = true;
				}
			}

			if ( pPortalFloor && m_bEnabled && m_bLaserOn && !m_bOutOfAmmo )
			{
				// Award friendly fire achievement if we're a live turret being knocked over by another turret.
				IGameEvent *event = gameeventmanager->CreateEvent( "turret_hit_turret" );
				if ( event )
				{
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}
}

void CNPC_Portal_FloorTurret::LaserOff( void )
{
	m_bLaserOn = false;
}

void CNPC_Portal_FloorTurret::LaserOn( void )
{
	if (m_nModelIndex == 2)
		m_bLaserOn = false;

	m_bLaserOn = true;
}
void CNPC_Portal_FloorTurret::FireBullet( const char *pTargetName )
{
	CBaseEntity *pEnemy = gEntList.FindEntityByName( NULL, pTargetName );
	if ( !pEnemy )
		return;

	//Get our shot positions
	Vector vecMid = EyePosition();
	Vector vecMidEnemy = pEnemy->BodyTarget( vecMid );

	// Store off our last seen location so we can suppress it later
	m_vecEnemyLKP = vecMidEnemy;

	//Calculate dir and dist to enemy
	Vector	vecDirToEnemy = vecMidEnemy - vecMid;
	m_flDistToEnemy = VectorNormalize( vecDirToEnemy );

	//We want to look at the enemy's eyes so we don't jitter
	Vector	vecDirToEnemyEyes = vecMidEnemy - vecMid;
	VectorNormalize( vecDirToEnemyEyes );

	QAngle vecAnglesToEnemy;
	VectorAngles( vecDirToEnemyEyes, vecAnglesToEnemy );

	Vector vecMuzzle, vecMuzzleDir;
	GetAttachment( m_iMuzzleAttachment, vecMuzzle, &vecMuzzleDir );

	if ( m_flShotTime < gpGlobals->curtime )
	{
		Vector2D vecDirToEnemy2D = vecDirToEnemy.AsVector2D();
		Vector2D vecMuzzleDir2D = vecMuzzleDir.AsVector2D();

		if ( m_flDistToEnemy < 60.0 )
		{
			vecDirToEnemy2D.NormalizeInPlace();
			vecMuzzleDir2D.NormalizeInPlace();
		}

		//Fire the gun
		if( m_bOutOfAmmo )
		{
			DryFire();
		}
		else
		{
			SetActivity( (Activity) ACT_FLOOR_TURRET_OPEN_IDLE );
			SetActivity( (Activity)( ( m_bShootWithBottomBarrels ) ? ( ACT_FLOOR_TURRET_FIRE2 ) : ( ACT_FLOOR_TURRET_FIRE ) ) );

			//Fire the weapon
#if !DISABLE_SHOT
			Shoot( vecMuzzle, vecDirToEnemy, true );
#endif
		}

		//If we can see our enemy, face it
		m_vecGoalAngles.y = vecAnglesToEnemy.y;
		m_vecGoalAngles.x = vecAnglesToEnemy.x;

		//Turn to face
		UpdateFacing();

		if(!this->m_bGagged)
			EmitSound( "NPC_FloorTurret.Alert" );
		SetThink( &CNPC_FloorTurret::SuppressThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fire a bullet in the specified direction
//-----------------------------------------------------------------------------
void CNPC_Portal_FloorTurret::InputFireBullet( inputdata_t &inputdata )
{
	FireBullet( inputdata.value.String() );
}

void CNPC_Portal_FloorTurret::InputEnableGagging(inputdata_t& inputdata)
{
	m_bGagged = true;
}

void CNPC_Portal_FloorTurret::InputDisableGagging(inputdata_t& inputdata)
{
	m_bGagged = false;
}

void CNPC_Portal_FloorTurret::InputEnablePickup(inputdata_t& inputdata)
{
	m_bPickupEnabled = true;
}

void CNPC_Portal_FloorTurret::InputDisablePickup(inputdata_t& inputdata)
{
	m_bPickupEnabled = false;
}

void CNPC_Portal_FloorTurret::InputSelfDestructImmediately(inputdata_t& inputdata)
{
	SetThink(&CNPC_Portal_FloorTurret::BreakThink);
	SetNextThink(gpGlobals->curtime);
}

void CNPC_Portal_FloorTurret::InputSetModel(inputdata_t& inputdata)
{
	SetTurretType(inputdata.value.Int());
	VPhysicsDestroyObject();
	CreateVPhysics();
}

void CNPC_Portal_FloorTurret::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = ToBasePlayer(pActivator);

	if ( this->m_bPickupEnabled && pActivator)
		pPlayer->PickupObject(this, false);
}

float CNPC_Portal_FloorTurret::GetFireConeZTolerance()
{
	return sv_portal_turret_fire_cone_z_tolerance.GetFloat();
}

bool CNPC_Portal_FloorTurret::IsMovingSuddenly()
{
	Vector vVelocity;

	if (m_pPhysicsObject)
	{
		if (m_pPhysicsObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
		{
			m_pPhysicsObject->GetVelocity(&vVelocity, NULL);
			m_flPreviousVelocity = (vVelocity.x * vVelocity.x + vVelocity.y * vVelocity.y + vVelocity.z * vVelocity.z);	
		
			if (200.0f < m_flPreviousVelocity)
			{
				return true;
			}
		}
	}
	
	return false;
}

bool CNPC_Portal_FloorTurret::IsEnemyBehindGlass(CPortal_Base2D* pPortal, CBaseEntity* pEnemy, Vector& vecMuzzle, Vector& vecDirToEnemy, float flDistToEnemy)
{
	bool pTraceFilter;

	Ray_t ray_to_enemy;
	
	ray_to_enemy.m_Start = vecMuzzle;
	ray_to_enemy.m_Delta = vecDirToEnemy * m_flDistToEnemy;
	ray_to_enemy.m_IsRay = true;

	trace_t tr;

	Vector target;

	CTraceFilterSimple filter(this, COLLISION_GROUP_NONE);

	pTraceFilter = UTIL_Portal_TraceRay_Bullets(pPortal, ray_to_enemy, CONTENTS_WINDOW, &filter, &tr);

	if (g_debug_turret.GetBool())
	{
		NDebugOverlay::Line(tr.startpos, tr.endpos, 255, 0, 0, true, 1.0f);
		if (pTraceFilter)
		{
			m_flDistToEnemy * pPortal->GetAbsOrigin() - vecMuzzle;
			target = vecMuzzle + vecDirToEnemy * m_flDistToEnemy;
			NDebugOverlay::Line(vecMuzzle, target, 0, 0, 255, true, 1.0f);
		}
		NDebugOverlay::Sphere(tr.endpos, 4.0f, 0, 255, 0, true, 1.0);
	}

	if ((tr.fraction < 1.0 || tr.allsolid) || (tr.startsolid))
	{
		return true;
	}

	return false;
}

void CNPC_Portal_FloorTurret::MakeSolid()
{
	if (!player_held_object_collide_with_player.GetBool() && m_pPhysicsObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
	{
		SetCollisionGroup(COLLISION_GROUP_PLAYER_HELD);
	}
	else
	{
		SetCollisionGroup(COLLISION_GROUP_NONE);
	}
}
