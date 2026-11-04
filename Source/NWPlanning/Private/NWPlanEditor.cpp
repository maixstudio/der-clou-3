#include "NWPlanEditor.h"

#include "NWSimConversion.h"
#include "Sim/Simulation.h"

#include <utility>

#define LOCTEXT_NAMESPACE "NWPlanEditor"

namespace
{
	constexpr int32 MaxUndoSteps = 100;

	FText Utf8(const std::string& Value)
	{
		return FText::FromString(UTF8_TO_TCHAR(Value.c_str()));
	}

	int64 DistSq(const FIntPoint& A, const FIntPoint& B)
	{
		const int64 DX = static_cast<int64>(A.X) - B.X;
		const int64 DY = static_cast<int64>(A.Y) - B.Y;
		return DX * DX + DY * DY;
	}

	FIntPoint ToPoint(const nw::sim::IntVec2& Value)
	{
		return FIntPoint(Value.X, Value.Y);
	}
}

void UNWPlanEditor::Initialize(nw::sim::ObjectDefinition InObject, std::vector<nw::sim::CrewDef> InCrew, TArray<FNWCrewPlan> InPlan, uint64 InSeed)
{
	Object = std::move(InObject);
	Crew = std::move(InCrew);
	Seed = InSeed;
	Plan = MoveTemp(InPlan);
	Plan.SetNum(GetCrewCount());
	UndoStack.Reset();
	RedoStack.Reset();
	SelectedCrew = 0;
	bPicking = false;
	Execution = FNWExecutionState();
	Refresh();
}

FText UNWPlanEditor::GetCrewName(int32 CrewIndex) const
{
	return IsValidCrew(CrewIndex) ? Utf8(Crew[static_cast<size_t>(CrewIndex)].Name) : FText::GetEmpty();
}

FText UNWPlanEditor::GetObjectName() const
{
	return Utf8(Object.Name);
}

void UNWPlanEditor::SelectCrew(int32 CrewIndex)
{
	if (IsValidCrew(CrewIndex) && CrewIndex != SelectedCrew)
	{
		SelectedCrew = CrewIndex;
		OnChanged.Broadcast();
	}
}

void UNWPlanEditor::AddAction(int32 CrewIndex, const FNWPlanAction& Action)
{
	if (!IsValidCrew(CrewIndex))
	{
		return;
	}
	TArray<FNWCrewPlan> NewPlan = Plan;
	NewPlan[CrewIndex].Actions.Add(Action);
	Commit(MoveTemp(NewPlan));
}

void UNWPlanEditor::RemoveAction(int32 CrewIndex, int32 ActionIndex)
{
	if (!IsValidCrew(CrewIndex) || !Plan[CrewIndex].Actions.IsValidIndex(ActionIndex))
	{
		return;
	}
	TArray<FNWCrewPlan> NewPlan = Plan;
	NewPlan[CrewIndex].Actions.RemoveAt(ActionIndex);
	Commit(MoveTemp(NewPlan));
}

void UNWPlanEditor::ClearCrew(int32 CrewIndex)
{
	if (!IsValidCrew(CrewIndex) || Plan[CrewIndex].Actions.IsEmpty())
	{
		return;
	}
	TArray<FNWCrewPlan> NewPlan = Plan;
	NewPlan[CrewIndex].Actions.Reset();
	Commit(MoveTemp(NewPlan));
}

bool UNWPlanEditor::Undo()
{
	if (UndoStack.IsEmpty())
	{
		return false;
	}
	RedoStack.Push(MoveTemp(Plan));
	Plan = UndoStack.Pop();
	Refresh();
	return true;
}

bool UNWPlanEditor::Redo()
{
	if (RedoStack.IsEmpty())
	{
		return false;
	}
	UndoStack.Push(MoveTemp(Plan));
	Plan = RedoStack.Pop();
	Refresh();
	return true;
}

void UNWPlanEditor::Commit(TArray<FNWCrewPlan> NewPlan)
{
	UndoStack.Push(MoveTemp(Plan));
	if (UndoStack.Num() > MaxUndoSteps)
	{
		UndoStack.RemoveAt(0);
	}
	RedoStack.Reset();
	Plan = MoveTemp(NewPlan);
	Refresh();
}

