using UnrealBuildTool;

// Deterministic heist simulation. The code under Public/Sim and Private/Sim is plain C++
// without Unreal headers so it can also be built and tested outside the engine
// (see Tools/SimTests).
public class NWCore : ModuleRules
{
	public NWCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
