#include "Sim/SimScenarios.h"

namespace nw::sim::scenarios
{
	Scenario MakeJewelerScenario()
	{
		using namespace jeweler;

		Scenario Result;
		ObjectDefinition& Object = Result.Object;
		Object.Name = "Jeweler Brandauer & Sons";
		Object.EntryNode = Van;
		Object.TimeLimitTicks = SecondsToTicks(1800);

		Object.Nodes = {
			{ "Van", { 0, 0 }, 0 },
			{ "Back door (outside)", { 400, 0 }, 0 },
			{ "Back door (inside)", { 550, 0 }, 1 },
			{ "Corridor", { 900, 0 }, 1 },
			{ "Cellar", { 900, -600 }, 2 },
			{ "Shop (back)", { 900, 500 }, 3 },
			{ "Shop (front)", { 1500, 500 }, 3 },
			{ "Office", { 1400, 0 }, 4 },
		};

		Object.Edges = {
			{ Van, BackDoorOutside, InvalidId },
			{ BackDoorOutside, BackDoorInside, BackDoor },
			{ BackDoorInside, Corridor, InvalidId },
			{ Corridor, Cellar, CellarDoor },
			{ Corridor, ShopBack, ShopDoor },
			{ ShopBack, ShopFront, InvalidId },
			{ Corridor, Office, OfficeDoor },
		};

		Object.Doors.resize(4);
		Object.Doors[BackDoor] = { "Back door", LockType::Security, true, InvalidId, 150 };
		Object.Doors[CellarDoor] = { "Cellar door", LockType::None, false, InvalidId, 150 };
		Object.Doors[ShopDoor] = { "Shop door", LockType::None, false, InvalidId, 150 };
		Object.Doors[OfficeDoor] = { "Office door", LockType::Simple, true, OfficeContact, 150 };

		Object.Alarms.resize(2);
		Object.Alarms[OfficeContact] = { "Office door contact", AlarmType::Contact, Cellar, InvalidId, 90, SecondsToTicks(180) };
		Object.Alarms[ShopLightBarrier] = { "Shop light barrier", AlarmType::LightBarrier, Cellar, ShopFront, 90, SecondsToTicks(150) };

		Object.Safes = { { "Wall safe", Office, 240, 85000 } };

		GuardDef Guard;
		Guard.Name = "Night watchman Behrens";
		Guard.StartNode = ShopFront;
		Guard.Route = {
			{ ShopFront, SecondsToTicks(30), SecondsToTicks(60) },
			{ ShopBack, SecondsToTicks(10), SecondsToTicks(20) },
		};
		Guard.SpeedCmPerTick = 12;
		Guard.SightRangeCm = 900;
		Guard.ResponseTicks = SecondsToTicks(120);
		Object.Guards = { Guard };

		Result.Crew.resize(3);
		Result.Crew[Driver] = { "Vic", 1, 0, 0, 10, 3 };
		Result.Crew[Electrician] = { "Marta", 3, 4, 0, 10, 4 };
		Result.Crew[Safecracker] = { "Otto", 2, 0, 4, 10, 2 };

		Result.Plan.Crew.resize(3);
		Result.Plan.Crew[Driver].Actions = {
			PlanAction::WaitForSignal(SafeOpen),
		};
		Result.Plan.Crew[Electrician].Actions = {
			PlanAction::MoveTo(BackDoorOutside),
			PlanAction::PickLock(BackDoor),
			PlanAction::MoveTo(Cellar),
			PlanAction::DisableAlarm(OfficeContact),
			PlanAction::Signal(AlarmOff),
			PlanAction::MoveTo(BackDoorInside),
			PlanAction::WaitForSignal(SafeOpen),
			PlanAction::MoveTo(Van),
		};
		Result.Plan.Crew[Safecracker].Actions = {
			PlanAction::MoveTo(BackDoorOutside),
			PlanAction::WaitForSignal(AlarmOff),
			PlanAction::MoveTo(Corridor),
			PlanAction::PickLock(OfficeDoor),
			PlanAction::MoveTo(Office),
			PlanAction::CrackSafe(WallSafe),
			PlanAction::Signal(SafeOpen),
			PlanAction::MoveTo(Van),
		};

		return Result;
	}
}
