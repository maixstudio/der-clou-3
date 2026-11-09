// -DCSelfTest=SecretDoor on L_Villa01: Rosa stands in the library, goes through the secret door (the turning
// bookshelf) into the secret room and picks up a painting there. Logs where the nav mesh is around the door
// before and after, and takes a picture from above with the nav mesh shown.

#include "DCCameraPawn.h"
#include "DCCrewCharacter.h"
#include "DCDoor.h"
#include "DCHeistState.h"
#include "DCItem.h"
#include "DCItemPickup.h"
#include "DCPlan.h"
#include "DCPlanRunner.h"
#include "DCPlayerController.h"
#include "DCRoomVolume.h"
#include "DerClou3.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	/** Plan metres (x east, y north) to Unreal cm, as in build_villa.py. */
	FVector SecretPoint(double X, double Y, double Z)
	{
		return FVector(Y * 100.0, X * 100.0, Z);
	}
}

void ADCPlayerController::StartSecretDoorSelfTest()
{
	UE_LOG(LogDerClou, Display, TEXT("Self-test SecretDoor started"));
	UDCPlanSubsystem::Get(this)->bAutoDecideFinds = true;
	const TSharedRef<bool> bPassed = MakeShared<bool>(true);
	const auto Expect = [bPassed](bool bCondition, const FString& What)
	{
		UE_LOG(LogDerClou, Display, TEXT("Self-test: %s %s"), bCondition ? TEXT("ok  ") : TEXT("FAIL"), *What);
		*bPassed &= bCondition;
	};
	const auto Rosa = [this]() -> ADCCrewCharacter*
	{
		for (TActorIterator<ADCCrewCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetCrewName().ToString() == TEXT("Rosa"))
			{
				return *It;
			}
		}
		return nullptr;
	};
	const auto Door = [this]() -> ADCDoor*
	{
		for (TActorIterator<ADCDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetInteractableName().ToString() == TEXT("Geheimtür"))
			{
				return *It;
			}
		}
		return nullptr;
	};
	// The painting on the south wall of the secret room.
	const auto Painting = [this]() -> ADCItemPickup*
	{
		ADCItemPickup* Best = nullptr;
		for (TActorIterator<ADCItemPickup> It(GetWorld()); It; ++It)
		{
			const UDCItem* Item = It->GetItem();
			if (Item && Item->GetDefId() == TEXT("painting") && (!Best || FVector::Dist2D(It->GetActorLocation(), SecretPoint(43.0, 16.21, 0.0))
				< FVector::Dist2D(Best->GetActorLocation(), SecretPoint(43.0, 16.21, 0.0))))
			{
				Best = *It;
			}
		}
		return Best;
	};
	// Where the nav mesh is around the door (On = a walkable spot within 40 cm on the storey).
	const auto Probe = [this, Door](const TCHAR* When)
	{
		const ADCDoor* Secret = Door();
		if (!Secret)
		{
			return;
		}
		UWorld* World = GetWorld();
		const FVector Library = Secret->GetStandPoint(SecretPoint(40.5, 17.25, 0.0), false);
		const FVector Inside = Secret->GetStandPoint(SecretPoint(40.5, 17.25, 0.0), true);
		const TPair<const TCHAR*, FVector> Points[] = {
			{ TEXT("library stand point"), Library },
			{ TEXT("pass target (inside)"), Inside },
			{ TEXT("secret room middle"), SecretPoint(43.0, 17.3, 0.0) },
			{ TEXT("secret room north"), SecretPoint(43.0, 18.0, 0.0) },
			{ TEXT("door centre"), Secret->GetCenter() },
		};
		for (const TPair<const TCHAR*, FVector>& Point : Points)
		{
			FVector On;
			const bool bOn = DCNav::ProjectOnStorey(World, Point.Value, 40.0, On);
			UE_LOG(LogDerClou, Display, TEXT("Self-test: probe %s - %s at %s: %s"), When, Point.Key, *Point.Value.ToCompactString(),
				bOn ? *FString::Printf(TEXT("nav mesh at %s (%.0f cm off)"), *On.ToCompactString(), FVector::Dist(On, Point.Value)) : TEXT("NO NAV MESH"));
		}
		FVector From;
		FVector To;
		TArray<FVector> Way;
		const bool bWay = DCNav::ProjectOnStorey(World, Library, 60.0, From) && DCNav::ProjectOnStorey(World, Inside, 60.0, To)
			&& DCNav::FindPath(World, From, To, &Way);
		UE_LOG(LogDerClou, Display, TEXT("Self-test: probe %s - way library -> inside: %s (%d points), door open %d, open amount %.2f"), When,
			bWay ? TEXT("yes") : TEXT("NO"), Way.Num(), Secret->IsOpen() ? 1 : 0, Secret->GetOpenAmount());
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

	SelfTestSteps.Add([this, Started = -1.0]() mutable
	{
		const double Now = FPlatformTime::Seconds();
		Started = Started < 0.0 ? Now : Started;
		return (Now - Started > 2.0 && UDCHeistStateSubsystem::IsNavigationReady(this)) || Now - Started > 60.0;
	});
	SelfTestSteps.Add([this, Expect, Rosa, Door, Painting, Probe, Camera]()
	{
		ADCCrewCharacter* Member = Rosa();
		ADCDoor* Secret = Door();
		ADCItemPickup* Picture = Painting();
		if (!Member || !Secret || !Picture)
		{
			Expect(false, TEXT("Rosa, the secret door and the painting found"));
			FinishSelfTest(false);
			return true;
		}
		Probe(TEXT("closed"));
		// Rosa in the library, a little in front of the bookshelf.
		Member->SetActorLocation(SecretPoint(40.6, 17.6, 92.15), false, nullptr, ETeleportType::TeleportPhysics);
		Select(Member);
		UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(this);
		Plans->SetMode(EDCPlanMode::Planning);
		Plans->ClearAll();
		FDCPlanTask Through;
		Through.Type = EDCPlanTaskType::Door;
		Through.Target = Secret->GetFName();
		Through.Action = static_cast<uint8>(EDCDoorAction::GoThrough);
		Plans->AddTask(Member->GetFName(), Through);
		FDCPlanTask Take;
		Take.Type = EDCPlanTaskType::PickUp;
		Take.Target = Picture->GetFName();
		Plans->AddTask(Member->GetFName(), Take);
		UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(this);
		Heist->Refresh();
		const FDCCrewSchedule* Plan = Heist->GetSchedule().Find(Member->GetFName());
		for (const FDCScheduledTask& Task : Plan ? Plan->Tasks : TArray<FDCScheduledTask>())
		{
			UE_LOG(LogDerClou, Display, TEXT("Self-test: Rosa %d. %s %.1f-%.1f%s %s"), Task.Index + 1, *Task.Label.ToString(), Task.Start, Task.End,
				Task.bBlocked ? TEXT(" BLOCKED") : TEXT(""), *Task.WaitReason.ToString());
		}
		Camera(SecretPoint(42.0, 17.3, 0.0), 1400.f);
		ConsoleCommand(TEXT("show Navigation"));
		return true;
	});
	SelfTestSteps.Add(Wait(1.5f));
	SelfTestSteps.Add(Shot(TEXT("secret_door_closed.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this]()
	{
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
	SelfTestSteps.Add(Wait(1.0f));
	SelfTestSteps.Add(Shot(TEXT("secret_door_open.png")));
	SelfTestSteps.Add(Wait(0.5f));
	SelfTestSteps.Add([this, Expect, Rosa, Probe, Painting, bPassed]()
	{
		Probe(TEXT("after playback"));
		if (const ADCItemPickup* Picture = Painting())
		{
			FVector Spot;
			const bool bSpot = DCNav::FindStandSpot(GetWorld(), Picture->GetActorLocation(), Picture->GetFootprintRadius() + 40.0,
				SecretPoint(42.5, 17.3, 0.0), Spot, [](const FVector&) { return false; });
			const ADCRoomVolume* SpotRoom = ADCRoomVolume::FindAt(this, Spot + FVector(0.0, 0.0, 50.0));
			UE_LOG(LogDerClou, Display, TEXT("Self-test: stand spot for the painting at %s: %s %s in %s"), *Picture->GetActorLocation().ToCompactString(),
				bSpot ? TEXT("found") : TEXT("NOT found"), *Spot.ToCompactString(), SpotRoom ? *SpotRoom->GetRoomName().ToString().Replace(TEXT("­"), TEXT("")) : TEXT("-"));
		}
		ADCCrewCharacter* Member = Rosa();
		const ADCRoomVolume* Room = Member ? ADCRoomVolume::FindAt(this, Member->GetActorLocation()) : nullptr;
		const FString RoomName = Room ? Room->GetRoomName().ToString().Replace(TEXT("­"), TEXT("")) : FString(TEXT("-"));
		UE_LOG(LogDerClou, Display, TEXT("Self-test: Rosa ends at %s in %s"), Member ? *Member->GetActorLocation().ToCompactString() : TEXT("-"), *RoomName);
		Expect(Member && Member->GetPlanRunner()->GetStatus(0) == EDCTaskStatus::Done && RoomName == TEXT("Geheimkammer"),
			TEXT("Rosa went through the secret door into the secret room"));
		Expect(Member && Member->GetPlanRunner()->GetStatus(1) == EDCTaskStatus::Done, TEXT("Rosa picked up the painting there"));
		FinishSelfTest(*bPassed);
		return true;
	});
}
