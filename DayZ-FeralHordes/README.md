# Feral Hordes — a DayZ server mod

Makes DayZ infected behave like the hordes from survival-horror games such as *Days Gone*:
packs that sprint at you, call each other in, sleep in nests during the day and pour out
across the map at night. The idea is that you can't just jog down a road anymore. You
have to watch the treeline, check the time, and decide whether it's worth driving
through a valley after dark.

> **Note on "reverse engineering".** Nothing here comes from *Days Gone*'s files: no code,
> models, sounds or names. DayZ runs on a completely different engine, so extracted code
> would be useless anyway (and redistributing it would be piracy). Everything is an original
> re-implementation, in DayZ's Enforce Script, of behaviour you can observe while playing.

**Server-side only.** Clients don't need to download anything.

---

## What it does

| Horde-game behaviour | How Feral Hordes does it in DayZ |
|---|---|
| Infected **sprint** at you and don't give up | Infected that are chasing are forced to sprint. Alerted ones jog by day and sprint at night (`Ambient.ChaseSpeed`, `AlertedSpeed*`). |
| One spots you and **the whole pack comes** | **Swarm call**: when an infected starts chasing, it makes a noise at your position that pulls in everything nearby (`SwarmCallStrength`). |
| **Screamers** that call the horde | ~4% of infected are screamers. When one spots you, its shriek frenzies every horde within 600 m. Hordes that haven't spawned in yet start *running* toward you. |
| Big **brutes** | ~6% are brutes. They hit 2× harder and soak 2.5× body damage. Headshots still kill normally. |
| **Hordes** of 30–50 | Placed hordes (with nests and routes) plus roaming hordes that spawn around travelling players. |
| Hordes **sleep in nests by day** and **roam at night** | Each placed horde has a nest and a route. At night it walks the route on a loop; at dawn it goes home and goes dormant. |
| Walking into a nest **wakes it** | Getting within 35 m of a dormant nest (17 m if crouched) sends the whole horde into a frenzy. |
| **Frenzy**: the horde hunts you, then loses interest | A frenzied horde pulses your position every 3 s and sprints after you. The frenzy ends 45 s after the last member loses sight of you, then the horde goes back to its schedule. |
| **Shooting a horde** sets it off | Hitting any horde member frenzies the whole horde (`HitAlertsHorde`). |
| They're **deadlier at night** | Night gives bigger sense radius, faster alerted infected, and +25% damage. |
| **Stealth** works, **noise** gets you killed | Infected notice you within 20 m by day and 40 m at night. Sprinting doubles that, a vehicle triples it, and crouching or prone while moving slowly halves it. |
| Hordes are **finite but come back** | Kill a horde and it stays dead for `RegrowMinutes` (default 90), then refills. |

### How it stays cheap on the server
When no player is near a horde, it exists only as a point on the map and a head count,
and it still walks its route. When a player comes within `MaterializeRadius` (350 m) the
horde spawns in, a few infected per second. When everyone is more than 500 m away it
despawns again but keeps its numbers. `MaxActiveZombies` caps the total number of horde
infected that can exist at once.

---

## Install (no tools needed)

The mod is **already packed** for you: `@FeralHordes/Addons/FeralHordes.pbo`.
A PBO is just DayZ's archive format (like a .zip) that holds a mod's files.

