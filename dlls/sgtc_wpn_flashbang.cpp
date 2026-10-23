// sgtc_wpn_flashbang.cpp — StargateTC CFlashBang (ref stc_i386.so @0xe6078).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "soundent.h"
#include "sgtc_weapons.h"

// ref client / server anim indices (grenade.mdl).
enum flashbang_e
{
	FLASH_IDLE = 0,
	FLASH_PINPULL = 2,
	FLASH_THROW1 = 3,
	FLASH_THROW2 = 4,
	FLASH_THROW3 = 5,
	FLASH_HOLSTER = 6,
	FLASH_DRAW = 7,
};

LINK_ENTITY_TO_CLASS( weapon_flashbang, CFlashBang );

void CFlashBang::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_grenade.mdl" );
	m_iId = SGTC_WPN_FLASHBANG;
	m_iClip = -1;
	m_iDefaultAmmo = 4;
	FallInit();
}

void CFlashBang::Precache( void )
{
	PRECACHE_MODEL( "models/w_grenade.mdl" );
	PRECACHE_MODEL( "models/v_grenade.mdl" );
	PRECACHE_MODEL( "models/p_grenade.mdl" );
	PRECACHE_SOUND( "weapons/flashbang.wav" );
	PRECACHE_SOUND( "common/null.wav" );
}

SGTC_GETITEM( CFlashBang, SGTC_AMMO_FLASH, 10, WEAPON_NOCLIP, 5, 5, SGTC_WPN_FLASHBANG, 24, 10 )
SGTC_ADD_TO_PLAYER( CFlashBang )

BOOL CFlashBang::Deploy( void )
{
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	return SGTC_DEPLOY( 2, "models/v_grenade.mdl", "models/p_grenade.mdl", FLASH_DRAW, "crowbar" );
}

BOOL CFlashBang::CanHolster( void )
{
	return ( m_flStartThrow == 0 && m_flNextPrimaryAttack <= UTIL_WeaponTimeBase() );
}

void CFlashBang::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		SendWeaponAnim( FLASH_HOLSTER );
	}
	else
	{
		m_pPlayer->pev->weapons &= ~( 1 << SGTC_WPN_FLASHBANG );
		DestroyItem();
	}

	if( m_flStartThrow )
	{
		m_flStartThrow = 0.0f;
		m_flReleaseThrow = 0.0f;
	}

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "common/null.wav", 1.0f, ATTN_NORM );
}

void CFlashBang::PrimaryAttack( void )
{
	if( gpGlobals->time < m_pPlayer->m_flGateTravel && !m_pPlayer->IsOnLadder() )
		return;
	if( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() )
		return;
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	m_flStartThrow = gpGlobals->time;
	m_flReleaseThrow = 0.0f;
	m_iClip = 0;

	SendWeaponAnim( FLASH_PINPULL );
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5f;
	m_pPlayer->SnagInvis();
}

void CFlashBang::WeaponIdle( void )
{
	if( m_flReleaseThrow == 0.0f && m_flStartThrow )
		m_flReleaseThrow = gpGlobals->time;

	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if( m_flStartThrow )
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
		if( angThrow.x < 0.0f )
			angThrow.x = -10.0f + angThrow.x * ( ( 90.0f - 10.0f ) / 90.0f );
		else
			angThrow.x = -10.0f + angThrow.x * ( ( 90.0f + 10.0f ) / 90.0f );

		float flVel = ( 90.0f - angThrow.x ) * 6.5f;
		if( flVel > 1000.0f )
			flVel = 1000.0f;

		UTIL_MakeVectors( angThrow );
		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16.0f;
		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		float time = m_flStartThrow - gpGlobals->time + 2.0f;
		if( time < 0.0f )
			time = 0.0f;

		int iSeed = m_pPlayer->random_seed;
#ifndef CLIENT_DLL
		if( flVel < 500.0f )
			CGrenade::LaunchSlideFlash( m_pPlayer->pev, vecSrc, vecThrow, time, iSeed );
		else
			CGrenade::ShootFlash( m_pPlayer->pev, vecSrc, vecThrow, time, iSeed );
#endif

		if( flVel < 500.0f )
			SendWeaponAnim( FLASH_THROW1 );
		else if( flVel < 1000.0f )
			SendWeaponAnim( FLASH_THROW2 );
		else
			SendWeaponAnim( FLASH_THROW3 );

		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_flStartThrow = 0.0f;
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5f;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
		return;
	}

	if( m_flReleaseThrow > 0.0f )
	{
		m_flStartThrow = 0.0f;
		if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			SendWeaponAnim( FLASH_DRAW );
		else
		{
			RetireWeapon();
			return;
		}
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10.0f, 15.0f );
		m_flReleaseThrow = -1.0f;
		return;
	}

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0f, 1.0f );
		if( flRand <= 0.75f )
		{
			iAnim = FLASH_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10.0f, 15.0f );
		}
		else
		{
			iAnim = FLASH_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5f;
		}
		SendWeaponAnim( iAnim );
	}
}

SGTC_NO_SECONDARY( CFlashBang )
SGTC_NO_RELOAD( CFlashBang )
