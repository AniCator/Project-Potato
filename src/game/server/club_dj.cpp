#include "cbase.h"
#include "deferred\CDefLight.h"
#include "deferred\deferred_shared_common.h"
#include "bass.h"
#include <string>
#include <sstream>

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

	CDeferredLight *lightMain;
	CDeferredLight *lightBass;
	CDeferredLight *lightHigh;
	CDeferredLight *lightGreen;
	CDeferredLight *lightYellow;

	float oldMain;
	float oldBass;
	float oldHigh;
 
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

	//DEFINE_KEYFIELD( lightMain, FIELD_EHANDLE, "lightMain" ),
	//DEFINE_KEYFIELD( lightBass, FIELD_EHANDLE, "lightBass" ),
	//DEFINE_KEYFIELD( lightHigh, FIELD_EHANDLE, "lightHigh" ),
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
		BASS_SetVolume(BASS_GetVolume());
	}
}

void CClubDJ::Spawn(){
	BaseClass::Spawn();

	lightMain = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light1"));
	lightBass = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light2"));
	lightHigh = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light3"));
	lightGreen = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light4"));
	lightYellow = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light5"));

	if(lightMain!=NULL){
		Msg("Found Main Light for club_dj.\n");
	}
	else{
		Warning("Could not find Main Light for club_dj!");
	}

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
				serverStream1=BASS_StreamCreateURL("http://iku.streams.bassdrive.com:8000", 0, 0, NULL, 0);
			}
			//Play stream
			BASS_ChannelPlay(serverStream1,true);
			BASS_ChannelSetAttribute(serverStream1,BASS_ATTRIB_VOL,0.0f);
			Msg("CoopCrowd Club is Live!\n");
		}
		else{
			Msg("CoopCrowd Club's DJ is experiencing brain thingies!\n");
		}
	}
	NetworkStateChanged();
}

float FFTAverage(float fft[],int index,int range){
	int low = index-(range/2);
	int high = index+(range/2);

	float sum = 0;
	int count = 0;
	for(int i = low;i<high;i++){
		sum+=fft[i];
		count++;
	}
	return sum/count;
}

void CClubDJ::Think(){
	BaseClass::Think();
	
	if(serverStream1!=NULL){
		float fft[512]; // fft data buffer
		BASS_ChannelGetData(serverStream1, fft, BASS_DATA_FFT1024);
		if(lightMain!=NULL){
			std::string diff = "255 0 0 ";
			std::stringstream ss;
			ss<<FFTAverage(fft,25,10)*10000;
			diff.append(ss.str());
			lightMain->SetColor_Diffuse(stringColToVec(diff.c_str()));
		}
		if(lightBass!=NULL){
			std::string diff = "0 0 255 ";
			std::stringstream ss;
			ss<<FFTAverage(fft,5,10)*5000;
			diff.append(ss.str());
			lightBass->SetColor_Diffuse(stringColToVec(diff.c_str()));
		}
		if(lightHigh!=NULL){
			std::string diff = "255 255 255 ";
			std::stringstream ss;
			ss<<FFTAverage(fft,100,10)*200000;
			diff.append(ss.str());
			lightHigh->SetColor_Diffuse(stringColToVec(diff.c_str()));
		}
		if(lightGreen!=NULL){
			std::string diff = "0 255 0 ";
			std::stringstream ss;
			ss<<FFTAverage(fft,300,10)*200000;
			diff.append(ss.str());
			lightGreen->SetColor_Diffuse(stringColToVec(diff.c_str()));
		}
		if(lightYellow!=NULL){
			std::string diff = "255 255 0 ";
			std::stringstream ss;
			ss<<FFTAverage(fft,400,10)*200000;
			diff.append(ss.str());
			lightYellow->SetColor_Diffuse(stringColToVec(diff.c_str()));
		}
	}

	SetNextThink( gpGlobals->curtime + 0.05 );
}