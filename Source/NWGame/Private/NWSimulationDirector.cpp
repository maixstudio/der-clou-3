#include "NWSimulationDirector.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NWCrewMemberAsset.h"
#include "NWGame.h"
#include "NWHeistObjectAsset.h"
#include "NWPlanEditor.h"
#include "NWSimConversion.h"
#include "Sim/SimScenarios.h"

#include <utility>

namespace
{
	constexpr float SimStepSeconds = 1.f / static_cast<float>(nw::sim::TicksPerSecond);
	constexpr int32 MaxStepsPerFrame = 64;
}

ANWSimulationDirector::ANWSimulationDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANWSimulationDirector::BeginPlay()
{
	Super::BeginPlay();
	if (bStartInPlanner)
	{
		CreatePlanEditor();
		PrepareSimulation();
	}
	else if (bAutoStart)
	{
		StartSimulation();
	}
}

ANWSimulationDirector::FSimSetup ANWSimulationDirector::GatherSetup() const
{
	FSimSetup Setup;
	if (bUseBuiltInJewelerScenario || !HeistObject)
	{
		if (!bUseBuiltInJewelerScenario)
		{
			UE_LOG(LogNachtwerk, Warning, TEXT("%s: no HeistObject assigned, falling back to the built-in jeweler."), *GetName());
		}
		nw::sim::scenarios::Scenario BuiltIn = nw::sim::scenarios::MakeJewelerScenario();
		Setup.Object = std::move(BuiltIn.Object);
		Setup.Crew = std::move(BuiltIn.Crew);
		Setup.Plan = std::move(BuiltIn.Plan);
		return Setup;
	}

	Setup.Object = NWSimConversion::ToSim(*HeistObject);
	for (const TObjectPtr<UNWCrewMemberAsset>& Member : Crew)
	{
		if (Member)
		{
			Setup.Crew.push_back(NWSimConversion::ToSim(*Member));
		}
	}
	Setup.Plan = NWSimConversion::ToSim(Plan);
	return Setup;
}

TUniquePtr<nw::sim::Simulation> ANWSimulationDirector::CreateSimulation() const
{
	FSimSetup Setup = GatherSetup();
	if (bHasRuntimePlan)
	{
		Setup.Plan = NWSimConversion::ToSim(RuntimePlan);
	}
	const uint64_t SimSeed = static_cast<uint64_t>(static_cast<uint32>(Seed));
	return MakeUnique<nw::sim::Simulation>(std::move(Setup.Object), std::move(Setup.Crew), std::move(Setup.Plan), SimSeed);
}

bool ANWSimulationDirector::PrepareSimulation()
{
	Sim = CreateSimulation();
	Accumulator = 0.f;
	bPaused = false;
	bPlaying = false;
	bFinishHandled = false;
	LoggedEventCount = 0;

	if (!Sim->IsValid())
	{
		UE_LOG(LogNachtwerk, Error, TEXT("%s: invalid heist setup: %s"), *GetName(), UTF8_TO_TCHAR(Sim->GetError().c_str()));
		return false;
	}

	CapturePositions(CurrentPositions);
	PreviousPositions = CurrentPositions;
	ReportToPlanEditor();
	return true;
}

void ANWSimulationDirector::StartSimulation()
{
	if (!PrepareSimulation())
	{
		HandleFinished();
		return;
	}
	bPlaying = true;
	UE_LOG(LogNachtwerk, Log, TEXT("%s: simulation started (seed %d)."), *GetName(), Seed);
}

void ANWSimulationDirector::ExecutePlan(const TArray<FNWCrewPlan>& InPlan)
{
	RuntimePlan = InPlan;
	bHasRuntimePlan = true;
	StartSimulation();
}

void ANWSimulationDirector::StopSimulation()
{
	PrepareSimulation();
	UE_LOG(LogNachtwerk, Log, TEXT("%s: back to the planner."), *GetName());
}

void ANWSimulationDirector::SetPlaybackSpeed(float Speed)
{
	PlaybackSpeed = FMath::Clamp(Speed, 0.f, 32.f);
	ReportToPlanEditor();
}

FIntPoint ANWSimulationDirector::WorldToSim(const FVector& WorldPosition) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPosition);
	return FIntPoint(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y));
}

void ANWSimulationDirector::CreatePlanEditor()
{
	FSimSetup Setup = GatherSetup();
	TArray<FNWCrewPlan> EditablePlan = NWSimConversion::FromSim(Setup.Plan);

	PlanEditor = NewObject<UNWPlanEditor>(this, TEXT("PlanEditor"));
	PlanEditor->Initialize(std::move(Setup.Object), std::move(Setup.Crew), MoveTemp(EditablePlan), static_cast<uint64>(static_cast<uint32>(Seed)));
	PlanEditor->OnExecuteRequested.AddWeakLambda(this, [this]() { ExecutePlan(PlanEditor->GetPlan()); });
	PlanEditor->OnStopRequested.AddUObject(this, &ANWSimulationDirector::StopSimulation);
	PlanEditor->OnPlaybackSpeedRequested.AddUObject(this, &ANWSimulationDirector::SetPlaybackSpeed);
}

