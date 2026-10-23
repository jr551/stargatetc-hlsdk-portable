// sgtc_wpn_goaregen.cpp — StargateTC CMedicxgoa (ref stc_i386.so @0xf67c8..0xf7044).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "soundent.h"
#include "sgtc_weapons.h"

LINK_ENTITY_TO_CLASS( weapon_goaregen, CMedicxgoa );

void CMedicxgoa::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_goaregen.mdl" );
	m_iId = SGTC_WPN_GOAREGEN;
	m_iClip = -1;
	m_iDefaultAmmo = 150;	// ref +0x1f8
	FallInit();
}

void CMedicxgoa::Precache( void )
{
	PRECACHE_MODEL( "models/v_goaregen.mdl" );
	PRECACHE_MODEL( "models/w_goaregen.mdl" );
	PRECACHE_MODEL( "models/p_goaregen.mdl" );
	PRECACHE_SOUND( "weapons/goaregen.wav" );
}

SGTC_GETITEM( CMedicxgoa, SGTC_AMMO_GOAREGEN, 150, WEAPON_NOCLIP, 3, 4, SGTC_WPN_GOAREGEN, 0, 0 )
SGTC_ADD_TO_PLAYER( CMedicxgoa )

BOOL CMedicxgoa::IsUseable( void )	// ref @0xf67c8 — always 1
{
	return TRUE;
}

BOOL CMedicxgoa::Deploy( void )	// ref @0xf699a — WeapDispSet(1), anim 2, "egon"
{
	return SGTC_DEPLOY( 1, "models/v_goaregen.mdl", "models/p_goaregen.mdl", 2, "egon" );
}

static void SGTC_GoaregenDlight( const Vector &vec )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vec );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( vec.x );
		WRITE_COORD( vec.y );
		WRITE_COORD( vec.z );
		WRITE_BYTE( 10 );	// ref radius byte (main-family 255,50,50)
		WRITE_BYTE( 255 );
		WRITE_BYTE( 50 );
		WRITE_BYTE( 50 );
		WRITE_BYTE( 7 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

void CMedicxgoa::PrimaryAttack( void )	// ref @0xf69f4
{
	if( m_pPlayer->m_flGateTravel > gpGlobals->time )	// ref player +0x408
		return;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 1 )
		return;

	if( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() )
		return;

	int iHeal = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];
	if( iHeal > 30 )	// ref cap 0x1e
		iHeal = 30;

	SendWeaponAnim( 1, 1, 0 );	// ref fires before trace
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->SnagInvis();

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192.0f;

	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr );

	SGTC_GoaregenDlight( vecSrc );
	SGTC_GoaregenDlight( tr.vecEndPos );

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/goaregen.wav", 1.0f, 0.8f );

	CBasePlayer *pTarget = (CBasePlayer *)CBaseEntity::Instance( tr.pHit );
	if( pTarget && pTarget->IsPlayer() && pTarget->IsAlive() )
	{
		BOOL bApplied = FALSE;
		if( pTarget->pev->team == m_pPlayer->pev->team )
		{
			if( pTarget->pev->health < 100.0f )
			{
				pTarget->m_iStamina = 100;
				pTarget->SGTC_UpdateStamina();
				pTarget->TakeHealth( (float)iHeal, DMG_GENERIC );
				pTarget->m_flSgtcLatch3fc = 0.0f;
				bApplied = TRUE;
			}
		}
		else if( pTarget->pev->health > 0.0f )
		{
			pTarget->m_iStamina = 0;
			pTarget->SGTC_UpdateStamina();
			pTarget->TakeDamage( pTarget->pev, m_pPlayer->pev, (float)iHeal, DMG_RADIATION );
			bApplied = TRUE;
		}

		if( bApplied )
		{
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= iHeal;
			if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0 )
				m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;
			m_pPlayer->m_iStamina -= iHeal / 2;
			if( m_pPlayer->m_iStamina < 1 )
				m_pPlayer->m_iStamina = 0;
			m_pPlayer->SGTC_UpdateStamina();
		}
	}

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.7f;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.7f;
}

void CMedicxgoa::SecondaryAttack( void )
{
}

void CMedicxgoa::WeaponIdle( void )	// ref @0xf6fcc
{
	if( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( 0, 1, 0 );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.1f;
}

void CMedicxgoa::Reload( void )
{
}
