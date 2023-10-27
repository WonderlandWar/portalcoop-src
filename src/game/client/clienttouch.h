#ifndef CLIENT_TOUCH_H
#define CLIENT_TOUCH_H

#include "c_baseentity.h"

class CClientTouchable
{
public:

	CClientTouchable();
	~CClientTouchable();

	// Put this in your ClientThink function or a function that gets constantly called
	virtual void HandleFakeTouch( void );
	
	virtual void HandleFakePhysicsTouch( void );

	// This is our touch condition.
	virtual bool TouchCondition( C_BaseEntity *pOther ) { return true; }
	
	bool EntityIsInBounds( C_BaseEntity *pEntity );
	bool IsTouchingEntity( C_BaseEntity *pEntity );
	
	CUtlVector<EHANDLE> m_TouchingEntities;

	virtual C_BaseEntity *GetTouchableBaseEntity( void ) { return NULL; }
	
protected:
	// Set this in your constructor
	//float m_flFakeTouchRadius;

private:

	void TestEndTouch( void );

};

extern CUtlVector<CClientTouchable*> g_AllTouchables;


#define DECLARE_TOUCHABLE() \
	C_BaseEntity *GetTouchableBaseEntity( void ) { return this; } \

#endif