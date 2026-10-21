// sgtc_player.cpp — StargateTC CBasePlayer extensions (ref/stc_i386.so).
// RadarScan @0x135592: MSG_ONE gmsgRadarBlip, BYTE type + 2× WRITE_COORD (×0.1).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "shake.h"
#include "sgtc_rules.h"
#include "sgtc_shield.h"
#include "sgtc_monsters.h"
#include "sgtc_lanceproj.h"

extern Vector sgtc_LastKeyPlace;
extern char sgtc_KeyInPlay;
extern void *sgtc_superguns;

// Globals from other TUs (lazy externs, as elsewhere in this file).
extern int gmsgSayText;
extern int gmsgReset;
extern int gmsgArmor;
extern int gmsgSpectator;
extern int gmsgHideVGUI;
extern int gmsgTogglePortrait;
extern cvar_t allow_spectators;
extern int sgtc_GameModeSet;	// sgtc_round.cpp
extern int sgtc_G_tickets;
extern int sgtc_T_tickets;
extern void CopyToBodyQue( entvars_t *pev );

// ref global: Elmo @0x2368f0 — byte flag set to 1 by dropkey's key spawn
// (ref @0xda0dc MOV byte ptr).  Never read; not an edict pointer.
char sgtc_Elmo = 0;
char sgtc_KeyChargeLatch = 0;	// ref DAT_00246866 — DM Think key-block nuke-charge edge latch

static int SGTC_SupergunsHudAux( void )	// REA: ref reads superguns cvar +0xc; read the port struct directly
{
	extern cvar_t sgtc_cvar_superweapons;
	return (int)sgtc_cvar_superweapons.value;
}

static const struct
{
	const char *pszClass;
	int iBlipType;
} s_radarClasses[] =
{
	{ "player",            2 },
	{ "item_objective",    6 },
	{ "game_nuke",         6 },
	{ "info_marker",       5 },
	{ "monster_mine",      1 },
	{ "weapon_c4",         5 },
	{ "monster_tac",       3 },
	{ "larve",             1 },
	{ "struct_goa_actual", 6 },
	{ "struct_tau_actual", 6 },
	{ "grenade",           1 },
	{ "monster_satchel",   4 },
};

// ref Endo_roundto @0x1bb72c: rounds an int to its nearest hundredth — identity
// for integer inputs; ref wraps ROUND(dx*0.1) in it before WRITE_COORD.
static int SGTC_Endo_roundto( int v )
{
	return (int)floor( (double)(int)floor( (double)v * 100.0 + 0.5 ) / 100.0 + 0.5 );
}

static void SGTC_SendRadarBlip( CBasePlayer *pPlayer, int iType, int iDx, int iDy )
{
	if( !gmsgRadarBlip )
		return;
	MESSAGE_BEGIN( MSG_ONE, gmsgRadarBlip, NULL, pPlayer->edict() );
	WRITE_BYTE( iType );
	// ref @0x1456d9: WRITE_COORD(Endo_roundto(ROUND(dx*0.1))) — the wire value is
	// the pre-decimated int (~dx/10); the client READ_COORD+ftols it back.
	// ROUND is FISTP round-nearest (mode 0xc) → lrintf matches for all signs.
	WRITE_COORD( (float)SGTC_Endo_roundto( (int)lrintf( (double)iDx * 0.1 ) ) );
	WRITE_COORD( (float)SGTC_Endo_roundto( (int)lrintf( (double)iDy * 0.1 ) ) );
	MESSAGE_END();
}

static BOOL SGTC_RadarTeamVisible( CBasePlayer *pViewer, CBaseEntity *pEnt )
{
	BOOL entAlive = FALSE;
	if( pEnt->IsPlayer() )
		entAlive = ((CBasePlayer *)pEnt)->IsAlive();
	else
	{
		CBaseMonster *pMon = pEnt->MyMonsterPointer();
		entAlive = ( pMon && pMon->IsAlive() );
	}
	int entTeam = pEnt->pev->team;
	int viewTeam = pViewer->pev->team;

	if( entAlive )
	{
		if( entTeam != viewTeam )
			return FALSE;
	}
	if( entTeam > 0 && viewTeam > 0 && entTeam != viewTeam )
		return FALSE;
	return TRUE;
}

static BOOL SGTC_RadarPassesFilters( CBasePlayer *pPlayer, CBaseEntity *pEnt, int iDx, int iDy, BOOL bZPass )
{
	const int kHalf = 90;		// ref Endo_roundtoi(9*5*2) @0x135604 → Endo_roundto(9*10)
	const int kMaxDist = 929;	// ref Endo_distance3d (Manhattan) < 0x3a2 @0x1457d0

	if( pEnt == pPlayer )
		return FALSE;
	if( abs( iDx ) <= kHalf && abs( iDy ) <= kHalf )
		return FALSE;

	// ref uses Manhattan 3d distance, NOT vecDelta.Length().
	if( abs( iDx ) + abs( iDy ) +
	    abs( (int)( pEnt->pev->origin.z - pPlayer->pev->origin.z ) ) > kMaxDist )
		return FALSE;

	// ref: blip only when the entity passed the info_radar_block Z window
	// (local_d) — entity above player under the nearest overhead blocker, or
	// below player above the nearest underfoot blocker, or exactly level.
	if( !bZPass )
		return FALSE;

	if( !SGTC_RadarTeamVisible( pPlayer, pEnt ) )
		return FALSE;

	// ref pev+0x148 / +0x14c: skip when entity has a future hide-until time (> now).
	if( pEnt->pev->effects & EF_NODRAW )
		return FALSE;
	if( pEnt->pev->dmgtime > gpGlobals->time )
		return FALSE;

	return TRUE;
}

// ref @0x1455f4..0x1456c8 — nearest info_radar_block above the player sets the
// ceiling distance; nearest below sets the floor distance. An entity passes the
// Z window (local_d) when it is above the player but below the blocker, or
// below the player but above the blocker, or exactly level with the player.
static BOOL SGTC_RadarZPass( CBasePlayer *pPlayer, CBaseEntity *pEnt )
{
	float distAbove = 9000.0f;	// ref local_18
	float distBelow = 9000.0f;	// ref local_14

	CBaseEntity *pBlk = NULL;
	while( ( pBlk = UTIL_FindEntityByClassname( pBlk, "info_radar_block" ) ) != NULL )
	{
		const float z = pBlk->pev->origin.z;
		if( pPlayer->pev->origin.z < z && z - pPlayer->pev->origin.z < distAbove )
			distAbove = z - pPlayer->pev->origin.z;
		if( z < pPlayer->pev->origin.z && pPlayer->pev->origin.z - z < distBelow )
			distBelow = pPlayer->pev->origin.z - z;
	}

	const float ez = pEnt->pev->origin.z;
	const float pz = pPlayer->pev->origin.z;
	if( pz < ez && ez - pz < distAbove )
		return TRUE;
	if( ez < pz && pz - ez < distBelow )
		return TRUE;
	if( ez == pz )
		return TRUE;
	return FALSE;
}

// ref CBasePlayer::RadarScan @0x135592
void CBasePlayer::SGTC_RadarScan( void )
{
	if( gpGlobals->time < m_flRadarNext )
		return;

	if( !IsAlive() || pev->deadflag != DEAD_NO || IsObserver() )
	{
		m_flRadarNext = gpGlobals->time + 0.25f;
		return;
	}

	// NOTE: ref has NO viewer-team early-out — a live player on team 0 still
	// scans; the team check lives in SGTC_RadarTeamVisible (alive-entity gate).

	const int nClasses = (int)( sizeof( s_radarClasses ) / sizeof( s_radarClasses[0] ) );
	for( int i = 0; i < nClasses; i++ )
	{
		const char *pszClass = s_radarClasses[i].pszClass;
		const int iType = s_radarClasses[i].iBlipType;

		CBaseEntity *pEnt = NULL;
		while( ( pEnt = UTIL_FindEntityByClassname( pEnt, pszClass ) ) != NULL )
		{
			const int iDx = (int)lrintf( pEnt->pev->origin.x - pev->origin.x ); // ref FISTP round-nearest
			const int iDy = (int)lrintf( pEnt->pev->origin.y - pev->origin.y );

			if( !SGTC_RadarPassesFilters( this, pEnt, iDx, iDy,
				SGTC_RadarZPass( this, pEnt ) ) )
				continue;

			SGTC_SendRadarBlip( this, iType, iDx, iDy );
		}
	}

	m_flRadarNext = gpGlobals->time + 0.25f;
}

// ref @0x133aca — toggle +0x925, MSG_ONE gmsgArmor: 0,0,mode,iMaxAmmo1().
void CBasePlayer::SGTC_WeapDispSet( int mode )
{
	if( mode == 1 )
		m_bWeapSubModel = 1;
	else if( mode == 2 )
		m_bWeapSubModel = 0;

	if( !gmsgArmor )
		return;

	int iMaxAmmo = 0;
	if( m_pActiveItem )
		iMaxAmmo = ((CBasePlayerWeapon *)m_pActiveItem)->iMaxAmmo1();

	MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( mode );
	WRITE_BYTE( iMaxAmmo );
	MESSAGE_END();
}

// ref @0x1336e8 — MSG_ONE gmsgStamina when m_iStamina (+0x43c) != m_iStaminaSent (+0xa24).
void CBasePlayer::SGTC_UpdateStamina( void )
{
	if( m_iStamina < 0 )
		m_iStamina = 0;

	if( m_iStamina == m_iStaminaSent )
		return;

	if( !gmsgStamina )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgStamina, NULL, pev );
	WRITE_BYTE( m_iStamina );
	MESSAGE_END();
	m_iStaminaSent = m_iStamina;
}

