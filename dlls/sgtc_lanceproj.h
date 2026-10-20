// sgtc_lanceproj.h — StargateTC energy-bolt projectiles (NEW file).
//
// Clean-room port of ref/stc_i386.so (STC 2.0):
//   CLanceProj    : class "lance_proj"      — staff-blast bolt (big_lance.cpp).
//   CBigLanceProj : class "big_lance_proj"  — deathglider cannon bolt.
//   CReetouProj   : class "reetou_proj"     — reetou energy bolt.
//   CCorpseController : class "corpse_control" — ignites corpses near blasts.
//
// All three bolts share one skeleton (CSGTCProjBase): CGrenade-derived,
// MOVETYPE_BOUNCE / SOLID_BBOX, zero-size, EffectThink accelerates the bolt
// to 2000 and sweeps a lead trace, kills nearby "monster_shieldemit", and
// explodes with DMG_ALWAYSGIB. The real mod adds CGrenade::LanceTouch —
// the stock base has no such member, so each bolt declares its own
// LanceTouch mirroring ref CGrenade::LanceTouch @0xf4662.
//
// Disasm references (ref/stc_i386.so):
//   CLanceProj::{Spawn 0xfe1c0, Accelere 0xfe3f8, Exploder 0xfe508,
//     ProjTouch 0xfe9d0, EffectThink 0xfeb9c, Precache 0xfefb4,
//     CreateTourrelleProj 0x1000ce, CreateLanceProj 0x1001b8}
//   CGrenade::LanceTouch 0xf4662; DoSpark 0xbf8e6;
//   CCorpseController::{Spawn 0x14ef3c, Precache 0x14efae, Think 0x14efde}
//   CBigLanceProj::{Spawn 0xb8804, Accelere 0xb8a36, Exploder 0xb8bfa,
//     ProjTouch 0xb91ae, EffectThink 0xb935e, Precache 0xb976a,
//     CreateTourrelleProj 0xb9d46, CreateBigLanceProj 0xb9e30}
//   CReetouProj::{Spawn 0x15170c, Explodera 0x151984, ProjTouch 0x151e70,
//     EffectThink 0x151fd2, Precache 0x152272, CreateReetouProj 0x152e32}

#ifndef SGTC_LANCEPROJ_H
#define SGTC_LANCEPROJ_H

class CLanceJaffa;
class CBigLanceJaffa;
class CReetou;

// ---------------------------------------------------------------------------
// CCorpseController — "corpse_control". Thinks every 0.1s; when a bolt
// Exploder has set m_bBurn it gibs its corpse + removes the effect entity
// and itself. Ref members: +0x19c corpse, +0x1a0 effect, +0x1a5 armed,
// +0x1a6 burn; cvar corpse_effects (ref D @0x2135c4) gates the burn.
// ---------------------------------------------------------------------------
class CCorpseController : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT Think( void );
	void RemoveForNewRound( void );	// ref NewRound sweep — inner corpse/effect + STOP_SOUND

	CBaseEntity *m_pCorpse;	// ref +0x19c — bound corpse (feed gate)
	CBaseEntity *m_pEffect;	// ref +0x1b8 — active effect ent (blocks feed)
	BOOL m_bArmed;			// ref +0x1a4
	char m_bFed;			// ref +0x1a5 — set by CBasePlayer::Talent feed
	BOOL m_bBurn;			// ref +0x1a7
};

// ---------------------------------------------------------------------------
// CSGTCProjBase — shared bolt skeleton for lance_proj / big_lance_proj /
// reetou_proj. Members decode the ref fields used by the shared bodies:
//   m_flDie           : this+0x128 — one-frame explode guard
//   m_fExplosionProtection : this+0x130 — suppresses RadiusDamage
//   m_fAttendExplosion     : this+0xd5 — armed/exploding flag
//   m_pOwner         : this+0x3f0 — firing entity (touch filter)
//   m_vecSavedVel / m_flSavedSpeed : this+0x418/0x424 (lance layout)
// ---------------------------------------------------------------------------
class CSGTCProjBase : public CGrenade
{
public:
	void BaseSpawn( const char *pszModel, const char *pszClass, float flSpeed );

	void Accelere( void );
	void Exploder( TraceResult *pTrace, int bitsDamageType );
	void EXPORT ProjTouch( CBaseEntity *pOther );
	void EXPORT LanceTouch( CBaseEntity *pOther );
	void EXPORT EffectThink( void );

	// per-class knobs
	virtual const char * const *DebrisSounds( void ) = 0;
	virtual BOOL BurnsCorpses( void ) { return TRUE; }

	float m_flDie;
	BOOL m_fAttendExplosion;
	BOOL m_fExplosionProtection;
	CBaseEntity *m_pWeapon;
	CBaseEntity *m_pOwner;
	Vector m_vecDir;        // impact aim point (factories)
	CBaseEntity *m_pTarget; // homing target (big lance only)
	Vector m_vecSavedVel;
	float m_flSavedSpeed;
};

class CLanceProj : public CSGTCProjBase
{
public:
	void Spawn( void );
	void Precache( void );
	virtual const char * const *DebrisSounds( void );

	static CLanceProj *CreateTourrelleProj( Vector vecOrigin, Vector vecAngles );
	static CLanceProj *CreateLanceProj( Vector vecStart, Vector vecVel,
		Vector vecTarget, CBaseEntity *pOwner, CLanceJaffa *pWeapon );
};

class CBigLanceProj : public CSGTCProjBase
{
public:
	void Spawn( void );
	void Precache( void );
	virtual const char * const *DebrisSounds( void );
	void Accelere( void );	// shared boost + homing tail

	static CBigLanceProj *CreateTourrelleProj( Vector vecOrigin, Vector vecAngles );
	static CBigLanceProj *CreateBigLanceProj( Vector vecStart, Vector vecVel,
		Vector vecTarget, CBaseEntity *pOwner, CBigLanceJaffa *pWeapon );
};

class CReetouProj : public CSGTCProjBase
{
public:
	void Spawn( void );
	void Precache( void );
	virtual const char * const *DebrisSounds( void );
	void Explodera( TraceResult *pTrace, int bitsDamageType );
	void EXPORT EffectThink( void );	// narco-sprite trail only

	static CReetouProj *CreateReetouProj( Vector vecStart, Vector vecAngles,
		CBaseEntity *pOwner, CReetou *pWeapon );

	float m_flNarcoNext;	// this+0x3f4 — TE_SPRITE trail timer
};

// Sprite index used by the reetou trail + explosion TE (ref: g_sModelIndexNarco
// BSS @0x23827e — file-global in the real mod).
extern int g_sModelIndexNarco;

#endif // SGTC_LANCEPROJ_H
