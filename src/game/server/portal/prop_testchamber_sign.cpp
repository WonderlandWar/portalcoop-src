#include "cbase.h"
#include "prop_testchamber_sign_shared.h"
#include "props.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TESTCHAMBER_SIGN_MODEL "models/props_animsigns/signage_blank.mdl"

class CPropTestchamberSign : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropTestchamberSign, CDynamicProp );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPropTestchamberSign();
	virtual void Spawn();
	virtual void Activate();
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

private:

	void InputSetChamberNumber( inputdata_t &inputdata );
	void InputSetTotalChambers( inputdata_t &inputdata );
	void InputSetIcon1Active( inputdata_t &inputdata );
	void InputSetIcon2Active( inputdata_t &inputdata );
	void InputSetIcon3Active( inputdata_t &inputdata );
	void InputSetIcon4Active( inputdata_t &inputdata );
	void InputSetIcon5Active( inputdata_t &inputdata );
	void InputSetIcon6Active( inputdata_t &inputdata );
	void InputSetIcon7Active( inputdata_t &inputdata );
	void InputSetIcon8Active( inputdata_t &inputdata );
	void InputSetIcon9Active( inputdata_t &inputdata );
	void InputSetIcon10Active( inputdata_t &inputdata );

	CNetworkVar( unsigned int, m_iChamberNumber );
	CNetworkVar( unsigned int, m_iTotalChambers );
	CNetworkArray( bool, m_bIconActive, NUM_HAZARD_ICONS );
	CNetworkVar( bool, m_bLegacyTotalChambers );
	
	CNetworkVar( string_t, m_iszAWEMaterial );
	CNetworkVar( string_t, m_iszAWEGreyMaterial );
};

IMPLEMENT_SERVERCLASS_ST( CPropTestchamberSign, DT_PropTestchamberSign )
	SendPropInt( SENDINFO( m_iChamberNumber ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iTotalChambers ), 7, SPROP_UNSIGNED ),
	
	SendPropArray3( SENDINFO_ARRAY3(m_bIconActive), SendPropInt( SENDINFO_ARRAY(m_bIconActive), 1, SPROP_UNSIGNED ) ),
	SendPropBool( SENDINFO( m_bLegacyTotalChambers ) ),
	
	SendPropStringT( SENDINFO( m_iszAWEMaterial ) ),
	SendPropStringT( SENDINFO( m_iszAWEGreyMaterial ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CPropTestchamberSign )
	DEFINE_KEYFIELD( m_iChamberNumber, FIELD_INTEGER, "chamber_number" ),
	DEFINE_KEYFIELD( m_iTotalChambers, FIELD_INTEGER, "total_chambers" ),
	
	DEFINE_KEYFIELD( m_iszAWEMaterial, FIELD_STRING, "awe_material" ),
	DEFINE_KEYFIELD( m_iszAWEGreyMaterial, FIELD_STRING, "awe_grey_material" ),

	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetChamberNumber", InputSetChamberNumber ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetTotalChambers", InputSetTotalChambers ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon1Active", InputSetIcon1Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon2Active", InputSetIcon2Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon3Active", InputSetIcon3Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon4Active", InputSetIcon4Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon5Active", InputSetIcon5Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon6Active", InputSetIcon6Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon7Active", InputSetIcon7Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon8Active", InputSetIcon8Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon9Active", InputSetIcon9Active ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetIcon10Active", InputSetIcon10Active ),

	DEFINE_KEYFIELD( m_bLegacyTotalChambers, FIELD_BOOLEAN, "LegacyTotalChambers" ),
	DEFINE_KEYFIELD( m_bIconActive[0], FIELD_BOOLEAN, "icon1active" ),
	DEFINE_KEYFIELD( m_bIconActive[1], FIELD_BOOLEAN, "icon2active" ),
	DEFINE_KEYFIELD( m_bIconActive[2], FIELD_BOOLEAN, "icon3active" ),
	DEFINE_KEYFIELD( m_bIconActive[3], FIELD_BOOLEAN, "icon4active" ),
	DEFINE_KEYFIELD( m_bIconActive[4], FIELD_BOOLEAN, "icon5active" ),
	DEFINE_KEYFIELD( m_bIconActive[5], FIELD_BOOLEAN, "icon6active" ),
	DEFINE_KEYFIELD( m_bIconActive[6], FIELD_BOOLEAN, "icon7active" ),
	DEFINE_KEYFIELD( m_bIconActive[7], FIELD_BOOLEAN, "icon8active" ),
	DEFINE_KEYFIELD( m_bIconActive[8], FIELD_BOOLEAN, "icon9active" ),
	DEFINE_KEYFIELD( m_bIconActive[9], FIELD_BOOLEAN, "icon10active" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_testchamber_sign, CPropTestchamberSign );

CPropTestchamberSign::CPropTestchamberSign()
{
	m_iChamberNumber = 12;
	m_iTotalChambers = 19;

	for ( int i = 0; i < NUM_HAZARD_ICONS; ++i )
		m_bIconActive.Set( i, false );

	m_bLegacyTotalChambers = true;
}

void CPropTestchamberSign::Spawn()
{
	const char *pszModelName = GetModelName().ToCStr();
	if ( !*pszModelName )
	{
		SetModelName( AllocPooledString_StaticConstantStringPointer( TESTCHAMBER_SIGN_MODEL ) );
	}
	
	BaseClass::Spawn();
}

void CPropTestchamberSign::Activate()
{	
	m_iChamberNumber = MIN( MAX_CHAMBER_NUMBER, m_iChamberNumber );
	m_iTotalChambers = MIN( MAX_CHAMBER_NUMBER, m_iTotalChambers );

	BaseClass::Activate();
}

bool CPropTestchamberSign::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( !Q_stricmp( szValue, "chamber_number" ) )
	{
		m_iChamberNumber = MIN( MAX_CHAMBER_NUMBER, atoi( szValue ) );
		return true;
	}
	
	if ( !Q_stricmp( szValue, "total_chambers" ) )
	{
		m_iTotalChambers = MIN( MAX_CHAMBER_NUMBER, atoi( szValue ) );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

void CPropTestchamberSign::InputSetChamberNumber( inputdata_t &inputdata )
{
	m_iChamberNumber = inputdata.value.Int();
}

void CPropTestchamberSign::InputSetTotalChambers( inputdata_t &inputdata )
{
	m_iTotalChambers = inputdata.value.Int();
}

void CPropTestchamberSign::InputSetIcon1Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 0, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon2Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 1, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon3Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 2, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon4Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 3, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon5Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 4, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon6Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 5, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon7Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 6, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon8Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 7, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon9Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 8, inputdata.value.Bool() );
}

void CPropTestchamberSign::InputSetIcon10Active( inputdata_t &inputdata )
{
	m_bIconActive.Set( 9, inputdata.value.Bool() );
}