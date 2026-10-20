// sgtc_vgui.h — StargateTC VGUI viewport + screen-status API.
//
// Port of the TFC VGUI analogues the SGTC client.dll needs
// (TeamFortressViewport team/class menus, ScorePanel, MOTD window,
// SpectatorPanel) onto the hlsdk base.
//
// Routing (ref MsgFunc_VGUIMenu @0x1004933b / ShowVGUIMenu @0x10047b39):
//   menu 2/3 team+class, 4 map briefing, 5 MOTD, 6 class help, 9 spec help
//     → the real TeamFortressViewport (gViewPort, created by VGui_Startup())
//       whenever the engine has a VGUI root; console stubs only when it is
//       absent (the VGUI-less client).
//   menu 10 briefing/objective, 11 team+class select, 12 weapon loadout and
//     60 pack select are SGTC-only panels (ref ShowVGUIMenu cases 10 / 0xb /
//     0xc / 0x3c) implemented in sgtc_vgui_panels.cpp — also real-VGUI only;
//     the VGUI-less tree keeps the console stubs.
//
// The two build trees differ: hlsdk-portable links the real widget stack
// (Label/Button/ScrollPanel/TextPanel + vgui_*.cpp), hlsdk-xash3d is
// VGUI-less (false_vgui: Panel/App/Scheme stubs only). SGTC_REAL_VGUI is
// derived from the include path so this file stays byte-identical in both.
//
// NEW file — sgtc_* only, no stock edits.
// Direct stub headers used: <VGUI_Panel.h>, <VGUI_App.h>
// (transitive: VGUI.h, VGUI_Dar.h, VGUI_Scheme.h, VGUI_Color.h,
//  VGUI_Cursor.h, VGUI_KeyCode.h, VGUI_MouseCode.h).
#pragma once

#include <VGUI_Panel.h>
#include <VGUI_App.h>

// Real widget stack present? (false_vgui ships Panel/App/Scheme only.)
#if defined(__has_include)
#  if __has_include(<VGUI_Label.h>) && __has_include(<VGUI_Button.h>) \
	&& __has_include(<VGUI_ScrollPanel.h>) && __has_include(<VGUI_TextPanel.h>)
#    define SGTC_REAL_VGUI 1
#  endif
#endif

// Menu ids mirror the server VGUIMenu byte (stock TFC values, kept so the
// SGTC server's VGUIMenu/ShowMenu protocol needs no changes).
#define SGTC_MENU_DEFAULT     1
#define SGTC_MENU_TEAM        2
#define SGTC_MENU_CLASS       3
#define SGTC_MENU_MAPBRIEFING 4
#define SGTC_MENU_INTRO       5
#define SGTC_MENU_CLASSHELP   6
#define SGTC_MENU_SPECHELP    9
// SGTC-only ids (ref TeamFortressViewport::ShowVGUIMenu switch cases 10/0xb).
#define SGTC_MENU_BRIEFING    10	// briefing/objective window (FUN_1003f640)
#define SGTC_MENU_TEAMCLASS   11	// "Identifiez-vous" team+class select (FUN_1002bec0)
#define SGTC_MENU_WPSEL       12	// weapon loadout (ref case 0xc -> FUN_10047ebb)
#define SGTC_MENU_PACKSEL     60	// pack select (ref case 0x3c -> FUN_10047fb8)

// SGTC teams: jointeam 1-4 + 5 = auto/spectate (client.dll strings:
// "jointeam 1".."jointeam 5", "jointeam %d").
#define SGTC_TEAM_TORRI   1
#define SGTC_TEAM_MARINE  2
#define SGTC_TEAM_GOAULD  3
#define SGTC_TEAM_UNAS    4
#define SGTC_TEAM_AUTO    5
#define SGTC_MAX_TEAMS    4
// ref m_sTeamNames stride is 0x10 bytes (v1 this+i*0x40 uses 0x40 per team for
// names+slots; 2.0 uses +0x10/entry) — 16 bytes per name, matching ref caps.
#define SGTC_MAX_TEAMNAME 16

// Team ids our SGTC server accepts for "jointeam" — mirror of the server's
// dlls/sgtc_rules.h (SGTC_TEAM_TORRI 1 / SGTC_TEAM_GOAULD 2) as handled by
// CStargateTeamDM::ClientCommand (dlls/sgtc_rules.cpp:1151): 1 = Torri,
// 2 = Goauld, 5 = auto (TeamWithFewestPlayers), anything else = "Teams
// Unbalanced". The SGTC_TEAM_* block above is the stock-TFC numbering
// (Goauld = 3) and is NOT what the server accepts — menu buttons use these.
#define SGTC_SRV_TEAM_TORRI   1
#define SGTC_SRV_TEAM_GOAULD  2
#define SGTC_SRV_TEAM_AUTO    5

class CSgtcViewport : public vgui::Panel
{
public:
	CSgtcViewport( int x, int y, int wide, int tall );

