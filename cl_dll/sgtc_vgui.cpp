// sgtc_vgui.cpp — StargateTC viewport routing + screen-status API.
//
// Port of the TFC VGUI analogues the SGTC client.dll needs
// (TeamFortressViewport team/class menus, ScorePanel, MOTD window,
// SpectatorPanel) onto the hlsdk base.
//
// ROUTING (ref MsgFunc_VGUIMenu @0x1004933b / ShowVGUIMenu @0x10047b39):
//   The real TeamFortressViewport (gViewPort) owns the stock TFC screens —
//   menu 2 team, 3 class, 4 map briefing, 5 MOTD, 6 class help, 9 spec help —
//   and hosts the two SGTC-only panels: 10 briefing/objective
//   (sgtc_vgui_panels.cpp CSgtcBriefingPanel) and 11 "Identifiez-vous"
//   team+class select (CSgtcTeamClassPanel). This viewport forwards to it and
//   only falls back to console stubs when there is no viewport (VGUI-less
//   client, e.g. hlsdk-xash3d whose false_vgui has no Label/Button/ScrollPanel/
//   TextPanel and no TeamFortressViewport at all).
//
// STATUS (stub vs live):
//   Root/viewport panel ....... LIVE  — engine VGUI root host + paint hook;
//                                the widget-less build keeps a drawing-less
//                                Panel that only carries the protocol handlers.
//   Team/class/menu 2-9 ...... LIVE  — delegated to the real viewport when it
//                                exists, else console stub + CHudMenu fallback.
//   Briefing (10) / team+class (11)  LIVE — SGTC panels via the viewport,
//                                else console stub.
//   ScorePanel ................ LIVE  — CHudScoreboard hooks ScoreInfo/
//                                TeamInfo/TeamScore directly.
//   MOTD / map briefing ....... LIVE  — CHudMOTD draws motd.txt text.
//   SpectatorPanel ............ LIVE  — CHudSpectator + Spec_Mode titles.
//   CommandMenu ............... STUB  — commandmenu.txt parsed only for
//                                console echo; no CCommandMenu widgets.
//   Server browser ............ STUB  — no browser on the SGTC client.
//   Gate/DHD overlay .......... STUB  — no client-side VGUI screens for the
//                                gate; DHD UI is world/model + ShowMenu.
//
// NEW file — sgtc_* only, no stock edits.
// Headers used: hud.h, cl_util.h, parsemsg.h, sgtc_vgui.h
//               (-> VGUI_Panel.h, VGUI_App.h + VGUI transitive set) and, when
//               the tree ships the widget stack, vgui_TeamFortressViewport.h

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "sgtc_vgui.h"
#include "sgtc_hud.h"

// The real TFC viewport (gViewPort) only exists in the tree that ships the
// widget stack — see SGTC_REAL_VGUI in sgtc_vgui.h.
#ifdef SGTC_REAL_VGUI
#include "vgui_TeamFortressViewport.h"
#endif

#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// routing helpers — ref MsgFunc_VGUIMenu @0x1004933b / ShowVGUIMenu @0x10047b39
// ---------------------------------------------------------------------------

// Menu ids the stock TeamFortressViewport implements itself (ref ShowVGUIMenu
// cases 2,3,4,5,6,9 → team, class, map briefing, MOTD, class help, spec help).
static bool SgtcVgui_IsStockMenu( int menu )
{
	return menu == SGTC_MENU_TEAM || menu == SGTC_MENU_CLASS
		|| menu == SGTC_MENU_MAPBRIEFING || menu == SGTC_MENU_INTRO
		|| menu == SGTC_MENU_CLASSHELP || menu == SGTC_MENU_SPECHELP;
}

// Menu ids this port knows about: the stock TFC set (2..9) plus the SGTC-only
// ids 10 (briefing), 11 (team+class), 12 (weapon loadout) and 60 (pack select).
static bool SgtcVgui_IsKnownMenu( int menu )
{
	return ( menu >= SGTC_MENU_DEFAULT && menu <= SGTC_MENU_WPSEL )
		|| menu == SGTC_MENU_PACKSEL;
}

