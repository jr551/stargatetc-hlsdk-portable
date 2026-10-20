// sgtc_round.cpp — StargateTC round lifecycle (NEW file).
//
// Clean-room port of the ref round core (ref/stc_i386.so):
//   CStargateTeamDM::PlayerThink  nm 0x17d088 (Ghidra 0x18d088)
//   CStargateTeamCP::PlayerThink  nm 0x17008c (Ghidra 0x18008c)
//   CStargateTeamDM::NewRound     nm 0x184f42 (Ghidra 0x194f42)
//   CStargateTeamCP::NewRound     nm 0x177b34 (Ghidra 0x187b34)
//   CStargateTeamDM::Goal(int)    nm 0x17b650 (Ghidra 0x18b650)
//   CStargateTeamDM::EndRound     nm 0x17b77c (Ghidra 0x18b77c)
//   PlayMusic(int)                nm 0x17b32c (Ghidra 0x18b32c)
// All Ghidra VMA == nm addr + 0x10000.
//
// Ref ABI map (verified from the decoded bodies):
//   gamerules vtable slots: +0x18 Think, +0x28 GetPlayerSpawnSpot,
//     +0x64 FPlayerCanRespawn, +0x94 ChangePlayerTeam,
//     +0x100 SetDefaultPlayerTeam, +0xe0 GetTeamIndex, +0x120 NewRound,
//     +0x34 RedrawPortrait (CBasePlayer*) — PlayerThink itself is +0x64 on
//     the PLAYER vtable, not here.
//   CBasePlayer vtable slots: +0x18 IsAlive, +0x24 (env-probe family call,
//     port: SendValueTo), +0x38 = per-player fade (port UTIL_ScreenFade).
//   CBaseEntity vtable slot +0x128 (ground weapon strip) -> port: solid=NOT
//     re-cache stand-in.
//   CBaseEntity+0x4 = pev. entvars: +0x170 = pev->team, +0x168 = pev->button,
//     +0x160 = pev->health (zone state marker).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "game.h"
#include "saverestore.h"
#include "shake.h"
#include "hltv.h"
#include "sgtc_rules.h"
#include "sgtc_misc.h" // CEntSpawn::Event
#include "sgtc_lanceproj.h"
#include "sgtc_zones.h"

// ref NewRound sweep: CSatchelCharge::Deactivate @nm 0x155a1c (pev+0x10c=0, UTIL_Remove)
class CSatchelCharge : public CGrenade
{
public:
	void Deactivate( void );
};

class CToken : public CBaseDelay
{
public:
	void ResetForNewRound( void );
};

class CMultiManager : public CBaseToggle
{
public:
	void Stop( void );
};

extern DLL_GLOBAL BOOL g_fGameOver;
extern void respawn( entvars_t *pev, BOOL fCopyCorpse ); // player.cpp
extern int gmsgScoreInfo;
extern int gmsgTeamScore;
extern int gmsgTeamInfo;
extern int gmsgShowGameTitle;
extern int gmsgNewRnd;
extern int gmsgGtickets;
extern int gmsgTtickets;
extern int sgtc_type; // sgtc_token.cpp (ref BSS type @0x236588)
extern Vector sgtc_firstplace; // sgtc_token.cpp (ref BSS firstplace @0x2368d4)
extern char sgtc_KeyInPlay; // sgtc_token.cpp (ref BSS KeyInPlay @0x2368c6)
extern int sgtc_G_players_last; // sgtc_player.cpp (ref BSS 0x2368a0)
extern int sgtc_T_players_last; // sgtc_player.cpp (ref BSS 0x236898)

// ref resolves "#SGTC_Round_XX" via pfnTextMessageGet (a client_textmessage_t
// lookup) purely as a presence guard on shipped localized strings. The server
// enginefuncs table exposes no TextMessageGet; the #SGTC_Round_* keys ship in
// the mod, so the index guard alone is behavior-equivalent. Returns TRUE once
// sz is formatted.
static BOOL SGTC_RoundMessage( char (&sz)[20], int index )
{
	if( index <= 0 || index > 20 )
		return FALSE;
	_snprintf( sz, sizeof( sz ), "#SGTC_Round_%02d", index );
	sz[sizeof( sz ) - 1] = '\0';
	return TRUE;
}

// ---------------------------------------------------------------------------
// Ref globals (nm addresses of ref/stc_i386.so; all touched only by this TU
// and sgtc_player/sgtc_token/sgtc_zones peers via the externs below).
// ---------------------------------------------------------------------------
int sgtc_GameModeSet = 0;      // nm 0x1e0ad8
float sgtc_gamemodex = -1.0f;  // nm 0x2133bc (mp_gamemode numeric mirror)
float sgtc_tourny = -1.0f;     // nm 0x213574 (mp_tournament)
float sgtc_allowmodevote = -1.0f; // nm 0x213510 (mp_modevoting)
float sgtc_crazymode = -1.0f;  // nm 0x213470 (mp_skulls)
float sgtc_friendlyfire = -1.0f; // nm 0x213358 (mp_friendlyfire mirror)
float sgtc_roundtime = 300.0f; // nm 0x21336c
float sgtc_tickets = 10.0f;    // nm 0x2133d0 (mp_tickets)
float sgtc_endtround = 0.0f;   // BSS 0x2368b8 (goal deadline; checked by Think)
float sgtc_nrcooler = 0.0f;    // BSS 0x2368b4 (NewRound cooldown gate)
char sgtc_hasplayed = 0;       // BSS 0x2368f1 (round played flag)
int sgtc_G_tickets = 0;        // BSS 0x236880
int sgtc_T_tickets = 0;        // BSS 0x236884
int sgtc_G_players_focused = 0;  // BSS 0x2368a8
int sgtc_T_players_focused = 0;  // BSS 0x2368a0
int sgtc_under = 1;            // D 0x2208f4
int sgtc_fadetoblack = 0;      // D 0x21342c (mp_fadetoblack — write-only)
int sgtc_teamed = 0;           // BSS 0x23658c
float sgtc_roundtime_official = 0.0f; // this+0x72c mirror (round start tick)
Vector sgtc_masterkey( 0, 0, 0 ); // BSS 0x2368c8
float sgtc_lcaptain[2] = { 0.0f, 0.0f }; // BSS 0x236890/0x236894 (T,G captain ticks)
int sgtc_LastState[5];         // BSS 0x236864
int sgtc_LastStateI[5];        // BSS 0x23686c
int sgtc_G_players = 0;        // BSS 0x21455c — alive-team-1 count (RecountTeams)
int sgtc_T_players = 0;        // BSS 0x20c7d0 — alive-team-2 count
int sgtc_G_players_last = 0;   // BSS 0x2147bc — previous focused-G count
int sgtc_T_players_last = 0;   // BSS 0x21401c — previous focused-T count
int sgtc_SendPL = 0;           // BSS — ref m_SendPL (CP InitHUD arms it)

// Per-class population tallies (BSS 0x2368f4..0x236918) — refilled by
// UpdateClassCount (vtable+0x118) and read by the classe cap switches.
int sgtc_count_syslord = 0;   // BSS 0x236904 (Goa'uld class 1)
int sgtc_count_prime = 0;     // BSS 0x236900 (Goa'uld class 2 — "prime")
int sgtc_count_jaffa = 0;     // BSS 0x2368fc (Goa'uld class 3)
int sgtc_count_serpent = 0;   // BSS 0x2368f8 (Goa'uld class 4)
int sgtc_count_horus = 0;     // BSS 0x2368f4 (Goa'uld class 5)
int sgtc_count_oniell = 0;    // BSS 0x236918 (Torri class 1 — "oniell" in .so)
int sgtc_count_carter = 0;    // BSS 0x236914 (Torri class 2)
int sgtc_count_jackson = 0;   // BSS 0x236910 (Torri class 3)
int sgtc_count_tealc = 0;     // BSS 0x23690c (Torri class 4)
int sgtc_count_commander = 0; // BSS 0x236908 (Torri class 5)

// ---------------------------------------------------------------------------
// Ref cvar table (12-byte .data blobs: {const char *name, const char *string,
// int flags}) — the .so reads/writes the string pointers directly:
//   friendlyfire @0x213358 mp_friendlyfire, roundtime @0x21336c mp_roundtime,
//   gamemodex @0x2133bc mp_gamemode, tickets @0x2133d0 mp_tickets,
//   crazymode @0x213470 mp_skulls, allowmodevote @0x213510 mp_modevoting,
//   tourny @0x213574 mp_tournament, superguns @0x2134d4 mp_superweapons.
// Ported as cvar_t over the stock registered cvars so NewRound/PlayerThink
// see live engine values (identical 32-bit layout).
// ---------------------------------------------------------------------------
cvar_t sgtc_cvar_friendlyfire = { "mp_friendlyfire", "0", FCVAR_SERVER };
cvar_t sgtc_cvar_roundtime = { "mp_roundtime", "300", FCVAR_SERVER };
cvar_t sgtc_cvar_gamemode = { "mp_gamemode", "sg_", FCVAR_SERVER };
cvar_t sgtc_cvar_tickets = { "mp_tickets", "10", FCVAR_SERVER };
cvar_t sgtc_cvar_skulls = { "mp_skulls", "0", FCVAR_SERVER };
cvar_t sgtc_cvar_modevoting = { "mp_modevoting", "0", FCVAR_SERVER };
cvar_t sgtc_cvar_tournament = { "mp_tournament", "0", FCVAR_SERVER };
cvar_t sgtc_cvar_superweapons = { "mp_superweapons", "0", FCVAR_SERVER };
cvar_t sgtc_cvar_particlelatency = { "particlelat", "10", FCVAR_SERVER };
cvar_t sgtc_cvar_respawntime = { "mp_respawntime", "10", FCVAR_SERVER };
cvar_t sgtc_cvar_diepoints = { "mp_diepoints", "2", FCVAR_SERVER };	// ref vicpoints @0x213524 — cvar_t.string "2"
cvar_t sgtc_cvar_killpoints = { "mp_killpoints", "1", FCVAR_SERVER };	// ref killpoints @0x213538 — cvar_t.string "1"
cvar_t sgtc_cvar_truezat = { "mp_truezat", "0", FCVAR_SERVER };	// ref truezat @0x2368d4
cvar_t sgtc_cvar_wepselmode = { "mp_wepselmode", "1", FCVAR_SERVER };	// ref weaponsel @0x223484 — spawn menu mode; ref cvar name @0x1e0f51 registers default "1" (@0x1dfabd)
cvar_t sgtc_cvar_freeguns = { "mp_freeguns", "0", FCVAR_SERVER };		// ref freeguns @0x2238f8 — CP spawn grants all slots when >1
cvar_t sgtc_cvar_attackcountdown = { "mp_attackcountdown", "30", FCVAR_SERVER };	// ref attackcountdown @0x22390c

