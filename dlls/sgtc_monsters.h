// hlsdk-xash3d/dlls/sgtc_monsters.h
//
// StargateTC custom server monsters — hlsdk-xash3d port (branch sgtc-port).
//
// Covers every custom monster_* factory from the 1.x hl.dll export table
// plus the 2.0-only additions found in ref/stc_i386.so:
//
//	monster_replic / monster_replicateur / monster_replicator / monster_larve
//	monster_implanted (CZombie alias) / monster_tac / monster_skull
//	monster_mine / monster_vortigaunt (CISlave alias)
//	monster_arm1-3 / monster_head1-3 / monster_chest1-3 / monster_leg1-3 /
//	monster_stomach1-3 (hitgroup-tier dummies, sk_monster_* cvars)
//	2.0-only: monster_zatboom / monster_trapbox / monster_shieldemit /
//	monster_lockdown
//
// Disassembly references (authoritative behavior sources):
//   CReplicMonster  — Spawn 0x1535c4 / Precache 0x15387a (models/replic.mdl),
//			HuntThink + SuperBounceTouch (hl.dll 0x100bb5dc / 0x100bbcff)
//			method set identical to CSqueakGrenade -> CGrenade base.
//   CLarveMonster   — Spawn 0x100b5e (models/w_larve.mdl),
//			Precache 0x100e42 (w_squeak.mdl, gloweyes, larve1/2,
//			sqk_deploy1), HuntThink 0x1007112c -> CGrenade base.
//   CReplicateurMonster — ReplicateurThink 0x161be4: seeks
//			"info_player_deathmatch" waypoint at 50 u/s, repath 0.1s,
//			Center z +6; Precache headcrab.mdl -> headcrab-like
//			CBaseMonster base.
//   CReetou::SearchInvisible (0x100bab25) and CInvis::InvisThink (0x1006c008)
//			inform CTac's decloak sweep (anti-invis behavior).
//   CTac — Spawn 0x18bf50 (models/w_tacb.mdl), Think/Smoker, tac/tacs/tacp/
//			tacact/place/invis sounds, events/tac.sc, gammawave.spr.
//   CRat (monster_skull) — Spawn 0x14b250 (models/crane_rose.mdl),
//			skull_attack/idle sounds, sprites/nargas.spr.
//   CZombie (monster_implanted) — Spawn 0x1b7ffe
//			(models/player/commander/commander.mdl).
//   CZatBoom — Spawn 0x14c91c (models/w_zat.mdl, zatoverload).
//   CTrapBox — Spawn 0x14de3c (models/trapbox.mdl, trapbox.wav, plasma.spr).
//   CSEmit (monster_shieldemit) — Spawn 0x1506c6 (models/shgen.mdl).
//   CLockDown (monster_lockdown) — Spawn 0x14fc0c (models/lockpatch.mdl).
//   CShield / BigShield — models/shield.mdl / models/bigshield.mdl.
//
// Stock-AI analogue per class is noted on each declaration; all classes
// derive from CBaseMonster / CGrenade (the only AI bases with dlls headers)
// and re-implement the decoded think/touch behavior with think loops.

#pragma once
#ifndef SGTC_MONSTERS_H
#define SGTC_MONSTERS_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "skill.h"

#ifndef CLIENT_DLL

// Snark-style hopping bioweapon. Stock analogue: CSqueakGrenade
// (CReplicMonster method set matches it 1:1). models/replic.mdl.
class CSGTCReplic : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_BIOWEAPON; }

	void EXPORT HuntThink( void );
	void EXPORT SuperBounceTouch( CBaseEntity *pOther );
	void Detonate( void );

	float m_flDie;
	float m_flNextHunt;
	float m_flNextSound;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// Snark-style grub. Stock analogue: CSqueakGrenade (CLarveMonster method
// set matches it 1:1). models/w_larve.mdl + weapons/larve1.wav.
class CSGTCLarve : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_BIOWEAPON; }

	void EXPORT HuntThink( void );
	void EXPORT SuperBounceTouch( CBaseEntity *pOther );
	void Detonate( void );

	float m_flDie;
	float m_flNextHunt;
	float m_flNextSound;
	int m_iSgtcFlag;	// [INFERENCE] ref +0xd8 — deploy-time flag set by Talent
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// Headcrab-style hunter. Stock analogue: CHeadCrab (Precache uses
// models/headcrab.mdl; BodyTarget/Center/SetYawSpeed/Classify shape matches).
// ReplicateurThink drifts toward info_player_deathmatch at 50 u/s.
class CSGTCReplicateur : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_MONSTER; }
	void SetYawSpeed( void ) { pev->yaw_speed = 120; }
	Vector Center( void );
	Vector BodyTarget( const Vector &posSrc );

	void EXPORT ReplicateurThink( void );
};