void UNWPlanEditor::Refresh()
{
	Timeline = nw::sim::BuildTimeline(nw::sim::Simulation(Object, Crew, NWSimConversion::ToSim(Plan), Seed));
	OnChanged.Broadcast();
}

void UNWPlanEditor::BeginPick(ENWPlanActionType Type)
{
	bPicking = true;
	PendingType = Type;
	OnChanged.Broadcast();
}

void UNWPlanEditor::CancelPick()
{
	if (bPicking)
	{
		bPicking = false;
		OnChanged.Broadcast();
	}
}

TArray<FNWPickCandidate> UNWPlanEditor::GetPickCandidates() const
{
	TArray<FNWPickCandidate> Out;
	if (!bPicking)
	{
		return Out;
	}

	switch (PendingType)
	{
	case ENWPlanActionType::MoveTo:
		for (int32 Index = 0; Index < static_cast<int32>(Object.Nodes.size()); ++Index)
		{
			Out.Add({ ToPoint(Object.Nodes[static_cast<size_t>(Index)].Position), Index });
		}
		break;

	case ENWPlanActionType::PickLock:
		for (const nw::sim::EdgeDef& Edge : Object.Edges)
		{
			if (Edge.Door == nw::sim::InvalidId || Object.Doors[static_cast<size_t>(Edge.Door)].Lock == nw::sim::LockType::None)
			{
				continue;
			}
			const FIntPoint A = ToPoint(Object.Nodes[static_cast<size_t>(Edge.A)].Position);
			const FIntPoint B = ToPoint(Object.Nodes[static_cast<size_t>(Edge.B)].Position);
			Out.Add({ (A + B) / 2, Edge.Door });
		}
		break;

	case ENWPlanActionType::DisableAlarm:
		for (int32 Index = 0; Index < static_cast<int32>(Object.Alarms.size()); ++Index)
		{
			const nw::sim::AlarmDef& Alarm = Object.Alarms[static_cast<size_t>(Index)];
			Out.Add({ ToPoint(Object.Nodes[static_cast<size_t>(Alarm.PanelNode)].Position), Index });
		}
		break;

	case ENWPlanActionType::CrackSafe:
		for (int32 Index = 0; Index < static_cast<int32>(Object.Safes.size()); ++Index)
		{
			Out.Add({ ToPoint(Object.Nodes[static_cast<size_t>(Object.Safes[static_cast<size_t>(Index)].Node)].Position), Index });
		}
		break;

	default:
		break;
	}
	return Out;
}

int32 UNWPlanEditor::CountTargeted(ENWPlanActionType Type, int32 Target) const
{
	int32 Count = 0;
	for (const FNWCrewPlan& Track : Plan)
	{
		for (const FNWPlanAction& Action : Track.Actions)
		{
			Count += (Action.Type == Type && Action.Target == Target) ? 1 : 0;
		}
	}
	return Count;
}

bool UNWPlanEditor::PickAt(const FIntPoint& PositionCm)
{
	if (!bPicking || !IsValidCrew(SelectedCrew))
	{
		return false;
	}

	const TArray<FNWPickCandidate> Candidates = GetPickCandidates();
	const FNWPickCandidate* Nearest = nullptr;
	int64 NearestDistSq = static_cast<int64>(PickRadiusCm) * PickRadiusCm;
	for (const FNWPickCandidate& Candidate : Candidates)
	{
		const int64 D = DistSq(Candidate.PositionCm, PositionCm);
		if (D <= NearestDistSq)
		{
			Nearest = &Candidate;
			NearestDistSq = D;
		}
	}
	if (!Nearest)
	{
		return false;
	}

	// Several targets can share a spot (two alarms on one panel): take the one planned least so far.
	const FNWPickCandidate* Best = Nearest;
	int32 BestUses = MAX_int32;
	for (const FNWPickCandidate& Candidate : Candidates)
	{
		if (Candidate.PositionCm == Nearest->PositionCm)
		{
			const int32 Uses = CountTargeted(PendingType, Candidate.Target);
			if (Uses < BestUses)
			{
				Best = &Candidate;
				BestUses = Uses;
			}
		}
	}

	FNWPlanAction Action;
	Action.Type = PendingType;
	Action.Target = Best->Target;
	bPicking = false;
	AddAction(SelectedCrew, Action);
	return true;
}

