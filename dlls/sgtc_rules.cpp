// sgtc_rules.cpp — StargateTC teams/rules/triggers/env (NEW file).
//
// WHAT THIS IS: clean-room port of the SGTC 2.0 server rules slice onto the
// hlsdk-xash3d base. Bodies reconstructed from ref/stc_i386.so disassembly
// (nm -C / objdump -d -C); exact verified facts are cited inline as
// fileoff/VMA notes. Anything layout-level the client drains opaquely (env
// payload field order) is marked BEST-EFFORT.
//
// STOCK HOOK POINTS (report only — NO stock file is edited by this change):
//   1. hlsdk-xash3d/dlls/gamerules.cpp:310 `InstallGameRules()` — add:
//         SGTC_InstallGameRules( STRING( gpGlobals->mapname ) )
//     which dispatches on the map-name prefix (never returns NULL).
//   2. hlsdk-xash3d/dlls/player.cpp:191 `LinkUserMessages()` — call
//     `SGTC_LinkUserMessages_Prefix()` before stock and `_Suffix()` after.
//   3. hlsdk-xash3d/dlls/player.cpp:2701 `EntSelectSpawnPoint()` — SGTC team
//     spawns are filtered in CStargateTeamDM/CP/Unas::GetPlayerSpawnSpot, so
//     no edit is strictly required; optional: fall through to
//     "info_player_torri"/"info_player_goauld" when no DM spot exists.
//   4. hlsdk-xash3d/dlls/triggers.cpp — no hook needed (SGTC trigger classes
//     derive CBaseToggle and live entirely here; stock CBaseTrigger/
//     CTriggerMultiple are TU-local to triggers.cpp with no header decl).
//   5. Build lists (see yield report for exact SRCS lines):
//        hlsdk-xash3d/dlls/CMakeLists.txt  ->  sgtc_rules.cpp
//        hlsdk-xash3d/dlls/Makefile        ->  sgtc_rules.o
//
// OUT OF SCOPE (owned by sibling slices): weapon_invis/CInvis (weapons),
// monster_implanted/skull, func_tankgoauld, gate/DHD, votesys/token.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "game.h"
#include "saverestore.h"
#include "sgtc_rules.h"
#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;
extern DLL_GLOBAL BOOL g_fGameOver;

// Brush-trigger setup shared by the SGTC trigger classes. Replicates
// CBaseTrigger::InitTrigger (triggers.cpp) — InitTrigger itself is TU-local
// to triggers.cpp and cannot be called from here.
void SGTC_InitTrigger( CBaseToggle *pTrigger )
{
	entvars_t *pev = pTrigger->pev;
	if( pev->angles != g_vecZero )
		SetMovedir( pev );
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	if( CVAR_GET_FLOAT( "showtriggers" ) == 0 )
		SetBits( pev->effects, EF_NODRAW );
}

#if !defined( _WIN32 ) && defined( __GNUC__ )
#pragma GCC visibility push( default )
#endif

// ---------------------------------------------------------------------------
// User messages. Header-owned (env/radar) + file-global HUD/round messages the
// rules classes send. Names are the exact BSS gmsg* symbols of
// ref/stc_i386.so (verified with `nm -C --defined-only ref/stc_i386.so`);
// they are deliberately NOT static so hl.so exports them, exactly as the
// reference does. Wire names + sizes are decoded from the ref
// LinkUserMessages() REG_USER_MSG table @0x132d44 (push imm before each
// `call *%edx`): -1 = variable-length, 0..n = fixed byte size.
// ---------------------------------------------------------------------------
int gmsgRadarBlip = 0;
int gmsgEnvParticule = 0;
int gmsgEnvRain = 0;
int gmsgEnvFountain = 0;
int gmsgEnvNewLens = 0;
int gmsgEnvAddRing = 0;
int gmsgChangeParticuleState = 0;
int gmsgProgress = 0;
int gmsgGtickets = 0;
int gmsgTtickets = 0;
int gmsgTicketMode = 0;
int gmsgHudInfoMode = 0;
int gmsgConcuss = 0;
int gmsgHasKey = 0;
int g_iSgtcMapType = 0;
int gmsgStamina = 0;
int gmsgSubModel = 0;
int gmsgArmor = 0;
int gmsgSpectator = 0;
int gmsgAddPortrait = 0;
int gmsgAboutMe = 0;
int gmsgTogglePortrait = 0;
int gmsgReset = 0;
int gmsgNewRnd = 0;
int gmsgSetHudRoundTime = 0;
int gmsgVGUIMenu = 0;
int gmsgHideVGUI = 0;
int gmsgZatTouch = 0;
int gmsgRadio = 0;

#if !defined( _WIN32 ) && defined( __GNUC__ )
#pragma GCC visibility pop
#endif

void SGTC_LinkUserMessages_Prefix( void )
{
	if( gmsgNewRnd )
		return;
	gmsgRadarBlip = REG_USER_MSG( "Blip", -1 );
	gmsgProgress = REG_USER_MSG( "Progress", -1 );
	gmsgGtickets = REG_USER_MSG( "Gtickets", 1 );
	gmsgTtickets = REG_USER_MSG( "Ttickets", 1 );
	gmsgTicketMode = REG_USER_MSG( "TicketMode", 1 );
	gmsgHudInfoMode = REG_USER_MSG( "HudInfoMode", 1 );
	gmsgConcuss = REG_USER_MSG( "Concuss", 2 );
	gmsgHasKey = REG_USER_MSG( "HasKey", 2 );
	gmsgStamina = REG_USER_MSG( "Stamina", 1 );
	gmsgSubModel = REG_USER_MSG( "SubModel", 1 );
	gmsgArmor = REG_USER_MSG( "Armor", 4 );
	gmsgSpectator = REG_USER_MSG( "Spectator", 2 );
	gmsgAddPortrait = REG_USER_MSG( "AddPortrait", -1 );
	gmsgAboutMe = REG_USER_MSG( "AboutMe", 5 );
	gmsgTogglePortrait = REG_USER_MSG( "TogglePor", 0 );
	gmsgReset = REG_USER_MSG( "Reset", -1 );
	gmsgNewRnd = REG_USER_MSG( "NewRnd", 0 );
}

void SGTC_LinkUserMessages_Suffix( void )
{
	if( gmsgRadio )
		return;
	gmsgVGUIMenu = REG_USER_MSG( "VGUIMenu", 1 );
	gmsgHideVGUI = REG_USER_MSG( "HideVGUI", 0 );
	gmsgSetHudRoundTime = REG_USER_MSG( "SetRndTim", 3 );
	gmsgZatTouch = REG_USER_MSG( "ZatTouch", 0 );
	gmsgEnvParticule = REG_USER_MSG( "EnvFull", 47 );
	gmsgEnvRain = REG_USER_MSG( "EnvRain", 15 );
	gmsgEnvFountain = REG_USER_MSG( "EnvFount", 45 );
	gmsgEnvNewLens = REG_USER_MSG( "NewLens", 10 );
	gmsgEnvAddRing = REG_USER_MSG( "AddRing", -1 );
	gmsgChangeParticuleState = REG_USER_MSG( "ChgPartSt", 2 );
	gmsgRadio = REG_USER_MSG( "Radio", 2 );
}

extern int g_teamplay;	// gamerules.cpp
extern int sgtc_GameModeSet;	// sgtc_round.cpp (ref g_mapmode)

// Mode installer. ref InstallGameRules @0x102e7a compares the MAP NAME
// (STRING(gpGlobals->mapname)) against the prefix table, NOT mp_gamemode:
//   sg_*->1 DM, sgi_*->2 DM, ctp_*->3 CP, un_*->5 Unas, inv_*->6 Invis,
//   rep_*->7 Replicateur, else->8 CHalfLifeMultiplay (g_teamplay=0).
// SGTC modes set g_teamplay=1. sgtc_GameModeSet mirrors ref g_mapmode.
// ---------------------------------------------------------------------------
#if !defined( _WIN32 ) && defined( __GNUC__ )
__attribute__(( visibility( "default" ) ))
#endif
CGameRules *SGTC_InstallGameRules( const char *pszMapName )
{
	if( pszMapName && *pszMapName )
	{
		if( !strncmp( pszMapName, "sg_", 3 ) )
		{
			sgtc_GameModeSet = 1;
			g_teamplay = 1;
			return new CStargateTeamDM;
		}
		if( !strncmp( pszMapName, "sgi_", 4 ) )
		{
			sgtc_GameModeSet = 2;
			g_teamplay = 1;
			return new CStargateTeamDM;
		}
		if( !strncmp( pszMapName, "ctp_", 4 ) )
		{
			sgtc_GameModeSet = 3;
			g_teamplay = 1;
			return new CStargateTeamCP;
		}
		if( !strncmp( pszMapName, "un_", 3 ) )
		{
			sgtc_GameModeSet = 5;
			g_teamplay = 1;
			return new CStargateUnas;
		}
		if( !strncmp( pszMapName, "inv_", 4 ) )
		{
			sgtc_GameModeSet = 6;
			g_teamplay = 1;
			return new CStargateInvis;
		}
		if( !strncmp( pszMapName, "rep_", 4 ) )
		{
			sgtc_GameModeSet = 7;
			g_teamplay = 1;
			return new CStargateReplicateur;
		}
	}
	sgtc_GameModeSet = 8;
	g_teamplay = 0;
	return new CHalfLifeMultiplay;
}

extern cvar_t timeleft, fragsleft;
extern int gmsgGameMode;
extern int gmsgSayText;
extern int gmsgTeamInfo;
extern int gmsgTeamNames;
extern int gmsgScoreInfo;
extern int gmsgDeathMsg;
extern int sgtc_teamed;	// sgtc_round.cpp (ref BSS teamed @0x23658c)
extern int sgtc_GameModeSet;			// sgtc_round.cpp (ref nm 0x1e0ad8)
extern float sgtc_gamemodex;			// sgtc_round.cpp (mp_gamemode numeric mirror)
extern int sgtc_G_players;				// sgtc_round.cpp (ref G_players BSS 0x21455c)
extern int sgtc_T_players;				// sgtc_round.cpp (ref T_players BSS 0x20c7d0)
extern int sgtc_G_players_focused;		// sgtc_round.cpp (ref BSS 0x2368a8)
extern int sgtc_T_players_focused;		// sgtc_round.cpp (ref BSS 0x2368a0)
extern int sgtc_G_players_last;			// sgtc_round.cpp (ref BSS 0x2147bc)
extern int sgtc_T_players_last;			// sgtc_round.cpp (ref BSS 0x21401c)
extern int sgtc_SendPL;					// sgtc_round.cpp (ref m_SendPL)
extern int sgtc_type;	// sgtc_token.cpp (ref BSS type @0x236588)
extern int sgtc_G_tickets;	// sgtc_round.cpp (ref G_tickets BSS 0x236880)
extern int sgtc_T_tickets;	// sgtc_round.cpp (ref T_tickets BSS 0x236884)
extern float sgtc_roundstart;	// sgtc_round.cpp (rules this+0x72c mirror)
extern cvar_t sgtc_cvar_wepselmode;		// sgtc_round.cpp (ref weaponsel @0x223484)
extern cvar_t sgtc_cvar_freeguns;		// sgtc_round.cpp (ref freeguns @0x2238f8)
extern cvar_t sgtc_cvar_attackcountdown;	// sgtc_round.cpp (ref @0x22390c)
extern cvar_t sgtc_cvar_superweapons;	// sgtc_round.cpp (ref superguns @0x2234d4)
extern void SetWeaponAllow( CBasePlayer *pPlayer );	// ref @0x17a26c — reads +0x93c, sends gmsgProgress variants
extern cvar_t *const sgtc_wpnC[28];	// sgtc_round.cpp — ref wpnC0..wpnC27 cost cvars
extern float sgtc_endtround;	// sgtc_round.cpp (ref BSS endtround @0x2468b8, goal+6s deadline)
extern char sgtc_round_official;			// sgtc_round.cpp (ref BSS 0x236888)
extern float sgtc_nrcooler;				// sgtc_round.cpp (ref BSS 0x2368b4)
extern cvar_t sgtc_cvar_autobalance;		// sgtc_round.cpp (ref autobalance @0x213420)
extern cvar_t sgtc_cvar_classlimitmodel;	// sgtc_round.cpp (ref classlimitmodel @0x2135d8)
extern cvar_t sgtc_cvar_max_syslord;		// ref @0x213650
extern cvar_t sgtc_cvar_max_anubis;			// ref @0x21363c
extern cvar_t sgtc_cvar_max_jaffa;			// ref @0x213628
extern cvar_t sgtc_cvar_max_serpent;		// ref @0x213614
extern cvar_t sgtc_cvar_max_horus;			// ref @0x213600
extern cvar_t sgtc_cvar_max_oniell;			// ref @0x2136b4 (max_oneill)
extern cvar_t sgtc_cvar_max_carter;			// ref @0x2136a0
extern cvar_t sgtc_cvar_max_jackson;		// ref @0x21368c
extern cvar_t sgtc_cvar_max_tealc;			// ref @0x213678
extern cvar_t sgtc_cvar_max_commander;		// ref @0x213664 (max_soldier)
extern int sgtc_count_syslord, sgtc_count_prime, sgtc_count_jaffa,
	sgtc_count_serpent, sgtc_count_horus, sgtc_count_oniell,
	sgtc_count_carter, sgtc_count_jackson, sgtc_count_tealc,
	sgtc_count_commander;					// sgtc_round.cpp — BSS 0x2368f4..0x236918
extern char sgtc_hasplayed;				// sgtc_round.cpp — BSS 0x2368f1
extern int sgtc_LastState[5];			// sgtc_round.cpp — ref BSS 0x236864
extern int sgtc_LastStateI[5];			// sgtc_round.cpp — ref BSS 0x23686c
extern float sgtc_KeyTime;				// sgtc_round.cpp — ref BSS KeyTime
extern char sgtc_BombCarrierLatch;		// sgtc_round.cpp — ref BOMBCARRIER latch
extern float sgtc_classchecktime;			// sgtc_round.cpp — ref classchecktime
extern float sgtc_recounttime;				// sgtc_round.cpp — ref recounttime
extern int sgtc_LastState[5];				// sgtc_round.cpp — BSS 0x236864
extern DLL_GLOBAL BOOL g_fGameOver;
extern cvar_t sgtc_cvar_camperdist;		// sgtc_round.cpp — ref camperdist @0x223544
extern cvar_t sgtc_cvar_camptime;			// sgtc_round.cpp — ref camptime @0x223558
extern cvar_t sgtc_cvar_friendlyfire;	// sgtc_round.cpp — ref friendlyfire @0x213358
extern char sgtc_Elmo;					// sgtc_player.cpp — ref BSS Elmo @0x236890
extern char sgtc_KeyChargeLatch;			// sgtc_player.cpp — ref DAT_00246866
extern int sgtc_bomby;					// sgtc_token.cpp — ref BSS bomby @0x2368f0
extern char sgtc_KeyInPlay;				// sgtc_token.cpp — ref BSS KeyInPlay @0x2368c6
extern int sgtc_GlobalBombState;			// sgtc_token.cpp — ref BSS GlobalBombState
extern Vector sgtc_masterkey;			// sgtc_round.cpp — ref BSS 0x2368c8
extern Vector sgtc_LastKeyPlace;		// sgtc_token.cpp — ref BSS LastKeyPlace
extern Vector sgtc_firstplace;			// sgtc_token.cpp — ref BSS firstplace
extern cvar_t sgtc_cvar_tickets;			// sgtc_round.cpp (ref tickets @0x2133f8)
BOOL SGTC_ThinkRunsMainRoundBody( void );	// ref Think mp_timelimit gate @0x1929ce
void SGTC_ThinkMpTimelimitResetTickets( void );
void SGTC_ThinkRefreshTimeleftCvar( void );

// Team spawn classname tables (rodata 0x1d7997 block ordering).
static const char *const s_SGTCDMSpawns[SGTC_MAX_SGTC_TEAMS] =
{
	"info_player_torri",
	"info_player_goauld",
};

// Portrait/model names from InitHUD disasm (oneill..unas, 11 names).
static const char *const s_SGTCPortraits[] =
{
	"oneill", "carter", "jackson", "tealc", "commander",
	"systemlord", "anubis", "goauld3", "snakegoauld", "horus", "unas",
};
#define SGTC_NUM_PORTRAITS ( sizeof( s_SGTCPortraits ) / sizeof( s_SGTCPortraits[0] ) )

// ===========================================================================
// CStargateTeamDM
// ===========================================================================
CStargateTeamDM::CStargateTeamDM( void )
{
	// ref ctor @0x188824: base CHalfLifeTeamplay ctor already read mp_teamlist
	// into m_szTeamList ("hgrunt;scientist" from game.cfg); the .so ctor then
	// OVERWRITES this+0x18 (m_szTeamList) with "Torri;Goa'uld", so mp_teamlist
	// never reaches the wire. RecountTeams() strtoks m_szTeamList into the
	// global team_names table (mirrored here as m_szSGTCTeams).
	SGTC_SetTeamList( "Torri;Goa'uld" );	// ref ctor this+0x18
	strncpy( m_szSGTCTeams[0], "Torri", SGTC_TEAMNAME_LEN );
	strncpy( m_szSGTCTeams[1], "Goa'uld", SGTC_TEAMNAME_LEN );
	m_iSGTCTeamCount = SGTC_MAX_SGTC_TEAMS;
	m_iSGTCTeamScores[0] = m_iSGTCTeamScores[1] = 0;
	m_bRoundOver = FALSE;
	m_bDisableDeathMessages = FALSE;	// ref this+0xc
	m_bDisableDeathPenalty = FALSE;	// ref this+0x10
	m_iSgtcModeVote = -1;	// ref +0x770 pending mode vote (-1 none)
	m_iSgtcPrevGameModeSet = 0;	// ref DM +0x75c latch (fresh object)
	m_iSgtcFocusedBoth = 0;		// ref DM +0x760 latch
	m_iSgtcSupergunsLatch = 0;	// ref DM +0x76c latch
	m_iSgtcFocusedBucket = 0;	// ref CP +0x75c latch (shared member)
	g_iSgtcMapType = 0;		// ref DM ctor sets MapType=0 (@0x188600)

	// ref: INDEXENT(0)->targetname (+0x22c) overrides the team list when the
	// mp_teamoverride cvar is nonzero and the name is non-empty.
	edict_t *pWorld = INDEXENT( 0 );
	if( pWorld && pWorld->v.targetname && CVAR_GET_FLOAT( "mp_teamoverride" ) != 0 )
	{
		const char *tn = STRING( pWorld->v.targetname );
		if( tn && *tn )
		{
			SGTC_SetTeamList( tn );
		}
	}
	// ref this+0x14 already set inside SGTC_SetTeamList (list[0]!=0).
	RecountTeams( FALSE );

	// ref: roundforce._value > 0 -> NewRound() right away.
	if( CVAR_GET_FLOAT( "mp_roundforce" ) > 0 )
		NewRound();
}

int CStargateTeamDM::GetTeamIndex( const char *pTeamName )
{
	// Exact shape of the .so GetTeamIndex: strcasecmp loop, -1 on miss.
	if( pTeamName && *pTeamName )
	{
		for( int tm = 0; tm < m_iSGTCTeamCount; tm++ )
		{
			if( !stricmp( m_szSGTCTeams[tm], pTeamName ) )
				return tm;
		}
	}
	return -1;
}

const char *CStargateTeamDM::GetIndexedTeamName( int teamIndex )
{
	if( teamIndex < 0 || teamIndex >= m_iSGTCTeamCount )
		return "";
	return m_szSGTCTeams[teamIndex];
}

BOOL CStargateTeamDM::IsValidTeam( const char *pTeamName )
{
	return ( GetTeamIndex( pTeamName ) != -1 ) ? TRUE : FALSE;
}

const char *CStargateTeamDM::GetTeamID( CBaseEntity *pEntity )
{
	if( !pEntity || !pEntity->pev )
		return "";
	return pEntity->TeamID();
}

// PlayerRelationship is inherited from CHalfLifeTeamplay (teamplay_gamerules.cpp
// :429): identical same-team stricmp logic; the SGTC header has no decl, so the
// stock virtual covers all call sites below.

BOOL CStargateTeamDM::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if( pAttacker && PlayerRelationship( pPlayer, pAttacker ) == GR_TEAMMATE )
	{
		if( friendlyfire.value == 0 && pAttacker != pPlayer )
			return FALSE;
	}
	return CHalfLifeMultiplay::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

BOOL CStargateTeamDM::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if( pTgt && pTgt->IsPlayer() && PlayerRelationship( pPlayer, pTgt ) == GR_TEAMMATE )
		return FALSE;
	return CHalfLifeMultiplay::ShouldAutoAim( pPlayer, target );
}

int CStargateTeamDM::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if( !pKilled )
		return 0;
	if( !pAttacker )
		return 1;
	if( pAttacker != pKilled && PlayerRelationship( pAttacker, pKilled ) == GR_TEAMMATE )
		return -1;
	return 1;
}

const char *CStargateTeamDM::TeamWithFewestPlayers( void )
{
	int teamCount[SGTC_MAX_SGTC_TEAMS] = { 0, 0 };
	int minPlayers = 9999;
	const char *pName = m_szSGTCTeams[0];
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if( plr )
		{
			int team = GetTeamIndex( plr->TeamID() );
			if( team >= 0 )
				teamCount[team]++;
		}
	}
	for( int i = 0; i < m_iSGTCTeamCount; i++ )
	{
		if( teamCount[i] < minPlayers )
		{
			minPlayers = teamCount[i];
			pName = m_szSGTCTeams[i];
		}
	}
	return pName;
}

