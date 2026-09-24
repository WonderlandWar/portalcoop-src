#include "cbase.h"
#include "prop_testchamber_sign_shared.h"
#include "c_props.h"
#include "debugoverlay_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum
{
	TESTSIGN_SKIN_UNUSED,
	TESTSIGN_SKIN_INACTIVE,
	TESTSIGN_SKIN_STARTUP_STAGE1,
	TESTSIGN_SKIN_STARTUP_STAGE2,
	TESTSIGN_SKIN_STARTUP_STAGE3,
	TESTSIGN_SKIN_STARTUP_STAGE4,
	TESTSIGN_SKIN_ACTIVE,
};

enum
{
	TESTSIGN_SKINSET_AWE_TOTAL,
	TESTSIGN_SKINSET_AWE_TOTAL_GREY,
	TESTSIGN_SKINSET_NEWSIGNAGE_BACK02,
	TESTSIGN_SKINSET_NEWSIGNAGE_BACK01,
	TESTSIGN_SKINSET_AWE_BLANK,
};

#define MAX_PROGRESS_BAR_QUADS 19

#define AWE_APERTURE_LOGO_HALF_WIDTH 10.57466
#define AWE_APERTURE_LOGO_HALF_HEIGHT 3.0214

#define AWE_HAZARD_ICON_HALF_WIDTH 6.4884
#define AWE_HAZARD_ICON_HALF_HEIGHT 6.4884

#define AWE_SEPARATOR_BAR_WIDTH 8.9866
#define AWE_SEPARATOR_BAR_HEIGHT 0.6077

#define AWE_PROGRESS_BAR_WIDTH 3.7839
#define AWE_PROGRESS_BAR_HEIGHT 4.4243

#define AWE_BIG_DIGIT_HALF_WIDTH 11.896215
#define AWE_BIG_DIGIT_HALF_HEIGHT 34.3518

#define AWE_SMALL_DIGIT_HALF_WIDTH 1.4563
#define AWE_SMALL_DIGIT_HALF_HEIGHT 1.845

#define AWE_TOTAL_CHAMBERS_LEGACY_HALF_WIDTH 3.13175
#define AWE_TOTAL_CHAMBERS_LEGACY_HALF_HEIGHT 1.845

#define AWE_TOTAL_SLASH_HALF_WIDTH 0.78295
#define AWE_TOTAL_SLASH_HALF_HEIGHT 1.845

class C_PropTestchamberSign : public C_DynamicProp
{
public:
	DECLARE_CLASS( C_PropTestchamberSign, C_DynamicProp );
	DECLARE_CLIENTCLASS();

	C_PropTestchamberSign();

	virtual void OnDataChanged( DataUpdateType_t updatetype );
	virtual void OnPreDataChanged( DataUpdateType_t updatetype );

	CStudioHdr *OnNewModel();	
	virtual int DrawModel( int flags );

	void CalcProgressBarQuads();
	void CalcCurrentDigits();
	void CalcTotalDigits();

	void InitializeMaterials();

	IMaterial *GetAWEMaterialForSkinSet( int skinset );

	void DrawAWE();
	void DrawApertureLogo( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up );
	void DrawHazardIcon( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center, int iconIndex );
	void DrawSeparatorBar( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start );
	void DrawSeparatorBarEdge( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start, bool bSwapUVs );
	void DrawSeparatorBarCenter( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start );
	void DrawProgressBarQuad( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start );
	void DrawBigDigit( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center, int digit );
	void DrawBigDigit_Number1( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center ); // 1 has a special shape that needs to be recreated
	void DrawBigDigit_Number9( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &baseCenter ); // 9 is slightly smaller than the rest
	void DrawSmallDigit( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center, int digit );
	void DrawSlash( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center );
	void DrawTotalChambers( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up );
	void DrawTotalChambersLegacy( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up );
	
private:

	enum
	{
		NUM_SEPARATOR_BARS = 2,
		NUM_DIGITS = 2,
	};

	// Networked variables
	unsigned int m_iChamberNumber;
	unsigned int m_iTotalChambers;
	bool m_bIconActive[NUM_HAZARD_ICONS];
	bool m_bLegacyTotalChambers;

	unsigned int m_iOldChamberNumber;
	unsigned int m_iOldTotalChambers;

	char m_iszAWEMaterial[AWE_MATERIAL_MAX_LEN];
	char m_iszAWEGreyMaterial[AWE_MATERIAL_MAX_LEN];

