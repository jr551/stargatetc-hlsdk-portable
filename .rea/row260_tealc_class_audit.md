# ROW-260 — Teal'c (`tealc`) class audit

Class 4, team Torri. Verified against `ref/stc_i386.so` (server 2.0),
`ref/sgtc2_dll/*` (client 2.0), the port at `hlsdk-portable` `1fb6258`
(branch `master (ROW-276)`; ROW-276 + ROW-265 + ROW-263 + ROW-257
dependencies included) and a live A/B on isolated Xvnc, 1024×768, map
`ctp_amon`, one game at a time (`:89` first pass; `:78` re-verify 2026-10-23).
Orig = wine `xash3d.exe` (FWGS 0.49.20 win32) with the shipped
`hl.dll`/`client.dll` md5 `ee24eb75`/`82ab4d29`; port = `build-sgtc`
`hl.so`/`client.so` md5 `9d41a170…`/`5ae812bd…` (engine Xash3D FWGS 0.49.21
linux-i386 build 4221).

**Revision 3** (2026-10-24): gates re-run on canonical `stargatetc-hlsdk-portable`
`1fb6258` (exports 95/0, client_hud errors=0, logic 52/52); evidence moved to
this repo on branch `row-260`.

**Revision 2** (2026-10-23, peer review): the third-person and menu-60 shell
captures were re-taken. The previous `row260_orig_model_tealc_3p.png` was a
first-person frame mislabelled as third person; it has been replaced with a
genuine third-person capture of the Teal'c body. The previous
`row260_orig_m60_tealc.png` was a menu-12 frame; it has been replaced with a
menu-60 shell. Mismatch #3 was rewritten accordingly.

**Verdict:** all five ticket criteria are **MATCH** on their verifiable
surfaces. No source change is required; no hlsdk-portable commit was made.
Third-person *appearance* is verified on the original only — the port
client's camera is MP-gated (client-wide, mismatch #3), so the port side
rests on the byte-identical model asset and its verified load. The
documented deltas and one intermittent crash are listed below.
All captures are in `docs/screenshots/row260_*` (14 files).

## 0. Identity chain (port + ref)

| Stage | Value | Evidence |
|---|---|---|
| Menu-11 Torri cell | index 3 (0-based, 4th row), label `#Tealc`, info `info/tealc.txt`, TGA `tealc` | `cl_dll/sgtc_vgui_panels.cpp:580` (list), `:813-826` (row/team) |
| Cell command (port) | `classe 4 1` (argv1 class slot 1..5, argv2 team 1 Torri) | `sgtc_vgui_panels.cpp:828-830`; ref client `FUN_1002d080` `"classe %d %d"` @`0x1002f56e` |
| Orig (shipped) client verbs | separate `classe 4` (class) + `jointeam 1` (team); both pending fields must be set to spawn | `.rea` spawn-gate verdict, same shipped binaries |
| Pending team/class | `+0x438 = 1` (Torri), `+0xa44 = 4` | ref `ClientCommand` `0x191336`; port `sgtc_rules.cpp:1585-1768` |
| Server team/class | `pev->team = 2` (wire; Torri), `m_iSgtcPortraitShown = 4` | port `sgtc_rules.cpp:990-1006` |
| Model key | `tealc` → `models/player/tealc/tealc.mdl` | `sgtc_rules.cpp:2015,2944` |
| HUD avatar id | 3 → avatar `tealc` (0-based table; Torri = cls−1) | `cl_dll/sgtc_hud.cpp:1449`; `SGTC_PortraitSprId` `sgtc_rules.cpp:2074-2084` (ROW-265) |
| Portrait assets | `gfx/vgui/1024_tealc.tga` md5 `b30b3a29…`, `640_tealc.tga` `efe0e19f…`, `sprites/avatars/tealc.spr` `ea35fc3f…` | present in the assets tree, byte-identical to the original install |

`jointeam <n>` alone is class-inert on both servers (team verb only);
class selection is `classe <class> <team>` on the port, and `jointeam` +
`classe` on the shipped orig — the ticket wording is corrected here, the
same way ROW-258/ROW-265 corrected it.

## 1. VGUIMenu 11 — team/class select

Captures: `row260_port_m11_tealc.png` (Teal'c cell highlighted on the Torri
side, `info/tealc.txt` panel — right edge clipped per mismatch #2 — and the
Teal'c portrait) and `row260_orig_m11_tealc.png` (same cell + complete blurb
+ 1024_tealc.tga portrait on the shipped build).

