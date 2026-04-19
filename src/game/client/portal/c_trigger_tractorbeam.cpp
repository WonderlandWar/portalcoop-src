#include "cbase.h"
#include "trigger_tractorbeam_shared.h"
#include "c_portal_player.h"
#include "soundinfo.h"
#include "c_trigger_tractorbeam.h"

#undef CProjectedTractorBeamEntity // Just in case

IMPLEMENT_CLIENTCLASS_DT( C_ProjectedTractorBeamEntity, DT_ProjectedTractorBeamEntity, CProjectedTractorBeamEntity )
	RecvPropEHandle( RECVINFO(m_hTractorBeamTrigger) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ProjectedTractorBeamEntity )

	DEFINE_FIELD( m_hTractorBeamTrigger, FIELD_EHANDLE ),

END_PREDICTION_DATA()

C_ProjectedTractorBeamEntity::C_ProjectedTractorBeamEntity()
{

}

C_ProjectedTractorBeamEntity::~C_ProjectedTractorBeamEntity()
{

}

void C_ProjectedTractorBeamEntity::GetProjectionExtents( Vector &outMins, Vector &outMaxs )
{
	outMins.x = -2.0;
	outMins.y = -2.0;
	outMins.z = 0.0;
	outMaxs.x = 2.0;
	outMaxs.y = 2.0;
	outMaxs.z = 0.0;
}

void C_ProjectedTractorBeamEntity::OnProjected( void )
{
	BaseClass::OnProjected();

	C_Trigger_TractorBeam *pTrigger = m_hTractorBeamTrigger;
	if ( pTrigger )
	{
		pTrigger->SetPredictionEligible( GetPredictionEligible() );

		if ( IsPlayerSimulated() )
		{
			if ( GetSimulatingPlayer() )
				pTrigger->SetPlayerSimulated( GetSimulatingPlayer() );
			else
				pTrigger->SetPlayerSimulated( NULL );
		}
		else
		{
			pTrigger->UnsetPlayerSimulated();
		}
		
		float flForce = pTrigger->GetLinearForce();
		if ( pTrigger->IsReversed() )
			flForce = -flForce;

		pTrigger->UpdateBeam( GetStartPoint(), GetEndPoint(), flForce );
	}
}

#undef CTrigger_TractorBeam

IMPLEMENT_CLIENTCLASS_DT( C_Trigger_TractorBeam, DT_Trigger_TractorBeam, CTrigger_TractorBeam )

	RecvPropFloat( RECVINFO( m_gravityScale ) ),
	RecvPropFloat( RECVINFO( m_addAirDensity ) ),
	RecvPropFloat( RECVINFO( m_linearLimit ) ),
	RecvPropFloat( RECVINFO( m_linearLimitDelta ) ),
	RecvPropFloat( RECVINFO( m_linearLimitTime ) ),
	RecvPropFloat( RECVINFO( m_linearLimitStart ) ),
	RecvPropFloat( RECVINFO( m_linearLimitStartTime ) ),
	RecvPropFloat( RECVINFO( m_linearScale ) ),
	RecvPropFloat( RECVINFO( m_angularLimit ) ),
	RecvPropFloat( RECVINFO( m_angularScale ) ),
	RecvPropFloat( RECVINFO( m_linearForce ) ),
	RecvPropFloat( RECVINFO( m_flRadius ) ),
	
	RecvPropQAngles( RECVINFO( m_linearForceAngles ) ),
	RecvPropVector( RECVINFO( m_vStart ), 0, &C_Trigger_TractorBeam::RecvProxy_Start ),
	RecvPropVector( RECVINFO( m_vEnd ), 0, &C_Trigger_TractorBeam::RecvProxy_End ),
	
	RecvPropBool( RECVINFO( m_bDisabled ) ),
	RecvPropBool( RECVINFO( m_bReversed ) ),
	RecvPropBool( RECVINFO( m_bFromPortal ) ),
	RecvPropBool( RECVINFO( m_bToPortal ) ),
	RecvPropBool( RECVINFO( m_bDisablePlayerMove ) ),