// ref ClientCommand 'classe' reads these cvar_t blobs live (@0x213420
// autobalance, @0x2135d8 mp_altclasslimiter, @0x213600..0x2136b4 class caps):
cvar_t sgtc_cvar_resetround = { "mp_resetround", "0", FCVAR_SERVER };	// ref resetround @0x21340c (Think pulse)
cvar_t sgtc_cvar_autobalance = { "mp_autobalance", "2", FCVAR_SERVER };	// ref autobalance @0x213420
cvar_t sgtc_cvar_classlimitmodel = { "mp_altclasslimiter", "0", FCVAR_SERVER };	// ref classlimitmodel @0x2135d8
cvar_t sgtc_cvar_max_syslord = { "max_syslord", "1", FCVAR_SERVER };	// ref @0x213650
cvar_t sgtc_cvar_max_anubis = { "max_anubis", "3", FCVAR_SERVER };	// ref @0x21363c (counts vs count_prime)
cvar_t sgtc_cvar_max_jaffa = { "max_jaffa", "0", FCVAR_SERVER };		// ref @0x213628
cvar_t sgtc_cvar_max_serpent = { "max_serpent", "1", FCVAR_SERVER };	// ref @0x213614
cvar_t sgtc_cvar_max_horus = { "max_horus", "3", FCVAR_SERVER };		// ref @0x213600
cvar_t sgtc_cvar_max_oniell = { "max_oneill", "1", FCVAR_SERVER };	// ref @0x2136b4
cvar_t sgtc_cvar_max_carter = { "max_carter", "1", FCVAR_SERVER };	// ref @0x2136a0
cvar_t sgtc_cvar_max_jackson = { "max_jackson", "1", FCVAR_SERVER };	// ref @0x21368c
cvar_t sgtc_cvar_max_tealc = { "max_tealc", "1", FCVAR_SERVER };		// ref @0x213678
cvar_t sgtc_cvar_max_commander = { "max_soldier", "0", FCVAR_SERVER };	// ref @0x213664

