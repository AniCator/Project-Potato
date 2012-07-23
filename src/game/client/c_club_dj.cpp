#include "cbase.h"
#include "bass.h"

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

void C_ClubDJ::ForcePlay(){
	//put stuff here
	Msg("huh? am i supposed to do something?\n");
	if(bassInit){
		stream1=BASS_StreamCreateURL("http://iku.streams.bassdrive.com:8000", 0, 0, NULL, 0);
		BASS_ChannelPlay(stream1,true);
	}
	else{
		Msg("I can't do shit when BASS is borked.\n");
	}
}

void C_ClubDJ::ForceStop(){
	//put stuff here
	Msg("huh? am i supposed to do something?\n");
	if(bassInit){
		HSTREAM stream=BASS_StreamCreateURL("http://iku.streams.bassdrive.com:8000", 0, 0, NULL, 0);
		BASS_ChannelStop(stream1);
	}
	else{
		Msg("I can't do shit when BASS is borked.\n");
	}
}

void C_ClubDJ::OnDataChanged( DataUpdateType_t type ){
	if(bDJEnabled){
		ForcePlay();
	}
	else{
		ForceStop();
	}
}