void CStargateTeamDM::RecountTeams( BOOL bResendInfo )
{
	// ref @0x18df82 — full recount engine: rebuild team_names from
	// m_szTeamList, count alive/focused players, tally mode votes,
	// autobalance, then optionally re-broadcast gmsgTeamInfo.
	m_iSGTCTeamCount = 0;
	char listBuf[512];
	strncpy( listBuf, SGTC_GetTeamList(), sizeof( listBuf ) - 1 );
	listBuf[sizeof( listBuf ) - 1] = '\0';
	char *tok = strtok( listBuf, ";" );
	while( tok && *tok )
	{
		if( GetTeamIndex( tok ) < 0 && m_iSGTCTeamCount < SGTC_MAX_SGTC_TEAMS )
		{
			strncpy( m_szSGTCTeams[m_iSGTCTeamCount], tok, SGTC_TEAMNAME_LEN - 1 );
			m_szSGTCTeams[m_iSGTCTeamCount][SGTC_TEAMNAME_LEN - 1] = '\0';
			m_iSGTCTeamCount++;
		}
		tok = strtok( NULL, ";" );
	}
	if( m_iSGTCTeamCount < 2 )
	{
		m_iSGTCTeamCount = 0;
		SGTC_SetTeamLimit( FALSE );	// ref this+0x14
	}

	// Vote tallies (per-player +0x90c in 1..5 -> slot idx-1).
	int votes[5] = { 0, 0, 0, 0, 0 };
	int voteWinners[5] = { 0, 0, 0, 0, 0 };
	int gCount = 0, tCount = 0, voters = 0, aliveNonObs = 0;
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !plr )
			continue;
		// ref: alive && !observer && pev->team >= 1 && !m_bSgtcBadGuy
		if( plr->IsAlive() && !plr->IsObserver() &&
			(int)plr->pev->team >= 1 && !plr->m_bSgtcBadGuy )
		{
			int team = (int)plr->pev->team;
			if( team == 1 ) gCount++;
			if( team == 2 ) tCount++;
			if( plr->m_iBombKeyTimer > 0 && plr->m_iBombKeyTimer < 6 )
			{
				voters++;
				votes[plr->m_iBombKeyTimer - 1]++;
			}
		}
		if( !plr->IsObserver() && (int)plr->pev->team > 0 && !plr->m_bSgtcBadGuy )
			aliveNonObs++;
	}

	// ref: winner = slot with max tally; ties collect into voteWinners.
	int best = -1, bestTally = 0, ties = 0;
	for( int s = 0; s < 5; s++ )
	{
		if( votes[s] == bestTally && bestTally > 0 )
		{
			ties++;
			voteWinners[ties] = s;
		}
		if( bestTally < votes[s] )
		{
			bestTally = votes[s];
			best = s;
			voteWinners[0] = s;
			ties = 0;
		}
	}
	if( best < 0 )
		m_iSgtcModeVote = (int)sgtc_gamemodex;	// ref: ROUND(gamemodex)
	else if( ties < 1 )
		m_iSgtcModeVote = best;
	else
		m_iSgtcModeVote = voteWinners[RANDOM_LONG( 0, ties )];
	if( m_iSgtcModeVote == best && bestTally == aliveNonObs && voters > 0 &&
		m_iSgtcModeVote != sgtc_GameModeSet )
		m_bSgtcVoteEqual = 1;					// ref this+0x774
	if( m_iSgtcModeVote == sgtc_GameModeSet )
		m_iSgtcModeVote = -1;

	sgtc_G_players = gCount;	// ref G_players
	sgtc_T_players = tCount;	// ref T_players

	// ref this+0x758 empty-teams state: 1=G empty, 2=T empty, 3=both, 0 populated.
	int empty = ( gCount < 1 ) ? 1 : 0;
	if( tCount < 1 ) empty = 2;
	if( gCount < 1 && tCount < 1 ) empty = 3;
	if( gCount > 0 && tCount > 0 ) empty = 0;
	int playerEnts = 0;
	CBaseEntity *scan = NULL;
	while( ( scan = UTIL_FindEntityByClassname( scan, "player" ) ) != NULL )
		playerEnts++;
	if( playerEnts < 1 ) empty = 0;
	m_iSgtcEmptyTeams = empty;

	// focused counts (m_iSgtcActiveTeam +0x430, not bad-guy).
	sgtc_T_players_focused = 0;
	sgtc_G_players_focused = 0;
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( plr && plr->m_bSgtcBadGuy != 1 )
		{
			if( plr->m_iSgtcActiveTeam == 1 ) sgtc_G_players_focused++;
			if( plr->m_iSgtcActiveTeam == 2 ) sgtc_T_players_focused++;
		}
	}

	// ref this+0x768 autobalance tri-state (dead while unregistered mp_autobalance==0).
	float autoBal = CVAR_GET_FLOAT( "mp_autobalance" );
	m_iSgtcAutoBalance = 0;
	if( autoBal > 0.0f )
	{
		if( sgtc_T_players_focused < sgtc_G_players_focused &&
			autoBal < (float)( sgtc_G_players_focused - sgtc_T_players_focused ) )
			m_iSgtcAutoBalance = 1;
		if( sgtc_G_players_focused < sgtc_T_players_focused &&
			autoBal < (float)( sgtc_T_players_focused - sgtc_G_players_focused ) )
			m_iSgtcAutoBalance = 2;
	}

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !plr )
			continue;
		// ref: net clients get +0x926 latch cleared only when a team GREW.
		if( plr->IsNetClient() &&
			!( sgtc_T_players_focused <= sgtc_T_players_last &&
			   sgtc_G_players_focused <= sgtc_G_players_last ) )
			plr->m_bSgtcPortraitLatch = 0;

		const char *teamName = plr->TeamID();
		int idx = GetTeamIndex( teamName );
		if( idx < 0 && !SGTC_TeamLimit() && m_iSGTCTeamCount < SGTC_MAX_SGTC_TEAMS )
		{
			strncpy( m_szSGTCTeams[m_iSGTCTeamCount], teamName, SGTC_TEAMNAME_LEN - 1 );
			m_szSGTCTeams[m_iSGTCTeamCount][SGTC_TEAMNAME_LEN - 1] = '\0';
			m_iSGTCTeamCount++;
		}
		if( bResendInfo && IsValidTeam( teamName ) )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
				WRITE_BYTE( plr->entindex() );
				WRITE_STRING( teamName );
			MESSAGE_END();
		}
	}
	sgtc_T_players_last = sgtc_T_players_focused;
	sgtc_G_players_last = sgtc_G_players_focused;
}

const char *CStargateTeamDM::SetDefaultPlayerTeam( CBasePlayer *pPlayer )
{
	char *mdls = g_engfuncs.pfnInfoKeyValue(
		g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" );
	snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", mdls );

	if( pPlayer->m_szTeamName[0] == '\0' || !IsValidTeam( pPlayer->m_szTeamName ) )
		snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", TeamWithFewestPlayers() );

	return pPlayer->m_szTeamName;
}

void CStargateTeamDM::ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib )
{
	// Mirrors CHalfLifeTeamplay::ChangePlayerTeam (kill, keyvalues, HUD fan-out).
	int damageFlags = DMG_GENERIC | ( bGib ? DMG_ALWAYSGIB : DMG_NEVERGIB );
	if( bKill )
	{
		// ref @0x1908c4: this+0xc / this+0x10 set around the forced kill —
		// suppresses DeathNotice and the PlayerKilled stock path.
		m_bDisableDeathMessages = TRUE;
		m_bDisableDeathPenalty = TRUE;
		entvars_t *pevWorld = VARS( INDEXENT( 0 ) );
		pPlayer->TakeDamage( pevWorld, pevWorld, 900, damageFlags );
		m_bDisableDeathMessages = FALSE;
		m_bDisableDeathPenalty = FALSE;
	}
	if( pPlayer->m_szTeamName != pTeamName )
		snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", pTeamName );

	int clientIndex = pPlayer->entindex();
	g_engfuncs.pfnSetClientKeyValue( clientIndex,
		g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", pPlayer->m_szTeamName );
	g_engfuncs.pfnSetClientKeyValue( clientIndex,
		g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", pPlayer->m_szTeamName );

	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( clientIndex );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( clientIndex );
		WRITE_SHORT( (int)pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );		// ref CStargateTeamDM param+0xdf0
		WRITE_SHORT( 0 );							// ref param+0xa40 (portrait-shown unless invis latch)
		WRITE_SHORT( GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		WRITE_SHORT( (int)pPlayer->pev->team );	// ref *(pev)+0x170
	MESSAGE_END();
}

void CStargateTeamDM::SGTC_SendTeamNames( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict() );
		WRITE_BYTE( m_iSGTCTeamCount );
		for( int i = 0; i < m_iSGTCTeamCount; i++ )
			WRITE_STRING( m_szSGTCTeams[i] );
		for( int z = 0; z < 28; z++ )
			WRITE_BYTE( 0 );	// ref: TeamNames carries a 28-byte pad tail
	MESSAGE_END();
}

static int SGTC_PortraitSprId( CBasePlayer *pPlayer );
static void SGTC_EmitAddPortrait( int msgDest, edict_t *to, CBasePlayer *subject );

void CStargateTeamDM::InitHUD( CBasePlayer *pPlayer )
{
	// ref @0x190b54 — zero the two 5-slot tables, clear portrait sel, team +
	// team-name, stock Multiplay InitHUD, then MSG_ONE TeamNames with 28
	// trailing zero bytes, RecountTeams(0), ShowVGUIMenu(11),
	// SetHudRoundTime(0,0), per-player TeamInfo/ScoreInfo to the joiner.
	static int s_SgtcHudInitA[5];	// ref this+0x438/+0xa44-adjacent tables
	static int s_SgtcHudInitB[5];
	for( int t = 0; t < 5; t++ ) { s_SgtcHudInitA[t] = 0; s_SgtcHudInitB[t] = 0; }
	pPlayer->m_iSgtcPortraitSel = 0;	// ref +0xa44
	pPlayer->m_iSgtcTeam = 0;			// ref +0x438
	pPlayer->m_iSgtcInfoLock = 0;		// ref +0xdc — info lock off at connect
	pPlayer->m_szTeamName[0] = '\0';	// ref +0xe08 strncpy
	CHalfLifeMultiplay::InitHUD( pPlayer );

	// ref: TeamNames = BYTE num_teams + STRING names + 28 zero bytes.
	MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict() );
		WRITE_BYTE( m_iSGTCTeamCount );
		for( int i = 0; i < m_iSGTCTeamCount; i++ )
			WRITE_STRING( m_szSGTCTeams[i] );
		for( int z = 0; z < 28; z++ )
			WRITE_BYTE( 0 );
	MESSAGE_END();

	RecountTeams( FALSE );
	pPlayer->ShowVGUIMenu( 11 );	// ref DM InitHUD menu id
	pPlayer->SetHudRoundTime( 0, 0 );
	pPlayer->m_flSgtcMenuDelay = gpGlobals->time + 3.0f;	// ref rodata +3.0

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if( !plr || !IsValidTeam( plr->TeamID() ) )
			continue;
		MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict() );
			WRITE_BYTE( plr->entindex() );
			WRITE_STRING( plr->TeamID() );
		MESSAGE_END();
		// ref gmsgScoreInfo (InitHUD variant): frags/deaths sent as 0.
		CBasePlayer *pPlr = (CBasePlayer *)plr;
		MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pPlayer->edict() );
			WRITE_BYTE( plr->entindex() );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
			WRITE_SHORT( pPlr->m_bSnagInvisLatch == 1 ? 6 : pPlr->m_iSgtcPortraitShown );
			WRITE_SHORT( GetTeamIndex( plr->TeamID() ) + 1 );
			WRITE_SHORT( (int)plr->pev->team );
		MESSAGE_END();
		// ref AddHudPortrait: full client layout, about this player, to the joiner.
		SGTC_EmitAddPortrait( MSG_ONE, pPlayer->edict(), (CBasePlayer *)plr );
	}
	if( sgtc_G_players_focused + sgtc_T_players_focused < 1 )
		pPlayer->StartDeathCam();
	sgtc_SendPL = 1;	// ref m_SendPL = 1 (DM InitHUD @0x191238)
}

void CStargateTeamCP::InitHUD( CBasePlayer *pPlayer )
{
	// ref @0x1833f2 — DM InitHUD variant minus ShowVGUIMenu: clears +0x3e3,
	// portrait-shown=1, portrait-sel/team/env-stage=0, empty team-name, stock
	// Multiplay InitHUD, MSG_ONE TeamNames + 28 pad bytes, RecountTeams(0),
	// SetHudRoundTime(0,0), per-player TeamInfo, self ScoreInfo, portrait
	// fan-out, StartDeathCam + mode-check when unfocused, menu delay +3s.
	pPlayer->m_bSgtcPad3e3 = 0;			// ref +0x3e3
	// Connect-observer latch reset: ClientPutInServer->Spawn() leaves one
	// DEAD_NO (alive) frame before InitHUD->StartDeathCam flips to observer,
	// and PlayerThink latches m_bSgtcRoundAlive=1 (+0x3e4) off it. That
	// permanently suppresses the menu-11 dead-tick emit (PlayerThink gate
	// `m_bSgtcRoundAlive != 1`). The ref connect is a pure observer (no alive
	// frame), so the latch must be cleared here — a fresh connect was never
	// alive this round.
	pPlayer->m_bSgtcRoundAlive = 0;	// ref +0x3e4 seen-alive latch
	pPlayer->m_iSgtcPortraitShown = 1;	// ref +0xa40
	pPlayer->m_iSgtcPortraitSel = 0;	// ref +0xa44
	pPlayer->m_iSgtcTeam = 0;			// ref +0x438
	pPlayer->m_iSgtcEnvStage = 0;		// ref +0xc90
	pPlayer->m_szTeamName[0] = '\0';	// ref +0xe08 strncpy
	CHalfLifeMultiplay::InitHUD( pPlayer );

	MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict() );
		WRITE_BYTE( m_iSGTCTeamCount );
		for( int i = 0; i < m_iSGTCTeamCount; i++ )
			WRITE_STRING( m_szSGTCTeams[i] );
		for( int z = 0; z < 28; z++ )
			WRITE_BYTE( 0 );
	MESSAGE_END();

	RecountTeams( FALSE );
	pPlayer->SetHudRoundTime( 0, 0 );

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

	// ref: ScoreInfo for the JOINER only (self row, zeroed frags/deaths).
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pPlayer->edict() );
		WRITE_BYTE( pPlayer->entindex() );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( pPlayer->m_iSgtcPortraitShown );
		WRITE_SHORT( GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		WRITE_SHORT( (int)pPlayer->pev->team );
	MESSAGE_END();

	// ref AddHudPortrait(param6=1): MSG_ONE to the JOINER carrying each
	// valid team player's portrait (entindex + team name) — same as DM.
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if( plr && IsValidTeam( plr->TeamID() ) )
		{
			SGTC_EmitAddPortrait( MSG_ONE, pPlayer->edict(), (CBasePlayer *)plr );
		}
	}

	pPlayer->StartDeathCam();
	// ref: if( T_players_focused + G_players_focused < 1 ) calls gamerules
	// vtable+0x38 (mode-check); port has no mapped equivalent and the gate
	// only fires before any client joins, so it is inert in play.
	if( sgtc_T_players_focused + sgtc_G_players_focused < 1 )
	{
	}
	pPlayer->m_flSgtcMenuDelay = gpGlobals->time + 3.0f;	// ref +0x4b0 = now+3.0
	sgtc_SendPL = 1;	// ref m_SendPL
}

void CStargateTeamDM::UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 1 ); // teamplay
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgTicketMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 ); // DM: no tickets
	MESSAGE_END();
}

// ---------------------------------------------------------------------------
// SetWeaponAllow — ref @0x18a26c (nm 0x17a26c). Zeroes the 28-entry
// Progress-allow table (CBasePlayer m_iProgressTeam, ref +0x93c) for the
// weapons the player's pev->team/class (+0xa40) may NOT pick up; other teams
// keep every slot at 1. Slot i lives at +0x93c + i*4. Indexes below are
// (ref 0x9NN - 0x93c) / 4. Copy verbatim from
// rea_dump/SetWeaponAllow.c, including the team==2 case-5 fallthrough.
// ---------------------------------------------------------------------------
void SetWeaponAllow( CBasePlayer *pPlayer )
{
	int team = (int)pPlayer->pev->team;
	int cls = pPlayer->m_iSgtcPortraitShown;
	// Slot lists re-read from ref SetWeaponAllow @0x18a26c (Ghidra pseudo-code,
	// offsets +0x93c..+0x9a8 zeroed per team/class). The earlier hand-typed
	// tables were off; these are the ref's exact zeroed-slot sets (slot =
	// (offset-0x93c)/4). Cross-check: the orig's menu-12 capture shows exactly
	// the 15 slots allowed by team 2 / class 1.
	static const int g_wa_team2[5][21] = {
		// cls 1 (13)
		{ 0,2,7,9,12,13,14,17,18,19,21,22,27,-1 },
		// cls 2 (16)
		{ 0,1,2,5,7,8,9,11,13,14,16,17,18,19,20,21,-1 },
		// cls 3 (20) — ref case 3 @SetWeaponAllow: 0x10 (mine), not 0x0f (hand grenade)
		{ 0,1,2,7,8,9,11,12,13,14,16,17,18,19,20,21,23,24,25,27,-1 },
		// cls 4 (13)
		{ 2,5,6,9,13,14,17,18,19,21,24,26,27,-1 },
		// cls 5 (18; ref case has no break — falls out of the switch)
		{ 0,2,9,10,11,12,13,14,16,17,18,19,20,21,22,24,26,27,-1 },
	};
	static const int g_wa_team1[5][26] = {
		// cls 1 (19)
		{ 0,1,3,4,5,6,7,8,11,12,15,16,19,20,22,23,24,25,26,-1 },
		// cls 2 (22)
		{ 0,1,3,4,5,6,7,8,9,13,15,16,17,18,19,20,22,23,24,25,26,27,-1 },
		// cls 3 (24)
		{ 0,1,2,3,4,5,6,7,8,9,12,14,15,16,17,18,20,21,22,23,24,25,26,27,-1 },
		// cls 4 (20)
		{ 1,3,4,5,6,7,8,9,13,14,15,16,17,18,20,22,23,24,25,26,-1 },
		// cls 5 (23)
		{ 0,1,3,4,5,6,7,8,9,10,12,14,15,16,18,19,20,21,22,23,24,25,26,-1 },
	};
	if( team == 2 && cls >= 1 && cls <= 5 )
	{
		const int *t = g_wa_team2[cls - 1];
		for( int k = 0; k < 21; k++ )
		{
			if( t[k] < 0 )
				break;
			pPlayer->m_iProgressTeam[t[k]] = 0;
		}
	}
	if( team == 1 && cls >= 1 && cls <= 5 )
	{
		const int *t = g_wa_team1[cls - 1];
		for( int k = 0; k < 26; k++ )
		{
			if( t[k] < 0 )
				break;
			pPlayer->m_iProgressTeam[t[k]] = 0;
		}
	}
	if( team == 0 )
	{
		for( int i = 0; i < 28; i++ )	// ref 0x1c entries
			pPlayer->m_iProgressTeam[i] = 0;
	}
}

edict_t *CStargateTeamDM::SGTC_FindTeamSpawn( const char *pszClassname )
{
	CBaseEntity *pSpot = NULL;
	for( int i = RANDOM_LONG( 1, 5 ); i > 0; i-- )
		pSpot = UTIL_FindEntityByClassname( pSpot, (char *)pszClassname );
	if( !pSpot )
		pSpot = UTIL_FindEntityByClassname( pSpot, (char *)pszClassname );
	CBaseEntity *pFirst = pSpot;
	while( pSpot )
	{
		if( pSpot->pev->origin != Vector( 0, 0, 0 ) )
			return pSpot->edict();
		pSpot = UTIL_FindEntityByClassname( pSpot, (char *)pszClassname );
		if( pSpot == pFirst )
			break;
	}
	return NULL;
}

edict_t *CStargateTeamDM::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	// Team-filtered spawn (.so Think/PlayerSpawn use info_player_torri /
	// info_player_goauld; stock EntSelectSpawnPoint only knows DM/start).
	int team = GetTeamIndex( pPlayer->m_szTeamName );
	const char *SpotClass = ( team == 1 ) ? s_SGTCDMSpawns[1] : s_SGTCDMSpawns[0];
	edict_t *spot = SGTC_FindTeamSpawn( SpotClass );
	if( !spot || FNullEnt( spot ) )
		spot = SGTC_FindTeamSpawn( "info_player_deathmatch" );
	if( !spot || FNullEnt( spot ) )
		return CGameRules::GetPlayerSpawnSpot( pPlayer );

	pPlayer->pev->origin = VARS( spot )->origin + Vector( 0, 0, 1 );
	pPlayer->pev->angles = VARS( spot )->angles;
	pPlayer->pev->v_angle = VARS( spot )->angles;
	pPlayer->pev->fixangle = TRUE;
	return spot;
}

// ---------------------------------------------------------------------------
// Class model/stat tables — ref DM::PlayerSpawn @0x18e7e2 Goa'uld/Torri
// switches on +0xa40 (m_iSgtcPortraitShown). Class 4 cloak: Goa'uld always,
// Torri only for tealc. Model key is written via pfnSetClientKeyValue.
// ---------------------------------------------------------------------------
struct SGTCSClassDef
{
	const char *model;
	float maxspeed;		// pev+0x210
	int snagArmorCap;	// +0x908 — armor HUD scale cap
	float armorvalue;	// pev+0x1bc
	int cloak;		// +0x448 cloak tick arm
};
static const SGTCSClassDef s_SgtcGoauldClass[6] =
{
	{ NULL, 0.0f, 0, 0.0f, 0 },				// class 0: no override
	{ "systemlord",  250.0f, 35,  35.0f, 1 },	// ref +0x908=0x23, armor 0x420c0000
	{ "anubis",      250.0f, 80,  80.0f, 1 },	// +0x908=0x50, armor 0x42a00000
	{ "goauld3",     325.0f, 40,  40.0f, 1 },	// +0x908=0x28, armor 0x42200000
	{ "snakegoauld", 275.0f, 80,  80.0f, 1 },	// maxspeed 0x43898000
	{ "horus",       325.0f, 30,  30.0f, 1 },	// +0x908=0x1e, armor 0x41f00000
};
static const SGTCSClassDef s_SgtcTorriClass[6] =
{
	{ NULL, 0.0f, 0, 0.0f, 0 },
	{ "oneill",      250.0f, 70,  70.0f, 0 },	// +0x908=0x46, armor 0x428c0000
	{ "carter",      275.0f, 40,  40.0f, 0 },
	{ "jackson",     225.0f, 70,  70.0f, 0 },	// maxspeed 0x43610000
	{ "tealc",       275.0f, 25,  25.0f, 1 },	// +0x908=0x19, cloak on
	{ "commander",   250.0f, 40,  40.0f, 0 },
};
// CP table differs from DM only in snakegoauld speed: 270.0f (0x43870000).
static const SGTCSClassDef s_SgtcCPGoauldClass[6] =
{
	{ NULL, 0.0f, 0, 0.0f, 0 },
	{ "systemlord",  250.0f, 35,  35.0f, 1 },
	{ "anubis",      250.0f, 80,  80.0f, 1 },
	{ "goauld3",     325.0f, 40,  40.0f, 1 },
	{ "snakegoauld", 270.0f, 80,  80.0f, 1 },
	{ "horus",       325.0f, 30,  30.0f, 1 },
};

// Weapon-grant table — ref WPLIST (nm 0x220c40) == WPLISTB (nm 0x220a60):
// identical 28 entries; CP spawn uses WPLISTB, DM spawn WPLIST, both the same
// names. Same 28-entry ordering as the gond4h9wp menu grant table in
// sgtc_player.cpp (sgtc_WPLIST) — both are non-NULL throughout.
static const char * const s_SgtcSpawnWeapons[28] =
{
	"weapon_glstaff",	"weapon_usas",		"weapon_batsup",
	"weapon_m92s",		"weapon_mp5",		"weapon_psg1",
	"weapon_p90",		"weapon_spas",		"weapon_m16",
	"weapon_main",		"weapon_zat",		"weapon_lance",
	"weapon_reetou",	"weapon_zatarc",	"weapon_tacgun",
	"weapon_handgrenade","weapon_mine",		"weapon_invis",
	"weapon_invis",		"weapon_gacgoa",	"weapon_c4",
	"weapon_larve",		"weapon_medic",		"weapon_smokegrenade",
	"weapon_nervegrenade","weapon_narcogrenade","weapon_mustardgrenade",
	"weapon_goaregen"
};

