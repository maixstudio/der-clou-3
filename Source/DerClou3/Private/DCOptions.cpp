#include "DCOptions.h"

#include "DCHeistState.h"
#include "DCPlan.h"
#include "DerClou3.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "DCOptions"

namespace
{
	const TCHAR* const OptionsSection = TEXT("DerClou.Options");
	const float CameraSpeedFactors[] = { 0.6f, 1.f, 1.6f };

	bool OptionsInSelfTest()
	{
		FString Ignored;
		return FParse::Value(FCommandLine::Get(), TEXT("DCSelfTest="), Ignored);
	}

	int32 ReadCameraSpeed()
	{
		int32 Speed = 1;
		if (GConfig && !OptionsInSelfTest())
		{
			GConfig->GetInt(OptionsSection, TEXT("CameraSpeed"), Speed, GGameUserSettingsIni);
		}
		return FMath::Clamp(Speed, 0, 2);
	}

	int32 CachedCameraSpeed = -1;

	int32 ToWindowModeIndex(EWindowMode::Type Mode)
	{
		switch (Mode)
		{
		case EWindowMode::Windowed: return 0;
		case EWindowMode::Fullscreen: return 2;
		default: return 1;
		}
	}

	EWindowMode::Type FromWindowModeIndex(int32 Index)
	{
		return Index == 0 ? EWindowMode::Windowed : Index == 2 ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;
	}
}

FDCOptionValues DCOptions::GetCurrent(const UObject* WorldContext)
{
	FDCOptionValues Values;
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Values.Quality = FMath::Clamp(Settings->GetOverallScalabilityLevel(), 0, 3);
		if (Settings->GetOverallScalabilityLevel() < 0)
		{
			// Mixed ("custom") levels: show the one for view distance as the overall level.
			Values.Quality = FMath::Clamp(Settings->GetViewDistanceQuality(), 0, 3);
		}
		Values.WindowMode = ToWindowModeIndex(Settings->GetFullscreenMode());
		Values.Resolution = Settings->GetScreenResolution();
	}
	if (CachedCameraSpeed < 0)
	{
		CachedCameraSpeed = ReadCameraSpeed();
	}
	Values.CameraSpeed = CachedCameraSpeed;
	if (const UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(WorldContext))
	{
		Values.bGhostAsPin = Plans->bGhostAsPin;
		Values.bHelpAsMenu = Plans->bHelpAsMenu;
		Values.bInsertOnlyWithButton = Plans->bInsertOnlyWithButton;
	}
	return Values;
}

void DCOptions::Apply(const UObject* WorldContext, const FDCOptionValues& Values)
{
	const bool bKeep = !OptionsInSelfTest();
	const FDCOptionValues Before = GetCurrent(WorldContext);
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const bool bDisplay = Before.WindowMode != Values.WindowMode || Before.Resolution != Values.Resolution;
		if (Before.Quality != Values.Quality)
		{
			Settings->SetOverallScalabilityLevel(Values.Quality);
		}
		if (bDisplay)
		{
			Settings->SetFullscreenMode(FromWindowModeIndex(Values.WindowMode));
			Settings->SetScreenResolution(Values.Resolution);
		}
		if (bDisplay || Before.Quality != Values.Quality)
		{
			Settings->ApplySettings(false);
		}
		if (bKeep)
		{
			Settings->SaveSettings();
		}
	}
	CachedCameraSpeed = FMath::Clamp(Values.CameraSpeed, 0, 2);
	if (UDCPlanSubsystem* Plans = UDCPlanSubsystem::Get(WorldContext))
	{
		Plans->bGhostAsPin = Values.bGhostAsPin;
		Plans->bHelpAsMenu = Values.bHelpAsMenu;
		Plans->bInsertOnlyWithButton = Values.bInsertOnlyWithButton;
		Plans->SaveVariants();
	}
	if (UDCHeistStateSubsystem* Heist = UDCHeistStateSubsystem::Get(WorldContext))
	{
		Heist->UpdateWorldLook();
	}
	if (GConfig && bKeep)
	{
		GConfig->SetInt(OptionsSection, TEXT("CameraSpeed"), CachedCameraSpeed, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UE_LOG(LogDerClou, Display, TEXT("Options: quality %d, window %d, %dx%d, camera %d, ghost %s, help %s, insert %s"), Values.Quality, Values.WindowMode,
		Values.Resolution.X, Values.Resolution.Y, Values.CameraSpeed, Values.bGhostAsPin ? TEXT("pin") : TEXT("figure"),
		Values.bHelpAsMenu ? TEXT("menu") : TEXT("bar"), Values.bInsertOnlyWithButton ? TEXT("button") : TEXT("picked step"));
}

float DCOptions::CameraSpeedFactor()
{
	if (CachedCameraSpeed < 0)
	{
		CachedCameraSpeed = ReadCameraSpeed();
	}
	return CameraSpeedFactors[FMath::Clamp(CachedCameraSpeed, 0, 2)];
}

TArray<FIntPoint> DCOptions::GetResolutions(int32 WindowMode, const FIntPoint& Current)
{
	TArray<FIntPoint> Found;
	if (WindowMode == 0)
	{
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Found);
	}
	else
	{
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Found);
	}
	TArray<FIntPoint> Unique;
	for (const FIntPoint& Size : Found)
	{
		// Nothing too small for the frame around the scene.
		if (Size.X >= 1280 && Size.Y >= 720)
		{
			Unique.AddUnique(Size);
		}
	}
	Unique.AddUnique(Current);
	Unique.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	return Unique;
}

FText DCOptions::QualityName(int32 Quality)
{
	switch (Quality)
	{
	case 0: return LOCTEXT("QualityLow", "Niedrig");
	case 1: return LOCTEXT("QualityMedium", "Mittel");
	case 2: return LOCTEXT("QualityHigh", "Hoch");
	default: return LOCTEXT("QualityEpic", "Sehr hoch");
	}
}

FText DCOptions::WindowModeName(int32 Mode)
{
	switch (Mode)
	{
	case 0: return LOCTEXT("ModeWindow", "Fenster");
	case 2: return LOCTEXT("ModeFull", "Vollbild");
	default: return LOCTEXT("ModeBorderless", "Randlos");
	}
}

FText DCOptions::CameraSpeedName(int32 Speed)
{
	switch (Speed)
	{
	case 0: return LOCTEXT("CamSlow", "Langsam");
	case 2: return LOCTEXT("CamFast", "Schnell");
	default: return LOCTEXT("CamNormal", "Normal");
	}
}

#undef LOCTEXT_NAMESPACE
