class CfgPatches
{
	class FeralHordes
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
	};
};

class CfgMods
{
	class FeralHordes
	{
		dir = "FeralHordes";
		name = "Feral Hordes";
		credits = "";
		author = "";
		authorID = "0";
		version = "1.0.0";
		extra = 0;
		type = "mod";
		hideName = 0;
		hidePicture = 1;
		picture = "";
		action = "";

		dependencies[] = {"World", "Mission"};

		class defs
		{
			class worldScriptModule
			{
				value = "";
				files[] = {"FeralHordes/Scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"FeralHordes/Scripts/5_Mission"};
			};
		};
	};
};

// Noise template used for horde lures, swarm calls and screamer shrieks.
// The final strength is this value multiplied by the *Strength settings in config.json.
class CfgFeralHordes
{
	class NoiseHordeCall
	{
		strength = 10;
		type = "sound";
	};
};