void CStargateTeamDM::PlayerSpawn( CBasePlayer *pPlayer )
{
	// ref CStargateTeamDM::PlayerSpawn @0x18e7e2 (rea_dump/CStargateTeamDM__PlayerSpawn.c).
	// Engfuncs-mirror DAT_00238fd4 = enginefuncs[117] = pfnGetPlayerWONId
	// (returns -1 for an unfound edict — the "+0x914 locked" sentinel).
	pPlayer->m_iSgtcSpecMode = (int)g_engfuncs.pfnGetPlayerWONId( pPlayer->edict() );
	pPlayer->m_bSgtcPad3e3 = 0;					// ref +0x3e3 = 0
	pPlayer->m_iSgtcFlagsCa8 &= ~0xB;				// ref +0xca8 &= 0xfffffff4
	pPlayer->StopObserver();
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );
	if( sgtc_GameModeSet != 3 )
	{
		pPlayer->m_bSnagInvisFlag = 0;			// ref +0x4df = 0
		pPlayer->m_bSnagInvisLatch = pPlayer->m_bSnagInvisFlag;	// +0x4de = +0x4df
	}
	pPlayer->m_bNukeViewSnap = 0;					// ref +0x463 = 0

	if( !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) )	// ref DAT_001ebb90, 7-char
	{
		pPlayer->pev->team = 1;					// ref pev+0x1ac = 1
		pPlayer->m_bSgtcCloakTick = 0;			// ref +0x448 = 0
		if( pPlayer->m_bSgtcSuperman != 1 )		// ref +0x924 != 1
		{
			int cls = pPlayer->m_iSgtcPortraitShown;	// ref +0xa40
			if( cls >= 1 && cls <= 5 )
			{
				const SGTCSClassDef &d = s_SgtcGoauldClass[cls];
				g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(),
					g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", d.model );
				pPlayer->pev->maxspeed = d.maxspeed;
				pPlayer->m_iSnagArmorCap = d.snagArmorCap;
				pPlayer->pev->armorvalue = d.armorvalue;
				pPlayer->m_bSgtcCloakTick = (char)d.cloak;
			}
		}
	}
	else if( !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) )	// ref "Torri", 5-char
	{
		pPlayer->pev->team = 2;					// ref pev+0x1ac = 2
		pPlayer->m_bSgtcCloakTick = 0;
		if( pPlayer->m_bSgtcSuperman != 1 )
		{
			int cls = pPlayer->m_iSgtcPortraitShown;
			if( cls >= 1 && cls <= 5 )
			{
				const SGTCSClassDef &d = s_SgtcTorriClass[cls];
				g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(),
					g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", d.model );
				pPlayer->pev->maxspeed = d.maxspeed;
				pPlayer->m_iSnagArmorCap = d.snagArmorCap;
				pPlayer->pev->armorvalue = d.armorvalue;
				pPlayer->m_bSgtcCloakTick = (char)d.cloak;
			}
		}
	}

	for( int i = 0; i < 28; i++ )				// ref +0x93c[0x1c] = 1
		pPlayer->m_iProgressTeam[i] = 1;
	SetWeaponAllow( pPlayer );					// ref @0x17a26c

	if( pPlayer->m_iSgtcPrevClass != pPlayer->m_iSgtcPortraitShown ||
		pPlayer->m_iSgtcPrevPeTeam != (int)pPlayer->pev->team )
	{
		// Class/team changed: resend Progress, unlock menu, wipe grant copy.
		MESSAGE_BEGIN( MSG_ONE, gmsgProgress, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( pPlayer->m_iNukeBonus );			// ref +0x934
			for( int i = 0; i < 28; i++ )
				WRITE_BYTE( pPlayer->m_iProgressTeam[i] );
			WRITE_BYTE( 1 );
		MESSAGE_END();
		pPlayer->m_bSgtcMenuLock = 0;				// ref +0x4e5
		for( int i = 0; i < 28; i++ )
			pPlayer->m_iSgtcGondArgs[i] = 0;		// ref +0x9ac[i] = 0
	}

	BOOL bHadLoadout = FALSE;					// ref local_29
	pPlayer->m_bSgtcMenuLockLatch = 0;			// ref +0x4e7
	pPlayer->m_iSgtcGondState = 0;				// ref +0x4f4

	// ref grant gate: vcall+0x94 (IsAlive) && !IsObserver && +0x4de == 0.
	if( pPlayer->IsAlive() && !pPlayer->IsObserver() && !pPlayer->m_bSnagInvisLatch )
	{
		for( int i = 0; i < 28; i++ )
		{
			if( pPlayer->m_iSgtcGondArgs[i] == 1 )	// ref +0x9ac[i] == 1
			{
				pPlayer->m_bSgtcMenuLockLatch = 1;	// ref +0x4e7 = 1
				bHadLoadout = TRUE;
				if( i == 0x11 || i == 0x12 )
				{
					// ref +0x4f4 shield-slot state machine (identical 4-step
					// merge as GiveFromMenu).
					if( pPlayer->m_iSgtcGondState == 0 && i == 0x11 )
						pPlayer->m_iSgtcGondState = 1;
					if( pPlayer->m_iSgtcGondState == 1 && i == 0x12 )
						pPlayer->m_iSgtcGondState = 3;
					if( pPlayer->m_iSgtcGondState == 0 && i == 0x12 )
						pPlayer->m_iSgtcGondState = 2;
					if( pPlayer->m_iSgtcGondState == 2 && i == 0x11 )
						pPlayer->m_iSgtcGondState = 3;
				}
				else
					pPlayer->GiveNamedItem( s_SgtcSpawnWeapons[i] );
			}
		}
		// ref: armed shield state + !(pev->weapons bit 9) grants WPLIST[26].
		if( pPlayer->m_iSgtcGondState != 0 &&
			( ( pPlayer->pev->weapons >> 9 ) & 1 ) != 1 )
			pPlayer->GiveNamedItem( s_SgtcSpawnWeapons[0x1a] );	// WPLIST._68_4_
		if( pPlayer->pev->team == 1 )
			pPlayer->GiveNamedItem( "weapon_coujaf" );	// Goa'uld melee
		else
			pPlayer->GiveNamedItem( "weapon_couteau" );	// Torri melee
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pPlayer->pev );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 4 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	if( pPlayer->m_bSnagInvisLatch )				// ref +0x4de != 0
	{
		// Invis-class override: hidden model, 430 speed, 180 cap/armor.
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(),
			g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "invis" );	// DAT_001ebc6c
		pPlayer->pev->maxspeed = 430.0f;			// 0x43d70000
		pPlayer->m_iSnagArmorCap = 0xb4;			// +0x908 = 180
		pPlayer->pev->armorvalue = 180.0f;			// 0x43340000
		pPlayer->m_iNukeBonus = 0;					// +0x934 = 0
		pPlayer->m_bSgtcCloakTick = 1;
		pPlayer->GiveNamedItem( "weapon_coujaf" );
		pPlayer->GiveNamedItem( "weapon_lance" );
		pPlayer->GiveNamedItem( "weapon_zat" );
		MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 5 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	// ref: pev->flags bit14 == FL_DUCKING selects the ducked hull.
	if( ( ( pPlayer->pev->flags >> 14 ) & 1 ) == 0 )
		UTIL_SetSize( pPlayer->pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );
	else
		UTIL_SetSize( pPlayer->pev, Vector( -16, -16, -20 ), Vector( 16, 16, 20 ) );
	pPlayer->pev->view_ofs = Vector( 0, 0, 28 );	// ref pev+0x174

	pPlayer->m_iSgtcPrevClass = pPlayer->m_iSgtcPortraitShown;	// +0x500 = +0xa40
	pPlayer->m_iSgtcPrevPeTeam = (int)pPlayer->pev->team;		// +0x504 = pev->team

	if( pPlayer->m_bSgtcPePreset )					// ref +0x465 PE preset
	{
		pPlayer->pev->maxspeed = 400.0f;			// 0x43c80000
		pPlayer->pev->armorvalue = 100.0f;			// 0x42c80000
		pPlayer->pev->renderfx = 0x13;				// pev+0x15c
		pPlayer->pev->renderamt = 2.0f;				// pev+0x14c = 0x40000000
		pPlayer->pev->rendercolor = Vector( 255.0f, 0.0f, 0.0f );	// pev+0x150/+0x154/+0x158
	}

	int clientIndex = pPlayer->entindex();
	if( pPlayer->pev->team == 1 )
		pPlayer->m_iSgtcSubmodelIdx = 0;			// ref +0x440
	else if( pPlayer->pev->team == 2 )
		pPlayer->m_iSgtcSubmodelIdx = 1;
	else
		pPlayer->m_iSgtcSubmodelIdx = 2;
	pPlayer->ChangeSubmodel();						// ref @0x153e88

	// ref: vcall+0x94 (IsAlive) && StopObserver — verbatim dispatch.
	if( pPlayer->IsAlive() )
		pPlayer->StopObserver();
	if( !pPlayer->IsObserver() )
	{
		pPlayer->pev->renderamt = 255.0f;			// pev+0x14c = 0x437f0000
		pPlayer->pev->renderfx = 0;				// pev+0x15c
		pPlayer->pev->rendermode = 0;				// pev+0x148
		pPlayer->m_bSgtcInWorld = 1;				// ref +0x44b
	}
	if( pPlayer->IsObserver() )
		pPlayer->pev->team = 0;						// ref pev+0x1ac = 0

	MESSAGE_BEGIN( MSG_ONE, gmsgTicketMode, NULL, pPlayer->pev );
		WRITE_BYTE( sgtc_GameModeSet > 0 ? 1 : 0 );	// ref (0 < GameModeSet)
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( clientIndex );
		WRITE_SHORT( (int)pPlayer->pev->frags );	// ref pev+0x164
		WRITE_SHORT( pPlayer->m_iDeaths );			// ref +0xdf0
		WRITE_SHORT( pPlayer->m_bSnagInvisLatch == 1 ? 6 : pPlayer->m_iSgtcPortraitShown );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pPlayer->pev );
		WRITE_BYTE( (int)( pPlayer->pev->armorvalue * 100.0f /
			(float)pPlayer->m_iSnagArmorCap ) );	// ref ROUND(armor*100/cap)
		WRITE_BYTE( (int)sgtc_cvar_superweapons.value );	// ref ROUND(superguns+0xc)
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	pPlayer->SendAboutMe( (int)pPlayer->pev->team,
		STRING( pPlayer->pev->netname ), clientIndex );	// ref @0x1262b8
	RecountTeams( FALSE );

	if( !bHadLoadout && pPlayer->m_bSnagInvisLatch != 1 )
	{
		if( sgtc_cvar_wepselmode.value == 1.0f )		// ref weaponsel == 1.0
			pPlayer->ShowVGUIMenu( 0xc );
		else if( sgtc_cvar_wepselmode.value == 2.0f )	// ref weaponsel == 2.0
			pPlayer->ShowVGUIMenu( 0x3c );
	}
	pPlayer->m_bSgtcLatchA9c = 0;					// ref +0xa9c = 0
	AddPortrait( pPlayer );							// ref vtable+0x110
	if( pPlayer->IsAlive() && !pPlayer->IsObserver() )
		StartPortrait( pPlayer );					// ref vtable+0x114
}


// ref CStargateTeamDM::FPlayerCanRespawn @0x18ba08 (CP @0x17ea2a, identical
// except the round-over field: DM +0x740 / CP +0x73c — see
// rea_dump/CStargateTeam{DM,CP}__FPlayerCanRespawn.c):
//   1. if (m_iSgtcSnapClass != m_iSgtcPortraitShown || 0 < m_iSgtcPortraitSel)
//      { UpdateClassCount(); cap-check the effective class (pending else
//      shown) against the +0x438 pick's team; on a cap hit re-roll
//      RANDOM_LONG(1,5) into +0xa44 until an UNLIMITED (max_* == 0) class
//      comes up; }
//   2. if (0 < +0xa44) +0xa40 = +0xa44 (consume pending -> shown);
//   3. +0xa44 = 0 (always clear the pending selection);
//   4. +0x480 = +0xa40; +0x484 = pev->team (snapshot for the change detect);
//   5. return the round gate: roundOver || (no-player && ?) || (early-round
//      grace of mp_roundtime + (now - roundEnd) < 20 — ref +0x3e3 is only
//      ever cleared (InitHUD/spawn), never set, so those clauses are dead).
// Cap mapping: 1=syslord/oneill, 2=anubis(vs count_prime)/carter,
// 3=jaffa/jackson, 4=serpent/tealc, 5=horus/soldier(vs count_commander).
static BOOL SGTC_ClassCapped( int iTeam, int iClass )
{
	float flMax, flCount;
	switch( iClass )
	{
	case 1:
		flMax = ( iTeam == 2 ) ? sgtc_cvar_max_syslord.value : sgtc_cvar_max_oniell.value;
		flCount = (float)( ( iTeam == 2 ) ? sgtc_count_syslord : sgtc_count_oniell );
		break;
	case 2:
		flMax = ( iTeam == 2 ) ? sgtc_cvar_max_anubis.value : sgtc_cvar_max_carter.value;
		flCount = (float)( ( iTeam == 2 ) ? sgtc_count_prime : sgtc_count_carter );
		break;
	case 3:
		flMax = ( iTeam == 2 ) ? sgtc_cvar_max_jaffa.value : sgtc_cvar_max_jackson.value;
		flCount = (float)( ( iTeam == 2 ) ? sgtc_count_jaffa : sgtc_count_jackson );
		break;
	case 4:
		flMax = ( iTeam == 2 ) ? sgtc_cvar_max_serpent.value : sgtc_cvar_max_tealc.value;
		flCount = (float)( ( iTeam == 2 ) ? sgtc_count_serpent : sgtc_count_tealc );
		break;
	case 5:
		flMax = ( iTeam == 2 ) ? sgtc_cvar_max_horus.value : sgtc_cvar_max_commander.value;
		flCount = (float)( ( iTeam == 2 ) ? sgtc_count_horus : sgtc_count_commander );
		break;
	default:
		return FALSE;
	}
	// ref FPU-status idiom: capped iff max <= count && max != 0 (0 = unlimited).
	return ( flMax <= flCount && flMax != 0.0f ) ? TRUE : FALSE;
}

static void SGTC_ClassReroll( int iTeam, int &iPending )
{
	while( iPending == 0 )
	{
		int iRoll = RANDOM_LONG( 1, 5 );
		if( iTeam == 2 )
		{
			if( sgtc_cvar_max_syslord.value == 0.0f && iRoll == 1 )
				iPending = 1;
			if( sgtc_cvar_max_anubis.value == 0.0f && iRoll == 2 )
				iPending = 2;
			if( sgtc_cvar_max_jaffa.value == 0.0f && iRoll == 3 )
				iPending = 3;
			if( sgtc_cvar_max_serpent.value == 0.0f && iRoll == 4 )
				iPending = 4;
			if( sgtc_cvar_max_horus.value == 0.0f && iRoll == 5 )
				iPending = 5;
		}
		else if( iTeam == 1 )
		{
			if( sgtc_cvar_max_oniell.value == 0.0f && iRoll == 1 )
				iPending = 1;
			if( sgtc_cvar_max_carter.value == 0.0f && iRoll == 2 )
				iPending = 2;
			if( sgtc_cvar_max_jackson.value == 0.0f && iRoll == 3 )
				iPending = 3;
			if( sgtc_cvar_max_tealc.value == 0.0f && iRoll == 4 )
				iPending = 4;
			if( sgtc_cvar_max_commander.value == 0.0f && iRoll == 5 )
				iPending = 5;
		}
	}
}

BOOL CStargateTeamDM::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if( pPlayer->m_iSgtcSnapClass != pPlayer->m_iSgtcPortraitShown ||
		0 < pPlayer->m_iSgtcPortraitSel )
	{
		UpdateClassCount();	// ref vtable+0x118

		int iClass = ( pPlayer->m_iSgtcPortraitSel != 0 )
			? pPlayer->m_iSgtcPortraitSel : pPlayer->m_iSgtcPortraitShown;
		int iTeam = pPlayer->m_iSgtcTeam;	// ref +0x438 pick numbering (1=Torri, 2=Goa'uld)

		if( SGTC_ClassCapped( iTeam, iClass ) )
		{
			pPlayer->m_iSgtcPortraitSel = 0;
			SGTC_ClassReroll( iTeam, pPlayer->m_iSgtcPortraitSel );
		}
	}

	if( 0 < pPlayer->m_iSgtcPortraitSel )
		pPlayer->m_iSgtcPortraitShown = pPlayer->m_iSgtcPortraitSel;
	pPlayer->m_iSgtcPortraitSel = 0;
	pPlayer->m_iSgtcSnapClass = pPlayer->m_iSgtcPortraitShown;	// +0x480
	pPlayer->m_iSgtcSnapTeam = (int)pPlayer->pev->team;		// +0x484

	// return gate (ref DM FPlayerCanRespawn @0x17c122, REA-verified 2026-10-17):
	// TRUE when +0x740==1, OR (+0x740==0 && +0x3e3!=0), OR (+0x3e3==0 &&
	// now >= (now - +0x724) + +0x728 + 20.0). The +0x724/+0x728 pair is the
	// rules round-start/duration pair (this+0x72c-roundstart + mp_roundtime in
	// this port), so the third clause is roundstart + mp_roundtime + 20s.
	return ( m_bRoundOver != 0 ||
		( sgtc_hasplayed == 0 && gpGlobals->time >=
			sgtc_roundstart + m_flSgtcRoundTime + 20.0f ) ) ? TRUE : FALSE;
}

BOOL CStargateTeamCP::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	// ref CP FPlayerCanRespawn @0x17ea2a — identical body to DM @0x18ba08 but
	// the round-over field is CP +0x73c (this port's m_iSgtcRoundTicks). Same
	// consume/cap/reroll/snapshot steps; see rea_dump/CStargateTeamCP__FPlayerCanRespawn.c
	if( pPlayer->m_iSgtcSnapClass != pPlayer->m_iSgtcPortraitShown ||
		0 < pPlayer->m_iSgtcPortraitSel )
	{
		UpdateClassCount();	// ref vtable+0x118

		int iClass = ( pPlayer->m_iSgtcPortraitSel != 0 )
			? pPlayer->m_iSgtcPortraitSel : pPlayer->m_iSgtcPortraitShown;
		int iTeam = pPlayer->m_iSgtcTeam;

		if( SGTC_ClassCapped( iTeam, iClass ) )
		{
			pPlayer->m_iSgtcPortraitSel = 0;
			SGTC_ClassReroll( iTeam, pPlayer->m_iSgtcPortraitSel );
		}
	}

	if( 0 < pPlayer->m_iSgtcPortraitSel )
		pPlayer->m_iSgtcPortraitShown = pPlayer->m_iSgtcPortraitSel;
	pPlayer->m_iSgtcPortraitSel = 0;
	pPlayer->m_iSgtcSnapClass = pPlayer->m_iSgtcPortraitShown;	// +0x480
	pPlayer->m_iSgtcSnapTeam = (int)pPlayer->pev->team;		// +0x484

	// return gate (ref CP FPlayerCanRespawn @0x16f144, REA-verified 2026-10-17):
	// TRUE when +0x73c==1, OR (+0x73c==0 && +0x3e3!=0), OR (+0x3e3==0 &&
	// now >= (now - +0x724) + +0x728 + 20.0) — the +0x724/+0x728 pair is the
	// rules round-start/duration pair (this+0x72c-roundstart + mp_roundtime in
	// this port), so the third clause is roundstart + mp_roundtime + 20s.
	return ( m_bRoundOver != 0 ||
		( sgtc_hasplayed == 0 && gpGlobals->time >=
			sgtc_roundstart + m_flSgtcRoundTime + 20.0f ) ) ? TRUE : FALSE;
}

BOOL CStargateTeamDM::FPlayerCanChangeInfo( CBasePlayer *pPlayer )
{
	(void)pPlayer;
	return TRUE;
}


void CStargateTeamDM::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	if( m_bDisableDeathMessages )	// ref this+0xc gate
		return;
	if( pVictim && pKiller && ( pKiller->flags & FL_CLIENT ) )
	{
		CBasePlayer *pk = (CBasePlayer *)CBaseEntity::Instance( pKiller );
		if( pk && pk != pVictim && PlayerRelationship( pVictim, pk ) == GR_TEAMMATE )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
				WRITE_BYTE( ENTINDEX( ENT( pKiller ) ) );
				WRITE_BYTE( ENTINDEX( pVictim->edict() ) );
				WRITE_STRING( "teammate" );
			MESSAGE_END();
			return;
		}
	}
	CHalfLifeMultiplay::DeathNotice( pVictim, pKiller, pInflictor );
}

void CStargateTeamDM::ClientDisconnected( edict_t *pClient )
{
	CHalfLifeMultiplay::ClientDisconnected( pClient );
	if( gmsgSpectator )	// ref @0x1179f2 sends Spectator on disconnect
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
			WRITE_BYTE( ENTINDEX( pClient ) );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
	RecountTeams( TRUE );
}

