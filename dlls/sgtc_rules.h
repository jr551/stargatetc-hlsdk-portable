// sgtc_rules.h — StargateTC teams/rules/triggers/env (NEW file).
//
// Authoritative refs: ref/stc_i386.so (2.0, unstripped — nm -C / objdump -d -C),
//   ref/stc_v1_i386.so, ref/v1_sourcefiles.txt (stargate_teamdm/cp/unas/invisi.cpp,
//   sgtc_entities.cpp), local://sgtc-server-inventory.md Appendix A/B,
//   stargatetc/stargate.fgd.
// Base: hlsdk-xash3d/dlls (CHalfLifeTeamplay / CBaseToggle / CPointEntity /
//   CBaseAnimating). Stock CBaseTrigger/CTriggerMultiple are TU-local to
//   triggers.cpp (no header decl), so SGTC triggers derive CBaseToggle and
//   carry their own touch/use wiring.
// Stock files are NEVER edited; hook points are listed in sgtc_rules.cpp top
// comment and in the yield report.
#pragma once
#ifndef SGTC_RULES_H
#define SGTC_RULES_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "items.h"

// ---------------------------------------------------------------------------
// Teams. Verified against .so rodata + ctor teamlists:
//   DM/CP  : "Torri;Goa'uld"  (fileoff 0x1db8c0 / 0x1db890)
//   Unas   : "Unas;Marine"     (fileoff 0x1dc5f9, Unas ctor)
//   Invis  : "Torri;Invisible" (fileoff 0x1dad91, Invis ctor)
// SGTC team ids on the wire: 1=Tau'ri/Torri, 2=Goa'uld (FGD trigger_goal),
// Unas mode reuses slots 1=Unas, 2=Marine (Unas PlayerSpawn strings).
// ---------------------------------------------------------------------------
#define SGTC_TEAM_TORRI   1
#define SGTC_TEAM_GOAULD  2
#define SGTC_TEAM_UNAS    1
#define SGTC_TEAM_MARINE  2
#define SGTC_MAX_SGTC_TEAMS 2
#define SGTC_TEAMNAME_LEN 16

// Server user messages owned by this file (BSS gmsg* in the .so, A.1).
// Stock LinkUserMessages (player.cpp) does NOT register these.
extern int gmsgRadarBlip;
extern int gmsgEnvParticule;
extern int gmsgEnvRain;
extern int gmsgEnvFountain;
extern int gmsgEnvNewLens;
extern int gmsgEnvAddRing;
extern int gmsgChangeParticuleState;
extern int gmsgGameMode;
extern int gmsgStamina;
extern int gmsgSubModel;
extern int gmsgProgress;
extern int gmsgHasKey;
extern int gmsgHudInfoMode;
extern int gmsgConcuss;
extern int gmsgArmor;
extern int gmsgZatTouch;
extern int g_iSgtcMapType;	// ref MapType @0x236594 (skip camp warn when ==1)
void SGTC_LinkUserMessages( void );
void SGTC_InitTrigger( CBaseToggle *pTrigger );
BOOL SGTC_HandleClientCommand( CBasePlayer *pPlayer, const char *pcmd );	// ref ClientCommand hurtinvert/recallall
// Round-lifecycle hooks implemented by the zone/rules owners (ref CP NewRound
// @0x187b34 zone_capture/info_cap_rules walks; bodies go in sgtc_zones.cpp /
// sgtc_rules.cpp).
void SGTC_Zones_ResetCaptureZones( void );	// +0x244/+0x240/+0x250=0, pev->health=69 (0x428a0000), +0x258=now-1
void SGTC_Rules_ReadCapInfo( void );		// sgtc_teamed = m_team, sgtc_type = "type"

// Mode-selecting installer. Stock InstallGameRules() (gamerules.cpp:310)
// cannot know about SGTC classes; the one-line hook is documented in the
// .cpp. mp_gamemode values (InstallGameRules chain): "sg_"->DM,
// "sgi_"->CP, "un_"->Unas, "inv_"->Invis, "rep_"->Replicateur, else stock.
CGameRules *SGTC_InstallGameRules( const char *pszMode );

