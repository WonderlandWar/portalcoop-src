//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Physics cannon
//
//=============================================================================//

#include "cbase.h"
#include "prop_portal_shared.h"
#include "portal_util_shared.h"
#include "gamerules.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "physics.h"
#include "in_buttons.h"
#include "IEffects.h"
#include "debugoverlay_shared.h"
#include "shake.h"
#include "beam_shared.h"
#include "Sprite.h"
#include "portal/weapon_physcannon.h"
#include "physics_saverestore.h"
#include "movevars_shared.h"
#include "weapon_portalbasecombatweapon.h"
#include "vphysics/friction.h"
#include "saverestore_utlvector.h"
#include "portal_gamerules.h"
#include "citadel_effects_shared.h"
#include "model_types.h"
#include "rumble_shared.h"
#include "gamestats.h"


#ifdef GAME_DLL
#include "player.h"
#include "weapon_portalgun.h"
#include "soundent.h"
#include "portal_player.h"
#include "hl2_player.h"
#include "util.h"
#include "ai_basenpc.h"
#include "player_pickup.h"
#include "physics_prop_ragdoll.h"
#include "globalstate.h"
#include "props.h"
#include "te_effect_dispatch.h"
#include "prop_combine_ball.h"
#include "physobj.h"
#include "eventqueue.h"
#include "ai_interactions.h"
#else
#include "c_physicsprop.h"
#include "c_baseplayer.h"
#include "vcollide_parse.h"
#include "c_weapon_portalgun.h"
#include "c_portal_player.h"
#include "cdll_util.h"
#include "c_ai_basenpc.h"
#include "c_props.h"
#include "c_te_effect_dispatch.h"
#include "c_prop_combine_ball.h"
#include "fx.h"
#include "particles_localspace.h"
#include "view.h"
#include "particles_attractor.h"
#include "prediction.h"
#endif

#ifdef CLIENT_DLL
#define CPhysicsProp C_PhysicsProp
#endif

// NVNT haptic utils
#include "haptics/haptic_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *s_pWaitForUpgradeContext = "WaitForUpgrade";

ConVar	g_debug_physcannon( "g_debug_physcannon", "0", FCVAR_REPLICATED );

ConVar physcannon_minforce( "physcannon_minforce", "700", FCVAR_REPLICATED );
ConVar physcannon_maxforce( "physcannon_maxforce", "1500", FCVAR_REPLICATED );
ConVar physcannon_maxmass( "physcannon_maxmass", "250", FCVAR_REPLICATED );
ConVar physcannon_tracelength( "physcannon_tracelength", "250", FCVAR_REPLICATED );
ConVar physcannon_mega_tracelength( "physcannon_mega_tracelength", "850", FCVAR_REPLICATED );
ConVar physcannon_chargetime("physcannon_chargetime", "2", FCVAR_REPLICATED );
ConVar physcannon_pullforce( "physcannon_pullforce", "4000", FCVAR_REPLICATED );
ConVar physcannon_mega_pullforce( "physcannon_mega_pullforce", "8000", FCVAR_REPLICATED );
ConVar physcannon_cone( "physcannon_cone", "0.97", FCVAR_REPLICATED );
ConVar physcannon_ball_cone( "physcannon_ball_cone", "0.997", FCVAR_REPLICATED );
ConVar physcannon_punt_cone( "physcannon_punt_cone", "0.997", FCVAR_REPLICATED );
ConVar player_throwforce( "player_throwforce", "1000", FCVAR_REPLICATED );
ConVar physcannon_dmg_glass( "physcannon_dmg_glass", "15", FCVAR_REPLICATED );

#ifdef CLIENT_DLL
ConVar cl_predicted_grabbing( "cl_predicted_grabbing", "0", FCVAR_CLIENTCMD_CAN_EXECUTE, "-Enable predicted grabbing, disabled for now because it will crash if the held object is held through portals." );
#endif

extern ConVar hl2_normspeed;
extern ConVar hl2_walkspeed;

#define PHYSCANNON_BEAM_SPRITE "sprites/orangelight1.vmt"
#define PHYSCANNON_GLOW_SPRITE "sprites/glow04_noz.vmt"
#define PHYSCANNON_ENDCAP_SPRITE "sprites/orangeflare1.vmt"
#define PHYSCANNON_CENTER_GLOW "sprites/orangecore1.vmt"
#define PHYSCANNON_BLAST_SPRITE "sprites/orangecore2.vmt"
 
#define MEGACANNON_BEAM_SPRITE "sprites/lgtning_noz.vmt"
#define MEGACANNON_GLOW_SPRITE "sprites/blueflare1_noz.vmt"
#define MEGACANNON_ENDCAP_SPRITE "sprites/blueflare1_noz.vmt"
#define MEGACANNON_CENTER_GLOW "effects/fluttercore.vmt"
#define MEGACANNON_BLAST_SPRITE "effects/fluttercore.vmt"

#define MEGACANNON_RAGDOLL_BOOGIE_SPRITE "sprites/lgtning_noz.vmt"

#define	MEGACANNON_MODEL "models/weapons/v_superphyscannon.mdl"
#define	MEGACANNON_SKIN	1

//-----------------------------------------------------------------------------
// Do we have the super-phys gun?
//-----------------------------------------------------------------------------
bool PlayerHasMegaPhysCannon()
{
	return ( PortalGameRules()->MegaPhyscannonActive() == true );
}

// -------------------------------------------------------------------------
//  Physcannon trace filter to handle special cases
// -------------------------------------------------------------------------

class CTraceFilterPhyscannon : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterPhyscannon, CTraceFilterSimple );

	CTraceFilterPhyscannon( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( NULL, collisionGroup ), m_pTraceOwner( passentity ) {	}

	// For this test, we only test against entities (then world brushes afterwards)
	virtual TraceType_t	GetTraceType() const { return TRACE_ENTITIES_ONLY; }

	bool HasContentsGrate( CBaseEntity *pEntity )
	{
		// FIXME: Move this into the GetModelContents() function in base entity

		// Find the contents based on the model type
		int nModelType = modelinfo->GetModelType( pEntity->GetModel() );
		if ( nModelType == mod_studio )
		{
			CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating *>(pEntity);
			if ( pAnim != NULL )
			{
				CStudioHdr *pStudioHdr = pAnim->GetModelPtr();
				if ( pStudioHdr != NULL && (pStudioHdr->contents() & CONTENTS_GRATE) )
					return true;
			}
		}
		else if ( nModelType == mod_brush )
		{
			// Brushes poll their contents differently
			int contents = modelinfo->GetModelContents( pEntity->GetModelIndex() );
			if ( contents & CONTENTS_GRATE )
				return true;
		}

		return false;
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
#ifdef GAME_DLL
		// Only skip ourselves (not things we own)
		if ( pHandleEntity == m_pTraceOwner )
			return false;

		// Get the entity referenced by this handle
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity == NULL )
			return false;

		// Handle grate entities differently
		if ( HasContentsGrate( pEntity ) )
		{
			// See if it's a grabbable physics prop
			CPhysicsProp *pPhysProp = dynamic_cast<CPhysicsProp *>(pEntity);
			if ( pPhysProp != NULL )
				return pPhysProp->CanBePickedUpByPhyscannon();

			// See if it's a grabbable physics prop
			if ( FClassnameIs( pEntity, "prop_physics" ) )
			{
				CPhysicsProp *pPhysProp = dynamic_cast<CPhysicsProp *>(pEntity);
				if ( pPhysProp != NULL )
					return pPhysProp->CanBePickedUpByPhyscannon();

				// Somehow had a classname that didn't match the class!
				Assert(0);
			}
			else if ( FClassnameIs( pEntity, "func_physbox" ) )
			{
				// Must be a moveable physbox
				CPhysBox *pPhysBox = dynamic_cast<CPhysBox *>(pEntity);
				if ( pPhysBox )
					return pPhysBox->CanBePickedUpByPhyscannon();

				// Somehow had a classname that didn't match the class!
				Assert(0);
			}

			// Don't bother with any other sort of grated entity
			return false;
		}

		// Use the default rules
		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
#else
		if (pHandleEntity != m_pTraceOwner)
			return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);

		return false;
#endif
	}

protected:
	const IHandleEntity *m_pTraceOwner;
};

// We want to test against brushes alone
class CTraceFilterOnlyBrushes : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterOnlyBrushes, CTraceFilterSimple );
	CTraceFilterOnlyBrushes( int collisionGroup ) : CTraceFilterSimple( NULL, collisionGroup ) {}
	virtual TraceType_t	GetTraceType() const { return TRACE_WORLD_ONLY; }
};

//-----------------------------------------------------------------------------
// this will hit skip the pass entity, but not anything it owns 
// (lets player grab own grenades)
class CTraceFilterNoOwnerTest : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterNoOwnerTest, CTraceFilterSimple );

	CTraceFilterNoOwnerTest( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( NULL, collisionGroup ), m_pPassNotOwner(passentity)
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( pHandleEntity != m_pPassNotOwner )
			return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );

		return false;
	}

protected:
	const IHandleEntity *m_pPassNotOwner;
};

//-----------------------------------------------------------------------------
// Purpose: Trace a line the special physcannon way!
//-----------------------------------------------------------------------------
void UTIL_PhyscannonTraceLine( const Vector &vecAbsStart, const Vector &vecAbsEnd, CBaseEntity *pTraceOwner, trace_t *pTrace )
{
	// Default to HL2 vanilla
	if ( hl2_episodic.GetBool() == false )
	{
		CTraceFilterNoOwnerTest filter( pTraceOwner, COLLISION_GROUP_NONE );
		UTIL_TraceLine( vecAbsStart, vecAbsEnd, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );
		return;
	}

	// First, trace against entities
	CTraceFilterPhyscannon filter( pTraceOwner, COLLISION_GROUP_NONE );
	UTIL_TraceLine( vecAbsStart, vecAbsEnd, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );

	// If we've hit something, test again to make sure no brushes block us
	if ( pTrace->m_pEnt != NULL )
	{
		trace_t testTrace;
		CTraceFilterOnlyBrushes brushFilter( COLLISION_GROUP_NONE );
		UTIL_TraceLine( pTrace->startpos, pTrace->endpos, MASK_SHOT, &brushFilter, &testTrace );

		// If we hit a brush, replace the trace with that result
		if ( testTrace.fraction < 1.0f || testTrace.startsolid || testTrace.allsolid )
		{
			*pTrace = testTrace;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Trace a hull for the physcannon
//-----------------------------------------------------------------------------
void UTIL_PhyscannonTraceHull( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &vecAbsMins, const Vector &vecAbsMaxs, CBaseEntity *pTraceOwner, trace_t *pTrace )
{
	// Default to HL2 vanilla
	if ( hl2_episodic.GetBool() == false )
	{
		CTraceFilterNoOwnerTest filter( pTraceOwner, COLLISION_GROUP_NONE );
		UTIL_TraceHull( vecAbsStart, vecAbsEnd, vecAbsMins, vecAbsMaxs, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );
		return;
	}

	// First, trace against entities
	CTraceFilterPhyscannon filter( pTraceOwner, COLLISION_GROUP_NONE );
	UTIL_TraceHull( vecAbsStart, vecAbsEnd, vecAbsMins, vecAbsMaxs, (MASK_SHOT|CONTENTS_GRATE), &filter, pTrace );

	// If we've hit something, test again to make sure no brushes block us
	if ( pTrace->m_pEnt != NULL )
	{
		trace_t testTrace;
		CTraceFilterOnlyBrushes brushFilter( COLLISION_GROUP_NONE );
		UTIL_TraceHull( pTrace->startpos, pTrace->endpos, vecAbsMins, vecAbsMaxs, MASK_SHOT, &brushFilter, &testTrace );

		// If we hit a brush, replace the trace with that result
		if ( testTrace.fraction < 1.0f || testTrace.startsolid || testTrace.allsolid )
		{
			*pTrace = testTrace;
		}
	}
}

static void MatrixOrthogonalize( matrix3x4_t &matrix, int column )
{
	Vector columns[3];
	int i;

	for ( i = 0; i < 3; i++ )
	{
		MatrixGetColumn( matrix, i, columns[i] );
	}

	int index0 = column;
	int index1 = (column+1)%3;
	int index2 = (column+2)%3;

	columns[index2] = CrossProduct( columns[index0], columns[index1] );
	columns[index1] = CrossProduct( columns[index2], columns[index0] );
	VectorNormalize( columns[index2] );
	VectorNormalize( columns[index1] );
	MatrixSetColumn( columns[index1], index1, matrix );
	MatrixSetColumn( columns[index2], index2, matrix );
}

#define SIGN(x) ( (x) < 0 ? -1 : 1 )

static QAngle AlignAngles( const QAngle &angles, float cosineAlignAngle )
{
	matrix3x4_t alignMatrix;
	AngleMatrix( angles, alignMatrix );

	// NOTE: Must align z first
	for ( int j = 3; --j >= 0; )
	{
		Vector vec;
		MatrixGetColumn( alignMatrix, j, vec );
		for ( int i = 0; i < 3; i++ )
		{
			if ( fabs(vec[i]) > cosineAlignAngle )
			{
				vec[i] = SIGN(vec[i]);
				vec[(i+1)%3] = 0;
				vec[(i+2)%3] = 0;
				MatrixSetColumn( vec, j, alignMatrix );
				MatrixOrthogonalize( alignMatrix, j );
				break;
			}
		}
	}

	QAngle out;
	MatrixAngles( alignMatrix, out );
	return out;
}


static void TraceCollideAgainstBBox( const CPhysCollide *pCollide, const Vector &start, const Vector &end, const QAngle &angles, const Vector &boxOrigin, const Vector &mins, const Vector &maxs, trace_t *ptr )
{
	physcollision->TraceBox( boxOrigin, boxOrigin + (start-end), mins, maxs, pCollide, start, angles, ptr );

	if ( ptr->DidHit() )
	{
		ptr->endpos = start * (1-ptr->fraction) + end * ptr->fraction;
		ptr->startpos = start;
		ptr->plane.dist = -ptr->plane.dist;
		ptr->plane.normal *= -1;
	}
}
#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: Finds the nearest ragdoll sub-piece to a location and returns it
// Input  : *pTarget - entity that is the potential ragdoll
//			&position - position we're testing against
// Output : IPhysicsObject - sub-object (if any)
//-----------------------------------------------------------------------------
IPhysicsObject *GetRagdollChildAtPosition( CBaseEntity *pTarget, const Vector &position )
{
	// Check for a ragdoll
	if ( dynamic_cast<CRagdollProp*>( pTarget ) == NULL )
		return NULL;

	// Get the root
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pTarget->VPhysicsGetObjectList( pList, ARRAYSIZE( pList ) );
	
	IPhysicsObject *pBestChild = NULL;
	float			flBestDist = 99999999.0f;
	float			flDist;
	Vector			vPos;

	// Find the nearest child to where we're looking
	for ( int i = 0; i < count; i++ )
	{
		pList[i]->GetPosition( &vPos, NULL );
		
		flDist = ( position - vPos ).LengthSqr();

		if ( flDist < flBestDist )
		{
			pBestChild = pList[i];
			flBestDist = flDist;
		}
	}

	// Make this our base now
	pTarget->VPhysicsSwapObject( pBestChild );

	return pTarget->VPhysicsGetObject();
}
#endif
//-----------------------------------------------------------------------------
// Purpose: Computes a local matrix for the player clamped to valid carry ranges
//-----------------------------------------------------------------------------
// when looking level, hold bottom of object 8 inches below eye level
#define PLAYER_HOLD_LEVEL_EYES	-8

// when looking down, hold bottom of object 0 inches from feet
#define PLAYER_HOLD_DOWN_FEET	2

// when looking up, hold bottom of object 24 inches above eye level
#define PLAYER_HOLD_UP_EYES		24

// use a +/-30 degree range for the entire range of motion of pitch
#define PLAYER_LOOK_PITCH_RANGE	30

// player can reach down 2ft below his feet (otherwise he'll hold the object above the bottom)
#define PLAYER_REACH_DOWN_DISTANCE	24

static void ComputePlayerMatrix( CBasePlayer *pPlayer, matrix3x4_t &out )
{
	if ( !pPlayer )
		return;

	QAngle angles = pPlayer->EyeAngles();
	Vector origin = pPlayer->EyePosition();
	
	// 0-360 / -180-180
	//angles.x = init ? 0 : AngleDistance( angles.x, 0 );
	//angles.x = clamp( angles.x, -PLAYER_LOOK_PITCH_RANGE, PLAYER_LOOK_PITCH_RANGE );
	angles.x = 0;

	float feet = pPlayer->GetAbsOrigin().z + pPlayer->WorldAlignMins().z;
	float eyes = origin.z;
	float zoffset = 0;
	// moving up (negative pitch is up)
	if ( angles.x < 0 )
	{
		zoffset = RemapVal( angles.x, 0, -PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_UP_EYES );
	}
	else
	{
		zoffset = RemapVal( angles.x, 0, PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_DOWN_FEET + (feet - eyes) );
	}
	origin.z += zoffset;
	angles.x = 0;
	AngleMatrix( angles, origin, out );
}

BEGIN_SIMPLE_DATADESC(game_shadowcontrol_params_t)

DEFINE_FIELD(targetPosition, FIELD_POSITION_VECTOR),
DEFINE_FIELD(targetRotation, FIELD_VECTOR),
DEFINE_FIELD(maxAngular, FIELD_FLOAT),
DEFINE_FIELD(maxDampAngular, FIELD_FLOAT),
DEFINE_FIELD(maxSpeed, FIELD_FLOAT),
DEFINE_FIELD(maxDampSpeed, FIELD_FLOAT),
DEFINE_FIELD(dampFactor, FIELD_FLOAT),
DEFINE_FIELD(teleportDistance, FIELD_FLOAT),

END_DATADESC()

//-----------------------------------------------------------------------------


BEGIN_SIMPLE_DATADESC(CGrabController)

	DEFINE_EMBEDDED(m_shadow),

	DEFINE_FIELD(m_timeToArrive, FIELD_FLOAT),
	DEFINE_FIELD(m_errorTime, FIELD_FLOAT),
	DEFINE_FIELD(m_error, FIELD_FLOAT),
	DEFINE_FIELD(m_contactAmount, FIELD_FLOAT),
	DEFINE_AUTO_ARRAY(m_savedRotDamping, FIELD_FLOAT),
	DEFINE_AUTO_ARRAY(m_savedMass, FIELD_FLOAT),
	DEFINE_FIELD(m_flLoadWeight, FIELD_FLOAT),
	DEFINE_FIELD(m_bCarriedEntityBlocksLOS, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bIgnoreRelativePitch, FIELD_BOOLEAN),
	DEFINE_FIELD(m_attachedEntity, FIELD_EHANDLE),
	DEFINE_FIELD(m_angleAlignment, FIELD_FLOAT),
	DEFINE_FIELD(m_vecPreferredCarryAngles, FIELD_VECTOR),
	DEFINE_FIELD(m_bHasPreferredCarryAngles, FIELD_BOOLEAN),
	DEFINE_FIELD(m_flDistanceOffset, FIELD_FLOAT),
	DEFINE_FIELD(m_attachedAnglesPlayerSpace, FIELD_VECTOR),
	DEFINE_FIELD(m_attachedPositionObjectSpace, FIELD_VECTOR),
	DEFINE_FIELD(m_bAllowObjectOverhead, FIELD_BOOLEAN),

	// Physptrs can't be inside embedded classes
	// DEFINE_PHYSPTR( m_controller ),

END_DATADESC()

BEGIN_NETWORK_TABLE_NOBASE( CGrabController, DT_GrabController)
END_NETWORK_TABLE()

const float DEFAULT_MAX_ANGULAR = 360.0f * 10.0f;
const float REDUCED_CARRY_MASS = 1.0f;

CGrabController::CGrabController( void )
{
	m_shadow.dampFactor = 1.0;
	m_shadow.teleportDistance = 0;
	m_errorTime = 0;
	m_error = 0;
	// make this controller really stiff!
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;
	m_shadow.maxDampSpeed = m_shadow.maxSpeed*2;
	m_shadow.maxDampAngular = m_shadow.maxAngular;
	m_attachedEntity = NULL;
	m_vecPreferredCarryAngles = vec3_angle;
	m_bHasPreferredCarryAngles = false;
	m_flDistanceOffset = 0;
	// NVNT constructing m_pControllingPlayer to NULL
	m_pControllingPlayer = NULL;
}

CGrabController::~CGrabController( void )
{
	DetachEntity( false );
}

void CGrabController::OnRestore()
{
	if ( m_controller )
	{
		m_controller->SetEventHandler( this );
	}
}

void CGrabController::SetTargetPosition( const Vector &target, const QAngle &targetOrientation )
{
	m_shadow.targetPosition = target;
	m_shadow.targetRotation = targetOrientation;

	m_timeToArrive = gpGlobals->frametime;

	CBaseEntity *pAttached = GetAttached();
	if ( pAttached )
	{
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
		Assert(pObj);
		if ( pObj != NULL )
		{
			pObj->Wake();
		}
		else
		{
			DetachEntity( false );
		}
	}
}

void CGrabController::GetTargetPosition( Vector *target, QAngle *targetOrientation )
{
	if ( target )
		*target = m_shadow.targetPosition;

	if ( targetOrientation )
		*targetOrientation = m_shadow.targetRotation;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CGrabController::ComputeError()
{
	if ( m_errorTime <= 0 )
		return 0;

	CBaseEntity *pAttached = GetAttached();
	if ( pAttached )
	{
		Vector pos;
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
		if ( pObj )
		{	
			pObj->GetShadowPosition( &pos, NULL );

			float error = (m_shadow.targetPosition - pos).Length();
			if ( m_errorTime > 0 )
			{
				if ( m_errorTime > 1 )
				{
					m_errorTime = 1;
				}
				float speed = error / m_errorTime;
				if ( speed > m_shadow.maxSpeed )
				{
					error *= 0.5;
				}
				m_error = (1-m_errorTime) * m_error + error * m_errorTime;
			}
		}
		else
		{
			DevMsg( "Object attached to Physcannon has no physics object\n" );
			DetachEntity( false );
			return 9999; // force detach
		}
	}
	
	if ( pAttached->IsEFlagSet( EFL_IS_BEING_LIFTED_BY_BARNACLE ) )
	{
		m_error *= 3.0f;
	}

	// If held across a portal but not looking at the portal multiply error
#ifndef CLIENTSHOULDNOTSEEPHYSCANNON
	CPortal_Player *pPortalPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pAttached );
#ifdef GAME_DLL
	if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
#else
	if ( pPortalPlayer && pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
#endif
	{
		Vector forward, right, up;
		QAngle playerAngles = pPortalPlayer->EyeAngles();

		float pitch = AngleDistance(playerAngles.x,0);
		playerAngles.x = clamp( pitch, -75, 75 );
		AngleVectors( playerAngles, &forward, &right, &up );

		Vector start = pPortalPlayer->Weapon_ShootPosition();

		// If the player is upside down then we need to hold the box closer to their feet.
		if ( up.z < 0.0f )
			start += pPortalPlayer->GetViewOffset() * up.z;
		if ( right.z < 0.0f )
			start += pPortalPlayer->GetViewOffset() * right.z;

		Ray_t rayPortalTest;
		rayPortalTest.Init( start, start + forward * 256.0f );

		if ( UTIL_IntersectRayWithPortal( rayPortalTest, pPortalPlayer->GetHeldObjectPortal() ) < 0.0f )
		{
			m_error *= 2.5f;
		}
	}
#endif
	m_errorTime = 0;

	return m_error;
}


#define MASS_SPEED_SCALE	60
#define MAX_MASS			40
#ifdef GAME_DLL
void CGrabController::ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics )
{

	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;

	// Compute total mass...
	float flMass = PhysGetEntityMass( pEntity );
	float flMaxMass = physcannon_maxmass.GetFloat();
	if ( flMass <= flMaxMass )
		return;

	float flLerpFactor = clamp( flMass, flMaxMass, 500.0f );
	flLerpFactor = SimpleSplineRemapVal( flLerpFactor, flMaxMass, 500.0f, 0.0f, 1.0f );

	float invMass = pPhysics->GetInvMass();
	float invInertia = pPhysics->GetInvInertia().Length();

	float invMaxMass = 1.0f / MAX_MASS;
	float ratio = invMaxMass / invMass;
	invMass = invMaxMass;
	invInertia *= ratio;

	float maxSpeed = invMass * MASS_SPEED_SCALE * 200;
	float maxAngular = invInertia * MASS_SPEED_SCALE * 360;

	m_shadow.maxSpeed = Lerp( flLerpFactor, m_shadow.maxSpeed, maxSpeed );
	m_shadow.maxAngular = Lerp( flLerpFactor, m_shadow.maxAngular, maxAngular );

}
#endif

QAngle CGrabController::TransformAnglesToPlayerSpace( const QAngle &anglesIn, CPortal_Player *pPlayer )
{
	Assert(pPlayer);
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();
		angleTest.x = 0;
		AngleMatrix( angleTest, test );
		return TransformAnglesToLocalSpace( anglesIn, test );
	}
	return TransformAnglesToLocalSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}

QAngle CGrabController::TransformAnglesFromPlayerSpace( const QAngle &anglesIn, CPortal_Player *pPlayer )
{
	Assert(pPlayer);
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();
		angleTest.x = 0;
		AngleMatrix( angleTest, test );
		return TransformAnglesToWorldSpace( anglesIn, test );
	}
	return TransformAnglesToWorldSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}


void CGrabController::AttachEntity( CPortal_Player *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition )
{
#ifdef CLIENT_DLL
	//uugghhh pEntity is garbage sometimes and this still doesn't fix it...
	if (!pPhys)
		return;
#endif
		
	Assert(pPlayer);
	// play the impact sound of the object hitting the player
	// used as feedback to let the player know he picked up the object
#ifdef GAME_DLL
	if ( !pPlayer->m_bSilentDropAndPickup )
	{
		int hitMaterial = pPhys->GetMaterialIndex();
		int playerMaterial = pPlayer->VPhysicsGetObject() ? pPlayer->VPhysicsGetObject()->GetMaterialIndex() : hitMaterial;
		PhysicsImpactSound( pPlayer, pPhys, CHAN_STATIC, hitMaterial, playerMaterial, 1.0, 64 );
	}
#endif

	pEntity->SetGrabController(this);

	Vector position;
	QAngle angles;
	pPhys->GetPosition( &position, &angles );
	
	// If it has a preferred orientation, use that instead.
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );

	//Fix attachment orientation weirdness
	if( pPlayer->IsHeldObjectOnOppositeSideOfPortal() )
	{
		Vector vPlayerForward;
		pPlayer->EyeVectors( &vPlayerForward );

		Vector radial = physcollision->CollideGetExtent( pPhys->GetCollide(), vec3_origin, pEntity->GetAbsAngles(), -vPlayerForward );
		Vector player2d = pPlayer->CollisionProp()->OBBMaxs();
		float playerRadius = player2d.Length2D();
		float flDot = DotProduct( vPlayerForward, radial );

		float radius = playerRadius + fabs( flDot );

		float distance = 24 + ( radius * 2.0f );		

		//find out which portal the object is on the other side of....
		Vector start = pPlayer->Weapon_ShootPosition();		
		Vector end = start + ( vPlayerForward * distance );

		CProp_Portal *pObjectPortal = NULL;
		pObjectPortal = pPlayer->GetHeldObjectPortal();

		// If our end point hasn't gone into the portal yet we at least need to know what portal is in front of us
		if ( !pObjectPortal )
		{
			Assert(!pObjectPortal);
			Ray_t rayPortalTest;
			rayPortalTest.Init( start, start + vPlayerForward * 1024.0f );

			int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
			if( iPortalCount != 0 )
			{
				CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
				Assert(pPortals);
				float fMinDist = 2.0f;
				for( int i = 0; i != iPortalCount; ++i )
				{
					CProp_Portal *pTempPortal = pPortals[i];
					Assert(pTempPortal);
					if( pTempPortal->IsActive() &&
						(pTempPortal->m_hLinkedPortal.Get() != NULL) )
					{
						float fDist = UTIL_IntersectRayWithPortal( rayPortalTest, pTempPortal );
						if( (fDist >= 0.0f) && (fDist < fMinDist) )
						{
							fMinDist = fDist;
							pObjectPortal = pTempPortal;
						}
					}
				}
			}
		}
		if (pObjectPortal && pObjectPortal->m_hLinkedPortal )
		{
			UTIL_Portal_AngleTransform( pObjectPortal->m_hLinkedPortal->MatrixThisToLinked(), angles, angles );
		}
	}

	VectorITransform( pEntity->WorldSpaceCenter(), pEntity->EntityToWorldTransform(), m_attachedPositionObjectSpace );
//	ComputeMaxSpeed( pEntity, pPhys );
#ifdef GAME_DLL
	// If we haven't been killed by a grab, we allow the gun to grab the nearest part of a ragdoll
	if ( bUseGrabPosition )
	{
		IPhysicsObject *pChild = GetRagdollChildAtPosition( pEntity, vGrabPosition );
		
		if ( pChild )
		{
			pPhys = pChild;
		}
	}
#endif
	// Carried entities can never block LOS
	m_bCarriedEntityBlocksLOS = pEntity->BlocksLOS();
	pEntity->SetBlocksLOS( false );
	m_controller = physenv->CreateMotionController( this );
	m_controller->AttachObject( pPhys, true );
	// Don't do this, it's causing trouble with constraint solvers.
	//m_controller->SetPriority( IPhysicsMotionController::HIGH_PRIORITY );

	pPhys->Wake();
	PhysSetGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
	SetTargetPosition( position, angles );
	m_attachedEntity = pEntity;
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	Assert(pList);
	int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
	m_flLoadWeight = 0;
	float damping = 10;
	float flFactor = count / 7.5f;
	if ( flFactor < 1.0f )
	{
		flFactor = 1.0f;
	}
	for ( int i = 0; i < count; i++ )
	{
		float mass = pList[i]->GetMass();
		pList[i]->GetDamping( NULL, &m_savedRotDamping[i] );
		m_flLoadWeight += mass;
		m_savedMass[i] = mass;

		// reduce the mass to prevent the player from adding crazy amounts of energy to the system
		pList[i]->SetMass( REDUCED_CARRY_MASS / flFactor );
		pList[i]->SetDamping( NULL, &damping );
	}

	// NVNT setting m_pControllingPlayer to the player attached
	m_pControllingPlayer = pPlayer;
	
	// Give extra mass to the phys object we're actually picking up
	pPhys->SetMass( REDUCED_CARRY_MASS );
	pPhys->EnableDrag( false );

	m_errorTime = bIsMegaPhysCannon ? -1.5f : -1.0f; // 1 seconds until error starts accumulating
	m_error = 0;
	m_contactAmount = 0;

	m_attachedAnglesPlayerSpace = TransformAnglesToPlayerSpace( angles, pPlayer );
	if ( m_angleAlignment != 0 )
	{
		m_attachedAnglesPlayerSpace = AlignAngles( m_attachedAnglesPlayerSpace, m_angleAlignment );
	}
#ifdef GAME_DLL
	// Ragdolls don't offset this way
	if ( dynamic_cast<CRagdollProp*>(pEntity) )
	{
		m_attachedPositionObjectSpace.Init();
	}
	else
	{
		VectorITransform( pEntity->WorldSpaceCenter(), pEntity->EntityToWorldTransform(), m_attachedPositionObjectSpace );
	}
#endif	

	// If it's a prop, see if it has desired carry angles
	CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(pEntity);
	if ( pProp )
	{
		m_bHasPreferredCarryAngles = pProp->GetPropDataAngles( "preferred_carryangles", m_vecPreferredCarryAngles );
		m_flDistanceOffset = pProp->GetCarryDistanceOffset();
	}
	else
	{
		m_bHasPreferredCarryAngles = false;
		m_flDistanceOffset = 0;
	}

	m_bAllowObjectOverhead = IsObjectAllowedOverhead(pEntity);

}

