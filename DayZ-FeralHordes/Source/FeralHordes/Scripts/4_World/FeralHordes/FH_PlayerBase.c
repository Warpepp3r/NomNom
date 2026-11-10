// =====================================================================
// Feral Hordes - infected hit harder (server only)
// =====================================================================

modded class PlayerBase
{
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		if (!GetGame().IsServer() || !damageResult || !IsAlive())
			return;

		ZombieBase z = ZombieBase.Cast(source);
		if (!z)
			return;

		FH_HordeManager mgr = FH_HordeManager.GetInstance();
		if (!mgr)
			return;

		float extra = mgr.GetDamageToPlayerMultiplier(z) - 1.0;
		if (extra == 0)
			return;

		FH_ApplyExtra(damageResult, "Health", extra);
		FH_ApplyExtra(damageResult, "Blood", extra);
		FH_ApplyExtra(damageResult, "Shock", extra);
	}

	protected void FH_ApplyExtra(TotalDamageResult damageResult, string healthType, float extra)
	{
		float dealt = damageResult.GetHighestDamage(healthType);
		if (dealt > 0)
			AddHealth("", healthType, -dealt * extra);
	}
}
