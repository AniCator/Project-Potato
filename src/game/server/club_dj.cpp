#include "cbase.h"
#include "deferred\CDefLight.h"
#include "bass.h"

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

	void Spawn();
	void Think();

	BOOL bassInit;
	HSTREAM serverStream1;
	HSTREAM serverStream2;

	EHANDLE lightMain;
	EHANDLE lightBass;
	EHANDLE lightHigh;
 
	//testvars
	CNetworkVar( bool, bDJEnabled );
	CNetworkVar( float, bDJStream1Pos);
};
 
LINK_ENTITY_TO_CLASS( club_dj, CClubDJ );
 
IMPLEMENT_SERVERCLASS_ST( CClubDJ, DT_ClubDJ )
	SendPropInt(SENDINFO(bDJEnabled), 0, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO(bDJStream1Pos), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

BEGIN_DATADESC( CClubDJ )
	DEFINE_INPUTFUNC( FIELD_VOID, "ForcePlay", ForcePlay ),

	DEFINE_KEYFIELD( lightMain, FIELD_EHANDLE, "lightMain" ),
	DEFINE_KEYFIELD( lightBass, FIELD_EHANDLE, "lightBass" ),
	DEFINE_KEYFIELD( lightHigh, FIELD_EHANDLE, "lightHigh" ),
END_DATADESC()

CClubDJ::CClubDJ(){
	bDJEnabled = false;

	bassInit = BASS_Init(-1, 44100, BASS_DEVICE_3D, 0, NULL);
	if(!bassInit)
	{
		int error = BASS_ErrorGetCode();
		if(error==BASS_ERROR_ALREADY){
			Msg("BASS: Probably running listen server. Bass is already running and doesn't have to be re-initialized.\n");
			bassInit=true;
		}
		else{
			Msg("BASS Init failed, error code %d\n", error);
			Error("BASS Init failed, error code %d\n", error);
		}
	}
	else{
		Msg("BASS module has been initialized...\n");
	}
}

void CClubDJ::Spawn(){
	BaseClass::Spawn();
	SetNextThink( gpGlobals->curtime);
}

void CClubDJ::ForcePlay(inputdata_t &inputData){
	//put stuff here
	Msg("executing forceplay lol\n");
	if(bDJEnabled){
		bDJEnabled=false;
		if(bassInit){
			BASS_ChannelStop(serverStream1);
			Msg("aaaaaaaand stream's stopped.\n");
		}
		else{
			Msg("CoopCrowd Club's DJ is experiencing brain thingies!\n");
		}
	}
	else{
		bDJEnabled=true;
		if(bassInit){
			if(serverStream1==NULL){
				//Create new stream
				serverStream1=BASS_StreamCreateURL("http://anicator.com/gallery/music/portalRedux.mp3", 0, 0, NULL, 0);
			}
			//Play stream
			BASS_ChannelPlay(serverStream1,true);
			BASS_ChannelSetAttribute(serverStream1,BASS_ATTRIB_VOL,0);
			Msg("CoopCrowd Club is Live!\n");
		}
		else{
			Msg("CoopCrowd Club's DJ is experiencing brain thingies!\n");
		}
	}
	NetworkStateChanged();
}

void CClubDJ::Think(){
	BaseClass::Think();
	
	if(serverStream1!=NULL){
		float fft[512]; // fft data buffer
		BASS_ChannelGetData(serverStream1, fft, BASS_DATA_FFT1024);
		if(lightMain!=NULL){
			CDeferredLight* lightMain_ = dynamic_cast<CDeferredLight*>( lightMain.Get() );
			delete lightMain_;
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}