// ---------------------------------------------------------------------------
// wpnC0..wpnC27 — ref .data cvar_t blobs @0x2236c8..0x2238e4 (20 B each:
// {name, default-string, FCVAR_SERVER=4, value, next}), read by the DM/CP
// Think m_SendPL block as ROUND(value) and written as the 28-byte cost tail of
// the TeamNames usermsg (menu-12 per-slot costs, client DAT_100dd974[28]).
// Names/defaults read from the ref .data + its rodata string pool
// (0x1e0ee9..0x1e12e2); defaults cross-checked against the orig menu-12
// screenshot (USAS 40, Beretta 5, MP5 7, PSG1 11, P90 9, M16 8, Zat 6,
// Lance 7, Hand Grenade 6, Mine 7, C4 13, Smoke 5, Nerve 6, Narco 6,
// Mustard 7).
// ---------------------------------------------------------------------------
cvar_t sgtc_cvar_wpnC0 = { "wpn_weapon_glstaff", "40", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC1 = { "wpn_weapon_usas", "40", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC2 = { "wpn_weapon_batsup", "3", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC3 = { "wpn_weapon_m92", "5", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC4 = { "wpn_weapon_mp5", "7", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC5 = { "wpn_weapon_psg1", "11", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC6 = { "wpn_weapon_p90", "9", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC7 = { "wpn_weapon_spas", "8", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC8 = { "wpn_weapon_m16", "8", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC9 = { "wpn_weapon_main", "8", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC10 = { "wpn_weapon_zat", "6", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC11 = { "wpn_weapon_lance", "7", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC12 = { "wpn_weapon_reetou", "9", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC13 = { "wpn_weapon_zatarc", "11", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC14 = { "wpn_weapon_tacgun", "6", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC15 = { "wpn_weapon_handgrenade", "6", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC16 = { "wpn_weapon_mine", "7", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC17 = { "wpn_weapon_invis", "8", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC18 = { "wpn_weapon_shield", "8", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC19 = { "wpn_weapon_gacgoa", "7", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC20 = { "wpn_weapon_c4", "13", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC21 = { "wpn_weapon_larve", "9", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC22 = { "wpn_weapon_medic", "8", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC23 = { "wpn_weapon_smokegrenade", "5", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC24 = { "wpn_weapon_nervegrenade", "6", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC25 = { "wpn_weapon_narcogrenade", "6", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC26 = { "wpn_weapon_mustardgrenade", "7", FCVAR_SERVER };
cvar_t sgtc_cvar_wpnC27 = { "wpn_weapon_goaregen", "7", FCVAR_SERVER };
// Index-ordered accessors for the cost tail (slot i ↔ wpnC<i>).
extern cvar_t *const sgtc_wpnC[28] =
{
	&sgtc_cvar_wpnC0,  &sgtc_cvar_wpnC1,  &sgtc_cvar_wpnC2,  &sgtc_cvar_wpnC3,
	&sgtc_cvar_wpnC4,  &sgtc_cvar_wpnC5,  &sgtc_cvar_wpnC6,  &sgtc_cvar_wpnC7,
	&sgtc_cvar_wpnC8,  &sgtc_cvar_wpnC9,  &sgtc_cvar_wpnC10, &sgtc_cvar_wpnC11,
	&sgtc_cvar_wpnC12, &sgtc_cvar_wpnC13, &sgtc_cvar_wpnC14, &sgtc_cvar_wpnC15,
	&sgtc_cvar_wpnC16, &sgtc_cvar_wpnC17, &sgtc_cvar_wpnC18, &sgtc_cvar_wpnC19,
	&sgtc_cvar_wpnC20, &sgtc_cvar_wpnC21, &sgtc_cvar_wpnC22, &sgtc_cvar_wpnC23,
	&sgtc_cvar_wpnC24, &sgtc_cvar_wpnC25, &sgtc_cvar_wpnC26, &sgtc_cvar_wpnC27,
};
void SGTC_RegisterRulesCvars( void )
{
	CVAR_REGISTER( &sgtc_cvar_friendlyfire );
	CVAR_REGISTER( &sgtc_cvar_roundtime );
	CVAR_REGISTER( &sgtc_cvar_gamemode );
	CVAR_REGISTER( &sgtc_cvar_tickets );
	CVAR_REGISTER( &sgtc_cvar_skulls );
	CVAR_REGISTER( &sgtc_cvar_modevoting );
	CVAR_REGISTER( &sgtc_cvar_tournament );
	CVAR_REGISTER( &sgtc_cvar_superweapons );
	CVAR_REGISTER( &sgtc_cvar_particlelatency );
	CVAR_REGISTER( &sgtc_cvar_respawntime );
	CVAR_REGISTER( &sgtc_cvar_diepoints );
	CVAR_REGISTER( &sgtc_cvar_killpoints );
	CVAR_REGISTER( &sgtc_cvar_truezat );
	CVAR_REGISTER( &sgtc_cvar_wepselmode );
	CVAR_REGISTER( &sgtc_cvar_freeguns );
	CVAR_REGISTER( &sgtc_cvar_attackcountdown );
	CVAR_REGISTER( &sgtc_cvar_resetround );
	CVAR_REGISTER( &sgtc_cvar_autobalance );
	CVAR_REGISTER( &sgtc_cvar_classlimitmodel );
	CVAR_REGISTER( &sgtc_cvar_max_syslord );
	CVAR_REGISTER( &sgtc_cvar_max_anubis );
	CVAR_REGISTER( &sgtc_cvar_max_jaffa );
	CVAR_REGISTER( &sgtc_cvar_max_serpent );
	CVAR_REGISTER( &sgtc_cvar_max_horus );
	CVAR_REGISTER( &sgtc_cvar_max_oniell );
	CVAR_REGISTER( &sgtc_cvar_max_carter );
	CVAR_REGISTER( &sgtc_cvar_max_jackson );
	CVAR_REGISTER( &sgtc_cvar_max_tealc );
	CVAR_REGISTER( &sgtc_cvar_max_commander );
	for( int i = 0; i < 28; i++ )
		CVAR_REGISTER( sgtc_wpnC[i] );
}

float sgtc_roundstart = 0.0f; // mirror of rules this+0x72c
char sgtc_round_official = 0; // ref BSS round_official (byte)
Vector sgtc_mKill( 0, 0, 0 );

// ---------------------------------------------------------------------------
// Local decode helpers (ref @0x1abc1c / 0x1ab90e / 0x1ab9c9).
// ---------------------------------------------------------------------------
int SGTC_Round_intconstrain( int lo, int hi, int v )
{
	return v < lo ? lo : ( v > hi ? hi : v );
}

static int SGTC_Round_roundf( float v )
{
	return (int)floorf( v + 0.5f );
}

static int SGTC_Round_rand( int lo, int hi )
{
	return RANDOM_LONG( lo, hi );
}

static bool SGTC_Round_vne( const Vector &a, const Vector &b )
{
	return a.x != b.x || a.y != b.y || a.z != b.z;
}

// GiveFromMenu — ref nm 0x17ae8a (free function; body owned by the player
// helpers slice, which implements the SGTC menu loadout).
void SGTC_GiveFromMenu( CBasePlayer *pPlayer );

// ---------------------------------------------------------------------------
// PlayMusic(int) — nm 0x17b32c (Ghidra 0x18b32c).
// MESSAGE_BEGIN(MSG_ALL, gmsgShowGameTitle) / WRITE_BYTE(mode+1) / MESSAGE_END,
// then the Torri team spawn point re-plays the randomised round sample
// (music/tau01-03 = mode 0, goa01-03 = mode 1, draw01-03 = mode 2; rodata
// 0x1dbae0..0x1dbb74, byte-verified "info_player_torri" + tau/tau/draw order).
// ---------------------------------------------------------------------------
void SGTC_PlayMusic( int iMode )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgShowGameTitle );
		WRITE_BYTE( iMode + 1 );
	MESSAGE_END();

	CBaseEntity *pSpot = UTIL_FindEntityByClassname( NULL, "info_player_torri" );
	// The ref dereferences the found entity unguarded; DM maps always
	// contain info_player_torri. Guard added for other map types [INFERENCE].
	if( !pSpot )
		return;

	int iPick = RANDOM_LONG( 1, 3 );
	const char *pszSample = NULL;
	if( iMode == 0 )
	{
		if( iPick == 1 ) pszSample = "music/tau01.wav";
		else if( iPick == 2 ) pszSample = "music/tau02.wav";
		else if( iPick == 3 ) pszSample = "music/tau03.wav";
	}
	else if( iMode == 1 )
	{
		if( iPick == 1 ) pszSample = "music/goa01.wav";
		else if( iPick == 2 ) pszSample = "music/goa02.wav";
		else if( iPick == 3 ) pszSample = "music/goa03.wav";
	}
	else if( iMode == 2 )
	{
		if( iPick == 1 ) pszSample = "music/draw01.wav";
		else if( iPick == 2 ) pszSample = "music/draw02.wav";
		else if( iPick == 3 ) pszSample = "music/draw03.wav";
	}
	if( pszSample )
		EMIT_SOUND( ENT( pSpot->pev ), 6, pszSample, 1.0, 0.0 );
}


void SGTC_GlobalText( int iTeam, int iText )
{
	char sz[20];
	if( !SGTC_RoundMessage( sz, iText ) )
		return;

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !pPlayer )
			continue;
		if( iTeam != 0 && g_pGameRules &&
			g_pGameRules->GetTeamIndex( pPlayer->TeamID() ) != iTeam - 1 )
			continue;
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, sz );
	}
}

void SGTC_GlobalTextAll( int iText )
{
	char sz[20];
	if( !SGTC_RoundMessage( sz, iText ) )
		return;

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !pPlayer )
			continue;
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, sz );
	}
}

// ---------------------------------------------------------------------------
// EndRound — nm 0x17b77c (Ghidra 0x18b77c).
// Per-team gmsgTeamScore {WRITE_STRING team name, WRITE_SHORT score,
// WRITE_SHORT 0}, then per alive player gmsgScoreInfo
// {BYTE entindex, SHORT frags, SHORT +0xdf0 (deaths),
//  SHORT +0x4de == 1 ? 6 : +0xa40, SHORT GetTeamIndex(+0xe08)+1,
//  SHORT pev+0x170}, then virtual +0x120 == CStargateTeamDM::NewRound(self).
// ---------------------------------------------------------------------------
static void SGTC_EndRoundScoreInfo( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( pPlayer->entindex() );
		WRITE_SHORT( SGTC_Round_roundf( pPlayer->pev->frags ) );	// ref round(pev+0x164)
		WRITE_SHORT( pPlayer->m_iDeaths ); // ref +0xdf0
		// ref @0x17b91a: if ((player+0x4de ^ 1) != 0) write +0xa40 else write 6.
		WRITE_SHORT( pPlayer->m_bSnagInvisLatch != 1 ? pPlayer->m_iSgtcPortraitShown
				: 6 ); // ref +0x4de / +0xa40
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->TeamID() ) + 1 );
		WRITE_SHORT( (int)pPlayer->pev->team ); // ref pev+0x170
	MESSAGE_END();
}

void CStargateTeamDM::EndRound( void )
{
	m_bRoundOver = TRUE;

	for( int i = 0; i < m_iSGTCTeamCount; i++ )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamScore );
			WRITE_STRING( m_szSGTCTeams[i] );
			WRITE_SHORT( m_iSGTCTeamScores[i] );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "player" ) ) != NULL )
	{
		if( pEnt->IsAlive() )
			SGTC_EndRoundScoreInfo( (CBasePlayer *)pEnt );
	}

	NewRound();
}

// ---------------------------------------------------------------------------
// Goal(int) — nm 0x17b650 (Ghidra 0x18b650).
// Guards: rules+0x73c == 0 (round restart not armed), sgtc_hasplayed == 0 and
// GameModeSet in {0, 2}. Team 1 (0x1dbb90 "Goa'uld"): KeyInPlay = 0,
// PlayMusic(1), endtround = now + 6.0 (rodata 0x1dbba0), team score++. Team 2
// (0x1dbb98 "Torri"): PlayMusic(0), team score++. Always: hasplayed = 1.
// ---------------------------------------------------------------------------
void CStargateTeamDM::Goal( int iTeam )
{
	// ref @0x17b650: guards on rules+0x73c (m_bRoundOver), hasplayed
	// (BSS 0x2368f1), GameModeSet==0 || ==2.
	if( m_bRoundOver != 0 || sgtc_hasplayed != 0 )
		return;
	if( sgtc_GameModeSet != 0 && sgtc_GameModeSet != 2 )
		return;

	if( iTeam == 1 )
	{
		// ref @0x17b6a7: KeyInPlay(BSS 0x2368c6) = 0 first, then PlayMusic(1).
		sgtc_KeyInPlay = 0;
		SGTC_PlayMusic( 1 );
		// ref @0x17b6c5: endtround = gpGlobals->time + 6.0f (rodata 0x1dbba0).
		sgtc_endtround = gpGlobals->time + 6.0f;
		m_iSGTCTeamScores[GetTeamIndex( "Goa'uld" )]++;	// ref name 0x1dbb88
	}
	else if( iTeam == 2 )
	{
		// ref @0x17b70e: KeyInPlay = 0 here too.
		sgtc_KeyInPlay = 0;
		SGTC_PlayMusic( 0 );
		sgtc_endtround = gpGlobals->time + 6.0f;
		m_iSGTCTeamScores[GetTeamIndex( "Torri" )]++;	// ref name 0x1dbb98
	}
	else
	{
		return;
	}

	sgtc_hasplayed = 1;	// ref @0x17b76d
}

// ---------------------------------------------------------------------------
// NewRound body helper — nm 0x184f42 shared tail (Ghidra 0x194f42).
// ---------------------------------------------------------------------------
static void SGTC_NewRoundStripGroundWeapons( void )
{
	// ref @0x184f42 head: edict-array walk; a "weapon_*"-prefixed cache
	// entity with no owner (CBaseEntity +0x4de == 0) gets virtual +0x128
	// (respawn re-cache; the port marks it spawn-ineligible until touched).
	for( int i = 0; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *ed = INDEXENT( i );
		if( FNullEnt( ed ) || !ed->v.classname )
			continue;
		const char *psz = STRING( ed->v.classname );
		if( strncmp( psz, "weapon_", 7 ) != 0 )
			continue;
		CBaseEntity *pEnt = CBaseEntity::Instance( ed );
		if( pEnt && ed->v.owner == 0 ) // ref: CBaseEntity +0x4de == 0
			pEnt->pev->solid = SOLID_NOT; // ref virtual +0x128
	}
}

static void SGTC_NewRoundRemoveClass( const char *pszClassname, BOOL bThink )
{
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, pszClassname ) ) != NULL )
	{
		if( bThink )
			pEnt->Think(); // ref Reset slot +0x1c
		else
			UTIL_Remove( pEnt );
	}
}

static void SGTC_NewRoundRemoveCorpseControllers( void )
{
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "corpse_control" ) ) != NULL )
	{
		((CCorpseController *)pEnt)->RemoveForNewRound();
		pEnt = NULL;
	}
}

static void SGTC_NewRoundRemoveSatchels( void )
{
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "monster_satchel" ) ) != NULL )
	{
		((CSatchelCharge *)pEnt)->Deactivate();
		pEnt = NULL;
	}
}

static void SGTC_NewRoundResetNukes( void )
{
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "game_nuke" ) ) != NULL )
	{
		((CToken *)pEnt)->ResetForNewRound();
		pEnt = NULL;
	}
}

static void SGTC_NewRoundStopMultiManagers( void )
{
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "multi_manager" ) ) != NULL )
	{
		((CMultiManager *)pEnt)->Stop();
		pEnt = NULL;
	}
}

