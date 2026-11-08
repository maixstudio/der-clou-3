#include "DCPlan.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "DCPlan"

namespace
{
	const TCHAR* const PlanVariantSection = TEXT("DerClou.Variants");
}

void UDCPlanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Ignored;
	const bool bSelfTest = FParse::Value(FCommandLine::Get(), TEXT("DCSelfTest="), Ignored);
	if (GConfig && !bSelfTest)
	{
		GConfig->GetBool(PlanVariantSection, TEXT("GhostAsPin"), bGhostAsPin, GGameUserSettingsIni);
		GConfig->GetBool(PlanVariantSection, TEXT("HelpAsMenu"), bHelpAsMenu, GGameUserSettingsIni);
		GConfig->GetBool(PlanVariantSection, TEXT("InsertOnlyWithButton"), bInsertOnlyWithButton, GGameUserSettingsIni);
	}
	FString Variant;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCVariant="), Variant))
	{
		bGhostAsPin = Variant.Contains(TEXT("GhostPin"));
		bHelpAsMenu = Variant.Contains(TEXT("HelpMenu"));
		bInsertOnlyWithButton = Variant.Contains(TEXT("InsertButton"));
	}
}

void UDCPlanSubsystem::SaveVariants() const
{
	// Self-tests never touch the player's settings.
	FString Ignored;
	if (GConfig && !FParse::Value(FCommandLine::Get(), TEXT("DCSelfTest="), Ignored))
	{
		GConfig->SetBool(PlanVariantSection, TEXT("GhostAsPin"), bGhostAsPin, GGameUserSettingsIni);
		GConfig->SetBool(PlanVariantSection, TEXT("HelpAsMenu"), bHelpAsMenu, GGameUserSettingsIni);
		GConfig->SetBool(PlanVariantSection, TEXT("InsertOnlyWithButton"), bInsertOnlyWithButton, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

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
	FDCPlanEditScope Edit(this, LOCTEXT("EditAdd", "Schritt angelegt"));
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
	MarkChanged();
	return Added.Id;
}

TArray<FGuid> UDCPlanSubsystem::InsertTasks(FName Crew, int32 Index, const TArray<FDCPlanTask>& Tasks)
{
	FDCPlanEditScope Edit(this, LOCTEXT("EditAddSeveral", "Schritte angelegt"));
	TArray<FGuid> Ids;
	int32 At = Index;
	for (const FDCPlanTask& Task : Tasks)
	{
		At = FMath::Clamp(At, 0, GetTasks(Crew).Num());
		Ids.Add(InsertTask(Crew, At, Task));
		++At;
	}
	return Ids;
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
	FDCPlanEditScope Edit(this, LOCTEXT("EditRemove", "Schritt gelöscht"));
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	if (Plan && !Plan->Tasks.IsEmpty())
	{
		Plan->Tasks.Pop();
		MarkChanged();
	}
}

int32 UDCPlanSubsystem::RemoveTask(FName Crew, int32 Index)
{
	FDCPlanEditScope Edit(this, LOCTEXT("EditRemove", "Schritt gelöscht"));
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	if (!Plan || !Plan->Tasks.IsValidIndex(Index))
	{
		return 0;
	}
	int32 Followers = 0;
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
	if (Removed.Type != EDCPlanTaskType::WaitFor)
	{
		// Nobody can wait for a step (or a signal) that is not there.
		for (FDCCrewPlan& Entry : Plans)
		{
			Followers += Entry.Tasks.RemoveAll([&Removed](const FDCPlanTask& Task) { return Task.Type == EDCPlanTaskType::WaitFor && Task.Ref == Removed.Id; });
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
	MarkChanged();
	return Followers;
}

void UDCPlanSubsystem::MoveTask(FName Crew, int32 Index, int32 Delta)
{
	const TArray<FDCPlanTask>& Tasks = GetTasks(Crew);
	if (Delta != 0 && Tasks.IsValidIndex(Index) && Tasks.IsValidIndex(Index + Delta))
	{
		MoveTaskTo(Crew, Index, Index + Delta);
	}
}

void UDCPlanSubsystem::MoveTaskTo(FName Crew, int32 From, int32 To)
{
	FDCPlanEditScope Edit(this, LOCTEXT("EditMove", "Schritt verschoben"));
	FDCCrewPlan* Plan = Plans.FindByPredicate([Crew](const FDCCrewPlan& Entry) { return Entry.Crew == Crew; });
	if (!Plan || !Plan->Tasks.IsValidIndex(From))
	{
		return;
	}
	To = FMath::Clamp(To, 0, Plan->Tasks.Num() - 1);
	if (To == From)
	{
		return;
	}
	const FDCPlanTask Task = Plan->Tasks[From];
	Plan->Tasks.RemoveAt(From);
	Plan->Tasks.Insert(Task, To);
	MarkChanged();
}

void UDCPlanSubsystem::ClearAll()
{
	FDCPlanEditScope Edit(this, LOCTEXT("EditClear", "Plan geleert"));
	if (Plans.ContainsByPredicate([](const FDCCrewPlan& Plan) { return !Plan.Tasks.IsEmpty(); }))
	{
		MarkChanged();
	}
	Plans.Reset();
}

void UDCPlanSubsystem::BeginEdit(const FText& What)
{
	if (EditDepth++ == 0)
	{
		OpenEdit.Plans = Plans;
		OpenEdit.What = What;
		bEditChanged = false;
	}
}

void UDCPlanSubsystem::EndEdit()
{
	if (EditDepth <= 0 || --EditDepth > 0 || !bEditChanged)
	{
		return;
	}
	bEditChanged = false;
	UndoStack.Add(MoveTemp(OpenEdit));
	OpenEdit = FDCPlanHistoryEntry();
	if (UndoStack.Num() > MaxHistory)
	{
		UndoStack.RemoveAt(0);
	}
	RedoStack.Reset();
	OnPlanChanged.Broadcast();
}

bool UDCPlanSubsystem::Undo()
{
	if (UndoStack.IsEmpty() || EditDepth > 0)
	{
		return false;
	}
	FDCPlanHistoryEntry Entry = UndoStack.Pop();
	RedoStack.Add({ Plans, Entry.What });
	Plans = MoveTemp(Entry.Plans);
	OnPlanChanged.Broadcast();
	return true;
}

bool UDCPlanSubsystem::Redo()
{
	if (RedoStack.IsEmpty() || EditDepth > 0)
	{
		return false;
	}
	FDCPlanHistoryEntry Entry = RedoStack.Pop();
	UndoStack.Add({ Plans, Entry.What });
	Plans = MoveTemp(Entry.Plans);
	OnPlanChanged.Broadcast();
	return true;
}

#undef LOCTEXT_NAMESPACE
