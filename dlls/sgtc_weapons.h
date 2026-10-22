// sgtc_weapons.h - StargateTC server weapon arsenal (NEW file).
//
// Rebuilds the SGTC 2.0 server-side arsenal on the hlsdk-xash3d base.
// Truth sources:
// - weapon_* -> class thunks + C<Weapon>::GetItemInfo disassembly of
//   ref/stc_i386.so (2.0 Linux server, unstripped) for exact iId/pos/
//   maxClip/maxAmmo/flags/weight/ammo strings.
// - view/world models, sounds, events: Spawn/Precache string refs in the
//   same .so + stargatetc/ assets.
// - FireBulletsPlayer bullet-type immediates + rodata cooldown consts for
//   rate/damage notes (see per-weapon comments in the .cpp).
// - Client contract: cl_dll/sgtc_weapons_client.h SGTC_WPN_* ids 0-31.
//   Server m_iId MUST equal those ids (stock HL ids 1-15 collide by design).
//
// Stock files are NEVER edited. Where the reference build reuses stock
// classes (weapon_mp5/weapon_9mmAR -> CMP5, weapon_spas -> CShotgun,
// weapon_handgrenade -> CHandGrenade, weapon_c4 -> CSatchel, weapon_mine ->
// tripmine, monster_mine -> CTripmineGrenade), stock registrations stand.
// Other links use SGTC-specific subclasses with identical link names/ids.
// Projectile/placed entities owned elsewhere (sgtc_entities peer:
// CBigLanceProj, CLanceProj, CReetouProj, CTac, effect_shield) are NOT
// redeclared; staff weapons fire local CGrenade-derived bolts or
// Create() those map names so there is no ODR clash.
#pragma once
#ifndef SGTC_WEAPONS_H
#define SGTC_WEAPONS_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"

// Server-authoritative SGTC weapon ids. MUST match
// cl_dll/sgtc_weapons_client.h SGTCWeaponId exactly (0-31).
enum SGTCWeaponId
{
	SGTC_WPN_FLASHBANG = 0,
	SGTC_WPN_COUJAF = 1,
	SGTC_WPN_M92S = 2,
	SGTC_WPN_COUTEAU = 3,
	SGTC_WPN_MP5 = 4,
	SGTC_WPN_PSG1 = 5,
	SGTC_WPN_P90 = 6,
	SGTC_WPN_SHOTGUN_STOCK = 7, // link weapon_spas
	SGTC_WPN_TAC = 8,
	SGTC_WPN_INVIS = 9,
	SGTC_WPN_GACGOA = 10,
	SGTC_WPN_M16 = 11,
	SGTC_WPN_HANDGRENADE = 12,
	SGTC_WPN_TRIPMINE = 13, // link weapon_mine
	SGTC_WPN_SATCHEL = 14, // link weapon_c4
	SGTC_WPN_USAS = 15,
	SGTC_WPN_MAIN = 16,
	SGTC_WPN_ZAT = 17,
	SGTC_WPN_BIGLANCE = 18, // link weapon_glstaff
	SGTC_WPN_LANCE = 19, // link weapon_lance
	SGTC_WPN_BATSUP = 20,
	SGTC_WPN_REETOU = 21,
	SGTC_WPN_LARVE = 22,
	SGTC_WPN_ZATARC = 23,
	SGTC_WPN_MEDIC = 24,
	SGTC_WPN_SHIELD = 25, // link weapon_shield (fills client-table gap)
	SGTC_WPN_SMOKEGRENADE = 26,
	SGTC_WPN_NERVEGRENADE = 27,
	SGTC_WPN_NARCOGRENADE = 28,
	SGTC_WPN_MUSTARDGRENADE = 29,
	SGTC_WPN_GOAREGEN = 30,
	SGTC_WPN_9MMAR = 31, // link weapon_9mmAR (shares CSGTCMP5 with MP5)
};

// SGTC custom bullet types observed at FireBulletsPlayer call sites.
// Stock types 1,2,4 keep stock meaning (1=9mm,2=MP5,4=buckshot);
// the rest are SGTC-extended switch cases in the 2.0 .so.
#define SGTC_BULLET_M92S 2 // shares stock MP5 path
#define SGTC_BULLET_MP5 2
#define SGTC_BULLET_SPAS 4
#define SGTC_BULLET_USAS 4
#define SGTC_BULLET_ZAT 13
#define SGTC_BULLET_M16 16
#define SGTC_BULLET_P90 15
#define SGTC_BULLET_ZATARC 18
#define SGTC_BULLET_PSG1 19