static void SGTC_NewRoundInitEntities( void )
{
	// ref NewRound tail (Ghidra 0x194f42) — Reset-style thinkers, then
	// round-lifecycle cleanup.
	SGTC_NewRoundRemoveClass( "func_breakable", TRUE );
	SGTC_NewRoundRemoveClass( "func_door", TRUE );
	SGTC_NewRoundRemoveClass( "func_door_rotating", TRUE );
	SGTC_NewRoundRemoveClass( "func_button", TRUE );
	SGTC_NewRoundRemoveClass( "func_rot_button", TRUE );
	SGTC_NewRoundStopMultiManagers();	// ref: CMultiManager::Stop (not Think)
	SGTC_NewRoundRemoveClass( "func_rotating", TRUE );
	SGTC_NewRoundRemoveClass( "func_train", TRUE );
	SGTC_NewRoundRemoveClass( "func_illusionary", TRUE );
	SGTC_NewRoundRemoveClass( "func_wall", TRUE );
	SGTC_NewRoundRemoveClass( "func_wall_toggle", TRUE );
	SGTC_NewRoundRemoveClass( "func_pushable", TRUE );
	SGTC_NewRoundRemoveClass( "func_tank", TRUE );
	SGTC_NewRoundRemoveClass( "func_tankgoauld", TRUE );
	SGTC_NewRoundRemoveClass( "func_tanklaser", TRUE );
	SGTC_NewRoundRemoveClass( "func_tankrocket", TRUE );
	SGTC_NewRoundRemoveClass( "func_tankmortar", TRUE );
	SGTC_NewRoundRemoveClass( "trigger_auto", TRUE );
	SGTC_NewRoundRemoveClass( "light", TRUE );
	SGTC_NewRoundRemoveClass( "light_spot", TRUE );
	SGTC_NewRoundResetNukes();		// ref: pev+0x160=30 then Think (+0xc4)
	SGTC_Zones_ResetBombTargets();	// ref: pev+0x160=69 then Think (+0xc4)
	SGTC_NewRoundRemoveClass( "trigger_changetarget", TRUE );
	SGTC_NewRoundRemoveClass( "trigger_multiple", TRUE );
	SGTC_NewRoundRemoveClass( "trigger_once", TRUE );
	SGTC_NewRoundRemoveClass( "multisource", TRUE );
	SGTC_NewRoundRemoveClass( "trigger_teleport", TRUE );
	SGTC_NewRoundRemoveClass( "trigger_adv_teleport", TRUE );
	SGTC_NewRoundRemoveClass( "trigger_hurt", TRUE );
	SGTC_NewRoundRemoveClass( "info_teleport_destination", TRUE );
	SGTC_NewRoundRemoveClass( "special_teleport_in", TRUE );
	SGTC_NewRoundRemoveClass( "env_render", TRUE );
	SGTC_NewRoundRemoveClass( "env_particule", TRUE );
	SGTC_NewRoundRemoveClass( "env_fountain", TRUE );
	SGTC_NewRoundRemoveClass( "env_beam", TRUE );

	SGTC_NewRoundRemoveCorpseControllers();
	SGTC_NewRoundRemoveClass( "effect_bigshield", FALSE );
	SGTC_NewRoundRemoveClass( "monster_shieldemit", FALSE );
	SGTC_NewRoundRemoveClass( "monster_lockdown", FALSE );
	SGTC_NewRoundRemoveClass( "monster_trapbox", FALSE );
	SGTC_NewRoundRemoveClass( "monster_zatboom", FALSE );
	SGTC_NewRoundRemoveClass( "monster_implanted", FALSE );
	SGTC_NewRoundRemoveClass( "monster_skull", FALSE );
	SGTC_NewRoundRemoveClass( "monster_tac", FALSE );
	SGTC_NewRoundRemoveClass( "monster_larve", FALSE );
	SGTC_NewRoundRemoveClass( "monster_mine", FALSE );
	SGTC_NewRoundRemoveSatchels();
	SGTC_NewRoundRemoveClass( "grenade", FALSE );
	SGTC_NewRoundRemoveClass( "lance_proj", FALSE );
	SGTC_NewRoundRemoveClass( "big_lance_proj", FALSE );
	SGTC_NewRoundRemoveClass( "reetou_proj", FALSE );
	SGTC_NewRoundRemoveClass( "effect_shield", FALSE );
	SGTC_NewRoundRemoveClass( "rpg_rocket", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_glock", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_9mmAR", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_medic", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_mp5", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_psg1", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_p90", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_tacgun", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_spas", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_usas", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_rpg", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_invis", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_gacgoa", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_m16", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_handgrenade", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_goaregen", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_mine", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_smokegrenade", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_nervegrenade", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_narcogrenade", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_mustardgrenade", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_c4", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_couteau", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_main", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_zat", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_flashbang", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_lance", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_glstaff", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_batsup", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_coujaf", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_reetou", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_larve", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_zatarc", FALSE );
	SGTC_NewRoundRemoveClass( "weapon_m92s", FALSE );
	SGTC_NewRoundRemoveClass( "larve", FALSE );
	SGTC_NewRoundRemoveClass( "struct_goa_actual", FALSE );
	SGTC_NewRoundRemoveClass( "struct_tau_actual", FALSE );
	SGTC_NewRoundRemoveClass( "struct_goa_actual_sub", FALSE );
	SGTC_NewRoundRemoveClass( "struct_tau_actual_sub", FALSE );
	SGTC_NewRoundRemoveClass( "item_objective", FALSE );
}

static void SGTC_NewRoundCorpseClear( void )
{
	// ref NewRound: corpse walk — pev->deadflag style state reset (0x148/0x14c)
	// then corpse origin moved off-map (4096,4096,4096).
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "corpse" ) ) != NULL )
	{
		pEnt->pev->team = 4;
		pEnt->pev->deadflag = 0;
		pEnt->pev->origin = Vector( 4096, 4096, 4096 );
	}
}

static void SGTC_NewRoundSpritesWithoutMarker( void )
{
	// ref NewRound: for each env_sprite, if no info_marker shares its origin,
	// reset it (ref calls Reset +0x1c).
	CBaseEntity *pSprite = NULL;
	while( ( pSprite = UTIL_FindEntityByClassname( pSprite, "env_sprite" ) ) != NULL )
	{
		BOOL bMatch = FALSE;
		CBaseEntity *pMarker = NULL;
		while( ( pMarker = UTIL_FindEntityByClassname( pMarker, "info_marker" ) ) != NULL )
		{
			if( pMarker->pev->origin == pSprite->pev->origin )
			{
				bMatch = TRUE;
				break;
			}
		}
		if( !bMatch )
			pSprite->Think();
	}
}

// ---------------------------------------------------------------------------
// CStargateTeamDM::PlayerThink — nm 0x17d088 (Ghidra 0x18d088).
// ---------------------------------------------------------------------------
void CStargateTeamDM::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink( pPlayer );
	if( g_fGameOver )
		return;

	const float flNow = gpGlobals->time;

	// env entity iterator (ref param_1 + 0xc88/+0xc90/+0xc94). Index 0 is a
	// NULL slot — ALLOC_PRIVATE zeroes the stage, so start at 1.
	if( pPlayer->m_iSgtcEnvStage == 0 )
		pPlayer->m_iSgtcEnvStage = 1;
	if( pPlayer->m_iSgtcEnvStage < 5 && pPlayer->m_flSgtcEnvNext < flNow )
	{
		static const char *const envClasses[] =
		{
			NULL, "env_particule", "env_rain", "env_fountain", "env_lensflare"
		};
		int stage = pPlayer->m_iSgtcEnvStage;
		EHANDLE hFound = pPlayer->m_hSgtcEnvIterator;
		CBaseEntity *pFound = UTIL_FindEntityByClassname( hFound, envClasses[stage] );
		pPlayer->m_hSgtcEnvIterator = pFound;
		if( pFound )
		{
			// ref calls the found entity's vtable+0x24 — the env particle
			// family's per-client init (SendValueTo) for this player.
			if( FClassnameIs( pFound->pev, "env_particule" ) )
				( (CEnvParticule *)pFound )->SendValueTo( pPlayer );
			else if( FClassnameIs( pFound->pev, "env_rain" ) )
				( (CEnvRain *)pFound )->SendValueTo( pPlayer );
			else if( FClassnameIs( pFound->pev, "env_fountain" ) )
				( (CEnvFountain *)pFound )->SendValueTo( pPlayer );
			else if( FClassnameIs( pFound->pev, "env_lensflare" ) )
				( (CEnvLensFlare *)pFound )->SendValueTo( pPlayer );
		}
		else
			pPlayer->m_iSgtcEnvStage++;
		int lat = SGTC_Round_intconstrain( 1, 10,
			SGTC_Round_roundf( sgtc_cvar_particlelatency.value ) );
		sgtc_cvar_particlelatency.value = lat;
		pPlayer->m_flSgtcEnvNext = sgtc_cvar_particlelatency.value / 10.0f + flNow;
	}

	// +0xa3d round-start tick: set 1s after the stored round time.
	if( ( sgtc_roundtime_official + 1.0f ) == flNow )
		pPlayer->m_bSgtcPadA3D = 1;

	// superweapons (+0x465 preset gate; the once-per-round latch rides
	// +0xa9c — ref pev+0x168 bits 18 (IN_ALT1)/15 (IN_SCORE)).
	if( pPlayer->m_bSgtcPePreset && pPlayer->m_bSgtcLatchA9c != 1 )
	{
		if( !FBitSet( pPlayer->pev->button, IN_ALT1 ) &&
			pPlayer->pev->team == 1 )
		{
			pPlayer->GiveNamedItem( "weapon_glstaff" );
			pPlayer->m_bSgtcLatchA9c = 1;
		}
		else if( !FBitSet( pPlayer->pev->button, IN_SCORE ) &&
			pPlayer->pev->team == 2 )
		{
			pPlayer->GiveNamedItem( "weapon_usas" );
			pPlayer->m_bSgtcLatchA9c = 1;
		}
	}

	// GiveFromMenu (0x4e5/0x4e7 + 0x4ac time gate) — ref bits 13 (IN_RELOAD)
	// and 8 (IN_RIGHT) on pev->button. The 15s window is measured against
	// +0x4ac (m_flSgtcMenuWindow), not +0x4b0 (the menu-10 delay).
	if( pPlayer->m_bSgtcMenuLock && !FBitSet( pPlayer->pev->button, IN_RELOAD ) &&
		!FBitSet( pPlayer->pev->button, IN_RIGHT ) &&
		( flNow > pPlayer->m_flSgtcMenuWindow + 15.0f ||
		  pPlayer->m_bSgtcMenuLockLatch != 1 ) )
	{
		SGTC_GiveFromMenu( pPlayer );
	}

	// ref DM PlayerThink @0x18d088: !IsAlive && !IsObserver &&
	// deadflag == DEAD_DEAD (pev+0x170 == 2) -> StartDeathCam. NOT pev->team.
	if( !pPlayer->IsAlive() && !pPlayer->IsObserver() &&
		pPlayer->pev->deadflag == DEAD_DEAD )
		pPlayer->StartDeathCam();

	if( pPlayer->m_flSgtcMenuDelay != 0.0f && pPlayer->m_flSgtcMenuDelay < flNow )
	{
		pPlayer->ShowVGUIMenu( 10 );
		pPlayer->m_flSgtcMenuDelay = 0.0f;
	}

	// 0x464 + alive → ScreenFade(0x38 slot, own pev, 5000.0, 1).
	if( pPlayer->m_bSgtcPad464 == 1 && sgtc_GameModeSet == 0 &&
		pPlayer->IsAlive() )
	{
		UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 5000.0f, 1.0f, 0, FFADE_IN );
	}

	if( pPlayer->m_flSgtcTeamsTick < flNow )
	{
		// ref DM PlayerThink @0x18d088 ticket flag: name == "Goa'uld" &&
		// 0 < G_tickets || name == "Torri" && 0 < T_tickets, taken BEFORE
		// +0xe08 is rewritten below — this snapshot is the consume-gate input.
		BOOL bTickets = ( !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) &&
			0 < sgtc_G_tickets && !g_fGameOver ) ||
			( !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) &&
			0 < sgtc_T_tickets && !g_fGameOver );

		// ref DM @0x18d088: ShowVGUIMenu(0xb) unless +0x4de, alive, tickets,
		// +0x3e4 (m_bSgtcRoundAlive) non-zero, or game-over. +0x4b0 only
		// arms the separate menu-10 emit above; it is NOT in this gate.
		if( !( pPlayer->m_bSnagInvisLatch || pPlayer->IsAlive() || bTickets ||
			( pPlayer->m_bSgtcRoundAlive || g_fGameOver ) ) )
			pPlayer->ShowVGUIMenu( 11 );	// ref 0xb

		// ref consume gate — verbatim transcription of the mode-dependent
		// chain (0x18d5c0..0x18d830). DM modes (GameModeSet >= 1): consume
		// only when the current team still has tickets OR a pending pick
		// exists (+0x438 != 0 && +0xa44 != -1). Mode 0: consume while the
		// round is < 30s old, a team is empty, or the player has tickets.
		// Alive non-observers and game-over never consume.
		BOOL bConsume;
		if( ( ( ( sgtc_GameModeSet < 1 || !bTickets ) &&
			( ( sgtc_GameModeSet < 1 ||
				( pPlayer->m_iSgtcTeam == 0 || pPlayer->m_iSgtcPortraitSel == -1 ) ) &&
				( sgtc_GameModeSet != 0 ||
					( ( pPlayer->m_bSgtcRoundAlive || pPlayer->m_iSgtcPortraitSel == -1 ) ||
						pPlayer->m_iSgtcTeam < 1 ) ) ) &&
			( ( sgtc_GameModeSet != 0 || sgtc_roundstart <= flNow - 30.0f ) || bTickets ) ) &&
			( ( sgtc_GameModeSet != 0 || m_iSgtcEmptyTeams == 0 ) || bTickets ) ) ||
			( ( pPlayer->IsAlive() && !pPlayer->IsObserver() ) || g_fGameOver ) )
			bConsume = FALSE;
		else
			bConsume = TRUE;

		if( bConsume )
		{
			if( pPlayer->m_iSgtcTeam == 1 )
				strncpy( pPlayer->m_szTeamName, "Torri", TEAM_NAME_LENGTH );
			else if( pPlayer->m_iSgtcTeam == 2 )
				strncpy( pPlayer->m_szTeamName, "Goa'uld", TEAM_NAME_LENGTH );

			if( pPlayer->m_iSgtcTeam != 0 )
			{
				pPlayer->m_iSgtcPrevTeam = pPlayer->m_iSgtcActiveTeam;
				pPlayer->m_iSgtcActiveTeam = pPlayer->m_iSgtcTeam;
			}
			// ref vtable+0x64 = FPlayerCanRespawn(this,player) — the pending-class
			// consumer (@0x18ba08), NOT SetDefaultPlayerTeam. The consume of
			// +0xa44 -> shown class happens inside.
			FPlayerCanRespawn( pPlayer );

			if( 0 < pPlayer->m_iSgtcPortraitSel )
				pPlayer->m_iSgtcPortraitShown = pPlayer->m_iSgtcPortraitSel;

			// ref re-tests tickets on the NEW name (post-consume).
			BOOL bJoinTickets = ( !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) &&
				0 < sgtc_G_tickets ) ||
				( !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) &&
				0 < sgtc_T_tickets );
			if( bJoinTickets )
			{
				ChangePlayerTeam( pPlayer, pPlayer->m_szTeamName, 0, 0 );	// ref vtable+0xec
				int iRespawn = SGTC_Round_intconstrain( 3, 120,
					SGTC_Round_roundf( sgtc_cvar_respawntime.value ) );
				if( sgtc_GameModeSet == 0 || !bTickets )
					iRespawn = 1;
				// ref: respawn only dead OBSERVERS; the mp_respawntime
				// delay applies only in modes >= 1 (mode 0 respawns at
				// once). Ref +0xc7c is PlayerDeathThink's m_fDeadTime;
				// the old m_flSgtcDeathTime member was a mislabel of this
				// offset and has been deleted.
				if( !pPlayer->IsAlive() && pPlayer->IsObserver() &&
					( sgtc_GameModeSet == 0 ||
						flNow - iRespawn > pPlayer->m_fDeadTime ) )
					respawn( pPlayer->pev, 0 );
			}
		}
		pPlayer->m_flSgtcTeamsTick = flNow + 1.0f;
	}

	if( pPlayer->IsAlive() )
		pPlayer->m_bSgtcRoundAlive = 1;
	// (ref tail slot +0x34 is a virtual forwarder, no extra base call here —
	// the base was already invoked at function entry.)
}

