// Checks that the authored jeweler data (/Game/NW/Data + the director in Graybox_Jeweler)
// matches the built-in reference scenario from NWCore, and that the ghost run of the
// director in the map succeeds. Filter "Nachtwerk.Game".

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "NWCrewMemberAsset.h"
#include "NWHeistObjectAsset.h"
#include "NWSimConversion.h"
#include "NWSimulationDirector.h"
#include "Sim/SimScenarios.h"
#include "Sim/Simulation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NWJewelerAssetTests
{
	const TCHAR* const MapPath = TEXT("/Game/NW/Maps/Graybox_Jeweler.Graybox_Jeweler");

	ANWSimulationDirector* FindDirector(FAutomationTestBase& Test)
	{
		UWorld* World = LoadObject<UWorld>(nullptr, MapPath);
		if (!World || !World->PersistentLevel)
		{
			Test.AddError(FString::Printf(TEXT("Could not load %s"), MapPath));
			return nullptr;
		}
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (ANWSimulationDirector* Director = Cast<ANWSimulationDirector>(Actor))
			{
				return Director;
			}
		}
		Test.AddError(TEXT("Graybox_Jeweler contains no NWSimulationDirector"));
		return nullptr;
	}

	FString Str(const std::string& Value)
	{
		return UTF8_TO_TCHAR(Value.c_str());
	}

	/** Compares two values and reports the field path on mismatch. */
	template <typename T>
	void Expect(FAutomationTestBase& Test, const FString& What, const T& Actual, const T& Expected)
	{
		if (!(Actual == Expected))
		{
			Test.AddError(FString::Printf(TEXT("%s differs from the built-in scenario"), *What));
		}
	}

	void Expect(FAutomationTestBase& Test, const FString& What, const std::string& Actual, const std::string& Expected)
	{
		if (Actual != Expected)
		{
			Test.AddError(FString::Printf(TEXT("%s: '%s' != '%s'"), *What, *Str(Actual), *Str(Expected)));
		}
	}

	template <typename T>
	bool ExpectSize(FAutomationTestBase& Test, const FString& What, const std::vector<T>& Actual, const std::vector<T>& Expected)
	{
		if (Actual.size() != Expected.size())
		{
			Test.AddError(FString::Printf(TEXT("%s: %d entries, expected %d"), *What, static_cast<int32>(Actual.size()), static_cast<int32>(Expected.size())));
			return false;
		}
		return true;
	}

	void CompareObject(FAutomationTestBase& Test, const nw::sim::ObjectDefinition& A, const nw::sim::ObjectDefinition& B)
	{
		Expect(Test, TEXT("Object.Name"), A.Name, B.Name);
		Expect(Test, TEXT("Object.EntryNode"), A.EntryNode, B.EntryNode);
		Expect(Test, TEXT("Object.TimeLimitTicks"), A.TimeLimitTicks, B.TimeLimitTicks);

		if (ExpectSize(Test, TEXT("Nodes"), A.Nodes, B.Nodes))
		{
			for (size_t I = 0; I < A.Nodes.size(); ++I)
			{
				const FString P = FString::Printf(TEXT("Nodes[%d]"), static_cast<int32>(I));
				Expect(Test, P + TEXT(".Label"), A.Nodes[I].Label, B.Nodes[I].Label);
				Expect(Test, P + TEXT(".Position.X"), A.Nodes[I].Position.X, B.Nodes[I].Position.X);
				Expect(Test, P + TEXT(".Position.Y"), A.Nodes[I].Position.Y, B.Nodes[I].Position.Y);
				Expect(Test, P + TEXT(".RoomId"), A.Nodes[I].RoomId, B.Nodes[I].RoomId);
			}
		}
		if (ExpectSize(Test, TEXT("Edges"), A.Edges, B.Edges))
		{
			for (size_t I = 0; I < A.Edges.size(); ++I)
			{
				const FString P = FString::Printf(TEXT("Edges[%d]"), static_cast<int32>(I));
				Expect(Test, P + TEXT(".A"), A.Edges[I].A, B.Edges[I].A);
				Expect(Test, P + TEXT(".B"), A.Edges[I].B, B.Edges[I].B);
				Expect(Test, P + TEXT(".Door"), A.Edges[I].Door, B.Edges[I].Door);
			}
		}
		if (ExpectSize(Test, TEXT("Doors"), A.Doors, B.Doors))
		{
			for (size_t I = 0; I < A.Doors.size(); ++I)
			{
				const FString P = FString::Printf(TEXT("Doors[%d]"), static_cast<int32>(I));
				Expect(Test, P + TEXT(".Label"), A.Doors[I].Label, B.Doors[I].Label);
				Expect(Test, P + TEXT(".Lock"), A.Doors[I].Lock, B.Doors[I].Lock);
				Expect(Test, P + TEXT(".bStartsLocked"), A.Doors[I].bStartsLocked, B.Doors[I].bStartsLocked);
				Expect(Test, P + TEXT(".Alarm"), A.Doors[I].Alarm, B.Doors[I].Alarm);
				Expect(Test, P + TEXT(".OpenNoiseRadiusCm"), A.Doors[I].OpenNoiseRadiusCm, B.Doors[I].OpenNoiseRadiusCm);
			}
		}
		if (ExpectSize(Test, TEXT("Alarms"), A.Alarms, B.Alarms))
		{
			for (size_t I = 0; I < A.Alarms.size(); ++I)
			{
				const FString P = FString::Printf(TEXT("Alarms[%d]"), static_cast<int32>(I));
				Expect(Test, P + TEXT(".Label"), A.Alarms[I].Label, B.Alarms[I].Label);
				Expect(Test, P + TEXT(".Type"), A.Alarms[I].Type, B.Alarms[I].Type);
				Expect(Test, P + TEXT(".PanelNode"), A.Alarms[I].PanelNode, B.Alarms[I].PanelNode);
				Expect(Test, P + TEXT(".GuardedNode"), A.Alarms[I].GuardedNode, B.Alarms[I].GuardedNode);
				Expect(Test, P + TEXT(".BaseDisableSeconds"), A.Alarms[I].BaseDisableSeconds, B.Alarms[I].BaseDisableSeconds);
				Expect(Test, P + TEXT(".PoliceResponseTicks"), A.Alarms[I].PoliceResponseTicks, B.Alarms[I].PoliceResponseTicks);
			}
		}
		if (ExpectSize(Test, TEXT("Safes"), A.Safes, B.Safes))
		{
			for (size_t I = 0; I < A.Safes.size(); ++I)
			{
				const FString P = FString::Printf(TEXT("Safes[%d]"), static_cast<int32>(I));
				Expect(Test, P + TEXT(".Label"), A.Safes[I].Label, B.Safes[I].Label);
				Expect(Test, P + TEXT(".Node"), A.Safes[I].Node, B.Safes[I].Node);
				Expect(Test, P + TEXT(".BaseCrackSeconds"), A.Safes[I].BaseCrackSeconds, B.Safes[I].BaseCrackSeconds);
				Expect(Test, P + TEXT(".LootValue"), A.Safes[I].LootValue, B.Safes[I].LootValue);
			}
		}
		if (ExpectSize(Test, TEXT("Guards"), A.Guards, B.Guards))
		{
			for (size_t I = 0; I < A.Guards.size(); ++I)
			{
				const FString P = FString::Printf(TEXT("Guards[%d]"), static_cast<int32>(I));
				const nw::sim::GuardDef& GA = A.Guards[I];
				const nw::sim::GuardDef& GB = B.Guards[I];
				Expect(Test, P + TEXT(".Name"), GA.Name, GB.Name);
				Expect(Test, P + TEXT(".StartNode"), GA.StartNode, GB.StartNode);
				Expect(Test, P + TEXT(".SpeedCmPerTick"), GA.SpeedCmPerTick, GB.SpeedCmPerTick);
				Expect(Test, P + TEXT(".SightRangeCm"), GA.SightRangeCm, GB.SightRangeCm);
				Expect(Test, P + TEXT(".ResponseTicks"), GA.ResponseTicks, GB.ResponseTicks);
				if (ExpectSize(Test, P + TEXT(".Route"), GA.Route, GB.Route))
				{
					for (size_t R = 0; R < GA.Route.size(); ++R)
					{
						const FString RP = FString::Printf(TEXT("%s.Route[%d]"), *P, static_cast<int32>(R));
						Expect(Test, RP + TEXT(".Node"), GA.Route[R].Node, GB.Route[R].Node);
						Expect(Test, RP + TEXT(".WaitMinTicks"), GA.Route[R].WaitMinTicks, GB.Route[R].WaitMinTicks);
						Expect(Test, RP + TEXT(".WaitMaxTicks"), GA.Route[R].WaitMaxTicks, GB.Route[R].WaitMaxTicks);
					}
				}
			}
		}
	}

	void CompareCrew(FAutomationTestBase& Test, const std::vector<nw::sim::CrewDef>& A, const std::vector<nw::sim::CrewDef>& B)
	{
		if (!ExpectSize(Test, TEXT("Crew"), A, B))
		{
			return;
		}
		for (size_t I = 0; I < A.size(); ++I)
		{
			const FString P = FString::Printf(TEXT("Crew[%d]"), static_cast<int32>(I));
			Expect(Test, P + TEXT(".Name"), A[I].Name, B[I].Name);
			Expect(Test, P + TEXT(".Lockpicking"), A[I].Lockpicking, B[I].Lockpicking);
			Expect(Test, P + TEXT(".Electrics"), A[I].Electrics, B[I].Electrics);
			Expect(Test, P + TEXT(".Safecracking"), A[I].Safecracking, B[I].Safecracking);
			Expect(Test, P + TEXT(".SpeedCmPerTick"), A[I].SpeedCmPerTick, B[I].SpeedCmPerTick);
			Expect(Test, P + TEXT(".Nerves"), A[I].Nerves, B[I].Nerves);
		}
	}

	void ComparePlan(FAutomationTestBase& Test, const nw::sim::HeistPlan& A, const nw::sim::HeistPlan& B)
	{
		if (!ExpectSize(Test, TEXT("Plan"), A.Crew, B.Crew))
		{
			return;
		}
		for (size_t I = 0; I < A.Crew.size(); ++I)
		{
			const FString P = FString::Printf(TEXT("Plan[%d]"), static_cast<int32>(I));
			if (!ExpectSize(Test, P, A.Crew[I].Actions, B.Crew[I].Actions))
			{
				continue;
			}
			for (size_t J = 0; J < A.Crew[I].Actions.size(); ++J)
			{
				const FString AP = FString::Printf(TEXT("%s.Actions[%d]"), *P, static_cast<int32>(J));
				const nw::sim::PlanAction& X = A.Crew[I].Actions[J];
				const nw::sim::PlanAction& Y = B.Crew[I].Actions[J];
				Expect(Test, AP + TEXT(".Type"), X.Type, Y.Type);
				Expect(Test, AP + TEXT(".Target"), X.Target, Y.Target);
				Expect(Test, AP + TEXT(".DurationTicks"), X.DurationTicks, Y.DurationTicks);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWJewelerAssetsMatchBuiltInTest, "Nachtwerk.Game.JewelerAssets.MatchBuiltIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWJewelerAssetsMatchBuiltInTest::RunTest(const FString& Parameters)
{
	using namespace NWJewelerAssetTests;

	const ANWSimulationDirector* Director = FindDirector(*this);
	if (!Director)
	{
		return false;
	}
	TestFalse(TEXT("Director uses the data assets"), Director->UsesBuiltInJewelerScenario());
	if (!Director->GetHeistObject())
	{
		AddError(TEXT("Director has no HeistObject"));
		return false;
	}

	nw::sim::ObjectDefinition Object = NWSimConversion::ToSim(*Director->GetHeistObject());
	std::vector<nw::sim::CrewDef> Crew;
	for (const TObjectPtr<UNWCrewMemberAsset>& Member : Director->GetCrew())
	{
		if (!Member)
		{
			AddError(TEXT("Director has an empty Crew entry"));
			return false;
		}
		Crew.push_back(NWSimConversion::ToSim(*Member));
	}
	nw::sim::HeistPlan Plan = NWSimConversion::ToSim(Director->GetPlan());

	const nw::sim::scenarios::Scenario BuiltIn = nw::sim::scenarios::MakeJewelerScenario();
	CompareObject(*this, Object, BuiltIn.Object);
	CompareCrew(*this, Crew, BuiltIn.Crew);
	ComparePlan(*this, Plan, BuiltIn.Plan);
	if (HasAnyErrors())
	{
		return false;
	}

	// Same data must give bit-identical runs.
	for (uint64 Seed = 1; Seed <= 50; ++Seed)
	{
		nw::sim::Simulation FromAssets(Object, Crew, Plan, Seed);
		nw::sim::Simulation Reference(BuiltIn.Object, BuiltIn.Crew, BuiltIn.Plan, Seed);
		const nw::sim::SimResult A = FromAssets.RunToEnd();
		const nw::sim::SimResult B = Reference.RunToEnd();
		if (A.Result != B.Result || A.LootValue != B.LootValue || A.EndTick != B.EndTick
			|| FromAssets.ComputeStateHash() != Reference.ComputeStateHash())
		{
			AddError(FString::Printf(TEXT("Seed %llu: asset run (%s, %d) differs from built-in run (%s, %d)"), Seed,
				UTF8_TO_TCHAR(nw::sim::Simulation::ToString(A.Result)), A.LootValue,
				UTF8_TO_TCHAR(nw::sim::Simulation::ToString(B.Result)), B.LootValue));
			return false;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWJewelerGhostPreviewTest, "Nachtwerk.Game.JewelerAssets.GhostPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWJewelerGhostPreviewTest::RunTest(const FString& Parameters)
{
	using namespace NWJewelerAssetTests;

	const ANWSimulationDirector* Director = FindDirector(*this);
	if (!Director)
	{
		return false;
	}
	TestEqual(TEXT("Seed"), Director->GetSeed(), 42);

	int32 Loot = 0;
	float Duration = 0.f;
	const ENWHeistOutcome Outcome = Director->RunGhostPreview(Loot, Duration);
	if (Outcome != ENWHeistOutcome::CleanGetaway)
	{
		AddError(FString::Printf(TEXT("Ghost run ended with %s, expected CleanGetaway"), *UEnum::GetValueAsString(Outcome)));
	}
	TestEqual(TEXT("Ghost run loot"), Loot, 85000);
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
