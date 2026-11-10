// =====================================================================
// Feral Hordes - per-infected behaviour (server only)
// =====================================================================

modded class ZombieBase
{
	static const float FH_THINK_INTERVAL = 0.5;

	protected FH_Horde m_FH_Horde;
	protected int m_FH_Role;
	protected bool m_FH_RoleAssigned;
	protected float m_FH_ThinkTimer;
	protected int m_FH_LastMind = -1;
	protected int m_FH_Mind;
	protected bool m_FH_SpeedOverridden;
	protected float m_FH_CallCooldown;
	protected float m_FH_SenseCooldown;

	// -----------------------------------------------------------------
	void FH_SetHorde(FH_Horde horde)
	{
		m_FH_Horde = horde;
	}

	FH_Horde FH_GetHorde()
	{
		return m_FH_Horde;
	}

	void FH_SetRole(int role)
	{
		m_FH_Role = role;
		m_FH_RoleAssigned = true;
	}

	int FH_GetRole()
	{
		return m_FH_Role;
	}

	bool FH_CanCall()
	{
		return m_FH_CallCooldown <= 0;
	}

	void FH_SetCallCooldown(float seconds)
	{
		m_FH_CallCooldown = seconds;
	}

	bool FH_IsAggressive()
	{
		return m_FH_Mind == DayZInfectedConstants.MINDSTATE_CHASE || m_FH_Mind == DayZInfectedConstants.MINDSTATE_FIGHT;
	}

	// -----------------------------------------------------------------
	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		if (GetGame().IsServer())
		{
			m_FH_ThinkTimer += pDt;
			if (m_FH_ThinkTimer >= FH_THINK_INTERVAL)
			{
				FH_Think(m_FH_ThinkTimer);
				m_FH_ThinkTimer = 0;
			}
		}

		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);
	}

	protected void FH_Think(float dt)
	{
		if (!IsAlive())
			return;

		FH_HordeManager mgr = FH_HordeManager.GetInstance();
		if (!mgr)
			return;

		DayZInfectedInputController ic = GetInputController();
		if (!ic)
			return;

		FH_Settings s = mgr.GetSettings();
		FH_AmbientSettings a = s.Ambient;

		if (!m_FH_RoleAssigned)
			FH_SetRole(mgr.RollRole());

		m_FH_CallCooldown -= dt;
		m_FH_SenseCooldown -= dt;

		m_FH_Mind = ic.GetMindState();
		bool night = mgr.IsNight();
		bool aggressive = FH_IsAggressive();
		bool wasAggressive = m_FH_LastMind == DayZInfectedConstants.MINDSTATE_CHASE || m_FH_LastMind == DayZInfectedConstants.MINDSTATE_FIGHT;

		if (aggressive && !wasAggressive)
			mgr.OnZombieAggro(this, PlayerBase.Cast(ic.GetTargetEntity()));

		m_FH_LastMind = m_FH_Mind;

		if (a.Enabled && a.SenseEnabled && !aggressive && m_FH_SenseCooldown <= 0)
			FH_Sense(mgr, a, night);

		// --- movement speed ---
		float speed = -1;
		if (a.Enabled)
		{
			if (m_FH_Mind == DayZInfectedConstants.MINDSTATE_CHASE)
			{
				speed = a.ChaseSpeed;
			}
			else if (m_FH_Mind == DayZInfectedConstants.MINDSTATE_ALERTED)
			{
				if (night)
					speed = a.AlertedSpeedNight;
				else
					speed = a.AlertedSpeedDay;
			}
		}

		if (m_FH_Horde && m_FH_Horde.IsFrenzied())
		{
			if (m_FH_Mind == DayZInfectedConstants.MINDSTATE_CHASE || m_FH_Mind == DayZInfectedConstants.MINDSTATE_ALERTED)
				speed = s.Horde.FrenzySpeed;
		}

		FH_ApplySpeed(ic, speed);
	}

	// Never touch speed while in FIGHT - the attack animations need to drive it.
	protected void FH_ApplySpeed(DayZInfectedInputController ic, float speed)
	{
		if (speed >= 0 && m_FH_Mind != DayZInfectedConstants.MINDSTATE_FIGHT)
		{
			ic.OverrideMovementSpeed(true, speed);
			m_FH_SpeedOverridden = true;
		}
		else if (m_FH_SpeedOverridden)
		{
			ic.OverrideMovementSpeed(false, 0);
			m_FH_SpeedOverridden = false;
		}
	}

	protected void FH_Sense(FH_HordeManager mgr, FH_AmbientSettings a, bool night)
	{
		m_FH_SenseCooldown = a.SenseCooldown;

		float dist;
		PlayerBase p = mgr.GetNearestPlayer(GetPosition(), dist);
		if (!p)
			return;

		float radius = a.SenseRadiusDay;
		if (night)
			radius = a.SenseRadiusNight;

		if (FH_Util.IsInVehicle(p))
		{
			radius = radius * a.VehicleSenseMultiplier;
		}
		else
		{
			float playerSpeed = mgr.GetPlayerSpeed(p);
			if (playerSpeed >= a.SprintSpeedThreshold)
				radius = radius * a.SprintSenseMultiplier;
			else if (FH_Util.IsLowStance(p) && playerSpeed < 2.5)
				radius = radius * a.StealthSenseMultiplier;
		}

		if (dist <= radius)
			mgr.SenseNoise(p, a.SenseNoiseStrength);
	}

	// -----------------------------------------------------------------
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		if (!GetGame().IsServer())
			return;

		FH_HordeManager mgr = FH_HordeManager.GetInstance();
		if (!mgr)
			return;

		// Toughness: give back part of the damage. Headshots are never reduced.
		if (IsAlive() && damageResult && dmgZone != "Head")
		{
			float tough = mgr.GetToughness(this);
			if (tough > 1.0)
			{
				float dealt = damageResult.GetHighestDamage("Health");
				if (dealt > 0)
					AddHealth("", "Health", dealt * (1.0 - 1.0 / tough));
			}
		}

		if (source)
		{
			PlayerBase attacker = PlayerBase.Cast(source.GetHierarchyRootPlayer());
			if (attacker)
				mgr.OnZombieHitByPlayer(this, attacker);
		}
	}
}
