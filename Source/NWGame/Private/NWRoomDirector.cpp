#include "NWRoomDirector.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NWGame.h"
#include "NWRoomAgent.h"
#include "NWRoomSession.h"
#include "Sim/RoomSim.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float Cell = static_cast<float>(nw::room::CellCm);
	constexpr float DoorHeight = 220.f;
	constexpr float SillHeight = 90.f;
	constexpr float PanelThickness = 6.f;
	constexpr float GlassThickness = 4.f;
	constexpr float OpenSpeed = 4.f;

	const FLinearColor AgentColors[] = {
		FLinearColor(1.f, 0.42f, 0.05f),
		FLinearColor(0.1f, 0.55f, 1.f),
		FLinearColor(0.85f, 0.15f, 0.65f),
		FLinearColor(0.2f, 0.75f, 0.3f),
	};

	UInstancedStaticMeshComponent* MakeInstances(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Component = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetupAttachment(Parent);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return Component;
	}

	FTransform Box(const FVector& Centre, const FVector& SizeCm, float Yaw = 0.f)
	{
		return FTransform(FRotator(0.0, Yaw, 0.0), Centre, SizeCm / 100.0);
	}
}

ANWRoomDirector::ANWRoomDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = ShapeMaterial.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	FloorTiles = MakeInstances(this, Root, TEXT("FloorTiles"), CubeMesh.Object);
	GroundTiles = MakeInstances(this, Root, TEXT("GroundTiles"), CubeMesh.Object);
	Walls = MakeInstances(this, Root, TEXT("Walls"), CubeMesh.Object);
	DoorPanels = MakeInstances(this, Root, TEXT("DoorPanels"), CubeMesh.Object);
	Locks = MakeInstances(this, Root, TEXT("Locks"), CubeMesh.Object);
	WindowPanes = MakeInstances(this, Root, TEXT("WindowPanes"), CubeMesh.Object);
	// Readability first: the room itself casts no shadows, only the figures do.
	for (UInstancedStaticMeshComponent* Mesh : { FloorTiles.Get(), GroundTiles.Get(), Walls.Get(), DoorPanels.Get(), Locks.Get(), WindowPanes.Get() })
	{
		Mesh->SetCastShadow(false);
	}

	// A single room: two windows at the back, a locked door on the right, the entrance at the front.
	Layout = {
		TEXT(",,,,,,,,,,,,,,,,,,,,,,,,,,"),
		TEXT(",,,,,,,,,,,,,,,,,,,,,,,,,,"),
		TEXT(",,######WW######WW######,,"),
		TEXT(",,#....................#,,"),
		TEXT(",,#....................#,,"),
		TEXT(",,#....................#,,"),
		TEXT(",,#....................L,,"),
		TEXT(",,#....................L,,"),
		TEXT(",,#....................#,,"),
		TEXT(",,#....1...............#,,"),
		TEXT(",,#....................#,,"),
		TEXT(",,#....................#,,"),
		TEXT(",,#########DD###########,,"),
		TEXT(",,,,,,,,,,,,,,,,,,,,,,,,,,"),
		TEXT(",,,,,,,,,,,,,,,,,,,,,,,,,,"),
	};
	CrewNames = { FText::FromString(TEXT("Vic")) };
}

void ANWRoomDirector::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildGeometry();
}

void ANWRoomDirector::BeginPlay()
{
	Super::BeginPlay();

	Session = NewObject<UNWRoomSession>(this, TEXT("RoomSession"));
	Session->Initialize(Layout, CrewNames);
	if (!Session->IsValid())
	{
		UE_LOG(LogNachtwerk, Error, TEXT("%s: invalid room layout: %s"), *GetName(), *Session->GetError());
		return;
	}
	BuildGeometry();
	SpawnAgents();
	UpdateOpenings(0.f, true);
}

void ANWRoomDirector::ApplyColor(UInstancedStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!BaseMaterial)
	{
		return;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Mesh->SetMaterial(0, Instance);
}

FVector ANWRoomDirector::CellToLocal(int32 X, int32 Y, float Height) const
{
	return FVector((static_cast<float>(LayoutHeight - 1 - Y) + 0.5f) * Cell, (static_cast<float>(X) + 0.5f) * Cell, Height);
}

FVector ANWRoomDirector::SimToLocal(int32 PosX, int32 PosY) const
{
	return FVector(static_cast<float>(LayoutHeight) * Cell - static_cast<float>(PosY), static_cast<float>(PosX), 0.f);
}

FVector ANWRoomDirector::CellToWorld(const FIntPoint& InCell, float Height) const
{
	return GetActorTransform().TransformPosition(CellToLocal(InCell.X, InCell.Y, Height));
}

bool ANWRoomDirector::WorldToCell(const FVector& WorldPosition, FIntPoint& OutCell) const
{
	if (!Session || !Session->IsValid())
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPosition);
	OutCell.X = FMath::FloorToInt(Local.Y / Cell);
	OutCell.Y = LayoutHeight - 1 - FMath::FloorToInt(Local.X / Cell);
	return Session->GetLayout().Contains({ OutCell.X, OutCell.Y });
}