1. Download **`FeralHordes_ready.zip`** from this folder and unzip it. You get a folder named `@FeralHordes`.
2. Copy `@FeralHordes` into your DayZ **server's** main folder (the one with `DayZServer_x64.exe`).
3. Add `-serverMod=@FeralHordes` to the server's launch parameters.
   - **Your own PC server:** in the shortcut or `.bat` that starts `DayZServer_x64.exe`, add it after the other parameters.
   - **Rented server (Nitrado, GTXGaming, etc.):** upload `@FeralHordes` with the host's file manager or FTP,
     then put `@FeralHordes` in the "server mods" / `-serverMod` field of their control panel.
     (Console servers on PlayStation/Xbox can't run script mods.)
4. Start the server once. It creates `profiles/FeralHordes/config.json`. Edit it if you want, then restart.

Players don't download anything. Only the server loads it.

### Re-packing after you change the scripts (optional)
- **Easy way:** `python3 tools/pack_pbo.py Source/FeralHordes @FeralHordes/Addons/FeralHordes.pbo`
- **Official way:** install **DayZ Tools** (free on Steam, under *Library → Tools*), open **Addon Builder**,
  set *Addon source directory* to `Source/FeralHordes`, *Destination* to `@FeralHordes/Addons`, and click **Pack**.

---

## Configuring

All options are in `config.json`. The most useful ones:

**Make it harder or easier**
- `Combat.DamageToPlayerMultiplier`: `1.0` is vanilla, `1.5` is the default, `2.0` is brutal.
- `Ambient.SenseRadiusNight`: how far away infected notice you at night.
- `Dynamic.ChanceNight` / `Dynamic.CheckIntervalMinutes`: how often roaming hordes find you.
- `Ambient.ScreamerChance`, `Ambient.BruteChance`.

**Placing your own hordes** (`Horde.Hordes`)
```json
{
    "Name": "Gorka Valley Horde",
    "Map": "chernarusplus",
    "Enabled": true,
    "Size": 40,
    "RegrowMinutes": 90,
    "Nest":  { "X": 9600, "Z": 8300 },
    "Route": [ { "X": 9500, "Z": 8800 }, { "X": 10400, "Z": 9800 } ]
}
```
- Use map coordinates from iZurvive or your admin tool. X and Z only; height is worked out automatically.
- `Map` must match the world name: `chernarusplus`, `enoch` (Livonia), `sakhal`, or a modded map's name. Leave it empty for any map.
- With an empty `Route`, the horde stays in its nest forever (a "cave" to clear).
- The four Chernarus hordes that ship with the mod are **examples with approximate
  coordinates**. Check them on a map and move the nests where you want them.
- Roaming hordes (`Dynamic`) work on every map with no setup.

**Night window:** `NightStartHour` / `NightEndHour` (default 20:00 to 06:00 in-game time).

**Debugging:** set `"DebugLogging": true` and the server's script log will show every horde
waking, materializing, frenzying and despawning.

### Speed values
`0` idle, `1` walk, `2` jog, `3` sprint, `-1` = don't touch (vanilla AI decides).
DayZ infected can't move faster than their sprint animation, so `3` is the ceiling.

---

## Tuning the noise strength (read this after your first test)

Hordes are steered with DayZ's own **noise system**, the same one that makes infected turn
toward gunshots. That means they path around buildings and fences naturally. How far a
noise carries depends on the `strength = 10` value in `config.cpp` × the `*Strength`
multipliers in `config.json`, and also on weather. The defaults are a starting point. After
a test on your server:

- Hordes don't follow their route, or stragglers get left behind → raise `Horde.LureStrength`.
- A frenzied horde doesn't converge on you → raise `Horde.FrenzyCallStrength`.
- Whole towns aggro off one zombie → lower `Ambient.SwarmCallStrength` or raise `SwarmCallCooldown`.
- Infected notice you through walls too easily → lower `Ambient.SenseNoiseStrength` or the sense radii.

If a pack gets stuck behind terrain, the **leash** quietly moves stragglers back to the
group, but only when no player is within 150 m to see it happen.

---

## More infected in general (optional, vanilla files)

This mod doesn't change your Central Economy. For denser towns, raise the `nominal` of the
`Infected*` events in your mission's `db/events.xml`. Don't go overboard: hordes already
add up to `MaxActiveZombies` on top.

---

## Compatibility and limitations

- The mod hooks `ZombieBase.CommandHandler` / `EEHitBy`, `PlayerBase.EEHitBy` and
  `MissionServer.OnInit`, always calling `super`. It should stack with most mods, including
  other AI mods, as long as they also call `super`.
- Infected spawned by hordes come from script, not the CE, so they carry no CE loot.
- Hordes reset to their nests on every server restart. Their state isn't saved.
- No custom sounds, so the server-only install works. Screamers use the normal infected vocals.
- **Not yet tested in-game.** This was written against the DayZ 1.2x script API but couldn't
  be compiled or run here. Expect a test-and-tune pass. If the server log reports a script
  error at startup, the file and line it names are the place to look.

## Files

```
Source/FeralHordes/
  config.cpp                         CfgPatches / CfgMods / noise template
  Scripts/4_World/FeralHordes/
    FH_Settings.c                    config.json schema + defaults
    FH_Util.c                        helpers, special-infected roles
    FH_Horde.c                       one horde: nest, route, frenzy, spawn in/out
    FH_HordeManager.c                runs all hordes, roaming hordes, events
    FH_ZombieBase.c                  per-infected speed, senses, swarm call, toughness
    FH_PlayerBase.c                  extra damage from infected
  Scripts/5_Mission/FeralHordes/
    FH_MissionServer.c               starts/stops the manager
@FeralHordes/                        drop-in server mod folder (PBO already packed)
FeralHordes_ready.zip                @FeralHordes zipped for download
tools/pack_pbo.py                    re-pack the PBO without DayZ Tools
examples/config.json                 full default config
```
