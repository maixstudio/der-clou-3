#include "Sim/RoomSim.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>
#include <utility>

namespace nw::room
{
	namespace
	{
		bool IsOpeningType(CellType Type)
		{
			return Type == CellType::Door || Type == CellType::Window;
		}

		bool IsBlockingNeighbour(CellType Type)
		{
			return Type == CellType::Wall || IsOpeningType(Type);
		}

		class Hasher
		{
		public:
			void Add(int64_t Value)
			{
				for (int Byte = 0; Byte < 8; ++Byte)
				{
					State ^= static_cast<uint64_t>((Value >> (Byte * 8)) & 0xff);
					State *= 1099511628211ull;
				}
			}
			uint64_t Get() const { return State; }

		private:
			uint64_t State = 1469598103934665603ull;
		};
	}

	Layout ParseLayout(const std::vector<std::string>& Rows)
	{
		Layout Out;
		Out.Height = static_cast<int32_t>(Rows.size());
		for (const std::string& Row : Rows)
		{
			Out.Width = std::max(Out.Width, static_cast<int32_t>(Row.size()));
		}
		if (Out.Width == 0 || Out.Height == 0)
		{
			Out.Error = "empty layout";
			return Out;
		}

		const size_t CellCount = static_cast<size_t>(Out.Width) * static_cast<size_t>(Out.Height);
		Out.Cells.assign(CellCount, CellType::Void);
		Out.CellOpening.assign(CellCount, -1);
		std::vector<bool> Locked(CellCount, false);
		std::vector<int32_t> StartIndex;

		for (int32_t Y = 0; Y < Out.Height; ++Y)
		{
			const std::string& Row = Rows[static_cast<size_t>(Y)];
			for (int32_t X = 0; X < static_cast<int32_t>(Row.size()); ++X)
			{
				const Cell C{ X, Y };
				const char Ch = Row[static_cast<size_t>(X)];
				CellType Type = CellType::Void;
				switch (Ch)
				{
				case '#': Type = CellType::Wall; break;
				case '.': Type = CellType::Floor; break;
				case ',': Type = CellType::Ground; break;
				case 'D': Type = CellType::Door; break;
				case 'L': Type = CellType::Door; Locked[Out.Index(C)] = true; break;
				case 'W': Type = CellType::Window; break;
				case ' ': Type = CellType::Void; break;
				default:
					if (Ch >= '1' && Ch <= '9')
					{
						Type = CellType::Floor;
						const size_t AgentIndex = static_cast<size_t>(Ch - '1');
						if (Out.Agents.size() <= AgentIndex)
						{
							Out.Agents.resize(AgentIndex + 1);
							StartIndex.resize(AgentIndex + 1, -1);
						}
						Out.Agents[AgentIndex].Name = std::string("Figur ") + Ch;
						Out.Agents[AgentIndex].Start = C;
						StartIndex[AgentIndex] = 1;
					}
					else
					{
						Out.Error = std::string("unknown layout character '") + Ch + "'";
						return Out;
					}
					break;
				}
				Out.Cells[Out.Index(C)] = Type;
			}
		}
		for (size_t Index = 0; Index < StartIndex.size(); ++Index)
		{
			if (StartIndex[Index] < 0)
			{
				Out.Error = "agent start " + std::to_string(Index + 1) + " is missing";
				return Out;
			}
		}

		// Group adjacent door / window cells into openings (4-neighbourhood, same kind).
		for (int32_t Y = 0; Y < Out.Height; ++Y)
		{
			for (int32_t X = 0; X < Out.Width; ++X)
			{
				const Cell Seed{ X, Y };
				const CellType Type = Out.TypeAt(Seed);
				if (!IsOpeningType(Type) || Out.OpeningAt(Seed) >= 0)
				{
					continue;
				}
				const int32_t OpeningIndex = static_cast<int32_t>(Out.Openings.size());
				Opening& New = Out.Openings.emplace_back();
				New.Kind = Type == CellType::Door ? OpeningKind::Door : OpeningKind::Window;

				std::vector<Cell> Stack{ Seed };
				Out.CellOpening[Out.Index(Seed)] = OpeningIndex;
				while (!Stack.empty())
				{
					const Cell C = Stack.back();
					Stack.pop_back();
					New.Cells.push_back(C);
					New.bLocked = New.bLocked || Locked[Out.Index(C)];
					const Cell Neighbours[] = { { C.X + 1, C.Y }, { C.X - 1, C.Y }, { C.X, C.Y + 1 }, { C.X, C.Y - 1 } };
					for (const Cell& N : Neighbours)
					{
						if (Out.TypeAt(N) == Type && Out.OpeningAt(N) < 0)
						{
							Out.CellOpening[Out.Index(N)] = OpeningIndex;
							Stack.push_back(N);
						}
					}
				}
				std::sort(New.Cells.begin(), New.Cells.end(), [](const Cell& A, const Cell& B)
				{
					return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
				});

				if (New.Cells.size() > 1)
				{
					New.bAlongX = New.Cells.front().Y == New.Cells.back().Y;
				}
				else
				{
					const Cell C = New.Cells.front();
					New.bAlongX = IsBlockingNeighbour(Out.TypeAt({ C.X - 1, C.Y })) || IsBlockingNeighbour(Out.TypeAt({ C.X + 1, C.Y }));
				}
			}
		}
		return Out;
	}

