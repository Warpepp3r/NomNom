// =====================================================================
// Feral Hordes - a single horde
//
// A horde is "virtual" (just a position + head count) while no player is
// near, and "materialized" (real infected in the world) when one is.
// This lets hordes travel the whole map at night without costing the
// server anything until you run into them.
// =====================================================================

enum FH_HordeState
{
	DORMANT,    // sleeping in its nest / standing still
	ROAMING,    // walking its route (or toward a screamer's call)
	RETURNING,  // heading back to the nest at dawn
	FRENZY      // hunting a player
}

class FH_Horde
{
	int m_Id;
	string m_Name;
	bool m_IsDynamic;
	bool m_Finished;
	bool m_RouteComplete;
	float m_Age;
	float m_MaxLifetime;

	vector m_Pos;
	vector m_Nest;
	bool m_HasNest;
	ref array<vector> m_Route;
	int m_RouteIndex;

	bool m_HasCall;
	vector m_CallPos;

	int m_Population;
	int m_MaxPopulation;
	float m_RegrowSeconds;
	float m_RegrowTimer;

	int m_State;
	int m_StateBeforeFrenzy;

	bool m_Materialized;
	int m_PendingSpawns;
	ref array<ZombieBase> m_Members;

	float m_LureTimer;
	float m_LeashTimer;
	float m_FrenzyTimer;
	float m_FrenzyPulseTimer;
	PlayerBase m_FrenzyTarget;

	void FH_Horde(int id, string name, vector pos, int population, bool dynamic)
	{
		m_Id = id;
		m_Name = name;
		m_Pos = pos;
		m_Nest = pos;
		m_HasNest = !dynamic;
		m_IsDynamic = dynamic;
		m_Population = population;
		m_MaxPopulation = population;
		m_Route = new array<vector>;
		m_Members = new array<ZombieBase>;
		m_State = FH_HordeState.DORMANT;
		if (dynamic)
			m_State = FH_HordeState.ROAMING;
	}

	string StateName()
	{
		if (m_State == FH_HordeState.ROAMING)
			return "ROAMING";
		if (m_State == FH_HordeState.RETURNING)
			return "RETURNING";
		if (m_State == FH_HordeState.FRENZY)
			return "FRENZY";
		return "DORMANT";
	}

	bool IsFrenzied()
	{
		return m_State == FH_HordeState.FRENZY;
	}

	int GetActiveCount()
	{
		return m_Members.Count();
	}

	// -----------------------------------------------------------------
	void Update(float dt, FH_HordeManager mgr)
	{
		FH_HordeSettings hs = mgr.GetSettings().Horde;
		m_Age += dt;

		PruneMembers();
		UpdateRegrow(dt);
		UpdateSchedule(mgr);
		UpdateMovement(dt, mgr, hs);

		float nearestDist;
		mgr.GetNearestPlayer(m_Pos, nearestDist);

		if (!m_Materialized)
		{
			if (m_Population > 0 && nearestDist <= hs.MaterializeRadius)
				Materialize(mgr);
		}
		else if (nearestDist > hs.DematerializeRadius && m_State != FH_HordeState.FRENZY)
		{
			Dematerialize(mgr);
		}

		if (m_Materialized)
		{
			SpawnPending(mgr, hs);
			UpdateFrenzy(dt, mgr, hs);
			UpdateLure(dt, mgr, hs);
			UpdateLeash(dt, mgr, hs);
			UpdateNestWake(mgr, hs);
		}

		if (m_IsDynamic)
		{
			if (m_Population <= 0)
				m_Finished = true;
			else if (!m_Materialized && (m_RouteComplete || m_Age > m_MaxLifetime))
				m_Finished = true;
		}
	}

	// -----------------------------------------------------------------
	protected void PruneMembers()
	{
		for (int i = m_Members.Count() - 1; i >= 0; i--)
		{
			ZombieBase z = m_Members[i];
			if (!z || !z.IsAlive())
			{
				m_Members.Remove(i);
				m_Population--;
			}
		}
		if (m_Population < 0)
			m_Population = 0;
	}

