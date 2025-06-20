//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "portal_base2d_shared.h"
#include "portal_collideable_enumerator.h"

#define PORTAL_TELEPORTATION_PLANE_OFFSET 7.0f

CPortalCollideableEnumerator::CPortalCollideableEnumerator( const CPortal_Base2D *pAssociatedPortal )
{
	Assert( pAssociatedPortal );
	m_hTestPortal = pAssociatedPortal;

	pAssociatedPortal->GetVectors( &m_vPlaneNormal, NULL, NULL );

	m_ptForward1000 = pAssociatedPortal->GetAbsOrigin();
	m_ptForward1000 += m_vPlaneNormal * PORTAL_TELEPORTATION_PLANE_OFFSET;
	m_fPlaneDist = m_vPlaneNormal.Dot( m_ptForward1000 );

	m_ptForward1000 += m_vPlaneNormal * 1000.0f;

	m_iHandleCount = 0;
}

IterationRetval_t CPortalCollideableEnumerator::EnumElement( IHandleEntity *pHandleEntity )
{
#if defined( CLIENT_DLL )
	IClientEntity *pClientEntity = cl_entitylist->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle() );
	C_BaseEntity *pEnt = pClientEntity ? pClientEntity->GetBaseEntity() : NULL;
#else
	CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#endif

	if( pEnt == NULL ) //I really never thought this would be necessary
		return ITERATION_CONTINUE;

	// SanyaSho: i'm not sure that this is correct way to do this check here because we can't cast CPortal_Base2D to CBaseEntity
	if( pEnt == m_hTestPortal )
		return ITERATION_CONTINUE; //ignore this portal

	/*if( staticpropmgr->IsStaticProp( pHandleEntity ) )
	{
		//we're dealing with a static prop, which unfortunately doesn't have everything I want to use for checking
		
		ICollideable *pCollideable = pEnt->GetCollideable();

		Vector vMins, vMaxs;
		pCollideable->WorldSpaceSurroundingBounds( &vMins, &vMaxs );

		Vector ptTest( (m_vPlaneNormal.x > 0.0f)?(vMaxs.x):(vMins.x),
						(m_vPlaneNormal.y > 0.0f)?(vMaxs.y):(vMins.y),
						(m_vPlaneNormal.z > 0.0f)?(vMaxs.z):(vMins.z) );
	
		float fPtPlaneDist = m_vPlaneNormal.Dot( ptTest ) - m_fPlaneDist;
		if( fPtPlaneDist <= 0.0f )
			return ITERATION_CONTINUE;
	}
	else*/
	{
		//not a static prop, w00t
		CCollisionProperty *pEntityCollision = pEnt->CollisionProp();

		// Ignore 'projected_wall_entity' objects for partial front/behind checks
		bool bIsProjectedWallEntity = FClassnameIs( pEnt, "projected_wall_entity" );

		if( !pEntityCollision->IsSolid() )
			return ITERATION_CONTINUE; //not solid

		Vector ptEntCenter = pEntityCollision->WorldSpaceCenter();

		float fBoundRadius = pEntityCollision->BoundingRadius();
		float fPtPlaneDist = m_vPlaneNormal.Dot( ptEntCenter ) - m_fPlaneDist;

		if( fPtPlaneDist < -fBoundRadius && !bIsProjectedWallEntity )
			return ITERATION_CONTINUE; //object wholly behind the portal

		if( !(fPtPlaneDist > fBoundRadius) && (fPtPlaneDist > -fBoundRadius) && !bIsProjectedWallEntity ) //object is not wholly in front of the portal, but could be partially in front, do more checks
		{
			Vector ptNearest;
			pEntityCollision->CalcNearestPoint( m_ptForward1000, &ptNearest );
			fPtPlaneDist = m_vPlaneNormal.Dot( ptNearest ) - m_fPlaneDist;
			if( fPtPlaneDist < 0.0f )
				return ITERATION_CONTINUE; //closest point was behind the portal plane, we don't want it
		}


	}

	//if we're down here, this entity needs to be added to our enumeration
	Assert( m_iHandleCount < 1024 );
	if( m_iHandleCount < 1024 ) 
		m_pHandles[m_iHandleCount] = pHandleEntity;
	++m_iHandleCount;

	return ITERATION_CONTINUE;
}