	RoomSim::RoomSim(Layout InLayout)
		: Room(std::move(InLayout))
	{
		Agents.resize(Room.Agents.size());
		Queues.resize(Room.Agents.size());
		QueuePos.assign(Room.Agents.size(), 0);
		for (size_t Index = 0; Index < Agents.size(); ++Index)
		{
			AgentState& Agent = Agents[Index];
			Agent.At = Room.Agents[Index].Start;
			Agent.PosX = CellCentreCm(Agent.At.X);
			Agent.PosY = CellCentreCm(Agent.At.Y);
		}
	}

	int32_t RoomSim::Enqueue(const Command& Cmd)
	{
		const int32_t RecordIndex = static_cast<int32_t>(Records.size());
		CommandRecord& Record = Records.emplace_back();
		Record.Cmd = Cmd;
		if (Cmd.Agent < 0 || static_cast<size_t>(Cmd.Agent) >= Agents.size())
		{
			Record.bFailed = true;
			Record.FailReason = "unknown agent";
			Record.Start = Record.End = CurrentTick;
			return RecordIndex;
		}
		Queues[static_cast<size_t>(Cmd.Agent)].push_back(RecordIndex);
		return RecordIndex;
	}

	bool RoomSim::IsIdle() const
	{
		for (size_t Index = 0; Index < Agents.size(); ++Index)
		{
			if (Agents[Index].Current >= 0 || QueuePos[Index] < Queues[Index].size())
			{
				return false;
			}
		}
		return true;
	}

	void RoomSim::Step()
	{
		++CurrentTick;
		for (int32_t Index = 0; Index < static_cast<int32_t>(Agents.size()); ++Index)
		{
			StepAgent(Index);
		}
	}

	void RoomSim::RunUntilIdle(Tick MaxTicks)
	{
		for (Tick Count = 0; Count < MaxTicks && !IsIdle(); ++Count)
		{
			Step();
		}
	}

	bool RoomSim::IsWalkable(const Cell& C) const
	{
		switch (Room.TypeAt(C))
		{
		case CellType::Floor:
		case CellType::Ground:
			return true;
		case CellType::Door:
			return Room.Openings[static_cast<size_t>(Room.OpeningAt(C))].bOpen;
		default:
			return false;
		}
	}

	Tick RoomSim::PickLockTicks(int32_t Agent) const
	{
		const int32_t Skill = std::max(0, Room.Agents[static_cast<size_t>(Agent)].Lockpicking);
		return BasePickLockTicks * 4 / (Skill + 1);
	}

