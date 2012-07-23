#include "cbase.h"
#include "bass.h"

class CClubDJ : public CLogicalEntity
{
	public:
	DECLARE_CLASS( CClubDJ, CLogicalEntity );
	DECLARE_DATADESC();

	CClubDJ();
 
	// Input function
	void ForcePlay( inputdata_t &inputData );
 
	private:
 
	int	m_nThreshold;	// Count at which to fire our output
	int	m_nCounter;	// Internal counter
 
	COutputEvent	m_OnThreshold;	// Output event when the counter reaches the threshold
};
 
LINK_ENTITY_TO_CLASS( club_dj, CClubDJ  );
 
// Start of our data description for the class
BEGIN_DATADESC( CClubDJ  )
 
// For save/load
DEFINE_FIELD( m_nCounter, FIELD_INTEGER ),
 
// Links our member variable to our keyvalue from Hammer
DEFINE_KEYFIELD( m_nThreshold, FIELD_INTEGER, "threshold" ),

//ForcePlay - Forces DJ entity to play track
DEFINE_INPUTFUNC( FIELD_VOID, "ForcePlay", ForcePlay),
 
// Links our output member to the output name used by Hammer
DEFINE_OUTPUT( m_OnThreshold, "OnThreshold" ),
 
END_DATADESC()

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

void CClubDJ::ForcePlay( inputdata_t &inputData )
{
	Msg("DJ: Shit should happen soon.. gotta code it first.\n\n");
	
	HSTREAM stream=BASS_StreamCreateURL("http://iku.streams.bassdrive.com:8000", 0, 0, NULL, 0);
	BASS_ChannelPlay(stream,true);
}