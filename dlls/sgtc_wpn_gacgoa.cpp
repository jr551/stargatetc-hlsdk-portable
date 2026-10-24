// sgtc_wpn_gacgoa.cpp — StargateTC CGrenadeChoc (split from sgtc_weapons.cpp; NEW file).

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
// CGrenadeChoc - "Grenade Choc" (Goa'uld shock grenade). Analogue:
// handgrenade.cpp. No client event.
// ===========================================================================

LINK_ENTITY_TO_CLASS( weapon_gacgoa, CGrenadeChoc );

void CGrenadeChoc::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_gacgoa.mdl" );
	m_iId = SGTC_WPN_GACGOA;
	m_iClip = -1;
	m_iDefaultAmmo = 4;
	FallInit();
}

void CGrenadeChoc::Precache( void )
{
	PRECACHE_MODEL( "models/w_gacgoa.mdl" );
	PRECACHE_MODEL( "models/v_gacgoa.mdl" );
	PRECACHE_MODEL( "models/v_gacgoaT.mdl" );
	PRECACHE_MODEL( "models/p_gacgoa.mdl" );
	PRECACHE_SOUND( "weapons/gacgoa.wav" );
	PRECACHE_SOUND( "weapons/gacencl.wav" );
	PRECACHE_SOUND( "common/null.wav" );
}

SGTC_GETITEM( CGrenadeChoc, SGTC_AMMO_GACG, 4, WEAPON_NOCLIP, 5, 4, SGTC_WPN_GACGOA, 24, 10 )
SGTC_ADD_TO_PLAYER( CGrenadeChoc )

BOOL CGrenadeChoc::Deploy( void )
{
	return SGTC_DEPLOY( 2, "models/v_gacgoa.mdl", "models/p_gacgoa.mdl", SGTC_DRAW, "crowbar" );
}

void CGrenadeChoc::PrimaryAttack( void )
{
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;
	Vector vecThrow = gpGlobals->v_forward * 400 + m_pPlayer->pev->velocity;

#ifndef CLIENT_DLL
	CGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecThrow, 2.0 );
#endif

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/gacgoa.wav", 1.0, ATTN_NORM );

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;

	if( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
}

SGTC_NO_SECONDARY( CGrenadeChoc )
SGTC_NO_RELOAD( CGrenadeChoc )
SGTC_WEAPON_IDLE( CGrenadeChoc )