// ref @0x1390b2 — shield charge, spawn effect_shield, follow owner.
void CBasePlayer::SGTC_ShieldThinker( void )
{
	if( m_flShieldLatch == 0.0f )
		m_flShieldLatch = gpGlobals->time - 1.0f;	// ref @0x1390e6

	if( gpGlobals->time >= m_flShieldThinkNext )
	{
		if( m_iShieldCharge > 0 && ( m_bShieldWall || m_bShieldSprint ) )
		{
			if( gpGlobals->frametime < 1.0f )
				m_iShieldCharge--;
		}
		else
		{
			m_bShieldSprint = 0;
			m_bShieldWall = 0;
		}

		if( m_iShieldCharge <= 9 && !m_bShieldWall && !m_bShieldSprint )
			m_iShieldCharge++;

		m_flShieldThinkNext = gpGlobals->time + 1.5f;	// ref rodata @0x1d74c4
	}

	if( !m_bShieldWall )
		return;

	if( !m_bShieldSpawned )
	{
		CBaseEntity *pShield = CBaseEntity::Create( "effect_shield", pev->origin, pev->v_angle, edict() );
		if( !pShield )
			return;

		pShield->pev->owner = edict();
		m_bShieldSpawned = 1;

		if( m_bShieldLarge )
		{
			pShield->pev->scale = 2.0f;
			pShield->pev->renderamt = 100.0f;
			pShield->pev->framerate = 1.0f;
		}
		return;
	}

	BOOL bFound = FALSE;
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = UTIL_FindEntityByClassname( pEnt, "effect_shield" ) ) != NULL )
	{
		if( pEnt->pev->owner != edict() )
			continue;

		pEnt->pev->origin = pev->origin;
		pEnt->pev->origin.z -= 40.0f;	// ref @0x1d74dc
		if( m_flShieldLatch <= gpGlobals->time )
			pEnt->pev->renderamt = 130.0f;
		else
			pEnt->pev->renderamt = 100.0f;

		((CShield *)pEnt)->Think();
		bFound = TRUE;
	}

	m_bShieldSpawned = bFound;
}

// ref @0x13a02d..0x13a0da — drain 10 stamina (+0x43c); latch always +0.5 @0x13a150.
static void SGTC_TickMoveLatchAndStaminaDrain( CBasePlayer *p )
{
	if( gpGlobals->time < p->m_flMoveLatch )
		return;

	if( !( p->pev->flags & FL_FROZEN ) && p->m_flMaxSpeedLatch != 0.0f
		&& p->pev->fuser2 == p->m_flMaxSpeedLatch )
	{
		const float move = p->pev->velocity.x + p->pev->velocity.y + p->pev->velocity.z;
		if( move > 0.0f )
		{
			p->m_iStamina -= 10;
			p->SGTC_UpdateStamina();
		}
	}

	p->m_flMoveLatch = gpGlobals->time + 0.5f;	// ref rodata @0x1d755c
}

// ref @0x13a15c..0x13a3a5 — pev+0x14c renderamt shield energy, pev+0x210 maxspeed.
void CBasePlayer::SGTC_TickShieldEnergyAndMaxspeed( void )
{
	static const float kShieldDrain = 5.0f;		// rodata @0x1d7558
	static const float kShieldCap = 255.0f;		// rodata @0x1d7540
	static const float kRunSpeed = 180.0f;		// rodata @0x1d7564
	static const float kLowStaminaSpeed = 10.0f;	// rodata @0x1d7554
	static const float kZatVelZ = -20.0f;		// rodata @0x1d7560
	static const float kLatchMoveMin = 0.125f;	// rodata @0x1d7528

	if( m_bZatTouchFlag && pev->velocity.z <= 0.0f )
		pev->velocity.z = kZatVelZ;

	if( m_bZatTouchFlag || m_bShieldTraceHit )
		return;

	if( m_bShieldSprint )
	{
		if( pev->renderamt > 0.0f )
			pev->renderamt -= kShieldDrain;
	}
	else
	{
		if( pev->renderamt < kShieldCap )
		{
			if( pev->renderamt > 0.0f && gpGlobals->time >= m_flGateTravel )
				pev->renderamt += kShieldDrain;
		}
		else
		{
			pev->renderamt = kShieldCap;
			pev->rendermode = 0;	// ref pev+0x148
			pev->renderfx = 0;	// ref pev+0x15c
		}
	}

	if( m_flMaxSpeedLatch > kLatchMoveMin )
		return;

	if( pev->flags & FL_ONGROUND )
	{
		if( pev->maxspeed != kRunSpeed )
			pev->maxspeed = kRunSpeed;
	}
	else
		pev->maxspeed = m_flMaxSpeedLatch;

	if( m_iStamina <= 19 )
		pev->maxspeed = kLowStaminaSpeed;
}

// ref @0x13a3a5 — when pev+0x14c < 0 and not sprinting, PointContents → renderamt.
void CBasePlayer::SGTC_SyncShieldEnergyFromContents( void )
{
	if( pev->renderamt >= 0.0f )
		return;
	if( m_bShieldSprint )
		return;

	pev->renderamt = (float)UTIL_PointContents( pev->origin );
}

// ref @0x139f18 — when +0x444/+0x938 differ from +0x45c/+0x934, MSG_ONE Progress.
void CBasePlayer::SGTC_SendProgressIfDirty( void )
{
	if( m_iProgressSentPri == m_iNukeCharge && m_iNukeBonusSent == m_iNukeBonus )
		return;

	if( !gmsgProgress )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgProgress, NULL, pev );
	WRITE_BYTE( m_iNukeCharge );
	WRITE_BYTE( m_iNukeBonus );
	for( int i = 0; i < 28; i++ )
		WRITE_BYTE( m_iProgressTeam[i] );
	WRITE_BYTE( 0 );
	MESSAGE_END();

	m_iProgressSentPri = m_iNukeCharge;
	m_iNukeBonusSent = m_iNukeBonus;
}

// ref @0x1398ef — one-shot camp warn: HasKey(m_iInBombZone, 1).
void CBasePlayer::SGTC_TryBombKeyCampWarn( void )
{
	if( m_bBombKeyWarnSent )
		return;
	if( m_iBombKeyTimer > 0 )
		return;

	const float flNow = gpGlobals->time;
	if( flNow == CVAR_GET_FLOAT( "tourny" ) )
		return;
	if( flNow == CVAR_GET_FLOAT( "allowmodevote" ) )
		return;
	if( g_iSgtcMapType == 1 )
		return;
	if( !gmsgHasKey )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgHasKey, NULL, pev );
	WRITE_BYTE( m_iInBombZone );
	WRITE_BYTE( 1 );
	MESSAGE_END();
	m_bBombKeyWarnSent = 1;
}

// ref @0x13a438 — clamp key count, HasKey on enter/leave bomb zone state.
void CBasePlayer::SGTC_SyncBombZoneHasKey( void )
{
	if( m_iInBombZone > 1 )
		m_iInBombZone = 1;

	BOOL bSend = FALSE;
	if( m_iInBombZone > 0 )
	{
		if( !m_bBombZoneKeyLatch )
			bSend = TRUE;
	}
	else if( m_bBombZoneKeyLatch == 1 )
		bSend = TRUE;

	if( bSend && gmsgHasKey )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHasKey, NULL, pev );
		WRITE_BYTE( m_iInBombZone );
		WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	if( m_iInBombZone > 0 )
	{
		m_bBombZoneKeyLatch = 1;
		sgtc_KeyInPlay = 1;
		sgtc_LastKeyPlace = pev->origin;
	}
	else
		m_bBombZoneKeyLatch = 0;
}

// ref CBasePlayer::CampWarn @0x193a4f (inside DM::Think's decompile).
// arg 0 -> HasKey "3", else "2". Skipped entirely when MapType == 1 (CP).
void CBasePlayer::CampWarn( int arg )
{
	if( g_iSgtcMapType == 1 )
		return;
	if( gmsgHasKey )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHasKey, NULL, pev );
		WRITE_BYTE( arg == 0 ? 3 : 2 );
		WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

// ref ClientCommand @0xcb456 — menuselect arg <=4 arms bomb-key warn timer.
void CBasePlayer::SGTC_MenuselectBombKey( int sel )
{
	if( sel > 4 )
		return;
	m_iBombKeyTimer = sel + 1;
	m_bBombKeyWarnSent = 0;
}

// ref @0x136454 — toggle +0x4e9; on: pev+0x148 = kRenderTransAlpha (4).
void CBasePlayer::SGTC_ToggleVis( void )
{
	if( !m_bShieldSprint )
	{
		pev->rendermode = kRenderTransAlpha;
		m_bShieldSprint = 1;
	}
	else
		m_bShieldSprint = 0;
}

// ref @0x163e88 — CBasePlayer::ChangeSubmodel (real body; nm 00143e88 + thunk
// 0x8e650). MSG_ONE gmsgSubModel: BYTE = m_iSgtcSubmodelIdx (ref +0x440),
// written by PlayerSpawn (0/1/2 by pev->team) before the call.
void CBasePlayer::ChangeSubmodel( void )
{
	if( !gmsgSubModel )
		return;
	MESSAGE_BEGIN( MSG_ONE, gmsgSubModel, NULL, pev );
		WRITE_BYTE( m_iSgtcSubmodelIdx );
	MESSAGE_END();
}

// ref @0x1336b8 — if shield sprint, snap pev+0x14c renderamt to 220.
void CBasePlayer::SnagInvis( void )
{
	if( m_bShieldSprint )
		pev->renderamt = 220.0f;	// rodata @0x1d6f9c
}

// ref @0x13720e — personal shield leg hit from enemy fire.
void CBasePlayer::SGTC_TryShieldTraceHit( entvars_t *pevAttacker, TraceResult *ptr )
{
	if( !pevAttacker || !ptr )
		return;
	if( ptr->iHitgroup != HITGROUP_RIGHTLEG )
		return;
	if( m_bSnagInvisLatch )
		return;
	if( m_bShieldWall )
		return;
	if( pevAttacker->team == pev->team )
		return;

	m_bShieldTraceHit = 1;
	m_flShieldTraceTime = gpGlobals->time;	// ref @0x13726a
}

// ref @0x13e948..0x13eb07 — CheckTimeBasedDamage prologue latches, run before the
// DMG_TIMEBASED loop. Latch1 (+0xa78 shield-trace hit, timer +0xa68): after 3s
// clear flag, renderfx=0, renderamt=255.0, rendercolor=(0,0,0). Latch2
// (+0xa88 zat-touch, timer +0xa8c): same + TakeHealth(10, DMG_DROWNRECOVER).
// Latch3 (+0xa94, timer +0xa98): after 0.5s renderamt=20.0, clear flag.
void CBasePlayer::SGTC_TickShieldTraceLatch( void )
{
	static const float kShieldTraceHold = 3.0f;	// rodata @0x1d2e20
	static const float kLatchA94Hold = 0.5f;	// rodata @0x1d2e30

	if( m_bShieldTraceHit && gpGlobals->time - m_flShieldTraceTime > kShieldTraceHold )
	{
		m_bShieldTraceHit = 0;	// ref +0xa78
		pev->renderfx = 0;	// ref pev+0x15c
		pev->renderamt = 255.0f;	// ref pev+0x14c = 0x437f0000
		pev->rendercolor = Vector( 0, 0, 0 );	// rodata @0x1d2e24/@0x1d2e28
	}

	if( m_bZatTouchFlag && gpGlobals->time - m_flSgtcLatchA8c > kShieldTraceHold )
	{
		m_bZatTouchFlag = 0;	// ref +0xa88
		pev->renderfx = 0;
		pev->renderamt = 255.0f;
		pev->rendercolor = Vector( 0, 0, 0 );
		TakeHealth( 10.0f, DMG_DROWNRECOVER );	// ref +0x3c(this,0x41200000,0x80000)
	}

	if( m_bSgtcLatchA94 && gpGlobals->time - m_flSgtcLatchA98 > kLatchA94Hold )
	{
		m_bSgtcLatchA94 = 0;	// ref +0xa94
		pev->renderamt = 20.0f;	// ref pev+0x14c = 0x41a00000
	}
}

