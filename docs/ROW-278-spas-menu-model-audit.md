# ROW-278 — Spas-12 (`spas` / `weapon_spas`, SGTC id 7) menu & model parity

## Menu 12 (VGUIMenu 12)

| Check | Original (`ref/` + `stargatetc/`) | Port (before) | Port (after) |
|-------|-----------------------------------|---------------|--------------|
| Slot index | 7 (`sgtc_WPLIST[7]` → `weapon_spas`) | 7 | 7 |
| Label token | `#WEAPON_SHOTGUN` → **SPAS-12** (`Titles.txt` L764–766) | `#WEAPON_SHOTGUN` | unchanged (resolves to SPAS-12 via `Titles.txt`) |
| Cost | `wpn_weapon_spas` **10** (`game.cfg` / `server.cfg`) | cvar default **8** | cvar default **10** (`sgtc_round.cpp` `sgtc_cvar_wpnC7`) |
| Preview art | `1024_spas.tga`, `info/spas.txt` | present in assets tree | unchanged |

### Class gating (`SetWeaponAllow`, slot 7)

Slot 7 is **denied** on menu 12 for Tau'ri classes 1–3 and 5 (zero-list includes 7). **Allowed** for Tau'ri class 4 (Jackson). Goa'uld classes all deny slot 7. Matches ref tables in `sgtc_rules.cpp`.

### Pack grants (menu 60)

| Pack | Classes | Slots incl. SPAS (7) |
|------|---------|----------------------|
| PACK 2 | O'Neill (`oneill`, team1 class0) | `{3,7,11,15,23}` — Beretta + **Spas-12** + Staff + HG + Smoke |
| PACK 2 | Teal'c (`tealc`, team1 class3) | `{7,10,12,15,25,26}` |

`info/pack2_oneill.txt` / `pack2_tealc.txt` list **Spas-12** (verbatim in both orig and port assets).

## Grant / ammo

- Entity: `weapon_spas` → `CShotgun` (ref `stc_i386.so` link table).
- Ammo: `buckshot`, max carry **125**, clip **8** (`GetItemInfo` / client `g_SGTCClientWeapons`).
- Default give: **60** buckshot on pickup (ROW-224 spas behaviour parity).

## Models & HUD

| Asset | Status |
|-------|--------|
| `models/v_spas.mdl` | byte-identical orig ↔ port assets |
| `models/w_spas.mdl` | byte-identical |
| `models/p_spas.mdl` | byte-identical |
| `sprites/weapon_spas.txt` + `weapons/spas.spr` | present, identical |

**Fix:** `dlls/shotgun.cpp` (SGTC `weapon_spas` path) now precaches/deploys **v/w/p_spas**, fires `events/spas.sc` / `events/spas2.sc` (ported from ROW-224 / ref). Master previously used stock `v_shotgun` / `w_shotgun` → error/fallback model at runtime.

HUD weapon icon: `weapon_spas.txt` → `sprites/weapons/spas.spr`; drawn via `CHudSgtc::Think` weapon-icon path (ROW-276 HIDEHUD_ALL gate fix on master).

## Menu 12 UX (Apply / Reset / Close / persist)

No SPAS-specific client changes required on master: weapon row uses shared `CSgtcWepSelPanel` (ROW-276 Reset → `ApplyServerState`, Close guard, Progress persistence).

## Build / gates

- `cmake --build build` (Release): **OK** (`hl.so` + `client.so`).
- `stargatetc-portable` `tests/test_logic.py`: **49/52 same-behavior** (pre-existing flashbang/mine/mp5 matrix gaps; unchanged by this ticket).

## Mismatches closed in this PR

1. Server world/view/player models for `weapon_spas` (stock shotgun MDLs → **spas** MDLs).
2. `GetItemInfo` slot/position/ammo/clip/id aligned with client table and ref stub.
3. Menu-12 cost default **8 → 10** to match shipped `game.cfg` / `server.cfg`.
