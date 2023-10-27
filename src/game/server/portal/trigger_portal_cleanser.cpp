//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which bumps portal placement. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "triggers.h"
#include "portal_player.h"
#include "weapon_portalgun.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "physobj.h"
#include "portal/weapon_physcannon.h"
#include "model_types.h"
#include "rumble_shared.h"
#include "trigger_portal_cleanser.h"
#include "vehicle_base.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CTriggerPortalCleanser )

// Outputs
DEFINE_OUTPUT( m_OnDissolve, "OnDissolve" ),
DEFINE_OUTPUT( m_OnFizzle, "OnFizzle" ),
DEFINE_OUTPUT( m_OnDissolveBox, "OnDissolveBox" ),
DEFINE_OUTPUT( m_OnDissolveSphere, "OnDissolveSphere" )

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CTriggerPortalCleanser, DT_TriggerPortalCleanser)
END_SEND_TABLE()


LINK_ENTITY_TO_CLASS( trigger_portal_cleanser, CTriggerPortalCleanser );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerPortalCleanser::Spawn( void )
{	
	SetTransmitState( FL_EDICT_PVSCHECK );

	BaseClass::Spawn();

	m_bClientSidePredicted = true;
	
	InitTrigger();
}

// Creates a base entity with model/physics matching the parameter ent.
// Used to avoid higher level functions on a disolving entity, which should be inert
// and not react the way it used to (touches, etc).
// Uses simple physics entities declared in physobj.cpp
CBaseEntity* ConvertToSimpleProp ( CBaseEntity* pEnt )
{
	CBaseEntity *pRetVal = NULL;
	int modelindex = pEnt->GetModelIndex();
	const model_t *model = modelinfo->GetModel( modelindex );
	if ( model && modelinfo->GetModelType(model) == mod_brush )
	{
		pRetVal = CreateEntityByName( "simple_physics_brush" );
	}
	else
	{
		pRetVal = CreateEntityByName( "simple_physics_prop" );
	}

	CBaseAnimating *pRetValAsAnimating = pRetVal->GetBaseAnimating();
	CBaseAnimating *pAnimating = pEnt->GetBaseAnimating();

	if (pAnimating && pRetValAsAnimating)
	{
		//Specific Stuff
		pRetValAsAnimating->m_nSkin = pAnimating->m_nSkin;
		pRetValAsAnimating->m_flModelScale = pAnimating->m_flModelScale;
	}

		pRetVal->KeyValue( "model", STRING(pEnt->GetModelName()) );
		pRetVal->SetAbsOrigin( pEnt->GetAbsOrigin() );
		pRetVal->SetAbsAngles( pEnt->GetAbsAngles() );
		pRetVal->Spawn();
		pRetVal->VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );

	return pRetVal;
}