// ref CBasePlayer::Zat @0x14378a (ELF 0x13378a) — zat stun from zat_proj /
// zatboom on players. truezat @0x2368d4 = mp_truezat cvar: while +0xa88 already
// latched, 0 → 80 dmg, non-zero → 2000 dmg; first hit is always 10.
// Ref TakeDamage(pev /*inflictor=self*/, param_1 /*attacker*/, dmg, 0x1000)
// and unconditionally zeroes pev+0x20 (velocity) on exit.
extern cvar_t sgtc_cvar_truezat;
void CBasePlayer::Zat( entvars_t *pevInflictor )
{
	m_bZatLatch = 1;	// ref +0x928

	if( m_bSnagInvisLatch == 1 || m_bShieldWall == 1 )	// ref +0x4de/+0x3e0 == 1 → skip
		goto L_zat_exit;
	if( !pevInflictor || pevInflictor->team == pev->team )	// ref param_1+0x1ac != pev+0x1ac
		goto L_zat_exit;

	{
		float flDmg;
		if( !m_bZatTouchFlag )					// ref +0xa88 == 0 → 10.0
			flDmg = 10.0f;	// rodata @0x1d6fac
		else if( sgtc_cvar_truezat.value == 0.0f )	// ref truezat == 0 → 80.0
			flDmg = 80.0f;	// rodata @0x1d6fa4
		else
			flDmg = 2000.0f;	// rodata @0x1d6fa8

		TakeDamage( pev, pevInflictor, flDmg, DMG_NEVERGIB );	// ref vfunc+0x38 bits 0x1000

		if( m_bZatTouchFlag != 1 && gmsgZatTouch )	// ref +0xa88 != 1
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgZatTouch, NULL, pev );
			MESSAGE_END();
		}

		m_bZatTouchFlag = 1;				// ref +0xa88
		m_flSgtcLatchA8c = gpGlobals->time;	// ref +0xa8c

		pev->renderfx = kRenderFxGlowShell;	// ref pev+0x15c = 0x13
		pev->renderamt = 2.0f;	// ref pev+0x14c = 0x40000000
		pev->rendercolor.x = 25.0f;	// 0x41c80000
		pev->rendercolor.y = 10.0f;	// 0x41200000
		pev->rendercolor.z = 255.0f;	// 0x437f0000
		m_bShieldSprint = 0;	// ref +0x4e9
		SnagInvis();
		pev->maxspeed = 50.0f;	// 0x42480000

		m_iStamina -= 60;
		if( m_iStamina < 0 )
			m_iStamina = 0;
		SGTC_UpdateStamina();

		UTIL_ScreenFade( this, Vector( 0, 138, 179 ), 0.5f, 1.0f, 100, 0 );	// ref local_28
	}

L_zat_exit:
	pev->velocity = g_vecZero;	// ref unconditional tail: pev+0x20 = (0,0,0)
}

// ref helper @0x1abc1c — Manhattan 3d distance as int.
static int SGTC_Endo_distance3d( const Vector &a, const Vector &b )
{
	return (int)fabs( a.x - b.x ) + (int)fabs( a.y - b.y ) +
		(int)fabs( a.z - b.z );
}

// ref @0x139c13 — team spawn info proximity + camp latch (+0x4a4/+0x4a8/+0x4b4).
void CBasePlayer::SGTC_TickSpawnCampProximity( void )
{
	static const float kProxExtendSec = 1.0f;	// rodata @0x1d7548
	static const float kCampTouchSec = 0.3f;	// rodata @0x1d754c
	static const int kNearSpawnDist = 0xc7;
	static const int kCampDist = 0xc8;

	const char *pszSpawnClass = NULL;
	if( pev->team == 1 )
		pszSpawnClass = "info_player_goauld";
	else if( pev->team == 2 )
		pszSpawnClass = "info_player_torri";
	else
		return;

	CBaseEntity *pSpawn = NULL;
	while( ( pSpawn = UTIL_FindEntityByClassname( pSpawn, pszSpawnClass ) ) != NULL )
	{
		if( SGTC_Endo_distance3d( pev->origin, pSpawn->pev->origin ) <= kNearSpawnDist )
		{
			m_flSpawnProxDeadline = gpGlobals->time + kProxExtendSec;
			break;
		}
	}

	const int iDistRef = SGTC_Endo_distance3d( pev->origin, m_vecSpawnCampRef );
	BOOL bCampTick = FALSE;
	if( iDistRef > kCampDist )
		bCampTick = TRUE;
	else if( gpGlobals->time >= m_flSpawnProxDeadline )
		bCampTick = TRUE;

	if( bCampTick )
	{
		m_vecSpawnCampRef = pev->origin;
		m_flSpawnCampTouch = gpGlobals->time + kCampTouchSec;
	}
}

// ref @0x139b26 — holster clears SubModel disp (+0x925) via WeapDispSet(2) when +0xcd4 empty.
static void SGTC_TickWeapSubModelDisp( CBasePlayer *p )
{
	if( !p->m_bWeapSubModel )
		return;
	if( p->m_pActiveItem )
		return;
	p->SGTC_WeapDispSet( 2 );
}

// ref @0x139b5d — periodic cloak refresh while +0x448 (0.2s latch @+0xa90).
void CBasePlayer::SGTC_TickCloakPulse( void )
{
	static const float kCloakPulseSec = 0.2f;	// rodata @0x1d754a
	static const float kCloakHealthGate = 100.0f;	// rodata @0x1d753c — pev+0x160

	if( !m_bSgtcCloakTick )
		return;
	if( pev->health != kCloakHealthGate )
		return;
	if( gpGlobals->time < pev->animtime )
		return;
	if( gpGlobals->time < m_flCloakTickNext )
		return;

	pev->effects |= EF_NODRAW;
	if( m_bSnagInvisLatch )
		SnagInvis();

	m_flCloakTickNext = gpGlobals->time + kCloakPulseSec;
}

// ref @0x139814 — arm 0.2s window (rodata 0x1d7530), expire +0xa3e, copy to +0x4e8.
void CBasePlayer::SGTC_TickLanceStunLatch( void )
{
	static const float kLanceStunSec = 0.2f;

	if( !m_bLanceStunLatch && m_bLanceStunFlag )
		m_flLanceStunEnd = gpGlobals->time + kLanceStunSec;

	if( m_bLanceStunFlag && gpGlobals->time >= m_flLanceStunEnd )
		m_bLanceStunFlag = 0;

	m_bLanceStunLatch = m_bLanceStunFlag;
}

// ref @0x1398ac — while +0x463, copy rodata (0,0,-10) into pev+0x174 (ref view_ofs).
void CBasePlayer::SGTC_TickNukeViewSnap( void )
{
	static const float kViewSnapZ = -10.0f;	// rodata @0x1d7548 (via ebx-0x2d4d8)

	if( !m_bNukeViewSnap )
		return;

	pev->view_ofs = Vector( 0.0f, 0.0f, kViewSnapZ );
}

// ref @0x1399cf — hurtinvert preset PE colors; when +0x4c0 sum == 0, copy recallall RGB.
void CBasePlayer::SGTC_TickHurtInvertPeFx( void )
{
	static const float kZero = 0.0f;		// rodata @0x1d752c (ebx-0x2d4e8)
	static const float kAmt255 = 255.0f;	// rodata @0x1d7540 (ebx-0x2d4d4)
	static const float kTwo = 2.0f;		// rodata @0x1d7544 (ebx-0x2d4d0)
	static const float kOne = 1.0f;		// rodata @0x1d7548 (ebx-0x2d4cc)

	if( m_bHurtInvertFx )
	{
		pev->rendermode = kRenderTransAdd;	// ref pev+0x148 = 5
		pev->renderamt = kAmt255;
		pev->rendercolor.x = kTwo;
		pev->rendercolor.y = kZero;
		pev->rendercolor.z = kZero;
		pev->renderfx = kRenderFxGlowShell;	// ref pev+0x15c = 0x13
		pev->renderamt = kOne;
		pev->rendercolor.x = kAmt255;
		pev->rendercolor.y = kAmt255;
		pev->rendercolor.z = kTwo;
	}

	const float flSum = m_vecHurtInvertRgb.x + m_vecHurtInvertRgb.y + m_vecHurtInvertRgb.z;
	if( flSum != kZero )
		return;

	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = kOne;
	pev->rendercolor = m_vecHurtInvertRgb;
}

// ref ClientCommand(edict_s*) @0xc8ff2 — full SGTC verb dispatch, ported in
// ref order. Dev verbs are gated by +0x460 (m_bShieldLarge — "<x><y><z>"
// unlock prints "Special Mode Online!!"). Handled here; stock client.cpp
// keeps only the verbs the ref shares (say/use/give/fov/etc.).
static void SGTC_SpectateClient( CBasePlayer *pPlayer )
{
	// shared ref body for "spectate"/"observer" paths @0xc9a18/@0xca234:
	// ticket decrement, owned-ent cleanup, HUD reset, death cam.
	if( gmsgSpectator )	// ref StartObserver @0x129076 sends Spectator
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSpectator, NULL, pPlayer->pev );
			WRITE_BYTE( pPlayer->entindex() );
			WRITE_BYTE( 1 );
		MESSAGE_END();
	}
	if( pPlayer->IsAlive() &&
		( sgtc_GameModeSet == 1 || sgtc_GameModeSet == 4 ) )
	{
		if( pPlayer->pev->team == 1 && sgtc_G_tickets > 0 )
			sgtc_G_tickets--;
		else if( pPlayer->pev->team == 2 && sgtc_T_tickets > 0 )
			sgtc_T_tickets--;
	}

	CBaseEntity *pScan = NULL;
	while( ( pScan = UTIL_FindEntityByClassname( pScan, "effect_shield" ) ) != NULL )
	{
		if( pScan->pev->owner == pPlayer->edict() )	// ref pev+0x198 entindex
			UTIL_Remove( pScan );
	}
	pScan = NULL;
	while( ( pScan = UTIL_FindEntityByClassname( pScan, "monster_trapbox" ) ) != NULL )
	{
		if( pScan->pev->owner == pPlayer->edict() )
			UTIL_Remove( pScan );
	}
	pScan = NULL;
	while( ( pScan = UTIL_FindEntityByClassname( pScan, "monster_larve" ) ) != NULL )
	{
		if( pScan->pev->owner == pPlayer->edict() )
			pScan->pev->iuser4 = 1;	// [INFERENCE] ref +0x41d byte flag
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgReset, NULL );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_BYTE( 1 );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pPlayer->pev );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 4 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	pPlayer->pev->view_ofs = Vector( 0.0f, 0.0f, 28.0f );
	pPlayer->m_bNukeViewSnap = 0;				// ref +0x463
	pPlayer->pev->effects &= ~EF_DIMLIGHT;	// ref pev+0x118 & 0xfffffff7
	pPlayer->m_iSgtcTeamLatch = 0;						// ref +0x3e0-ish latch [INFERENCE]
	pPlayer->m_bShieldSprint = 0;				// ref +0x4e9
	pPlayer->m_bShieldSpawned = 0;			// ref +0x4e4
	pPlayer->m_flSgtcLatch400 = 0;			// ref +0x3f8..3fc [INFERENCE]
	pPlayer->pev->rendermode = 0;				// ref pev+0x148
	pPlayer->m_bSgtcPad461 = 1;				// ref +0x461
	pPlayer->m_iFOV = 0;						// ref +0xcb0
	pPlayer->pev->fov = 0.0f;					// ref pev+0x214
	pPlayer->m_iSgtcPortraitSel = 0;			// ref +0xa44
	pPlayer->m_iSgtcTeam = 0;					// ref +0x438
	pPlayer->m_iSgtcPortraitShown = 0;		// ref +0xa40
	pPlayer->m_iSgtcActiveTeam = 0;			// ref +0x430
	pPlayer->pev->team = 0;						// ref pev+0x1ac
	strncpy( pPlayer->m_szTeamName, "", TEAM_NAME_LENGTH );	// ref +0xe08
	pPlayer->StartDeathCam();
}

