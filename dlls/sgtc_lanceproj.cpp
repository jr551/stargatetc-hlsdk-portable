// sgtc_lanceproj.cpp — StargateTC energy-bolt projectiles (NEW file).
//
// Clean-room port of ref/stc_i386.so (STC 2.0). All behavior facts decoded
// from the reference binary; offsets cited inline.
//
// entvars offsets verified against the ref binary's own usage:
//   pev->dmg=0x1e0, pev->takedamage=0x16c, pev->owner=0x198,
//   pev->enemy=0x190, pev->movetype=0x108, pev->solid=0x10c, pev->model=0x6c,
//   pev->classname=0x0, pev->nextthink=0x104, pev->origin=0x8,
//   pev->velocity=0x20, pev->v_angle=0x50, pev->friction=0x11c.
// C++ members: this+0x134 m_pfnThink, this+0x13c m_pfnTouch,
//   this+0x140 m_pfnUse, this+0x128 m_flDie, this+0x130
//   m_fExplosionProtection, this+0xd5 m_fAttendExplosion.
//
// pev->dmg sources (STC-extended skilldata_t — stock field names mapped):
//   lance  gSkillData+0x118 -> batteryCapacity
//   big    gSkillData+0x120 -> healthkitCapacity
//   reetou gSkillData+0x124 -> scientistHeal

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "skill.h"
#include "sgtc_weapons.h"
#include "sgtc_monsters.h"
#include "sgtc_lanceproj.h"

// ref helper @0x1abc1c: componentwise int 3d distance.
static int SGTC_Endo_distance3d( const Vector &a, const Vector &b )
{
	return (int)fabs( a.x - b.x ) + (int)fabs( a.y - b.y ) +
		(int)fabs( a.z - b.z );
}

// ref helper @0xbf8e6 DoSpark: sparks + random spark sound at impact.
static void SGTC_DoSpark( entvars_t *pev, const Vector &pos )
{
	UTIL_Sparks( pos + pev->velocity * 0.5f );
	EMIT_SOUND( ENT( pev ), CHAN_BODY,
		UTIL_VarArgs( "buttons/spark%d.wav", RANDOM_LONG( 1, 5 ) ),
		0.8f, 0.25f );
}

int g_sModelIndexNarco = 0;

// ===========================================================================
// CCorpseController — "corpse_control"
// ===========================================================================

LINK_ENTITY_TO_CLASS( corpse_control, CCorpseController );

void CCorpseController::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;		// ref @0x14ef7a writes 5
	m_bArmed = FALSE;
	m_bBurn = FALSE;
	m_bFed = 0;	// ref +0x1a5 — set by CBasePlayer::Talent feed
	m_pCorpse = NULL;
	m_pEffect = NULL;
	pev->nextthink = gpGlobals->time + 0.1;
}

void CCorpseController::Precache( void )
{
	PRECACHE_SOUND( "common/bodysplat.wav" );
	PRECACHE_SOUND( "ambience/burning3.wav" );
}

// ref NewRound entity sweep (Ghidra 0x194f42) + Think @0x14efde +0x1b8 path
// (rea_dump/wpn/corpse.c:75-91): corpse pev reset, STOP_SOUND burning3, drop
// flame sprite, then UTIL_Remove the controller.
void CCorpseController::RemoveForNewRound( void )
{
	if( m_pCorpse )
	{
		m_pCorpse->pev->health = 0.0f;
		m_pCorpse->pev->deadflag = 0;
		m_pCorpse->pev->flags |= FL_KILLME;
		STOP_SOUND( ENT( m_pCorpse->pev ), CHAN_STATIC, "ambience/burning3.wav" );
	}
	if( m_pEffect )
		UTIL_Remove( m_pEffect );
	m_pEffect = NULL;
	UTIL_Remove( this );
}

void CCorpseController::Think( void )
{
	// ref @0x14efde: when armed, corpse_effects cvar on, and a corpse bound —
	// gib the corpse + remove the effect + remove self.
	if( m_bBurn && CVAR_GET_FLOAT( "corpse_effects" ) > 0.0f && m_pCorpse )
	{
		m_pCorpse->pev->health = 0.0f;
		m_pCorpse->pev->deadflag = DEAD_DEAD;
		m_pCorpse->pev->flags |= FL_KILLME;
		if( m_pEffect )
			UTIL_Remove( m_pEffect );
		UTIL_Remove( this );
		return;
	}
	pev->nextthink = gpGlobals->time + 0.1;
}

// ===========================================================================
// CSGTCProjBase — shared bolt skeleton
// ===========================================================================

