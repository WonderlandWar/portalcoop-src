//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_basehlcombatweapon.h"
#include "igamemovement.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CHLMachineGun
#undef CHLMachineGun
#endif

#ifdef CHLSelectFireMachineGun
#undef CHLSelectFireMachineGun
#endif

IMPLEMENT_CLIENTCLASS_DT( C_HLMachineGun, DT_HLMachineGun, CHLMachineGun )

	RecvPropInt(RECVINFO(m_nShotsFired))

END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_HLSelectFireMachineGun, DT_HLSelectFireMachineGun, CHLSelectFireMachineGun )
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_BaseHLBludgeonWeapon, DT_BaseHLBludgeonWeapon, CBaseHLBludgeonWeapon )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA(C_HLMachineGun)
	DEFINE_PRED_FIELD(m_nShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()

C_HLMachineGun::C_HLMachineGun()
{
	SetPredictionEligible(true);
}

void C_HLMachineGun::WeaponSound(WeaponSound_t sound_type, float soundtime /* = 0.0f */)
{
		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = GetWpnData().aShootSounds[ sound_type ]; 
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only
		if ( !te->CanPredict() )
			return;
				
		CBaseEntity::EmitSound( filter, GetOwner()->entindex(), shootsound, &GetOwner()->GetAbsOrigin() ); 
}

void C_HLMachineGun::PrimaryAttack( void )
{
	m_nShotsFired++;

	BaseClass::PrimaryAttack();
}

//-----------------------------------------------------------------------------
// Purpose: Reset our shots fired
//-----------------------------------------------------------------------------
bool C_HLMachineGun::Deploy( void )
{
	m_nShotsFired = 0;

	return BaseClass::Deploy();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_HLMachineGun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	// Debounce the recoiling counter
	if ( ( pOwner->m_nButtons & IN_ATTACK ) == false )
	{
		m_nShotsFired = 0;
	}

	BaseClass::ItemPostFrame();
}

bool C_HLMachineGun::ShouldPredict()
{
	if (GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer())
		return true;

	return BaseClass::ShouldPredict();
}


C_HLSelectFireMachineGun::C_HLSelectFireMachineGun()
{
	SetPredictionEligible(true);
}

void C_HLSelectFireMachineGun::WeaponSound(WeaponSound_t sound_type, float soundtime /* = 0.0f */)
{
		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = GetWpnData().aShootSounds[ sound_type ]; 
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only
		if ( !te->CanPredict() )
			return;
				
		CBaseEntity::EmitSound( filter, GetOwner()->entindex(), shootsound, &GetOwner()->GetAbsOrigin() ); 
}

bool C_HLSelectFireMachineGun::ShouldPredict()
{
	if (GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer())
		return true;

	return BaseClass::ShouldPredict();
}

C_BaseHLBludgeonWeapon::C_BaseHLBludgeonWeapon()
{
	SetPredictionEligible(true);
}

void C_BaseHLBludgeonWeapon::WeaponSound(WeaponSound_t sound_type, float soundtime /* = 0.0f */)
{
		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = GetWpnData().aShootSounds[ sound_type ]; 
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only
		if ( !te->CanPredict() )
			return;
				
		CBaseEntity::EmitSound( filter, GetOwner()->entindex(), shootsound, &GetOwner()->GetAbsOrigin() ); 
}

bool C_BaseHLBludgeonWeapon::ShouldPredict()
{
	if (GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer())
		return true;

	return BaseClass::ShouldPredict();
}