	protected void UpdateRegrow(float dt)
	{
		if (m_IsDynamic || m_Materialized || m_RegrowSeconds <= 0 || m_Population >= m_MaxPopulation)
		{
			m_RegrowTimer = 0;
			return;
		}

		m_RegrowTimer += dt;
		if (m_RegrowTimer >= m_RegrowSeconds)
		{
			m_RegrowTimer = 0;
			m_Population = m_MaxPopulation;
		}
	}

	protected void UpdateSchedule(FH_HordeManager mgr)
	{
		if (m_State == FH_HordeState.FRENZY)
			return;

		if (m_HasCall)
		{
			m_State = FH_HordeState.ROAMING;
			return;
		}

		if (m_IsDynamic)
		{
			if (m_RouteComplete)
				m_State = FH_HordeState.DORMANT;
			else
				m_State = FH_HordeState.ROAMING;
			return;
		}

		// Nest-only horde: always heads home.
		if (m_Route.Count() == 0)
		{
			if (m_State == FH_HordeState.ROAMING)
				m_State = FH_HordeState.RETURNING;
			return;
		}

		bool active = mgr.IsNight() || !mgr.GetSettings().Horde.SleepInNestDuringDay;
		if (active)
		{
			if (m_State != FH_HordeState.ROAMING)
			{
				m_State = FH_HordeState.ROAMING;
				mgr.Log(m_Name + " leaves its nest");
			}
		}
		else if (m_State == FH_HordeState.ROAMING)
		{
			m_State = FH_HordeState.RETURNING;
			mgr.Log(m_Name + " heads back to its nest");
		}
	}

	protected vector GetMoveTarget()
	{
		if (m_HasCall)
			return m_CallPos;
		if (m_State == FH_HordeState.RETURNING || m_Route.Count() == 0)
			return m_Nest;
		if (m_RouteIndex >= m_Route.Count())
			m_RouteIndex = 0;
		return m_Route[m_RouteIndex];
	}

	protected void UpdateMovement(float dt, FH_HordeManager mgr, FH_HordeSettings hs)
	{
		if (m_State == FH_HordeState.FRENZY)
		{
			if (m_Materialized && m_Members.Count() > 0)
				m_Pos = GetCentroid();
			return;
		}

		if (m_State == FH_HordeState.DORMANT)
			return;

		// Don't let the virtual position run off while a player is watching the pack lag behind.
		if (m_Materialized && m_Members.Count() > 0)
		{
			vector centroid = GetCentroid();
			if (FH_Util.Dist2D(centroid, m_Pos) > hs.LeashDistance)
			{
				float watcherDist;
				mgr.GetNearestPlayer(centroid, watcherDist);
				if (watcherDist < hs.LeashHiddenDistance)
					return;
			}
		}

		vector target = GetMoveTarget();
		vector delta = target - m_Pos;
		delta[1] = 0;
		float remaining = delta.Length();
		if (remaining <= hs.WaypointReachRadius)
		{
			OnWaypointReached(mgr);
			return;
		}

		float speed = hs.MoveSpeed;
		if (m_HasCall)
			speed = speed * hs.CallMoveSpeedMultiplier;

		float step = speed * dt;
		if (step > remaining)
			step = remaining;

		m_Pos = m_Pos + delta.Normalized() * step;
		m_Pos[1] = GetGame().SurfaceY(m_Pos[0], m_Pos[2]);
	}

	protected void OnWaypointReached(FH_HordeManager mgr)
	{
		if (m_HasCall)
		{
			m_HasCall = false;
			return;
		}

		if (m_State == FH_HordeState.RETURNING)
		{
			m_State = FH_HordeState.DORMANT;
			mgr.Log(m_Name + " is back in its nest");
			return;
		}

		m_RouteIndex++;
		if (m_RouteIndex >= m_Route.Count())
		{
			if (m_IsDynamic)
			{
				m_RouteComplete = true;
				m_State = FH_HordeState.DORMANT;
				m_RouteIndex = m_Route.Count() - 1;
			}
			else
			{
				m_RouteIndex = 0;
			}
		}
	}