// ---------------------------------------------------------------------------
// CStargateTeamDM — 38 methods in .so (B.4). Base is CHalfLifeTeamplay so the
// stock teamplay patterns (teamplay_gamerules.cpp) are mirrored on the fixed
// 2-team SGTC tables below.
// CStargateTeamCP adds InverseSpawns + capture logic; same 38 minus Goal.
// ---------------------------------------------------------------------------
class CStargateTeamDM : public CHalfLifeTeamplay
{
public:
	CStargateTeamDM( void );

	// CGameRules
	void Think( void ) override;
	BOOL IsTeamplay( void ) override { return TRUE; }
	BOOL IsStargate( void ) { return TRUE; }
	const char *GetGameDescription( void ) override { return "StargateTC-DM"; }
	void InitHUD( CBasePlayer *pPlayer ) override;
	void UpdateGameMode( CBasePlayer *pPlayer ) override;
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override;
	BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker ) override;
	BOOL FPlayerCanChangeInfo( CBasePlayer *pPlayer );
	BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target ) override;
	int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled ) override;
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;
	void DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;
	void ClientDisconnected( edict_t *pClient ) override;
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;
	void SGTC_CommandClasse( CBasePlayer *pPlayer, BOOL bCaptureMode );	// ref 'classe' verb body (DM @0x191336 tail / CP @0x183b88 tail)
	void ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer ) override;
	int DeadPlayerWeapons( CBasePlayer *pPlayer ) override;
	int DeadPlayerAmmo( CBasePlayer *pPlayer ) override;
	void PlayerThink( CBasePlayer *pPlayer ) override;
	edict_t *GetPlayerSpawnSpot( CBasePlayer *pPlayer );
	// teamplay
	const char *GetTeamID( CBaseEntity *pEntity ) override;
	int GetTeamIndex( const char *pTeamName ) override;
	const char *GetIndexedTeamName( int teamIndex ) override;
	BOOL IsValidTeam( const char *pTeamName ) override;
	const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer ) override;
	void ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib ) override;
	void RecountTeams( BOOL bResendInfo = FALSE );
	const char *TeamWithFewestPlayers( void );
	// SGTC extras (exact .so names)
	void Goal( int iTeam );
	void EndRound( void );
	void NewRound( void );
	virtual void GlobalText( int iTeam, int iText );
	virtual void AddPortrait( CBasePlayer *pPlayer );		// ref DM vtable+0x110 (spawn tail)
	virtual void StartPortrait( CBasePlayer *pPlayer );		// ref DM vtable+0x114 (spawn tail)
	virtual void RedrawPortrait( CBasePlayer *pPlayer );
	virtual void UpdateClassCount( void );
	void Penalty( CBasePlayer *pVictim, entvars_t *pKillerEv );	// ref Penalty @0x18ace2 (PlayerKilled path)

protected:
	void SGTC_SendTeamNames( CBasePlayer *pPlayer );
	edict_t *SGTC_FindTeamSpawn( const char *pszClassname );
	void SGTC_NewRoundCommon( BOOL bCaptureMode );	// shared NewRound body (sgtc_round.cpp)

	char m_szSGTCTeams[SGTC_MAX_SGTC_TEAMS][SGTC_TEAMNAME_LEN];
	int m_iSGTCTeamScores[SGTC_MAX_SGTC_TEAMS];
	int m_iSGTCTeamCount;
	BOOL m_bRoundOver;
	// Ref gamerules fields (this+0xNNN), mapped for the round core:
	int m_iSgtcModeVote;		// ref +0x770: pending mode vote (-1 none)
	float m_flSgtcRoundTime;	// ref +0x728: round duration (s)
	float m_flSgtcRoundEnd;		// ref +0x724: round end tick
	int m_iSgtcRoundTicks;		// ref +0x73c: Goal guard + NewRound zero (restart counter)
	int m_iSgtcFirstTicks;		// ref +0x750: NewRound writes -5 (0xfffffffb)
	int m_iSgtcEmptyTeams;		// ref +0x758: 0 both/G/T=1/2/3 empty-teams state
	int m_iSgtcFocusedBucket;	// ref +0x75c: 0 none / 1 one side / 2 both focused
	int m_iSgtcAutoBalance;		// ref +0x768: 0 none, 1 ->T light, 2 ->G light
	char m_bSgtcVoteEqual;		// ref +0x774: unanimous-vote flag
	BOOL m_bDisableDeathMessages;	// ref this+0xc — armed around forced ChangePlayerTeam kill
	BOOL m_bDisableDeathPenalty;	// ref this+0x10 — gates PlayerKilled stock path
};