// Ammo registry names (exact .rodata strings from GetItemInfo).
#define SGTC_AMMO_9MMC "9mmC"
#define SGTC_AMMO_MP5 "Mp5"
#define SGTC_AMMO_M16 "m16 ammo"
#define SGTC_AMMO_P90 "p90 ammo"
#define SGTC_AMMO_PSG1 "psg1 clip"
#define SGTC_AMMO_BUCKSHOT "buckshot"
#define SGTC_AMMO_ENERGIE "Energie"
#define SGTC_AMMO_ENERGIE2 "Energie2"
#define SGTC_AMMO_ENERGIE3 "Energie3"
#define SGTC_AMMO_ENERGIE5 "Energie5"
#define SGTC_AMMO_ENERGIEGL "EnergieGL"
#define SGTC_AMMO_TER "TER"
#define SGTC_AMMO_LARVE "Larve"
#define SGTC_AMMO_MEDGOO "medgoo"
#define SGTC_AMMO_GOAREGEN "goaregen"
#define SGTC_AMMO_INVIS "Invis"
#define SGTC_AMMO_TAC "Tacgun"
#define SGTC_AMMO_SATCHEL "Satchel Charge"
#define SGTC_AMMO_TRIPMINE "Trip Mine"
#define SGTC_AMMO_HAND "Hand Grenade"
#define SGTC_AMMO_FLASH "Flash Grenade"
#define SGTC_AMMO_SMOKE "Smoke Grenade"
#define SGTC_AMMO_NERVE "Nerve Grenade"
#define SGTC_AMMO_NARCO "Narco Grenade"
#define SGTC_AMMO_MUSTARD "Mustard Grenade"
#define SGTC_AMMO_GACG "GrenadeChoc"

// ref CBasePlayer::WeapDispSet @0x133aca — called at start of each SGTC Deploy.
#define SGTC_DEPLOY( disp, vmdl, pmdl, anim, ext ) \
	( m_pPlayer->SGTC_WeapDispSet( disp ), DefaultDeploy( vmdl, pmdl, anim, ext ) )

// Common SGTC deploy/idle helper: every class uses DefaultDeploy with its
// v_/p_ models and a stock-style animation extension.
#define SGTC_DECLARE_WEAPON() \
	int GetItemInfo( ItemInfo *p ); \
	int AddToPlayer( CBasePlayer *pPlayer ); \
	void Spawn( void ); \
	void Precache( void ); \
	BOOL Deploy( void ); \
	void PrimaryAttack( void ); \
	void SecondaryAttack( void ); \
	void Reload( void ); \
	void WeaponIdle( void ); \
	virtual BOOL UseDecrement( void ) { return FALSE; }

// ---- Hitscan rifles / pistols / energy guns ----

class CM92s : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 3; }
	void M92sFire( float flSpread, float flCycleTime );
private:
	unsigned short m_usFire;
};

// CSGTCMP5 removed: reference maps weapon_mp5/weapon_9mmAR to stock CMP5.

class CM16 : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 1; }
private:
	unsigned short m_usFire;
};

class CP90 : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 1; }
private:
	unsigned short m_usFire;
};

class CPsg1 : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 4; }
private:
	unsigned short m_usFire;
};

// CSGTCSpas removed: reference maps weapon_spas to stock CShotgun.

class CUsas : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 5; }
private:
	unsigned short m_usFire;
};

class CZat : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 2; }
private:
	unsigned short m_usFireA;
	unsigned short m_usFireB;
};

class CZatarc : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 1; }
private:
	unsigned short m_usFire;
	int m_iSgtcZoomStage;	// ref CZatarc+0x204 zoom toggle
};

// ---- Staff / projectile weapons (local bolts; map entities owned by entities peer) ----

class CLanceJaffa : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 2; }
	void EXPORT SwingAgain( void );	// ref @0x10ffaa: Swing(0)
	int Swing( int fFirst );		// ref @0x10f5d0: melee + TraceResult store
private:
	unsigned short m_usFire;
	unsigned short m_usFire2;
	int m_iSwing;
	TraceResult m_trLastHit;		// ref this+0x208..0x23b last-hit TraceResult
};