// ---------------------------------------------------------------------------
// CStargateTeamCP::PlayerThink — nm 0x17008c (Ghidra 0x18008c).
// Shares the DM env/death-cam/menu frame; diverges in the team/ticket block
// (capture types 2/4 gate team-lock + respawn), no DM supergun branch.
// ---------------------------------------------------------------------------
void CStargateTeamCP::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink( pPlayer );
	if( g_fGameOver )
		return;

	const float flNow = gpGlobals->time;

	// env entity iterator (identical to DM).
	if( pPlayer->m_iSgtcEnvStage == 0 )
		pPlayer->m_iSgtcEnvStage = 1;
	if( pPlayer->m_iSgtcEnvStage < 5 && pPlayer->m_flSgtcEnvNext < flNow )
	{
		static const char *const envClasses[] =
		{
			NULL, "env_particule", "env_rain", "env_fountain", "env_lensflare"
		};
		int stage = pPlayer->m_iSgtcEnvStage;
		EHANDLE hFound = pPlayer->m_hSgtcEnvIterator;
		CBaseEntity *pFound = UTIL_FindEntityByClassname( hFound, envClasses[stage] );
		pPlayer->m_hSgtcEnvIterator = pFound;
		if( pFound )
		{
			// ref calls the found entity's vtable+0x24 — per-client init
			// (SendValueTo) for this player.
			if( FClassnameIs( pFound->pev, "env_particule" ) )
				( (CEnvParticule *)pFound )->SendValueTo( pPlayer );
			else if( FClassnameIs( pFound->pev, "env_rain" ) )
				( (CEnvRain *)pFound )->SendValueTo( pPlayer );
			else if( FClassnameIs( pFound->pev, "env_fountain" ) )
				( (CEnvFountain *)pFound )->SendValueTo( pPlayer );
			else if( FClassnameIs( pFound->pev, "env_lensflare" ) )
				( (CEnvLensFlare *)pFound )->SendValueTo( pPlayer );
		}
		else
			pPlayer->m_iSgtcEnvStage++;
		int lat = SGTC_Round_intconstrain( 1, 10,
			SGTC_Round_roundf( sgtc_cvar_particlelatency.value ) );
		sgtc_cvar_particlelatency.value = lat;
		pPlayer->m_flSgtcEnvNext = sgtc_cvar_particlelatency.value / 10.0f + flNow;
	}

	if( ( sgtc_roundtime_official + 1.0f ) == flNow )
		pPlayer->m_bSgtcPadA3D = 1;

	// GiveFromMenu (0x4e5 + 0x4ac gate) — no supergun branch in CP.
	// ref pev+0x168 bits 13/8 = IN_RELOAD / IN_RIGHT. The 15s window is
	// measured against +0x4ac (m_flSgtcMenuWindow), not +0x4b0.
	if( pPlayer->m_bSgtcMenuLock && !FBitSet( pPlayer->pev->button, IN_RELOAD ) &&
		!FBitSet( pPlayer->pev->button, IN_RIGHT ) &&
		( flNow > pPlayer->m_flSgtcMenuWindow + 15.0f ||
		  pPlayer->m_bSgtcMenuLockLatch != 1 ) )
	{
		SGTC_GiveFromMenu( pPlayer );
	}

	// ref CP PlayerThink @0x18008c: !IsAlive && !IsObserver &&
	// deadflag == DEAD_DEAD (pev+0x170 == 2) -> StartDeathCam. NOT pev->team.
	if( !pPlayer->IsAlive() && !pPlayer->IsObserver() &&
		pPlayer->pev->deadflag == DEAD_DEAD )
		pPlayer->StartDeathCam();

	if( pPlayer->m_flSgtcMenuDelay != 0.0f && pPlayer->m_flSgtcMenuDelay < flNow )
	{
		pPlayer->ShowVGUIMenu( 10 );
		pPlayer->m_flSgtcMenuDelay = 0.0f;
	}

	pPlayer->m_bSgtcPortraitsFrozen = 1;
	if( pPlayer->m_bSgtcMenuLock )
		SGTC_GiveFromMenu( pPlayer );

	// 0x464 + type 2 + alive: the capture-holding team gets the fade tick
	// (teamed 1 -> "Goa'uld" team string, teamed 2 -> "Torri").
	if( pPlayer->m_bSgtcPad464 == 1 && sgtc_type == 2 && pPlayer->IsAlive() )
	{
		BOOL bFade = FALSE;
		if( sgtc_teamed == 1 && !stricmp( pPlayer->TeamID(), "Goa'uld" ) )
			bFade = TRUE;
		if( sgtc_teamed == 2 && !stricmp( pPlayer->TeamID(), "Torri" ) )
			bFade = TRUE;
		if( bFade )
		{
			// ref vtable+0x38: per-player fade helper (own pev, 5000.0, 1).
			UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 5000.0f, 1.0f, 0, FFADE_IN );
		}
	}

	// ref CP PlayerThink @0x18008c gate: skip only when the 1s tick is due OR
	// the player is alive — observers and dead players consume below.
	if( flNow > pPlayer->m_flSgtcTeamsTick && !pPlayer->IsAlive() )
	{
		// ref CP @0x18008c: +0x3e4 != 1 && !g_fGameOver. +0x4b0 is menu-10 only.
		if( pPlayer->m_bSgtcRoundAlive != 1 && !g_fGameOver )
			pPlayer->ShowVGUIMenu( 11 );	// ref 0xb

		if( pPlayer->m_iSgtcTeam == 1 )
			strncpy( pPlayer->m_szTeamName, "Torri", TEAM_NAME_LENGTH );
		else if( pPlayer->m_iSgtcTeam == 2 )
			strncpy( pPlayer->m_szTeamName, "Goa'uld", TEAM_NAME_LENGTH );

		if( pPlayer->m_iSgtcTeam != 0 )
		{
			pPlayer->m_iSgtcPrevTeam = pPlayer->m_iSgtcActiveTeam;
			pPlayer->m_iSgtcActiveTeam = pPlayer->m_iSgtcTeam;
		}
		// ref CP vtable+0x64 = FPlayerCanRespawn(this,player) — the pending-class
		// consumer (@0x17ea2a), NOT SetDefaultPlayerTeam. The consume of
		// +0xa44 -> shown class happens inside.
		FPlayerCanRespawn( pPlayer );

		if( 0 < pPlayer->m_iSgtcPortraitSel )
			pPlayer->m_iSgtcPortraitShown = pPlayer->m_iSgtcPortraitSel;

		// ref: either team NAME alone arms the team change — no ticket test
		// in CP (unlike the DM variant at 0x18d088).
		BOOL bJoin = !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) ||
			!strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 );

		// type 2 + 0x464: the capture-holding team is excluded.
		if( sgtc_type == 2 && pPlayer->m_bSgtcPad464 )
		{
			if( sgtc_teamed == 1 && !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) )
				bJoin = FALSE;
			if( sgtc_teamed == 2 && !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) )
				bJoin = FALSE;
		}
		// type 4 + 0x464: rules+0x734 (m_iSgtcCPSide) excludes the chosen team.
		if( sgtc_type == 4 && pPlayer->m_bSgtcPad464 )
		{
			if( m_iSgtcCPSide != 0 && !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) )
				bJoin = FALSE;
			if( m_iSgtcCPSide == 0 && !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) )
				bJoin = FALSE;
		}

		if( bJoin )
		{
			ChangePlayerTeam( pPlayer, pPlayer->m_szTeamName, 0, 0 );	// ref vtable+0xec
			// ref: respawn only dead OBSERVERS (the fresh-join path).
			if( !pPlayer->IsAlive() && pPlayer->IsObserver() )
				respawn( pPlayer->pev, 0 );
		}
		pPlayer->m_flSgtcTeamsTick = flNow + 1.0f;
	}

	if( pPlayer->IsAlive() )
		pPlayer->m_bSgtcRoundAlive = 1;
	// (ref tail slot +0x34 is a virtual forwarder, no extra base call here —
	// the base was already invoked at function entry.)
}

