// =====================================================================
// Feral Hordes - settings
// Loaded from $profile:FeralHordes/config.json (created on first start).
// Member names below ARE the JSON keys - do not rename them.
// =====================================================================

class FH_Point
{
	float X;
	float Z;

	void FH_Point(float x = 0, float z = 0)
	{
		X = x;
		Z = z;
	}

	vector ToVector()
	{
		return Vector(X, GetGame().SurfaceY(X, Z), Z);
	}
}

// A hand-placed horde: sleeps in its nest during the day, walks its route at night.
class FH_HordeDef
{
	string Name;
	string Map;                 // world name, e.g. "chernarusplus", "enoch", "sakhal". Empty = any map.
	bool Enabled = true;
	int Size = 30;
	float RegrowMinutes = 90;   // a wiped horde refills after this long (<= 0 = never comes back)
	ref FH_Point Nest;
	ref array<ref FH_Point> Route;

	static FH_HordeDef Make(string name, string map, int size, float nestX, float nestZ)
	{
		FH_HordeDef def = new FH_HordeDef();
		def.Name = name;
		def.Map = map;
		def.Size = size;
		def.Nest = new FH_Point(nestX, nestZ);
		def.Route = new array<ref FH_Point>;
		return def;
	}

	FH_HordeDef AddPoint(float x, float z)
	{
		Route.Insert(new FH_Point(x, z));
		return this;
	}
}

// Behaviour applied to EVERY infected on the server (CE-spawned ones too).
class FH_AmbientSettings
{
	bool Enabled = true;

	// Movement speed forcing. 0 = idle, 1 = walk, 2 = jog, 3 = sprint. -1 = leave vanilla.
	float ChaseSpeed = 3.0;
	float AlertedSpeedDay = 2.0;
	float AlertedSpeedNight = 3.0;

	// Swarm call: when an infected starts chasing it pulls in the ones around it.
	bool SwarmCallEnabled = true;
	float SwarmCallStrength = 2.0;
	float SwarmCallCooldown = 20.0;

	// Heightened senses: infected notice players inside these radii (metres).
	bool SenseEnabled = true;
	float SenseRadiusDay = 20.0;
	float SenseRadiusNight = 40.0;
	float SprintSenseMultiplier = 2.0;   // running makes you easier to notice
	float StealthSenseMultiplier = 0.5;  // crouched / prone and moving slowly
	float VehicleSenseMultiplier = 3.0;  // engines are loud
	float SprintSpeedThreshold = 5.0;    // m/s
	float SenseCooldown = 5.0;
	float SenseNoiseStrength = 1.0;

	// Special infected roles (rolled once per infected).
	float ScreamerChance = 0.04;         // shrieks and summons nearby hordes when it spots you
	float ScreamerCallStrength = 6.0;
	float ScreamerHordeRadius = 600.0;
	float BruteChance = 0.06;            // hits much harder, soaks body shots
}

class FH_CombatSettings
{
	float DamageToPlayerMultiplier = 1.5;   // applies to Health, Blood and Shock
	float NightDamageMultiplier = 1.25;     // stacks on top at night
	float BruteDamageMultiplier = 2.0;      // stacks on top for brutes
	float ZombieToughness = 1.0;            // >1 = body shots do less; headshots always full damage
	float BruteToughness = 2.5;
	bool HitAlertsHorde = true;             // shooting/hitting a horde member enrages the whole horde
}

class FH_HordeSettings
{
	bool Enabled = true;
	int MaxActiveZombies = 250;          // hard cap on horde infected that exist at once (server perf)
	int SpawnBatchSize = 6;              // infected spawned per second while a horde materializes

	float MaterializeRadius = 350.0;     // horde spawns in when a player gets this close
	float DematerializeRadius = 500.0;   // and despawns (keeping its numbers) when everyone leaves
	float SpreadRadius = 25.0;

	float MoveSpeed = 1.6;               // m/s of the horde while roaming
	float CallMoveSpeedMultiplier = 2.5; // when summoned by a screamer
	float WaypointReachRadius = 30.0;
	float LureInterval = 8.0;            // seconds between "follow the pack" lures while roaming
	float LureStrength = 3.0;
	float LeashDistance = 90.0;          // stragglers further than this get regrouped...
	float LeashHiddenDistance = 150.0;   // ...but only when no player is this close to see it

	bool SleepInNestDuringDay = true;
	float NestWakeRadius = 35.0;         // walking into a sleeping nest wakes it (halved while crouched)

	float FrenzyDuration = 45.0;         // how long the horde keeps hunting after losing sight of you
	float FrenzyPulseInterval = 3.0;
	float FrenzyCallStrength = 8.0;
	float FrenzyMaxChaseDistance = 400.0;
	float FrenzySpeed = 3.0;

	ref array<string> Classnames;
	ref array<ref FH_HordeDef> Hordes;
}

// Roaming hordes that spawn around travelling players. Works on any map.
class FH_DynamicSettings
{
	bool Enabled = true;
	float CheckIntervalMinutes = 8.0;
	float ChanceDay = 0.15;
	float ChanceNight = 0.5;
	int MaxConcurrent = 2;
	int SizeMin = 20;
	int SizeMax = 45;
	float SpawnDistance = 260.0;
	float MaxLifetimeMinutes = 30.0;
}

class FH_Settings
{
	static const string DIR = "$profile:FeralHordes";
	static const string PATH = "$profile:FeralHordes/config.json";

	int ConfigVersion = 1;
	bool DebugLogging = false;
	int NightStartHour = 20;
	int NightEndHour = 6;