class CBigLanceJaffa : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 5; }
private:
	unsigned short m_usFire;
};

class CReetou : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 4; }
private:
	unsigned short m_usFire;
	unsigned short m_usFire2;
	BOOL m_bSgtcCloakOn;		// ref CReetou+0x204 cloak toggle
	float m_flSgtcCloakTimer;	// ref CReetou+0x1fc cloak timer
	void ThinkSearchInvisible( void );	// ref SearchInvisible @0x175 (m_pfnThink arm)
	void ThinkGuardStub( void );	// ref guard stub @0x175
};

// Local bolt spawned by staff weapons (CGrenade child so no ODR clash
// with the entities peer's CBigLanceProj/CLanceProj/CReetouProj).
class CSGTCBolt : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	static CSGTCBolt *CreateBolt( const Vector &vecOrigin, const Vector &vecAngles, CBaseEntity *pOwner, float flDamage, const char *pszModel );
	void EXPORT BoltTouch( CBaseEntity *pOther );
	float m_flDamage;
};

// ---- Melee ----

class CBatonSupplice : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	void EXPORT SwingAgain( void );
	int Swing( int fFirst );
	int iItemSlot( void ) { return 1; }
	void Holster( int skiplocal = 0 );
	int m_iSwing;
	TraceResult m_trHit;
private:
	unsigned short m_usFire;
	unsigned short m_usFire2;
};

class CCoujaf : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	void EXPORT SwingAgain( void );
	int Swing( int fFirst );
	int iItemSlot( void ) { return 1; }
	int m_iSwing;
	TraceResult m_trHit;
private:
	unsigned short m_usFire;
};

class CCouteau : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	void EXPORT Smack( void );
	void EXPORT SwingAgain( void );
	int Swing( int fFirst );
	void ThrowIt( void );
	int iItemSlot( void ) { return 1; }
	int m_iSwing;
	TraceResult m_trHit;
private:
	unsigned short m_usFire;
};

// ---- Grenades / thrown ----

class CFlashBang : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	BOOL CanHolster( void ) { return TRUE; }
	void Holster( int skiplocal = 0 );
	int iItemSlot( void ) { return 5; }
};

// CSGTCHandGrenade removed: reference maps weapon_handgrenade to stock CHandGrenade.

class CGrenadeChoc : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 4; }
};

class CSmokeGrenade : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 2; }
};

class CNerveGrenade : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 3; }
};

class CNarcoGrenade : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 4; }
};

class CMustardGrenade : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 5; }
};

// ---- Placeable / special ----

// CSGTCC4 removed: reference maps weapon_c4 to stock CSatchel.

// CSGTCMine (weapon) removed: reference maps weapon_mine to stock tripmine
// class; name collided with sgtc_monsters.h's CSGTCMine monster.

class CTacgun : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	void Holster( int skiplocal = 0 );
	int iItemSlot( void ) { return 2; }
};

// ---- Device / heal / stealth ----

class CMainDieu : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 1; }
private:
	unsigned short m_usFire;
	unsigned short m_usFire2;
	float m_flSgtcBlastTimer;	// ref CMainDieu+0x1fc blast cooldown
	BOOL m_bSgtcBlastFlag;		// ref CMainDieu+0x200 flag
};

class CMedicx : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	BOOL IsUseable( void );
	int iItemSlot( void ) { return 1; }
};

class CMedicxgoa : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	BOOL IsUseable( void );
	int iItemSlot( void ) { return 4; }
};

class CInvis : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	void EXPORT InvisThink( void );
	int iItemSlot( void ) { return 3; }

	char	m_bInvisActive;		// ref weapon +0x208
	float	m_flInvisLatchA;	// ref weapon +0x134
	float	m_flInvisLatchB;	// ref weapon +0x138
};

class CLarve : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 1; }
};

// weapon_shield: personal shield wall. Spawns the entities peer's
// effect_shield via Create() so no CShield ODR clash.
class CShieldGun : public CBasePlayerWeapon
{
public:
	SGTC_DECLARE_WEAPON();
	int iItemSlot( void ) { return 4; }
};

// ---- Ammo ----

class CPsg1AmmoClip : public CBasePlayerAmmo
{
public:
	void Spawn( void );
	void Precache( void );
	BOOL AddAmmo( CBaseEntity *pOther );
};

