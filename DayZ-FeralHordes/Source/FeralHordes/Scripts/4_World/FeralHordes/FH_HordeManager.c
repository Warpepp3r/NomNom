// =====================================================================
// Feral Hordes - server-side manager (singleton, created by MissionServer)
// =====================================================================

class FH_HordeManager
{
	protected static ref FH_HordeManager s_Instance;

	static const int TICK_MS = 1000;
	static const int SENSE_NOISE_COOLDOWN_MS = 2000;

	protected ref FH_Settings m_Settings;
	protected ref array<ref FH_Horde> m_Hordes;
	protected ref array<PlayerBase> m_Players;
	protected ref array<float> m_PlayerSpeeds;
	protected ref map<int, vector> m_LastPlayerPos;
	protected ref map<int, int> m_LastSenseNoise;
	protected ref NoiseParams m_NoiseParams;
	protected bool m_IsNight;
	protected float m_DynamicTimer;
	protected int m_NextId;
	protected string m_WorldName;
	protected ref array<ref FH_SafeZone> m_SafeZones;

	// -----------------------------------------------------------------
	static void Start()
	{
		if (s_Instance)
			return;
		s_Instance = new FH_HordeManager();
		s_Instance.Init();
	}

	static void Stop()
	{
		if (!s_Instance)
			return;
		s_Instance.Shutdown();
		s_Instance = null;
	}

	static FH_HordeManager GetInstance()
	{
		return s_Instance;
	}

	void FH_HordeManager()
	{
		m_Hordes = new array<ref FH_Horde>;
		m_Players = new array<PlayerBase>;
		m_PlayerSpeeds = new array<float>;
		m_LastPlayerPos = new map<int, vector>;
		m_LastSenseNoise = new map<int, int>;
		m_SafeZones = new array<ref FH_SafeZone>;
	}