	bool RoomSim::FindPath(const Cell& From, const std::vector<Cell>& Goals, std::vector<Cell>& OutPath) const
	{
		OutPath.clear();
		if (Goals.empty() || !Room.Contains(From))
		{
			return false;
		}
		for (const Cell& Goal : Goals)
		{
			if (Goal == From)
			{
				return true;
			}
		}

		const size_t CellCount = Room.Cells.size();
		constexpr int32_t Unvisited = std::numeric_limits<int32_t>::max();
		std::vector<int32_t> Dist(CellCount, Unvisited);
		std::vector<int32_t> Parent(CellCount, -1);
		std::vector<bool> IsGoal(CellCount, false);
		for (const Cell& Goal : Goals)
		{
			if (Room.Contains(Goal) && IsWalkable(Goal))
			{
				IsGoal[Room.Index(Goal)] = true;
			}
		}

		using Entry = std::pair<int32_t, int32_t>; // cost, cell index (ties broken by index: deterministic)
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Open;
		const int32_t Start = static_cast<int32_t>(Room.Index(From));
		Dist[static_cast<size_t>(Start)] = 0;
		Open.push({ 0, Start });

		static constexpr int32_t DX[] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		static constexpr int32_t DY[] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		int32_t Found = -1;
		while (!Open.empty())
		{
			const auto [Cost, Index] = Open.top();
			Open.pop();
			if (Cost > Dist[static_cast<size_t>(Index)])
			{
				continue;
			}
			if (IsGoal[static_cast<size_t>(Index)])
			{
				Found = Index;
				break;
			}
			const Cell C{ Index % Room.Width, Index / Room.Width };
			for (int Dir = 0; Dir < 8; ++Dir)
			{
				const Cell N{ C.X + DX[Dir], C.Y + DY[Dir] };
				if (!Room.Contains(N) || !IsWalkable(N))
				{
					continue;
				}
				const bool bDiagonal = Dir >= 4;
				if (bDiagonal && (!IsWalkable({ C.X + DX[Dir], C.Y }) || !IsWalkable({ C.X, C.Y + DY[Dir] })))
				{
					continue; // no corner cutting
				}
				const int32_t NewCost = Cost + (bDiagonal ? 14 : 10);
				const size_t NIndex = Room.Index(N);
				if (NewCost < Dist[NIndex])
				{
					Dist[NIndex] = NewCost;
					Parent[NIndex] = Index;
					Open.push({ NewCost, static_cast<int32_t>(NIndex) });
				}
			}
		}
		if (Found < 0)
		{
			return false;
		}
		for (int32_t Index = Found; Index != Start; Index = Parent[static_cast<size_t>(Index)])
		{
			OutPath.push_back({ Index % Room.Width, Index / Room.Width });
		}
		std::reverse(OutPath.begin(), OutPath.end());
		return true;
	}

	std::vector<Cell> RoomSim::SideCells(int32_t OpeningIndex) const
	{
		std::vector<Cell> Out;
		const Opening& O = Room.Openings[static_cast<size_t>(OpeningIndex)];
		for (const Cell& C : O.Cells)
		{
			const Cell Sides[] = { O.bAlongX ? Cell{ C.X, C.Y - 1 } : Cell{ C.X - 1, C.Y }, O.bAlongX ? Cell{ C.X, C.Y + 1 } : Cell{ C.X + 1, C.Y } };
			for (const Cell& S : Sides)
			{
				const CellType Type = Room.TypeAt(S);
				if (Type == CellType::Floor || Type == CellType::Ground)
				{
					Out.push_back(S);
				}
			}
		}
		return Out;
	}