static void ClampPhysicsVelocity( IPhysicsObject *pPhys, float linearLimit, float angularLimit )
{

	Assert(pPhys);
	Vector vel;
	AngularImpulse angVel;
	pPhys->GetVelocity( &vel, &angVel );
	float speed = VectorNormalize(vel) - linearLimit;
	float angSpeed = VectorNormalize(angVel) - angularLimit;
	speed = speed < 0 ? 0 : -speed;
	angSpeed = angSpeed < 0 ? 0 : -angSpeed;
	vel *= speed;
	angVel *= angSpeed;
	pPhys->AddVelocity( &vel, &angVel );
}

void CGrabController::DetachEntity( bool bClearVelocity )
{
	CBaseEntity *pEntity = GetAttached();
#ifdef GAME_DLL
	Assert(!PhysIsInCallback());
	if ( pEntity )
	{
		// Restore the LS blocking state
		pEntity->SetBlocksLOS( m_bCarriedEntityBlocksLOS );
		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		Assert(pList);
		int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
		for ( int i = 0; i < count; i++ )
		{
			IPhysicsObject *pPhys = pList[i];
			Assert(pPhys);
			if ( !pPhys )
				continue;

			// on the odd chance that it's gone to sleep while under anti-gravity
			pPhys->EnableDrag( true );
			pPhys->Wake();
			pPhys->SetMass( m_savedMass[i] );
			pPhys->SetDamping( NULL, &m_savedRotDamping[i] );
			PhysClearGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
			if ( bClearVelocity )
			{
				PhysForceClearVelocity( pPhys );
			}
			else
			{
				ClampPhysicsVelocity( pPhys, hl2_normspeed.GetFloat() * 1.5f, 2.0f * 360.0f );
			}

		}
	}

	if (physenv)
		physenv->DestroyMotionController( m_controller );
	m_controller = NULL;
#else
	if (m_attachedEntity && cl_predict->GetInt() && cl_predicted_grabbing.GetBool() )
	{
		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity(m_attachedEntity);
		if (pSimulator)
			pSimulator->ReleaseOwnershipOfEntity(m_attachedEntity);
		m_attachedEntity->VPhysicsDestroyObject();
	}
#endif
	if (pEntity)
		pEntity->SetGrabController(NULL);
	
	m_attachedEntity = NULL;
}

static bool InContactWithHeavyObject( IPhysicsObject *pObject, float heavyMass )
{
	bool contact = false;
	IPhysicsFrictionSnapshot *pSnapshot = pObject->CreateFrictionSnapshot();
	Assert(pSnapshot);
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		Assert(pOther);
		if ( !pOther->IsMoveable() || pOther->GetMass() > heavyMass )
		{
			contact = true;
			break;
		}
		pSnapshot->NextFrictionData();
	}
	pObject->DestroyFrictionSnapshot( pSnapshot );
	return contact;
	
}


IMotionEvent::simresult_e CGrabController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{

	Assert(pController);
	Assert(pObject);
	game_shadowcontrol_params_t shadowParams = m_shadow;
	if ( InContactWithHeavyObject( pObject, GetLoadWeight() ) )
	{
		m_contactAmount = Approach( 0.1f, m_contactAmount, deltaTime*2.0f );
	}
	else
	{
		m_contactAmount = Approach( 1.0f, m_contactAmount, deltaTime*2.0f );
	}
	shadowParams.maxAngular = m_shadow.maxAngular * m_contactAmount * m_contactAmount * m_contactAmount;
	m_timeToArrive = pObject->ComputeShadowControl( shadowParams, m_timeToArrive, deltaTime );
	
	// Slide along the current contact points to fix bouncing problems
	Vector velocity;
	AngularImpulse angVel;
	pObject->GetVelocity( &velocity, &angVel );
	PhysComputeSlideDirection( pObject, velocity, angVel, &velocity, &angVel, GetLoadWeight() );
	pObject->SetVelocityInstantaneous( &velocity, NULL );

	linear.Init();
	angular.Init();
	m_errorTime += deltaTime;

	return SIM_LOCAL_ACCELERATION;
}

float CGrabController::GetSavedMass( IPhysicsObject *pObject )
{
	CBaseEntity *pHeld = m_attachedEntity;
	if ( pHeld )
	{
		if ( pObject->GetGameData() == (void*)pHeld )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int count = pHeld->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			for ( int i = 0; i < count; i++ )
			{
				if ( pList[i] == pObject )
					return m_savedMass[i];
			}
		}
	}
	return 0.0f;
}
//-----------------------------------------------------------------------------
// Is this an object that the player is allowed to lift to a position 
// directly overhead? The default behavior prevents lifting objects directly
// overhead, but there are exceptions for gameplay purposes.
//-----------------------------------------------------------------------------
bool CGrabController::IsObjectAllowedOverhead( CBaseEntity *pEntity )
{
	// Allow combine balls overhead 
	if( UTIL_IsCombineBallDefinite(pEntity) )
		return true;

	// Allow props that are specifically flagged as such
	CPhysicsProp *pPhysProp = dynamic_cast<CPhysicsProp *>(pEntity);
	if ( pPhysProp != NULL && pPhysProp->HasInteraction( PROPINTER_PHYSGUN_ALLOW_OVERHEAD ) )
		return true;

	// String checks are fine here, we only run this code one time- when the object is picked up.
#ifdef GAME_DLL
	if( pEntity->ClassMatches("grenade_helicopter") )
		return true;

	if( pEntity->ClassMatches("weapon_striderbuster") )
		return true;
#else
	if (FClassnameIs(pEntity, "grenade_helicopter"))
		return true;

	if (FClassnameIs(pEntity, "weapon_striderbuster"))
		return true;

#endif

	return false;
}
#ifdef GAME_DLL
#endif



void CGrabController::SetPortalPenetratingEntity( CBaseEntity *pPenetrated )
{
	Assert(pPenetrated);
	m_PenetratedEntity = pPenetrated;
}



//-----------------------------------------------------------------------------
// Player pickup controller
//-----------------------------------------------------------------------------

static const char *s_pThinkPickupContext = "ThinkPickupContext";

#ifdef CLIENT_DLL
#define CPlayerPickupController C_PlayerPickupController
#endif

class CPlayerPickupController : public CBaseEntity
{
public:
	DECLARE_CLASS( CPlayerPickupController, CBaseEntity );

	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();

	CPlayerPickupController();

	void Initiate( CPortal_Player *pPlayer, CBaseEntity *pObject );
	void Shutdown( bool bThrown = false );
	bool OnControls( CBaseEntity *pControls ) { return true; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
#ifdef CLIENT_DLL
	virtual	void	Simulate();
	virtual void	OnDataChanged( DataUpdateType_t type );
	virtual void	OnPreDataChanged( DataUpdateType_t type );
	virtual void	PreDataUpdate( DataUpdateType_t type );
#endif


#ifdef GAME_DLL
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
#else
	bool		m_bShouldInit;
#endif

	void OnRestore()
	{
		m_grabController.OnRestore();
	}
	void VPhysicsUpdate( IPhysicsObject *pPhysics ){}
	void VPhysicsShadowUpdate( IPhysicsObject *pPhysics ) {}

	bool IsHoldingEntity( CBaseEntity *pEnt );

	CGrabController &GetGrabController() { return m_grabController; }

	
	CNetworkHandle( CBaseEntity, m_hAttachedObject );
	CNetworkHandle( CBaseEntity, m_hNetworkedObject );
	
	EHANDLE m_hOldAttachedObject;

#ifdef CLIENT_DLL
	void			ManagePredictedObject(void);
#endif

	QAngle	m_attachedAnglesPlayerSpace;
	Vector	m_attachedPositionObjectSpace;
	
	CNetworkHandle( CPortal_Player, m_hPlayer );

private:
	CGrabController		m_grabController;

};

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CPlayerPickupController )

	DEFINE_EMBEDDED( m_grabController ),

	// Physptrs can't be inside embedded classes
	DEFINE_PHYSPTR( m_grabController.m_controller ),

	DEFINE_FIELD( m_hPlayer,		FIELD_EHANDLE ),
#ifdef GAME_DLL
	DEFINE_THINKFUNC(Think),
#endif
END_DATADESC()
IMPLEMENT_NETWORKCLASS_ALIASED( PlayerPickupController, DT_PlayerPickupController )

BEGIN_NETWORK_TABLE( CPlayerPickupController, DT_PlayerPickupController)
#ifdef GAME_DLL
	SendPropEHandle(SENDINFO(m_hAttachedObject)),
	SendPropEHandle(SENDINFO(m_hNetworkedObject)),
	SendPropEHandle(SENDINFO(m_hPlayer)),
#else
	RecvPropEHandle(RECVINFO(m_hAttachedObject)),
	RecvPropEHandle(RECVINFO(m_hNetworkedObject)),
	RecvPropEHandle(RECVINFO(m_hPlayer)),
#endif
END_NETWORK_TABLE();

LINK_ENTITY_TO_CLASS_ALIASED( player_pickup, PlayerPickupController );