void CTriggerPortalCleanser::Touch( CBaseEntity *pOther )
{
	if ( !PassesTriggerFilters( pOther ) )
		return;

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
					CProp_Portal *pPortal = CProp_Portal::FindPortal( pPortalgun->m_iPortalLinkageGroupID, false );

					if ( pPortal && pPortal->IsActive() )
					{
						Assert(0); // Just looking for a callstack...
						pPortal->DoFizzleEffect( PORTAL_FIZZLE_KILLED, pPortal->m_iPortalColorSet, false );
						pPortal->Fizzle();
						//pPortal->SetActive(false);
						// HACK HACK! Used to make the gun visually change when going through a cleanser!
						pPortalgun->m_fEffectsMaxSize1 = 50.0f;

						bFizzledPortal = true;
					}

					// Cancel portals that are still mid flight
					if ( pPortal && pPortal->GetNextThink( s_pDelayedPlacementContext ) > gpGlobals->curtime )
					{
						pPortal->SetContextThink( NULL, gpGlobals->curtime, s_pDelayedPlacementContext ); 
						pPortalgun->m_fEffectsMaxSize2 = 50.0f;
						bFizzledPortal = true;
					}
				}

				if ( pPortalgun->CanFirePortal2() )
				{
					CProp_Portal *pPortal = CProp_Portal::FindPortal( pPortalgun->m_iPortalLinkageGroupID, true );

					if ( pPortal && pPortal->IsActive() )
					{
						pPortal->DoFizzleEffect( PORTAL_FIZZLE_KILLED, pPortal->m_iPortalColorSet, false );
						pPortal->Fizzle();
						//pPortal->SetActive(false);
						// HACK HACK! Used to make the gun visually change when going through a cleanser!
						pPortalgun->m_fEffectsMaxSize2 = 50.0f;

						bFizzledPortal = true;
					}
					
					// Cancel portals that are still mid flight
					if ( pPortal && pPortal->GetNextThink( s_pDelayedPlacementContext ) > gpGlobals->curtime )
					{
						pPortal->SetContextThink( NULL, gpGlobals->curtime, s_pDelayedPlacementContext ); 
						pPortalgun->m_fEffectsMaxSize2 = 50.0f;
						bFizzledPortal = true;
					}
				}

				if ( bFizzledPortal )
				{
					pPortalgun->SendWeaponAnim( ACT_VM_FIZZLE );
					pPortalgun->SetLastFiredPortal( 0 );
					m_OnFizzle.FireOutput( pOther, this );
					pPlayer->RumbleEffect( RUMBLE_RPG_MISSILE, 0, RUMBLE_FLAG_RESTART );
				}
			}
		}

		return;
	}

	CBaseAnimating *pBaseAnimating = pOther->GetBaseAnimating();

	if ( pBaseAnimating && !pBaseAnimating->IsDissolving() )
	{
		int i = 0;

		while ( g_pszPortalNonCleansable[ i ] )
		{
			if ( FClassnameIs( pBaseAnimating, g_pszPortalNonCleansable[ i ] ) )
			{
				// Don't dissolve non cleansable objects
				return;
			}

			++i;
		}
		
		//Fixes vehicle crashing
		CPropVehicleDriveable *pVehicle = dynamic_cast<CPropVehicleDriveable*>( pBaseAnimating );
		if (pVehicle)
		{
			CPortal_Player *pPlayer = (CPortal_Player*)pVehicle->GetDriver();

			if (pPlayer)
			pPlayer->LeaveVehicle();
		}


		// The portal weight box, used for puzzles in the portal mod is differentiated by its name
		// always being 'box'. We use special logic when the cleanser dissolves a box so this is a special output for it.
		if ( pBaseAnimating->NameMatches( "box" ) )
		{
			m_OnDissolveBox.FireOutput( pOther, this );
		}

		if ( pBaseAnimating->NameMatches( "sphere" ) )
		{
			m_OnDissolveSphere.FireOutput( pOther, this );
		}

		if ( FClassnameIs( pBaseAnimating, "updateitem2" ) )
		{
			pBaseAnimating->EmitSound( "UpdateItem.Fizzle" );
		}

		Vector vOldVel;
		AngularImpulse vOldAng;
		pBaseAnimating->GetVelocity( &vOldVel, &vOldAng );

		IPhysicsObject* pOldPhys = pBaseAnimating->VPhysicsGetObject();

		if ( pOldPhys && ( pOldPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
		{
			CPortal_Player *pPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pBaseAnimating );
			if( pPlayer )
			{
				// Modify the velocity for held objects so it gets away from the player
				pPlayer->ForceDropOfCarriedPhysObjects( pBaseAnimating );

				pPlayer->GetAbsVelocity();
				vOldVel = pPlayer->GetAbsVelocity() + Vector( pPlayer->EyeDirection2D().x * 4.0f, pPlayer->EyeDirection2D().y * 4.0f, -32.0f );
			}
		}

		// Swap object with an disolving physics model to avoid touch logic
		CBaseEntity *pDisolvingObj = ConvertToSimpleProp( pBaseAnimating );


		if ( pDisolvingObj )
		{
			// Remove old prop, transfer name and children to the new simple prop
			pDisolvingObj->SetName( pBaseAnimating->GetEntityName() );
			UTIL_TransferPoseParameters( pBaseAnimating, pDisolvingObj );
			TransferChildren( pBaseAnimating, pDisolvingObj );
			pDisolvingObj->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
			pBaseAnimating->AddSolidFlags( FSOLID_NOT_SOLID );
			pBaseAnimating->AddEffects( EF_NODRAW );

			IPhysicsObject* pPhys = pDisolvingObj->VPhysicsGetObject();
			if ( pPhys )
			{
				pPhys->EnableGravity( false );

				Vector vVel = vOldVel;
				AngularImpulse vAng = vOldAng;

				// Disolving hurts, damp and blur the motion a little
				vVel *= 0.5f;
				vAng.z += 20.0f;

				pPhys->SetVelocity( &vVel, &vAng );
			}

			pBaseAnimating->AddFlag( FL_DISSOLVING );
			UTIL_Remove( pBaseAnimating );
		}
		
		CBaseAnimating *pDisolvingAnimating = pDisolvingObj->GetBaseAnimating();
		if ( pDisolvingAnimating ) 
		{
			pDisolvingAnimating->Dissolve( "", gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
		}

		m_OnDissolve.FireOutput( pOther, this );
	}
}