void CSGTCProjBase::BaseSpawn( const char *pszModel, const char *pszClass,
	float flSpeed )
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	SET_MODEL( ENT( pev ), pszModel );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );
	pev->classname = MAKE_STRING( pszClass );

	SetThink( &CSGTCProjBase::EffectThink );
	SetUse( NULL );
	SetTouch( &CSGTCProjBase::LanceTouch );

	m_flDie = 0.0f;
	m_fAttendExplosion = FALSE;
	m_fExplosionProtection = FALSE;
	m_pWeapon = NULL;
	m_pOwner = NULL;
	m_pTarget = NULL;
	m_vecDir = g_vecZero;

	UTIL_MakeVectors( pev->v_angle );
	pev->velocity = pev->v_angle.Normalize() * flSpeed;	// ref uses v_angle
	pev->friction = 0.01f;
	m_vecSavedVel = pev->velocity;
	m_flSavedSpeed = pev->velocity.Length();
	pev->nextthink = gpGlobals->time + 0.001;
}

// ref CLanceProj::Accelere @0xfe3f8: at top speed add savedVel scaled by
// (savedSpeed/1000)*0.8, otherwise decay the bolt by 0.8.
void CSGTCProjBase::Accelere( void )
{
	if( pev->velocity.Length() == 2000.0f )
		pev->velocity = pev->velocity +
			m_vecSavedVel * ( m_flSavedSpeed / 1000.0f * 0.8f );
	else
		pev->velocity = pev->velocity * 0.8f;
}

// ref CLanceProj::Exploder @0xfe508 (big: 0xb8bfa, reetou: 0x151984 —
// same skeleton, per-class debris set + TE sprite).
void CSGTCProjBase::Exploder( TraceResult *pTrace, int bitsDamageType )
{
	pev->model = iStringNull;	// ref @0x10e508 zeroes pev+0xb8 (model)
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;

	if( pTrace->flFraction != 1.0 )
	{
		pev->origin = pTrace->vecEndPos +
			( pTrace->vecPlaneNormal * ( pev->dmg - 24.0f ) * 0.6f );
	}

	UTIL_PointContents( pev->origin );	// ref evaluates and discards

	entvars_t *pevOwner = NULL;
	if( pev->owner )
		pevOwner = VARS( pev->owner );
	pev->owner = NULL;

	// TE_SPRITE @ 0/0.5 — identical arg order in all three ref bodies.
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( g_sModelIndexNarco );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	if( RANDOM_FLOAT( 0, 1 ) < 0.5f )
		UTIL_DecalTrace( pTrace, 11 );	// ref pushes 0xb
	else
		UTIL_DecalTrace( pTrace, 12 );	// ref pushes 0xc

	// ref @0x10e508: (m_flDie <= gpGlobals->time || m_fAttendExplosion) &&
	// !m_fExplosionProtection -> RadiusDamage(pev, pevOwner, pev->dmg,
	// CLASS_NONE, bitsDamageType).
	if( ( m_flDie <= gpGlobals->time || m_fAttendExplosion ) &&
		!m_fExplosionProtection )
	{
		RadiusDamage( pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType );
	}

	// Ignite corpse_control entities within 29u (ref rolls RANDOM_LONG>6).
	if( BurnsCorpses() )
	{
		CBaseEntity *pCtrl = NULL;
		while( ( pCtrl = UTIL_FindEntityByClassname( pCtrl,
			"corpse_control" ) ) != NULL )
		{
			if( SGTC_Endo_distance3d( pev->origin, pCtrl->pev->origin ) <= 29 &&
				RANDOM_LONG( 0, 10 ) > 6 )
				((CCorpseController *)pCtrl)->m_bBurn = TRUE;
		}
	}

	EMIT_SOUND( ENT( pev ), CHAN_ITEM,
		DebrisSounds()[RANDOM_LONG( 0, 2 )], 0.8f, 0.9f );

	pev->effects |= EF_NODRAW;	// ref @0x10e508 tail: |0x80 then g_vecZero
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3f;
	SetThink( &CBaseEntity::SUB_Remove );
}

// ref CLanceProj::ProjTouch @0xfe9d0.
void CSGTCProjBase::ProjTouch( CBaseEntity *pOther )
{
	if( pOther == m_pOwner )
		return;

	TraceResult tr;
	pev->enemy = pOther->edict();
	Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32.0f;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64.0f,
		ignore_monsters, ENT( pev ), &tr );

	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
	// ref @0xfeb28 — IsPlayer() then byte +0xa3e = 1.
	if( pHit && pHit->IsPlayer() )
		((CBasePlayer *)pHit)->m_bLanceStunFlag = 1;

	SGTC_DoSpark( pev, tr.vecEndPos );
	Exploder( &tr, DMG_ALWAYSGIB );
}

