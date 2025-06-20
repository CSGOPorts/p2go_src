//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_baseentity.h"
#include "prediction.h"
#include "model_types.h"
#include "iviewrender_beams.h"
#include "dlight.h"
#include "iviewrender.h"
#include "view.h"
#include "iefx.h"
#include "c_team.h"
#include "clientmode.h"
#include "usercmd.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "tier0/vprof.h"
#include "fx_line.h"
#include "interface.h"
#include "materialsystem/imaterialsystem.h"
#include "soundinfo.h"
#include "mathlib/vmatrix.h"
#include "isaverestore.h"
#include "tier2/interval.h"
#include "engine/ivdebugoverlay.h"
#include "c_ai_basenpc.h"
#include "apparent_velocity_helper.h"
#include "c_baseanimatingoverlay.h"
#include "tier1/keyvalues.h"
#include "hltvcamera.h"
#include "datacache/imdlcache.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "decals.h"
#include "cdll_bounded_cvars.h"
#include "inetchannelinfo.h"
#include "clientalphaproperty.h"
#include "cellcoord.h"
#include "gamestringpool.h"
#include "tier1/callqueue.h"

#if defined( CSTRIKE15 ) && defined( CSTRIKE_DLL )
#include "cs_gamerules.h"
#include "c_cs_player.h"
#endif

#ifdef DOTA_DLL
#include "dota_in_main.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifdef INTERPOLATEDVAR_PARANOID_MEASUREMENT
	int g_nInterpolatedVarsChanged = 0;
	bool g_bRestoreInterpolatedVarValues = false;
#endif


static bool g_bWasSkipping = (bool)-1;
static bool g_bWasThreaded =(bool)-1;

void cc_cl_interp_all_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.GetInt() )
	{
		C_BaseEntityIterator iterator;
		C_BaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != NULL )	
		{
			if ( pEnt->ShouldInterpolate() )
			{
				pEnt->AddToEntityList(ENTITY_LIST_INTERPOLATE);
			}
		}
	}
}

static ConVar  report_cliententitysim( "report_cliententitysim", "0", FCVAR_CHEAT, "List all clientside simulations and time - will report and turn itself off." );
static ConVar  cl_extrapolate( "cl_extrapolate", "1", FCVAR_CHEAT, "Enable/disable extrapolation if interpolation history runs out." );
static ConVar  cl_interp_npcs( "cl_interp_npcs", "0.0", 0, "Interpolate NPC positions starting this many seconds in past (or cl_interp, if greater)" );  
static ConVar  cl_interp_all( "cl_interp_all", "0", 0, "Disable interpolation list optimizations.", 0, 0, 0, 0, cc_cl_interp_all_changed );
ConVar  r_drawmodeldecals( "r_drawmodeldecals", "1" );
extern ConVar	cl_showerror;
int C_BaseEntity::m_nPredictionRandomSeed = -1;
C_BasePlayer *C_BaseEntity::m_pPredictionPlayer = NULL;
bool C_BaseEntity::s_bAbsQueriesValid = true;
bool C_BaseEntity::s_bAbsRecomputationEnabled = true;
bool C_BaseEntity::s_bInterpolate = true;
int C_BaseEntity::s_nIncomingPacketCommandsAcknowledged = -1;

bool C_BaseEntity::sm_bDisableTouchFuncs = false;	// Disables PhysicsTouch and PhysicsStartTouch function calls

static ConVar  r_drawrenderboxes( "r_drawrenderboxes", "0", FCVAR_CHEAT, "(0 - off) (1 - Draws the bounding box of entities) (2 - Draws the axis aligned bounding box used for culling) (3 - draws both bounding boxes)" );  

static bool g_bAbsRecomputationStack[8];
static unsigned short g_iAbsRecomputationStackPos = 0;

// All the entities that want Interpolate() called on them.
static CUtlLinkedList<C_BaseEntity*, unsigned short> g_EntityLists[NUM_ENTITY_LISTS];
static bool s_bImmediateRemovesAllowed = true;
bool SetImmediateEntityRemovesAllowed( bool bAllowed )
{
	bool bOldValue = s_bImmediateRemovesAllowed;
	s_bImmediateRemovesAllowed = bAllowed;
	return bOldValue;
}

#if !defined( NO_ENTITY_PREDICTION )

// Create singleton
static CPredictableList g_Predictables[ MAX_SPLITSCREEN_PLAYERS ];
CPredictableList *GetPredictables( int nSlot )
{
	AssertMsg1( nSlot >= 0, "Tried to get prediction slot for player %d. This probably means you are predicting something that isn't local. Crash ensues.", nSlot );
	return &g_Predictables[ nSlot ];
}

//-----------------------------------------------------------------------------
// Purpose: Add entity to list
// Input  : add - 
// Output : int
//-----------------------------------------------------------------------------
void CPredictableList::AddToPredictableList( C_BaseEntity *add )
{
	// This is a hack to remap slot to index
	if ( m_Predictables.Find( add ) != m_Predictables.InvalidIndex() )
	{
		return;
	}

	// Add to general sorted list
	m_Predictables.Insert( add );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : remove - 
//-----------------------------------------------------------------------------
void CPredictableList::RemoveFromPredictablesList( C_BaseEntity *remove )
{
	m_Predictables.FindAndRemove( remove );
}

//-----------------------------------------------------------------------------
// Purpose: Searc predictables for previously created entity (by testId)
// Input  : testId - 
// Output : static C_BaseEntity
//-----------------------------------------------------------------------------
static C_BaseEntity *FindPreviouslyCreatedEntity( CPredictableId& testId )
{
#if defined( USE_PREDICTABLEID )
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		int c = GetPredictables( hh )->GetPredictableCount();

		int i;
		for ( i = 0; i < c; i++ )
		{
			C_BaseEntity *e = GetPredictables( hh )->GetPredictable( i );
			if ( !e || !e->IsClientCreated() )
				continue;

			// Found it, note use of operator ==
			if ( testId == e->m_PredictableID )
			{
				return e;
			}
		}
	}
#endif
	return NULL;
}
#endif

abstract_class IRecordingList
{
public:
	virtual ~IRecordingList() {};
	virtual void	AddToList( ClientEntityHandle_t add ) = 0;
	virtual void	RemoveFromList( ClientEntityHandle_t remove ) = 0;

	virtual int		Count() = 0;
	virtual IClientRenderable *Get( int index ) = 0;
};

class CRecordingList : public IRecordingList
{
public:
	virtual void	AddToList( ClientEntityHandle_t add );
	virtual void	RemoveFromList( ClientEntityHandle_t remove );

	virtual int		Count();
	IClientRenderable *Get( int index );
private:
	CUtlVector< ClientEntityHandle_t > m_Recording;
};

static CRecordingList g_RecordingList;
IRecordingList *recordinglist = &g_RecordingList;

//-----------------------------------------------------------------------------
// Purpose: Add entity to list
// Input  : add - 
// Output : int
//-----------------------------------------------------------------------------
void CRecordingList::AddToList( ClientEntityHandle_t add )
{
	// This is a hack to remap slot to index
	if ( m_Recording.Find( add ) != m_Recording.InvalidIndex() )
	{
		return;
	}

	// Add to general list
	m_Recording.AddToTail( add );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : remove - 
//-----------------------------------------------------------------------------
void CRecordingList::RemoveFromList( ClientEntityHandle_t remove )
{
	m_Recording.FindAndRemove( remove );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slot - 
// Output : IClientRenderable
//-----------------------------------------------------------------------------
IClientRenderable *CRecordingList::Get( int index )
{
	return cl_entitylist->GetClientRenderableFromHandle( m_Recording[ index ] );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CRecordingList::Count()
{
	return m_Recording.Count();
}

// Helper object implementation
CCurTimeScopeGuard::CCurTimeScopeGuard( float flNewCurTime, bool bOptionalCondition /*= true*/ )
{
	m_bActive = bOptionalCondition;
	if ( m_bActive )
	{
		m_flSavedTime = gpGlobals->curtime;
		gpGlobals->curtime = flNewCurTime;
	}
	else
	{
		m_flSavedTime = 0.0f;
	}
}

CCurTimeScopeGuard::~CCurTimeScopeGuard()
{
	if ( m_bActive )
	{
		gpGlobals->curtime = m_flSavedTime;
	}
}

// Should these be somewhere else?
#define PITCH 0

//-----------------------------------------------------------------------------
// Purpose: Decodes animtime and notes when it changes
// Input  : *pStruct - ( C_BaseEntity * ) used to flag animtime is changine
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_AnimTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BaseEntity *pEntity = ( C_BaseEntity * )pStruct;
	Assert( pOut == &pEntity->m_flAnimTime );

	int t;
	int tickbase;
	int addt;

	// Unpack the data.
	addt	= pData->m_Value.m_Int;

	// Note, this needs to be encoded relative to packet timestamp, not raw client clock
	tickbase = gpGlobals->GetNetworkBase( gpGlobals->tickcount, pEntity->entindex() );

	t = tickbase;
											//  and then go back to floating point time.
	t += addt;				// Add in an additional up to 256 100ths from the server

	// center m_flAnimTime around current time.
	while (t < gpGlobals->tickcount - 127)
		t += 256;
	while (t > gpGlobals->tickcount + 127)
		t -= 256;
	
	pEntity->m_flAnimTime = ( t * TICK_INTERVAL );
}

void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BaseEntity *pEntity = ( C_BaseEntity * )pStruct;
	Assert( pOut == &pEntity->m_flSimulationTime );

	int t;
	int tickbase;
	int addt;

	// Unpack the data.
	addt	= pData->m_Value.m_Int;

	// Note, this needs to be encoded relative to packet timestamp, not raw client clock
	tickbase = gpGlobals->GetNetworkBase( gpGlobals->tickcount, pEntity->entindex() );

	t = tickbase;
											//  and then go back to floating point time.
	t += addt;				// Add in an additional up to 256 100ths from the server

	// center m_flSimulationTime around current time.
	while (t < gpGlobals->tickcount - 127)
		t += 256;
	while (t > gpGlobals->tickcount + 127)
		t -= 256;
	
	float simtime = ( t * TICK_INTERVAL );

	if ( simtime != pEntity->m_flSimulationTime )
	{
		pEntity->OnSimulationTimeChanging( pEntity->m_flSimulationTime, simtime );
		pEntity->m_flSimulationTime = simtime;
	}
}

void C_BaseEntity::RecvProxy_CellBits( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	if ( pEnt->SetCellBits( pData->m_Value.m_Int ) )
	{
		if ( pEnt->ShouldRegenerateOriginFromCellBits() )
		{
			pEnt->m_vecNetworkOrigin.x = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellX, pEnt->m_vecCellOrigin.x );
			pEnt->m_vecNetworkOrigin.y = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellY, pEnt->m_vecCellOrigin.y );
			pEnt->m_vecNetworkOrigin.z = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellZ, pEnt->m_vecCellOrigin.z );
		}
	}
}

void C_BaseEntity::RecvProxy_CellX( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	int *cellX = (int *)pOut;
	Assert( cellX == &pEnt->m_cellX );

	*cellX = pData->m_Value.m_Int;

	// Cell changed, update world position
	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		pEnt->m_vecNetworkOrigin.x = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellX, pEnt->m_vecCellOrigin.x );
	}
}

void C_BaseEntity::RecvProxy_CellY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	int *cellY = (int *)pOut;
	Assert( cellY == &pEnt->m_cellY );

	*cellY = pData->m_Value.m_Int;

	// Cell changed, update world position
	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		pEnt->m_vecNetworkOrigin.y = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellY, pEnt->m_vecCellOrigin.y );
	}
}

void C_BaseEntity::RecvProxy_CellZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	int *cellZ = (int *)pOut;
	Assert( cellZ == &pEnt->m_cellZ );

	*cellZ = pData->m_Value.m_Int;

	// Cell changed, update world position
	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		pEnt->m_vecNetworkOrigin.z = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellZ, pEnt->m_vecCellOrigin.z );
	}
}

void C_BaseEntity::RecvProxy_CellOrigin( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	Vector *vecNetworkOrigin = (Vector *)pOut;

	Assert( vecNetworkOrigin == &pEnt->m_vecNetworkOrigin );

	pEnt->m_vecCellOrigin.x = pData->m_Value.m_Vector[0];
	pEnt->m_vecCellOrigin.y = pData->m_Value.m_Vector[1];
	pEnt->m_vecCellOrigin.z = pData->m_Value.m_Vector[2];

	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		register int const cellwidth = pEnt->m_cellwidth; // Load it into a register
		vecNetworkOrigin->x = CoordFromCell( cellwidth, pEnt->m_cellX, pData->m_Value.m_Vector[0] );
		vecNetworkOrigin->y = CoordFromCell( cellwidth, pEnt->m_cellY, pData->m_Value.m_Vector[1] );
		vecNetworkOrigin->z = CoordFromCell( cellwidth, pEnt->m_cellZ, pData->m_Value.m_Vector[2] );
	}
}

void C_BaseEntity::RecvProxy_CellOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	Vector *vecNetworkOrigin = (Vector *)pOut;

	Assert( vecNetworkOrigin == &pEnt->m_vecNetworkOrigin );

	pEnt->m_vecCellOrigin.x = pData->m_Value.m_Vector[0];
	pEnt->m_vecCellOrigin.y = pData->m_Value.m_Vector[1];

	register int const cellwidth = pEnt->m_cellwidth; // Load it into a register

	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		vecNetworkOrigin->x = CoordFromCell( cellwidth, pEnt->m_cellX, pData->m_Value.m_Vector[0] );
		vecNetworkOrigin->y = CoordFromCell( cellwidth, pEnt->m_cellY, pData->m_Value.m_Vector[1] );
	}
}

void C_BaseEntity::RecvProxy_CellOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	float *vecNetworkOriginZ = (float *)pOut;

	Assert( vecNetworkOriginZ == &pEnt->m_vecNetworkOrigin[2] );

	pEnt->m_vecCellOrigin.z = pData->m_Value.m_Float;

	register int const cellwidth = pEnt->m_cellwidth; // Load it into a register

	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		*vecNetworkOriginZ = CoordFromCell( cellwidth, pEnt->m_cellZ, pData->m_Value.m_Float );
	}
}

void RecvProxy_LocalVelocity( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	Vector vecVelocity;
	
	vecVelocity.x = pData->m_Value.m_Vector[0];
	vecVelocity.y = pData->m_Value.m_Vector[1];
	vecVelocity.z = pData->m_Value.m_Vector[2];

	// SetLocalVelocity checks to see if the value has changed
	pEnt->SetLocalVelocity( vecVelocity );
}
void RecvProxy_ToolRecording( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	if ( !ToolsEnabled() )
		return;

	CBaseEntity *pEnt = (CBaseEntity *)pStruct;
	pEnt->SetToolRecording( pData->m_Value.m_Int != 0 );
}

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS(C_BaseEntity, DT_BaseEntity, CBaseEntity);

static void RecvProxy_MoveType( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((C_BaseEntity*)pStruct)->SetMoveType( (MoveType_t)(pData->m_Value.m_Int) );
}

static void RecvProxy_MoveCollide( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((C_BaseEntity*)pStruct)->SetMoveCollide( (MoveCollide_t)(pData->m_Value.m_Int) );
}

static void RecvProxy_Solid( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((C_BaseEntity*)pStruct)->SetSolid( (SolidType_t)pData->m_Value.m_Int );
}

static void RecvProxy_SolidFlags( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((C_BaseEntity*)pStruct)->SetSolidFlags( pData->m_Value.m_Int );
}

void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((C_BaseEntity*)pStruct)->SetEffects( pData->m_Value.m_Int );
}

void RecvProxy_ClrRender( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// This proxy will cause the alpha modulation to get updated correctly
	C_BaseEntity *pEnt = (C_BaseEntity*)pStruct;
	uint32 color = LittleDWord((uint32)pData->m_Value.m_Int);
	color32 c = *(color32*)( &color );
	pEnt->SetRenderColor( c.r, c.g, c.b );
	pEnt->SetRenderAlpha( c.a );
}

void C_BaseEntity::RecvProxyOldSpottedByMask( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Well this is fun.  pStruct points to the start of the array, so we have to work back to get base entity
	C_BaseEntity *pEnt = (C_BaseEntity*) ( (byte*)pStruct - offsetof( C_BaseEntity, m_bSpottedBy ) );
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );
	int nPlayerIndex = (bool*)pOut - pEnt->m_bSpottedBy;
	pEnt->SetIsSpottedBy( nPlayerIndex );
}

BEGIN_RECV_TABLE_NOBASE( C_BaseEntity, DT_AnimTimeMustBeFirst )
	RecvPropInt( RECVINFO(m_flAnimTime), 0, RecvProxy_AnimTime ),
END_RECV_TABLE()

BEGIN_ENT_SCRIPTDESC_ROOT( C_BaseEntity, "Root class of all client-side entities" )
	DEFINE_SCRIPTFUNC_NAMED( GetAbsOrigin, "GetOrigin", ""  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetForward, "GetForwardVector", "Get the forward vector of the entity"  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetLeft, "GetLeftVector", "Get the left vector of the entity"  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetUp, "GetUpVector", "Get the up vector of the entity"  )
	DEFINE_SCRIPTFUNC( GetTeamNumber, "Gets this entity's team" )
END_SCRIPTDESC();


#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
BEGIN_RECV_TABLE_NOBASE( C_BaseEntity, DT_PredictableId )
	RecvPropPredictableId( RECVINFO( m_PredictableID ) ),
	RecvPropInt( RECVINFO( m_bIsPlayerSimulated ) ),
END_RECV_TABLE()
#endif

BEGIN_RECV_TABLE_NOBASE(C_BaseEntity, DT_BaseEntity)
	RecvPropDataTable( "AnimTimeMustBeFirst", 0, 0, &REFERENCE_RECV_TABLE(DT_AnimTimeMustBeFirst) ),
	RecvPropInt( RECVINFO(m_flSimulationTime), 0, RecvProxy_SimulationTime ),
#if defined(ENABLE_CREATE_TIME)
	RecvPropFloat( RECVINFO( m_flCreateTime ) ),
#endif
	RecvPropInt( RECVINFO( m_cellbits ), 0, C_BaseEntity::RecvProxy_CellBits ),
//	RecvPropArray( RecvPropInt( RECVINFO(m_cellXY[0]) ), m_cellXY ),
	RecvPropInt( RECVINFO( m_cellX ), 0, C_BaseEntity::RecvProxy_CellX ),
	RecvPropInt( RECVINFO( m_cellY ), 0, C_BaseEntity::RecvProxy_CellY ),
	RecvPropInt( RECVINFO( m_cellZ ), 0, C_BaseEntity::RecvProxy_CellZ ),
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ), 0, C_BaseEntity::RecvProxy_CellOrigin ),
#if PREDICTION_ERROR_CHECK_LEVEL > 1 
	RecvPropVector( RECVINFO_NAME( m_angNetworkAngles, m_angRotation ) ),
#else
	RecvPropQAngles( RECVINFO_NAME( m_angNetworkAngles, m_angRotation ) ),
#endif
	RecvPropInt(RECVINFO(m_nModelIndex) ),

	RecvPropInt(RECVINFO(m_fEffects), 0, RecvProxy_EffectFlags ),
	RecvPropInt(RECVINFO(m_nRenderMode)),
	RecvPropInt(RECVINFO(m_nRenderFX)),
	RecvPropInt(RECVINFO(m_clrRender), 0, RecvProxy_ClrRender ),
	RecvPropInt(RECVINFO(m_iTeamNum)),
	RecvPropInt(RECVINFO(m_iPendingTeamNum)),
	RecvPropInt(RECVINFO(m_CollisionGroup)),
	RecvPropFloat(RECVINFO(m_flElasticity)),
	RecvPropFloat(RECVINFO(m_flShadowCastDistance)),
	RecvPropEHandle( RECVINFO(m_hOwnerEntity) ),
	RecvPropEHandle( RECVINFO(m_hEffectEntity) ),
	RecvPropInt( RECVINFO_NAME(m_hNetworkMoveParent, moveparent), 0, RecvProxy_IntToMoveParent ),
	RecvPropInt( RECVINFO( m_iParentAttachment ) ),

	RecvPropString( RECVINFO( m_iName ) ),

#if defined ( PORTAL2 )
	RecvPropString( RECVINFO( m_iSignifierName ) ),
#endif

	RecvPropInt( "movetype", 0, SIZEOF_IGNORE, 0, RecvProxy_MoveType ),
	RecvPropInt( "movecollide", 0, SIZEOF_IGNORE, 0, RecvProxy_MoveCollide ),
	RecvPropDataTable( RECVINFO_DT( m_Collision ), 0, &REFERENCE_RECV_TABLE(DT_CollisionProperty) ),
	
	RecvPropInt( RECVINFO ( m_iTextureFrameIndex ) ),
	
#if defined ( PORTAL2 )
	RecvPropInt		( RECVINFO( m_iObjectCapsCache ) ),
#endif
	
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	RecvPropEHandle (RECVINFO(m_hPlayerSimulationOwner)),
	RecvPropDataTable( "predictable_id", 0, 0, &REFERENCE_RECV_TABLE( DT_PredictableId ) ),
#endif

	RecvPropInt		( RECVINFO( m_bSimulatedEveryTick ), 0, RecvProxy_InterpolationAmountChanged ),
	RecvPropInt		( RECVINFO( m_bAnimatedEveryTick ), 0, RecvProxy_InterpolationAmountChanged ),
	RecvPropBool	( RECVINFO( m_bAlternateSorting ) ),
	RecvPropBool	( RECVINFO( m_bSpotted ) ),
	RecvPropArray3	( RECVINFO_ARRAY( m_bSpottedBy ), RecvPropInt( RECVINFO( m_bSpottedBy[0] ), SPROP_UNSIGNED, C_BaseEntity::RecvProxyOldSpottedByMask ) ), // OLD SPOTTED BY FOR DEMOS
	RecvPropArray3  ( RECVINFO_ARRAY( m_bSpottedByMask ), RecvPropInt( RECVINFO( m_bSpottedByMask[0] ), SPROP_UNSIGNED ) ),

	RecvPropBool	( RECVINFO( m_bIsAutoaimTarget ) ),

	RecvPropFloat( RECVINFO( m_fadeMinDist ) ), 
	RecvPropFloat( RECVINFO( m_fadeMaxDist ) ), 
	RecvPropFloat( RECVINFO( m_flFadeScale ) ), 

#if 1
// #ifndef _GAMECONSOLE -- X360 client and Win32 XLSP dedicated server need equivalent SendTables
	RecvPropInt( RECVINFO( m_nMinCPULevel ) ), 
	RecvPropInt( RECVINFO( m_nMaxCPULevel ) ), 
	RecvPropInt( RECVINFO( m_nMinGPULevel ) ), 
	RecvPropInt( RECVINFO( m_nMaxGPULevel ) ), 
#endif

	RecvPropFloat( RECVINFO( m_flUseLookAtAngle ) ),

	RecvPropFloat( RECVINFO( m_flLastMadeNoiseTime ) ),

END_RECV_TABLE()

const float coordTolerance = 2.0f / (float)( 1 << COORD_FRACTIONAL_BITS );

