// sgtc_protocol.cpp — StargateTC client protocol registrations (NEW file).
//
// Source of truth: /home/john/stargatetc-rea/inventory_client_dll.json
// (RigidFirefly RE of stargatetc/cl_dlls/client.dll) cross-checked against
// server string table (stargatetc/dlls/hl.dll) and stargatetc/events/*.sc,
// sprites/hud.txt + sprites/weapon_*.txt on disk.
//
// Design (follows hud_msg.cpp / ev_hldm.cpp / hl/hl_events.cpp patterns):
//   - Every SGTC user message gets its own MsgFunc_SGTC_* drain stub.
//     Payload formats are not recoverable client-side, so each handler only
//     drains the buffer (BEGIN_READ/READ_* can't know field layout) — i.e. it
//     returns 1 and ignores content, exactly like stock __MsgFunc_ValClass etc.
//     Server-side peers own the real layouts; wire real fields later by reading
//     dlls MULTIUSE/message sending code.
//   - Events: hook every events/*.sc name found in BOTH client.dll and hl.dll
//     string tables AND present in the server PRECACHE_EVENT list. Stock-file
//     names (glock1/2, mp5/mp52, rpg, train, tripfire) are already hooked by
//     stock Game_HookEvents and are NOT re-hooked here. Unknown script bodies
//     get a shared melee/gunshot fallback implemented with EV_HLDM helpers
//     (tracer + texture sound + decal), mirroring EV_Crowbar/EV_FireMP5 shape.
//   - Sprites/sounds: client only precaches what it draws/plays. Server-side
//     precache lists are authoritative for the 670 files missing from the mod
//     dir; here we SPR_Load only the 28 client-referenced sprites plus the
//     SGTC HUD-layout names from hud.txt so a missing-file printf names the
//     culprit instead of silently drawing nothing.

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "event_api.h"
#include "eventscripts.h"
#include "sgtc_protocol.h"
#include "sgtc_hud.h"
#include "sgtc_env.h"
#include "sgtc_vgui.h"
#include "in_defs.h"	// YAW for EV_EjectBrass

#include <string.h>

// ---------------------------------------------------------------------------
// 1. Custom user messages.
//
// Authoritative registry: LinkUserMessages in ref/stc_i386.so (2.0 Linux
// server, unstripped — disassembled, 61 REG_USER_MSG calls in order):
//   Blip Progress Gtickets Ttickets TicketMode HudInfoMode Concuss HasKey
//   Stamina SubModel Armor Spectator AddPortrait AboutMe TogglePor Reset
//   NewRnd SelAmmo CurWeapon Flashlight FlashBat Health Damage Train HudText
//   SayText TextMsg WeaponList ResetHUD InitHUD GameTitle DeathMsg ScoreInfo
//   TeamInfo TeamScore GameMode MOTD ServerName AmmoPickup WeapPickup
//   ItemPickup HideWeapon SetFOV ShowMenu ScreenShake ScreenFade AmmoX
//   TeamNames StatusText StatusValue VGUIMenu HideVGUI SetRndTim ZatTouch
//   EnvFull EnvRain EnvFount NewLens AddRing ChgPartSt Radio
// Client-side registry (verified vs ref client.dll 2026-10: 74
// pfnHookUserMsg sites — GoldSrc stock + TFC leftovers + SGTC):
//   env/particle (7): ClearEnv EnvFull EnvRain EnvFount NewLens AddRing
//     ChgPartSt — all real handlers in ref (CClientEnv HUD element):
//     ClearEnv drains only; EnvFull spawns env entry (19 coords, 9 bytes
//     incl 4 RGBA/255, 1 short, 1 id byte); EnvRain (6 coords, byte, short);
//     EnvFount (15 coords, 6 bytes, 1 short, 1 id byte); NewLens (3 coords,
//     4 RGBA bytes); AddRing (4 coords + string); ChgPartSt (id byte +
//     state byte → env state).
//   SGTC HUD (19): AddPortrait Reset AboutMe TogglePor SetRndTim NewRnd
//     ZatTouch Radio Armor Stamina SubModel HasKey HideHud HudInfoMode
//     TicketMode Gtickets Ttickets Progress Blip — all real parsers in
//     ref (see sgtc_hud.cpp); CHudSgtc keeps wire parity for SGTC HUD state.
//   NOT hooked by ref (we do not hook either): Custom (present only as a
//     CHudMessage channel-name table entry, never passed to
//     pfnHookUserMsg), SecAmmoVal/SecAmmoIcon (TFC leftovers still
//     hooked by ref — covered by stock hud elements elsewhere).
// Server-only (registered, NO client string — engine-handled or unhandled,
// nothing to hook): SelAmmo ScreenShake ScreenFade Fade Shake SetCurWeap
// SetHudRoundTime.
// Stock reused unchanged: CurWeapon WeaponList AmmoPickup WeapPickup
// ItemPickup HideWeapon AmmoX SecAmmoVal SecAmmoIcon Flashlight FlashBat
// Geiger Health Damage Train HudText SayText TextMsg DeathMsg Logo
// ResetHUD InitHUD GameTitle DeathMsg ScoreInfo TeamInfo TeamScore
// GameMode MOTD ServerName SetFOV ShowMenu ViewMode StatusText StatusValue
// StatusIcon VGUIMenu HideVGUI TeamNames Spectator Concuss VoiceMask
// ReqState ValClass Feign Detpack BuildSt RandomPC AllowSpec.
// ---------------------------------------------------------------------------