// Hand a menu id to the real viewport. Returns true when it owns the id (even
// if it decided not to show anything); false → caller uses the console stubs,
// i.e. the VGUI-less client.
static bool SgtcVgui_RouteToViewport( int menu )
{
#ifdef SGTC_REAL_VGUI
	if ( !gViewPort )
		return false;

	if ( SgtcVgui_IsStockMenu( menu ) )
	{
		// A stock menu (MOTD/team/class/briefing) is replacing whatever SGTC
		// panel was up — drop every SGTC singleton so none ghost underneath.
		SgtcPanels_HideAll();
		gViewPort->ShowVGUIMenu( menu );
		return true;
	}

	switch ( menu )
	{
	case SGTC_MENU_BRIEFING:
		return SgtcBriefing_ShowPanel() != NULL;
	case SGTC_MENU_TEAMCLASS:
		return SgtcTeamClass_ShowPanel() != NULL;
	case SGTC_MENU_WPSEL:
		return SgtcWeaponSel_ShowPanel() != NULL;
	case SGTC_MENU_PACKSEL:
		return SgtcPackSel_ShowPanel() != NULL;
	default:
		break;
	}
#endif
	return false;
}

// ---------------------------------------------------------------------------
// globals
// ---------------------------------------------------------------------------

CSgtcViewport *g_pSgtcViewport = NULL;

// Fallback names for jointeam 1-5 (client.dll has "jointeam %d" and the
// engine sends TeamNames for the display text; these match mod order).
static const char *s_sgtcTeamNames[SGTC_TEAM_AUTO + 1] = {
	"",			// 0 unused
	"Torri",		// SGTC_TEAM_TORRI
	"Marine",		// SGTC_TEAM_MARINE
	"Goa'uld",		// SGTC_TEAM_GOAULD
	"Unas",			// SGTC_TEAM_UNAS
	"Auto/Spectate"		// SGTC_TEAM_AUTO
};

// ---------------------------------------------------------------------------
// CSgtcViewport — Panel-derived root; only paintBackground is real on Xash
// ---------------------------------------------------------------------------

CSgtcViewport::CSgtcViewport( int x, int y, int wide, int tall )
	: Panel( x, y, wide, tall )
{
	g_pSgtcViewport = this;
	m_iCurrentMenu = SGTC_MENU_DEFAULT;
	m_iNumberOfTeams = SGTC_MAX_TEAMS;
	memset( m_sTeamNames, 0, sizeof( m_sTeamNames ) );
	memset( m_iValidClasses, 0, sizeof( m_iValidClasses ) );
	for ( int i = 1; i <= SGTC_TEAM_AUTO; i++ )
		strncpy( m_sTeamNames[i], s_sgtcTeamNames[i], SGTC_MAX_TEAMNAME - 1 );
}
// HUD_GetRect() is defined in cdll_int.cpp (stock file — we don't edit it);
// declare it so this translation unit links against the existing helper.
extern int *HUD_GetRect( void );


void CSgtcViewport::paintBackground()
{
	// Same hook as the stock xash TeamFortressViewport: let the engine paint
	// the world behind HUD panels. Safe on false_vgui (virtual, engine call).
	gEngfuncs.VGui_ViewportPaintBackground( HUD_GetRect() );
}

// ---------------------------------------------------------------------------
// menu dispatch — the real viewport owns the stock TFC screens and hosts the
// two SGTC-only panels; the console stubs below are the VGUI-less fallback.
// ---------------------------------------------------------------------------

void CSgtcViewport::ShowVGUIMenu( int menu )
{

	m_iCurrentMenu = menu;

	// Ref ShowVGUIMenu @0x10047b39.
	if ( SgtcVgui_RouteToViewport( menu ) )
		return;

	switch ( menu )
	{
	case SGTC_MENU_TEAM:
		gEngfuncs.Con_Printf(
			"[sgtc] Team select — jointeam 1=Torri 2=Marine 3=Goa'uld 4=Unas 5=Auto. "
			"VGUI buttons stubbed; use \"jointeam <n>\" or the ShowMenu text menu.\n" );
		break;
	case SGTC_MENU_CLASS:
		gEngfuncs.Con_Printf(
			"[sgtc] Class select — VGUI class panel stubbed; server ShowMenu "
			"(CHudMenu text menu) is the live path. See info/<class>.txt for blurbs.\n" );
		break;
	case SGTC_MENU_MAPBRIEFING:
		gEngfuncs.Con_Printf( "[sgtc] Map briefing — showing MOTD text (VGUI briefing stubbed).\n" );
		// fall through to MOTD display path
		m_iCurrentMenu = SGTC_MENU_INTRO;
		break;
	case SGTC_MENU_INTRO:
		// MOTD window — live CHudMOTD draws it; just note the stub window.
		if ( gHUD.m_MOTD.m_bShow )
			gEngfuncs.Con_Printf( "[sgtc] MOTD on HUD (VGUI MOTD window stubbed).\n" );
		break;
	case SGTC_MENU_CLASSHELP:
	case SGTC_MENU_SPECHELP:
		gEngfuncs.Con_Printf( "[sgtc] Help panel stubbed (VGUI-less).\n" );
		break;
	case SGTC_MENU_BRIEFING:
		// Ref: briefing/objective window (FUN_1003f640) — needs the VGUI stack.
		gEngfuncs.Con_Printf( "[sgtc] Briefing/objective window stubbed (VGUI-less); "
			"objectives live in maps/<map>.txt.\n" );
		break;
	case SGTC_MENU_TEAMCLASS:
		// Ref: "Identifiez-vous" team+class select (FUN_1002bec0).
		gEngfuncs.Con_Printf( "[sgtc] Team/class select stubbed (VGUI-less); "
			"use \"jointeam <n>\" / \"classe <n>\" / autopick / spectate.\n" );
		break;
	case SGTC_MENU_WPSEL:
		// Ref: weapon loadout (FUN_100261c0, apply = "gond4h9wp <28 values>").
		gEngfuncs.Con_Printf( "[sgtc] Weapon loadout stubbed (VGUI-less); "
			"the server applies \"gond4h9wp <28 values>\" at spawn.\n" );
		break;
	case SGTC_MENU_PACKSEL:
		// Ref: pack select (FUN_10029ea0).
		gEngfuncs.Con_Printf( "[sgtc] Pack select stubbed (VGUI-less).\n" );
		break;
	default:
		break;
	}
}