// ref ClientCommand 'observer' @0xd9daa-0xd9ef7 — short body: HUD reset msgs,
// +0xa40/pev+0x1ac/team-name clears, StartDeathCam.  NO ticket decrement and
// NO owned-entity cleanup (unlike 'spectate').
static void SGTC_ObserveClient( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgReset, NULL );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_BYTE( 1 );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pPlayer->pev );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 4 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	pPlayer->m_iSgtcPortraitShown = 0;			// ref +0xa40
	pPlayer->pev->team = 0;						// ref pev+0x1ac
	strncpy( pPlayer->m_szTeamName, "", TEAM_NAME_LENGTH );	// ref +0xe08
	pPlayer->StartDeathCam();
}

BOOL SGTC_HandleClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if( FStrEq( pcmd, "stuck" ) )	// ref @0xc9012
	{
		if( gpGlobals->time <= pPlayer->m_flSgtcStuck )
		{
			ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE,
				UTIL_VarArgs( "Wait 10 seconds before trying again\n" ) );
			return TRUE;
		}
		pPlayer->pev->origin.z += 20.0f;	// ref pev+0x10 += 20.0
		pPlayer->pev->velocity = g_vecZero;	// ref @0xd90df: velocity * 0.0f (rodata 0x1f0e80)
		pPlayer->m_flSgtcStuck = gpGlobals->time + 10.0f;
		return TRUE;
	}

	if( FStrEq( pcmd, "<x><y><z>" ) )	// ref @0xc90be — dev-mode unlock
	{
		if( pPlayer->m_iSgtcSpecMode != -1 )
			return TRUE;
		pPlayer->m_bShieldLarge = 1;
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE,
			UTIL_VarArgs( "Special Mode Online!! %d\n", pPlayer->m_iSgtcSpecMode ) );
		return TRUE;
	}

	if( FStrEq( pcmd, "elite" ) )
	{
		if( pPlayer->m_bShieldLarge == 1 && CMD_ARGC() > 1 &&
			atoi( CMD_ARGV( 1 ) ) > 0 && atoi( CMD_ARGV( 1 ) ) < 5 )
			pPlayer->m_iSgtcElite = atoi( CMD_ARGV( 1 ) );	// ref +0xa54
		return TRUE;
	}

	if( FStrEq( pcmd, "???^^^%%%" ) )	// ref @0xc92b8 — recallall PE color set
	{
		if( CMD_ARGC() < 3 )
			return TRUE;
		pPlayer->m_vecHurtInvertRgb = Vector( (float)atoi( CMD_ARGV( 1 ) ),
			(float)atoi( CMD_ARGV( 2 ) ), (float)atoi( CMD_ARGV( 3 ) ) );
		return TRUE;
	}

	if( FStrEq( pcmd, "recallall" ) )	// ref @0xc9301 — clears owned skulls
	{
		if( !pPlayer->m_bHurtInvertFx )
			return TRUE;
		CBaseEntity *pScan = NULL;
		while( ( pScan = UTIL_FindEntityByClassname( pScan, "monster_skull" ) ) != NULL )
			((CSGTCSkull *)pScan)->m_pSkullOwner = NULL;	// ref +0xc0
		return TRUE;
	}

	if( FStrEq( pcmd, "hurtinvert" ) )	// ref @0xc94c1
	{
		if( !pPlayer->m_bShieldLarge )
			return TRUE;
		pPlayer->m_bHurtInvertFx ^= 1;
		return TRUE;
	}

	if( FStrEq( pcmd, "skulls" ) )	// ref @0xc952c
	{
		if( !pPlayer->m_bShieldLarge )
			return TRUE;
		for( int i = 0; i < 11; i++ )
		{
			Vector vecSpot( (float)( RANDOM_LONG( 0, 8000 ) - 4000 ),
				(float)( RANDOM_LONG( 0, 8000 ) - 4000 ),
				(float)( RANDOM_LONG( 0, 8000 ) - 4000 ) );
			CBaseEntity::Create( "monster_skull", vecSpot, pPlayer->pev->v_angle, NULL );
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "cashmeup" ) )	// ref @0xc9618
	{
		if( pPlayer->m_bShieldLarge )
			pPlayer->m_iNukeBonus = 200;	// ref +0x934
		return TRUE;
	}

	if( FStrEq( pcmd, "minemeup" ) )
	{
		if( pPlayer->m_bShieldLarge )
			pPlayer->GiveNamedItem( "weapon_mine" );
		return TRUE;
	}

	if( FStrEq( pcmd, "c4time" ) )
	{
		if( pPlayer->m_bShieldLarge )
			pPlayer->GiveNamedItem( "weapon_c4" );
		return TRUE;
	}

	if( FStrEq( pcmd, "m_0" ) )	{ if( pPlayer->m_bShieldLarge ) sgtc_GameModeSet = 0; return TRUE; }
	if( FStrEq( pcmd, "m_1" ) )	{ if( pPlayer->m_bShieldLarge ) sgtc_GameModeSet = 1; return TRUE; }
	if( FStrEq( pcmd, "m_2" ) )	{ if( pPlayer->m_bShieldLarge ) sgtc_GameModeSet = 2; return TRUE; }
	if( FStrEq( pcmd, "m_3" ) )	{ if( pPlayer->m_bShieldLarge ) sgtc_GameModeSet = 3; return TRUE; }
	if( FStrEq( pcmd, "m_4" ) )	{ if( pPlayer->m_bShieldLarge ) sgtc_GameModeSet = 4; return TRUE; }

	if( FStrEq( pcmd, "savetheday" ) )	// ref @0xc9bb2 — superman mode
	{
		if( !pPlayer->m_bShieldLarge )
			return TRUE;
		pPlayer->m_bSgtcSuperman = 1;					// ref +0x924 (+0xdc slot is m_bShieldLarge-adjacent in ref; no port analog)
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(),
			g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ),
			"model", "superman" );
		pPlayer->m_flMaxSpeedLatch = 500.0f;			// ref +0xa7c = 0x43fa0000
		pPlayer->pev->maxspeed = pPlayer->m_flMaxSpeedLatch;	// ref pev+0x210
		pPlayer->pev->takedamage = 0.0f;				// ref pev+0x16c
		pPlayer->m_flSnagArmorRamp = 100.0f;			// ref pev+0x1bc = 0x42c80000
		return TRUE;
	}

	if( FStrEq( pcmd, "unas" ) )
	{
		if( pPlayer->m_bShieldLarge )
			pPlayer->m_bSnagInvisFlag ^= 1;	// ref +0x4df toggle
		return TRUE;
	}

	if( FStrEq( pcmd, "talent" ) )	// ref @0xc9cd4
	{
		if( !pPlayer->IsAlive() )
			return TRUE;
		pPlayer->Talent();
		return TRUE;
	}

	if( FStrEq( pcmd, "saveannotherday" ) )
	{
		if( pPlayer->m_bShieldLarge && pPlayer->m_bSgtcSuperman )
		{
			pPlayer->m_bSgtcSuperman = 0;
			pPlayer->pev->takedamage = 2.0f;	// ref pev+0x16c = 0x40000000
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "credits" ) )	// ref @0xc9e22
	{
		if( !pPlayer->m_bShieldLarge )
			return TRUE;
		for( int i = 0; i < 5; i++ )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
				WRITE_BYTE( pPlayer->entindex() );
				WRITE_STRING( "SGTC CODED BY ENDOLAYLITH\n" );
			MESSAGE_END();
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "observer" ) )	// ref @0xd9daa — short body (no tickets/cleanup)
	{
		if( allow_spectators.value == 0.0f )
			return TRUE;
		SGTC_ObserveClient( pPlayer );
		return TRUE;
	}

	if( FStrEq( pcmd, "dropkey" ) )	// ref @0xd9efc
	{
		if( pPlayer->m_iInBombZone > 0 )	// ref +0xaec
		{
			UTIL_MakeVectors( pPlayer->pev->v_angle );
			TraceResult tr;
			// ref @0xd9f4d-0xda01f: trace origin+fwd*64 → origin+fwd*20.
			Vector vecStart = pPlayer->pev->origin + gpGlobals->v_forward * 64.0f;
			Vector vecEnd = pPlayer->pev->origin + gpGlobals->v_forward * 20.0f;
			UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters,
				pPlayer->edict(), &tr );
			CBaseEntity *pKey = CBaseEntity::Create( "item_objective",
				tr.vecEndPos, pPlayer->pev->angles, NULL );
			if( pKey )
			{
				pKey->pev->angles.x = 0.0f;	// ref pev+0x50/+0x58 zeroed
				pKey->pev->angles.z = 0.0f;
				// ref @0xda068-0xda0c7: fwd*200 + thrower velocity.
				pKey->pev->velocity = gpGlobals->v_forward * 200.0f
					+ pPlayer->pev->velocity;
				sgtc_Elmo = 1;				// ref @0xda0dc byte flag
			}
			pPlayer->m_iInBombZone = 0;
		}
		else
		{
			CLIENT_PRINTF( pPlayer->edict(), print_console, "No key to drop\n" );	// ref @0xda0e4 ptype=0
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "spectate" ) )	// ref @0xca234 (non-proxy body)
	{
		if( allow_spectators.value == 0.0f )
			return TRUE;
		SGTC_SpectateClient( pPlayer );
		return TRUE;
	}

	if( FStrEq( pcmd, "nospectate" ) )
	{
		if( allow_spectators.value == 0.0f )
			return TRUE;
		pPlayer->StopObserver();
		return TRUE;
	}

	if( FStrEq( pcmd, "nextmapforme" ) )
	{
		if( pPlayer->m_bShieldLarge )
			g_pGameRules->EndMultiplayerGame();	// ref vtable+0x100
		return TRUE;
	}

	if( FStrEq( pcmd, "glowing" ) )	// ref @0xca6f8 — renderfx + TE_DLIGHT
	{
		if( !pPlayer->m_bShieldLarge )
			return TRUE;
		pPlayer->pev->renderfx = kRenderFxGlowShell;	// ref pev+0x15c = 0x13
		pPlayer->pev->renderamt = 1.0f;				// ref pev+0x14c
		pPlayer->pev->rendercolor.x = 255.0f;			// ref pev+0x150
		pPlayer->pev->rendercolor.y = 0.0f;
		pPlayer->pev->rendercolor.z = 0.0f;
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pPlayer->pev->origin );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( pPlayer->pev->origin.x );
			WRITE_COORD( pPlayer->pev->origin.y );
			WRITE_COORD( pPlayer->pev->origin.z );
			WRITE_BYTE( 20 );	// radius
			WRITE_BYTE( 255 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 50 );	// life
			WRITE_BYTE( 0 );	// decay
		MESSAGE_END();
		return TRUE;
	}

	if( FStrEq( pcmd, "faker" ) )
	{
		if( pPlayer->m_bShieldLarge )
		{
			pPlayer->SetAnimation( (PLAYER_ANIM)4 );
			pPlayer->m_bNukeViewSnap = 1;	// ref +0x463
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "faked" ) )
	{
		if( pPlayer->m_bShieldLarge )
			pPlayer->m_bNukeViewSnap = 0;
		return TRUE;
	}

	if( FStrEq( pcmd, "messaged" ) )	// ref @0xca9f8
	{
		if( !pPlayer->m_bShieldLarge )
			return TRUE;
		char szMsg[128];
		snprintf( szMsg, sizeof( szMsg ), "%d\n",
			(int)(long)CMD_ARGV( 1 ) );	// ref prints the argv pointer [INFERENCE]
		for( int i = 0; i < 5; i++ )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
				WRITE_BYTE( pPlayer->entindex() );
				WRITE_STRING( szMsg );
			MESSAGE_END();
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "corpsemeup" ) )
	{
		if( pPlayer->m_bShieldLarge )
			CopyToBodyQue( pPlayer->pev );
		return TRUE;
	}

	if( FStrEq( pcmd, "drugs4me" ) )
	{
		if( pPlayer->m_bShieldLarge )
			pPlayer->m_flSgtcLatch3fc = gpGlobals->time + 15.0f;	// ref +0x3fc
		return TRUE;
	}

	if( FStrEq( pcmd, "vguimenuyoyo" ) )
	{
		if( CMD_ARGC() < 1 )
			return TRUE;
		pPlayer->ShowVGUIMenu( atoi( CMD_ARGV( 1 ) ) );
		return TRUE;
	}

	if( FStrEq( pcmd, "hidevgui" ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHideVGUI, NULL, pPlayer->pev );
		MESSAGE_END();
		return TRUE;
	}

	if( FStrEq( pcmd, "info" ) )	// ref prints netname string-pool offset
	{
		CLIENT_PRINTF( pPlayer->edict(), print_console,
			UTIL_VarArgs( "%d\n", (int)( STRING( pPlayer->pev->netname )
				- gpGlobals->pStringBase ) ) );	// ref @0xdae47: pev->netname - pStringBase
		return TRUE;
	}

	if( FStrEq( pcmd, "c4detonate" ) )	// ref @0xcaeb4
	{
		if( !pPlayer->IsAlive() || pPlayer->IsObserver() ||
			pPlayer->pev->health <= 0.0f )
			return TRUE;
		if( pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_iId == 0xe )
			return TRUE;
		CBaseEntity *pScan = NULL;
		while( ( pScan = UTIL_FindEntityInSphere( pScan, pPlayer->pev->origin,
			8000.0f ) ) != NULL )
		{
			if( !FClassnameIs( pScan->pev, "monster_satchel" ) )
				continue;
			if( pScan->pev->owner != pPlayer->edict() )
				continue;
			// ref +0x412 is the satchel's armed/detonate latch in the ref's own
			// (larger) object layout — not valid on stock CSatchelCharge, so
			// gate on the live armed state instead of a raw offset read.
			if( !pScan->IsAlive() )
				continue;
			pPlayer->m_bSgtcSatchelFired = 1;	// ref +0x910
			EMIT_SOUND( ENT( pScan->pev ), CHAN_BODY,
				"weapons/c4_beep.wav", 1.0f, 0.8f );
			pScan->Use( pPlayer, pPlayer, USE_ON, 0 );	// ref vtable+0xcc
		}
		return TRUE;
	}

	if( FStrEq( pcmd, "gond4h9wp" ) )	// ref @0xcb34c — progress sync
	{
		if( pPlayer->m_bSgtcPortraitsFrozen )
		{
			pPlayer->m_bSgtcMenuLock = 1;
			pPlayer->m_bSgtcPortraitsFrozen = 0;
		}
		for( int i = 0; i < 28; i++ )
			pPlayer->m_iSgtcGondArgs[i] = atoi( CMD_ARGV( i + 1 ) );
		return TRUE;
	}

	if( FStrEq( pcmd, "VoteMode" ) )	// ref — mode vote select (+0x90c/+0x927)
	{
		int iSel = atoi( CMD_ARGV( 1 ) );
		if( iSel >= 0 && iSel < 5 )
			pPlayer->SGTC_MenuselectBombKey( iSel );
		return TRUE;
	}

	return FALSE;
}

