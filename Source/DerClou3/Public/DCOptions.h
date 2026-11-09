#pragma once

#include "CoreMinimal.h"

/** The settings of "Optionen", as edited before "Übernehmen". */
struct FDCOptionValues
{
	/** 0 niedrig .. 3 episch. */
	int32 Quality = 3;
	/** 0 Fenster, 1 Vollbild im Fenster, 2 Vollbild. */
	int32 WindowMode = 1;
	FIntPoint Resolution = FIntPoint(1920, 1080);
	/** 0 langsam, 1 normal, 2 schnell. */
	int32 CameraSpeed = 1;
	bool bGhostAsPin = false;
	bool bHelpAsMenu = false;
	bool bInsertOnlyWithButton = false;
	bool bPauseOnEveryFind = false;

	bool operator==(const FDCOptionValues& Other) const
	{
		return Quality == Other.Quality && WindowMode == Other.WindowMode && Resolution == Other.Resolution && CameraSpeed == Other.CameraSpeed
			&& bGhostAsPin == Other.bGhostAsPin && bHelpAsMenu == Other.bHelpAsMenu && bInsertOnlyWithButton == Other.bInsertOnlyWithButton
			&& bPauseOnEveryFind == Other.bPauseOnEveryFind;
	}
	bool operator!=(const FDCOptionValues& Other) const { return !(*this == Other); }
};

/**
 * Options: graphics and display through UGameUserSettings, camera speed and the planning variants in
 * GameUserSettings.ini (sections DerClou.Options and DerClou.Variants). Self-tests never write them.
 */
namespace DCOptions
{
	DERCLOU3_API FDCOptionValues GetCurrent(const UObject* WorldContext);
	/** Applies and keeps them. */
	DERCLOU3_API void Apply(const UObject* WorldContext, const FDCOptionValues& Values);
	/** Panning and turning speed of the planning camera (1 = normal). */
	DERCLOU3_API float CameraSpeedFactor();
	/** Resolutions to choose from for a window mode (always contains the current one). */
	DERCLOU3_API TArray<FIntPoint> GetResolutions(int32 WindowMode, const FIntPoint& Current);

	DERCLOU3_API FText QualityName(int32 Quality);
	DERCLOU3_API FText WindowModeName(int32 Mode);
	DERCLOU3_API FText CameraSpeedName(int32 Speed);
}
