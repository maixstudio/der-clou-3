// Standalone tests for the NWCore simulation (no Unreal Engine required).
// Build: cmake -S Tools/SimTests -B Tools/SimTests/build && cmake --build Tools/SimTests/build
// Run:   ctest --test-dir Tools/SimTests/build --output-on-failure

#include "Sim/RoomSim.h"
#include "Sim/SimScenarios.h"
#include "Sim/SimTimeline.h"
#include "Sim/Simulation.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

using namespace nw::sim;
using namespace nw::sim::scenarios;

namespace
{
	int GFailures = 0;

#define NW_EXPECT(Condition)                                                              \
	do                                                                                    \
	{                                                                                     \
		if (!(Condition))                                                                 \
		{                                                                                 \
			std::printf("    FAILED: %s (%s:%d)\n", #Condition, __FILE__, __LINE__);    \
			++GFailures;                                                                  \
		}                                                                                 \
	} while (0)

	Simulation MakeSim(const Scenario& Setup, uint64_t Seed)
	{
		return Simulation(Setup.Object, Setup.Crew, Setup.Plan, Seed);
	}

	bool HasEvent(const Simulation& Sim, EventType Type)
	{
		const auto& Events = Sim.GetEvents();
		return std::any_of(Events.begin(), Events.end(), [Type](const SimEvent& Event) { return Event.Type == Type; });
	}

	void RemoveAction(Scenario& Setup, int32_t CrewIndex, ActionType Type)
	{
		auto& Actions = Setup.Plan.Crew[static_cast<size_t>(CrewIndex)].Actions;
		Actions.erase(std::remove_if(Actions.begin(), Actions.end(),
			[Type](const PlanAction& Action) { return Action.Type == Type; }), Actions.end());
	}

	void TestReferencePlanSucceedsForAllSeeds()
	{
		const Scenario Setup = MakeJewelerScenario();
		for (uint64_t Seed = 1; Seed <= 200; ++Seed)
		{
			Simulation Sim = MakeSim(Setup, Seed);
			NW_EXPECT(Sim.IsValid());
			const SimResult Result = Sim.RunToEnd();
			NW_EXPECT(Result.Result == Outcome::CleanGetaway);
			NW_EXPECT(Result.LootValue == 85000);
			NW_EXPECT(!Result.bAlarmRaised);
			if (Result.Result != Outcome::CleanGetaway)
			{
				std::printf("    seed %llu ended with %s\n", static_cast<unsigned long long>(Seed), Simulation::ToString(Result.Result));
				return;
			}
		}
	}

	void TestDeterminism()
	{
		const Scenario Setup = MakeJewelerScenario();
		Simulation A = MakeSim(Setup, 42);
		Simulation B = MakeSim(Setup, 42);
		while (!A.IsFinished() && !B.IsFinished())
		{
			A.Step();
			B.Step();
			const bool bSameState = A.ComputeStateHash() == B.ComputeStateHash();
			NW_EXPECT(bSameState);
			if (!bSameState)
			{
				return;
			}
		}
		NW_EXPECT(A.IsFinished() && B.IsFinished());
		NW_EXPECT(A.GetEvents().size() == B.GetEvents().size());
	}

	void TestSeedChangesGuardBehaviour()
	{
		const Scenario Setup = MakeJewelerScenario();
		std::set<uint64_t> Hashes;
		for (uint64_t Seed = 1; Seed <= 20; ++Seed)
		{
			Simulation Sim = MakeSim(Setup, Seed);
			Sim.RunToEnd();
			Hashes.insert(Sim.ComputeStateHash());
		}
		NW_EXPECT(Hashes.size() > 1);
	}

	void TestGhostRunFromCopyMatchesOriginal()
	{
		const Scenario Setup = MakeJewelerScenario();
		Simulation Original = MakeSim(Setup, 7);
		for (int32_t Index = 0; Index < SecondsToTicks(60); ++Index)
		{
			Original.Step();
		}
		Simulation Ghost = Original;
		const SimResult GhostResult = Ghost.RunToEnd();
		const SimResult RealResult = Original.RunToEnd();
		NW_EXPECT(GhostResult.Result == RealResult.Result);
		NW_EXPECT(GhostResult.EndTick == RealResult.EndTick);
		NW_EXPECT(Ghost.ComputeStateHash() == Original.ComputeStateHash());
	}

	void TestSkippingAlarmTriggersPolice()
	{
		Scenario Setup = MakeJewelerScenario();
		RemoveAction(Setup, jeweler::Electrician, ActionType::DisableAlarm);
		Simulation Sim = MakeSim(Setup, 1);
		const SimResult Result = Sim.RunToEnd();
		NW_EXPECT(HasEvent(Sim, EventType::AlarmTriggered));
		NW_EXPECT(Result.bAlarmRaised);
		// The safe takes ~144 s, police need 180 s: the crew just makes it.
		NW_EXPECT(Result.Result == Outcome::EscapedWithAlarm);
	}

	void TestLockedDoorBlocksPlan()
	{
		Scenario Setup = MakeJewelerScenario();
		RemoveAction(Setup, jeweler::Safecracker, ActionType::PickLock);
		Simulation Sim = MakeSim(Setup, 1);
		const SimResult Result = Sim.RunToEnd();
		NW_EXPECT(HasEvent(Sim, EventType::ActionFailed));
		NW_EXPECT(Sim.GetCrewStates()[jeweler::Safecracker].Status == CrewStatus::Blocked);
		NW_EXPECT(Result.Result == Outcome::TimedOut);
	}

	void TestWalkingIntoShopGetsSpotted()
	{
		Scenario Setup = MakeJewelerScenario();
		Setup.Plan.Crew[jeweler::Safecracker].Actions = {
			PlanAction::MoveTo(jeweler::BackDoorOutside),
			PlanAction::WaitForSignal(jeweler::AlarmOff),
			PlanAction::MoveTo(jeweler::ShopFront),
		};
		Simulation Sim = MakeSim(Setup, 1);
		const SimResult Result = Sim.RunToEnd();
		NW_EXPECT(HasEvent(Sim, EventType::GuardSpotted));
		NW_EXPECT(Result.Result == Outcome::Busted);
	}

	void TestNoiseMakesGuardInvestigate()
	{
		Scenario Setup = MakeJewelerScenario();
		// Lock picking right next to the shop is loud enough to be heard through the wall.
		Setup.Object.Nodes[jeweler::Corridor].Position = { 900, 300 };
		Simulation Sim = MakeSim(Setup, 1);
		const SimResult Result = Sim.RunToEnd();
		NW_EXPECT(HasEvent(Sim, EventType::GuardSuspicious));
		// The guard walks over, finds the safecracker at the office door and calls the police.
		NW_EXPECT(HasEvent(Sim, EventType::GuardSpotted));
		NW_EXPECT(Result.Result == Outcome::Busted);
	}

	void TestRerouteMidEdgeReachesNewTarget()
	{
		// Regression: a guard re-routed while walking an edge must not skip the first waypoint.
		Scenario Setup = MakeJewelerScenario();
		Setup.Object.Nodes[jeweler::Corridor].Position = { 900, 300 };
		// Whether the guard is close enough to hear depends on his randomised patrol timing,
		// so this plan is risky rather than doomed - exactly what the ghost run should reveal.
		int32_t SuspiciousRuns = 0;
		int32_t CleanRuns = 0;
		for (uint64_t Seed = 1; Seed <= 50; ++Seed)
		{
			Simulation Sim = MakeSim(Setup, Seed);
			const SimResult Result = Sim.RunToEnd();
			if (HasEvent(Sim, EventType::GuardSuspicious))
			{
				++SuspiciousRuns;
				NW_EXPECT(HasEvent(Sim, EventType::GuardSpotted));
			}
			else if (Result.Result == Outcome::CleanGetaway)
			{
				++CleanRuns;
			}
		}
		NW_EXPECT(SuspiciousRuns > 0);
		NW_EXPECT(CleanRuns > 0);
	}

	void TestInvalidDefinitionIsRejected()
	{
		Scenario Setup = MakeJewelerScenario();
		Setup.Object.Edges.push_back({ jeweler::Van, 99, InvalidId });
		Simulation Sim = MakeSim(Setup, 1);
		NW_EXPECT(!Sim.IsValid());
		NW_EXPECT(!Sim.GetError().empty());
		NW_EXPECT(Sim.IsFinished());
	}

	void TestTimelineCoversReferencePlan()
	{
		const Scenario Setup = MakeJewelerScenario();
		const PlanTimeline Timeline = BuildTimeline(MakeSim(Setup, 42));
		NW_EXPECT(Timeline.Result.Result == Outcome::CleanGetaway);
		NW_EXPECT(Timeline.Crew.size() == Setup.Plan.Crew.size());
		for (size_t Index = 0; Index < Timeline.Crew.size(); ++Index)
		{
			const CrewTimeline& Track = Timeline.Crew[Index];
			NW_EXPECT(Track.Actions.size() == Setup.Plan.Crew[Index].Actions.size());
			NW_EXPECT(Track.FinalStatus == CrewStatus::Escaped);
			Tick Previous = 0;
			for (const ActionSpan& Span : Track.Actions)
			{
				// Every action ran, back to back, without failing.
				NW_EXPECT(Span.Start == Previous);
				NW_EXPECT(Span.End >= Span.Start);
				NW_EXPECT(!Span.bFailed);
				NW_EXPECT(!Span.bUnfinished);
				Previous = Span.End;
			}
			NW_EXPECT(Track.EscapedAt >= Previous);
		}

		// The driver waits exactly until the safecracker signals "safe open".
		const ActionSpan& DriverWait = Timeline.Crew[jeweler::Driver].Actions[0];
		const ActionSpan& SignalSafeOpen = Timeline.Crew[jeweler::Safecracker].Actions[6];
		NW_EXPECT(DriverWait.End >= SignalSafeOpen.End);
		NW_EXPECT(DriverWait.End - SignalSafeOpen.End <= 1);
	}

	void TestTimelineMarksFailedAction()
	{
		Scenario Setup = MakeJewelerScenario();
		RemoveAction(Setup, jeweler::Safecracker, ActionType::PickLock);
		const PlanTimeline Timeline = BuildTimeline(MakeSim(Setup, 1));
		const CrewTimeline& Track = Timeline.Crew[jeweler::Safecracker];
		// MoveTo(Office) is now action 3 and runs into the locked office door.
		NW_EXPECT(Track.FinalStatus == CrewStatus::Blocked);
		NW_EXPECT(Track.Actions[3].bFailed);
		NW_EXPECT(Track.Actions[3].bUnfinished);
		NW_EXPECT(Track.Actions[3].End == Timeline.Result.EndTick);
		// Later actions were never reached.
		NW_EXPECT(Track.Actions[4].Start == NoTick);
		NW_EXPECT(Timeline.Result.Result == Outcome::TimedOut);
	}

	void TestTimelineOfInvalidSetup()
	{
		Scenario Setup = MakeJewelerScenario();
		Setup.Object.Edges.push_back({ jeweler::Van, 99, InvalidId });
		const PlanTimeline Timeline = BuildTimeline(MakeSim(Setup, 1));
		NW_EXPECT(Timeline.Result.Result == Outcome::InvalidSetup);
		NW_EXPECT(!Timeline.Error.empty());
		NW_EXPECT(Timeline.Crew.empty());
	}

	// --- Room prototype -----------------------------------------------------------------------

	const std::vector<std::string> TestRoom = {
		",,,,,,,,,",
		",###W###,",
		",#.....#,",
		",#1....D,",
		",#.....#,",
		",##L####,",
		",,,,,,,,,",
	};

	nw::room::Command RoomCmd(nw::room::CommandType Type, int32_t Opening = -1, nw::room::Cell Target = {})
	{
		nw::room::Command Cmd;
		Cmd.Type = Type;
		Cmd.Opening = Opening;
		Cmd.Target = Target;
		return Cmd;
	}

	void TestRoomLayoutParses()
	{
		using namespace nw::room;
		const Layout Room = ParseLayout(TestRoom);
		NW_EXPECT(Room.IsValid());
		NW_EXPECT(Room.Width == 9 && Room.Height == 7);
		NW_EXPECT(Room.Openings.size() == 3);
		NW_EXPECT(Room.Agents.size() == 1);
		NW_EXPECT((Room.Agents[0].Start == Cell{ 2, 3 }));
		const int32_t Window = Room.OpeningAt({ 4, 1 });
		const int32_t Door = Room.OpeningAt({ 7, 3 });
		const int32_t Locked = Room.OpeningAt({ 3, 5 });
		NW_EXPECT(Window >= 0 && Room.Openings[static_cast<size_t>(Window)].Kind == OpeningKind::Window);
		NW_EXPECT(Room.Openings[static_cast<size_t>(Window)].bAlongX);
		NW_EXPECT(Door >= 0 && !Room.Openings[static_cast<size_t>(Door)].bAlongX);
		NW_EXPECT(Locked >= 0 && Room.Openings[static_cast<size_t>(Locked)].bLocked);
		NW_EXPECT(!ParseLayout({ "#?#" }).IsValid());
	}

	void TestRoomWalkAndWalls()
	{
		using namespace nw::room;
		RoomSim Sim(ParseLayout(TestRoom));
		Sim.Enqueue(RoomCmd(CommandType::MoveTo, -1, { 6, 4 }));
		// Everything is closed: there is no way outside.
		Sim.Enqueue(RoomCmd(CommandType::MoveTo, -1, { 0, 0 }));
		Sim.RunUntilIdle();
		const auto& Records = Sim.GetRecords();
		NW_EXPECT(!Records[0].bFailed);
		NW_EXPECT(Records[0].End > Records[0].Start);
		NW_EXPECT((Sim.GetAgents()[0].At == Cell{ 6, 4 }));
		NW_EXPECT(Records[1].bFailed);
		NW_EXPECT(Records[1].Start >= Records[0].End);
	}

	void TestRoomDoorsAndLocks()
	{
		using namespace nw::room;
		const Layout Room = ParseLayout(TestRoom);
		const int32_t Door = Room.OpeningAt({ 7, 3 });
		const int32_t Locked = Room.OpeningAt({ 3, 5 });

		RoomSim Sim(Room);
		Sim.Enqueue(RoomCmd(CommandType::GoThrough, Door));
		Sim.RunUntilIdle();
		NW_EXPECT(!Sim.GetRecords()[0].bFailed);
		NW_EXPECT(Sim.GetOpenings()[static_cast<size_t>(Door)].bOpen);
		NW_EXPECT((Sim.GetAgents()[0].At == Cell{ 8, 3 }));

		RoomSim Locks(Room);
		Locks.Enqueue(RoomCmd(CommandType::GoThrough, Locked));
		Locks.Enqueue(RoomCmd(CommandType::PickLock, Locked));
		Locks.Enqueue(RoomCmd(CommandType::GoThrough, Locked));
		Locks.RunUntilIdle();
		const auto& Records = Locks.GetRecords();
		NW_EXPECT(Records[0].bFailed);
		NW_EXPECT(!Records[1].bFailed);
		NW_EXPECT(Records[1].End - Records[1].Start >= Locks.PickLockTicks(0));
		NW_EXPECT(!Records[2].bFailed);
		NW_EXPECT((Locks.GetAgents()[0].At == Cell{ 3, 6 }));
		NW_EXPECT(!Locks.GetOpenings()[static_cast<size_t>(Locked)].bLocked);
	}

	void TestRoomClimbWindow()
	{
		using namespace nw::room;
		const Layout Room = ParseLayout(TestRoom);
		const int32_t Window = Room.OpeningAt({ 4, 1 });
		RoomSim Sim(Room);
		Sim.Enqueue(RoomCmd(CommandType::ClimbThrough, Window));
		Sim.RunUntilIdle();
		NW_EXPECT(!Sim.GetRecords()[0].bFailed);
		NW_EXPECT((Sim.GetAgents()[0].At == Cell{ 4, 0 }));
		NW_EXPECT(Sim.GetOpenings()[static_cast<size_t>(Window)].bOpen);
		// Closing it again from outside works, and it is not walkable afterwards.
		Sim.Enqueue(RoomCmd(CommandType::Close, Window));
		Sim.RunUntilIdle();
		NW_EXPECT(!Sim.GetOpenings()[static_cast<size_t>(Window)].bOpen);
		NW_EXPECT(!Sim.IsWalkable({ 4, 1 }));
	}

	void TestRoomOptionsFollowQueue()
	{
		using namespace nw::room;
		const Layout Room = ParseLayout(TestRoom);
		const int32_t Locked = Room.OpeningAt({ 3, 5 });
		RoomSim Sim(Room);
		const std::vector<CommandType> Before = Sim.GetOptions(0, Locked);
		NW_EXPECT(Before.size() == 1 && Before[0] == CommandType::PickLock);
		// Once a lock-pick is queued, the menu already offers what comes after it.
		Sim.Enqueue(RoomCmd(CommandType::PickLock, Locked));
		const std::vector<CommandType> After = Sim.GetOptions(0, Locked);
		NW_EXPECT(After.size() == 2 && After[0] == CommandType::GoThrough && After[1] == CommandType::Open);
		NW_EXPECT(Sim.GetTick() == 0); // options do not advance the real simulation
	}

	void TestRoomDeterminism()
	{
		using namespace nw::room;
		const Layout Room = ParseLayout(TestRoom);
		RoomSim A(Room);
		RoomSim B(Room);
		for (RoomSim* Sim : { &A, &B })
		{
			Sim->Enqueue(RoomCmd(CommandType::PickLock, Room.OpeningAt({ 3, 5 })));
			Sim->Enqueue(RoomCmd(CommandType::GoThrough, Room.OpeningAt({ 3, 5 })));
			Sim->Enqueue(RoomCmd(CommandType::MoveTo, -1, { 8, 0 }));
		}
		while (!A.IsIdle())
		{
			A.Step();
			B.Step();
			NW_EXPECT(A.ComputeStateHash() == B.ComputeStateHash());
		}
		NW_EXPECT(B.IsIdle());
		NW_EXPECT(!A.GetRecords()[2].bFailed);
	}

	struct TestCase
	{
		const char* Name;
		void (*Run)();
	};
}

int main()
{
	const TestCase Tests[] = {
		{ "ReferencePlanSucceedsForAllSeeds", TestReferencePlanSucceedsForAllSeeds },
		{ "Determinism", TestDeterminism },
		{ "SeedChangesGuardBehaviour", TestSeedChangesGuardBehaviour },
		{ "GhostRunFromCopyMatchesOriginal", TestGhostRunFromCopyMatchesOriginal },
		{ "SkippingAlarmTriggersPolice", TestSkippingAlarmTriggersPolice },
		{ "LockedDoorBlocksPlan", TestLockedDoorBlocksPlan },
		{ "WalkingIntoShopGetsSpotted", TestWalkingIntoShopGetsSpotted },
		{ "NoiseMakesGuardInvestigate", TestNoiseMakesGuardInvestigate },
		{ "RerouteMidEdgeReachesNewTarget", TestRerouteMidEdgeReachesNewTarget },
		{ "InvalidDefinitionIsRejected", TestInvalidDefinitionIsRejected },
		{ "TimelineCoversReferencePlan", TestTimelineCoversReferencePlan },
		{ "TimelineMarksFailedAction", TestTimelineMarksFailedAction },
		{ "TimelineOfInvalidSetup", TestTimelineOfInvalidSetup },
		{ "RoomLayoutParses", TestRoomLayoutParses },
		{ "RoomWalkAndWalls", TestRoomWalkAndWalls },
		{ "RoomDoorsAndLocks", TestRoomDoorsAndLocks },
		{ "RoomClimbWindow", TestRoomClimbWindow },
		{ "RoomOptionsFollowQueue", TestRoomOptionsFollowQueue },
		{ "RoomDeterminism", TestRoomDeterminism },
	};

	for (const TestCase& Test : Tests)
	{
		const int Before = GFailures;
		Test.Run();
		std::printf("%s %s\n", GFailures == Before ? "[ OK ]" : "[FAIL]", Test.Name);
	}

	std::printf("%d failure(s)\n", GFailures);
	return GFailures == 0 ? 0 : 1;
}