END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( trigger_tractorbeam, C_Trigger_TractorBeam )


BEGIN_PREDICTION_DATA( C_Trigger_TractorBeam )

	DEFINE_PRED_FIELD( m_linearForceAngles, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vStart, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vEnd, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()


IMPLEMENT_AUTO_LIST( ITriggerTractorBeamAutoList )

C_Trigger_TractorBeam::C_Trigger_TractorBeam()
{
	m_hProxyEntity = NULL;
	m_bDisabled = false;
	m_pMaterial1 = NULL;
	m_pPanelMaterial = NULL;
	m_flPanelSpinStartTime = 0.0f;
}

C_Trigger_TractorBeam::~C_Trigger_TractorBeam()
{

}

void C_Trigger_TractorBeam::Spawn( void )
{
	BaseClass::Spawn();
	if (!m_pMaterial1)
		m_pMaterial1 = materials->FindMaterial( "effects/tractor_beam", NULL, false );
	
	if (!m_pPanelMaterial)
		m_pPanelMaterial = materials->FindMaterial("particle/particle_ring_pulled_add_oriented", NULL, false);

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_Trigger_TractorBeam::UpdateOnRemove( void )
{
	if (m_pController)
	{
		IPhysicsObject *pList[1024];
		int objectcount = m_pController->CountObjects();
		m_pController->GetObjects( pList );
		for ( int i = 0; i < objectcount; ++i)
		{
			if ( pList[i] )
				pList[i]->Wake();
		}

		physenv->DestroyMotionController( m_pController );
		m_pController = NULL;
	}

	for ( int i = 1; i <= MAX_PLAYERS; ++i)
	{
		C_Portal_Player* pPlayer = (C_Portal_Player *)UTIL_PlayerByIndex( i );
		
		if ( !pPlayer )
			continue;

		C_Trigger_TractorBeam *pTrigger = pPlayer->GetTractorBeam();

		if ( pTrigger == this )
			pPlayer->SetLeaveTractorBeam( this, false );
	}
	BaseClass::UpdateOnRemove();
}

void C_Trigger_TractorBeam::StartTouch( C_BaseEntity *pOther )
{
	C_Portal_Player *pPlayer = ToPortalPlayer( pOther );

	if ( pPlayer )
	{
		pPlayer->SetInTractorBeam( this );
	}
	else
	{
		if ( m_pController && pOther->VPhysicsGetObject() )
		{
			 m_pController->AttachObject( pOther->VPhysicsGetObject(), true );
		}
	}

	BaseClass::StartTouch( pOther );
}

int C_Trigger_TractorBeam::DrawModel( int flags )
{
	//bool bDrawOuterColumn = ( gpu_level.GetInt() > 1 );
	
	Vector vDir = m_vEnd - m_vStart;

	float flLength = VectorNormalize(vDir);

	//if ( gpGlobals->curtime < ( m_flStartTime + TRACTOR_BEAM_EXTEND_TIME ) )
	//{
		//float flLengthMod;

		//float flMod2;
		//float flRemainingTime = (gpGlobals->curtime - m_flStartTime) / ( ( m_flStartTime + 0.5 ) - m_flStartTime );
		//if (flRemainingTime >= 0.0)
		//{
		//	if (flRemainingTime <= 1.0)
		//		flMod2 = flRemainingTime;
		//	else
		//		flMod2 = 1.0;
		//}
		//else
		//{
		//	flMod2 = 0.0;
		//}
		//flLengthMod = ( (flMod2 * flMod2) * 3.0 ) - ( ( (flMod2 * flMod2) * 2.0 ) * flMod2 );
		//flLength = flRemainingTime * flLength;
	//}

	matrix3x4_t xform;
	QAngle angles;

	VectorAngles( vDir, angles );
	AngleMatrix( angles, m_vStart, xform );

	Vector xAxis;
	Vector yAxis;
	MatrixGetColumn( xform, 2, xAxis);
	MatrixGetColumn( xform, 1, yAxis);

	// This probably used the preprocessor definition
	C_Trigger_TractorBeam::DrawColumn( m_pMaterial1, m_vStart, vDir, flLength, xAxis, yAxis, TRACTOR_BEAM_RADIUS_INNER, 1.0, m_bFromPortal, m_bToPortal, 0.0 );

	// Outer column (tractor_beam2) is skipped on low GPU levels
	//if (bDrawOuterColumn)
	//	C_Trigger_TractorBeam::DrawColumn( m_pMaterial2, m_vStart, vDir, flLength, xAxis, yAxis, TRACTOR_BEAM_RADIUS_OUTER, 1.0, m_bFromPortal, m_bToPortal, 0.0 );

	Vector vDirReversed = -vDir;
	DrawRotatingPanels( m_vStart, vDirReversed );
	DrawRotatingPanels( m_vEnd, vDir );

	return 1;
}

void C_Trigger_TractorBeam::DrawRotatingPanels( const Vector &vImpact, const Vector &vNormal )
{
	if (!m_pPanelMaterial)
		return;

	Vector vPanelNormal = vNormal;
	if (VectorNormalize(vPanelNormal) <= 0.0f)
		return;

	Vector vReference = (fabsf(vPanelNormal.z) < 0.999f) ? Vector(0.0f, 0.0f, 1.0f) : Vector(0.0f, 1.0f, 0.0f);
	Vector vPlaneRight;
	CrossProduct(vReference, vPanelNormal, vPlaneRight);
	if (VectorNormalize(vPlaneRight) <= 0.0f)
		return;

	Vector vPlaneUp;
	CrossProduct(vPanelNormal, vPlaneRight, vPlaneUp);
	VectorNormalize(vPlaneUp);

	const float flSpinSpeedDegrees = 180.0f;
	const float flElapsed = gpGlobals->curtime - m_flPanelSpinStartTime;
	const float flClockwise = DEG2RAD(flElapsed * flSpinSpeedDegrees);
	const float flCounterClockwise = -flClockwise;
	const float flPi = 3.14159265f;
	const float flHalfPi = flPi * 0.5f;

	const float flHalfPanelWidth = TRACTOR_BEAM_RADIUS_INNER * 1.25f;
	const float flHalfPanelLength = TRACTOR_BEAM_RADIUS_INNER * 1.25f;
	const float flAngles[4] = {
		flClockwise,
		flClockwise + flPi,
		flCounterClockwise + flHalfPi,
		flCounterClockwise + (flHalfPi * 3.0f),
	};

	const int nColorR = m_bReversed ? 255 : 0;
	const int nColorG = m_bReversed ? 56 : 120;
	const int nColorB = m_bReversed ? 8 : 189;
	const int nColorA = 90;

	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->Bind(m_pPanelMaterial, GetClientRenderable());
	IMesh *pMesh = pRenderContext->GetDynamicMesh(false, NULL, NULL, m_pPanelMaterial);

	CMeshBuilder meshBuilder;
	meshBuilder.Begin(pMesh, MATERIAL_QUADS, 4);

	Vector vOrigin = vImpact - vNormal;

	for (int i = 0; i < ARRAYSIZE(flAngles); ++i)
	{
		const float flCos = cosf(flAngles[i]);
		const float flSin = sinf(flAngles[i]);
		const Vector vPanelRight = (vPlaneRight * flCos) + (vPlaneUp * flSin);
		Vector vPanelUp = CrossProduct(vPanelNormal, vPanelRight);
		VectorNormalize(vPanelUp);

		const Vector vStartA = (vOrigin - (vPanelUp * flHalfPanelLength)) - (vPanelRight * flHalfPanelWidth);
		const Vector vStartB = (vOrigin - (vPanelUp * flHalfPanelLength)) + (vPanelRight * flHalfPanelWidth);
		const Vector vEndA = (vOrigin + (vPanelUp * flHalfPanelLength)) - (vPanelRight * flHalfPanelWidth);
		const Vector vEndB = (vOrigin + (vPanelUp * flHalfPanelLength)) + (vPanelRight * flHalfPanelWidth);

		meshBuilder.Color4ub(nColorR, nColorG, nColorB, nColorA);
		meshBuilder.TexCoord2f(0, 0.0f, 0.0f);
		meshBuilder.Position3fv(vStartA.Base());
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub(nColorR, nColorG, nColorB, nColorA);
		meshBuilder.TexCoord2f(0, 1.0f, 0.0f);
		meshBuilder.Position3fv(vStartB.Base());
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub(nColorR, nColorG, nColorB, nColorA);
		meshBuilder.TexCoord2f(0, 1.0f, 1.0f);
		meshBuilder.Position3fv(vEndB.Base());
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub(nColorR, nColorG, nColorB, nColorA);
		meshBuilder.TexCoord2f(0, 0.0f, 1.0f);
		meshBuilder.Position3fv(vEndA.Base());
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}

void C_Trigger_TractorBeam::DrawColumn( IMaterial *pMaterial, Vector &vecStart, Vector vDir, float flLength,
	Vector &vecXAxis, Vector &vecYAxis, float flRadius, float flAlpha, bool bPinchIn, bool bPinchOut, float flTextureOffset )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial, GetClientRenderable() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, pMaterial );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 256 );
	
	DrawColumnSegment( meshBuilder, vecStart, vDir, flLength, vecXAxis, vecYAxis, flRadius, flAlpha, flTextureOffset, pMaterial->GetVertexFormat() );
	
	meshBuilder.End();
	
	pMesh->Draw();
}

