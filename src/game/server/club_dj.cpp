//Club DJ entity for CoopCrowd Club mod

#include "cbase.h"
#include "bass.h"

class CClubDJ : public CBaseEntity
{
	public:
	DECLARE_CLASS( CClubDJ, CBaseEntity );

	DECLARE_SERVERCLASS();

	DECLARE_DATADESC();

	int UpdateTransmitState()	// always send to all clients
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	CClubDJ();
 
	// Input function
	void ForcePlay( inputdata_t &inputData );
 
	private:
 
	int	m_nThreshold;	// Count at which to fire our output
	int	m_nCounter;	// Internal counter
 
	COutputEvent	m_OnThreshold;	// Output event when the counter reaches the threshold
};
 
LINK_ENTITY_TO_CLASS( club_dj, CClubDJ  );

IMPLEMENT_SERVERCLASS_ST( CClubDJ, DT_ClubDJ )
END_SEND_TABLE()
 
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

CreateEntityByName( "myentity" );

CClubDJ::CClubDJ ()
{
	m_nCounter = 0;
}

void CClubDJ::ForcePlay( inputdata_t &inputData )
{
	Msg("DJ: Shit should happen soon.. gotta code it first.\n\n");
}