// ref CGrenade::LanceTouch @0xf4662 — trace back then forward, DMG_BLAST.
void CSGTCProjBase::LanceTouch( CBaseEntity *pOther )
{
	TraceResult tr;
	pev->enemy = pOther->edict();
	Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32.0f;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64.0f,
		ignore_monsters, ENT( pev ), &tr );
	Explode( &tr, DMG_BLAST );
}

// ref CLanceProj::EffectThink @0xfeb9c.
void CSGTCProjBase::EffectThink( void )
{
	pev->flags |= FL_DUCKING;	// ref @0xfebc6: pev+0x118 |= 0x40
	pev->nextthink = gpGlobals->time + 0.1;

	UTIL_MakeVectors( pev->angles );
	Accelere();

	// Lead sweep: origin -> origin+v_forward*50 (dont_ignore); if something
	// is inside the lead distance, force a Touch.
	Vector vecStart = pev->origin;
	TraceResult tr;
	UTIL_TraceLine( vecStart, vecStart + gpGlobals->v_forward * 50.0f,
		dont_ignore_monsters, ENT( pev ), &tr );

	// ref @0x10eb9c: only touch when the trace hit closer than 50u lead
	// (vecEndPos vs origin+v_forward*50 via Endo_distance3d).
	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
	if( pHit && SGTC_Endo_distance3d( tr.vecEndPos,
		vecStart + gpGlobals->v_forward * 50.0f ) < 50 )
	{
		Touch( pHit );
	}

	// Kill nearby monster_shieldemit inside the z window (ref compares
	// bolt z vs emitter z within [100,200]).
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityInSphere( pEnt, pev->origin, 250.0f ) ) !=
		NULL )
	{
		if( !FClassnameIs( pEnt->pev, "monster_shieldemit" ) )
			continue;

		float dz = pev->origin.z - pEnt->pev->origin.z;	// bolt.z - emitter.z
		// ref @0x10ec63: emitter above bolt by >200 or bolt above emitter
		// by >100; then explode and set the EMITTER's nextthink = 1.0.
		if( ( pEnt->pev->origin.z - pev->origin.z > 200.0f ||
				dz > 100.0f ) && !m_fAttendExplosion )
		{
			m_fAttendExplosion = TRUE;
			m_fExplosionProtection = TRUE;
			pEnt->pev->nextthink = 1.0f;
			Exploder( &tr, DMG_NEVERGIB );
		}
	}

	// End-of-life (ref @0xfef18): gpGlobals->time < m_flDie && !m_fAttendExplosion
	// -> link owner's think + Exploder.
	if( gpGlobals->time < m_flDie && !m_fAttendExplosion )
	{
		if( pev->owner )
			VARS( pev->owner )->nextthink = 1.0f;
		Exploder( &tr, DMG_ALWAYSGIB );
		return;
	}

	pev->nextthink = gpGlobals->time + 0.008;
}

// ===========================================================================
// CLanceProj — "lance_proj" (spawn speed 320; turret 250; debris1-3)
// ===========================================================================

LINK_ENTITY_TO_CLASS( lance_proj, CLanceProj );

static const char *s_sgLanceDebris[3] = {
	"weapons/debris1.wav", "weapons/debris2.wav", "weapons/debris3.wav"
};

const char * const *CLanceProj::DebrisSounds( void ) { return s_sgLanceDebris; }

void CLanceProj::Spawn( void )
{
	BaseSpawn( "models/proj.mdl", "lance_proj", 320.0f );
	pev->dmg = gSkillData.batteryCapacity;	// ref: gSkillData+0x118
}