// ---------------------------------------------------------------------------
// SGTC_NewRoundCommon — the ~700-line shared body of both NewRound variants
// (ref nm 0x184f42 / 0x177b34): entity reset, corpse clear, round state
// commit, key/objective restore, per-player re-team + respawn, tournament
// fan-out, skulls, ticket broadcast and the ref's virtual +0x120 tail call.
// bCaptureMode selects the CP-only divergences.
// ---------------------------------------------------------------------------
void CStargateTeamDM::SGTC_NewRoundCommon( BOOL bCaptureMode )
{
	// Ref reads the registered cvar_t .value live (GameDLLInit-registered);
	// refresh the mirrors so RCON/config changes track (Pass4/REA verified).
	sgtc_tourny        = sgtc_cvar_tournament.value;
	sgtc_allowmodevote = sgtc_cvar_modevoting.value;
	sgtc_crazymode     = sgtc_cvar_skulls.value;
	sgtc_gamemodex     = sgtc_cvar_gamemode.value;
	sgtc_roundtime     = CVAR_GET_FLOAT( "mp_roundtime" );
	sgtc_type          = 0;
	sgtc_teamed        = 0;

	// (b) strip every ground "weapon_*" cache entity with no owner.
	SGTC_NewRoundStripGroundWeapons();

	RecountTeams( FALSE );

	// (e) empty gmsgNewRnd broadcast.
	MESSAGE_BEGIN( MSG_ALL, gmsgNewRnd );
	MESSAGE_END();

	// (f) round state reset.
	for( int i = 0; i < 5; i++ )
	{
		sgtc_LastStateI[i] = 0;
		sgtc_LastState[i] = 0;
	}
	sgtc_round_official = 0;

	// (g) tournament gate.
	if( sgtc_tourny != 0.0f )
	{
		int avg = ( sgtc_G_players_focused + sgtc_T_players_focused ) / 2;
		BOOL fair = sgtc_G_players_focused <= avg + 2 &&
			sgtc_T_players_focused <= avg + 2;
		if( sgtc_G_players_focused < 4 || sgtc_T_players_focused < 4 || !fair )
		{
			sgtc_fadetoblack = 0;
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "-=== Tournament Round Reject ===-\n" );
			if( sgtc_G_players_focused < 4 || sgtc_T_players_focused == 0 )
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Not enough players!\n" );
			if( !fair )
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Teams Unfair!\n" );
		}
		else
		{
			sgtc_fadetoblack = 1;
			sgtc_round_official = 1;
			if( !bCaptureMode )
			{
				if( sgtc_GameModeSet == 1 )
					sgtc_G_tickets = sgtc_T_tickets = avg * 7;
				else if( sgtc_GameModeSet == 0 )
					sgtc_roundtime = 300.0f;
				else if( sgtc_GameModeSet == 3 )
				{
					sgtc_GameModeSet = 2;
					sgtc_G_tickets = sgtc_T_tickets = avg * 7;
				}
				else if( sgtc_GameModeSet == 4 )
					sgtc_G_tickets = sgtc_T_tickets = avg * 7;
			}
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY,
				"-=== Tournament Round Accepted ===-\n" );
		}
	}

	// (h)+(i) entity reset + removal.
	SGTC_NewRoundInitEntities();
	SGTC_NewRoundCorpseClear();
	SGTC_NewRoundSpritesWithoutMarker();

	// entity_spawn_round markers fire Event() (CEntSpawn, sgtc_misc).
	CBaseEntity *pSpawn = NULL;
	while( ( pSpawn = UTIL_FindEntityByClassname( pSpawn,
			"entity_spawn_round" ) ) != NULL )
	{
		( (CEntSpawn *)pSpawn )->Event();
	}

	sgtc_mKill = Vector( 0, 0, 0 );
	BOOL bModeError = sgtc_GameModeSet > 4;
	if( bModeError )
	{
		ALERT( at_console, "Null #:Game Mode, Defaulting\n" );
		sgtc_GameModeSet = 0;
	}

	// (j) masterkey / firstplace / item_objective restore.
	if( SGTC_Round_vne( sgtc_masterkey, g_vecZero ) )
	{
		if( sgtc_GameModeSet < 1 )
		{
			sgtc_firstplace = sgtc_masterkey;
		}
		else
		{
			CBaseEntity *pObj = NULL;
			while( ( pObj = UTIL_FindEntityByClassname( pObj,
					"item_objective" ) ) != NULL )
				UTIL_Remove( pObj );
			sgtc_firstplace = Vector( 4096, 4096, 4096 );
		}
		if( SGTC_Round_vne( sgtc_firstplace, g_vecZero ) )
		{
			CBaseEntity *pObj = NULL;
			while( ( pObj = UTIL_FindEntityByClassname( pObj,
					"item_objective" ) ) != NULL )
				UTIL_Remove( pObj );
			CBaseEntity::Create( "item_objective", sgtc_firstplace, g_vecZero, NULL );
		}
	}

	m_bRoundOver = FALSE;
	// ref writes: +0x750 = -5 (0xfffffffb), +0x74c = 0, +0x73c = 0, +0x754 = 0.
	m_iSgtcFirstTicks = -5;
	m_iSgtcRoundTicks = 0;
	// ref: LastKeyPlace = firstplace (BSS 0x2368e0)
	{
		extern Vector sgtc_LastKeyPlace;
		sgtc_LastKeyPlace = sgtc_firstplace;
	}
	sgtc_KeyInPlay = 0;

	m_flSgtcRoundTime = CVAR_GET_FLOAT( "mp_roundtime" );	// ref NewRound @0x194f42: this+0x728 = mp_roundtime
	sgtc_roundtime = m_flSgtcRoundTime;	// refresh the mirror from the live cvar
	if( 0 < sgtc_GameModeSet )
		m_flSgtcRoundTime = 599.0f;	// ref GameModeSet>0 default (clamped below)
	sgtc_G_tickets = SGTC_Round_intconstrain( 0, 99,
		SGTC_Round_roundf( sgtc_cvar_tickets.value ) );
	sgtc_T_tickets = sgtc_G_tickets;
	m_flSgtcRoundTime = (float)SGTC_Round_intconstrain( 30, 599,
		SGTC_Round_roundf( m_flSgtcRoundTime ) );
	sgtc_roundstart = gpGlobals->time;	// ref this+0x72c round-start tick
	sgtc_roundtime_official = sgtc_roundstart;	// port anchor read at PlayerThink :538/:684
	m_flSgtcRoundEnd = m_flSgtcRoundTime + gpGlobals->time;	// ref this+0x724 = mp_roundtime + roundstart

	if( sgtc_GameModeSet == 4 )
	{
		sgtc_lcaptain[0] = 0.0f;
		sgtc_lcaptain[1] = 0.0f;
		// ref SetCap(3) — capture captain mode marker (lcaptain reset path)
		m_iSgtcFirstTicks = 3;
	}

	if( sgtc_friendlyfire != 0.0f )
		sgtc_friendlyfire = 0.0f;

	// (k) DM mode 2: guarantee one struct_goa/struct_tau and spawn the
	// actual/sub captain structures.
	if( !bCaptureMode && sgtc_GameModeSet == 2 )
	{
		sgtc_G_tickets = 100;
		sgtc_T_tickets = 100;
		// guarantee: count struct_goa / struct_tau; while either is missing
		// (max 10 rounds) clone one from a random team spawn spot.
		for( int tries = 0; tries < 10; tries++ )
		{
			int goa = 0, tau = 0;
			CBaseEntity *pEnt = NULL;
			while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "struct_goa" ) ) != NULL )
				goa++;
			pEnt = NULL;
			while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "struct_tau" ) ) != NULL )
				tau++;
			if( goa > 0 && tau > 0 )
				break;
			if( goa < 1 )
			{
				CBaseEntity *pSpot = NULL, *pPick = NULL;
				int count = 0, pick = SGTC_Round_rand( 1, 5 );
				while( ( pSpot = UTIL_FindEntityByClassname( pSpot,
						"info_player_goauld" ) ) != NULL )
				{
					count++;
					if( count == pick )
						pPick = pSpot;
				}
				if( pPick )
				{
					Vector spotOrg = pPick->pev->origin;
					Vector traceOrg = spotOrg;
					traceOrg.z += 10.0f;
					// ref Endo_FloorTrace drops the spot onto the floor.
					TraceResult tr;
					UTIL_TraceLine( traceOrg,
						traceOrg + Vector( 0, 0, -1024 ), ignore_monsters, NULL, &tr );
					spotOrg = tr.vecEndPos;
					CBaseEntity::Create( "struct_goa", spotOrg, g_vecZero, NULL );
				}
			}
			if( tau < 1 )
			{
				CBaseEntity *pSpot = NULL, *pPick = NULL;
				int count = 0, pick = SGTC_Round_rand( 1, 5 );
				while( ( pSpot = UTIL_FindEntityByClassname( pSpot,
						"info_player_torri" ) ) != NULL )
				{
					count++;
					if( count == pick )
						pPick = pSpot;
				}
				if( pPick )
				{
					Vector spotOrg = pPick->pev->origin;
					Vector traceOrg = spotOrg;
					traceOrg.z += 10.0f;
					TraceResult tr;
					UTIL_TraceLine( traceOrg,
						traceOrg + Vector( 0, 0, -1024 ), ignore_monsters, NULL, &tr );
					spotOrg = tr.vecEndPos;
					CBaseEntity::Create( "struct_tau", spotOrg, g_vecZero, NULL );
				}
			}
		}

		int goa = 0, tau = 0;
		CBaseEntity *pEnt = NULL;
		while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "struct_goa" ) ) != NULL )
			goa++;
		pEnt = NULL;
		while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "struct_tau" ) ) != NULL )
			tau++;
		if( tau < 1 && goa < 1 )
		{
			ALERT( at_console, "Null #:struct_goa/struct_tau \n" );
			sgtc_GameModeSet = 0;
			bModeError = TRUE;
		}
		else
		{
			// actual + actual_sub captain structures at the random structs.
			CBaseEntity *pPick = NULL;
			int count = 0, pick = SGTC_Round_rand( 1, goa );
			pEnt = NULL;
			while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "struct_goa" ) ) != NULL )
			{
				count++;
				if( count == pick )
					pPick = pEnt;
			}
			if( pPick )
			{
				CBaseEntity::Create( "struct_goa_actual",
					pPick->pev->origin, pPick->pev->angles, NULL );
				CBaseEntity::Create( "struct_goa_actual_sub",
					pPick->pev->origin, pPick->pev->angles, NULL );
			}
			count = 0;
			pick = SGTC_Round_rand( 1, tau );
			pEnt = NULL;
			while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "struct_tau" ) ) != NULL )
			{
				count++;
				if( count == pick )
					pPick = pEnt;
			}
			if( pPick )
			{
				CBaseEntity::Create( "struct_tau_actual",
					pPick->pev->origin, pPick->pev->angles, NULL );
				CBaseEntity::Create( "struct_tau_actual_sub",
					pPick->pev->origin, pPick->pev->angles, NULL );
			}
			bModeError = FALSE;
		}
	}

	// (l) per-player round restart: re-team, strip, respawn, tournament text.
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( pPlayer && !IsValidTeam( pPlayer->TeamID() ) &&
			pPlayer->m_iSgtcTeam == 0 )
			pPlayer = NULL;

		if( !pPlayer )
			continue;

		pPlayer->m_bSgtcPad464 = 0;
		pPlayer->m_bSgtcLatchA9c = 0;	// ref +0xa9c — superweapon re-arm each round
		if( !bCaptureMode && sgtc_GameModeSet != 4 )
			pPlayer->m_bSgtcPePreset = 0;
		pPlayer->ResetPortrait( 1 );

		if( pPlayer->m_iSgtcTeam == 1 )
			strncpy( pPlayer->m_szTeamName, "Torri", TEAM_NAME_LENGTH );
		else if( pPlayer->m_iSgtcTeam == 2 )
			strncpy( pPlayer->m_szTeamName, "Goa'uld", TEAM_NAME_LENGTH );

		if( pPlayer->m_iSgtcTeam > 0 )
		{
			pPlayer->m_iSgtcPrevTeam = pPlayer->m_iSgtcActiveTeam;
			pPlayer->m_iSgtcActiveTeam = pPlayer->m_iSgtcTeam;
			pPlayer->pev->team = pPlayer->m_iSgtcActiveTeam;
		}

		SetDefaultPlayerTeam( pPlayer ); // ref gamerules vtable+0x100

		if( 0 < pPlayer->m_iSgtcPortraitSel )
			pPlayer->m_iSgtcPortraitShown = pPlayer->m_iSgtcPortraitSel;

		ChangePlayerTeam( pPlayer, pPlayer->TeamID(), 0, 0 ); // vtable+0x94
		pPlayer->RemoveAllItems( TRUE );
		SetDefaultPlayerTeam( pPlayer ); // ref g_pGameRules vtable+0xf0
		GetPlayerSpawnSpot( pPlayer );   // ref g_pGameRules vtable+0x6c

		// ref: DM loop calls SetHudRoundTime(ROUND(+0x728),1)+respawn;
		// CP loop calls SetHudRoundTime(type==2 ? (0,0) : (ROUND(+0x728),1))
		// then respawn, then the type-4 state writes (+0x934=6, +0x9ac[29]=0).
		if( !bCaptureMode )
		{
			pPlayer->SetHudRoundTime( (int)m_flSgtcRoundTime, 1 );
		}
		else if( sgtc_type == 2 )
		{
			pPlayer->SetHudRoundTime( 0, 0 );
		}
		else
		{
			pPlayer->SetHudRoundTime( (int)m_flSgtcRoundTime, 1 );
			if( sgtc_type == 4 )
			{
				pPlayer->m_iNukeBonus = 6; // ref +0x934 = 6
				for( int t = 0; t < 29; t++ )
				pPlayer->m_iSgtcGondArgs[t] = 0; // ref +0x9ac[29] = 0 — gond args, not m_iProgressTeam
			}
		}
		respawn( pPlayer->pev, 0 );

		if( sgtc_tourny != 0.0f )
		{
			if( sgtc_round_official == 0 )
				pPlayer->SendGlobalTextIndex( 12 );
			else
				pPlayer->SendGlobalTextIndex( 11 );

			if( bModeError )
				pPlayer->SendGlobalTextIndex( 11 );
			else if( sgtc_GameModeSet == 0 )
				pPlayer->SendGlobalTextIndex( 16 );
			else if( sgtc_GameModeSet == 1 )
				pPlayer->SendGlobalTextIndex( 17 );
			else if( sgtc_GameModeSet == 2 )
				pPlayer->SendGlobalTextIndex( 18 );
			else if( sgtc_GameModeSet == 3 )
				pPlayer->SendGlobalTextIndex( 19 );
			else if( sgtc_GameModeSet == 4 )
				pPlayer->SendGlobalTextIndex( 20 );
		}
		else if( pPlayer ) // ref else: spectator HUD round time, mode 2
		{
			pPlayer->SetHudRoundTime( (int)m_flSgtcRoundTime, 2 );
		}
	}

	// (m) CP divergences.
	if( bCaptureMode )
	{
		if( sgtc_type != 4 )
		{
			sgtc_G_tickets = 0;
			sgtc_T_tickets = 0;
		}
		sgtc_under = 1;
		if( sgtc_type == 3 || sgtc_type == 4 )
		{
			m_flSgtcRoundTime = 599.0f;
			m_flSgtcRoundEnd = gpGlobals->time + m_flSgtcRoundTime;
		}
		else if( sgtc_type == 2 )
		{
			m_flSgtcRoundTime = 60.0f;
			m_flSgtcRoundEnd = gpGlobals->time + 60.0f;
		}
		sgtc_roundstart = gpGlobals->time;
	}
	sgtc_mKill = Vector( 0, 0, 0 );

	// (n) crazymode skulls.
	if( sgtc_crazymode != 0.0f )
	{
		CBasePlayer *pFirst = (CBasePlayer *)UTIL_PlayerByIndex( 1 );
		if( pFirst )
		{
			for( int s = 0; s < 31; s++ )
			{
				Vector vecOrg(
					(float)SGTC_Round_rand( -4000, 4000 ),
					(float)SGTC_Round_rand( -4000, 4000 ),
					(float)SGTC_Round_rand( -4000, 4000 ) );
				CBaseEntity::Create( "monster_skull", vecOrg, g_vecZero, NULL );
			}
		}
	}

	// (o) ticket broadcasts.
	if( bCaptureMode )
	{
		if( sgtc_type == 4 )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgGtickets );
				WRITE_BYTE( sgtc_G_tickets );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_ALL, gmsgTtickets );
				WRITE_BYTE( sgtc_T_tickets );
			MESSAGE_END();
		}
	}
	else if( 0 < sgtc_GameModeSet )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgGtickets );
			WRITE_BYTE( sgtc_G_tickets );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgTtickets );
			WRITE_BYTE( sgtc_T_tickets );
		MESSAGE_END();
	}

	// (p) round state commit.
	sgtc_hasplayed = 0;
	sgtc_nrcooler = gpGlobals->time + 2.0f;
	// ref CGlobalState::ClearBodyQue — corpse-queue reset (gGlobalState).
	gGlobalState.ClearStates();

	// (q) ref tail: virtual +0x120 — re-enter NewRound once when the mode
	// defaulted. The nrcooler armed above would self-block the re-entry.
	if( bModeError )
	{
		sgtc_nrcooler = 0.0f;
		NewRound();
	}
}

