// sgtc_wpn_invis.cpp — StargateTC CInvis (split from sgtc_weapons.cpp; NEW file).
// Personal cloak + personal shield wall (ref/stc_i386.so invis.c @0x10b7a0..0x10c154).
// weapon_shield is NOT in the ref link table; shield arms via SecondaryAttack/WeaponIdle here.

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
// CInvis — events/invis.sc (cloak) / events/shield.sc (shield wall FX hook).
// ===========================================================================

LINK_ENTITY_TO_CLASS( weapon_invis, CInvis );

void CInvis::Spawn( void )
{
	m_bInvisActive = 0;
	Precache();
	SET_MODEL( ENT( pev ), "models/w_invis.mdl" );
	m_iId = SGTC_WPN_INVIS;
	m_iClip = -1;
	m_iDefaultAmmo = 10;
	m_flInvisOnTime = 0.0f;
	m_flShieldLoadTime = 0.0f;
	FallInit();
}

void CInvis::Precache( void )
{
	PRECACHE_MODEL( "models/w_invis.mdl" );
	PRECACHE_MODEL( "models/v_invis.mdl" );
	PRECACHE_MODEL( "models/p_invis.mdl" );
	PRECACHE_SOUND( "weapons/invis.wav" );
	PRECACHE_SOUND( "weapons/empty.wav" );
	PRECACHE_SOUND( "forcefield/forcefield_fail.wav" );
	PRECACHE_SOUND( "forcefield/forcefield_loading.wav" );
	PRECACHE_SOUND( "forcefield/forcefield_unloading.wav" );

	m_usInvisFire = PRECACHE_EVENT( 1, "events/invis.sc" );
	m_usShieldFire = PRECACHE_EVENT( 1, "events/shield.sc" );
}

int CInvis::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = SGTC_AMMO_INVIS;
	p->iMaxAmmo1 = 10;
	p->pszAmmo2 = SGTC_AMMO_INVIS;
	p->iMaxAmmo2 = 10;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iFlags = 5;
	p->iId = m_iId = SGTC_WPN_INVIS;
	p->iWeight = -10;
	return 1;
}

SGTC_ADD_TO_PLAYER( CInvis )

BOOL CInvis::Deploy( void )
{
	m_pPlayer->SGTC_WeapDispSet( 2 );
	m_bInvisActive = 0;
	m_flShieldLoadTime = 0.0f;
	return DefaultDeploy( "models/v_invis.mdl", "models/p_invis.mdl", SGTC_DRAW, "python" );
}

void CInvis::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.3f;
	if( m_bInvisActive )
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/invis.wav", 1.0, 0.8 );

	m_bInvisActive = 0;
	SetThink( NULL );
	m_flShieldLoadTime = 0.0f;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 )
		pev->nextthink = gpGlobals->time + 0.1f;

	CBasePlayerWeapon::Holster( skiplocal );
}

// ref @0x10bb62 — blocked when shield wall up or gond state == 2; ammo > 3.
void CInvis::PrimaryAttack( void )
{
	if( !m_pPlayer )
		return;
	if( m_pPlayer->m_bShieldWall )
		return;
	if( m_pPlayer->m_iSgtcGondState == 2 )
		return;
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 3 )
		return;

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/invis.wav", 1.0, 0.8 );

	m_bInvisActive ^= 1;
	m_pPlayer->SGTC_ToggleVis();

	if( m_bInvisActive )
	{
		m_flInvisOnTime = gpGlobals->time;
		SetThink( &CInvis::InvisThink );
		pev->nextthink = gpGlobals->time + 0.1f;
	}
	else
	{
		m_flInvisOnTime = 0.0f;
		SetThink( NULL );
	}

	PLAYBACK_EVENT_FULL( SGTC_EVENT_FLAGS, m_pPlayer->edict(), m_usInvisFire,
		0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0, 0, 0 );

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_flTimeWeaponIdle = gpGlobals->time;
	m_pPlayer->m_flNextAttack = GetNextAttackDelay( 0.5f );
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
}

// ref @0x10bd92 — shield wall raise/lower (not cloak).
void CInvis::SecondaryAttack( void )
{
	if( !m_pPlayer )
		return;
	if( m_bInvisActive )
		return;
	if( gpGlobals->time < m_pPlayer->m_flGateTravel && !m_pPlayer->IsOnLadder() )
		return;
	if( m_pPlayer->m_iSgtcGondState == 1 )
		return;
	if( m_pPlayer->m_flNextAttack > UTIL_WeaponTimeBase() )
		return;

	if( !m_pPlayer->m_bShieldWall )
	{
		if( ( ( m_pPlayer->pev->flags >> 9 ) & 1 ) == 1 )
		{
			if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 2 &&
				m_pPlayer->m_flShieldLatch < gpGlobals->time )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "forcefield/forcefield_loading.wav", 1.0, 0.8 );
				m_flShieldLoadTime = gpGlobals->time;
				m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
			}
		}
		else
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "forcefield/forcefield_fail.wav", 1.0, 0.8 );
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "forcefield/forcefield_unloading.wav", 1.0, 0.8 );
		m_pPlayer->m_bShieldWall = 0;
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase();
	m_pPlayer->m_flNextAttack = GetNextAttackDelay( 1.0f );
}

void CInvis::InvisThink( void )
{
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 1 )
	{
		m_bInvisActive = 0;
		m_pPlayer->SGTC_ToggleVis();
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/empty.wav", 0.8, 0.8 );
		RetireWeapon();
		m_pPlayer->m_flNextAttack = GetNextAttackDelay( 1.0f );
		m_flTimeWeaponIdle = gpGlobals->time;
		SetThink( NULL );
		return;
	}
	pev->nextthink = gpGlobals->time + 0.1f;
}

// ref @0x10c154 — after 0.5s loading latch, set player +0x3e0 (m_bShieldWall).
void CInvis::WeaponIdle( void )
{
	if( !m_pPlayer )
		return;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = m_pPlayer->m_iShieldCharge;	// ref +0x158

	if( m_flShieldLoadTime != 0.0f && m_flShieldLoadTime < gpGlobals->time - 0.5f )
	{
		m_pPlayer->m_bShieldWall = 1;
		m_flShieldLoadTime = 0.0f;
		PLAYBACK_EVENT_FULL( SGTC_EVENT_FLAGS, m_pPlayer->edict(), m_usShieldFire,
			0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0, 0, 0 );
	}

	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	if( RANDOM_FLOAT( 0.0f, 1.0f ) <= 0.75f )
	{
		iAnim = SGTC_IDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10.0f, 15.0f );
	}
	else
	{
		iAnim = 3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5f;
	}

	SendWeaponAnim( iAnim );
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = m_pPlayer->m_iShieldCharge;	// ref pev ammo <- +0x158
	pev->nextthink = gpGlobals->time + 0.25f;
}

SGTC_NO_RELOAD( CInvis )
