//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( CDLL_UTIL_H )
#define CDLL_UTIL_H

#ifdef _WIN32
#pragma once
#endif

#include <soundflags.h>
#include "mathlib/vector.h"
#include <shareddefs.h>
#include "color.h"

#include "shake.h"
#include "bitmap/imageformat.h"
#include "ispatialpartition.h"
#include "materialsystem/MaterialSystemUtil.h"

class Vector;
class QAngle;
class IMaterial;
class ITexture;
class IClientEntity;
class CHudTexture;
class CGameTrace;
class C_BaseEntity;
class C_BasePlayer;

struct Ray_t;
struct client_textmessage_t;
typedef CGameTrace trace_t;

namespace vgui
{
	typedef unsigned long HFont;
	class EditablePanel;
};



extern bool g_MakingDevShots;

// ScreenHeight returns the height of the screen, in pixels
int		ScreenHeight( void );
// ScreenWidth returns the width of the screen, in pixels
int		ScreenWidth( void );

inline void ConvertNormalizedScreenSpaceToPixelScreenSpace( Vector &in )
{
	in.x = ( ( 1.0f + in.x ) * 0.5f ) * ScreenWidth();
	in.y = ScreenHeight() - ( ( ( 1.0f + in.y ) * 0.5f ) * ScreenHeight() );
}


#define XRES(x)	( x  * ( ( float )ScreenWidth() / 640.0 ) )
#define YRES(y)	( y  * ( ( float )ScreenHeight() / 480.0 ) )

int		UTIL_ComputeStringWidth( vgui::HFont& font, const char *str );
int		UTIL_ComputeStringWidth( vgui::HFont& font, const wchar_t *str );
float	UTIL_AngleDiff( float destAngle, float srcAngle );
void	UTIL_Bubbles( const Vector& mins, const Vector& maxs, int count );
void	UTIL_Smoke( const Vector &origin, const float scale, const float framerate );
void	UTIL_ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName = NULL );
int		UTIL_PrecacheDecal( const char *name, bool preload = false );
void	UTIL_EmitAmbientSound( C_BaseEntity *entity, const Vector &vecOrigin, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch );
void	UTIL_SetOrigin( C_BaseEntity *entity, const Vector &vecOrigin );
void	UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake=false );
byte	*UTIL_LoadFileForMe( const char *filename, int *pLength );
void	UTIL_FreeFile( byte *buffer );
void	UTIL_MakeSafeName( const char *oldName, char *newName, int newNameBufSize );	///< Cleans up player names for putting in vgui controls (cleaned names can be up to original*2+1 in length)
const char *UTIL_SafeName( const char *oldName );	///< Wraps UTIL_MakeSafeName, and returns a static buffer
void	UTIL_ReplaceKeyBindings( const wchar_t *inbuf, int inbufsizebytes, OUT_Z_BYTECAP(outbufsizebytes) wchar_t *outbuf, int outbufsizebytes );
void	UTIL_SetControlStringWithKeybindings( vgui::EditablePanel *panel, const char *controlName, const char *str );

void	UTIL_MessageText( C_BasePlayer *player, const char *text, Color color = Color( 0, 0, 0, 0 ) );

// Fade out an entity based on distance fades
unsigned char UTIL_ComputeEntityFade( C_BaseEntity *pEntity, float flMinDist, float flMaxDist, float flFadeScale );

client_textmessage_t	*TextMessageGet( const char *pName );

char	*VarArgs( PRINTF_FORMAT_STRING const char *format, ... );
	

// Get the entity the local player is spectating (can be a player or a ragdoll entity).
int		GetSpectatorTarget();
//bool	CanSeeSpectatorOnlyTools( void );
bool	CanToggleXRayView( void );
int		GetSpectatorMode( void );
bool	IsPlayerIndex( int index );
int		GetLocalPlayerIndex( void );
int		GetLocalPlayerTeam( void );
bool	IsLocalPlayerSpectator( void );
void	NormalizeAngles( QAngle& angles );
void	InterpolateAngles( const QAngle& start, const QAngle& end, QAngle& output, float frac );
void	InterpolateVector( float frac, const Vector& src, const Vector& dest, Vector& output );