// ---------------------------------------------------------------------------
// CStargateTeamDM::NewRound — nm 0x184f42 (Ghidra 0x194f42).
// ---------------------------------------------------------------------------
void CStargateTeamDM::NewRound( void )
{
	if( g_fGameOver || sgtc_nrcooler > gpGlobals->time )
		return;

	// (d) mode vote.
	if( m_iSgtcModeVote >= 0 && sgtc_tourny == 0.0f &&
		sgtc_allowmodevote != 0.0f )
	{
		BOOL changed = m_iSgtcModeVote != sgtc_GameModeSet;
		sgtc_GameModeSet = m_iSgtcModeVote;
		m_iSgtcModeVote = -1;
		if( changed )
			SGTC_GlobalTextAll( 0 );
	}

	SGTC_NewRoundCommon( FALSE );
}

// ---------------------------------------------------------------------------
// CStargateTeamCP::NewRound — nm 0x177b34 (Ghidra 0x187b34).
// CP divergences: no mode-vote block, zone_capture + info_cap_rules reads
// (teamed/type), tickets zeroed unless type 4, type-driven round windows and
// 0x934/0x9ac per-player state in the loop.
// ---------------------------------------------------------------------------
void CStargateTeamCP::NewRound( void )
{
	if( g_fGameOver || sgtc_nrcooler > gpGlobals->time )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgNewRnd );
	MESSAGE_END();

	for( int i = 0; i < 5; i++ )
	{
		sgtc_LastStateI[i] = 0;
		sgtc_LastState[i] = 0;
	}
	sgtc_round_official = 0;

	if( sgtc_tourny != 0.0f )
	{
		int avg = ( sgtc_G_players_focused + sgtc_T_players_focused ) / 2;
		BOOL fair = sgtc_G_players_focused <= avg + 2 &&
			sgtc_T_players_focused <= avg + 2;
		if( sgtc_G_players_focused < 4 || sgtc_T_players_focused < 4 || !fair )
		{
			sgtc_fadetoblack = 0;
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY,
				"-=== Tournament Round Reject ===-\n" );
			if( sgtc_G_players_focused < 4 || sgtc_T_players_focused == 0 )
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Not enough players!\n" );
			if( !fair )
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Teams Unfair!\n" );
		}
		else
		{
			sgtc_fadetoblack = 1;
			sgtc_round_official = 1;
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY,
				"-=== Tournament Round Accepted ===-\n" );
		}
	}

	// zone_capture reset (0x240/0x244/0x250 zero, pev->health 80, switch tick).
	// CTriggerCapture lives TU-local in sgtc_zones.cpp; NewRound drives the
	// same state through its public fields by going through the entity walk
	// the ref uses. CTriggerCapture::CaptureThink reads pev->health and the
	// flag fields; the port keeps the reset in sgtc_round via its Spawn-side
	// accessors [ref fields +0x240/+0x244/+0x250/+0x258 mirrored there].
		SGTC_Zones_ResetCaptureZones();
	// info_cap_rules: teamed/type globals (m_team / "type" KeyValue).
	// CCapInfo is TU-local to sgtc_rules.cpp; read the state through the
	// same class keys via the exposed setter.
		SGTC_Rules_ReadCapInfo();

	SGTC_NewRoundCommon( TRUE );
}

