#include "cbase.h"
#include "c_club_dj.h"

C_ClubDJ::C_ClubDJ(){
	Msg("Club DJ\nBy: AniCator & Th13teen\nInitializing...\n");

	HWND hWndPotato = FindWindowA("Valve001", "Project Potato");
	if(!hWndPotato)
	{
		Error("Unable to find Garry's Mod window for BASS library");
	}

	BOOL bassInit = BASS_Init(-1, 44100, BASS_DEVICE_3D, hWndPotato, NULL);
	if(!bassInit)
	{
		int error = BASS_ErrorGetCode();
		Msg("BASS Init failed, error code %d\n", error);
		Error("BASS Init error");
	}
	else{
		Msg("Club DJ's body is ready!\n");
	}
}

C_ClubDJ::~C_ClubDJ(){
	BASS_Free();
}

void C_ClubDJ::ForcePlay( inputdata_t &inputData )
{
	Msg("(ClientSide) DJ: Shit should happen soon.. gotta code it first.\n\n");
	
	HSTREAM stream=BASS_StreamCreateURL("http://iku.streams.bassdrive.com:8000", 0, 0, NULL, 0);
	BASS_ChannelPlay(stream,true);
}

IMPLEMENT_CLIENTCLASS_DT( C_ClubDJ, DT_ClubDJ, CClubDJ )
END_RECV_TABLE()