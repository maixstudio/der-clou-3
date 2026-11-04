#pragma once

#include "CoreMinimal.h"
#include "NWPlanTypes.h"
#include "Sim/SimDefinition.h"
#include "Sim/SimPlan.h"

class UNWCrewMemberAsset;
class UNWHeistObjectAsset;

/** Converts editor-authored data (seconds, floats, FNames) into integer simulation data. */
namespace NWSimConversion
{
	NWPLANNING_API nw::sim::ObjectDefinition ToSim(const UNWHeistObjectAsset& Asset);
	NWPLANNING_API nw::sim::CrewDef ToSim(const UNWCrewMemberAsset& Asset);
	NWPLANNING_API nw::sim::HeistPlan ToSim(const TArray<FNWCrewPlan>& Plan);

	/** Inverse of ToSim for plans (used to edit the built-in scenario in the planner). */
	NWPLANNING_API TArray<FNWCrewPlan> FromSim(const nw::sim::HeistPlan& Plan);
}
