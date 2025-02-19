#ifndef COMMENTARY_SYSTEM_H
#define COMMENTARY_SYSTEM_H

#include "cbase.h"

//-----------------------------------------------------------------------------
// Purpose: An entity that marks a spot for a piece of commentary
//-----------------------------------------------------------------------------
class CPointCommentaryNode : public CBaseAnimating
{
	DECLARE_CLASS( CPointCommentaryNode, CBaseAnimating );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void Spawn( void );
	void Precache( void );
	void Activate( void );
	void SpinThink( void );
	void StartCommentary( CBasePlayer *pPlayer );
	void FinishCommentary( bool bBlendOut = true );
	void CleanupPostCommentary( void );
	void UpdateViewThink( void );
	void UpdateViewPostThink( void );
	bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );
	bool HasViewTarget( void ) { return (m_hViewTarget != NULL || m_hViewPosition.Get() != NULL); }
	bool PreventsMovement( void );
	bool CannotBeStopped( void ) { return (m_bUnstoppable || m_bPreventChangesWhileMoving); }
	int  UpdateTransmitState( void );
	void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
	void SetDisabled( bool bDisabled );
	void SetNodeNumber( int iCount ) { m_iNodeNumber = iCount; }

	// Called to tell the node when it's moved under/not-under the player's crosshair
	void SetUnderCrosshair( bool bUnderCrosshair );

	// Called when the player attempts to activate the node
	void PlayerActivated( CBasePlayer *pPlayer );
	void StopPlaying( void );
	void AbortPlaying( void );
	void TeleportTo( CBasePlayer *pPlayer );
	bool CanTeleportTo( void );

	// Inputs
	void InputStartCommentary( inputdata_t &inputdata );
	void InputStartUnstoppableCommentary( inputdata_t &inputdata );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	void InputUse( inputdata_t &inputdata );

private:
	string_t	m_iszPreCommands;
	string_t	m_iszPostCommands;
	CNetworkVar( string_t, m_iszCommentaryFile );
	CNetworkVar( string_t, m_iszCommentaryFileNoHDR );
	string_t	m_iszViewTarget;
	EHANDLE		m_hViewTarget;
	EHANDLE		m_hViewTargetAngles;		// Entity used to blend view angles to look at the target
	string_t	m_iszViewPosition;
	CNetworkVar( EHANDLE, m_hViewPosition );
	EHANDLE		m_hViewPositionMover;		// Entity used to blend the view to the viewposition entity
	bool		m_bPreventMovement;
	bool		m_bUnderCrosshair;
	bool		m_bUnstoppable;
	float		m_flFinishedTime;
	Vector		m_vecFinishOrigin;
	QAngle		m_vecOriginalAngles;
	QAngle		m_vecFinishAngles;
	bool		m_bPreventChangesWhileMoving;
	bool		m_bDisabled;
	Vector		m_vecTeleportOrigin;

	COutputEvent	m_pOnCommentaryStarted;
	COutputEvent	m_pOnCommentaryStopped;

	CNetworkVar( bool, m_bActive );
	CNetworkVar( float, m_flStartTime );
	CNetworkVar( string_t, m_iszSpeakers );
	CNetworkVar( int, m_iNodeNumber );
	CNetworkVar( int, m_iNodeNumberMax );
};

#endif // COMMENTARY_SYSTEM_H