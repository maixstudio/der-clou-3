#include "Sim/SimDefinition.h"

namespace nw::sim
{
	bool ValidateDefinition(const ObjectDefinition& Definition, std::string& OutError)
	{
		const int32_t NodeCount = static_cast<int32_t>(Definition.Nodes.size());
		const int32_t DoorCount = static_cast<int32_t>(Definition.Doors.size());
		const int32_t AlarmCount = static_cast<int32_t>(Definition.Alarms.size());
		const auto IsNode = [NodeCount](NodeId Id) { return Id >= 0 && Id < NodeCount; };

		if (NodeCount == 0)
		{
			OutError = "object has no nodes";
			return false;
		}
		if (!IsNode(Definition.EntryNode))
		{
			OutError = "entry node is invalid";
			return false;
		}
		if (Definition.TimeLimitTicks <= 0)
		{
			OutError = "time limit must be positive";
			return false;
		}

		std::vector<int32_t> DoorUse(Definition.Doors.size(), 0);
		for (size_t EdgeIndex = 0; EdgeIndex < Definition.Edges.size(); ++EdgeIndex)
		{
			const EdgeDef& Edge = Definition.Edges[EdgeIndex];
			if (!IsNode(Edge.A) || !IsNode(Edge.B) || Edge.A == Edge.B)
			{
				OutError = "edge " + std::to_string(EdgeIndex) + " references invalid nodes";
				return false;
			}
			if (Edge.Door != InvalidId)
			{
				if (Edge.Door < 0 || Edge.Door >= DoorCount)
				{
					OutError = "edge " + std::to_string(EdgeIndex) + " references an invalid door";
					return false;
				}
				if (++DoorUse[static_cast<size_t>(Edge.Door)] > 1)
				{
					OutError = "door " + std::to_string(Edge.Door) + " is used by more than one edge";
					return false;
				}
			}
		}

		for (size_t DoorIndex = 0; DoorIndex < Definition.Doors.size(); ++DoorIndex)
		{
			const DoorDef& Door = Definition.Doors[DoorIndex];
			if (Door.Alarm != InvalidId && (Door.Alarm < 0 || Door.Alarm >= AlarmCount))
			{
				OutError = "door " + std::to_string(DoorIndex) + " references an invalid alarm";
				return false;
			}
			if (Door.OpenNoiseRadiusCm < 0)
			{
				OutError = "door " + std::to_string(DoorIndex) + " has a negative noise radius";
				return false;
			}
		}

		for (size_t AlarmIndex = 0; AlarmIndex < Definition.Alarms.size(); ++AlarmIndex)
		{
			const AlarmDef& Alarm = Definition.Alarms[AlarmIndex];
			if (!IsNode(Alarm.PanelNode))
			{
				OutError = "alarm " + std::to_string(AlarmIndex) + " has an invalid panel node";
				return false;
			}
			if (Alarm.Type == AlarmType::LightBarrier && !IsNode(Alarm.GuardedNode))
			{
				OutError = "light barrier " + std::to_string(AlarmIndex) + " has an invalid guarded node";
				return false;
			}
			if (Alarm.BaseDisableSeconds <= 0 || Alarm.PoliceResponseTicks <= 0)
			{
				OutError = "alarm " + std::to_string(AlarmIndex) + " needs positive durations";
				return false;
			}
		}

		for (size_t SafeIndex = 0; SafeIndex < Definition.Safes.size(); ++SafeIndex)
		{
			const SafeDef& Safe = Definition.Safes[SafeIndex];
			if (!IsNode(Safe.Node) || Safe.BaseCrackSeconds <= 0)
			{
				OutError = "safe " + std::to_string(SafeIndex) + " is invalid";
				return false;
			}
		}

		for (size_t GuardIndex = 0; GuardIndex < Definition.Guards.size(); ++GuardIndex)
		{
			const GuardDef& Guard = Definition.Guards[GuardIndex];
			if (!IsNode(Guard.StartNode) || Guard.SpeedCmPerTick <= 0 || Guard.SightRangeCm < 0 || Guard.ResponseTicks <= 0)
			{
				OutError = "guard " + std::to_string(GuardIndex) + " is invalid";
				return false;
			}
			for (const PatrolPoint& Point : Guard.Route)
			{
				if (!IsNode(Point.Node) || Point.WaitMinTicks < 0 || Point.WaitMaxTicks < Point.WaitMinTicks)
				{
					OutError = "guard " + std::to_string(GuardIndex) + " has an invalid patrol point";
					return false;
				}
			}
		}

		return true;
	}
}
