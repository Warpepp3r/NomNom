modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		FH_HordeManager.Start();
	}

	override void OnMissionFinish()
	{
		FH_HordeManager.Stop();
		super.OnMissionFinish();
	}
}