	protected void Init()
	{
		m_Settings = FH_Settings.Load();

		m_WorldName = GetGame().GetWorldName();
		m_WorldName.ToLower();

		m_NoiseParams = new NoiseParams();
		m_NoiseParams.LoadFromPath("CfgFeralHordes NoiseHordeCall");

		UpdateTime();
		LoadSafeZones();

		if (m_Settings.Horde.Enabled)
			CreateStaticHordes();

		// First roaming-horde roll happens a bit sooner than the normal interval.
		m_DynamicTimer = m_Settings.Dynamic.CheckIntervalMinutes * 30.0;

		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Tick, TICK_MS, true);
		Print(string.Format("[FeralHordes] Started on '%1' with %2 placed hordes", m_WorldName, m_Hordes.Count()));
	}

	protected void Shutdown()
	{
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(Tick);
		foreach (FH_Horde h : m_Hordes)
		{
			h.Dematerialize(this);
		}
		m_Hordes.Clear();
	}

	protected void LoadSafeZones()
	{
		foreach (FH_SafeZone zone : m_Settings.SafeZones)
		{
			if (!zone || !zone.Enabled || zone.Radius <= 0)
				continue;
			string zoneMap = zone.Map;
			zoneMap.ToLower();
			if (zoneMap != "" && zoneMap != m_WorldName)
				continue;
			m_SafeZones.Insert(zone);
			Print(string.Format("[FeralHordes] Safe zone '%1' at %2 %3, radius %4m", zone.Name, zone.X, zone.Z, zone.Radius));
		}
	}

	bool IsInSafeZone(vector pos)
	{
		foreach (FH_SafeZone zone : m_SafeZones)
		{
			float dx = pos[0] - zone.X;
			float dz = pos[2] - zone.Z;
			if (dx * dx + dz * dz <= zone.Radius * zone.Radius)
				return true;
		}
		return false;
	}

	protected void CreateStaticHordes()
	{
		foreach (FH_HordeDef def : m_Settings.Horde.Hordes)
		{
			if (!def || !def.Enabled || !def.Nest || def.Size <= 0)
				continue;

			string defMap = def.Map;
			defMap.ToLower();
			if (defMap != "" && defMap != m_WorldName)
				continue;

			FH_Horde horde = new FH_Horde(NextId(), def.Name, def.Nest.ToVector(), def.Size, false);
			horde.m_RegrowSeconds = def.RegrowMinutes * 60.0;
			if (def.Route)
			{
				foreach (FH_Point pt : def.Route)
				{
					if (pt)
						horde.m_Route.Insert(pt.ToVector());
				}
			}
			m_Hordes.Insert(horde);
			Log(string.Format("Placed horde '%1' (%2 infected, %3 waypoints)", def.Name, def.Size, horde.m_Route.Count()));
		}
	}

	// -----------------------------------------------------------------
	protected void Tick()
	{
		float dt = TICK_MS / 1000.0;

		UpdateTime();
		UpdatePlayers(dt);

		if (!m_Settings.Horde.Enabled)
			return;

		for (int i = m_Hordes.Count() - 1; i >= 0; i--)
		{
			FH_Horde h = m_Hordes[i];
			h.Update(dt, this);
			if (h.m_Finished)
			{
				h.Dematerialize(this);
				Log(h.m_Name + " is gone");
				m_Hordes.Remove(i);
			}
		}

		if (m_Settings.Dynamic.Enabled)
			UpdateDynamic(dt);
	}

	protected void UpdateTime()
	{
		int year, month, day, hour, minute;
		GetGame().GetWorld().GetDate(year, month, day, hour, minute);

		int start = m_Settings.NightStartHour;
		int end = m_Settings.NightEndHour;
		if (start > end)
			m_IsNight = hour >= start || hour < end;
		else
			m_IsNight = hour >= start && hour < end;
	}

	protected void UpdatePlayers(float dt)
	{
		array<Man> men = new array<Man>;
		GetGame().GetPlayers(men);

		m_Players.Clear();
		m_PlayerSpeeds.Clear();
		map<int, vector> newPositions = new map<int, vector>;

		foreach (Man man : men)
		{
			PlayerBase pb = PlayerBase.Cast(man);
			if (!pb || !pb.IsAlive())
				continue;

			vector pos = pb.GetPosition();
			int pid = pb.GetID();
			float speed = 0;
			vector prev;
			if (m_LastPlayerPos.Find(pid, prev))
				speed = FH_Util.Dist2D(prev, pos) / dt;
			if (speed > 60)
				speed = 0; // teleport / respawn

			newPositions.Set(pid, pos);
			m_Players.Insert(pb);
			m_PlayerSpeeds.Insert(speed);
		}

		m_LastPlayerPos = newPositions;
	}

	// -----------------------------------------------------------------
	// Roaming hordes: spawn out of sight and cross the path of a player.
	protected void UpdateDynamic(float dt)
	{
		FH_DynamicSettings ds = m_Settings.Dynamic;

		m_DynamicTimer -= dt;
		if (m_DynamicTimer > 0)
			return;
		m_DynamicTimer = Math.Max(30.0, ds.CheckIntervalMinutes * 60.0);

		if (CountDynamicHordes() >= ds.MaxConcurrent || m_Players.Count() == 0)
			return;

		float chance = ds.ChanceDay;
		if (m_IsNight)
			chance = ds.ChanceNight;
		if (Math.RandomFloat01() > chance)
			return;

		array<PlayerBase> candidates = new array<PlayerBase>;
		foreach (PlayerBase candidate : m_Players)
		{
			if (candidate && !IsInSafeZone(candidate.GetPosition()))
				candidates.Insert(candidate);
		}
		if (candidates.Count() == 0)
			return;

		SpawnDynamicHordeNear(candidates.GetRandomElement());
	}

	bool SpawnDynamicHordeNear(PlayerBase p)
	{
		FH_DynamicSettings ds = m_Settings.Dynamic;
		vector pp = p.GetPosition();

		for (int attempt = 0; attempt < 8; attempt++)
		{
			float ang = Math.RandomFloat(0, Math.PI2);
			vector dir = Vector(Math.Cos(ang), 0, Math.Sin(ang));
			vector start = pp + dir * ds.SpawnDistance;
			if (FH_Util.IsWater(start[0], start[2]))
				continue;

			// Never pop a horde in right next to somebody else.
			float closest;
			GetNearestPlayer(start, closest);
			if (closest < ds.SpawnDistance * 0.75)
				continue;

			start[1] = GetGame().SurfaceY(start[0], start[2]);
			vector cross = FH_Util.RandomPointAround(pp, 40);
			vector end = pp - dir * ds.SpawnDistance;
			end[1] = GetGame().SurfaceY(end[0], end[2]);

			int size = Math.RandomIntInclusive(ds.SizeMin, ds.SizeMax);
			int id = NextId();
			FH_Horde h = new FH_Horde(id, "Roaming horde #" + id.ToString(), start, size, true);
			h.m_MaxLifetime = ds.MaxLifetimeMinutes * 60.0;
			h.m_Route.Insert(cross);
			if (!FH_Util.IsWater(end[0], end[2]))
				h.m_Route.Insert(end);
			m_Hordes.Insert(h);

			Log(string.Format("%1 (%2 infected) spawned %3m from a player at %4", h.m_Name, size, ds.SpawnDistance, start.ToString()));
			return true;
		}
		return false;
	}

	protected int CountDynamicHordes()
	{
		int count = 0;
		foreach (FH_Horde h : m_Hordes)
		{
			if (h.m_IsDynamic)
				count++;
		}
		return count;
	}

	// -----------------------------------------------------------------
	// Events coming from infected / players
	void OnZombieAggro(ZombieBase z, PlayerBase target)
	{
		if (!z || !target || IsInSafeZone(target.GetPosition()))
			return;

		FH_AmbientSettings a = m_Settings.Ambient;

		FH_Horde horde = z.FH_GetHorde();
		if (horde)
			horde.TriggerFrenzy(target, this);

		if (z.FH_GetRole() == FH_Role.SCREAMER)
		{
			EmitNoise(target, target.GetPosition(), a.ScreamerCallStrength);
			CallHordesTo(z.GetPosition(), a.ScreamerHordeRadius, target);
			Log("A screamer spotted a player at " + target.GetPosition().ToString());
		}
		else if (a.Enabled && a.SwarmCallEnabled && z.FH_CanCall())
		{
			EmitNoise(target, target.GetPosition(), a.SwarmCallStrength);
			z.FH_SetCallCooldown(a.SwarmCallCooldown);
		}
	}

	void OnZombieHitByPlayer(ZombieBase z, PlayerBase attacker)
	{
		if (!z || !attacker || !m_Settings.Combat.HitAlertsHorde || IsInSafeZone(attacker.GetPosition()))
			return;

		FH_Horde horde = z.FH_GetHorde();
		if (horde)
			horde.TriggerFrenzy(attacker, this);
	}

	// Materialized hordes nearby frenzy on the target, far away ones start running toward it.
	void CallHordesTo(vector origin, float radius, PlayerBase target)
	{
		if (!m_Settings.Horde.Enabled)
			return;

		foreach (FH_Horde h : m_Hordes)
		{
			if (FH_Util.Dist2D(h.m_Pos, origin) > radius || h.m_Population <= 0)
				continue;

			if (h.m_Materialized)
				h.TriggerFrenzy(target, this);
			else
				h.CallTo(target.GetPosition(), this);
		}
	}

	// Called by infected that noticed a player through heightened senses.
	void SenseNoise(PlayerBase p, float strength)
	{
		if (!p || IsInSafeZone(p.GetPosition()))
			return;

		int now = GetGame().GetTime();
		int pid = p.GetID();
		int last;
		if (m_LastSenseNoise.Find(pid, last) && now - last < SENSE_NOISE_COOLDOWN_MS)
			return;

		m_LastSenseNoise.Set(pid, now);
		EmitNoise(p, p.GetPosition(), strength);
	}

	void EmitNoise(EntityAI source, vector pos, float strength)
	{
		if (!source || !m_NoiseParams || strength <= 0)
			return;

		NoiseSystem ns = GetGame().GetNoiseSystem();
		if (ns)
			ns.AddNoisePos(source, pos, m_NoiseParams, strength);
	}

	// -----------------------------------------------------------------
	// Queries
	FH_Settings GetSettings()
	{
		return m_Settings;
	}

	bool IsNight()
	{
		return m_IsNight;
	}

	PlayerBase GetNearestPlayer(vector pos, out float dist)
	{
		PlayerBase best = null;
		dist = 1000000000.0;
		foreach (PlayerBase p : m_Players)
		{
			if (!p)
				continue;
			float d = FH_Util.Dist2D(p.GetPosition(), pos);
			if (d < dist)
			{
				dist = d;
				best = p;
			}
		}
		return best;
	}

	float GetPlayerSpeed(PlayerBase p)
	{
		int idx = m_Players.Find(p);
		if (idx < 0)
			return 0;
		return m_PlayerSpeeds[idx];
	}

	int GetActiveZombieCount()
	{
		int count = 0;
		foreach (FH_Horde h : m_Hordes)
		{
			count += h.GetActiveCount();
		}
		return count;
	}

	bool CanSpawnZombie()
	{
		return GetActiveZombieCount() < m_Settings.Horde.MaxActiveZombies;
	}

	int RollRole()
	{
		FH_AmbientSettings a = m_Settings.Ambient;
		float r = Math.RandomFloat01();
		if (r < a.ScreamerChance)
			return FH_Role.SCREAMER;
		if (r < a.ScreamerChance + a.BruteChance)
			return FH_Role.BRUTE;
		return FH_Role.NORMAL;
	}

	float GetDamageToPlayerMultiplier(ZombieBase z)
	{
		FH_CombatSettings c = m_Settings.Combat;
		float mult = c.DamageToPlayerMultiplier;
		if (m_IsNight)
			mult = mult * c.NightDamageMultiplier;
		if (z && z.FH_GetRole() == FH_Role.BRUTE)
			mult = mult * c.BruteDamageMultiplier;
		return mult;
	}

	float GetToughness(ZombieBase z)
	{
		FH_CombatSettings c = m_Settings.Combat;
		float tough = c.ZombieToughness;
		if (z && z.FH_GetRole() == FH_Role.BRUTE)
			tough = tough * c.BruteToughness;
		return tough;
	}

	protected int NextId()
	{
		m_NextId++;
		return m_NextId;
	}

	void Log(string msg)
	{
		if (m_Settings && m_Settings.DebugLogging)
			Print("[FeralHordes] " + msg);
	}
}
