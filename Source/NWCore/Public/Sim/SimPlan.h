#pragma once

#include "Sim/SimTypes.h"

#include <vector>

namespace nw::sim
{
	enum class ActionType : uint8_t
	{
		/** Target = node. Path avoids locked doors and opens closed ones. */
		MoveTo,
		/** DurationTicks. */
		Wait,
		/** Target = signal id. */
		Signal,
		/** Target = signal id. */
		WaitForSignal,
		/** Target = door. Must stand at one end of the door's edge. */
		PickLock,
		/** Target = alarm. Must stand at the alarm panel. */
		DisableAlarm,
		/** Target = safe. Must stand at the safe. */
		CrackSafe
	};

	struct PlanAction
	{
		ActionType Type = ActionType::Wait;
		int32_t Target = InvalidId;
		Tick DurationTicks = 0;

		static PlanAction MoveTo(NodeId Node) { return { ActionType::MoveTo, Node, 0 }; }
		static PlanAction Wait(Tick Duration) { return { ActionType::Wait, InvalidId, Duration }; }
		static PlanAction Signal(SignalId Id) { return { ActionType::Signal, Id, 0 }; }
		static PlanAction WaitForSignal(SignalId Id) { return { ActionType::WaitForSignal, Id, 0 }; }
		static PlanAction PickLock(DoorId Door) { return { ActionType::PickLock, Door, 0 }; }
		static PlanAction DisableAlarm(AlarmId Alarm) { return { ActionType::DisableAlarm, Alarm, 0 }; }
		static PlanAction CrackSafe(SafeId Safe) { return { ActionType::CrackSafe, Safe, 0 }; }
	};

	/** One track of the planner timeline. */
	struct CrewPlan
	{
		std::vector<PlanAction> Actions;
	};

	/** Crew[i] belongs to crew member i. */
	struct HeistPlan
	{
		std::vector<CrewPlan> Crew;
	};
}