int32 ANWRoomDirector::FindAgent(const AActor* Actor) const
{
	const ANWRoomAgent* Agent = Cast<ANWRoomAgent>(Actor);
	return Agent && Agent->GetOwner() == this ? Agent->GetAgentIndex() : INDEX_NONE;
}

FVector ANWRoomDirector::GetAgentLocation(int32 Agent) const
{
	return Agents.IsValidIndex(Agent) && Agents[Agent] ? Agents[Agent]->GetActorLocation() : GetActorLocation();
}

void ANWRoomDirector::BuildGeometry()
{
	std::vector<std::string> Rows;
	for (const FString& Row : Layout)
	{
		Rows.emplace_back(TCHAR_TO_UTF8(*Row));
	}
	const nw::room::Layout Room = nw::room::ParseLayout(Rows);

	for (UInstancedStaticMeshComponent* Mesh : { FloorTiles.Get(), GroundTiles.Get(), Walls.Get(), DoorPanels.Get(), Locks.Get(), WindowPanes.Get() })
	{
		Mesh->ClearInstances();
	}
	OpeningVisuals.Reset();
	if (!Room.IsValid())
	{
		return;
	}
	LayoutHeight = Room.Height;

	ApplyColor(FloorTiles, FloorColor);
	ApplyColor(GroundTiles, GroundColor);
	ApplyColor(Walls, WallColor);
	ApplyColor(DoorPanels, DoorColor);
	ApplyColor(Locks, FLinearColor(0.85f, 0.05f, 0.05f));
	ApplyColor(WindowPanes, GlassColor);

	const FVector Tile(Cell, Cell, 10.f);
	for (int32 Y = 0; Y < Room.Height; ++Y)
	{
		for (int32 X = 0; X < Room.Width; ++X)
		{
			const nw::room::CellType Type = Room.TypeAt({ X, Y });
			switch (Type)
			{
			case nw::room::CellType::Floor:
				FloorTiles->AddInstance(Box(CellToLocal(X, Y, -5.f), Tile));
				break;
			case nw::room::CellType::Ground:
				GroundTiles->AddInstance(Box(CellToLocal(X, Y, -5.f), Tile));
				break;
			case nw::room::CellType::Wall:
				Walls->AddInstance(Box(CellToLocal(X, Y, WallHeight * 0.5f), FVector(Cell, Cell, WallHeight)));
				break;
			case nw::room::CellType::Door:
				FloorTiles->AddInstance(Box(CellToLocal(X, Y, -5.f), Tile));
				Walls->AddInstance(Box(CellToLocal(X, Y, (DoorHeight + WallHeight) * 0.5f), FVector(Cell, Cell, WallHeight - DoorHeight)));
				break;
			case nw::room::CellType::Window:
				FloorTiles->AddInstance(Box(CellToLocal(X, Y, -5.f), Tile));
				Walls->AddInstance(Box(CellToLocal(X, Y, SillHeight * 0.5f), FVector(Cell, Cell, SillHeight)));
				Walls->AddInstance(Box(CellToLocal(X, Y, (DoorHeight + WallHeight) * 0.5f), FVector(Cell, Cell, WallHeight - DoorHeight)));
				break;
			default:
				break;
			}
		}
	}

	for (int32 Index = 0; Index < static_cast<int32>(Room.Openings.size()); ++Index)
	{
		const nw::room::Opening& O = Room.Openings[static_cast<size_t>(Index)];
		FOpeningVisual& Visual = OpeningVisuals.AddDefaulted_GetRef();
		Visual.Opening = Index;
		Visual.bDoor = O.Kind == nw::room::OpeningKind::Door;
		Visual.Length = static_cast<float>(O.Cells.size()) * Cell;

		const nw::room::Cell First = O.Cells.front();
		if (O.bAlongX)
		{
			// Wall runs along local Y: hinge at the low-Y end, panel pointing along +Y.
			Visual.Anchor = FVector((static_cast<float>(LayoutHeight - 1 - First.Y) + 0.5f) * Cell, static_cast<float>(First.X) * Cell, 0.f);
			Visual.ClosedYaw = 0.f;
		}
		else
		{
			// Wall runs along local X (grid Y grows towards -X): panel pointing along -X.
			Visual.Anchor = FVector(static_cast<float>(LayoutHeight - First.Y) * Cell, (static_cast<float>(First.X) + 0.5f) * Cell, 0.f);
			Visual.ClosedYaw = 90.f;
		}

		if (Visual.bDoor)
		{
			Visual.Instance = DoorPanels->AddInstance(FTransform::Identity);
			Visual.LockInstance = Locks->AddInstance(FTransform::Identity);
		}
		else
		{
			Visual.Instance = WindowPanes->AddInstance(FTransform::Identity);
		}
	}
	UpdateOpenings(0.f, true);
}