	bool RoomSim::OppositeCell(int32_t OpeningIndex, const Cell& Stand, Cell& OutCell, Cell& OutThrough) const
	{
		const Opening& O = Room.Openings[static_cast<size_t>(OpeningIndex)];
		for (const Cell& C : O.Cells)
		{
			if (O.bAlongX && Stand.X == C.X && std::abs(Stand.Y - C.Y) == 1)
			{
				OutThrough = C;
				OutCell = { C.X, 2 * C.Y - Stand.Y };
				return true;
			}
			if (!O.bAlongX && Stand.Y == C.Y && std::abs(Stand.X - C.X) == 1)
			{
				OutThrough = C;
				OutCell = { 2 * C.X - Stand.X, C.Y };
				return true;
			}
		}
		return false;
	}

	bool RoomSim::StartNext(int32_t Index)
	{
		AgentState& Agent = Agents[static_cast<size_t>(Index)];
		const size_t AgentIndex = static_cast<size_t>(Index);
		if (QueuePos[AgentIndex] >= Queues[AgentIndex].size())
		{
			return false;
		}
		Agent.Current = Queues[AgentIndex][QueuePos[AgentIndex]];
		CommandRecord& Record = Records[static_cast<size_t>(Agent.Current)];
		Record.Start = CurrentTick;
		Agent.Progress = 0;
		Agent.Duration = 0;
		Agent.Path.clear();
		Agent.PathIndex = 0;

		const Command& Cmd = Record.Cmd;
		if (Cmd.Type == CommandType::Wait)
		{
			Agent.Phase = AgentPhase::Act;
			Agent.Duration = std::max<Tick>(1, Cmd.Duration);
			return true;
		}
		if (Cmd.Type == CommandType::MoveTo)
		{
			if (!IsWalkable(Cmd.Target))
			{
				Fail(Index, "Ziel ist nicht begehbar");
				return true;
			}
			if (!FindPath(Agent.At, { Cmd.Target }, Agent.Path))
			{
				Fail(Index, "kein Weg dorthin");
				return true;
			}
			Agent.Phase = AgentPhase::Approach;
			return true;
		}

		if (Cmd.Opening < 0 || static_cast<size_t>(Cmd.Opening) >= Room.Openings.size())
		{
			Fail(Index, "unbekannte Tür oder Fenster");
			return true;
		}
		if (!FindPath(Agent.At, SideCells(Cmd.Opening), Agent.Path))
		{
			Fail(Index, "kein Weg dorthin");
			return true;
		}
		Agent.Phase = AgentPhase::Approach;
		return true;
	}

	void RoomSim::Finish(int32_t Index)
	{
		AgentState& Agent = Agents[static_cast<size_t>(Index)];
		Records[static_cast<size_t>(Agent.Current)].End = CurrentTick;
		Agent.Current = -1;
		Agent.Phase = AgentPhase::Idle;
		Agent.Path.clear();
		Agent.PathIndex = 0;
		++QueuePos[static_cast<size_t>(Index)];
	}

	void RoomSim::Fail(int32_t Index, const char* Reason)
	{
		AgentState& Agent = Agents[static_cast<size_t>(Index)];
		CommandRecord& Record = Records[static_cast<size_t>(Agent.Current)];
		Record.bFailed = true;
		Record.FailReason = Reason;
		Finish(Index);
	}

	bool RoomSim::Advance(AgentState& Agent, int32_t Speed)
	{
		int32_t Remaining = Speed;
		while (Remaining > 0 && Agent.PathIndex < Agent.Path.size())
		{
			const Cell& Next = Agent.Path[Agent.PathIndex];
			const int32_t TX = CellCentreCm(Next.X);
			const int32_t TY = CellCentreCm(Next.Y);
			const int64_t DX = TX - Agent.PosX;
			const int64_t DY = TY - Agent.PosY;
			const int32_t Dist = nw::sim::IntSqrt(DX * DX + DY * DY);
			if (Dist <= Remaining)
			{
				Agent.PosX = TX;
				Agent.PosY = TY;
				Agent.At = Next;
				Remaining -= Dist;
				++Agent.PathIndex;
			}
			else
			{
				Agent.PosX += static_cast<int32_t>(DX * Remaining / Dist);
				Agent.PosY += static_cast<int32_t>(DY * Remaining / Dist);
				Remaining = 0;
			}
		}
		return Agent.PathIndex >= Agent.Path.size();
	}

