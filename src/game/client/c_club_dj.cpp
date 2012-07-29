#include "cbase.h"
#include "deferred\CDefLight.h"
#include "deferred\deferred_shared_common.h"
#include "mathlib\mathlib.h"
#include "bass.h"
#include "bass_fx.h"
#include <string>
#include <sstream>

ConVar club_url("club_url", "http://iku.streams.bassdrive.com:8000", FCVAR_CHEAT | FCVAR_REPLICATED, "Club - Playback URL (SHOUTcast or just regular *.mp3 and *.ogg files" );

class C_ClubDJ : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ClubDJ, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_ClubDJ();
	~C_ClubDJ();

	void ForcePlay();
	void ForceStop();

	void OnDataChanged( DataUpdateType_t type );
	void Spawn();
	void ClientThink();
 
	BOOL bassInit;
	HWND hWndPotato;
	HSTREAM stream1;
	HSTREAM stream2;

	HFX dsp;

	//Light EHANDLEs
	CNetworkVar( CDeferredLight *, eLightMain);
	CNetworkVar( CDeferredLight *, eLightBass);
	CNetworkVar( CDeferredLight *, eLightHigh);
	CNetworkVar( CDeferredLight *, eLightGreen);
	CNetworkVar( CDeferredLight *, eLightYellow);

	//Light pointers
	CDeferredLight *lightMain;
	CDeferredLight *lightBass;
	CDeferredLight *lightHigh;
	CDeferredLight *lightGreen;
	CDeferredLight *lightYellow;
	
	//Old light colours (for interpolation)
	std::string oldLightMain;
	std::string oldLightBass;
	std::string oldLightHigh;
	std::string oldLightGreen;
	std::string oldLightYellow;

	//Old angles of lights for interpolation
	QAngle oldAngYellow;
	QAngle oldAngGreen;

	//testvars
	CNetworkVar( bool, bDJEnabled );
	CNetworkVar( float, bDJStream1Pos);
};

LINK_ENTITY_TO_CLASS( club_dj, C_ClubDJ );

IMPLEMENT_CLIENTCLASS_DT( C_ClubDJ, DT_ClubDJ, CClubDJ )
	RecvPropInt( RECVINFO( bDJEnabled ) ),
	RecvPropFloat(RECVINFO(bDJStream1Pos)),
	RecvPropEHandle(RECVINFO(eLightMain)),
	RecvPropEHandle(RECVINFO(eLightBass)),
	RecvPropEHandle(RECVINFO(eLightHigh)),
	RecvPropEHandle(RECVINFO(eLightGreen)),
	RecvPropEHandle(RECVINFO(eLightYellow)),
END_RECV_TABLE()

C_ClubDJ::C_ClubDJ(){
	bDJEnabled=false;

	hWndPotato = FindWindowA("Valve001", "Project Potato");
	if(!hWndPotato)
	{
		Error("Unable to find window for BASS library");
	}

	bassInit = BASS_Init(-1, 44100, BASS_DEVICE_3D, hWndPotato, NULL);
	if(!bassInit)
	{
		int error = BASS_ErrorGetCode();
		if(error==BASS_ERROR_ALREADY){
			Msg("BASS: Probably running listen server. Bass is already running and doesn't have to be re-initialized.\n");
			bassInit=true;
		}
		else if(error==-1){
			Error("Unable to initialize module required for DJ audio system.\nTry restarting the mod. This error usually doesn't occur twice in a row.\nError: %d\n");
		}
		else{
			Error("Unable to initialize module required for DJ audio system.\nTry restarting the mod.\nError: %d\nIf shit still goes sick, contact AniCator.\n", error);
		}
	}
	else{
		Msg("BASS module has been initialized...\n");
		BASS_SetVolume(BASS_GetVolume());
		HPLUGIN fxPlugin = BASS_PluginLoad("bass_fx.dll",0);
		if(fxPlugin==0){
			int error = BASS_ErrorGetCode();
			Warning("Could not initialize BASS_FX: error %i",error);
		}
	}

	//Initialize old angles (might not be neccisary)
	oldAngYellow = QAngle(-90,0,0);
	oldAngGreen = QAngle(-90,0,0);
}

C_ClubDJ::~C_ClubDJ(){
	if(bassInit){
		BASS_Free();
	}
}