	ref FH_AmbientSettings Ambient;
	ref FH_CombatSettings Combat;
	ref FH_HordeSettings Horde;
	ref FH_DynamicSettings Dynamic;

	static FH_Settings Load()
	{
		FH_Settings settings = new FH_Settings();
		if (FileExist(PATH))
		{
			JsonFileLoader<FH_Settings>.JsonLoadFile(PATH, settings);
			settings.Validate(false);
			Print("[FeralHordes] Loaded " + PATH);
		}
		else
		{
			settings.Validate(true);
			if (!FileExist(DIR))
				MakeDirectory(DIR);
			JsonFileLoader<FH_Settings>.JsonSaveFile(PATH, settings);
			Print("[FeralHordes] Wrote default config to " + PATH);
		}
		return settings;
	}

	void Validate(bool fresh)
	{
		if (!Ambient)
			Ambient = new FH_AmbientSettings();
		if (!Combat)
			Combat = new FH_CombatSettings();
		if (!Horde)
			Horde = new FH_HordeSettings();
		if (!Dynamic)
			Dynamic = new FH_DynamicSettings();

		if (!Horde.Classnames || Horde.Classnames.Count() == 0)
			Horde.Classnames = DefaultClassnames();

		if (!Horde.Hordes)
		{
			if (fresh)
				Horde.Hordes = DefaultHordes();
			else
				Horde.Hordes = new array<ref FH_HordeDef>;
		}

		NightStartHour = ClampInt(NightStartHour, 0, 23);
		NightEndHour = ClampInt(NightEndHour, 0, 23);

		Ambient.ChaseSpeed = Math.Clamp(Ambient.ChaseSpeed, -1, 3);
		Ambient.AlertedSpeedDay = Math.Clamp(Ambient.AlertedSpeedDay, -1, 3);
		Ambient.AlertedSpeedNight = Math.Clamp(Ambient.AlertedSpeedNight, -1, 3);
		Horde.FrenzySpeed = Math.Clamp(Horde.FrenzySpeed, -1, 3);

		Horde.MaxActiveZombies = ClampInt(Horde.MaxActiveZombies, 0, 2000);
		Horde.SpawnBatchSize = ClampInt(Horde.SpawnBatchSize, 1, 50);
		Horde.LureInterval = Math.Max(1, Horde.LureInterval);
		Horde.FrenzyPulseInterval = Math.Max(1, Horde.FrenzyPulseInterval);
		if (Horde.DematerializeRadius < Horde.MaterializeRadius + 50)
			Horde.DematerializeRadius = Horde.MaterializeRadius + 50;

		Dynamic.SizeMin = ClampInt(Dynamic.SizeMin, 1, 500);
		Dynamic.SizeMax = ClampInt(Dynamic.SizeMax, Dynamic.SizeMin, 500);
		Dynamic.MaxConcurrent = ClampInt(Dynamic.MaxConcurrent, 0, 20);
	}

	static int ClampInt(int value, int min, int max)
	{
		if (value < min)
			return min;
		if (value > max)
			return max;
		return value;
	}

	static array<string> DefaultClassnames()
	{
		array<string> list = new array<string>;
		list.Insert("ZmbM_HermitSkinny_Beige");
		list.Insert("ZmbM_HermitSkinny_Black");
		list.Insert("ZmbM_CitizenASkinny_Blue");
		list.Insert("ZmbM_CitizenASkinny_Grey");
		list.Insert("ZmbM_CitizenASkinny_Red");
		list.Insert("ZmbM_FarmerFat_Beige");
		list.Insert("ZmbM_FarmerFat_Green");
		list.Insert("ZmbM_HikerSkinny_Blue");
		list.Insert("ZmbM_HikerSkinny_Green");
		list.Insert("ZmbM_HunterOld_Autumn");
		list.Insert("ZmbM_JoggerSkinny_Blue");
		list.Insert("ZmbM_PrisonerSkinny");
		list.Insert("ZmbF_CitizenANormal_Beige");
		list.Insert("ZmbF_CitizenANormal_Blue");
		list.Insert("ZmbF_JoggerSkinny_Red");
		list.Insert("ZmbF_JoggerSkinny_Green");
		list.Insert("ZmbF_SurvivorNormal_Blue");
		list.Insert("ZmbF_SurvivorNormal_Red");
		return list;
	}

	// Example hordes for Chernarus. Coordinates are approximate - check them on a map
	// tool (e.g. iZurvive) and move nests into forests/valleys you like.
	static array<ref FH_HordeDef> DefaultHordes()
	{
		array<ref FH_HordeDef> list = new array<ref FH_HordeDef>;

		FH_HordeDef sobor = FH_HordeDef.Make("Sobor Woods Horde", "chernarusplus", 35, 6550, 8250);
		sobor.AddPoint(6100, 7750).AddPoint(7100, 7700).AddPoint(5300, 8600);
		list.Insert(sobor);

		FH_HordeDef coast = FH_HordeDef.Make("Coastal Drifters", "chernarusplus", 30, 8300, 3150);
		coast.AddPoint(8000, 3250).AddPoint(9800, 2350).AddPoint(7300, 2900);
		list.Insert(coast);

		FH_HordeDef north = FH_HordeDef.Make("Airfield Pack", "chernarusplus", 40, 4200, 10500);
		north.AddPoint(3800, 8900).AddPoint(4600, 10200).AddPoint(2700, 10000);
		list.Insert(north);

		FH_HordeDef east = FH_HordeDef.Make("Berezino Swarm", "chernarusplus", 35, 12500, 9800);
		east.AddPoint(12000, 9100).AddPoint(10400, 9800).AddPoint(12300, 10800);
		list.Insert(east);

		return list;
	}
}