void ANWSimulationDirector::ReportToPlanEditor() const
{
	if (!PlanEditor || !Sim.IsValid())
	{
		return;
	}
	FNWExecutionState State;
	State.bActive = bPlaying;
	State.bFinished = Sim->IsFinished();
	State.SimulatedSeconds = GetSimulatedSeconds();
	State.PlaybackSpeed = PlaybackSpeed;
	State.Result = Sim->GetResult();
	PlanEditor->ReportExecution(State);
}

ENWHeistOutcome ANWSimulationDirector::RunGhostPreview(int32& OutLootValue, float& OutDurationSeconds) const
{
	TUniquePtr<nw::sim::Simulation> Ghost = Sim.IsValid() ? MakeUnique<nw::sim::Simulation>(*Sim) : CreateSimulation();
	const nw::sim::SimResult Result = Ghost->RunToEnd();

	OutLootValue = Result.LootValue;
	OutDurationSeconds = static_cast<float>(Result.EndTick) * SimStepSeconds;
	UE_LOG(LogNachtwerk, Log, TEXT("Ghost run: %s after %.1f s, loot %d"),
		UTF8_TO_TCHAR(nw::sim::Simulation::ToString(Result.Result)), OutDurationSeconds, OutLootValue);
	return ToOutcome(Result.Result);
}

void ANWSimulationDirector::SetSimulationPaused(bool bInPaused)
{
	bPaused = bInPaused;
}

bool ANWSimulationDirector::IsSimulationRunning() const
{
	return Sim.IsValid() && bPlaying && !Sim->IsFinished() && !bPaused;
}

float ANWSimulationDirector::GetSimulatedSeconds() const
{
	return Sim.IsValid() ? static_cast<float>(Sim->GetTick()) * SimStepSeconds : 0.f;
}

void ANWSimulationDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Sim.IsValid() || !Sim->IsValid())
	{
		return;
	}

	if (bPlaying && !bPaused && !Sim->IsFinished())
	{
		Accumulator += DeltaSeconds * PlaybackSpeed;
		int32 Steps = 0;
		while (Accumulator >= SimStepSeconds && !Sim->IsFinished() && Steps < MaxStepsPerFrame)
		{
			PreviousPositions = CurrentPositions;
			Sim->Step();
			CapturePositions(CurrentPositions);
			Accumulator -= SimStepSeconds;
			++Steps;
		}
		Accumulator = FMath::Min(Accumulator, SimStepSeconds);

		LogNewEvents();
		if (Sim->IsFinished())
		{
			HandleFinished();
		}
		ReportToPlanEditor();
	}

	if (bDrawDebug)
	{
		DrawDebugState(FMath::Clamp(Accumulator / SimStepSeconds, 0.f, 1.f));
	}
}

void ANWSimulationDirector::CapturePositions(TArray<FVector>& OutPositions) const
{
	OutPositions.Reset();
	for (const nw::sim::CrewState& State : Sim->GetCrewStates())
	{
		OutPositions.Add(ToWorld(Sim->GetPosition(State.Move), 60.f));
	}
	for (const nw::sim::GuardState& State : Sim->GetGuardStates())
	{
		OutPositions.Add(ToWorld(Sim->GetPosition(State.Move), 60.f));
	}
}

void ANWSimulationDirector::LogNewEvents()
{
	const std::vector<nw::sim::SimEvent>& Events = Sim->GetEvents();
	for (; LoggedEventCount < static_cast<int32>(Events.size()); ++LoggedEventCount)
	{
		const nw::sim::SimEvent& Event = Events[static_cast<size_t>(LoggedEventCount)];
		UE_LOG(LogNachtwerk, Log, TEXT("[%6.1fs] %-16s agent=%d target=%d"),
			static_cast<float>(Event.At) * SimStepSeconds, UTF8_TO_TCHAR(nw::sim::Simulation::ToString(Event.Type)), Event.Agent, Event.Target);
	}
}

void ANWSimulationDirector::HandleFinished()
{
	if (bFinishHandled)
	{
		return;
	}
	bFinishHandled = true;

	const nw::sim::SimResult& Result = Sim->GetResult();
	UE_LOG(LogNachtwerk, Log, TEXT("Heist finished: %s after %.1f s, loot %d"),
		UTF8_TO_TCHAR(nw::sim::Simulation::ToString(Result.Result)), static_cast<float>(Result.EndTick) * SimStepSeconds, Result.LootValue);
	OnHeistFinished.Broadcast(ToOutcome(Result.Result), Result.LootValue);
}

FVector ANWSimulationDirector::ToWorld(const nw::sim::IntVec2& Position, float Height) const
{
	return GetActorTransform().TransformPosition(FVector(static_cast<double>(Position.X), static_cast<double>(Position.Y), static_cast<double>(Height)));
}

