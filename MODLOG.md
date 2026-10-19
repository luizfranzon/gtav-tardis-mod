# MODLOG — TARDIS for GTA V

## Setup
- GTA V Enhanced, Steam, `E:\SteamLibrary\steamapps\common\Grand Theft Auto V Enhanced`, `GTA5_Enhanced.exe` 1.0.1158.16. Story mode only.
- ScriptHookV v3889.0 / 1158.13 (dev-c.com). It officially lists Enhanced 1.0.1158.13, and the game is .16, so this may need a newer ScriptHookV.
- SDK 1.0.617.1a in `third_party/shv`. The repo also holds the runtime (`third_party/runtime`), miniaudio, the extracted sounds (`assets/sounds`) and the gen9 TardisV model (`assets/tardisv-gen9`), for personal use: this repo has no remote and none of it is redistributable. MSVC from VS 2022 Build Tools 14.44.
- Source mod: TARDIS Rewrite, Workshop 307175678, `E:\SteamLibrary\steamapps\workshop\content\4000\307175678\fastgmad.gma`. Lua and .mdl files were extracted to the session scratchpad with a GMA parser.

## Route
ScriptHookV + C++ `.asi`, because it is the standard script route for GTA V story mode. Models will be converted Source `.mdl` → `.ydr` with Blender + Sollumz and packed into an `.rpf`, for personal use only.

## Scale
1 GMod unit = 0.0254 m (a 72 u player is 1.83 m tall).
- Toyota exterior: 119 u ≈ 3.0 m.

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
- Game folder additions: ScriptHookV.dll, dinput8.dll, xinput1_4.dll, args.txt (`-nobattleye -noBE`), OpenRPF.asi, TARDIS.asi, TARDIS.ini.
- Model: `powershell -File tools/install_model.ps1 <CodeWalker> <GTA> <gen9 files>` writes `mods/update/x64/dlcpacks/tardis/dlc.rpf` and `mods/update/update.rpf` (a 3 GB copy with `dlcpacks:/tardis/` in dlclist.xml). Rerun it after a game update. Delete both to remove the model.
- Game folder `TARDIS/`: `sounds/*.wav` and `translations/*.ini`.
- Runtime file written: TARDIS.log. Nothing is saved between sessions (user, 2026-10-18): the TARDIS doesn't exist until F5.

## Architecture
Dependencies only point inward: infra → app → domain.
- `src/domain`: pure, no SDK.
  - `tardis_state.h`: the GMod control rules.
  - `flight_model`: all flight/float physics behind `Step()`.
  - `pilot_camera`: the third-person orbit, with the raycast injected.
  - `math.h`
- `src/app`: the `Tardis` use case, driven by `Update(FrameInput, dt)`. `input.h` holds the actions and bind ids. `ports.h` defines `World` and `Audio`. `text.h` looks up the translated lines.
- `src/infra`: the only layer that includes the ScriptHookV SDK.
  - `GtaWorld` and `GtaInput`: natives and Win32 keys.
  - `LoadConfig`, `LoadText`, `Log`.
- `src/main.cpp`: composition root.
- `tests/tests.cpp`: domain tests, plus `Tardis` tested through `FakeWorld`/`FakeAudio`, the second adapter of each port, with the real en-US.ini. Run it with `tests\run_tests.bat`.

## Status
- Stage 1 built (2026-10-18). Loads on Enhanced 1.0.1158.13 with ScriptHookV (asiloader.log, ScriptHookV.log). Gotcha: the Enhanced window class is `sgaWindow`, not `grcWindow`, so focus is checked by process id.
- BattlEye: args.txt was ignored on the first launch (GTA5_Enhanced_BE.exe ran and blocked xinput1_4.dll). The second launch ran GTA5_Enhanced.exe without BE.
- Exterior model, as the user chose (2026-10-18): TardisV (`Downloads\97d8da-TardisV.rar`, 2016, Legacy). It used to replace the Maverick. Since 2026-10-18 it is the add-on vehicle `tardis` (a DLC pack, see Install), so the Maverick is untouched. The mod spawns `tardis` with its doors locked. It needs:
  - Gen9 conversion with CodeWalker 30 dev48+ (Discord only), Tools > Asset Converter;
  - OpenRPF 0.3 (`OpenRPF.asi`, installed);
  - the `tardis` DLC pack plus a dlclist.xml entry (see Install).
  Legacy files crash on Enhanced.
  - `tools/convert_gen9.ps1` (CodeWalker.Core: `RpfManager.IsGen9 = true`, Load, Save) and `tools/install_model.ps1` (`RpfFile.CreateNew` with OPEN encryption for the pack; `CreateFile` for dlclist.xml in the update.rpf copy). Keys load from the GTA folder (`GTA5Keys.LoadFromPath(gtaDir, gen9: true)`).
  - `tools/dlc/vehicles.meta` is the Maverick's entry from common.rpf, renamed: same handling (`MAVERICK`), layout and no-traffic flags. `carvariations.meta` is the Maverick's first colour set (update.rpf `x64/data/carvariations.ymt`). The files are renamed maverick* → tardis*.
  - Enhanced pack layout, copied from `mp2024_01`: `content.xml`, `setup2.xml`, `common/data/...`, `x64/levels/gta5/vehicles/<name>.rpf`. dlclist.xml is in `update.rpf/common/data`.
  - Model: 26 geometries, no script_rt textures, bounds z −1.01..2.31, so 3.32 m tall (GMod Toyota is 3.0 m).