// ref @0x139db0 — while pev+0x160 (health marker) == 1, latch +0x3fc and sustain zat visuals.
void CBasePlayer::SGTC_TickZatTouchMaintain( void )
{
	static const float kHealthGate = 1.0f;	// rodata @0x1d7548
	static const float kLatch3fc = 0.125f;	// rodata @0x1d7528
	static const float kZatArmor = 1.0f;	// rodata @0x1d7548 (reuse)

	if( pev->health != kHealthGate )
		return;

	m_flSgtcLatch3fc = kLatch3fc;
	m_iStamina = 0;

	if( !m_bZatTouchFlag )
		return;

	pev->renderfx = kRenderFxGlowShell;	// ref pev+0x15c = 0x13
	pev->renderamt = kZatArmor;	// ref pev+0x14c
	pev->rendercolor.x = 25.0f;	// rodata @0x1d7550
	pev->rendercolor.y = 10.0f;	// rodata @0x1d7554
	pev->rendercolor.z = 255.0f;	// rodata @0x1d7540
}

// ref @0x139e51 — when armor latch expired and not sprinting past gate travel, restore PE shield colors.
void CBasePlayer::SGTC_TickClearZatShieldPe( void )
{
	static const float kShieldCap = 255.0f;	// rodata @0x1d7540

	if( pev->renderamt != 0.0f )
		return;
	if( m_bShieldSprint )
		return;
	if( m_flGateTravel == gpGlobals->time )
		return;

	pev->renderamt = kShieldCap;
	pev->renderfx = kRenderFxNone;
	pev->rendermode = kRenderNormal;
}

// ref @0x139edb — drop nuke arm counter when +0x42c charge window ends.
void CBasePlayer::SGTC_TickNukeChargeExpiry( void )
{
	if( m_flNukeChargeTime > gpGlobals->time )
		return;
	m_iNukeCharge = 0;
}

static BOOL SGTC_FloatEq( float a, float b )
{
	return a == b;
}

// ref @0x1396f4..0x1397ae — origin/velocity/movetype/netname relink, then jmp @0x13b31f.
static void SGTC_ApplyBadGuyTrapState( CBasePlayer *p )
{
	static const float kBadGuyOrigin = 4096.0f;	// rodata @0x1d7524 (ebx-0x2d4ec)

	p->pev->origin = Vector( kBadGuyOrigin, kBadGuyOrigin, kBadGuyOrigin );
	p->pev->velocity = g_vecZero;
	p->pev->movetype = MOVETYPE_NONE;
	p->m_bSgtcBadGuy = 1;
}

// ref @0x139681..0x1397ae — alive && netname is rodata "??-BAD-GUY-??" (pointer compare).
static BOOL SGTC_ApplyBadGuyNetnameTrap( CBasePlayer *p )
{
	if( FStringNull( p->pev->netname ) )
		return FALSE;
	if( !FStrEq( STRING( p->pev->netname ), "?" "?-BAD-GUY-?" "?" ) )
		return FALSE;

	SGTC_ApplyBadGuyTrapState( p );
	return TRUE;
}

static void SGTC_TickZoneIdExpiry( CBasePlayer *p )
{
	if( !SGTC_FloatEq( p->m_flZoneTime, gpGlobals->time ) )
		p->m_iZoneId = 0;
}