class CStargateTeamCP : public CStargateTeamDM
{
public:
	CStargateTeamCP( void );
	const char *GetGameDescription( void ) override { return "StargateTC-CTP"; }
	void Think( void ) override;	// ref @0x1853e8 — resetround type-4 ticket tail
	void InverseSpawns( void );
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	void PlayerThink( CBasePlayer *pPlayer ) override;
	void NewRound( void );
	void InitHUD( CBasePlayer *pPlayer ) override;	// ref @0x1833f2
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;	// ref @0x183b88
	void ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer ) override;	// ref @0x182c02
	edict_t *GetPlayerSpawnSpot( CBasePlayer *pPlayer );
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override;	// ref @0x17ea2a
	BOOL m_bSpawnsInversed;
	int m_iSgtcCPSide;			// ref this+0x734 — type-4 50hp side selector
	// NOTE: ref CP vtable re-points AddPortrait(0x17f72e)/StartPortrait(0x17f99c)/
	// RedrawPortrait(0x17fc58)/UpdateClassCount(0x17f308)/GlobalText(0x17f244);
	// those CP bodies are a separate slice (PlayerSpawn ports the tail calls so
	// they dispatch correctly once ported).
};

// Unas / Invis modes: 27 + 27 methods in .so (B.4). Thin teamplay shells:
// fixed 2-team lists, team-appropriate spawn loadouts, no rounds/portraits.
class CStargateUnas : public CHalfLifeTeamplay
{
public:
	CStargateUnas( void );
	BOOL IsTeamplay( void ) override { return TRUE; }
	BOOL IsStargate( void ) { return TRUE; }
	const char *GetGameDescription( void ) override { return "Sg-Unas"; }
	void Think( void ) override;
	void InitHUD( CBasePlayer *pPlayer ) override;
	void UpdateGameMode( CBasePlayer *pPlayer ) override;
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled ) override;
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;
	void DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;
	BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker ) override;
	BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target ) override;
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;
	void ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer ) override;
	const char *GetTeamID( CBaseEntity *pEntity ) override;
	int GetTeamIndex( const char *pTeamName ) override;
	const char *GetIndexedTeamName( int teamIndex ) override;
	BOOL IsValidTeam( const char *pTeamName ) override;
	const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer ) override;
	void ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib ) override;
	void RecountTeams( BOOL bResendInfo = FALSE );
	const char *TeamWithFewestPlayers( void );
protected:
	void SGTC_SendTeamNames( CBasePlayer *pPlayer );
	edict_t *SGTC_FindTeamSpawn( const char *pszClassname );
	char m_szSGTCTeams[SGTC_MAX_SGTC_TEAMS][SGTC_TEAMNAME_LEN];
	int m_iSGTCTeamCount;
};

class CStargateInvis : public CStargateUnas
{
public:
	CStargateInvis( void );
	const char *GetGameDescription( void ) override { return "StargateTC"; }
	void InitHUD( CBasePlayer *pPlayer ) override;	// ref @0x17d1f0 — ShowVGUIMenu(11) variant
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;	// ref @0x17d4c4
};