// ref CStargateTeamDM::ClientCommand @0x191336 — verbatim verb chain:
// voice → menuselect (drains argv) → client → radio → customcolor → spook →
// fakeman → spookstorm → timeleft → sayitloud → autopick → jointeam (inert)
// → mod → showmenu(0xb) → showequip → showmotd(10) → kill → classe →
// resetptr/recount/toggleportrait tail. No CHalfLifeTeamplay fallback — the
// ref returns 0 (engine prints "Unknown command").
BOOL CStargateTeamDM::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if( FStrEq( pcmd, "menuselect" ) )
	{
		// ref drains argv(1) and returns — the export-level ClientCommand
		// @0xd8ff2 has no menuselect case, so this rules handler is the only
		// receiver and does nothing (bomb-key arming is the VoteMode verb).
		if( CMD_ARGC() > 1 )
			atoi( CMD_ARGV( 1 ) );
		return TRUE;
	}

	if( FStrEq( pcmd, "client" ) )	// ref: missing arg prints notice
	{
		if( CMD_ARGC() < 1 )
		{
			ClientPrint( pPlayer->pev, HUD_PRINTNOTIFY,
				"missing parameter, use clindex\n" );
			return TRUE;
		}
		pPlayer->m_iSgtcClientIdx = atoi( CMD_ARGV( 1 ) );	// ref +0x3e8
		return TRUE;
	}

	if( FStrEq( pcmd, "radio" ) )
	{
		if( CMD_ARGC() < 1 )
			return TRUE;
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( pPlayer->m_iSgtcClientIdx );
		if( plr != NULL )
		{
			// ref: MSG_ONE gmsgRadio to the addressed client — BYTE cmd, BYTE sender.
			MESSAGE_BEGIN( MSG_ONE, gmsgRadio, NULL, plr->edict() );
				WRITE_BYTE( atoi( CMD_ARGV( 1 ) ) );
				WRITE_BYTE( pPlayer->entindex() );
			MESSAGE_END();
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "customcolor" ) )
	{
		if( CMD_ARGC() > 0 )
			pPlayer->m_iSgtcCustomColor = atoi( CMD_ARGV( 1 ) );	// ref +0x450
		return TRUE;
	}

	if( FStrEq( pcmd, "spook" ) )
	{
		if( !pPlayer->m_bShieldLarge )	// ref +0x460 gate — dev bigshield flag
			return FALSE;
		Vector vSpawn = pPlayer->pev->origin;
		vSpawn.z += 120.0f;	// ref: z +120.0
		vSpawn.x += 70.0f;	// ref: x +70.0
		CBaseEntity *pMon = CBaseEntity::Create( "monster_implanted",
			vSpawn, pPlayer->pev->v_angle, NULL );
		if( pMon )
			pMon->pev->owner = pPlayer->edict();	// ref ent+0xc4 owner write
		return TRUE;
	}

	if( FStrEq( pcmd, "fakeman" ) )
	{
		if( !pPlayer->m_bShieldLarge )
			return FALSE;
		// ref: MSG_ALL fake AddPortrait — "Endo-Clone" scoreboard row.
		MESSAGE_BEGIN( MSG_ALL, gmsgAddPortrait, NULL );
			WRITE_SHORT( RANDOM_LONG( 1, 10 ) );
			WRITE_LONG( 100 );
			WRITE_SHORT( 2 );
			WRITE_SHORT( RANDOM_LONG( 1, 5 ) );
			WRITE_SHORT( RANDOM_LONG( 1, 16 ) );
			WRITE_SHORT( 100 );
			WRITE_SHORT( 100 );
			WRITE_SHORT( 0 );
			WRITE_STRING( "Endo-Clone" );
			WRITE_BYTE( 0 );
		MESSAGE_END();
		return TRUE;
	}

	if( FStrEq( pcmd, "spookstorm" ) )
	{
		if( pPlayer->m_bShieldLarge != 1 || CMD_ARGC() < 1 )
			return FALSE;
		// ref: spawn a monster_implanted at each info_player_torri spawn,
		// argv(1) entries deep; stop when the spawn list runs out.
		int iDepth = 0;
		CBaseEntity *pSpot = NULL;
		while( ( pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_torri" ) ) != NULL &&
			iDepth < atoi( CMD_ARGV( 1 ) ) )
		{
			iDepth++;
			CBaseEntity *pMon = CBaseEntity::Create( "monster_implanted",
				pSpot->pev->origin, pSpot->pev->v_angle, NULL );
			if( pMon )
				pMon->pev->owner = pPlayer->edict();	// ref ent+0xc4
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "timeleft" ) )
	{
		char szTime[64];
		int iSec = (int)timeleft.value;
		if( iSec % 60 < 10 )
			snprintf( szTime, sizeof( szTime ), "%d:0%d", iSec / 60, iSec % 60 );
		else
			snprintf( szTime, sizeof( szTime ), "%d:%d", iSec / 60, iSec % 60 );
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "%s: %s\n", "#time_left", szTime );
		return FALSE;	// ref returns 0 — engine prints "Unknown command"
	}

	if( FStrEq( pcmd, "sayitloud" ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTTALK,
			UTIL_VarArgs( "SteamID: %d\n", pPlayer->m_iSgtcSpecMode ) );	// ref +0x914
		return TRUE;
	}

	if( FStrEq( pcmd, "autopick" ) )
	{
		// ref: focused-count auto team pick + random class (1..5).
		int iTeam = 0;
		if( sgtc_G_players_focused < sgtc_T_players_focused )
			iTeam = 1;
		else if( sgtc_T_players_focused < sgtc_G_players_focused )
			iTeam = 2;
		else
			iTeam = RANDOM_LONG( 1, 2 );
		pPlayer->m_iSgtcTeam = iTeam;					// ref +0x438
		pPlayer->m_iSgtcPortraitSel = RANDOM_LONG( 1, 5 );	// ref +0xa44
		pPlayer->m_bSgtcBadGuy = 0;						// ref +0x462
		return TRUE;
	}

	if( FStrEq( pcmd, "jointeam" ) )
		return TRUE;	// ref: inert verb (team comes from classe argv 2)

	if( FStrEq( pcmd, "mod" ) )
		return FALSE;	// ref returns 0

	if( FStrEq( pcmd, "showmenu" ) )
	{
		pPlayer->ShowVGUIMenu( 11 );	// ref 0xb — team/class select
		return TRUE;
	}

	if( FStrEq( pcmd, "showequip" ) )
	{
		if( pPlayer->m_bSnagInvisLatch != 1 )	// ref +0x4de
		{
			if( sgtc_cvar_wepselmode.value == 1.0f )
				pPlayer->ShowVGUIMenu( 0xc );
			else if( sgtc_cvar_wepselmode.value == 2.0f )
				pPlayer->ShowVGUIMenu( 0x3c );
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "showmotd" ) )
	{
		pPlayer->ShowVGUIMenu( 10 );	// ref 0xa
		return TRUE;
	}

	if( FStrEq( pcmd, "kill" ) )
	{
		pPlayer->m_bLanceStunFlag = 0;	// ref +0xa3e
		return TRUE;
	}

	if( FStrEq( pcmd, "classe" ) )
	{
		SGTC_CommandClasse( pPlayer, FALSE );
		return TRUE;
	}

	if( FStrEq( pcmd, "resetptr" ) )
	{
		pPlayer->ResetPortrait( 1 );
		return FALSE;	// ref returns 0 after the reset
	}

	if( FStrEq( pcmd, "recount" ) )
	{
		RecountTeams( FALSE );
		return TRUE;
	}

	if( FStrEq( pcmd, "toggleportrait" ) )
	{
		pPlayer->TogglePortrait();
		return TRUE;
	}

	return FALSE;	// ref returns 0 — engine prints "Unknown command"
}

// ---------------------------------------------------------------------------
// SGTC_CommandClasse — shared body of the ref 'classe' verb
// (DM @0x191336 tail / CP @0x183b88 tail). argv(1) = class 1..5, argv(2) =
// team 1|2 (1 = Torri, 2 = Goa'uld — the pending +0x438 numbering, opposite
// of pev->team where 1 = Goa'uld, 2 = Torri). The pending fields land in
// +0x438/+0xa44 and PlayerThink consumes them on the 1s dead tick.
// bCaptureMode selects the CP-only type-4 side gate (ref rules+0x734).
// ---------------------------------------------------------------------------
void CStargateTeamDM::SGTC_CommandClasse( CBasePlayer *pPlayer, BOOL bCaptureMode )
{
	pPlayer->m_bSgtcBadGuy = 0;		// ref +0x462 = 0
	pPlayer->m_iSgtcTeam = 0;		// ref +0x438 = 0
	RecountTeams( FALSE );
	UpdateClassCount();				// ref vtable+0x118

	int iTeamArg = atoi( CMD_ARGV( 2 ) );

	// ref DM gate: during an official round an EXPLICIT team arg is refused
	// (auto-pick path below still runs — ref allows classe <class> to slip
	// through with a count-based pick). CP skips this gate entirely.
	if( !bCaptureMode && sgtc_round_official == 1 && iTeamArg >= 1 )
		return;	// ref return 1 — pick silently refused

	if( iTeamArg >= 1 )
	{
		if( bCaptureMode && sgtc_type == 4 )
		{
			// ref CP: a player already on a side cannot swap INTO the
			// attacker team — m_iSgtcCPSide==0 makes Torri (arg 1) the
			// attacker; nonzero makes Goa'uld (arg 2) the attacker.
			const int iSide = ((CStargateTeamCP *)this)->m_iSgtcCPSide;
			if( ( iSide == 0 && iTeamArg == 1 && pPlayer->pev->team == 2 ) ||
				( iSide != 0 && iTeamArg == 2 && pPlayer->pev->team == 1 ) )
				return;	// ref return 1 — pick silently refused
		}

		if( sgtc_cvar_autobalance.value <= 0.0f )
		{
			pPlayer->m_iSgtcTeam = iTeamArg;
		}
		else
		{
			// ref autobalance: joining the BIGGER focused team past the
			// cvar threshold is refused — the player is flipped to the
			// other side and told.
			BOOL bAllow1 = TRUE, bAllow2 = TRUE;
			if( sgtc_T_players_focused < sgtc_G_players_focused &&
				sgtc_cvar_autobalance.value < (float)( sgtc_G_players_focused - sgtc_T_players_focused ) )
				bAllow2 = FALSE;	// ref local_22 (can't join team 2)
			if( sgtc_G_players_focused < sgtc_T_players_focused &&
				sgtc_cvar_autobalance.value < (float)( sgtc_T_players_focused - sgtc_G_players_focused ) )
				bAllow1 = FALSE;	// ref local_21 (can't join team 1)
			if( ( iTeamArg == 1 && bAllow1 ) || ( iTeamArg == 2 && bAllow2 ) )
				pPlayer->m_iSgtcTeam = iTeamArg;
			else
			{
				pPlayer->m_iSgtcTeam = ( iTeamArg == 1 ) ? 2 : 1;
				ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "Teams Unbalanced\n" );
			}
		}
	}

	if( pPlayer->m_iSgtcTeam == 0 )
	{
		if( sgtc_T_players_focused < sgtc_G_players_focused )
			pPlayer->m_iSgtcTeam = 2;
		else if( sgtc_G_players_focused < sgtc_T_players_focused )
			pPlayer->m_iSgtcTeam = 1;
		else
			pPlayer->m_iSgtcTeam = RANDOM_LONG( 1, 2 );
	}

	if( CMD_ARGC() > 0 )
	{
		pPlayer->m_iSgtcPortraitSel = 0;							// ref +0xa44
		pPlayer->m_iSgtcPortraitPrev = pPlayer->m_iSgtcPortraitShown;	// ref +0xa38
		if( sgtc_cvar_classlimitmodel.value != 0.0f )
			pPlayer->m_iSgtcPortraitShown = 0;					// ref +0xa40

		int iClass = atoi( CMD_ARGV( 1 ) );
		int iTeam = pPlayer->m_iSgtcTeam;
		BOOL bCapped = FALSE;
		RecountTeams( FALSE );
		UpdateClassCount();	// ref vtable+0x118 — fresh counts for the caps

		if( iTeam == 2 )	// Goa'uld side (ref max_syslord/anubis/jaffa/serpent/horus)
		{
			switch( iClass )
			{
			case 1:
				if( sgtc_cvar_max_syslord.value <= (float)sgtc_count_syslord &&
					sgtc_cvar_max_syslord.value != 0.0f )
					bCapped = TRUE;
				break;
			case 2:
				if( sgtc_cvar_max_anubis.value <= (float)sgtc_count_prime &&
					sgtc_cvar_max_anubis.value != 0.0f )
					bCapped = TRUE;
				break;
			case 3:
				if( sgtc_cvar_max_jaffa.value <= (float)sgtc_count_jaffa &&
					sgtc_cvar_max_jaffa.value != 0.0f )
					bCapped = TRUE;
				break;
			case 4:
				if( sgtc_cvar_max_serpent.value <= (float)sgtc_count_serpent &&
					sgtc_cvar_max_serpent.value != 0.0f )
					bCapped = TRUE;
				break;
			case 5:
				if( sgtc_cvar_max_horus.value <= (float)sgtc_count_horus &&
					sgtc_cvar_max_horus.value != 0.0f )
					bCapped = TRUE;
				break;
			}
		}
		else if( iTeam == 1 )	// Torri side (ref max_oneill/carter/jackson/tealc/soldier)
		{
			switch( iClass )
			{
			case 1:
				if( sgtc_cvar_max_oniell.value <= (float)sgtc_count_oniell &&
					sgtc_cvar_max_oniell.value != 0.0f )
					bCapped = TRUE;
				break;
			case 2:
				if( sgtc_cvar_max_carter.value <= (float)sgtc_count_carter &&
					sgtc_cvar_max_carter.value != 0.0f )
					bCapped = TRUE;
				break;
			case 3:
				if( sgtc_cvar_max_jackson.value <= (float)sgtc_count_jackson &&
					sgtc_cvar_max_jackson.value != 0.0f )
					bCapped = TRUE;
				break;
			case 4:
				if( sgtc_cvar_max_tealc.value <= (float)sgtc_count_tealc &&
					sgtc_cvar_max_tealc.value != 0.0f )
					bCapped = TRUE;
				break;
			case 5:
				if( sgtc_cvar_max_commander.value <= (float)sgtc_count_commander &&
					sgtc_cvar_max_commander.value != 0.0f )
					bCapped = TRUE;
				break;
			}
		}

		if( !bCapped && iClass > 0 && iClass < 6 )
		{
			pPlayer->m_iSgtcPortraitSel = iClass;
			return;
		}

		// ref: invalid or capped pick → keep rolling 1..5 until an UNLIMITED
		// (max_* == 0) class comes up for the chosen team.
		if( iTeam == 2 )
		{
			while( pPlayer->m_iSgtcPortraitSel == 0 )
			{
				int iRoll = RANDOM_LONG( 1, 5 );
				if( sgtc_cvar_max_syslord.value == 0.0f && iRoll == 1 )
					pPlayer->m_iSgtcPortraitSel = 1;
				if( sgtc_cvar_max_anubis.value == 0.0f && iRoll == 2 )
					pPlayer->m_iSgtcPortraitSel = 2;
				if( sgtc_cvar_max_jaffa.value == 0.0f && iRoll == 3 )
					pPlayer->m_iSgtcPortraitSel = 3;
				if( sgtc_cvar_max_serpent.value == 0.0f && iRoll == 4 )
					pPlayer->m_iSgtcPortraitSel = 4;
				if( sgtc_cvar_max_horus.value == 0.0f && iRoll == 5 )
					pPlayer->m_iSgtcPortraitSel = 5;
			}
		}
		if( iTeam == 1 )
		{
			while( pPlayer->m_iSgtcPortraitSel == 0 )
			{
				int iRoll = RANDOM_LONG( 1, 5 );
				if( sgtc_cvar_max_oniell.value == 0.0f && iRoll == 1 )
					pPlayer->m_iSgtcPortraitSel = 1;
				if( sgtc_cvar_max_carter.value == 0.0f && iRoll == 2 )
					pPlayer->m_iSgtcPortraitSel = 2;
				if( sgtc_cvar_max_jackson.value == 0.0f && iRoll == 3 )
					pPlayer->m_iSgtcPortraitSel = 3;
				if( sgtc_cvar_max_tealc.value == 0.0f && iRoll == 4 )
					pPlayer->m_iSgtcPortraitSel = 4;
				if( sgtc_cvar_max_commander.value == 0.0f && iRoll == 5 )
					pPlayer->m_iSgtcPortraitSel = 5;
			}
		}
	}
}

// ref CStargateTeamCP::ClientCommand @0x183b88 — CP verb chain. The CP table
// drops jointeam/mod (classe does the join work) and adds n00bkill +
// oldobserver before recount/toggleportrait. classe is the CP variant (the
// type-4 side gate replaces DM's round_official gate).
BOOL CStargateTeamCP::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if( FStrEq( pcmd, "menuselect" ) )
	{
		if( CMD_ARGC() > 1 )
			atoi( CMD_ARGV( 1 ) );	// ref drains the arg
		return TRUE;
	}

	if( FStrEq( pcmd, "spook" ) )
	{
		if( !pPlayer->m_bShieldLarge )
			return FALSE;
		Vector vSpawn = pPlayer->pev->origin;
		vSpawn.z += 120.0f;
		vSpawn.x += 70.0f;
		CBaseEntity *pMon = CBaseEntity::Create( "monster_implanted",
			vSpawn, pPlayer->pev->v_angle, NULL );
		if( pMon )
			pMon->pev->owner = pPlayer->edict();
		return TRUE;
	}

	if( FStrEq( pcmd, "spookstorm" ) )
	{
		if( pPlayer->m_bShieldLarge != 1 || CMD_ARGC() < 1 )
			return FALSE;
		int iDepth = 0;
		CBaseEntity *pSpot = NULL;
		while( ( pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_torri" ) ) != NULL &&
			iDepth < atoi( CMD_ARGV( 1 ) ) )
		{
			iDepth++;
			CBaseEntity *pMon = CBaseEntity::Create( "monster_implanted",
				pSpot->pev->origin, pSpot->pev->v_angle, NULL );
			if( pMon )
				pMon->pev->owner = pPlayer->edict();
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "customcolor" ) )
	{
		if( CMD_ARGC() > 0 )
			pPlayer->m_iSgtcCustomColor = atoi( CMD_ARGV( 1 ) );	// ref +0x450
		return TRUE;
	}

	if( FStrEq( pcmd, "client" ) )
	{
		if( CMD_ARGC() < 1 )
		{
			ClientPrint( pPlayer->pev, HUD_PRINTNOTIFY,
				"parametre manquant, utilisez la commande \"client clindex\"\n" );	// ref string
			return TRUE;
		}
		pPlayer->m_iSgtcClientIdx = atoi( CMD_ARGV( 1 ) );	// ref +0x3e8
		return TRUE;
	}

	if( FStrEq( pcmd, "radio" ) )
	{
		if( CMD_ARGC() < 1 )
			return TRUE;
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( pPlayer->m_iSgtcClientIdx );
		if( plr != NULL )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgRadio, NULL, plr->edict() );
				WRITE_BYTE( atoi( CMD_ARGV( 1 ) ) );
				WRITE_BYTE( pPlayer->entindex() );
			MESSAGE_END();
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "testmsg" ) )
		return TRUE;	// ref: inert

	if( FStrEq( pcmd, "timeleft" ) )
	{
		char szTime[64];
		int iSec = (int)timeleft.value;
		if( iSec % 60 < 10 )
			snprintf( szTime, sizeof( szTime ), "%d:0%d", iSec / 60, iSec % 60 );
		else
			snprintf( szTime, sizeof( szTime ), "%d:%d", iSec / 60, iSec % 60 );
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "%s: %s\n", "#time_left", szTime );
		return FALSE;	// ref returns 0
	}

	if( FStrEq( pcmd, "autopick" ) )
	{
		int iTeam = 0;
		if( sgtc_G_players_focused < sgtc_T_players_focused )
			iTeam = 1;
		else if( sgtc_T_players_focused < sgtc_G_players_focused )
			iTeam = 2;
		else
			iTeam = RANDOM_LONG( 1, 2 );
		pPlayer->m_iSgtcTeam = iTeam;
		pPlayer->m_iSgtcPortraitSel = RANDOM_LONG( 1, 5 );
		pPlayer->m_bSgtcBadGuy = 0;
		return TRUE;
	}

	if( FStrEq( pcmd, "n00bkill" ) )
	{
		// ref: dev nuke — kills every ALIVE player on the OTHER pev->team.
		if( pPlayer->m_bShieldLarge )
		{
			for( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if( plr && plr->IsAlive() &&
					plr->pev->team != pPlayer->pev->team )
					PlayerKilled( plr, pPlayer->pev, plr->pev );	// ref vtable+0x84
			}
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "showmenu" ) )
	{
		pPlayer->ShowVGUIMenu( 11 );	// ref 0xb
		return TRUE;
	}

	if( FStrEq( pcmd, "showequip" ) )
	{
		if( pPlayer->m_bSnagInvisLatch != 1 )
		{
			if( sgtc_cvar_wepselmode.value == 1.0f )
				pPlayer->ShowVGUIMenu( 0xc );
			else if( sgtc_cvar_wepselmode.value == 2.0f )
				pPlayer->ShowVGUIMenu( 0x3c );
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "showmotd" ) )
	{
		pPlayer->ShowVGUIMenu( 10 );
		return TRUE;
	}

	if( FStrEq( pcmd, "kill" ) )
	{
		pPlayer->m_bLanceStunFlag = 0;	// ref +0xa3e
		return TRUE;
	}

	if( FStrEq( pcmd, "classe" ) )
	{
		SGTC_CommandClasse( pPlayer, TRUE );
		return TRUE;
	}

	if( FStrEq( pcmd, "resetptr" ) )
	{
		pPlayer->ResetPortrait( 1 );
		return FALSE;	// ref returns 0
	}

	if( FStrEq( pcmd, "recount" ) )
	{
		RecountTeams( FALSE );
		return TRUE;
	}

	if( FStrEq( pcmd, "oldobserver" ) )
	{
		// ref: untime the ground flag, wipe the team name, move into a traced
		// observer spot 128u up, then resend TeamInfo for valid teammates.
		pPlayer->pev->flags &= ~FL_ONGROUND;	// ref pev+0x1a4 & 0xfffffdff
		pPlayer->m_szTeamName[0] = '\0';
		RecountTeams( FALSE );
		TraceResult tr;
		UTIL_TraceLine( pPlayer->pev->origin, pPlayer->pev->origin + Vector( 0, 0, 128 ),
			ignore_monsters, pPlayer->edict(), &tr );
		pPlayer->StartObserver( tr.vecEndPos,
			UTIL_VecToAngles( tr.vecEndPos - pPlayer->pev->origin ) );
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
		return TRUE;
	}

	if( FStrEq( pcmd, "toggleportrait" ) )
	{
		pPlayer->TogglePortrait();
		return TRUE;
	}

	return FALSE;
}

void CStargateTeamDM::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	// ref CStargateTeamDM::ClientUserInfoChanged @0x190288.
	// NOT the stock teamplay body — no IsValidTeam/"Can't change team"
	// rejection; the ref derives pev->team + the "model" key from
	// m_szTeamName/m_iSgtcPortraitShown.
	(void)infobuffer;
	if( pPlayer->IsObserver() )
		pPlayer->pev->team = 0;						// ref pev+0x1ac = 0

	if( pPlayer->m_iSgtcInfoLock != 1 &&			// ref +0xdc != 1
		pPlayer->m_bSnagInvisLatch != 1 )			// ref +0x4de != 1
	{
		char *infobuf = g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() );	// ref DAT_00238fa8
		if( !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) )
		{
			pPlayer->pev->team = 1;
			switch( pPlayer->m_iSgtcPortraitShown )				// ref +0xa40
			{
			case 1:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "systemlord" );	break;
			case 2:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "anubis" );		break;
			case 3:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "goauld3" );		break;
			case 4:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "snakegoauld" );	break;
			case 5:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "horus" );		break;
			}
		}
		else if( !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) )
		{
			pPlayer->pev->team = 2;
			switch( pPlayer->m_iSgtcPortraitShown )
			{
			case 1:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "oneill" );		break;
			case 2:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "carter" );		break;
			case 3:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "jackson" );	break;
			case 4:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "tealc" );		break;
			case 5:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "commander" );	break;
			}
		}
	}

	// DM-only tail: invis latch (+0x4de) pushes "unasteammate" (DAT_001ebc6c)
	// when the info lock (+0xdc) isn't forced.
	if( pPlayer->m_bSnagInvisLatch != 0 && pPlayer->m_iSgtcInfoLock != 1 )
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(),
			g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "unasteammate" );

	pPlayer->SendAboutMe( (int)pPlayer->pev->team,
		STRING( pPlayer->pev->netname ), pPlayer->entindex() );	// ref: pev+0x1ac, STRING(netname), pev+0x1d0
	RecountTeams( FALSE );

	if( !pPlayer->IsObserver() )
		pPlayer->m_bSgtcPadA3D = 1;					// ref +0xa3d = 1 (DM only)

	UpdateGameMode( pPlayer );						// ref vtable+0x34 tail
}

