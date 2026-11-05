using UnrealBuildTool;

// Primary game module of "der clou 3". Starts empty; features are added step by step.
public class DerClou3 : ModuleRules
{
	public DerClou3(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
	}
}