CPlayerPickupController::CPlayerPickupController()
{
#ifdef GAME_DLL
	SetTransmitState( FL_EDICT_ALWAYS );
#else
	m_bShouldInit = true;
	//SetNextClientThink(CLIENT_THINK_ALWAYS);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			*pObject - 
//-----------------------------------------------------------------------------
void CPlayerPickupController::Initiate(CPortal_Player *pPlayer, CBaseEntity *pObject)
{
	CWeaponPhysCannon *pPhysgun = pObject->GetPhysgun();
	if (pPhysgun)
		return;

	CGrabController *pGrabController = pObject->GetGrabController();
	if (pGrabController)
		pGrabController->DetachEntity(false);

	// Holster player's weapon
	if ( pPlayer->GetActiveWeapon() )
	{
		// Don't holster the portalgun
		if ( FClassnameIs( pPlayer->GetActiveWeapon(), "weapon_portalgun" ) )
		{
			CWeaponPortalgun *pPortalGun = (CWeaponPortalgun*)(pPlayer->GetActiveWeapon());
			pPortalGun->OpenProngs( true );
			pPortalGun->m_bCanAttack = false;
		}
		else
		{
			if ( !pPlayer->GetActiveWeapon()->Holster() )
			{
				Shutdown();
				return;
			}
		}
	}
	m_attachedPositionObjectSpace = m_grabController.m_attachedPositionObjectSpace;
	m_attachedAnglesPlayerSpace = m_grabController.m_attachedAnglesPlayerSpace;

	CPortal_Player *pOwner = ToPortalPlayer( pPlayer );
	if ( pOwner )
	{
		pOwner->EnableSprint( false );
	}
	// If the target is debris, convert it to non-debris
	if ( pObject->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
	{
		// Interactive debris converts back to debris when it comes to rest
		pObject->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}
	// done so I'll go across level transitions with the player
	SetParent( pPlayer );
	m_grabController.SetIgnorePitch( true );
#ifdef GAME_DLL
	m_grabController.SetAngleAlignment( DOT_30DEGREE );
#else
	m_grabController.SetAngleAlignment(0.866025403784);	
#endif

#ifdef GAME_DLL
	m_hPlayer = pPlayer;
#endif

	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
	Assert(pPhysics);

#ifdef CLIENT_DLL

#endif

	if ( !pOwner->m_bSilentDropAndPickup )
	{
		Pickup_OnPhysGunPickup( pObject, m_hPlayer, PICKED_UP_BY_PLAYER );
	}

	m_grabController.AttachEntity( pPlayer, pObject, pPhysics, false, vec3_origin, false );
#ifdef WIN32
	// NVNT apply a downward force to simulate the mass of the held object.
	HapticSetConstantForce(m_hPlayer,clamp(m_grabController.GetLoadWeight()*0.1,1,6)*Vector(0,-1,0));
#endif	
	m_hPlayer->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;
	m_hPlayer->SetUseEntity( this );

	//Network this
	
	m_attachedPositionObjectSpace = m_grabController.m_attachedPositionObjectSpace;
	m_attachedAnglesPlayerSpace = m_grabController.m_attachedAnglesPlayerSpace;
#ifdef GAME_DLL
	m_hAttachedObject = pObject;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CPlayerPickupController::Shutdown( bool bThrown )
{
	CBaseEntity *pObject = m_grabController.GetAttached();

	bool bClearVelocity = false;
	if ( !bThrown && pObject && pObject->VPhysicsGetObject() && pObject->VPhysicsGetObject()->GetContactPoint(NULL,NULL) )
	{
		bClearVelocity = true;
	}

	m_grabController.DetachEntity( bClearVelocity );
#ifdef WIN32
	// NVNT if we have a player, issue a zero constant force message
	if(m_hPlayer)
		HapticSetConstantForce(m_hPlayer,Vector(0,0,0));
#endif

	if ( pObject != NULL )
	{
		if ( !ToPortalPlayer( m_hPlayer )->m_bSilentDropAndPickup )
		{
			Pickup_OnPhysGunDrop( pObject, m_hPlayer, bThrown ? THROWN_BY_PLAYER : DROPPED_BY_PLAYER );
		}
	}

	if ( m_hPlayer )
	{
		CPortal_Player *pOwner = ToPortalPlayer( m_hPlayer );
		if ( pOwner )
		{
			pOwner->EnableSprint( true );
		}

		m_hPlayer->SetUseEntity( NULL );
		if ( !pOwner->m_bSilentDropAndPickup )
		{
			if ( m_hPlayer->GetActiveWeapon() )
			{
				if ( FClassnameIs( m_hPlayer->GetActiveWeapon(), "weapon_portalgun" ) )
				{
					m_hPlayer->SetNextAttack( gpGlobals->curtime + 0.5f );
					CWeaponPortalgun *pPortalGun = (CWeaponPortalgun*)(m_hPlayer->GetActiveWeapon());
					pPortalGun->DelayAttack( 0.5f );
					pPortalGun->OpenProngs( false );
					pPortalGun->m_bCanAttack = true;
				}
				else
				{
					if ( !m_hPlayer->GetActiveWeapon()->Deploy() )
					{
						// We tried to restore the player's weapon, but we couldn't.
						// This usually happens when they're holding an empty weapon that doesn't
						// autoswitch away when out of ammo. Switch to next best weapon.
						m_hPlayer->SwitchToNextBestWeapon( NULL );
					}
				}
			}

			m_hPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
		}
	}
#ifdef CLIENT_DLL

	if ( m_hAttachedObject )
	{
		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( m_hAttachedObject );
		if (pSimulator)
			pSimulator->ReleaseOwnershipOfEntity( m_hAttachedObject );

		m_hAttachedObject->VPhysicsDestroyObject();
		m_hAttachedObject = NULL;
	}
	
	if ( m_hOldAttachedObject )
	{
		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( m_hOldAttachedObject );
		if (pSimulator)
			pSimulator->ReleaseOwnershipOfEntity( m_hOldAttachedObject );

		m_hOldAttachedObject->VPhysicsDestroyObject();
		m_hOldAttachedObject = NULL;
	}

#endif
#ifdef GAME_DLL
	m_hAttachedObject = NULL;

	Remove();
#endif
}

#ifdef CLIENT_DLL

void CPlayerPickupController::Simulate( void )
{
	BaseClass::Simulate();

#ifdef CLIENT_DLL	
	CPortal_Player *localplayer = CPortal_Player::GetLocalPlayer();

	if ( localplayer && !localplayer->IsObserver() && m_hPlayer->IsLocalPlayer() )
		ManagePredictedObject();

	m_grabController.UpdateObject( m_hPlayer, 12 );
#endif

	//SetNextClientThink( CLIENT_THINK_ALWAYS );
}
#endif


void CPlayerPickupController::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ToBasePlayer(pActivator) == m_hPlayer )
	{
		CBaseEntity *pAttached = m_grabController.GetAttached();

		// UNDONE: Use vphysics stress to decide to drop objects
		// UNDONE: Must fix case of forcing objects into the ground you're standing on (causes stress) before that will work
		if ( !pAttached || useType == USE_OFF || (m_hPlayer->m_nButtons & IN_ATTACK2) || m_grabController.ComputeError() > 12 )
		{
			Shutdown();
			return;
		}
		
		//Adrian: Oops, our object became motion disabled, let go!
		IPhysicsObject *pPhys = pAttached->VPhysicsGetObject();
		if ( pPhys && pPhys->IsMoveable() == false )
		{
			Shutdown();
			return;
		}

#if STRESS_TEST
		vphysics_objectstress_t stress;
		CalculateObjectStress( pPhys, pAttached, &stress );
		if ( stress.exertedStress > 250 )
		{
			Shutdown();
			return;
		}
#endif
		// +ATTACK will throw phys objects
		if ( m_hPlayer->m_nButtons & IN_ATTACK )
		{
			Shutdown( true );
			Vector vecLaunch;
			m_hPlayer->EyeVectors( &vecLaunch );
			// If throwing from the opposite side of a portal, reorient the direction
			if( ((CPortal_Player *)m_hPlayer)->IsHeldObjectOnOppositeSideOfPortal() )
			{
				CProp_Portal *pHeldPortal = ((CPortal_Player *)m_hPlayer)->GetHeldObjectPortal();
				Assert(pHeldPortal);
				UTIL_Portal_VectorTransform( pHeldPortal->MatrixThisToLinked(), vecLaunch, vecLaunch );
			}

			(m_hPlayer)->SetHeldObjectOnOppositeSideOfPortal( false );
			// JAY: Scale this with mass because some small objects really go flying
			float massFactor = clamp( pPhys->GetMass(), 0.5, 15 );
			massFactor = RemapVal( massFactor, 0.5, 15, 0.5, 4 );
			vecLaunch *= player_throwforce.GetFloat() * massFactor;

			pPhys->ApplyForceCenter( vecLaunch );
			AngularImpulse aVel = RandomAngularImpulse( -10, 10 ) * massFactor;
			pPhys->ApplyTorqueCenter( aVel );
			return;
		}

		if ( useType == USE_SET )
		{
			m_grabController.UpdateObject( m_hPlayer, 12 );
		}
	}
}
#ifdef CLIENT_DLL
void CPlayerPickupController::ManagePredictedObject(void)
{
	if ( ( !cl_predict->GetInt() || !cl_predicted_grabbing.GetBool() ) || ( m_hPlayer && !m_hPlayer->IsLocalPlayer() ) )
		return;

	CBaseEntity *pAttachedObject = m_hAttachedObject.Get();

#if 0
	CPortalSimulator *pSimulator = NULL;
	CPortalSimulator *pSimulatorOld = NULL;
	if (pAttachedObject)
		pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity(pAttachedObject);
	
	if (m_hOldAttachedObject)
		pSimulatorOld = CPortalSimulator::GetSimulatorThatOwnsEntity(m_hOldAttachedObject);
	if (pSimulator || pSimulatorOld)
	{
		if (pSimulator && pAttachedObject && pAttachedObject->VPhysicsGetObject())
		{
			pAttachedObject->VPhysicsDestroyObject();
		}
		if (pSimulatorOld && m_hOldAttachedObject && m_hOldAttachedObject->VPhysicsGetObject())
		{
			m_hOldAttachedObject->VPhysicsDestroyObject();
		}

		m_hPlayer->SetHeldPhysicsPortal(NULL);

		GetGrabController().DetachEntity(false);
		return;
	}
#endif
	if ( m_hAttachedObject )
	{
		
		IPhysicsObject *pPhysics = pAttachedObject->VPhysicsGetObject();

		// NOTE :This must happen after OnPhysGunPickup because that can change the mass
		if ( pAttachedObject != GetGrabController().GetAttached() )
		{			
			if ( pPhysics == NULL )
			{
				solid_t tmpSolid;
				PhysModelParseSolid( tmpSolid, m_hAttachedObject, pAttachedObject->GetModelIndex() );
				int nSolidFlags = pAttachedObject->GetSolidFlags();
				pAttachedObject->VPhysicsInitNormal( SOLID_VPHYSICS, nSolidFlags, false, &tmpSolid );
			}

			pPhysics = pAttachedObject->VPhysicsGetObject();
			Assert(pPhysics);
			if ( pPhysics )
			{
				m_hPlayer->SetHeldPhysicsPortal(pPhysics);

				if (m_bShouldInit && pAttachedObject != NULL)
				{
					m_bShouldInit = false;
					//Initiate(localplayer, pAttachedObject);
				
					SetParent( m_hPlayer );
					m_grabController.SetIgnorePitch( true );
					m_grabController.SetAngleAlignment(0.866025403784);	
					
					m_grabController.AttachEntity( m_hPlayer, pAttachedObject, pPhysics, false, vec3_origin, false );
					//GetGrabController().AttachEntity( m_hPlayer, pAttachedObject, pPhysics, false, vec3_origin, false );
					m_grabController.m_attachedPositionObjectSpace = m_attachedPositionObjectSpace;
					m_grabController.m_attachedAnglesPlayerSpace = m_attachedAnglesPlayerSpace;
				}
			}
		}
	}
	else
	{
		if ( m_hOldAttachedObject && m_hOldAttachedObject->VPhysicsGetObject() )
		{
			GetGrabController().DetachEntity( false );
			m_hPlayer->SetHeldPhysicsPortal(NULL);
			
			CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( m_hOldAttachedObject );
			if (pSimulator)
				pSimulator->ReleaseOwnershipOfEntity( m_hOldAttachedObject );

			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}

	m_hOldAttachedObject = m_hAttachedObject;
}
#endif
#ifdef CLIENT_DLL
void CPlayerPickupController::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( !cl_predict->GetInt() && !cl_predicted_grabbing.GetBool() )
		return;

	SetNextClientThink( CLIENT_THINK_ALWAYS );

	if ( m_hPlayer == NULL )
	{
		if ( m_hAttachedObject )
		{
			CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( m_hAttachedObject );
			if (pSimulator)
				pSimulator->ReleaseOwnershipOfEntity( m_hAttachedObject );

			m_hAttachedObject->VPhysicsDestroyObject();
		}

		if ( m_hOldAttachedObject )
		{
			CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( m_hOldAttachedObject );
			if (pSimulator)
				pSimulator->ReleaseOwnershipOfEntity( m_hOldAttachedObject );
			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}
}
void CPlayerPickupController::OnPreDataChanged( DataUpdateType_t type )
{	
	BaseClass::OnPreDataChanged(type);
}

void CPlayerPickupController::PreDataUpdate( DataUpdateType_t type )
{
	BaseClass::PreDataUpdate(type);
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPlayerPickupController::IsHoldingEntity( CBaseEntity *pEnt )
{

	if ( pEnt )
	{
		return ( m_grabController.GetAttached() == pEnt );
	}

	return ( m_grabController.GetAttached() != 0 );

}
void PlayerPickupObject(CBasePlayer *pPlayer, CBaseEntity *pObject)
{
#ifdef GAME_DLL
	CPortal_Player *pPortalPlayer = ToPortalPlayer(pPlayer);

	//Don't pick up if we don't have a phys object.
	if ( pObject->VPhysicsGetObject() == NULL )
		return;

	if ( pObject->GetBaseAnimating() && pObject->GetBaseAnimating()->IsDissolving() )
		return;
	CPlayerPickupController *pController = (CPlayerPickupController *)CBaseEntity::Create( "player_pickup", pObject->GetAbsOrigin(), vec3_angle, pPortalPlayer );
	if ( !pController )
		return;
	pController->m_hNetworkedObject = pObject;
	pController->Initiate( pPortalPlayer, pController->m_hNetworkedObject );
#endif
}

//-----------------------------------------------------------------------------


BEGIN_SIMPLE_DATADESC( thrown_objects_t )
	DEFINE_FIELD( fTimeThrown, FIELD_TIME ),
	DEFINE_FIELD( hEntity,	FIELD_EHANDLE	),
END_DATADESC()

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPhysCannon, DT_WeaponPhysCannon )

BEGIN_NETWORK_TABLE( CWeaponPhysCannon, DT_WeaponPhysCannon )
#ifdef GAME_DLL
	SendPropBool( SENDINFO( m_bIsCurrentlyUpgrading ) ),
	SendPropBool( SENDINFO( m_bCanPickupObject ) ),
	SendPropBool( SENDINFO( m_bActive ) ),
#ifdef SENDPHYSCANNONINFOTOCLIENT
	SendPropBool( SENDINFO( m_bAllowsPunts ) ),
	SendPropEHandle( SENDINFO( m_hAttachedObject ) ),
	SendPropVector(SENDINFO( m_attachedPositionObjectSpace ), -1, SPROP_COORD),
	SendPropAngle( SENDINFO_VECTORELEM(m_attachedAnglesPlayerSpace, 0 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_attachedAnglesPlayerSpace, 1 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_attachedAnglesPlayerSpace, 2 ), 11 ),
	SendPropInt( SENDINFO( m_EffectState ) ),
	SendPropBool( SENDINFO( m_bOpen ) ),
#endif
	
#else
	RecvPropBool( RECVINFO( m_bIsCurrentlyUpgrading ) ),
	RecvPropBool( RECVINFO( m_bCanPickupObject ) ),	
	RecvPropBool( RECVINFO( m_bAllowsPunts ) ),
	RecvPropBool( RECVINFO( m_bActive ) ),
	RecvPropEHandle( RECVINFO( m_hAttachedObject ) ),
	RecvPropVector( RECVINFO( m_attachedPositionObjectSpace ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[0] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[1] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[2] ) ),
	RecvPropInt( RECVINFO( m_EffectState ) ),
	RecvPropBool( RECVINFO( m_bOpen ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponPhysCannon )
	DEFINE_PRED_FIELD( m_EffectState,	FIELD_INTEGER,	FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bOpen,			FIELD_BOOLEAN,	FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bCanPickupObject,			FIELD_BOOLEAN,	FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_physcannon, WeaponPhysCannon );
PRECACHE_WEAPON_REGISTER( weapon_physcannon );

BEGIN_DATADESC( CWeaponPhysCannon )

	DEFINE_FIELD( m_bOpen, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_nChangeState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flCheckSuppressTime, FIELD_TIME ),
	DEFINE_FIELD( m_flElementDebounce, FIELD_TIME ),
	DEFINE_FIELD( m_flElementPosition, FIELD_FLOAT ),
	DEFINE_FIELD( m_flElementDestination, FIELD_FLOAT ),
	DEFINE_FIELD( m_nAttack2Debounce, FIELD_INTEGER ),
	DEFINE_FIELD( m_bIsCurrentlyUpgrading, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_EffectState, FIELD_INTEGER ),

#ifdef GAME_DLL
	DEFINE_AUTO_ARRAY( m_hBeams, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_hGlowSprites, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_hEndSprites, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_flEndSpritesOverride, FIELD_TIME ),
	DEFINE_FIELD( m_hCenterSprite, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hBlastSprite, FIELD_EHANDLE ),
#endif
	DEFINE_FIELD( m_flLastDenySoundPlayed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPhyscannonState, FIELD_BOOLEAN ),
	DEFINE_SOUNDPATCH( m_sndMotor ),

	DEFINE_EMBEDDED( m_grabController ),

	// Physptrs can't be inside embedded classes
	DEFINE_PHYSPTR( m_grabController.m_controller ),
#ifdef GAME_DLL
	DEFINE_THINKFUNC( WaitForUpgradeThink ),

	DEFINE_UTLVECTOR( m_ThrownEntities, FIELD_EMBEDDED ),

	DEFINE_FIELD( m_flTimeNextObjectPurge, FIELD_TIME ),
#endif
	END_DATADESC()
#ifdef GAME_DLL
#endif

enum
{
	ELEMENT_STATE_NONE = -1,
	ELEMENT_STATE_OPEN,
	ELEMENT_STATE_CLOSED,
};

enum
{
	EFFECT_NONE,
	EFFECT_CLOSED,
	EFFECT_READY,
	EFFECT_HOLDING,
	EFFECT_LAUNCH,
};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponPhysCannon::CWeaponPhysCannon( void )
{
	m_flElementPosition		= 0.0f;
	m_flElementDestination	= 0.0f;
	m_bOpen					= false;
	m_nChangeState			= ELEMENT_STATE_NONE;
	m_flCheckSuppressTime	= 0.0f;
	m_EffectState			= EFFECT_NONE;
	m_flLastDenySoundPlayed	= false;
#ifdef GAME_DLL
	m_flEndSpritesOverride[0] = 0.0f;
	m_flEndSpritesOverride[1] = 0.0f;
#endif
	m_bPhyscannonState = false;
#ifdef CLIENT_DLL
	m_bWasUpgraded = false;
#endif

#ifdef CLIENT_DLL
	m_nOldEffectState = EFFECT_NONE;
	m_bOldOpen = false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Precache( void )
{
	PrecacheModel( PHYSCANNON_BEAM_SPRITE );
	PrecacheModel( PHYSCANNON_GLOW_SPRITE );
	PrecacheModel( PHYSCANNON_ENDCAP_SPRITE );
	PrecacheModel( PHYSCANNON_CENTER_GLOW );
	PrecacheModel( PHYSCANNON_BLAST_SPRITE );

	PrecacheModel( MEGACANNON_BEAM_SPRITE );
	PrecacheModel( MEGACANNON_GLOW_SPRITE );
	PrecacheModel( MEGACANNON_ENDCAP_SPRITE );
	PrecacheModel( MEGACANNON_CENTER_GLOW );
	PrecacheModel( MEGACANNON_BLAST_SPRITE );

	PrecacheModel( MEGACANNON_RAGDOLL_BOOGIE_SPRITE );

	// Precache our alternate model
	PrecacheModel( MEGACANNON_MODEL );

	PrecacheScriptSound( "Weapon_PhysCannon.HoldSound" );
	PrecacheScriptSound( "Weapon_Physgun.Off" );

	PrecacheScriptSound( "Weapon_MegaPhysCannon.DryFire" );
	PrecacheScriptSound( "Weapon_MegaPhysCannon.Launch" );
	PrecacheScriptSound( "Weapon_MegaPhysCannon.Pickup");
	PrecacheScriptSound( "Weapon_MegaPhysCannon.Drop");
	PrecacheScriptSound( "Weapon_MegaPhysCannon.HoldSound");
	PrecacheScriptSound( "Weapon_MegaPhysCannon.ChargeZap");

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Spawn( void )
{
	BaseClass::Spawn();

	// Need to get close to pick it up
	CollisionProp()->UseTriggerBounds( false );

	m_bPhyscannonState = IsMegaPhysCannon();

	// The megacannon uses a different skin
	if ( IsMegaPhysCannon() )
	{
		m_nSkin = MEGACANNON_SKIN;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Restore
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::OnRestore()
{
	BaseClass::OnRestore();
	m_grabController.OnRestore();

	m_bPhyscannonState = IsMegaPhysCannon();

	// Tracker 8106:  Physcannon effects disappear through level transition, so
	//  just recreate any effects here
	if ( m_EffectState != EFFECT_NONE )
	{
		DoEffect( m_EffectState, NULL );
	}
}


//-----------------------------------------------------------------------------
// On Remove
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::UpdateOnRemove(void)
{
	DestroyEffects( );
	BaseClass::UpdateOnRemove();
}

#ifdef CLIENT_DLL
void CWeaponPhysCannon::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( !cl_predict->GetInt() && !cl_predicted_grabbing.GetBool())
		return;

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false );
		StartEffects();
	}

	if ( GetOwner() == NULL )
	{
		if ( m_hAttachedObject )
		{
			CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( m_hAttachedObject );
			if (pSimulator)
				pSimulator->ReleaseOwnershipOfEntity( m_hAttachedObject );
			m_hAttachedObject->VPhysicsDestroyObject();
		}

		if ( m_hOldAttachedObject )
		{
			CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( m_hOldAttachedObject );
			if (pSimulator)
				pSimulator->ReleaseOwnershipOfEntity( m_hOldAttachedObject );
			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}
	// Update effect state when out of parity with the server
	if ( m_nOldEffectState != m_EffectState )
	{
		DoEffect( m_EffectState );
		m_nOldEffectState = m_EffectState;
	}

	// Update element state when out of parity
	if ( m_bOldOpen != m_bOpen )
	{
		
		if ( m_bOpen )
		{
			m_ElementParameter.InitFromCurrent( 1.0f, 0.2f, INTERP_SPLINE );
		}
		else
		{	
			m_ElementParameter.InitFromCurrent( 0.0f, 0.5f, INTERP_SPLINE );
		}
	
	
		m_bOldOpen = (bool) m_bOpen;
	}
}

void CWeaponPhysCannon::ManagePredictedObject( void )
{
	if ( ( !cl_predict->GetInt() || !cl_predicted_grabbing.GetBool() ) || ( GetPlayerOwner() && !GetPlayerOwner()->IsLocalPlayer() ) )
		return;

	CBaseEntity *pAttachedObject = m_hAttachedObject.Get();

#if 0
	C_Portal_Player *localplayer = C_Portal_Player::GetLocalPlayer();
	CPortalSimulator *pSimulator = NULL;
	CPortalSimulator *pSimulatorOld = NULL;
	if (pAttachedObject)
		pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity(pAttachedObject);
	
	if (m_hOldAttachedObject)
		pSimulatorOld = CPortalSimulator::GetSimulatorThatOwnsEntity(m_hOldAttachedObject);
	if (pSimulator || pSimulatorOld)
	{
		if (pSimulator && pAttachedObject && pAttachedObject->VPhysicsGetObject())
		{
			pAttachedObject->VPhysicsDestroyObject();
		}
		if (pSimulatorOld && m_hOldAttachedObject && m_hOldAttachedObject->VPhysicsGetObject())
		{
			m_hOldAttachedObject->VPhysicsDestroyObject();
		}

		localplayer->SetHeldPhysicsPortal(NULL);

		GetGrabController().DetachEntity(false);
		return;
	}
#endif
	if ( m_hAttachedObject )
	{
		// NOTE :This must happen after OnPhysGunPickup because that can change the mass
		if ( pAttachedObject != GetGrabController().GetAttached() )
		{
			IPhysicsObject *pPhysics = pAttachedObject->VPhysicsGetObject();

			if ( pPhysics == NULL )
			{
				solid_t tmpSolid;
				PhysModelParseSolid( tmpSolid, m_hAttachedObject, pAttachedObject->GetModelIndex() );
				
				int nSolidFlags = pAttachedObject->GetSolidFlags();
				pAttachedObject->VPhysicsInitNormal( SOLID_VPHYSICS, nSolidFlags, false, &tmpSolid );
			}

			pPhysics = pAttachedObject->VPhysicsGetObject();

			if ( pPhysics )
			{
				m_grabController.SetIgnorePitch( false );
				m_grabController.SetAngleAlignment( 0 );

				GetGrabController().AttachEntity( ToPortalPlayer( GetOwner() ), pAttachedObject, pPhysics, false, vec3_origin, false );
				GetGrabController().m_attachedPositionObjectSpace = m_attachedPositionObjectSpace;
				GetGrabController().m_attachedAnglesPlayerSpace = m_attachedAnglesPlayerSpace;
			}
		}
	}
	else
	{
		if ( m_hOldAttachedObject && m_hOldAttachedObject->VPhysicsGetObject() )
		{
			GetGrabController().DetachEntity( false );

			CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity(m_hOldAttachedObject);
			if (pSimulator)
				pSimulator->ReleaseOwnershipOfEntity(m_hOldAttachedObject);
			m_hOldAttachedObject->VPhysicsDestroyObject();
		}
	}

	m_hOldAttachedObject = m_hAttachedObject;
}

#endif

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Update the pose parameter for the gun
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::UpdateElementPosition( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	float flElementPosition = m_ElementParameter.Interp( gpGlobals->curtime );

	if ( ShouldDrawUsingViewModel() )
	{
		if ( pOwner != NULL )	
		{
			CBaseViewModel *vm = pOwner->GetViewModel();
			
			if ( vm != NULL )
			{
				vm->SetPoseParameter( "active", flElementPosition );
			}
		}
	}
	else
	{
		SetPoseParameter("active", flElementPosition);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function for the client
//-----------------------------------------------------------------------------

void CWeaponPhysCannon::ClientThink( void )
{
	// Update our elements visually
	UpdateElementPosition();

	// Update our effects
	DoEffectIdle();
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Sprite scale factor 
//-----------------------------------------------------------------------------
inline float CWeaponPhysCannon::SpriteScaleFactor() 
{
	return IsMegaPhysCannon() ? 1.5f : 1.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::Deploy( void )
{
	CloseElements();
	DoEffect( EFFECT_READY );

	// Unbloat our bounds
	if ( IsMegaPhysCannon() )
	{
		CollisionProp()->UseTriggerBounds( false );
	}

	m_flTimeNextObjectPurge = gpGlobals->curtime;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::SetViewModel( void )
{
	if ( !IsMegaPhysCannon() )
	{
		BaseClass::SetViewModel();
		return;
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
	if ( vm == NULL )
		return;

	vm->SetWeaponModel( MEGACANNON_MODEL, this );
}

//-----------------------------------------------------------------------------
// Purpose: Force the cannon to drop anything it's carrying
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ForceDrop( void )
{
	CloseElements();
	DetachObject();
	StopEffects();
}


//-----------------------------------------------------------------------------
// Purpose: Drops its held entity if it matches the entity passed in
// Input  : *pTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::DropIfEntityHeld( CBaseEntity *pTarget )
{
	if ( pTarget == NULL )
		return false;

	CBaseEntity *pHeld = m_grabController.GetAttached();
	
	if ( pHeld == NULL )
		return false;

	if ( pHeld == pTarget )
	{
		ForceDrop();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Drop( const Vector &vecVelocity )
{
	ForceDrop();
	BaseClass::Drop( vecVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::CanHolster( void ) 
{ 
	//Don't holster this weapon if we're holding onto something
	if ( m_bActive )
		return false;

	return BaseClass::CanHolster();
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::Holster( CBaseCombatWeapon *pSwitchingTo )
{

	//Don't holster this weapon if we're holding onto something
	if ( m_bActive )
	{
		if ( m_grabController.GetAttached() == pSwitchingTo && 
			GetOwner()->Weapon_OwnsThisType( pSwitchingTo->GetClassname(), pSwitchingTo->GetSubType()) )
		{
		
		}
		else
		{
			m_bCanHolster = false;
			return m_bCanHolster;
		}
	}
#ifdef GAME_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
		pOwner->RumbleEffect( RUMBLE_PHYSCANNON_OPEN, 0, RUMBLE_FLAG_STOP );
	}
#endif
	ForceDrop();

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DryFire( void )
{
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	WeaponSound( EMPTY );
#ifdef GAME_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
		pOwner->RumbleEffect( RUMBLE_PISTOL, 0, RUMBLE_FLAG_RESTART );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PrimaryFireEffect( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	pOwner->ViewPunch( QAngle(-6, random->RandomInt(-2,2) ,0) );
#ifdef GAME_DLL
	color32 white = { 245, 245, 255, 32 };
	UTIL_ScreenFade( pOwner, white, 0.1f, 0.0f, FFADE_IN );
#endif
	WeaponSound( SINGLE );
}

#define	MAX_KNOCKBACK_FORCE	128

//-----------------------------------------------------------------------------
// Punt non-physics
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PuntNonVPhysics( CBaseEntity *pEntity, const Vector &forward, trace_t &tr )
{
#ifdef GAME_DLL
	float flDamage = 1.0f;
	if ( FClassnameIs( pEntity, "func_breakable" ) )
	{
		CBreakable *pBreak = dynamic_cast <CBreakable *>(pEntity);
		if ( pBreak && ( pBreak->GetMaterialType() == matGlass ) )
		{
			flDamage = physcannon_dmg_glass.GetFloat();
		}
	}

	CTakeDamageInfo	info;
	
	info.SetAttacker( GetOwner() );
	info.SetInflictor( this );
	info.SetDamage( flDamage );
	info.SetDamageType( DMG_CRUSH | DMG_PHYSGUN );
	info.SetDamageForce( forward );	// Scale?
	info.SetDamagePosition( tr.endpos );

	pEntity->DispatchTraceAttack( info, forward, &tr );

	ApplyMultiDamage();
#endif
	//Explosion effect
	DoEffect( EFFECT_LAUNCH, &tr.endpos );

	PrimaryFireEffect();
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.5f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;
}

//-----------------------------------------------------------------------------
// What happens when the physgun picks up something 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::Physgun_OnPhysGunPickup( CBaseEntity *pEntity, CBasePlayer *pOwner, PhysGunPickup_t reason )
{
	// If the target is debris, convert it to non-debris
	if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
	{
		// Interactive debris converts back to debris when it comes to rest
		pEntity->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}

	float mass = 0.0f;
	if( pEntity->VPhysicsGetObject() )
	{
		mass = pEntity->VPhysicsGetObject()->GetMass();
	}

	if( reason == PUNTED_BY_CANNON )
	{
#ifdef GAME_DLL
		pOwner->RumbleEffect( RUMBLE_357, 0, RUMBLE_FLAGS_NONE );
#endif
		RecordThrownObject( pEntity );
	}

#ifdef GAME_DLL
	// Warn Alyx if the player is punting a car around.
	if( hl2_episodic.GetBool() && mass > 250.0f )
	{
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		int nAIs = g_AI_Manager.NumAIs();

		for ( int i = 0; i < nAIs; i++ )
		{
			if( ppAIs[ i ]->Classify() == CLASS_PLAYER_ALLY_VITAL )
			{
				ppAIs[ i ]->DispatchInteraction( g_interactionPlayerPuntedHeavyObject, pEntity, pOwner );
			}
		}
	}
#endif
	Pickup_OnPhysGunPickup( pEntity, pOwner, reason );
}
#ifdef GAME_DLL
#endif

//-----------------------------------------------------------------------------
// Punt vphysics
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PuntVPhysics( CBaseEntity *pEntity, const Vector &vecForward, trace_t &tr )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	Assert(pOwner);

#ifdef GAME_DLL
	CTakeDamageInfo	info;

	Vector forward = vecForward;

	info.SetAttacker( GetOwner() );
	info.SetInflictor( this );
	info.SetDamage( 0.0f );
	info.SetDamageType( DMG_PHYSGUN );
	pEntity->DispatchTraceAttack( info, forward, &tr );
	ApplyMultiDamage();

		if ( Pickup_OnAttemptPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON ) )
	{
		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int listCount = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
		if ( !listCount )
		{
			//FIXME: Do we want to do this if there's no physics object?
			Physgun_OnPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON );
			DryFire();
			return;
		}
				
		if( forward.z < 0 )
		{
			//reflect, but flatten the trajectory out a bit so it's easier to hit standing targets
			forward.z *= -0.65f;
		}
		
		// NOTE: Do this first to enable motion (if disabled) - so forces will work
		// Tell the object it's been punted
		Physgun_OnPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON );

		// don't push vehicles that are attached to the world via fixed constraints
		// they will just wiggle...
		if ( (pList[0]->GetGameFlags() & FVPHYSICS_CONSTRAINT_STATIC) && pEntity->GetServerVehicle() )
		{
			forward.Init();
		}

		if ( !IsMegaPhysCannon() && !Pickup_ShouldPuntUseLaunchForces( pEntity, PHYSGUN_FORCE_PUNTED ) )
		{
			int i;

			// limit mass to avoid punting REALLY huge things
			float totalMass = 0;
			for ( i = 0; i < listCount; i++ )
			{
				totalMass += pList[i]->GetMass();
			}
			float maxMass = 250;
			IServerVehicle *pVehicle = pEntity->GetServerVehicle();
			if ( pVehicle )
			{
				maxMass *= 2.5;	// 625 for vehicles
			}
			float mass = MIN(totalMass, maxMass); // max 250kg of additional force

			// Put some spin on the object
			for ( i = 0; i < listCount; i++ )
			{
				const float hitObjectFactor = 0.5f;
				const float otherObjectFactor = 1.0f - hitObjectFactor;
  				// Must be light enough
				float ratio = pList[i]->GetMass() / totalMass;
				if ( pList[i] == pEntity->VPhysicsGetObject() )
				{
					ratio += hitObjectFactor;
					ratio = MIN(ratio,1.0f);
				}
				else
				{
					ratio *= otherObjectFactor;
				}
				pList[i]->ApplyForceCenter( forward * 15000.0f * ratio );
				pList[i]->ApplyForceOffset( forward * mass * 600.0f * ratio, tr.endpos );
			}
		}
		else
		{
			ApplyVelocityBasedForce( pEntity, vecForward, tr.endpos, PHYSGUN_FORCE_PUNTED );
		}
	}
#endif
	// Add recoil
	QAngle	recoil = QAngle( random->RandomFloat( 1.0f, 2.0f ), random->RandomFloat( -1.0f, 1.0f ), 0 );
	pOwner->ViewPunch( recoil );

	//Explosion effect
	DoEffect( EFFECT_LAUNCH, &tr.endpos );

	PrimaryFireEffect();
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.5f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;

	// Don't allow the gun to regrab a thrown object!!
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
}
#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: Applies velocity-based forces to throw the entity. This code is
//			called from both punt and launch carried code.
//			ASSUMES: that pEntity is a vphysics entity.
// Input  : - 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ApplyVelocityBasedForce( CBaseEntity *pEntity, const Vector &forward, const Vector &vecHitPos, PhysGunForce_t reason )
{
	// Get the launch velocity
	Vector vVel = Pickup_PhysGunLaunchVelocity( pEntity, forward, reason );
	
	// Get the launch angular impulse
	AngularImpulse aVel = Pickup_PhysGunLaunchAngularImpulse( pEntity, reason );
		
	// Get the physics object (MUST have one)
	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();
	if ( pPhysicsObject == NULL )
	{
		Assert( 0 );
		return;
	}

	// Affect the object
	CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp*>( pEntity );
	if ( pRagdoll == NULL )
	{
#ifdef HL2_EPISODIC
		// The jeep being punted needs special force overrides
		if ( reason == PHYSGUN_FORCE_PUNTED && pEntity->GetServerVehicle() )
		{
			// We want the point to emanate low on the vehicle to move it along the ground, not to twist it
			Vector vecFinalPos = vecHitPos;
			vecFinalPos.z = pEntity->GetAbsOrigin().z;
			pPhysicsObject->ApplyForceOffset( vVel, vecFinalPos );
		}
		else
		{
			pPhysicsObject->AddVelocity( &vVel, &aVel );
		}
#else

		pPhysicsObject->AddVelocity( &vVel, &aVel );

#endif // HL2_EPISODIC
	}
	else
	{
		Vector	vTempVel;
		AngularImpulse vTempAVel;

		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll( );
		for ( int j = 0; j < pRagdollPhys->listCount; ++j )
		{
			pRagdollPhys->list[j].pObject->AddVelocity( &vVel, &aVel ); 
		}
	}
}

//-----------------------------------------------------------------------------
// Punt non-physics
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PuntRagdoll( CBaseEntity *pEntity, const Vector &vecForward, trace_t &tr )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	Pickup_OnPhysGunDrop( pEntity, pOwner, LAUNCHED_BY_CANNON );

	CTakeDamageInfo	info;

	Vector forward = vecForward;
	info.SetAttacker( GetOwner() );
	info.SetInflictor( this );
	info.SetDamage( 0.0f );
	info.SetDamageType( DMG_PHYSGUN );
	pEntity->DispatchTraceAttack( info, forward, &tr );
	ApplyMultiDamage();

	if ( Pickup_OnAttemptPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON ) )
	{
		Physgun_OnPhysGunPickup( pEntity, pOwner, PUNTED_BY_CANNON );

		if( forward.z < 0 )
		{
			//reflect, but flatten the trajectory out a bit so it's easier to hit standing targets
			forward.z *= -0.65f;
		}
		
		Vector			vVel = forward * 1500;
		AngularImpulse	aVel = Pickup_PhysGunLaunchAngularImpulse( pEntity, PHYSGUN_FORCE_PUNTED );

		CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp*>( pEntity );
		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll( );

		int j;
		for ( j = 0; j < pRagdollPhys->listCount; ++j )
		{
			pRagdollPhys->list[j].pObject->AddVelocity( &vVel, NULL ); 
		}
	}
	
	// Add recoil
	QAngle	recoil = QAngle( random->RandomFloat( 1.0f, 2.0f ), random->RandomFloat( -1.0f, 1.0f ), 0 );
	pOwner->ViewPunch( recoil );

	//Explosion effect
	DoEffect( EFFECT_LAUNCH, &tr.endpos );

	PrimaryFireEffect();
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.5f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;

	// Don't allow the gun to regrab a thrown object!!
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
}
#endif

//-----------------------------------------------------------------------------
// Trace length
//-----------------------------------------------------------------------------
float CWeaponPhysCannon::TraceLength()
{
	if ( !IsMegaPhysCannon() )
	{
		return physcannon_tracelength.GetFloat();
	}
	
	return physcannon_mega_tracelength.GetFloat();
}

//-----------------------------------------------------------------------------
// If there's any special rejection code you need to do per entity then do it here
// This is kinda nasty but I'd hate to move more physcannon related stuff into CBaseEntity
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::EntityAllowsPunts( CBaseEntity *pEntity )
{
#ifdef GAME_DLL

	m_bAllowsPunts = true;

	if ( pEntity->HasSpawnFlags( SF_PHYSBOX_NEVER_PUNT ) )
	{
		CPhysBox *pPhysBox = dynamic_cast<CPhysBox*>(pEntity);

		if ( pPhysBox != NULL )
		{
			if ( pPhysBox->HasSpawnFlags( SF_PHYSBOX_NEVER_PUNT ) )
			{
				m_bAllowsPunts = false;
			}
		}
	}

	if ( pEntity->HasSpawnFlags( SF_WEAPON_NO_PHYSCANNON_PUNT ) )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>(pEntity);

		if ( pWeapon != NULL )
		{
			if ( pWeapon->HasSpawnFlags( SF_WEAPON_NO_PHYSCANNON_PUNT ) )
			{
				m_bAllowsPunts = false;
			}
		}
	}
#endif
	return m_bAllowsPunts;
}


//-----------------------------------------------------------------------------
// Purpose: 
//
// This mode is a toggle. Primary fire one time to pick up a physics object.
// With an object held, click primary fire again to drop object.
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::PrimaryAttack( void )
{
	if( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	if( m_bActive )
	{
		// Punch the object being held!!
		Vector forward;
		pOwner->EyeVectors( &forward );

		// Validate the item is within punt range
		CBaseEntity *pHeld = m_grabController.GetAttached();
		Assert( pHeld != NULL );

		if ( pHeld != NULL )
		{
			float heldDist = ( pHeld->WorldSpaceCenter() - pOwner->WorldSpaceCenter() ).Length();

			if ( heldDist > physcannon_tracelength.GetFloat() )
			{
				// We can't punt this yet
				DryFire();
				return;
			}
		}

		LaunchObject( forward, physcannon_maxforce.GetFloat() );

		PrimaryFireEffect();
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
		return;
	}

	// If not active, just issue a physics punch in the world.
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	Vector forward;
	pOwner->EyeVectors( &forward );

	// NOTE: Notice we're *not* using the mega tracelength here
	// when you have the mega cannon. Punting has shorter range.
	Vector start, end;
	start = pOwner->Weapon_ShootPosition();
	float flPuntDistance = physcannon_tracelength.GetFloat();
	VectorMA( start, flPuntDistance, forward, end );

	trace_t tr;
	UTIL_PhyscannonTraceHull( start, end, -Vector(8,8,8), Vector(8,8,8), pOwner, &tr );
	bool bValid = true;
	CBaseEntity *pEntity = tr.m_pEnt;
	if ( tr.fraction == 1 || !tr.m_pEnt || tr.m_pEnt->IsEFlagSet( EFL_NO_PHYSCANNON_INTERACTION ) )
	{
		bValid = false;
	}
	else if ( (pEntity->GetMoveType() != MOVETYPE_VPHYSICS) && ( pEntity->m_takedamage == DAMAGE_NO ) )
	{
		bValid = false;
	}

	// If the entity we've hit is invalid, try a traceline instead
	if ( !bValid )
	{
		UTIL_PhyscannonTraceLine( start, end, pOwner, &tr );
		if ( tr.fraction == 1 || !tr.m_pEnt || tr.m_pEnt->IsEFlagSet( EFL_NO_PHYSCANNON_INTERACTION ) )
		{
#ifdef GAME_DLL
			if( hl2_episodic.GetBool() )
			{
				// Try to find something in a very small cone. 
				CBaseEntity *pObject = FindObjectInCone( start, forward, physcannon_punt_cone.GetFloat() );

				if( pObject )
				{
					// Trace to the object.
					UTIL_PhyscannonTraceLine( start, pObject->WorldSpaceCenter(), pOwner, &tr );

					if( tr.m_pEnt && tr.m_pEnt == pObject && !(pObject->IsEFlagSet(EFL_NO_PHYSCANNON_INTERACTION)) )
					{
						bValid = true;
						pEntity = pObject;
					}
				}
			}
#endif
		}
		else
		{
			bValid = true;
			pEntity = tr.m_pEnt;
		}
	}

	if ( ToPortalPlayer( pOwner )->IsHeldObjectOnOppositeSideOfPortal() )
	{
		CProp_Portal *pPortal = ToPortalPlayer( pOwner )->GetHeldObjectPortal();
		Assert(pPortal);
		UTIL_Portal_VectorTransform( pPortal->MatrixThisToLinked(), forward, forward );
	}

	if( !bValid )
	{
		DryFire();
		return;
	}

	// See if we hit something
	if ( pEntity->GetMoveType() != MOVETYPE_VPHYSICS )
	{
		if ( pEntity->m_takedamage == DAMAGE_NO )
		{
			DryFire();
			return;
		}
#ifdef GAME_DLL
		if( GetOwner()->IsPlayer() && !IsMegaPhysCannon() )
		{
			// Don't let the player zap any NPC's except regular antlions and headcrabs.
			if( pEntity->IsNPC() && pEntity->Classify() != CLASS_HEADCRAB && !FClassnameIs(pEntity, "npc_antlion") )
			{
				DryFire();
				return;
			}
		}

		if ( IsMegaPhysCannon() )
		{
			if ( pEntity->IsNPC() && !pEntity->IsEFlagSet( EFL_NO_MEGAPHYSCANNON_RAGDOLL ) && pEntity->MyNPCPointer()->CanBecomeRagdoll() )
			{
				CTakeDamageInfo info( pOwner, pOwner, 1.0f, DMG_GENERIC );
				CBaseEntity *pRagdoll = CreateServerRagdoll( pEntity->MyNPCPointer(), 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
				PhysSetEntityGameFlags( pRagdoll, FVPHYSICS_NO_SELF_COLLISIONS );
				pRagdoll->SetCollisionBounds( pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs() );

				// Necessary to cause it to do the appropriate death cleanup
				CTakeDamageInfo ragdollInfo( pOwner, pOwner, 10000.0, DMG_PHYSGUN | DMG_REMOVENORAGDOLL );
				pEntity->TakeDamage( ragdollInfo );

				PuntRagdoll( pRagdoll, forward, tr );
				return;
			}
		}
#endif
		PuntNonVPhysics( pEntity, forward, tr );
	}
	else
	{
		if ( EntityAllowsPunts( pEntity) == false )
		{
			DryFire();
			return;
		}

		if ( !IsMegaPhysCannon() )
		{
			if ( pEntity->VPhysicsIsFlesh( ) )
			{
				DryFire();
				return;
			}
			PuntVPhysics( pEntity, forward, tr );
		}
		else
		{
#ifdef GAME_DLL
			if ( dynamic_cast<CRagdollProp*>(pEntity) )
			{
				PuntRagdoll( pEntity, forward, tr );
			}
			else
#endif
			{
				PuntVPhysics( pEntity, forward, tr );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Click secondary attack whilst holding an object to hurl it.
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::SecondaryAttack( void )
{

	if ( m_flNextSecondaryAttack > gpGlobals->curtime )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	// See if we should drop a held item
	if ( ( m_bActive ) && ( pOwner->m_afButtonPressed & IN_ATTACK2 ) )
	{
		// Drop the held object
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;

		DetachObject();

		DoEffect( EFFECT_READY );

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}
	else
	{
		// Otherwise pick it up
		FindObjectResult_t result = FindObject();
		switch ( result )
		{
		case OBJECT_FOUND:
			WeaponSound( SPECIAL1 );
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;

			// We found an object. Debounce the button
			m_nAttack2Debounce |= pOwner->m_nButtons;
			break;

		case OBJECT_NOT_FOUND:
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.1f;
			CloseElements();
			break;

		case OBJECT_BEING_DETACHED:
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.01f;
			break;
		}

		DoEffect( EFFECT_HOLDING );
	}
}	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::WeaponIdle( void )
{
	if ( HasWeaponIdleTimeElapsed() )
	{
		if ( m_bActive )
		{
			//Shake when holding an item
			SendWeaponAnim( ACT_VM_RELOAD );
		}
		else
		{
			//Otherwise idle simply
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::AttachObject( CBaseEntity *pObject, const Vector &vPosition )
{
	
	if ( m_bActive )
		return false;

	if ( CanPickupObject( pObject ) == false )
		return false;

	m_grabController.SetIgnorePitch( false );
	m_grabController.SetAngleAlignment( 0 );

	bool bKilledByGrab = false;

	bool bIsMegaPhysCannon = IsMegaPhysCannon();

	if ( bIsMegaPhysCannon )
	{
		if ( pObject->IsNPC() && !pObject->IsEFlagSet( EFL_NO_MEGAPHYSCANNON_RAGDOLL ) )
		{
#ifdef GAME_DLL
			Assert( pObject->MyNPCPointer()->CanBecomeRagdoll() );
			CTakeDamageInfo info( GetOwner(), GetOwner(), 1.0f, DMG_GENERIC );
			CBaseEntity *pRagdoll = CreateServerRagdoll( pObject->MyNPCPointer(), 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
			PhysSetEntityGameFlags( pRagdoll, FVPHYSICS_NO_SELF_COLLISIONS );

			pRagdoll->SetCollisionBounds( pObject->CollisionProp()->OBBMins(), pObject->CollisionProp()->OBBMaxs() );

			// Necessary to cause it to do the appropriate death cleanup
			CTakeDamageInfo ragdollInfo( GetOwner(), GetOwner(), 10000.0, DMG_PHYSGUN | DMG_REMOVENORAGDOLL );
			pObject->TakeDamage( ragdollInfo );

			// Now we act on the ragdoll for the remainder of the time
			pObject = pRagdoll;
#endif
			bKilledByGrab = true;
		}
	}

	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();

	// Must be valid
	if ( !pPhysics )
		return false;

	CPortal_Player *pOwner = ToPortalPlayer( GetOwner() );

	m_bActive = true;
	if( pOwner )
	{
#ifdef GAME_DLL
#ifdef HL2_EPISODIC
		CBreakableProp *pProp = dynamic_cast< CBreakableProp * >( pObject );

		if ( pProp && pProp->HasInteraction( PROPINTER_PHYSGUN_CREATE_FLARE ) )
		{
			pOwner->FlashlightTurnOff();
		}
#endif
#endif

		// NOTE: This can change the mass; so it must be done before max speed setting
		Physgun_OnPhysGunPickup( pObject, pOwner, PICKED_UP_BY_CANNON );
	}

	pObject->SetPhysgun(this);

	// NOTE :This must happen after OnPhysGunPickup because that can change the mass
	m_grabController.AttachEntity( pOwner, pObject, pPhysics, bIsMegaPhysCannon, vPosition, (!bKilledByGrab) );

	m_hAttachedObject = pObject;

	m_attachedPositionObjectSpace = m_grabController.m_attachedPositionObjectSpace;
	m_attachedAnglesPlayerSpace = m_grabController.m_attachedAnglesPlayerSpace;
	m_bResetOwnerEntity = false;

	if ( m_hAttachedObject->GetOwnerEntity() == NULL )
	{
		m_hAttachedObject->SetOwnerEntity( pOwner );
		m_bResetOwnerEntity = true;
	}

	if( pOwner )
	{
#ifdef WIN32
		// NVNT set the players constant force to simulate holding mass
		HapticSetConstantForce(pOwner,clamp(m_grabController.GetLoadWeight()*0.05,1,5)*Vector(0,-1,0));
#endif
		pOwner->EnableSprint( false );
		float	loadWeight = ( 1.0f - GetLoadPercentage() );
		float	maxSpeed = hl2_walkspeed.GetFloat() + ( ( hl2_normspeed.GetFloat() - hl2_walkspeed.GetFloat() ) * loadWeight );

		//Msg( "Load perc: %f -- Movement speed: %f/%f\n", loadWeight, maxSpeed, hl2_normspeed.GetFloat() );
		pOwner->SetMaxSpeed( maxSpeed );
	}

	// Don't drop again for a slight delay, in case they were pulling objects near them
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.4f;

	DoEffect( EFFECT_HOLDING );
	OpenElements();

	if ( GetMotorSound() )
	{
		(CSoundEnvelopeController::GetController()).Play( GetMotorSound(), 0.0f, 50 );
		(CSoundEnvelopeController::GetController()).SoundChangePitch( GetMotorSound(), 100, 0.5f );
		(CSoundEnvelopeController::GetController()).SoundChangeVolume( GetMotorSound(), 0.8f, 0.5f );
	}

	return true;
}
void CWeaponPhysCannon::FindObjectTrace( CBasePlayer *pPlayer, trace_t *pTraceResult )
{
	Vector forward;
	pPlayer->EyeVectors( &forward );

	// Setup our positions
	Vector	start = pPlayer->Weapon_ShootPosition();
	float	testLength = TraceLength() * 4.0f;
	Vector	end = start + forward * testLength;
#ifdef GAME_DLL
	if( IsMegaPhysCannon() && hl2_episodic.GetBool() )
	{
		Vector vecAutoAimDir = pPlayer->GetAutoaimVector( 1.0f, testLength );
		end = start + vecAutoAimDir * testLength;
	}
#endif
	// Try to find an object by looking straight ahead
	UTIL_PhyscannonTraceLine( start, end, pPlayer, pTraceResult );

	// Try again with a hull trace
	if ( !pTraceResult->DidHitNonWorldEntity() )
	{
		UTIL_PhyscannonTraceHull( start, end, -Vector(4,4,4), Vector(4,4,4), pPlayer, pTraceResult );
	}
}

CWeaponPhysCannon::FindObjectResult_t CWeaponPhysCannon::FindObject( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	Assert( pPlayer );
	if ( pPlayer == NULL )
		return OBJECT_NOT_FOUND;
	
	Vector forward;
	pPlayer->EyeVectors( &forward );

	// Setup our positions
	Vector	start = pPlayer->Weapon_ShootPosition();
	float	testLength = TraceLength() * 4.0f;
	Vector	end = start + forward * testLength;
	trace_t tr;
	FindObjectTrace( pPlayer, &tr );
	CBaseEntity *pEntity = tr.m_pEnt ? tr.m_pEnt->GetRootMoveParent() : NULL;
	bool	bAttach = false;
	bool	bPull = false;

	// If we hit something, pick it up or pull it
	if ( ( tr.fraction != 1.0f ) && ( tr.m_pEnt ) && ( tr.m_pEnt->IsWorld() == false ) )
	{
		// Attempt to attach if within range
		if ( tr.fraction <= 0.25f )
		{
			bAttach = true;
		}
		else if ( tr.fraction > 0.25f )
		{
			bPull = true;
		}
	}
	

	// Find anything within a general cone in front
	CBaseEntity *pConeEntity = NULL;
#ifdef GAME_DLL
	if ( !IsMegaPhysCannon() )
	{
		if (!bAttach && !bPull)
		{
			pConeEntity = FindObjectInCone( start, forward, physcannon_cone.GetFloat() );
		}
	}
	else
	{
		pConeEntity = MegaPhysCannonFindObjectInCone( start, forward, 
			physcannon_cone.GetFloat(), physcannon_ball_cone.GetFloat(), bAttach || bPull );
	}
#endif
	if ( pConeEntity )
	{
		pEntity = pConeEntity;

		// If the object is near, grab it. Else, pull it a bit.
		if ( pEntity->WorldSpaceCenter().DistToSqr( start ) <= (testLength * testLength) )
		{
			bAttach = true;
		}
		else
		{
			bPull = true;
		}
	}

	if ( CanPickupObject( pEntity ) == false )
	{
		CBaseEntity *pNewObject = Pickup_OnFailedPhysGunPickup( pEntity, start );

		if ( pNewObject && CanPickupObject( pNewObject ) )
		{
			pEntity = pNewObject;
		}
		else
		{
			// Make a noise to signify we can't pick this up
			if ( !m_flLastDenySoundPlayed )
			{
				m_flLastDenySoundPlayed = true;
				WeaponSound( SPECIAL3 );
			}

			return OBJECT_NOT_FOUND;
		}
	}
	// Check to see if the object is constrained + needs to be ripped off...
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( !Pickup_OnAttemptPhysGunPickup( pEntity, pOwner, PICKED_UP_BY_CANNON ) )
		return OBJECT_BEING_DETACHED;

	if ( bAttach )
	{
		CWeaponPhysCannon *pPhysgun = pEntity->GetPhysgun();
		CGrabController *pController = pEntity->GetGrabController();
		//Allow steal other objects if we are the megacannon but the object's physcannon isn't mega.
		if (pPhysgun && pPhysgun != this)
		{
			if (!IsMegaPhysCannon())
				return OBJECT_NOT_FOUND;

			if (pPhysgun->IsMegaPhysCannon())
				return OBJECT_NOT_FOUND;
		}

		if (pController && pController != &m_grabController)
			pController->DetachEntity(false);

		return AttachObject( pEntity, tr.endpos ) ? OBJECT_FOUND : OBJECT_NOT_FOUND;
	}

	if ( !bPull )
		return OBJECT_NOT_FOUND;

	// FIXME: This needs to be run through the CanPickupObject logic
	IPhysicsObject *pObj = pEntity->VPhysicsGetObject();
	if ( !pObj )
		return OBJECT_NOT_FOUND;

	// If we're too far, simply start to pull the object towards us
	Vector	pullDir = start - pEntity->WorldSpaceCenter();
	VectorNormalize( pullDir );
	pullDir *= IsMegaPhysCannon() ? physcannon_mega_pullforce.GetFloat() : physcannon_pullforce.GetFloat();
#ifdef GAME_DLL
	float mass = PhysGetEntityMass( pEntity );
	if ( mass < 50.0f )
	{
		pullDir *= (mass + 0.5) * (1/50.0f);
	}
#endif
	// Nudge it towards us
	pObj->ApplyForceCenter( pullDir );
	return OBJECT_NOT_FOUND;

}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponPhysCannon::MegaPhysCannonFindObjectInCone( const Vector &vecOrigin, 
   const Vector &vecDir, float flCone, float flCombineBallCone, bool bOnlyCombineBalls )
{
	// Find the nearest physics-based item in a cone in front of me.
	CBaseEntity *list[1024];
	float flMaxDist = TraceLength() + 1.0;
	float flNearestDist = flMaxDist;
	bool bNearestIsCombineBall = bOnlyCombineBalls ? true : false;
	Vector mins = vecOrigin - Vector( flNearestDist, flNearestDist, flNearestDist );
	Vector maxs = vecOrigin + Vector( flNearestDist, flNearestDist, flNearestDist );

	CBaseEntity *pNearest = NULL;

	int count = UTIL_EntitiesInBox( list, 1024, mins, maxs, 0 );
	for( int i = 0 ; i < count ; i++ )
	{
		if ( !list[ i ]->VPhysicsGetObject() )
			continue;

		bool bIsCombineBall = FClassnameIs( list[ i ], "prop_combine_ball" );
		if ( !bIsCombineBall && bNearestIsCombineBall )
			continue;

		// Closer than other objects
		Vector los;
		VectorSubtract( list[ i ]->WorldSpaceCenter(), vecOrigin, los );
		float flDist = VectorNormalize( los );

		if ( !bIsCombineBall || bNearestIsCombineBall )
		{
			// Closer than other objects
			if( flDist >= flNearestDist )
				continue;

			// Cull to the cone
			if ( DotProduct( los, vecDir ) <= flCone )
				continue;
		}
		else
		{
			// Close enough?
			if ( flDist >= flMaxDist )
				continue;

			// Cull to the cone
			if ( DotProduct( los, vecDir ) <= flCone )
				continue;

			// NOW: If it's either closer than nearest dist or within the ball cone, use it!
			if ( (flDist > flNearestDist) && (DotProduct( los, vecDir ) <= flCombineBallCone) )
				continue;
		}

		// Make sure it isn't occluded!
		trace_t tr;
		UTIL_PhyscannonTraceLine( vecOrigin, list[ i ]->WorldSpaceCenter(), GetOwner(), &tr );
		if( tr.m_pEnt == list[ i ] )
		{
			flNearestDist = flDist;
			pNearest = list[ i ];
			bNearestIsCombineBall = bIsCombineBall;
		}
	}

	return pNearest;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponPhysCannon::FindObjectInCone( const Vector &vecOrigin, const Vector &vecDir, float flCone )
{
	// Find the nearest physics-based item in a cone in front of me.
	CBaseEntity *list[256];
	float flNearestDist = physcannon_tracelength.GetFloat() + 1.0; //Use regular distance.
	Vector mins = vecOrigin - Vector( flNearestDist, flNearestDist, flNearestDist );
	Vector maxs = vecOrigin + Vector( flNearestDist, flNearestDist, flNearestDist );

	CBaseEntity *pNearest = NULL;

	int count = UTIL_EntitiesInBox( list, 256, mins, maxs, 0 );
	for( int i = 0 ; i < count ; i++ )
	{
		if ( !list[ i ]->VPhysicsGetObject() )
			continue;

		// Closer than other objects
		Vector los = ( list[ i ]->WorldSpaceCenter() - vecOrigin );
		float flDist = VectorNormalize( los );
		if( flDist >= flNearestDist )
			continue;

		// Cull to the cone
		if ( DotProduct( los, vecDir ) <= flCone )
			continue;

		// Make sure it isn't occluded!
		trace_t tr;
		UTIL_PhyscannonTraceLine( vecOrigin, list[ i ]->WorldSpaceCenter(), GetOwner(), &tr );
		if( tr.m_pEnt == list[ i ] )
		{
			flNearestDist = flDist;
			pNearest = list[ i ];
		}
	}

	return pNearest;
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CGrabController::UpdateObject( CPortal_Player *pPlayer, float flError )
{
	CBaseEntity *pPenetratedEntity = m_PenetratedEntity.Get();
	if( pPenetratedEntity )
	{
		//FindClosestPassableSpace( pPenetratedEntity, Vector( 0.0f, 0.0f, 1.0f ) );
		IPhysicsObject *pPhysObject = pPenetratedEntity->VPhysicsGetObject();
		if( pPhysObject )
			pPhysObject->Wake();
#ifdef CLIENT_DLL
		//pPhysObject = pPlayer->GetHeldPhysicsPortal();
#endif


		m_PenetratedEntity = NULL; //assume we won
	}

	CBaseEntity *pEntity = GetAttached();
	if ( !pEntity || ComputeError() > flError || pPlayer->GetGroundEntity() == pEntity || !pEntity->VPhysicsGetObject() )
	{
		return false;
	}

	//Adrian: Oops, our object became motion disabled, let go!
	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if ( (pPhys && pPhys->IsMoveable() == false) || !pPhys )
	{
		return false;
	}

	Vector forward, right, up;
	QAngle playerAngles = pPlayer->EyeAngles();
	float pitch = AngleDistance(playerAngles.x,0);
	if( !m_bAllowObjectOverhead )
	{
		playerAngles.x = clamp( pitch, -75, 75 );
	}
	else
	{
		playerAngles.x = clamp( pitch, -90, 75 );
	}
	AngleVectors( playerAngles, &forward, &right, &up );
	if ( PortalGameRules()->MegaPhyscannonActive() )
	{
		Vector los = ( pEntity->WorldSpaceCenter() - pPlayer->Weapon_ShootPosition() );
		VectorNormalize( los );

		float flDot = DotProduct( los, forward );

		//Let go of the item if we turn around too fast.
		if ( flDot <= 0.35f )
			return false;
	}

	Vector start = pPlayer->Weapon_ShootPosition();

	// If the player is upside down then we need to hold the box closer to their feet.
	if ( up.z < 0.0f )
		start += pPlayer->GetViewOffset() * up.z;
	if ( right.z < 0.0f )
		start += pPlayer->GetViewOffset() * right.z;

	CPortal_Player *pPortalPlayer = ToPortalPlayer( pPlayer );
	Assert(pPortalPlayer);
	// Find out if it's being held across a portal
	bool bLookingAtHeldPortal = true;
	CProp_Portal *pPortal = pPortalPlayer->GetHeldObjectPortal();

	if ( !pPortal )
	{
		// If the portal is invalid make sure we don't try to hold it across the portal
		pPortalPlayer->SetHeldObjectOnOppositeSideOfPortal( false );
	}

	if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
	{
		Ray_t rayPortalTest;
		rayPortalTest.Init( start, start + forward * 1024.0f );

		// Check if we're looking at the portal we're holding through
		if ( pPortal )
		{
#ifdef GAME_DLL
			if ( UTIL_IntersectRayWithPortal( rayPortalTest, pPortal ) < 0.0f )
			{
				bLookingAtHeldPortal = false;
			}
#endif
		}

		// If our end point hasn't gone into the portal yet we at least need to know what portal is in front of us
		else
		{
			int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
			if( iPortalCount != 0 )
			{
				CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
				Assert(pPortals);
				float fMinDist = 2.0f;
				for( int i = 0; i != iPortalCount; ++i )
				{
					CProp_Portal *pTempPortal = pPortals[i];
					Assert(pTempPortal);
					if( pTempPortal->IsActive() &&
						(pTempPortal->m_hLinkedPortal.Get() != NULL) )
					{
						float fDist = UTIL_IntersectRayWithPortal( rayPortalTest, pTempPortal );
						if( (fDist >= 0.0f) && (fDist < fMinDist) )
						{
							fMinDist = fDist;
							pPortal = pTempPortal;
						}
					}
				}
			}
		}
	}
	else
	{
		pPortal = NULL;
	}

	QAngle qEntityAngles = pEntity->GetAbsAngles();

	if ( pPortal )
	{
		// If the portal isn't linked we need to drop the object
		if ( !pPortal->m_hLinkedPortal.Get() )
		{
			pPlayer->ForceDropOfCarriedPhysObjects(pEntity);

			return false;
		}

		UTIL_Portal_AngleTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), qEntityAngles, qEntityAngles );
	}
	
	// Now clamp a sphere of object radius at end to the player's bbox
#
	Vector radial = physcollision->CollideGetExtent( pPhys->GetCollide(), vec3_origin, qEntityAngles, -forward );
	Vector player2d = pPlayer->CollisionProp()->OBBMaxs();
	float playerRadius = player2d.Length2D();

	float radius = playerRadius + radial.Length();

	float distance = 24 + ( radius * 2.0f );

	// Add the prop's distance offset
	distance += m_flDistanceOffset;

	Vector end = start + ( forward * distance );

	trace_t	tr;
	CTraceFilterSkipTwoEntities traceFilter( pPlayer, pEntity, COLLISION_GROUP_NONE );
	Ray_t ray;
	ray.Init( start, end );
	//enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );
	UTIL_Portal_TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

	if ( tr.fraction < 0.5 )
	{
		end = start + forward * (radius*0.5f);
	}
	else if ( tr.fraction <= 1.0f )
	{
		end = start + forward * ( distance - radius );
	}
	Vector playerMins, playerMaxs, nearest;
	pPlayer->CollisionProp()->WorldSpaceAABB( &playerMins, &playerMaxs );
	Vector playerLine = pPlayer->CollisionProp()->WorldSpaceCenter();
	CalcClosestPointOnLine( end, playerLine+Vector(0,0,playerMins.z), playerLine+Vector(0,0,playerMaxs.z), nearest, NULL );

	if( !m_bAllowObjectOverhead )
	{
		Vector delta = end - nearest;
		float len = VectorNormalize(delta);
		if ( len < radius )
		{
			end = nearest + radius * delta;
		}
	}

	QAngle angles = TransformAnglesFromPlayerSpace( m_attachedAnglesPlayerSpace, pPlayer );

	//Show overlays of radius
	if ( g_debug_physcannon.GetBool() )
	{
		NDebugOverlay::Box( end, -Vector( 2,2,2 ), Vector(2,2,2), 0, 255, 0, true, 0 );

		NDebugOverlay::Box( GetAttached()->WorldSpaceCenter(), 
							-Vector( radius, radius, radius), 
							Vector( radius, radius, radius ),
							255, 0, 0,
							true,
							0.0f );
	}

	// If it has a preferred orientation, update to ensure we're still oriented correctly.
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );
	// We may be holding a prop that has preferred carry angles
	if ( m_bHasPreferredCarryAngles )
	{
		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );
		angles = TransformAnglesToWorldSpace( m_vecPreferredCarryAngles, tmp );
	}
	
	matrix3x4_t attachedToWorld;
	Vector offset;
	AngleMatrix( angles, attachedToWorld );
	VectorRotate( m_attachedPositionObjectSpace, attachedToWorld, offset );

	// Translate hold position and angles across portal
	if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
	{
		CProp_Portal *pPortalLinked = pPortal->m_hLinkedPortal;
		if ( pPortal && pPortal->IsActive() && pPortalLinked != NULL )
		{
			Vector vTeleportedPosition;
			QAngle qTeleportedAngles;

			if ( !bLookingAtHeldPortal && ( start - pPortal->GetAbsOrigin() ).Length() > distance - radius )
			{
				// Pull the object through the portal
				Vector vPortalLinkedForward;
				pPortalLinked->GetVectors( &vPortalLinkedForward, NULL, NULL );
				vTeleportedPosition = pPortalLinked->GetAbsOrigin() - vPortalLinkedForward * ( 1.0f + offset.Length() );
				qTeleportedAngles = pPortalLinked->GetAbsAngles();
			}
			else
			{
				// Translate hold position and angles across the portal
				VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
				UTIL_Portal_PointTransform( matThisToLinked, end - offset, vTeleportedPosition );
				UTIL_Portal_AngleTransform( matThisToLinked, angles, qTeleportedAngles );
			}

			SetTargetPosition( vTeleportedPosition, qTeleportedAngles );
			pPortalPlayer->SetHeldObjectPortal( pPortal );
		}
		else
		{
#ifdef GAME_DLL
			pPlayer->ForceDropOfCarriedPhysObjects(pEntity);
#endif
		}
	}
	else
	{
		SetTargetPosition( end - offset, angles );
		pPortalPlayer->SetHeldObjectPortal( NULL );
	}

	return true;
}



void CWeaponPhysCannon::UpdateObject( void )
{
	CPortal_Player *pPlayer = ToPortalPlayer( GetOwner() );
	Assert( pPlayer );

	float flError = IsMegaPhysCannon() ? 18 : 12;
	if ( !m_grabController.UpdateObject( pPlayer, flError ) )
	{
		pPlayer->SetHeldObjectOnOppositeSideOfPortal( false );
		DetachObject();
		return;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DetachObject( bool playSound, bool wasLaunched )
{

#ifdef GAME_DLL
	if ( m_bActive == false )
		return;

	CPortal_Player *pOwner = (CPortal_Player *)ToBasePlayer( GetOwner() );
	if( pOwner != NULL )
	{
		pOwner->EnableSprint( true );
		pOwner->SetMaxSpeed( hl2_normspeed.GetFloat() );
		pOwner->SetHeldObjectOnOppositeSideOfPortal( false );
#ifdef GAME_DLL
		if( wasLaunched )
		{
			pOwner->RumbleEffect( RUMBLE_357, 0, RUMBLE_FLAG_RESTART );
		}
#endif
#ifdef WIN32
		// NVNT clear constant force
		HapticSetConstantForce(pOwner,Vector(0,0,0));
#endif
	}

	CBaseEntity *pObject = m_grabController.GetAttached();
	Assert(pObject);

	if (pObject)
	pObject->SetPhysgun(NULL);

	m_grabController.DetachEntity( wasLaunched );

	if ( pObject != NULL )
	{
		Pickup_OnPhysGunDrop( pObject, pOwner, wasLaunched ? LAUNCHED_BY_CANNON : DROPPED_BY_CANNON );
	}

	// Stop our looping sound
	if ( GetMotorSound() )
	{
		(CSoundEnvelopeController::GetController()).SoundChangeVolume( GetMotorSound(), 0.0f, 1.0f );
		(CSoundEnvelopeController::GetController()).SoundChangePitch( GetMotorSound(), 50, 1.0f );
	}
	
	if ( pObject && m_bResetOwnerEntity == true )
	{
		pObject->SetOwnerEntity( NULL );
	}


	m_hAttachedObject = NULL;
	m_bActive = false;
	
	if(pObject)
	RecordThrownObject( pObject );
#else
	if (m_hAttachedObject && cl_predict->GetInt() && cl_predicted_grabbing.GetBool() )
	{
		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity(m_hAttachedObject);
		if (pSimulator)
			pSimulator->ReleaseOwnershipOfEntity(m_hAttachedObject);
		m_hAttachedObject->VPhysicsDestroyObject();
	}
#endif
	
	if ( playSound )
	{
		//Play the detach sound
		WeaponSound( MELEE_MISS );
	}

}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ItemPreFrame()
{
	BaseClass::ItemPreFrame();
	
#ifdef CLIENT_DLL
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();

	if ( localplayer && !localplayer->IsObserver() )
		ManagePredictedObject();
#endif

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;
#ifdef GAME_DLL
	m_flElementPosition = UTIL_Approach( m_flElementDestination, m_flElementPosition, 0.1f );
#endif
	CBaseViewModel *vm = pOwner->GetViewModel();
	
	if ( vm != NULL )
	{
		vm->SetPoseParameter( "active", m_flElementPosition );
	}

	// Update the object if the weapon is switched on.
	if( m_bActive )
	{
		UpdateObject();
	}

	if( gpGlobals->curtime >= m_flTimeNextObjectPurge )
	{
		PurgeThrownObjects();
		m_flTimeNextObjectPurge = gpGlobals->curtime + 0.5f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::CheckForTarget( void )
{
	//See if we're suppressing this
	if ( m_flCheckSuppressTime > gpGlobals->curtime )
		return;

	// holstered
	if ( IsEffectActive( EF_NODRAW ) )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	if ( m_bActive )
		return;

	trace_t	tr;
	FindObjectTrace( pOwner, &tr );

	if ( ( tr.fraction != 1.0f ) && ( tr.m_pEnt != NULL ) )
	{
		float dist = (tr.endpos - tr.startpos).Length();
		if ( dist <= TraceLength() )
		{
			// FIXME: Try just having the elements always open when pointed at a physics object

			if (CanPickupObject(tr.m_pEnt) || Pickup_ForcePhysGunOpen(tr.m_pEnt, pOwner))
			// if ( ( tr.m_pEnt->VPhysicsGetObject() != NULL ) && ( tr.m_pEnt->GetMoveType() == MOVETYPE_VPHYSICS ) )
			{
				m_nChangeState = ELEMENT_STATE_NONE;
				OpenElements();
				return;
			}
		}
	}

	// Close the elements after a delay to prevent overact state switching
	if ( ( m_flElementDebounce < gpGlobals->curtime ) && ( m_nChangeState == ELEMENT_STATE_NONE ) )
	{
		m_nChangeState = ELEMENT_STATE_CLOSED;
		m_flElementDebounce = gpGlobals->curtime + 0.5f;
	}
}


//-----------------------------------------------------------------------------
// Begin upgrading!
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::BeginUpgrade()
{
	if ( IsMegaPhysCannon() )
		return;
	
	if ( m_bIsCurrentlyUpgrading )
		return;

	SetSequence( SelectWeightedSequence( ACT_PHYSCANNON_UPGRADE ) );
	ResetSequenceInfo();

	m_bIsCurrentlyUpgrading = true;

	SetContextThink( &CWeaponPhysCannon::WaitForUpgradeThink, gpGlobals->curtime + 6.0f, s_pWaitForUpgradeContext );

	EmitSound( "WeaponDissolve.Charge" );

	// Bloat our bounds
	CollisionProp()->UseTriggerBounds( true, 32.0f );

	// Turn on the new skin
	m_nSkin = MEGACANNON_SKIN;
}


//-----------------------------------------------------------------------------
// Wait until we're done upgrading
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::WaitForUpgradeThink()
{
	Assert( m_bIsCurrentlyUpgrading );

	StudioFrameAdvance();
	if ( !IsActivityFinished() )
	{
		SetContextThink( &CWeaponPhysCannon::WaitForUpgradeThink, gpGlobals->curtime + 0.1f, s_pWaitForUpgradeContext );
		return;
	}
#ifdef GAME_DLL
	if ( !GlobalEntity_IsInTable( "super_phys_gun" ) )
	{
		GlobalEntity_Add( MAKE_STRING("super_phys_gun"), gpGlobals->mapname, GLOBAL_ON );
	}
	else
	{
		GlobalEntity_SetState( MAKE_STRING("super_phys_gun"), GLOBAL_ON );
	}
#endif
	m_bIsCurrentlyUpgrading = false;

	// This is necessary to get the effects to look different
	DestroyEffects();
#ifdef GAME_DLL
	// HACK: Hacky notification back to the level that we've finish upgrading
	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, "script_physcannon_upgrade" );
	if ( pEnt )
	{
		variant_t emptyVariant;
		pEnt->AcceptInput( "Trigger", this, this, emptyVariant, 0 );
	}
#endif
	StopSound( "WeaponDissolve.Charge" );

	// Re-enable weapon pickup
	AddSolidFlags( FSOLID_TRIGGER );

	SetContextThink( NULL, gpGlobals->curtime, s_pWaitForUpgradeContext );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectIdle( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		StopEffects();
		return;
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	if ( m_bPhyscannonState != IsMegaPhysCannon() )
	{
		DestroyEffects();
		StartEffects();

		m_bPhyscannonState = IsMegaPhysCannon();

		//This means we just switched to regular physcannon this frame.
		if ( m_bPhyscannonState == false )
		{
			EmitSound( "Weapon_Physgun.Off" );

			ForceDrop();
#ifdef GAME_DLL
#ifdef HL2_EPISODIC
			
			CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>( pOwner );

			if ( pPlayer )
			{
				pPlayer->StartArmorReduction();
			}
#endif

			CCitadelEnergyCore *pCore = static_cast<CCitadelEnergyCore*>( CreateEntityByName( "env_citadel_energy_core" ) );

			if ( pCore == NULL )
				return;

			CBaseAnimating *pBeamEnt = pOwner->GetViewModel();
			
			if ( pBeamEnt )
			{
				int iAttachment = pBeamEnt->LookupAttachment( "muzzle" );

				Vector vOrigin;
				QAngle vAngle;

				pBeamEnt->GetAttachment( iAttachment, vOrigin, vAngle );

				pCore->SetAbsOrigin( vOrigin );
				pCore->SetAbsAngles( vAngle );

				DispatchSpawn( pCore );
				pCore->Activate();

				pCore->SetParent( pBeamEnt, iAttachment );
				pCore->SetScale( 2.5f );

				variant_t variant;
				variant.SetFloat( 1.0f );
		
				g_EventQueue.AddEvent( pCore, "StartDischarge", 0, pOwner, pOwner );
				g_EventQueue.AddEvent( pCore, "Stop", variant, 1, pOwner, pOwner );

				pCore->SetThink ( &CCitadelEnergyCore::SUB_Remove );
				pCore->SetNextThink( gpGlobals->curtime + 10.0f );

				m_nSkin = 0;
			}
#endif
		}
	}

#ifdef GAME_DLL
	float flScaleFactor = SpriteScaleFactor();
	// Flicker the end sprites
	if ( ( m_hEndSprites[0] != NULL ) && ( m_hEndSprites[1] != NULL ) )
	{
		//Make the end points flicker as fast as possible
		//FIXME: Make this a property of the CSprite class!
		for ( int i = 0; i < 2; i++ )
		{
			m_hEndSprites[i]->SetBrightness( random->RandomInt( 200, 255 ) );
			m_hEndSprites[i]->SetScale( random->RandomFloat( 0.1, 0.15 ) * flScaleFactor );
		}
	}

	// Flicker the glow sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] == NULL )
			continue;

		if ( IsMegaPhysCannon() )
		{
			m_hGlowSprites[i]->SetBrightness( random->RandomInt( 32, 48 ) );
			m_hGlowSprites[i]->SetScale( random->RandomFloat( 0.15, 0.2 ) * flScaleFactor );
		}
		else
		{
			m_hGlowSprites[i]->SetBrightness( random->RandomInt( 16, 24 ) );
			m_hGlowSprites[i]->SetScale( random->RandomFloat( 0.3, 0.35 ) * flScaleFactor );
		}
	}

	// Only do these effects on the mega-cannon
	if ( IsMegaPhysCannon() )
	{
		// Randomly arc between the elements and core
		if ( random->RandomInt( 0, 100 ) == 0 && !engine->IsPaused() )
		{
			CBeam *pBeam = CBeam::BeamCreate( MEGACANNON_BEAM_SPRITE, 1 );

			CBaseEntity *pBeamEnt = pOwner->GetViewModel();
			pBeam->EntsInit( pBeamEnt, pBeamEnt );

			int	startAttachment;
			int	sprite;

			if ( random->RandomInt( 0, 1 ) )
			{
				startAttachment = LookupAttachment( "fork1t" );
				sprite = 0;
			}
			else
			{
				startAttachment = LookupAttachment( "fork2t" );
				sprite = 1;
			}

			int endAttachment	= 1;

			pBeam->SetStartAttachment( startAttachment );
			pBeam->SetEndAttachment( endAttachment );
			pBeam->SetNoise( random->RandomFloat( 8.0f, 16.0f ) );
			pBeam->SetColor( 255, 255, 255 );
			pBeam->SetScrollRate( 25 );
			pBeam->SetBrightness( 128 );
			pBeam->SetWidth( 1 );
			pBeam->SetEndWidth( random->RandomFloat( 2, 8 ) );
			
			float lifetime = random->RandomFloat( 0.2f, 0.4f );

			pBeam->LiveForTime( lifetime );
			
			if ( m_hEndSprites[sprite] != NULL )
			{
				// Turn on the sprite for awhile
				m_hEndSprites[sprite]->TurnOn();
				m_flEndSpritesOverride[sprite] = gpGlobals->curtime + lifetime;
				EmitSound( "Weapon_MegaPhysCannon.ChargeZap" );
			}
		}

		if ( m_hCenterSprite != NULL )
		{
			if ( m_EffectState == EFFECT_HOLDING )
			{
				m_hCenterSprite->SetBrightness( random->RandomInt( 32, 64 ) );
				m_hCenterSprite->SetScale( random->RandomFloat( 0.2, 0.25 ) * flScaleFactor );
			}
			else
			{
				m_hCenterSprite->SetBrightness( random->RandomInt( 32, 64 ) );
				m_hCenterSprite->SetScale( random->RandomFloat( 0.125, 0.15 ) * flScaleFactor );
			}
		}
		
		if ( m_hBlastSprite != NULL )
		{
			if ( m_EffectState == EFFECT_HOLDING )
			{
				m_hBlastSprite->SetBrightness( random->RandomInt( 125, 150 ) );
				m_hBlastSprite->SetScale( random->RandomFloat( 0.125, 0.15 ) * flScaleFactor );
			}
			else
			{
				m_hBlastSprite->SetBrightness( random->RandomInt( 32, 64 ) );
				m_hBlastSprite->SetScale( random->RandomFloat( 0.075, 0.15 ) * flScaleFactor );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Update our idle effects even when deploying
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ItemBusyFrame( void )
{
	DoEffectIdle();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
	{
		// We found an object. Debounce the button
		m_nAttack2Debounce = 0;
		return;
	}

	//Check for object in pickup range
	if ( m_bActive == false )
	{
		CheckForTarget();

		if ( ( m_flElementDebounce < gpGlobals->curtime ) && ( m_nChangeState != ELEMENT_STATE_NONE ) )
		{
			if (m_nChangeState == ELEMENT_STATE_OPEN)
			{
				OpenElements();
			}
			else if ( m_nChangeState == ELEMENT_STATE_CLOSED )
			{
				CloseElements();
			}

			m_nChangeState = ELEMENT_STATE_NONE;
		}
	}

	// NOTE: Attack2 will be considered to be pressed until the first item is picked up.
	int nAttack2Mask = pOwner->m_nButtons & (~m_nAttack2Debounce);
	if ( nAttack2Mask & IN_ATTACK2 )
	{
		if(!m_bHolstered)
		SecondaryAttack();
	}
	else
	{
		// Reset our debouncer
		m_flLastDenySoundPlayed = false;

		if ( m_bActive == false )
		{
			DoEffect( EFFECT_READY );
		}
	}
	
	if (( pOwner->m_nButtons & IN_ATTACK2 ) == 0 )
	{
		m_nAttack2Debounce = 0;
	}

	if ( pOwner->m_nButtons & IN_ATTACK )
	{
		if(!m_bHolstered)
		PrimaryAttack();
	}
	else 
	{
		WeaponIdle();
	}

	if ( hl2_episodic.GetBool() == true )
	{
		if ( IsMegaPhysCannon() )
		{
			if ( !( pOwner->m_nButtons & IN_ATTACK ) )
			{
				m_flNextPrimaryAttack = gpGlobals->curtime;
			}
		}
	}

	// Update our idle effects (flickers, etc)
	DoEffectIdle();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define PHYSCANNON_DANGER_SOUND_RADIUS 128

void CWeaponPhysCannon::LaunchObject( const Vector &vecDir, float flForce )
{
	// FIRE!!!
	if( m_grabController.GetAttached() )
	{
		CBaseEntity *pObject = m_grabController.GetAttached();
		Assert(pObject);
#ifdef GAME_DLL
		gamestats->Event_Punted( pObject );
#endif
		DetachObject( false, true );

		// Trace ahead a bit and make a chain of danger sounds ahead of the phys object
		// to scare potential targets
		trace_t	tr;
		Vector	vecStart = pObject->GetAbsOrigin();
		Vector	vecSpot;
		int		iLength;
		

		UTIL_TraceLine( vecStart, vecStart + vecDir * flForce, MASK_SHOT, pObject, COLLISION_GROUP_NONE, &tr );
		iLength = ( tr.startpos - tr.endpos ).Length();
		vecSpot = vecStart + vecDir * PHYSCANNON_DANGER_SOUND_RADIUS;
#ifdef GAME_DLL
		int		i;

		for( i = PHYSCANNON_DANGER_SOUND_RADIUS ; i < iLength ; i += PHYSCANNON_DANGER_SOUND_RADIUS )
		{
			CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, vecSpot, PHYSCANNON_DANGER_SOUND_RADIUS, 0.5, pObject );
			vecSpot = vecSpot + ( vecDir * PHYSCANNON_DANGER_SOUND_RADIUS );
		}
				
		// Launch
		ApplyVelocityBasedForce( pObject, vecDir, tr.endpos, PHYSGUN_FORCE_LAUNCHED );
		
		m_hAttachedObject = NULL;
#endif
		// Don't allow the gun to regrab a thrown object!!
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		
		Vector	center = pObject->WorldSpaceCenter();

		//Do repulse effect
		DoEffect( EFFECT_LAUNCH, &center );

	}

	// Stop our looping sound
	if ( GetMotorSound() )
	{
		(CSoundEnvelopeController::GetController()).SoundChangeVolume( GetMotorSound(), 0.0f, 1.0f );
		(CSoundEnvelopeController::GetController()).SoundChangePitch( GetMotorSound(), 50, 1.0f );
	}

	//Close the elements and suppress checking for a bit
	m_nChangeState = ELEMENT_STATE_CLOSED;
	m_flElementDebounce = gpGlobals->curtime + 0.1f;
	m_flCheckSuppressTime = gpGlobals->curtime + 0.25f;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPhysCannon::CanPickupObject( CBaseEntity *pTarget )
{
	if ( pTarget == NULL )
	{
		m_bCanPickupObject = false;
		return false;
	}
	if ( pTarget->GetBaseAnimating() && pTarget->GetBaseAnimating()->IsDissolving() )
	{
		m_bCanPickupObject = false;
		return false;
	}
	CWeaponPhysCannon *pPhysgun = pTarget->GetPhysgun();
	//Allow steal other objects if we are the megacannon but the object's physcannon isn't mega.
	if (pPhysgun && pPhysgun != this)
	{
		if (!IsMegaPhysCannon())
			return false;

		if (pPhysgun->IsMegaPhysCannon())
			return false;			
	}
#ifdef GAME_DLL
	if ( pTarget->HasSpawnFlags( SF_PHYSBOX_ALWAYS_PICK_UP ) || pTarget->HasSpawnFlags( SF_PHYSBOX_NEVER_PICK_UP ) )
	{
		// It may seem strange to check this spawnflag before we know the class of this object, since the 
		// spawnflag only applies to func_physbox, but it can act as a filter of sorts to reduce the number 
		// of irrelevant entities that fall through to this next casting check, which is slower.
		CPhysBox *pPhysBox = dynamic_cast<CPhysBox*>(pTarget);

		if ( pPhysBox != NULL )
		{
			if ( pTarget->HasSpawnFlags( SF_PHYSBOX_NEVER_PICK_UP ) )
			{

				m_bCanPickupObject = false;
				return false;
			}
			else
			{
				m_bCanPickupObject = true;
				return true;
			}
		}
	}

	if ( pTarget->HasSpawnFlags(SF_PHYSPROP_ALWAYS_PICK_UP) )
	{
		// It may seem strange to check this spawnflag before we know the class of this object, since the 
		// spawnflag only applies to func_physbox, but it can act as a filter of sorts to reduce the number 
		// of irrelevant entities that fall through to this next casting check, which is slower.
		CPhysicsProp *pPhysProp = dynamic_cast<CPhysicsProp*>(pTarget);
		if ( pPhysProp != NULL )
		{
			m_bCanPickupObject = true;
			return true;
		}
	}
#endif
	if ( pTarget->IsEFlagSet( EFL_NO_PHYSCANNON_INTERACTION ) )
	{
		m_bCanPickupObject = false;
		return false;
	}
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && pOwner->GetGroundEntity() == pTarget )
	{
		m_bCanPickupObject = false;
		return false;
	}
	if ( !IsMegaPhysCannon() )
	{
		if ( pTarget->VPhysicsIsFlesh( ) )
		{
			m_bCanPickupObject = false;
			return false;
		}
#ifdef GAME_DLL
		m_bCanPickupObject = CBasePlayer::CanPickupObject(pTarget, physcannon_maxmass.GetFloat(), 0);
		return CBasePlayer::CanPickupObject( pTarget, physcannon_maxmass.GetFloat(), 0 );
#endif
	}
#ifdef GAME_DLL
	if ( pTarget->IsNPC() && pTarget->MyNPCPointer()->CanBecomeRagdoll() )
	{
		m_bCanPickupObject = true;
		return true;
	}
	if ( dynamic_cast<CRagdollProp*>(pTarget) )
	{
		m_bCanPickupObject = true;
		return true;
	}
	m_bCanPickupObject = CBasePlayer::CanPickupObject(pTarget, 0, 0);
	return CBasePlayer::CanPickupObject( pTarget, 0, 0 );
#else
	return m_bCanPickupObject;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::OpenElements( void )
{
	if ( m_bOpen )
		return;

	WeaponSound( SPECIAL2 );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;
#ifdef GAME_DLL
	if( !IsMegaPhysCannon() )
	{
		pOwner->RumbleEffect(RUMBLE_PHYSCANNON_OPEN, 0, RUMBLE_FLAG_RESTART|RUMBLE_FLAG_LOOP);
	}
#endif
	if ( m_flElementPosition < 0.0f )
		m_flElementPosition = 0.0f;

	m_flElementDestination = 1.0f;

	SendWeaponAnim( ACT_VM_IDLE );

	m_bOpen = true;

	DoEffect( EFFECT_READY );

#ifdef CLIENT_DLL
	// Element prediction 
	m_ElementParameter.InitFromCurrent(1.0f, 0.2f, INTERP_SPLINE);
	m_bOldOpen = true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::CloseElements( void )
{
	// The mega cannon cannot be closed!
	if ( IsMegaPhysCannon() )
	{
		OpenElements();
		return;
	}

	if ( m_bOpen == false )
		return;

	WeaponSound( MELEE_HIT );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;
#ifdef GAME_DLL
	pOwner->RumbleEffect(RUMBLE_PHYSCANNON_OPEN, 0, RUMBLE_FLAG_STOP);
#endif
	if ( m_flElementPosition > 1.0f )
		m_flElementPosition = 1.0f;

	m_flElementDestination = 0.0f;

	SendWeaponAnim( ACT_VM_IDLE );

	m_bOpen = false;

	if ( GetMotorSound() )
	{
		(CSoundEnvelopeController::GetController()).SoundChangeVolume( GetMotorSound(), 0.0f, 1.0f );
		(CSoundEnvelopeController::GetController()).SoundChangePitch( GetMotorSound(), 50, 1.0f );
	}
	
	DoEffect( EFFECT_CLOSED );
	
#ifdef CLIENT_DLL
	// Element prediction 
	m_ElementParameter.InitFromCurrent( 0.0f, 0.5f, INTERP_SPLINE );
	m_bOldOpen = false;
#endif
}

#define	PHYSCANNON_MAX_MASS		500


//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CWeaponPhysCannon::GetLoadPercentage( void )
{
	float loadWeight = m_grabController.GetLoadWeight();
	loadWeight /= physcannon_maxmass.GetFloat();	
	loadWeight = clamp( loadWeight, 0.0f, 1.0f );
	return loadWeight;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CSoundPatch
//-----------------------------------------------------------------------------
CSoundPatch *CWeaponPhysCannon::GetMotorSound( void )
{
	if ( m_sndMotor == NULL )
	{
		CPASAttenuationFilter filter( this );
		
		if ( IsMegaPhysCannon() )
		{
			m_sndMotor = (CSoundEnvelopeController::GetController()).SoundCreate( filter, entindex(), CHAN_STATIC, "Weapon_MegaPhysCannon.HoldSound", ATTN_NORM );
		}
		else
		{
			m_sndMotor = (CSoundEnvelopeController::GetController()).SoundCreate( filter, entindex(), CHAN_STATIC, "Weapon_PhysCannon.HoldSound", ATTN_NORM );
		}
	}

	return m_sndMotor;
}


//-----------------------------------------------------------------------------
// Shuts down sounds
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::StopLoopingSounds()
{
	if ( m_sndMotor != NULL )
	{
		 (CSoundEnvelopeController::GetController()).SoundDestroy( m_sndMotor );
		 m_sndMotor = NULL;
	}
#ifdef GAME_DLL
	BaseClass::StopLoopingSounds();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DestroyEffects( void )
{
#ifdef GAME_DLL
	//Turn off main glow
	if ( m_hCenterSprite != NULL )
	{
#ifdef GAME_DLL
		UTIL_Remove( m_hCenterSprite );
#endif
		m_hCenterSprite = NULL;
	}

	if ( m_hBlastSprite != NULL )
	{
#ifdef GAME_DLL
		UTIL_Remove( m_hBlastSprite );
#endif
		m_hBlastSprite = NULL;
	}

	// Turn off beams
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
#ifdef GAME_DLL
			UTIL_Remove( m_hBeams[i] );
#endif
			m_hBeams[i] = NULL;
		}
	}

	// Turn off sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
#ifdef GAME_DLL
			UTIL_Remove( m_hGlowSprites[i] );
#endif
			m_hGlowSprites[i] = NULL;
		}
	}

	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
#ifdef GAME_DLL
			UTIL_Remove( m_hEndSprites[i] );
#endif
			m_hEndSprites[i] = NULL;
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::StopEffects( bool stopSound )
{
	// Turn off our effect state
	DoEffect( EFFECT_NONE );
#ifdef GAME_DLL
	//Turn off main glow
	if ( m_hCenterSprite != NULL )
	{
		m_hCenterSprite->TurnOff();
	}

	if ( m_hBlastSprite != NULL )
	{
		m_hBlastSprite->TurnOff();
	}

	//Turn off beams
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
			m_hBeams[i]->SetBrightness( 0 );
		}
	}

	//Turn off sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
			m_hGlowSprites[i]->TurnOff();
		}
	}

	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
			m_hEndSprites[i]->TurnOff();
		}
	}
#endif
	//Shut off sounds
	if ( stopSound && GetMotorSound() != NULL )
	{
		(CSoundEnvelopeController::GetController()).SoundFadeOut( GetMotorSound(), 0.1f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::StartEffects( void )
{
#ifdef GAME_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	bool bIsMegaCannon = IsMegaPhysCannon();

	int i;
	float flScaleFactor = SpriteScaleFactor();
	CBaseEntity *pBeamEnt = pOwner->GetViewModel();

	// Create the beams
	for ( i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] )
			continue;

		const char *beamAttachNames[] = 
		{
			"fork1t",
			"fork2t",
			"fork1t",
			"fork2t",
			"fork1t",
			"fork2t",
		};

		m_hBeams[i] = CBeam::BeamCreate( 
			bIsMegaCannon ? MEGACANNON_BEAM_SPRITE : PHYSCANNON_BEAM_SPRITE, 1.0f );
		m_hBeams[i]->EntsInit( pBeamEnt, pBeamEnt );

		int	startAttachment = LookupAttachment( beamAttachNames[i] );
		int endAttachment	= 1;

		m_hBeams[i]->FollowEntity( pBeamEnt );
		m_hBeams[i]->AddSpawnFlags( SF_BEAM_TEMPORARY );	

		m_hBeams[i]->SetStartAttachment( startAttachment );
		m_hBeams[i]->SetEndAttachment( endAttachment );
		m_hBeams[i]->SetNoise( random->RandomFloat( 8.0f, 16.0f ) );
		m_hBeams[i]->SetColor( 255, 255, 255 );
		m_hBeams[i]->SetScrollRate( 25 );
		m_hBeams[i]->SetBrightness( 128 );
		m_hBeams[i]->SetWidth( 0 );
		m_hBeams[i]->SetEndWidth( random->RandomFloat( 2, 4 ) );
	}

	//Create the glow sprites
	for ( i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] )
			continue;

		const char *attachNames[] = 
		{
			"fork1b",
			"fork1m",
			"fork1t",
			"fork2b",
			"fork2m",
			"fork2t"
		};

		m_hGlowSprites[i] = CSprite::SpriteCreate( 
			bIsMegaCannon ? MEGACANNON_GLOW_SPRITE : PHYSCANNON_GLOW_SPRITE, 
			GetAbsOrigin(), false );

		m_hGlowSprites[i]->SetAsTemporary();

		m_hGlowSprites[i]->SetAttachment( pOwner->GetViewModel(), LookupAttachment( attachNames[i] ) );
		
		if ( bIsMegaCannon )
		{
			m_hGlowSprites[i]->SetTransparency( kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone );
		}
		else
		{
			m_hGlowSprites[i]->SetTransparency( kRenderTransAdd, 255, 128, 0, 64, kRenderFxNoDissipation );
		}

		m_hGlowSprites[i]->SetBrightness( 255, 0.2f );
		m_hGlowSprites[i]->SetScale( 0.25f * flScaleFactor, 0.2f );
	}

	//Create the endcap sprites
	for ( i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] == NULL )
		{
			const char *attachNames[] = 
			{
				"fork1t",
				"fork2t"
			};

			m_hEndSprites[i] = CSprite::SpriteCreate( 
				bIsMegaCannon ? MEGACANNON_ENDCAP_SPRITE : PHYSCANNON_ENDCAP_SPRITE, 
				GetAbsOrigin(), false );

			m_hEndSprites[i]->SetAsTemporary();
			m_hEndSprites[i]->SetAttachment( pOwner->GetViewModel(), LookupAttachment( attachNames[i] ) );
			m_hEndSprites[i]->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
			m_hEndSprites[i]->SetBrightness( 255, 0.2f );
			m_hEndSprites[i]->SetScale( 0.25f * flScaleFactor, 0.2f );
			m_hEndSprites[i]->TurnOff();
		}
	}

	//Create the center glow
	if ( m_hCenterSprite == NULL )
	{
		m_hCenterSprite = CSprite::SpriteCreate( 
			bIsMegaCannon ? MEGACANNON_CENTER_GLOW : PHYSCANNON_CENTER_GLOW, 
			GetAbsOrigin(), false );

		m_hCenterSprite->SetAsTemporary();
		m_hCenterSprite->SetAttachment( pOwner->GetViewModel(), 1 );
		m_hCenterSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		m_hCenterSprite->SetBrightness( 255, 0.2f );
		m_hCenterSprite->SetScale( 0.1f, 0.2f );
	}

	//Create the blast sprite
	if ( m_hBlastSprite == NULL )
	{
		m_hBlastSprite = CSprite::SpriteCreate( 
			bIsMegaCannon ? MEGACANNON_BLAST_SPRITE : PHYSCANNON_BLAST_SPRITE, 
			GetAbsOrigin(), false );

		m_hBlastSprite->SetAsTemporary();
		m_hBlastSprite->SetAttachment( pOwner->GetViewModel(), 1 );
		m_hBlastSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		m_hBlastSprite->SetBrightness( 255, 0.2f );
		m_hBlastSprite->SetScale( 0.1f, 0.2f );
		m_hBlastSprite->TurnOff();
	}
#endif
}

//-----------------------------------------------------------------------------
// Closing effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectClosed( void )
{
#ifdef GAME_DLL
	float flScaleFactor = SpriteScaleFactor();

	// Turn off the center sprite
	if ( m_hCenterSprite != NULL )
	{
		m_hCenterSprite->SetBrightness( 0.0, 0.1f );
		m_hCenterSprite->SetScale( 0.0f, 0.1f );
		m_hCenterSprite->TurnOff();
	}

	// Turn off the end-caps
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
			m_hEndSprites[i]->TurnOff();
		}
	}

	// Turn off the lightning
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
			m_hBeams[i]->SetBrightness( 0 );
		}
	}

	// Turn on the glow sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
			m_hGlowSprites[i]->TurnOn();
			m_hGlowSprites[i]->SetBrightness( 16.0f, 0.2f );
			m_hGlowSprites[i]->SetScale( 0.3f * flScaleFactor, 0.2f );
		}
	}
	
	// Prepare for scale down
	if ( m_hBlastSprite != NULL )
	{
		m_hBlastSprite->TurnOn();
		m_hBlastSprite->SetScale( 1.0f, 0.0f );
		m_hBlastSprite->SetBrightness( 0, 0.0f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Closing effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectClosed( void )
{
#ifdef GAME_DLL
	float flScaleFactor = SpriteScaleFactor();

	// Turn off the center sprite
	if ( m_hCenterSprite != NULL )
	{
		m_hCenterSprite->SetBrightness( 0.0, 0.1f );
		m_hCenterSprite->SetScale( 0.0f, 0.1f );
		m_hCenterSprite->TurnOff();
	}

	// Turn off the end-caps
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
			m_hEndSprites[i]->TurnOff();
		}
	}

	// Turn off the lightning
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
			m_hBeams[i]->SetBrightness( 0 );
		}
	}

	// Turn on the glow sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
			m_hGlowSprites[i]->TurnOn();
			m_hGlowSprites[i]->SetBrightness( 16.0f, 0.2f );
			m_hGlowSprites[i]->SetScale( 0.3f * flScaleFactor, 0.2f );
		}
	}
	
	// Prepare for scale down
	if ( m_hBlastSprite != NULL )
	{
		m_hBlastSprite->TurnOn();
		m_hBlastSprite->SetScale( 1.0f, 0.0f );
		m_hBlastSprite->SetBrightness( 0, 0.0f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Ready effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectReady( )
{
#ifdef GAME_DLL
	float flScaleFactor = SpriteScaleFactor();

	//Turn on the center sprite
	if ( m_hCenterSprite != NULL )
	{
		m_hCenterSprite->SetBrightness( 128, 0.2f );
		m_hCenterSprite->SetScale( 0.15f, 0.2f );
		m_hCenterSprite->TurnOn();
	}

	//Turn off the end-caps
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
			m_hEndSprites[i]->TurnOff();
		}
	}

	//Turn off the lightning
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
			m_hBeams[i]->SetBrightness( 0 );
		}
	}

	//Turn on the glow sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
			m_hGlowSprites[i]->TurnOn();
			m_hGlowSprites[i]->SetBrightness( 32.0f, 0.2f );
			m_hGlowSprites[i]->SetScale( 0.4f * flScaleFactor, 0.2f );
		}
	}

	//Scale down
	if ( m_hBlastSprite != NULL )
	{
		m_hBlastSprite->TurnOn();
		m_hBlastSprite->SetScale( 0.1f, 0.2f );
		m_hBlastSprite->SetBrightness( 255, 0.1f );
	}
#endif
}


//-----------------------------------------------------------------------------
// Holding effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectHolding( )
{
#ifdef GAME_DLL
	float flScaleFactor = SpriteScaleFactor();

	// Turn off the center sprite
	if ( m_hCenterSprite != NULL )
	{
		m_hCenterSprite->SetBrightness( 255, 0.1f );
		m_hCenterSprite->SetScale( 0.2f, 0.2f );
		m_hCenterSprite->TurnOn();
	}

	// Turn off the end-caps
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
			m_hEndSprites[i]->TurnOn();
		}
	}

	// Turn off the lightning
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
			m_hBeams[i]->SetBrightness( 128 );
		}
	}

	// Turn on the glow sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
			m_hGlowSprites[i]->TurnOn();
			m_hGlowSprites[i]->SetBrightness( 64.0f, 0.2f );
			m_hGlowSprites[i]->SetScale( 0.5f * flScaleFactor, 0.2f );
		}
	}

	// Prepare for scale up
	if ( m_hBlastSprite != NULL )
	{
		m_hBlastSprite->TurnOff();
		m_hBlastSprite->SetScale( 0.1f, 0.0f );
		m_hBlastSprite->SetBrightness( 0, 0.0f );
	}