	// -----------------------------------------------------------------
	void Materialize(FH_HordeManager mgr)
	{
		m_Materialized = true;
		m_PendingSpawns = m_Population - m_Members.Count();
		mgr.Log(string.Format("%1 materializes (%2 infected, %3) at %4", m_Name, m_Population, StateName(), m_Pos.ToString()));
	}

	void Dematerialize(FH_HordeManager mgr)
	{
		if (!m_Materialized)
			return;

		foreach (ZombieBase z : m_Members)
		{
			if (z)
				GetGame().ObjectDelete(z);
		}
		m_Members.Clear();
		m_PendingSpawns = 0;
		m_Materialized = false;
		mgr.Log(string.Format("%1 dematerializes (%2 left)", m_Name, m_Population));
	}

	protected void SpawnPending(FH_HordeManager mgr, FH_HordeSettings hs)
	{
		int budget = hs.SpawnBatchSize;
		while (m_PendingSpawns > 0 && budget > 0 && mgr.CanSpawnZombie())
		{
			budget--;
			m_PendingSpawns--;
			ZombieBase spawned = SpawnMember(mgr, hs);
			if (spawned)
				m_Members.Insert(spawned);
			else
				m_Population--;
		}
	}

	protected ZombieBase SpawnMember(FH_HordeManager mgr, FH_HordeSettings hs)
	{
		if (!hs.Classnames || hs.Classnames.Count() == 0)
			return null;

		string cls = hs.Classnames.GetRandomElement();
		vector pos = FH_Util.RandomPointAround(m_Pos, hs.SpreadRadius);
		Object obj = GetGame().CreateObjectEx(cls, pos, ECE_PLACE_ON_SURFACE | ECE_INITAI | ECE_EQUIP_ATTACHMENTS);
		ZombieBase z = ZombieBase.Cast(obj);
		if (!z)
		{
			if (obj)
				GetGame().ObjectDelete(obj);
			mgr.Log("Could not spawn infected class '" + cls + "' - check Horde.Classnames");
			return null;
		}

		z.SetOrientation(Vector(Math.RandomFloat(0, 360), 0, 0));
		z.FH_SetHorde(this);
		z.FH_SetRole(mgr.RollRole());
		return z;
	}

	// -----------------------------------------------------------------
	void TriggerFrenzy(PlayerBase target, FH_HordeManager mgr)
	{
		if (!target || !target.IsAlive() || mgr.IsInSafeZone(target.GetPosition()))
			return;

		if (m_State != FH_HordeState.FRENZY)
		{
			m_StateBeforeFrenzy = m_State;
			m_State = FH_HordeState.FRENZY;
			mgr.Log(m_Name + " goes into a FRENZY");
		}

		m_HasCall = false;
		m_FrenzyTarget = target;
		m_FrenzyTimer = mgr.GetSettings().Horde.FrenzyDuration;
		m_FrenzyPulseTimer = 0;
	}

	// A screamer (or anything else) summons this horde to a position.
	void CallTo(vector pos, FH_HordeManager mgr)
	{
		if (m_State == FH_HordeState.FRENZY || mgr.IsInSafeZone(pos))
			return;
		m_HasCall = true;
		m_CallPos = pos;
		m_State = FH_HordeState.ROAMING;
		mgr.Log(m_Name + " answers a call toward " + pos.ToString());
	}

	protected void UpdateFrenzy(float dt, FH_HordeManager mgr, FH_HordeSettings hs)
	{
		if (m_State != FH_HordeState.FRENZY)
			return;

		m_FrenzyTimer -= dt;
		if (AnyMemberAggressive())
			m_FrenzyTimer = hs.FrenzyDuration;

		if (m_FrenzyTarget)
		{
			if (!m_FrenzyTarget.IsAlive() || FH_Util.Dist2D(m_FrenzyTarget.GetPosition(), m_Pos) > hs.FrenzyMaxChaseDistance || mgr.IsInSafeZone(m_FrenzyTarget.GetPosition()))
				m_FrenzyTarget = null;
		}

		if (m_FrenzyTarget)
		{
			m_FrenzyPulseTimer -= dt;
			if (m_FrenzyPulseTimer <= 0)
			{
				m_FrenzyPulseTimer = hs.FrenzyPulseInterval;
				mgr.EmitNoise(m_FrenzyTarget, m_FrenzyTarget.GetPosition(), hs.FrenzyCallStrength);
			}
		}

		if (m_FrenzyTimer <= 0)
			EndFrenzy(mgr);
	}

