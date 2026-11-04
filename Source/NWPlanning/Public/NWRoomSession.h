#pragma once

#include "CoreMinimal.h"
#include "Sim/RoomSim.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"

#include "NWRoomSession.generated.h"

DECLARE_MULTICAST_DELEGATE(FNWOnRoomSessionChanged);

/**
 * "Plan by doing" for the room prototype: every command the player gives is executed right away
 * by the live simulation and recorded. The recorded commands are the plan: undo removes the last
 * one (the room snaps to the state without it), replay plays the whole plan again from the start.
 */
UCLASS()
class NWPLANNING_API UNWRoomSession : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const TArray<FString>& LayoutRows, const TArray<FText>& AgentNames);
	bool IsValid() const { return Room.IsValid(); }
	FString GetError() const { return UTF8_TO_TCHAR(Room.Error.c_str()); }

	const nw::room::Layout& GetLayout() const { return Room; }
	const nw::room::RoomSim& GetSim() const { return *Sim; }
	int32 GetAgentCount() const { return static_cast<int32>(Room.Agents.size()); }
	FText GetAgentName(int32 Agent) const;

	int32 GetSelectedAgent() const { return SelectedAgent; }
	void SelectAgent(int32 Agent);

	// --- Giving commands (executed immediately, appended to the plan) -----------------------

	void IssueMoveTo(const FIntPoint& Cell);
	void IssueOpeningCommand(nw::room::CommandType Type, int32 Opening);
	void IssueWait(float Seconds);

	/** What the selected agent can do at this door / window (given its queued commands). */
	TArray<nw::room::CommandType> GetOptions(int32 Opening) const;

	// --- Plan ---------------------------------------------------------------------------------

	int32 GetCommandCount() const { return Plan.Num(); }
	bool CanUndo() const { return Plan.Num() > 0; }
	/** Removes the last command; the room jumps to the state after the remaining plan. */
	bool Undo();
	/** Removes all commands and resets the room. */
	void ClearAll();
	/** Plays the whole plan again from the start. */
	void Replay();
	bool IsReplaying() const { return bReplaying; }

	void SetPlaybackSpeed(float Speed) { PlaybackSpeed = FMath::Clamp(Speed, 0.25f, 16.f); }
	float GetPlaybackSpeed() const { return PlaybackSpeed; }

	/** Advances the live simulation (call every frame). */
	void Tick(float DeltaSeconds);
	/** 0..1 between the last two simulation steps, for smooth rendering. */
	float GetInterpolationAlpha() const;
	float GetSeconds() const { return static_cast<float>(Sim->GetTick()) / static_cast<float>(nw::room::TicksPerSecond); }

	// --- Labels -------------------------------------------------------------------------------

	FText DescribeCommand(const nw::room::Command& Cmd) const;
	FText DescribeOption(nw::room::CommandType Type, int32 Opening) const;
	FText DescribeOpening(int32 Opening) const;
	/** What the agent is doing right now, e.g. "Schloss knacken" or "wartet auf Befehle". */
	FText DescribeActivity(int32 Agent) const;

	/** Broadcast when the plan changes (commands added, undone, cleared) or a replay starts. */
	FNWOnRoomSessionChanged OnChanged;

private:
	void Issue(const nw::room::Command& Cmd);
	void Rebuild(bool bRunToEnd);

	nw::room::Layout Room;
	TUniquePtr<nw::room::RoomSim> Sim;
	TArray<nw::room::Command> Plan;
	int32 SelectedAgent = 0;
	float PlaybackSpeed = 2.f;
	float Accumulator = 0.f;
	bool bReplaying = false;
};