int CStargateTeamDM::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	(void)pPlayer;
	return GR_PLR_DROP_GUN_ACTIVE; // .so DeadPlayerWeapons: keep active gun
}

int CStargateTeamDM::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	(void)pPlayer;
	return GR_PLR_DROP_AMMO_ACTIVE; // .so DeadPlayerAmmo path
}

// Goal / EndRound / NewRound moved to sgtc_round.cpp (ref nm 0x17b650,
// 0x17b77c, 0x184f42) — the stub bodies here were superseded by the full
// round-lifecycle port.

void CStargateTeamDM::GlobalText( int iTeam, int iText )
{
	// .so GlobalText: per-team SayText fan-out.
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !pl || pl->pev->deadflag != DEAD_NO )
			continue;
		if( iTeam && GetTeamIndex( pl->m_szTeamName ) != iTeam - 1 )
			continue;
		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, pl->edict() );
			WRITE_BYTE( ENTINDEX( INDEXENT( 0 ) ) );
			WRITE_STRING( UTIL_VarArgs( "#SGTC_GLOBAL_%d", iText ) );
		MESSAGE_END();
	}
}

// ref client MsgFunc_AddPortrait @0x100768d4 reads
// SHORT spr, LONG hpPct, SHORT team, SHORT aux, SHORT ent,
// SHORT hp0, SHORT hp1, SHORT hp2, STRING name, BYTE colorId.
// Team 2 tints the radar green; any other value is red.
static int SGTC_PortraitSprId( CBasePlayer *pPlayer )
{
	int cls = pPlayer->m_iSgtcPortraitShown;
	if( cls < 1 || cls > 5 )
		cls = 1;
	// Avatar table (client FUN_10072923 / SGTC_PortraitAvatarSprite) is
	// 0-based: 0..4 Tau'ri, 5..9 Goa'uld. Ref own-cell avatar for Prima
	// (Goa'uld class 4) is index 8 = snakegoauld; the old cls+5 / cls form
	// shifted every class by one (Prima drew horus, Torri 1 drew carter) and
	// the trailing id byte was hardcoded 0 (everyone drew oneill).
	return ( (int)pPlayer->pev->team == 1 ) ? cls + 4 : cls - 1;
}

static void SGTC_EmitAddPortrait( int msgDest, edict_t *to, CBasePlayer *subject )
{
	int hp = (int)subject->pev->health;
	if( hp < 0 )
		hp = 0;
	if( hp > 100 )
		hp = 100;
	const char *name = STRING( subject->pev->netname );
	if( !name || !name[0] )
		name = subject->m_szTeamName[0] ? subject->m_szTeamName : "player";

	int sprId = SGTC_PortraitSprId( subject );	// ref own-cell + slot avatar id

	MESSAGE_BEGIN( msgDest, gmsgAddPortrait, NULL, to );
		WRITE_SHORT( sprId );
		WRITE_LONG( hp );
		WRITE_SHORT( (int)subject->pev->team );
		WRITE_SHORT( subject->m_iSgtcPortraitShown );
		WRITE_SHORT( subject->entindex() );
		WRITE_SHORT( hp );
		WRITE_SHORT( 100 );
		WRITE_SHORT( (int)subject->pev->armorvalue );
		WRITE_STRING( name );
		WRITE_BYTE( sprId );
	MESSAGE_END();
}

void CStargateTeamDM::AddPortrait( CBasePlayer *pPlayer )
{
	SGTC_EmitAddPortrait( MSG_ONE, pPlayer->edict(), pPlayer );
}

void CStargateTeamDM::StartPortrait( CBasePlayer *pPlayer )
{
	SGTC_EmitAddPortrait( MSG_ONE, pPlayer->edict(), pPlayer );
}

void CStargateTeamDM::RedrawPortrait( CBasePlayer *pPlayer )
{
	// ref CStargateTeamCP::RedrawPortrait @0x17fc58 (DM @0x18cc54): when
	// +0x462 (m_bSgtcBadGuy) is clear, map client "model" to portrait id 1..12
	// and AddHudPortrait(MSG_ALL).
	if( pPlayer->m_bSgtcBadGuy )
		return;

	char *infobuf = g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() );
	const char *model = g_engfuncs.pfnInfoKeyValue( infobuf, "model" );
	int sprId = 12;
	if( model && model[0] )
	{
		for( int i = 0; i < (int)SGTC_NUM_PORTRAITS; i++ )
		{
			if( !strcmp( model, s_SGTCPortraits[i] ) )
			{
				sprId = i;	// client avatar table is 0-based (0 = oneill)
				break;
			}
		}
	}

	int hp = (int)pPlayer->pev->health;
	if( hp < 0 )
		hp = 0;
	if( hp > 100 )
		hp = 100;
	const char *name = STRING( pPlayer->pev->netname );
	if( !name || !name[0] )
		name = pPlayer->m_szTeamName[0] ? pPlayer->m_szTeamName : "player";

	MESSAGE_BEGIN( MSG_ALL, gmsgAddPortrait, NULL );
		WRITE_SHORT( sprId );
		WRITE_LONG( hp );
		WRITE_SHORT( (int)pPlayer->pev->team );
		WRITE_SHORT( pPlayer->m_iSgtcPortraitShown );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_SHORT( hp );
		WRITE_SHORT( 100 );
		WRITE_SHORT( (int)pPlayer->pev->armorvalue );
		WRITE_STRING( name );
		WRITE_BYTE( sprId );
	MESSAGE_END();
}

// ref CStargateTeamDM::UpdateClassCount @0x18c2e2 (CP @0x17f308, identical body)
// — counts each alive-teamed player's SHOWN class (+0xa40) into the per-class
// globals by pev->team (1=Goa'uld, 2=Torri). vtable+0xa8 = TeamID() non-null
// check; the +0xa44!=0 && classlimitmode!=0 gate skips players mid-pick unless
// class limiting is off. The classe verb calls this via vtable+0x118.
void CStargateTeamDM::UpdateClassCount( void )
{
	sgtc_count_oniell = sgtc_count_carter = sgtc_count_jackson =
		sgtc_count_tealc = sgtc_count_commander = 0;
	sgtc_count_syslord = sgtc_count_prime = sgtc_count_jaffa =
		sgtc_count_serpent = sgtc_count_horus = 0;

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !plr || !plr->TeamID() )
			continue;
		if( plr->m_iSgtcPortraitShown <= 0 || plr->pev->team <= 0 )
			continue;
		if( plr->m_iSgtcPortraitSel != 0 &&
			sgtc_cvar_classlimitmodel.value != 0.0f )
			continue;
		if( plr->pev->team == 1 )
		{
			switch( plr->m_iSgtcPortraitShown )
			{
			case 1: sgtc_count_syslord++;	break;
			case 2: sgtc_count_prime++;		break;
			case 3: sgtc_count_jaffa++;		break;
			case 4: sgtc_count_serpent++;	break;
			case 5: sgtc_count_horus++;		break;
			}
		}
		else if( plr->pev->team == 2 )
		{
			switch( plr->m_iSgtcPortraitShown )
			{
			case 1: sgtc_count_oniell++;	break;
			case 2: sgtc_count_carter++;	break;
			case 3: sgtc_count_jackson++;	break;
			case 4: sgtc_count_tealc++;		break;
			case 5: sgtc_count_commander++;	break;
			}
		}
	}
}

static int SGTC_Rules_Dist3d( const Vector &a, const Vector &b )
{
	// ref Endo_distance3d @0x1bbc1c — Manhattan 3d distance as int.
	return (int)( fabsf( a.x - b.x ) + fabsf( a.y - b.y ) + fabsf( a.z - b.z ) );
}

static int SGTC_Rules_TakeFromLarger( int a, int b )
{
	// ref Endo_takefromlarger — returns the larger-magnitude operand.
	return ( abs( a ) >= abs( b ) ) ? a : b;
}

// ref DM Think mid-block (DM-only): superguns armor HUD, GameModeSet latch,
// focused-both latch, +0x774 clear, VoiceGameMgr.Update. Returns TRUE when the
// caller should keep running the shared tail (never FALSE here — reserved).
BOOL CStargateTeamDM::SGTC_ThinkDMCoreLatches( void )
{
	// ref: (float)this+0x76c latch vs mp_superweapons; while below the cvar
	// the rules re-broadcast per-player armor HUD rows to every LIVE client.
	if( (float)m_iSgtcSupergunsLatch <= sgtc_cvar_superweapons.value )
	{
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if( pl && pl->IsAlive() )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pl->pev );
					WRITE_BYTE( (int)( pl->pev->armorvalue * 100.0f /
						(float)pl->m_iSnagArmorCap ) );	// ref ROUND(armor*100/cap)
					WRITE_BYTE( (int)( sgtc_cvar_superweapons.value + 0.5f ) );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
				MESSAGE_END();
			}
		}
	}
	if( sgtc_GameModeSet < 0 )
		sgtc_GameModeSet = (int)( sgtc_gamemodex + 0.5f );
	if( m_iSgtcPrevGameModeSet != sgtc_GameModeSet )
		EndRound();	// ref vfun+0x38
	m_iSgtcSupergunsLatch = (int)( sgtc_cvar_superweapons.value + 0.5f );
	m_iSgtcPrevGameModeSet = sgtc_GameModeSet;

	const int iBothFocused =
		( sgtc_G_players_focused < 1 || sgtc_T_players_focused < 1 ) ? 0 : 1;
	if( m_iSgtcFocusedBoth == 0 && iBothFocused == 1 )
	{
		sgtc_nrcooler = 0;
		EndRound();	// ref vfun+0x38
	}
	if( m_bSgtcVoteEqual != 0 )
	{
		m_bSgtcVoteEqual = 0;
		EndRound();	// ref vfun+0x38 — unanimous mode vote (Think, not NewRound-only)
	}
	m_iSgtcFocusedBoth = iBothFocused;

	// ref: CVoiceGameMgr::Update(gpGlobals->frametime) — DM Think carries the
	// voice-manager tick itself (no CHalfLifeTeamplay::Think in ref).
	g_VoiceGameMgr.Update( gpGlobals->frametime );
	return TRUE;
}

// ref DM Think @0x1934a5 (DM-only): timed key/objective sweep — key-holder
// state -> BOMBCARRIER / LastState text, respawn the objective when no key
// holder and no zone, or when >1 holder (reset all +0xaec, respawn at
// firstplace). Runs while KeyInPlay && KeyTime < now.
void CStargateTeamDM::SGTC_ThinkKeyBlock( void )
{
	if( !( sgtc_KeyInPlay == 1 && sgtc_KeyTime < gpGlobals->time ) )
		return;

	if( sgtc_masterkey == g_vecZero )
	{
		sgtc_KeyTime = gpGlobals->time + 2.0f;
		return;
	}

	// ref DM Think @0x1934a5 — key held (masterkey != 0).
	if( sgtc_GlobalBombState > 0 && sgtc_LastStateI[0] != sgtc_GlobalBombState &&
		sgtc_GlobalBombState == 2 )
		GlobalText( 0, 5 );

	char keyHeld = 0;
	int nKeyHolders = 0;
	char keyCharging = 0;
	CBaseEntity *pScan = NULL;
	while( ( pScan = UTIL_FindEntityByClassname( pScan, "player" ) ) != NULL )
	{
		CBasePlayer *pl = (CBasePlayer *)pScan;
		if( pl->m_iInBombZone <= 0 )
			continue;
		keyHeld = 1;
		nKeyHolders++;
		if( pl->m_iNukeCharge > 5 && pl->m_iNukeCharge < 100 )
			keyCharging = 1;
	}

	if( sgtc_KeyChargeLatch != keyCharging && keyCharging != 1 &&
		( sgtc_LastStateI[0] == sgtc_GlobalBombState || sgtc_GlobalBombState != 2 ) )
		GlobalText( 0, 4 );
	if( sgtc_KeyChargeLatch != keyCharging && keyCharging != 0 )
		GlobalText( 0, 3 );

	sgtc_LastStateI[0] = sgtc_GlobalBombState;
	sgtc_KeyChargeLatch = keyCharging;
	sgtc_GlobalBombState = 0;

	if( (char)sgtc_LastState[0] != keyHeld && keyHeld != 0 )
		GlobalText( 0, 1 );
	if( sgtc_Elmo != 0 )
		GlobalText( 0, 2 );
	sgtc_Elmo = 0;
	sgtc_LastState[0] = keyHeld;

	if( keyHeld != 1 )
	{
		CBaseEntity *pObj = NULL;
		while( ( pObj = UTIL_FindEntityByClassname( pObj, "item_objective" ) ) != NULL )
			keyHeld = 1;
	}
	if( keyHeld != 1 && sgtc_bomby != 1 )
		CBaseEntity::Create( "item_objective", sgtc_LastKeyPlace, g_vecZero, NULL );

	if( nKeyHolders > 1 )
	{
		ALERT( at_console, "GameRule: More than one player has a key!! Total %d\n", nKeyHolders );
		pScan = NULL;
		while( ( pScan = UTIL_FindEntityByClassname( pScan, "player" ) ) != NULL )
		{
			CBasePlayer *pl = (CBasePlayer *)pScan;
			pl->m_iInBombZone = 0;
		}
		CBaseEntity::Create( "item_objective", sgtc_firstplace, g_vecZero, NULL );
	}

	sgtc_KeyTime = gpGlobals->time + 2.0f;
}

// ref DM Think recount block (DM-only): RecountTeams cadence + per-player
// observerless-entity sweep + spawn-camp warn.
void CStargateTeamDM::SGTC_ThinkRecountCamperSweep( void )
{
	if( sgtc_recounttime >= gpGlobals->time )
		return;
	RecountTeams( FALSE );
	sgtc_recounttime = gpGlobals->time + 1.0f;

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !( pl && pl->IsAlive() ) )
			continue;

		// ref: alive, non-observer, no focused team (+0x430 == 0) -> the entity
		// is a stale body: snap it to (4096,4096,4096), clear movetype, remove.
		if( !pl->IsObserver() && pl->m_iSgtcActiveTeam == 0 )
		{
			pl->pev->origin = Vector( 4096.0f, 4096.0f, 4096.0f );
			pl->pev->movetype = MOVETYPE_NONE;
			UTIL_Remove( pl );
		}

		// ref camper warn: alive, non-observer, team >= 1, GameModeSet >= 1 and
		// != 2, near the opposing team's info_player_* spawn.
		if( pl->IsObserver() )
			continue;
		if( sgtc_GameModeSet == 2 || pl->pev->team < 1 || sgtc_GameModeSet < 1 )
			continue;

		const char *pszSpawn = ( pl->pev->team == 1 )
			? "info_player_goauld" : "info_player_torri";
		CBaseEntity *pSpot = NULL;
		while( ( pSpot = UTIL_FindEntityByClassname( pSpot, pszSpawn ) ) != NULL )
		{
			const int iDist = SGTC_Rules_Dist3d( pl->pev->origin, pSpot->pev->origin );
			const int iZ = SGTC_Rules_TakeFromLarger(
				(int)( pSpot->pev->origin.z + 0.5f ),
				(int)( pl->pev->origin.z + 0.5f ) );
			if( !( (float)iDist < sgtc_cvar_camperdist.value ||
				(float)iZ < sgtc_cvar_camperdist.value ) )
				continue;

			if( pl->m_bCampZoneLatch != 1 )
			{
				pl->m_bCampZoneLatch = 1;
				pl->m_flCampWarnDeadline =
					gpGlobals->time + (float)(int)( sgtc_cvar_camptime.value + 0.5f );
				pl->m_flCampNextWarn = 0;
			}
			if( pl->m_bCampZoneLatch != 0 &&
				pl->m_flCampWarnDeadline < gpGlobals->time &&
				pl->m_flCampNextWarn < gpGlobals->time )
			{
				pl->CampWarn( 1 );
				pl->m_flCampRearm = gpGlobals->time + 5.0f;
				pl->m_flCampNextWarn = gpGlobals->time + 3.0f;
			}
		}
	}
}

void CStargateTeamDM::Think( void )
{
	// ref CStargateTeamDM::Think @0x1929ce — the .so body is self-contained:
	// no CHalfLifeTeamplay::Think call (voice/timer work is inlined below).
	if( g_fGameOver )
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	// ref: when m_SendPL is armed (both InitHUDs set it) the rules broadcast
	// TeamNames with the 28 wpnC cost bytes and disarm the flag.
	if( sgtc_SendPL )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
			WRITE_BYTE( m_iSGTCTeamCount );
			for( int i = 0; i < m_iSGTCTeamCount; i++ )
				WRITE_STRING( m_szSGTCTeams[i] );
			for( int i = 0; i < 28; i++ )
				WRITE_BYTE( (int)( sgtc_wpnC[i]->value + 0.5f ) );
		MESSAGE_END();
		sgtc_SendPL = 0;	// ref m_SendPL = 0
	}

	const BOOL bDM = !SGTC_IsCaptureMode();

	if( bDM )
	{
		// ref: round_official set but tournament fairness / min-4 reject —
		// DM calls its own virtual (+0x120); CP runs its variant post-resetround.
		if( sgtc_round_official != 0 )
		{
			int iAvg = ( sgtc_G_players_focused + sgtc_T_players_focused ) / 2;
			BOOL bFair = sgtc_G_players_focused <= iAvg + 2 &&
				sgtc_T_players_focused <= iAvg + 2;
			if( !bFair || sgtc_G_players_focused < 4 || sgtc_T_players_focused < 4 )
				NewRound();	// ref DM vfun+0x120 (tournament reject — no EndRound score pass)
		}

		SGTC_ThinkDMCoreLatches();
	}

	// ref: resetround pulse (score clear at >1, then EndRound + cvar reset).
	// NOTE: the ref FALLS THROUGH after handling; it does not return.
	float flResetRound = CVAR_GET_FLOAT( "mp_resetround" );
	if( flResetRound > 0.0f )
	{
		if( flResetRound > 1.0f )
		{
			int iTorri = GetTeamIndex( "Torri" );
			int iGoauld = GetTeamIndex( "Goa'uld" );
			if( iTorri >= 0 )
				m_iSGTCTeamScores[iTorri] = 0;
			if( iGoauld >= 0 )
				m_iSGTCTeamScores[iGoauld] = 0;
		}
		EndRound();
		CVAR_SET_FLOAT( "mp_resetround", 0.0f );
	}

	if( bDM )
	{
		SGTC_ThinkKeyBlock();				// ref @0x1934a5
		if( sgtc_classchecktime < gpGlobals->time )
		{
			UpdateClassCount();				// ref vfun+0x118
			sgtc_classchecktime = gpGlobals->time + 5.0f;
		}
		SGTC_ThinkRecountCamperSweep();		// ref recounttime cadence + camper warn
	}

	if( m_bRoundOver )
		return;

	// ref Think @0x1929ce: mp_timelimit outer gate — skip fraglimit, PlayMusic,
	// ticket logic, endtround rollover, and timeleft while map limit elapsed
	// and (mode 0 or post-stinger: mode>=1, hasplayed, time>endtround).
	if( SGTC_ThinkRunsMainRoundBody() )
	{
		if( SGTC_ThinkPlayMusic() )	// mode 0 fraglimit may call +0x108
			return;
		SGTC_ThinkEndtroundRollover();
		SGTC_ThinkRefreshTimeleftCvar();	// ref @0x1929ce local_bc / Think()::last_time
	}
	else
	{
		SGTC_ThinkMpTimelimitResetTickets();
		GoToIntermission();
	}
}

static int SGTC_FocusedPresenceBucket( void )
{
	// ref CStargateTeamCP::Think @0x1853e8 local_84 from G/T_players_focused.
	if( sgtc_G_players_focused < 1 || sgtc_T_players_focused < 1 )
	{
		if( sgtc_G_players_focused < 1 && sgtc_T_players_focused < 1 )
			return 0;
		return 1;
	}
	return 2;
}

void CStargateTeamCP::SGTC_Type4TicketSideReset( void )
{
	if( sgtc_type != 4 )
		return;
	if( m_iSgtcCPSide == 1 )
		InverseSpawns();
	m_iSgtcCPSide = 0;
	sgtc_G_tickets = 0;
	sgtc_T_tickets = 0;
}

void CStargateTeamCP::Think( void )
{
	// ref CStargateTeamCP::Think @0x1853e8 — the .so body IS the shared DM
	// block sequence plus CP-only gates (friendlyfire clamp, +0x758 type
	// latch, +0x75c bucket, +0x768 superguns latch, resetround type-4 tail,
	// round_official reject) ending in the DM tail (recount/mode/timeleft).
	if( g_fGameOver )
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	// ref: friendlyfire cvar clamped to 0 when NaN/<=0.
	if( !( sgtc_cvar_friendlyfire.value > 0.0f ) )
		CVAR_SET_FLOAT( "mp_friendlyfire", 0.0f );

	// ref: this+0x758 type latch — EndRound when the map 'type' flips.
	if( m_iSgtcCPTypeLatch != sgtc_type )
		EndRound();

	// ref: m_SendPL TeamNames+wpnC broadcast is handled inside
	// CStargateTeamDM::Think (shared tail) — ref order here matches.
	if( sgtc_SendPL )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
			WRITE_BYTE( m_iSGTCTeamCount );
			for( int i = 0; i < m_iSGTCTeamCount; i++ )
				WRITE_STRING( m_szSGTCTeams[i] );
			for( int i = 0; i < 28; i++ )
				WRITE_BYTE( (int)( sgtc_wpnC[i]->value + 0.5f ) );
		MESSAGE_END();
		sgtc_SendPL = 0;
	}

	// ref: this+0x768 superguns latch — armor HUD refresh while rising.
	if( (float)m_iSgtcCPSupergunsLatch <= sgtc_cvar_superweapons.value )
	{
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if( pl && pl->IsAlive() )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pl->pev );
					WRITE_BYTE( (int)( pl->pev->armorvalue * 100.0f /
						(float)pl->m_iSnagArmorCap ) );
					WRITE_BYTE( (int)( sgtc_cvar_superweapons.value + 0.5f ) );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
				MESSAGE_END();
			}
		}
	}

	// ref: this+0x75c focused-presence transition (empty-team paths).
	int iBucket = SGTC_FocusedPresenceBucket();
	if( m_iSgtcFocusedBucket != iBucket )
	{
		SGTC_Type4TicketSideReset();
		sgtc_nrcooler = 0.0f;
		EndRound();
	}
	m_iSgtcCPSupergunsLatch = (int)( sgtc_cvar_superweapons.value + 0.5f );
	m_iSgtcFocusedBucket = iBucket;
	m_iSgtcCPTypeLatch = sgtc_type;

	// ref: resetround pulse (score clear at >1; type-4 side+ticket reset;
	// EndRound + cvar reset) — ref FALLS THROUGH after handling.
	float flResetRound = CVAR_GET_FLOAT( "mp_resetround" );
	if( flResetRound > 0.0f )
	{
		if( flResetRound > 1.0f )
		{
			int iTorri = GetTeamIndex( "Torri" );
			int iGoauld = GetTeamIndex( "Goa'uld" );
			if( iTorri >= 0 )
				m_iSGTCTeamScores[iTorri] = 0;
			if( iGoauld >= 0 )
				m_iSGTCTeamScores[iGoauld] = 0;
		}
		SGTC_Type4TicketSideReset();
		EndRound();
		CVAR_SET_FLOAT( "mp_resetround", 0.0f );
	}

	// ref: round_official set but tournament fairness / min-4 reject.
	if( sgtc_round_official != 0 )
	{
		int iAvg = ( sgtc_G_players_focused + sgtc_T_players_focused ) / 2;
		BOOL bFair = sgtc_G_players_focused <= iAvg + 2 &&
			sgtc_T_players_focused <= iAvg + 2;
		if( !bFair || sgtc_G_players_focused < 4 || sgtc_T_players_focused < 4 )
		{
			SGTC_Type4TicketSideReset();
			EndRound();
		}
	}

	if( m_bRoundOver )
		return;

	// ref tail: shared DM round body (recount cadence is DM-only; CP skips it
	// via SGTC_IsCaptureMode), mp_timelimit gate + mode/timeleft logic.
	if( SGTC_ThinkRunsMainRoundBody() )
	{
		if( SGTC_ThinkPlayMusic() )
			return;
		SGTC_ThinkEndtroundRollover();
		SGTC_ThinkRefreshTimeleftCvar();
	}
	else
	{
		SGTC_ThinkMpTimelimitResetTickets();
		GoToIntermission();
	}
}