	protected void EndFrenzy(FH_HordeManager mgr)
	{
		m_FrenzyTarget = null;
		m_State = m_StateBeforeFrenzy;
		if (m_IsDynamic)
		{
			if (m_RouteComplete)
				m_State = FH_HordeState.DORMANT;
			else
				m_State = FH_HordeState.ROAMING;
		}
		else if (m_State == FH_HordeState.DORMANT)
		{
			// It wandered off while hunting - walk back home.
			m_State = FH_HordeState.RETURNING;
		}
		mgr.Log(m_Name + " calms down (" + StateName() + ")");
	}

	protected void UpdateLure(float dt, FH_HordeManager mgr, FH_HordeSettings hs)
	{
		if (m_State != FH_HordeState.ROAMING && m_State != FH_HordeState.RETURNING)
			return;

		m_LureTimer -= dt;
		if (m_LureTimer > 0)
			return;
		m_LureTimer = hs.LureInterval;

		ZombieBase source = GetAnyMember();
		if (source)
			mgr.EmitNoise(source, m_Pos, hs.LureStrength);
	}

	// Regroups stragglers that got stuck, but only where no player can see it happen.
	protected void UpdateLeash(float dt, FH_HordeManager mgr, FH_HordeSettings hs)
	{
		if (m_State == FH_HordeState.FRENZY)
			return;

		m_LeashTimer -= dt;
		if (m_LeashTimer > 0)
			return;
		m_LeashTimer = 5;

		float destWatch;
		mgr.GetNearestPlayer(m_Pos, destWatch);
		if (destWatch < hs.LeashHiddenDistance)
			return;

		foreach (ZombieBase z : m_Members)
		{
			if (!z || !z.IsAlive() || z.FH_IsAggressive())
				continue;

			vector zp = z.GetPosition();
			if (FH_Util.Dist2D(zp, m_Pos) <= hs.LeashDistance)
				continue;

			float memberWatch;
			mgr.GetNearestPlayer(zp, memberWatch);
			if (memberWatch < hs.LeashHiddenDistance)
				continue;

			z.SetPosition(FH_Util.RandomPointAround(m_Pos, hs.SpreadRadius));
		}
	}

	// Walking into a sleeping nest wakes the whole horde. Crouching lets you get closer.
	protected void UpdateNestWake(FH_HordeManager mgr, FH_HordeSettings hs)
	{
		if (m_State != FH_HordeState.DORMANT || hs.NestWakeRadius <= 0)
			return;

		float dist;
		PlayerBase p = mgr.GetNearestPlayer(m_Pos, dist);
		if (!p)
			return;

		float radius = hs.NestWakeRadius;
		if (FH_Util.IsLowStance(p))
			radius = radius * 0.5;

		if (dist <= radius)
			TriggerFrenzy(p, mgr);
	}

	// -----------------------------------------------------------------
	protected bool AnyMemberAggressive()
	{
		foreach (ZombieBase z : m_Members)
		{
			if (z && z.IsAlive() && z.FH_IsAggressive())
				return true;
		}
		return false;
	}

	protected ZombieBase GetAnyMember()
	{
		foreach (ZombieBase z : m_Members)
		{
			if (z && z.IsAlive())
				return z;
		}
		return null;
	}

	protected vector GetCentroid()
	{
		vector sum = vector.Zero;
		int n = 0;
		foreach (ZombieBase z : m_Members)
		{
			if (!z)
				continue;
			sum = sum + z.GetPosition();
			n++;
		}
		if (n == 0)
			return m_Pos;
		return sum * (1.0 / n);
	}
}