void CLanceProj::Precache( void )
{
	PRECACHE_MODEL( "models/proj.mdl" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
}

// ref @0x1000ce.
CLanceProj *CLanceProj::CreateTourrelleProj( Vector vecOrigin, Vector vecAngles )
{
	CLanceProj *pProj = GetClassPtr( (CLanceProj *)NULL );

	UTIL_SetOrigin( pProj->pev, vecOrigin + vecAngles * 20.0f );
	pProj->pev->v_angle = vecAngles;
	pProj->Spawn();
	pProj->pev->velocity = vecAngles * 250.0f;
	pProj->SetTouch( &CLanceProj::ProjTouch );
	pProj->m_pfnUse = NULL;
	return pProj;
}

// ref @0x1001b8.
CLanceProj *CLanceProj::CreateLanceProj( Vector vecStart, Vector vecVel,
	Vector vecTarget, CBaseEntity *pOwner, CLanceJaffa *pWeapon )
{
	CBasePlayer *pPlayer = pWeapon ? pWeapon->m_pPlayer : NULL;
	CLanceProj *pProj = GetClassPtr( (CLanceProj *)NULL );

	UTIL_SetOrigin( pProj->pev, vecStart );
	pProj->pev->v_angle = vecTarget - vecStart;	// ref stores raw delta
	pProj->Spawn();
	pProj->SetTouch( &CLanceProj::ProjTouch );
	pProj->m_pfnUse = NULL;
	pProj->m_pWeapon = pWeapon;
	pProj->pev->owner = pOwner ? pOwner->edict() : NULL;
	if( pOwner && pOwner->IsPlayer() )
		pProj->m_pOwner = pOwner;
	pProj->pev->velocity = pProj->pev->velocity + vecVel * 1.5f;
	pProj->m_vecDir = vecVel;
	pProj->pev->sequence = 4;
	pProj->pev->framerate = 0.0f;
	// ref @0x110358 tail: bolt arming window -> m_fAttendExplosion (this+0xd5),
	// then proj->pev->v_angle := player->pev->v_angle (v_angle copy).
	if( pPlayer && gpGlobals->time < pPlayer->m_flTalentCooldown )
		pProj->m_fAttendExplosion = TRUE;
	pProj->pev->v_angle = pPlayer->pev->v_angle;
	return pProj;
}

// ===========================================================================
// CBigLanceProj — "big_lance_proj" (dmg base 240, homing tail)
// ===========================================================================

LINK_ENTITY_TO_CLASS( big_lance_proj, CBigLanceProj );

static const char *s_sgBigLanceDebris[3] = {
	"weapons/explo1.wav", "weapons/explo2.wav", "weapons/explode3.wav"
};

const char * const *CBigLanceProj::DebrisSounds( void )
{
	return s_sgBigLanceDebris;
}

void CBigLanceProj::Spawn( void )
{
	BaseSpawn( "models/proj.mdl", "big_lance_proj", 320.0f );
	pev->dmg = gSkillData.healthkitCapacity;	// ref: gSkillData+0x120
}

void CBigLanceProj::Precache( void )
{
	PRECACHE_MODEL( "models/proj.mdl" );
	PRECACHE_SOUND( "weapons/explo1.wav" );
	PRECACHE_SOUND( "weapons/explo2.wav" );
	PRECACHE_SOUND( "weapons/explode3.wav" );
}

void CBigLanceProj::Accelere( void )
{
	CSGTCProjBase::Accelere();

	// Homing tail (ref @0xb8b28+): once slow (<300), steer the saved vector
	// toward the target at 1.5 gain.
	if( pev->velocity.Length() < 300.0f && m_pTarget )
	{
		m_vecSavedVel =
			( m_pTarget->pev->origin - pev->origin ).Normalize() *
			m_flSavedSpeed * 1.5f;
	}
}

CBigLanceProj *CBigLanceProj::CreateTourrelleProj( Vector vecOrigin,
	Vector vecAngles )
{
	CBigLanceProj *pProj = GetClassPtr( (CBigLanceProj *)NULL );

	UTIL_SetOrigin( pProj->pev, vecOrigin + vecAngles * 20.0f );
	pProj->pev->v_angle = vecAngles;
	pProj->Spawn();
	pProj->pev->velocity = vecAngles * 250.0f;
	pProj->SetTouch( &CBigLanceProj::ProjTouch );
	pProj->m_pfnUse = NULL;
	return pProj;
}

// ref @0xb9e30.
CBigLanceProj *CBigLanceProj::CreateBigLanceProj( Vector vecStart,
	Vector vecVel, Vector vecTarget, CBaseEntity *pOwner,
	CBigLanceJaffa *pWeapon )
{
	CBigLanceProj *pProj = GetClassPtr( (CBigLanceProj *)NULL );

	UTIL_SetOrigin( pProj->pev, vecStart );
	pProj->pev->v_angle = vecTarget - vecStart;
	pProj->Spawn();
	pProj->SetTouch( &CBigLanceProj::ProjTouch );
	pProj->m_pfnUse = NULL;
	pProj->m_pWeapon = pWeapon;
	pProj->pev->owner = pOwner ? pOwner->edict() : NULL;
	if( pOwner && pOwner->IsPlayer() )
		pProj->m_pOwner = pOwner;
	pProj->pev->velocity = pProj->pev->velocity + vecVel * 1.5f;
	pProj->m_vecDir = vecVel;
	pProj->pev->sequence = 4;
	pProj->pev->framerate = 0.0f;
	pProj->pev->dmg = pProj->pev->dmg * 2.0f;	// ref @0xb9fa7 doubles the field
	if( pOwner && pOwner->pev->deadflag != DEAD_NO )
		pProj->m_fAttendExplosion = TRUE;
	return pProj;
}

// ===========================================================================
// CReetouProj — "reetou_proj" (speed 1600, narco trail)
// ===========================================================================

LINK_ENTITY_TO_CLASS( reetou_proj, CReetouProj );

static const char *s_sgReetouDebris[3] = {
	"weapons/terblast1.wav", "weapons/terblast2.wav", "weapons/terblast3.wav"
};

const char * const *CReetouProj::DebrisSounds( void )
{
	return s_sgReetouDebris;
}

void CReetouProj::Spawn( void )
{
	BaseSpawn( "models/w_projreetou.mdl", "reetou_proj", 1600.0f );
	SetTouch( &CReetouProj::ProjTouch );
	m_flNarcoNext = 0.0f;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmg = gSkillData.scientistHeal;	// ref: gSkillData+0x124
	pev->sequence = 4;
	pev->body = 19;
	pev->skin = 120;
	pev->rendermode = kRenderNormal;
	pev->renderamt = 255.0f;
	pev->scale = 50.0f;
}

void CReetouProj::Precache( void )
{
	PRECACHE_MODEL( "models/w_projreetou.mdl" );
	PRECACHE_MODEL( "sprites/xfireball3.spr" );
	g_sModelIndexNarco = PRECACHE_MODEL( "sprites/xfireball3.spr" );
	PRECACHE_SOUND( "weapons/terblast1.wav" );
	PRECACHE_SOUND( "weapons/terblast2.wav" );
	PRECACHE_SOUND( "weapons/terblast3.wav" );
}

void CReetouProj::Explodera( TraceResult *pTrace, int bitsDamageType )
{
	Exploder( pTrace, bitsDamageType );
}

void CReetouProj::EffectThink( void )
{
	// Narco-sprite trail every 0.1s (ref @0x151fe0-0x152143):
	// TE_SPRITE at origin, scale=(dmg-50)*0.7, brightness=12.
	if( m_flNarcoNext < gpGlobals->time )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( g_sModelIndexNarco );
			WRITE_BYTE( (int)( ( pev->dmg - 50.0f ) * 0.7f ) );
			WRITE_BYTE( 12 );
		MESSAGE_END();
		m_flNarcoNext = gpGlobals->time + 0.1;
	}

	// ref @0x152143: end-of-life — owner link + explode, 0.008s re-think.
	if( gpGlobals->time < m_flDie && !m_fAttendExplosion )
	{
		if( pev->owner )
			VARS( pev->owner )->nextthink = 1.0f;
		TraceResult tr;
		UTIL_MakeVectors( pev->angles );
		UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward,
			dont_ignore_monsters, ENT( pev ), &tr );
		Explodera( &tr, DMG_NEVERGIB );
	}

	pev->nextthink = gpGlobals->time + 0.008;
}

// ref @0x152e32.
CReetouProj *CReetouProj::CreateReetouProj( Vector vecStart, Vector vecAngles,
	CBaseEntity *pOwner, CReetou *pWeapon )
{
	CReetouProj *pProj = GetClassPtr( (CReetouProj *)NULL );

	UTIL_SetOrigin( pProj->pev, vecStart );
	pProj->pev->v_angle = vecAngles;
	pProj->Spawn();
	pProj->m_pfnUse = NULL;
	pProj->m_pWeapon = (CBaseEntity *)pWeapon;
	pProj->pev->owner = pOwner ? pOwner->edict() : NULL;
	if( pOwner && pOwner->IsPlayer() )
		pProj->m_pOwner = pOwner;	// ref sets this+0x3f0 only when IsPlayer()
	pProj->pev->velocity = vecAngles * 1600.0f;
	// ref @0x162e32 tail: owner arming window -> m_fAttendExplosion.
	if( pOwner && gpGlobals->time < pOwner->pev->fuser4 )
		pProj->m_fAttendExplosion = TRUE;
	pProj->pev->nextthink = gpGlobals->time + 0.15;
	return pProj;
}
