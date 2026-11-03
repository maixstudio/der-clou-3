#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWPlanTypes.h"
#include "Sim/Simulation.h"
#include "Templates/UniquePtr.h"

#include "NWSimulationDirector.generated.h"

class UNWCrewMemberAsset;
class UNWHeistObjectAsset;

UENUM(BlueprintType)
enum class ENWHeistOutcome : uint8
{
	Running,
	CleanGetaway,
	EscapedWithAlarm,
	Busted,
	TimedOut,
	InvalidSetup
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNWOnHeistFinished, ENWHeistOutcome, Outcome, int32, LootValue);

/**
 * Owns one nw::sim::Simulation and plays it back in the world.
 *
 * The simulation advances in fixed steps (nw::sim::TicksPerSecond); rendering interpolates
 * between the last two steps. Until real art exists, the state is drawn with debug shapes:
 * place this actor in an empty level and press Play.
 */
UCLASS()
class NWGAME_API ANWSimulationDirector : public AActor
{
	GENERATED_BODY()

public:
	ANWSimulationDirector();

	virtual void Tick(float DeltaSeconds) override;

	/** (Re)creates the simulation from the current setup and starts playback. */
	UFUNCTION(BlueprintCallable, Category = "Nachtwerk|Simulation")
	void StartSimulation();

	/**
	 * Runs a copy of the simulation to the end without rendering ("ghost run").
	 * Starts from the current state if a simulation is running, otherwise from the beginning.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nachtwerk|Simulation")
	ENWHeistOutcome RunGhostPreview(int32& OutLootValue, float& OutDurationSeconds) const;

	UFUNCTION(BlueprintCallable, Category = "Nachtwerk|Simulation")
	void SetSimulationPaused(bool bInPaused);

	UFUNCTION(BlueprintPure, Category = "Nachtwerk|Simulation")
	bool IsSimulationRunning() const;

	UFUNCTION(BlueprintPure, Category = "Nachtwerk|Simulation")
	float GetSimulatedSeconds() const;

	UPROPERTY(BlueprintAssignable, Category = "Nachtwerk|Simulation")
	FNWOnHeistFinished OnHeistFinished;

protected:
	virtual void BeginPlay() override;

	/** Use the hard-coded graybox jeweler from NWCore instead of the assets below. */
	UPROPERTY(EditAnywhere, Category = "Setup")
	bool bUseBuiltInJewelerScenario = true;

	UPROPERTY(EditAnywhere, Category = "Setup", meta = (EditCondition = "!bUseBuiltInJewelerScenario"))
	TObjectPtr<UNWHeistObjectAsset> HeistObject;

	UPROPERTY(EditAnywhere, Category = "Setup", meta = (EditCondition = "!bUseBuiltInJewelerScenario"))
	TArray<TObjectPtr<UNWCrewMemberAsset>> Crew;

	/** Plan[i] belongs to Crew[i]. */
	UPROPERTY(EditAnywhere, Category = "Setup", meta = (EditCondition = "!bUseBuiltInJewelerScenario"))
	TArray<FNWCrewPlan> Plan;

	UPROPERTY(EditAnywhere, Category = "Setup")
	int32 Seed = 42;

	UPROPERTY(EditAnywhere, Category = "Playback")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, Category = "Playback", meta = (ClampMin = "0", ClampMax = "32"))
	float PlaybackSpeed = 1.f;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = true;

private:
	TUniquePtr<nw::sim::Simulation> CreateSimulation() const;
	void CapturePositions(TArray<FVector>& OutPositions) const;
	void LogNewEvents();
	void HandleFinished();
	void DrawDebugState(float Alpha) const;
	FVector ToWorld(const nw::sim::IntVec2& Position, float Height = 0.f) const;

	static ENWHeistOutcome ToOutcome(nw::sim::Outcome Value);

	TUniquePtr<nw::sim::Simulation> Sim;

	/** Crew positions followed by guard positions, before / after the last step. */
	TArray<FVector> PreviousPositions;
	TArray<FVector> CurrentPositions;

	float Accumulator = 0.f;
	bool bPaused = false;
	bool bFinishHandled = false;
	int32 LoggedEventCount = 0;
};
