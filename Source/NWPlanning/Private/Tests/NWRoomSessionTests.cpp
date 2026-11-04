// Tests for UNWRoomSession ("plan by doing" in the room prototype). Filter "Nachtwerk.Room".

#include "Misc/AutomationTest.h"
#include "NWRoomSession.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NWRoomSessionTests
{
	const TArray<FString> TestRoom = {
		TEXT(",,,,,,,,,"),
		TEXT(",###W###,"),
		TEXT(",#.....#,"),
		TEXT(",#1....D,"),
		TEXT(",#.....#,"),
		TEXT(",##L####,"),
		TEXT(",,,,,,,,,"),
	};

	void RunUntilIdle(UNWRoomSession& Session)
	{
		for (int32 Frame = 0; Frame < 2000 && !Session.GetSim().IsIdle(); ++Frame)
		{
			Session.Tick(0.1f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNWRoomSessionPlanByDoingTest, "Nachtwerk.Room.Session.PlanByDoing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNWRoomSessionPlanByDoingTest::RunTest(const FString& Parameters)
{
	using namespace NWRoomSessionTests;
	UNWRoomSession* Session = NewObject<UNWRoomSession>();
	Session->Initialize(TestRoom, { FText::FromString(TEXT("Vic")) });
	if (!TestTrue(TEXT("Layout valid"), Session->IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("Agent name"), Session->GetAgentName(0).ToString(), FString(TEXT("Vic")));

	const int32 Locked = Session->GetLayout().OpeningAt({ 3, 5 });
	TArray<nw::room::CommandType> Options = Session->GetOptions(Locked);
	TestTrue(TEXT("Locked door offers lock picking"), Options.Num() == 1 && Options[0] == nw::room::CommandType::PickLock);
	TestEqual(TEXT("Option label"), Session->DescribeOption(Options[0], Locked).ToString(), FString(TEXT("Schloss knacken")));

	// Plan by doing: the command runs right away.
	Session->IssueOpeningCommand(nw::room::CommandType::PickLock, Locked);
	Session->IssueOpeningCommand(nw::room::CommandType::GoThrough, Locked);
	RunUntilIdle(*Session);
	TestEqual(TEXT("Two commands in the plan"), Session->GetCommandCount(), 2);
	TestFalse(TEXT("Unlocked"), Session->GetSim().GetOpenings()[static_cast<size_t>(Locked)].bLocked);
	TestTrue(TEXT("Went through"), Session->GetSim().GetAgents()[0].At == nw::room::Cell{ 3, 6 });
	const float PlanSeconds = Session->GetSeconds();
	TestTrue(TEXT("Plan time advanced"), PlanSeconds > 8.f);

	// Undo jumps to the state without the last command.
	TestTrue(TEXT("Undo"), Session->Undo());
	TestTrue(TEXT("Back inside"), Session->GetSim().GetAgents()[0].At.Y < 5);
	TestFalse(TEXT("Still unlocked"), Session->GetSim().GetOpenings()[static_cast<size_t>(Locked)].bLocked);

	// Replay starts from the beginning and reaches the same state.
	Session->IssueOpeningCommand(nw::room::CommandType::GoThrough, Locked);
	RunUntilIdle(*Session);
	const uint64 Hash = Session->GetSim().ComputeStateHash();
	Session->Replay();
	TestTrue(TEXT("Replaying"), Session->IsReplaying());
	TestTrue(TEXT("Replay starts at 0"), Session->GetSeconds() == 0.f);
	RunUntilIdle(*Session);
	TestEqual(TEXT("Replay ends in the same state"), Session->GetSim().ComputeStateHash(), Hash);

	Session->ClearAll();
	TestEqual(TEXT("Cleared"), Session->GetCommandCount(), 0);
	TestTrue(TEXT("Locked again"), Session->GetSim().GetOpenings()[static_cast<size_t>(Locked)].bLocked);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
