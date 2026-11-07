#include "DCSkills.h"

#define LOCTEXT_NAMESPACE "DCSkills"

int32 FDCSkills::Get(EDCSkill Skill) const
{
	switch (Skill)
	{
	case EDCSkill::Locks: return Locks;
	case EDCSkill::Safes: return Safes;
	case EDCSkill::Electronics: return Electronics;
	case EDCSkill::Explosives: return Explosives;
	case EDCSkill::Driving: return Driving;
	case EDCSkill::Fighting: return Fighting;
	case EDCSkill::Lookout: return Lookout;
	case EDCSkill::Strength: return Strength;
	case EDCSkill::Count: break;
	}
	return 0;
}

FText FDCTaskCheck::Describe() const
{
	if (!bPossible)
	{
		return Reason;
	}
	if (Seconds <= 0.f)
	{
		return FText::GetEmpty();
	}
	const FText Base = FText::Format(LOCTEXT("Describe", "ca. {0} s · Erfolg {1} %"),
		FText::AsNumber(FMath::RoundToInt(Seconds)), FText::AsNumber(FMath::RoundToInt((1.f - FailChance) * 100.f)));
	return SkillNote.IsEmpty() ? Base : FText::Format(LOCTEXT("DescribeSkill", "{0} · {1}"), Base, SkillNote);
}

FText FDCTaskCheck::DescribeShort() const
{
	if (!bPossible)
	{
		return Reason;
	}
	if (Seconds <= 0.f)
	{
		return FText::GetEmpty();
	}
	const FText Word = Rating > 0 ? LOCTEXT("ShortEasy", "geübt") : Rating < 0 ? LOCTEXT("ShortHard", "schwierig") : LOCTEXT("ShortFair", "passt");
	return FText::Format(LOCTEXT("Short", "{0} s · {1} % · {2}"), FText::AsNumber(FMath::RoundToInt(Seconds)),
		FText::AsNumber(FMath::RoundToInt((1.f - FailChance) * 100.f)), SkillNote.IsEmpty() ? FText::GetEmpty() : Word);
}

namespace DCSkills
{
	FText Name(EDCSkill Skill)
	{
		switch (Skill)
		{
		case EDCSkill::Locks: return LOCTEXT("Locks", "Schlösser");
		case EDCSkill::Safes: return LOCTEXT("Safes", "Tresore");
		case EDCSkill::Electronics: return LOCTEXT("Electronics", "Elektronik");
		case EDCSkill::Explosives: return LOCTEXT("Explosives", "Sprengstoff");
		case EDCSkill::Driving: return LOCTEXT("Driving", "Fahren");
		case EDCSkill::Fighting: return LOCTEXT("Fighting", "Kampf");
		case EDCSkill::Lookout: return LOCTEXT("Lookout", "Aufpassen");
		case EDCSkill::Strength: return LOCTEXT("Strength", "Kraft");
		case EDCSkill::Count: break;
		}
		return FText::GetEmpty();
	}

	bool IsUnused(EDCSkill Skill)
	{
		return Skill != EDCSkill::Locks && Skill != EDCSkill::Safes && Skill != EDCSkill::Strength;
	}

	FDCTaskCheck Evaluate(EDCSkill Skill, int32 Value, int32 Difficulty, float BaseSeconds, const FText& CrewName)
	{
		FDCTaskCheck Check;
		const int32 Delta = Value - Difficulty;
		const FText SkillName = Name(Skill);
		if (Delta < -MaxShortfall)
		{
			Check.bPossible = false;
			Check.Reason = FText::Format(LOCTEXT("TooLow", "{0} kann das nicht ({1} {2}, nötig mindestens {3})"),
				CrewName, SkillName, FText::AsNumber(Value), FText::AsNumber(Difficulty - MaxShortfall));
			return Check;
		}
		if (Delta < 0)
		{
			// Below the task: slower, clumsier, louder.
			const int32 Shortfall = -Delta;
			Check.Seconds = BaseSeconds * (1.f + 0.5f * Shortfall);
			Check.FailChance = 0.25f * Shortfall;
			Check.NoiseFactor = 1.f + 0.3f * Shortfall;
			Check.Rating = -1;
			Check.SkillNote = FText::Format(LOCTEXT("Hard", "{0} {1} (nötig {2}): schwierig – langsamer, lauter, kann misslingen"),
				SkillName, FText::AsNumber(Value), FText::AsNumber(Difficulty));
		}
		else if (Delta == 0)
		{
			Check.Seconds = BaseSeconds;
			Check.SkillNote = FText::Format(LOCTEXT("Fair", "{0} {1} (nötig {2}): passt"), SkillName, FText::AsNumber(Value), FText::AsNumber(Difficulty));
		}
		else
		{
			// Above the task: quicker and quieter.
			Check.Seconds = BaseSeconds * FMath::Max(0.4f, 1.f - 0.15f * Delta);
			Check.NoiseFactor = FMath::Max(0.5f, 1.f - 0.1f * Delta);
			Check.Rating = 1;
			Check.SkillNote = FText::Format(LOCTEXT("Easy", "{0} {1} (nötig {2}): geübt – schneller, leiser"), SkillName, FText::AsNumber(Value), FText::AsNumber(Difficulty));
		}
		return Check;
	}
}

#undef LOCTEXT_NAMESPACE
