using UnrealBuildTool;

// CommonUI screens: hub menus, planner timeline, debrief.
public class NWUI : ModuleRules
{
	public NWUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UMG", "CommonUI", "NWCore", "NWPlanning" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "InputCore" });
	}
}
