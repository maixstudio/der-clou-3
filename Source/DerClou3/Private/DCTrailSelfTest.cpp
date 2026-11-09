// -DCSelfTest=Trail on L_Villa01: the plan on the floor as footprints in the crew colours, signs with the step
// number and an icon of the activity, clicking a sign, and the camera following the selected crew member.

#include "DCCameraPawn.h"
#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCHeistState.h"
#include "DCPlan.h"
#include "DCPlanTrail.h"
#include "DCPlayerController.h"
#include "DCWorldBadges.h"
#include "DerClou3.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "UnrealClient.h"

namespace
{
	/** Plan metres (x east, y north) to Unreal cm, as in build_villa.py. */
	FVector TrailTestPoint(double X, double Y, double Z)
	{
		return FVector(Y * 100.0, X * 100.0, Z);
	}
}

void ADCPlayerController::StartTrailSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Trail started"));
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Wait = [](float Seconds)
	{
		return [Seconds, End = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			End = End < 0.0 ? Now + Seconds : End;
			return Now >= End;
		};
	};
	const auto Shot = [](const TCHAR* Name)
	{
		return [Name]()
		{
			const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name);
			FScreenshotRequest::RequestScreenshot(File, true, false);
			UE_LOG(LogDerClou, Display, TEXT("Self-test: screenshot %s"), *File);
			return true;
		};
	};
	const auto CrewNamed = [this](const TCHAR* Name) -> ADCCrewCharacter*
	{
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetCrewName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto DoorNamed = [this](const TCHAR* Name) -> ADCDoor*
	{
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetInteractableName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto Zoom = [this](float Distance)
	{
		if (ADCCameraPawn* Pawn = Cast<ADCCameraPawn>(GetPawn()))
		{
			if (USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>())
			{
				Arm->TargetArmLength = Distance;
			}
		}
	};
	const auto CountFootprints = [this]()
	{
		int32 Count = 0;
		for (TActorIterator<ADCPlanTrail> It(GetWorld()); It; ++It)
		{
			Count += It->GetFootprintCount();
		}
		return Count;
	};

	// The nav mesh of the whole plot is built at start; wait for it.
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const bool bBuilding = !Nav || Nav->IsNavigationBuildInProgress() || Nav->GetNumRemainingBuildTasks() > 0;
		return Now - Started > 3.0 && (!bBuilding || Now - Started > 240.0);
	});

	// Vic picks the garden gate, walks into the garden and picks the front door; Rosa waits for his first step and follows.
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed, Zoom, CountFootprints]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCDoor* Gate = DoorNamed(TEXT("Gartenpforte"));
		ADCDoor* Front = DoorNamed(TEXT("Haustür"));
		if (!Plans || !Heist || !Vic || !Rosa || !Gate || !Front)
		{
			Expect(false, TEXT("Vic, Rosa, garden gate and front door are there"));
			return true;
		}
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		const auto Pick = [](const ADCDoor* Door)
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Door;
			Task.Target = Door->GetFName();
			Task.Action = static_cast<uint8>(EDCDoorAction::PickLock);
			return Task;
		};
		FDCPlanTask Garden;
		Garden.Type = EDCPlanTaskType::Move;
		Garden.Location = TrailTestPoint(32.5, 12.0, 0.0);
		SelectFromBar(Vic);
		IssueTask(Pick(Gate));
		IssueTask(Garden);
		IssueTask(Pick(Front));
		SelectFromBar(Rosa);
		FDCWaitChoice Choice;
		Choice.Kind = FDCWaitChoice::EKind::AfterStep;
		Choice.Crew = Vic->GetFName();
		Choice.Step = 0;
		ApplyWaitChoice(Choice);
		FDCPlanTask Follow;
		Follow.Type = EDCPlanTaskType::Move;
		Follow.Location = TrailTestPoint(29.5, 11.0, 0.0);
		IssueTask(Follow);
		SelectFromBar(Vic);
		SetPreviewTime(-1.f);
		Heist->Refresh();
		Zoom(2200.f);

		const int32 Footprints = CountFootprints();
		Expect(Footprints > 30, FString::Printf(TEXT("footprints along both ways (%d)"), Footprints));
		const FDCWorldBadge* VicPick = nullptr;
		bool bRosaQuiet = true;
		bool bRosaThere = false;
		for (const FDCWorldBadge& Badge : Heist->GetStepBadges())
		{
			for (const FDCBadgeEntry& Entry : Badge.Entries)
			{
				UE_LOG(LogDerClou, Display, TEXT("Self-test: sign %s %d %s \"%s\"%s%s"), *Badge.Crew.ToString(), Entry.Number, *Entry.Icon.ToString(), *Entry.Verb.ToString(),
					Badge.bFocus ? TEXT(" focus") : TEXT(""), Entry.bProblem ? TEXT(" PROBLEM") : TEXT(""));
				if (Badge.Crew == Vic->GetFName() && Entry.Number == 1 && Entry.Icon == TEXT("lock-pick") && Entry.Verb.ToString() == TEXT("knacken"))
				{
					VicPick = &Badge;
				}
			}
			if (Badge.Crew == Rosa->GetFName())
			{
				bRosaThere = true;
				bRosaQuiet &= !Badge.bFocus;
			}
		}
		Expect(VicPick && VicPick->bFocus && FVector::Dist2D(VicPick->Anchor, Gate->GetCenter()) < 200.0,
			TEXT("Vic's step 1 has a sign at the garden gate: 1, lock with a pick, „knacken“, in full"));
		Expect(bRosaThere && bRosaQuiet, TEXT("Rosa's signs are there, but quiet while Vic is selected"));
		bool bGateSign = false;
		bool bFrontSign = false;
		for (const FDCWorldBadge& Badge : Heist->GetDoorBadges())
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: door sign \"%s\" (%s)"), *Badge.Text.ToString(), *Badge.Icon.ToString());
			bGateSign |= Badge.Text.ToString().Contains(TEXT("Vic")) && FVector::Dist2D(Badge.Anchor, Gate->GetCenter()) < 50.0;
			bFrontSign |= Badge.Text.ToString().Contains(TEXT("aufgesperrt")) && FVector::Dist2D(Badge.Anchor, Front->GetCenter()) < 50.0;
		}
		Expect(bGateSign, TEXT("the garden gate has a sign by Vic (he picks it and walks through: „offen … · Vic“)"));
		Expect(bFrontSign, TEXT("the front door has a sign „aufgesperrt … · Vic“"));
		return true;
	});
	SelfTestSteps.Add(Wait(2.5f));
	SelfTestSteps.Add(Shot(TEXT("trail_vic.png")));

	// A click on one of Rosa's signs picks her step - and the camera goes over to her.
	SelfTestSteps.Add([this, Expect, CrewNamed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		if (!Rosa || !WorldBadges)
		{
			Expect(false, TEXT("Rosa and the sign layer are there"));
			return true;
		}
		const UDCWorldBadges::FDrawn* Sign = WorldBadges->Drawn.FindByPredicate([Rosa](const UDCWorldBadges::FDrawn& Entry) { return Entry.Crew == Rosa->GetFName(); });
		Expect(Sign != nullptr, FString::Printf(TEXT("one of Rosa's signs is drawn (%d signs drawn)"), WorldBadges->Drawn.Num()));
		if (Sign)
		{
			const FVector2D Center = Sign->Rect.GetCenter();
			const FVector2D Pixel = Center * WorldBadges->DrawnScale;
			HandleClickAt(Pixel);
			Expect(Selected == Rosa && Heist->GetFocusCrew() == Rosa->GetFName() && Heist->GetFocusStep() == Sign->Step,
				FString::Printf(TEXT("clicking her sign picks Rosa's step %d"), Sign->Step + 1));
		}
		const ADCCameraPawn* Camera = Cast<ADCCameraPawn>(GetPawn());
		Expect(Camera && Camera->IsFollowing(), TEXT("the camera follows Rosa"));
		return true;
	});
	SelfTestSteps.Add(Wait(2.f));
	SelfTestSteps.Add(Shot(TEXT("trail_rosa.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCCameraPawn* Camera = Cast<ADCCameraPawn>(GetPawn());
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		FVector Spot;
		if (!Rosa || !Camera || !Heist->GetPlanSpot(Rosa->GetFName(), Spot))
		{
			Expect(false, TEXT("camera and Rosa's plan spot"));
			return true;
		}
		const double Away = FVector::Dist2D(Camera->GetActorLocation(), Spot);
		Expect(Away < 400.0, FString::Printf(TEXT("the camera looks at where Rosa's plan has got to (%.0f cm off)"), Away));
		// Sent elsewhere, the camera stops following; F (ResumeCameraFollow) brings it back.
		Camera->FocusOn(Spot + FVector(2500.0, 0.0, 0.0));
		Expect(!Camera->IsFollowing(), TEXT("panned away, the camera is free („Kamera frei · F folgt“)"));
		ResumeCameraFollow();
		Expect(Camera->IsFollowing(), TEXT("F: it follows again"));
		return true;
	});
	SelfTestSteps.Add(Wait(2.f));
	SelfTestSteps.Add([this, Expect, CrewNamed]()
	{
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		const ADCCameraPawn* Camera = Cast<ADCCameraPawn>(GetPawn());
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		FVector Spot;
		if (!Rosa || !Vic || !Camera || !Heist->GetPlanSpot(Rosa->GetFName(), Spot))
		{
			return true;
		}
		const double Away = FVector::Dist2D(Camera->GetActorLocation(), Spot);
		Expect(Away < 400.0, FString::Printf(TEXT("... and is back at Rosa (%.0f cm off)"), Away));
		// Without Vic's pick Rosa gets stuck at the gate: her sign there is red.
		Plans->RemoveTask(Vic->GetFName(), 0);
		Heist->Refresh();
		bool bRed = false;
		for (const FDCWorldBadge& Badge : Heist->GetStepBadges())
		{
			bRed |= Badge.Crew == Rosa->GetFName() && Badge.bProblem;
		}
		Expect(bRed, TEXT("without Vic's pick, Rosa's sign is red where she gets stuck"));
		return true;
	});
	SelfTestSteps.Add(Wait(1.5f));
	SelfTestSteps.Add(Shot(TEXT("trail_problem.png")));
	SelfTestSteps.Add([this, Expect]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Plans->Undo();
		Heist->Refresh();
		// The time preview: where everyone is then, as signs with what they do.
		SetPreviewTime(FMath::Max(1.f, Heist->GetSchedule().Duration * 0.5f));
		Expect(Heist->GetMomentBadges().Num() == 2, FString::Printf(TEXT("the preview shows where both are (%d signs)"), Heist->GetMomentBadges().Num()));
		for (const FDCWorldBadge& Badge : Heist->GetMomentBadges())
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: moment \"%s\""), *Badge.Text.ToString());
		}
		return true;
	});
	SelfTestSteps.Add(Wait(1.5f));
	SelfTestSteps.Add(Shot(TEXT("trail_preview.png")));
	SelfTestSteps.Add([this, bPassed]()
	{
		FinishSelfTest(*bPassed);
		return true;
	});
}
