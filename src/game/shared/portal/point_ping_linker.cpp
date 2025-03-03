#include "cbase.h"
#include "point_ping_linker.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( PointPingLinker, DT_PortalGameRulesProxy )

BEGIN_NETWORK_TABLE( CPointPingLinker, DT_PointPingLinker )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS_ALIASED( point_ping_linker, PointPingLinker )

#ifdef GAME_DLL

BEGIN_DATADESC(CPointPingLinker)

	DEFINE_KEYFIELD( m_iszEntityName1, FIELD_STRING, "LinkedEntity1" ),
	DEFINE_KEYFIELD( m_iszEntityName2, FIELD_STRING, "LinkedEntity2" ),
	DEFINE_KEYFIELD( m_iszEntityName3, FIELD_STRING, "LinkedEntity3" ),
	DEFINE_KEYFIELD( m_iszEntityName4, FIELD_STRING, "LinkedEntity4" ),
	DEFINE_KEYFIELD( m_iszEntityName5, FIELD_STRING, "LinkedEntity5" ),
	DEFINE_KEYFIELD( m_iszEntityName6, FIELD_STRING, "LinkedEntity6" ),
	DEFINE_KEYFIELD( m_iszEntityName7, FIELD_STRING, "LinkedEntity7" ),
	DEFINE_KEYFIELD( m_iszEntityName8, FIELD_STRING, "LinkedEntity8" ),
	DEFINE_KEYFIELD( m_iszEntityName9, FIELD_STRING, "LinkedEntity9" ),
	DEFINE_KEYFIELD( m_iszEntityName10, FIELD_STRING, "LinkedEntity10" ),

END_DATADESC()
#endif

CPointPingLinker::CPointPingLinker()
{

}

#ifdef GAME_DLL
void CPointPingLinker::PingLinkedEntity( const char *pszName, float flTime, Vector vColor )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByName( pEntity, pszName ) ) != NULL )
	{
		CBaseAnimating *pAnimating = pEntity ? pEntity->GetBaseAnimating() : NULL;
		if ( !pAnimating )
			continue;
		
		if (pAnimating->m_bGlowEnabled)
		{
			pAnimating->RemoveGlowEffect();
			pAnimating->m_bGlowEnabled.Set(false);
		}

		pAnimating->SetGlowEffectColor(vColor.x, vColor.y, vColor.z);
		pAnimating->AddGlowTime(gpGlobals->curtime);
		pAnimating->RemoveGlowTime(flTime);
	}
}


void CPointPingLinker::PingLinkedEntities( float flTime, Vector vColor, CBaseEntity *pOwner, const char* pszSoundName )
{
	PingLinkedEntity( m_iszEntityName1.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName2.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName3.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName4.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName5.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName6.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName7.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName8.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName9.ToCStr(), flTime, vColor );
	PingLinkedEntity( m_iszEntityName10.ToCStr(), flTime, vColor );
	
	extern void ShowAnnotation( Vector location, int follow_entindex, int entindex, int forcedpingicon = -1 );
	ShowAnnotation( GetAbsOrigin(), entindex(), pOwner->entindex(), m_iPingIcon );
}

bool CPointPingLinker::HasThisEntity( CBaseAnimating *pAnimating )
{
	if (!pAnimating)
		return false;	
	
	CBaseAnimating *pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName1.ToCStr()));
	if ( pFound == pAnimating)
		return true;
	
	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName2.ToCStr()));
	
	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName3.ToCStr()));

	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName4.ToCStr()));

	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName5.ToCStr()));

	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName6.ToCStr()));

	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName7.ToCStr()));

	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName8.ToCStr()));

	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName9.ToCStr()));

	if (pFound == pAnimating)
		return true;

	pFound = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName(NULL, m_iszEntityName10.ToCStr()));

	if (pFound == pAnimating)
		return true;
	
	

	return false;
}
#endif