void ANWRoomDirector::UpdateOpenings(float DeltaSeconds, bool bSnap)
{
	const nw::room::RoomSim* Sim = Session && Session->IsValid() ? &Session->GetSim() : nullptr;

	std::vector<std::string> Rows;
	nw::room::Layout EditorRoom;
	if (!Sim)
	{
		for (const FString& Row : Layout)
		{
			Rows.emplace_back(TCHAR_TO_UTF8(*Row));
		}
		EditorRoom = nw::room::ParseLayout(Rows);
	}
	const std::vector<nw::room::Opening>& Openings = Sim ? Sim->GetOpenings() : EditorRoom.Openings;

	for (FOpeningVisual& Visual : OpeningVisuals)
	{
		if (Visual.Opening < 0 || static_cast<size_t>(Visual.Opening) >= Openings.size())
		{
			continue;
		}
		const nw::room::Opening& O = Openings[static_cast<size_t>(Visual.Opening)];
		const float Target = O.bOpen ? 1.f : 0.f;
		Visual.OpenAmount = bSnap ? Target : FMath::FInterpConstantTo(Visual.OpenAmount, Target, DeltaSeconds, OpenSpeed);

		const FRotator ClosedRotation(0.0, Visual.ClosedYaw, 0.0);
		if (Visual.bDoor)
		{
			const float Yaw = Visual.ClosedYaw + 90.f * Visual.OpenAmount;
			const FVector Centre = Visual.Anchor + FRotator(0.0, Yaw, 0.0).RotateVector(FVector(0.0, Visual.Length * 0.5, DoorHeight * 0.5));
			DoorPanels->UpdateInstanceTransform(Visual.Instance, Box(Centre, FVector(PanelThickness, Visual.Length - 4.f, DoorHeight - 4.f), Yaw), false, true, true);

			const FVector LockCentre = Visual.Anchor + ClosedRotation.RotateVector(FVector(0.0, Visual.Length * 0.5, 110.0));
			const FVector LockSize = O.bLocked ? FVector(16.f, 16.f, 16.f) : FVector(0.01f);
			Locks->UpdateInstanceTransform(Visual.LockInstance, Box(LockCentre, LockSize, Visual.ClosedYaw), false, true, true);
		}
		else
		{
			// Windows slide up into the lintel.
			const FVector Centre = Visual.Anchor + ClosedRotation.RotateVector(FVector(0.0, Visual.Length * 0.5, 0.0))
				+ FVector(0.0, 0.0, (SillHeight + DoorHeight) * 0.5 + 110.0 * Visual.OpenAmount);
			WindowPanes->UpdateInstanceTransform(Visual.Instance, Box(Centre, FVector(GlassThickness, Visual.Length - 2.f, DoorHeight - SillHeight - 2.f), Visual.ClosedYaw), false, true, true);
		}
	}
}

void ANWRoomDirector::SpawnAgents()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const nw::room::RoomSim& Sim = Session->GetSim();
	for (int32 Index = 0; Index < Session->GetAgentCount(); ++Index)
	{
		const nw::room::AgentState& State = Sim.GetAgents()[static_cast<size_t>(Index)];
		const FVector Location = GetActorTransform().TransformPosition(SimToLocal(State.PosX, State.PosY));
		FActorSpawnParameters Params;
		Params.Owner = this;
		ANWRoomAgent* Agent = World->SpawnActor<ANWRoomAgent>(Location, GetActorRotation(), Params);
		if (!Agent)
		{
			continue;
		}
		Agent->Setup(Index, Session->GetAgentName(Index), AgentColors[Index % UE_ARRAY_COUNT(AgentColors)]);
		Agents.Add(Agent);
		PreviousAgentLocations.Add(Location);
		CurrentAgentLocations.Add(Location);
	}
	LastSimTick = Sim.GetTick();
}

void ANWRoomDirector::UpdateAgents(float DeltaSeconds)
{
	const nw::room::RoomSim& Sim = Session->GetSim();
	const int32 SimTick = Sim.GetTick();
	const bool bContinuous = SimTick == LastSimTick + 1;
	if (SimTick != LastSimTick)
	{
		for (int32 Index = 0; Index < Agents.Num(); ++Index)
		{
			const nw::room::AgentState& State = Sim.GetAgents()[static_cast<size_t>(Index)];
			const FVector Location = GetActorTransform().TransformPosition(SimToLocal(State.PosX, State.PosY));
			// Undo / replay jump in time: no interpolation across the jump.
			PreviousAgentLocations[Index] = bContinuous ? CurrentAgentLocations[Index] : Location;
			CurrentAgentLocations[Index] = Location;
		}
		LastSimTick = SimTick;
	}

	const float Alpha = Session->GetInterpolationAlpha();
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		if (ANWRoomAgent* Agent = Agents[Index])
		{
			Agent->UpdatePose(FMath::Lerp(PreviousAgentLocations[Index], CurrentAgentLocations[Index], Alpha), DeltaSeconds);
			Agent->SetSelected(Index == Session->GetSelectedAgent());
		}
	}
}

void ANWRoomDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Session || !Session->IsValid())
	{
		return;
	}
	Session->Tick(DeltaSeconds);
	UpdateAgents(DeltaSeconds);
	UpdateOpenings(DeltaSeconds, false);
}