//		void C_ClubDJ::ForcePlay()
//
//			Plays BASS channel.
//	Stream is created if it's non-existing
//
////////////////////////////////////////////
void C_ClubDJ::ForcePlay(){
	//put stuff here
	if(bassInit){
		//Create new stream
		ConVarRef url = ConVarRef("club_url");
		stream1=BASS_StreamCreateURL(url.GetString(), 0, BASS_SAMPLE_MONO | BASS_SAMPLE_3D, NULL, 0);
		if(BASS_FX_GetVersion()==BASSVERSION){
			dsp = BASS_ChannelSetFX(stream1,BASS_FX_BFX_LPF,0);
			if(!dsp){
				int error = BASS_ErrorGetCode();
				DevMsg("Could not set FX on channel. Error: %i\n",error);
			}
		}
		else{
			DevMsg("Incorrect BASS_FX version loaded.\n");
		}
		//Play stream
		if(stream1!=NULL){
			BASS_ChannelPlay(stream1,true);
			BASS_ChannelSetAttribute(stream1,BASS_ATTRIB_VOL,1.0f);
		}
		else{
			Warning("Could not open stream.\n");
		}
	}
	else{
		Msg("CoopCrowd Club's DJ is experiencing brain thingies!\n");
	}
}

//		void C_ClubDJ::ForceStop()
//
//			Stops BASS channel.
//
////////////////////////////////////////////
void C_ClubDJ::ForceStop(){
	//put stuff here
	if(bassInit){
		BASS_ChannelStop(stream1);
		BASS_ChannelRemoveFX(dsp,BASS_FX_BFX_LPF);
	}
	else{
		DevMsg("CoopCrowd Club's DJ is experiencing brain thingies!\n");
	}
}

void C_ClubDJ::OnDataChanged( DataUpdateType_t type ){
	BaseClass::OnDataChanged( type );
	if(bDJEnabled){
		ForcePlay();
	}
	else{
		ForceStop();
	}

	if(eLightMain!=NULL){
		//TODO: This crashes the mod atm
		//lightMain = static_cast<CDeferredLight *>(cl_entitylist->GetBaseEntity(eLightMain.Get()->index));

		//Check if light isn't NULL (test check)
		if(lightMain!=NULL){
			Msg("Found Main Light for club_dj.\n");
		}
		else{
			Warning("Could not find Main Light for club_dj!");
		}
	}
}

