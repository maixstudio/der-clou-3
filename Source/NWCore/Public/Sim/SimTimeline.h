#pragma once

// Plain C++ (no Unreal headers): this file is also compiled by Tools/SimTests.

#include "Sim/Simulation.h"

#include <vector>

namespace nw::sim
{
	/** When one plan action ran during a ghost run. Start/End are NoTick if it was never reached. */
	struct ActionSpan
	{
		Tick Start = NoTick;
		/** First tick after the action, or the end of the run if it was still running. */
		Tick End = NoTick;
		bool bFailed = false;
		/** The run ended (or the crew member got stuck) while this action was running. */
		bool bUnfinished = false;
	};

	struct CrewTimeline
	{
		std::vector<ActionSpan> Actions;
		Tick EscapedAt = NoTick;
		CrewStatus FinalStatus = CrewStatus::Active;
	};

	/**
	 * Result of simulating a plan to the end: per-action timing for the planner timeline,
	 * plus the event log (spotted, alarms, failures) for conflict markers.
	 */
	struct PlanTimeline
	{
		std::vector<CrewTimeline> Crew;
		std::vector<SimEvent> Events;
		SimResult Result;
		std::string Error;
	};

	/** Runs a copy of the given simulation to the end and records when each action ran. */
	NWCORE_API PlanTimeline BuildTimeline(Simulation Sim);
}
