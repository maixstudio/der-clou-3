using UnrealBuildTool;

// Primary game module: game mode, camera, presentation of the simulation.
public class NWGame : ModuleRules
{
	public NWGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NWCore", "NWPlanning" });
	}
}
