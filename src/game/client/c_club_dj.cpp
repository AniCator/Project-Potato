#include "cbase.h"
#include "deferred\CDefLight.h"
#include "deferred\deferred_shared_common.h"
#include "mathlib\mathlib.h"
#include "bass.h"
#include "bass_fx.h"
#include <string>
#include <sstream>

ConVar club_url("club_url", "http://mirror.anicator.com/wannadance.mp3", FCVAR_CHEAT | FCVAR_REPLICATED, "Club - Playback URL (SHOUTcast or just regular *.mp3 and *.ogg files" );

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
	HFX dsp2;

	BASS_BFX_LPF lpf;
	BASS_BFX_ECHO2 echo2;

	//LPF interp values
	float oldCutoff;
	//Echo2 interp values
	float oldWet;
	float oldDry;
	float oldDelay;

	//Light EHANDLEs
	CNetworkHandle( CDeferredLight, eLightMain);
	CNetworkHandle( CDeferredLight, eLightBass);
	CNetworkHandle( CDeferredLight, eLightHigh);
	CNetworkHandle( CDeferredLight, eLightGreen);
	CNetworkHandle( CDeferredLight, eLightYellow);

	//Light pointers
	def_light_t *lightMain;
	def_light_t *lightBass;
	def_light_t *lightHigh;
	def_light_t *lightGreen;
	def_light_t *lightYellow;

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

	if (HIWORD(BASS_GetVersion())!=BASSVERSION) {
		Warning("An incorrect version of BASS.DLL was loaded (2.4 is required)\n");
	}
	if (HIWORD(BASS_FX_GetVersion())!=BASSVERSION) {
		Warning("An incorrect version of BASS_FX.DLL was loaded (2.4 is required)\n");
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
	}

	//Initialize old angles (might not be neccisary)
	oldAngYellow = QAngle(-90,0,0);
	oldAngGreen = QAngle(-90,0,0);

	oldCutoff = 100.0f;

	oldWet = 1.0f;
	oldDry = 0.0f;
	oldDelay = 0.05f;
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
		//stream1=BASS_StreamCreateURL(url.GetString(), 0, BASS_SAMPLE_MONO | BASS_SAMPLE_3D, NULL, 0);
		stream1=BASS_StreamCreateURL(url.GetString(), 0, 0, NULL, 0);
		
		//Play stream
		if(stream1!=NULL){
			BASS_ChannelSetAttribute(stream1,BASS_ATTRIB_VOL,1.0f);
			
			//order is important
			dsp2 = BASS_ChannelSetFX(stream1,BASS_FX_BFX_ECHO2,0);
			dsp = BASS_ChannelSetFX(stream1,BASS_FX_BFX_LPF,0);
			
			if(!dsp){
				int error = BASS_ErrorGetCode();
				DevMsg("Could not set LPF FX on channel. Error: %i\n",error);
			}
			if(!dsp2){
				int error = BASS_ErrorGetCode();
				DevMsg("Could not set ECHO2 FX on channel. Error: %i\n",error);
			}
			BASS_ChannelPlay(stream1,true);
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
		BASS_ChannelRemoveFX(dsp2, BASS_FX_BFX_ECHO2);
		BASS_ChannelRemoveFX(dsp, BASS_FX_BFX_LPF);
		BASS_StreamFree(stream1);
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

	//Check if light isn't NULL (test check)
	bool allWentWell=true;
	if(eLightMain!=NULL){
		lightMain = eLightMain.Get()->GetDefLightT();
	}
	else allWentWell=false;
	if(eLightBass!=NULL){
		lightBass = eLightBass.Get()->GetDefLightT();
	}
	else allWentWell=false;
	if(eLightHigh!=NULL){
		lightHigh = eLightHigh.Get()->GetDefLightT();
	}
	else allWentWell=false;
	if(eLightGreen!=NULL){
		lightGreen = eLightGreen.Get()->GetDefLightT();
	}
	else allWentWell=false;
	if(eLightYellow!=NULL){
		lightYellow = eLightYellow.Get()->GetDefLightT();
	}
	else allWentWell=false;

	//Check if all lights loaded.
	if(!allWentWell){
		Warning("Client: Not all lights have been specified for client-side lightshow automation.\n");
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
ConVarRef lowpassf = ConVarRef("club_lowpassf");
ConVarRef echof = ConVarRef("club_echof");

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
void OnChangeLowpassFactor( IConVar *var, const char *pOldValue, float flOldValue ){
	lowpassf = ConVarRef("club_lowpassf");
}
void OnChangeEchoFactor( IConVar *var, const char *pOldValue, float flOldValue ){
	echof = ConVarRef("club_echof");
}

//ConVars
ConVar club_distf("club_distf", "0.4", FCVAR_CHEAT, "BASS - Audible distance factor", OnChangeDistFactor);
ConVar club_roll("club_roll", "2", FCVAR_CHEAT, "BASS - Rollof factor", OnChangeRolloff);
ConVar club_doppler("club_doppler", "0.01", FCVAR_CHEAT, "BASS - Doppler factor", OnChangeDoppler);

ConVar club_maxdist("club_maxdist", "5000", FCVAR_CHEAT, "BASS - Maximum audible distance", OnChangeMaxDist);
ConVar club_mindist("club_mindist", "500", FCVAR_CHEAT, "BASS - Minimum audible distance", OnChangeMinDist);
ConVar club_lowpassfactor("club_lowpassf", "0.9", FCVAR_CHEAT, "Lowpass factor/Muffling factor", OnChangeLowpassFactor);
ConVar club_echofactor("club_echof", "0.0003", FCVAR_CHEAT, "Echo factor/Delay factor", OnChangeEchoFactor);

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
				int r,g,b,i;
				r = 255;
				g = 0;
				b = 0;
				i = FFTAverage(fft,25,10)*20000;

				int* oldCol = vecToIntArray(eLightMain.Get()->GetColor_Diffuse());
				//r = (r*0.2)+(oldCol[0]*0.8);
				//g = (g*0.2)+(oldCol[1]*0.8);
				//b = (b*0.2)+(oldCol[2]*0.8);
				//i = (i*0.1)+(oldCol[3]*0.9);

				std::stringstream ss;
				ss<<r<<" "<<g<<" "<<b<<" "<<i;
				std::string diff = ss.str();

				lightMain->col_diffuse = stringColToVec(diff.c_str());
			}
			if(lightBass!=NULL){
				int r,g,b,i;
				r = 0;
				g = 0;
				b = 255;
				i = FFTAverage(fft,7,10)*10000;

				int *oldCol = vecToIntArray(eLightBass.Get()->GetColor_Diffuse());
				//r = (r*0.2)+(oldCol[3]*0.8);
				//g = (g*0.2)+(oldCol[2]*0.8);
				//b = (b*0.2)+(oldCol[1]*0.8);
				//i = (i*0.1)+(oldCol[0]*0.9);

				std::stringstream ss;
				ss<<r<<" "<<g<<" "<<b<<" "<<i;

				lightBass->col_diffuse = stringColToVec(ss.str().c_str());
			}
			if(lightHigh!=NULL){
				std::string diff = "255 255 255 ";
				std::stringstream ss;
				ss<<FFTAverage(fft,100,20)*200000;
				diff.append(ss.str());
				Vector oldCol = eLightHigh.Get()->GetColor_Diffuse();
				Vector newCol = stringColToVec(diff.c_str());
				newCol = (newCol*0.2)+(oldCol*0.8);
				lightHigh->col_diffuse = stringColToVec(diff.c_str());
			}
			if(lightGreen!=NULL){
				float avg = FFTAverage(fft,250,30);
				std::string diff = "0 255 0 ";
				std::stringstream ss;
				ss<<avg*200000;
				diff.append(ss.str());
				Vector oldCol = eLightGreen.Get()->GetColor_Diffuse();
				Vector newCol = stringColToVec(diff.c_str());
				newCol = (newCol*0.2)+(oldCol*0.8);
				eLightGreen.Get()->SetColor_Diffuse(newCol);

				float tMult = sin(gpGlobals->curtime)*2;
				float aAvg = avg*10000;

				QAngle aLocal(90+tMult*aAvg,tMult*aAvg*-1,tMult*aAvg*-1);
				aLocal = (aLocal*0.1)+(oldAngGreen*0.9);

				eLightGreen.Get()->SetAbsAngles(aLocal);

				oldAngGreen = aLocal;
			}
			if(lightYellow!=NULL){
				float avg = FFTAverage(fft,200,30);
				std::string diff = "255 255 0 ";
				std::stringstream ss;
				ss<<avg*200000;
				diff.append(ss.str());
				Vector oldCol = eLightYellow.Get()->GetColor_Diffuse();
				Vector newCol = stringColToVec(diff.c_str());
				newCol = (newCol*0.2)+(oldCol*0.8);
				eLightYellow.Get()->SetColor_Diffuse(newCol);

				float tMult = sin(gpGlobals->curtime)+cos(gpGlobals->curtime);
				float aAvg = avg*10000;

				QAngle aLocal(90+tMult*aAvg,tMult*aAvg,tMult*aAvg);
				aLocal = (aLocal*0.3)+(oldAngYellow*0.7);

				eLightYellow.Get()->SetLocalAngles(aLocal);

				oldAngYellow = aLocal;
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

		//FX stuff

		vec_t distance = C_BasePlayer::GetLocalPlayer()->GetAbsOrigin().DistTo(GetAbsOrigin());

		trace_t trace;
		UTIL_TraceLine(C_BasePlayer::GetLocalPlayer()->GetAbsOrigin(),GetAbsOrigin(),MASK_SHOT,this,COLLISION_GROUP_NONE,&trace);

		vec_t lpfDist = distance;

		//LPF FX

		if(trace.DidHit()){
			lpfDist*=3;
		}
		
		float cutoff;
		if(lowpassf.IsValid()){
			cutoff = 20000.0f-(lpfDist*2*lowpassf.GetFloat());
		}
		else{
			cutoff = 20000.0f-(lpfDist*2);
		}

		cutoff = (cutoff*0.075)+(oldCutoff*0.925);

		if(cutoff>18000.0f){
			cutoff=18000.0f;
		}
		if(cutoff<100.0f){
			cutoff=100.0f;
		}
		
		lpf.fCutOffFreq=cutoff;
		lpf.fResonance=1.0f;
		lpf.lChannel=BASS_BFX_CHANALL;
		BASS_FXSetParameters(dsp,&lpf);

		oldCutoff = cutoff;

		//ECHO2 FX
		vec_t echo2Dist = distance;

		if(trace.DidHit()){
			echo2Dist*=3;
		}

		float dryMix=1.0f;
		float wetMix=0.0f;
		float delay=0.07f;

		float scaleRange;
		if(echof.IsValid()){
			scaleRange = distance*echof.GetFloat();
		}
		else{
			scaleRange = distance*0.0003;
		}

		if(scaleRange>1.0f){
			scaleRange=1.0f;
		}
		if(scaleRange<0.0f){
			scaleRange=0.0f;
		}

		dryMix=1-scaleRange;
		wetMix=scaleRange;
		//delay=scaleRange*0.1;

		//delay = (delay*0.1)+(oldDelay*0.9);

		echo2.fDryMix=dryMix;
		echo2.fWetMix=wetMix;
		echo2.fDelay=delay;
		echo2.fFeedback=-0.3f;
		echo2.lChannel=BASS_BFX_CHANALL;
		BASS_FXSetParameters(dsp2,&echo2);

		oldDry=dryMix;
		oldWet=wetMix;
		oldDelay=delay;

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