// ---------------------------------------------------------------------------
// Shared implementation helpers — used by every sgtc_wpn_*.cpp. Moved here
// from sgtc_weapons.cpp when the bodies were split per-weapon.
// ---------------------------------------------------------------------------

enum sgtc_anim_e
{
	SGTC_IDLE = 0,
	SGTC_DRAW,
	SGTC_FIRE1,
	SGTC_FIRE2,
	SGTC_FIRE3,
	SGTC_RELOAD,
	SGTC_HOLSTER,
	SGTC_LAUNCH,
	SGTC_PUMP
};

#ifdef CLIENT_WEAPONS
#define SGTC_EVENT_FLAGS FEV_NOTHOST
#else
#define SGTC_EVENT_FLAGS 0
#endif


#define SGTC_ADD_TO_PLAYER( cls )												\
int cls::AddToPlayer( CBasePlayer *pPlayer )								\
{																			\
	if( CBasePlayerWeapon::AddToPlayer( pPlayer ) )							\
	{																		\
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );		\
			WRITE_BYTE( m_iId );											\
		MESSAGE_END();														\
		return TRUE;														\
	}																		\
	return FALSE;															\
}

#define SGTC_GETITEM( cls, A1, MA1, MC, SL, PS, ID, FL, WT )					\
int cls::GetItemInfo( ItemInfo *p )											\
{																			\
	p->pszName = STRING( pev->classname );									\
	p->pszAmmo1 = A1;														\
	p->iMaxAmmo1 = MA1;														\
	p->pszAmmo2 = NULL;														\
	p->iMaxAmmo2 = -1;														\
	p->iMaxClip = MC;														\
	p->iSlot = SL;															\
	p->iPosition = PS;														\
	p->iFlags = FL;															\
	p->iId = m_iId = ID;													\
	p->iWeight = WT;														\
	return 1;																\
}

#define SGTC_WEAPON_IDLE( cls )													\
void cls::WeaponIdle( void )												\
{																			\
	ResetEmptySound();														\
	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )						\
		return;																\
	SendWeaponAnim( SGTC_IDLE );											\
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() +							\
		UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );			\
}

#define SGTC_WEAPON_RELOAD( cls, MAXCLIP )										\
void cls::Reload( void )													\
{																			\
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == MAXCLIP )\
		return;																\
	DefaultReload( MAXCLIP, SGTC_RELOAD, 1.5 );								\
}

#define SGTC_NO_RELOAD( cls )													\
void cls::Reload( void )													\
{																			\
}

#define SGTC_NO_SECONDARY( cls )												\
void cls::SecondaryAttack( void )											\
{																			\
}

// Hitscan rifle/pistol body. Mirrors stock CMP5::PrimaryAttack.
#define SGTC_GUN_PRIMARY( cls, EVENTVAR, BULLET, SPREAD, SHOTS, DIST, DAMAGE, TRACER, CYCLE ) \
void cls::PrimaryAttack( void )												\
{																			\
	if( m_pPlayer->pev->waterlevel == 3 )									\
	{																		\
		PlayEmptySound();													\
		m_flNextPrimaryAttack = 0.15;										\
		return;																\
	}																		\
	if( m_iClip <= 0 )														\
	{																		\
		PlayEmptySound();													\
		m_flNextPrimaryAttack = 0.15;										\
		return;																}																		\
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;							\
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;							\
	m_iClip--;																\
	m_pPlayer->pev->effects = (int)( m_pPlayer->pev->effects ) | EF_MUZZLEFLASH; \
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );								\
	Vector vecSrc = m_pPlayer->GetGunPosition();							\
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );		\
	Vector vecDir = m_pPlayer->FireBulletsPlayer( SHOTS, vecSrc, vecAiming,	\
		SPREAD, DIST, BULLET, TRACER, DAMAGE, m_pPlayer->pev, m_pPlayer->random_seed ); \
	PLAYBACK_EVENT_FULL( SGTC_EVENT_FLAGS, m_pPlayer->edict(), EVENTVAR,	\
		0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y,	\
		0, 0, 0, 0 );														\
	if( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )			\
	m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );					\
	m_flNextPrimaryAttack = GetNextAttackDelay( CYCLE );					\
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() +							\
		UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );			\
}

#endif // SGTC_WEAPONS_H