void CSgtcViewport::HideVGUIMenu()
{
#ifdef SGTC_REAL_VGUI
	// Ref viewport vtable slot: remove every VGUI menu, which includes the SGTC
	// panels hosted above (the server sends HideVGUI on round restart).
	if ( gViewPort )
		gViewPort->HideVGUIMenu();
	// Drop every SGTC singleton too — an orphaned instance not in the current
	// chain survives HideVGUIMenu otherwise.
	SgtcPanels_HideAll();
#endif
	m_iCurrentMenu = SGTC_MENU_DEFAULT;
}

// ---------------------------------------------------------------------------
// message handlers — same wire format as stock vgui_TeamFortressViewport
// ---------------------------------------------------------------------------

int CSgtcViewport::MsgFunc_VGUIMenu( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int menu = READ_BYTE();

	// Stock TFC ids: the real viewport re-reads the message itself (and stores
	// the map name for the briefing), so hand it the untouched buffer.
	if ( SgtcVgui_IsStockMenu( menu ) )
	{
#ifdef SGTC_REAL_VGUI
		if ( gViewPort )
			return gViewPort->MsgFunc_VGUIMenu( pszName, iSize, pbuf );
#endif
		// VGUI-less fallback: consume the briefing's map name like the stock
		// handler does, then let ShowVGUIMenu print its stub.
		if ( menu == SGTC_MENU_MAPBRIEFING )
			READ_STRING();
	}

	// Known ids only: 1..12 (stock 2..9 + SGTC 10 briefing, 11 team+class,
	// 12 weapon loadout) plus 60 (pack select). Unknown ids are dropped like the
	// stock handler's default branch does.
	if ( !SgtcVgui_IsKnownMenu( menu ) )
		return 1;

	ShowVGUIMenu( menu );
	return 1;
}

int CSgtcViewport::MsgFunc_TeamNames( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iNumberOfTeams = READ_BYTE();

	for ( int i = 0; i < m_iNumberOfTeams && i <= SGTC_TEAM_AUTO; i++ )
	{
		int teamNum = i + 1;
		gHUD.m_TextMessage.LocaliseTextString(
			READ_STRING(), m_sTeamNames[teamNum], SGTC_MAX_TEAMNAME );
	}

	// ref FUN_1004910b tail @0x10049231: after the team strings the message
	// always carries 28 cost BYTEs (READ_BYTE ×28) → DAT_100dd974, the weapon
	// cost table the menu-12 panel reads. The shipped server pads them with
	// zeros (ref InitHUD @0x190b54), so every cost is 0 in the orig too.
	unsigned char szCosts[28];
	for ( int i = 0; i < 28; i++ )
		szCosts[i] = (unsigned char)READ_BYTE();
	SgtcWepSel_SetCosts( szCosts );
	return 1;
}

int CSgtcViewport::MsgFunc_ValClass( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	for ( int i = 0; i < 5; i++ )
		m_iValidClasses[i] = READ_SHORT();
	return 1;
}

// ---------------------------------------------------------------------------
// attach/detach — same shape as stock HUD_VidInit block
// ---------------------------------------------------------------------------