// ref CStargateReplicateur::CStargateReplicateur @0x16e364 — base CHalfLifeMultiplay.
class CStargateReplicateur : public CHalfLifeMultiplay
{
public:
	CStargateReplicateur( void );
	BOOL IsStargate( void ) { return TRUE; }
	const char *GetGameDescription( void ) override { return "StargateTC-Replic"; }
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	// ref decompile @0x17de78/0x17d998/0x17e19a
	void RecountTeams( BOOL bResendInfo );
	void ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib ) override;
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;
	int GetTeamIndex( const char *pTeamName ) override;
	BOOL IsValidTeam( const char *pTeamName ) override;
protected:
	char m_szRepTeamList[512];	// ref this+0x18 — "Replic;Torri" or worldspawn teamlist
	BOOL m_teamLimit;			// ref this+0x14
	char m_szRepTeams[8][16];	// ref team_names[64][0x10]
	BOOL m_bDisableDeathMessages;	// ref this+0xc
	BOOL m_bDisableDeathPenalty;	// ref this+0x10
	int m_iRepScores[32];
	int m_iRepTeamCount;
};

// ---------------------------------------------------------------------------
// Spawn points. In the .so these are bare CPointEntity factories
// (info_player_torri 0x18b8b4, marine 0x18b864, goauld 0x18b88c,
// unas 0x18b83c). Team filtering happens in GetPlayerSpawnSpot, not in the
// entities themselves — same design here.
// ---------------------------------------------------------------------------
class CSGTCPlayerStart : public CPointEntity
{
public:
	void Spawn( void );
};

// ---------------------------------------------------------------------------
// Point entities: info_marker / info_radar_block / info_observer are bare
// CPointEntity factories (0x18b7ec / 0x18b7c4 / 0x18b814). info_flash is
// CFlash (FlashTouch/MyTouch/Precache/Spawn/Think). info_cap_rules is
// CCapInfo (KeyValue type + m_team, Spawn).
// ---------------------------------------------------------------------------
class CFlash : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT FlashTouch( CBaseEntity *pOther );
	BOOL MyTouch( CBasePlayer *pPlayer );
	void EXPORT Think( void );
};

class CCapInfo : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	int m_iWinType; // "type": 0=all 1=75% 2=hold-60s 3=most-points-10min
	int m_iTeam;    // "m_team": 0=all 1=goauld 2=tau'ri
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// ---------------------------------------------------------------------------
// Triggers. Base is CBaseToggle (CBaseTrigger is TU-local to triggers.cpp).
// CTriggerTeleportAdvanced: KeyValue "time_tel" (rodata 0x1ddc14), Reset,
//   Spawn, AdvTeleportTouch (arms, stores time+target), TeleportThink (fires
//   when armed time elapses).
// CTriggerGoal: KeyValue "m_team" (atoi), Spawn, GoalTouch (master check +
//   alive-check + team-mismatch check, then g_pGameRules->Goal()).
// CTriggerRandom: KeyValue "minperiod"/"maxperiod" (rodata 0x1d9b44/4e),
//   RandomTouch/RandomUse (own MultiTouch/ToggleUse equivalents),
//   RandomThink (optional noise sound, SUB_UseTargets, reschedules itself).
// trigger_secret: string-only in all 3 binaries (rodata 0x1dd920), NO
//   factory and NO class — aliased to CTriggerRandom here.
// ---------------------------------------------------------------------------
class CTriggerTeleportAdvanced : public CBaseToggle
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Reset( void );
	void EXPORT AdvTeleportTouch( CBaseEntity *pOther );
	void EXPORT TeleportThink( void );
	float m_flTimeTel;
	EHANDLE m_hTeleportTarget;
	float m_flArmTime;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CTriggerGoal : public CBaseToggle
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT GoalTouch( CBaseEntity *pOther );
	int m_iTeam; // FGD m_team: 1=Tau'ri 2=Goa'uld
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CTriggerRandom : public CBaseToggle
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT RandomTouch( CBaseEntity *pOther );
	void EXPORT RandomUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT RandomThink( void );
	float m_flMinPeriod;
	float m_flMaxPeriod;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// ---------------------------------------------------------------------------
