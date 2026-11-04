using UnrealBuildTool;

public class DerClou3EditorTarget : TargetRules
{
	public DerClou3EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "NWCore", "NWPlanning", "NWGame", "NWUI" });
	}
}
