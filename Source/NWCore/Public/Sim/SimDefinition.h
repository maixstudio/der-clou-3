#pragma once

#include "Sim/SimTypes.h"

#include <string>
#include <vector>

namespace nw::sim
{
	enum class LockType : uint8_t
	{
		None,
		Simple,
		Security
	};

	enum class AlarmType : uint8_t
	{
		/** Triggers when an attached door is opened. */
		Contact,
		/** Triggers when a crew member enters the guarded node. */
		LightBarrier
	};

	/** A point on the navigation graph. Rooms decide line of sight (same room = visible). */
	struct NodeDef
	{
		std::string Label;
		IntVec2 Position;
		int32_t RoomId = 0;
	};

	struct EdgeDef
	{
		NodeId A = InvalidId;
		NodeId B = InvalidId;
		/** Optional door on this edge. Each door belongs to at most one edge. */
		DoorId Door = InvalidId;
	};

	struct DoorDef
	{
		std::string Label;
		LockType Lock = LockType::None;
		bool bStartsLocked = false;
		AlarmId Alarm = InvalidId;
		int32_t OpenNoiseRadiusCm = 150;
	};

	struct AlarmDef
	{
		std::string Label;
		AlarmType Type = AlarmType::Contact;
		/** Where the alarm can be disabled. */
		NodeId PanelNode = InvalidId;
		/** Only for LightBarrier. */
		NodeId GuardedNode = InvalidId;
		int32_t BaseDisableSeconds = 90;
		Tick PoliceResponseTicks = SecondsToTicks(180);
	};

	struct SafeDef
	{
		std::string Label;
		NodeId Node = InvalidId;
		int32_t BaseCrackSeconds = 240;
		int32_t LootValue = 0;
	};

	struct PatrolPoint
	{
		NodeId Node = InvalidId;
		Tick WaitMinTicks = 0;
		Tick WaitMaxTicks = 0;
	};

	struct GuardDef
	{
		std::string Name;
		NodeId StartNode = InvalidId;
		std::vector<PatrolPoint> Route;
		int32_t SpeedCmPerTick = 12;
		int32_t SightRangeCm = 900;
		/** Police response time once this guard spots someone. */
		Tick ResponseTicks = SecondsToTicks(120);
	};

	/** Static description of a heist target. Immutable during a run. */
	struct ObjectDefinition
	{
		std::string Name;
		std::vector<NodeDef> Nodes;
		std::vector<EdgeDef> Edges;
		std::vector<DoorDef> Doors;
		std::vector<AlarmDef> Alarms;
		std::vector<SafeDef> Safes;
		std::vector<GuardDef> Guards;
		/** Crew starts here and escapes from here (getaway car). */
		NodeId EntryNode = InvalidId;
		Tick TimeLimitTicks = SecondsToTicks(1800);
	};

	struct CrewDef
	{
		std::string Name;
		/** Skills 0..5. A skill of 0 means the crew member cannot perform that action. */
		int32_t Lockpicking = 0;
		int32_t Electrics = 0;
		int32_t Safecracking = 0;
		int32_t SpeedCmPerTick = 10;
		/** 0..5, reserved for panic behaviour. */
		int32_t Nerves = 3;
	};

	/** Checks indices and value ranges. Returns false and fills OutError on the first problem. */
	NWCORE_API bool ValidateDefinition(const ObjectDefinition& Definition, std::string& OutError);
}