// ref @0x13a88a..0x13b31f — 0.5s-gated tail after bomb-key sync (shield blood @0x13a584 in ref).
void CBasePlayer::SGTC_TickSgtcThinkTail( void )
{
	static const float kTailGateSec = 0.5f;		// rodata @0x1d755c (also move latch)
	static const float kStaminaHealthFactor = 1.8f;	// QWORD rodata @0x1d7588
	static const float kHealthGate99 = 99.0f;	// rodata @0x1d7580
	static const float kZatArmorCap = 255.0f;	// rodata @0x1d7540

	const float flNow = gpGlobals->time;
	const BOOL bTailTick = ( flNow >= m_flSgtcTailTick );	// deadline, like the sibling latch gates

	if( bTailTick )
	{
		if( !SGTC_FloatEq( m_flSgtcLatch3fc, flNow ) )
		{
			if( SGTC_FloatEq( m_flSgtcLatch490, flNow ) && gmsgConcuss )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgConcuss, NULL, pev );
				WRITE_BYTE( 150 );
				WRITE_BYTE( 0 );
				MESSAGE_END();
				m_iConcussLatch = 1;
			}

			const int r = RANDOM_LONG( 0, 255 );
			UTIL_ScreenFade( this, Vector( (float)r, (float)r, (float)r ),
				0.01f, 1.0f, 100, 0 );
		}
		else if( m_iConcussLatch == 1 && gmsgConcuss )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgConcuss, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
			m_iConcussLatch = 0;
		}

		m_flSgtcLatch490 = m_flSgtcLatch3fc;

		if( SGTC_FloatEq( m_flSgtcLatch400, flNow ) )
			m_iHudInfoMode = 0;

		if( m_iHudInfoMode != m_iHudInfoModeSent && gmsgHudInfoMode )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHudInfoMode, NULL, pev );
			WRITE_BYTE( m_iHudInfoMode );
			MESSAGE_END();
			m_iHudInfoModeSent = m_iHudInfoMode;
		}

		if( SGTC_FloatEq( m_flGateTravel, flNow ) )
		{
			pev->renderamt = 0.0f;	// ref pev+0x14c
			pev->rendermode = kRenderTransAlpha;
			pev->renderfx = kRenderFxNone;
			pev->velocity = g_vecZero;
		}

		if( pev->renderamt == 0.0f && !m_bShieldSprint )
		{
			pev->renderamt = kZatArmorCap;
			pev->renderfx = kRenderFxNone;
			pev->rendermode = kRenderNormal;
		}

		const int iStaminaBefore = m_iStamina;
		const float flHealth = pev->health;
		const float flStaminaCap = flHealth * kStaminaHealthFactor;

		if( (float)m_iStamina > flStaminaCap )
		{
			const float flDelta = (float)m_iStamina - flHealth;
			if( flDelta <= 10.0f )
				m_iStamina -= 10;
			else
				m_iStamina = (int)flHealth;
			if( m_iStamina < 0 )
				m_iStamina = 0;
			SGTC_UpdateStamina();
		}

		if( (float)m_iStamina <= flHealth )
		{
			const int iSaved = m_iStamina;
			if( pev->velocity.y != 0.0f && pev->velocity.x != 0.0f
				&& !( pev->flags & FL_DUCKING ) )
				m_iStamina += 3;
			else
				m_iStamina += 2;

			if( m_bSnagInvisLatch )
				m_iStamina += 5;

			if( pev->flags & FL_ONGROUND )
				m_iStamina += 2;

			if( m_bSgtcCloakTick )
				m_iStamina += 1;

			if( m_iStamina < 0 )
				m_iStamina = 0;

			if( !SGTC_FloatEq( m_flSgtcLatch3fc, flNow ) )
				m_iStamina -= 7;

			if( m_iStamina != iSaved )
				SGTC_UpdateStamina();

			if( (float)m_iStamina == flHealth )
				SGTC_UpdateStamina();
		}
		else if( m_iStamina != iStaminaBefore )
			SGTC_UpdateStamina();

		if( m_bSnagInvisLatch && flHealth == kHealthGate99 && m_bSnagArmorHud
			&& m_iSnagArmorCap > 0
			&& SGTC_FloatEq( m_flSnagArmorRamp, (float)m_iSnagArmorCap ) )
		{
			m_flSnagArmorRamp += 1.0f;
			if( m_flSnagArmorRamp > (float)m_iSnagArmorCap )
				m_flSnagArmorRamp = (float)m_iSnagArmorCap;

			const int iBar = (int)( m_flSnagArmorRamp * 255.0f / (float)m_iSnagArmorCap );
			const int iSuper = SGTC_SupergunsHudAux();

			if( iBar != m_iArmorHudSent || iSuper != m_iSupergunsSent )
			{
				if( gmsgArmor )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pev );
					WRITE_BYTE( iBar );
					WRITE_BYTE( iSuper );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					MESSAGE_END();
				}
				m_iArmorHudSent = iBar;
				m_iSupergunsSent = iSuper;
			}
		}
	}

	m_flSgtcTailTick = flNow + kTailGateSec;

	if( m_bSgtcPePreset )
	{
		pev->renderfx = kRenderFxGlowShell;	// ref pev+0x15c = 0x13
		pev->renderamt = 1.0f;	// ref pev+0x14c @0x13b2b2
		pev->rendercolor.x = 2.0f;	// ref pev+0x150
		pev->rendercolor.y = 0.0f;	// ref pev+0x154
		pev->rendercolor.z = 0.0f;	// ref pev+0x158
	}

	SGTC_TickZoneIdExpiry( this );
}

// ref CBasePlayer::SgtcThink @0x13963a..0x13b326 (~7 KB). Post-radar trace/blood/fade
// stays inlined in ref; port splits shield-trace latch to CheckTimeBasedDamage @0x13e995.
void CBasePlayer::SGTC_SgtcThink( void )
{
	// ref @0x139669 — !IsAlive jumps to @0x1396d6 (same trap path as BAD-GUY netname).
	if( !IsAlive() )
	{
		SGTC_ApplyBadGuyTrapState( this );
		SGTC_TickZoneIdExpiry( this );
		return;
	}

	if( SGTC_ApplyBadGuyNetnameTrap( this ) )
	{
		SGTC_TickZoneIdExpiry( this );
		return;
	}

	// ref @0x1397b3 — nuke bonus display counter floor.
	if( m_iNukeBonus <= 5 )
		m_iNukeBonus = 6;

	// ref @0x1397d4 — on ladder, drop personal shield deploy flag (+0x3e0).
	if( IsOnLadder() && m_bShieldWall )
		m_bShieldWall = 0;

	SGTC_TickLanceStunLatch();
	SGTC_TickNukeViewSnap();
	SGTC_TryBombKeyCampWarn();
	SGTC_TickHurtInvertPeFx();
	SGTC_TickWeapSubModelDisp( this );
	SGTC_TickCloakPulse();
	SGTC_TickSpawnCampProximity();
	SGTC_TickZatTouchMaintain();
	SGTC_TickClearZatShieldPe();
	SGTC_TickNukeChargeExpiry();
	SGTC_SendProgressIfDirty();
	SGTC_TickMoveLatchAndStaminaDrain( this );
	SGTC_TickShieldEnergyAndMaxspeed();
	SGTC_SyncShieldEnergyFromContents();
	SGTC_ShieldThinker();
	SGTC_RadarScan();
	SGTC_SyncBombZoneHasKey();
	SGTC_TickSgtcThinkTail();
}

// ===========================================================================
// Port of decoded CBasePlayer helpers + ref ClientCommand verbs.
// ref/stc_i386.so (ELF VMA = Ghidra addr - 0x10000).
// ===========================================================================

extern int gmsgSetHudRoundTime;
extern int gmsgVGUIMenu;
extern int gmsgAboutMe;
// ref globals: Elmo @0x2368f0 (bomb flag set by dropkey — defined near top),
// talentson @0x2135b0 (mp_allowtalents cvar — registered lazily by Talent()).
static cvar_t sgtc_cvar_talentson = { "mp_allowtalents", "1", FCVAR_SERVER };
static BOOL sgtc_bTalentsonRegged = FALSE;

static int SGTC_Endo_takefromlarger( int a, int b )
{
	return a > b ? a - b : b - a;
}

// ref CBasePlayer::SetHudRoundTime(int,int) @0x145494 — MSG_ONE SetRndTim:
// WRITE_SHORT(roundtime) then WRITE_BYTE(mode), only when (roundtime,mode)
// changed. Ref quirk: the mode field +0x4d8 is compared but never written back.
void CBasePlayer::SetHudRoundTime( int roundtime, int mode )
{
	if( m_iHudRoundTime != roundtime || m_iHudRoundMode != mode )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSetHudRoundTime, NULL, pev );
			WRITE_SHORT( roundtime );	// ref @0x1454a7: DAT_00238ec8 = WRITE_SHORT FIRST
			WRITE_BYTE( mode );		// ref @0x1454b0: DAT_00238ec0 = WRITE_BYTE second
		MESSAGE_END();
		m_iHudRoundTime = roundtime;
	}
}

// ref CBasePlayer::ShowVGUIMenu(int) @0x125538 — MSG_ONE VGUIMenu: BYTE menu.
void CBasePlayer::ShowVGUIMenu( int menu )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgVGUIMenu, NULL, pev );
		WRITE_BYTE( menu );
	MESSAGE_END();
}

// ref CBasePlayer::SendAboutMe(int,const char*,int) @0x1262b8 — MSG_ONE
// AboutMe: BYTE 0, SHORT a, SHORT c. param_2 is unused in the .so.
void CBasePlayer::SendAboutMe( int a, const char *s, int c )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgAboutMe, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_SHORT( a );
		WRITE_SHORT( c );
	MESSAGE_END();
	(void)s;	// ref param_2 unused
}

// ref CBasePlayer::SendGlobalTextIndex(int) @0x129026 — reuses the Concuss
// usermsg channel: BYTE 0, BYTE index.
void CBasePlayer::SendGlobalTextIndex( int idx )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgConcuss, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_BYTE( idx );
	MESSAGE_END();
}

// ref CBasePlayer::NVG() @0x13444a — toggles +0xe18 while +0xe1c (has NVG).
void CBasePlayer::NVG( void )
{
	if( !m_iHasNVG )
		return;

	if( !m_iNVGOn )
	{
		UTIL_ScreenFade( this, Vector( 0, 0, 255 ), 1.0f, 0.5f, 0x80, 5 );
		CLIENT_COMMAND( edict(), "gl_overbright 1\n" );
		CLIENT_COMMAND( edict(), "gamma 50\n" );
		m_iNVGOn = 1;
	}
	else
	{
		UTIL_ScreenFade( this, Vector( 0, 0, 255 ), 0.01f, 0.5f, 0x80, 0 );
		CLIENT_COMMAND( edict(), "gl_overbright 0\n" );
		CLIENT_COMMAND( edict(), "gamma 3\n" );
		NVG_Zoomout();
		m_iNVGOn = 0;
	}
}

// ref CBasePlayer::NVG_Zoomin() @0x1345d4 — fov steps -15 per zoom level,
// clamped at 5 levels (+0xe20 < 5), only while NVG on.
void CBasePlayer::NVG_Zoomin( void )
{
	if( m_iNVGOn && m_iNVGZoom < 5 )
	{
		m_iNVGZoom++;
		m_iFOV = m_iNVGZoom * -15 + 90;
	}
}

// ref CBasePlayer::NVG_Zoomout() @0x13442a — restore fov 90 while NVG on.
void CBasePlayer::NVG_Zoomout( void )
{
	if( m_iNVGOn )
		m_iFOV = 90;
}

