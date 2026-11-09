// -DCSelfTest=Replay [-DCSave=auto_villa]: plays a saved plan (default: the villa autosave) on the map the game
// was started with and writes down how it went, slow frames included. For replaying what a user played.

#include "DCCrewCharacter.h"
#include "DCGameFlow.h"
#include "DCHeistState.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
#include "DerClou3.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ADCPlayerController::StartReplaySelfTest()
{
	FString Slot = TEXT("auto_villa");
	FParse::Value(FCommandLine::Get(), TEXT("DCSave="), Slot);
	UE_LOG(LogDerClou, Display, TEXT("Self-test Replay started (save %s)"), *Slot);
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	struct FReplayState
	{
		double WorstFrame = 0.0;
		int32 SlowFrames = 0;
		bool bLoaded = false;
	};
	const TSharedRef<FReplayState> State = MakeShared<FReplayState>();

	// The ways first (the villa builds its nav mesh at start).
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		return (Now - Started > 2.0 && UDCHeistStateSubsystem::IsNavigationReady(this)) || Now - Started > 60.0;
	});
	SelfTestSteps.Add([this, Slot, State]()
	{
		const UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		if (!Save || !Plans || !Heist)
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: FAIL save %s could not be loaded"), *Slot);
			FinishSelfTest(false);
			return true;
		}
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->SetPlans(Save->Plans);
		Plans->SetSpeed(Save->Speed);
		const double Before = FPlatformTime::Seconds();
		Heist->Refresh();
		UE_LOG(LogDerClou, Display, TEXT("Self-test: plan simulated in %.0f ms, planned duration %.1f s, speed %.1f"),
			(FPlatformTime::Seconds() - Before) * 1000.0, Heist->GetSchedule().Duration, Save->Speed);
		for (const FDCCrewSchedule& Crew : Heist->GetSchedule().Crews)
		{
			for (const FDCScheduledTask& Task : Crew.Tasks)
			{
				UE_LOG(LogDerClou, Display, TEXT("Self-test: plan %s %d. %s %.1f-%.1f%s %s"), *Crew.Crew.ToString(), Task.Index + 1, *Task.Label.ToString(),
					Task.Start, Task.End, Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.WaitReason.ToString());
			}
		}
		State->bLoaded = true;
		return true;
	});
	SelfTestSteps.Add([End = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		End = End < 0.0 ? Now + 1.0 : End;
		return Now >= End;
	});
	SelfTestSteps.Add([this]()
	{
		StartPlayback();
		return true;
	});
	SelfTestSteps.Add([this, State, Started = -1.0, LastFrame = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		if (LastFrame > 0.0)
		{
			const double Frame = Now - LastFrame;
			State->WorstFrame = FMath::Max(State->WorstFrame, Frame);
			if (Frame > 0.5)
			{
				++State->SlowFrames;
				UE_LOG(LogDerClou, Display, TEXT("Self-test: slow frame %.2f s (%.1f s into playback)"), Frame, Now - Started);
			}
		}
		LastFrame = Now;
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 300.0;
	});
	SelfTestSteps.Add([this, State]()
	{
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		const bool bFinished = Heist && Heist->IsPlaybackFinished();
		int32 Done = 0;
		int32 Steps = 0;
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It && Plans; ++It)
		{
			const int32 Count = Plans->GetTasks(It->GetFName()).Num();
			FString Line;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const EDCTaskStatus Status = It->GetPlanRunner()->GetStatus(Index);
				Done += Status == EDCTaskStatus::Done ? 1 : 0;
				Line += Status == EDCTaskStatus::Done ? TEXT("+") : Status == EDCTaskStatus::Failed ? TEXT("x") : TEXT(".");
			}
			Steps += Count;
			UE_LOG(LogDerClou, Display, TEXT("Self-test: %s steps [%s]"), *It->GetCrewName().ToString(), *Line);
		}
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback %s after %.1f s (planned %.1f s), %d/%d steps done, worst frame %.2f s, %d slow frames"),
			bFinished ? TEXT("finished") : TEXT("NOT finished"), Heist ? Heist->GetPlaybackTime() : 0.f, Heist ? Heist->GetSchedule().Duration : 0.f,
			Done, Steps, State->WorstFrame, State->SlowFrames);
		FinishSelfTest(State->bLoaded && bFinished && State->WorstFrame < 1.0);
		return true;
	});
}