// ===========================================================================
// CStargateTeamCP
// ===========================================================================
CStargateTeamCP::CStargateTeamCP( void ) : m_bSpawnsInversed( FALSE )
{
	g_iSgtcMapType = 1;	// ref CP ctor sets MapType=1 (@0x179eb7)
	m_iSgtcCPTypeLatch = 0;		// ref CP +0x758 starts 0; first type!=0 read EndRounds
	m_iSgtcCPSupergunsLatch = 0;	// ref CP +0x768
}

void CStargateTeamCP::InverseSpawns( void )
{
	// .so InverseSpawns: swaps the torri/goauld spawn tables mid-round.
	m_bSpawnsInversed = !m_bSpawnsInversed;
}

edict_t *CStargateTeamCP::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	int team = GetTeamIndex( pPlayer->m_szTeamName );
	if( m_bSpawnsInversed )
		team = 1 - team;
	const char *SpotClass = ( team == 1 ) ? s_SGTCDMSpawns[1] : s_SGTCDMSpawns[0];
	edict_t *spot = SGTC_FindTeamSpawn( SpotClass );
	if( !spot || FNullEnt( spot ) )
		return CStargateTeamDM::GetPlayerSpawnSpot( pPlayer );
	pPlayer->pev->origin = VARS( spot )->origin + Vector( 0, 0, 1 );
	pPlayer->pev->angles = VARS( spot )->angles;
	pPlayer->pev->v_angle = VARS( spot )->angles;
	pPlayer->pev->fixangle = TRUE;
	return spot;
}

void CStargateTeamCP::PlayerSpawn( CBasePlayer *pPlayer )
{
	// ref CStargateTeamCP::PlayerSpawn @0x1814a6
	// (rea_dump/CStargateTeamCP__PlayerSpawn.c). This is a DM-shaped spawn
	// with CP deltas: no GameModeSet!=3 invis-latch clear, +0x463 = 0 kept,
	// snakegoauld 270.0f, type/teamed 50hp tails, freeguns override, solid
	// gate on the melee give, type-4 +0x4ac clear, TicketMode byte from type,
	// ScoreInfo without the invis==6 override, tail order
	// SendAboutMe -> AddPortrait -> RecountTeams, no +0xa9c latch clear,
	// type-4 attack deadline.
	pPlayer->m_iSgtcSpecMode = (int)g_engfuncs.pfnGetPlayerWONId( pPlayer->edict() );
	pPlayer->m_bSgtcPad3e3 = 0;					// ref +0x3e3 = 0
	pPlayer->m_iSgtcFlagsCa8 &= ~0xB;				// ref +0xca8 &= 0xfffffff4
	pPlayer->StopObserver();
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );
	pPlayer->m_bNukeViewSnap = 0;					// ref +0x463 = 0

	if( !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) )	// ref DAT_001eb14c, 7-char
	{
		pPlayer->pev->team = 1;
		pPlayer->m_bSgtcCloakTick = 0;
		if( pPlayer->m_bSgtcSuperman != 1 )		// ref +0x924 != 1
		{
			int cls = pPlayer->m_iSgtcPortraitShown;
			if( cls >= 1 && cls <= 5 )
			{
				const SGTCSClassDef &d = s_SgtcCPGoauldClass[cls];
				g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(),
					g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", d.model );
				pPlayer->pev->maxspeed = d.maxspeed;
				pPlayer->m_iSnagArmorCap = d.snagArmorCap;
				pPlayer->pev->armorvalue = d.armorvalue;
				pPlayer->m_bSgtcCloakTick = (char)d.cloak;
			}
		}
		if( ( sgtc_type == 2 && sgtc_teamed == 2 ) ||
			( sgtc_type == 4 && m_iSgtcCPSide == 0 ) )
		{
			// ref: this side plays the weakened "50hp" shell.
			pPlayer->pev->armorvalue = 0;
			pPlayer->pev->health = 50.0f;		// pev+0x160 = 0x42480000
			pPlayer->pev->max_health = 50.0f;	// pev+0x1b0
		}
		else
		{
			pPlayer->m_bSgtcCloakTick = 0;		// ref +0x448 = 0
			pPlayer->m_iSnagArmorCap = 100;		// ref +0x908 = 100
			pPlayer->pev->armorvalue = 100.0f;	// 0x42c80000
			pPlayer->m_iNukeBonus = 200;		// ref +0x934 = 200
		}
	}
	else if( !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) )
	{
		pPlayer->pev->team = 2;
		pPlayer->m_bSgtcCloakTick = 0;
		if( pPlayer->m_bSgtcSuperman != 1 )
		{
			int cls = pPlayer->m_iSgtcPortraitShown;
			if( cls >= 1 && cls <= 5 )
			{
				const SGTCSClassDef &d = s_SgtcTorriClass[cls];
				g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(),
					g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", d.model );
				pPlayer->pev->maxspeed = d.maxspeed;
				pPlayer->m_iSnagArmorCap = d.snagArmorCap;
				pPlayer->pev->armorvalue = d.armorvalue;
				pPlayer->m_bSgtcCloakTick = (char)d.cloak;
			}
		}
		if( ( sgtc_type == 2 && sgtc_teamed == 1 ) ||
			( sgtc_type == 4 && m_iSgtcCPSide != 0 ) )
		{
			pPlayer->pev->armorvalue = 0;
			pPlayer->pev->health = 50.0f;
			pPlayer->pev->max_health = 50.0f;
		}
		else
		{
			pPlayer->m_bSgtcCloakTick = 0;
			pPlayer->m_iSnagArmorCap = 100;
			pPlayer->pev->armorvalue = 100.0f;
			pPlayer->m_iNukeBonus = 200;
		}
	}

	for( int i = 0; i < 28; i++ )				// ref +0x93c[0x1c] = 1
		pPlayer->m_iProgressTeam[i] = 1;
	SetWeaponAllow( pPlayer );

	if( pPlayer->m_iSgtcPrevClass != pPlayer->m_iSgtcPortraitShown ||
		pPlayer->m_iSgtcPrevPeTeam != (int)pPlayer->pev->team )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgProgress, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( pPlayer->m_iNukeBonus );
			for( int i = 0; i < 28; i++ )
				WRITE_BYTE( pPlayer->m_iProgressTeam[i] );
			WRITE_BYTE( 1 );
		MESSAGE_END();
		pPlayer->m_bSgtcMenuLock = 0;			// ref +0x4e5
		for( int i = 0; i < 28; i++ )
			pPlayer->m_iSgtcGondArgs[i] = 0;	// ref +0x9ac[i] = 0
	}

	BOOL bHadLoadout = FALSE;					// ref bVar8
	pPlayer->m_iSgtcGondState = 0;				// ref +0x4f4
	// ref CP grant gate: vcall+0x94 (IsAlive) && !IsObserver — NO +0x4de check.
	// Grants fire on ANY nonzero +0x9ac[i] when freeguns > 1.
	if( pPlayer->IsAlive() && !pPlayer->IsObserver() )
	{
		for( int i = 0; i < 28; i++ )
		{
			if( pPlayer->m_iSgtcGondArgs[i] != 0 ||
				sgtc_cvar_freeguns.value > 1.0f )
			{
				bHadLoadout = TRUE;
				if( i == 0x11 || i == 0x12 )
				{
					if( pPlayer->m_iSgtcGondState == 0 && i == 0x11 )
						pPlayer->m_iSgtcGondState = 1;
					if( pPlayer->m_iSgtcGondState == 1 && i == 0x12 )
						pPlayer->m_iSgtcGondState = 3;
					if( pPlayer->m_iSgtcGondState == 0 && i == 0x12 )
						pPlayer->m_iSgtcGondState = 2;
					if( pPlayer->m_iSgtcGondState == 2 && i == 0x11 )
						pPlayer->m_iSgtcGondState = 3;
				}
				else
					pPlayer->GiveNamedItem( s_SgtcSpawnWeapons[i] );	// ref WPLISTB[i]
			}
		}
		// ref: armed shield state + !(pev->weapons bit 9) grants WPLISTB[26].
		if( pPlayer->m_iSgtcGondState != 0 &&
			( ( pPlayer->pev->weapons >> 9 ) & 1 ) != 1 )
			pPlayer->GiveNamedItem( s_SgtcSpawnWeapons[0x1a] );
		// ref: melee only when solid && !observer.
		if( !pPlayer->IsObserver() && pPlayer->pev->solid != 0 )
		{
			if( pPlayer->pev->team == 1 )
				pPlayer->GiveNamedItem( "weapon_coujaf" );
			else
				pPlayer->GiveNamedItem( "weapon_couteau" );
		}
	}

	if( sgtc_type == 4 )
	{
		pPlayer->m_iSgtcPad4ac = 0;				// ref +0x4ac = 0
		m_iSgtcCPSide = 0;					// ref +0x734 = 0
	}
	pPlayer->m_iSgtcPrevClass = pPlayer->m_iSgtcPortraitShown;
	pPlayer->m_iSgtcPrevPeTeam = (int)pPlayer->pev->team;

	int clientIndex = pPlayer->entindex();
	if( pPlayer->pev->team == 1 )
		pPlayer->m_iSgtcSubmodelIdx = 0;
	else if( pPlayer->pev->team == 2 )
		pPlayer->m_iSgtcSubmodelIdx = 1;
	else
		pPlayer->m_iSgtcSubmodelIdx = 2;
	pPlayer->ChangeSubmodel();

	if( pPlayer->IsAlive() )
		pPlayer->StopObserver();			// ref vcall+0x94 != 0 -> StopObserver
	if( !pPlayer->IsObserver() )
	{
		pPlayer->pev->renderamt = 255.0f;
		pPlayer->pev->renderfx = 0;
		pPlayer->pev->rendermode = 0;
		pPlayer->m_bSgtcInWorld = 1;		// ref +0x44b
	}
	if( pPlayer->IsObserver() )
		pPlayer->pev->team = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgTicketMode, NULL, pPlayer->pev );
		WRITE_BYTE( ( sgtc_type < 2 || sgtc_type == 4 ) ? 1 : 0 );	// ref (type<2 || type==4)
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( clientIndex );
		WRITE_SHORT( (int)pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iSgtcPortraitShown );	// ref: no invis==6 override in CP
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pPlayer->pev );
		WRITE_BYTE( (int)( pPlayer->pev->armorvalue * 100.0f /
			(float)pPlayer->m_iSnagArmorCap ) );
		WRITE_BYTE( (int)sgtc_cvar_superweapons.value );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	pPlayer->SendAboutMe( (int)pPlayer->pev->team,
		STRING( pPlayer->pev->netname ), clientIndex );
	AddPortrait( pPlayer );					// ref vtable+0x110 — CP override
	RecountTeams( FALSE );

	if( !bHadLoadout )
	{
		if( sgtc_cvar_wepselmode.value == 1.0f )
			pPlayer->ShowVGUIMenu( 0xc );
		else if( sgtc_cvar_wepselmode.value == 2.0f )
			pPlayer->ShowVGUIMenu( 0x3c );
	}
	// NOTE: ref CP has no +0xa9c (m_bSgtcLatchA9c) clear here.
	if( pPlayer->IsAlive() && !pPlayer->IsObserver() )
		StartPortrait( pPlayer );			// ref vtable+0x114 — CP override

	if( sgtc_type == 4 )
	{
		// ref: the non-side team arms its attack deadline.
		if( m_iSgtcCPSide == 0 )
		{
			if( pPlayer->pev->team == 1 )
				pPlayer->m_flSgtcAttackEnd = sgtc_cvar_attackcountdown.value + sgtc_roundstart;
			else
				pPlayer->m_flSgtcAttackEnd = 0.0f;
		}
		else
		{
			if( pPlayer->pev->team == 2 )
				pPlayer->m_flSgtcAttackEnd = sgtc_cvar_attackcountdown.value + sgtc_roundstart;
			else
				pPlayer->m_flSgtcAttackEnd = 0.0f;
		}
	}
}

void CStargateTeamCP::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	// ref CStargateTeamCP::ClientUserInfoChanged @0x182c02 — same body as the
	// DM variant MINUS the +0x4de "unasteammate" tail and the +0xa3d latch.
	(void)infobuffer;
	if( pPlayer->IsObserver() )
		pPlayer->pev->team = 0;						// ref pev+0x1ac = 0

	if( pPlayer->m_iSgtcInfoLock != 1 &&			// ref +0xdc != 1
		pPlayer->m_bSnagInvisLatch != 1 )			// ref +0x4de != 1
	{
		char *infobuf = g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() );
		if( !strncmp( pPlayer->m_szTeamName, "Goa'uld", 7 ) )
		{
			pPlayer->pev->team = 1;
			switch( pPlayer->m_iSgtcPortraitShown )				// ref +0xa40
			{
			case 1:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "systemlord" );	break;
			case 2:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "anubis" );		break;
			case 3:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "goauld3" );		break;
			case 4:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "snakegoauld" );	break;
			case 5:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "horus" );		break;
			}
		}
		else if( !strncmp( pPlayer->m_szTeamName, "Torri", 5 ) )
		{
			pPlayer->pev->team = 2;
			switch( pPlayer->m_iSgtcPortraitShown )
			{
			case 1:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "oneill" );		break;
			case 2:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "carter" );		break;
			case 3:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "jackson" );	break;
			case 4:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "tealc" );		break;
			case 5:	g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), infobuf, "model", "commander" );	break;
			}
		}
	}

	pPlayer->SendAboutMe( (int)pPlayer->pev->team,
		STRING( pPlayer->pev->netname ), pPlayer->entindex() );
	RecountTeams( FALSE );

	UpdateGameMode( pPlayer );						// ref vtable+0x34 tail — virtual, dispatches CP
}

// ===========================================================================
// CStargateUnas / CStargateInvis
// ===========================================================================
CStargateUnas::CStargateUnas( void )
{
	// .so Unas ctor teamlist "Unas;Marine" (fileoff 0x1dc5f9).
	strncpy( m_szSGTCTeams[0], "Unas", SGTC_TEAMNAME_LEN );
	strncpy( m_szSGTCTeams[1], "Marine", SGTC_TEAMNAME_LEN );
	m_iSGTCTeamCount = SGTC_MAX_SGTC_TEAMS;
	RecountTeams( FALSE );
}

int CStargateUnas::GetTeamIndex( const char *pTeamName )
{
	if( pTeamName && *pTeamName )
	{
		for( int tm = 0; tm < m_iSGTCTeamCount; tm++ )
		{
			if( !stricmp( m_szSGTCTeams[tm], pTeamName ) )
				return tm;
		}
	}
	return -1;
}

const char *CStargateUnas::GetIndexedTeamName( int teamIndex )
{
	if( teamIndex < 0 || teamIndex >= m_iSGTCTeamCount )
		return "";
	return m_szSGTCTeams[teamIndex];
}

BOOL CStargateUnas::IsValidTeam( const char *pTeamName )
{
	return ( GetTeamIndex( pTeamName ) != -1 ) ? TRUE : FALSE;
}

const char *CStargateUnas::GetTeamID( CBaseEntity *pEntity )
{
	if( !pEntity || !pEntity->pev )
		return "";
	return pEntity->TeamID();
}

// PlayerRelationship is inherited from CHalfLifeTeamplay: identical same-team
// stricmp logic; no decl in the SGTC header, so the stock virtual is used.

BOOL CStargateUnas::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if( pAttacker && PlayerRelationship( pPlayer, pAttacker ) == GR_TEAMMATE )
	{
		if( friendlyfire.value == 0 && pAttacker != pPlayer )
			return FALSE;
	}
	return CHalfLifeMultiplay::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

BOOL CStargateUnas::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if( pTgt && pTgt->IsPlayer() && PlayerRelationship( pPlayer, pTgt ) == GR_TEAMMATE )
		return FALSE;
	return CHalfLifeMultiplay::ShouldAutoAim( pPlayer, target );
}

int CStargateUnas::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if( !pKilled )
		return 0;
	if( !pAttacker )
		return 1;
	if( pAttacker != pKilled && PlayerRelationship( pAttacker, pKilled ) == GR_TEAMMATE )
		return -1;
	return 1;
}

const char *CStargateUnas::TeamWithFewestPlayers( void )
{
	int teamCount[SGTC_MAX_SGTC_TEAMS] = { 0, 0 };
	int minPlayers = 9999;
	const char *pName = m_szSGTCTeams[0];
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if( plr )
		{
			int team = GetTeamIndex( plr->TeamID() );
			if( team >= 0 )
				teamCount[team]++;
		}
	}
	for( int i = 0; i < m_iSGTCTeamCount; i++ )
	{
		if( teamCount[i] < minPlayers )
		{
			minPlayers = teamCount[i];
			pName = m_szSGTCTeams[i];
		}
	}
	return pName;
}

void CStargateUnas::RecountTeams( BOOL bResendInfo )
{
	if( !bResendInfo )
		return;
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
		WRITE_BYTE( m_iSGTCTeamCount );
		for( int i = 0; i < m_iSGTCTeamCount; i++ )
			WRITE_STRING( m_szSGTCTeams[i] );
	MESSAGE_END();
}

const char *CStargateUnas::SetDefaultPlayerTeam( CBasePlayer *pPlayer )
{
	char *mdls = g_engfuncs.pfnInfoKeyValue(
		g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" );
	snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", mdls );
	if( pPlayer->m_szTeamName[0] == '\0' || !IsValidTeam( pPlayer->m_szTeamName ) )
		snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", TeamWithFewestPlayers() );
	return pPlayer->m_szTeamName;
}

void CStargateUnas::ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib )
{
	// ref @0x198e7a: bKill → world TakeDamage( 900, bGib ? ALWAYSGIB : NEVERGIB );
	// bGib also forces pev->frags = -100. Then m_szTeamName copy, gmsgTeamInfo
	// broadcast, and gmsgScoreInfo {entindex, frags, m_iDeaths, +0xa40, GetTeamIndex+1, 0}.
	int damageFlags = DMG_GENERIC | ( bGib ? DMG_ALWAYSGIB : DMG_NEVERGIB );
	if( bKill )
	{
		entvars_t *pevWorld = VARS( INDEXENT( 0 ) );
		if( bGib )
			pPlayer->pev->frags = -100.0f;
		pPlayer->TakeDamage( pevWorld, pevWorld, 900, damageFlags );
	}
	if( pPlayer->m_szTeamName != pTeamName )
		snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", pTeamName );

	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( pPlayer->entindex() );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( pPlayer->entindex() );
		WRITE_SHORT( (int)pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );		// ref param+0xdf0
		WRITE_SHORT( pPlayer->m_iSgtcPortraitShown );	// ref param+0xa40
		WRITE_SHORT( GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		WRITE_SHORT( 0 );						// ref param+0x170
	MESSAGE_END();
}

void CStargateUnas::SGTC_SendTeamNames( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict() );
		WRITE_BYTE( m_iSGTCTeamCount );
		for( int i = 0; i < m_iSGTCTeamCount; i++ )
			WRITE_STRING( m_szSGTCTeams[i] );
		for( int z = 0; z < 28; z++ )
			WRITE_BYTE( 0 );	// ref: TeamNames carries a 28-byte pad tail
	MESSAGE_END();
}


void CStargateUnas::UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 1 );
	MESSAGE_END();
}

edict_t *CStargateUnas::SGTC_FindTeamSpawn( const char *pszClassname )
{
	CBaseEntity *pSpot = NULL;
	for( int i = RANDOM_LONG( 1, 5 ); i > 0; i-- )
		pSpot = UTIL_FindEntityByClassname( pSpot, (char *)pszClassname );
	if( !pSpot )
		pSpot = UTIL_FindEntityByClassname( pSpot, (char *)pszClassname );
	return pSpot ? pSpot->edict() : NULL;
}

void CStargateUnas::PlayerSpawn( CBasePlayer *pPlayer )
{
	// ref @0x19924c spot-check (rea_dump/CStargateUnas__PlayerSpawn.c):
	// "Unas" branch — health/max_health 250.0f, lance+invis, +0x3b0 = 0xc3
	// (BLOOD_COLOR_YELLOW), "Vous etes le Unas, bonne boucherie :-)\n"; NO
	// maxspeed write. "Marine" branch (stricmp, not blanket else) —
	// 9mmhandgun/9mmAR/main/lance/zat/flashbang, GiveAmmo(100,"9mm",48),
	// GiveAmmo(2,"ARgrenades",10), health/max_health 100.0f, +0x3b0 = 0xf7
	// (BLOOD_COLOR_RED), "Vous etes un Marine, bonne chance...\n".
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );
	if( !stricmp( pPlayer->m_szTeamName, "Unas" ) )
	{
		pPlayer->pev->health = 250.0f;		// ref pev+0x160 = 0x437a0000
		pPlayer->pev->max_health = 250.0f;	// ref pev+0x1b0
		pPlayer->GiveNamedItem( "weapon_lance" );
		pPlayer->GiveNamedItem( "weapon_invis" );
		pPlayer->m_iBloodColor = BLOOD_COLOR_YELLOW;	// ref +0x3b0 = 0xc3
		UTIL_SayText( "Vous etes le Unas, bonne boucherie :-)\n", pPlayer );
	}
	else if( !stricmp( pPlayer->m_szTeamName, "Marine" ) )	// ref "Marine", 6-char
	{
		pPlayer->GiveNamedItem( "weapon_9mmhandgun" );
		pPlayer->GiveNamedItem( "weapon_9mmAR" );
		pPlayer->GiveNamedItem( "weapon_main" );
		pPlayer->GiveNamedItem( "weapon_lance" );
		pPlayer->GiveNamedItem( "weapon_zat" );
		pPlayer->GiveNamedItem( "weapon_flashbang" );
		pPlayer->GiveAmmo( 100, "9mm", 48 );		// ref vcall+0x6c (100, "9mm", 0x30)
		pPlayer->GiveAmmo( 2, "ARgrenades", 10 );	// ref vcall+0x6c (2, "ARgrenades", 10)
		pPlayer->pev->health = 100.0f;		// ref pev+0x160 = 0x42c80000
		pPlayer->pev->max_health = 100.0f;	// ref pev+0x1b0
		pPlayer->m_iBloodColor = BLOOD_COLOR_RED;	// ref +0x3b0 = 0xf7
		UTIL_SayText( "Vous etes un Marine, bonne chance...\n", pPlayer );
	}
}