void ANWSimulationDirector::DrawDebugState(float Alpha) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const nw::sim::ObjectDefinition& Def = Sim->GetDefinition();
	const std::vector<nw::sim::DoorState>& Doors = Sim->GetDoorStates();

	// Graph: nodes and edges. Doors are coloured by state: red = locked, yellow = closed, green = open.
	for (const nw::sim::NodeDef& Node : Def.Nodes)
	{
		const FVector Location = ToWorld(Node.Position);
		DrawDebugPoint(World, Location, 8.f, FColor::Silver);
		DrawDebugString(World, Location + FVector(0.0, 0.0, 30.0), UTF8_TO_TCHAR(Node.Label.c_str()), nullptr, FColor::White, 0.f, false, 0.8f);
	}
	for (const nw::sim::EdgeDef& Edge : Def.Edges)
	{
		const FVector A = ToWorld(Def.Nodes[static_cast<size_t>(Edge.A)].Position);
		const FVector B = ToWorld(Def.Nodes[static_cast<size_t>(Edge.B)].Position);
		FColor Color = FColor(90, 90, 110);
		if (Edge.Door != nw::sim::InvalidId)
		{
			const nw::sim::DoorState& Door = Doors[static_cast<size_t>(Edge.Door)];
			Color = Door.bLocked ? FColor::Red : (Door.bOpen ? FColor::Green : FColor::Yellow);
		}
		DrawDebugLine(World, A, B, Color, false, -1.f, 0, Edge.Door != nw::sim::InvalidId ? 6.f : 2.f);
	}

	// Safes: orange = closed, green = cracked.
	for (size_t Index = 0; Index < Def.Safes.size(); ++Index)
	{
		const bool bCracked = Sim->GetSafeStates()[Index].bCracked;
		DrawDebugBox(World, ToWorld(Def.Nodes[static_cast<size_t>(Def.Safes[Index].Node)].Position, 40.f), FVector(30.0), bCracked ? FColor::Green : FColor::Orange);
	}

	// Agents (interpolated).
	const int32 CrewCount = static_cast<int32>(Sim->GetCrewStates().size());
	for (int32 Index = 0; Index < CurrentPositions.Num(); ++Index)
	{
		const FVector From = PreviousPositions.IsValidIndex(Index) ? PreviousPositions[Index] : CurrentPositions[Index];
		const FVector Location = FMath::Lerp(From, CurrentPositions[Index], Alpha);
		if (Index < CrewCount)
		{
			const nw::sim::CrewState& State = Sim->GetCrewStates()[static_cast<size_t>(Index)];
			if (State.Status == nw::sim::CrewStatus::Escaped)
			{
				continue;
			}
			const FColor Color = State.Status == nw::sim::CrewStatus::Blocked ? FColor::Magenta : FColor(80, 160, 255);
			DrawDebugSphere(World, Location, 35.f, 12, Color);
			DrawDebugString(World, Location + FVector(0.0, 0.0, 60.0),
				UTF8_TO_TCHAR(Sim->GetCrewDefs()[static_cast<size_t>(Index)].Name.c_str()), nullptr, Color, 0.f, false, 1.f);
		}
		else
		{
			const size_t GuardIndex = static_cast<size_t>(Index - CrewCount);
			const nw::sim::GuardState& State = Sim->GetGuardStates()[GuardIndex];
			const FColor Color = State.Status == nw::sim::GuardStatus::Alerted ? FColor::Red
				: (State.Status == nw::sim::GuardStatus::Investigating ? FColor::Orange : FColor(255, 220, 120));
			DrawDebugSphere(World, Location, 35.f, 12, Color);
			DrawDebugCircle(World, Location, static_cast<float>(Def.Guards[GuardIndex].SightRangeCm), 48, Color,
				false, -1.f, 0, 1.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		}
	}

	// Planner: highlight what the pending action can target.
	if (PlanEditor && PlanEditor->IsPicking())
	{
		const FColor PickColor(0, 230, 230);
		for (const FNWPickCandidate& Candidate : PlanEditor->GetPickCandidates())
		{
			const FVector Location = ToWorld({ Candidate.PositionCm.X, Candidate.PositionCm.Y }, 5.f);
			DrawDebugCircle(World, Location, 55.f, 32, PickColor, false, -1.f, 0, 4.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		}
	}
}

ENWHeistOutcome ANWSimulationDirector::ToOutcome(nw::sim::Outcome Value)
{
	switch (Value)
	{
	case nw::sim::Outcome::Running: return ENWHeistOutcome::Running;
	case nw::sim::Outcome::CleanGetaway: return ENWHeistOutcome::CleanGetaway;
	case nw::sim::Outcome::EscapedWithAlarm: return ENWHeistOutcome::EscapedWithAlarm;
	case nw::sim::Outcome::Busted: return ENWHeistOutcome::Busted;
	case nw::sim::Outcome::TimedOut: return ENWHeistOutcome::TimedOut;
	case nw::sim::Outcome::InvalidSetup: return ENWHeistOutcome::InvalidSetup;
	}
	return ENWHeistOutcome::InvalidSetup;
}
