#pragma once

#include "Sim/SimDefinition.h"
#include "Sim/SimPlan.h"
#include "Sim/SimRandom.h"

#include <set>
#include <string>
#include <vector>

namespace nw::sim
{
	enum class EventType : uint8_t
	{
		DoorOpened,      // Agent = crew, Target = door
		DoorUnlocked,    // Agent = crew, Target = door
		AlarmDisabled,   // Agent = crew, Target = alarm
		AlarmTriggered,  // Agent = crew, Target = alarm
		SafeCracked,     // Agent = crew, Target = safe
		SignalSent,      // Agent = crew, Target = signal
		GuardSuspicious, // Agent = guard, Target = node being investigated
		GuardSpotted,    // Agent = guard, Target = crew
		ActionFailed,    // Agent = crew, Target = action index
		CrewEscaped,     // Agent = crew
		PoliceArrived    // Agent = -1
	};

	struct SimEvent
	{
		Tick At = 0;
		EventType Type = EventType::DoorOpened;
		int32_t Agent = InvalidId;
		int32_t Target = InvalidId;
	};

	enum class Outcome : uint8_t
	{
		Running,
		CleanGetaway,
		EscapedWithAlarm,
		Busted,
		TimedOut,
		InvalidSetup
	};

	struct SimResult
	{
		Outcome Result = Outcome::Running;
		Tick EndTick = 0;
		int32_t LootValue = 0;
		bool bAlarmRaised = false;
	};

	/** Position on the graph: at Node, or on the edge Node -> NextNode. */
	struct MoverState
	{
		NodeId Node = InvalidId;
		NodeId NextNode = InvalidId;
		int32_t EdgeIndex = InvalidId;
		int32_t ProgressCm = 0;
		std::vector<NodeId> Path;
		size_t PathIndex = 0;
	};

	enum class CrewStatus : uint8_t
	{
		Active,
		/** Plan finished somewhere other than the getaway car. */
		Idle,
		/** An action failed; the crew member waits for the player to intervene. */
		Blocked,
		Escaped
	};

	struct CrewState
	{
		MoverState Move;
		CrewStatus Status = CrewStatus::Active;
		size_t ActionIndex = 0;
		Tick ActionProgress = 0;
		Tick ActionDuration = 0;
		bool bActionStarted = false;
		int32_t CarriedLoot = 0;
	};

	enum class GuardStatus : uint8_t
	{
		Patrolling,
		Waiting,
		Investigating,
		Alerted
	};

	struct GuardState
	{
		MoverState Move;
		GuardStatus Status = GuardStatus::Patrolling;
		size_t RouteIndex = 0;
		Tick WaitRemaining = 0;
		NodeId InvestigateNode = InvalidId;
		bool bInvestigateArrived = false;
		bool bHasPath = false;
	};

	struct DoorState
	{
		bool bLocked = false;
		bool bOpen = false;
	};

	struct AlarmState
	{
		bool bArmed = true;
		bool bTriggered = false;
	};

	struct SafeState
	{
		bool bCracked = false;
	};

	/**
	 * Deterministic heist simulation. Same definition + crew + plan + seed => identical run.
	 *
	 * The class is copyable: copying a running simulation and calling RunToEnd() on the copy
	 * is how the "ghost run" preview works.
	 *
	 * Simplifications of this first version (see docs/02-game-design.md for the target design):
	 * - Guards carry keys and ignore doors and alarms.
	 * - Line of sight = same room and within sight range.
	 * - Hearing = guard within the noise radius.
	 */
	class NWCORE_API Simulation
	{
	public:
		Simulation(ObjectDefinition InDefinition, std::vector<CrewDef> InCrew, HeistPlan InPlan, uint64_t Seed);

		/** Advances the simulation by one tick (1 / TicksPerSecond seconds). */
		void Step();

		/** Steps until the heist has ended. */
		SimResult RunToEnd();

		bool IsFinished() const { return Result.Result != Outcome::Running; }
		bool IsValid() const { return Result.Result != Outcome::InvalidSetup; }
		const std::string& GetError() const { return Error; }

		Tick GetTick() const { return CurrentTick; }
		Tick GetPoliceArrivalTick() const { return PoliceArrivalTick; }
		const SimResult& GetResult() const { return Result; }
		const std::vector<SimEvent>& GetEvents() const { return Events; }

		const ObjectDefinition& GetDefinition() const { return Definition; }
		const std::vector<CrewDef>& GetCrewDefs() const { return CrewDefs; }
		const std::vector<CrewState>& GetCrewStates() const { return Crew; }
		const std::vector<GuardState>& GetGuardStates() const { return Guards; }
		const std::vector<DoorState>& GetDoorStates() const { return Doors; }
		const std::vector<AlarmState>& GetAlarmStates() const { return Alarms; }
		const std::vector<SafeState>& GetSafeStates() const { return Safes; }

		IntVec2 GetPosition(const MoverState& Move) const;
		int32_t GetRoom(const MoverState& Move) const;

		/** FNV-1a hash over the complete mutable state. Used by determinism tests. */
		uint64_t ComputeStateHash() const;

		static const char* ToString(Outcome Value);
		static const char* ToString(EventType Value);

	private:
		struct Noise
		{
			IntVec2 Position;
			int32_t RadiusCm = 0;
			NodeId Node = InvalidId;
		};

		enum class MoveResult : uint8_t
		{
			Moving,
			Arrived,
			Blocked
		};

		void BuildGraph();
		int32_t FindEdge(NodeId From, NodeId To) const;
		bool FindPath(NodeId From, NodeId To, bool bIsCrew, std::vector<NodeId>& OutPath) const;
		bool SetDestination(MoverState& Move, NodeId Target, bool bIsCrew) const;
		MoveResult Advance(MoverState& Move, int32_t SpeedCm, int32_t CrewIndex);

		void StepCrew(int32_t CrewIndex);
		void PerceiveGuard(int32_t GuardIndex);
		void StepGuard(int32_t GuardIndex);

		bool IsAtNode(const CrewState& State, NodeId Node) const;
		void CompleteAction(CrewState& State);
		void FailAction(int32_t CrewIndex);
		void OpenDoor(DoorId Door, int32_t CrewIndex, NodeId AtNode);
		void OnCrewEnteredNode(int32_t CrewIndex, NodeId Node);
		void TriggerAlarm(AlarmId Alarm, int32_t CrewIndex);
		void CallPolice(Tick ResponseTicks);
		void EmitNoise(NodeId AtNode, int32_t RadiusCm);
		void AddEvent(EventType Type, int32_t Agent, int32_t Target);
		void EvaluateEnd();
		void Finish(Outcome Value);

		ObjectDefinition Definition;
		std::vector<CrewDef> CrewDefs;
		HeistPlan Plan;
		Random Rng;
		std::string Error;

		std::vector<int32_t> EdgeLengths;
		std::vector<std::vector<int32_t>> Adjacency;
		std::vector<int32_t> DoorEdges;

		Tick CurrentTick = 0;
		Tick PoliceArrivalTick = NoTick;
		int32_t BankedLoot = 0;
		SimResult Result;

		std::vector<CrewState> Crew;
		std::vector<GuardState> Guards;
		std::vector<DoorState> Doors;
		std::vector<AlarmState> Alarms;
		std::vector<SafeState> Safes;
		std::set<SignalId> Signals;
		std::vector<Noise> PendingNoise;
		std::vector<SimEvent> Events;
	};
}