void C_Trigger_TractorBeam::DrawColumnSegment( CMeshBuilder &meshBuilder, Vector &vecStart, Vector &vDir, float flLength, Vector &vecXAxis,
										Vector &vecYAxis, float flRadius, float flAlpha, float flTextureOffset, VertexFormat_t vertexFormat )
{
	bool bSetTangentS = (vertexFormat & VERTEX_TANGENT_S) != 0;
	bool bSetTangentT = (vertexFormat & VERTEX_TANGENT_T) != 0;
	
	Vector vecPosition = vecStart + (vecXAxis *flRadius);

	int colorR = 10;
	int colorG;
	int colorB;
	if ( m_bReversed )
	{
		colorR = 255;
		colorG = 160;
		colorB = 0;
	}
	else
	{
		colorB = 255;
		colorG = 190;
	}
	
	float flLastV = 0.0;
	float flU = flLength * 0.00390625;

	int i = 1;
	while (1)
	{
		Vector vecLastPosition = vecPosition;
		float fl = i * 0.098174773; // Unnamed
		float flCos = cos( fl );
		float flSin = sin( fl );
		
		float flV = i * 0.015625; 
		vecPosition = (vecStart + ( vecXAxis * flCos ) * flRadius) + ( ( vecYAxis * flSin ) * flRadius );
		Vector normal = vecStart - vecPosition;

		Vector tangents = vDir;
		VectorNormalize( tangents );
		Vector tangentt = (tangents * normal) - (tangents * normal);
		VectorNormalize( tangentt );
		VectorNormalize( normal );
		
		// Vert 1
		meshBuilder.Color3ub( colorR, colorG, colorB );
		meshBuilder.TexCoord2f( 0, 0, flV );
		meshBuilder.Position3fv( vecPosition.Base() );

		if ( vertexFormat & VERTEX_NORMAL )
			meshBuilder.Normal3fv( normal.Base() );

		if ( bSetTangentS )
			meshBuilder.TangentS3fv( tangents.Base() );
		if ( bSetTangentT )
			meshBuilder.TangentT3fv( tangentt.Base() );

		meshBuilder.AdvanceVertex();

		// Vert 2
		Vector vert2 = vDir * flLength + vecPosition;
		meshBuilder.Color3ub( colorR, colorG, colorB );
		meshBuilder.TexCoord2f( 0, flU, flV );
		meshBuilder.Position3fv( vert2.Base() );
		
		if ( vertexFormat & VERTEX_NORMAL )
			meshBuilder.Normal3fv( normal.Base() );

		if ( bSetTangentS )
			meshBuilder.TangentS3fv( tangents.Base() );
		if ( bSetTangentT )
			meshBuilder.TangentT3fv( tangentt.Base() );

		meshBuilder.AdvanceVertex();

		// Vert 3
		Vector vert3 = vDir * flLength + vecLastPosition;
		normal = vecStart - vecLastPosition;
		VectorNormalize( normal );
		tangentt = (tangents * normal) - (tangents * normal);
		VectorNormalize( tangentt );
		
		meshBuilder.Color3ub( colorR, colorG, colorB );
		meshBuilder.TexCoord2f( 0, flU, flLastV );
		meshBuilder.Position3fv( vert3.Base() );
		
		if ( vertexFormat & VERTEX_NORMAL )
			meshBuilder.Normal3fv( normal.Base() );

		if ( bSetTangentS )
			meshBuilder.TangentS3fv( tangents.Base() );
		if ( bSetTangentT )
			meshBuilder.TangentT3fv( tangentt.Base() );

		meshBuilder.AdvanceVertex();

		// Vert 4
		meshBuilder.Color3ub( colorR, colorG, colorB );
		meshBuilder.TexCoord2f( 0, 0, flLastV );
		meshBuilder.Position3fv( vecLastPosition.Base() );
		
		if ( vertexFormat & VERTEX_NORMAL )
			meshBuilder.Normal3fv( normal.Base() );

		if ( bSetTangentS )
			meshBuilder.TangentS3fv( tangents.Base() );
		if ( bSetTangentT )
			meshBuilder.TangentT3fv( tangentt.Base() );

		meshBuilder.AdvanceVertex();

		if (++i > 64)
			break;

		flLastV = flV;
	}
}

