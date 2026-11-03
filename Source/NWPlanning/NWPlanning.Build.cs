using UnrealBuildTool;

// Data assets for heist objects, crew and plans, plus conversion into NWCore simulation data.
public class NWPlanning : ModuleRules
{
	public NWPlanning(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NWCore" });
	}
}
