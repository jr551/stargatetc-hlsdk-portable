// sgtc_wpn_coujaf.cpp — StargateTC CCoujaf (split from sgtc_weapons.cpp; NEW file).
// Ref: CCoujaf @ ref/stc_i386.so (PrimaryAttack @0xd54e2, Swing @0xd4962).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "soundent.h"
#include "sgtc_weapons.h"

extern void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity );

// Ref .rodata (coujaf PrimaryAttack / Swing).
static const float kCoujafSnagReleaseDelay = 0.5f;	// ref +0x37270
static const float kCoujafSwingMissDelay = 0.5f;	// ref +0x372ac
static const float kCoujafSwingHitDelay = 0.28f;	// ref +0x37278 qword
static const float kCoujafThinkDelay = 0.1f;		// ref +0x37248 qword
static const float kCoujafCorpseSpeedGate = 1000.0f;	// ref +0x37268
static const float kCoujafMeleeRange = 32.0f;		// ref forward trace scale

LINK_ENTITY_TO_CLASS( weapon_coujaf, CCoujaf );

void CCoujaf::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_coujaf.mdl" );
	m_iId = SGTC_WPN_COUJAF;
	m_iClip = -1;
	m_iSwing = 0;
	m_hSnagTarget = NULL;
	FallInit();
}

void CCoujaf::Precache( void )
{
	PRECACHE_MODEL( "models/v_coujaf.mdl" );
	PRECACHE_MODEL( "models/w_coujaf.mdl" );
	PRECACHE_MODEL( "models/p_coujaf.mdl" );
	PRECACHE_MODEL( "sprites/shockwave.spr" );

	PRECACHE_SOUND( "weapons/nightstrike.wav" );
	PRECACHE_SOUND( "weapons/coujafcorps.wav" );
	PRECACHE_SOUND( "weapons/coujafmur.wav" );
	PRECACHE_SOUND( "weapons/coujafvide.wav" );
	PRECACHE_SOUND( "weapons/cbar_hit1.wav" );
	PRECACHE_SOUND( "weapons/cbar_hit2.wav" );
	PRECACHE_SOUND( "weapons/hitbody1.wav" );
	PRECACHE_SOUND( "weapons/hitbody2.wav" );
	PRECACHE_SOUND( "weapons/hitbody3.wav" );
	PRECACHE_SOUND( "weapons/swing1.wav" );
	PRECACHE_SOUND( "weapons/swing2.wav" );
	PRECACHE_SOUND( "weapons/swing3.wav" );

	m_usFire = PRECACHE_EVENT( 1, "events/coujaf.sc" );
}

SGTC_GETITEM( CCoujaf, NULL, -1, WEAPON_NOCLIP, 0, 0, SGTC_WPN_COUJAF, 0, 0 )
SGTC_ADD_TO_PLAYER( CCoujaf )

BOOL CCoujaf::Deploy( void )
{
	return SGTC_DEPLOY( 2, "models/v_coujaf.mdl", "models/p_coujaf.mdl", SGTC_DRAW, "crowbar" );
}

void CCoujaf::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;
	SendWeaponAnim( SGTC_HOLSTER );
}

#ifndef CLIENT_DLL
static void SGTC_CoujafCorpseFeed( CBasePlayer *pAttacker, CBasePlayer *pVictim )
{
	// ref CCoujaf::PrimaryAttack @0xd571e — same-team living target with pev+0x210 > 1000.
	pVictim->m_iStamina = 100;
	pVictim->SGTC_UpdateStamina();
	pVictim->m_flSnagArmorRamp = 100.0f;
	pVictim->m_iSnagArmorCap = 100;
	pVictim->pev->maxspeed = kCoujafCorpseSpeedGate;
	pAttacker->m_flSnagArmorRamp = 100.0f;
	pAttacker->m_iSnagArmorCap = 100;
	UTIL_ScreenFade( pVictim, g_vecZero, 10.0f, 10.0f, 0, 0 );
	EMIT_SOUND( ENT( pAttacker->pev ), CHAN_WEAPON, "weapons/coujafcorps.wav", 0.8f, ATTN_NORM );
}
#endif

