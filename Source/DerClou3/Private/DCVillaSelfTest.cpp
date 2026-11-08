// -DCSelfTest=Villa on L_Villa01 (Scripts/Levels/build_villa.py): the villa's rooms, doors and stairs,
// a planned way from the street into the cellar, and the camera showing one storey at a time.

#include "DCCameraPawn.h"
#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCFloors.h"
#include "DCHeistState.h"
#include "DCItemPickup.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
#include "DCRoomVolume.h"
#include "DerClou3.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UnrealClient.h"

namespace
{
	/** Plan metres (x east, y north) to Unreal cm, as in build_villa.py. */
	FVector VillaPoint(double X, double Y, double Z)
	{
		return FVector(Y * 100.0, X * 100.0, Z);
	}
}

void ADCPlayerController::StartVillaSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test Villa started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const int32 Stage = UDCPlanSubsystem::Get(this)->SelfTestStage;
	const auto Wait = [](float Seconds)
	{
		return [Seconds, End = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			if (End < 0.0)
			{
				End = Now + Seconds;
			}
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
	const auto RoomNamed = [this](const TCHAR* Name) -> ADCRoomVolume*
	{
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			if (It->GetRoomName().ToString() == Name)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto RoomOf = [this](const FVector& Location)
	{
		const ADCRoomVolume* Room = ADCRoomVolume::FindAt(this, Location);
		return Room ? Room->GetRoomName().ToString() : FString(TEXT("-"));
	};
	const auto FirstOnFloor = [this](const TCHAR* Tag) -> AActor*
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(Tag) && It->IsA<AStaticMeshActor>())
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto Camera = [this](const FVector& Look, float Distance)
	{
		if (ADCCameraPawn* Pawn = Cast<ADCCameraPawn>(GetPawn()))
		{
			Pawn->FocusOn(Look);
			if (USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>())
			{
				Arm->TargetArmLength = Distance;
			}
		}
	};

	if (Stage == 1)
	{
		// Freshly loaded: "Abspielen" right away must wait for the ways instead of leaving everyone standing.
		SelfTestSteps.Add([this, Expect, CrewNamed]()
		{
			UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
			ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
			Expect(!UDCHeistStateSubsystem::IsNavigationReady(this), TEXT("right after loading the ways are still being built"));
			Plans->SetMode(EDCPlanMode::Planning);
			Plans->ClearAll();
			if (Vic)
			{
				FDCPlanTask Go;
				Go.Type = EDCPlanTaskType::Move;
				Go.Location = VillaPoint(6.5, -1.6, 0.0);  // west, away from Rosa (she stands east of him)
				Plans->AddTask(Vic->GetFName(), Go);
			}
			StartPlayback();
			return true;
		});
		SelfTestSteps.Add([this, Started = -1.0]() mutable
		{
			const double Now = FPlatformTime::Seconds();
			Started = Started < 0.0 ? Now : Started;
			const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
			return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 60.0;
		});
		SelfTestSteps.Add([this, Expect, CrewNamed]()
		{
			ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
			const double Walked = Vic ? FVector::Dist2D(Vic->GetActorLocation(), VillaPoint(13.6, -1.1, 0.0)) : 0.0;
			Expect(Vic && Vic->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Done && Walked > 500.0,
				FString::Printf(TEXT("playing right after loading: Vic still walks (%.0f cm)"), Walked));
			return true;
		});
		SelfTestSteps.Add([this, bPassed]()
		{
			UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
			Plans->SelfTestStage = 0;
			FinishSelfTest(Plans->bSelfTestPassed && *bPassed);
			return true;
		});
		return;
	}

	// The nav mesh of the whole plot is built at start; wait for it.
	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const int32 Remaining = Nav ? Nav->GetNumRemainingBuildTasks() : 0;
		const bool bBuilding = !Nav || Nav->IsNavigationBuildInProgress() || Remaining > 0;
		if (Now - Started > 3.0 && (!bBuilding || Now - Started > 240.0))
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: nav mesh ready after %.1f s%s"), Now - Started, bBuilding ? TEXT(" (still building)") : TEXT(""));
			return true;
		}
		static double LastLog = 0.0;
		if (Now - LastLog > 2.0)
		{
			LastLog = Now;
			UE_LOG(LogDerClou, Display, TEXT("Self-test: nav mesh building, %d tasks left (%.0f s)"), Remaining, Now - Started);
		}
		return false;
	});

	// What build_villa.py put into the level.
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed, RoomNamed, RoomOf]()
	{
		int32 Rooms = 0;
		for (TActorIterator<ADCRoomVolume> It(GetWorld()); It; ++It)
		{
			++Rooms;
		}
		Expect(Rooms == 29, FString::Printf(TEXT("29 rooms (house, cellar, Remise, garden, street) - %d"), Rooms));
		int32 Doors = 0;
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			++Doors;
		}
		Expect(Doors == 34, FString::Printf(TEXT("34 doors and gates - %d"), Doors));
		for (const TCHAR* Name : { TEXT("Haustür"), TEXT("Kellertür"), TEXT("Tresorraum"), TEXT("Kelleraußentür"), TEXT("Gartenpforte"),
			TEXT("Garagentor"), TEXT("Geheimtür"), TEXT("Hebeschiebetür Wohnzimmer") })
		{
			Expect(DoorNamed(Name) != nullptr, FString::Printf(TEXT("door \"%s\""), Name));
		}
		for (const TCHAR* Name : { TEXT("Vic"), TEXT("Rosa") })
		{
			const ADCCrewCharacter* Member = CrewNamed(Name);
			Expect(Member && RoomOf(Member->GetActorLocation()) == TEXT("Straße"), FString::Printf(TEXT("%s starts on the street"), Name));
		}
		const ADCItemPickup* Safe = FindPickup(TEXT("safe"));
		Expect(Safe && RoomOf(Safe->GetActorLocation()) == TEXT("Tresorraum"), TEXT("the safe stands in the vault (cellar, not the library above it)"));
		const ADCItemPickup* Crowbar = FindPickup(TEXT("crowbar"));
		Expect(Crowbar && RoomOf(Crowbar->GetActorLocation()) == TEXT("Werkstatt"), TEXT("the crowbar lies in the workshop"));
		Expect(RoomOf(VillaPoint(23.4, 25.2, 1.0)) == TEXT("Speisekammer"), TEXT("the pantry inside the kitchen counts as its own room"));
		Expect(RoomOf(VillaPoint(40.0, 18.5, 1.0)) == TEXT("Bibliothek") && RoomOf(VillaPoint(40.0, 18.5, -289.0)) == TEXT("Tresorraum"),
			TEXT("the library and the vault below it are told apart by height"));

		// Ways that need no door: down the cellar stair, and down the outside stair to the cellar door.
		UWorld* World = GetWorld();
		// Complete, and really ending at the goal's height (a cellar goal must not snap to the floor above).
		const auto Reachable = [World](const FVector& From, const FVector& To)
		{
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To);
			return Path && Path->IsValid() && !Path->IsPartial() && !Path->PathPoints.IsEmpty()
				&& FMath::Abs(Path->PathPoints.Last().Z - To.Z) < 60.0;
		};
		Expect(Reachable(VillaPoint(29.9, 16.75, 0.0), VillaPoint(29.9, 23.0, -290.0)), TEXT("the cellar stair can be walked (landing -> cellar corridor)"));
		Expect(Reachable(VillaPoint(48.0, 23.9, 0.0), VillaPoint(44.9, 29.2, -290.0)), TEXT("the outside stair can be walked (garden path -> cellar door)"));
		Expect(!Reachable(VillaPoint(13.6, -1.1, 0.0), VillaPoint(25.0, 6.0, 0.0)) && !Reachable(VillaPoint(32.5, -0.7, 0.0), VillaPoint(32.5, 15.3, 0.0)),
			TEXT("the fence keeps the street out while the gates are shut"));
		const UDCFloorSubsystem* Floors = UDCFloorSubsystem::Get(this);
		Expect(Floors && Floors->HasSeveralFloors() && Floors->GetShownFloor() == 0, TEXT("two storeys, the ground floor is shown"));
		return true;
	});

	// User case: Vic picks the garden gate, Rosa waits until he is done and then walks into the garden.
	// Her way has to go through the gate, not straight through the fence.
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCCrewCharacter* Rosa = CrewNamed(TEXT("Rosa"));
		ADCDoor* Gate = DoorNamed(TEXT("Gartenpforte"));
		if (!Plans || !Heist || !Vic || !Rosa || !Gate)
		{
			Expect(false, TEXT("pieces for Rosa's way found"));
			return true;
		}
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		// A fresh start: switching to someone without a plan shows the very beginning.
		Select(Vic);
		SelectFromBar(Rosa);
		Expect(Heist->GetPreviewTime() == 0.f, FString::Printf(TEXT("Rosa without a plan: the time cursor jumps to the start (%.1f)"), Heist->GetPreviewTime()));
		SelectFromBar(Vic);
		FDCPlanTask Pick;
		Pick.Type = EDCPlanTaskType::Door;
		Pick.Target = Gate->GetFName();
		Pick.Action = static_cast<uint8>(EDCDoorAction::PickLock);
		IssueTask(Pick);
		SelectFromBar(Rosa);
		FDCWaitChoice Choice;
		Choice.Kind = FDCWaitChoice::EKind::AfterStep;
		Choice.Crew = Vic->GetFName();
		Choice.Step = 0;
		ApplyWaitChoice(Choice);
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = VillaPoint(32.5, 12.0, 0.0);
		IssueTask(Go);
		Heist->Refresh();
		const FDCCrewSchedule* RosaPlan = Heist->GetSchedule().Find(Rosa->GetFName());
		const FDCScheduledTask* Walk = RosaPlan && !RosaPlan->Tasks.IsEmpty() ? &RosaPlan->Tasks.Last() : nullptr;
		double ClosestToGate = TNumericLimits<double>::Max();
		for (const FVector& Point : Walk ? Walk->Path : TArray<FVector>())
		{
			ClosestToGate = FMath::Min(ClosestToGate, FVector::Dist2D(Point, Gate->GetCenter()));
		}
		UE_LOG(LogDerClou, Display, TEXT("Self-test: Rosa's walk: %d points, %.0f cm from the gate, %.1f-%.1f s%s"), Walk ? Walk->Path.Num() : 0,
			ClosestToGate, Walk ? Walk->Start : 0.f, Walk ? Walk->End : 0.f, Walk && Walk->bBlocked ? TEXT(" BLOCKED") : TEXT(""));
		Expect(Walk && !Walk->bBlocked && Walk->Path.Num() > 2 && ClosestToGate < 150.0,
			TEXT("Rosa's way into the garden goes through the garden gate (not straight through the fence)"));
		// The time cursor followed Rosa's new step; switching to Vic jumps to the end of his plan.
		Expect(Walk && FMath::IsNearlyEqual(Heist->GetPreviewTime(), Walk->End, 0.2f),
			FString::Printf(TEXT("the time cursor followed Rosa's new step (%.1f s)"), Heist->GetPreviewTime()));
		SelectFromBar(Vic);
		const FDCCrewSchedule* VicPlan = Heist->GetSchedule().Find(Vic->GetFName());
		const float VicEnd = VicPlan && !VicPlan->Tasks.IsEmpty() ? VicPlan->Tasks.Last().End : -1.f;
		Expect(FMath::IsNearlyEqual(Heist->GetPreviewTime(), VicEnd, 0.2f),
			FString::Printf(TEXT("switching to Vic jumps to the end of his plan (%.1f s, his end %.1f s)"), Heist->GetPreviewTime(), VicEnd));
		Expect(DescribeDeleteStep().ToString() == TEXT("Letzten Schritt l\u00f6schen"), FString::Printf(TEXT("delete button: \"%s\""), *DescribeDeleteStep().ToString()));
		const int32 VicSteps = Plans->GetTasks(Vic->GetFName()).Num();
		DeleteStep();
		Expect(Plans->GetTasks(Vic->GetFName()).Num() == VicSteps - 1, TEXT("\"Letzten Schritt l\u00f6schen\" takes Vic's last step away"));

		// Doors count on their own storey only: from the living room into the library, not through the vault door below.
		FString Names;
		for (const ADCDoor* Door : Heist->FindDoorsOnWay(VillaPoint(40.0, 26.0, 0.0), VillaPoint(40.0, 18.5, 0.0)))
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Door->GetInteractableName().ToString();
		}
		Expect(Names == TEXT("Wohnzimmer \u2192 Bibliothek"), FString::Printf(TEXT("doors from the living room to the library: %s"), *Names));
		Plans->ClearAll();
		SetPreviewTime(-1.f);
		return true;
	});

	// Plan: Vic picks the garden gate, the front door and the cellar door, then walks into the cellar corridor.
	SelfTestSteps.Add([this, Expect, CrewNamed, DoorNamed, RoomNamed, Camera]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		ADCDoor* Gate = DoorNamed(TEXT("Gartenpforte"));
		ADCDoor* Front = DoorNamed(TEXT("Haustür"));
		ADCDoor* Cellar = DoorNamed(TEXT("Kellertür"));
		ADCRoomVolume* Corridor = RoomNamed(TEXT("Kellerflur"));
		if (!Plans || !Heist || !Vic || !Gate || !Front || !Cellar || !Corridor)
		{
			Expect(false, TEXT("plan pieces found"));
			return true;
		}
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		Select(Vic);
		const auto DoorStep = [](ADCDoor* Door)
		{
			FDCPlanTask Task;
			Task.Type = EDCPlanTaskType::Door;
			Task.Target = Door->GetFName();
			Task.Action = static_cast<uint8>(EDCDoorAction::PickLock);
			return Task;
		};
		Plans->AddTask(Vic->GetFName(), DoorStep(Gate));
		Plans->AddTask(Vic->GetFName(), DoorStep(Front));
		Plans->AddTask(Vic->GetFName(), DoorStep(Cellar));
		FDCPlanTask Go;
		Go.Type = EDCPlanTaskType::Move;
		Go.Location = Corridor->GetActorLocation() - FVector(0.0, 0.0, 120.0) + FVector(0.0, -600.0, 0.0);
		Plans->AddTask(Vic->GetFName(), Go);
		Heist->Refresh();
		const FDCCrewSchedule* Plan = Heist->GetSchedule().Find(Vic->GetFName());
		bool bAllPossible = Plan && Plan->Tasks.Num() == 4;
		for (const FDCScheduledTask& Task : Plan ? Plan->Tasks : TArray<FDCScheduledTask>())
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: Vic %d. %s %.1f-%.1f%s %s"), Task.Index + 1, *Task.Label.ToString(), Task.Start, Task.End,
				Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.WaitReason.ToString());
			bAllPossible &= !Task.bBlocked;
		}
		Expect(bAllPossible, TEXT("street -> garden gate -> front door -> cellar door -> cellar corridor can be planned"));
		Plans->SetSpeed(3.f);
		Camera(VillaPoint(30.0, 14.0, 0.0), 3600.f);
		return true;
	});
	SelfTestSteps.Add(Wait(2.f));
	SelfTestSteps.Add(Shot(TEXT("villa_plan.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]() { StartPlayback(); return true; });
	SelfTestSteps.Add([this, CrewNamed, Camera, Started = -1.0, LastFocus = 0.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		if (Now - LastFocus > 1.0)
		{
			LastFocus = Now;
			if (const ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic")))
			{
				Camera(Vic->GetActorLocation(), 2400.f);
			}
		}
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		return (Heist && Heist->IsPlaybackFinished()) || Now - Started > 150.0;
	});
	SelfTestSteps.Add(Wait(1.5f));
	SelfTestSteps.Add(Shot(TEXT("villa_keller.png")));
	SelfTestSteps.Add([this, Expect, CrewNamed, RoomOf, FirstOnFloor, Camera]()
	{
		ADCCrewCharacter* Vic = CrewNamed(TEXT("Vic"));
		const UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: playback took %.1f s (planned %.1f s), Vic in %s"), Heist->GetPlaybackTime(),
			Heist->GetSchedule().Duration, Vic ? *RoomOf(Vic->GetActorLocation()) : TEXT("-"));
		Expect(Vic && RoomOf(Vic->GetActorLocation()) == TEXT("Kellerflur"), TEXT("Vic stands in the cellar corridor"));
		for (int32 Index = 0; Vic && Index < 4; ++Index)
		{
			Expect(Vic->GetPlanRunner()->GetStatus(Index) == EDCTaskStatus::Done, FString::Printf(TEXT("Vic step %d done"), Index + 1));
		}
		UDCFloorSubsystem* Floors = UDCFloorSubsystem::Get(this);
		const AActor* Slab = FirstOnFloor(TEXT("DCFloor_EG"));
		Expect(Floors && Floors->GetShownFloor() == -1, TEXT("the camera followed Vic into the cellar"));
		Expect(Slab && Slab->IsHidden(), TEXT("the ground floor is hidden while the cellar is shown"));
		// Page Up: the ground floor again (Vic stays below, under the floor).
		if (Floors)
		{
			Floors->StepFloor(1);
		}
		Expect(Floors && Floors->GetShownFloor() == 0 && Slab && !Slab->IsHidden(), TEXT("one storey up shows the ground floor again"));
		Camera(VillaPoint(33.0, 22.0, 0.0), 3200.f);
		return true;
	});
	SelfTestSteps.Add(Wait(2.f));
	SelfTestSteps.Add(Shot(TEXT("villa_eg.png")));
	SelfTestSteps.Add([Camera]()
	{
		Camera(VillaPoint(30.0, 22.0, 0.0), 5500.f);
		return true;
	});
	SelfTestSteps.Add(Wait(2.5f));
	SelfTestSteps.Add(Shot(TEXT("villa_overview.png")));
	SelfTestSteps.Add([this, bPassed]()
	{
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->bSelfTestPassed = *bPassed;
		Plans->SelfTestStage = 1;
		UE_LOG(LogDerClou, Display, TEXT("Self-test: reloading for stage 2 (play right after loading)"));
		ReturnToPlanning();
		return true;
	});
}