	// Shows a VGUIMenu id: stock TFC ids go to the real viewport when it
	// exists, 10/11 to the SGTC panels; the console stubs below are the
	// VGUI-less fallback (see SgtcVgui_ScreenStatus).
	void ShowVGUIMenu( int menu );
	void HideVGUIMenu();

	// Message handlers — same wire format as stock TeamFortressViewport.
	int MsgFunc_VGUIMenu( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamNames( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ValClass( const char *pszName, int iSize, void *pbuf );

	virtual void paintBackground();

	int m_iCurrentMenu;
	int m_iNumberOfTeams;
	char m_sTeamNames[SGTC_TEAM_AUTO + 1][SGTC_MAX_TEAMNAME];
	int m_iValidClasses[5];
};

extern CSgtcViewport *g_pSgtcViewport;

// --- SGTC-only VGUI panels (sgtc_vgui_panels.cpp) ---------------------------
// Ref menu 10 = briefing/objective window (FUN_1003f640, server sends it 3s
// after connect: sgtc_round.cpp Think → ShowVGUIMenu(10)); ref menu 11 =
// "Identifiez-vous" team+class select (FUN_1002bec0, sent for dead/teamless
// players). Both are CMenuPanel subclasses hosted by the real
// TeamFortressViewport: created once and re-opened per message, the same
// pre-create-and-reuse pattern the ref uses in the viewport ctor.
// Return the panel that was shown, or NULL when the engine has no VGUI root
// (gViewPort == NULL — then sgtc_vgui.cpp's console stubs stay in charge).
class CMenuPanel;
CMenuPanel *SgtcBriefing_ShowPanel( void );		// VGUIMenu 10
CMenuPanel *SgtcTeamClass_ShowPanel( void );		// VGUIMenu 11
CMenuPanel *SgtcWeaponSel_ShowPanel( void );		// VGUIMenu 12
CMenuPanel *SgtcPackSel_ShowPanel( void );		// VGUIMenu 60

// True while any SGTC VGUI menu panel is on screen. Lets the cursor-gate in
// TeamFortressViewport::UpdateCursorState keep the mouse ungrabbed for SGTC
// menus even when gViewPort->m_pCurrentMenu doesn't reference them.
bool SgtcPanels_AnyMenuVisible( void );

// Force-hide every SGTC menu singleton and unlink them from the
// m_pCurrentMenu chain, and clear the user-dismiss mark. Called when the
// server sends HideVGUI (close-all semantics): a hidden chained node would
// otherwise keep deduping the next same-id resend.
void SgtcPanels_HideAll( void );

// --- weapon-selection economy state (ref client.dll globals) ----------------
// The server feeds the menu-12 economy through two usermessages:
//   TeamNames  : BYTE num_teams + STRING names + 28 BYTE costs  → DAT_100dd974
//                (handler FUN_1004910b @0x1004910b, READ_BYTE ×28)
//   Progress   : BYTE nuke + BYTE budget + 28 BYTE slot states + BYTE reset
//                → DAT_100ddad4 / DAT_100dd9e4 / DAT_100ddad8
//                (handler FUN_10073faa @0x10073faa)
//   Armor      : SHORT armor + BYTE mode → DAT_100f225c (FUN_10073e85)
// These are the exact values the ref's CSgtcWeaponSelPanel::Update/Reset read.
void SgtcWepSel_SetCosts( const unsigned char *pCosts );	// 28 bytes (TeamNames tail)
void SgtcWepSel_SetProgress( int iBudget, const unsigned char *pStates, int iReset );	// Progress
void SgtcWepSel_SetArmorMode( int iMode );			// Armor (DAT_100f225c)
void SgtcWepSel_Refresh( void );				// re-open/refresh the panel if alive
// Pack preset table producer (ref FUN_1002b15b): team × class × pack → slots.
// Returns the number of slot indices written into piSlots (max 5).
int SgtcPack_GetSlots( int iTeam, int iClass, int iPack, int *piSlots );

// Create + parent to the engine root panel (no-op when the engine has no
// VGUI root, which is the normal Xash case). Call from HUD_VidInit.
void SgtcVgui_AttachToRoot( void );
void SgtcVgui_Detach( void );

// Per-level reset (called from SGTC_ClientHUD_VidInit): forget the last
// user-dismissed SGTC menu so a connect-time resend isn't suppressed on a
// fresh map. No-op for the VGUI-less build.
void SgtcVgui_NewLevel( void );

// Team helpers (client.dll protocol: "jointeam %d", "menuselect %d").
const char *SgtcTeamName( int team );
void Sgtc_JoinTeam( int team );

// Per-screen live-vs-stub status: returns "live", "stub", or "unknown".
// Screens: "team", "class", "score", "motd", "spectator", "progress",
// "briefing", "teamclass", "wpsel", "packsel", "commandmenu", "serverbrowser",
// "gate".
const char *SgtcVgui_ScreenStatus( const char *screen );
void SgtcVgui_DumpStatus( void );