// ---------------------------------------------------------------------------
// PlayerKilled additions — nm 0x17fb4a (Ghidra 0x18fb4a).
// Ref order: Penalty FIRST, then killpoints/vicpoints scoring, ticket
// decrement + broadcast for GameModeSet 1/3/4, and — only when
// !m_bDisableDeathPenalty (ref this+0x10) — stock PlayerKilled + RecountTeams.
// ---------------------------------------------------------------------------
void CStargateTeamDM::Penalty( CBasePlayer *pVictim, entvars_t *pKillerEv )
{
	// ref Penalty(CBasePlayer*, entvars_t*) @Ghidra 0x18ace2:
	// guards: killer ev non-null, victim non-null, killer entity alive
	// (vtable+0xa8 IsAlive), killer entity != victim entity,
	// victim +0x465 (m_bSgtcPePreset) set.
	if( !pVictim || !pKillerEv )
		return;
	CBaseEntity *pKiller = CBaseEntity::Instance( pKillerEv );
	if( !pKiller || !pKiller->IsAlive() )
		return;
	if( pKiller == pVictim || !pVictim->m_bSgtcPePreset )
		return;

	// Walk every "player" entity SKIPPING THE VICTIM (ref @0x18ad88:
	// CMP param_1(victim) vs scan). For each scanned player:
	//   same pev->team (+0x1ac) as victim && alive (vtable+0x94 IsAlive)
	//     -> vtable+0x38 = Use( scan, killerEv, useType=0x800000, 1000.0f )
	//        (stack pushes verified @0x18adec-0x18dfc: last push 0x800000
	//        is useType; rodata 0x447a0000 = 1000.0f is value)
	//   same team  -> SendGlobalTextIndex(10)
	//   other team -> SendGlobalTextIndex(9)
	const int victimTeam = (int)pVictim->pev->team;
	CBaseEntity *pScan = NULL;
	while( ( pScan = UTIL_FindEntityByClassname( pScan, "player" ) ) != NULL )
	{
		if( pScan == pVictim )
			continue;
		CBasePlayer *pMate = (CBasePlayer *)pScan;

		if( (int)pScan->pev->team == victimTeam && pMate->IsAlive() )
			pMate->Use( pMate, pKiller, (USE_TYPE)0x800000, 1000.0f );

		if( (int)pScan->pev->team == victimTeam )
			pMate->SendGlobalTextIndex( 10 );
		else
			pMate->SendGlobalTextIndex( 9 );
	}
}

void CStargateTeamDM::PlayerKilled( CBasePlayer *pVictim,
	entvars_t *pKiller, entvars_t *pInflictor )
{
	// ref @0x18fb66: Penalty(victim, killerEv) runs FIRST, before stock.
	Penalty( pVictim, pKiller );

	if( pKiller )
	{
		CBaseEntity *pKillerEnt = CBaseEntity::Instance( pKiller );
		// ref @0x18fb4a / @0x18262a: killer != victim && killer team (+0x1ac)
		// != victim team -> killer +0x934 += ROUND(killpoints), victim +0x934
		// += ROUND(vicpoints). +0x934 is the SGTC point score (CBasePlayer
		// m_iNukeBonus — the menu-12 "Total" budget byte), NOT pev->frags.
		if( pKillerEnt && pKillerEnt != pVictim &&
			pKillerEnt->pev->team != pVictim->pev->team )
		{
			( (CBasePlayer *)pKillerEnt )->m_iNukeBonus +=
				SGTC_Round_roundf( sgtc_cvar_killpoints.value );
			pVictim->m_iNukeBonus +=
				SGTC_Round_roundf( sgtc_cvar_diepoints.value );
		}
	}

	// monster_skull drop on death (PlayerKilled strings).
	CBaseEntity::Create( "monster_skull", pVictim->pev->origin,
		pVictim->pev->angles, NULL );

	// ref +0x4de flag reset: the invis snag latch is cleared on death.
	pVictim->m_bSnagInvisLatch = 0;

	// ref @0x18fe70: GameModeSet 1/3/4 -> victim team ticket decrement +
	// gmsgGtickets/gmsgTtickets WRITE_BYTE broadcasts.
	if( sgtc_GameModeSet == 1 || sgtc_GameModeSet == 3 || sgtc_GameModeSet == 4 )
	{
		if( (int)pVictim->pev->team == 1 && sgtc_G_tickets > 0 )
			sgtc_G_tickets--;
		if( (int)pVictim->pev->team == 2 && sgtc_T_tickets > 0 )
			sgtc_T_tickets--;

		MESSAGE_BEGIN( MSG_ALL, gmsgGtickets );
			WRITE_BYTE( sgtc_G_tickets );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgTtickets );
			WRITE_BYTE( sgtc_T_tickets );
		MESSAGE_END();
	}

	// ref this+0x10 (m_bDisableDeathPenalty): forced ChangePlayerTeam kills
	// suppress the stock death handling entirely.
	if( !m_bDisableDeathPenalty )
	{
		CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
		RecountTeams( TRUE );
	}
}

// ---------------------------------------------------------------------------
// InitHUD — ref @0x19d126 (Unas, ShowVGUIMenu(2)) / @0x17d1f0 (Invis,
// ShowVGUIMenu(11)): both = stock base + TeamNames + RecountTeams + VGUI
// team menu + per-player TeamInfo fan-out.
// ---------------------------------------------------------------------------
void CStargateUnas::InitHUD( CBasePlayer *pPlayer )
{
	// ref @0x18a126: CHalfLifeMultiplay::InitHUD, MSG_ONE TeamNames, then
	// RecountTeams(0), ShowVGUIMenu(2), then per-player TeamInfo fan-out.
	CHalfLifeMultiplay::InitHUD( pPlayer );
	SGTC_SendTeamNames( pPlayer );
	RecountTeams( FALSE );
	pPlayer->ShowVGUIMenu( 2 );

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if( plr && IsValidTeam( plr->TeamID() ) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict() );
				WRITE_BYTE( plr->entindex() );
				WRITE_STRING( plr->TeamID() );
			MESSAGE_END();
		}
	}
}

void CStargateInvis::InitHUD( CBasePlayer *pPlayer )
{
	// ref @0x16d1f0: CHalfLifeMultiplay::InitHUD, then MSG_ONE TeamNames
	// (gmsg slot -0x794 → gmsgTeamNames) with all m_teamlist entries, then
	// RecountTeams(0), then ShowVGUIMenu(0xb), then per-player MSG_ONE
	// TeamInfo {BYTE entindex, STRING TeamID} while IsValidTeam(TeamID).
	CHalfLifeMultiplay::InitHUD( pPlayer );
	SGTC_SendTeamNames( pPlayer );
	RecountTeams( FALSE );
	pPlayer->ShowVGUIMenu( 11 );

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if( plr && IsValidTeam( plr->TeamID() ) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict() );
				WRITE_BYTE( plr->entindex() );
				WRITE_STRING( plr->TeamID() );
			MESSAGE_END();
		}
	}
}
