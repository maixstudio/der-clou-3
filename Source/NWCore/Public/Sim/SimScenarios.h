#pragma once

#include "Sim/SimDefinition.h"
#include "Sim/SimPlan.h"

#include <vector>

namespace nw::sim::scenarios
{
	struct Scenario
	{
		ObjectDefinition Object;
		std::vector<CrewDef> Crew;
		HeistPlan Plan;
	};

	/**
	 * Graybox version of the vertical-slice target "Jeweler" (docs/02-game-design.md, section 13).
	 *
	 *   Room 0 (alley)    Room 1 (corridor)            Room 4 (office, safe)
	 *   [Van]--[BackOut]==D0==[BackIn]--[Corridor]==D3(alarm)==[Office]
	 *                                      ||   ||
	 *                                     D1    D2
	 *                                      ||   ||
	 *                        Room 2 [Cellar]   [ShopBack]--[ShopFront]  Room 3 (shop, guard,
	 *                        (alarm panel)                  light barrier)
	 */
	namespace jeweler
	{
		constexpr NodeId Van = 0;
		constexpr NodeId BackDoorOutside = 1;
		constexpr NodeId BackDoorInside = 2;
		constexpr NodeId Corridor = 3;
		constexpr NodeId Cellar = 4;
		constexpr NodeId ShopBack = 5;
		constexpr NodeId ShopFront = 6;
		constexpr NodeId Office = 7;

		constexpr DoorId BackDoor = 0;
		constexpr DoorId CellarDoor = 1;
		constexpr DoorId ShopDoor = 2;
		constexpr DoorId OfficeDoor = 3;

		constexpr AlarmId OfficeContact = 0;
		constexpr AlarmId ShopLightBarrier = 1;

		constexpr SafeId WallSafe = 0;

		constexpr int32_t Driver = 0;
		constexpr int32_t Electrician = 1;
		constexpr int32_t Safecracker = 2;

		constexpr SignalId AlarmOff = 0;
		constexpr SignalId SafeOpen = 1;
	}

	/** Object, crew and a reference plan that pulls off a clean getaway for every seed. */
	NWCORE_API Scenario MakeJewelerScenario();
}
