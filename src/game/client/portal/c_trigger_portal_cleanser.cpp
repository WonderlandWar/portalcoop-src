//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which bumps portal placement. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "c_triggers.h"
#include "c_portal_player.h"
#include "c_weapon_portalgun.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "model_types.h"
#include "rumble_shared.h"

#include "c_trigger_portal_cleanser.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_TriggerPortalCleanser, DT_TriggerPortalCleanser, CTriggerPortalCleanser)
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( trigger_portal_cleanser, C_TriggerPortalCleanser );

C_TriggerPortalCleanser::C_TriggerPortalCleanser()
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TriggerPortalCleanser::Spawn( void )
{	
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Make this trigger touchable for the client
//-----------------------------------------------------------------------------
void C_TriggerPortalCleanser::UpdatePartitionListEntry()
{
#if 1
	if ( !m_bClientSidePredicted )
	{
		BaseClass::UpdatePartitionListEntry();
		return;
	}

	::partition->RemoveAndInsert(
		PARTITION_CLIENT_STATIC_PROPS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES | PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // add
		CollisionProp()->GetPartitionHandle() );
#endif

	BaseClass::UpdatePartitionListEntry();

}


// We only need to remove portals mid-flight because attempting to remove client sided portals just won't work and causes particles and sounds to play too much.
void C_TriggerPortalCleanser::Touch( C_BaseEntity *pOther )
{	
	if (!PassesTriggerFilters(pOther))
	{
		return;
	}

	if (m_bDisabled )
	{
		return;
	}

	if ( pOther->IsPlayer() )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( pOther );

		if ( pPlayer )
		{
			CWeaponPortalgun *pPortalgun = static_cast<CWeaponPortalgun*>( pPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );

			if ( pPortalgun )
			{
				bool bFizzledPortal = false;

				if ( pPortalgun->CanFirePortal1() )
				{
					CProp_Portal *pPortal = pPortalgun->m_hPrimaryPortal;

					// Cancel portals that are still mid flight
					if ( pPortal && pPortal->GetNextThink( s_pDelayedPlacementContext ) > gpGlobals->curtime )
					{
						pPortal->SetContextThink( NULL, gpGlobals->curtime, s_pDelayedPlacementContext ); 
						bFizzledPortal = true;
					}
				}

				if ( pPortalgun->CanFirePortal2() )
				{
					CProp_Portal *pPortal = pPortalgun->m_hSecondaryPortal;

					// Cancel portals that are still mid flight
					if (pPortal && pPortal->GetNextThink(s_pDelayedPlacementContext) > gpGlobals->curtime)
					{
						pPortal->SetContextThink(NULL, gpGlobals->curtime, s_pDelayedPlacementContext);
						bFizzledPortal = true;
					}
				}
				
				if ( bFizzledPortal )
				{
					pPortalgun->SendWeaponAnim( ACT_VM_FIZZLE );
				}
			}
		}
	}
}