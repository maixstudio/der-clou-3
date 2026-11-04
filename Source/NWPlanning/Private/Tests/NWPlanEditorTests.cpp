// Tests for UNWPlanEditor (planner editing model). Filter "Nachtwerk.Planning".

#include "Misc/AutomationTest.h"
#include "NWPlanEditor.h"
#include "NWSimConversion.h"
#include "Sim/SimScenarios.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NWPlanEditorTests
{
	using namespace nw::sim::scenarios;

	UNWPlanEditor* MakeJewelerEditor()
	{
		Scenario Setup = MakeJewelerScenario();
		UNWPlanEditor* Editor = NewObject<UNWPlanEditor>();
		Editor->Initialize(Setup.Object, Setup.Crew, NWSimConversion::FromSim(Setup.Plan), 42);
		return Editor;
	}

	FIntPoint NodePosition(const UNWPlanEditor& Editor, int32 Node)
	{
		const nw::sim::IntVec2& P = Editor.GetObjectDefinition().Nodes[static_cast<size_t>(Node)].Position;
		return FIntPoint(P.X, P.Y);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWPlanEditorGhostRunTest, "Nachtwerk.Planning.PlanEditor.GhostRunAndUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWPlanEditorGhostRunTest::RunTest(const FString& Parameters)
{
	using namespace NWPlanEditorTests;
	UNWPlanEditor* Editor = MakeJewelerEditor();

	TestEqual(TEXT("Crew count"), Editor->GetCrewCount(), 3);
	TestTrue(TEXT("Reference plan escapes cleanly"), Editor->GetTimeline().Result.Result == nw::sim::Outcome::CleanGetaway);
	TestEqual(TEXT("Reference plan loot"), Editor->GetTimeline().Result.LootValue, 85000);
	TestFalse(TEXT("Nothing to undo yet"), Editor->CanUndo());

	int32 Changes = 0;
	Editor->OnChanged.AddLambda([&Changes]() { ++Changes; });

	// Without picking the office lock, Otto runs into the locked door.
	Editor->RemoveAction(jeweler::Safecracker, 3);
	TestTrue(TEXT("Change was broadcast"), Changes > 0);
	TestEqual(TEXT("Action removed"), Editor->GetPlan()[jeweler::Safecracker].Actions.Num(), 7);
	TestTrue(TEXT("Broken plan times out"), Editor->GetTimeline().Result.Result == nw::sim::Outcome::TimedOut);
	TestTrue(TEXT("Failed action is flagged"), Editor->GetTimeline().Crew[jeweler::Safecracker].Actions[3].bFailed);

	TestTrue(TEXT("Undo"), Editor->Undo());
	TestTrue(TEXT("Undo restores the clean run"), Editor->GetTimeline().Result.Result == nw::sim::Outcome::CleanGetaway);
	TestTrue(TEXT("Redo possible"), Editor->CanRedo());
	TestTrue(TEXT("Redo"), Editor->Redo());
	TestTrue(TEXT("Redo breaks it again"), Editor->GetTimeline().Result.Result == nw::sim::Outcome::TimedOut);

	// A new edit clears the redo history.
	FNWPlanAction Wait;
	Wait.Type = ENWPlanActionType::Wait;
	Wait.DurationSeconds = 10.f;
	Editor->AddAction(jeweler::Driver, Wait);
	TestFalse(TEXT("Redo cleared"), Editor->CanRedo());
	TestEqual(TEXT("Wait appended"), Editor->GetPlan()[jeweler::Driver].Actions.Num(), 2);
	TestEqual(TEXT("Wait label"), Editor->DescribeAction(Wait).ToString(), FString(TEXT("Warten: 10 s")));

	Editor->ClearCrew(jeweler::Driver);
	TestEqual(TEXT("Track cleared"), Editor->GetPlan()[jeweler::Driver].Actions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWPlanEditorPickTest, "Nachtwerk.Planning.PlanEditor.PickTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWPlanEditorPickTest::RunTest(const FString& Parameters)
{
	using namespace NWPlanEditorTests;
	UNWPlanEditor* Editor = MakeJewelerEditor();
	Editor->SelectCrew(jeweler::Driver);
	Editor->ClearCrew(jeweler::Driver);

	// Clicking next to the cellar node places "MoveTo(Cellar)".
	Editor->BeginPick(ENWPlanActionType::MoveTo);
	TestTrue(TEXT("Picking"), Editor->IsPicking());
	TestEqual(TEXT("Every node is a MoveTo candidate"), Editor->GetPickCandidates().Num(), 8);
	TestTrue(TEXT("Pick near the cellar"), Editor->PickAt(NodePosition(*Editor, jeweler::Cellar) + FIntPoint(40, -30)));
	TestFalse(TEXT("Picking ends after a pick"), Editor->IsPicking());
	const TArray<FNWPlanAction>& Track = Editor->GetPlan()[jeweler::Driver].Actions;
	TestEqual(TEXT("One action"), Track.Num(), 1);
	TestTrue(TEXT("MoveTo(Cellar)"), Track[0].Type == ENWPlanActionType::MoveTo && Track[0].Target == jeweler::Cellar);

	// Far away from everything: nothing happens, picking stays active.
	Editor->BeginPick(ENWPlanActionType::MoveTo);
	TestFalse(TEXT("Click into nowhere"), Editor->PickAt(FIntPoint(5000, 5000)));
	TestTrue(TEXT("Still picking"), Editor->IsPicking());
	Editor->CancelPick();
	TestFalse(TEXT("Cancelled"), Editor->IsPicking());

	// Only lockable doors are lock-picking targets.
	Editor->BeginPick(ENWPlanActionType::PickLock);
	TestEqual(TEXT("Two lockable doors"), Editor->GetPickCandidates().Num(), 2);
	const FIntPoint OfficeDoor = (NodePosition(*Editor, jeweler::Corridor) + NodePosition(*Editor, jeweler::Office)) / 2;
	TestTrue(TEXT("Pick office door"), Editor->PickAt(OfficeDoor));
	TestTrue(TEXT("PickLock(OfficeDoor)"), Editor->GetPlan()[jeweler::Driver].Actions.Last().Target == jeweler::OfficeDoor);

	// Both alarms share the cellar panel: the first click takes the one planned least.
	// The reference plan already disables the office contact, so the light barrier comes first.
	Editor->BeginPick(ENWPlanActionType::DisableAlarm);
	TestTrue(TEXT("Pick alarm panel"), Editor->PickAt(NodePosition(*Editor, jeweler::Cellar)));
	TestEqual(TEXT("Light barrier first"), Editor->GetPlan()[jeweler::Driver].Actions.Last().Target, static_cast<int32>(jeweler::ShopLightBarrier));
	Editor->BeginPick(ENWPlanActionType::DisableAlarm);
	TestTrue(TEXT("Pick alarm panel again"), Editor->PickAt(NodePosition(*Editor, jeweler::Cellar)));
	TestEqual(TEXT("Then the office contact"), Editor->GetPlan()[jeweler::Driver].Actions.Last().Target, static_cast<int32>(jeweler::OfficeContact));

	TestEqual(TEXT("Label"), Editor->DescribeAction(Editor->GetPlan()[jeweler::Driver].Actions[0]).ToString(), FString(TEXT("Gehe zu: Cellar")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWPlanEditorSignalLinksTest, "Nachtwerk.Planning.PlanEditor.SignalLinks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWPlanEditorSignalLinksTest::RunTest(const FString& Parameters)
{
	using namespace NWPlanEditorTests;
	UNWPlanEditor* Editor = MakeJewelerEditor();

	// Reference plan: Vic and Marta wait for Otto's "safe open", Otto waits for Marta's "alarm off".
	const TArray<FNWSignalLink>& Links = Editor->GetSignalLinks();
	TestEqual(TEXT("Three waits"), Links.Num(), 3);
	for (const FNWSignalLink& Link : Links)
	{
		TestTrue(TEXT("Released"), Link.bReleased);
		TestTrue(TEXT("Signal in plan"), Link.bSignalInPlan);
		TestTrue(TEXT("Sent before release"), Link.SentAt != nw::sim::NoTick && Link.SentAt <= Link.ReleasedAt);
	}

	const FNWSignalLink* Vic = Editor->FindSignalLink(jeweler::Driver, 0);
	const FNWSignalLink* Otto = Editor->FindSignalLink(jeweler::Safecracker, 1);
	if (!TestNotNull(TEXT("Vic's wait"), Vic) || !TestNotNull(TEXT("Otto's wait"), Otto))
	{
		return false;
	}
	TestEqual(TEXT("Vic waits for signal 1"), Vic->Signal, static_cast<int32>(jeweler::SafeOpen));
	TestEqual(TEXT("... sent by Otto"), Vic->SignalCrew, static_cast<int32>(jeweler::Safecracker));
	TestEqual(TEXT("... with his 7th action"), Vic->SignalAction, 6);
	TestEqual(TEXT("Otto waits for Marta"), Otto->SignalCrew, static_cast<int32>(jeweler::Electrician));
	TestEqual(TEXT("... and her Signal action"), Otto->SignalAction, 4);
	TestTrue(TEXT("Label"), Editor->DescribeLink(*Otto).ToString().StartsWith(TEXT("Wartet auf Signal 0 von Marta")));

	// Without Otto's "safe open" signal nobody releases Vic and Marta.
	Editor->RemoveAction(jeweler::Safecracker, 6);
	const FNWSignalLink* Dangling = Editor->FindSignalLink(jeweler::Driver, 0);
	if (!TestNotNull(TEXT("Vic's wait after removal"), Dangling))
	{
		return false;
	}
	TestFalse(TEXT("Not released"), Dangling->bReleased);
	TestFalse(TEXT("No matching signal"), Dangling->bSignalInPlan);
	TestTrue(TEXT("Dangling label"), Editor->DescribeLink(*Dangling).ToString().StartsWith(TEXT("Niemand sendet Signal 1")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
