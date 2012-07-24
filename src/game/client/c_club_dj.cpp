#include "cbase.h"
#include "mathlib\mathlib.h"
#include "bass.h"
#include "bass_vst.h"

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
	HSTREAM stream1;
	HSTREAM stream2;
public:
	//testvars
	CNetworkVar( bool, bDJEnabled );
};

LINK_ENTITY_TO_CLASS( club_dj, C_ClubDJ );

IMPLEMENT_CLIENTCLASS_DT( C_ClubDJ, DT_ClubDJ, CClubDJ )
	RecvPropInt( RECVINFO( bDJEnabled ) ),
END_RECV_TABLE()

C_ClubDJ::C_ClubDJ(){
	bDJEnabled=false;

	HWND hWndPotato = FindWindowA("Valve001", "Project Potato");
	if(!hWndPotato)
	{
		Error("Unable to find window for BASS library");
	}

	bassInit = BASS_Init(-1, 44100, BASS_DEVICE_3D, hWndPotato, NULL);
	if(!bassInit)
	{
		int error = BASS_ErrorGetCode();
		Msg("BASS Init failed, error code %d\n", error);
		Error("BASS Init error");
	}
	else{
		Msg("BASS module has been initialized...\n");
	}
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
		if(stream1==NULL){
			//Create new stream
			stream1=BASS_StreamCreateURL("http://iku.streams.bassdrive.com:8000", 0, BASS_SAMPLE_MONO | BASS_SAMPLE_3D, NULL, 0);
			DWORD dsp = BASS_VST_ChannelSetDSP(stream1,"ClassicReverb.dll",0,0);
		}
		//Play stream
		BASS_ChannelPlay(stream1,true);
		Msg("CoopCrowd Club is Live!\n");
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
		Msg("aaaaaaaand stream's stopped.\n");
	}
	else{
		Msg("CoopCrowd Club's DJ is experiencing brain thingies!\n");
	}
}

void C_ClubDJ::OnDataChanged( DataUpdateType_t type ){
	if(type==DATA_UPDATE_DATATABLE_CHANGED){
		if(bDJEnabled){
			ForcePlay();
		}
		else{
			ForceStop();
		}
	}
}

void C_ClubDJ::Spawn(){
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

void C_ClubDJ::ClientThink(){
	if(stream1!=NULL){
		//Register player position and velocity
		BASS_3DVECTOR *playerPos = Get3DVect(C_BasePlayer::GetLocalPlayer()->GetAbsOrigin());
        BASS_3DVECTOR *playerVel = Get3DVect(C_BasePlayer::GetLocalPlayer()->GetAbsVelocity());

		//Register player angles
		Vector fwd, rt, up;
		AngleVectors(C_BasePlayer::GetLocalPlayer()->GetAbsAngles(), &fwd, &rt, &up);
		BASS_3DVECTOR *playerFront = Get3DVect(fwd*-1); //Inverse fwd vector since BASS uses it the other way around
		BASS_3DVECTOR *playerTop = Get3DVect(up);

		//Set 3D Factors and player position
		BASS_Set3DFactors(1.0, 0.001, 0.005);
		BASS_Set3DPosition(playerPos,playerVel,playerFront,playerTop);

		//Register club_dj position, angles and velocity
		BASS_3DVECTOR *pos = Get3DVect(GetAbsOrigin());
        BASS_3DVECTOR *orient = Get3DVect(Vector(GetAbsAngles().x,GetAbsAngles().y,GetAbsAngles().z));
        BASS_3DVECTOR *vel = Get3DVect(GetAbsVelocity());
		
		//Set club_dj position on BASS interface
        BASS_ChannelSet3DAttributes(stream1, BASS_3DMODE_NORMAL, 0, 0, 360, 360, 0);
        BASS_ChannelSet3DPosition(stream1, pos, orient, vel);

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