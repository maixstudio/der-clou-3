#include "Sim/Simulation.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace nw::sim
{
	namespace
	{
		constexpr int32_t CrewMoveNoiseCm = 100;
		constexpr int32_t LockpickNoiseCm = 300;
		constexpr int32_t AlarmWorkNoiseCm = 50;
		constexpr int32_t SafeCrackNoiseCm = 150;
		constexpr Tick GuardSuspicionTicks = SecondsToTicks(5);

		int32_t LockBaseSeconds(LockType Lock)
		{
			switch (Lock)
			{
			case LockType::None:
				return 0;
			case LockType::Simple:
				return 30;
			case LockType::Security:
				return 60;
			}
			return 0;
		}

		/** Skill 1 takes 1.5x the base time, skill 2 = 1x, skill 5 = 0.5x. Skill 0 is impossible. */
		Tick SkillDuration(int32_t BaseSeconds, int32_t Skill)
		{
			if (Skill <= 0)
			{
				return NoTick;
			}
			const Tick Duration = SecondsToTicks(BaseSeconds) * 3 / (Skill + 1);
			return std::max<Tick>(Duration, 1);
		}

		class StateHasher
		{
		public:
			void Add(int64_t Value)
			{
				const uint64_t Bits = static_cast<uint64_t>(Value);
				for (int32_t ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
				{
					Hash ^= (Bits >> (ByteIndex * 8)) & 0xFFu;
					Hash *= 1099511628211ull;
				}
			}

			void AddMover(const MoverState& Move)
			{
				Add(Move.Node);
				Add(Move.NextNode);
				Add(Move.ProgressCm);
				Add(static_cast<int64_t>(Move.PathIndex));
				Add(static_cast<int64_t>(Move.Path.size()));
			}

			uint64_t Get() const
			{
				return Hash;
			}

		private:
			uint64_t Hash = 1469598103934665603ull;
		};
	}

	Simulation::Simulation(ObjectDefinition InDefinition, std::vector<CrewDef> InCrew, HeistPlan InPlan, uint64_t Seed)
		: Definition(std::move(InDefinition))
		, CrewDefs(std::move(InCrew))
		, Plan(std::move(InPlan))
		, Rng(Seed)
	{
		Plan.Crew.resize(CrewDefs.size());

		if (!ValidateDefinition(Definition, Error))
		{
			Result.Result = Outcome::InvalidSetup;
			return;
		}
		if (CrewDefs.empty())
		{
			Error = "no crew";
			Result.Result = Outcome::InvalidSetup;
			return;
		}
		for (const CrewDef& Member : CrewDefs)
		{
			if (Member.SpeedCmPerTick <= 0)
			{
				Error = "crew member '" + Member.Name + "' needs a positive speed";
				Result.Result = Outcome::InvalidSetup;
				return;
			}
		}

		BuildGraph();

		Doors.reserve(Definition.Doors.size());
		for (const DoorDef& Door : Definition.Doors)
		{
			DoorState State;
			State.bLocked = Door.bStartsLocked && Door.Lock != LockType::None;
			Doors.push_back(State);
		}
		Alarms.assign(Definition.Alarms.size(), AlarmState{});
		Safes.assign(Definition.Safes.size(), SafeState{});

		Crew.resize(CrewDefs.size());
		for (CrewState& State : Crew)
		{
			State.Move.Node = Definition.EntryNode;
		}

		Guards.resize(Definition.Guards.size());
		for (size_t GuardIndex = 0; GuardIndex < Guards.size(); ++GuardIndex)
		{
			Guards[GuardIndex].Move.Node = Definition.Guards[GuardIndex].StartNode;
		}
	}

	void Simulation::BuildGraph()
	{
		Adjacency.assign(Definition.Nodes.size(), {});
		EdgeLengths.assign(Definition.Edges.size(), 1);
		DoorEdges.assign(Definition.Doors.size(), InvalidId);

		for (size_t EdgeIndex = 0; EdgeIndex < Definition.Edges.size(); ++EdgeIndex)
		{
			const EdgeDef& Edge = Definition.Edges[EdgeIndex];
			const int32_t Index = static_cast<int32_t>(EdgeIndex);
			const IntVec2& PosA = Definition.Nodes[static_cast<size_t>(Edge.A)].Position;
			const IntVec2& PosB = Definition.Nodes[static_cast<size_t>(Edge.B)].Position;
			EdgeLengths[EdgeIndex] = std::max(IntSqrt(DistSq(PosA, PosB)), 1);
			Adjacency[static_cast<size_t>(Edge.A)].push_back(Index);
			Adjacency[static_cast<size_t>(Edge.B)].push_back(Index);
			if (Edge.Door != InvalidId)
			{
				DoorEdges[static_cast<size_t>(Edge.Door)] = Index;
			}
		}
	}

	int32_t Simulation::FindEdge(NodeId From, NodeId To) const
	{
		for (const int32_t EdgeIndex : Adjacency[static_cast<size_t>(From)])
		{
			const EdgeDef& Edge = Definition.Edges[static_cast<size_t>(EdgeIndex)];
			if ((Edge.A == From && Edge.B == To) || (Edge.B == From && Edge.A == To))
			{
				return EdgeIndex;
			}
		}
		return InvalidId;
	}

	bool Simulation::FindPath(NodeId From, NodeId To, bool bIsCrew, std::vector<NodeId>& OutPath) const
	{
		OutPath.clear();
		const int32_t NodeCount = static_cast<int32_t>(Definition.Nodes.size());
		if (From < 0 || To < 0 || From >= NodeCount || To >= NodeCount)
		{
			return false;
		}
		if (From == To)
		{
			return true;
		}

		// Dijkstra. Ties are broken by node id via std::pair ordering, so the result is deterministic.
		using Entry = std::pair<int64_t, NodeId>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Open;
		std::vector<int64_t> Dist(Definition.Nodes.size(), (std::numeric_limits<int64_t>::max)());
		std::vector<NodeId> Prev(Definition.Nodes.size(), InvalidId);

		Dist[static_cast<size_t>(From)] = 0;
		Open.push({ 0, From });
		while (!Open.empty())
		{
			const Entry Current = Open.top();
			Open.pop();
			const int64_t Cost = Current.first;
			const NodeId At = Current.second;
			if (Cost > Dist[static_cast<size_t>(At)])
			{
				continue;
			}
			if (At == To)
			{
				break;
			}
			for (const int32_t EdgeIndex : Adjacency[static_cast<size_t>(At)])
			{
				const EdgeDef& Edge = Definition.Edges[static_cast<size_t>(EdgeIndex)];
				if (bIsCrew && Edge.Door != InvalidId && Doors[static_cast<size_t>(Edge.Door)].bLocked)
				{
					continue;
				}
				const NodeId Other = Edge.A == At ? Edge.B : Edge.A;
				const int64_t NewCost = Cost + EdgeLengths[static_cast<size_t>(EdgeIndex)];
				if (NewCost < Dist[static_cast<size_t>(Other)])
				{
					Dist[static_cast<size_t>(Other)] = NewCost;
					Prev[static_cast<size_t>(Other)] = At;
					Open.push({ NewCost, Other });
				}
			}
		}

		if (Prev[static_cast<size_t>(To)] == InvalidId)
		{
			return false;
		}
		for (NodeId At = To; At != From; At = Prev[static_cast<size_t>(At)])
		{
			OutPath.push_back(At);
		}
		std::reverse(OutPath.begin(), OutPath.end());
		return true;
	}

	bool Simulation::SetDestination(MoverState& Move, NodeId Target, bool bIsCrew) const
	{
		// A mover on an edge always finishes that edge first; the new path starts at its end.
		const NodeId Start = Move.NextNode != InvalidId ? Move.NextNode : Move.Node;
		Move.PathIndex = 0;
		return FindPath(Start, Target, bIsCrew, Move.Path);
	}

	Simulation::MoveResult Simulation::Advance(MoverState& Move, int32_t SpeedCm, int32_t CrewIndex)
	{
		const bool bIsCrew = CrewIndex != InvalidId;
		int32_t Budget = SpeedCm;
		while (true)
		{
			if (Move.NextNode == InvalidId)
			{
				if (Move.PathIndex >= Move.Path.size())
				{
					return MoveResult::Arrived;
				}
				if (Budget <= 0)
				{
					return MoveResult::Moving;
				}
				const NodeId Next = Move.Path[Move.PathIndex];
				const int32_t EdgeIndex = FindEdge(Move.Node, Next);
				if (EdgeIndex == InvalidId)
				{
					return MoveResult::Blocked;
				}
				if (bIsCrew)
				{
					const DoorId Door = Definition.Edges[static_cast<size_t>(EdgeIndex)].Door;
					if (Door != InvalidId)
					{
						const DoorState& State = Doors[static_cast<size_t>(Door)];
						if (State.bLocked)
						{
							return MoveResult::Blocked;
						}
						if (!State.bOpen)
						{
							OpenDoor(Door, CrewIndex, Move.Node);
						}
					}
				}
				Move.NextNode = Next;
				Move.EdgeIndex = EdgeIndex;
				Move.ProgressCm = 0;
			}

			if (Budget <= 0)
			{
				return MoveResult::Moving;
			}

			const int32_t Remaining = EdgeLengths[static_cast<size_t>(Move.EdgeIndex)] - Move.ProgressCm;
			if (Budget >= Remaining)
			{
				Budget -= Remaining;
				Move.Node = Move.NextNode;
				Move.NextNode = InvalidId;
				Move.EdgeIndex = InvalidId;
				Move.ProgressCm = 0;
				// The edge may have been started before the path was re-planned (SetDestination);
				// only consume the waypoint if this node actually is the next one on the path.
				if (Move.PathIndex < Move.Path.size() && Move.Path[Move.PathIndex] == Move.Node)
				{
					++Move.PathIndex;
				}
				if (bIsCrew)
				{
					OnCrewEnteredNode(CrewIndex, Move.Node);
				}
			}
			else
			{
				Move.ProgressCm += Budget;
				Budget = 0;
			}
		}
	}

	void Simulation::Step()
	{
		if (IsFinished())
		{
			return;
		}

		PendingNoise.clear();

		const int32_t CrewCount = static_cast<int32_t>(Crew.size());
		for (int32_t CrewIndex = 0; CrewIndex < CrewCount; ++CrewIndex)
		{
			StepCrew(CrewIndex);
		}

		const int32_t GuardCount = static_cast<int32_t>(Guards.size());
		for (int32_t GuardIndex = 0; GuardIndex < GuardCount; ++GuardIndex)
		{
			PerceiveGuard(GuardIndex);
			StepGuard(GuardIndex);
		}

		++CurrentTick;
		EvaluateEnd();
	}

	SimResult Simulation::RunToEnd()
	{
		while (!IsFinished())
		{
			Step();
		}
		return Result;
	}

	bool Simulation::IsAtNode(const CrewState& State, NodeId Node) const
	{
		return State.Move.NextNode == InvalidId && State.Move.Node == Node;
	}

	void Simulation::StepCrew(int32_t CrewIndex)
	{
		CrewState& State = Crew[static_cast<size_t>(CrewIndex)];
		if (State.Status == CrewStatus::Blocked || State.Status == CrewStatus::Escaped)
		{
			return;
		}

		const CrewPlan& Track = Plan.Crew[static_cast<size_t>(CrewIndex)];
		if (State.ActionIndex >= Track.Actions.size())
		{
			if (IsAtNode(State, Definition.EntryNode))
			{
				State.Status = CrewStatus::Escaped;
				BankedLoot += State.CarriedLoot;
				State.CarriedLoot = 0;
				AddEvent(EventType::CrewEscaped, CrewIndex, InvalidId);
			}
			else
			{
				State.Status = CrewStatus::Idle;
			}
			return;
		}

		const PlanAction& Action = Track.Actions[State.ActionIndex];
		const CrewDef& Member = CrewDefs[static_cast<size_t>(CrewIndex)];

		switch (Action.Type)
		{
		case ActionType::MoveTo:
		{
			if (!State.bActionStarted)
			{
				if (!SetDestination(State.Move, Action.Target, true))
				{
					FailAction(CrewIndex);
					return;
				}
				State.bActionStarted = true;
			}
			const MoveResult Moved = Advance(State.Move, Member.SpeedCmPerTick, CrewIndex);
			EmitNoise(State.Move.Node, CrewMoveNoiseCm);
			if (Moved == MoveResult::Blocked)
			{
				FailAction(CrewIndex);
			}
			else if (Moved == MoveResult::Arrived)
			{
				CompleteAction(State);
			}
			break;
		}

		case ActionType::Wait:
			if (++State.ActionProgress >= Action.DurationTicks)
			{
				CompleteAction(State);
			}
			break;

		case ActionType::Signal:
			Signals.insert(Action.Target);
			AddEvent(EventType::SignalSent, CrewIndex, Action.Target);
			CompleteAction(State);
			break;

		case ActionType::WaitForSignal:
			if (Signals.count(Action.Target) > 0)
			{
				CompleteAction(State);
			}
			break;

		case ActionType::PickLock:
		{
			const DoorId Door = Action.Target;
			if (Door < 0 || Door >= static_cast<int32_t>(Doors.size()))
			{
				FailAction(CrewIndex);
				return;
			}
			DoorState& Lock = Doors[static_cast<size_t>(Door)];
			if (!State.bActionStarted)
			{
				const int32_t DoorEdge = DoorEdges[static_cast<size_t>(Door)];
				const bool bAtDoor = DoorEdge != InvalidId
					&& (IsAtNode(State, Definition.Edges[static_cast<size_t>(DoorEdge)].A)
						|| IsAtNode(State, Definition.Edges[static_cast<size_t>(DoorEdge)].B));
				if (!bAtDoor)
				{
					FailAction(CrewIndex);
					return;
				}
				if (!Lock.bLocked)
				{
					CompleteAction(State);
					return;
				}
				State.ActionDuration = SkillDuration(LockBaseSeconds(Definition.Doors[static_cast<size_t>(Door)].Lock), Member.Lockpicking);
				if (State.ActionDuration == NoTick)
				{
					FailAction(CrewIndex);
					return;
				}
				State.bActionStarted = true;
			}
			EmitNoise(State.Move.Node, LockpickNoiseCm);
			if (++State.ActionProgress >= State.ActionDuration)
			{
				Lock.bLocked = false;
				AddEvent(EventType::DoorUnlocked, CrewIndex, Door);
				CompleteAction(State);
			}
			break;
		}

		case ActionType::DisableAlarm:
		{
			const AlarmId Alarm = Action.Target;
			if (Alarm < 0 || Alarm >= static_cast<int32_t>(Alarms.size()))
			{
				FailAction(CrewIndex);
				return;
			}
			const AlarmDef& AlarmInfo = Definition.Alarms[static_cast<size_t>(Alarm)];
			if (!State.bActionStarted)
			{
				if (!IsAtNode(State, AlarmInfo.PanelNode))
				{
					FailAction(CrewIndex);
					return;
				}
				if (!Alarms[static_cast<size_t>(Alarm)].bArmed)
				{
					CompleteAction(State);
					return;
				}
				State.ActionDuration = SkillDuration(AlarmInfo.BaseDisableSeconds, Member.Electrics);
				if (State.ActionDuration == NoTick)
				{
					FailAction(CrewIndex);
					return;
				}
				State.bActionStarted = true;
			}
			EmitNoise(State.Move.Node, AlarmWorkNoiseCm);
			if (++State.ActionProgress >= State.ActionDuration)
			{
				Alarms[static_cast<size_t>(Alarm)].bArmed = false;
				AddEvent(EventType::AlarmDisabled, CrewIndex, Alarm);
				CompleteAction(State);
			}
			break;
		}

		case ActionType::CrackSafe:
		{
			const SafeId Safe = Action.Target;
			if (Safe < 0 || Safe >= static_cast<int32_t>(Safes.size()))
			{
				FailAction(CrewIndex);
				return;
			}
			const SafeDef& SafeInfo = Definition.Safes[static_cast<size_t>(Safe)];
			if (!State.bActionStarted)
			{
				if (!IsAtNode(State, SafeInfo.Node))
				{
					FailAction(CrewIndex);
					return;
				}
				if (Safes[static_cast<size_t>(Safe)].bCracked)
				{
					CompleteAction(State);
					return;
				}
				State.ActionDuration = SkillDuration(SafeInfo.BaseCrackSeconds, Member.Safecracking);
				if (State.ActionDuration == NoTick)
				{
					FailAction(CrewIndex);
					return;
				}
				State.bActionStarted = true;
			}
			EmitNoise(State.Move.Node, SafeCrackNoiseCm);
			if (++State.ActionProgress >= State.ActionDuration)
			{
				Safes[static_cast<size_t>(Safe)].bCracked = true;
				State.CarriedLoot += SafeInfo.LootValue;
				AddEvent(EventType::SafeCracked, CrewIndex, Safe);
				CompleteAction(State);
			}
			break;
		}
		}
	}

	void Simulation::PerceiveGuard(int32_t GuardIndex)
	{
		GuardState& Guard = Guards[static_cast<size_t>(GuardIndex)];
		if (Guard.Status == GuardStatus::Alerted)
		{
			return;
		}

		const GuardDef& Info = Definition.Guards[static_cast<size_t>(GuardIndex)];
		const IntVec2 GuardPos = GetPosition(Guard.Move);
		const int32_t GuardRoom = GetRoom(Guard.Move);
		const int64_t SightSq = static_cast<int64_t>(Info.SightRangeCm) * Info.SightRangeCm;

		// Sight
		for (size_t CrewIndex = 0; CrewIndex < Crew.size(); ++CrewIndex)
		{
			const CrewState& Member = Crew[CrewIndex];
			if (Member.Status == CrewStatus::Escaped || GetRoom(Member.Move) != GuardRoom)
			{
				continue;
			}
			if (DistSq(GuardPos, GetPosition(Member.Move)) <= SightSq)
			{
				Guard.Status = GuardStatus::Alerted;
				Guard.Move.Path.clear();
				Guard.Move.PathIndex = 0;
				Result.bAlarmRaised = true;
				AddEvent(EventType::GuardSpotted, GuardIndex, static_cast<int32_t>(CrewIndex));
				CallPolice(Info.ResponseTicks);
				return;
			}
		}

		// Hearing
		for (const Noise& Heard : PendingNoise)
		{
			const int64_t RadiusSq = static_cast<int64_t>(Heard.RadiusCm) * Heard.RadiusCm;
			if (DistSq(GuardPos, Heard.Position) > RadiusSq)
			{
				continue;
			}
			if (Guard.Status != GuardStatus::Investigating || Guard.InvestigateNode != Heard.Node)
			{
				Guard.Status = GuardStatus::Investigating;
				Guard.InvestigateNode = Heard.Node;
				Guard.bInvestigateArrived = false;
				Guard.bHasPath = false;
				AddEvent(EventType::GuardSuspicious, GuardIndex, Heard.Node);
			}
			else if (Guard.bInvestigateArrived)
			{
				Guard.WaitRemaining = GuardSuspicionTicks;
			}
			return;
		}
	}

	void Simulation::StepGuard(int32_t GuardIndex)
	{
		GuardState& Guard = Guards[static_cast<size_t>(GuardIndex)];
		const GuardDef& Info = Definition.Guards[static_cast<size_t>(GuardIndex)];

		switch (Guard.Status)
		{
		case GuardStatus::Alerted:
			break;

		case GuardStatus::Patrolling:
		{
			if (Info.Route.empty())
			{
				break;
			}
			const PatrolPoint& Point = Info.Route[Guard.RouteIndex];
			if (!Guard.bHasPath)
			{
				SetDestination(Guard.Move, Point.Node, false);
				Guard.bHasPath = true;
			}
			if (Advance(Guard.Move, Info.SpeedCmPerTick, InvalidId) == MoveResult::Arrived)
			{
				Guard.WaitRemaining = Rng.RangeInclusive(Point.WaitMinTicks, Point.WaitMaxTicks);
				Guard.Status = GuardStatus::Waiting;
				Guard.bHasPath = false;
			}
			break;
		}

		case GuardStatus::Waiting:
			if (Guard.WaitRemaining > 0)
			{
				--Guard.WaitRemaining;
			}
			else
			{
				Guard.RouteIndex = (Guard.RouteIndex + 1) % Info.Route.size();
				Guard.Status = GuardStatus::Patrolling;
			}
			break;

		case GuardStatus::Investigating:
			if (!Guard.bInvestigateArrived)
			{
				if (!Guard.bHasPath)
				{
					SetDestination(Guard.Move, Guard.InvestigateNode, false);
					Guard.bHasPath = true;
				}
				if (Advance(Guard.Move, Info.SpeedCmPerTick, InvalidId) == MoveResult::Arrived)
				{
					Guard.bInvestigateArrived = true;
					Guard.WaitRemaining = GuardSuspicionTicks;
				}
			}
			else if (Guard.WaitRemaining > 0)
			{
				--Guard.WaitRemaining;
			}
			else
			{
				Guard.Status = GuardStatus::Patrolling;
				Guard.InvestigateNode = InvalidId;
				Guard.bInvestigateArrived = false;
				Guard.bHasPath = false;
			}
			break;
		}
	}

	void Simulation::CompleteAction(CrewState& State)
	{
		++State.ActionIndex;
		State.ActionProgress = 0;
		State.ActionDuration = 0;
		State.bActionStarted = false;
		State.Move.Path.clear();
		State.Move.PathIndex = 0;
	}

	void Simulation::FailAction(int32_t CrewIndex)
	{
		CrewState& State = Crew[static_cast<size_t>(CrewIndex)];
		State.Status = CrewStatus::Blocked;
		AddEvent(EventType::ActionFailed, CrewIndex, static_cast<int32_t>(State.ActionIndex));
	}

	void Simulation::OpenDoor(DoorId Door, int32_t CrewIndex, NodeId AtNode)
	{
		Doors[static_cast<size_t>(Door)].bOpen = true;
		AddEvent(EventType::DoorOpened, CrewIndex, Door);

		const DoorDef& Info = Definition.Doors[static_cast<size_t>(Door)];
		EmitNoise(AtNode, Info.OpenNoiseRadiusCm);
		if (Info.Alarm != InvalidId)
		{
			TriggerAlarm(Info.Alarm, CrewIndex);
		}
	}

	void Simulation::OnCrewEnteredNode(int32_t CrewIndex, NodeId Node)
	{
		for (size_t AlarmIndex = 0; AlarmIndex < Definition.Alarms.size(); ++AlarmIndex)
		{
			const AlarmDef& Info = Definition.Alarms[AlarmIndex];
			if (Info.Type == AlarmType::LightBarrier && Info.GuardedNode == Node)
			{
				TriggerAlarm(static_cast<AlarmId>(AlarmIndex), CrewIndex);
			}
		}
	}

	void Simulation::TriggerAlarm(AlarmId Alarm, int32_t CrewIndex)
	{
		AlarmState& State = Alarms[static_cast<size_t>(Alarm)];
		if (!State.bArmed || State.bTriggered)
		{
			return;
		}
		State.bTriggered = true;
		Result.bAlarmRaised = true;
		AddEvent(EventType::AlarmTriggered, CrewIndex, Alarm);
		CallPolice(Definition.Alarms[static_cast<size_t>(Alarm)].PoliceResponseTicks);
	}

	void Simulation::CallPolice(Tick ResponseTicks)
	{
		const Tick Arrival = CurrentTick + ResponseTicks;
		if (PoliceArrivalTick == NoTick || Arrival < PoliceArrivalTick)
		{
			PoliceArrivalTick = Arrival;
		}
	}

	void Simulation::EmitNoise(NodeId AtNode, int32_t RadiusCm)
	{
		if (RadiusCm <= 0)
		{
			return;
		}
		Noise Sound;
		Sound.Position = Definition.Nodes[static_cast<size_t>(AtNode)].Position;
		Sound.RadiusCm = RadiusCm;
		Sound.Node = AtNode;
		PendingNoise.push_back(Sound);
	}

	void Simulation::AddEvent(EventType Type, int32_t Agent, int32_t Target)
	{
		SimEvent Event;
		Event.At = CurrentTick;
		Event.Type = Type;
		Event.Agent = Agent;
		Event.Target = Target;
		Events.push_back(Event);
	}

	void Simulation::EvaluateEnd()
	{
		const bool bAllEscaped = std::all_of(Crew.begin(), Crew.end(),
			[](const CrewState& State) { return State.Status == CrewStatus::Escaped; });
		if (bAllEscaped)
		{
			Finish(Result.bAlarmRaised ? Outcome::EscapedWithAlarm : Outcome::CleanGetaway);
			return;
		}
		if (PoliceArrivalTick != NoTick && CurrentTick >= PoliceArrivalTick)
		{
			AddEvent(EventType::PoliceArrived, InvalidId, InvalidId);
			Finish(Outcome::Busted);
			return;
		}
		if (CurrentTick >= Definition.TimeLimitTicks)
		{
			Finish(Outcome::TimedOut);
		}
	}

	void Simulation::Finish(Outcome Value)
	{
		Result.Result = Value;
		Result.EndTick = CurrentTick;
		Result.LootValue = BankedLoot;
	}

	IntVec2 Simulation::GetPosition(const MoverState& Move) const
	{
		const IntVec2& From = Definition.Nodes[static_cast<size_t>(Move.Node)].Position;
		if (Move.NextNode == InvalidId)
		{
			return From;
		}
		const IntVec2& To = Definition.Nodes[static_cast<size_t>(Move.NextNode)].Position;
		const int64_t Length = EdgeLengths[static_cast<size_t>(Move.EdgeIndex)];
		IntVec2 Position;
		Position.X = static_cast<int32_t>(From.X + (static_cast<int64_t>(To.X) - From.X) * Move.ProgressCm / Length);
		Position.Y = static_cast<int32_t>(From.Y + (static_cast<int64_t>(To.Y) - From.Y) * Move.ProgressCm / Length);
		return Position;
	}

	int32_t Simulation::GetRoom(const MoverState& Move) const
	{
		if (Move.NextNode == InvalidId)
		{
			return Definition.Nodes[static_cast<size_t>(Move.Node)].RoomId;
		}
		const bool bFirstHalf = Move.ProgressCm * 2 < EdgeLengths[static_cast<size_t>(Move.EdgeIndex)];
		return Definition.Nodes[static_cast<size_t>(bFirstHalf ? Move.Node : Move.NextNode)].RoomId;
	}

	uint64_t Simulation::ComputeStateHash() const
	{
		StateHasher Hasher;
		Hasher.Add(CurrentTick);
		Hasher.Add(PoliceArrivalTick);
		Hasher.Add(BankedLoot);
		Hasher.Add(static_cast<int64_t>(Result.Result));
		Hasher.Add(static_cast<int64_t>(Rng.GetState()));
		for (const CrewState& State : Crew)
		{
			Hasher.AddMover(State.Move);
			Hasher.Add(static_cast<int64_t>(State.Status));
			Hasher.Add(static_cast<int64_t>(State.ActionIndex));
			Hasher.Add(State.ActionProgress);
			Hasher.Add(State.CarriedLoot);
		}
		for (const GuardState& State : Guards)
		{
			Hasher.AddMover(State.Move);
			Hasher.Add(static_cast<int64_t>(State.Status));
			Hasher.Add(static_cast<int64_t>(State.RouteIndex));
			Hasher.Add(State.WaitRemaining);
			Hasher.Add(State.InvestigateNode);
		}
		for (const DoorState& State : Doors)
		{
			Hasher.Add((State.bLocked ? 1 : 0) | (State.bOpen ? 2 : 0));
		}
		for (const AlarmState& State : Alarms)
		{
			Hasher.Add((State.bArmed ? 1 : 0) | (State.bTriggered ? 2 : 0));
		}
		for (const SafeState& State : Safes)
		{
			Hasher.Add(State.bCracked ? 1 : 0);
		}
		for (const SignalId Id : Signals)
		{
			Hasher.Add(Id);
		}
		Hasher.Add(static_cast<int64_t>(Events.size()));
		return Hasher.Get();
	}

	const char* Simulation::ToString(Outcome Value)
	{
		switch (Value)
		{
		case Outcome::Running: return "Running";
		case Outcome::CleanGetaway: return "CleanGetaway";
		case Outcome::EscapedWithAlarm: return "EscapedWithAlarm";
		case Outcome::Busted: return "Busted";
		case Outcome::TimedOut: return "TimedOut";
		case Outcome::InvalidSetup: return "InvalidSetup";
		}
		return "Unknown";
	}

	const char* Simulation::ToString(EventType Value)
	{
		switch (Value)
		{
		case EventType::DoorOpened: return "DoorOpened";
		case EventType::DoorUnlocked: return "DoorUnlocked";
		case EventType::AlarmDisabled: return "AlarmDisabled";
		case EventType::AlarmTriggered: return "AlarmTriggered";
		case EventType::SafeCracked: return "SafeCracked";
		case EventType::SignalSent: return "SignalSent";
		case EventType::GuardSuspicious: return "GuardSuspicious";
		case EventType::GuardSpotted: return "GuardSpotted";
		case EventType::ActionFailed: return "ActionFailed";
		case EventType::CrewEscaped: return "CrewEscaped";
		case EventType::PoliceArrived: return "PoliceArrived";
		}
		return "Unknown";
	}
}