	// Model info
	int m_iAttachmentApertureLogo;
	int m_iAttachmentHazardIcons[NUM_HAZARD_ICONS];
	int m_iAttachmentSeparatorBars[NUM_SEPARATOR_BARS];
	int m_iAttachmentProgressBar;
	int m_iAttachmentBigDigits[NUM_DIGITS];
	int m_iAttachmentProgressDigits[NUM_DIGITS];
	int m_iAttachmentTotalDigits[NUM_DIGITS];
	int m_iAttachmentTotalChambersLegacy;
	int m_iAttachmentTotalSlash;
	
	// Materials
	IMaterial *m_pMaterialAWETotal;
	IMaterial *m_pMaterialAWETotalGrey;
	IMaterial *m_pMaterialAWEBlank;

	// Other
	unsigned int m_iProgressBarQuads;

	// The digits for the chamber number (8) becomes (08)
	uint8 m_iCurrentDigits[NUM_DIGITS];
	uint8 m_iTotalDigits[NUM_DIGITS];
};

void RecvProxy_AWEMaterials( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	char *pStrOut = (char*)pOut;
	if ( pData->m_pRecvProp->m_StringBufferSize <= 0 )
	{
		return;
	}

	for ( int i=0; i < pData->m_pRecvProp->m_StringBufferSize; i++ )
	{
		pStrOut[i] = pData->m_Value.m_pString[i];
		if ( pStrOut[i] == 0 )
			break;
	}
	
	pStrOut[pData->m_pRecvProp->m_StringBufferSize-1] = 0;

	C_PropTestchamberSign *pSign = (C_PropTestchamberSign*)pStruct;
	pSign->InitializeMaterials();
}