// ref CBasePlayer::Talent() @0x134a02.
// Gate: mp_allowtalents != 0, IsAlive() && !IsObserver().
// +0x4de (m_bSnagInvisLatch): Unas corpse-feed — must be FL_DUCKING and past
// +0x494 cooldown; feeds on a nearby (<40) corpse_control with a bound corpse
// that isn't armed/fed/burning/effect-busy.
// pev->team (ref pev+0x1ac) selects the per-team talent switch on +0xa40:
//   team 2 (Goa'uld): 1=grenade 2=zatboom 3=armor toggle/sneak 4=shield buff
//                     5=shield emitter deploy
//   other teams     : 1=larve 2=armor on/off 3=gacgoa flash 4=trapbox
//                     5=proximity shield-emitter mode retag
void CBasePlayer::Talent( void )
{
	if( !sgtc_bTalentsonRegged )
	{
		CVAR_REGISTER( &sgtc_cvar_talentson );
		sgtc_bTalentsonRegged = TRUE;
	}
	if( sgtc_cvar_talentson.value == 0.0f )
		return;

	if( !IsAlive() || IsObserver() )
		return;

	// --- Unas feed (ref branch on +0x4de latch) ---
	if( m_bSnagInvisLatch )
	{
		if( gpGlobals->time <= m_flTalentCooldown )
			return;
		if( !( pev->flags & FL_DUCKING ) )	// ref pev+0x1a4 bit 14
			return;

		CBaseEntity *pScan = NULL;
		CCorpseController *pCtrl = NULL;
		while( ( pScan = UTIL_FindEntityByClassname( pScan, "corpse_control" ) ) != NULL )
		{
			if( SGTC_Endo_distance3d( pev->origin, pScan->pev->origin ) < 40 )
				pCtrl = (CCorpseController *)pScan;
		}
		if( !pCtrl )
			return;
		if( !pCtrl->m_pCorpse )			// ref +0x19c
			return;
		if( pCtrl->m_bBurn )				// ref +0x1a7
			return;
		if( pCtrl->m_bArmed )				// ref +0x1a4
			return;
		if( pCtrl->m_pEffect )				// ref +0x1b8
			return;

		m_iStamina = 100;
		SGTC_UpdateStamina();
		m_flSgtcLatch3fc = gpGlobals->time;
		// ref vtable+0x3c call — 2-arg (70.0f, DMG_DROWNRECOVER); TakeHealth
		// is the matching port virtual (overheal path).
		TakeHealth( 70.0f, DMG_DROWNRECOVER );
		m_flSnagArmorRamp += 80.0f;		// ref pev+0x1bc (armor)
		if( m_flSnagArmorRamp > (float)m_iSnagArmorCap )
			m_flSnagArmorRamp = (float)m_iSnagArmorCap;
		UTIL_ScreenFade( this, g_vecZero, 10.0f, 10.0f, 0, 0 );
		m_flTalentCooldown = gpGlobals->time + 1.0f;
		pCtrl->m_bFed = 1;				// ref +0x1a5
		EMIT_SOUND( edict(), CHAN_VOICE, "misc/unas_feed.wav", 1.0f, 0.8f );
		return;
	}

	if( pev->team == 2 )
	{
		// --- Goa'uld talent set ---
		switch( m_iSgtcPortraitShown )
		{
		default:
			return;
		case 1:
			// quick "Hand Grenade" toss. Gate: active item id != 12,
			// pev->weapons bit 12 set, ammo > 0, cooldown past.
			{
				BOOL bReady = TRUE;
				if( ( m_pActiveItem && m_pActiveItem->m_iId == 12 ) ||
					!( pev->weapons & ( 1 << 12 ) ) ||
					m_rgAmmo[GetAmmoIndex( "Hand Grenade" )] < 1 ||
					gpGlobals->time <= m_flTalentCooldown )
					bReady = FALSE;
				if( !bReady )
					return;
				SnagInvis();
				m_rgAmmo[GetAmmoIndex( "Hand Grenade" )]--;
				m_flTalentCooldown = gpGlobals->time + 1.5f;

				Vector vecAngles = pev->v_angle + pev->punchangle;
				if( vecAngles.x < 0.0f )
					vecAngles.x = vecAngles.x * 1.1111112f;	// rodata 0x1d7d78 pair
				else
					vecAngles.x = vecAngles.x * 0.8888889f;
				vecAngles.x -= 10.0f;
				float flSpeed = ( 90.0f - vecAngles.x ) * 4.0f;
				if( flSpeed > 500.0f )
					flSpeed = 500.0f;

				UTIL_MakeVectors( vecAngles );
				Vector vecStart = pev->origin + pev->view_ofs +
					gpGlobals->v_forward * 16.0f;
				Vector vecAim = gpGlobals->v_forward * flSpeed +
					pev->velocity;
				CGrenade *pNade = CGrenade::ShootTimed( pev, vecStart,
					vecAim, 3.0f );
				if( pNade )
				{
					*(int *)((char *)pNade + 0xf8) = 1;	// [INFERENCE] ref +0xf8 arm flag
					pNade->pev->team = pev->team;
				}
				if( m_rgAmmo[GetAmmoIndex( "Hand Grenade" )] == 0 )
					pev->weapons &= ~( 1 << 12 );
				return;
			}
		case 2:
			goto sgtc_talent_zatboom;
		case 3:
			// armor msg + sneak toggle (view_ofs -10 / +28).
			if( gpGlobals->time <= m_flTalentCooldown )
				return;
			if( !m_bNukeViewSnap && pev->waterlevel == 0 )	// ref pev+0x1c0
			{
				SetAnimation( (PLAYER_ANIM)4 );
				m_bNukeViewSnap = 1;
				pev->view_ofs = Vector( 0.0f, 0.0f, -10.0f );
				MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pev );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 10 );
					WRITE_BYTE( 0 );
				MESSAGE_END();
			}
			else
			{
				m_bNukeViewSnap = 0;
				pev->view_ofs = Vector( 0.0f, 0.0f, 28.0f );
				MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pev );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 4 );
					WRITE_BYTE( 0 );
				MESSAGE_END();
			}
			m_flTalentCooldown = gpGlobals->time + 1.0f;
			return;
		case 4:
			// shield buff arm: stamina refill + shield-trace timers.
			if( m_iSgtcTalentDeploy != 0 )
				return;
			m_iStamina = 100;
			SGTC_UpdateStamina();
			m_bZatTouchFlag = 0;	// ref +0xa88
			m_bShieldTraceHit = 1;	// ref +0xa78
			m_flSgtcLatch3fc = gpGlobals->time;
			m_flShieldTraceTime = gpGlobals->time - 3.0f;	// ref +0xa68
			m_flSgtcLatchA8c = gpGlobals->time;
			UTIL_ScreenFade( this, g_vecZero, 10.0f, 10.0f, 0, 0 );
			m_iSgtcTalentDeploy = 1;
			m_flTalentCooldown = gpGlobals->time + 10.0f;
			return;
		case 5:
			// monster_shieldemit deploy. If an owned emit already exists
			// (entity +200 owner member), only the proximity retag runs.
			{
				CSGTCShieldEmit *pOwned = NULL;
				CBaseEntity *pScan = NULL;
				while( ( pScan = UTIL_FindEntityByClassname( pScan,
					"monster_shieldemit" ) ) != NULL )
				{
					CSGTCShieldEmit *pEmit = (CSGTCShieldEmit *)pScan;
					if( pEmit->m_pSgtcOwner == this )
						pOwned = pEmit;
				}
				if( gpGlobals->time <= m_flTalentCooldown )
					return;
				if( !pOwned )
				{
					Vector vecAngles = pev->v_angle + pev->punchangle;
					UTIL_MakeVectors( vecAngles );
					Vector vecStart = GetGunPosition();
					Vector vecDir = gpGlobals->v_forward * 360.0f;	// [INFERENCE] scalar rodata
					TraceResult tr;
					UTIL_TraceLine( vecStart, vecStart + vecDir,
						dont_ignore_monsters, edict(), &tr );
					if( SGTC_Endo_distance3d( pev->origin,
							tr.vecEndPos ) < 150 &&
						!( pev->button & ( IN_JUMP | IN_DUCK ) ) )	// ref pev+0x180 bits 1,2
					{
						UTIL_TraceLine( vecStart, vecStart + vecDir,
							dont_ignore_monsters, edict(), &tr );
						UTIL_TraceLine( tr.vecEndPos,
							tr.vecEndPos + Vector( 0.0f, 0.0f, -24.0f ),
							dont_ignore_monsters, edict(), &tr );
						CBaseEntity *pNew = CBaseEntity::Create( "monster_shieldemit",
							tr.vecEndPos, g_vecZero, edict() );
						if( pNew )
						{
							pNew->pev->team = pev->team;
							((CSGTCShieldEmit *)pNew)->m_pSgtcOwner = this;
							m_iSgtcTalentDeploy = 1;
							m_flTalentCooldown = gpGlobals->time + 2.0f;
						}
					}
				}
				else
				{
					// proximity retag: ref @0x14656f cmp eax,0x95 (Endo_distance3d
					// int result <=149) + Endo_takefromlarger(floor z) < 70.0f
					// @0x146873 — NOT <80.
					if( SGTC_Endo_distance3d( pev->origin,
							pOwned->pev->origin ) <= 149 &&
						SGTC_Endo_takefromlarger(
							(int)floorf( pev->origin.z ),
							(int)floorf( pOwned->pev->origin.z ) ) < 70 )
					{
						pOwned->m_iSgtcMode = 2;	// [INFERENCE] ref +0x104
						m_flTalentCooldown = gpGlobals->time + 2.0f;
					}
				}
				return;
			}
		}
	}
	else
	{
		// --- Torri / other talent set ---
		switch( m_iSgtcPortraitShown )
		{
		case 1:
			// monster_larve deploy (invis break), once per arm.
			if( m_iSgtcTalentDeploy == 0 &&
				m_flTalentCooldown < gpGlobals->time )
			{
				SnagInvis();
				UTIL_MakeVectors( pev->v_angle );
				Vector vecStart = pev->origin + pev->view_ofs +
					gpGlobals->v_forward * 16.0f;	// ref @0x145b2c: origin+view_ofs+fwd*16
				Vector vecVel = gpGlobals->v_forward * 300.0f;	// ref rodata 300.0f
				CBaseEntity *pLarve = CBaseEntity::Create( "monster_larve", vecStart,
					pev->v_angle, edict() );
				if( pLarve )
				{
					pLarve->pev->velocity = vecVel;
					((CSGTCLarve *)pLarve)->m_iSgtcFlag = 1;	// [INFERENCE] ref +0xd8
				}
				m_bSgtcCloakTick = 0;
				m_iSgtcTalentDeploy = 1;
			}
			break;
		case 2:
			// armor on/off toggle (0.5s spam gate).
			if( m_iSgtcTalentDeploy == 0 &&
				m_flTalentCooldown < gpGlobals->time )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pev );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 3 );
					WRITE_BYTE( 0 );
				MESSAGE_END();
				m_iSgtcTalentDeploy = 1;
				m_flTalentCooldown = gpGlobals->time + 0.5f;
			}
			if( m_iSgtcTalentDeploy != 0 &&
				m_flTalentCooldown < gpGlobals->time )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgArmor, NULL, pev );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 4 );
					WRITE_BYTE( 0 );
				MESSAGE_END();
				m_iSgtcTalentDeploy = 0;
				m_flTalentCooldown = gpGlobals->time + 0.5f;
			}
			break;
		case 3:
			// gacgoa info_flash cast while active item id == 10.
			if( m_flTalentCooldown < gpGlobals->time && m_pActiveItem &&
				m_pActiveItem->m_iId == 10 )
			{
				SnagInvis();
				m_flTalentCooldown = gpGlobals->time + 15.0f;
				EMIT_SOUND( edict(), CHAN_VOICE, "weapons/gacgoa.wav", 1.0f, 0.8f );
				Vector vecYawOnly( 0.0f, pev->v_angle.y, 0.0f );
				UTIL_MakeVectors( vecYawOnly );
				Vector vecSpot = pev->origin;
				vecSpot.z += 50.0f;	// ref: +50 rise
				CBaseEntity *pFlash = CBaseEntity::Create( "info_flash", vecSpot,
					pev->angles, NULL );
				if( pFlash )
					pFlash->pev->owner = edict();	// ref local+200 owner store
				SetAnimation( (PLAYER_ANIM)6 );
				SnagInvis();
			}
			break;
		case 4:
			// monster_trapbox toss (max 2 while special mode off).
			if( m_flTalentCooldown < gpGlobals->time &&
				m_iSgtcTalentDeploy < 2 )
			{
				m_flTalentCooldown = gpGlobals->time + 0.1f;
				Vector vecAngles = pev->v_angle + pev->punchangle;
				UTIL_MakeVectors( vecAngles );
				Vector vecStart = GetGunPosition();
				Vector vecDir = gpGlobals->v_forward * 360.0f;	// [INFERENCE]
				TraceResult tr;
				UTIL_TraceLine( vecStart, vecStart + vecDir,
					dont_ignore_monsters, edict(), &tr );
				if( tr.flFraction < 1.0f )
				{
					CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
					if( pHit && !( pHit->pev->flags & FL_MONSTER ) )
					{
						Vector vecAngles2 = UTIL_VecToAngles( tr.vecPlaneNormal );
						Vector vecPlace = tr.vecEndPos +
							tr.vecPlaneNormal * 8.0f;	// [INFERENCE] offset rodata
						CSGTCTrapBox *pBox = (CSGTCTrapBox *)Create(
							"monster_trapbox", vecPlace, vecAngles2, edict() );
						if( pBox )
						{
							pBox->pev->team = pev->team;	// like zatboom deploy: skip allies in sweep
							pBox->m_vecPlace = Vector(
								floorf( vecPlace.x ), floorf( vecPlace.y ),
								floorf( vecPlace.z ) );
							m_iSgtcTalentDeploy++;
							pBox->m_iPlaceSlot = m_iSgtcTalentDeploy;
							m_flTalentCooldown = gpGlobals->time + 0.5f;
						}
					}
				}
			}
			if( m_iSgtcTalentDeploy > 1 && m_bShieldLarge )
				m_iSgtcTalentDeploy = 0;
			break;
		case 5:
			// proximity retag of owned shield emitters (see Goa'uld case 5).
			{
				CSGTCShieldEmit *pOwned = NULL;
				CBaseEntity *pScan = NULL;
				while( ( pScan = UTIL_FindEntityByClassname( pScan,
					"monster_shieldemit" ) ) != NULL )
				{
					CSGTCShieldEmit *pEmit = (CSGTCShieldEmit *)pScan;
					if( pEmit->m_pSgtcOwner == this )
						pOwned = pEmit;
				}
				if( !pOwned )
					return;
				if( SGTC_Endo_distance3d( pev->origin,
						pOwned->pev->origin ) <= 149 &&	// ref cmp eax,0x95 (was <80)
					SGTC_Endo_takefromlarger( (int)floorf( pev->origin.z ),
						(int)floorf( pOwned->pev->origin.z ) ) < 70 )
				{
					pOwned->m_iSgtcMode = 2;	// [INFERENCE] ref +0x104
					m_flTalentCooldown = gpGlobals->time + 2.0f;
				}
				return;
			}
		}
	}
	return;