const char *nexttoken(char *token, const char *str, char sep);

void UTIL_PrecacheOther( const char *szClassname );

void UTIL_SetTrace(trace_t& tr, const Ray_t& ray, C_BaseEntity *edict, float fraction, int hitgroup, unsigned int contents, const Vector& normal, float intercept );

bool GetVectorInScreenSpace( Vector pos, int& iX, int& iY, Vector *vecOffset = NULL );
bool GetTargetInScreenSpace( C_BaseEntity *pTargetEntity, int& iX, int& iY, Vector *vecOffset = NULL );

// prints messages through the HUD (stub in client .dll right now )
class C_BasePlayer;
void ClientPrint( C_BasePlayer *player, int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );

C_BasePlayer* UTIL_PlayerByUserId( int userID );

C_BaseEntity* UTIL_EntityFromUserMessageEHandle( long nEncodedEHandle );

// Pass in an array of pointers and an array size, it fills the array and returns the number inserted
int			UTIL_EntitiesInBox( C_BaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask, int partitionMask = PARTITION_CLIENT_NON_STATIC_EDICTS );
int			UTIL_EntitiesInSphere( C_BaseEntity **pList, int listMax, const Vector &center, float radius, int flagMask, int partitionMask = PARTITION_CLIENT_NON_STATIC_EDICTS );

int			UTIL_RenderablesInBox( C_BaseEntity** pList, int listMax, const Vector &mins, const Vector &maxs );

// make this a fixed size so it just sits on the stack
#define MAX_SPHERE_QUERY	256
class CEntitySphereQuery
{
public:
	// currently this builds the list in the constructor
	// UNDONE: make an iterative query of ISpatialPartition so we could
	// make queries like this optimal
	CEntitySphereQuery( const Vector &center, float radius, int flagMask=0, int partitionMask = PARTITION_CLIENT_NON_STATIC_EDICTS );
	C_BaseEntity *GetCurrentEntity();
	inline void NextEntity() { m_listIndex++; }

private:
	int			m_listIndex;
	int			m_listCount;
	C_BaseEntity *m_pList[MAX_SPHERE_QUERY];
};

C_BaseEntity *CreateEntityByName( const char *className );
// creates an entity by name, and ensure it's correctness
// does not spawn the entity
// use the CREATE_ENTITY() macro which wraps this, instead of using it directly
template< class T >
T *_CreateEntity( T *newClass, const char *className )
{
	T *newEnt = dynamic_cast<T*>( CreateEntityByName(className) );
	if ( !newEnt )
	{
		Warning( "classname %s used to create wrong class type\n", className );
		Assert(0);
	}

	return newEnt;
}

#define CREATE_ENTITY( newClass, className ) _CreateEntity( (newClass*)NULL, className )
#define CREATE_UNSAVED_ENTITY( newClass, className ) _CreateEntityTemplate( (newClass*)NULL, className )

// Misc useful
inline bool FStrEq(const char *sz1, const char *sz2)
{
	return(stricmp(sz1, sz2) == 0);
}

// Given a vector, clamps the scalar axes to MAX_COORD_FLOAT ranges from worldsize.h
void UTIL_BoundToWorldSize( Vector *pVecPos );

void UTIL_ApproachTarget( float target, float increaseSpeed, float decreaseSpeed, float *val );
void UTIL_ApproachTarget( const Vector &target, float increaseSpeed, float decreaseSpeed, Vector *val );


// Returns true if the user has loaded any maps, false otherwise.
bool UTIL_HasLoadedAnyMap();

wchar_t *UTIL_GetLocalizedKeyString( const char *command, const char *fmt, const wchar_t *arg1 = NULL, const wchar_t *arg2 = NULL, const wchar_t *arg3 = NULL );

void UTIL_GetClientStatusText( char *buffer, int bufsize );

class CGameTrace;
typedef CGameTrace trace_t;
void		UTIL_ClearTrace			( trace_t &trace );

#endif // !CDLL_UTIL_H
