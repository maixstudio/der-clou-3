#pragma once

// Plain C++ (no Unreal headers): this file is also compiled by Tools/SimTests.
//
// Room prototype: a small grid world (50 cm cells) with walls, doors and windows, and crew
// members that follow a queue of commands ("plan by doing": every command the player gives is
// executed right away and recorded with its start and end tick for the timeline).

#include "Sim/SimTypes.h"

#include <string>
#include <vector>

namespace nw::room
{
	using nw::sim::Tick;
	using nw::sim::NoTick;
	using nw::sim::TicksPerSecond;

	constexpr int32_t CellCm = 50;

	struct Cell
	{
		int32_t X = 0;
		int32_t Y = 0;

		bool operator==(const Cell& Other) const { return X == Other.X && Y == Other.Y; }
		bool operator!=(const Cell& Other) const { return !(*this == Other); }
	};

	enum class CellType : uint8_t
	{
		Void,    // ' '  outside the playable area
		Floor,   // '.'  inside
		Ground,  // ','  outside, walkable
		Wall,    // '#'
		Door,    // 'D' (closed) or 'L' (closed and locked)
		Window   // 'W'
	};

	enum class OpeningKind : uint8_t
	{
		Door,
		Window
	};

	/** A door or window: one or more adjacent cells in a wall. */
	struct Opening
	{
		OpeningKind Kind = OpeningKind::Door;
		std::vector<Cell> Cells;
		/** True if the wall runs along X (the two sides are at Y-1 and Y+1). */
		bool bAlongX = true;
		bool bOpen = false;
		bool bLocked = false;
	};

	struct AgentDef
	{
		std::string Name;
		Cell Start;
		int32_t SpeedCmPerTick = 15;
		/** 0..5; 0 = cannot pick locks. */
		int32_t Lockpicking = 3;
	};

	/** Parsed room. Row 0 of the text is Y = 0, column 0 is X = 0. Digits mark agent starts. */
	struct Layout
	{
		int32_t Width = 0;
		int32_t Height = 0;
		std::vector<CellType> Cells;
		/** Opening index per cell, or -1. */
		std::vector<int32_t> CellOpening;
		std::vector<Opening> Openings;
		std::vector<AgentDef> Agents;
		std::string Error;

		bool IsValid() const { return Error.empty() && Width > 0 && Height > 0; }
		bool Contains(const Cell& C) const { return C.X >= 0 && C.Y >= 0 && C.X < Width && C.Y < Height; }
		CellType TypeAt(const Cell& C) const { return Contains(C) ? Cells[Index(C)] : CellType::Void; }
		int32_t OpeningAt(const Cell& C) const { return Contains(C) ? CellOpening[Index(C)] : -1; }
		size_t Index(const Cell& C) const { return static_cast<size_t>(C.Y) * static_cast<size_t>(Width) + static_cast<size_t>(C.X); }
	};

	/**
	 * Parses a text layout: '#' wall, '.' floor, ',' outdoor ground, ' ' void, 'D' door, 'L' locked door,
	 * 'W' window, '1'..'9' agent start on floor. Adjacent door/window cells form one opening.
	 */
	NWCORE_API Layout ParseLayout(const std::vector<std::string>& Rows);

	enum class CommandType : uint8_t
	{
		MoveTo,        // Target cell
		Open,          // Opening
		Close,         // Opening
		PickLock,      // Opening (doors)
		GoThrough,     // Opening (doors): opens a closed door, then walks to the other side
		ClimbThrough,  // Opening (windows): opens a closed window, then climbs to the other side
		Wait           // Duration
	};

	struct Command
	{
		CommandType Type = CommandType::Wait;
		int32_t Agent = 0;
		Cell Target;
		int32_t Opening = -1;
		Tick Duration = 0;
	};

	/** One executed (or queued) command for the timeline. */
	struct CommandRecord
	{
		Command Cmd;
		Tick Start = NoTick;
		Tick End = NoTick;
		bool bFailed = false;
		std::string FailReason;
	};

	enum class AgentPhase : uint8_t
	{
		Idle,
		Approach,  // walking to the target or to the opening
		Act,       // working on the opening (or waiting)
		Pass       // walking or climbing through the opening
	};

	struct AgentState
	{
		/** Centimetres, cell centres at X * CellCm + CellCm / 2. */
		int32_t PosX = 0;
		int32_t PosY = 0;
		Cell At;
		std::vector<Cell> Path;
		size_t PathIndex = 0;
		AgentPhase Phase = AgentPhase::Idle;
		/** Index into the records of the command being executed, or -1. */
		int32_t Current = -1;
		Tick Progress = 0;
		Tick Duration = 0;
		/** Climbing: start and end of the straight move through a window. */
		Cell ClimbFrom;
		Cell ClimbTo;
	};

	// Durations in ticks.
	constexpr Tick OpenDoorTicks = 10;
	constexpr Tick CloseDoorTicks = 8;
	constexpr Tick OpenWindowTicks = 15;
	constexpr Tick CloseWindowTicks = 12;
	constexpr Tick ClimbTicks = 30;
	/** Lock picking takes BasePickLockTicks * 4 / (Lockpicking + 1). */
	constexpr Tick BasePickLockTicks = 80;

	class NWCORE_API RoomSim
	{
	public:
		explicit RoomSim(Layout InLayout);

		/** Appends a command to its agent's queue. Returns the record index. */
		int32_t Enqueue(const Command& Cmd);

		void Step();
		/** Steps until every queue is done (bounded). */
		void RunUntilIdle(Tick MaxTicks = 36000);
		bool IsIdle() const;

		Tick GetTick() const { return CurrentTick; }
		const Layout& GetLayout() const { return Room; }
		const std::vector<AgentState>& GetAgents() const { return Agents; }
		const std::vector<Opening>& GetOpenings() const { return Room.Openings; }
		const std::vector<CommandRecord>& GetRecords() const { return Records; }

		/** Commands that make sense for the agent at this opening, given the state after its queue. */
		std::vector<CommandType> GetOptions(int32_t Agent, int32_t OpeningIndex) const;

		bool IsWalkable(const Cell& C) const;
		Tick PickLockTicks(int32_t Agent) const;

		uint64_t ComputeStateHash() const;

		static const char* ToString(CommandType Type);

	private:
		void StepAgent(int32_t Index);
		bool StartNext(int32_t Index);
		void Finish(int32_t Index);
		void Fail(int32_t Index, const char* Reason);
		/** Advances along the path. Returns true when the end is reached. */
		bool Advance(AgentState& Agent, int32_t Speed);
		/** Shortest path from the agent's cell to the nearest goal (8 directions, no corner cutting). */
		bool FindPath(const Cell& From, const std::vector<Cell>& Goals, std::vector<Cell>& OutPath) const;
		/** Floor cells directly in front of / behind the opening. */
		std::vector<Cell> SideCells(int32_t OpeningIndex) const;
		/** The cell on the other side of the opening, seen from Stand. */
		bool OppositeCell(int32_t OpeningIndex, const Cell& Stand, Cell& OutCell, Cell& OutThrough) const;

		Layout Room;
		std::vector<AgentState> Agents;
		std::vector<CommandRecord> Records;
		/** Per agent: record indices in queue order. */
		std::vector<std::vector<int32_t>> Queues;
		std::vector<size_t> QueuePos;
		Tick CurrentTick = 0;
	};

	inline int32_t CellCentreCm(int32_t CellCoordinate)
	{
		return CellCoordinate * CellCm + CellCm / 2;
	}
}