	void RoomSim::StepAgent(int32_t Index)
	{
		AgentState& Agent = Agents[static_cast<size_t>(Index)];
		if (Agent.Current < 0 && !StartNext(Index))
		{
			return;
		}
		if (Agent.Current < 0)
		{
			return; // started and failed immediately
		}

		const Command Cmd = Records[static_cast<size_t>(Agent.Current)].Cmd;
		const int32_t Speed = Room.Agents[static_cast<size_t>(Index)].SpeedCmPerTick;
		Opening* O = Cmd.Opening >= 0 && static_cast<size_t>(Cmd.Opening) < Room.Openings.size() ? &Room.Openings[static_cast<size_t>(Cmd.Opening)] : nullptr;
		const bool bDoor = O && O->Kind == OpeningKind::Door;

		auto BeginPass = [&]()
		{
			Cell Other;
			Cell Through;
			if (!OppositeCell(Cmd.Opening, Agent.At, Other, Through))
			{
				Fail(Index, "steht nicht vor der Öffnung");
				return;
			}
			const CellType OtherType = Room.TypeAt(Other);
			if (OtherType != CellType::Floor && OtherType != CellType::Ground)
			{
				Fail(Index, "die andere Seite ist versperrt");
				return;
			}
			Agent.Phase = AgentPhase::Pass;
			Agent.Progress = 0;
			if (bDoor)
			{
				Agent.Path = { Through, Other };
				Agent.PathIndex = 0;
			}
			else
			{
				Agent.ClimbFrom = Agent.At;
				Agent.ClimbTo = Other;
				Agent.Duration = ClimbTicks;
			}
		};

		switch (Agent.Phase)
		{
		case AgentPhase::Approach:
		{
			if (!Advance(Agent, Speed))
			{
				return;
			}
			if (Cmd.Type == CommandType::MoveTo)
			{
				Finish(Index);
				return;
			}
			// Standing at the opening: decide what to do.
			Agent.Progress = 0;
			switch (Cmd.Type)
			{
			case CommandType::Open:
				if (O->bOpen) { Finish(Index); return; }
				if (O->bLocked) { Fail(Index, "verschlossen"); return; }
				Agent.Duration = bDoor ? OpenDoorTicks : OpenWindowTicks;
				break;
			case CommandType::Close:
				if (!O->bOpen) { Finish(Index); return; }
				Agent.Duration = bDoor ? CloseDoorTicks : CloseWindowTicks;
				break;
			case CommandType::PickLock:
				if (!bDoor) { Fail(Index, "nur Türen haben Schlösser"); return; }
				if (!O->bLocked) { Finish(Index); return; }
				if (Room.Agents[static_cast<size_t>(Index)].Lockpicking <= 0) { Fail(Index, "kann keine Schlösser knacken"); return; }
				Agent.Duration = PickLockTicks(Index);
				break;
			case CommandType::GoThrough:
				if (!bDoor) { Fail(Index, "durch Fenster steigt man"); return; }
				if (O->bLocked) { Fail(Index, "verschlossen"); return; }
				if (O->bOpen) { BeginPass(); return; }
				Agent.Duration = OpenDoorTicks;
				break;
			case CommandType::ClimbThrough:
				if (bDoor) { Fail(Index, "durch Türen geht man"); return; }
				if (O->bOpen) { BeginPass(); return; }
				Agent.Duration = OpenWindowTicks;
				break;
			default:
				break;
			}
			Agent.Phase = AgentPhase::Act;
			return;
		}

		case AgentPhase::Act:
		{
			if (++Agent.Progress < Agent.Duration)
			{
				return;
			}
			switch (Cmd.Type)
			{
			case CommandType::Wait: Finish(Index); return;
			case CommandType::Open: O->bOpen = true; Finish(Index); return;
			case CommandType::Close: O->bOpen = false; Finish(Index); return;
			case CommandType::PickLock: O->bLocked = false; Finish(Index); return;
			case CommandType::GoThrough:
			case CommandType::ClimbThrough:
				O->bOpen = true;
				BeginPass();
				return;
			default:
				Finish(Index);
				return;
			}
		}

		case AgentPhase::Pass:
		{
			if (bDoor)
			{
				if (Advance(Agent, Speed))
				{
					Finish(Index);
				}
				return;
			}
			// Climbing: straight line from the stand cell through the window to the other side.
			++Agent.Progress;
			const int32_t FX = CellCentreCm(Agent.ClimbFrom.X);
			const int32_t FY = CellCentreCm(Agent.ClimbFrom.Y);
			const int32_t TX = CellCentreCm(Agent.ClimbTo.X);
			const int32_t TY = CellCentreCm(Agent.ClimbTo.Y);
			Agent.PosX = FX + (TX - FX) * Agent.Progress / Agent.Duration;
			Agent.PosY = FY + (TY - FY) * Agent.Progress / Agent.Duration;
			if (Agent.Progress >= Agent.Duration)
			{
				Agent.At = Agent.ClimbTo;
				Finish(Index);
			}
			return;
		}

		case AgentPhase::Idle:
			return;
		}
	}