void C_ClubDJ::Spawn(){
	BaseClass::Spawn();

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//BASS_3DVECTOR *Get3DVect(const Vector vect)
//
//	 Converts Vector to BASS_3DVECTOR.
//
////////////////////////////////////////////
BASS_3DVECTOR *Get3DVect(const Vector vect)
{
        BASS_3DVECTOR *vec = new BASS_3DVECTOR;
        vec->x = vect.x;
        vec->y = vect.y;
        vec->z = vect.z;
        return vec;
}

ConVarRef mindist = ConVarRef("club_mindist");
ConVarRef maxdist = ConVarRef("club_maxdist");
ConVarRef doppl = ConVarRef("club_doppler");
ConVarRef roll = ConVarRef("club_roll");
ConVarRef dist = ConVarRef("club_distf");

static void OnChangeMinDist( IConVar *var, const char *pOldValue, float flOldValue ){
	mindist = ConVarRef("club_mindist");
}

void OnChangeMaxDist( IConVar *var, const char *pOldValue, float flOldValue ){
	maxdist = ConVarRef("club_maxdist");
}

void OnChangeDoppler( IConVar *var, const char *pOldValue, float flOldValue ){
	doppl = ConVarRef("club_doppler");
}
void OnChangeRolloff( IConVar *var, const char *pOldValue, float flOldValue ){
	roll = ConVarRef("club_roll");
}
void OnChangeDistFactor( IConVar *var, const char *pOldValue, float flOldValue ){
	dist = ConVarRef("club_distf");
}

//ConVars
ConVar club_distf("club_distf", "0.4", FCVAR_CHEAT, "BASS - Audible distance factor", OnChangeDistFactor);
ConVar club_roll("club_roll", "2", FCVAR_CHEAT, "BASS - Rollof factor", OnChangeRolloff);
ConVar club_doppler("club_doppler", "0.01", FCVAR_CHEAT, "BASS - Doppler factor", OnChangeDoppler);

ConVar club_maxdist("club_maxdist", "5000", FCVAR_CHEAT, "BASS - Maximum audible distance", OnChangeMaxDist);
ConVar club_mindist("club_mindist", "500", FCVAR_CHEAT, "BASS - Minimum audible distance", OnChangeMinDist);

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

void C_ClubDJ::ClientThink(){
	BaseClass::ClientThink();

	//Check if stream 1 is not NULL
	if(stream1!=NULL){
		if(BASS_ChannelIsActive(stream1)==BASS_ACTIVE_PLAYING){
			float fft[512]; // fft data buffer
			BASS_ChannelGetData(stream1, fft, BASS_DATA_FFT1024);
			//Check if lights are not NULL and apply lightshow data
			if(lightMain!=NULL){
				std::string diff = "255 0 0 ";
				std::stringstream ss;
				ss<<FFTAverage(fft,24,10)*20000;
				diff.append(ss.str());
				lightMain->SetColor_Diffuse(stringColToVec(diff.c_str()));
				oldLightMain = diff;
			}
			if(lightBass!=NULL){
				std::string diff = "0 0 255 ";
				std::stringstream ss;
				ss<<FFTAverage(fft,4,10)*10000;
				ss<<FFTAverage(fft,5,10)*5000;
				diff.append(ss.str());
				lightBass->SetColor_Diffuse(stringColToVec(diff.c_str()));
				oldLightBass = diff;
			}
			if(lightHigh!=NULL){
				std::string diff = "255 255 255 ";
				std::stringstream ss;
				ss<<FFTAverage(fft,100,10)*200000;
				diff.append(ss.str());
				lightHigh->SetColor_Diffuse(stringColToVec(diff.c_str()));
				oldLightHigh = diff;
			}
			if(lightGreen!=NULL){
				float avg = FFTAverage(fft,300,10);
				std::string diff = "0 255 0 ";
				std::stringstream ss;
				ss<<avg*200000;
				diff.append(ss.str());
				lightGreen->SetColor_Diffuse(stringColToVec(diff.c_str()));
				oldLightGreen = diff;
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
				oldLightYellow = diff;
				QAngle aLocal = QAngle(sin(gpGlobals->curtime)*avg*20000,sin(gpGlobals->curtime)*avg*20000,sin(gpGlobals->curtime)*avg*20000);
				aLocal = (aLocal*0.2)+(oldAngYellow*0.8);
				lightYellow->SetLocalAngles(aLocal);
				lightYellow->SetAbsAngles(oldAngYellow);
			}
		}

		//Register player position and velocity
		BASS_3DVECTOR *playerPos = Get3DVect(C_BasePlayer::GetLocalPlayer()->GetAbsOrigin());
        BASS_3DVECTOR *playerVel = Get3DVect(C_BasePlayer::GetLocalPlayer()->GetAbsVelocity());

		//Register player angles
		Vector fwd, rt, up;
		AngleVectors(C_BasePlayer::GetLocalPlayer()->GetAbsAngles(), &fwd, &rt, &up);
		BASS_3DVECTOR *playerFront = Get3DVect(fwd*-1); //Inverse fwd vector since BASS uses it the other way around
		BASS_3DVECTOR *playerTop = Get3DVect(up);

		//Set 3D Factors and player position
		BASS_Set3DFactors(dist.GetFloat(), roll.GetFloat(), doppl.GetFloat());
		BASS_Set3DPosition(playerPos,playerVel,playerFront,playerTop);

		//Register club_dj position, angles and velocity
		BASS_3DVECTOR *pos = Get3DVect(GetAbsOrigin());
        BASS_3DVECTOR *orient = Get3DVect(Vector(GetAbsAngles().x,GetAbsAngles().y,GetAbsAngles().z));
        BASS_3DVECTOR *vel = Get3DVect(GetAbsVelocity());
		
		//Set club_dj position on BASS interface
        BASS_ChannelSet3DAttributes(stream1, BASS_3DMODE_NORMAL, mindist.GetFloat(), maxdist.GetFloat(), 360, 360, 0);
        BASS_ChannelSet3DPosition(stream1, pos, orient, vel);
		
		//Update volume
		ConVarRef volume("volume");
		ConVarRef musicVolume("snd_musicvolume");
		float multVolume = volume.GetFloat()*musicVolume.GetFloat();
		
		if(GetFocus()==hWndPotato){
			BASS_ChannelSetAttribute(stream1,BASS_ATTRIB_VOL,multVolume);
		}
		else{
			BASS_ChannelSetAttribute(stream1,BASS_ATTRIB_VOL,0.0);
		}

		//Apply effects
		BASS_BFX_LPF *fx = new BASS_BFX_LPF();
		fx->fCutOffFreq = 200.0f;
		fx->fResonance = 5.0f;
		BASS_FXSetParameters(dsp,fx);
		delete fx;

		//Apply 3D data changes
		BASS_Apply3D();

		//Clean up vectors
		delete pos;
		delete orient;
		delete vel;
		delete playerPos;
		delete playerVel;
		delete playerFront;
		delete playerTop;
	}
	SetNextClientThink( CLIENT_THINK_ALWAYS ); //Update 3D data every frame
}