- Label resolves through `stargatetc/Titles.txt:244-246` (`Tealc { Teal'c }`),
  byte-identical in the original package.
- Info panel loads `info/tealc.txt` via `SgtcPanels_LoadFile`; fallback
  `fichier absent %s` is the ref string (`ref` rodata `0x100b43a0`).
- Torri column is the left list; `classe 4 1` selects Teal'c live on the
  port (`JoinMark id 11` → `game_playerspawn`, HUD avatar `tealc`, green
  Torri radar, `models/player/tealc/tealc.mdl` active). On the orig the
  equivalent is `jointeam 1` + `classe 4`; the live capture shows the same
  cell and blurb.

## 2. Player model

- Assets are byte-identical to the original package (`cmp`/md5):
  `tealc.mdl` md5 `6d338111fee3c085e24ca0d7cbd0224f`, 811096 bytes, in both
  the assets tree and the original install
  (`…/stargatetc-portable-main/stargatetc/models/player/tealc/tealc.mdl`).
  `models/player/tealc/` contains exactly one file; there is no
  `tealcXXL.mdl` variant.
- MDL structure (header parse, independently re-run this pass): 1 bodypart
  (`body`) → 1 submodel (`tealc`), 38 bones, 85 sequences, 4 attachments,
  2 textures (`tealc1.bmp` face, `tealc2.bmp` clothing), **1 skin family**
  referencing both textures. All submodels/skins are therefore exercised by
  the default skin; nothing is left unloaded. The two 512×512 embedded
  textures decode to the Teal'c face (gold emblem) and the black
  shirt/green camo-trouser body.
- Precache: `dlls/client.cpp:858` `PRECACHE_MODEL("models/player/tealc/tealc.mdl")`
  in `ClientPrecache()` (every map load); ref `ClientPrecache`
  `0xc5f67→0x1cc668`. Live port log (`:78` re-verify):
  `[17:48:12] loading models/player/tealc/tealc.mdl` at map load, with no
  error-model / fallback line (the log's `CL_FireEvent: not precached` is an
  unrelated weapon event, not a model).
- Third person, original: the shipped client has **no** `thirdperson`,
  `chase_active` or `cam_thirdperson` command (string check), but it does
  register the HLSDK `cam_command` cvar and honours value 1 in `CAM_Think`.
  `cam_command 1` in MP therefore gives a working third-person camera, and
  `row260_orig_model_tealc_3p.png` is a genuine capture of the class body:
  bald dark-skinned head, black shirt, green camo trousers — i.e. the
  `tealc1`/`tealc2` textures above. (This replaces the previous revision's
  file, which was a first-person staff-weapon frame mislabelled as third
  person.)
- Third person, port: blocked by the stock HLSDK MP gate in the port client
  (`cl_dll/in_camera.cpp:172`: `GetMaxClients() > 1 && CL_IsThirdPerson()`
  → `CAM_ToFirstPerson()`), so `cam_command 1` is reverted every frame —
  live proof `row260_port_3p_gated.png` (console reports
  `"cam_command" is "1"` while the view stays first person). Single-player
  mode (`maxplayers 1`) opens the gate but does not run the SGTC class
  rules (`classe` is unknown there and the server spawns the default
  model), so a port class-3p capture is not obtainable. Client-wide, not
  class-specific — mismatch #3.
- First person / HUD identity: `row260_port_model_tealc_1p.png` (live knife
  spawn). The bottom-right own-cell avatar is `sprites/avatars/tealc.spr`
  (bald head with gold emblem; the capture's blue cast is the additive
  draw `0xa8,0xd3,0xff` at `cl_dll/sgtc_hud.cpp:1663-1675`), plus the
  `1024_tealc.tga` class portrait in both menu-11 captures.
- Net: the model asset, its precache, and its in-world appearance on the
  original are verified. The port's third-person *appearance* is not
  capturable because the port client's camera is MP-gated (mismatch #3);
  the port load and the byte-identical file are the port-side basis.

## 3. VGUIMenu 12 — loadout

Captures: `row260_port_m12_tealc.png` and `row260_orig_m12_tealc.png`.
Both show exactly the same 15 visible rows in the same two-column layout
(names and costs identical):

Glider Staff 40 · USAS 12 40 · Beretta 92 5 · MP5 7 · SPAS-12 8 · M16A2 8 ·
Zat'ni'katel 6 · Staff Weapon 7 · T.E.R. 9 · Hand Grenade 6 · D-32 Land Mine 7 ·
C4 Pack 13 · Epinephrine Pen 8 · Smoke Grenade 5 · Narcotic Grenade 6.

