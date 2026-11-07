#include "DCPlan.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDCPlanSubsystem* UDCPlanSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDCPlanSubsystem>() : nullptr;
}

void UDCPlanSubsystem::SetMode(EDCPlanMode InMode)
{
	if (Mode != InMode)
	{
		Mode = InMode;
		OnPlanChanged.Broadcast();
	}
}

const TArray<FDCPlanTask>& UDCPlanSubsystem::GetTasks(FName Crew) const
{
	static const TArray<FDCPlanTask> None;
	const FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	return Plan ? Plan->Tasks : None;
}

FGuid UDCPlanSubsystem::AddTask(FName Crew, const FDCPlanTask& Task)
{
	return InsertTask(Crew, TNumericLimits<int32>::Max(), Task);
}

FGuid UDCPlanSubsystem::InsertTask(FName Crew, int32 Index, const FDCPlanTask& Task)
{
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	if (!Plan)
	{
		Plan = &Plans.AddDefaulted_GetRef();
		Plan->Crew = Crew;
	}
	FDCPlanTask Added = Task;
	if (!Added.Id.IsValid())
	{
		Added.Id = FGuid::NewGuid();
	}
	Plan->Tasks.Insert(Added, FMath::Clamp(Index, 0, Plan->Tasks.Num()));
	OnPlanChanged.Broadcast();
	return Added.Id;
}

const FDCPlanTask* UDCPlanSubsystem::FindTask(const FGuid& Id, FName* OutCrew, int32* OutIndex) const
{
	if (!Id.IsValid())
	{
		return nullptr;
	}
	for (const FDCCrewPlan& Plan : Plans)
	{
		for (int32 Index = 0; Index < Plan.Tasks.Num(); ++Index)
		{
			if (Plan.Tasks[Index].Id == Id)
			{
				if (OutCrew)
				{
					*OutCrew = Plan.Crew;
				}
				if (OutIndex)
				{
					*OutIndex = Index;
				}
				return &Plan.Tasks[Index];
			}
		}
	}
	return nullptr;
}

int32 UDCPlanSubsystem::GetNextSignalNumber() const
{
	int32 Highest = 0;
	for (const FDCCrewPlan& Plan : Plans)
	{
		for (const FDCPlanTask& Task : Plan.Tasks)
		{
			if (Task.Type == EDCPlanTaskType::Signal)
			{
				Highest = FMath::Max(Highest, static_cast<int32>(Task.Action));
			}
		}
	}
	return Highest + 1;
}

void UDCPlanSubsystem::RemoveLastTask(FName Crew)
{
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	if (Plan && !Plan->Tasks.IsEmpty())
	{
		Plan->Tasks.Pop();
		OnPlanChanged.Broadcast();
	}
}

void UDCPlanSubsystem::RemoveTask(FName Crew, int32 Index)
{
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	if (!Plan || !Plan->Tasks.IsValidIndex(Index))
	{
		return;
	}
	const FDCPlanTask Removed = Plan->Tasks[Index];
	Plan->Tasks.RemoveAt(Index);
	const auto WaitsFor = [this](const FGuid& Id)
	{
		int32 Count = 0;
		for (const FDCCrewPlan& Entry : Plans)
		{
			Count += Entry.Tasks.FilterByPredicate([&Id](const FDCPlanTask& Task) { return Task.Type == EDCPlanTaskType::WaitFor && Task.Ref == Id; }).Num();
		}
		return Count;
	};
	if (Removed.Type == EDCPlanTaskType::Signal)
	{
		// Nobody can wait for a signal that is not given.
		for (FDCCrewPlan& Entry : Plans)
		{
			Entry.Tasks.RemoveAll([&Removed](const FDCPlanTask& Task) { return Task.Type == EDCPlanTaskType::WaitFor && Task.Ref == Removed.Id; });
		}
	}
	else if (Removed.Type == EDCPlanTaskType::WaitFor && Removed.Ref.IsValid() && WaitsFor(Removed.Ref) == 0)
	{
		// A signal nobody waits for any more goes too.
		for (FDCCrewPlan& Entry : Plans)
		{
			Entry.Tasks.RemoveAll([&Removed](const FDCPlanTask& Task) { return Task.Type == EDCPlanTaskType::Signal && Task.Id == Removed.Ref; });
		}
	}
	OnPlanChanged.Broadcast();
}

void UDCPlanSubsystem::MoveTask(FName Crew, int32 Index, int32 Delta)
{
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	const int32 Target = Index + Delta;
	if (Plan && Plan->Tasks.IsValidIndex(Index) && Plan->Tasks.IsValidIndex(Target) && Delta != 0)
	{
		const FDCPlanTask Task = Plan->Tasks[Index];
		Plan->Tasks.RemoveAt(Index);
		Plan->Tasks.Insert(Task, Target);
		OnPlanChanged.Broadcast();
	}
}

void UDCPlanSubsystem::ClearAll()
{
	Plans.Reset();
	OnPlanChanged.Broadcast();
}
