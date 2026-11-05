using UnrealBuildTool;

public class DerClou3Target : TargetRules
{
	public DerClou3Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DerClou3");
	}
}