void SgtcVgui_AttachToRoot( void )
{
	vgui::Panel *root = (vgui::Panel *)gEngfuncs.VGui_GetPanel();
	if ( !root )
	{
		// Normal on Xash (no vgui.dll host). Viewport still exists for the
		// protocol handlers; it just has no parent and draws nothing.
		gEngfuncs.Con_Printf( "[sgtc] VGUI root panel absent (VGUI-less client); menus stubbed to HUD.\n" );
		if ( !g_pSgtcViewport )
			g_pSgtcViewport = new CSgtcViewport( 0, 0, 0, 0 );
		return;
	}

	if ( g_pSgtcViewport )
	{
		g_pSgtcViewport->setSize( root->getWide(), root->getTall() );
	}
	else
	{
		g_pSgtcViewport = new CSgtcViewport( 0, 0, root->getWide(), root->getTall() );
		g_pSgtcViewport->setParent( root );
	}
}

void SgtcVgui_Detach( void )
{
	delete g_pSgtcViewport;
	g_pSgtcViewport = NULL;
}

// ---------------------------------------------------------------------------
// team helpers
// ---------------------------------------------------------------------------

const char *SgtcTeamName( int team )
{
	if ( team >= SGTC_TEAM_TORRI && team <= SGTC_TEAM_AUTO )
		return g_pSgtcViewport && g_pSgtcViewport->m_sTeamNames[team][0]
			? g_pSgtcViewport->m_sTeamNames[team]
			: s_sgtcTeamNames[team];
	return "Unknown";
}

void Sgtc_JoinTeam( int team )
{
	if ( team < SGTC_TEAM_TORRI || team > SGTC_TEAM_AUTO )
		return;
	char cmd[32];
	snprintf( cmd, sizeof( cmd ), "jointeam %d", team );
	ClientCmd( cmd );
}

// ---------------------------------------------------------------------------
// stub-vs-live status table — the acceptance report
// ---------------------------------------------------------------------------

const char *SgtcVgui_ScreenStatus( const char *screen )
{
#ifdef SGTC_REAL_VGUI
	// With the real viewport up the stock TFC screens and the two SGTC panels
	// are drawn by the widget stack (see SgtcVgui_RouteToViewport).
	if ( gViewPort )
	{
		if ( !stricmp( screen, "team" ) || !stricmp( screen, "class" )
			|| !stricmp( screen, "motd" ) || !stricmp( screen, "briefing" )
			|| !stricmp( screen, "teamclass" ) || !stricmp( screen, "wpsel" )
			|| !stricmp( screen, "packsel" ) )
			return "live";
	}
#endif
	if ( !stricmp( screen, "team" ) )		return "stub";	// VGUI buttons → jointeam console
	if ( !stricmp( screen, "class" ) )		return "live";	// ShowMenu/CHudMenu text menu
	if ( !stricmp( screen, "score" ) )		return "live";	// CHudScoreboard
	if ( !stricmp( screen, "motd" ) )		return "live";	// CHudMOTD
	if ( !stricmp( screen, "spectator" ) )	return "live";	// CHudSpectator
	if ( !stricmp( screen, "briefing" ) )	return "stub";	// menu 10 — VGUI-only panel
	if ( !stricmp( screen, "teamclass" ) )	return "stub";	// menu 11 — VGUI-only panel
	if ( !stricmp( screen, "wpsel" ) )	return "stub";	// menu 12 — VGUI-only panel
	if ( !stricmp( screen, "packsel" ) )	return "stub";	// menu 60 — VGUI-only panel
	if ( !stricmp( screen, "commandmenu" ) )	return "stub";	// commandmenu.txt echo only
	if ( !stricmp( screen, "serverbrowser" ) )return "stub";
	if ( !stricmp( screen, "gate" ) )		return "stub";	// no client VGUI gate screens
	// ref CProgressPanel @0x1002a9ca has no SPR_Draw*; chip paint = CHudSgtc HUD bridge (sgtc_hud.cpp).
	if ( !stricmp( screen, "progress" ) )	return "live";
	return "unknown";
}

void SgtcVgui_DumpStatus( void )
{
	static const char *screens[] = {
		"team", "class", "score", "motd", "spectator", "progress",
		"briefing", "teamclass", "wpsel", "packsel",
		"commandmenu", "serverbrowser", "gate", NULL
	};
	gEngfuncs.Con_Printf( "[sgtc] VGUI screen status (live=draws in Xash, stub=false_vgui no-op):\n" );
	for ( int i = 0; screens[i]; i++ )
		gEngfuncs.Con_Printf( "  %-14s : %s\n", screens[i], SgtcVgui_ScreenStatus( screens[i] ) );
}