	std::vector<CommandType> RoomSim::GetOptions(int32_t Agent, int32_t OpeningIndex) const
	{
		std::vector<CommandType> Out;
		if (Agent < 0 || static_cast<size_t>(Agent) >= Agents.size() || OpeningIndex < 0 || static_cast<size_t>(OpeningIndex) >= Room.Openings.size())
		{
			return Out;
		}
		// Options depend on the state once the queued commands are done.
		RoomSim Future(*this);
		Future.RunUntilIdle();
		const Opening& O = Future.Room.Openings[static_cast<size_t>(OpeningIndex)];
		if (O.Kind == OpeningKind::Door)
		{
			if (O.bLocked)
			{
				Out.push_back(CommandType::PickLock);
			}
			else
			{
				Out.push_back(CommandType::GoThrough);
				Out.push_back(O.bOpen ? CommandType::Close : CommandType::Open);
			}
		}
		else
		{
			Out.push_back(CommandType::ClimbThrough);
			Out.push_back(O.bOpen ? CommandType::Close : CommandType::Open);
		}
		return Out;
	}

	uint64_t RoomSim::ComputeStateHash() const
	{
		Hasher H;
		H.Add(CurrentTick);
		for (const AgentState& Agent : Agents)
		{
			H.Add(Agent.PosX);
			H.Add(Agent.PosY);
			H.Add(Agent.At.X);
			H.Add(Agent.At.Y);
			H.Add(static_cast<int64_t>(Agent.Phase));
			H.Add(Agent.Current);
			H.Add(Agent.Progress);
		}
		for (const Opening& O : Room.Openings)
		{
			H.Add(O.bOpen ? 1 : 0);
			H.Add(O.bLocked ? 1 : 0);
		}
		for (const CommandRecord& Record : Records)
		{
			H.Add(Record.Start);
			H.Add(Record.End);
			H.Add(Record.bFailed ? 1 : 0);
		}
		return H.Get();
	}

	const char* RoomSim::ToString(CommandType Type)
	{
		switch (Type)
		{
		case CommandType::MoveTo: return "MoveTo";
		case CommandType::Open: return "Open";
		case CommandType::Close: return "Close";
		case CommandType::PickLock: return "PickLock";
		case CommandType::GoThrough: return "GoThrough";
		case CommandType::ClimbThrough: return "ClimbThrough";
		case CommandType::Wait: return "Wait";
		}
		return "?";
	}
}
