#include "Sim/SimTimeline.h"

namespace nw::sim
{
	PlanTimeline BuildTimeline(Simulation Sim)
	{
		PlanTimeline Timeline;
		Timeline.Result = Sim.GetResult();
		if (!Sim.IsValid())
		{
			Timeline.Error = Sim.GetError();
			return Timeline;
		}

		const std::vector<CrewState>& Crew = Sim.GetCrewStates();
		const HeistPlan& Plan = Sim.GetPlan();
		const size_t CrewCount = Crew.size();
		Timeline.Crew.resize(CrewCount);

		// LastIndex[i] = action index of crew member i as of the previous tick.
		std::vector<size_t> LastIndex(CrewCount, 0);
		for (size_t Index = 0; Index < CrewCount; ++Index)
		{
			const size_t PlanSize = Index < Plan.Crew.size() ? Plan.Crew[Index].Actions.size() : 0;
			Timeline.Crew[Index].Actions.resize(PlanSize);
			LastIndex[Index] = Crew[Index].ActionIndex;
			if (LastIndex[Index] < PlanSize && Crew[Index].Status == CrewStatus::Active)
			{
				Timeline.Crew[Index].Actions[LastIndex[Index]].Start = Sim.GetTick();
			}
		}

		while (!Sim.IsFinished())
		{
			Sim.Step();
			const Tick Now = Sim.GetTick();
			for (size_t Index = 0; Index < CrewCount; ++Index)
			{
				std::vector<ActionSpan>& Actions = Timeline.Crew[Index].Actions;
				size_t& Last = LastIndex[Index];
				while (Last < Crew[Index].ActionIndex && Last < Actions.size())
				{
					// Action Last completed during this tick; the next one starts now.
					Actions[Last].End = Now;
					++Last;
					if (Last < Actions.size())
					{
						Actions[Last].Start = Now;
					}
				}
			}
		}

		const Tick EndTick = Sim.GetTick();
		for (size_t Index = 0; Index < CrewCount; ++Index)
		{
			CrewTimeline& Track = Timeline.Crew[Index];
			Track.FinalStatus = Crew[Index].Status;
			const size_t Current = LastIndex[Index];
			if (Current < Track.Actions.size() && Track.Actions[Current].Start != NoTick)
			{
				Track.Actions[Current].End = EndTick;
				Track.Actions[Current].bUnfinished = true;
			}
		}

		for (const SimEvent& Event : Sim.GetEvents())
		{
			// Only crew-agent events are attributed here (guard events use guard indices as agent).
			if (Event.Type != EventType::ActionFailed && Event.Type != EventType::CrewEscaped)
			{
				continue;
			}
			if (Event.Agent < 0 || static_cast<size_t>(Event.Agent) >= CrewCount)
			{
				continue;
			}
			CrewTimeline& Track = Timeline.Crew[static_cast<size_t>(Event.Agent)];
			if (Event.Type == EventType::CrewEscaped)
			{
				Track.EscapedAt = Event.At;
			}
			else if (Event.Target >= 0 && static_cast<size_t>(Event.Target) < Track.Actions.size())
			{
				Track.Actions[static_cast<size_t>(Event.Target)].bFailed = true;
			}
		}

		Timeline.Events = Sim.GetEvents();
		Timeline.Result = Sim.GetResult();
		return Timeline;
	}
}