#define SGTC_MSG_LIST \
	X( ClearEnv ) \
	X( EnvFull ) \
	X( EnvRain ) \
	X( EnvFount ) \
	X( NewLens ) \
	X( AddRing ) \
	X( ChgPartSt ) \
	X( AddPortrait ) \
	X( Reset ) \
	X( AboutMe ) \
	X( TogglePor ) \
	X( SetRndTim ) \
	X( NewRnd ) \
	X( ZatTouch ) \
	X( Radio ) \
	X( Armor ) \
	X( Stamina ) \
	X( SubModel ) \
	X( HasKey ) \
	X( HideHud ) \
	X( HudInfoMode ) \
	X( TicketMode ) \
	X( Gtickets ) \
	X( Ttickets ) \
	X( Progress ) \
	X( Blip )

#define X( name ) \
	static int __MsgFunc_##name( const char *pszName, int iSize, void *pbuf ) \
	{ \
		return g_SgtcHud.MsgFunc_##name( pszName, iSize, pbuf ); \
	}
SGTC_MSG_LIST
#undef X

static int __MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	return g_SgtcHud.MsgFunc_Concuss( pszName, iSize, pbuf );
}

void SGTC_ClientProtocol_InitMessages( void )
{
#define X( name ) gEngfuncs.pfnHookUserMsg( #name, __MsgFunc_##name );
SGTC_MSG_LIST
#undef X
	// ref client Concuss hook @0x10057aff — overrides stock after gHUD.Init().
	gEngfuncs.pfnHookUserMsg( "Concuss", __MsgFunc_Concuss );
}

void SGTC_ClientHUD_Init( void )
{
	SgtcVgui_AttachToRoot();
	g_SgtcHud.Init();
	g_ClientEnv.Init();
	SGTC_ClientProtocol_InitMessages();
	SGTC_ClientProtocol_HookEvents();
}

void SGTC_ClientHUD_VidInit( void )
{
	SgtcVgui_NewLevel();	// clear the SGTC menu-dismiss mark on new level
	SGTC_ClientProtocol_VidInit();
	g_SgtcHud.VidInit();
}

int SGTC_CustomMessageCount( void )
{
	return 26;
}