// item_objective: factory calls GetClassPtr<CItemSecurity> in the .so
// (0xfcb34) — i.e. security-card behaviour with model models/key.mdl (FGD).
// CItemSecurity is local to stock items.cpp, so this derives CItem directly
// and mirrors its MyTouch (ITEM_SECURITY++).
// NOTE: weapon_invis (CInvis) is owned by the weapons slice, not here.
// ---------------------------------------------------------------------------
class CItemObjective : public CItem
{
public:
	void Spawn( void );
	void Precache( void );
	BOOL MyTouch( CBasePlayer *pPlayer );
};

// ---------------------------------------------------------------------------
// env_model: factory calls GetClassPtr<CModel> (0xe30a2). Base is
// CBaseAnimating (Animate needs StudioFrameAdvance/ResetSequenceInfo).
// FGD keys: model, anim, spawnflag 2 = follow target.
// env_particule / env_rain / env_lensflare / env_fountain: particle net
// entities. Key names from rodata (0x1d9b5f..): nombre/id, acceleration,
// v0, ecart, taille, _couleur, vie, bounce, sprite, type (+densite for rain,
// directeur/minAngle/maxAngle/minRadius/maxRadius/minVelocity/maxVelocity for
// fountain, useorigine/origine for lensflare). Wire: Use toggles + SendValue
// (MSG_ALL init) / SendValueTo (MSG_ONE init per client).
// ---------------------------------------------------------------------------
class CModel : public CBaseAnimating
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT Animate( void );
	int m_iAnim;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CClientFog : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	float m_flStartDist;
	float m_flEndDist;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CEnvParticule : public CPointEntity
{
public:
	void Spawn( void );
	void Activate( void );
	void KeyValue( KeyValueData *pkvd );
	void Reset( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SendValue( void );
	void SendValueTo( CBasePlayer *pPlayer );

	int m_iCount;        // nombre (wire: WRITE_SHORT)
	int m_iId;           // id (wire: WRITE_BYTE); ChgPartSt key
	Vector m_vecAccel;   // acceleration
	Vector m_vecV0;      // v0
	Vector m_vecSpread;  // ecart
	Vector m_vecSize;    // taille (x,y) stored as float, wire: WRITE_COORD
	float m_flColor[4];  // _couleur (r,g,b,a) wire: 4x WRITE_BYTE
	float m_flLife;      // vie
	float m_flBounce;    // bounce
	int m_iSprite;       // sprite
	int m_iType;         // type 1=rain 2=snow
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CEnvRain : public CEnvParticule
{
public:
	void KeyValue( KeyValueData *pkvd );
	void SendValue( void );
	void SendValueTo( CBasePlayer *pPlayer );
	float m_flDensity; // densite
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CEnvLensFlare : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void SendValue( void );
	void SendValueTo( CBasePlayer *pPlayer );
	Vector m_vecColor;
	BOOL m_bUseOrigin;
	Vector m_vecOrigin;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CEnvFountain : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SendValue( void );
	void SendValueTo( CBasePlayer *pPlayer );
	int m_iId;           // ref this+0x208
	int m_iCount;        // ref this+0x20c nombre
	float m_flMinAngle, m_flMaxAngle;   // +0x210,+0x214
	float m_flMinVel, m_flMaxVel;       // +0x218,+0x21c
	float m_flMinRadius, m_flMaxRadius; // +0x220,+0x224
	Vector m_vecDir;     // directeur +0x228
	Vector m_vecOrigine; // origine +0x234
	Vector m_vecSize;    // taille +0x240
	float m_flColor[4];  // +0x248
	float m_flLife;      // +0x258
	float m_flBounce;    // +0x25c
	int m_iSprite;       // +0x260
	int m_iType;         // +0x264
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// mode_relay (FGD: mode_a..f + triggerstate + fireonnew) and ent_spawn helper.
class CModeRelay : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iModes[6];
	int m_iTriggerState;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

#endif // SGTC_RULES_H
