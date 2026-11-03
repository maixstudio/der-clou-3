using UnrealBuildTool;

public class NachtwerkTarget : TargetRules
{
	public NachtwerkTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "NWCore", "NWPlanning", "NWGame", "NWUI" });
	}
}