bool C_Trigger_TractorBeam::GetSoundSpatialization( SpatializationInfo_t& info )
{
	bool bResult = false;
	float outT[4]; // [esp+2Ch] [ebp-1Ch] BYREF
		
	if ( IsDormant() )
		return 0;

	if (info.pOrigin)
	{
		CalcClosestPointOnLine( info.info.vListenerOrigin, m_vStart, m_vEnd, *info.pOrigin, outT );
		if (outT[0] >= 0.0)
		{
			if (outT[0] > 1.0)
			{
				bResult = true;
				*info.pOrigin = m_vEnd;
				if (!info.pAngles)
					return bResult;
				goto LABEL_6;
			}
		}
		else
		{
			*info.pOrigin = m_vStart;
		}
	}
	bResult = true;
	if (info.pAngles)
	{
	LABEL_6:
			
		QAngle qAng = CollisionProp()->GetCollisionAngles();

		*info.pAngles = qAng;
	}
	
	return bResult;
}

void C_Trigger_TractorBeam::CreateParticles( void )
{	
	m_flPanelSpinStartTime = gpGlobals->curtime;
}

void C_Trigger_TractorBeam::PhysicsSimulate( void )
{
	BaseClass::PhysicsSimulate();
}

void C_Trigger_TractorBeam::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType != DATA_UPDATE_CREATED )
	{
		if ( m_bRecreateParticles )
		{
			m_bRecreateParticles = false;
			CreateParticles();
		}
	}
	else
	{
		if ( physenv && !m_pController )
		{
			m_pController = physenv->CreateMotionController( this );

			C_ProjectedTractorBeamEntity *pOwner = (C_ProjectedTractorBeamEntity *)GetOwnerEntity();

			float flForce = m_linearForce;
			if (m_bReversed)
				flForce = -m_linearForce;
			
			UpdateBeam( pOwner->GetStartPoint(), pOwner->GetEndPoint(), flForce );
			m_hProxyEntity = pOwner;
		}

		CreateParticles();
	}
}

