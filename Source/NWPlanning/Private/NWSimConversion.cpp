#include "NWSimConversion.h"

#include "NWCrewMemberAsset.h"
#include "NWHeistObjectAsset.h"

namespace
{
	nw::sim::Tick SecondsToSimTicks(float Seconds)
	{
		return FMath::Max(0, FMath::RoundToInt(Seconds * static_cast<float>(nw::sim::TicksPerSecond)));
	}

	int32 SpeedToCmPerTick(float CmPerSecond)
	{
		return FMath::Max(1, FMath::RoundToInt(CmPerSecond / static_cast<float>(nw::sim::TicksPerSecond)));
	}

	std::string ToStdString(const FName& Name)
	{
		return std::string(TCHAR_TO_UTF8(*Name.ToString()));
	}

	std::string ToStdString(const FText& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text.ToString()));
	}
}

namespace NWSimConversion
{
	nw::sim::ObjectDefinition ToSim(const UNWHeistObjectAsset& Asset)
	{
		using namespace nw::sim;

		ObjectDefinition Out;
		Out.Name = ToStdString(Asset.DisplayName);
		Out.EntryNode = Asset.EntryNode;
		Out.TimeLimitTicks = SecondsToTicks(Asset.TimeLimitSeconds);

		for (const FNWNodeDef& Node : Asset.Nodes)
		{
			NodeDef Def;
			Def.Label = ToStdString(Node.Label);
			Def.Position = { Node.PositionCm.X, Node.PositionCm.Y };
			Def.RoomId = Node.RoomId;
			Out.Nodes.push_back(Def);
		}

		for (const FNWEdgeDef& Edge : Asset.Edges)
		{
			Out.Edges.push_back({ Edge.NodeA, Edge.NodeB, Edge.Door });
		}

		for (const FNWDoorDef& Door : Asset.Doors)
		{
			DoorDef Def;
			Def.Label = ToStdString(Door.Label);
			Def.Lock = static_cast<LockType>(Door.Lock);
			Def.bStartsLocked = Door.bStartsLocked;
			Def.Alarm = Door.Alarm;
			Def.OpenNoiseRadiusCm = Door.OpenNoiseRadiusCm;
			Out.Doors.push_back(Def);
		}

		for (const FNWAlarmDef& Alarm : Asset.Alarms)
		{
			AlarmDef Def;
			Def.Label = ToStdString(Alarm.Label);
			Def.Type = static_cast<AlarmType>(Alarm.Type);
			Def.PanelNode = Alarm.PanelNode;
			Def.GuardedNode = Alarm.GuardedNode;
			Def.BaseDisableSeconds = Alarm.BaseDisableSeconds;
			Def.PoliceResponseTicks = SecondsToTicks(Alarm.PoliceResponseSeconds);
			Out.Alarms.push_back(Def);
		}

		for (const FNWSafeDef& Safe : Asset.Safes)
		{
			SafeDef Def;
			Def.Label = ToStdString(Safe.Label);
			Def.Node = Safe.Node;
			Def.BaseCrackSeconds = Safe.BaseCrackSeconds;
			Def.LootValue = Safe.LootValue;
			Out.Safes.push_back(Def);
		}

		for (const FNWGuardDef& Guard : Asset.Guards)
		{
			GuardDef Def;
			Def.Name = ToStdString(Guard.Name);
			Def.StartNode = Guard.StartNode;
			Def.SpeedCmPerTick = SpeedToCmPerTick(Guard.SpeedCmPerSecond);
			Def.SightRangeCm = Guard.SightRangeCm;
			Def.ResponseTicks = SecondsToTicks(Guard.PoliceResponseSeconds);
			for (const FNWPatrolPoint& Point : Guard.Route)
			{
				const Tick WaitMin = SecondsToSimTicks(Point.WaitMinSeconds);
				const Tick WaitMax = FMath::Max(WaitMin, SecondsToSimTicks(Point.WaitMaxSeconds));
				Def.Route.push_back({ Point.Node, WaitMin, WaitMax });
			}
			Out.Guards.push_back(Def);
		}

		return Out;
	}

	nw::sim::CrewDef ToSim(const UNWCrewMemberAsset& Asset)
	{
		nw::sim::CrewDef Out;
		Out.Name = ToStdString(Asset.DisplayName);
		Out.Lockpicking = Asset.Lockpicking;
		Out.Electrics = Asset.Electrics;
		Out.Safecracking = Asset.Safecracking;
		Out.Nerves = Asset.Nerves;
		Out.SpeedCmPerTick = SpeedToCmPerTick(Asset.SpeedCmPerSecond);
		return Out;
	}

	nw::sim::HeistPlan ToSim(const TArray<FNWCrewPlan>& Plan)
	{
		nw::sim::HeistPlan Out;
		for (const FNWCrewPlan& Track : Plan)
		{
			nw::sim::CrewPlan& SimTrack = Out.Crew.emplace_back();
			for (const FNWPlanAction& Action : Track.Actions)
			{
				nw::sim::PlanAction SimAction;
				SimAction.Type = static_cast<nw::sim::ActionType>(Action.Type);
				SimAction.Target = Action.Target;
				SimAction.DurationTicks = SecondsToSimTicks(Action.DurationSeconds);
				SimTrack.Actions.push_back(SimAction);
			}
		}
		return Out;
	}
}
