#include "cbase.h"
#include "deferred\CDefLight.h"
#include "deferred\deferred_shared_common.h"
#include "mathlib\mathlib.h"
#include "bass.h"
#include <string>
#include <sstream>

ConVar club_url("club_url", "http://mirror.anicator.com/dainumo/faster.mp3", FCVAR_REPLICATED, "Club - Playback URL (SHOUTcast or just regular *.mp3 and *.ogg files" );

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

	//Light EHANDLEs
	CNetworkHandle( CDeferredLight, eLightMain);
	CNetworkHandle( CDeferredLight, eLightBass);
	CNetworkHandle( CDeferredLight, eLightHigh);
	CNetworkHandle( CDeferredLight, eLightGreen);
	CNetworkHandle( CDeferredLight, eLightYellow);

	//Light strings (for keyfields)
	char *lightMainStr;
	char *lightBassStr;
	char *lightHighStr;
	char *lightGreenStr;
	char *lightYellowStr;
 
	//Networked vars
	CNetworkVar( bool, bDJEnabled );
	CNetworkVar( float, bDJStream1Pos); //unused atm
};
 
LINK_ENTITY_TO_CLASS( club_dj, CClubDJ );
 
IMPLEMENT_SERVERCLASS_ST( CClubDJ, DT_ClubDJ )
	SendPropInt(SENDINFO(bDJEnabled), 0, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO(bDJStream1Pos), 0, SPROP_NOSCALE ),
	SendPropEHandle(SENDINFO(eLightMain)),
	SendPropEHandle(SENDINFO(eLightBass)),
	SendPropEHandle(SENDINFO(eLightHigh)),
	SendPropEHandle(SENDINFO(eLightGreen)),
	SendPropEHandle(SENDINFO(eLightYellow)),
END_SEND_TABLE()

BEGIN_DATADESC( CClubDJ )
	DEFINE_INPUTFUNC( FIELD_VOID, "ForcePlay", ForcePlay ),

	//Get keyfield info from entity for lights (specified in Hammer)
	DEFINE_KEYFIELD( lightMainStr, FIELD_STRING, "lightMain" ),
	DEFINE_KEYFIELD( lightBassStr, FIELD_STRING, "lightBass" ),
	DEFINE_KEYFIELD( lightHighStr, FIELD_STRING, "lightHigh" ),
	DEFINE_KEYFIELD( lightGreenStr, FIELD_STRING, "lightGreen" ),
	DEFINE_KEYFIELD( lightYellowStr, FIELD_STRING, "lightYellow" ),
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
		else if(error==-1){
			Error("Unable to initialize module required for DJ audio system.\nTry restarting the mod. This error usually doesn't occur twice in a row.\nError: %d\n");
		}
		else{
			Error("Unable to initialize module required for DJ audio system.\nTry restarting the mod.\nError: %d\n", error);
		}
	}
	else{
		Msg("BASS module has been initialized...\n");
		BASS_SetVolume(BASS_GetVolume());
	}
}

void CClubDJ::Spawn(){
	BaseClass::Spawn();

	//Link up lights for lightshow usage
	eLightMain = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,lightMainStr));
	eLightBass = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,lightBassStr));
	eLightHigh = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,lightHighStr));
	eLightGreen = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,lightGreenStr));
	eLightYellow = static_cast<CDeferredLight *>(gEntList.FindEntityByName(this,lightYellowStr));

	DevMsg("Debug: Transmitting light info to clients.\n");
	NetworkStateChanged();

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
	SetNextThink( gpGlobals->curtime + 0.05 );
}