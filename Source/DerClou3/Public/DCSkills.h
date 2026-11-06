#pragma once

#include "CoreMinimal.h"

#include "DCSkills.generated.h"

/** What a crew member can do well. Values 0 (none) to 10 (master). */
enum class EDCSkill : uint8
{
	Locks,
	Safes,
	Electronics,
	Explosives,
	Driving,
	Fighting,
	Lookout,
	Strength,
	Count
};

/** The skills of one crew member (editable per placed crew member). */
USTRUCT()
struct FDCSkills
{
	GENERATED_BODY()

	/** Picking door, drawer and case locks. */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Locks = 0;

	/** Cracking safes. */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Safes = 0;

	/** Alarm systems, cameras (not used yet). */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Electronics = 0;

	/** Blowing things open (not used yet). */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Explosives = 0;

	/** The getaway car (not used yet). */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Driving = 0;

	/** Dealing with guards (not used yet). */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Fighting = 0;

	/** Keeping watch (not used yet). */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Lookout = 0;

	/** Breaking things open, smashing glass, how much one can carry. */
	UPROPERTY(EditAnywhere, Category = "Skills", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Strength = 5;

	int32 Get(EDCSkill Skill) const;
};

/** Whether and how well a crew member can do one task. */
struct FDCTaskCheck
{
	bool bPossible = true;
	/** Why not (or empty). */
	FText Reason;
	/** 0 = instant. */
	float Seconds = 0.f;
	/** 0..1. */
	float FailChance = 0.f;
	/** Multiplies the noise the task makes. */
	float NoiseFactor = 1.f;
	/** Skill used and its comparison, for the hint ("Schlösser 6 – geübt"). */
	FText SkillNote;

	/** One line for tooltips: "ca. 3 s · Erfolg 100 % · Schlösser 6 (nötig 3): geübt". */
	FText Describe() const;
};

namespace DCSkills
{
	DERCLOU3_API FText Name(EDCSkill Skill);
	/** True for skills nothing in the game uses yet. */
	DERCLOU3_API bool IsUnused(EDCSkill Skill);

	/**
	 * Compares skill with difficulty (both 0..10):
	 * more than 2 below = impossible; 1–2 below = slower, louder and may fail;
	 * equal = as described; above = faster and quieter.
	 */
	DERCLOU3_API FDCTaskCheck Evaluate(EDCSkill Skill, int32 Value, int32 Difficulty, float BaseSeconds, const FText& CrewName);

	/** How far below the difficulty a skill may be before the task is impossible. */
	constexpr int32 MaxShortfall = 2;
}