C_BasePlayer *C_Trigger_TractorBeam::GetPredictionOwner( void )
{
	return GetSimulatingPlayer();
}

bool C_Trigger_TractorBeam::ShouldPredict( void )
{
	return GetSimulatingPlayer() == C_BasePlayer::GetLocalPlayer();
}

void C_Trigger_TractorBeam::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( !V_stricmp(pszParticleName, "tractor_beam_src") )
		pNewParticleEffect->SetControlPoint( 2, m_vEnd );
}

RenderGroup_t C_Trigger_TractorBeam::GetRenderGroup( void )
{
	return RENDER_GROUP_TRANSLUCENT_ENTITY;
}

void C_Trigger_TractorBeam::GetToolRecordingState( KeyValues *msg )
{
	//No IFM for Swarm!
#if 0
	const char *HandlerIDKeyString; // eax

	BaseClass::GetToolRecordingState(msg);
	KeyValues *pKeyValues = CIFM_EntityKeyValuesHandler_AutoRegister::FindOrCreateNonConformantKeyValues(msg);
	HandlerIDKeyString = CIFM_EntityKeyValuesHandler_AutoRegister::GetHandlerIDKeyString();
	pKeyValues->SetString( HandlerIDKeyString, "C_Trigger_TractorBeam");

	pKeyValues->SetInt( "entIndex", entindex());
	pKeyValues->SetFloat( "starttime", m_flStartTime);
	pKeyValues->SetInt( "reversed", m_bReversed);
	pKeyValues->SetFloat( "force", m_linearForce);

	pKeyValues->SetFloat( "sp_x", m_vStart.x);
	pKeyValues->SetFloat( "sp_y", m_vStart.y);
	pKeyValues->SetFloat( "sp_z", m_vStart.z);

	pKeyValues->SetFloat( "ep_x", m_vEnd.x);
	pKeyValues->SetFloat( "ep_y", m_vEnd.y);
	pKeyValues->SetFloat( "ep_z", m_vEnd.z);
#endif
}