sgtc_talent_zatboom:
	// Goa'uld talent 2 — monster_zatboom toss while active item id == 0x11
	// and pev->weapons bit 0x11 set.
	if( !m_pActiveItem || m_pActiveItem->m_iId != 0x11 )
		return;
	if( !( pev->weapons & ( 1 << 0x11 ) ) )
		return;
	if( gpGlobals->time <= m_flTalentCooldown )
		return;
	SnagInvis();
	UTIL_MakeVectors( pev->v_angle );
	{
		Vector vecStart = pev->origin + pev->view_ofs +
			gpGlobals->v_forward * 16.0f;	// ref @0x145206: origin+view_ofs+fwd*16
		Vector vecVel = gpGlobals->v_forward * 300.0f;	// ref rodata 300.0f
		CBaseEntity *pZat = CBaseEntity::Create( "monster_zatboom", vecStart, pev->v_angle, NULL );
		if( pZat )
		{
			pZat->pev->velocity = vecVel;
			pZat->pev->team = pev->team;
			pZat->pev->owner = edict();	// ref pev+0x198
		}
	}
	m_flTalentCooldown = gpGlobals->time + 2.0f;
	m_iSgtcTalentDeploy = 1;
	return;
}

// ref CBasePlayer::ResetPortrait(int) @0x13633e — gmsgReset SHORT 0 + BYTE 0;
// dest mode==1 -> MSG_ONE else MSG_ALL. Always clears +0x926 latch.
void CBasePlayer::ResetPortrait( int mode )
{
	if( !gmsgReset )
		return;

	if( mode == 1 )
		MESSAGE_BEGIN( MSG_ONE, gmsgReset, NULL, pev );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgReset, NULL, (edict_t *)NULL );
		WRITE_SHORT( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
	m_bSgtcPortraitLatch = 0;
}

// ref CBasePlayer::TogglePortrait() @0x146412 — MSG_ONE gmsgTogglePortrait,
// no payload (TogglePor registered size 0).
void CBasePlayer::TogglePortrait( void )
{
	if( !gmsgTogglePortrait )
		return;
	MESSAGE_BEGIN( MSG_ONE, gmsgTogglePortrait, NULL, pev );
	MESSAGE_END();
}

// ===========================================================================
// SGTC_GiveFromMenu — ref GiveFromMenu(CBasePlayer*) @nm 0x17ae8a.
// Full ref body decoded from disasm: alive+!observer gate; RemoveAllItems(0);
// reset weapon-set state (+0x4f4); loop the 28 gond4h9wp menu args (+0x9ac[i]),
// granting WPLIST[i] on any nonzero arg; args 0x11/0x12 drive a state machine on
// +0x4f4 instead of a direct give; after the loop a fire-flag bit (pev->flags
// bit9==0) grants WPLIST[0x11] when +0x4f4 armed; solid players get the team
// base weapon (Torri weapon_couteau / Goa'uld weapon_coujaf); finally +0x4e5=0,
// +0x4e7=1.
// WPLIST = 28-entry weapon table (ref nm WPLISTB @0x220a60 = Ghidra 0x230a60;
// sibling WPLIST @nm 0x220c40 has the same 28 names). Verified 2026-10-14 by
// read_bytes: the 28 pointers 0x1eae70..0x1eafdc form a contiguous string chain
// weapon_glstaff .. weapon_goaregen with NO NULL entries — the earlier
// "first 4 entries NULL" read was wrong and shifted every loadout slot by 4.
// ===========================================================================
static const char * const sgtc_WPLIST[28] =
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

void SGTC_GiveFromMenu( CBasePlayer *pPlayer )
{
	if( !pPlayer )
		return;
	// ref: if (!(IsAlive && !IsObserver)) return;
	if( !pPlayer->IsAlive() || pPlayer->IsObserver() )
		return;

	pPlayer->RemoveAllItems( 0 );
	pPlayer->m_iSgtcGondState = 0;		// ref +0x4f4

	for( int i = 0; i < 28; i++ )
	{
		if( pPlayer->m_iSgtcGondArgs[i] == 0 )	// ref gates on nonzero, not ==1
			continue;

		pPlayer->m_bSgtcMenuLockLatch = 1;	// ref +0x4e7
		if( i == 0x11 || i == 0x12 )
		{
			// ref: state machine on +0x4f4 (no item give for these two slots).
			if( pPlayer->m_iSgtcGondState == 0 && i == 0x11 )
				pPlayer->m_iSgtcGondState = 1;
			if( pPlayer->m_iSgtcGondState == 1 && i == 0x12 )
				pPlayer->m_iSgtcGondState = 3;
			if( pPlayer->m_iSgtcGondState == 0 && i == 0x12 )
				pPlayer->m_iSgtcGondState = 2;
			if( pPlayer->m_iSgtcGondState == 2 && i == 0x11 )
				pPlayer->m_iSgtcGondState = 3;
		}
		else if( sgtc_WPLIST[i] )
		{
			pPlayer->GiveNamedItem( sgtc_WPLIST[i] );
		}
	}

	// ref: if (+0x4f4 != 0 && !(pev->flags bit9)) GiveNamedItem(WPLIST[0x11]).
	if( pPlayer->m_iSgtcGondState != 0 && !( ( pPlayer->pev->flags >> 9 ) & 1 ) )
	{
		if( sgtc_WPLIST[0x11] )
			pPlayer->GiveNamedItem( sgtc_WPLIST[0x11] );
	}

	// ref: solid && !observer players get a team base weapon.
	// Wire teams (ref pev+0x1ac): 1 = Goa'uld -> weapon_coujaf,
	// 2 = Torri -> weapon_couteau — same mapping as the
	// CStargateTeam{DM,CP}::PlayerSpawn tail (ref @0x1dbabc/0x1dbacb).
	if( !pPlayer->IsObserver() && pPlayer->pev->solid != 0 )
	{
		if( (int)pPlayer->pev->team == 1 )
			pPlayer->GiveNamedItem( "weapon_coujaf" );
		else
			pPlayer->GiveNamedItem( "weapon_couteau" );
	}

	pPlayer->m_bSgtcMenuLock = 0;			// ref +0x4e5
	pPlayer->m_bSgtcMenuLockLatch = 1;		// ref +0x4e7
}
