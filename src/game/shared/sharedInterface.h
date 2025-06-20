//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Exposes client-server neutral interfaces implemented in both places
//
// $NoKeywords: $
//=============================================================================//

#ifndef SHAREDINTERFACE_H
#define SHAREDINTERFACE_H

#ifdef POSIX
#define random random_valve// stdlib.h defined random() and our class defn conflicts so under POSIX rename it using the preprocessor
#endif

class IFileSystem;
class IUniformRandomStream;
class CGaussianRandomStream;
class IEngineSound;
class IMapData;
class IGameTypes;

extern IFileSystem				*filesystem;
#if defined(_STATIC_LINKED) && defined(_SUBSYSTEM) && (defined(CLIENT_DLL) || defined(GAME_DLL))
namespace _SUBSYSTEM
{
extern IUniformRandomStream		*random;
}
#else
extern IUniformRandomStream		*random;
#endif
extern CGaussianRandomStream *randomgaussian;
extern IEngineSound				*enginesound;
extern IMapData					*g_pMapData;			// TODO: current implementations of the 
														// interface are in TF2, should probably move
														// to TF2/HL2 neutral territory
#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
extern IGameTypes				*g_pGameTypes;
#endif


#endif // SHAREDINTERFACE_H