#endif
}


//-----------------------------------------------------------------------------
// Launch effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectLaunch( Vector *pos )
{
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	Assert( pos );
	if ( pos == NULL )
		return;
	
	Vector	endpos = *pos;

	Vector	shotDir = ( endpos - pOwner->Weapon_ShootPosition() );
	VectorNormalize( shotDir );
	
	// See if we need to predict this position
	if ( pos == NULL )
	{
		// Hit an entity if we're holding one
		if ( m_hAttachedObject )
		{
			endpos = m_hAttachedObject->WorldSpaceCenter();
			
			shotDir = endpos - pOwner->Weapon_ShootPosition();
			VectorNormalize( shotDir );
		}
		else
		{
			// Otherwise try and find the right spot
			endpos = pOwner->Weapon_ShootPosition();
			pOwner->EyeVectors( &shotDir );

			trace_t	tr;
			UTIL_TraceLine( endpos, endpos + ( shotDir * MAX_TRACE_LENGTH ), MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );
			
			endpos = tr.endpos;
			shotDir = endpos - pOwner->Weapon_ShootPosition();
			VectorNormalize( shotDir );
		}
	}
	else
	{
		// Use what is supplied
		endpos = *pos;
		shotDir = ( endpos - pOwner->Weapon_ShootPosition() );
		VectorNormalize( shotDir );
	}

	CPVSFilter filter(endpos);
	
	// Don't send this to the owning player, they already had it predicted
	if ( IsPredicted() )
	{
		filter.UsePredictionRules();
	}

#ifdef GAME_DLL

	if ( pOwner == NULL )
		return;
	
	// Check to store off our view model index
	CBeam *pBeam = CBeam::BeamCreate( IsMegaPhysCannon() ? MEGACANNON_BEAM_SPRITE : PHYSCANNON_BEAM_SPRITE, 8 );

	if ( pBeam != NULL )
	{
		pBeam->PointEntInit( endpos, this );
		pBeam->SetEndAttachment( 1 );
		pBeam->SetWidth( 6.4 );
		pBeam->SetEndWidth( 12.8 );
		pBeam->SetBrightness( 255 );
		pBeam->SetColor( 255, 255, 255 );
		pBeam->LiveForTime( 0.1f );
		pBeam->RelinkBeam();
		pBeam->SetNoise( 2 );
	}


	//End hit
	//FIXME: Probably too big
	te->GaussExplosion( filter, 0.0f, endpos - ( shotDir * 4.0f ), RandomVector(-1.0f, 1.0f), 0 );
	
	if ( m_hBlastSprite != NULL )
	{
		m_hBlastSprite->TurnOn();
		m_hBlastSprite->SetScale( 2.0f, 0.1f );
		m_hBlastSprite->SetBrightness( 0.0f, 0.1f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pos - 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectLaunch( Vector *pos )
{
#ifdef GAME_DLL
	Assert( pos );
	if ( pos == NULL )
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	Vector	endpos = *pos;

	// Check to store off our view model index
	CBaseViewModel *vm = pOwner->GetViewModel();
	
	int numBeams = random->RandomInt( 1, 2 );

	CBeam *pBeam = CBeam::BeamCreate( IsMegaPhysCannon() ? MEGACANNON_BEAM_SPRITE : PHYSCANNON_BEAM_SPRITE, 0.8 );

	if ( pBeam != NULL )
	{
		pBeam->PointEntInit( endpos, vm );
		pBeam->SetEndAttachment( 1 );
		pBeam->SetWidth( 2 );
		pBeam->SetEndWidth( 12 );
		pBeam->SetBrightness( 255 );
		pBeam->SetColor( 255, 255, 255 );
		pBeam->LiveForTime( 0.1f );
		pBeam->RelinkBeam();
		pBeam->SetNoise( 0 );
	}

	for ( int i = 0; i < numBeams; i++ )
	{
		pBeam = CBeam::BeamCreate( IsMegaPhysCannon() ? MEGACANNON_BEAM_SPRITE : PHYSCANNON_BEAM_SPRITE, 0.8 );

		if ( pBeam != NULL )
		{
			pBeam->PointEntInit( endpos, vm );
			pBeam->SetEndAttachment( 1 );
			pBeam->SetWidth( 2 );
			pBeam->SetEndWidth( random->RandomInt( 1, 2 ) );
			pBeam->SetBrightness( 255 );
			pBeam->SetColor( 255, 255, 255 );
			pBeam->LiveForTime( 0.1f );
			pBeam->RelinkBeam();
			pBeam->SetNoise( random->RandomInt( 8, 12 ) );
		}
	}
	
	Vector	shotDir = ( endpos - pOwner->Weapon_ShootPosition() );
	VectorNormalize( shotDir );

	//End hit
	//FIXME: Probably too big
	CPVSFilter filter( endpos );
	te->GaussExplosion( filter, 0.0f, endpos - ( shotDir * 4.0f ), RandomVector(-1.0f, 1.0f), 0 );
#endif
}

//-----------------------------------------------------------------------------
// Holding effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectHolding( void )
{
#ifdef GAME_DLL
	float flScaleFactor = SpriteScaleFactor();

	// Turn off the center sprite
	if ( m_hCenterSprite != NULL )
	{
		m_hCenterSprite->SetBrightness( 255, 0.1f );
		m_hCenterSprite->SetScale( 0.2f, 0.2f );
		m_hCenterSprite->TurnOn();
	}

	// Turn off the end-caps
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
			m_hEndSprites[i]->TurnOn();
		}
	}

	// Turn off the lightning
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
			m_hBeams[i]->SetBrightness( 128 );
		}
	}

	// Turn on the glow sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
			m_hGlowSprites[i]->TurnOn();
			m_hGlowSprites[i]->SetBrightness( 32.0f, 0.2f );
			m_hGlowSprites[i]->SetScale( 0.25f * flScaleFactor, 0.2f );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Ready effects
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffectReady( void )
{
#ifdef GAME_DLL
	float flScaleFactor = SpriteScaleFactor();

	//Turn on the center sprite
	if ( m_hCenterSprite != NULL )
	{
		m_hCenterSprite->SetBrightness( 128, 0.2f );
		m_hCenterSprite->SetScale( 0.15f, 0.2f );
		m_hCenterSprite->TurnOn();
	}

	//Turn off the end-caps
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_hEndSprites[i] != NULL )
		{
			if ( m_flEndSpritesOverride[i] < gpGlobals->curtime )
			{
				m_hEndSprites[i]->TurnOff();
			}
		}
	}

	//Turn off the lightning
	for ( int i = 0; i < NUM_BEAMS; i++ )
	{
		if ( m_hBeams[i] != NULL )
		{
			m_hBeams[i]->SetBrightness( 0 );
		}
	}

	//Turn on the glow sprites
	for ( int i = 0; i < NUM_SPRITES; i++ )
	{
		if ( m_hGlowSprites[i] != NULL )
		{
			m_hGlowSprites[i]->TurnOn();
			m_hGlowSprites[i]->SetBrightness( 24.0f, 0.2f );
			m_hGlowSprites[i]->SetScale( 0.2f * flScaleFactor, 0.2f );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown for the weapon when it's holstered
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffectNone( void )
{
#ifdef GAME_DLL
	if ( m_hBlastSprite != NULL )
	{
		// Become small
		m_hBlastSprite->SetScale( 0.001f );
	}	
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : effectType - 
//			*pos - 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoMegaEffect( int effectType, Vector *pos )
{
	switch( effectType )
	{
	case EFFECT_CLOSED:
		DoMegaEffectClosed();
		break;

	case EFFECT_READY:
		DoMegaEffectReady();
		break;

	case EFFECT_HOLDING:
		DoMegaEffectHolding();
		break;

	case EFFECT_LAUNCH:
		DoMegaEffectLaunch( pos );
		break;

	default:
	case EFFECT_NONE:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : effectType - 
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::DoEffect( int effectType, Vector *pos )
{
	// Make sure we're active
	StartEffects();

	m_EffectState = effectType;

	// Do different effects when upgraded
	if ( IsMegaPhysCannon() )
	{
		DoMegaEffect( effectType, pos );
		return;
	}

	switch( effectType )
	{
	case EFFECT_CLOSED:
		DoEffectClosed( );
		break;

	case EFFECT_READY:
		DoEffectReady( );
		break;

	case EFFECT_HOLDING:
		DoEffectHolding();
		break;

	case EFFECT_LAUNCH:
		DoEffectLaunch( pos );
		break;

	default:
	case EFFECT_NONE:
		DoEffectNone();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iIndex - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CWeaponPhysCannon::GetShootSound( int iIndex ) const
{
	// Just do this normally if we're a normal physcannon
	if ( PlayerHasMegaPhysCannon() == false )
		return BaseClass::GetShootSound( iIndex );

	// We override this if we're the charged up version
	switch( iIndex )
	{
	case EMPTY:
		return "Weapon_MegaPhysCannon.DryFire";
		break;

	case SINGLE:
		return "Weapon_MegaPhysCannon.Launch";
		break;

	case SPECIAL1:
		return "Weapon_MegaPhysCannon.Pickup";
		break;

	case MELEE_MISS:
		return "Weapon_MegaPhysCannon.Drop";
		break;

	default:
		break;
	}

	return BaseClass::GetShootSound( iIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Adds the specified object to the list of objects that have been
//			propelled by this physgun, along with a timestamp of when the 
//			object was added to the list. This list is checked when a physics
//			object strikes another entity, to resolve whether the player is 
//			accountable for the impact.
//
// Input  : pObject - pointer to the object being thrown by the physcannon.
//-----------------------------------------------------------------------------
void CWeaponPhysCannon::RecordThrownObject( CBaseEntity *pObject )
{

	Assert(pObject);
	thrown_objects_t thrown;
	thrown.hEntity = pObject;
	thrown.fTimeThrown = gpGlobals->curtime;

	// Get rid of stale and dead objects in the list.
	PurgeThrownObjects();

	// See if this object is already in the list.
	int count = m_ThrownEntities.Count();

	for( int i = 0 ; i < count ; i++ )
	{
		if( m_ThrownEntities[i].hEntity == pObject )
		{
			// Just update the time.
			//Msg("++UPDATING: %s (%d)\n", m_ThrownEntities[i].hEntity->GetClassname(), m_ThrownEntities[i].hEntity->entindex() );
			m_ThrownEntities[i] = thrown;
			return;
		}
	}

	//Msg("++ADDING: %s (%d)\n", pObject->GetClassname(), pObject->entindex() );

	m_ThrownEntities.AddToTail(thrown);
}

//-----------------------------------------------------------------------------
// Purpose: Go through the objects in the thrown objects list and discard any
//			objects that have gone 'stale'. (Were thrown several seconds ago), or
//			have been destroyed or removed.
//
//-----------------------------------------------------------------------------
#define PHYSCANNON_THROWN_LIST_TIMEOUT	10.0f
void CWeaponPhysCannon::PurgeThrownObjects()
{
	bool bListChanged;

	// This is bubble-sorty, but the list is also very short.
	do
	{
		bListChanged = false;

		int count = m_ThrownEntities.Count();
		for( int i = 0 ; i < count ; i++ )
		{
			bool bRemove = false;

			if( !m_ThrownEntities[i].hEntity.Get() )
			{
				bRemove = true;
			}
			else if( gpGlobals->curtime > (m_ThrownEntities[i].fTimeThrown + PHYSCANNON_THROWN_LIST_TIMEOUT) )
			{
				bRemove = true;
			}
			else
			{
				IPhysicsObject *pObject = m_ThrownEntities[i].hEntity->VPhysicsGetObject();

				if( pObject && pObject->IsAsleep() )
				{
					bRemove = true;
				}
			}

			if( bRemove )
			{
				//Msg("--REMOVING: %s (%d)\n", m_ThrownEntities[i].hEntity->GetClassname(), m_ThrownEntities[i].hEntity->entindex() );
				m_ThrownEntities.Remove(i);
				bListChanged = true;
				break;
			}
		}
	} while( bListChanged );
}

bool CWeaponPhysCannon::IsAccountableForObject( CBaseEntity *pObject )
{
	Assert(pObject);
	// Clean out the stale and dead items.
	PurgeThrownObjects();

	// Now if this object is in the list, the player is accountable for it striking something.
	int count = m_ThrownEntities.Count();

	for( int i = 0 ; i < count ; i++ )
	{
		if( m_ThrownEntities[i].hEntity == pObject )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// EXTERNAL API
//-----------------------------------------------------------------------------
void PhysCannonForceDrop( CBaseCombatWeapon *pActiveWeapon, CBaseEntity *pOnlyIfHoldingThis )
{
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pActiveWeapon);
	if ( pCannon )
	{
		if ( pOnlyIfHoldingThis )
		{
			pCannon->DropIfEntityHeld( pOnlyIfHoldingThis );
		}
		else
		{
			pCannon->ForceDrop();
		}
	}
}

void PhysCannonBeginUpgrade( CBaseAnimating *pAnim )
{
	CWeaponPhysCannon *pWeaponPhyscannon = assert_cast<	CWeaponPhysCannon* >( pAnim );
	pWeaponPhyscannon->BeginUpgrade();
}


bool PlayerPickupControllerIsHoldingEntity( CBaseEntity *pPickupControllerEntity, CBaseEntity *pHeldEntity )
{
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);

	return pController ? pController->IsHoldingEntity( pHeldEntity ) : false;
}

void ShutdownPickupController( CBaseEntity *pPickupControllerEntity )
{
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);

	pController->Shutdown( false );
}

float PhysCannonGetHeldObjectMass( CBaseCombatWeapon *pActiveWeapon, IPhysicsObject *pHeldObject )
{
	float mass = 0.0f;
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pActiveWeapon);
	if ( pCannon )
	{
		CGrabController &grab = pCannon->GetGrabController();
		mass = grab.GetSavedMass( pHeldObject );
	}

	return mass;
}

CBaseEntity *PhysCannonGetHeldEntity( CBaseCombatWeapon *pActiveWeapon )
{
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pActiveWeapon);
	if ( pCannon )
	{
		CGrabController &grab = pCannon->GetGrabController();
		return grab.GetAttached();
	}

	return NULL;
}

CBasePlayer *GetPlayerHoldingEntity(CBaseEntity *pEntity)
{
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer)
		{
			if (GetPlayerHeldEntity(pPlayer) == pEntity || PhysCannonGetHeldEntity(pPlayer->GetActiveWeapon()) == pEntity)
				return pPlayer;
		}
	}
	return NULL;
}


CBaseEntity *GetPlayerHeldEntity( CBasePlayer *pPlayer )
{
	CBaseEntity *pObject = NULL;
	CPlayerPickupController *pPlayerPickupController = (CPlayerPickupController *)(pPlayer->GetUseEntity());

	if ( pPlayerPickupController )
	{
		pObject = pPlayerPickupController->GetGrabController().GetAttached();
	}

	return pObject;
}

CGrabController *GetGrabControllerForPlayer( CBasePlayer *pPlayer )
{
	CPlayerPickupController *pPlayerPickupController = (CPlayerPickupController *)(pPlayer->GetUseEntity());
	if( pPlayerPickupController )
		return &(pPlayerPickupController->GetGrabController());

	return NULL;
}
CGrabController *GetGrabControllerForPhysCannon( CBaseCombatWeapon *pActiveWeapon )
{
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pActiveWeapon);
	if ( pCannon )
	{
		return &(pCannon->GetGrabController());
	}

	return NULL;
}
void GetSavedParamsForCarriedPhysObject( CGrabController *pGrabController, IPhysicsObject *pObject, float *pSavedMassOut, float *pSavedRotationalDampingOut )
{
	CBaseEntity *pHeld = pGrabController->m_attachedEntity;
	if( pHeld )
	{
		if( pObject->GetGameData() == (void*)pHeld )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			Assert(pList);
			int count = pHeld->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			for ( int i = 0; i < count; i++ )
			{
				if ( pList[i] == pObject )
				{
					if( pSavedMassOut )
						*pSavedMassOut = pGrabController->m_savedMass[i];

					if( pSavedRotationalDampingOut )
						*pSavedRotationalDampingOut = pGrabController->m_savedRotDamping[i];

					return;
				}
			}
		}
	}

	if( pSavedMassOut )
		*pSavedMassOut = 0.0f;

	if( pSavedRotationalDampingOut )
		*pSavedRotationalDampingOut = 0.0f;

	return;
}

void UpdateGrabControllerTargetPosition( CPortal_Player *pPlayer, Vector *vPosition, QAngle *qAngles )
{
	CGrabController *pGrabController = GetGrabControllerForPlayer( pPlayer );

	if ( !pGrabController )
		return;
	
	pGrabController->UpdateObject( pPlayer, 12 );
	pGrabController->GetTargetPosition( vPosition, qAngles );
}
bool PhysCannonAccountableForObject( CBaseCombatWeapon *pPhysCannon, CBaseEntity *pObject )
{
	CWeaponPhysCannon *pCannon = dynamic_cast<CWeaponPhysCannon *>(pPhysCannon);
	if ( pCannon )
	{
		return pCannon->IsAccountableForObject(pObject);
	}

	return false;
}
float PlayerPickupGetHeldObjectMass( CBaseEntity *pPickupControllerEntity, IPhysicsObject *pHeldObject )
{
	float mass = 0.0f;
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);
	if ( pController )
	{
		CGrabController &grab = pController->GetGrabController();
		mass = grab.GetSavedMass( pHeldObject );
	}
	return mass;
}


void GrabController_SetPortalPenetratingEntity( CGrabController *pController, CBaseEntity *pPenetrated )
{
	Assert(pController);
	Assert(pPenetrated);
	pController->SetPortalPenetratingEntity( pPenetrated );
}