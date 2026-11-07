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

void UDCPlanSubsystem::AddTask(FName Crew, const FDCPlanTask& Task)
{
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	if (!Plan)
	{
		Plan = &Plans.AddDefaulted_GetRef();
		Plan->Crew = Crew;
	}
	Plan->Tasks.Add(Task);
	OnPlanChanged.Broadcast();
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

void UDCPlanSubsystem::ClearAll()
{
	Plans.Reset();
	OnPlanChanged.Broadcast();
}
