//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "basehlcombatweapon_shared.h"

#ifndef C_BASEHLCOMBATWEAPON_H
#define C_BASEHLCOMBATWEAPON_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#define CHLMachineGun C_HLMachineGun
#define CHLSelectFireMachineGun C_HLSelectFireMachineGun
#endif

class C_HLMachineGun : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_HLMachineGun, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_HLMachineGun();
	
	virtual void WeaponSound(WeaponSound_t sound_type, float soundtime = 0.0f);

	virtual bool IsPredicted() const { return true; }
	virtual bool ShouldPredict();

	virtual void PrimaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual bool Deploy( void );

protected:

	int	m_nShotsFired;	// Number of consecutive shots fired
};

class C_HLSelectFireMachineGun : public C_HLMachineGun
{
public:

	C_HLSelectFireMachineGun();
	
	virtual void WeaponSound(WeaponSound_t sound_type, float soundtime = 0.0f);

	virtual bool IsPredicted() const { return true; }
	virtual bool ShouldPredict();

	DECLARE_CLASS( C_HLSelectFireMachineGun, C_HLMachineGun );
	DECLARE_CLIENTCLASS();
};

class C_BaseHLBludgeonWeapon : public C_BaseHLCombatWeapon
{
public:

	C_BaseHLBludgeonWeapon();
	
	virtual void WeaponSound( WeaponSound_t sound_type, float soundtime = 0.0f );

	virtual bool IsPredicted() const { return true; }
	virtual bool ShouldPredict();

	DECLARE_CLASS( C_BaseHLBludgeonWeapon, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
};

#endif // C_BASEHLCOMBATWEAPON_H