- Row gate: `SetWeaponAllow` `g_wa_team2` class-4 row
  (`dlls/sgtc_rules.cpp:823`) zeroes 13 of 28 slots — batsup, psg1, p90, main,
  zatarc, tacgun, invis ×2, gacgoa, larve, **nerve grenade**, mustard,
  goaregen. The 15 remaining slots are exactly the rows above; the blocked
  rows keep empty outlines with black cost text. Byte-verified against ref
  `0x17a26c` in ROW-267 and re-checked here against the live orig capture.
- Costs come from the 28 `wpnC*` cvar defaults (`sgtc_round.cpp:202-229`),
  cross-checked against the orig screenshot (all 15 match).
- Apply (`:1718-1733`, raw 28 flags → `gond4h9wp`) and Close
  (`#Annuler` dismiss, `:1462`) work: applying **PACK 2** dismissed the menu
  and equipped the Zat'ni'katel (`row260_port_apply_pack2_zat.png`).
  Reset keeps blocked rows hidden (`ApplyServerState`, ROW-276).
- Budget: the shipped orig shows `Total 6`, the port `Total 200` — the known
  cross-server skew from ROW-276/ROW-267; names/costs/rows unaffected.

## 4. VGUIMenu 60 — packs

Captures: `row260_port_m60_tealc.png` (shell, reached after death) and
`row260_orig_m60_tealc.png` (shell, forced with the shipped server's
`vguimenuyoyo 60`; the previous revision's file was a menu-12 frame),
`row260_{port,orig}_m60_tealc_pack1.png`,
`row260_{port,orig}_m60_tealc_pack2.png` (natural hover/selection).

- Exactly **PACK 1 + PACK 2** are offered (`s_SgtcPackSlots[0][3]`,
  `sgtc_vgui_panels.cpp:1774` → `{{8,11,16,23,24},{7,10,12,15,25}}`; packs 3/4
  are `{-1}`), matching ref `FUN_1002b15b` and the ROW-257 button gating.
- PACK 1 blurb = `info/pack1_tealc.txt` byte-for-byte: Combat Knife, M16A2,
  Staff Weapon, D-32 Land Mines, Smoke Grenade, Nerve Gas Grenade.
- PACK 2 blurb = `info/pack2_tealc.txt` byte-for-byte: Combat Knife,
  Zat'ni'katel, T.E.R., Spas-12, Hand Grenades, Narcotic Gas Grenade.
- Both blurbs are identical on the port and the orig captures.
- Apply is table-driven (`SGTC_GiveFromMenu`, `sgtc_player.cpp:2075-2133`):
  PACK 1 → m16, lance, mine, smokegrenade, nervegrenade (+ team knife);
  PACK 2 → spas, zat, reetou, handgrenade, narcogrenade (+ team knife).
  Live port proof: PACK 2 Apply equipped the Zat
  (`row260_port_apply_pack2_zat.png`); PACK 1 Apply equipped the M16A2.

## 5. Stats and special

`info/tealc.txt`: Speed Medium, Weapons Heavy, Armour Light,
Special: Goa'uld Symbiote.

- Ref `CStargateTeamDM::PlayerSpawn` (`rea_dump/CStargateTeamDM__PlayerSpawn.c`
  line 42, ref `0x18e7e2`): class 4 → model `tealc`, maxspeed 275.0,
  `+0x908` armor-HUD cap `0x19` = 25, armor 25.0 (`0x41c80000`),
  `+0x448` cloak arm = 1. Port table `s_SgtcTorriClass[4]`
  (`sgtc_rules.cpp:921`) is identical; maxspeed applies via the ROW-265 latch.
- Teal'c is the only Torri class with `cloak = 1`; the port arms
  `m_bSgtcCloakTick` and runs the ref cloak pulse
  (`sgtc_player.cpp:701-719`, ref `0x139b5d`: `EF_NODRAW` while
  `pev->health == 100`, 0.2 s latch). That is the class's only special
  gameplay hook.
- "Goa'uld Symbiote" is display text only: the string exists solely in
  `info/tealc.txt` (0 hits in ref `stc_i386.so`, ref client, shipped
  `hl.dll`/`client.dll`, and all port sources/assets), same conclusion as
  ROW-258's "Protein Marker" and ROW-265's "Goa'uld Symbiote" line.

## 6. Mismatch list

| # | Item | Finding | Action |
|---|---|---|---|
| 1 | `jointeam <n>` selects the class (ticket wording) | team verb only on both servers; class select = `classe <class> <team>` (orig needs both verbs) | wording corrected here, no change |
| 2 | Menu-11 info panel clips the longest lines on the port | the panel is ref-sized (`XRES(409)`/`XRES(209)`, `sgtc_vgui_panels.cpp:847`), but the port resolves the scheme's "Default" font (Times New Roman) to a ~10 % wider substitute; Teal'c's 43-char lines lose their last 2-4 glyphs ("…of Apo"). Orig renders the same file complete. Content loads; geometry is ref-faithful | no change — fixing would require shipping/selecting a metrically matching font or diverging from ref panel geometry |
| 3 | MP third-person surface | orig: no `thirdperson`/`chase_active` command, but `cam_command 1` enables the stock camera in MP (captured, §2). Port: stock HLSDK MP gate forces first person whenever `maxclients > 1` (`cl_dll/in_camera.cpp:172`), so the cvar is set but reverted each frame (`row260_port_3p_gated.png`). Ref client 2.0 carries `3way` instead (`ref/sgtc2_dll/client.dll` `0xb9cdc`). Client-wide, not class-specific | documented; no change in a class audit (would alter the shared client camera for every class). Note: this also means ROW-258's "same observable for the orig build" is not strictly true — the orig camera works via the cvar |
| 4 | Intermittent respawn crash after a pack Apply | 1 of 4 apply+`kill` runs crashed on the respawn (`signal 11 at 0x134`, hl.so, vcall on a null object; engine stack points into the death/spawn path). Three clean runs with PACK 1 and PACK 2 on the release client; the orig respawned cleanly in the same flow. Not reproducible deterministically | not fixed in this pass — flagged; likely the latent weaponbox/drop bug recorded in ROW-267 §6.4 |
| 5 | Menu-12 budget | orig `Total 6` vs port `Total 200` | known cross-server skew (ROW-276), names/costs unaffected |
| 6 | Orig menu-11 class-list paint | under the shipped build the list window can render black over the cells until the WM/focus is up; after focus it renders all five labels | capture-environment artifact, not a port defect |

## 7. Verification

- `cmake --build hlsdk-portable/build-sgtc` and `build-dbg` — exit 0
  (incremental, no source change; re-run 2026-10-23). Deployed
  `stargatetc/dlls/hl.so` md5 `9d41a170…` and `stargatetc/cl_dlls/client.so`
  `5ae812bd…` == build outputs (re-run 2026-10-24 on `1fb6258`).
  Mirror `hlsdk-xash3d/build-sgtc` server builds; its `cl_dll` target still
  fails on the pre-existing `MOTD.cpp`/`death.cpp` SDK mismatch (untouched).
- Gate suite (`hlsdk-portable`): `test_exports.py` PASS — 95 `sgtc_*`
  factories, 0 missing, 0 gmsg missing, engine export contract PASS
  (`GetNewDLLFunctions` non-required); `test_client_hud.py` —
  `client_hud_spec errors=0`; `test_logic.py` —
  `matrix=52 same-behavior=52 verdict-or-detail-diff=0 missing-side=0`.
  Mirror (`SGTC_TREE=hlsdk-xash3d`): exports PASS (95/95 via dynsym),
  `client_hud errors=0`; `test_logic` reports `missing-side` because the
  mirror `build-dbg` server is not rebuilt (mirror `cl_dll` link is
  pre-existing-broken, see above), same state as ROW-258/ROW-267.
- Live port (`:78`, 2026-10-23): fresh `+map ctp_amon`, `classe 4 1` join,
  menu 11/12/60, PACK 1/2 arm + Apply, `kill`/respawn; `loading
  models/player/tealc/tealc.mdl` at map load, no fallback model, no
  `svc_bad`.
- Live orig (`:78`, 2026-10-23): menu 10 → Tau'ri → Teal'c cell click; class
  body confirmed in third person via `cam_command 1`; menu 60 shell via the
  shipped server's `vguimenuyoyo 60`; menu 11/12 and pack captures as
  listed. (Wine input is mouse click + `vncdotool` keys only, per the
  harness note.)
- Tree state: evidence committed on `stargatetc-hlsdk-portable` branch
  `row-260` at `1fb6258` (no source delta; gates re-run 2026-10-24).
- Capture log: `/tmp/row260v2/` (this VM session; `/tmp/row260/` first pass).