- Gotcha: `SET_ENTITY_HAS_GRAVITY` does not turn gravity off for vehicles. `SET_VEHICLE_GRAVITY` is needed too, or flight-up (8.4 m/s²) loses to gravity.
- Flight off keeps the spin: the use case hands its angular velocity to game physics (`SET_ENTITY_ANGULAR_VELOCITY` 0x8339643499D1222E, world space, rad/s assumed).
- Flight/float turn the exterior by `SET_ENTITY_ANGULAR_VELOCITY` too, not `SET_ENTITY_QUATERNION`. Gotcha: setting the rotation every frame teleported it through cars it hit, and the game fought the overlap every frame, which caused the shaking and the FPS drop. TARDIS.log prints `flight: turned X deg off the model's rotation` every 2 s, to check the assumed frame.
- While piloting, the ped is made invisible, unarmed and unable to fire every frame. Gotcha: the water turned Franklin visible again, and left click beeped.
- Sounds (pulled forward from stage 3): miniaudio 0.11.21 plays WAVs pulled from the GMA by `tools/extract_sounds.py` into `GTA/TARDIS/sounds`:
  - flight loop: pitch (95 + min(speed_u/250, 15))/100, volume 0.75, doppler;
  - spawn / delete;
  - landing: thud below −100 u/s, crash below −1500 u/s, 2 s apart.
  The listener is GTA's final rendered camera. A watchdog pauses the audio engine when script frames stop (pause menu).
- Add-on vehicle `tardis` verified in game (2026-10-18): it spawns with the TardisV model, and the Maverick is the stock helicopter again.
- E next to the exterior pilots it (this stays: there is no console).
- Gotcha (2026-10-18): attached to the exterior, the pilot was an invisible ped on foot. The police knew where they were but couldn't target them, and tanks found no vehicle to shoot. The pilot now sits in the vehicle's driver seat (`SET_PED_INTO_VEHICLE` -1, not draggable out, doors still locked), so the AI attacks the TARDIS. Attaching is the fallback for a prop exterior or a model with no seat (TARDIS.log says which). Leaving: `CLEAR_PED_TASKS_IMMEDIATELY`, then the exit spot. Not yet verified in game.