void C_Trigger_TractorBeam::RestoreToToolRecordedState( KeyValues *pKV )
{
#if 0
	_DWORD vecMin[2]; // [esp+Ch] [ebp-18h] BYREF
	unsigned __int64 vecMin_8; // [esp+14h] [ebp-10h] BYREF

	m_flStartTime = pKV->GetFloat("starttime", 0.0);
	m_bReversed = pKV->GetInt("reversed", 0) == 1;
	m_linearForce = pKV->GetFloat("force", 0.0);
	m_vStart.x = pKV->GetFloat("sp_x", 0.0);
	m_vStart.y = pKV->GetFloat("sp_y", 0.0);
	m_vStart.z = pKV->GetFloat("sp_z", 0.0);
	m_vEnd.x = pKV->GetFloat("ep_x", 0.0);
	m_vEnd.y = pKV->GetFloat("ep_y", 0.0);
	m_vEnd.z = pKV->GetFloat("ep_z", 0.0);
	float v5 = 16384.0;
	float v6 = 16384.0;
	*(float *)vecMin = 16384.0;
	*(float *)&vecMin[1] = 16384.0;
	vecMin_8 = __PAIR64__(LODWORD(16384.0), LODWORD(16384.0));
	SetSize( (const Vector *)vecMin, (const Vector *)((char *)&vecMin_8 + 4));
#endif
	m_pMaterial1 = materials->FindMaterial("effects/tractor_beam", 0, 0, 0);
}

bool C_Trigger_TractorBeam::ShouldDraw( void )
{
	return true;
}

