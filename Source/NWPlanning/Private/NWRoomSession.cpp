#include "NWRoomSession.h"

#define LOCTEXT_NAMESPACE "NWRoomSession"

namespace
{
	constexpr float StepSeconds = 1.f / static_cast<float>(nw::room::TicksPerSecond);
	constexpr int32 MaxStepsPerFrame = 64;
}

void UNWRoomSession::Initialize(const TArray<FString>& LayoutRows, const TArray<FText>& AgentNames)
{
	std::vector<std::string> Rows;
	for (const FString& Row : LayoutRows)
	{
		Rows.emplace_back(TCHAR_TO_UTF8(*Row));
	}
	Room = nw::room::ParseLayout(Rows);
	for (int32 Index = 0; Index < AgentNames.Num() && Index < GetAgentCount(); ++Index)
	{
		Room.Agents[static_cast<size_t>(Index)].Name = TCHAR_TO_UTF8(*AgentNames[Index].ToString());
	}
	Plan.Reset();
	SelectedAgent = 0;
	Rebuild(false);
}

FText UNWRoomSession::GetAgentName(int32 Agent) const
{
	return Agent >= 0 && Agent < GetAgentCount() ? FText::FromString(UTF8_TO_TCHAR(Room.Agents[static_cast<size_t>(Agent)].Name.c_str())) : FText::GetEmpty();
}

void UNWRoomSession::SelectAgent(int32 Agent)
{
	if (Agent >= 0 && Agent < GetAgentCount() && Agent != SelectedAgent)
	{
		SelectedAgent = Agent;
		OnChanged.Broadcast();
	}
}

void UNWRoomSession::Rebuild(bool bRunToEnd)
{
	Sim = MakeUnique<nw::room::RoomSim>(Room);
	for (const nw::room::Command& Cmd : Plan)
	{
		Sim->Enqueue(Cmd);
	}
	if (bRunToEnd)
	{
		Sim->RunUntilIdle();
	}
	Accumulator = 0.f;
}

void UNWRoomSession::Issue(const nw::room::Command& Cmd)
{
	if (!IsValid() || Cmd.Agent < 0 || Cmd.Agent >= GetAgentCount())
	{
		return;
	}
	Plan.Add(Cmd);
	Sim->Enqueue(Cmd);
	OnChanged.Broadcast();
}

void UNWRoomSession::IssueMoveTo(const FIntPoint& Cell)
{
	nw::room::Command Cmd;
	Cmd.Type = nw::room::CommandType::MoveTo;
	Cmd.Agent = SelectedAgent;
	Cmd.Target = { Cell.X, Cell.Y };
	Issue(Cmd);
}

void UNWRoomSession::IssueOpeningCommand(nw::room::CommandType Type, int32 Opening)
{
	nw::room::Command Cmd;
	Cmd.Type = Type;
	Cmd.Agent = SelectedAgent;
	Cmd.Opening = Opening;
	Issue(Cmd);
}

void UNWRoomSession::IssueWait(float Seconds)
{
	nw::room::Command Cmd;
	Cmd.Type = nw::room::CommandType::Wait;
	Cmd.Agent = SelectedAgent;
	Cmd.Duration = FMath::Max(1, FMath::RoundToInt(Seconds * static_cast<float>(nw::room::TicksPerSecond)));
	Issue(Cmd);
}

TArray<nw::room::CommandType> UNWRoomSession::GetOptions(int32 Opening) const
{
	TArray<nw::room::CommandType> Out;
	for (nw::room::CommandType Type : Sim->GetOptions(SelectedAgent, Opening))
	{
		Out.Add(Type);
	}
	return Out;
}

bool UNWRoomSession::Undo()
{
	if (Plan.IsEmpty())
	{
		return false;
	}
	Plan.Pop();
	bReplaying = false;
	Rebuild(true);
	OnChanged.Broadcast();
	return true;
}

void UNWRoomSession::ClearAll()
{
	Plan.Reset();
	bReplaying = false;
	Rebuild(false);
	OnChanged.Broadcast();
}

void UNWRoomSession::Replay()
{
	bReplaying = !Plan.IsEmpty();
	Rebuild(false);
	OnChanged.Broadcast();
}

