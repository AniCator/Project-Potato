#include "cbase.h"
#include "bass.h"

class C_ClubDJ : public C_BaseEntity
{
	public:
	DECLARE_CLASS( C_ClubDJ, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_ClubDJ();
	~C_ClubDJ();
 
	// Input function
	void ForcePlay( inputdata_t &inputData );
};

LINK_ENTITY_TO_CLASS( club_dj, C_ClubDJ  );