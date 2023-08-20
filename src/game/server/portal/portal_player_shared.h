//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef PORTAL_PLAYER_SHARED_H
#define PORTAL_PLAYER_SHARED_H
#pragma once

#define PORTAL_PUSHAWAY_THINK_INTERVAL		(1.0f / 20.0f)
#include "studio.h"

enum
{
	PLAYER_MODEL_CHELL = 1,
	PLAYER_MODEL_MEL,
	PLAYER_MODEL_MALE_PORTAL_PLAYER,
};

enum
{
	PLAYER_SOUNDS_CITIZEN = 0,
	PLAYER_SOUNDS_COMBINESOLDIER,
	PLAYER_SOUNDS_METROPOLICE,
	PLAYER_SOUNDS_MAX,
};

enum 
{
	CONCEPT_PLAYER_DEAD,
	CONCEPT_CHELL_IDLE,
	CONCEPT_ABBY_IDLE,
	CONCEPT_MEL_IDLE,
	CONCEPT_MALE_PORTAL_PLAYER_IDLE,
	//Escape Idles
	CONCEPT_MEL_ESCAPE_IDLE,
};

enum StickCameraCorrectionMethod
{
	QUATERNION_CORRECT = 0,
	ROTATE_UP,
	SNAP_UP,
	DO_NOTHING
};

extern const char *g_pszPortalPlayerConcepts[];
int GetChellConceptIndexFromString( const char *pszConcept );

#if defined( CLIENT_DLL )
#define CPortal_Player C_Portal_Player
#endif

#define OLD_HL2DM_ANIMSTATE 0

#if OLD_HL2DM_ANIMSTATE

class CPlayerAnimState
{
public:
	enum
	{
		TURN_NONE = 0,
		TURN_LEFT,
		TURN_RIGHT
	};

	CPlayerAnimState( CPortal_Player *outer );

	Activity			BodyYawTranslateActivity( Activity activity );

	void				Update();

	const QAngle&		GetRenderAngles();
				
	void				GetPoseParameters( CStudioHdr *pStudioHdr, float poseParameter[MAXSTUDIOPOSEPARAM] );

	CPortal_Player		*GetOuter();

private:
	void				GetOuterAbsVelocity( Vector& vel );

	int					ConvergeAngles( float goal,float maxrate, float dt, float& current );

	void				EstimateYaw( void );
	void				ComputePoseParam_BodyYaw( void );
	void				ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr );
	void				ComputePoseParam_BodyLookYaw( void );

	void				ComputePlaybackRate();
	
	void    Teleport( Vector *pOldOrigin, QAngle *pOldAngles );

	CPortal_Player		*m_pOuter;

	float				m_flGaitYaw;
	float				m_flStoredCycle;

	// The following variables are used for tweaking the yaw of the upper body when standing still and
	//  making sure that it smoothly blends in and out once the player starts moving
	// Direction feet were facing when we stopped moving
	float				m_flGoalFeetYaw;
	float				m_flCurrentFeetYaw;

	float				m_flCurrentTorsoYaw;

	// To check if they are rotating in place
	float				m_flLastYaw;
	// Time when we stopped moving
	float				m_flLastTurnTime;

	// One of the above enums
	int					m_nTurningInPlace;

	QAngle				m_angRender;

	float				m_flTurnCorrectionTime;
};

#endif //OLD_HL2DM_ANIMSTATE

#endif //PORTAL_PLAYER_SHARED_h