void CStargateUnas::Think( void )
{
	CHalfLifeTeamplay::Think();
}

void CStargateUnas::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
	RecountTeams( TRUE );
}

void CStargateUnas::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::DeathNotice( pVictim, pKiller, pInflictor );
}

BOOL CStargateUnas::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	// ref @0x19a33e: voice hop, menuselect (arg1 consumed, no-op),
	// jointeam {1->Unas, else->Marine}, showmenu -> ShowVGUIMenu(10), unknown -> 0.
	if( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if( FStrEq( pcmd, "menuselect" ) )
	{
		if( CMD_ARGC() > 1 )
			(void)atoi( CMD_ARGV( 1 ) );	// ref consumes arg 1 (no effect)
		return TRUE;
	}
	if( FStrEq( pcmd, "jointeam" ) )
	{
		if( CMD_ARGC() > 0 )
		{
			if( atoi( CMD_ARGV( 1 ) ) == 1 )
				strncpy( pPlayer->m_szTeamName, "Unas", 16 );
			else
				strncpy( pPlayer->m_szTeamName, "Marine", 16 );
			ChangePlayerTeam( pPlayer, pPlayer->m_szTeamName, 1, 0 );
			RecountTeams( FALSE );
		}
		return TRUE;
	}
	if( FStrEq( pcmd, "showmenu" ) )
	{
		pPlayer->ShowVGUIMenu( 10 );	// ref @0x19a33e ShowVGUIMenu(10)
		return TRUE;
	}
	return FALSE;	// ref: unknown verbs fall out as 0 (no base-class chain)
}

// ref Unas ClientCommand @0x19a33e / Invis @0x16d4c4 — identical shape:
// voice hop -> menuselect (argc>=2 eats arg) -> jointeam (arg1: 1->team0,
// else team1; ChangePlayerTeam vfun+0xec + RecountTeams) -> showmenu
// (Unas: ShowVGUIMenu(10), Invis: ShowVGUIMenu(0xb)) -> return 0.

BOOL CStargateInvis::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	// ref @0x16d4c4: voice hop, menuselect, jointeam {1->Torri, else->Invisible},
	// showmenu -> ShowVGUIMenu(0xb), unknown -> 0.
	if( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if( FStrEq( pcmd, "menuselect" ) )
	{
		if( CMD_ARGC() > 1 )
			(void)atoi( CMD_ARGV( 1 ) );
		return TRUE;
	}
	if( FStrEq( pcmd, "jointeam" ) )
	{
		if( CMD_ARGC() > 0 )
		{
			if( atoi( CMD_ARGV( 1 ) ) == 1 )
				strncpy( pPlayer->m_szTeamName, "Torri", 16 );
			else
				strncpy( pPlayer->m_szTeamName, "Invisible", 16 );
			ChangePlayerTeam( pPlayer, pPlayer->m_szTeamName, 1, 0 );
			RecountTeams( FALSE );
		}
		return TRUE;
	}
	if( FStrEq( pcmd, "showmenu" ) )
	{
		pPlayer->ShowVGUIMenu( 11 );	// ref @0x16d4c4 ShowVGUIMenu(0xb)
		return TRUE;
	}
	return FALSE;	// ref: unknown verbs fall out as 0 (no base-class chain)
}

void CStargateUnas::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	char *mdls = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );
	if( !stricmp( mdls, pPlayer->m_szTeamName ) )
		return;
	if( !IsValidTeam( mdls ) )
	{
		int clientIndex = pPlayer->entindex();
		g_engfuncs.pfnSetClientKeyValue( clientIndex,
			g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", pPlayer->m_szTeamName );
		return;
	}
	ChangePlayerTeam( pPlayer, mdls, TRUE, TRUE );
	RecountTeams( TRUE );
}

CStargateInvis::CStargateInvis( void )
{
	// .so Invis ctor teamlist "Torri;Invisible" (fileoff 0x1dad91).
	strncpy( m_szSGTCTeams[0], "Torri", SGTC_TEAMNAME_LEN );
	strncpy( m_szSGTCTeams[1], "Invisible", SGTC_TEAMNAME_LEN );
}

void CStargateInvis::PlayerSpawn( CBasePlayer *pPlayer )
{
	// ref @0x17c44e spot-check (rea_dump/CStargateInvis__PlayerSpawn.c):
	// "Invisible" branch (checked FIRST) — maxspeed 1000.0f (0x447a0000);
	// ToggleVis only when +0x4e9 != 1; "Vous etes invisible... :-)\n".
	// "Torri" branch — ToggleVis when +0x4e9 != 0; maxspeed 400.0f
	// (0x43c80000); 9mmhandgun/m92s/handgrenade/flashbang/lance/zat;
	// GiveAmmo(68,"9mm",48) + GiveAmmo(2,"Hand Grenade",3);
	// "Trouvez l invisible...\n". NO trailing +0x4de=0/extra ToggleVis.
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );
	if( !stricmp( pPlayer->m_szTeamName, "Invisible" ) )
	{
		pPlayer->pev->maxspeed = 1000.0f;	// ref pev+0x210 = 0x447a0000
		if( pPlayer->m_bShieldSprint != 1 )	// ref +0x4e9 != 1
			pPlayer->SGTC_ToggleVis();		// ref CBasePlayer::ToggleVis @0x136454
		UTIL_SayText( "Vous etes invisible... :-)\n", pPlayer );
	}
	else if( !stricmp( pPlayer->m_szTeamName, "Torri" ) )
	{
		if( pPlayer->m_bShieldSprint != 0 )		// ref +0x4e9 != 0
			pPlayer->SGTC_ToggleVis();
		pPlayer->pev->maxspeed = 400.0f;	// ref pev+0x210 = 0x43c80000
		pPlayer->GiveNamedItem( "weapon_9mmhandgun" );
		pPlayer->GiveNamedItem( "weapon_m92s" );
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		pPlayer->GiveNamedItem( "weapon_flashbang" );
		pPlayer->GiveNamedItem( "weapon_lance" );
		pPlayer->GiveNamedItem( "weapon_zat" );
		pPlayer->GiveAmmo( 68, "9mm", 48 );		// ref vcall+0x6c (0x44, "9mm", 0x30)
		pPlayer->GiveAmmo( 2, "Hand Grenade", 3 );	// ref vcall+0x6c (2, "Hand Grenade", 3)
		UTIL_SayText( "Trouvez l invisible...\n", pPlayer );
	}
}

CStargateReplicateur::CStargateReplicateur( void )
{
	// ref CStargateReplicateur::CStargateReplicateur @0x17e364
	m_bDisableDeathMessages = FALSE;
	m_bDisableDeathPenalty = FALSE;
	m_iRepTeamCount = 0;
	memset( m_szRepTeams, 0, sizeof( m_szRepTeams ) );
	memset( m_iRepScores, 0, sizeof( m_iRepScores ) );
	strncpy( m_szRepTeamList, "Replic;Torri", sizeof( m_szRepTeamList ) - 1 );
	// ref: worldspawn 'team' key overrides teamlist (INDEXENT(0)+0x22c via strings)
	edict_t *pWorld = INDEXENT( 0 );
	if( pWorld && pWorld->v.team )
	{
		char *pList = (char *)STRING( pWorld->v.team );
		if( pList && pList[0] )
			strncpy( m_szRepTeamList, pList, sizeof( m_szRepTeamList ) - 1 );
	}
	m_teamLimit = ( m_szRepTeamList[0] != 0 );
	RecountTeams( FALSE );
}

// ref @0x17de78 — parse m_szRepTeamList into m_szRepTeams, then rescan players.
void CStargateReplicateur::RecountTeams( BOOL bResendInfo )
{
	m_iRepTeamCount = 0;
	char teamlist[512];
	strncpy( teamlist, m_szRepTeamList, sizeof( teamlist ) - 1 );
	teamlist[sizeof( teamlist ) - 1] = 0;
	for( char *pName = strtok( teamlist, ";" ); pName && *pName;
		 pName = strtok( NULL, ";" ) )
	{
		if( GetTeamIndex( pName ) < 0 && m_iRepTeamCount < 8 )
		{
			strncpy( m_szRepTeams[m_iRepTeamCount], pName, 16 );
			m_iRepTeamCount++;
		}
	}
	if( m_iRepTeamCount < 2 )
	{
		m_iRepTeamCount = 0;
		m_teamLimit = FALSE;
	}
	memset( m_iRepScores, 0, sizeof( m_iRepScores ) );

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if( !plr )
			continue;
		const char *pTeam = plr->TeamID();
		int idx = GetTeamIndex( pTeam );
		if( idx < 0 && !m_teamLimit && m_iRepTeamCount < 8 )
		{
			idx = m_iRepTeamCount;
			m_iRepScores[idx] = 0;
			strncpy( m_szRepTeams[m_iRepTeamCount], pTeam, 16 );
			m_iRepTeamCount++;
		}
		if( idx >= 0 )
			m_iRepScores[idx] += (int)plr->pev->frags;
		if( bResendInfo && IsValidTeam( pTeam ) )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
				WRITE_BYTE( plr->entindex() );
				WRITE_STRING( pTeam );
			MESSAGE_END();
		}
	}
}

int CStargateReplicateur::GetTeamIndex( const char *pTeamName )
{
	if( pTeamName && *pTeamName )
		for( int t = 0; t < m_iRepTeamCount; t++ )
			if( !stricmp( m_szRepTeams[t], pTeamName ) )
				return t;
	return -1;
}

BOOL CStargateReplicateur::IsValidTeam( const char *pTeamName )
{
	if( !m_teamLimit )
		return TRUE;
	return GetTeamIndex( pTeamName ) != -1;
}

// ref @0x17d998.
void CStargateReplicateur::ChangePlayerTeam( CBasePlayer *pPlayer,
	const char *pTeamName, BOOL bKill, BOOL bGib )
{
	int clientIndex = pPlayer->entindex();
	int damageFlags = DMG_GENERIC | ( bGib ? DMG_ALWAYSGIB : DMG_NEVERGIB );
	if( bKill )
	{
		m_bDisableDeathMessages = TRUE;	// ref this+0xc
		m_bDisableDeathPenalty = TRUE;	// ref this+0x10
		entvars_t *pevWorld = VARS( INDEXENT( 0 ) );
		pPlayer->TakeDamage( pevWorld, pevWorld, 900, damageFlags ); // 0x44610000
		m_bDisableDeathMessages = FALSE;
		m_bDisableDeathPenalty = FALSE;
	}
	snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", pTeamName );
	g_engfuncs.pfnSetClientKeyValue( clientIndex,
		g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", pPlayer->m_szTeamName );
	g_engfuncs.pfnSetClientKeyValue( clientIndex,
		g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", pPlayer->m_szTeamName );

	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( clientIndex );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( clientIndex );
		WRITE_SHORT( (int)pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );		// ref param_1+0xdf0
		WRITE_SHORT( 0 );							// ref param_1+0xa40
		WRITE_SHORT( GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		WRITE_SHORT( (int)pPlayer->pev->team );	// ref *(param_1+4)+0x170
	MESSAGE_END();
}

// ref @0x17e19a — menuselect drains, jointeam: 1=Replic else Torri.
BOOL CStargateReplicateur::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if( SGTC_HandleClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;
	if( FStrEq( pcmd, "menuselect" ) )
	{
		if( CMD_ARGC() >= 2 )
			pPlayer->SGTC_MenuselectBombKey( atoi( CMD_ARGV( 1 ) ) );
		return TRUE;
	}
	if( FStrEq( pcmd, "jointeam" ) )
	{
		if( CMD_ARGC() > 0 )
		{
			int team = atoi( CMD_ARGV( 1 ) );
			snprintf( pPlayer->m_szTeamName, TEAM_NAME_LENGTH, "%s", team == 1 ? "Replic" : "Torri" );
			char text[64];
			snprintf( text, sizeof( text ), "you joined the %s team\n", pPlayer->m_szTeamName );
			ClientPrint( pPlayer->pev, HUD_PRINTNOTIFY, text );
			ChangePlayerTeam( pPlayer, pPlayer->m_szTeamName, TRUE, FALSE );
			RecountTeams( FALSE );
		}
		return TRUE;
	}
	if( FStrEq( pcmd, "radio" ) )	// ref TeamDM @0x18149b / TeamCP @0x173fe4: MSG_ONE Radio + CMD_ARGV(1)
	{
		if( CMD_ARGC() > 1 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgRadio, NULL, pPlayer->edict() );
				WRITE_STRING( CMD_ARGV( 1 ) );
			MESSAGE_END();
		}
		return TRUE;
	}
	return FALSE;
}


void CStargateReplicateur::PlayerSpawn( CBasePlayer *pPlayer )
{
	// ref @0x17dc7e (rea_dump/CStargateReplicateur__PlayerSpawn.c): base spawn,
	// +0xca8 &= ~0xb, then shrink the "Replic" team hull.
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );
	pPlayer->m_iSgtcFlagsCa8 &= ~0xB;	// ref param_1+0xca8 &= 0xfffffff4
	if( !stricmp( pPlayer->m_szTeamName, "Replic" ) ) // ref rodata 0x1eade0
	{
		pPlayer->pev->view_ofs = Vector( 0, 0, 6 );	// +0x174
		pPlayer->pev->fov = 110.0f;			// +0x214 = 0x42dc0000
		pPlayer->pev->movetype = 14;			// +0x108 (SGTC custom)
		UTIL_SetSize( pPlayer->pev,
			Vector( -16, -16, -20 ), Vector( 16, 16, 20 ) );
	}
	else
	{
		pPlayer->pev->view_ofs = Vector( 0, 0, 28 );	// standard eye height
		pPlayer->pev->fov = 0;				// +0x214 = 0
		pPlayer->pev->movetype = MOVETYPE_WALK;		// +0x108 = 3
		UTIL_SetSize( pPlayer->pev,
			VEC_HULL_MIN, VEC_HULL_MAX );
	}
}

// ===========================================================================
// Spawn points + info_* point entities
// ===========================================================================
void CSGTCPlayerStart::Spawn( void )
{
	CPointEntity::Spawn();
}
LINK_ENTITY_TO_CLASS( info_player_torri, CSGTCPlayerStart );
LINK_ENTITY_TO_CLASS( info_player_marine, CSGTCPlayerStart );
LINK_ENTITY_TO_CLASS( info_player_goauld, CSGTCPlayerStart );
LINK_ENTITY_TO_CLASS( info_player_unas, CSGTCPlayerStart );

LINK_ENTITY_TO_CLASS( info_marker, CPointEntity );
LINK_ENTITY_TO_CLASS( info_radar_block, CPointEntity );
LINK_ENTITY_TO_CLASS( info_observer, CPointEntity );

void CFlash::Spawn( void )
{
	Precache();
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	if( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	SetTouch( &CFlash::FlashTouch );
	SetThink( &CFlash::Think );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CFlash::Precache( void )
{
	PRECACHE_SOUND( "weapons/flashbang.wav" );
}

void CFlash::FlashTouch( CBaseEntity *pOther )
{
	if( !pOther || !pOther->IsPlayer() )
		return;
	MyTouch( (CBasePlayer *)pOther );
}

BOOL CFlash::MyTouch( CBasePlayer *pPlayer )
{
	// Flash pickup: full flash battery + sound (mirrors h_battery).
	if( pPlayer->m_iFlashBattery >= 100 )
		return FALSE;
	pPlayer->m_iFlashBattery = 100;
	EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/flashbang.wav", 1, ATTN_NORM );
	return TRUE;
}

void CFlash::Think( void )
{
	pev->nextthink = gpGlobals->time + 0.1f;
}
LINK_ENTITY_TO_CLASS( info_flash, CFlash );

TYPEDESCRIPTION CCapInfo::m_SaveData[] =
{
	DEFINE_FIELD( CCapInfo, m_iWinType, FIELD_INTEGER ),
	DEFINE_FIELD( CCapInfo, m_iTeam, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CCapInfo, CPointEntity );

void CCapInfo::Spawn( void )
{
	// ref CCapInfo::Spawn @0x19576a: CBaseEntity::Think() (vtable call only,
	// no body) + pev->nextthink = gpGlobals->time + 0.1. No solid write.
	Think();
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CCapInfo::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "type" ) )
	{
		m_iWinType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_team" ) )
	{
		m_iTeam = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}
LINK_ENTITY_TO_CLASS( info_cap_rules, CCapInfo );

// SGTC_Rules_ReadCapInfo — ref CP NewRound @0x187b34 info_cap_rules walk:
// teamed = CCapInfo +0x19c (m_team), type = CCapInfo +0x1a0 ("type").
void SGTC_Rules_ReadCapInfo( void )
{
	CBaseEntity *pScan = NULL;
	while( ( pScan = UTIL_FindEntityByClassname( pScan, "info_cap_rules" ) ) != NULL )
	{
		CCapInfo *pInfo = (CCapInfo *)pScan;
		sgtc_teamed = pInfo->m_iTeam;
		sgtc_type = pInfo->m_iWinType;
	}
}

// ===========================================================================
// Triggers (CBaseToggle-based; see header note)
// ===========================================================================
TYPEDESCRIPTION CTriggerTeleportAdvanced::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerTeleportAdvanced, m_flTimeTel, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerTeleportAdvanced, m_flArmTime, FIELD_TIME ),
	DEFINE_FIELD( CTriggerTeleportAdvanced, m_hTeleportTarget, FIELD_EHANDLE ),
};
IMPLEMENT_SAVERESTORE( CTriggerTeleportAdvanced, CBaseToggle );

void CTriggerTeleportAdvanced::Spawn( void )
{
	SGTC_InitTrigger( this );
	SetTouch( &CTriggerTeleportAdvanced::AdvTeleportTouch );
	SetThink( &CTriggerTeleportAdvanced::TeleportThink );
	Reset();
}

void CTriggerTeleportAdvanced::KeyValue( KeyValueData *pkvd )
{
	// Verified key: "time_tel" (rodata 0x1ddc14), atof.
	if( FStrEq( pkvd->szKeyName, "time_tel" ) )
	{
		m_flTimeTel = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CTriggerTeleportAdvanced::Reset( void )
{
	m_hTeleportTarget = NULL;
	// .so Reset: nextthink = now + delay, ltime = now - delay.
	pev->nextthink = gpGlobals->time + m_flTimeTel;
	pev->ltime = gpGlobals->time - m_flTimeTel;
	m_flArmTime = 0;
}

void CTriggerTeleportAdvanced::AdvTeleportTouch( CBaseEntity *pOther )
{
	// .so AdvTeleportTouch: arms only if now - ltime >= delay; stores the
	// toucher and its trigger time for TeleportThink.
	if( gpGlobals->time - pev->ltime < m_flTimeTel )
		return;
	if( !pOther )
		return;
	m_hTeleportTarget = pOther;
	m_flArmTime = gpGlobals->time;
	pev->ltime = gpGlobals->time;
}

void CTriggerTeleportAdvanced::TeleportThink( void )
{
	// Fires the armed teleport: move the stored entity to our target
	// (info_teleport_destination / info_marker, FGD "special in" chain).
	pev->nextthink = gpGlobals->time + 0.1f;
	CBaseEntity *pOther = m_hTeleportTarget;
	if( !pOther )
		return;
	if( gpGlobals->time - m_flArmTime < m_flTimeTel )
		return;
	m_hTeleportTarget = NULL;
	edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
	if( FNullEnt( pentTarget ) )
		return;
	Vector tmp = VARS( pentTarget )->origin;
	if( pOther->IsPlayer() )
		tmp.z -= pOther->pev->mins.z;
	tmp.z++;
	pOther->pev->flags &= ~FL_ONGROUND;
	UTIL_SetOrigin( pOther->pev, tmp );
	pOther->pev->angles = pentTarget->v.angles;
	if( pOther->IsPlayer() )
		pOther->pev->v_angle = pentTarget->v.angles;
	pOther->pev->fixangle = TRUE;
	pOther->pev->velocity = pOther->pev->basevelocity = g_vecZero;
}
LINK_ENTITY_TO_CLASS( trigger_adv_teleport, CTriggerTeleportAdvanced );

TYPEDESCRIPTION CTriggerGoal::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerGoal, m_iTeam, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CTriggerGoal, CBaseToggle );

void CTriggerGoal::Spawn( void )
{
	// .so Spawn: solid trigger, touch = GoalTouch.
	SGTC_InitTrigger( this );
	SetTouch( &CTriggerGoal::GoalTouch );
}

void CTriggerGoal::KeyValue( KeyValueData *pkvd )
{
	// Verified key: "m_team" atoi (FGD 1=Tau'ri 2=Goa'uld).
	if( FStrEq( pkvd->szKeyName, "m_team" ) )
	{
		m_iTeam = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CTriggerGoal::GoalTouch( CBaseEntity *pOther )
{
	// .so GoalTouch: master check -> team-mismatch mapping (1<->2) ->
	// alive+player check -> same-team guard -> g_pGameRules->Goal(mapped).
	if( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;
	int mapped = 0;
	if( m_iTeam == 1 )
		mapped = 2;
	else if( m_iTeam == 2 )
		mapped = 1;
	if( !mapped || !pOther || !pOther->IsPlayer() || !pOther->IsAlive() )
		return;
	CBasePlayer *pl = (CBasePlayer *)pOther;
	if( !g_pGameRules || !g_pGameRules->IsTeamplay() )
		return;
	int idx = g_pGameRules->GetTeamIndex( pl->m_szTeamName );
	if( idx < 0 || idx + 1 == m_iTeam )
		return; // already on the goal's team: nothing to award
	CStargateTeamDM *sg = (CStargateTeamDM *)g_pGameRules;
	sg->Goal( mapped );	// ref GoalTouch @0x16e4aa pushes mapped 1/2, not mapped-1
}
LINK_ENTITY_TO_CLASS( trigger_goal, CTriggerGoal );

TYPEDESCRIPTION CTriggerRandom::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerRandom, m_flMinPeriod, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerRandom, m_flMaxPeriod, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CTriggerRandom, CBaseToggle );

void CTriggerRandom::Spawn( void )
{
	SGTC_InitTrigger( this );
	SetTouch( &CTriggerRandom::RandomTouch );
	SetUse( &CTriggerRandom::RandomUse );
	SetThink( &CTriggerRandom::RandomThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( m_flMinPeriod, m_flMaxPeriod );
}

void CTriggerRandom::KeyValue( KeyValueData *pkvd )
{
	// Verified keys: "minperiod"/"maxperiod" (rodata 0x1d9b44/0x1d9b4e).
	if( FStrEq( pkvd->szKeyName, "minperiod" ) )
	{
		m_flMinPeriod = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxperiod" ) )
	{
		m_flMaxPeriod = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CTriggerRandom::RandomTouch( CBaseEntity *pOther )
{
	// Own MultiTouch equivalent (stock MultiTouch is TU-local): clients /
	// allowed monsters / pushables, master-gated, then fire.
	entvars_t *pevToucher = pOther->pev;
	if( ( ( pevToucher->flags & FL_CLIENT ) && !( pev->spawnflags & SF_TRIGGER_NOCLIENTS ) ) ||
		( ( pevToucher->flags & FL_MONSTER ) && ( pev->spawnflags & SF_TRIGGER_ALLOWMONSTERS ) ) ||
		( ( pev->spawnflags & SF_TRIGGER_PUSHABLES ) && FClassnameIs( pevToucher, "func_pushable" ) ) )
	{
		if( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
			return;
		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
	}
}

void CTriggerRandom::RandomUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Own ToggleUse equivalent (stock ToggleUse is TU-local).
	if( pev->solid == SOLID_NOT )
	{
		pev->solid = SOLID_TRIGGER;
		gpGlobals->force_retouch++;
	}
	else
	{
		pev->solid = SOLID_NOT;
	}
	UTIL_SetOrigin( pev, pev->origin );
	(void)pActivator; (void)pCaller; (void)useType; (void)value;
}

void CTriggerRandom::RandomThink( void )
{
	// .so RandomThink: optional noise, SUB_UseTargets, reschedule by
	// RANDOM(min,max) added to now.
	if( !FStringNull( pev->noise ) )
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM );
	SUB_UseTargets( this, USE_TOGGLE, 0 );
	float lo = m_flMinPeriod, hi = m_flMaxPeriod;
	if( hi < lo )
		hi = lo;
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( lo, hi );
	SetThink( &CTriggerRandom::RandomThink );
}
LINK_ENTITY_TO_CLASS( trigger_random, CTriggerRandom );
// trigger_secret: string-only in all 3 binaries (rodata 0x1dd920), no
// factory/class — maps use it as a one-shot random trigger. Alias it.
LINK_ENTITY_TO_CLASS( trigger_secret, CTriggerRandom );

// ===========================================================================
// item_objective (CItemSecurity behaviour + models/key.mdl)
// ===========================================================================
void CItemObjective::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/key.mdl" );
	CItem::Spawn();
}

void CItemObjective::Precache( void )
{
	PRECACHE_MODEL( "models/key.mdl" );
	PRECACHE_SOUND( "items/gunpickup2.wav" );
}

BOOL CItemObjective::MyTouch( CBasePlayer *pPlayer )
{
	// Mirrors stock CItemSecurity::MyTouch (items.cpp:292).
	pPlayer->m_rgItems[ITEM_SECURITY] += 1;
	pPlayer->m_iInBombZone++;	// ref CItemSecurity::MyTouch @0xfd819
	MESSAGE_BEGIN( MSG_ONE, gmsgHasKey, NULL, pPlayer->edict() );
		WRITE_BYTE( pPlayer->m_rgItems[ITEM_SECURITY] );
		WRITE_BYTE( 0 ); // ref HasKey REG size 2 (key + vote byte)
	MESSAGE_END();
	return TRUE;
}
LINK_ENTITY_TO_CLASS( item_objective, CItemObjective );

// ===========================================================================
// env_model (CModel)
// ===========================================================================
TYPEDESCRIPTION CModel::m_SaveData[] =
{
	DEFINE_FIELD( CModel, m_iAnim, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CModel, CBaseAnimating );

void CModel::Spawn( void )
{
	PRECACHE_MODEL( (char *)STRING( pev->model ) );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetSize( pev, Vector( -20, -20, -20 ), Vector( 20, 20, 20 ) );
	if( m_iAnim != 0 )
	{
		pev->sequence = m_iAnim;
		pev->frame = 0;
		ResetSequenceInfo();
	}
	SetThink( &CModel::Animate );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CModel::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "anim" ) )
	{
		m_iAnim = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseAnimating::KeyValue( pkvd );
}

void CModel::Animate( void )
{
	// FGD spawnflag 2 = follow target.
	if( FBitSet( pev->spawnflags, 2 ) && pev->target )
	{
		CBaseEntity *t = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
		if( t )
			UTIL_SetOrigin( pev, t->pev->origin );
	}
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1f;
}
LINK_ENTITY_TO_CLASS( env_model, CModel );

// ===========================================================================
// Particle net entities. Payload layouts are BEST-EFFORT (client handlers
// drain opaquely): entindex + origin + param block, MSG_ALL on Spawn,
// MSG_ONE in SendValueTo.
// ===========================================================================
static void SGTC_ParseVector( const char *str, Vector &out )
{
	out = Vector( 0, 0, 0 );
	if( str )
		sscanf( str, "%f %f %f", &out.x, &out.y, &out.z );
}

static int SGTC_ColorByte( float f )
{
	int c = (int)f;
	if( c < 0 )
		c = 0;
	if( c > 255 )
		c = 255;
	return c;
}

static int SGTC_Rules_roundf( float v )	// ref (int)ROUND(float) byte conversion
{
	return (int)( v >= 0.0f ? v + 0.5f : v - 0.5f );
}

static void SGTC_ParseColor4( const char *str, float out[4] )
{
	int r = 128, g = 128, b = 128, a = 128;
	if( str && sscanf( str, "%d %d %d %d", &r, &g, &b, &a ) == 4 )
	{
		out[0] = (float)r;
		out[1] = (float)g;
		out[2] = (float)b;
		out[3] = (float)a;
	}
}

static void SGTC_ParseColor( const char *str, Vector &out )
{
	float c[4];
	SGTC_ParseColor4( str, c );
	out = Vector( c[0], c[1], c[2] );
}

static void SGTC_WriteEnvParticulePayload( CEnvParticule *p )
{
	// ref CEnvParticule::SendValue @0x15ec68 — 47 bytes (19 WRITE_COORD + 7 BYTE + 1 SHORT).
	// First 6 coords: pev->origin then pev->oldorigin (entvars +0xc4..+0xd8), not absmin/max.
	WRITE_COORD( p->pev->origin.x );
	WRITE_COORD( p->pev->origin.y );
	WRITE_COORD( p->pev->origin.z );
	WRITE_COORD( p->pev->oldorigin.x );
	WRITE_COORD( p->pev->oldorigin.y );
	WRITE_COORD( p->pev->oldorigin.z );
	WRITE_COORD( p->m_vecAccel.x );
	WRITE_COORD( p->m_vecAccel.y );
	WRITE_COORD( p->m_vecAccel.z );
	WRITE_COORD( p->m_vecV0.x );
	WRITE_COORD( p->m_vecV0.y );
	WRITE_COORD( p->m_vecV0.z );
	WRITE_COORD( p->m_vecSpread.x );
	WRITE_COORD( p->m_vecSpread.y );
	WRITE_COORD( p->m_vecSpread.z );
	for( int i = 0; i < 4; i++ )
		WRITE_BYTE( SGTC_ColorByte( p->m_flColor[i] ) );
	WRITE_COORD( p->m_vecSize.x );
	WRITE_COORD( p->m_vecSize.y );
	WRITE_COORD( p->m_flLife );
	WRITE_COORD( p->m_flBounce );
	WRITE_BYTE( p->m_iSprite );
	WRITE_BYTE( p->m_iType );
	WRITE_SHORT( p->m_iCount );
	WRITE_BYTE( p->m_iId );
}

TYPEDESCRIPTION CEnvParticule::m_SaveData[] =
{
	DEFINE_FIELD( CEnvParticule, m_iCount, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvParticule, m_iId, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvParticule, m_vecAccel, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvParticule, m_vecV0, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvParticule, m_vecSpread, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvParticule, m_vecSize, FIELD_VECTOR ),
	DEFINE_ARRAY( CEnvParticule, m_flColor, FIELD_FLOAT, 4 ),
	DEFINE_FIELD( CEnvParticule, m_flLife, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvParticule, m_flBounce, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvParticule, m_iSprite, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvParticule, m_iType, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CEnvParticule, CPointEntity );

void CEnvParticule::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SetObjectCollisionBox();
}

void CEnvParticule::Activate( void )
{
	CPointEntity::Activate();
	SendValue();
}

void CEnvParticule::KeyValue( KeyValueData *pkvd )
{
	// Verified rodata keys (0x1d9b5f..0x1d9bbf block).
	if( FStrEq( pkvd->szKeyName, "nombre" ) )
	{
		m_iCount = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "id" ) )
	{
		m_iId = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "acceleration" ) )
	{
		SGTC_ParseVector( pkvd->szValue, m_vecAccel );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "v0" ) )
	{
		SGTC_ParseVector( pkvd->szValue, m_vecV0 );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "ecart" ) )
	{
		SGTC_ParseVector( pkvd->szValue, m_vecSpread );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "taille" ) )
	{
		int x = 2, y = 2;
		if( sscanf( pkvd->szValue, "%d %d", &x, &y ) == 2 )
			m_vecSize = Vector( (float)x, (float)y, 0 );
		else
			m_vecSize = Vector( 2, 2, 0 );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "_couleur" ) )
	{
		SGTC_ParseColor4( pkvd->szValue, m_flColor );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "vie" ) )
	{
		m_flLife = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bounce" ) )
	{
		m_flBounce = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sprite" ) )
	{
		m_iSprite = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "type" ) )
	{
		m_iType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CEnvParticule::Reset( void )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgChangeParticuleState );
		WRITE_BYTE( m_iId );
		WRITE_BYTE( FBitSet( pev->spawnflags, 1 ) ? 1 : 0 );
	MESSAGE_END();
}

void CEnvParticule::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType, FBitSet( pev->spawnflags, 1 ) ) )
		return;
	if( FBitSet( pev->spawnflags, 1 ) )
		pev->spawnflags &= ~1;
	else
		pev->spawnflags |= 1;
	MESSAGE_BEGIN( MSG_ALL, gmsgChangeParticuleState );
		WRITE_BYTE( m_iId );
		WRITE_BYTE( FBitSet( pev->spawnflags, 1 ) ? 1 : 0 );
	MESSAGE_END();
	(void)pActivator; (void)pCaller; (void)value;
}

