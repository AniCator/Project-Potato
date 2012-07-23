#include "cbase.h"
#include "bass.h"

CClubDJ::CClubDJ(){
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