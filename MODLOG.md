# MODLOG — TARDIS for GTA V

## Setup
- GTA V Enhanced, Steam, `E:\SteamLibrary\steamapps\common\Grand Theft Auto V Enhanced`, `GTA5_Enhanced.exe` 1.0.1158.16. Story mode only.
- ScriptHookV v3889.0 / 1158.13 (dev-c.com). It officially lists Enhanced 1.0.1158.13, and the game is .16, so this may need a newer ScriptHookV.
- SDK 1.0.617.1a in `third_party/shv` (gitignored). MSVC from VS 2022 Build Tools 14.44.
- Source mod: TARDIS Rewrite, Workshop 307175678, `E:\SteamLibrary\steamapps\workshop\content\4000\307175678\fastgmad.gma`. Lua and .mdl files were extracted to the session scratchpad with a GMA parser.

## Route
ScriptHookV + C++ `.asi`, because it is the standard script route for GTA V story mode. Models will be converted Source `.mdl` → `.ydr` with Blender + Sollumz and packed into an `.rpf`, for personal use only.

## Scale
1 GMod unit = 0.0254 m (a 72 u player is 1.83 m tall).
- Toyota exterior: 119 u ≈ 3.0 m.
- Toyota interior: 765×815×413 u ≈ 19×21×10 m.

## Physics constants (sh_flight.lua, sh_float.lua; Source runs them per 66 Hz tick)
- Flight thrust: 15 u/s per tick, so 990 u/s² = 25.1 m/s².
- Flight vertical: 5 u/s per tick, so 8.4 m/s².
- Linear brake: 1% per tick, k = 0.663/s, so cruise top speed ≈ 1493 u/s ≈ 38 m/s.
- Boost: ×2.5 when spin is on, otherwise ×max(1, 1.5).
- Yaw (Alt+A/D): 2 °/s per tick. Angular brake: 1.5% per tick, k = 0.9975/s.
- Spin: twist = −spindir·√(speed_u/200) °/s per tick.
- Lean: equilibrium 0.005° per u/s. Tilt: 5° of roll per held direction key, only while spinning.
- Float: gravity off. W/S/A/D add 1 °/s per tick around the view axes (×2.5 with boost). Space brakes 5% per tick.
- Third-person camera: pivot 60 u above the origin, distance 210 u (wheel zoom 90–500), traced against the world.

## Install / restore
- `./build.bat`, then `bash install.sh`. `bash install.sh --remove` takes it out.
- Game folder additions: ScriptHookV.dll, dinput8.dll, xinput1_4.dll, args.txt (`-nobattleye -noBE`), TARDIS.asi, TARDIS.ini.
- Runtime files written: TARDIS.log, TARDIS_state.ini.

## Architecture
Dependencies only point inward: infra → app → domain.
- `src/domain`: pure, no SDK.
  - `tardis_state.h`: the GMod control rules.
  - `flight_model`: all flight/float physics behind `Step()`.
  - `pilot_camera`: the third-person orbit, with the raycast injected.
  - `math.h`
- `src/app`: the `Tardis` use case, driven by `Update(FrameInput, dt)`. `input.h` holds the actions and bind ids. `ports.h` defines `World` and `Store`.
- `src/infra`: the only layer that includes the ScriptHookV SDK.
  - `GtaWorld` and `GtaInput`: natives and Win32 keys.
  - `IniStore`, `LoadConfig`, `Log`.
- `src/main.cpp`: composition root.
- `tests/tests.cpp`: domain tests, plus `Tardis` tested through `FakeWorld`/`FakeStore`, the second adapter of each port. Run it with `tests\run_tests.bat`.

## Status
- Stage 1 built (2026-10-18). Loads on Enhanced 1.0.1158.13 with ScriptHookV (asiloader.log, ScriptHookV.log). Gotcha: the Enhanced window class is `sgaWindow`, not `grcWindow`, so focus is checked by process id.
- BattlEye: args.txt was ignored on the first launch (GTA5_Enhanced_BE.exe ran and blocked xinput1_4.dll). The second launch ran GTA5_Enhanced.exe without BE.
- Exterior model, as the user chose (2026-10-18): TardisV (`Downloads\97d8da-TardisV.rar`, 2016, Legacy). It replaces the Maverick, so the mod spawns vehicle `maverick` with its doors locked. It needs:
  - Gen9 conversion with CodeWalker 30 dev48+ (Discord only), Tools > Asset Converter;
  - OpenRPF 0.3 (`OpenRPF.asi`, installed);
  - the modded `mods/x64e.rpf/levels/gta5/vehicles.rpf`.
  Legacy files crash on Enhanced.
  - Done 2026-10-18: `tools/convert_gen9.ps1` (CodeWalker.Core: `RpfManager.IsGen9 = true`, Load, Save) and `tools/install_model.ps1` (copies x64e.rpf to `mods/`, sets the vehicles.rpf chain to OPEN encryption, `RpfFile.CreateFile`). Keys load from the GTA folder (`GTA5Keys.LoadFromPath(gtaDir, gen9: true)`).
  - Maverick lives in `x64e.rpf\levels\gta5\vehicles.rpf` on Enhanced too.
  - Model: 26 geometries, no script_rt textures, bounds z −1.01..2.31, so 3.32 m tall (GMod Toyota is 3.0 m).
- Gotcha: `SET_ENTITY_HAS_GRAVITY` does not turn gravity off for vehicles. `SET_VEHICLE_GRAVITY` is needed too, or flight-up (8.4 m/s²) loses to gravity.
- Flight off keeps the spin: the use case hands its angular velocity to game physics (`SET_ENTITY_ANGULAR_VELOCITY` 0x8339643499D1222E, world space, rad/s assumed).
- Sounds (pulled forward from stage 3): miniaudio 0.11.21 plays WAVs pulled from the GMA by `tools/extract_sounds.py` into `GTA/TARDIS/sounds`:
  - flight loop: pitch (95 + min(speed_u/250, 15))/100, volume 0.75, doppler;
  - spawn / delete;
  - landing: thud below −100 u/s, crash below −1500 u/s, 2 s apart.
  The listener is GTA's final rendered camera. A watchdog pauses the audio engine when script frames stop (pause menu).
- Stage 1 stand-ins: model `prop_portaloo_01a`, and E next to the exterior to pilot (stage 2 moves this to the console).

## Scope changes (user, 2026-10-18)
- No vortex at all: not in stage 3 (demat goes straight to mat at the destination), and no vortex flight (`[`, vortex-toggle) in stage 4.
- No tracking (X, tracking-rotation): stage 4 is cloak (L) and physlock (G) only.
- No power (P) and no health. Shields, repair and HADS go with them, since they hang off health. The TARDIS is always indestructible. `TardisState.power` was removed.