// Zombie-style bruiser. Stock analogue: CZombie (monster_implanted alias).
// Custom skin: models/player/commander/commander.mdl.
class CSGTCImplanted : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_MONSTER; }
	void SetYawSpeed( void ) { pev->yaw_speed = 120; }

	void EXPORT ImplantedThink( void );
	void PainSound( void );
	void AlertSound( void );
};

// Slave-style zapper. Stock analogue: CISlave (monster_vortigaunt alias,
// same dual-factory pattern as stock). models/islave.mdl.
class CSGTCVortigaunt : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_MILITARY; }
	void SetYawSpeed( void ) { pev->yaw_speed = 90; }

	void EXPORT VortigauntThink( void );

	float m_flNextZap;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// Rat-style skitterer. Stock analogue: CRat. models/crane_rose.mdl +
// misc/skull_* sounds + sprites/nargas.spr.
class CSGTCSkull : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_INSECT; }
	void SetYawSpeed( void ) { pev->yaw_speed = 45; }

	void EXPORT ScuttleThink( void );
	void EXPORT SkullTouch( CBaseEntity *pOther );

	float m_flNextSound;
	CBasePlayer *m_pSkullOwner;	// ref entity+0xc0 — owned skulls (PlayerKilled sweep)
};

// Proximity mine. Stock analogue: CTripmineGrenade (CGrenade base).
// World model: models/w_mine.mdl.
class CSGTCMine : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MACHINE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void EXPORT PowerupThink( void );
	void EXPORT ActiveThink( void );
	void Detonate( CBaseEntity *pVictim );

	float m_flPowerUp;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// Static sentry gun. Stock analogue: CBaseTurret/C Sentry (Think + Smoker
// + tac sound set + decloak sweep after CReetou::SearchInvisible).
class CSGTCTac : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MACHINE; }
	BOOL IsTac( void ) { return TRUE; }

	void EXPORT TacThink( void );
	void Smoker( void );

	float m_flNextFire;
	float m_flNextSweep;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// Hitgroup-tier damage dummy, one class linked to all 15 factories
// (monster_arm/head/chest/leg/stomach 1-3). Health scales with tier and
// the matching sk_monster_* cvar. Stock analogue: none (map-test dummy);
// uses genericmonster-style CBaseMonster base. Model: models/skelete.mdl.
class CSGTCBodyPart : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_MONSTER; }

	void EXPORT IdleThink( void );
};

// Overloading zat cell (2.0). Stock analogue: none (timed CGrenade-style
// trap on a CBaseMonster base). models/w_zat.mdl + zatoverload/zatblow.
class CSGTCZatBoom : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MACHINE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void EXPORT ChargeThink( void );
	void Discharge( void );

	float m_flBlastTime;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// Trapped crate (2.0). Stock analogue: CSatchelCharge (proximity +
// shot-triggered). models/trapbox.mdl + weapons/trapbox.wav.
class CSGTCTrapBox : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MACHINE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );

	void EXPORT WatchThink( void );
	void Trigger( void );
	Vector m_vecPlace;	// [INFERENCE] ref +0xec..0xf4 — spawn position store
	int m_iPlaceSlot;	// [INFERENCE] ref +0xe8 — deploy slot index
};

// Shield emitter (2.0). Stock analogue: none (area-buff dynamite-style
// support on a CBaseMonster base). models/shgen.mdl; maintains a glow-shell
// "shield" + trickle heal on nearby allies every 1 s.
class CSGTCShieldEmit : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MACHINE; }

	void EXPORT EmitThink( void );
	CBasePlayer *m_pSgtcOwner;	// ref entity +200 — deployer (Talent scan)
	int m_iSgtcMode;			// ref +0x104 — state machine (1 child-frame, 2 teardown)
	CBaseEntity *m_pSgtcEffect;	// ref +0x3f8 — effect_bigshield child
	float m_flSgtcNextFx;		// ref +0x3f4 — FX heartbeat
};

// Lockdown patch (2.0). Wall/box blocker toggled by Use.
// models/lockpatch.mdl + weapons/place.wav.
class CSGTCLockDown : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MACHINE; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

#endif // CLIENT_DLL

#endif // SGTC_MONSTERS_H
