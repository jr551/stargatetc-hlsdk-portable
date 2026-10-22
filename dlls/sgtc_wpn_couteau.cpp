// sgtc_wpn_couteau.cpp — StargateTC CCouteau (split from sgtc_weapons.cpp; NEW file).

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
// CCouteau - "Couteau" throwing knife. Analogue: crowbar.cpp + thrown item.
// Event: events/couteau.sc. Secondary throws the knife (ThrowIt).
// Ref: CCouteau::Swing @0xd70ec, Throwit @0xd65b2, Smack @0xd6ee4 (empty).
// ===========================================================================

LINK_ENTITY_TO_CLASS( weapon_couteau, CCouteau );

static void sgtcHullIntersect( const Vector &vecSrc, TraceResult &tr,
	const float *mins, const float *maxs, edict_t *pEntity )
{
	TraceResult tmpTrace;
	Vector vecHullEnd = tr.vecEndPos;
	Vector vecEnd = vecSrc + ( vecHullEnd - vecSrc ) * 2.0f;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if( tmpTrace.flFraction < 1.0f )
		tr = tmpTrace;
}

void CCouteau::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_couteau.mdl" );
	m_iId = SGTC_WPN_COUTEAU;
	m_iClip = -1;
	FallInit();
}

void CCouteau::Precache( void )
{
	PRECACHE_MODEL( "models/v_couteau.mdl" );
	PRECACHE_MODEL( "models/w_couteau.mdl" );
	PRECACHE_MODEL( "models/p_couteau.mdl" );

	PRECACHE_SOUND( "weapons/cbar_hit1.wav" );
	PRECACHE_SOUND( "weapons/cbar_hit2.wav" );
	PRECACHE_SOUND( "weapons/hitbody1.wav" );
	PRECACHE_SOUND( "weapons/hitbody2.wav" );
	PRECACHE_SOUND( "weapons/hitbody3.wav" );
	PRECACHE_SOUND( "weapons/swing1.wav" );
	PRECACHE_SOUND( "weapons/swing2.wav" );
	PRECACHE_SOUND( "weapons/swing3.wav" );

	m_usFire = PRECACHE_EVENT( 1, "events/couteau.sc" );
}

SGTC_GETITEM( CCouteau, NULL, 3, 0, 0, 1, SGTC_WPN_COUTEAU, 0, 0 )
SGTC_ADD_TO_PLAYER( CCouteau )

BOOL CCouteau::Deploy( void )
{
	return SGTC_DEPLOY( 2, "models/v_couteau.mdl", "models/p_couteau.mdl", SGTC_DRAW, "crowbar" );
}

void CCouteau::Smack( void )
{
	// ref @0xd6ee4 — immediate return (no DecalGunshot).
}

void CCouteau::PrimaryAttack( void )
{
	if( m_pPlayer->pev->waterlevel == 3 )
		return;
	if( m_pPlayer->IsObserver() )
		return;

	if( !Swing( 1 ) )
	{
#ifndef CLIENT_DLL
		SetThink( &CCouteau::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
#endif
	}
}

void CCouteau::SwingAgain( void )
{
	Swing( 0 );
}

int CCouteau::Swing( int fFirst )
{
	int fDidHit = FALSE;
	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if( tr.flFraction < 1.0 )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if( !pHit || pHit->IsBSPModel() )
				sgtcHullIntersect( vecSrc, tr, (float *)VEC_DUCK_HULL_MIN,
					(float *)VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;
		}
	}
#endif

	if( fFirst )
	{
		PLAYBACK_EVENT_FULL( SGTC_EVENT_FLAGS, m_pPlayer->edict(), m_usFire,
			0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0, 0, 0 );
	}

	if( tr.flFraction >= 1.0 )
	{
		if( fFirst )
		{
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.4 );
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
			if( ( m_flNextPrimaryAttack + 1.0f < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.plrDmgCrowbar, gpGlobals->v_forward, &tr, DMG_CLUB );
			else
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.plrDmgCrowbar * 0.5f, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

			if( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				switch( RANDOM_LONG( 0, 2 ) )
				{
				case 0:
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/hitbody1.wav", 1, ATTN_NORM );
					break;
				case 1:
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/hitbody2.wav", 1, ATTN_NORM );
					break;
				default:
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/hitbody3.wav", 1, ATTN_NORM );
					break;
				}
			}
		}

		m_trHit = tr;
		Smack();
#endif

		if( fFirst )
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.4 );
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() +
		UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );

	return fDidHit;
}

void CCouteau::SecondaryAttack( void )
{
	if( m_pPlayer->pev->waterlevel == 3 )
		return;
	if( m_pPlayer->IsObserver() )
		return;
	if( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() )
		return;
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	ThrowIt();
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
}

void CCouteau::ThrowIt( void )
{
#ifndef CLIENT_DLL
	m_pPlayer->SnagInvis();

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 2048;
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	Vector vecSpawn = vecSrc + gpGlobals->v_forward * 16;
	if( tr.flFraction < 1.0f )
		vecSpawn = tr.vecEndPos;

	CSGTCBolt *pBolt = CSGTCBolt::CreateBolt( vecSpawn, m_pPlayer->pev->v_angle, m_pPlayer,
		20.0f, "models/w_couteau.mdl" );
	if( pBolt )
		pBolt->pev->velocity = gpGlobals->v_forward * 1200 + m_pPlayer->pev->velocity;
#endif
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
}

SGTC_NO_RELOAD( CCouteau )
SGTC_WEAPON_IDLE( CCouteau )
