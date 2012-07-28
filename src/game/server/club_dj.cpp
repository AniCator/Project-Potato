#include "cbase.h"
#include "deferred\CDefLight.h"
#include "deferred\deferred_shared_common.h"
#include "mathlib\mathlib.h"
#include "bass.h"
#include <string>
#include <sstream>

ConVar club_url("club_url", "http://iku.streams.bassdrive.com:8000", FCVAR_REPLICATED, "Club - Playback URL (SHOUTcast or just regular *.mp3 and *.ogg files" );

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

	//TODO: Should be named Toggle
	//Enables/disables stream
	void ForcePlay(inputdata_t &inputData);

	void Spawn();
	void Think();

	BOOL bassInit;
	HSTREAM serverStream1;
	HSTREAM serverStream2;

	//Light pointers
	CDeferredLight *lightMain;
	CDeferredLight *lightBass;
	CDeferredLight *lightHigh;
	CDeferredLight *lightGreen;
	CDeferredLight *lightYellow;

	//Light strings (for keyfields)
	char *lightMainStr;

	//Old angles of lights for interpolation
	QAngle oldAngYellow;
	QAngle oldAngGreen;
 
	//Networked vars
	CNetworkVar( bool, bDJEnabled );
	CNetworkVar( float, bDJStream1Pos); //unused atm
};
 
LINK_ENTITY_TO_CLASS( club_dj, CClubDJ );
 
IMPLEMENT_SERVERCLASS_ST( CClubDJ, DT_ClubDJ )
	SendPropInt(SENDINFO(bDJEnabled), 0, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO(bDJStream1Pos), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

BEGIN_DATADESC( CClubDJ )
	DEFINE_INPUTFUNC( FIELD_VOID, "ForcePlay", ForcePlay ),

	//Get keyfield info from entity for lights (specified in Hammer)
	DEFINE_KEYFIELD( lightMainStr, FIELD_STRING, "lightMain" ),
	//DEFINE_KEYFIELD( lightBass, FIELD_STRING, "lightBass" ),
	//DEFINE_KEYFIELD( lightHigh, FIELD_STRING, "lightHigh" ),
END_DATADESC()

CClubDJ::CClubDJ(){
	bDJEnabled = false;

	//Initialize BASS module
	bassInit = BASS_Init(-1, 44100, BASS_DEVICE_3D, 0, NULL);
	if(!bassInit)
	{
		//Error handling
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

	//Initialize old angles (might not be neccisary)
	oldAngYellow = QAngle(-90,0,0);
	oldAngGreen = QAngle(-90,0,0);
}

void CClubDJ::Spawn(){
	BaseClass::Spawn();

	//Link up lights for lightshow usage
	lightMain = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light1"));
	lightBass = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light2"));
	lightHigh = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light3"));
	lightGreen = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light4"));
	lightYellow = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,"light5"));

	//Test check
	if(lightMain!=NULL){
		Msg("Found Main Light for club_dj.\n");
	}
	else{
		Warning("Could not find Main Light for club_dj!");
	}

	//Moar test checkz but for the keyfield string instead here
	if(lightMainStr!=NULL){
		Msg("YEAH! %s\n",lightMainStr);
	}
	else{
		Msg("SHIT!\n");
	}

	//Start thinking
	SetNextThink( gpGlobals->curtime);
}

void CClubDJ::ForcePlay(inputdata_t &inputData){
	if(bDJEnabled){
		bDJEnabled=false;
		if(bassInit){
			BASS_ChannelStop(serverStream1);
		}
		else{
			DevMsg("CoopCrowd Club's DJ is experiencing brain thingies!\n");
		}
	}
	else{
		bDJEnabled=true;
		if(bassInit){
			//Create new stream (or refresh)
			ConVarRef url = ConVarRef("club_url");
			serverStream1=BASS_StreamCreateURL(url.GetString(), 0, 0, NULL, 0);
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

//Calculates the average of input range
//TODO: unstable but it works still have to add some checks
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
	
	//Check if stream 1 is not NULL
	if(serverStream1!=NULL){
		float fft[512]; // fft data buffer
		BASS_ChannelGetData(serverStream1, fft, BASS_DATA_FFT1024);
		//Check if lights are not NULL and apply lightshow data
		if(lightMain!=NULL){
			std::string diff = "255 0 0 ";
			std::stringstream ss;
			ss<<FFTAverage(fft,24,10)*20000;
			diff.append(ss.str());
			lightMain->SetColor_Diffuse(stringColToVec(diff.c_str()));
		}
		if(lightBass!=NULL){
			std::string diff = "0 0 255 ";
			std::stringstream ss;
			ss<<FFTAverage(fft,4,10)*10000;
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
			float avg = FFTAverage(fft,300,10);
			std::string diff = "0 255 0 ";
			std::stringstream ss;
			ss<<avg*200000;
			diff.append(ss.str());
			lightGreen->SetColor_Diffuse(stringColToVec(diff.c_str()));
			QAngle aLocal = QAngle(sin(gpGlobals->curtime)*avg*20000,sin(gpGlobals->curtime)*avg*20000,sin(gpGlobals->curtime)*avg*20000);
			aLocal = (aLocal*0.2)+(oldAngGreen*0.8);
			lightGreen->SetLocalAngles(aLocal);
			lightGreen->SetAbsAngles(oldAngGreen);
			oldAngGreen=aLocal;
		}
		if(lightYellow!=NULL){
			float avg = FFTAverage(fft,200,10);
			std::string diff = "255 255 0 ";
			std::stringstream ss;
			ss<<avg*200000;
			diff.append(ss.str());
			lightYellow->SetColor_Diffuse(stringColToVec(diff.c_str()));
			QAngle aLocal = QAngle(sin(gpGlobals->curtime)*avg*20000,sin(gpGlobals->curtime)*avg*20000,sin(gpGlobals->curtime)*avg*20000);
			aLocal = (aLocal*0.2)+(oldAngYellow*0.8);
			lightYellow->SetLocalAngles(aLocal);
			lightYellow->SetAbsAngles(oldAngYellow);
		}
	}

	SetNextThink( gpGlobals->curtime + 0.05 );
}