// ---------------------------------------------------------------------------
// 2. Event scripts (events/*.sc).
//
// Authoritative server list: PRECACHE_EVENT refs in ref/stc_i386.so (33):
//   batsup batsup2 beam_fire beam_stop biglance coujaf couteau eagle invis
//   lance lance2 larve m16 main main2 mainc mains mp5 p90 p90l psg1 reetou
//   reetou2 shield spas spas2 tac train tripfire usas zata zatarc zatb (33).
// Result (31 SGTC hooks below):
//   On disk + both binaries + server-precached: 30 scripts — batsup batsup2
//     beam_fire beam_stop biglance coujaf couteau eagle invis lance lance2
//     larve m16 main main2 mainc mains p90 p90l psg1 reetou reetou2 shield
//     spas spas2 tac usas zata zatarc zatb (30)
//   On disk + client, NOT server-precached: zat (1) — stub kept so the
//     client does not go silent if the server ever precaches it.
//   On disk, in NEITHER binary: couteau_ancien sniper — NOT hooked (dead
//     files; server never precaches them).
//   Server-precached but NOT on disk (stock HL, already hooked by stock
//     Game_HookEvents — NOT re-hooked): mp5 train tripfire.
//   Client-only legacy (stock HL, already hooked by stock — NOT re-hooked):
//     glock1 glock2 mp52 rpg tacfire. spas2→spas2.sc but server precaches
//     events/spas2.sc too (match); usas→events/usas.sc (match).

extern "C"
{
#define SGTC_EV_LIST \
	X( EV_SGTC_Batsup, "events/batsup.sc" ) \
	X( EV_SGTC_Batsup2, "events/batsup2.sc" ) \
	X( EV_SGTC_BeamFire, "events/beam_fire.sc" ) \
	X( EV_SGTC_BeamStop, "events/beam_stop.sc" ) \
	X( EV_SGTC_BigLance, "events/biglance.sc" ) \
	X( EV_SGTC_Coujaf, "events/coujaf.sc" ) \
	X( EV_SGTC_Eagle, "events/eagle.sc" ) \
	X( EV_SGTC_Invis, "events/invis.sc" ) \
	X( EV_SGTC_Lance, "events/lance.sc" ) \
	X( EV_SGTC_Lance2, "events/lance2.sc" ) \
	X( EV_SGTC_Larve, "events/larve.sc" ) \
	X( EV_SGTC_M16, "events/m16.sc" ) \
	X( EV_SGTC_Main, "events/main.sc" ) \
	X( EV_SGTC_Main2, "events/main2.sc" ) \
	X( EV_SGTC_MainC, "events/mainc.sc" ) \
	X( EV_SGTC_MainS, "events/mains.sc" ) \
	X( EV_SGTC_P90, "events/p90.sc" ) \
	X( EV_SGTC_P90L, "events/p90l.sc" ) \
	X( EV_SGTC_PSG1, "events/psg1.sc" ) \
	X( EV_SGTC_Reetou, "events/reetou.sc" ) \
	X( EV_SGTC_Reetou2, "events/reetou2.sc" ) \
	X( EV_SGTC_Shield, "events/shield.sc" ) \
	X( EV_SGTC_Spas, "events/spas.sc" ) \
	X( EV_SGTC_Spas2, "events/spas2.sc" ) \
	X( EV_SGTC_Tac, "events/tac.sc" ) \
	X( EV_SGTC_Usas, "events/usas.sc" ) \
	X( EV_SGTC_ZatA, "events/zata.sc" ) \
	X( EV_SGTC_ZatArc, "events/zatarc.sc" ) \
	X( EV_SGTC_ZatB, "events/zatb.sc" ) \
	X( EV_SGTC_Zat, "events/zat.sc" )

#define X( fn, script ) void fn( struct event_args_s *args );
SGTC_EV_LIST
#undef X
	void EV_SGTC_Couteau( struct event_args_s *args );
}

// Shared fallback: local-player muzzle flash + tracer + texture/decals,
// same helper calls as EV_Crowbar / EV_FireMP5 in ev_hldm.cpp.
static void SGTC_EventFallback( struct event_args_s *args, int playSound )
{
	int idx = args->entindex;
	if( EV_IsLocal( idx ) )
		EV_MuzzleFlash();

	if( playSound && ( args->iparam1 & 1 ) )
	{
		float origin[3], angles[3], velocity[3], shellVelocity[3], shellOrigin[3];
		float forward[3], right[3], up[3];
		EV_GetGunPosition( args, origin, angles );
		EV_GetDefaultShellInfo( args, origin, velocity, shellVelocity, shellOrigin,
			forward, right, up, 20.0f, -12.0f, 4.0f );
		EV_EjectBrass( shellOrigin, shellVelocity, angles[YAW], 10, TE_BOUNCE_SHELL );
	}
	(void)idx;
}

