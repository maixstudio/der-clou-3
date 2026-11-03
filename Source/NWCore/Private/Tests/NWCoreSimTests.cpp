// In-engine counterparts of Tools/SimTests. Run via Session Frontend -> Automation,
// filter "Nachtwerk", or: UnrealEditor-Cmd Nachtwerk.uproject -ExecCmds="Automation RunTests Nachtwerk" -unattended

#include "Misc/AutomationTest.h"
#include "Sim/SimScenarios.h"
#include "Sim/Simulation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWSimDeterminismTest, "Nachtwerk.Core.Sim.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWSimDeterminismTest::RunTest(const FString& Parameters)
{
	const nw::sim::scenarios::Scenario Setup = nw::sim::scenarios::MakeJewelerScenario();
	nw::sim::Simulation A(Setup.Object, Setup.Crew, Setup.Plan, 42);
	nw::sim::Simulation B(Setup.Object, Setup.Crew, Setup.Plan, 42);

	while (!A.IsFinished() && !B.IsFinished())
	{
		A.Step();
		B.Step();
		if (A.ComputeStateHash() != B.ComputeStateHash())
		{
			AddError(FString::Printf(TEXT("State hash diverged at tick %d"), A.GetTick()));
			return false;
		}
	}
	TestTrue(TEXT("Both runs finished"), A.IsFinished() && B.IsFinished());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWSimJewelerReferencePlanTest, "Nachtwerk.Core.Sim.JewelerReferencePlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWSimJewelerReferencePlanTest::RunTest(const FString& Parameters)
{
	const nw::sim::scenarios::Scenario Setup = nw::sim::scenarios::MakeJewelerScenario();
	for (uint64 Seed = 1; Seed <= 50; ++Seed)
	{
		nw::sim::Simulation Sim(Setup.Object, Setup.Crew, Setup.Plan, Seed);
		const nw::sim::SimResult Result = Sim.RunToEnd();
		if (Result.Result != nw::sim::Outcome::CleanGetaway)
		{
			AddError(FString::Printf(TEXT("Seed %llu ended with %s"), Seed, UTF8_TO_TCHAR(nw::sim::Simulation::ToString(Result.Result))));
			return false;
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
