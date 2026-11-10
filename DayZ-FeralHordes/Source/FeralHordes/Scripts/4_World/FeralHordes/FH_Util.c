enum FH_Role
{
	NORMAL,
	SCREAMER,
	BRUTE
}

class FH_Util
{
	static float Dist2D(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}

	static bool IsWater(float x, float z)
	{
		return GetGame().SurfaceIsSea(x, z) || GetGame().SurfaceIsPond(x, z);
	}

	static vector RandomPointAround(vector center, float radius)
	{
		float ang = Math.RandomFloat(0, Math.PI2);
		float r = Math.RandomFloat(0, radius);
		float x = center[0] + Math.Cos(ang) * r;
		float z = center[2] + Math.Sin(ang) * r;
		if (IsWater(x, z))
		{
			x = center[0];
			z = center[2];
		}
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	static bool IsLowStance(PlayerBase p)
	{
		if (!p)
			return false;
		return p.IsPlayerInStance(DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_PRONE);
	}

	static bool IsInVehicle(PlayerBase p)
	{
		return p && p.GetCommand_Vehicle() != null;
	}
}