FText UNWPlanEditor::DescribeAction(const FNWPlanAction& Action, bool bShort) const
{
	auto Label = [](const auto& Items, int32 Index) -> FText
	{
		return Index >= 0 && Index < static_cast<int32>(Items.size()) ? Utf8(Items[static_cast<size_t>(Index)].Label) : FText::AsNumber(Index);
	};
	const FText Signal = FText::AsNumber(Action.Target);

	switch (Action.Type)
	{
	case ENWPlanActionType::MoveTo:
		return bShort ? Label(Object.Nodes, Action.Target) : FText::Format(LOCTEXT("MoveTo", "Gehe zu: {0}"), Label(Object.Nodes, Action.Target));
	case ENWPlanActionType::Wait:
		return FText::Format(bShort ? LOCTEXT("WaitShort", "{0} s") : LOCTEXT("Wait", "Warten: {0} s"), FText::AsNumber(FMath::RoundToInt(Action.DurationSeconds)));
	case ENWPlanActionType::Signal:
		return FText::Format(bShort ? LOCTEXT("SignalShort", "Signal {0}") : LOCTEXT("Signal", "Signal {0} senden"), Signal);
	case ENWPlanActionType::WaitForSignal:
		return FText::Format(bShort ? LOCTEXT("WaitSignalShort", "auf {0}") : LOCTEXT("WaitSignal", "Warte auf Signal {0}"), Signal);
	case ENWPlanActionType::PickLock:
		return bShort ? LOCTEXT("PickLockShort", "Schloss") : FText::Format(LOCTEXT("PickLock", "Schloss knacken: {0}"), Label(Object.Doors, Action.Target));
	case ENWPlanActionType::DisableAlarm:
		return bShort ? LOCTEXT("AlarmShort", "Alarm") : FText::Format(LOCTEXT("Alarm", "Alarm aus: {0}"), Label(Object.Alarms, Action.Target));
	case ENWPlanActionType::CrackSafe:
		return bShort ? LOCTEXT("SafeShort", "Tresor") : FText::Format(LOCTEXT("Safe", "Tresor knacken: {0}"), Label(Object.Safes, Action.Target));
	}
	return FText::GetEmpty();
}

FText UNWPlanEditor::DescribeOutcome(nw::sim::Outcome Outcome)
{
	switch (Outcome)
	{
	case nw::sim::Outcome::Running: return LOCTEXT("Running", "läuft");
	case nw::sim::Outcome::CleanGetaway: return LOCTEXT("CleanGetaway", "Sauber entkommen");
	case nw::sim::Outcome::EscapedWithAlarm: return LOCTEXT("EscapedWithAlarm", "Entkommen, aber Alarm");
	case nw::sim::Outcome::Busted: return LOCTEXT("Busted", "Geschnappt");
	case nw::sim::Outcome::TimedOut: return LOCTEXT("TimedOut", "Zeit abgelaufen");
	case nw::sim::Outcome::InvalidSetup: return LOCTEXT("InvalidSetup", "Ungültiger Aufbau");
	}
	return FText::GetEmpty();
}

FText UNWPlanEditor::DescribeEvent(const nw::sim::SimEvent& Event) const
{
	switch (Event.Type)
	{
	case nw::sim::EventType::GuardSpotted: return LOCTEXT("EvSpotted", "Von einer Wache gesehen");
	case nw::sim::EventType::GuardSuspicious: return LOCTEXT("EvSuspicious", "Wache wird misstrauisch");
	case nw::sim::EventType::AlarmTriggered: return LOCTEXT("EvAlarm", "Alarm ausgelöst");
	case nw::sim::EventType::ActionFailed: return LOCTEXT("EvFailed", "Aktion gescheitert");
	case nw::sim::EventType::PoliceArrived: return LOCTEXT("EvPolice", "Polizei eingetroffen");
	default: return Utf8(nw::sim::Simulation::ToString(Event.Type));
	}
}

#undef LOCTEXT_NAMESPACE
