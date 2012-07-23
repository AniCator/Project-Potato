#include "cbase.h"

class CClubDJ : public CBaseEntity
{
public:
	DECLARE_CLASS(CClubDJ, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
 
	int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS ); //always send shit.. i guess
	}

	CClubDJ();
	void ForcePlay(inputdata_t &inputData);
 
	//testvars
	CNetworkVar( bool, bDJEnabled );
};
 
LINK_ENTITY_TO_CLASS( club_dj, CClubDJ );
 
IMPLEMENT_SERVERCLASS_ST( CClubDJ, DT_ClubDJ )
	SendPropInt(SENDINFO(bDJEnabled), 0, SPROP_UNSIGNED ),
END_SEND_TABLE()

BEGIN_DATADESC( CClubDJ )
	DEFINE_INPUTFUNC( FIELD_VOID, "ForcePlay", ForcePlay ),
END_DATADESC()

CClubDJ::CClubDJ(){
	bDJEnabled = false;
}

void CClubDJ::ForcePlay(inputdata_t &inputData){
	//put stuff here
	Msg("executing forceplay lol\n");
	if(bDJEnabled){
		bDJEnabled=false;
	}
	else{
		bDJEnabled=true;
	}
	NetworkStateChanged();
}