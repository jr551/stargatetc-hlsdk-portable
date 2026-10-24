// sgtc_wpn_p90.cpp — StargateTC CP90 (split from sgtc_weapons.cpp; NEW file).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "soundent.h"
#include "sgtc_weapons.h"

// ===========================================================================
// CP90 - "P90" SMG. Analogue: mp5.cpp.
// Events: events/p90.sc (fire) / events/p90l.sc (laser secondary).
// ===========================================================================

LINK_ENTITY_TO_CLASS( weapon_p90, CP90 );

void CP90::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_p90.mdl" );
	m_iId = SGTC_WPN_P90;
	m_iDefaultAmmo = 200;
	FallInit();
}

void CP90::Precache( void )
{
	PRECACHE_MODEL( "models/v_p90.mdl" );
	PRECACHE_MODEL( "models/w_p90.mdl" );
	PRECACHE_MODEL( "models/p_p90.mdl" );
	PRECACHE_MODEL( "models/rshell.mdl" );
	PRECACHE_MODEL( "models/w_9mmARclip.mdl" );

	PRECACHE_SOUND( "items/9mmclip1.wav" );
	PRECACHE_SOUND( "items/clipinsert1.wav" );
	PRECACHE_SOUND( "items/cliprelease1.wav" );
	PRECACHE_SOUND( "weapons/P90-1.wav" );
	PRECACHE_SOUND( "weapons/P90recharge.wav" );
	PRECACHE_SOUND( "weapons/357_cock1.wav" );

	m_usFire = PRECACHE_EVENT( 1, "events/p90.sc" );
	m_usFireL = PRECACHE_EVENT( 1, "events/p90l.sc" );
}

SGTC_GETITEM( CP90, SGTC_AMMO_P90, 250, 50, 2, 0, SGTC_WPN_P90, 0, 15 )
SGTC_ADD_TO_PLAYER( CP90 )

BOOL CP90::Deploy( void )
{
	return SGTC_DEPLOY( 2, "models/v_p90.mdl", "models/p_p90.mdl", SGTC_DRAW, "mp5" );
}

SGTC_GUN_PRIMARY( CP90, m_usFire, SGTC_BULLET_P90, VECTOR_CONE_6DEGREES, 1, 8192, 6, 1, 0.07 )
SGTC_NO_SECONDARY( CP90 )
SGTC_WEAPON_RELOAD( CP90, 50 )
SGTC_WEAPON_IDLE( CP90 )