IMPLEMENT_CLIENTCLASS_DT( C_PropTestchamberSign, DT_PropTestchamberSign, CPropTestchamberSign )
	RecvPropInt( RECVINFO( m_iChamberNumber ) ),
	RecvPropInt( RECVINFO( m_iTotalChambers ) ),
	
	RecvPropArray3( RECVINFO_ARRAY(m_bIconActive), RecvPropInt( RECVINFO(m_bIconActive[0]))),
	RecvPropBool( RECVINFO( m_bLegacyTotalChambers ) ),

	RecvPropString( RECVINFO( m_iszAWEMaterial ), 0, RecvProxy_AWEMaterials ),
	RecvPropString( RECVINFO( m_iszAWEGreyMaterial ), 0, RecvProxy_AWEMaterials ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PropTestchamberSign::C_PropTestchamberSign()
{
	int i;

	m_iChamberNumber = m_iOldChamberNumber = 0;
	m_iTotalChambers = m_iOldTotalChambers = 0;

	strcpy( m_iszAWEMaterial, DEFAULT_AWE_MATERIAL );
	strcpy( m_iszAWEGreyMaterial, DEFAULT_AWE_GREY_MATERIAL );

	CalcCurrentDigits();
	CalcTotalDigits();
	CalcProgressBarQuads();

	m_iAttachmentApertureLogo = -1;

	for ( i = 0; i != NUM_HAZARD_ICONS; ++i )
		m_iAttachmentHazardIcons[i] = -1;

	for ( i = 0; i != NUM_SEPARATOR_BARS; ++i )
		m_iAttachmentSeparatorBars[i] = -1;

	m_iAttachmentProgressBar = -1;
	
	for ( i = 0; i != NUM_DIGITS; ++i )
	{
		m_iAttachmentBigDigits[i] = -1;
		m_iAttachmentProgressDigits[i] = -1;
		m_iAttachmentTotalDigits[i] = -1;
	}

	m_iAttachmentTotalChambersLegacy = -1;
	m_iAttachmentTotalSlash = -1;
	
	InitializeMaterials();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::OnDataChanged( DataUpdateType_t updatetype )
{
	if ( m_iOldChamberNumber != m_iChamberNumber ||
		m_iOldTotalChambers != m_iTotalChambers )
	{
		if ( m_iOldChamberNumber != m_iChamberNumber )
		{
			CalcCurrentDigits();
		}

		if ( m_iOldTotalChambers != m_iTotalChambers )
		{
			CalcTotalDigits();
		}

		CalcProgressBarQuads();
	}

	BaseClass::OnDataChanged( updatetype );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::OnPreDataChanged( DataUpdateType_t updatetype )
{
	m_iOldChamberNumber = m_iChamberNumber;
	m_iOldTotalChambers = m_iTotalChambers;
	BaseClass::OnPreDataChanged( updatetype );
}

//-----------------------------------------------------------------------------
// Purpose: Parse the model data
//-----------------------------------------------------------------------------
CStudioHdr *C_PropTestchamberSign::OnNewModel()
{
	CStudioHdr *hdr = BaseClass::OnNewModel();
	int i;

	m_iAttachmentApertureLogo = LookupAttachment( "aperture_logo" );

	for ( i = 0; i != NUM_HAZARD_ICONS; ++i )
	{
		m_iAttachmentHazardIcons[i] = LookupAttachment( VarArgs( "hazard_icon_%i", i+1 ) );
	}

	for ( i = 0; i != NUM_SEPARATOR_BARS; ++i )
	{
		m_iAttachmentSeparatorBars[i] = LookupAttachment( VarArgs( "separator_bar_%i", i+1 ) );
	}

	m_iAttachmentProgressBar = LookupAttachment( "progress_bar" );
	
	for ( i = 0; i != NUM_DIGITS; ++i )
	{
		m_iAttachmentBigDigits[i] = LookupAttachment( VarArgs( "big_digit_%i", i+1 ) );
		m_iAttachmentProgressDigits[i] = LookupAttachment( VarArgs( "progress_digit_%i", i+1 ) );
		m_iAttachmentTotalDigits[i] = LookupAttachment( VarArgs( "total_digit_%i", i+1 ) );
	}

	m_iAttachmentTotalChambersLegacy = LookupAttachment( "total_chambers_legacy" );

	m_iAttachmentTotalSlash = LookupAttachment( "total_slash" );

	return hdr;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the model and the AWE
//-----------------------------------------------------------------------------
int C_PropTestchamberSign::DrawModel( int flags )
{
	int ret = BaseClass::DrawModel( flags );
	if ( !ret )
		return 0;

	DrawAWE();

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the number of progress bar quads
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::CalcProgressBarQuads()
{
	if ( m_iTotalChambers == 0 )
	{
		m_iProgressBarQuads = MAX_PROGRESS_BAR_QUADS;
		return;
	}

	if ( m_iChamberNumber >= m_iTotalChambers )
	{
		m_iProgressBarQuads = MAX_PROGRESS_BAR_QUADS;
		return;
	}

	m_iProgressBarQuads = ((float)m_iChamberNumber / (float)m_iTotalChambers) * MAX_PROGRESS_BAR_QUADS;
	m_iProgressBarQuads = MIN( MAX_PROGRESS_BAR_QUADS, m_iProgressBarQuads );
}

//-----------------------------------------------------------------------------
// Purpose: Get the digits for the current test chamber
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::CalcCurrentDigits()
{
	int digit_1 = (int)((float)m_iChamberNumber / 10.0);
	int digit_2 = m_iChamberNumber - (digit_1 * 10);

	m_iCurrentDigits[0] = digit_1;
	m_iCurrentDigits[1] = digit_2;
}

//-----------------------------------------------------------------------------
// Purpose: Get the digits for the total amount of test chambers
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::CalcTotalDigits()
{
	int digit_1 = (int)((float)m_iTotalChambers / 10.0);
	int digit_2 = m_iTotalChambers - (digit_1 * 10);

	m_iTotalDigits[0] = digit_1;
	m_iTotalDigits[1] = digit_2;
}

void C_PropTestchamberSign::InitializeMaterials()
{
	m_pMaterialAWETotal = materials->FindMaterial( m_iszAWEMaterial, TEXTURE_GROUP_MODEL );
	m_pMaterialAWETotalGrey = materials->FindMaterial( m_iszAWEGreyMaterial, TEXTURE_GROUP_MODEL );
	m_pMaterialAWEBlank = materials->FindMaterial( "models/props_animsigns/awe_blank", TEXTURE_GROUP_MODEL );
}

IMaterial *C_PropTestchamberSign::GetAWEMaterialForSkinSet( int skinset )
{
	switch ( GetSkin() )
	{
		case TESTSIGN_SKIN_UNUSED:
		case TESTSIGN_SKIN_INACTIVE:
		case TESTSIGN_SKIN_STARTUP_STAGE1:
		{
			return m_pMaterialAWEBlank;
		}
	}
			

	switch ( skinset )
	{
		case TESTSIGN_SKINSET_AWE_TOTAL:
		{
			switch ( GetSkin() )
			{
				case TESTSIGN_SKIN_STARTUP_STAGE2:
					return m_pMaterialAWETotalGrey;
				case TESTSIGN_SKIN_STARTUP_STAGE3:
				case TESTSIGN_SKIN_STARTUP_STAGE4:
				case TESTSIGN_SKIN_ACTIVE:
					return m_pMaterialAWETotal;
			}
			break;
		}
		
		case TESTSIGN_SKINSET_AWE_TOTAL_GREY:
		{
			return m_pMaterialAWETotalGrey;
		}
		
		case TESTSIGN_SKINSET_NEWSIGNAGE_BACK02:
		{
			return m_pMaterialAWETotal;
		}
		
		case TESTSIGN_SKINSET_NEWSIGNAGE_BACK01:
		{
			switch ( GetSkin() )
			{
				case TESTSIGN_SKIN_ACTIVE:
					return m_pMaterialAWETotal;
			}
			break;
		}
		
		case TESTSIGN_SKINSET_AWE_BLANK:
		{
			return m_pMaterialAWETotal;
		}
	}

	return m_pMaterialAWEBlank;
}

// For testing the UVs
//ConVar cl_testchamber_sign_digit( "cl_testchamber_sign_digit", "-1", 0, "", true, -1, true, 9 );

//-----------------------------------------------------------------------------
// Purpose: Draw all of the AWE icons
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawAWE()
{
	unsigned int i;

	Vector temp;
	CMatRenderContextPtr pRenderContext( materials );
	CMeshBuilder meshBuilder;

	// We could use the angles of the attachment and get the vectors that way, but that would be quite expensive
	// Doing it once (assuming every icon is facing the same direction) should work just fine.
	Vector right, up;
	GetVectors( NULL, &right, &up );

	// Aperture Logo
	DrawApertureLogo( pRenderContext, meshBuilder, right, up );

	// Hazard Icons
	for ( i = 0; i != NUM_HAZARD_ICONS; ++i )
	{
		GetAttachment( m_iAttachmentHazardIcons[i], temp );
		DrawHazardIcon( pRenderContext, meshBuilder, right, up, temp, i );
	}

	// Separator Bars
	for ( i = 0; i != NUM_SEPARATOR_BARS; ++i )
	{
		GetAttachment( m_iAttachmentSeparatorBars[i], temp );
		DrawSeparatorBar( pRenderContext, meshBuilder, right, up, temp );
	}

	// Progress bar
	GetAttachment( m_iAttachmentProgressBar, temp );
	for ( i = 0; i != m_iProgressBarQuads; ++i )
	{
		DrawProgressBarQuad( pRenderContext, meshBuilder, right, up, temp );
		temp += -right * AWE_PROGRESS_BAR_WIDTH;
	}

	// Digits (big and large)
	for ( i = 0; i != NUM_DIGITS; ++i )
	{
		//if ( cl_testchamber_sign_digit.GetInt() != -1 )
		//	m_iCurrentDigits[i] = cl_testchamber_sign_digit.GetInt();

		GetAttachment( m_iAttachmentBigDigits[i], temp );
		DrawBigDigit( pRenderContext, meshBuilder, right, up, temp, m_iCurrentDigits[i] );
		
		GetAttachment( m_iAttachmentProgressDigits[i], temp );
		DrawSmallDigit( pRenderContext, meshBuilder, right, up, temp, m_iCurrentDigits[i] );
	}

	DrawTotalChambers( pRenderContext, meshBuilder, right, up );
}

//-----------------------------------------------------------------------------
// Purpose: Draws the Aperture logo on the bottom of the screen
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawApertureLogo( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up )
{
	const float x1 = 0.925;
	const float x2 = 1.0;

	const float y1 = 1.0;
	const float y2 = 0.415;

	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_NEWSIGNAGE_BACK02 ) );

	Vector center, temp;
	GetAttachment( m_iAttachmentApertureLogo, center );
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	// Top Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = center;
	temp += up * AWE_APERTURE_LOGO_HALF_HEIGHT;
	temp += right * AWE_APERTURE_LOGO_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y2);
	temp = center;
	temp += up * AWE_APERTURE_LOGO_HALF_HEIGHT;
	temp += right * -AWE_APERTURE_LOGO_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = center;
	temp += up * -AWE_APERTURE_LOGO_HALF_HEIGHT;
	temp += right * -AWE_APERTURE_LOGO_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = center;
	temp += up * -AWE_APERTURE_LOGO_HALF_HEIGHT;
	temp += right * AWE_APERTURE_LOGO_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw a hazard icon at a specified location
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawHazardIcon( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center, int iconIndex )
{
	pRenderContext->Bind( GetAWEMaterialForSkinSet( m_bIconActive[iconIndex] ? TESTSIGN_SKINSET_AWE_TOTAL : TESTSIGN_SKINSET_AWE_TOTAL_GREY ) );

	Vector temp;
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	// Top Left
	static const Vector2D s_HazardUVs1[] =
	{
		Vector2D( 0.000195, 0.0 ),
		Vector2D( 0.166862, 0.0 ),
		Vector2D( 0.333529, 0.0 ),
		Vector2D( 0.000195, 0.333333 ),
		Vector2D( 0.166862, 0.333333 ),
		Vector2D( 0.333529, 0.333333 ),
		Vector2D( 0.000195, 0.666667 ),
		Vector2D( 0.166862, 0.666667 ),
		Vector2D( 0.333529, 0.666667 ),
		Vector2D( 0.5, 0.00178 ),
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs1[iconIndex].x, s_HazardUVs1[iconIndex].y);
	temp = center;
	temp += up * AWE_HAZARD_ICON_HALF_HEIGHT;
	temp += right * AWE_HAZARD_ICON_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	static const Vector2D s_HazardUVs2[] =
	{
		Vector2D( 0.166862, 0.0 ),
		Vector2D( 0.333529, 0.0 ),
		Vector2D( 0.500196, 0.0 ),
		Vector2D( 0.166862, 0.333333 ),
		Vector2D( 0.333529, 0.333333 ),
		Vector2D( 0.500196, 0.333333 ),
		Vector2D( 0.166862, 0.666667 ),
		Vector2D( 0.333529, 0.666667 ),
		Vector2D( 0.500196, 0.666667 ),
		Vector2D( 0.666796, 0.00178 ),
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs2[iconIndex].x, s_HazardUVs2[iconIndex].y);
	temp = center;
	temp += up * AWE_HAZARD_ICON_HALF_HEIGHT;
	temp += right * -AWE_HAZARD_ICON_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	// Bottom Right
	static const Vector2D s_HazardUVs3[] =
	{
		Vector2D( 0.166862, 0.333333 ),
		Vector2D( 0.333529, 0.333333 ),
		Vector2D( 0.500196, 0.333333 ),
		Vector2D( 0.166862, 0.666667 ),
		Vector2D( 0.333529, 0.666667 ),
		Vector2D( 0.500196, 0.666667 ),
		Vector2D( 0.166862, 1.0 ),
		Vector2D( 0.333529, 1.0 ),
		Vector2D( 0.500196, 1.0 ),
		Vector2D( 0.666796, 0.334145 ),
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs3[iconIndex].x, s_HazardUVs3[iconIndex].y);
	temp = center;
	temp += up * -AWE_HAZARD_ICON_HALF_HEIGHT;
	temp += right * -AWE_HAZARD_ICON_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	static const Vector2D s_HazardUVs4[] =
	{
		Vector2D( 0.000195, 0.333333 ),
		Vector2D( 0.166862, 0.333333 ),
		Vector2D( 0.333529, 0.333333 ),
		Vector2D( 0.000195, 0.666667 ),
		Vector2D( 0.166862, 0.666667 ),
		Vector2D( 0.333529, 0.666667 ),
		Vector2D( 0.000195, 1.0 ),
		Vector2D( 0.166862, 1.0 ),
		Vector2D( 0.333529, 1.0 ),
		Vector2D( 0.499985, 0.334145 ),
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs4[iconIndex].x, s_HazardUVs4[iconIndex].y);
	temp = center;
	temp += up * -AWE_HAZARD_ICON_HALF_HEIGHT;
	temp += right * AWE_HAZARD_ICON_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw a full separator bar
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawSeparatorBar( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start )
{
	const int NUM_SEPARATOR_CENTER_BARS = 6;

	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_AWE_BLANK ) );

	DrawSeparatorBarEdge( pRenderContext, meshBuilder, right, up, start, false );
	Vector adjustedStart = start;
	for ( int i = 0; i != NUM_SEPARATOR_CENTER_BARS; ++i )
	{
		adjustedStart += -right * AWE_SEPARATOR_BAR_WIDTH;
		DrawSeparatorBarCenter( pRenderContext, meshBuilder, right, up, adjustedStart );
	}

	adjustedStart += -right * AWE_SEPARATOR_BAR_WIDTH; // Add it one more time
	DrawSeparatorBarEdge( pRenderContext, meshBuilder, right, up, adjustedStart, true );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the edges of the bars
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawSeparatorBarEdge( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start, bool bSwapUVs )
{
	const float x1 = 0.789853;
	const float x2 = 0.798437;

	const float y1 = bSwapUVs ? 0.075581 : 0.005251;
	const float y2 = bSwapUVs ? 0.005251 : 0.075581;

	Vector temp;
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	// Top Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = start;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = start;
	temp += right * -AWE_SEPARATOR_BAR_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y2);
	temp = start;
	temp += up * -AWE_SEPARATOR_BAR_HEIGHT;
	temp += right * -AWE_SEPARATOR_BAR_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = start;
	temp += up * -AWE_SEPARATOR_BAR_HEIGHT;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw the center of the bar
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawSeparatorBarCenter( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start )
{
	const float x1 = 0.798437;
	const float x2 = 0.789853;

	const float y1 = 0.067145;
	const float y2 = 0.075581;

	Vector temp;
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	const Vector extents(0.25, 0.25, 0.25);

	// Top Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = start;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y2);
	temp = start;
	temp += right * -AWE_SEPARATOR_BAR_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = start;
	temp += up * -AWE_SEPARATOR_BAR_HEIGHT;
	temp += right * -AWE_SEPARATOR_BAR_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = start;
	temp += up * -AWE_SEPARATOR_BAR_HEIGHT;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draws a progress bar quad
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawProgressBarQuad( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &start )
{
	const float x1 = 0.79089;
	const float x2 = 0.821924;

	const float y1 = 0.002391;
	const float y2 = 0.144201;
	
	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_NEWSIGNAGE_BACK01 ) );

	Vector temp;
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	const Vector extents(0.25, 0.25, 0.25);

	// Top Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = start;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = start;
	temp += right * -AWE_PROGRESS_BAR_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = start;
	temp += up * -AWE_PROGRESS_BAR_HEIGHT;
	temp += right * -AWE_PROGRESS_BAR_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y2);
	temp = start;
	temp += up * -AWE_PROGRESS_BAR_HEIGHT;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw a large digit for the chamber number
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawBigDigit( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center, int digit )
{
	switch ( digit ) // Specially shaped digits
	{
		case 1:
		DrawBigDigit_Number1( pRenderContext, meshBuilder, right, up, center );
		return;
		
		case 9:
		DrawBigDigit_Number9( pRenderContext, meshBuilder, right, up, center );
		return;
	}
	
	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_NEWSIGNAGE_BACK02 ) );

	Vector temp;
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	// Top Left
	static const Vector2D s_HazardUVs1[] =
	{
		Vector2D( 0.520528, 0.569525 ),	// 0
		Vector2D( 0.714507, 0.15196 ),	// 1
		Vector2D( 0.783765, 0.15196 ),	// 2
		Vector2D( 0.855164, 0.15196 ),	// 3
		Vector2D( 0.501083, 0.576876 ),	// 4
		Vector2D( 0.573429, 0.576876 ),	// 5
		Vector2D( 0.64216, 0.576876 ),	// 6
		Vector2D( 0.713603, 0.576876 ),	// 7
		Vector2D( 0.783237, 0.576876 ),	// 8
		Vector2D( 0.854681, 0.576876 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs1[digit].x, s_HazardUVs1[digit].y);
	temp = center;
	temp += up * AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	static const Vector2D s_HazardUVs2[] =
	{
		Vector2D( 0.520528, 0.424829 ),	// 0
		Vector2D( 0.786855, 0.15196 ),	// 1
		Vector2D( 0.856113, 0.15196 ),	// 2
		Vector2D( 0.927512, 0.15196 ),	// 3
		Vector2D( 0.573431, 0.576876 ),	// 4
		Vector2D( 0.645777, 0.576876 ),	// 5
		Vector2D( 0.714508, 0.576876 ),	// 6
		Vector2D( 0.785951, 0.576876 ),	// 7
		Vector2D( 0.855585, 0.576876 ),	// 8
		Vector2D( 0.923775, 0.576876 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs2[digit].x, s_HazardUVs2[digit].y);
	temp = center;
	temp += up * AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	// Bottom Right
	static const Vector2D s_HazardUVs3[] =
	{
		Vector2D( 0.72827, 0.424829 ),	// 0
		Vector2D( 0.786855, 0.567444 ),	// 1
		Vector2D( 0.856113, 0.567444 ),	// 2
		Vector2D( 0.927512, 0.567444 ),	// 3
		Vector2D( 0.573431, 0.992359 ),	// 4
		Vector2D( 0.645777, 0.992359 ),	// 5
		Vector2D( 0.714508, 0.992359 ),	// 6
		Vector2D( 0.785951, 0.992359 ),	// 7
		Vector2D( 0.855585, 0.992359 ),	// 8
		Vector2D( 0.923775, 0.992359 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs3[digit].x, s_HazardUVs3[digit].y);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	static const Vector2D s_HazardUVs4[] =
	{
		Vector2D( 0.72827, 0.569525 ),	// 0
		Vector2D( 0.714507, 0.567444 ),	// 1
		Vector2D( 0.783765, 0.567444 ),	// 2
		Vector2D( 0.855164, 0.567444 ),	// 3
		Vector2D( 0.501083, 0.992359 ),	// 4
		Vector2D( 0.573429, 0.992359 ),	// 5
		Vector2D( 0.64216, 0.992359 ),	// 6
		Vector2D( 0.713603, 0.992359 ),	// 7
		Vector2D( 0.783237, 0.992359 ),	// 8
		Vector2D( 0.854681, 0.992359 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs4[digit].x, s_HazardUVs4[digit].y);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw a large 1 for the chamber number (uses special geo for the UV)
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawBigDigit_Number1( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center )
{
	// For the vertex near the center
	const float AWE_BIG_DIGIT_ONE_SPECIAL_WIDTH = 5.3474;
	const float AWE_BIG_DIGIT_ONE_SPECIAL_HEIGHT = 9.9837;

	const float x1 = 0.714507;
	const float x2 = 0.786855;
	const float x3 = 0.73442;

	const float y1 = 0.15196;
	const float y2 = 0.567444;
	const float y3 = 0.420078;
	
	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_NEWSIGNAGE_BACK02 ) );

	Vector temp;
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 3 );

	// Top triangle 
	//

	// Top Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = center;
	temp += up * AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = center;
	temp += up * AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Center-ish
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x3, y3);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_ONE_SPECIAL_HEIGHT;
	temp += right * AWE_BIG_DIGIT_ONE_SPECIAL_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Right triangle 
	//

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = center;
	temp += up * AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Center-ish
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x3, y3);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_ONE_SPECIAL_HEIGHT;
	temp += right * AWE_BIG_DIGIT_ONE_SPECIAL_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom triangle 
	//
	
	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_BIG_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	// Bottom center
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x3, y2);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * AWE_BIG_DIGIT_ONE_SPECIAL_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Center-ish
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x3, y3);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_ONE_SPECIAL_HEIGHT;
	temp += right * AWE_BIG_DIGIT_ONE_SPECIAL_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw a large 9 which is slightly smaller than the rest
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawBigDigit_Number9( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &baseCenter )
{
	// The half width for the 9 is 11.412935 and the difference is 0.48328
	const float HALF_WIDTH_PERCENTAGE_DIFF = 0.959375314;
	const float HALF_WIDTH = AWE_BIG_DIGIT_HALF_WIDTH * HALF_WIDTH_PERCENTAGE_DIFF;

	// TODO: It'd be way more elegant to make this multiply to get the desired result,
	// but it's giving inaccurate results and won't work for both sides
	
	// The offset difference is 0.4832
	//const float CENTER_X_OFFSET_PERCENTAGE = 0.965887029;
	//Vector center = baseCenter + (right * CENTER_X_OFFSET_PERCENTAGE);

	const float CENTER_X_OFFSET = 0.4832;
	Vector center = baseCenter + (right * CENTER_X_OFFSET);

	const float x1 = 0.854681;
	const float x2 = 0.924089;

	const float y1 = 0.576876;
	const float y2 = 0.992359;

	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_NEWSIGNAGE_BACK02 ) );

	Vector temp;
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	// Top Left
	//
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = center;
	temp += up * AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	//
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = center;
	temp += up * AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	// Bottom Right
	//
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * -HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	//
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y2);
	temp = center;
	temp += up * -AWE_BIG_DIGIT_HALF_HEIGHT;
	temp += right * HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw a small digit
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawSmallDigit( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center, int digit )
{
	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_NEWSIGNAGE_BACK02 ) );

	Vector temp;
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	// Top Left
	static const Vector2D s_HazardUVs1[] =
	{
		Vector2D( 0.938167, 0.001115 ),	// 0
		Vector2D( 0.967302, 0.001115 ),	// 1
		Vector2D( 0.937673, 0.08328 ),	// 2
		Vector2D( 0.966555, 0.08328 ),	// 3
		Vector2D( 0.937673, 0.165605 ),	// 4
		Vector2D( 0.966553, 0.165605 ),	// 5
		Vector2D( 0.936498, 0.247742 ),	// 6
		Vector2D( 0.967003, 0.247742 ),	// 7
		Vector2D( 0.934481, 0.331201 ),	// 8
		Vector2D( 0.967611, 0.331201 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs1[digit].x, s_HazardUVs1[digit].y);
	temp = center;
	temp += up * AWE_SMALL_DIGIT_HALF_HEIGHT;
	temp += right * AWE_SMALL_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	static const Vector2D s_HazardUVs2[] =
	{
		Vector2D( 0.970272, 0.001115 ),	// 0
		Vector2D( 0.999407, 0.001115 ),	// 1
		Vector2D( 0.969778, 0.08328 ),	// 2
		Vector2D( 0.99866, 0.08328 ),	// 3
		Vector2D( 0.969778, 0.165605 ),	// 4
		Vector2D( 0.998658, 0.165605 ),	// 5
		Vector2D( 0.968603, 0.247742 ),	// 6
		Vector2D( 0.997473, 0.247742 ),	// 7
		Vector2D( 0.966586, 0.331201 ),	// 8
		Vector2D( 0.998385, 0.331201 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs2[digit].x, s_HazardUVs2[digit].y);
	temp = center;
	temp += up * AWE_SMALL_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_SMALL_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	// Bottom Right
	static const Vector2D s_HazardUVs3[] =
	{
		Vector2D( 0.970272, 0.086139 ),	// 0
		Vector2D( 0.999407, 0.086139 ),	// 1
		Vector2D( 0.969778, 0.168304 ),	// 2
		Vector2D( 0.99866, 0.168304 ),	// 3
		Vector2D( 0.969778, 0.250628 ),	// 4
		Vector2D( 0.998658, 0.250628 ),	// 5
		Vector2D( 0.968603, 0.332765 ),	// 6
		Vector2D( 0.997473, 0.332765 ),	// 7
		Vector2D( 0.966586, 0.416224 ),	// 8
		Vector2D( 0.998385, 0.416224 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs3[digit].x, s_HazardUVs3[digit].y);
	temp = center;
	temp += up * -AWE_SMALL_DIGIT_HALF_HEIGHT;
	temp += right * -AWE_SMALL_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	static const Vector2D s_HazardUVs4[] =
	{
		Vector2D( 0.938167, 0.086139 ),	// 0
		Vector2D( 0.967302, 0.086139 ),	// 1
		Vector2D( 0.937673, 0.168304 ),	// 2
		Vector2D( 0.966555, 0.168304 ),	// 3
		Vector2D( 0.937673, 0.250628 ),	// 4
		Vector2D( 0.966553, 0.250628 ),	// 5
		Vector2D( 0.936498, 0.332765 ),	// 6
		Vector2D( 0.967003, 0.332765 ),	// 7
		Vector2D( 0.934481, 0.416224 ),	// 8
		Vector2D( 0.967611, 0.416224 ),	// 9
	};

	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, s_HazardUVs4[digit].x, s_HazardUVs4[digit].y);
	temp = center;
	temp += up * -AWE_SMALL_DIGIT_HALF_HEIGHT;
	temp += right * AWE_SMALL_DIGIT_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draw a slash...
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawSlash( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up, const Vector &center )
{
	const float x1 = 0.674963;
	const float x2 = 0.693362;

	const float y1 = 0.004663;
	const float y2 = 0.089784;

	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_AWE_TOTAL ) );

	Vector temp;
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	// Top Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = center;
	temp += up * AWE_TOTAL_SLASH_HALF_HEIGHT;
	temp += right * AWE_TOTAL_SLASH_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = center;
	temp += up * AWE_TOTAL_SLASH_HALF_HEIGHT;
	temp += right * -AWE_TOTAL_SLASH_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = center;
	temp += up * -AWE_TOTAL_SLASH_HALF_HEIGHT;
	temp += right * -AWE_TOTAL_SLASH_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y2);
	temp = center;
	temp += up * -AWE_TOTAL_SLASH_HALF_HEIGHT;
	temp += right * AWE_TOTAL_SLASH_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Display the total amount of chambers
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawTotalChambers( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up )
{
	if ( m_bLegacyTotalChambers )
	{
		DrawTotalChambersLegacy( pRenderContext, meshBuilder, right, up );
		return;
	}

	// Draw the slash
	Vector temp;
	GetAttachment( m_iAttachmentTotalSlash, temp );
	DrawSlash( pRenderContext, meshBuilder, right, up, temp );

	for ( int i = 0; i != NUM_DIGITS; ++i )
	{
		GetAttachment( m_iAttachmentTotalDigits[i], temp );
		DrawSmallDigit( pRenderContext, meshBuilder, right, up, temp, m_iTotalDigits[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use the old method for displaying total chambers
//-----------------------------------------------------------------------------
void C_PropTestchamberSign::DrawTotalChambersLegacy( IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, const Vector &right, const Vector &up )
{
	const float x1 = 0.674963;
	const float x2 = 0.74856;

	const float y1 = 0.004663;
	const float y2 = 0.089784;

	pRenderContext->Bind( GetAWEMaterialForSkinSet( TESTSIGN_SKINSET_AWE_TOTAL ) );

	Vector center, temp;
	GetAttachment( m_iAttachmentTotalChambersLegacy, center );
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	// Top Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y1);
	temp = center;
	temp += up * AWE_TOTAL_CHAMBERS_LEGACY_HALF_HEIGHT;
	temp += right * AWE_TOTAL_CHAMBERS_LEGACY_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Top Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y1);
	temp = center;
	temp += up * AWE_TOTAL_CHAMBERS_LEGACY_HALF_HEIGHT;
	temp += right * -AWE_TOTAL_CHAMBERS_LEGACY_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Right
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x2, y2);
	temp = center;
	temp += up * -AWE_TOTAL_CHAMBERS_LEGACY_HALF_HEIGHT;
	temp += right * -AWE_TOTAL_CHAMBERS_LEGACY_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();

	// Bottom Left
	meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
	meshBuilder.TexCoord2f (0.0, x1, y2);
	temp = center;
	temp += up * -AWE_TOTAL_CHAMBERS_LEGACY_HALF_HEIGHT;
	temp += right * AWE_TOTAL_CHAMBERS_LEGACY_HALF_WIDTH;
	meshBuilder.Position3fv (temp.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}