void UNWRoomSession::Tick(float DeltaSeconds)
{
	if (!Sim.IsValid() || Sim->IsIdle())
	{
		Accumulator = 0.f;
		bReplaying = false;
		return;
	}
	Accumulator += DeltaSeconds * PlaybackSpeed;
	int32 Steps = 0;
	while (Accumulator >= StepSeconds && Steps < MaxStepsPerFrame && !Sim->IsIdle())
	{
		Sim->Step();
		Accumulator -= StepSeconds;
		++Steps;
	}
	Accumulator = FMath::Min(Accumulator, StepSeconds);
}

float UNWRoomSession::GetInterpolationAlpha() const
{
	return Sim.IsValid() && !Sim->IsIdle() ? FMath::Clamp(Accumulator / StepSeconds, 0.f, 1.f) : 1.f;
}

FText UNWRoomSession::DescribeOpening(int32 Opening) const
{
	if (Opening < 0 || Opening >= static_cast<int32>(Room.Openings.size()))
	{
		return FText::GetEmpty();
	}
	const nw::room::Opening& O = Sim->GetOpenings()[static_cast<size_t>(Opening)];
	if (O.Kind == nw::room::OpeningKind::Window)
	{
		return O.bOpen ? LOCTEXT("WindowOpen", "Fenster (offen)") : LOCTEXT("WindowClosed", "Fenster (zu)");
	}
	if (O.bLocked)
	{
		return LOCTEXT("DoorLocked", "Tür (verschlossen)");
	}
	return O.bOpen ? LOCTEXT("DoorOpen", "Tür (offen)") : LOCTEXT("DoorClosed", "Tür (zu)");
}

FText UNWRoomSession::DescribeOption(nw::room::CommandType Type, int32 Opening) const
{
	const bool bWindow = Opening >= 0 && Opening < static_cast<int32>(Room.Openings.size())
		&& Room.Openings[static_cast<size_t>(Opening)].Kind == nw::room::OpeningKind::Window;
	switch (Type)
	{
	case nw::room::CommandType::MoveTo: return LOCTEXT("OptMove", "Hingehen");
	case nw::room::CommandType::Open: return bWindow ? LOCTEXT("OptOpenWindow", "Fenster öffnen") : LOCTEXT("OptOpenDoor", "Tür öffnen");
	case nw::room::CommandType::Close: return bWindow ? LOCTEXT("OptCloseWindow", "Fenster schließen") : LOCTEXT("OptCloseDoor", "Tür schließen");
	case nw::room::CommandType::PickLock: return LOCTEXT("OptPick", "Schloss knacken");
	case nw::room::CommandType::GoThrough: return LOCTEXT("OptThrough", "Durchgehen");
	case nw::room::CommandType::ClimbThrough: return LOCTEXT("OptClimb", "Durchsteigen");
	case nw::room::CommandType::Wait: return LOCTEXT("OptWait", "Warten");
	}
	return FText::GetEmpty();
}

FText UNWRoomSession::DescribeCommand(const nw::room::Command& Cmd) const
{
	if (Cmd.Type == nw::room::CommandType::Wait)
	{
		return FText::Format(LOCTEXT("CmdWait", "Warten {0} s"), FText::AsNumber(Cmd.Duration / nw::room::TicksPerSecond));
	}
	if (Cmd.Type == nw::room::CommandType::MoveTo)
	{
		return LOCTEXT("CmdMove", "Gehen");
	}
	return DescribeOption(Cmd.Type, Cmd.Opening);
}

FText UNWRoomSession::DescribeActivity(int32 Agent) const
{
	if (Agent < 0 || Agent >= GetAgentCount())
	{
		return FText::GetEmpty();
	}
	const nw::room::AgentState& State = Sim->GetAgents()[static_cast<size_t>(Agent)];
	if (State.Current < 0)
	{
		return LOCTEXT("Idle", "wartet auf deinen Befehl");
	}
	const nw::room::CommandRecord& Record = Sim->GetRecords()[static_cast<size_t>(State.Current)];
	const FText What = DescribeCommand(Record.Cmd);
	if (State.Phase == nw::room::AgentPhase::Approach && Record.Cmd.Type != nw::room::CommandType::MoveTo)
	{
		return FText::Format(LOCTEXT("Approach", "geht hin: {0}"), What);
	}
	if (State.Phase == nw::room::AgentPhase::Act && State.Duration > 0)
	{
		const int32 Remaining = FMath::Max(0, (State.Duration - State.Progress + nw::room::TicksPerSecond - 1) / nw::room::TicksPerSecond);
		return FText::Format(LOCTEXT("Acting", "{0} … noch {1} s"), What, FText::AsNumber(Remaining));
	}
	return What;
}

#undef LOCTEXT_NAMESPACE
