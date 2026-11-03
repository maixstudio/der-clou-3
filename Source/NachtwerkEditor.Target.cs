using UnrealBuildTool;

public class NachtwerkEditorTarget : TargetRules
{
	public NachtwerkEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "NWCore", "NWPlanning", "NWGame", "NWUI" });
	}
}