void CCoujaf::PrimaryAttack( void )
{
#ifndef CLIENT_DLL
	if( m_pPlayer->IsObserver() )
		return;

	if( m_pPlayer->m_bShieldLarge || m_pPlayer->m_bHurtInvertFx )
	{
		if( !Swing( 1 ) && m_flNextPrimaryAttack <= UTIL_WeaponTimeBase() )
		{
			SetThink( &CCoujaf::SwingAgain );
			pev->nextthink = gpGlobals->time + kCoujafThinkDelay;
		}
		return;
	}

	if( m_hSnagTarget != NULL )
	{
		m_hSnagTarget = NULL;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + kCoujafSnagReleaseDelay;
		m_flNextPrimaryAttack = GetNextAttackDelay( kCoujafSnagReleaseDelay );
		return;
	}

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * kCoujafMeleeRange;
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
	CBasePlayer *pVictim = ( pHit && pHit->IsPlayer() ) ? (CBasePlayer *)pHit : NULL;
	if( pVictim && pVictim->IsAlive() )
	{
		if( pVictim->pev->team != m_pPlayer->pev->team )
		{
			m_hSnagTarget = pVictim;
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + kCoujafSnagReleaseDelay;
			m_flNextPrimaryAttack = GetNextAttackDelay( kCoujafSnagReleaseDelay );
			return;
		}

		if( pVictim->pev->maxspeed > kCoujafCorpseSpeedGate )
		{
			SGTC_CoujafCorpseFeed( m_pPlayer, pVictim );
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + kCoujafSnagReleaseDelay;
			m_flNextPrimaryAttack = GetNextAttackDelay( kCoujafSnagReleaseDelay );
			return;
		}
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + kCoujafSnagReleaseDelay;
	m_flNextPrimaryAttack = GetNextAttackDelay( kCoujafSnagReleaseDelay );

	if( !Swing( 1 ) && m_flNextPrimaryAttack <= UTIL_WeaponTimeBase() )
	{
		SetThink( &CCoujaf::SwingAgain );
		pev->nextthink = gpGlobals->time + kCoujafThinkDelay;
	}
#endif
}

void CCoujaf::SwingAgain( void )
{
	Swing( 0 );
}

void CCoujaf::Smack( void )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

int CCoujaf::Swing( int fFirst )
{
	int fDidHit = FALSE;
	TraceResult tr;

	if( m_iSwing == 1 && m_pPlayer->m_bShieldSprint )
		m_iSwing = 0;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * kCoujafMeleeRange;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if( tr.flFraction >= 1.0f )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if( tr.flFraction < 1.0f )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
		}
	}
#endif

	if( fFirst )
	{
		PLAYBACK_EVENT_FULL( SGTC_EVENT_FLAGS, m_pPlayer->edict(), m_usFire,
			0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0, 0, 0 );
	}

	if( tr.flFraction >= 1.0f )
	{
		if( fFirst )
		{
			m_flNextPrimaryAttack = GetNextAttackDelay( kCoujafSwingMissDelay );
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if( pEntity )
		{
			ClearMultiDamage();
			float flDamage = gSkillData.plrDmgCrowbar;
			if( m_iSwing == 1 )
				flDamage *= 0.5f;
			pEntity->TraceAttack( m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

			if( pEntity->IsPlayer() )
			{
				CBasePlayer *pOther = (CBasePlayer *)pEntity;
				if( pOther->IsAlive() && pOther->pev->team != m_pPlayer->pev->team )
				{
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/coujafcorps.wav", 0.8f, ATTN_NORM );
					pOther->pev->renderamt = 0x80;
				}
			}

			if( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				const char *pszHit = "weapons/hitbody2.wav";
				switch( RANDOM_LONG( 0, 2 ) )
				{
				case 0: pszHit = "weapons/hitbody1.wav"; break;
				case 1: pszHit = "weapons/hitbody2.wav"; break;
				case 2: pszHit = "weapons/hitbody3.wav"; break;
				}
				EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, pszHit, 1, ATTN_NORM );
			}
			else
			{
				switch( RANDOM_LONG( 0, 1 ) )
				{
				case 0:
					EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/cbar_hit1.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
					break;
				case 1:
					EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/cbar_hit2.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
					break;
				}
			}
		}

		m_iSwing = 0;
		if( fFirst )
			m_flNextPrimaryAttack = GetNextAttackDelay( kCoujafSwingHitDelay );

		m_trHit = tr;
		Smack();
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + kCoujafSwingHitDelay;
#endif
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() +
		UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );

	return fDidHit;
}

SGTC_NO_SECONDARY( CCoujaf )
SGTC_NO_RELOAD( CCoujaf )
SGTC_WEAPON_IDLE( CCoujaf )