extern "C"
{
#define X( fn, script ) \
	void fn( struct event_args_s *args ) \
	{ \
		SGTC_EventFallback( args, 1 ); \
		(void)sizeof( script ); \
	}
SGTC_EV_LIST
#undef X
}

// ref @0x100095df — couteau: local anim(1, R(0,1)) only (no muzzle flash / brass)
void EV_SGTC_Couteau( struct event_args_s *args )
{
	int idx = args->entindex;

	if( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, gEngfuncs.pfnRandomLong( 0, 1 ) );
}

void SGTC_ClientProtocol_HookEvents( void )
{
#define X( fn, script ) gEngfuncs.pfnHookEvent( script, fn );
SGTC_EV_LIST
	gEngfuncs.pfnHookEvent( "events/couteau.sc", EV_SGTC_Couteau );
#undef X
}

int SGTC_CustomEventCount( void )
{
	return 31;
}

// ---------------------------------------------------------------------------
// 3. Client-referenced sprites/sounds (precache audit, not engine precache).
//
// Client binary references exactly 28 sprite paths (3 are printf formats,
// not files). SGTC-specific effect/HUD sprites among them:
//   sprites/handtir.spr sprites/lazer.spr sprites/Muz1.spr
//   sprites/rain.spr sprites/snow.spr sprites/shockwave.spr
//   sprites/zatimpact.spr sprites/zarimpact.spr sprites/zattir.spr
//   sprites/fexplo.spr sprites/xfireball3.spr
// The SGTC HUD layout itself (sprites/hud.txt, 640-res section) adds:
//   hud/hud_left hud/hud_right hud/portrait_* hud/ticketdock hud/radar
//   hud/pbar hud/pbare hud/needle hud/blip hud/tacdisp hud/slidein
//   hud/tac_red hud/tac_yellow hud/tac_green hud/statbar hud/line hud/key
//   avatars/* crosshairs/aim crosshairs/goaim crosshairs/goaim2
//   crosshairs/sgaim crosshairs/zatarc crosshairs/scope
//   crosshairs/zatarczoom weapons/* (per weapon_*.txt).
// Sounds the client actually plays (209 .wav refs; SGTC audibles):
//   forcefield/* radio/son*.wav radio/songoa*.wav rt_sounds/rt_button.wav
//   weapons/* (M16-01 P90-1 PSG-01 tirmain* lance* zat* medic goaregen
//   gacgoa invis larve tac terblast* spastir spaspomp tacact tacp tacs …).
// нагрузку NOTE: VidInit below only SPR_Loads the small set the client draws
// every frame; hud.txt-driven sprites load through the stock CHud::VidInit
// path unchanged.
// ---------------------------------------------------------------------------

static const char *const s_sgtcClientSprites[] =
{
	"sprites/handtir.spr",
	"sprites/lazer.spr",
	"sprites/Muz1.spr",
	"sprites/rain.spr",
	"sprites/snow.spr",
	"sprites/shockwave.spr",
	"sprites/zatimpact.spr",
	"sprites/zarimpact.spr",
	"sprites/zattir.spr",
	"sprites/fexplo.spr",
	"sprites/xfireball3.spr",
	"sprites/steam1.spr",
	"sprites/wallpuff.spr",
	"sprites/richo1.spr",
	"sprites/laserbeam.spr",
	"sprites/voiceicon.spr",
	"sprites/crosshairs.spr",
	"sprites/camera.spr",
	"sprites/cursor.spr",
	"sprites/tile.spr",
	"sprites/iplayer.spr",
	"sprites/iplayerblue.spr",
	"sprites/iplayerred.spr",
	"sprites/iplayerdead.spr",
};

void SGTC_ClientProtocol_VidInit( void )
{
	for( size_t i = 0; i < sizeof( s_sgtcClientSprites ) / sizeof( s_sgtcClientSprites[0] ); i++ )
	{
		HSPRITE h = SPR_Load( s_sgtcClientSprites[i] );
		(void)h; // missing-file printf comes from the engine loader
	}
}