void CEnvParticule::SendValue( void )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgEnvParticule, NULL );
	SGTC_WriteEnvParticulePayload( this );
	MESSAGE_END();
}

void CEnvParticule::SendValueTo( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgEnvParticule, NULL, pPlayer->pev );
	SGTC_WriteEnvParticulePayload( this );
	MESSAGE_END();
}
LINK_ENTITY_TO_CLASS( env_particule, CEnvParticule );

TYPEDESCRIPTION CEnvRain::m_SaveData[] =
{
	DEFINE_FIELD( CEnvRain, m_flDensity, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CEnvRain, CEnvParticule );

void CEnvRain::KeyValue( KeyValueData *pkvd )
{
	// .so CEnvRain::KeyValue handles "densite", rest falls to particule.
	if( FStrEq( pkvd->szKeyName, "densite" ) )
	{
		m_flDensity = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CEnvParticule::KeyValue( pkvd );
}

static void SGTC_RainEnsureCount( CEnvRain *pRain )
{
	if( pRain->m_iCount != 0 )
	{
		if( pRain->m_iCount < 0 )
			pRain->m_iCount = -pRain->m_iCount;
		return;
	}
	// ref CEnvRain::SendValue @0x15f84a — volume from origin vs oldorigin (+0xc4..+0xd8).
	const float vol = ( pRain->pev->origin.x - pRain->pev->oldorigin.x ) *
		( pRain->pev->origin.y - pRain->pev->oldorigin.y ) *
		( pRain->pev->origin.z - pRain->pev->oldorigin.z );
	pRain->m_iCount = (int)( pRain->m_flDensity * vol );
	if( pRain->m_iCount < 0 )
		pRain->m_iCount = -pRain->m_iCount;
}

static void SGTC_WriteEnvRainPayload( CEnvRain *pRain )
{
	// ref CEnvRain::SendValue @0x15f82a — 15 bytes (6 COORD + BYTE + SHORT).
	WRITE_COORD( pRain->pev->origin.x );
	WRITE_COORD( pRain->pev->origin.y );
	WRITE_COORD( pRain->pev->origin.z );
	WRITE_COORD( pRain->pev->oldorigin.x );
	WRITE_COORD( pRain->pev->oldorigin.y );
	WRITE_COORD( pRain->pev->oldorigin.z );
	WRITE_BYTE( pRain->m_iSprite );
	WRITE_SHORT( pRain->m_iCount );
}

void CEnvRain::SendValue( void )
{
	SGTC_RainEnsureCount( this );
	MESSAGE_BEGIN( MSG_ALL, gmsgEnvRain, NULL );
	SGTC_WriteEnvRainPayload( this );
	MESSAGE_END();
}

void CEnvRain::SendValueTo( CBasePlayer *pPlayer )
{
	SGTC_RainEnsureCount( this );
	MESSAGE_BEGIN( MSG_ONE, gmsgEnvRain, NULL, pPlayer->pev );
	SGTC_WriteEnvRainPayload( this );
	MESSAGE_END();
}
LINK_ENTITY_TO_CLASS( env_rain, CEnvRain );

TYPEDESCRIPTION CEnvLensFlare::m_SaveData[] =
{
	DEFINE_FIELD( CEnvLensFlare, m_vecColor, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvLensFlare, m_bUseOrigin, FIELD_BOOLEAN ),
	DEFINE_FIELD( CEnvLensFlare, m_vecOrigin, FIELD_VECTOR ),
};
IMPLEMENT_SAVERESTORE( CEnvLensFlare, CPointEntity );

void CEnvLensFlare::KeyValue( KeyValueData *pkvd )
{
	// Verified keys: _couleur / useorigine / origine.
	if( FStrEq( pkvd->szKeyName, "_couleur" ) )
	{
		SGTC_ParseColor( pkvd->szValue, m_vecColor );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "useorigine" ) )
	{
		m_bUseOrigin = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "origine" ) )
	{
		SGTC_ParseVector( pkvd->szValue, m_vecOrigin );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

static void SGTC_WriteLensFlarePayload( CEnvLensFlare *pLens )
{
	// ref CEnvLensFlare::SendValue @0x161184 — 10 bytes (3 COORD + 4 BYTE).
	Vector org = pLens->m_bUseOrigin ? pLens->pev->origin : pLens->m_vecOrigin;
	WRITE_COORD( org.x );
	WRITE_COORD( org.y );
	WRITE_COORD( org.z );
	WRITE_BYTE( (int)pLens->m_vecColor.x );
	WRITE_BYTE( (int)pLens->m_vecColor.y );
	WRITE_BYTE( (int)pLens->m_vecColor.z );
	WRITE_BYTE( 255 );
}

void CEnvLensFlare::SendValue( void )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgEnvNewLens, NULL );
	SGTC_WriteLensFlarePayload( this );
	MESSAGE_END();
}

void CEnvLensFlare::SendValueTo( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgEnvNewLens, NULL, pPlayer->pev );
	SGTC_WriteLensFlarePayload( this );
	MESSAGE_END();
}
LINK_ENTITY_TO_CLASS( env_lensflare, CEnvLensFlare );

TYPEDESCRIPTION CEnvFountain::m_SaveData[] =
{
	DEFINE_FIELD( CEnvFountain, m_iId, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvFountain, m_iCount, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvFountain, m_flMinAngle, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_flMaxAngle, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_flMinVel, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_flMaxVel, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_flMinRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_flMaxRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvFountain, m_vecOrigine, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvFountain, m_vecSize, FIELD_VECTOR ),
	DEFINE_ARRAY( CEnvFountain, m_flColor, FIELD_FLOAT, 4 ),
	DEFINE_FIELD( CEnvFountain, m_flLife, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_flBounce, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFountain, m_iSprite, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvFountain, m_iType, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CEnvFountain, CPointEntity );

void CEnvFountain::KeyValue( KeyValueData *pkvd )
{
	// Verified rodata keys (0x1d9be5..0x1d9c51 block).
	if( FStrEq( pkvd->szKeyName, "nombre" ) )
	{
		m_iCount = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "id" ) )
	{
		m_iId = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "directeur" ) )
	{
		SGTC_ParseVector( pkvd->szValue, m_vecDir );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "origine" ) )
	{
		SGTC_ParseVector( pkvd->szValue, m_vecOrigine );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "taille" ) )
	{
		int x = 2, y = 2;
		if( sscanf( pkvd->szValue, "%d %d", &x, &y ) == 2 )
			m_vecSize = Vector( (float)x, (float)y, 0 );
		else
			m_vecSize = Vector( 2, 2, 0 );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "_couleur" ) )
	{
		SGTC_ParseColor4( pkvd->szValue, m_flColor );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "minAngle" ) )
	{
		m_flMinAngle = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxAngle" ) )
	{
		m_flMaxAngle = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "minRadius" ) )
	{
		m_flMinRadius = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxRadius" ) )
	{
		m_flMaxRadius = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "minVelocity" ) )
	{
		m_flMinVel = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxVelocity" ) )
	{
		m_flMaxVel = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "vie" ) )
	{
		m_flLife = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bounce" ) )
	{
		m_flBounce = (float)atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sprite" ) )
	{
		m_iSprite = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "type" ) )
	{
		m_iType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CEnvFountain::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType, FBitSet( pev->spawnflags, 1 ) ) )
		return;
	if( FBitSet( pev->spawnflags, 1 ) )
		pev->spawnflags &= ~1;
	else
		pev->spawnflags |= 1;
	MESSAGE_BEGIN( MSG_ALL, gmsgChangeParticuleState );
		WRITE_BYTE( m_iId );
		WRITE_BYTE( FBitSet( pev->spawnflags, 1 ) ? 1 : 0 );
	MESSAGE_END();
	(void)pActivator; (void)pCaller; (void)value;
}

static void SGTC_WriteEnvFountainPayload( CEnvFountain *p )
{
	// ref CEnvFountain::SendValue @0x1605c2 — 45 bytes.
	WRITE_COORD( p->pev->origin.x );
	WRITE_COORD( p->pev->origin.y );
	WRITE_COORD( p->pev->origin.z );
	WRITE_COORD( p->m_flMinRadius );
	WRITE_COORD( p->m_flMaxRadius );
	WRITE_COORD( p->m_vecOrigine.x );
	WRITE_COORD( p->m_vecOrigine.y );
	WRITE_COORD( p->m_vecOrigine.z );
	WRITE_COORD( p->m_flMinAngle );
	WRITE_COORD( p->m_flMaxAngle );
	WRITE_COORD( p->m_flMinVel );
	WRITE_COORD( p->m_flMaxVel );
	WRITE_COORD( p->m_vecDir.x );
	WRITE_COORD( p->m_vecDir.y );
	WRITE_COORD( p->m_vecDir.z );
	for( int i = 0; i < 4; i++ )
		WRITE_BYTE( SGTC_ColorByte( p->m_flColor[i] ) );
	WRITE_BYTE( (int)SGTC_Rules_roundf( p->m_vecSize.x ) );
	WRITE_BYTE( (int)SGTC_Rules_roundf( p->m_vecSize.y ) );
	WRITE_BYTE( (int)SGTC_Rules_roundf( p->m_flLife ) );
	WRITE_BYTE( (int)SGTC_Rules_roundf( p->m_flBounce ) );
	WRITE_BYTE( p->m_iSprite );
	WRITE_BYTE( p->m_iType );
	WRITE_SHORT( p->m_iCount );
	WRITE_BYTE( p->m_iId );
}

void CEnvFountain::SendValue( void )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgEnvFountain, NULL );
	SGTC_WriteEnvFountainPayload( this );
	MESSAGE_END();
}

void CEnvFountain::SendValueTo( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgEnvFountain, NULL, pPlayer->pev );
	SGTC_WriteEnvFountainPayload( this );
	MESSAGE_END();
}
LINK_ENTITY_TO_CLASS( env_fountain, CEnvFountain );

// ===========================================================================
// env_fog — CClientFog (client-driven distance fog volume).
// Verified vs ref/stc_i386.so:
//   env_fog factory thunk @0x19d364 -> GetClassPtr<CClientFog>(...);
//   CClientFog::KeyValue  @0x19c03a: keys "startdist" (rodata 0x1ddcd0) and
//     "enddist" (0x1ddcda), both parsed with atoi() into float members
//     (this+0x19c / this+0x1a0), pkvd->fHandled = TRUE.
//   CClientFog::Spawn     @0x19c106: pev->movetype = 0 (MOVETYPE_NONE),
//     pev->solid = 0 (SOLID_NOT), pev->rendermode = 2 (kRenderTransTexture),
//     pev->renderamt = 0.0f. No Precache, no model, no think.
// The server side is a passive data entity; the client renders the fog from
// the startdist/enddist baseline fields.
// ===========================================================================
TYPEDESCRIPTION CClientFog::m_SaveData[] =
{
	DEFINE_FIELD( CClientFog, m_flStartDist, FIELD_FLOAT ),
	DEFINE_FIELD( CClientFog, m_flEndDist, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CClientFog, CBaseEntity );

void CClientFog::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "startdist" ) )
	{
		m_flStartDist = (float)atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "enddist" ) )
	{
		m_flEndDist = (float)atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CClientFog::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0.0f;
}
LINK_ENTITY_TO_CLASS( env_fog, CClientFog );

// ===========================================================================
// mode_relay
// ===========================================================================
TYPEDESCRIPTION CModeRelay::m_SaveData[] =
{
	DEFINE_ARRAY( CModeRelay, m_iModes, FIELD_INTEGER, 6 ),
	DEFINE_FIELD( CModeRelay, m_iTriggerState, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CModeRelay, CPointEntity );

void CModeRelay::Spawn( void )
{
	pev->solid = SOLID_NOT;
}

void CModeRelay::KeyValue( KeyValueData *pkvd )
{
	// Verified keys: mode_a..mode_f + triggerstate (rodata 0x1dd3e0..).
	static const char *const keys[6] =
		{ "mode_a", "mode_b", "mode_c", "mode_d", "mode_e", "mode_f" };
	for( int i = 0; i < 6; i++ )
	{
		if( FStrEq( pkvd->szKeyName, keys[i] ) )
		{
			m_iModes[i] = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
			return;
		}
	}
	if( FStrEq( pkvd->szKeyName, "triggerstate" ) )
	{
		m_iTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CModeRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Fires targets when the active mp_gamemode letter matches a set mode;
	// with no mode_* keys set it behaves as a plain relay.
	const char *mode = CVAR_GET_STRING( "mp_gamemode" );
	BOOL anyMode = FALSE;
	BOOL match = FALSE;
	static const char letters[6] = { 'n', 't', 'c', 'k', 'e', 'f' };
	for( int i = 0; i < 6; i++ )
	{
		if( !m_iModes[i] )
			continue;
		anyMode = TRUE;
		if( mode && strchr( mode, letters[i] ) )
			match = TRUE;
	}
	if( !anyMode || match )
		SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
	(void)pCaller; (void)useType; (void)value;
}
LINK_ENTITY_TO_CLASS( mode_relay, CModeRelay );
