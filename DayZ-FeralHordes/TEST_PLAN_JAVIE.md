# Feral Hordes: test plan for Javie's server

For Claude running on Javie's PC. Read `DAYZ_SERVER_NOTES.md` and the latest handoff first;
their hard rules apply (back up saves first, no restart of the main server without Javie's go,
stop servers with CloseMainWindow and never kill them, Developer Mode stays off).

## 1. Get the files
- Download `FeralHordes_ready.zip` from GitHub repo `Warpepp3r/NomNom`, branch
  `claude/dayz-zombie-days-gone-mod-vvoapt`, folder `DayZ-FeralHordes/`.
  (Or use the copy Javie downloaded from chat.)
- Unzip so you get `D:\GAMES\STEAM\steamapps\common\DayZServer\@FeralHordes\Addons\FeralHordes.pbo`.
- No bikey is needed: it is a server-only mod, so clients never load it.

## 2. Test server first (script memory is at 82%)
- Use the test setup: `serverDZ_test.cfg`, port 2402, `-profiles=profiles_test`, instanceId 9.
- Do NOT use `test_mods*.bat` (they `taskkill /f` every DayZServer process).
  Start `DayZServer_x64.exe` directly with the same `-mod` list as `start_server.bat`, and change the
  server-mod part to `-serverMod=@Dynamic AI Missions;@FeralHordes`. Only stop that PID.
- Check the newest `profiles_test\script_*.log` and `.RPT` for:
  - `[FeralHordes] Wrote default config` / `Started on 'chernarusplus' with 4 placed hordes`
  - `Safe zone 'Home Base' at 3367 6484`
  - any `SCRIPT (E)` / `Can't compile` lines that name an `FH_*.c` file. Fix those first.
  - the World module memory percentage. Compare it with the 82% from before.

## 3. In-game checks on the test server (set `"DebugLogging": true` first)
| Check | How | Expected |
|---|---|---|
| Sprinting infected | Aggro a zombie | It sprints, not jogs |
| Swarm call | Get spotted near a group | Nearby infected come too |
| Stealth | Crouch-walk past infected | Noticed much later than when sprinting |
| Placed horde | VPP teleport near 6550 / 8250 at night (set time to 22:00) | Log: `Sobor Woods Horde materializes`, pack of about 35 |
| Nest sleeps by day | Same spot at 12:00 | Horde dormant at nest; walking within 35 m wakes it |
| Frenzy | Shoot one member | Whole horde chases; log `goes into a FRENZY`, later `calms down` |
| Roaming horde | Temporarily set `Dynamic.CheckIntervalMinutes` 1, `ChanceNight` 1 | Log `Roaming horde #N spawned 260m from a player` |
| Safe zone | Run from a horde into the home base (3367 / 6484) | Horde gives up at the 200 m edge |
| Damage | Let one hit you | Hits about 1.5× harder (1.875× at night) |
| Despawn | Fly or teleport over 500 m away | Log `dematerializes (N left)` |

If hordes don't follow their route or don't converge, raise `Horde.LureStrength` /
`Horde.FrenzyCallStrength` (see README "Tuning the noise strength").

## 4. Things to watch on this server
- **Expansion AI bots** are PlayerBase too. Infected that chase an AI bot can frenzy a horde
  onto it. That's probably fun, but note it if it causes trouble at AI missions.
- **@Dynamic AI Missions** is also a server mod. Keep both in one `-serverMod=` separated by `;`.
- Other mods that override `ZombieBase.CommandHandler` / `EEHitBy` need to call `super`. If
  zombies stop sprinting, compare the load order and the log.

## 5. Going live (only on Javie's go)
1. Back up `mpmissions\dayzOffline.chernarusplus\storage_1` to `_backups\<date>_before_feral_hordes`.
   (The mod doesn't write world saves, but the rule is the rule.)
2. In `start_server.bat`, change `-serverMod=@Dynamic AI Missions` to
   `-serverMod=@Dynamic AI Missions;@FeralHordes`.
   `join_my_server.bat` does NOT change, because it's a server mod. (The rule about both bats
   keeping the same `-mod` list is unaffected.)
3. Copy the tuned `profiles_test\FeralHordes\config.json` to `profiles\FeralHordes\config.json`.
4. Restart only when Javie says go (closing the window normally).