BEGIN_PREDICTION_DATA_NO_BASE( C_BaseEntity )

	// These have a special proxy to handle send/receive
	DEFINE_PRED_TYPEDESCRIPTION( m_Collision, CCollisionProperty ),

	DEFINE_PRED_FIELD( m_MoveType, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_MoveCollide, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),

	DEFINE_FIELD( m_vecAbsVelocity, FIELD_VECTOR ),
	DEFINE_PRED_FIELD_TOL( m_vecVelocity, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.5f ),
	DEFINE_PRED_FIELD( m_fEffects, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nRenderMode, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nRenderFX, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_flAnimTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_flSimulationTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fFlags, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_vecViewOffset, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.25f ),
	DEFINE_PRED_FIELD( m_nModelIndex, FIELD_SHORT, FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX ),
	DEFINE_PRED_FIELD( m_flFriction, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iTeamNum, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iPendingTeamNum, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#ifndef INFESTED_DLL // alien swarm is temporarily unpredicting health to see if prediction is cause of a bug
	DEFINE_FIELD( m_iHealth, FIELD_INTEGER ),
#endif
	DEFINE_PRED_FIELD( m_hOwnerEntity, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

//	DEFINE_FIELD( m_nSimulationTick, FIELD_INTEGER ),

	DEFINE_PRED_FIELD( m_hNetworkMoveParent, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_pMoveParent, FIELD_EHANDLE ),
//	DEFINE_PRED_FIELD( m_pMoveChild, FIELD_EHANDLE ),
//	DEFINE_PRED_FIELD( m_pMovePeer, FIELD_EHANDLE ),
//	DEFINE_PRED_FIELD( m_pMovePrevPeer, FIELD_EHANDLE ),

	DEFINE_PRED_FIELD_TOL( m_vecNetworkOrigin, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, coordTolerance ),
	DEFINE_PRED_FIELD( m_angNetworkAngles, FIELD_VECTOR, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_FIELD( m_vecAbsOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_angAbsRotation, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_angRotation, FIELD_VECTOR ),

	DEFINE_FIELD( m_hGroundEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( m_nWaterLevel, FIELD_CHARACTER ),
	DEFINE_FIELD( m_nWaterType, FIELD_CHARACTER ),
	DEFINE_FIELD( m_vecAngVelocity, FIELD_VECTOR ),
//	DEFINE_FIELD( m_vecAbsAngVelocity, FIELD_VECTOR ),

//	DEFINE_FIELD( m_nMinCPULevel, FIELD_CHARACTER ),
//	DEFINE_FIELD( m_nMaxCPULevel, FIELD_CHARACTER ),
//	DEFINE_FIELD( m_nMinGPULevel, FIELD_CHARACTER ),
//	DEFINE_FIELD( m_nMaxGPULevel, FIELD_CHARACTER ),

//	DEFINE_FIELD( model, FIELD_INTEGER ), // writing pointer literally
//	DEFINE_FIELD( index, FIELD_INTEGER ),
//	DEFINE_FIELD( m_ClientHandle, FIELD_SHORT ),
//	DEFINE_FIELD( m_Partition, FIELD_SHORT ),
//	DEFINE_FIELD( m_hRender, FIELD_SHORT ),
//	DEFINE_FIELD( m_bDormant, FIELD_BOOLEAN ),
//	DEFINE_FIELD( current_position, FIELD_INTEGER ),
//	DEFINE_FIELD( m_flLastMessageTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecBaseVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_iEFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_flGravity, FIELD_FLOAT ),
//	DEFINE_FIELD( m_ModelInstance, FIELD_SHORT ),
	DEFINE_FIELD( m_flProxyRandomValue, FIELD_FLOAT ),

	DEFINE_FIELD( m_bEverHadPredictionErrorsForThisCommand, FIELD_BOOLEAN ),

#if defined( USE_PREDICTABLEID )
	DEFINE_PRED_FIELD( m_hPlayerSimulationOwner, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),	
//	DEFINE_FIELD( m_PredictableID, FIELD_INTEGER ),
//	DEFINE_FIELD( m_pPredictionContext, FIELD_POINTER ),
#endif
	// Stuff specific to rendering and therefore not to be copied back and forth
	// DEFINE_PRED_FIELD( m_clrRender, color32, FTYPEDESC_INSENDTABLE  ),
	// DEFINE_FIELD( m_bReadyToDraw, FIELD_BOOLEAN ),
	// DEFINE_FIELD( anim, CLatchedAnim ),
	// DEFINE_FIELD( mouth, CMouthInfo ),
	// DEFINE_FIELD( GetAbsOrigin(), FIELD_VECTOR ),
	// DEFINE_FIELD( GetAbsAngles(), FIELD_VECTOR ),
	// DEFINE_FIELD( m_nNumAttachments, FIELD_SHORT ),
	// DEFINE_FIELD( m_pAttachmentAngles, FIELD_VECTOR ),
	// DEFINE_FIELD( m_pAttachmentOrigin, FIELD_VECTOR ),
	// DEFINE_FIELD( m_listentry, CSerialEntity ),
	// DEFINE_FIELD( m_ShadowHandle, ClientShadowHandle_t ),
	// DEFINE_FIELD( m_hThink, ClientThinkHandle_t ),
	// Definitely private and not copied around
	// DEFINE_FIELD( m_bPredictable, FIELD_BOOLEAN ),
	// DEFINE_FIELD( m_CollisionGroup, FIELD_INTEGER ),
	// DEFINE_FIELD( m_DataChangeEventRef, FIELD_INTEGER ),
#if !defined( CLIENT_DLL )
	// DEFINE_FIELD( m_bPredictionEligible, FIELD_BOOLEAN ),
#endif
	DEFINE_FIELD( m_flUseLookAtAngle, FIELD_FLOAT ),
END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------

void SpewInterpolatedVar( CInterpolatedVar< Vector > *pVar )
{
	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	float prevtime = 0.0f;
	while ( 1 )
	{
		float changetime, vel;
		Vector *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		vel = apparent.AddSample( changetime, *pVal );
		Msg( "%6.6f: (%.2f %.2f %.2f), vel: %.2f [dt %.1f]\n", changetime, VectorExpand( *pVal ), vel, prevtime == 0.0f ? 0.0f : 1000.0f * ( changetime - prevtime ) );
		i = pVar->GetNext( i );
		prevtime = changetime;
	}
	Msg( "--------------------------------------------------\n" );
}

void SpewInterpolatedVar( CInterpolatedVar< Vector > *pVar, float flNow, float flInterpAmount, bool bSpewAllEntries = true )
{
	float target = flNow - flInterpAmount;

	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	float newtime = 999999.0f;
	Vector newVec( 0, 0, 0 );
	bool bSpew = true;

	while ( 1 )
	{
		float changetime, vel;
		Vector *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		if ( bSpew && target >= changetime )
		{
			Vector o;
			pVar->DebugInterpolate( &o, flNow );
			bool bInterp = newtime != 999999.0f;
			float frac = 0.0f;
			char desc[ 32 ];

			if ( bInterp )
			{
				frac = ( target - changetime ) / ( newtime - changetime );
				Q_snprintf( desc, sizeof( desc ), "interpolated [%.2f]", frac );
			}
			else
			{
				bSpew = true;
				int savei = i;
				i = pVar->GetNext( i );
				float oldtertime = 0.0f;
				pVar->GetHistoryValue( i, oldtertime );

				if ( changetime != oldtertime )
				{
					frac = ( target - changetime ) / ( changetime - oldtertime );
				}

				Q_snprintf( desc, sizeof( desc ), "extrapolated [%.2f]", frac );
				i = savei;
			}

			if ( bSpew )
			{
				Msg( "  > %6.6f: (%.2f %.2f %.2f) %s for %.1f msec\n", 
					target, 
					VectorExpand( o ), 
					desc,
					1000.0f * ( target - changetime ) );
				bSpew = false;
			}
		}

		vel = apparent.AddSample( changetime, *pVal );
		if ( bSpewAllEntries )
		{
			Msg( "    %6.6f: (%.2f %.2f %.2f), vel: %.2f [dt %.1f]\n", changetime, VectorExpand( *pVal ), vel, newtime == 999999.0f ? 0.0f : 1000.0f * ( newtime - changetime ) );
		}
		i = pVar->GetNext( i );
		newtime = changetime;
		newVec = *pVal;
	}
	Msg( "--------------------------------------------------\n" );
}
void SpewInterpolatedVar( CInterpolatedVar< float > *pVar )
{
	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<float> apparent;
	while ( 1 )
	{
		float changetime, vel;
		float *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		vel = apparent.AddSample( changetime, *pVal );
		Msg( "%6.6f: (%.2f), vel: %.2f\n", changetime, *pVal, vel );
		i = pVar->GetNext( i );
	}
	Msg( "--------------------------------------------------\n" );
}

template<class T>
void GetInterpolatedVarTimeRange( CInterpolatedVar<T> *pVar, float &flMin, float &flMax )
{
	flMin = 1e23;
	flMax = -1e23;

	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	while ( 1 )
	{
		float changetime;
		if ( !pVar->GetHistoryValue( i, changetime ) )
			return;

		flMin = MIN( flMin, changetime );
		flMax = MAX( flMax, changetime );
		i = pVar->GetNext( i );
	}
}


//-----------------------------------------------------------------------------
// Global methods related to when abs data is correct
//-----------------------------------------------------------------------------
void C_BaseEntity::SetAbsQueriesValid( bool bValid )
{
	// @MULTICORE: Always allow in worker threads, assume higher level code is handling correctly
	if ( !ThreadInMainThread() )
		return;

	if ( !bValid )
	{
		s_bAbsQueriesValid = false;
	}
	else
	{
		s_bAbsQueriesValid = true;
	}
}

bool C_BaseEntity::IsAbsQueriesValid( void )
{
	if ( !ThreadInMainThread() )
		return true;
	return s_bAbsQueriesValid;
}

void C_BaseEntity::PushEnableAbsRecomputations( bool bEnable )
{
	if ( !ThreadInMainThread() )
		return;
	if ( g_iAbsRecomputationStackPos < ARRAYSIZE( g_bAbsRecomputationStack ) )
	{
		g_bAbsRecomputationStack[g_iAbsRecomputationStackPos] = s_bAbsRecomputationEnabled;
		++g_iAbsRecomputationStackPos;
		s_bAbsRecomputationEnabled = bEnable;
	}
	else
	{
		Assert( false );
	}
}

void C_BaseEntity::PopEnableAbsRecomputations()
{
	if ( !ThreadInMainThread() )
		return;
	if ( g_iAbsRecomputationStackPos > 0 )
	{
		--g_iAbsRecomputationStackPos;
		s_bAbsRecomputationEnabled = g_bAbsRecomputationStack[g_iAbsRecomputationStackPos];
	}
	else
	{
		Assert( false );
	}
}

void C_BaseEntity::EnableAbsRecomputations( bool bEnable )
{
	if ( !ThreadInMainThread() )
		return;
	// This should only be called at the frame level. Use PushEnableAbsRecomputations
	// if you're blocking out a section of code.
	Assert( g_iAbsRecomputationStackPos == 0 );

	s_bAbsRecomputationEnabled = bEnable;
}

bool C_BaseEntity::IsAbsRecomputationsEnabled()
{
	if ( !ThreadInMainThread() )
		return true;
	return s_bAbsRecomputationEnabled;
}

int	C_BaseEntity::GetTextureFrameIndex( void )
{
	return m_iTextureFrameIndex;
}

void C_BaseEntity::SetTextureFrameIndex( int iIndex )
{
	m_iTextureFrameIndex = iIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *map - 
//-----------------------------------------------------------------------------
void C_BaseEntity::Interp_SetupMappings( VarMapping_t *map )
{
	if( !map )
		return;

	int c = map->m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;
		void *data = e->data;
		int type = e->type;

		watcher->Setup( data, type );
		watcher->SetInterpolationAmount( GetInterpolationAmount( watcher->GetType() ) ); 
	}
}

void C_BaseEntity::Interp_RestoreToLastNetworked( VarMapping_t *map, int flags )
{
	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, "restoretolastnetworked" );

	Vector oldOrigin = GetLocalOrigin();
	QAngle oldAngles = GetLocalAngles();

	int c = map->m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();
		if ( flags && !(type & flags) )
			continue;

		watcher->RestoreToLastNetworked();
	}

	BaseInterpolatePart2( oldOrigin, oldAngles, 0 );
}

void C_BaseEntity::Interp_UpdateInterpolationAmounts( VarMapping_t *map )
{
	if( !map )
		return;

	int c = map->m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;
		watcher->SetInterpolationAmount( GetInterpolationAmount( watcher->GetType() ) ); 
	}
}

void C_BaseEntity::Interp_HierarchyUpdateInterpolationAmounts()
{
	Interp_UpdateInterpolationAmounts( GetVarMapping() );

	for ( C_BaseEntity *pChild = FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
	{
		pChild->Interp_HierarchyUpdateInterpolationAmounts();
	}
}

inline int C_BaseEntity::Interp_Interpolate( VarMapping_t *map, float currentTime )
{
	int bNoMoreChanges = 1;
	if ( currentTime < map->m_lastInterpolationTime )
	{
		for ( int i = 0; i < map->m_nInterpolatedEntries; i++ )
		{
			VarMapEntry_t *e = &map->m_Entries[ i ];

			e->m_bNeedsToInterpolate = true;
		}
	}
	map->m_lastInterpolationTime = currentTime;

	for ( int i = 0; i < map->m_nInterpolatedEntries; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];

		if ( !e->m_bNeedsToInterpolate )
			continue;
			
		IInterpolatedVar *watcher = e->watcher;
		Assert( !( watcher->GetType() & EXCLUDE_AUTO_INTERPOLATE ) );


		if ( watcher->Interpolate( currentTime ) )
			e->m_bNeedsToInterpolate = false;
		else
			bNoMoreChanges = 0;
	}

	return bNoMoreChanges;
}

//-----------------------------------------------------------------------------
// Functions.
//-----------------------------------------------------------------------------
C_BaseEntity::C_BaseEntity() : 
	m_iv_vecOrigin( "C_BaseEntity::m_iv_vecOrigin" ),
	m_iv_angRotation( "C_BaseEntity::m_iv_angRotation" )
{
	AddVar( &m_vecOrigin, &m_iv_vecOrigin, LATCH_SIMULATION_VAR );
	AddVar( &m_angRotation, &m_iv_angRotation, LATCH_SIMULATION_VAR );

	// made noise 'a long time ago'
	m_flLastMadeNoiseTime = -99999.0f;

	m_iTeamNum = TEAM_UNASSIGNED;
	m_nMinCPULevel = m_nMaxCPULevel = 0;
	m_nMinGPULevel = m_nMaxGPULevel = 0;

	m_flFadeScale = 0.0f;
	m_fadeMinDist = m_fadeMaxDist = 0.0f;
	m_pClientAlphaProperty = NULL;
	m_nSplitUserPlayerPredictionSlot = 0;
	m_DataChangeEventRef = -1;
	m_EntClientFlags = 0;
	m_bRenderWithViewModels = false;
	m_bDisableCachedRenderBounds = false;
	m_iParentAttachment = 0;
	m_bIsValidIKAttachment = false;

	SetPredictionEligible( false );
	m_bPredictable = false;

	m_bSimulatedEveryTick = false;
	m_bAnimatedEveryTick = false;
	m_pPhysicsObject = NULL;
	m_bDisableSimulationFix = false;
	m_bSpotted = false;
	for ( int i = 0; i < kNumSpottedByMask; i++ )
		m_bSpottedByMask.Set( i, 0 );

#ifdef _DEBUG
	m_vecAbsOrigin = vec3_origin;
	m_angAbsRotation = vec3_angle;
	m_vecNetworkOrigin.Init();
	m_angNetworkAngles.Init();
	m_vecAbsOrigin.Init();
//	m_vecAbsAngVelocity.Init();
	m_vecVelocity.Init();
	m_vecAbsVelocity.Init();
	m_vecViewOffset.Init();
	m_vecBaseVelocity.Init();

	m_iCurrentThinkContext = NO_THINK_CONTEXT;

#endif
	SetIdentityMatrix( m_rgflCoordinateFrame );
	m_nSimulationTick = -1;

	// Assume drawing everything
	m_bReadyToDraw = true;
	m_bClientSideRagdoll = false;
	m_flProxyRandomValue = 0.0f;

	m_fBBoxVisFlags = 0;
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	m_pPredictionContext = NULL;
#endif
	
	for ( int i = 0; i < NUM_ENTITY_LISTS; i++ )
	{
		m_ListEntry[i] = 0xFFFF;
	}
	AddToEntityList( ENTITY_LIST_PRERENDER );


	Clear();

#ifndef NO_TOOLFRAMEWORK
	m_bEnabledInToolView = true;
	m_bToolRecording = false;
	m_ToolHandle = 0;
	m_nLastRecordedFrame = -1;
	m_bRecordInTools = true;
#endif

	ParticleProp()->Init( this );

	m_spawnflags = 0;
#if defined(ENABLE_CREATE_TIME)
	m_flCreateTime = 0.0f;
#endif

	m_flUseLookAtAngle = DEFAULT_LOOK_AT_USE_ANGLE;
}


//-----------------------------------------------------------------------------
// Deallocates the alpha property
//-----------------------------------------------------------------------------
void C_BaseEntity::CleanUpAlphaProperty()
{
	if ( m_pClientAlphaProperty )
	{
		g_pClientAlphaPropertyMgr->DestroyClientAlphaProperty( m_pClientAlphaProperty );
		m_pClientAlphaProperty = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
C_BaseEntity::~C_BaseEntity()
{
	Term();
	CleanUpAlphaProperty();
	ClearDataChangedEvent( m_DataChangeEventRef );
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	delete m_pPredictionContext;
#endif
	for ( int i = 0; i < NUM_ENTITY_LISTS; i++ )
	{
		RemoveFromEntityList(entity_list_ids_t(i));
	}
}

void C_BaseEntity::Clear( void )
{
	m_bDormant = true;
	m_bCanUseBrushModelFastPath = false;
	m_nCreationTick = -1;
	m_RefEHandle.Term();
	m_ModelInstance = MODEL_INSTANCE_INVALID;
	m_ShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_hRender = INVALID_CLIENT_RENDER_HANDLE;
	m_hThink = INVALID_THINK_HANDLE;
	m_AimEntsListHandle = INVALID_AIMENTS_LIST_HANDLE;

	index = -1;
	m_Collision.Init( this );
	CleanUpAlphaProperty();
	m_pClientAlphaProperty = static_cast< CClientAlphaProperty * >( g_pClientAlphaPropertyMgr->CreateClientAlphaProperty( this ) );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
	model = NULL;
	m_vecAbsOrigin.Init();
	m_angAbsRotation.Init();
	m_vecVelocity.Init();
	ClearFlags();
	m_vecViewOffset.Init();
	m_vecBaseVelocity.Init();
	m_nModelIndex = 0;
	m_flAnimTime = 0;
	m_flSimulationTime = 0;
	SetSolid( SOLID_NONE );
	SetSolidFlags( 0 );
	SetMoveCollide( MOVECOLLIDE_DEFAULT );
	SetMoveType( MOVETYPE_NONE );

	ClearEffects();
	m_iEFlags = 0;
	m_nRenderMode = 0;
	m_nOldRenderMode = 0;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	SetRenderFX( kRenderFxNone );
	m_flFriction = 0.0f;       
	m_flGravity = 0.0f;
	SetCheckUntouch( false );
	m_ShadowDirUseOtherEntity = NULL;

	m_nLastThinkTick = gpGlobals->tickcount;

	// Remove prediction context if it exists
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	delete m_pPredictionContext;
	m_pPredictionContext = NULL;
#endif
	// Do not enable this on all entities. It forces bone setup for entities that
	// don't need it.
	//AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// IClientUnknown 
//-----------------------------------------------------------------------------
IClientAlphaProperty* C_BaseEntity::GetClientAlphaProperty()
{
	return m_pClientAlphaProperty;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::Activate()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::SpawnClientEntity( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Attach to entity
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::Init( int entnum, int iSerialNum )
{
	Assert( entnum >= 0 && entnum < NUM_ENT_ENTRIES );

	index = entnum;
	m_pClientAlphaProperty->SetDesyncOffset( index );

	cl_entitylist->AddNetworkableEntity( GetIClientUnknown(), entnum, iSerialNum );

	CollisionProp()->CreatePartitionHandle();

	InitSharedVars();

	Interp_SetupMappings( GetVarMapping() );

	m_nCreationTick = gpGlobals->tickcount;

	m_hScriptInstance = NULL;
	
	return true;
}
					  
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseEntity::InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels )
{
	int nModelIndex;

	if ( pszModelName != NULL )
	{
		nModelIndex = modelinfo->GetModelIndex( pszModelName );
		
		if ( nModelIndex == -1 )
		{
			// Model could not be found
			Assert( !"Model could not be found, index is -1" );
			return false;
		}
	}
	else
	{
		nModelIndex = -1;
	}

	Interp_SetupMappings( GetVarMapping() );

	return InitializeAsClientEntityByIndex( nModelIndex, bRenderWithViewModels );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseEntity::InitializeAsClientEntityByIndex( int iIndex, bool bRenderWithViewModels )
{
	// Setup model data.
	RenderWithViewModels( bRenderWithViewModels );

	// NOTE: This will add the client entity to the renderable "leaf system" (Renderable)
	SetModelByIndex( iIndex );

	// Add the client entity to the master entity list.
	cl_entitylist->AddNonNetworkableEntity( GetIClientUnknown() );
	Assert( GetClientHandle() != ClientEntityList().InvalidHandle() );

	// Add the client entity to the spatial partition. (Collidable)
	CollisionProp()->CreatePartitionHandle();

	index = -1;
	m_pClientAlphaProperty->SetDesyncOffset( rand() % 1024 );

	SpawnClientEntity();

	return true;
}


void C_BaseEntity::Term()
{
	C_BaseEntity::PhysicsRemoveTouchedList( this );
	C_BaseEntity::PhysicsRemoveGroundList( this );
	DestroyAllDataObjects();

#if !defined( NO_ENTITY_PREDICTION )
	// Remove from the predictables list
	if ( GetPredictable() || IsClientCreated() )
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			GetPredictables( i )->RemoveFromPredictablesList( this );
		}
	}

	// If it's play simulated, remove from simulation list if the player still exists...
	if ( IsPlayerSimulated() )
	{
		UnsetPlayerSimulated();
	}
#endif

	if ( GetClientHandle() != INVALID_CLIENTENTITY_HANDLE )
	{
		if ( GetThinkHandle() != INVALID_THINK_HANDLE )
		{
			ClientThinkList()->RemoveThinkable( GetClientHandle() );
		}

		// Remove from the client entity list.
		ClientEntityList().RemoveEntity( GetClientHandle() );

		m_RefEHandle = INVALID_CLIENTENTITY_HANDLE;
	}
	
	// Are we in the partition?
	CollisionProp()->DestroyPartitionHandle();

	// If Client side only entity index will be -1
	if ( index != -1 )
	{
		beams->KillDeadBeams( this );
	}

	// Clean up the model instance
	DestroyModelInstance();

	// Clean up drawing
	RemoveFromLeafSystem();

	RemoveFromAimEntsList();

	if ( m_hScriptInstance )
	{
		g_pScriptVM->RemoveInstance( m_hScriptInstance );
		m_hScriptInstance = NULL;
	}
}


void C_BaseEntity::SetRefEHandle( const CBaseHandle &handle )
{
	m_RefEHandle = handle;
}


const CBaseHandle& C_BaseEntity::GetRefEHandle() const
{
	return m_RefEHandle;
}

//-----------------------------------------------------------------------------
// Purpose: Free beams and destroy object
//-----------------------------------------------------------------------------
void C_BaseEntity::Release()
{
	{
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
		UnlinkFromHierarchy();
	}

	// Note that this must be called from here, not the destructor, because otherwise the
	//  vtable is hosed and the derived classes function is not going to get called!!!
	if ( IsIntermediateDataAllocated() )
	{
		DestroyIntermediateData();
	}

	UpdateOnRemove();

	delete this;
}


//-----------------------------------------------------------------------------
// Only meant to be called from subclasses.
// Returns true if instance valid, false otherwise
//-----------------------------------------------------------------------------
void C_BaseEntity::CreateModelInstance()
{
	if ( m_ModelInstance == MODEL_INSTANCE_INVALID )
	{
		m_ModelInstance = modelrender->CreateInstance( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::DestroyModelInstance()
{
	if (m_ModelInstance != MODEL_INSTANCE_INVALID)
	{
		modelrender->DestroyInstance( m_ModelInstance );
		m_ModelInstance = MODEL_INSTANCE_INVALID;
	}
}

void C_BaseEntity::SetRemovalFlag( bool bRemove ) 
{ 
	if (bRemove) 
		m_iEFlags |= EFL_KILLME; 
	else 
		m_iEFlags &= ~EFL_KILLME; 
}


//-----------------------------------------------------------------------------
// Alpha
//-----------------------------------------------------------------------------
void C_BaseEntity::SetRenderAlpha( byte a )
{
	if ( m_clrRender.GetA() != a )
	{
		m_clrRender.SetA( a );
		m_pClientAlphaProperty->SetAlphaModulation( a );
	}
}

byte C_BaseEntity::GetRenderAlpha() const
{
	return m_pClientAlphaProperty->GetAlphaModulation( );
}


//-----------------------------------------------------------------------------
// Methods related to fade scale
//-----------------------------------------------------------------------------
float C_BaseEntity::GetMinFadeDist( ) const
{
	return m_fadeMinDist;
}

float C_BaseEntity::GetMaxFadeDist( ) const
{
	return m_fadeMaxDist;
}

void C_BaseEntity::SetDistanceFade( float flMinDist, float flMaxDist )
{
	m_fadeMinDist = flMinDist;
	m_fadeMaxDist = flMaxDist;

	// NOTE: Setting the fade will not necessarily produce the same values
	// as what was passed in. I'm deliberately choosing not to grab them back out
	// because I'm not sure what client logic depends on them being negative, for example

	// Specifically, I'm certain the loading logic in C_PhysPropClientside,
	// as well as code inside of C_PhysPropClientside::Initialize
	// will definitely not work unless I'm doing it the way I'm currently doing it.
	AlphaProp()->SetFade( m_flFadeScale, m_fadeMinDist, m_fadeMaxDist );
}

void C_BaseEntity::SetGlobalFadeScale( float flFadeScale )
{
	m_flFadeScale = flFadeScale;
	int modelType = modelinfo->GetModelType( model );
	if ( modelType == mod_studio )
	{
		MDLCACHE_CRITICAL_SECTION();
		MDLHandle_t hStudioHdr = modelinfo->GetCacheHandle( model );
		if ( hStudioHdr != MDLHANDLE_INVALID )
		{
			const studiohdr_t *pStudioHdr = mdlcache->LockStudioHdr( hStudioHdr );
			if ( pStudioHdr->flags & STUDIOHDR_FLAGS_NO_FORCED_FADE )
			{
				flFadeScale = 0.0f;
			}
			mdlcache->UnlockStudioHdr( hStudioHdr );
		}
	}
	AlphaProp()->SetFade( flFadeScale, m_fadeMinDist, m_fadeMaxDist );
}

float C_BaseEntity::GetGlobalFadeScale( ) const
{
	return m_flFadeScale;
}


//-----------------------------------------------------------------------------
// VPhysics objects..
//-----------------------------------------------------------------------------
int C_BaseEntity::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		// multi-object entities must implement this function
		Assert( !(pPhys->GetGameFlags() & FVPHYSICS_MULTIOBJECT_ENTITY) );
		if ( listMax > 0 )
		{
			pList[0] = pPhys;
			return 1;
		}
	}
	return 0;
}

bool C_BaseEntity::VPhysicsIsFlesh( void )
{
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
	for ( int i = 0; i < count; i++ )
	{
		int material = pList[i]->GetMaterialIndex();
		const surfacedata_t *pSurfaceData = physprops->GetSurfaceData( material );
		// Is flesh ?, don't allow pickup
		if ( pSurfaceData->game.material == CHAR_TEX_ANTLION || pSurfaceData->game.material == CHAR_TEX_FLESH || pSurfaceData->game.material == CHAR_TEX_BLOODYFLESH || pSurfaceData->game.material == CHAR_TEX_ALIENFLESH )
			return true;
	}
	return false;
}

void C_BaseEntity::VPhysicsCompensateForPredictionErrors( const byte *predicted_state_data )
{
	Assert( GetPredictable() );

#if defined( DBGFLAG_ASSERT )
	const byte *networked_state_data = (const byte *)GetOriginalNetworkDataObject();
#endif

#if 0
	{
		int iSavedCommand;
		int iNetworkedCommand;
		{
			const typedescription_t *tdSavedCommand = CPredictionCopy::FindFlatFieldByName( "m_SavedCommandNum", GetPredDescMap() );
			Assert( tdSavedCommand );
			Q_memcpy( &iSavedCommand, predicted_state_data + tdSavedCommand->flatOffset[ TD_OFFSET_PACKED ], sizeof( int ) );
			Q_memcpy( &iNetworkedCommand, networked_state_data + tdSavedCommand->flatOffset[ TD_OFFSET_PACKED ], sizeof( int ) );
		}
		Assert( iNetworkedCommand == iSavedCommand );
	}
#endif

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	IPredictedPhysicsObject *pPredictedObject = pPhysicsObject ? pPhysicsObject->GetPredictedInterface() : NULL;
	if( pPredictedObject )
	{
		Vector vPredictedOrigin;
		
		{
			const typedescription_t *tdOrigin = CPredictionCopy::FindFlatFieldByName( "m_vecNetworkOrigin", GetPredDescMap() );
			Assert( tdOrigin );
			Q_memcpy( &vPredictedOrigin, predicted_state_data + tdOrigin->flatOffset[ TD_OFFSET_PACKED ], sizeof( Vector ) );

#if defined( DBGFLAG_ASSERT )
			Vector vNetworkedOrigin;
			Q_memcpy( &vNetworkedOrigin, networked_state_data + tdOrigin->flatOffset[ TD_OFFSET_PACKED ], sizeof( Vector ) );
			Assert( vNetworkedOrigin == m_vecNetworkOrigin );
#endif
		}

		Vector vOriginDelta = m_vecNetworkOrigin - vPredictedOrigin;

		

		Vector vPredictedVelocity;
		{
			const typedescription_t *tdVelocity = CPredictionCopy::FindFlatFieldByName( "m_vecAbsVelocity", GetPredDescMap() );			
			Assert( tdVelocity );
			Q_memcpy( &vPredictedVelocity, predicted_state_data + tdVelocity->flatOffset[ TD_OFFSET_PACKED ], sizeof( Vector ) );
		}

		Vector vVelocityDelta = m_vecAbsVelocity - vPredictedVelocity;

#if defined( DEBUG_MOTION_CONTROLLERS )
		extern void DebugVelocity( const char *szString, const Vector &vStart, const Vector &vEnd, uint8 iRed, uint8 iGreen, uint8 iBlue );
		extern void DebugBox( const char *szString, const Vector &vPos, const Vector &vSize, uint8 iRed, uint8 iGreen, uint8 iBlue, uint8 iAlpha );
		if( (GetFlags() & FL_ONGROUND) == 0 )
		{
			DebugVelocity( "Compensate", vPredictedOrigin, m_vecNetworkOrigin, 0, 0, 255 );
			DebugBox( "Compensate", m_vecNetworkOrigin, Vector( 0.25f, 0.25f, 0.25f ), 0, 255, 0, 100 );
		}
#endif

#if 0
		const float kMaxVelocityDelta = 50.0f;
		float fVelocityLengthSqr = vVelocityDelta.LengthSqr();
		if( vVelocityDelta.LengthSqr() > (kMaxVelocityDelta * kMaxVelocityDelta) )
		{
			vVelocityDelta *= (kMaxVelocityDelta / sqrtf(fVelocityLengthSqr));
		}
#endif
		
		pPredictedObject->SetErrorDelta_Position( vOriginDelta );
		pPredictedObject->SetErrorDelta_Velocity( vVelocityDelta );
	}	
}

//-----------------------------------------------------------------------------
// Returns the health fraction
//-----------------------------------------------------------------------------
float C_BaseEntity::HealthFraction() const
{
	if (GetMaxHealth() == 0)
		return 1.0f;

	float flFraction = (float)GetHealth() / (float)GetMaxHealth();
	flFraction = clamp( flFraction, 0.0f, 1.0f );
	return flFraction;
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves the coordinate frame for this entity.
// Input  : forward - Receives the entity's forward vector.
//			right - Receives the entity's right vector.
//			up - Receives the entity's up vector.
//-----------------------------------------------------------------------------
void C_BaseEntity::GetVectors(Vector* pForward, Vector* pRight, Vector* pUp) const
{
	// This call is necessary to cause m_rgflCoordinateFrame to be recomputed
	const matrix3x4_t &entityToWorld = EntityToWorldTransform();

	if (pForward != NULL)
	{
		MatrixGetColumn( entityToWorld, 0, *pForward ); 
	}

	if (pRight != NULL)
	{
		MatrixGetColumn( entityToWorld, 1, *pRight ); 
		*pRight *= -1.0f;
	}

	if (pUp != NULL)
	{
		MatrixGetColumn( entityToWorld, 2, *pUp ); 
	}
}


void C_BaseEntity::UpdateVisibilityAllEntities()
{
	C_BaseEntityIterator iterator;
	C_BaseEntity *pEnt;
	while ( (pEnt = iterator.Next()) != NULL )	
	{
		pEnt->UpdateVisibility();	
	}
}

// (static function)
CON_COMMAND( cl_updatevisibility, "Updates visibility bits." )
{
	C_BaseEntity::UpdateVisibilityAllEntities();
}
			    
void C_BaseEntity::RenderWithViewModels( bool bEnable )
{
	m_bRenderWithViewModels = bEnable;
	g_pClientLeafSystem->RenderWithViewModels( m_hRender, bEnable );
}

void C_BaseEntity::RenderForceOpaquePass( bool bEnable )
{
	g_pClientLeafSystem->EnableForceOpaquePass( m_hRender, bEnable );
}

bool C_BaseEntity::IsRenderForceOpaquePass() const
{
	return (m_hRender != INVALID_CLIENT_RENDER_HANDLE && g_pClientLeafSystem->IsEnableForceOpaquePass( m_hRender ));
}

bool C_BaseEntity::IsRenderingWithViewModels() const
{
	Assert( ( m_hRender == INVALID_CLIENT_RENDER_HANDLE ) || 
		( m_bRenderWithViewModels == g_pClientLeafSystem->IsRenderingWithViewModels( m_hRender ) ) );
	return m_bRenderWithViewModels;
}

void C_BaseEntity::DisableCachedRenderBounds( bool bDisabled )
{
	m_bDisableCachedRenderBounds = bDisabled;
	g_pClientLeafSystem->DisableCachedRenderBounds( m_hRender, bDisabled );
}

bool C_BaseEntity::IsCachedRenderBoundsDisabled() const
{
	return m_bDisableCachedRenderBounds;
}

void C_BaseEntity::UpdateVisibility()
{
#if MAX_SPLITSCREEN_PLAYERS > 1
	uint32 nPreviousValue = m_VisibilityBits.GetDWord( 0 );
#endif

	m_VisibilityBits.ClearAll();
	
	// NOTE: Do not reactivate this. We need to use ShouldSuppressForSplitScreenPlayer
	// for make portals work. Make sure ShouldDraw always returns true
	bool bDraw;
	{
		CSetActiveSplitScreenPlayerGuard g( __FILE__, __LINE__ );
		bDraw = ( ShouldDraw() && !IsDormant() && ( !ToolsEnabled() || IsEnabledInToolView() ) );
	}
	if ( bDraw )
	{
		bool bIsSplitScreenActive = engine->IsSplitScreenActive();
		if ( !bIsSplitScreenActive )
		{
			C_BasePlayer::SetRemoteSplitScreenPlayerViewsAreLocalPlayer( true );
			IterateRemoteSplitScreenViewSlots_Push( true );
		}

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			bool bShouldSkip = ShouldSuppressForSplitScreenPlayer( hh );
			if ( !bShouldSkip )
			{
				m_VisibilityBits.Set( hh );
			}
		}

		if ( !bIsSplitScreenActive )
		{
			IterateRemoteSplitScreenViewSlots_Pop();
			C_BasePlayer::SetRemoteSplitScreenPlayerViewsAreLocalPlayer( false );
		}
	}


#if MAX_SPLITSCREEN_PLAYERS > 1
	if ( nPreviousValue != m_VisibilityBits.GetDWord( 0 ) )
	{
		OnSplitscreenRenderingChanged();
	}
#endif

	if ( bDraw )
	{
		// add/update leafsystem
		AddToLeafSystem();
	}
	else
	{
		// remove from leaf system
		RemoveFromLeafSystem();
	}
}

bool C_BaseEntity::ShouldDrawForSplitScreenUser( int nSlot )
{ 
	return m_VisibilityBits.IsBitSet( nSlot );
}


//-----------------------------------------------------------------------------
// Hooks into the fast path render system
//-----------------------------------------------------------------------------
IClientModelRenderable*	C_BaseEntity::GetClientModelRenderable()
{
	if ( !m_bReadyToDraw || !m_bCanUseBrushModelFastPath )
		return NULL;

#ifdef PORTAL
	// Cannot participate if it has a render clip plane
	if ( GetRenderClipPlane() != NULL )
		return NULL;
#endif

	return this; 
}


//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
bool C_BaseEntity::GetRenderData( void *pData, ModelDataCategory_t nCategory )
{
	switch ( nCategory )
	{
	case MODEL_DATA_STENCIL:
		return false; //ComputeStencilState( (ShaderStencilState_t*)pData );

	default:
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether object should render.
//-----------------------------------------------------------------------------
bool C_BaseEntity::ShouldDraw()
{
// Only test this in tf2
#if defined( INVASION_CLIENT_DLL )
	// Let the client mode (like commander mode) reject drawing entities.
	if (GetClientMode() && !GetClientMode()->ShouldDrawEntity(this) )
		return false;
#endif

	// Some rendermodes prevent rendering
	if ( m_nRenderMode == kRenderNone )
		return false;

	if ( !IsGameConsole() )
	{
		CPULevel_t nCPULevel = GetCPULevel();
		bool bNoDraw = ( m_nMinCPULevel && m_nMinCPULevel-1 > nCPULevel );
		bNoDraw = bNoDraw || ( m_nMaxCPULevel && m_nMaxCPULevel-1 < nCPULevel );
		if ( bNoDraw )
			return false;

		GPULevel_t nGPULevel = GetGPULevel();
		bNoDraw = ( m_nMinGPULevel && m_nMinGPULevel-1 > nGPULevel );
		bNoDraw = bNoDraw || ( m_nMaxGPULevel && m_nMaxGPULevel-1 < nGPULevel );
		if ( bNoDraw )
			return false;
	}

	return (model != 0) && !IsEffectActive(EF_NODRAW) && (index != 0);
}

bool C_BaseEntity::TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace )
{
	return false;
}

bool C_BaseEntity::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return false;
}

//-----------------------------------------------------------------------------
// Used when the collision prop is told to ask game code for the world-space surrounding box
//-----------------------------------------------------------------------------
void C_BaseEntity::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	// This should only be called if you're using USE_GAME_CODE on the server
	// and you forgot to implement the client-side version of this method.
	Assert(0);
}


//-----------------------------------------------------------------------------
// Purpose: Derived classes will have to write their own message cracking routines!!!
// Input  : length - 
//			*data - 
//-----------------------------------------------------------------------------
void C_BaseEntity::ReceiveMessage( int classID, bf_read &msg )
{
	// BaseEntity doesn't have a base class we could relay this message to
	Assert( classID == GetClientClass()->m_ClassID );
	
	int messageType = msg.ReadByte();
	switch( messageType )
	{
		case BASEENTITY_MSG_REMOVE_DECALS:	RemoveAllDecals();
											break;
	}
}


void* C_BaseEntity::GetDataTableBasePtr()
{
	return this;
}


//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_BaseEntity::ShadowCastType()
{
	if (IsEffectActive(EF_NODRAW | EF_NOSHADOW))
		return SHADOWS_NONE;

	int modelType = modelinfo->GetModelType( model );
	return (modelType == mod_studio) ? SHADOWS_RENDER_TO_TEXTURE : SHADOWS_NONE;
}

//-----------------------------------------------------------------------------
// Fast reflections
//-----------------------------------------------------------------------------
bool C_BaseEntity::ComputeIsRenderingInFastReflections() const
{
	if ( IsEffectActive( EF_MARKED_FOR_FAST_REFLECTION ) )
		return true;

	C_BaseEntity *pParent = GetMoveParent();
	if ( pParent )
		return pParent->ComputeIsRenderingInFastReflections();
	return false;
}

bool C_BaseEntity::IsRenderingInFastReflections() const
{
	Assert( ( m_hRender == INVALID_CLIENT_RENDER_HANDLE ) || 
		( ComputeIsRenderingInFastReflections() == g_pClientLeafSystem->IsRenderingInFastReflections( m_hRender ) ) );
	return ComputeIsRenderingInFastReflections();
}

void C_BaseEntity::OnFastReflectionRenderingChanged()
{
	bool bIsReflecting = ComputeIsRenderingInFastReflections();

	bool bChanged = ( m_hRender == INVALID_CLIENT_RENDER_HANDLE ) ||
		( bIsReflecting != g_pClientLeafSystem->IsRenderingInFastReflections( m_hRender ) );
	if ( bChanged )
	{
		g_pClientLeafSystem->RenderInFastReflections( m_hRender, bIsReflecting );

		// Children must also update
		for ( C_BaseEntity *pChild = FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
		{
			pChild->OnFastReflectionRenderingChanged();
		}
	}
}

void C_BaseEntity::OnDisableShadowDepthRenderingChanged()
{
	bool bIsShadowDepthRenderingDisabled = IsEffectActive( EF_NOSHADOWDEPTH );
	g_pClientLeafSystem->DisableShadowDepthRendering( m_hRender, bIsShadowDepthRenderingDisabled );
}

void C_BaseEntity::OnDisableCSMRenderingChanged()
{
	bool bIsCSMRenderingDisabled = IsEffectActive( EF_NOCSM );
	g_pClientLeafSystem->DisableCSMRendering( m_hRender, bIsCSMRenderingDisabled );
}

void C_BaseEntity::OnShadowDepthRenderingCacheableStateChanged()
{
	bool bIsShadowDepthRenderingCacheDisabled = IsEffectActive( EF_SHADOWDEPTH_NOCACHE );
	g_pClientLeafSystem->DisableShadowDepthCaching( m_hRender, bIsShadowDepthRenderingCacheDisabled );
}


//-----------------------------------------------------------------------------
// Per-entity shadow cast distance + direction
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetShadowCastDistance( float *pDistance, ShadowType_t shadowType ) const			
{ 
	if ( m_flShadowCastDistance != 0.0f )
	{
		*pDistance = m_flShadowCastDistance; 
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseEntity *C_BaseEntity::GetShadowUseOtherEntity( void ) const
{
	return m_ShadowDirUseOtherEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetShadowUseOtherEntity( C_BaseEntity *pEntity )
{
	m_ShadowDirUseOtherEntity = pEntity;
}

CDiscontinuousInterpolatedVar< QAngle >& C_BaseEntity::GetRotationInterpolator()
{
	return m_iv_angRotation;
}

CDiscontinuousInterpolatedVar< Vector >& C_BaseEntity::GetOriginInterpolator()
{
	return m_iv_vecOrigin;
}

//-----------------------------------------------------------------------------
// Purpose: Return a per-entity shadow cast direction
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const			
{ 
	if ( m_ShadowDirUseOtherEntity )
		return m_ShadowDirUseOtherEntity->GetShadowCastDirection( pDirection, shadowType );

	return false;
}


//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_BaseEntity::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( IsEffectActive( EF_NODRAW ) )
		 return false;

 	if ( IsEffectActive( EF_NOFLASHLIGHT ) )
 		 return false;

	if( ( flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) != 0 )
	{
		if ( GetRenderMode() > kRenderNormal && GetRenderAlpha() == 0 )
			 return false;

		return true;
	}

	Assert( flags & SHADOW_FLAGS_SHADOW );

	if ( IsEffectActive( EF_NORECEIVESHADOW ) )
		 return false;

	if (modelinfo->GetModelType( model ) == mod_studio)
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Shadow-related methods
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsShadowDirty( )
{
	return IsEFlagSet( EFL_DIRTY_SHADOWUPDATE );
}

void C_BaseEntity::MarkShadowDirty( bool bDirty )
{
	if ( bDirty )
	{
		AddEFlags( EFL_DIRTY_SHADOWUPDATE );
	}
	else
	{
		RemoveEFlags( EFL_DIRTY_SHADOWUPDATE );
	}
}

IClientRenderable *C_BaseEntity::GetShadowParent()
{
	C_BaseEntity *pParent = GetMoveParent();
	return pParent ? pParent->GetClientRenderable() : NULL;
}

IClientRenderable *C_BaseEntity::FirstShadowChild()
{
	C_BaseEntity *pChild = FirstMoveChild();
	return pChild ? pChild->GetClientRenderable() : NULL;
}

IClientRenderable *C_BaseEntity::NextShadowPeer()
{
	C_BaseEntity *pPeer = NextMovePeer();
	return pPeer ? pPeer->GetClientRenderable() : NULL;
}

	
//-----------------------------------------------------------------------------
// Purpose: Returns index into entities list for this entity
// Output : Index
//-----------------------------------------------------------------------------
int	C_BaseEntity::entindex( void ) const
{
	return index;
}

int C_BaseEntity::GetSoundSourceIndex() const
{
#ifdef _DEBUG
	if ( index != -1 )
	{
		Assert( index == GetRefEHandle().GetEntryIndex() );
	}
#endif
	return GetRefEHandle().GetEntryIndex();
}

//-----------------------------------------------------------------------------
// Get render origin and angles
//-----------------------------------------------------------------------------
const Vector& C_BaseEntity::GetRenderOrigin( void )
{
	return GetAbsOrigin();
}

const QAngle& C_BaseEntity::GetRenderAngles( void )
{
	return GetAbsAngles();
}

const matrix3x4_t &C_BaseEntity::RenderableToWorldTransform()
{
	return EntityToWorldTransform();
}

IPVSNotify* C_BaseEntity::GetPVSNotifyInterface()
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : theMins - 
//			theMaxs - 
//-----------------------------------------------------------------------------
void C_BaseEntity::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	int nModelType = modelinfo->GetModelType( model );
	if (nModelType == mod_studio || nModelType == mod_brush)
	{
		modelinfo->GetModelRenderBounds( GetModel(), theMins, theMaxs );
	}
	else
	{
		// By default, we'll just snack on the collision bounds, transform
		// them into entity-space, and call it a day.
		if ( GetRenderAngles() == CollisionProp()->GetCollisionAngles() )
		{
			theMins = CollisionProp()->OBBMins();
			theMaxs = CollisionProp()->OBBMaxs();
		}
		else
		{
			Assert( CollisionProp()->GetCollisionAngles() == vec3_angle );
			if ( IsPointSized() )
			{
				//theMins = CollisionProp()->GetCollisionOrigin();
				//theMaxs	= theMins;
				theMins = theMaxs = vec3_origin;
			}
			else
			{
				// NOTE: This shouldn't happen! Or at least, I haven't run
				// into a valid case where it should yet.
//				Assert(0);
				IRotateAABB( EntityToWorldTransform(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), theMins, theMaxs );
			}
		}
	}
}

void C_BaseEntity::GetRenderBoundsWorldspace( Vector& mins, Vector& maxs )
{
	DefaultRenderBoundsWorldspace( this, mins, maxs );
}


void C_BaseEntity::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	m_EntClientFlags |= ENTCLIENTFLAG_GETTINGSHADOWRENDERBOUNDS;
	GetRenderBounds( mins, maxs );
	m_EntClientFlags &= ~ENTCLIENTFLAG_GETTINGSHADOWRENDERBOUNDS;
}


//-----------------------------------------------------------------------------
// Purpose: Last received origin
// Output : const float
//-----------------------------------------------------------------------------
const Vector& C_BaseEntity::GetAbsOrigin( void ) const
{
	Assert( s_bAbsQueriesValid );
	const_cast<C_BaseEntity*>(this)->CalcAbsolutePosition();
	return m_vecAbsOrigin;
}


//-----------------------------------------------------------------------------
// Purpose: Last received angles
// Output : const
//-----------------------------------------------------------------------------
const QAngle& C_BaseEntity::GetAbsAngles( void ) const
{
	Assert( s_bAbsQueriesValid );
	const_cast<C_BaseEntity*>(this)->CalcAbsolutePosition();
	return m_angAbsRotation;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetNetworkOrigin( const Vector& org )
{
	m_vecNetworkOrigin = org;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetNetworkAngles( const QAngle& ang )
{
	m_angNetworkAngles = ang;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const Vector&
//-----------------------------------------------------------------------------
const Vector& C_BaseEntity::GetNetworkOrigin() const
{
	return m_vecNetworkOrigin;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const QAngle&
//-----------------------------------------------------------------------------
const QAngle& C_BaseEntity::GetNetworkAngles() const
{
	return m_angNetworkAngles;
}


//-----------------------------------------------------------------------------
// Purpose: Get current model pointer for this entity
// Output : const struct model_s
//-----------------------------------------------------------------------------
const model_t *C_BaseEntity::GetModel( void ) const
{
	return model;
}



//-----------------------------------------------------------------------------
// Purpose: Get model index for this entity
// Output : int - model index
//-----------------------------------------------------------------------------
int C_BaseEntity::GetModelIndex( void ) const
{
	return m_nModelIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetModelIndex( int index )
{
	m_nModelIndex = index;
	const model_t *pModel = modelinfo->GetModel( m_nModelIndex );
	SetModelPointer( pModel );
}

void C_BaseEntity::SetModelPointer( const model_t *pModel )
{
	if ( pModel != model )
	{
		DestroyModelInstance();
		model = pModel;
		OnNewModel();
		UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : val - 
//			moveCollide - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetMoveType( MoveType_t val, MoveCollide_t moveCollide /*= MOVECOLLIDE_DEFAULT*/ )
{
	// Make sure the move type + move collide are compatible...
#ifdef _DEBUG
	if ((val != MOVETYPE_FLY) && (val != MOVETYPE_FLYGRAVITY))
	{
		Assert( moveCollide == MOVECOLLIDE_DEFAULT );
	}
#endif

 	m_MoveType = val;
	SetMoveCollide( moveCollide );
}

void C_BaseEntity::SetMoveCollide( MoveCollide_t val )
{
	m_MoveCollide = val;
}

//-----------------------------------------------------------------------------
// Purpose: Get rendermode
// Output : int - the render mode
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsTransparent( void )
{
	bool modelIsTransparent = modelinfo->IsTranslucent(model);
	return modelIsTransparent || (m_nRenderMode != kRenderNormal);
}

//-----------------------------------------------------------------------------
// Default implementation of compute translucency type
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t C_BaseEntity::ComputeTranslucencyType()
{
	if ( m_bIsBlurred )
		return RENDERABLE_IS_TRANSLUCENT;
	return modelinfo->ComputeTranslucencyType( model, GetSkin(), GetBody() );
}


//-----------------------------------------------------------------------------
// Client code should call this under any circumstances where translucency type may change
//-----------------------------------------------------------------------------
void C_BaseEntity::OnTranslucencyTypeChanged()
{
	if ( m_hRender != INVALID_CLIENT_RENDER_HANDLE )
	{
		g_pClientLeafSystem->SetTranslucencyType( m_hRender, ComputeTranslucencyType() );
	}
}


//-----------------------------------------------------------------------------
// Client code should call this under any circumstances where splitscreen rendering may change
//-----------------------------------------------------------------------------
void C_BaseEntity::OnSplitscreenRenderingChanged()
{
	if ( IsSplitScreenSupported() && ( m_hRender != INVALID_CLIENT_RENDER_HANDLE ) )
	{
		g_pClientLeafSystem->EnableSplitscreenRendering( m_hRender, ComputeSplitscreenRenderingFlags( this ) );
	}
}


int C_BaseEntity::GetRenderFlags( void )
{
	const model_t *pModel = GetModel();
	if ( pModel && render->DoesBrushModelNeedPowerOf2Framebuffer( pModel ) )
	{
		return ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB;
	}
	else
	{
		return 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get pointer to CMouthInfo data
// Output : CMouthInfo
//-----------------------------------------------------------------------------
CMouthInfo *C_BaseEntity::GetMouth( void )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Retrieve sound spatialization info for the specified sound on this entity
// Input  : info - 
// Output : Return false to indicate sound is not audible
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetSoundSpatialization( SpatializationInfo_t& info )
{
	// World is always audible
	if ( entindex() == 0 )
	{
		return true;
	}

	// Out of PVS
	if ( IsDormant() )
	{
		return false;
	}

	// pModel might be NULL, but modelinfo can handle that
	const model_t *pModel = GetModel();
	
	if ( info.pflRadius )
	{
		*info.pflRadius = modelinfo->GetModelRadius( pModel );
	}
	
	if ( info.pOrigin )
	{
		*info.pOrigin = GetAbsOrigin();

		// move origin to middle of brush
		if ( modelinfo->GetModelType( pModel ) == mod_brush )
		{
			Vector mins, maxs, center;

			modelinfo->GetModelBounds( pModel, mins, maxs );
			VectorAdd( mins, maxs, center );
			VectorScale( center, 0.5f, center );

			(*info.pOrigin) += center;
		}
	}

	if ( info.pAngles )
	{
		VectorCopy( GetAbsAngles(), *info.pAngles );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get attachment point by index
// Input  : number - which point
// Output : float * - the attachment point
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	origin = GetAbsOrigin();
	angles = GetAbsAngles();
	return true;
}

bool C_BaseEntity::GetAttachment( int number, Vector &origin )
{
	origin = GetAbsOrigin();
	return true;
}

bool C_BaseEntity::GetAttachment( int number, matrix3x4_t &matrix )
{
	MatrixCopy( EntityToWorldTransform(), matrix );
	return true;
}

bool C_BaseEntity::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	originVel = GetAbsVelocity();
	angleVel.Init();
	return true;
}

bool C_BaseEntity::ComputeLightingOrigin( int nAttachmentIndex, Vector modelLightingCenter, const matrix3x4_t &matrix, Vector &transformedLightingCenter )
{
	if ( nAttachmentIndex <= 0 )
	{
		VectorTransform( modelLightingCenter, matrix, transformedLightingCenter );
	}
	else
	{
		matrix3x4_t attachmentTransform;
		GetAttachment( nAttachmentIndex, attachmentTransform );
		VectorTransform( modelLightingCenter, attachmentTransform, transformedLightingCenter );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get this entity's rendering clip plane if one is defined
// Output : float * - The clip plane to use, or NULL if no clip plane is defined
//-----------------------------------------------------------------------------
float *C_BaseEntity::GetRenderClipPlane( void )
{
	if( m_bEnableRenderingClipPlane )
		return m_fRenderingClipPlane;
	else
		return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseEntity::DrawBrushModel( bool bDrawingTranslucency, int nFlags, bool bTwoPass )
{
	VPROF_BUDGET( "C_BaseEntity::DrawBrushModel", VPROF_BUDGETGROUP_BRUSHMODEL_RENDERING );
	// Identity brushes are drawn in view->DrawWorld as an optimization
	Assert ( modelinfo->GetModelType( model ) == mod_brush );

	ERenderDepthMode_t DepthMode = DEPTH_MODE_NORMAL;
	if ( ( nFlags & STUDIO_SSAODEPTHTEXTURE ) != 0 )
	{
		DepthMode = DEPTH_MODE_SSA0;
	}
	else if ( ( nFlags & STUDIO_SHADOWDEPTHTEXTURE ) != 0 )
	{
		DepthMode = DEPTH_MODE_SHADOW;
	}


	if ( DepthMode != DEPTH_MODE_NORMAL )
	{
		render->DrawBrushModelShadowDepth( this, (model_t *)model, GetAbsOrigin(), GetAbsAngles(), DepthMode );
	}
	else
	{
		DrawBrushModelMode_t mode = DBM_DRAW_ALL;
		if ( bTwoPass )
		{
			mode = bDrawingTranslucency ? DBM_DRAW_TRANSLUCENT_ONLY : DBM_DRAW_OPAQUE_ONLY;
		}
		render->DrawBrushModelEx( this, (model_t *)model, GetAbsOrigin(), GetAbsAngles(), mode );
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
int C_BaseEntity::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	int drawn = 0;
	if ( !model )
	{
		return drawn;
	}

	int modelType = modelinfo->GetModelType( model );
	switch ( modelType )
	{
	case mod_brush:
		render->SetBlend( ( flags & STUDIO_SHADOWDEPTHTEXTURE ) ? 1.0f : (float)instance.m_nAlpha * ( 1.0f / 255.0f ) );
		drawn = DrawBrushModel( ( flags & STUDIO_TRANSPARENCY ) ? true : false, ( flags & STUDIO_SHADOWDEPTHTEXTURE ) ? true : false, ( flags & STUDIO_TWOPASS ) ? true : false );
		break;
	case mod_studio:
		// All studio models must be derived from C_BaseAnimating.  Issue warning.
		Warning( "ERROR:  Can't draw studio model %s because %s is not derived from C_BaseAnimating\n",
			modelinfo->GetModelName( model ), GetClientClass()->m_pNetworkName ? GetClientClass()->m_pNetworkName : "unknown" );
		break;
	case mod_sprite:
		//drawn = DrawSprite();
		Warning( "ERROR:  Sprite model's not supported any more except in legacy temp ents\n" );
		break;
	default:
		break;
	}

	// If we're visualizing our bboxes, draw them
	DrawBBoxVisualizations();

	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: Setup the bones for drawing
//-----------------------------------------------------------------------------
bool C_BaseEntity::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_BaseEntity::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
}


//-----------------------------------------------------------------------------
// Purpose: Process any local client-side animation events
//-----------------------------------------------------------------------------
void C_BaseEntity::DoAnimationEvents( )
{
}


void C_BaseEntity::UpdatePartitionListEntry()
{
	// Don't add the world entity
	CollideType_t shouldCollide = GetCollideType();

	// Choose the list based on what kind of collisions we want
	int list = PARTITION_CLIENT_NON_STATIC_EDICTS;
	if (shouldCollide == ENTITY_SHOULD_COLLIDE)
		list |= PARTITION_CLIENT_SOLID_EDICTS;
	else if (shouldCollide == ENTITY_SHOULD_RESPOND)
		list |= PARTITION_CLIENT_RESPONSIVE_EDICTS;

	if ( m_bIsValidIKAttachment )
	{
		list |= PARTITION_CLIENT_IK_ATTACHMENT;
	}

	// add the entity to the KD tree so we will collide against it
	::partition->RemoveAndInsert( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS | PARTITION_CLIENT_IK_ATTACHMENT, list, CollisionProp()->GetPartitionHandle() );
}


void C_BaseEntity::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Init should have been called before we get in here.
	Assert( CollisionProp()->GetPartitionHandle() != PARTITION_INVALID_HANDLE );
	if ( entindex() < 0 )
		return;
	
	switch( state )
	{
	case SHOULDTRANSMIT_START:
		{
			// We've just been sent by the server. Become active.
			SetDormant( false );
			
			UpdatePartitionListEntry();

#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
			// Note that predictables get a chance to hook up to their server counterparts here
			if ( m_PredictableID.IsActive() )
			{
				// Find corresponding client side predicted entity and remove it from predictables
				m_PredictableID.SetAcknowledged( true );

				C_BaseEntity *otherEntity = FindPreviouslyCreatedEntity( m_PredictableID );
				if ( otherEntity )
				{
					Assert( otherEntity->IsClientCreated() );
					Assert( otherEntity->m_PredictableID.IsActive() );
					Assert( ClientEntityList().IsHandleValid( otherEntity->GetClientHandle() ) );

					otherEntity->m_PredictableID.SetAcknowledged( true );

					if ( OnPredictedEntityRemove( false, otherEntity ) )
					{
						// Mark it for delete after receive all network data
						otherEntity->Release();
					}
				}
			}
#endif
		}
		break;

	case SHOULDTRANSMIT_END:
		{
			// Clear out links if we're out of the picture...
			UnlinkFromHierarchy();

			// We're no longer being sent by the server. Become dormant.
			SetDormant( true );
			
			// remove the entity from the KD tree so we won't collide against it
			::partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );
		
		}
		break;

	default:
		Assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Call this in PostDataUpdate if you don't chain it down!
//-----------------------------------------------------------------------------
void C_BaseEntity::MarkMessageReceived()
{
	m_flLastMessageTime = engine->GetLastTimeStamp();
}


//-----------------------------------------------------------------------------
// Purpose: Entity is about to be decoded from the network stream
// Input  : bnewentity - is this a new entity this update?
//-----------------------------------------------------------------------------
void C_BaseEntity::PreDataUpdate( DataUpdateType_t updateType )
{
	// Register for an OnDataChanged call and call OnPreDataChanged().
	if ( AddDataChangeEvent( this, updateType, &m_DataChangeEventRef ) )
	{
		OnPreDataChanged( updateType );
	}


	// Need to spawn on client before receiving original network data 
	// in case it overrides any values set up in spawn ( e.g., m_iState )
	bool bnewentity = (updateType == DATA_UPDATE_CREATED);

	if ( !bnewentity )
	{
		Interp_RestoreToLastNetworked( GetVarMapping(), 0 /*both simulation and animation vars*/ );
	}

	if ( bnewentity && !IsClientCreated() )
	{
		m_flSpawnTime = engine->GetLastTimeStamp();
		MDLCACHE_CRITICAL_SECTION();
		Spawn();
	}

#if 0 // Yahn suggesting commenting this out as a fix to demo recording not working
	// If the entity moves itself every FRAME on the server but doesn't update animtime,
	// then use the current server time as the time for interpolation.
	if ( IsSelfAnimating() )
	{
		m_flAnimTime = engine->GetLastTimeStamp();
	}
#endif

	m_vecOldOrigin = GetNetworkOrigin();
	m_vecOldAngRotation = GetNetworkAngles();

	m_flOldAnimTime = m_flAnimTime;
	m_flOldSimulationTime = m_flSimulationTime;

	m_nOldRenderMode = m_nRenderMode;

	if ( m_hRender != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->EnableAlternateSorting( m_hRender, m_bAlternateSorting );
	}
}

const Vector& C_BaseEntity::GetOldOrigin()
{
	return m_vecOldOrigin;
}


void C_BaseEntity::UnlinkChild( C_BaseEntity *pParent, C_BaseEntity *pChild )
{
	Assert( pChild );
	Assert( pParent != pChild );
	Assert( pChild->GetMoveParent() == pParent );

	// Unlink from parent
	// NOTE: pParent *may well be NULL*! This occurs
	// when a child has unlinked from a parent, and the child
	// remains in the PVS but the parent has not
	if (pParent && (pParent->m_pMoveChild == pChild))
	{
		Assert( !(pChild->m_pMovePrevPeer.IsValid()) );
		pParent->m_pMoveChild = pChild->m_pMovePeer;
	}

	// Unlink from siblings...
	if (pChild->m_pMovePrevPeer)
	{
		pChild->m_pMovePrevPeer->m_pMovePeer = pChild->m_pMovePeer;
	}
	if (pChild->m_pMovePeer)
	{
		pChild->m_pMovePeer->m_pMovePrevPeer = pChild->m_pMovePrevPeer;
	}

	pChild->m_pMovePeer = NULL;
	pChild->m_pMovePrevPeer = NULL;
	pChild->m_pMoveParent = NULL;
	pChild->RemoveFromAimEntsList();
	pChild->OnFastReflectionRenderingChanged();

	Interp_HierarchyUpdateInterpolationAmounts();
}

void C_BaseEntity::LinkChild( C_BaseEntity *pParent, C_BaseEntity *pChild )
{
	Assert( !pChild->m_pMovePeer.IsValid() );
	Assert( !pChild->m_pMovePrevPeer.IsValid() );
	Assert( !pChild->m_pMoveParent.IsValid() );
	Assert( pParent != pChild );

#ifdef _DEBUG
	// Make sure the child isn't already in this list
	C_BaseEntity *pExistingChild;
	for ( pExistingChild = pParent->FirstMoveChild(); pExistingChild; pExistingChild = pExistingChild->NextMovePeer() )
	{
		Assert( pChild != pExistingChild );
	}
#endif

	pChild->m_pMovePrevPeer = NULL;
	pChild->m_pMovePeer = pParent->m_pMoveChild;
	if (pChild->m_pMovePeer)
	{
		pChild->m_pMovePeer->m_pMovePrevPeer = pChild;
	}
	pParent->m_pMoveChild = pChild;
	pChild->m_pMoveParent = pParent;
	pChild->AddToAimEntsList();
	pChild->OnFastReflectionRenderingChanged();

	Interp_HierarchyUpdateInterpolationAmounts();
}

CUtlVector< C_BaseEntity * >	g_AimEntsList;


//-----------------------------------------------------------------------------
// Moves all aiments
//-----------------------------------------------------------------------------
void C_BaseEntity::MarkAimEntsDirty()
{
	// FIXME: With the dirty bits hooked into cycle + sequence, it's unclear
	// that this is even necessary any more (provided aiments are always accessing
	// joints or attachments of the move parent).
	//
	// NOTE: This is a tricky algorithm. This list does not actually contain
	// all aim-ents in its list. It actually contains all hierarchical children,
	// of which aim-ents are a part. We can tell if something is an aiment if it has
	// the EF_BONEMERGE effect flag set.
	// 
	// We will first iterate over all aiments and clear their DIRTY_ABSTRANSFORM flag, 
	// which is necessary to cause them to recompute their aim-ent origin 
	// the next time CalcAbsPosition is called. Because CalcAbsPosition calls MoveToAimEnt
	// and MoveToAimEnt calls SetAbsOrigin/SetAbsAngles, that is how CalcAbsPosition
	// will cause the aim-ent's (and all its children's) dirty state to be correctly updated.
	//
	// Then we will iterate over the loop a second time and call CalcAbsPosition on them,
	int i;
	int c = g_AimEntsList.Count();
	for ( i = 0; i < c; ++i )
	{
		C_BaseEntity *pEnt = g_AimEntsList[ i ];
		Assert( pEnt && pEnt->GetMoveParent() );
		if ( pEnt->IsEffectActive(EF_BONEMERGE | EF_PARENT_ANIMATES) )
		{
			pEnt->AddEFlags( EFL_DIRTY_ABSTRANSFORM );
		}
	}
}


void C_BaseEntity::CalcAimEntPositions()
{
	VPROF("CalcAimEntPositions");
	int i;
	int c = g_AimEntsList.Count();
	for ( i = 0; i < c; ++i )
	{
		C_BaseEntity *pEnt = g_AimEntsList[ i ];
		Assert( pEnt );
		Assert( pEnt->GetMoveParent() );
		if ( pEnt->IsEffectActive(EF_BONEMERGE) )
		{
			pEnt->CalcAbsolutePosition( );
		}
	}
}


void C_BaseEntity::AddToAimEntsList()
{
	// Already in list
	if ( m_AimEntsListHandle != INVALID_AIMENTS_LIST_HANDLE )
		return;

	m_AimEntsListHandle = g_AimEntsList.AddToTail( this );
}

void C_BaseEntity::RemoveFromAimEntsList()
{
	// Not in list yet
	if ( INVALID_AIMENTS_LIST_HANDLE == m_AimEntsListHandle )
	{
		return;
	}

	unsigned int c = g_AimEntsList.Count();

	Assert( m_AimEntsListHandle < c );

	unsigned int last = c - 1;

	if ( last == m_AimEntsListHandle )
	{
		// Just wipe the final entry
		g_AimEntsList.FastRemove( last );
	}
	else
	{
		C_BaseEntity *lastEntity = g_AimEntsList[ last ];
		// Remove the last entry
		g_AimEntsList.FastRemove( last );

		// And update it's handle to point to this slot.
		lastEntity->m_AimEntsListHandle = m_AimEntsListHandle;
		g_AimEntsList[ m_AimEntsListHandle ] = lastEntity;
	}

	// Invalidate our handle no matter what.
	m_AimEntsListHandle = INVALID_AIMENTS_LIST_HANDLE;
}

//-----------------------------------------------------------------------------
// Update move-parent if needed. For SourceTV.
//-----------------------------------------------------------------------------
void C_BaseEntity::HierarchyUpdateMoveParent()
{
	if ( m_hNetworkMoveParent.ToInt() == m_pMoveParent.ToInt() )
		return;

	HierarchySetParent( m_hNetworkMoveParent );
}


//-----------------------------------------------------------------------------
// Connects us up to hierarchy
//-----------------------------------------------------------------------------
void C_BaseEntity::HierarchySetParent( C_BaseEntity *pNewParent )
{
	// NOTE: When this is called, we expect to have a valid
	// local origin, etc. that we received from network daa
	EHANDLE newParentHandle;
	newParentHandle.Set( pNewParent );
	if (newParentHandle.ToInt() == m_pMoveParent.ToInt())
		return;
	
	if (m_pMoveParent.IsValid())
	{
		UnlinkChild( m_pMoveParent, this );
	}
	if (pNewParent)
	{
		LinkChild( pNewParent, this );
	}

	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );

	// iterate the hierarchy using a ring buffer
	CBaseEntity *list[1024];	// assume power of 2 elements
	int listReadIndex = 0;
	int listWriteIndex = 1;
	list[0] = this;

	while ( listReadIndex != listWriteIndex )
	{
		CBaseEntity *pParent = list[listReadIndex];
		pParent->InvalidateAttachments();
		listReadIndex = (listReadIndex+1) & (ARRAYSIZE(list)-1);
		for (CBaseEntity *pChild = pParent->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer())
		{
			list[listWriteIndex] = pChild;
			listWriteIndex = (listWriteIndex+1) & (ARRAYSIZE(list)-1);
		}
	}
}


//-----------------------------------------------------------------------------
// Unlinks from hierarchy
//-----------------------------------------------------------------------------
void C_BaseEntity::SetParent( C_BaseEntity *pParentEntity, int iParentAttachment )
{
	// NOTE: This version is meant to be called *outside* of PostDataUpdate
	// as it assumes the moveparent has a valid handle
	EHANDLE newParentHandle;
	newParentHandle.Set( pParentEntity );
	if (newParentHandle.ToInt() == m_pMoveParent.ToInt())
		return;

	// NOTE: Have to do this before the unlink to ensure local coords are valid
	Vector vecAbsOrigin = GetAbsOrigin();
	QAngle angAbsRotation = GetAbsAngles();
	Vector vecAbsVelocity = GetAbsVelocity();

	// First deal with unlinking
	if (m_pMoveParent.IsValid())
	{
		UnlinkChild( m_pMoveParent, this );
	}

	if (pParentEntity)
	{
		LinkChild( pParentEntity, this );
	}

	if ( !IsServerEntity() )
	{
		m_hNetworkMoveParent = pParentEntity;
	}
	
	m_iParentAttachment = iParentAttachment;
	
	m_vecAbsOrigin.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	m_angAbsRotation.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	m_vecAbsVelocity.Init( FLT_MAX, FLT_MAX, FLT_MAX );

	SetAbsOrigin(vecAbsOrigin);
	SetAbsAngles(angAbsRotation);
	SetAbsVelocity(vecAbsVelocity);

}


//-----------------------------------------------------------------------------
// Unlinks from hierarchy
//-----------------------------------------------------------------------------
void C_BaseEntity::UnlinkFromHierarchy()
{
	// Clear out links if we're out of the picture...
	if ( m_pMoveParent.IsValid() )
	{
		UnlinkChild( m_pMoveParent, this );
	}

	//Adrian: This was causing problems with the local network backdoor with entities coming in and out of the PVS at certain times.
	//This would work fine if a full entity update was coming (caused by certain factors like too many entities entering the pvs at once).
	//but otherwise it would not detect the change on the client (since the server and client shouldn't be out of sync) and the var would not be updated like it should.
	//m_iParentAttachment = 0;

	// unlink also all move children
	C_BaseEntity *pChild = FirstMoveChild();
	while( pChild )
	{
		if ( pChild->m_pMoveParent != this )
		{
			Warning( "C_BaseEntity::UnlinkFromHierarchy(): Entity has a child with the wrong parent!\n" );
			Assert( 0 );
			UnlinkChild( this, pChild );
			pChild->UnlinkFromHierarchy();
		}
		else
			pChild->UnlinkFromHierarchy();
		pChild = FirstMoveChild();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Make sure that the correct model is referenced for this entity
//-----------------------------------------------------------------------------
void C_BaseEntity::ValidateModelIndex( void )
{
	SetModelByIndex( m_nModelIndex );
}

bool C_BaseEntity::IsParentChanging()
{
	return ( m_hNetworkMoveParent.ToInt() != m_pMoveParent.ToInt() );
}

//-----------------------------------------------------------------------------
// Purpose: Entity data has been parsed and unpacked.  Now do any necessary decoding, munging
// Input  : bnewentity - was this entity new in this update packet?
//-----------------------------------------------------------------------------
void C_BaseEntity::PostDataUpdate( DataUpdateType_t updateType )
{
	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, "postdataupdate" );

	// NOTE: This *has* to happen first. Otherwise, Origin + angles may be wrong 
	if ( m_bClientSideRagdoll && updateType == DATA_UPDATE_CREATED )
	{
		MoveToLastReceivedPosition( true );
	}
	else
	{
		MoveToLastReceivedPosition( false );
	}

	// If it's the world, force solid flags
	if ( index == 0 )
	{
		m_nModelIndex = 1;
		SetSolid( SOLID_BSP );

		// FIXME: Should these be assertions?
		SetAbsOrigin( vec3_origin );
		SetAbsAngles( vec3_angle );
	}

	if ( m_nOldRenderMode != m_nRenderMode )
	{
		SetRenderMode( (RenderMode_t)m_nRenderMode, true );
	}

	bool animTimeChanged = ( m_flAnimTime != m_flOldAnimTime ) ? true : false;
	bool originChanged = ( m_vecOldOrigin != GetLocalOrigin() ) ? true : false;
	bool anglesChanged = ( m_vecOldAngRotation != GetLocalAngles() ) ? true : false;
	bool simTimeChanged = ( m_flSimulationTime != m_flOldSimulationTime ) ? true : false;

	// Detect simulation changes 
	bool simulationChanged = originChanged || anglesChanged || simTimeChanged;

	bool bPredictable = GetPredictable();

	// For non-predicted and non-client only ents, we need to latch network values into the interpolation histories
	if ( !bPredictable && !IsClientCreated() )
	{
		if ( animTimeChanged )
		{
			OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
		}

		if ( simulationChanged )
		{
			OnLatchInterpolatedVariables( LATCH_SIMULATION_VAR );
		}
	}
	// For predictables, we also need to store off the last networked value
	else if ( bPredictable )
	{
		// Just store off last networked value for use in prediction
		OnStoreLastNetworkedValue();
	}

	// Deal with hierarchy. Have to do it here (instead of in a proxy)
	// because this is the only point at which all entities are loaded
	// If this condition isn't met, then a child was sent without its parent
	Assert( m_hNetworkMoveParent.Get() || !m_hNetworkMoveParent.IsValid() );
	HierarchySetParent(m_hNetworkMoveParent);

	MarkMessageReceived();

	// Make sure that the correct model is referenced for this entity
	ValidateModelIndex();

	// If this entity was new, then latch in various values no matter what.
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Construct a random value for this instance
		m_flProxyRandomValue = random->RandomFloat( 0, 1 );

		ResetLatched();

		m_nCreationTick = gpGlobals->tickcount;
	}

#if !defined( NO_ENTITY_PREDICTION )
	// It's possible that a new entity will need to be forceably added to the 
	//   player simulation list.  If so, do this here
	if ( IsPlayerSimulated() && C_BasePlayer::IsLocalPlayer( m_hPlayerSimulationOwner ) )
	{
		// Make sure player is driving simulation (field is only ever sent to local player)
		SetPlayerSimulated( ToBasePlayer( m_hPlayerSimulationOwner ) );
	}
#endif

	UpdatePartitionListEntry();
	
	// Add the entity to the nointerp list.
	if ( !IsClientCreated() )
	{
		if ( Teleported() || IsEffectActive(EF_NOINTERP) )
			AddToEntityList(ENTITY_LIST_TELEPORT);
	}

	// if we changed parents, recalculate visibility
	if ( m_hOldMoveParent != m_hNetworkMoveParent )
	{
		UpdateVisibility();
	}
}

static ConVar cl_simulationtimefix( "cl_simulationtimefix", "1", FCVAR_DEVELOPMENTONLY );
void C_BaseEntity::OnSimulationTimeChanging( float flPreviousSimulationTime, float flNextSimulationTime )
{
	if ( m_bDisableSimulationFix )
		return;

	if ( !cl_simulationtimefix.GetBool() )
	{
		return;
	}

	if ( GetPredictable() || IsClientCreated() )
	{
		return;
	}

	if ( !ShouldDraw() )
	{
		return;
	}

	// If the m_flSimulationTime is changing faster than or in lockstep with the interpolation amount, then never do a fixup
	float flOriginInterpolationAmount = m_iv_vecOrigin.GetInterpolationAmount();
	float dtSimulationTimestamps = ROUND_TO_TICKS( flNextSimulationTime - flPreviousSimulationTime );
	if ( dtSimulationTimestamps <= flOriginInterpolationAmount )
		return;

	// In the worst case (w/o packet loss) the engine could hit a slow frame and have to run off 0.1 (MAX_FRAMETIME) seconds worth of ticks.
	// Thus, something moving would get new m_flSimulationTime sample up to ROUND_TO_TICKS( 0.1f ) seconds from the previous time which
	//  is longer than the interpolation interval but still considered "smooth" and continuous motion.  We don't want to mess that case up, so what we
	//  do is to see how far in the past the previous packet time stamp was and if the simulation time delta is even greater than that only then do we 
	//  add some fixup samples.

	// Since we haven't called PostDataUpdate, m_flLastMessageTime is the timestamp of the last packet containing this entity and engine->GetLastTimeStamp() is the timestamp of the currently being 
	//  processed packet
	// TBD:  Avoid the call into the engine-> virtual interface!!!
	float dtFromLastPacket = ROUND_TO_TICKS( engine->GetLastTimeStamp() - m_flLastMessageTime );

	// If the m_flSimulationTime time will have changed more quickly than the timestamps of the last two packets received, also skip fixup
	if ( dtSimulationTimestamps <= dtFromLastPacket )
		return;

	// Worst case backfill is 100 msecs (default multiplayer interpolation amount)
	float flSampleBackFillMaxTime = ROUND_TO_TICKS( 0.1f );

	m_iv_vecOrigin.RestoreToLastNetworked();
	m_iv_angRotation.RestoreToLastNetworked();

	float flTimeBeforeWhichToLeaveOldSamples = flPreviousSimulationTime + TICK_INTERVAL * 0.5f;

	for ( float simtime = flNextSimulationTime - flSampleBackFillMaxTime;  
		  simtime < flNextSimulationTime; 
		  simtime += TICK_INTERVAL )
	{
		// Don't stomp preexisting data, though
		if ( simtime < flTimeBeforeWhichToLeaveOldSamples )
			continue;

		m_iv_vecOrigin.NoteChanged( gpGlobals->curtime, simtime, false );
		m_iv_angRotation.NoteChanged( gpGlobals->curtime, simtime, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Latch simulation values when the entity has not changed
//-----------------------------------------------------------------------------
void C_BaseEntity::OnDataUnchangedInPVS()
{
	Assert( m_hNetworkMoveParent.Get() || !m_hNetworkMoveParent.IsValid() );
	HierarchySetParent(m_hNetworkMoveParent);
	
	MarkMessageReceived();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *context - 
//-----------------------------------------------------------------------------
void C_BaseEntity::CheckInitPredictable( const char *context )
{
#if !defined( NO_ENTITY_PREDICTION )
	if ( !ShouldPredict() )
		return;

	// Prediction is disabled
	if ( !cl_predict->GetInt() )
		return;

	if ( !C_BasePlayer::HasAnyLocalPlayer() )
		return;

	if ( !GetPredictionEligible() )
	{
		bool bOkay = false;
#if defined( USE_PREDICTABLEID )
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
		{
			int nIndex = engine->GetSplitScreenPlayer( i );

			if ( m_PredictableID.IsActive() &&
				( nIndex - 1 ) == m_PredictableID.GetPlayer() )
			{
				// If it comes through with an ID, it should be eligible
				SetPredictionEligible( true );
				bOkay = true;
			}
		}
#endif
		if ( !bOkay )
		{
			return;
		}
	}

	if ( IsClientCreated() )
		return;

	if ( IsIntermediateDataAllocated() )
		return;

	// Msg( "Predicting init %s at %s\n", GetClassname(), context );

	// It's either a player, a weapon or a view model
	C_BasePlayer *pOwner = GetPredictionOwner();
	Assert( pOwner );
	if ( !pOwner )
		return;

	InitPredictable( pOwner );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: See if a predictable should stop predicting
// Input  : *context - 
//-----------------------------------------------------------------------------
void C_BaseEntity::CheckShutdownPredictable( const char *context )
{
	if ( IsClientCreated() )
		return;

	if ( !ShouldPredict() || 
		!GetPredictionEligible() ||
		(GetPredictionOwner() == NULL) )
	{
		if( IsIntermediateDataAllocated() )
		{
			ShutdownPredictable();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
C_BasePlayer* C_BaseEntity::GetPredictionOwner()
{
	C_BasePlayer *pOwner = ToBasePlayer( this );
	if ( !pOwner )
	{
		pOwner = ToBasePlayer( GetOwnerEntity() );
		if ( !pOwner )
		{
			C_BaseViewModel *vm = ToBaseViewModel(this);
			if ( vm )
			{
				pOwner = ToBasePlayer( vm->GetOwner() );
			}
		}
	}
	return pOwner;
}

bool C_BaseEntity::IsSelfAnimating()
{
	return true;
}


//-----------------------------------------------------------------------------
// EFlags.. 
//-----------------------------------------------------------------------------
int C_BaseEntity::GetEFlags() const
{
	return m_iEFlags;
}

void C_BaseEntity::SetEFlags( int iEFlags )
{
	m_iEFlags = iEFlags;
}


//-----------------------------------------------------------------------------
// Sets the model... 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetModelByIndex( int nModelIndex )
{
	SetModelIndex( nModelIndex );
}


//-----------------------------------------------------------------------------
// Set model... (NOTE: Should only be used by client-only entities
//-----------------------------------------------------------------------------
bool C_BaseEntity::SetModel( const char *pModelName )
{
	if ( pModelName )
	{
		int nModelIndex = modelinfo->GetModelIndex( pModelName );
		SetModelByIndex( nModelIndex );
		return ( nModelIndex != -1 );
	}
	else
	{
		SetModelByIndex( -1 );
		return false;
	}
}

void C_BaseEntity::OnStoreLastNetworkedValue()
{
	bool bRestore = false;
	Vector savePos;
	QAngle saveAng;

	// Kind of a hack, but we want to latch the actual networked value for origin/angles, not what's sitting in m_vecOrigin in the
	//  ragdoll case where we don't copy it over in MoveToLastNetworkOrigin
	if ( m_bClientSideRagdoll && GetPredictable() )
	{
		bRestore = true;
		savePos = GetLocalOrigin();
		saveAng = GetLocalAngles();

		MoveToLastReceivedPosition( true );
	}

	int c = m_VarMap.m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &m_VarMap.m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();

		if ( type & EXCLUDE_AUTO_LATCH )
			continue;

		watcher->NoteLastNetworkedValue();
	}

	if ( bRestore )
	{
		SetLocalOrigin( savePos );
		SetLocalAngles( saveAng );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The animtime is about to be changed in a network update, store off various fields so that
//  we can use them to do blended sequence transitions, etc.
// Input  : *pState - the (mostly) previous state data
//-----------------------------------------------------------------------------

void C_BaseEntity::OnLatchInterpolatedVariables( int flags )
{
	float changetime = GetLastChangeTime( flags );

	bool bUpdateLastNetworkedValue = !(flags & INTERPOLATE_OMIT_UPDATE_LAST_NETWORKED) ? true : false;

	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, bUpdateLastNetworkedValue ? "latch+net" : "latch" );

	int c = m_VarMap.m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &m_VarMap.m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();

		if ( !(type & flags) )
			continue;

		if ( type & EXCLUDE_AUTO_LATCH )
			continue;

		if ( watcher->NoteChanged( gpGlobals->curtime, changetime, bUpdateLastNetworkedValue ) )
			e->m_bNeedsToInterpolate = true;
	}
	
	if ( ShouldInterpolate() )
	{
		AddToEntityList(ENTITY_LIST_INTERPOLATE);
	}
}

float CBaseEntity::GetEffectiveInterpolationCurTime( float currentTime )
{
	if ( GetPredictable() || IsClientCreated() )
	{
		int slot = GetSplitUserPlayerPredictionSlot();
		Assert( slot != -1 );
		C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer( slot );
		if ( localplayer )
		{
			currentTime = localplayer->GetFinalPredictedTime();
			currentTime -= TICK_INTERVAL;
			currentTime += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
		}
	}

	return currentTime;
}

static ConVar cl_interp_watch( "cl_interp_watch", "-2", 0 );

int CBaseEntity::BaseInterpolatePart1( float &currentTime, Vector &oldOrigin, QAngle &oldAngles, int &bNoMoreChanges )
{
	// Don't mess with the world!!!
	bNoMoreChanges = 1;
	

	// These get moved to the parent position automatically
	if ( IsFollowingEntity() || !IsInterpolationEnabled() )
	{
		// Assume current origin ( no interpolation )
		MoveToLastReceivedPosition();
		return INTERPOLATE_STOP;
	}


	if ( GetPredictable() || IsClientCreated() )
	{
		int slot = GetSplitUserPlayerPredictionSlot();
		Assert( slot != -1 );
		C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer( slot );
		if ( localplayer && currentTime == gpGlobals->curtime )
		{
			currentTime = localplayer->GetFinalPredictedTime();
			currentTime -= TICK_INTERVAL;
			currentTime += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
		}
	}

	//
	// When playing GOTV demo or watching HLTV make sure that the IN_EYE
	// entity and their weapon don't interpolate causing lag behind shots
	//
	if ( HLTVCamera() && engine->IsPlayingDemo() && engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bAnonymousPlayerIdentity &&
		( HLTVCamera()->GetMode() == OBS_MODE_IN_EYE ) )
	{
		static const ConVar *s_pUpdateRate = g_pCVar->FindVar( "cl_updaterate" );
		C_BaseEntity *pInEyeEntity = HLTVCamera()->GetPrimaryTarget();
		if ( pInEyeEntity && pInEyeEntity->IsPlayer() && s_pUpdateRate )
		{
			float flAdjustDirection = ( ( this == pInEyeEntity ) || ( this->GetMoveParent() == pInEyeEntity ) ) ? 1.0f : -1.0f;
			float flTimeAdjustment = ( 1.0f / s_pUpdateRate->GetFloat() ); // ( 1/16 ) / ( 1/64 ) = 4
			currentTime += flTimeAdjustment * flAdjustDirection; // differently interpolate IN_EYE observed entity and children
		}
	}

	//
	// Run normal calculations to compute interpolation amount
	//

	oldOrigin = m_vecOrigin;
	oldAngles = m_angRotation;

	bNoMoreChanges = Interp_Interpolate( GetVarMapping(), currentTime );
	if ( cl_interp_all.GetInt() || (m_EntClientFlags & ENTCLIENTFLAG_ALWAYS_INTERPOLATE) )
		bNoMoreChanges = 0;

	if ( cl_interp_watch.GetInt() == index )
	{
		SpewInterpolatedVar( &m_iv_vecOrigin, currentTime, m_iv_vecOrigin.GetInterpolationAmount(), true );

	}

	return INTERPOLATE_CONTINUE;
}

void C_BaseEntity::BaseInterpolatePart2( Vector &oldOrigin, QAngle &oldAngles, int nChangeFlags )
{
	if ( m_vecOrigin != oldOrigin )
	{
		nChangeFlags |= POSITION_CHANGED;
	}

	if( m_angRotation != oldAngles )
	{
		nChangeFlags |= ANGLES_CHANGED;
	}

	if ( nChangeFlags != 0 )
	{
		InvalidatePhysicsRecursive( nChangeFlags );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Default interpolation for entities
// Output : true means entity should be drawn, false means probably not
//-----------------------------------------------------------------------------
bool C_BaseEntity::Interpolate( float currentTime )
{
	VPROF( "C_BaseEntity::Interpolate" );

	Vector oldOrigin;
	QAngle oldAngles;

	int bNoMoreChanges;
	int retVal = BaseInterpolatePart1( currentTime, oldOrigin, oldAngles, bNoMoreChanges );

	// If all the Interpolate() calls returned that their values aren't going to
	// change anymore, then get us out of the interpolation list.
	if ( bNoMoreChanges )
		RemoveFromEntityList(ENTITY_LIST_INTERPOLATE);

	if ( retVal == INTERPOLATE_STOP )
		return true;

	int nChangeFlags = 0;
	BaseInterpolatePart2( oldOrigin, oldAngles, nChangeFlags );

	return true;
}

CStudioHdr *C_BaseEntity::OnNewModel()
{
	OnTranslucencyTypeChanged();
	g_pClientLeafSystem->SetModelType( m_hRender );
	InvalidatePhysicsRecursive( BOUNDS_CHANGED | SEQUENCE_CHANGED );
	SetGlobalFadeScale( GetGlobalFadeScale() );

	// Can we use the model fast path?
	const model_t *pModel = GetModel();
	m_bCanUseBrushModelFastPath = pModel && ( modelinfo->GetModelType( pModel ) == mod_brush ) &&
		!modelinfo->ModelHasMaterialProxy( pModel );

	return NULL;
}

void C_BaseEntity::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	return;
}

void C_BaseEntity::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	return;
}

// Above this velocity and we'll assume a warp/teleport
#define MAX_INTERPOLATE_VELOCITY 4000.0f
#define MAX_INTERPOLATE_VELOCITY_PLAYER 1250.0f

//-----------------------------------------------------------------------------
// Purpose: Determine whether entity was teleported ( so we can disable interpolation )
// Input  : *ent - 
// Output : bool
//-----------------------------------------------------------------------------
bool C_BaseEntity::Teleported( void )
{
	// Disable interpolation when hierarchy changes
	if (m_hOldMoveParent != m_hNetworkMoveParent || m_iOldParentAttachment != m_iParentAttachment)
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Is this a submodel of the world ( model name starts with * )?
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsSubModel( void )
{
	if ( model &&
		modelinfo->GetModelType( model ) == mod_brush &&
		modelinfo->GetModelName( model )[0] == '*' )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Create entity lighting effects
//-----------------------------------------------------------------------------
bool C_BaseEntity::CreateLightEffects( void )
{
	dlight_t *dl;

	bool bHasLightEffects = false;
	// Is this for player flashlights only, if so move to linkplayers?
	if ( !IsViewEntity() )
	{
		if (IsEffectActive(EF_BRIGHTLIGHT))
		{
			bHasLightEffects = true;
			dl = effects->CL_AllocDlight ( index );
			dl->origin = GetAbsOrigin();
			dl->origin[2] += 16;
			dl->color.r = dl->color.g = dl->color.b = 250;
			dl->radius = random->RandomFloat(400,431);
			dl->die = gpGlobals->curtime + 0.001;
		}
		if (IsEffectActive(EF_DIMLIGHT))
		{
			bHasLightEffects = true;
			dl = effects->CL_AllocDlight ( index );
			dl->origin = GetAbsOrigin();
			dl->color.r = dl->color.g = dl->color.b = 100;
			dl->radius = random->RandomFloat(200,231);
			dl->die = gpGlobals->curtime + 0.001;
		}
	}
	return bHasLightEffects;
}

void C_BaseEntity::MoveToLastReceivedPosition( bool force )
{
	if ( force || ( !m_bClientSideRagdoll ) )
	{
		SetLocalOrigin( GetNetworkOrigin() );
		SetLocalAngles( GetNetworkAngles() );
	}
}

bool C_BaseEntity::ShouldInterpolate()
{
	if ( IsViewEntity() )
		return true;

	if ( index == 0 || !GetModel() )
		return false;

	// always interpolate if visible
	if ( INVALID_CLIENT_RENDER_HANDLE != m_hRender &&
		!m_VisibilityBits.IsAllClear() )
	{
		return true;
	}

	// if any movement child needs interpolation, we have to interpolate too
	C_BaseEntity *pChild = FirstMoveChild();
	while( pChild )
	{
		if ( pChild->ShouldInterpolate() )	
			return true;

		pChild = pChild->NextMovePeer();
	}

	// don't interpolate
	return false;
}


void C_BaseEntity::ProcessTeleportList()
{
	int iNext;
	for ( int iCur=g_EntityLists[ENTITY_LIST_TELEPORT].Head(); iCur != g_EntityLists[ENTITY_LIST_TELEPORT].InvalidIndex(); iCur=iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_TELEPORT].Next( iCur );
		C_BaseEntity *pCur = g_EntityLists[ENTITY_LIST_TELEPORT].Element(iCur);

		bool teleport = pCur->Teleported();
		bool ef_nointerp = pCur->IsEffectActive(EF_NOINTERP);
	
		if ( teleport || ef_nointerp )
		{
			// Undo the teleport flag..
			pCur->m_hOldMoveParent = pCur->m_hNetworkMoveParent;			
			pCur->m_iOldParentAttachment = pCur->m_iParentAttachment;
			// Zero out all but last update.
			pCur->MoveToLastReceivedPosition( true );
			pCur->ResetLatched();
		}
		else
		{
			// Get it out of the list as soon as we can.
			pCur->RemoveFromEntityList(ENTITY_LIST_TELEPORT);
		}
	}
}

void C_BaseEntity::CheckInterpolatedVarParanoidMeasurement()
{
	// What we're doing here is to check all the entities that were not in the interpolation
	// list and make sure that there's no entity that should be in the list that isn't.
	
#ifdef INTERPOLATEDVAR_PARANOID_MEASUREMENT
	int iHighest = ClientEntityList().GetHighestEntityIndex();
	for ( int i=0; i <= iHighest; i++ )
	{
		C_BaseEntity *pEnt = ClientEntityList().GetBaseEntity( i );
		if ( !pEnt || pEnt->m_ListEntry[ENTITY_LIST_INTERPOLATE] != 0xFFFF || !pEnt->ShouldInterpolate() )
			continue;
		
		// Player angles always generates this error when the console is up.
		if ( pEnt->entindex() == 1 && engine->Con_IsVisible() )
			continue;
			
		// View models tend to screw up this test unnecesarily because they modify origin,
		// angles, and 
		if ( ToBaseViewModel( pEnt ) )
			continue;

		g_bRestoreInterpolatedVarValues = true;
		g_nInterpolatedVarsChanged = 0;
		pEnt->Interpolate( gpGlobals->curtime );
		g_bRestoreInterpolatedVarValues = false;
		
		if ( g_nInterpolatedVarsChanged > 0 )
		{
			static int iWarningCount = 0;
			Warning( "(%d): An entity (%d) should have been in g_InterpolationList.\n", iWarningCount++, pEnt->entindex() );
			break;
		}
	}
#endif
}


void C_BaseEntity::ProcessInterpolatedList()
{
	CheckInterpolatedVarParanoidMeasurement();

	// Interpolate the minimal set of entities that need it.
	int iNext;
	for ( int iCur=g_EntityLists[ENTITY_LIST_INTERPOLATE].Head(); iCur != g_EntityLists[ENTITY_LIST_INTERPOLATE].InvalidIndex(); iCur=iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_INTERPOLATE].Next( iCur );
		C_BaseEntity *pCur = g_EntityLists[ENTITY_LIST_INTERPOLATE].Element(iCur);
		
		pCur->m_bReadyToDraw = pCur->Interpolate( gpGlobals->curtime );
	}
}


//-----------------------------------------------------------------------------
// Returns the aiment render origin + angles
//-----------------------------------------------------------------------------
void C_BaseEntity::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pOrigin, QAngle *pAngles )
{
	// Should be overridden for things that attach to attchment points

	// Slam origin to the origin of the entity we are attached to...
	*pOrigin = pAttachedTo->GetAbsOrigin();
	*pAngles = pAttachedTo->GetAbsAngles();
}


void C_BaseEntity::StopFollowingEntity( )
{
	Assert( IsFollowingEntity() );

	SetParent( NULL );
	RemoveEffects( EF_BONEMERGE );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
}

bool C_BaseEntity::IsFollowingEntity()
{
	return IsEffectActive(EF_BONEMERGE) && (GetMoveType() == MOVETYPE_NONE) && GetMoveParent();
}

C_BaseEntity *CBaseEntity::GetFollowedEntity()
{
	if (!IsFollowingEntity())
		return NULL;
	return GetMoveParent();
}


//-----------------------------------------------------------------------------
// Default implementation for GetTextureAnimationStartTime
//-----------------------------------------------------------------------------
float C_BaseEntity::GetTextureAnimationStartTime()
{
	return m_flSpawnTime;
}


//-----------------------------------------------------------------------------
// Default implementation, indicates that a texture animation has wrapped
//-----------------------------------------------------------------------------
void C_BaseEntity::TextureAnimationWrapped()
{
}


void C_BaseEntity::ClientThink()
{
}

static ConVar hermite( "hermite_fix", "1", FCVAR_DEVELOPMENTONLY, "Don't interpolate previous hermite sample when fixing up times." );

extern bool g_bHermiteFix;

// Defined in engine
static ConVar cl_interpolate( "cl_interpolate", "1", FCVAR_USERINFO | FCVAR_HIDDEN, "Enables or disables interpolation on listen servers or during demo playback" );

// (static function)
void C_BaseEntity::InterpolateServerEntities()
{
	VPROF_BUDGET( "C_BaseEntity::InterpolateServerEntities", VPROF_BUDGETGROUP_INTERPOLATION );

	bool bPrevInterpolate = s_bInterpolate;

	bool bHermiteFix = hermite.GetBool();
	if ( bHermiteFix != g_bHermiteFix )
	{
		g_bHermiteFix = bHermiteFix;
	}

	// Determine whether interpolation is enabled
	bool bInterpolate = cl_interpolate.GetBool();
	if ( !bInterpolate && engine->IsConnected() && !engine->IsPlayingDemo() && !engine->IsClientLocalToActiveServer() )
		bInterpolate = true; // client is connected and not playing demo and not on listen server, force interpolation ON
	s_bInterpolate = bInterpolate;

	// Don't interpolate during timedemo playback or when engine is paused
	if ( engine->IsPlayingTimeDemo() || engine->IsPaused() )
	{										 
		s_bInterpolate = false;
	}

	// Don't interpolate, either, if we are timing out
	INetChannelInfo *nci = engine->GetNetChannelInfo();
	if ( nci && nci->GetTimeSinceLastReceived() > 0.5f )
	{
		s_bInterpolate = false;
	}

	if ( IsSimulatingOnAlternateTicks() != g_bWasSkipping || IsEngineThreaded() != g_bWasThreaded )
	{
		g_bWasSkipping = IsSimulatingOnAlternateTicks();
		g_bWasThreaded = IsEngineThreaded();

		C_BaseEntityIterator iterator;
		C_BaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != NULL )
		{
			pEnt->Interp_UpdateInterpolationAmounts( pEnt->GetVarMapping() );
		}
	}

	// Enable extrapolation?
	CInterpolationContext context;
	context.SetLastTimeStamp( engine->GetLastTimeStamp() );
	if ( cl_extrapolate.GetBool() && !engine->IsPaused() )
	{
		context.EnableExtrapolation( true );
	}

	if ( bPrevInterpolate != s_bInterpolate && !s_bInterpolate )
	{
		// Clear interp history when we disable interpolation
		C_BaseEntityIterator iterator;
		C_BaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != NULL )
		{
			pEnt->ResetLatched();
		}
	}

	// Smoothly interpolate position for server entities.
	ProcessTeleportList();
	ProcessInterpolatedList();
}


// (static function)
void C_BaseEntity::AddVisibleEntities()
{
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	VPROF_BUDGET( "C_BaseEntity::AddVisibleEntities", VPROF_BUDGETGROUP_WORLD_RENDERING );

	// Let non-dormant client created predictables get added, too
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		int c = GetPredictables( hh )->GetPredictableCount();
		for ( int i = 0 ; i < c ; i++ )
		{
			C_BaseEntity *pEnt = GetPredictables( hh )->GetPredictable( i );
			if ( !pEnt )
				continue;

			if ( !pEnt->IsClientCreated() )
				continue;

			// Only draw until it's ack'd since that means a real entity has arrived
			if ( pEnt->m_PredictableID.GetAcknowledged() )
				continue;

			// Don't draw if dormant
			if ( pEnt->IsDormantPredictable() )
				continue;

			pEnt->UpdateVisibility();	
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_BaseEntity::OnPreDataChanged( DataUpdateType_t type )
{
	m_hOldMoveParent = m_hNetworkMoveParent;
	m_iOldParentAttachment = m_iParentAttachment;
}

void C_BaseEntity::OnDataChanged( DataUpdateType_t type )
{
	// Set up shadows; do it here so that objects can change shadowcasting state
	CreateShadow();

	if ( type == DATA_UPDATE_CREATED )
	{
		UpdateVisibility();
	}

	// These may have changed in the network update
	AlphaProp()->SetRenderFX( GetRenderFX(), GetRenderMode() );
	AlphaProp()->SetDesyncOffset( index );

	// Copy in fade parameters
	AlphaProp()->SetFade( m_flFadeScale, m_fadeMinDist, m_fadeMaxDist );
}

ClientThinkHandle_t C_BaseEntity::GetThinkHandle()
{
	return m_hThink;
}


void C_BaseEntity::SetThinkHandle( ClientThinkHandle_t hThink )
{
	m_hThink = hThink;
}


//-----------------------------------------------------------------------------
// Determine the color modulation amount
//-----------------------------------------------------------------------------
void C_BaseEntity::GetColorModulation( float* color )
{
	color[0] = m_clrRender->r / 255.0f;
	color[1] = m_clrRender->g / 255.0f;
	color[2] = m_clrRender->b / 255.0f;
}


//-----------------------------------------------------------------------------
// Returns true if we should add this to the collision list
//-----------------------------------------------------------------------------
CollideType_t C_BaseEntity::GetCollideType( void )
{
	if ( !m_nModelIndex || !model )
		return ENTITY_SHOULD_NOT_COLLIDE;

	if ( !IsSolid( ) )
		return ENTITY_SHOULD_NOT_COLLIDE;

	// If the model is a bsp or studio (i.e. it can collide with the player
	if ( ( modelinfo->GetModelType( model ) != mod_brush ) && ( modelinfo->GetModelType( model ) != mod_studio ) )
		return ENTITY_SHOULD_NOT_COLLIDE;

	// Don't get stuck on point sized entities ( world doesn't count )
	if ( m_nModelIndex != 1 )
	{
		if ( IsPointSized() )
			return ENTITY_SHOULD_NOT_COLLIDE;
	}

	return ENTITY_SHOULD_COLLIDE;
}


//-----------------------------------------------------------------------------
// Is this a brush model?
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsBrushModel() const
{
	int modelType = modelinfo->GetModelType( model );
	return (modelType == mod_brush);
}


//-----------------------------------------------------------------------------
// This method works when we've got a studio model
//-----------------------------------------------------------------------------
void C_BaseEntity::AddStudioDecal( const Ray_t& ray, int hitbox, int decalIndex, 
								  bool doTrace, trace_t& tr, int maxLODToDecal, int nAdditionalDecalFlags )
{
	if (doTrace)
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );

		// Trace the ray against the entity
		if (tr.fraction == 1.0f)
			return;

		// Set the trace index appropriately...
		tr.m_pEnt = this;
	}

	// Exit out after doing the trace so any other effects that want to happen can happen.
	if ( !r_drawmodeldecals.GetBool() )
		return;

	// Found the point, now lets apply the decals
	CreateModelInstance();

	// FIXME: Pass in decal up?
	Vector up(0, 0, 1);

	if (doTrace && (GetSolid() == SOLID_VPHYSICS) && !tr.startsolid && !tr.allsolid)
	{
		// Choose a more accurate normal direction
		// Also, since we have more accurate info, we can avoid pokethru
		Vector temp;
		VectorSubtract( tr.endpos, tr.plane.normal, temp );
		Ray_t betterRay;
		betterRay.Init( tr.endpos, temp );
		modelrender->AddDecal( m_ModelInstance, betterRay, up, decalIndex, GetStudioBody(), true, maxLODToDecal );
	}
	else
	{
		modelrender->AddDecal( m_ModelInstance, ray, up, decalIndex, GetStudioBody(), false, maxLODToDecal );
	}
}

void C_BaseEntity::AddStudioMaterialDecal( const Ray_t& ray, IMaterial *pDecalMaterial, float flInputRadius, Vector vec_up )
{
	
	if ( !r_drawmodeldecals.GetBool() || pDecalMaterial == NULL )
		return;

	// Found the point, now lets apply the decals
	CreateModelInstance();
	modelrender->AddDecal( m_ModelInstance, ray, -vec_up, -1, GetStudioBody(), false, ADDDECAL_TO_ALL_LODS, pDecalMaterial, flInputRadius, flInputRadius );
}

//-----------------------------------------------------------------------------
// This method works when we've got a brush model
//-----------------------------------------------------------------------------
void C_BaseEntity::AddBrushModelDecal( const Ray_t& ray, const Vector& decalCenter, 
									  int decalIndex, bool doTrace, trace_t& tr, const Vector *saxis, int nAdditionalDecalFlags  )
{
	Vector vecNormal;
	if ( doTrace )
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );
		if ( tr.fraction == 1.0f )
			return;
		vecNormal = tr.plane.normal;
	}
	else
	{
		vecNormal = ray.m_Delta;
		VectorNormalize( vecNormal );
		vecNormal *= -1.0f;
	}

	effects->DecalShoot( decalIndex, index, 
		model, GetAbsOrigin(), GetAbsAngles(), decalCenter, saxis, 0, &vecNormal, nAdditionalDecalFlags );
}


//-----------------------------------------------------------------------------
// A method to apply a decal to an entity
//-----------------------------------------------------------------------------
void C_BaseEntity::AddDecal( const Vector& rayStart, const Vector& rayEnd,
		const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal, const Vector *saxis, int nAdditionalDecalFlags )
{
	Ray_t ray;
	ray.Init( rayStart, rayEnd );

	// FIXME: Better bloat?
	// Bloat a little bit so we get the intersection
	ray.m_Delta *= 1.1f;

	int modelType = modelinfo->GetModelType( model );
	switch ( modelType )
	{
	case mod_studio:
		AddStudioDecal( ray, hitbox, decalIndex, doTrace, tr, maxLODToDecal, nAdditionalDecalFlags );
		break;

	case mod_brush:
		AddBrushModelDecal( ray, decalCenter, decalIndex, doTrace, tr, saxis, nAdditionalDecalFlags );
		break;

	default:
		// By default, no collision
		tr.fraction = 1.0f;
		break;
	}
}

//-----------------------------------------------------------------------------
// A method to remove all decals from an entity
//-----------------------------------------------------------------------------
void C_BaseEntity::RemoveAllDecals( void )
{
	// For now, we only handle removing decals from studiomodels
	if ( modelinfo->GetModelType( model ) == mod_studio )
	{
		CreateModelInstance();
		modelrender->RemoveAllDecals( m_ModelInstance );
	}
}

bool C_BaseEntity::SnatchModelInstance( C_BaseEntity *pToEntity )
{
	if ( !modelrender->ChangeInstance(  GetModelInstance(), pToEntity ) )
		return false;  // engine could move modle handle

	// remove old handle from toentity if any
	if ( pToEntity->GetModelInstance() != MODEL_INSTANCE_INVALID )
		 pToEntity->DestroyModelInstance();

	// move the handle to other entity
	pToEntity->SetModelInstance(  GetModelInstance() );

	// delete own reference
	SetModelInstance( MODEL_INSTANCE_INVALID );

	return true;
}

#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// C_BaseEntity new/delete
// All fields in the object are all initialized to 0.
//-----------------------------------------------------------------------------
void *C_BaseEntity::operator new( size_t stAllocateBlock )
{
	Assert( stAllocateBlock != 0 );	
	MEM_ALLOC_CREDIT();
	void *pMem = MemAlloc_AllocAligned( stAllocateBlock, 16 );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *C_BaseEntity::operator new[]( size_t stAllocateBlock )
{
	Assert( stAllocateBlock != 0 );				
	MEM_ALLOC_CREDIT();
	void *pMem = MemAlloc_AllocAligned( stAllocateBlock, 16 );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *C_BaseEntity::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	Assert( stAllocateBlock != 0 );	
	void *pMem = MemAlloc_AllocAlignedFileLine( stAllocateBlock, 16, pFileName, nLine );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *C_BaseEntity::operator new[]( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	Assert( stAllocateBlock != 0 );				
	void *pMem = MemAlloc_AllocAlignedFileLine( stAllocateBlock, 16, pFileName, nLine );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMem - 
//-----------------------------------------------------------------------------
void C_BaseEntity::operator delete( void *pMem )
{
#ifdef _DEBUG
	// set the memory to a known value
	int size = MemAlloc_GetSizeAligned( pMem );
	Q_memset( pMem, 0xdd, size );
#endif

	// get the engine to free the memory
	MemAlloc_FreeAligned( pMem );
}

#include "tier0/memdbgon.h"

//========================================================================================
// TEAM HANDLING
//========================================================================================
C_Team *C_BaseEntity::GetTeam( void )
{
	return GetGlobalTeam( m_iTeamNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseEntity::GetTeamNumber( void ) const
{
	return m_iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseEntity::GetPendingTeamNumber( void ) const
{
	return m_iPendingTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_BaseEntity::GetRenderTeamNumber( void )
{
	return GetTeamNumber();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if these entities are both in at least one team together
//-----------------------------------------------------------------------------
bool C_BaseEntity::InSameTeam( C_BaseEntity *pEntity )
{
	if ( !pEntity )
		return false;

	return ( pEntity->GetTeam() == GetTeam() );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity's on the same team as the local player
//-----------------------------------------------------------------------------
bool C_BaseEntity::InLocalTeam( void )
{
	return ( GetTeam() == GetLocalTeam() );
}


void C_BaseEntity::SetNextClientThink( float nextThinkTime )
{
	Assert( GetClientHandle() != INVALID_CLIENTENTITY_HANDLE );
	ClientThinkList()->SetNextClientThink( GetClientHandle(), nextThinkTime );
}

void C_BaseEntity::AddToLeafSystem()
{
	AddToLeafSystem( IsRenderingWithViewModels() );
}

void C_BaseEntity::AddToLeafSystem( bool bRenderWithViewModels )
{
	m_bRenderWithViewModels = bRenderWithViewModels;
	if( m_hRender == INVALID_CLIENT_RENDER_HANDLE )
	{
		// create new renderer handle
		ClientLeafSystem()->AddRenderable( this, bRenderWithViewModels, ComputeTranslucencyType(), RENDERABLE_MODEL_UNKNOWN_TYPE, ComputeSplitscreenRenderingFlags( this ) );
		ClientLeafSystem()->EnableAlternateSorting( m_hRender, m_bAlternateSorting );
		ClientLeafSystem()->DisableCachedRenderBounds( m_hRender, m_bDisableCachedRenderBounds );
	}
	else
	{
		// handle already exists, just update group & origin
		ClientLeafSystem()->RenderWithViewModels( m_hRender, bRenderWithViewModels );
		// this should already be edge detected, no need to do it per frame
		// ClientLeafSystem()->SetTranslucencyType( m_hRender, ComputeTranslucencyType() );
		ClientLeafSystem()->SetModelType( m_hRender );
		ClientLeafSystem()->DisableCachedRenderBounds( m_hRender, m_bDisableCachedRenderBounds );
	}
	OnFastReflectionRenderingChanged();
	OnDisableShadowDepthRenderingChanged();
	OnDisableCSMRenderingChanged();
	OnShadowDepthRenderingCacheableStateChanged();
}


//-----------------------------------------------------------------------------
// Creates the shadow (if it doesn't already exist) based on shadow cast type
//-----------------------------------------------------------------------------
void C_BaseEntity::CreateShadow()
{
	CBitVec< MAX_SPLITSCREEN_PLAYERS > bvPrevBits;
	bvPrevBits.Copy( m_ShadowBits );
	m_ShadowBits.ClearAll();

	ShadowType_t typeSeen = SHADOWS_NONE;
	ShadowType_t shadowType[ MAX_SPLITSCREEN_PLAYERS ];
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		shadowType[ hh ]= ShadowCastType();
		if ( shadowType[ hh ] != SHADOWS_NONE )
		{
			m_ShadowBits.Set( hh );

			// This check is to make sure that if the shadow gets drawn for an entity that it's always the 
			//  same type for each split screen viewport (or SHADOWS_NONE if not visible in one of the viewports!)
			// For now, we just pick the "best" (highest enum type) shadow
			if ( shadowType[ hh ] > typeSeen  )
			{
				typeSeen = shadowType[ hh ];
			}
		}
	}

	if ( m_ShadowBits.IsAllClear() || 
		!m_ShadowBits.Compare( bvPrevBits ) )
	{
		DestroyShadow();
	}
	
	if ( !m_ShadowBits.IsAllClear() )
	{
		if ( m_ShadowHandle == CLIENTSHADOW_INVALID_HANDLE )
		{
			Assert( typeSeen != SHADOWS_NONE );
			int flags = SHADOW_FLAGS_SHADOW;
			if (typeSeen != SHADOWS_SIMPLE)
				flags |= SHADOW_FLAGS_USE_RENDER_TO_TEXTURE;
			if (typeSeen == SHADOWS_RENDER_TO_TEXTURE_DYNAMIC || typeSeen == SHADOWS_RENDER_TO_TEXTURE_DYNAMIC_CUSTOM)
				flags |= SHADOW_FLAGS_ANIMATING_SOURCE;
			if (typeSeen == SHADOWS_RENDER_TO_TEXTURE_DYNAMIC_CUSTOM)
				flags |= SHADOW_FLAGS_ANIMATING_SOURCE | SHADOW_FLAGS_CUSTOM_DRAW;
			m_ShadowHandle = g_pClientShadowMgr->CreateShadow(GetClientHandle(), entindex(), flags, &m_ShadowBits );
		}
		else
		{
			Assert( m_ShadowBits.Compare( bvPrevBits ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Removes the shadow
//-----------------------------------------------------------------------------
void C_BaseEntity::DestroyShadow()
{
	// NOTE: This will actually cause the shadow type to be recomputed
	// if the entity doesn't immediately go away
	if (m_ShadowHandle != CLIENTSHADOW_INVALID_HANDLE)
	{
		g_pClientShadowMgr->DestroyShadow(m_ShadowHandle);
		m_ShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Removes the entity from the leaf system
//-----------------------------------------------------------------------------
void C_BaseEntity::RemoveFromLeafSystem()
{
	// Detach from the leaf lists.
	if( m_hRender != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->RemoveRenderable( m_hRender );
		m_hRender = INVALID_CLIENT_RENDER_HANDLE;
	}
	DestroyShadow();
}


//-----------------------------------------------------------------------------
// Purpose: Flags this entity as being inside or outside of this client's PVS
//			on the server.
//			NOTE: this is meaningless for client-side only entities.
// Input  : inside_pvs - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetDormant( bool bDormant )
{
	Assert( IsServerEntity() );
	m_bDormant = bDormant;

	// Kill drawing if we became dormant.
	UpdateVisibility();

	ParticleProp()->OwnerSetDormantTo( bDormant );

	OnSetDormant( bDormant );
	cl_entitylist->SetDormant(index, bDormant);
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this entity is dormant. Client/server entities become
//			dormant when they leave the PVS on the server. Client side entities
//			can decide for themselves whether to become dormant.
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsDormant( void ) const
{
	if ( IsServerEntity() )
	{
		return m_bDormant;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Tells the entity that it's about to be destroyed due to the client receiving
// an uncompressed update that's caused it to destroy all entities & recreate them.
//-----------------------------------------------------------------------------
void C_BaseEntity::SetDestroyedOnRecreateEntities( void )
{
	// Robin: We need to destroy all our particle systems immediately, because 
	// we're about to be recreated, and their owner EHANDLEs will match up to 
	// the new entity, but it won't know anything about them.
	ParticleProp()->StopEmissionAndDestroyImmediately();
}

//-----------------------------------------------------------------------------
// These methods recompute local versions as well as set abs versions
//-----------------------------------------------------------------------------
void C_BaseEntity::SetAbsOrigin( const Vector& absOrigin )
{
	// This is necessary to get the other fields of m_rgflCoordinateFrame ok
	CalcAbsolutePosition();

	if ( m_vecAbsOrigin == absOrigin )
		return;

	// All children are invalid, but we are not
	InvalidatePhysicsRecursive( POSITION_CHANGED );
	RemoveEFlags( EFL_DIRTY_ABSTRANSFORM );

	m_vecAbsOrigin = absOrigin;
	MatrixSetColumn( absOrigin, 3, m_rgflCoordinateFrame ); 

	C_BaseEntity *pMoveParent = GetMoveParent();

	if ( !pMoveParent )
	{
		m_vecOrigin = absOrigin;
		return;
	}

	// Moveparent case: transform the abs position into local space
	VectorITransform( absOrigin, pMoveParent->EntityToWorldTransform(), (Vector&)m_vecOrigin );
}

void C_BaseEntity::SetAbsAngles( const QAngle& absAngles )
{
	// This is necessary to get the other fields of m_rgflCoordinateFrame ok
	CalcAbsolutePosition();

	// FIXME: The normalize caused problems in server code like momentary_rot_button that isn't
	//        handling things like +/-180 degrees properly. This should be revisited.
	//QAngle angleNormalize( AngleNormalize( absAngles.x ), AngleNormalize( absAngles.y ), AngleNormalize( absAngles.z ) );

	if ( m_angAbsRotation == absAngles )
		return;

	InvalidatePhysicsRecursive( ANGLES_CHANGED );
	RemoveEFlags( EFL_DIRTY_ABSTRANSFORM );

	m_angAbsRotation = absAngles;
	AngleMatrix( absAngles, m_rgflCoordinateFrame );
	MatrixSetColumn( m_vecAbsOrigin, 3, m_rgflCoordinateFrame ); 

	C_BaseEntity *pMoveParent = GetMoveParent();
	
	if (!pMoveParent)
	{
		m_angRotation = absAngles;
		return;
	}

	// Moveparent case: we're aligned with the move parent
	if ( m_angAbsRotation == pMoveParent->GetAbsAngles() )
	{
		m_angRotation.Init( );
	}
	else
	{
		// Moveparent case: transform the abs transform into local space
		matrix3x4_t worldToParent, localMatrix;
		MatrixInvert( pMoveParent->EntityToWorldTransform(), worldToParent );
		ConcatTransforms( worldToParent, m_rgflCoordinateFrame, localMatrix );
		MatrixAngles( localMatrix, (QAngle &)m_angRotation );
	}
}

void C_BaseEntity::SetAbsVelocity( const Vector &vecAbsVelocity )
{
	if ( m_vecAbsVelocity == vecAbsVelocity )
		return;

	// The abs velocity won't be dirty since we're setting it here
	InvalidatePhysicsRecursive( VELOCITY_CHANGED );
	m_iEFlags &= ~EFL_DIRTY_ABSVELOCITY;

	m_vecAbsVelocity = vecAbsVelocity;

	C_BaseEntity *pMoveParent = GetMoveParent();

	if (!pMoveParent)
	{
		m_vecVelocity = vecAbsVelocity;
		return;
	}

	// First subtract out the parent's abs velocity to get a relative
	// velocity measured in world space
	Vector relVelocity;
	VectorSubtract( vecAbsVelocity, pMoveParent->GetAbsVelocity(), relVelocity );

	// Transform velocity into parent space
	VectorIRotate( relVelocity, pMoveParent->EntityToWorldTransform(), m_vecVelocity );
}

/*
void C_BaseEntity::SetAbsAngularVelocity( const QAngle &vecAbsAngVelocity )
{
	// The abs velocity won't be dirty since we're setting it here
	InvalidatePhysicsRecursive( EFL_DIRTY_ABSANGVELOCITY );
	m_iEFlags &= ~EFL_DIRTY_ABSANGVELOCITY;

	m_vecAbsAngVelocity = vecAbsAngVelocity;

	C_BaseEntity *pMoveParent = GetMoveParent();
	if (!pMoveParent)
	{
		m_vecAngVelocity = vecAbsAngVelocity;
		return;
	}

	// First subtract out the parent's abs velocity to get a relative
	// angular velocity measured in world space
	QAngle relAngVelocity;
	relAngVelocity = vecAbsAngVelocity - pMoveParent->GetAbsAngularVelocity();

	matrix3x4_t entityToWorld;
	AngleMatrix( relAngVelocity, entityToWorld );

	// Moveparent case: transform the abs angular vel into local space
	matrix3x4_t worldToParent, localMatrix;
	MatrixInvert( pMoveParent->EntityToWorldTransform(), worldToParent );
	ConcatTransforms( worldToParent, entityToWorld, localMatrix );
	MatrixAngles( localMatrix, m_vecAngVelocity );
}
*/


// Prevent these for now until hierarchy is properly networked
const Vector& C_BaseEntity::GetLocalOrigin( void ) const
{
	return m_vecOrigin;
}

vec_t C_BaseEntity::GetLocalOriginDim( int iDim ) const
{
	return m_vecOrigin[iDim];
}

// Prevent these for now until hierarchy is properly networked
void C_BaseEntity::SetLocalOrigin( const Vector& origin )
{
	if (m_vecOrigin != origin)
	{
		InvalidatePhysicsRecursive( POSITION_CHANGED );
		m_vecOrigin = origin;
	}
}

void C_BaseEntity::SetLocalOriginDim( int iDim, vec_t flValue )
{
	if (m_vecOrigin[iDim] != flValue)
	{
		InvalidatePhysicsRecursive( POSITION_CHANGED );
		m_vecOrigin[iDim] = flValue;
	}
}


// Prevent these for now until hierarchy is properly networked
const QAngle& C_BaseEntity::GetLocalAngles( void ) const
{
	return m_angRotation;
}

vec_t C_BaseEntity::GetLocalAnglesDim( int iDim ) const
{
	return m_angRotation[iDim];
}

// Prevent these for now until hierarchy is properly networked
void C_BaseEntity::SetLocalAngles( const QAngle& angles )
{
	// NOTE: The angle normalize is a little expensive, but we can save
	// a bunch of time in interpolation if we don't have to invalidate everything
	// and sometimes it's off by a normalization amount

	// FIXME: The normalize caused problems in server code like momentary_rot_button that isn't
	//        handling things like +/-180 degrees properly. This should be revisited.
	//QAngle angleNormalize( AngleNormalize( angles.x ), AngleNormalize( angles.y ), AngleNormalize( angles.z ) );

	if (m_angRotation != angles)
	{
		// This will cause the velocities of all children to need recomputation
		InvalidatePhysicsRecursive( ANGLES_CHANGED );
		m_angRotation = angles;
	}
}

void C_BaseEntity::SetLocalAnglesDim( int iDim, vec_t flValue )
{
	flValue = AngleNormalize( flValue );
	if (m_angRotation[iDim] != flValue)
	{
		// This will cause the velocities of all children to need recomputation
		InvalidatePhysicsRecursive( ANGLES_CHANGED );
		m_angRotation[iDim] = flValue;
	}
}

void C_BaseEntity::SetLocalVelocity( const Vector &vecVelocity )
{
	if (m_vecVelocity != vecVelocity)
	{
		InvalidatePhysicsRecursive( VELOCITY_CHANGED );
		m_vecVelocity = vecVelocity; 
	}
}

void C_BaseEntity::SetLocalAngularVelocity( const QAngle &vecAngVelocity )
{
	if (m_vecAngVelocity != vecAngVelocity)
	{
//		InvalidatePhysicsRecursive( ANG_VELOCITY_CHANGED );
		m_vecAngVelocity = vecAngVelocity;
	}
}


void C_BaseEntity::Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity )
{
	//TODO: Beef this up to work more like the server version.
	Assert( GetPredictable() ); //does this even make sense unless we're predicting the teleportation?
	int iEffects = GetEffects();
	if( newPosition )
	{
		SetNetworkOrigin( *newPosition );
		iEffects |= EF_NOINTERP;
	}
	if( newAngles )
	{
		SetNetworkAngles( *newAngles );
		iEffects |= EF_NOINTERP;
	}
	if( newVelocity )
	{
		SetLocalVelocity( *newVelocity );
		iEffects |= EF_NOINTERP;
	}
	SetEffects( iEffects );
}


//-----------------------------------------------------------------------------
// Sets the local position from a transform
//-----------------------------------------------------------------------------
void C_BaseEntity::SetLocalTransform( const matrix3x4_t &localTransform )
{
	Vector vecLocalOrigin;
	QAngle vecLocalAngles;
	MatrixGetColumn( localTransform, 3, vecLocalOrigin );
	MatrixAngles( localTransform, vecLocalAngles );
	SetLocalOrigin( vecLocalOrigin );
	SetLocalAngles( vecLocalAngles );
}


//-----------------------------------------------------------------------------
// FIXME: REMOVE!!!
//-----------------------------------------------------------------------------
void C_BaseEntity::MoveToAimEnt( )
{
	Vector vecAimEntOrigin;
	QAngle vecAimEntAngles;
	GetAimEntOrigin( GetMoveParent(), &vecAimEntOrigin, &vecAimEntAngles );
	SetAbsOrigin( vecAimEntOrigin );
	SetAbsAngles( vecAimEntAngles );
}


void C_BaseEntity::BoneMergeFastCullBloat( Vector &localMins, Vector &localMaxs, const Vector &thisEntityMins, const Vector &thisEntityMaxs ) const
{
	// By default, we bloat the bbox for fastcull ents by the maximum length it could hang out of the parent bbox,
	// it one corner were touching the edge of the parent's box, and the whole diagonal stretched out.
	float flExpand = (thisEntityMaxs - thisEntityMins).Length();

	localMins.x -= flExpand;
	localMins.y -= flExpand;
	localMins.z -= flExpand;

	localMaxs.x += flExpand;
	localMaxs.y += flExpand;
	localMaxs.z += flExpand;
}


matrix3x4_t& C_BaseEntity::GetParentToWorldTransform( matrix3x4_t &tempMatrix )
{
	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		Assert( false );
		SetIdentityMatrix( tempMatrix );
		return tempMatrix;
	}

	if ( m_iParentAttachment != 0 )
	{
		Vector vOrigin;
		QAngle vAngles;
		if ( pMoveParent->GetAttachment( m_iParentAttachment, vOrigin, vAngles ) )
		{
			AngleMatrix( vAngles, vOrigin, tempMatrix );
			return tempMatrix;
		}
	}
	
	// If we fall through to here, then just use the move parent's abs origin and angles.
	return pMoveParent->EntityToWorldTransform();
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the absolute position of an edict in the world
//			assumes the parent's absolute origin has already been calculated
//-----------------------------------------------------------------------------
void C_BaseEntity::CalcAbsolutePosition( )
{
	// There are periods of time where we're gonna have to live with the
	// fact that we're in an indeterminant state and abs queries (which
	// shouldn't be happening at all; I have assertions for those), will
	// just have to accept stale data.
	if (!s_bAbsRecomputationEnabled)
		return;

	// FIXME: Recompute absbox!!!
	if ((m_iEFlags & EFL_DIRTY_ABSTRANSFORM) == 0)
	{
		// quick check to make sure we really don't need an update
		// Assert( m_pMoveParent || m_vecAbsOrigin == GetLocalOrigin() );
		return;
	}

	AUTO_LOCK( m_CalcAbsolutePositionMutex );

	if ((m_iEFlags & EFL_DIRTY_ABSTRANSFORM) == 0) // need second check in event another thread grabbed mutex and did the calculation
	{
		return;
	}

	RemoveEFlags( EFL_DIRTY_ABSTRANSFORM );

	if (!m_pMoveParent)
	{
		// Construct the entity-to-world matrix
		// Start with making an entity-to-parent matrix
		AngleMatrix( GetLocalAngles(), GetLocalOrigin(), m_rgflCoordinateFrame );
		m_vecAbsOrigin = GetLocalOrigin();
		m_angAbsRotation = GetLocalAngles();
		NormalizeAngles( m_angAbsRotation );
		return;
	}
	
	if ( IsEffectActive(EF_BONEMERGE) )
	{
		MoveToAimEnt();
		return;
	}

	// Construct the entity-to-world matrix
	// Start with making an entity-to-parent matrix
	ALIGN16 matrix3x4_t matEntityToParent ALIGN16_POST;
	AngleMatrix( GetLocalAngles(), matEntityToParent );
	MatrixSetColumn( GetLocalOrigin(), 3, matEntityToParent );

	// concatenate with our parent's transform
	m_pMoveParent->CalcAbsolutePosition();
	ALIGN16 matrix3x4_t scratchMatrix ALIGN16_POST;
	ConcatTransforms( GetParentToWorldTransform( scratchMatrix ), matEntityToParent, m_rgflCoordinateFrame );

	// pull our absolute position out of the matrix
	MatrixGetColumn( m_rgflCoordinateFrame, 3, m_vecAbsOrigin );

	// if we have any angles, we have to extract our absolute angles from our matrix
	if ( m_angRotation == vec3_angle && m_iParentAttachment == 0 )
	{
		// just copy our parent's absolute angles
		VectorCopy( m_pMoveParent->GetAbsAngles(), m_angAbsRotation );
	}
	else
	{
		MatrixAngles( m_rgflCoordinateFrame, m_angAbsRotation );
	}

	// This is necessary because it's possible that our moveparent's CalculateIKLocks will trigger its move children 
	// (ie: this entity) to call GetAbsOrigin(), and they'll use the moveparent's OLD bone transforms to get their attachments
	// since the moveparent is right in the middle of setting up new transforms. 
	//
	// So here, we keep our absorigin invalidated. It means we're returning an origin that is a frame old to CalculateIKLocks,
	// but we'll still render with the right origin.
	if ( m_iParentAttachment != 0 && (m_pMoveParent->GetFlags() & EFL_SETTING_UP_BONES) )
	{
		m_iEFlags |= EFL_DIRTY_ABSTRANSFORM;
	}
}

void C_BaseEntity::CalcAbsoluteVelocity()
{
	if ((m_iEFlags & EFL_DIRTY_ABSVELOCITY ) == 0)
		return;

	AUTO_LOCK( m_CalcAbsoluteVelocityMutex );

	if ((m_iEFlags & EFL_DIRTY_ABSVELOCITY) == 0) // need second check in event another thread grabbed mutex and did the calculation
	{
		return;
	}

	m_iEFlags &= ~EFL_DIRTY_ABSVELOCITY;

	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		m_vecAbsVelocity = m_vecVelocity;
		return;
	}

	VectorRotate( m_vecVelocity, pMoveParent->EntityToWorldTransform(), m_vecAbsVelocity );


	// Add in the attachments velocity if it exists
	if ( m_iParentAttachment != 0 )
	{
		Vector vOriginVel;
		Quaternion vAngleVel;
		if ( pMoveParent->GetAttachmentVelocity( m_iParentAttachment, vOriginVel, vAngleVel ) )
		{
			m_vecAbsVelocity += vOriginVel;
			return;
		}
	}

	// Now add in the parent abs velocity
	m_vecAbsVelocity += pMoveParent->GetAbsVelocity();
}

/*
void C_BaseEntity::CalcAbsoluteAngularVelocity()
{
	if ((m_iEFlags & EFL_DIRTY_ABSANGVELOCITY ) == 0)
		return;

	m_iEFlags &= ~EFL_DIRTY_ABSANGVELOCITY;

	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		m_vecAbsAngVelocity = m_vecAngVelocity;
		return;
	}

	matrix3x4_t angVelToParent, angVelToWorld;
	AngleMatrix( m_vecAngVelocity, angVelToParent );
	ConcatTransforms( pMoveParent->EntityToWorldTransform(), angVelToParent, angVelToWorld );
	MatrixAngles( angVelToWorld, m_vecAbsAngVelocity );

	// Now add in the parent abs angular velocity
	m_vecAbsAngVelocity += pMoveParent->GetAbsAngularVelocity();
}
*/


//-----------------------------------------------------------------------------
// Computes the abs position of a point specified in local space
//-----------------------------------------------------------------------------
void C_BaseEntity::ComputeAbsPosition( const Vector &vecLocalPosition, Vector *pAbsPosition )
{
	C_BaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		*pAbsPosition = vecLocalPosition;
	}
	else
	{
		VectorTransform( vecLocalPosition, pMoveParent->EntityToWorldTransform(), *pAbsPosition );
	}
}


//-----------------------------------------------------------------------------
// Computes the abs position of a point specified in local space
//-----------------------------------------------------------------------------
void C_BaseEntity::ComputeAbsDirection( const Vector &vecLocalDirection, Vector *pAbsDirection )
{
	C_BaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		*pAbsDirection = vecLocalDirection;
	}
	else
	{
		VectorRotate( vecLocalDirection, pMoveParent->EntityToWorldTransform(), *pAbsDirection );
	}
}



//-----------------------------------------------------------------------------
// Mark shadow as dirty 
//-----------------------------------------------------------------------------
void C_BaseEntity::MarkRenderHandleDirty( )
{
	// Invalidate render leaf too
	ClientRenderHandle_t handle = GetRenderHandle();
	if ( handle != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->RenderableChanged( handle );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::ShutdownPredictable( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( GetPredictable() );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		GetPredictables( i )->RemoveFromPredictablesList( this );
	}
	DestroyIntermediateData();
	SetPredictable( false );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Turn entity into something the predicts locally
//-----------------------------------------------------------------------------
void C_BaseEntity::InitPredictable( C_BasePlayer *pOwner )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( !GetPredictable() );
	Assert( pOwner );

	// Mark as predictable
	SetPredictable( true );

	int slot = C_BasePlayer::GetSplitScreenSlotForPlayer( pOwner );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );
	m_nSplitUserPlayerPredictionSlot = slot;
	// Allocate buffers into which we copy data
	AllocateIntermediateData();
	// Add to list of predictables
	GetPredictables( slot )->AddToPredictableList( this );
	// Copy everything from "this" into the original_state_data
	//  object.  Don't care about client local stuff, so pull from slot 0 which

	//  should be empty anyway...
	PostNetworkDataReceived( 0 );

	// Copy original data into all prediction slots, so we don't get an error saying we "mispredicted" any
	//  values which are still at their initial values
	for ( int i = 0; i < MULTIPLAYER_BACKUP; i++ )
	{
		SaveData( "InitPredictable", i, PC_EVERYTHING );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetPredictable( bool state )
{
	m_bPredictable = state;

	// update interpolation times
	Interp_UpdateInterpolationAmounts( GetVarMapping() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetPredictable( void ) const
{
	return m_bPredictable;
}

//-----------------------------------------------------------------------------
// Purpose: Transfer data for intermediate frame to current entity
// Input  : copyintermediate - 
//			last_predicted - 
//-----------------------------------------------------------------------------
void C_BaseEntity::PreEntityPacketReceived( int commands_acknowledged )					
{				
#if !defined( NO_ENTITY_PREDICTION )
	// Don't need to copy intermediate data if server did ack any new commands
	bool copyintermediate = ( commands_acknowledged > 0 ) ? true : false;

	Assert( GetPredictable() );
	Assert( cl_predict->GetInt() );

	// First copy in any intermediate predicted data for non-networked fields
	if ( copyintermediate )
	{
		RestoreData( "PreEntityPacketReceived", commands_acknowledged - 1, PC_NON_NETWORKED_ONLY );
		RestoreData( "PreEntityPacketReceived", SLOT_ORIGINALDATA, PC_NETWORKED_ONLY );
	}
	else
	{
		RestoreData( "PreEntityPacketReceived(no commands ack)", SLOT_ORIGINALDATA, PC_EVERYTHING );
	}

	// At this point the entity has original network data restored as of the last time the 
	// networking was updated, and it has any intermediate predicted values properly copied over
	// Unpacked and OnDataChanged will fill in any changed, networked fields.

	// That networked data will be copied forward into the starting slot for the next prediction round

#endif
}	

//-----------------------------------------------------------------------------
// Purpose: Called every time PreEntityPacket received is called
//  copy any networked data into original_state
// Input  : errorcheck - 
//			last_predicted - 
//-----------------------------------------------------------------------------
void C_BaseEntity::PostEntityPacketReceived( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( GetPredictable() );
	Assert( cl_predict->GetInt() );

	// Save networked fields into "original data" store
	SaveData( "PostEntityPacketReceived", SLOT_ORIGINALDATA, PC_NETWORKED_ONLY );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame after all updating is done
// Input  : errorcheck - 
//			last_predicted - 
//-----------------------------------------------------------------------------
bool C_BaseEntity::PostNetworkDataReceived( int commands_acknowledged )
{
	bool haderrors = false;
#if !defined( NO_ENTITY_PREDICTION )
	Assert( GetPredictable() );

	bool errorcheck = ( commands_acknowledged > 0 ) ? true : false;

	// Store network data into post networking pristine state slot (slot 64) 
	SaveData( "PostNetworkDataReceived", SLOT_ORIGINALDATA, PC_EVERYTHING );

	// Show any networked fields that are different
	bool showthis = cl_showerror.GetInt() >= 2;

	if ( cl_showerror.GetInt() < 0 )
	{
		if ( entindex() == -cl_showerror.GetInt() )
		{
			showthis = true;
		}
		else
		{
			showthis = false;
		}
	}

	if ( errorcheck )
	{
		byte *predicted_state_data = (byte *)GetPredictedFrame( commands_acknowledged - 1 );	
		Assert( predicted_state_data );												
		const byte *original_state_data =  (const byte *)GetOriginalNetworkDataObject();
		Assert( original_state_data );

		CPredictionCopy errorCheckHelper( PC_NETWORKED_ONLY, 
			predicted_state_data, TD_OFFSET_PACKED, 
			original_state_data, TD_OFFSET_PACKED, 
			showthis ? 
				CPredictionCopy::TRANSFERDATA_ERRORCHECK_SPEW : 
				CPredictionCopy::TRANSFERDATA_ERRORCHECK_NOSPEW );
		haderrors = errorCheckHelper.TransferData( "", entindex(), GetPredDescMap() ) > 0 ? true : false;
	}
#endif
	return haderrors;
}

void C_BaseEntity::HandlePredictionError( bool bErrorInThisEntity )
{

}


// Stuff implemented for weapon prediction code
void C_BaseEntity::SetSize( const Vector &vecMin, const Vector &vecMax )
{
	SetCollisionBounds( vecMin, vecMax );
}

//-----------------------------------------------------------------------------
// Purpose: Just look up index
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseEntity::PrecacheModel( const char *name )
{
	return modelinfo->GetModelIndex( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *obj - 
//-----------------------------------------------------------------------------
void C_BaseEntity::Remove( )
{
	if ( IsMarkedForDeletion( ) )
		return;
	AddEFlags( EFL_KILLME );	// Make sure to ignore further calls into here or UTIL_Remove.

	// Nothing for now, if it's a predicted entity, could flag as "delete" or dormant
	if ( GetPredictable() || IsClientCreated() )
	{
		// Make it solid
		AddSolidFlags( FSOLID_NOT_SOLID );
		SetMoveType( MOVETYPE_NONE );
	}

	if ( !s_bImmediateRemovesAllowed )
	{
		AddToEntityList( ENTITY_LIST_DELETE );
		return;
	}

	Release();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetPredictionEligible( void ) const
{
#if !defined( NO_ENTITY_PREDICTION )
	return m_bPredictionEligible;
#else
	return false;
#endif
}


C_BaseEntity* C_BaseEntity::Instance( CBaseHandle hEnt )
{
	return ClientEntityList().GetBaseEntityFromHandle( hEnt );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iEnt - 
// Output : C_BaseEntity
//-----------------------------------------------------------------------------
C_BaseEntity *C_BaseEntity::Instance( int iEnt )
{
	return ClientEntityList().GetBaseEntity( iEnt );
}

#ifdef WIN32
#pragma warning( push )
#include <typeinfo>
#pragma warning( pop )
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_BaseEntity::GetClassname( void )
{
	static char outstr[ 256 ];
	outstr[ 0 ] = 0;
	bool gotname = false;
	ClientClass *pClientClass = GetClientClass();
	if ( pClientClass && pClientClass->m_pMapClassname )
		return pClientClass->m_pMapClassname;

#ifndef NO_ENTITY_PREDICTION
	if ( GetPredDescMap() )
	{
		const char *mapname =  GetClassMap().Lookup( GetPredDescMap()->dataClassName );
		if ( mapname && mapname[ 0 ] ) 
		{
			Q_strncpy( outstr, mapname, sizeof( outstr ) );
			gotname = true;
		}
	}
#endif

	if ( !gotname )
	{
		Q_strncpy( outstr, typeid( *this ).name(), sizeof( outstr ) );
	}

	return outstr;
}

const char *C_BaseEntity::GetDebugName( void )
{
	return GetClassname();
}

//-----------------------------------------------------------------------------
// Purpose: Creates an entity by string name, but does not spawn it
// Input  : *className - 
// Output : C_BaseEntity
//-----------------------------------------------------------------------------
C_BaseEntity *CreateEntityByName( const char *className )
{
	C_BaseEntity *ent = GetClassMap().CreateEntity( className );
	if ( ent )
	{
		return ent;
	}

	Warning( "Can't find factory for entity: %s\n", className );
	return NULL;
}

#ifdef _DEBUG
CON_COMMAND( cl_sizeof, "Determines the size of the specified client class." )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "cl_sizeof <gameclassname>\n" );
		return;
	}

	int size = GetClassMap().GetClassSize( args[ 1 ] );

	Msg( "%s is %i bytes\n", args[ 1 ], size );
}
#endif

CON_COMMAND_F( dlight_debug, "Creates a dlight in front of the player", FCVAR_CHEAT )
{
	dlight_t *el = effects->CL_AllocDlight( 1 );
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;
	Vector start = player->EyePosition();
	Vector forward;
	player->EyeVectors( &forward );
	Vector end = start + forward * MAX_TRACE_LENGTH;
	trace_t tr;
	UTIL_TraceLine( start, end, MASK_SHOT_HULL & (~CONTENTS_GRATE), player, COLLISION_GROUP_NONE, &tr );
	el->origin = tr.endpos - forward * 12.0f;
	el->radius = 200; 
	el->decay = el->radius / 5.0f;
	el->die = gpGlobals->curtime + 5.0f;
	el->color.r = 255;
	el->color.g = 192;
	el->color.b = 64;
	el->color.exponent = 5;

}
//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsClientCreated( void ) const
{
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	if ( m_pPredictionContext != NULL )
	{
		// For now can't be both
		Assert( !GetPredictable() );
		return true;
	}
#endif
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classname - 
//			*module - 
//			line - 
// Output : C_BaseEntity
//-----------------------------------------------------------------------------
C_BaseEntity *C_BaseEntity::CreatePredictedEntityByName( const char *classname, const char *module, int line, bool persist /*= false */ )
{
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	C_BasePlayer *player = C_BaseEntity::GetPredictionPlayer();

	int slot = C_BasePlayer::GetSplitScreenSlotForPlayer( player );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );

	Assert( player );
	Assert( player->m_pCurrentCommand );
	Assert( prediction->InPrediction() );

	C_BaseEntity *ent = NULL;

	// What's my birthday (should match server)
	int command_number	= player->m_pCurrentCommand->command_number;
	// Who's my daddy?
	int player_index	= player->entindex() - 1;

	// Create id/context
	CPredictableId testId;
	testId.Init( player_index, command_number, classname, module, line );

	// If repredicting, should be able to find the entity in the previously created list
	if ( !prediction->IsFirstTimePredicted() )
	{
		// Only find previous instance if entity was created with persist set
		if ( persist )
		{
			ent = FindPreviouslyCreatedEntity( testId );
			if ( ent )
			{
				return ent;
			}
		}

		return NULL;
	}

	// Try to create it
	ent = CreateEntityByName( classname );
	if ( !ent )
	{
		return NULL;
	}

	// It's predictable
	ent->SetPredictionEligible( true );	

	// Set up "shared" id number
	ent->m_PredictableID.SetRaw( testId.GetRaw() );

	// Get a context (mostly for debugging purposes)
	PredictionContext *context			= new PredictionContext;
	context->m_bActive					= true;
	context->m_nCreationCommandNumber	= command_number;
	context->m_nCreationLineNumber		= line;
	context->m_pszCreationModule		= module;

	// Attach to entity
	ent->m_pPredictionContext = context;
	ent->m_nSplitUserPlayerPredictionSlot = slot;

	// Add to client entity list
	ClientEntityList().AddNonNetworkableEntity( ent );

	//  and predictables
	GetPredictables( slot )->AddToPredictableList( ent );

	// Duhhhh..., but might as well be safe
	Assert( !ent->GetPredictable() );
	Assert( ent->IsClientCreated() );

	// Add the client entity to the spatial partition. (Collidable)
	ent->CollisionProp()->CreatePartitionHandle();

	// CLIENT ONLY FOR NOW!!!
	ent->index = -1;

	if ( AddDataChangeEvent( ent, DATA_UPDATE_CREATED, &ent->m_DataChangeEventRef ) )
	{
		ent->OnPreDataChanged( DATA_UPDATE_CREATED );
	}

	ent->Interp_UpdateInterpolationAmounts( ent->GetVarMapping() );
	
	return ent;
#else
	return NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called each packet that the entity is created on and finally gets called after the next packet
//  that doesn't have a create message for the "parent" entity so that the predicted version
//  can be removed.  Return true to delete entity right away.
//-----------------------------------------------------------------------------
bool C_BaseEntity::OnPredictedEntityRemove( bool isbeingremoved, C_BaseEntity *predicted )
{
#if !defined( NO_ENTITY_PREDICTION ) && defined( USE_PREDICTABLEID )
	// Nothing right now, but in theory you could look at the error in origins and set
	//  up something to smooth out the error
	PredictionContext *ctx = predicted->m_pPredictionContext;
	Assert( ctx );
	if ( ctx )
	{
		// Create backlink to actual entity
		ctx->m_hServerEntity = this;

		/*
		Msg( "OnPredictedEntity%s:  %s created %s(%i) instance(%i)\n",
			isbeingremoved ? "Remove" : "Acknowledge",
			predicted->GetClassname(),
			ctx->m_pszCreationModule,
			ctx->m_nCreationLineNumber,
			predicted->m_PredictableID.GetInstanceNumber() );
		*/
	}

	// If it comes through with an ID, it should be eligible
	SetPredictionEligible( true );

	// Start predicting simulation forward from here
	CheckInitPredictable( "OnPredictedEntityRemove" );

	// Always mark it dormant since we are the "real" entity now
	predicted->SetDormantPredictable( true );

	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );

	// By default, signal that it should be deleted right away
	// If a derived class implements this method, it might chain to here but return
	// false if it wants to keep the dormant predictable around until the chain of
	//  DATA_UPDATE_CREATED messages passes
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetOwnerEntity( C_BaseEntity *pOwner )
{
	m_hOwnerEntity = pOwner;
}

//-----------------------------------------------------------------------------
// Purpose: Put the entity in the specified team
//-----------------------------------------------------------------------------
void C_BaseEntity::ChangeTeam( int iTeamNum )
{
	m_iTeamNum = iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : name - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetModelName( string_t name )
{
	m_ModelName = name;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : string_t
//-----------------------------------------------------------------------------
string_t C_BaseEntity::GetModelName( void ) const
{
	return m_ModelName;
}

//-----------------------------------------------------------------------------
// Purpose: Nothing yet, could eventually supercede Term()
//-----------------------------------------------------------------------------
void C_BaseEntity::UpdateOnRemove( void )
{
	VPhysicsDestroyObject(); 

	Assert( !GetMoveParent() );
	UnlinkFromHierarchy();
	SetGroundEntity( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : canpredict - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetPredictionEligible( bool canpredict )
{
#if !defined( NO_ENTITY_PREDICTION )
	m_bPredictionEligible = canpredict;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns a value that scales all damage done by this entity.
//-----------------------------------------------------------------------------
float C_BaseEntity::GetAttackDamageScale( void )
{
	float flScale = 1;
// Not hooked up to prediction yet
#if 0
	FOR_EACH_LL( m_DamageModifiers, i )
	{
		if ( !m_DamageModifiers[i]->IsDamageDoneToMe() )
		{
			flScale *= m_DamageModifiers[i]->GetModifier();
		}
	}
#endif
	return flScale;
}

#if !defined( NO_ENTITY_PREDICTION )
//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsDormantPredictable( void ) const
{
	return m_bDormantPredictable;
}
#endif
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dormant - 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetDormantPredictable( bool dormant )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( IsClientCreated() );

	m_bDormantPredictable = true;
	m_nIncomingPacketEntityBecameDormant = prediction->GetIncomingPacketNumber();

// Do we need to do the following kinds of things?
#if 0
	// Remove from collisions
	SetSolid( SOLID_NOT );
	// Don't render
	AddEffects( EF_NODRAW );
#endif
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Used to determine when a dorman client predictable can be safely deleted
//  Note that it can be deleted earlier than this by OnPredictedEntityRemove returning true
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::BecameDormantThisPacket( void ) const
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( IsDormantPredictable() );

	if ( m_nIncomingPacketEntityBecameDormant != prediction->GetIncomingPacketNumber() )
		return false;

	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsIntermediateDataAllocated( void ) const
{
#if !defined( NO_ENTITY_PREDICTION )
	return m_pOriginalData != NULL ? true : false;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::AllocateIntermediateData( void )
{				
#if !defined( NO_ENTITY_PREDICTION )
	if ( m_pOriginalData )
		return;
	size_t allocsize = GetIntermediateDataSize();
	Assert( allocsize > 0 );

	m_pOriginalData = new unsigned char[ allocsize ];
	Q_memset( m_pOriginalData, 0, allocsize );
	for ( int i = 0; i < ARRAYSIZE( m_pIntermediateData ); i++ )
	{
		m_pIntermediateData[ i ] = new unsigned char[ allocsize ];
		Q_memset( m_pIntermediateData[ i ], 0, allocsize );
	}

	if( !physenv || physenv->IsPredicted() ) //either predicted physics or don't know if we're predicting physics
	{
		for ( int i = 0; i < ARRAYSIZE( m_pIntermediateData_FirstPredicted ); i++ )
		{
			m_pIntermediateData_FirstPredicted[i] = new unsigned char[ allocsize ];
			Q_memset( m_pIntermediateData_FirstPredicted[ i ], 0, allocsize );
		}

		m_nIntermediateData_FirstPredictedShiftMarker = -1;
	}

	m_nIntermediateDataCount = -1;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::DestroyIntermediateData( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	if ( !m_pOriginalData )
		return;
	for ( int i = 0; i < ARRAYSIZE( m_pIntermediateData ); i++ )
	{
		delete[] m_pIntermediateData[ i ];
		m_pIntermediateData[ i ] = NULL;
	}

	if( m_pIntermediateData_FirstPredicted[0] != NULL )
	{
		for ( int i = 0; i < ARRAYSIZE( m_pIntermediateData_FirstPredicted ); i++ )
		{
			delete[] m_pIntermediateData_FirstPredicted[ i ];
			m_pIntermediateData_FirstPredicted[ i ] = NULL;
		}		
	}

	delete[] m_pOriginalData;
	m_pOriginalData = NULL;

	m_nIntermediateDataCount = -1;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove - 
//			number_of_commands_run - 
//-----------------------------------------------------------------------------
void C_BaseEntity::ShiftIntermediateDataForward( int slots_to_remove, int number_of_commands_run )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( m_pIntermediateData );
	if ( !m_pIntermediateData )
		return;

	Assert( number_of_commands_run >= slots_to_remove );

	// Just moving pointers, yeah
	byte *saved[ ARRAYSIZE( m_pIntermediateData ) ];

	// Remember first slots
	int i = 0;
	for ( ; i < slots_to_remove; i++ )
	{
		saved[ i ] = m_pIntermediateData[ i ];
	}

	// Move rest of slots forward up to last slot
	for ( ; i < number_of_commands_run; i++ )
	{
		m_pIntermediateData[ i - slots_to_remove ] = m_pIntermediateData[ i ];
	}

	// Put remembered slots onto end
	for ( i = 0; i < slots_to_remove; i++ )
	{
		int slot = number_of_commands_run - slots_to_remove + i;

		m_pIntermediateData[ slot ] = saved[ i ];
	}

	m_nIntermediateDataCount -= slots_to_remove;
	if( m_nIntermediateDataCount < -1 )
	{
		m_nIntermediateDataCount = -1;
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove -  
//-----------------------------------------------------------------------------
void C_BaseEntity::ShiftFirstPredictedIntermediateDataForward( int slots_to_remove )
{
#if !defined( NO_ENTITY_PREDICTION )
	if ( !m_pIntermediateData_FirstPredicted[0] || m_nIntermediateData_FirstPredictedShiftMarker == -1 )
		return;

	if( m_nIntermediateData_FirstPredictedShiftMarker <= slots_to_remove ) //acknowledged more commands than we predicted, early out
	{
		m_nIntermediateData_FirstPredictedShiftMarker = 0;
		return;
	}

	// Just moving pointers, yeah
	byte *saved_FirstPredicted[ ARRAYSIZE( m_pIntermediateData_FirstPredicted ) ];

	// Remember first slots
	int i = 0;
	for ( ; i < slots_to_remove; i++ )
	{
		saved_FirstPredicted[ i ] = m_pIntermediateData_FirstPredicted[ i ];
	}

	// Move rest of slots forward up to last slot
	for ( ; i <= m_nIntermediateData_FirstPredictedShiftMarker; i++ )
	{
		m_pIntermediateData_FirstPredicted[ i - slots_to_remove ] = m_pIntermediateData_FirstPredicted[ i ];
	}

	int iEndBase = (m_nIntermediateData_FirstPredictedShiftMarker + 1) - slots_to_remove;

	Assert( iEndBase >= 0 );

	// Put remembered slots onto end
	for ( i = 0; i < slots_to_remove; i++ )
	{
		m_pIntermediateData_FirstPredicted[ iEndBase + i ] = saved_FirstPredicted[ i ];
	}

	m_nIntermediateData_FirstPredictedShiftMarker -= slots_to_remove;
	
#endif
}


//-----------------------------------------------------------------------------
// Purpose: For a predicted entity that is physically simulated, compensate for the prediction frames being coupled to player commands
//			by shifting them around if there's an unequal number of ticks and player commands executed on the server
// Input  : delta - number of server ticks elapsed minus the number of player commands acknowledged
//			last_slot - the number of valid frames currently stored in m_pIntermediateData
//-----------------------------------------------------------------------------
void C_BaseEntity::ShiftIntermediateData_TickAdjust( int delta, int last_slot )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( m_pIntermediateData );
	if ( !m_pIntermediateData || (last_slot == 0) )
		return;

	//Warning( "C_BaseEntity::ShiftIntermediateData_TickAdjust( %f ) delta: %i  last: %i\n", gpGlobals->curtime, delta, last_slot );

	if( delta > last_slot )
	{
		delta = last_slot;
	}
	else if( delta < -last_slot )
	{
		return; //acknowledged more commands than we predicted. Won't be restoring frames anyway
	}

	//Warning( "\t" );

	size_t allocsize = GetIntermediateDataSize();

#if defined( DBGFLAG_ASSERT ) && 1
	// Remember starting configuration
	byte *debugCheck[ ARRAYSIZE( m_pIntermediateData ) ];
	memcpy( debugCheck, m_pIntermediateData, ARRAYSIZE( m_pIntermediateData ) * sizeof( byte * ) );
#endif

	byte *saved[ ARRAYSIZE( m_pIntermediateData ) ];
	memcpy( saved, m_pIntermediateData, last_slot * sizeof( byte * ) );

	if( delta < 0 ) //more commands acknowledged than ticks run, slots indices should increment in value by negative delta
	{
		int i = 0;
		int iStop = last_slot + delta;
		for( ; i < iStop; ++i )
		{
			//Warning( "%i<-%i,", i - delta, i );
			m_pIntermediateData[i - delta] = saved[i];
		}

		for( ; i < last_slot; ++i )
		{
			//Warning( "%i<-%i,", i - iStop, i );
			m_pIntermediateData[i - iStop] = saved[i];
			memcpy( m_pIntermediateData[i - iStop], saved[0], allocsize ); //make duplicates of the first frame we have available
		}
	}
	else //more ticks run than commands acknowledged, slot indices should decrement by delta
	{
		int i = 0;
		int iStop = last_slot - delta;
		for( ; i < iStop; ++i )
		{
			//Warning( "%i<-%i,", i, i + delta );
			m_pIntermediateData[i] = saved[i + delta];
		}

		for( ; i < last_slot; ++i )
		{
			//Warning( "%i<-%i,", i, i - iStop );
			m_pIntermediateData[i] = saved[i - iStop];
			memcpy( m_pIntermediateData[i], saved[last_slot - 1], allocsize ); //make duplicates of the last frame we have available
		}
	}

	//Warning( "\n" );

#if defined( DBGFLAG_ASSERT ) && 1
	for( int i = 0; i < ARRAYSIZE( m_pIntermediateData ); ++i )
	{
		int j = 0;
		for( ; j < ARRAYSIZE( m_pIntermediateData ); ++j )
		{
			if( m_pIntermediateData[i] == debugCheck[j] )
				break;
		}

		Assert( j != ARRAYSIZE( m_pIntermediateData ) );
	}
#endif

#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : framenumber - 
//-----------------------------------------------------------------------------
void *C_BaseEntity::GetPredictedFrame( int framenumber )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( framenumber >= 0 );

	if ( !m_pOriginalData )
	{
		Assert( 0 );
		return NULL;
	}
	return (void *)m_pIntermediateData[ framenumber % ARRAYSIZE( m_pIntermediateData ) ];
#else
	return NULL;
#endif
}

void *C_BaseEntity::GetFirstPredictedFrame( int framenumber )
{
#if !defined( NO_ENTITY_PREDICTION )
	Assert( framenumber >= 0 );
	Assert( m_pIntermediateData_FirstPredicted[0] != 0 );

	return (void *)m_pIntermediateData_FirstPredicted[ framenumber % ARRAYSIZE( m_pIntermediateData_FirstPredicted ) ];
#else
	return NULL;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Get the range of predicted frames we may restore from in prediction
//-----------------------------------------------------------------------------
void C_BaseEntity::GetUnacknowledgedPredictedFrameRange( int &iStart, int &iEnd )
{
	iStart = MAX( s_nIncomingPacketCommandsAcknowledged, 0 );
	iEnd = m_nIntermediateDataCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void *C_BaseEntity::GetOriginalNetworkDataObject( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	if ( !m_pOriginalData )
	{
		Assert( 0 );
		return NULL;
	}
	return (void *)m_pOriginalData;
#else
	return NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::ComputePackedOffsets( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	datamap_t *map = GetPredDescMap();
	if ( !map || map->m_pOptimizedDataMap )
		return;
	CPredictionCopy::PrepareDataMap( map );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseEntity::GetIntermediateDataSize( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	ComputePackedOffsets();

	const datamap_t *map = GetPredDescMap();

	Assert( map->m_pOptimizedDataMap );

	int size = map->m_nPackedSize;

	Assert( size > 0 );	

	// At least 4 bytes to avoid some really bad stuff
	return MAX( size, 4 );
#else
	return 0;
#endif
}	

// Convenient way to delay removing oneself
void C_BaseEntity::SUB_Remove( void )
{
	if (m_iHealth > 0)
	{
		// this situation can screw up NPCs who can't tell their entity pointers are invalid.
		m_iHealth = 0;
		DevWarning( 2, "SUB_Remove called on entity with health > 0\n");
	}

	Remove( );
}

CBaseEntity *FindEntityInFrontOfLocalPlayer()
{
#if DOTA_DLL
	// Get the entity under our mouse cursor
	return DOTAInput()->GetCrosshairEntity();
#endif

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		// Get the entity under my crosshair
		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,	MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 && tr.DidHitNonWorldEntity() )
		{
			return tr.m_pEnt;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Debug command to wipe the decals off an entity
//-----------------------------------------------------------------------------
static void RemoveDecals_f( void )
{
	CBaseEntity *pHit = FindEntityInFrontOfLocalPlayer();
	if ( pHit )
	{
		pHit->RemoveAllDecals();
	}
}

static ConCommand cl_removedecals( "cl_removedecals", RemoveDecals_f, "Remove the decals from the entity under the crosshair.", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::ClearBBoxVisualization( void )
{
	m_fBBoxVisFlags = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::ToggleBBoxVisualization( int fVisFlags )
{
	if ( m_fBBoxVisFlags & fVisFlags )
	{
		m_fBBoxVisFlags &= ~fVisFlags;
	}
	else
	{
		m_fBBoxVisFlags |= fVisFlags;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void ToggleBBoxVisualization( int fVisFlags, const CCommand &args )
{
	CBaseEntity *pHit;

	int iEntity = -1;
	if ( args.ArgC() >= 2 )
	{
		iEntity = atoi( args[ 1 ] );
	}

	if ( iEntity == -1 )
	{
		pHit = FindEntityInFrontOfLocalPlayer();
	}
	else
	{
		pHit = cl_entitylist->GetBaseEntity( iEntity );
	}

	if ( pHit )
	{
		pHit->ToggleBBoxVisualization( fVisFlags );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
CON_COMMAND_F( cl_ent_bbox, "Displays the client's bounding box for the entity under the crosshair.", FCVAR_CHEAT )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_COLLISION_BOUNDS, args );
}


//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
CON_COMMAND_F( cl_ent_absbox, "Displays the client's absbox for the entity under the crosshair.", FCVAR_CHEAT )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_SURROUNDING_BOUNDS, args );
}


//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
CON_COMMAND_F( cl_ent_rbox, "Displays the client's render box for the entity under the crosshair.", FCVAR_CHEAT )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_RENDER_BOUNDS, args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::DrawBBoxVisualizations( void )
{
	if ( m_fBBoxVisFlags & VISUALIZE_COLLISION_BOUNDS )
	{
		debugoverlay->AddBoxOverlay( CollisionProp()->GetCollisionOrigin(), CollisionProp()->OBBMins(),
			CollisionProp()->OBBMaxs(), CollisionProp()->GetCollisionAngles(), 190, 190, 0, 0, 0.01 );
	}

	if ( m_fBBoxVisFlags & VISUALIZE_SURROUNDING_BOUNDS )
	{
		Vector vecSurroundMins, vecSurroundMaxs;
		CollisionProp()->WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
		debugoverlay->AddBoxOverlay( vec3_origin, vecSurroundMins,
			vecSurroundMaxs, vec3_angle, 0, 255, 255, 0, 0.01 );
	}

	if ( m_fBBoxVisFlags & VISUALIZE_RENDER_BOUNDS || r_drawrenderboxes.GetInt() > 0 )
	{
		Vector vecRenderMins, vecRenderMaxs;

		if ( (r_drawrenderboxes.GetInt() & 0x1) > 0 )
		{
			GetRenderBounds( vecRenderMins, vecRenderMaxs );

			debugoverlay->AddBoxOverlay( GetRenderOrigin(), vecRenderMins, vecRenderMaxs,
				GetRenderAngles(), 255, 0, 255, 0, 0.01 );
		}

		if ( (r_drawrenderboxes.GetInt() & 0x2) > 0 )
		{
			GetRenderBoundsWorldspace( vecRenderMins, vecRenderMaxs );

			// Now draw the AABB 
			debugoverlay->AddBoxOverlay( vec3_origin, vecRenderMins, vecRenderMaxs,
				vec3_angle, 0, 255, 0, 0, 0.01 );
		}
	}
}


//-----------------------------------------------------------------------------
// Sets the render mode
//-----------------------------------------------------------------------------
void C_BaseEntity::SetRenderMode( RenderMode_t nRenderMode, bool bForceUpdate )
{
	if ( nRenderMode != m_nRenderMode )
	{
		m_nRenderMode = nRenderMode;
		m_pClientAlphaProperty->SetRenderFX( GetRenderFX(), nRenderMode );
	}
}

void CBaseEntity::SetRenderFX( RenderFx_t nRenderFX, float flStartTime, float flDuration )
{
	bool bStartTimeUnspecified = ( flStartTime == FLT_MAX );
	if ( nRenderFX != m_nRenderFX || !bStartTimeUnspecified )
	{
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		m_nRenderFX = nRenderFX;
		m_pClientAlphaProperty->SetRenderFX( nRenderFX, GetRenderMode(), flStartTime, flDuration );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Copy from this entity into one of the save slots (original or intermediate)
// Input  : slot - 
//			type - 
//			false - 
//			false - 
//			true - 
//			false - 
//			NULL - 
// Output : int
//-----------------------------------------------------------------------------
void C_BaseEntity::SaveData( const char *context, int slot, int type )
{
#if !defined( NO_ENTITY_PREDICTION )
	VPROF( "C_BaseEntity::SaveData" );

	void *dest = ( slot == SLOT_ORIGINALDATA ) ? GetOriginalNetworkDataObject() : GetPredictedFrame( slot );
	Assert( dest );
	if ( slot != SLOT_ORIGINALDATA )
	{
		// Remember high water mark so that we can detect below if we are reading from a slot not yet predicted into...
		m_nIntermediateDataCount = slot;
	}

	CPredictionCopy copyHelper( type, (byte *)dest, TD_OFFSET_PACKED, (const byte *)this, TD_OFFSET_NORMAL, CPredictionCopy::TRANSFERDATA_COPYONLY );
	copyHelper.TransferData( "C_BaseEntity::SaveData", entindex(), GetPredDescMap() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Restore data from specified slot into current entity
// Input  : slot - 
//			type - 
//			false - 
//			false - 
//			true - 
//			false - 
//			NULL - 
// Output : int
//-----------------------------------------------------------------------------
void C_BaseEntity::RestoreData( const char *context, int slot, int type )
{
#if !defined( NO_ENTITY_PREDICTION )
	VPROF( "C_BaseEntity::RestoreData" );

	const void *src = ( slot == SLOT_ORIGINALDATA ) ? GetOriginalNetworkDataObject() : GetPredictedFrame( slot );
	Assert( src );
	
	// some flags shouldn't be predicted - as we find them, add them to the savedEFlagsMask
	const int savedEFlagsMask = EFL_DIRTY_SHADOWUPDATE | EFL_DIRTY_SPATIAL_PARTITION;
	int savedEFlags = GetEFlags() & savedEFlagsMask;

	CPredictionCopy copyHelper( type, (byte *)this, TD_OFFSET_NORMAL, (const byte *)src, TD_OFFSET_PACKED, CPredictionCopy::TRANSFERDATA_COPYONLY  );
	copyHelper.TransferData( "C_BaseEntity::RestoreData", entindex(), GetPredDescMap() );

	// set non-predicting flags back to their prior state
	RemoveEFlags( savedEFlagsMask );
	AddEFlags( savedEFlags );
#endif
}


void C_BaseEntity::OnPostRestoreData()
{
	// HACK Force recomputation of origin
	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );

	if ( GetMoveParent() )
	{
		AddToAimEntsList();
	}

	// If our model index has changed, then make sure it's reflected in our model pointer.
	if ( GetModel() != modelinfo->GetModel( GetModelIndex() ) )
	{
		MDLCACHE_CRITICAL_SECTION();
		SetModelByIndex( GetModelIndex() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determine approximate velocity based on updates from server
// Input  : vel - 
//-----------------------------------------------------------------------------
void C_BaseEntity::EstimateAbsVelocity( Vector& vel )
{
	if ( C_BasePlayer::IsLocalPlayer( this ) )
	{
		vel = GetAbsVelocity();
		return;
	}

	CInterpolationContext context;
	context.EnableExtrapolation( true );
	m_iv_vecOrigin.GetDerivative_SmoothVelocity( &vel, gpGlobals->curtime, !IsPlayer() ); //disable Hermite interpolation fix for the velocity estimation for players. Fixes bugbait #82165. Limiting to players only to reduce risk of regressions
}

void C_BaseEntity::Interp_Reset( VarMapping_t *map )
{
	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, "reset" );
	int c = map->m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		watcher->Reset( gpGlobals->curtime );
	}
}

void C_BaseEntity::ResetLatched()
{
	if ( IsClientCreated() )
		return;

	Interp_Reset( GetVarMapping() );
}

//-----------------------------------------------------------------------------
// Purpose: Fixme, this needs a better solution
// Input  : flags - 
// Output : float
//-----------------------------------------------------------------------------

static float AdjustInterpolationAmount( C_BaseEntity *pEntity, float baseInterpolation )
{
	if ( cl_interp_npcs.GetFloat() > 0 )
	{
		const float minNPCInterpolationTime = cl_interp_npcs.GetFloat();
		const float minNPCInterpolation = TICK_INTERVAL * ( TIME_TO_TICKS( minNPCInterpolationTime ) + 1 );

		if ( minNPCInterpolation > baseInterpolation )
		{
			while ( pEntity )
			{
				if ( pEntity->IsNPC() )
					return minNPCInterpolation;

				pEntity = pEntity->GetMoveParent();
			}
		}
	}

	return baseInterpolation;
}

//-------------------------------------
float C_BaseEntity::GetInterpolationAmount( int flags )
{
	// If single player server is "skipping ticks" everything needs to interpolate for a bit longer
	int serverTickMultiple = 1;
	if ( IsSimulatingOnAlternateTicks() )
	{
		serverTickMultiple = 2;
	}

	if ( GetPredictable() || IsClientCreated() )
	{
		return TICK_INTERVAL * serverTickMultiple;
	}

	// Always fully interpolate during multi-player or during demo playback...
	if ( ( gpGlobals->maxClients > 1 && gpGlobals->IsRemoteClient() ) || engine->IsPlayingDemo() )
	{
		int numTicksToInterpolate = TIME_TO_TICKS( GetClientInterpAmount() ) + serverTickMultiple;
		return AdjustInterpolationAmount( this, TICKS_TO_TIME( numTicksToInterpolate ) );
	}

	int expandedServerTickMultiple = serverTickMultiple;

	if ( IsAnimatedEveryTick() && IsSimulatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}

	if ( ( flags & LATCH_ANIMATION_VAR ) && IsAnimatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}
	if ( ( flags & LATCH_SIMULATION_VAR ) && IsSimulatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}

	return AdjustInterpolationAmount( this, TICK_INTERVAL * ( TIME_TO_TICKS( GetClientInterpAmount() ) +  serverTickMultiple ) );
}


float C_BaseEntity::GetLastChangeTime( int flags )
{
	if ( GetPredictable() || IsClientCreated() )
	{
		return gpGlobals->curtime;
	}
	
	// make sure not both flags are set, we can't resolve that
	Assert( !( (flags & LATCH_ANIMATION_VAR) && (flags & LATCH_SIMULATION_VAR) ) );
	
	if ( flags & LATCH_ANIMATION_VAR )
	{
		return GetAnimTime();
	}

	if ( flags & LATCH_SIMULATION_VAR )
	{
		float st = GetSimulationTime();
		if ( st == 0.0f )
		{
			return gpGlobals->curtime;
		}
		return st;
	}

	Assert( 0 );

	return gpGlobals->curtime;
}

const Vector& C_BaseEntity::GetPrevLocalOrigin() const
{
	return m_iv_vecOrigin.GetPrev();
}

const QAngle& C_BaseEntity::GetPrevLocalAngles() const
{
	return m_iv_angRotation.GetPrev();
}

//-----------------------------------------------------------------------------
// Simply here for game shared 
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsFloating()
{
	// NOTE: This is only here because it's called by game shared.
	// The server uses it to lower falling impact damage
	return false;
}


BEGIN_DATADESC_NO_BASE( C_BaseEntity )
	DEFINE_FIELD( m_ModelName, FIELD_STRING ),
	DEFINE_FIELD( m_vecAbsOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_angAbsRotation, FIELD_VECTOR ),
	DEFINE_ARRAY( m_rgflCoordinateFrame, FIELD_FLOAT, 12 ), // NOTE: MUST BE IN LOCAL SPACE, NOT POSITION_VECTOR!!! (see CBaseEntity::Restore)
	DEFINE_FIELD( m_fFlags, FIELD_INTEGER ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::ShouldSavePhysics()
{
	return false;
}

//-----------------------------------------------------------------------------
// handler to do stuff before you are saved
//-----------------------------------------------------------------------------
void C_BaseEntity::OnSave()
{
	// Here, we must force recomputation of all abs data so it gets saved correctly
	// We can't leave the dirty bits set because the loader can't cope with it.
	CalcAbsolutePosition();
	CalcAbsoluteVelocity();
}


//-----------------------------------------------------------------------------
// handler to do stuff after you are restored
//-----------------------------------------------------------------------------
void C_BaseEntity::OnRestore()
{
	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );
	
	UpdatePartitionListEntry();
	CollisionProp()->UpdatePartition();

	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// Purpose: Saves the current object out to disk, by iterating through the objects
//			data description hierarchy
// Input  : &save - save buffer which the class data is written to
// Output : int	- 0 if the save failed, 1 on success
//-----------------------------------------------------------------------------
int C_BaseEntity::Save( ISave &save )
{
	// loop through the data description list, saving each data desc block
	int status = SaveDataDescBlock( save, GetDataDescMap() );

	return status;
}

//-----------------------------------------------------------------------------
// Purpose: Recursively saves all the classes in an object, in reverse order (top down)
// Output : int 0 on failure, 1 on success
//-----------------------------------------------------------------------------
int C_BaseEntity::SaveDataDescBlock( ISave &save, datamap_t *dmap )
{
	int nResult = save.WriteAll( this, dmap );
	return nResult;
}

void C_BaseEntity::SetClassname( const char *className )
{
	m_iClassname = MAKE_STRING( className );
}


//-----------------------------------------------------------------------------
// Purpose: Restores the current object from disk, by iterating through the objects
//			data description hierarchy
// Input  : &restore - restore buffer which the class data is read from
// Output : int	- 0 if the restore failed, 1 on success
//-----------------------------------------------------------------------------
int C_BaseEntity::Restore( IRestore &restore )
{
	// loops through the data description list, restoring each data desc block in order
	int status = RestoreDataDescBlock( restore, GetDataDescMap() );

	// NOTE: Do *not* use GetAbsOrigin() here because it will
	// try to recompute m_rgflCoordinateFrame!
	MatrixSetColumn( m_vecAbsOrigin, 3, m_rgflCoordinateFrame );

	// Restablish ground entity
	if ( m_hGroundEntity != NULL )
	{
		m_hGroundEntity->AddEntityToGroundList( this );
	}

	return status;
}

//-----------------------------------------------------------------------------
// Purpose: Recursively restores all the classes in an object, in reverse order (top down)
// Output : int 0 on failure, 1 on success
//-----------------------------------------------------------------------------
int C_BaseEntity::RestoreDataDescBlock( IRestore &restore, datamap_t *dmap )
{
	return restore.ReadAll( this, dmap );
}

//-----------------------------------------------------------------------------
// capabilities
//-----------------------------------------------------------------------------
int C_BaseEntity::ObjectCaps( void ) 
{
	return 0; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : C_AI_BaseNPC
//-----------------------------------------------------------------------------
C_AI_BaseNPC *C_BaseEntity::MyNPCPointer( void )
{
	if ( IsNPC() ) 
	{
		return assert_cast<C_AI_BaseNPC *>(this);
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: For each client (only can be local client in client .dll ) checks the client has disabled CC and if so, removes them from 
//  the recipient list.
// Input  : filter - 
//-----------------------------------------------------------------------------
void C_BaseEntity::RemoveRecipientsIfNotCloseCaptioning( C_RecipientFilter& filter )
{
	extern ConVar closecaption;
	if ( !closecaption.GetBool() )
	{
		filter.Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : recording - 
// Output : inline void
//-----------------------------------------------------------------------------
void C_BaseEntity::EnableInToolView( bool bEnable )
{
#ifndef NO_TOOLFRAMEWORK
	m_bEnabledInToolView = bEnable;
	UpdateVisibility();
#endif
}

void C_BaseEntity::SetToolRecording( bool recording )
{
#ifndef NO_TOOLFRAMEWORK
	m_bToolRecording = recording;
	if ( m_bToolRecording )
	{
		recordinglist->AddToList( GetClientHandle() );
		OnToolStartRecording();
	}
	else
	{
        recordinglist->RemoveFromList( GetClientHandle() );
	}
#endif
}

bool C_BaseEntity::HasRecordedThisFrame() const
{
#ifndef NO_TOOLFRAMEWORK
	Assert( m_nLastRecordedFrame <= gpGlobals->framecount );
	return m_nLastRecordedFrame == gpGlobals->framecount;
#else
	return false;
#endif
}

void C_BaseEntity::GetToolRecordingState( KeyValues *msg )
{
	Assert( ToolsEnabled() );
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "C_BaseEntity::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );

	C_BaseEntity *pOwner = m_hOwnerEntity;

	static BaseEntityRecordingState_t state;

	state.m_flTime = gpGlobals->curtime;
	state.m_pModelName = modelinfo->GetModelName( GetModel() );
	state.m_nOwner = pOwner ? pOwner->entindex() : -1;
	state.m_fEffects = m_fEffects;
	state.m_bVisible = ShouldDraw() && !IsDormant();
	state.m_bRecordFinalVisibleSample = false;
	state.m_vecRenderOrigin = GetRenderOrigin();
	state.m_vecRenderAngles = GetRenderAngles();
	state.m_numEffects = 0;
	state.m_pEffects = NULL;

	// use EF_NOINTERP if the owner or a hierarchical parent has NO_INTERP
	if ( pOwner && pOwner->IsEffectActive( EF_NOINTERP ) )
	{
		state.m_fEffects |= EF_NOINTERP;
	}
	C_BaseEntity *pParent = GetMoveParent();
	while ( pParent )
	{
		if ( pParent->IsEffectActive( EF_NOINTERP ) )
		{
			state.m_fEffects |= EF_NOINTERP;
			break;
		}

		pParent = pParent->GetMoveParent();
	}

	msg->SetPtr( "baseentity", &state );
}

void C_BaseEntity::CleanupToolRecordingState( KeyValues *msg )
{
}

void C_BaseEntity::RecordToolMessage()
{
	Assert( IsToolRecording() );
	if ( !IsToolRecording() )
		return;

	if ( HasRecordedThisFrame() )
		return;

	KeyValues *msg = new KeyValues( "entity_state" );

	// Post a message back to all IToolSystems
	GetToolRecordingState( msg );
	Assert( (int)GetToolHandle() != 0 );
	ToolFramework_PostToolMessage( GetToolHandle(), msg );
	CleanupToolRecordingState( msg );

	msg->deleteThis();

	m_nLastRecordedFrame = gpGlobals->framecount;
}

// (static function)
void C_BaseEntity::ToolRecordEntities()
{
	VPROF_BUDGET( "C_BaseEntity::ToolRecordEnties", VPROF_BUDGETGROUP_TOOLS );

	if ( !ToolsEnabled() || !clienttools->IsInRecordingMode() )
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	// Let non-dormant client created predictables get added, too
	int c = recordinglist->Count();
	for ( int i = 0 ; i < c ; i++ )
	{
		IClientRenderable *pRenderable = recordinglist->Get( i );
		if ( !pRenderable )
			continue;

		pRenderable->RecordToolMessage();
	}
}

#ifdef _DEBUG
static entity_list_ids_t s_nSuppressChanges = NUM_ENTITY_LISTS;
#endif

bool C_BaseEntity::IsSpottedBy( int nPlayerIndex ) 
{ 
	Assert( nPlayerIndex >= 0 && nPlayerIndex < MAX_PLAYERS );
	if ( nPlayerIndex >= 0 && nPlayerIndex <= MAX_PLAYERS )
	{
		int maskBitIndex = nPlayerIndex;
		int maskIndex = BitVec_Int( maskBitIndex );
		return ( m_bSpottedByMask[ maskIndex ] & BitVec_Bit( maskBitIndex ) ) != 0;
	}
	return false;
}

bool C_BaseEntity::IsSpottedByFriends( int nPlayerIndex )
{
	int nPlayerEntIndex = nPlayerIndex + 1;

	CBasePlayer* pThisPlayer = UTIL_PlayerByIndex( nPlayerEntIndex );
	if ( !pThisPlayer )
		return false;

	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && 
			 pPlayer != pThisPlayer && 
			 pThisPlayer->GetTeamNumber() == pPlayer->GetTeamNumber() && 
			 IsSpottedBy( i ) )
		{
			return true;
		}
	}

	return false;
}

void C_BaseEntity::SetIsSpottedBy( int nPlayerIndex )
{
	if ( engine->IsPlayingDemo() && nPlayerIndex == 64 ) // old demo artifact
	{
		return;
	}

	Assert( nPlayerIndex >= 0 && nPlayerIndex < MAX_PLAYERS );
	if ( nPlayerIndex >= 0 && nPlayerIndex < MAX_PLAYERS )
	{
		int maskBitIndex = nPlayerIndex;
		int maskIndex = BitVec_Int( maskBitIndex );
		m_bSpottedByMask.Set( maskIndex, m_bSpottedByMask.Get( maskIndex ) | BitVec_Bit( maskBitIndex ) );
	}
}

void C_BaseEntity::AddToEntityList( entity_list_ids_t listId )
{
	Assert(listId < NUM_ENTITY_LISTS);
	if ( m_ListEntry[listId] == 0xFFFF )
	{
		m_ListEntry[listId] = g_EntityLists[listId].AddToTail( this );
	}
}

void C_BaseEntity::RemoveFromEntityList( entity_list_ids_t listId )
{
	Assert( s_nSuppressChanges != listId );
	Assert( listId < NUM_ENTITY_LISTS );
	if ( m_ListEntry[listId] != 0xFFFF )
	{
		g_EntityLists[listId].Remove( m_ListEntry[listId] );
		m_ListEntry[listId] = 0xFFFF;
	}
}


void C_BaseEntity::AddVar( void *data, IInterpolatedVar *watcher, int type, bool bSetup )
{
	// Only add it if it hasn't been added yet.
	bool bAddIt = true;
	for ( int i=0; i < m_VarMap.m_Entries.Count(); i++ )
	{
		if ( m_VarMap.m_Entries[i].watcher == watcher )
		{
			if ( (type & EXCLUDE_AUTO_INTERPOLATE) != (watcher->GetType() & EXCLUDE_AUTO_INTERPOLATE) )
			{
				// Its interpolation mode changed, so get rid of it and re-add it.
				RemoveVar( m_VarMap.m_Entries[i].data, true );
			}
			else
			{
				// They're adding something that's already there. No need to re-add it.
				bAddIt = false;
			}
			
			break;	
		}
	}
	
	if ( bAddIt )
	{
		// watchers must have a debug name set
		Assert( watcher->GetDebugName() != NULL );

		VarMapEntry_t map;
		map.data = data;
		map.watcher = watcher;
		map.type = type;
		map.m_bNeedsToInterpolate = true;
		if ( type & EXCLUDE_AUTO_INTERPOLATE )
		{
			m_VarMap.m_Entries.AddToTail( map );
		}
		else
		{
			m_VarMap.m_Entries.AddToHead( map );
			++m_VarMap.m_nInterpolatedEntries;
		}
	}

	if ( bSetup )
	{
		watcher->Setup( data, type );
		watcher->SetInterpolationAmount( GetInterpolationAmount( watcher->GetType() ) );
	}
}


void C_BaseEntity::RemoveVar( void *data, bool bAssert )
{
	for ( int i=0; i < m_VarMap.m_Entries.Count(); i++ )
	{
		if ( m_VarMap.m_Entries[i].data == data )
		{
			if ( !( m_VarMap.m_Entries[i].type & EXCLUDE_AUTO_INTERPOLATE ) )
				--m_VarMap.m_nInterpolatedEntries;

			m_VarMap.m_Entries.Remove( i );
			return;
		}
	}
	if ( bAssert )
	{
		Assert( !"RemoveVar" );
	}
}

void C_BaseEntity::CheckCLInterpChanged()
{
	float flCurValue_Interp = GetClientInterpAmount();
	static float flLastValue_Interp = flCurValue_Interp;

	float flCurValue_InterpNPCs = cl_interp_npcs.GetFloat();
	static float flLastValue_InterpNPCs = flCurValue_InterpNPCs;
	
	if ( flLastValue_Interp != flCurValue_Interp || 
		 flLastValue_InterpNPCs != flCurValue_InterpNPCs  )
	{
		flLastValue_Interp = flCurValue_Interp;
		flLastValue_InterpNPCs = flCurValue_InterpNPCs;
	
		// Tell all the existing entities to update their interpolation amounts to account for the change.
		C_BaseEntityIterator iterator;
		C_BaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != NULL )
		{
			pEnt->Interp_UpdateInterpolationAmounts( pEnt->GetVarMapping() );
		}
	}
}

void C_BaseEntity::DontRecordInTools()
{
#ifndef NO_TOOLFRAMEWORK
	m_bRecordInTools = false;
#endif
}

int C_BaseEntity::GetCreationTick() const
{
	return m_nCreationTick;
}


static CCallQueue s_SimulateEntitiesCallQueue;

CCallQueue *C_BaseEntity::GetSimulateCallQueue( void )
{
	return &s_SimulateEntitiesCallQueue;
}

// static method
void C_BaseEntity::SimulateEntities()
{
	s_bImmediateRemovesAllowed = false;

	if ( !report_cliententitysim.GetBool() )
	{
		int iNext;
		for ( int iCur = g_EntityLists[ENTITY_LIST_SIMULATE].Head(); iCur != g_EntityLists[ENTITY_LIST_SIMULATE].InvalidIndex(); iCur = iNext )
		{
			iNext = g_EntityLists[ENTITY_LIST_SIMULATE].Next( iCur );
			C_BaseEntity *pCur = g_EntityLists[ENTITY_LIST_SIMULATE].Element(iCur);
			if ( pCur->IsEFlagSet( EFL_KILLME ) )
				continue;

#ifdef _DEBUG
			s_nSuppressChanges = ENTITY_LIST_SIMULATE;
#endif
			bool bRemove = !pCur->Simulate();
#ifdef _DEBUG
			s_nSuppressChanges = NUM_ENTITY_LISTS;
#endif
			if ( bRemove )
			{
				pCur->RemoveFromEntityList(ENTITY_LIST_SIMULATE);
			}
		}
	}
	else
	{
		CFastTimer fastTimer;
	
		int iNext;
		for ( int iCur = g_EntityLists[ENTITY_LIST_SIMULATE].Head(); iCur != g_EntityLists[ENTITY_LIST_SIMULATE].InvalidIndex(); iCur = iNext )
		{
			iNext = g_EntityLists[ENTITY_LIST_SIMULATE].Next( iCur );
			C_BaseEntity *pCur = g_EntityLists[ENTITY_LIST_SIMULATE].Element(iCur);
			if ( pCur->IsEFlagSet( EFL_KILLME ) )
				continue;

			fastTimer.Start();
#ifdef _DEBUG
			s_nSuppressChanges = ENTITY_LIST_SIMULATE;
#endif
			bool bRemove = !pCur->Simulate();
#ifdef _DEBUG
			s_nSuppressChanges = NUM_ENTITY_LISTS;
#endif
			if ( bRemove )
			{
				pCur->RemoveFromEntityList(ENTITY_LIST_SIMULATE);
			}
			fastTimer.End();
			Msg( "Entity(%d): %s - %f\n", pCur->entindex(), pCur->GetDebugName(), fastTimer.GetDuration().GetMillisecondsF() );
		}

		// Report only once per turn on.
		report_cliententitysim.SetValue( 0 );
	}

	s_SimulateEntitiesCallQueue.CallQueued();

	s_bImmediateRemovesAllowed = true;
	PurgeRemovedEntities();
}

// static method
void C_BaseEntity::PurgeRemovedEntities()
{
	int iNext;
	for ( int iCur = g_EntityLists[ENTITY_LIST_DELETE].Head(); iCur != g_EntityLists[ENTITY_LIST_DELETE].InvalidIndex(); iCur = iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_DELETE].Next( iCur );
		C_BaseEntity *pCur = g_EntityLists[ENTITY_LIST_DELETE].Element(iCur);
		pCur->Release();
	}
	g_EntityLists[ENTITY_LIST_DELETE].RemoveAll();
}

// static method
// This is the per-viewport setup hook
void C_BaseEntity::PreRenderEntities( int nSplitScreenPlayerSlot )
{
	MDLCACHE_CRITICAL_SECTION();
	int iNext;
	for ( int iCur = g_EntityLists[ENTITY_LIST_PRERENDER].Head(); iCur != g_EntityLists[ENTITY_LIST_PRERENDER].InvalidIndex(); iCur = iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_PRERENDER].Next( iCur );
		C_BaseEntity *pCur = g_EntityLists[ENTITY_LIST_PRERENDER].Element(iCur);

#ifdef _DEBUG
		s_nSuppressChanges = ENTITY_LIST_PRERENDER;
#endif
		bool bRemove = !pCur->PreRender(nSplitScreenPlayerSlot);
#ifdef _DEBUG
		s_nSuppressChanges = NUM_ENTITY_LISTS;
#endif	
		if ( bRemove )
		{
			pCur->RemoveFromEntityList(ENTITY_LIST_PRERENDER);
		}
	}

}


bool C_BaseEntity::PreRender( int nSplitScreenPlayerSlot )
{
	bool bNeedsPrerender = false;

	// Create flashlight effects, etc.
	if ( CreateLightEffects() )
	{
		bNeedsPrerender = true;
	}
	return bNeedsPrerender;
}

bool C_BaseEntity::IsViewEntity() const
{
	return render->IsViewEntity( entindex() );
}

bool C_BaseEntity::IsAbleToHaveFireEffect( void ) const
{
	return !UTIL_IsLowViolence();
}


void C_BaseEntity::SetBlurState( bool bShouldBlur )
{
	if( bShouldBlur != m_bIsBlurred )
	{
		m_bIsBlurred = bShouldBlur;
		OnTranslucencyTypeChanged();
	}
}

bool C_BaseEntity::IsBlurred( void )
{
	return m_bIsBlurred;
}

void C_BaseEntity::OnParseMapDataFinished()
{
}

//-----------------------------------------------------------------------------
// Adjust the number of cell bits
//-----------------------------------------------------------------------------
bool C_BaseEntity::SetCellBits( int cellbits )
{
	if ( m_cellbits == cellbits )
		return false;

	m_cellbits = cellbits;
	m_cellwidth = ( 1 << cellbits );
	return true;
}


bool C_BaseEntity::ShouldRegenerateOriginFromCellBits() const
{
	return true;
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HSCRIPT C_BaseEntity::GetScriptInstance()
{
	if ( !m_hScriptInstance )
	{
		if ( m_iszScriptId == NULL_STRING )
		{
			char *szName = (char *)stackalloc( 1024 );
			g_pScriptVM->GenerateUniqueKey( ( m_iName != NULL_STRING ) ? STRING(GetEntityName()) : GetClassname(), szName, 1024 );
			m_iszScriptId = AllocPooledString( szName );
		}

		m_hScriptInstance = g_pScriptVM->RegisterInstance( GetScriptDesc(), this );
		g_pScriptVM->SetInstanceUniqeId( m_hScriptInstance, STRING(m_iszScriptId) );
	}
	return m_hScriptInstance;
}

bool C_BaseEntity::IsAutoaimTarget( void ) const
{
	return m_bIsAutoaimTarget;
}

//------------------------------------------------------------------------------
void CC_CL_Find_Ent( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: cl_find_ent <substring>\n" );
		return;
	}

	int iCount = 0;
	const char *pszSubString = args[1];
	Msg("Searching for client entities with classname containing substring: '%s'\n", pszSubString );

	C_BaseEntity *ent = NULL;
	while ( (ent = ClientEntityList().NextBaseEntity(ent)) != NULL )
	{
		const char *pszClassname = ent->GetClassname();

		bool bMatches = false;
		if ( pszClassname && pszClassname[0] )
		{
			if ( Q_stristr( pszClassname, pszSubString ) )
			{
				bMatches = true;
			}
		}

		if ( bMatches )
		{
			iCount++;
			Msg("   '%s' (entindex %d) %s \n", pszClassname ? pszClassname : "[NO NAME]", ent->entindex(), ent->IsDormant() ? "(DORMANT)" : "" );
		}
	}

	Msg("Found %d matches.\n", iCount);
}
static ConCommand cl_find_ent("cl_find_ent", CC_CL_Find_Ent, "Find and list all client entities with classnames that contain the specified substring.\nFormat: cl_find_ent <substring>\n", FCVAR_CHEAT);

//------------------------------------------------------------------------------
void CC_CL_Find_Ent_Index( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: cl_find_ent_index <index>\n" );
		return;
	}

	int iIndex = atoi(args[1]);
	C_BaseEntity *ent = ClientEntityList().GetBaseEntity( iIndex );
	if ( ent )
	{
		const char *pszClassname;
		pszClassname = ent->GetClassname();
		Msg("   '%s' (entindex %d) %s \n", pszClassname ? pszClassname : "[NO NAME]", iIndex, ent->IsDormant() ? "(DORMANT)" : "" );
	}
	else
	{
		Msg("Found no entity at %d.\n", iIndex);
	}
}
static ConCommand cl_find_ent_index("cl_find_ent_index", CC_CL_Find_Ent_Index, "Display data for clientside entity matching specified index.\nFormat: cl_find_ent_index <index>\n", FCVAR_CHEAT);