float C_Trigger_TractorBeam::GetLinearForce( void )
{
	return m_linearForce;
}

bool C_Trigger_TractorBeam::HasLinearLimit( void )
{
	return m_linearLimit != 0.0;
}

bool C_Trigger_TractorBeam::HasLinearScale( void )
{
	return m_linearScale != 1.0;
}

bool C_Trigger_TractorBeam::HasAngularScale( void )
{
	return m_angularScale != 1.0;
}

bool C_Trigger_TractorBeam::HasAngularLimit( void )
{
	return m_angularLimit != 0.0;
}

bool C_Trigger_TractorBeam::HasAirDensity( void )
{
	return m_addAirDensity != 0.0;
}

void C_Trigger_TractorBeam::RecvProxy_Start( const CRecvProxyData *pData, void *pStruct, void *pOut )
{	
	C_Trigger_TractorBeam *pTractorBeam = static_cast<C_Trigger_TractorBeam*>( pStruct );

	Vector vStart = Vector( pData->m_Value.m_Vector[0], pData->m_Value.m_Vector[1], pData->m_Value.m_Vector[2] );

	if (pTractorBeam->m_vStart != vStart )
	{
		pTractorBeam->m_vStart = vStart;
		pTractorBeam->m_bRecreateParticles = true;
		pTractorBeam->m_flStartTime = gpGlobals->curtime;
	}
}

void C_Trigger_TractorBeam::RecvProxy_End( const CRecvProxyData *pData, void *pStruct, void *pOut )
{	
	C_Trigger_TractorBeam *pTractorBeam = static_cast<C_Trigger_TractorBeam*>( pStruct );
	Vector vEnd = Vector( pData->m_Value.m_Vector[0], pData->m_Value.m_Vector[1], pData->m_Value.m_Vector[2] );

	if ( pTractorBeam->m_vEnd != vEnd )
	{
		Vector vOldDiff = pTractorBeam->m_vEnd - pTractorBeam->m_vStart;
		float fOldLength = VectorNormalize( vOldDiff );

		Vector vNewDiff = vEnd - pTractorBeam->m_vStart;
		float fNewLength = VectorNormalize( vNewDiff );

		if ( fabs( vOldDiff.x - vNewDiff.x ) > 0.1 || fabs( vOldDiff.y - vNewDiff.y ) > 0.1 || fabs( vOldDiff.z - vNewDiff.z ) > 0.1 )
		{
			pTractorBeam->m_flStartTime = gpGlobals->curtime;
		}
		else if ( fNewLength > 0.0 )
		{
			pTractorBeam->m_flStartTime = gpGlobals->curtime
				- ((((((gpGlobals->curtime
				- pTractorBeam->m_flStartTime)
				+ (gpGlobals->curtime
				- pTractorBeam->m_flStartTime))
				+ 0.0)
				* fOldLength)
				/ fNewLength)
				* 0.5);
		}

		pTractorBeam->m_vEnd = vEnd;
		pTractorBeam->m_bRecreateParticles = true;
	}
}

// Tractor Beam Proxy

bool CTractorBeamProxy::Init( IMaterial* pMaterial, KeyValues* pKeyValues )
{
	// This doesn't really need to exist, but the decompiler indicates it might have
	return CResultProxy::Init(pMaterial, pKeyValues);
}

void CTractorBeamProxy::OnBind( void* pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity* pEntity = BindArgToEntity(pC_BaseEntity);
	if ( !pEntity )
		return;

	C_Trigger_TractorBeam* pTractorBeam = dynamic_cast<C_Trigger_TractorBeam*>(pEntity);
	if ( !pTractorBeam )
		return;

	SetFloatResult( pTractorBeam->GetLinearForce() / 512 );
}

EXPOSE_INTERFACE( CTractorBeamProxy, IMaterialProxy, "TractorBeam" IMATERIAL_PROXY_INTERFACE_VERSION );