## Travel (stage 3, 2026-10-18)
- H while piloting goes to the map waypoint. There is no destination picker, and the key is `teleport-demat` (GMod's default for it is MOUSE_LEFT).
- `domain/teleport.h` holds the GMod sequence (base.lua Exterior.Teleport, Toyota delays):
  - 2.5 s wait, then demat fades alpha 255→200→100→150→50→100→0 at 0.77/tick, about 11.5 s in all;
  - premat lasts 8.5 s from the mat sound;
  - mat fades 0→50→150→100→200→150→255, about 9 s.
  - Sounds: Toyota `demat_ext.ogg` / `mat_ext.ogg`, plus base `demat_fail.wav` when the handbrake blocks it.
- With no vortex, the exterior fades out where it is and waits there, invisible and frozen, until the landing spot is ready. H adds a navmesh region at the waypoint (`ADD_NAVMESH_REQUIRED_REGION`), which keeps the map around the player.
  - Near trip (≤ 300 m): both places stay streamed, and the spot is searched from H, so it is usually ready before the ~11.5 s fade-out ends.
  - Far trip: once it has vanished, the screen fades to black (0.5 s), and `SET_FOCUS_POS_AND_VEL` + `NEW_LOAD_SCENE_START_SPHERE` (150 m) stream the destination in. It lands when the spot is found and `IS_NEW_LOAD_SCENE_LOADED` (8 s at most), then the focus is cleared and the screen fades back in. Gotcha: moving the focus at H unloaded the map the player was still looking at, leaving only the cars.
  - Gotcha: parking it 1000 m above the waypoint (the first try) kept collision and navmesh from streaming at all, and swung the camera into the sky.
  - The spot is settled onto the ground below it (`GET_SAFE_COORD_FOR_PED` returns a ped's root, ~1 m up), and `PlaceExterior` puts the model's base there (`GET_MODEL_DIMENSIONS` min z, 1.01 m for TardisV). It used to put the origin there, so it landed half buried and popped up ~1 m.
  - `GET_SAFE_COORD_FOR_PED` with flags 2|4|8 worked in only 1 of 7 trips (2026-10-18). It now tries 2|4|8, then 4|8, then 4, and logs `safe-coord: flags N`.
  - Picking the landing spot, retrying every frame (times are from when the search starts):
  1. `GET_SAFE_COORD_FOR_PED` (onGround, flags 2|4|8: not isolated, not interior, not water), with `ADD_NAVMESH_REQUIRED_REGION` around the waypoint;
  2. after 3 s, the bare ground (`GET_GROUND_Z_FOR_3D_COORD` from 1000 m);
  3. after 15 s, `GET_CLOSEST_VEHICLE_NODE`.
  The waypoint's own z is never used: the blip at Beeker's (Paleto) said 20 m with the ground at ~31 m, and it landed underground. `CLEAR_AREA` clears 2 m around the spot, then it is put down, the mat sound plays, and after 8.5 s it fades in with physics back on. TARDIS.log gets `travel:` lines with t= seconds since H: waypoint, landing spot via <tier>, vanished, placed (with the invisible wait), landed (1 s into the fade-in), arrived.
- Demat/mat sounds are attached to the exterior (they follow it, like GMod's EmitSound); played at a fixed point, flying away left them inaudible.
- The flight loop ducks from 0.75 to 0.2 over 0.5 s for the whole journey (demat until Arrived), so demat/mat are heard over it.
- Not ported: handbrake during a journey doesn't interrupt it (GMod's InterruptTeleport). E (leave) is blocked mid-journey, and nothing is saved mid-journey.

## Stage 4 and the panel (2026-10-18, not yet verified in game)
- Cloak (L, piloting only): alpha 255 → 0 over 3 s and back (no clip-plane sweep: GTA has no per-pixel clip, and the user dropped it). The exterior shows the lower of the cloak's and the journey's alpha. Phase sounds `phase_enable.wav` / `phase_disable.wav` (`cloak_on.wav` / `cloak_off.wav`). Cloaked: no flight loop, silent demat/mat, collision kept. Not saved.
- Cloak hides the player from the police while piloting (not in GMod): every frame `CLEAR_PLAYER_WANTED_LEVEL`, `SET_MAX_WANTED_LEVEL(0)`, `SET_POLICE_IGNORE_PLAYER`. `GtaWorld::Maintain` gives max 5 back once the TARDIS stops asking (cloak off, or out on foot).
- Physlock (G, piloting only, stays on when you leave): `FREEZE_ENTITY_POSITION`. Flight under it warns (GMod's WarnPhyslockEngaged) and flight/float stop driving the body. No ExplodeIfFast: there is no health.
  - Trains ignore frozen entities, so `HoldTrains` stops them by hand: every train vehicle (`IS_THIS_MODEL_A_TRAIN`, from `worldGetAllVehicles`) whose model box, grown 1 m, contains the TARDIS gets its engine (`GET_TRAIN_CARRIAGE`) held at `SET_TRAIN_SPEED`/`SET_TRAIN_CRUISE_SPEED` 0. The first frame plays the crash sound and shakes the camera. Once the TARDIS is off the train or the physlock is off, the engine gets its old cruise speed back (at least 5 m/s).
- Flight speed (not in GMod): Num 8 / Num 5 change `speedOffset` in km/h, Num 0 resets it. A tap is 1; held, it repeats every 0.1 s after 0.35 s, in 5s from the 5th repeat (`domain/speed_dial.h`). It scales flight's thrust (horizontal and vertical), so the top speed is GMod's ~136.5 km/h + offset (floor 1 km/h, offset ≥ −135). The brake is unchanged, and boost multiplies on top.
- Panel (replaces the legend): only while piloting, console style on the right edge (`GtaWorld::DrawPanel`): a light per system (flight, anti-gravs, handbrake, cloak, physlock), travel with its phase and a progress bar (`Teleport::Progress`), speed and spin in flight, and E to leave. No movement keys.
- Translations: `translations/en-US.ini` and `pt-BR.ini`, key=text UTF-8, wording from TARDIS Rewrite's en.lua / pt-br.lua. The language comes from `GET_CURRENT_LANGUAGE` (0x2BDD44CC428A7EAE, 5 = pt-BR), and the file is laid over en-US line by line.
- Lamp (`domain/lamp.h`, sh_exterior_light.lua): on and pulsing (sin 3.7 rad/s) while floating/flying or travelling, steady with `[Lamp] AlwaysOn=1`, off cloaked or vanished. Drawn as two `DRAW_LIGHT_WITH_RANGE`: GMod's dynamic light (Toyota colour 255 231 95, 5 m, brightness 2.5, radius ±10%) and a tight bright one on the lens standing in for the sprite. It sits `BelowTop` (0.15 m) under the model's top.
- Rotorwash (GMod env_rotorwash_emitter: flight or travel, not cloaked): the game's own helicopter downwash, from update.rpf `vfxvehicleinfo.ymt` (VFXVEHICLEINFO_HELI_GENERIC: hashes resolved to `veh_downwash`, `_sand`, `_dirt`, `_water`, `_foliage` in `core`, 20 m range). A looped ptfx is moved every frame to the ray hit straight below (or the water surface), evolution `speed` 1 at the ground fading to 0 at 20 m. Only `veh_downwash` and `_water` are used so far.

- Shield (K, piloting only, stays on when you leave; not in GMod, whose shields are a health bar). `src/infra/gta_shield.cpp`, every frame while on:
  - Bubble: centred on the model box, radius half its diagonal + 3 m (~5 m for TardisV). Invisible; a hit (or the toggle) shows it for 0.5 s as `DRAW_MARKER` 28 (scale taken as the radius, to check in game) plus a bright spot and `core`/`ent_brk_sparking_wires` sparks. Cloaked: it still works, with no flash.
  - Shots: a ped's `GET_PED_LAST_WEAPON_IMPACT_COORD` that changes on the box within 0.5 s of `IS_PED_SHOOTING` → `SHOOT_SINGLE_BULLET_BETWEEN_COORDS` back at its head with the same weapon (the vehicle weapon if it's in one), 1000 damage, owned by the player (user: it's the player's crime). 0.25 s later a survivor is finished off (ped: health 0, which isn't on the player's record; vehicle: `ADD_OWNED_EXPLOSION` by the player).
  - Projectiles: known models (RPG/homing/firework rockets, 40 mm, vehicle and air missiles, frag, sticky, pipe, smoke, molotov) inside the bubble are deleted and fired back as a matching weapon at the ped that fired lately (15 s) nearest the line it came along, then the same finishing strike after the flight time (40 m/s assumed).
  - Push: peds on foot (ragdoll + `APPLY_FORCE_TO_ENTITY` type 1, ×3 as an impulse), vehicles and loose objects (`SET_ENTITY_VELOCITY`) inside the bubble are thrown once, sized to land 6 m past it: 6 m/s up (~1.2 s airborne) and out at max(15, distance left / airtime × 1.3) m/s, at most once per 2 s each. Gotcha: a flat 12 m/s + 4 up every 0.6 s left cars inside, nudged again and again.
  - Shock on contact (not cloaked): a `WEAPON_STUNGUN` bolt from the bubble to the thing (0 damage, no owner, so no crime), three jagged `DRAW_LINE` arcs redrawn every frame for 0.35 s, and a flickering blue light. Not the player, their vehicle, trains, attached things or the TARDIS. The player's own shots aren't reflected.
  - Sounds: GMod's `power_on.wav` / `power_off.wav` (`shield_on.wav` / `shield_off.wav`).

## Scope changes (user, 2026-10-18)
- No vortex at all: not in stage 3 (demat goes straight to mat at the destination), and no vortex flight (`[`, vortex-toggle) in stage 4.
- No tracking (X, tracking-rotation): stage 4 is cloak (L) and physlock (G) only.
- No power (P) and no health. Shields, repair and HADS go with them, since they hang off health. The TARDIS is always indestructible. `TardisState.power` was removed.
- No interior, no door and no console (stage 2 is dropped). The mod is the exterior only, and E next to it starts piloting.
- No chameleon: the exterior is the TardisV model only, with no Toyota/Legacy switch.
