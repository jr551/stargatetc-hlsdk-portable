# ROW-273 — Beretta 92 (`m92s`, SGTC id 2, menu-12 slot 3)

Buy-menu and model parity audit vs `ref/stc_i386.so` + `ref/sgtc2_dll/client.dll`.
Weapon behaviour is ROW-221; this ticket covers menu 12, grants, ammo/models/HUD only.

## Menu 12 (VGUIMenu 12)

| Check | Ref / original | Port | Verdict |
|-------|----------------|------|---------|
| Slot index | 3 (`m92s` label) | `s_SgtcWeapons[3]` → `"m92s"`, `#WEAPON_BER` (`cl_dll/sgtc_vgui_panels.cpp`) | **MATCH** |
| Display name | `Titles.txt` → `Beretta 92` | `stargatetc/Titles.txt` `WEAPON_BER` / `Beretta 92` | **MATCH** (byte-identical assets) |
| Cost | `wpn_weapon_m92` default **5** (`strings ref/stc_i386.so`) | `sgtc_cvar_wpnC3` `{ "wpn_weapon_m92", "5" }` (`dlls/sgtc_round.cpp:205`) | **MATCH** |
| Shipped cfg tuning key | `wpn_weapon_m92s "5"` in both trees' `game.cfg` / `listenserver.cfg` | same | **shared quirk** — cfg key does not match registered cvar; effective cost remains 5 |
| Tau'ri (team 2) classes | Slot 3 denied for cls 1–5 except cls 3 (Jackson) per `SetWeaponAllow` | same table in `dlls/sgtc_rules.cpp` | **MATCH** ref gate lists |
| Goa'uld (team 1) | Slot 3 allowed all classes | same | **MATCH** |

Live menu-12 economy previously verified in `.rea/draw_audit.md` / `.rea/menu12_parity.md`
(Beretta **5** in weapon list).

## Grants, ammo, clip

| Path | Port |
|------|------|
| Menu Apply (`gond4h9wp` slot 3) | `sgtc_WPLIST[3]` → `weapon_m92s` (`dlls/sgtc_player.cpp`) |
| Spawn tables | `s_SgtcSpawnWeapons[3]` → `weapon_m92s` (`dlls/sgtc_rules.cpp`) |
| Packs | `pack1_jackson`, `pack2_oneill`, `pack2_soldat`, `pack3_soldat` list Beretta 92 (`stargatetc/info/*.txt`) |
| Ammo type | `"9mmC"`, max 60 (`SGTC_GETITEM` / `sgtc_weapons_client.cpp`) |
| Clip | 12 (`SGTC_GETITEM` … `12` clip arg in `dlls/sgtc_wpn_m92s.cpp`) |
| Default grant ammo | `m_iDefaultAmmo = 60` on spawn |

## Models and HUD

| Asset | Port | vs original installed tree |
|-------|------|----------------------------|
| `models/v_m92s.mdl` | precached server + client row | md5 **identical** |
| `models/w_m92s.mdl` | precached | md5 **identical** |
| `models/p_m92s.mdl` | precached | md5 **identical** |
| `models/v_m92sT.mdl` | on disk; not referenced in code (ref client also uses `v_m92s` only) | md5 **identical** |
| HUD | `sprites/weapon_m92s.txt` → `weapons/m92s.spr` | md5 **identical** |
| Preview TGA | `gfx/vgui/1024_m92s.tga` only (all weapons use 1024\_ previews) | md5 **identical** |

## Apply / Reset / Close / persistence

Client builds `gond4h9wp` + 28 slot flags (`CSgtcHandler_WepSelApply`); server stores
`m_iSgtcGondArgs[]` and grants on next `SGTC_GiveFromMenu` / spawn paths. Reset/Close
behaviour matches ref menu-12 handlers in `sgtc_vgui_panels.cpp` (see `.rea/menu12_parity.md`).

## Code changes for ROW-273

**None required.** Prior debug edits (`pfnClientCmd` / console spam) were reverted; port at
`master` already matches ref for this weapon.

## Mismatch list (documented, not port regressions)

1. Shipped `wpn_weapon_m92s` cfg key vs registered `wpn_weapon_m92` — present in **both**
   original and port cfgs; ref server registers `wpn_weapon_m92`.
2. `v_m92sT.mdl` shipped but unused in code — same in ref strings (client uses `v_m92s`).
3. Weapon preview TGAs are `1024_*` only for all slots — not m92s-specific.

## Verification (2026-10-23)

```bash
cmake -S hlsdk-portable -B hlsdk-portable/build-sgtc -DGAMEDIR=$PWD/stargatetc
make -C hlsdk-portable/build-sgtc -j$(nproc)
python3 tests/test_exports.py      # PASS
python3 tests/test_client_hud.py   # errors=0
```

Detailed line-level port notes: `ROW-273` worktree `evidence/port_audit.md` and
`evidence/assets_audit.md`.
