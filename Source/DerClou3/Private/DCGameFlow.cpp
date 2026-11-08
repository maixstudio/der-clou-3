#include "DCGameFlow.h"

#include "DCHeistState.h"
#include "DCLoadingScreen.h"
#include "DCPlan.h"
#include "DerClou3.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

#define LOCTEXT_NAMESPACE "DCGameFlow"

namespace
{
	/** The loading screen stays at least this long, so it does not flash. */
	constexpr double LoadingMinSeconds = 1.0;
	/** ... and at most this long after the map is there (then the game goes on anyway). */
	constexpr double LoadingMaxSeconds = 90.0;
	constexpr double LoadingFadeSeconds = 0.35;
	/** Autosave this long after the last plan change. */
	constexpr double AutosaveDelay = 1.0;
	const TCHAR* const SelfTestSlotPrefix = TEXT("DCSelfTest_");

	FText HeistKindText(const FDCHeistInfo* Heist)
	{
		return Heist && Heist->bPractice ? LOCTEXT("KindPractice", "Übung") : LOCTEXT("KindHeist", "Einbruch");
	}
}

UDCGameFlowSubsystem* UDCGameFlowSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDCGameFlowSubsystem>() : nullptr;
}

FText UDCGameFlowSubsystem::GetGameTitle()
{
	return LOCTEXT("GameTitle", "Lindenhof");
}

const TArray<FDCHeistInfo>& UDCGameFlowSubsystem::GetHeists()
{
	static const TArray<FDCHeistInfo> Heists = {
		{ TEXT("villa"), LOCTEXT("VillaName", "Villa Lindenhof"),
			LOCTEXT("VillaText", "Großes Grundstück mit Remise, Erdgeschoss und Keller. Im Keller liegt der Tresorraum."),
			TEXT("/Game/DerClou/Maps/L_Villa01"), false },
		{ TEXT("house"), LOCTEXT("HouseName", "Testhaus"),
			LOCTEXT("HouseText", "Garten, Flur, Büro, Schlafzimmer – zum Üben von Türen, Wegen und Abstimmung."),
			TEXT("/Game/DerClou/Maps/L_House01"), true },
		{ TEXT("room"), LOCTEXT("RoomName", "Testraum"),
			LOCTEXT("RoomText", "Ein Raum voller Möbel – zum Üben von Inventar, Werkzeug und Lärm."),
			TEXT("/Game/DerClou/Maps/L_Room01"), true }
	};
	return Heists;
}

const FDCHeistInfo* UDCGameFlowSubsystem::FindHeist(FName Id)
{
	return GetHeists().FindByPredicate([Id](const FDCHeistInfo& Heist) { return Heist.Id == Id; });
}

const FDCHeistInfo* UDCGameFlowSubsystem::FindHeistByMap(const FString& Map)
{
	return GetHeists().FindByPredicate([&Map](const FDCHeistInfo& Heist) { return Heist.Map == Map; });
}

const TCHAR* UDCGameFlowSubsystem::MenuMap()
{
	return TEXT("/Game/DerClou/Maps/L_Villa01");
}

void UDCGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UDCPlanSubsystem>();
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), TEXT("DCSelfTest="), SelfTest);
	bScreens = SelfTest.IsEmpty() || SelfTest == TEXT("Menu");
	LoadingState = MakeShared<FDCLoadingState>();
	if (UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>())
	{
		PlanChangedHandle = Plans->OnPlanChanged.AddUObject(this, &UDCGameFlowSubsystem::OnPlanChanged);
	}
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UDCGameFlowSubsystem::OnPreLoadMap);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDCGameFlowSubsystem::Tick));
	if (SelfTest == TEXT("Menu"))
	{
		StartMenuSelfTest();
	}
}

void UDCGameFlowSubsystem::Deinitialize()
{
	FlushAutosave();
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	if (UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>())
	{
		Plans->OnPlanChanged.Remove(PlanChangedHandle);
	}
	HideOverlay();
	LoadingWidget.Reset();
	Super::Deinitialize();
}

bool UDCGameFlowSubsystem::DecideMenu(UWorld* World, const FString& Options)
{
	const FString Map = World ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString();
	const bool bFirst = !bBooted;
	bBooted = true;
	bRedirectToMenu = false;
	bMenu = UGameplayStatics::HasOption(Options, TEXT("menu"));
	if (!bMenu && bFirst && bScreens)
	{
		// The first map of a session shows the start menu; it lives on the villa.
		bMenu = true;
		bRedirectToMenu = Map != MenuMap();
	}
	const FDCHeistInfo* Heist = FindHeistByMap(Map);
	CurrentHeist = !bMenu && Heist ? Heist->Id : NAME_None;
	if (bMenu)
	{
		// Nothing of a plan in the menu scene (markers, ghosts, doors in their planned state).
		TGuardValue<bool> Guard(bApplyingPlan, true);
		if (UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>())
		{
			Plans->ResetForNewHeist();
		}
		SavedSignature = PlanSignature();
	}
	UE_LOG(LogDerClou, Display, TEXT("Game flow: %s %s%s"), *Map, bMenu ? TEXT("as start menu") : *FString::Printf(TEXT("heist %s"), *CurrentHeist.ToString()),
		bRedirectToMenu ? TEXT(" (on to the menu)") : TEXT(""));

	// The loading screen covers the new world until it can be used (also the very first one in the editor).
	if (bScreens)
	{
		if (Phase == EPhase::Hidden || Phase == EPhase::FadingOut)
		{
			ShowOverlay(bMenu ? LOCTEXT("MenuTitle", "Hauptmenü") : Heist ? Heist->Name : FText::FromString(FPaths::GetBaseFilename(Map)),
				bMenu ? LOCTEXT("MenuKind", "Start") : HeistKindText(Heist));
		}
		Phase = EPhase::Preparing;
		PhaseStart = FPlatformTime::Seconds();
		FramesPrepared = 0;
		LoadingState->Progress = FMath::Max(LoadingState->Progress, 0.4f);
	}
	return bMenu;
}

void UDCGameFlowSubsystem::OpenMainMenu()
{
	FlushAutosave();
	{
		TGuardValue<bool> Guard(bApplyingPlan, true);
		if (UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>())
		{
			Plans->ResetForNewHeist();
		}
	}
	SavedSignature = PlanSignature();
	TravelTo(MenuMap(), TEXT("menu"), LOCTEXT("MenuTitle", "Hauptmenü"), LOCTEXT("MenuKind", "Start"));
}

void UDCGameFlowSubsystem::StartHeist(FName HeistId)
{
	const FDCHeistInfo* Heist = FindHeist(HeistId);
	if (!Heist)
	{
		return;
	}
	FlushAutosave();
	{
		TGuardValue<bool> Guard(bApplyingPlan, true);
		if (UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>())
		{
			Plans->ResetForNewHeist();
		}
	}
	SavedSignature = PlanSignature();
	UE_LOG(LogDerClou, Display, TEXT("Game flow: new plan for %s"), *Heist->Id.ToString());
	TravelTo(Heist->Map, FString(), Heist->Name, HeistKindText(Heist));
}

bool UDCGameFlowSubsystem::LoadSave(const FString& Slot)
{
	const UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	const FDCHeistInfo* Heist = Save ? FindHeist(Save->Heist) : nullptr;
	if (Save && !Heist)
	{
		Heist = FindHeistByMap(Save->Map);
	}
	if (!Heist)
	{
		UE_LOG(LogDerClou, Warning, TEXT("Game flow: save %s cannot be loaded"), *Slot);
		return false;
	}
	FlushAutosave();
	{
		TGuardValue<bool> Guard(bApplyingPlan, true);
		if (UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>())
		{
			Plans->SetPlans(Save->Plans);
			Plans->SetSpeed(Save->Speed);
		}
	}
	SavedSignature = PlanSignature();
	UE_LOG(LogDerClou, Display, TEXT("Game flow: loaded %s (%s)"), *Slot, *Heist->Id.ToString());
	TravelTo(Heist->Map, FString(), Heist->Name, HeistKindText(Heist));
	return true;
}

void UDCGameFlowSubsystem::ReloadCurrentMap(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	const FDCHeistInfo* Heist = FindHeistByMap(Map);
	TravelTo(Map, FString(), Heist ? Heist->Name : FText::FromString(FPaths::GetBaseFilename(Map)), HeistKindText(Heist));
}

void UDCGameFlowSubsystem::QuitGame(const UObject* WorldContext)
{
	FlushAutosave();
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UKismetSystemLibrary::QuitGame(World, World ? World->GetFirstPlayerController() : nullptr, EQuitPreference::Quit, false);
}

bool UDCGameFlowSubsystem::IsLoading() const
{
	return Phase != EPhase::Hidden;
}

void UDCGameFlowSubsystem::TravelTo(const FString& Map, const FString& Options, const FText& Title, const FText& Kind)
{
	FlushAutosave();
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World)
	{
		return;
	}
	UGameplayStatics::SetGamePaused(World, false);
	UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	UE_LOG(LogDerClou, Display, TEXT("Game flow: travel to %s%s%s"), *Map, Options.IsEmpty() ? TEXT("") : TEXT("?"), *Options);
	if (!bScreens)
	{
		UGameplayStatics::OpenLevel(World, FName(*Map), true, Options);
		return;
	}
	ShowOverlay(Title, Kind);
	PendingMap = Map;
	PendingOptions = Options;
	// Loading blocks the screen: draw the loading screen first, then open the map.
	FramesToWait = 3;
	Phase = EPhase::Travelling;
	PhaseStart = FPlatformTime::Seconds();
}

void UDCGameFlowSubsystem::OnPreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance() || !bScreens)
	{
		return;
	}
	// Loads the engine starts itself (the first map) get the loading screen too.
	if (Phase == EPhase::Hidden || Phase == EPhase::FadingOut)
	{
		const FDCHeistInfo* Heist = FindHeistByMap(UWorld::RemovePIEPrefix(MapName));
		ShowOverlay(Heist ? Heist->Name : GetGameTitle(), Heist ? HeistKindText(Heist) : LOCTEXT("MenuKind", "Start"));
	}
	Phase = EPhase::Loading;
	PhaseStart = FPlatformTime::Seconds();
}

void UDCGameFlowSubsystem::ShowOverlay(const FText& Title, const FText& Kind)
{
	LoadingState->Title = Title;
	LoadingState->Kind = Kind;
	LoadingState->Status = LOCTEXT("Loading", "Lade …");
	LoadingState->Tip = SDCLoadingScreen::NextTip();
	LoadingState->Progress = 0.05f;
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!Viewport)
	{
		return;
	}
	if (!LoadingWidget.IsValid())
	{
		LoadingWidget = SNew(SDCLoadingScreen).State(LoadingState);
	}
	if (OverlayViewport.Get() != Viewport)
	{
		HideOverlay();
		// Straight into the viewport (not a UMG widget): it stays up while the old world goes and the new one comes.
		Viewport->AddViewportWidgetContent(LoadingWidget.ToSharedRef(), 10000);
		OverlayViewport = Viewport;
	}
	LoadingWidget->SetRenderOpacity(1.f);
}

void UDCGameFlowSubsystem::HideOverlay()
{
	if (UGameViewportClient* Viewport = OverlayViewport.Get(); Viewport && LoadingWidget.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(LoadingWidget.ToSharedRef());
	}
	OverlayViewport.Reset();
}

bool UDCGameFlowSubsystem::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	if (AutosaveDue > 0.0 && Now >= AutosaveDue)
	{
		FlushAutosave();
	}
	if (!SelfTestSteps.IsEmpty() && SelfTestSteps[0]())
	{
		SelfTestSteps.RemoveAt(0);
	}
	UWorld* World = GetGameInstance()->GetWorld();
	switch (Phase)
	{
	case EPhase::Hidden:
		break;
	case EPhase::Travelling:
		if (--FramesToWait <= 0 && World)
		{
			Phase = EPhase::Loading;
			PhaseStart = Now;
			LoadingState->Progress = 0.15f;
			UGameplayStatics::OpenLevel(World, FName(*PendingMap), true, PendingOptions);
		}
		break;
	case EPhase::Loading:
		if (Now - PhaseStart > LoadingMaxSeconds)
		{
			UE_LOG(LogDerClou, Warning, TEXT("Game flow: %s did not load"), *PendingMap);
			HideOverlay();
			Phase = EPhase::Hidden;
		}
		break;
	case EPhase::Preparing:
	{
		if (bRedirectToMenu)
		{
			if (World && World->HasBegunPlay())
			{
				OpenMainMenu();
			}
			break;
		}
		++FramesPrepared;
		const double Elapsed = Now - PhaseStart;
		bool bReady = false;
		if (!World || !World->HasBegunPlay() || IsAsyncLoading())
		{
			LoadingState->Status = LOCTEXT("Loading", "Lade …");
		}
		else if (!UDCHeistStateSubsystem::IsNavigationReady(World))
		{
			LoadingState->Status = LOCTEXT("Ways", "Berechne Wege …");
			LoadingState->Progress = FMath::Max(LoadingState->Progress, 0.55f);
		}
#if WITH_EDITOR
		else if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
		{
			LoadingState->Status = FText::Format(LOCTEXT("Shaders", "Bereite die Grafik vor … (noch {0})"), FText::AsNumber(GShaderCompilingManager->GetNumRemainingJobs()));
			LoadingState->Progress = FMath::Max(LoadingState->Progress, 0.75f);
		}
#endif
		else
		{
			LoadingState->Status = LOCTEXT("Ready", "Bereit");
			LoadingState->Progress = 1.f;
			bReady = true;
		}
		if (!bReady && Elapsed > LoadingMaxSeconds)
		{
			UE_LOG(LogDerClou, Warning, TEXT("Game flow: still not ready after %.0f s - going on"), Elapsed);
			bReady = true;
		}
		if (bReady && Elapsed >= LoadingMinSeconds && FramesPrepared >= 6)
		{
			UE_LOG(LogDerClou, Display, TEXT("Game flow: ready after %.1f s"), Elapsed);
			Phase = EPhase::FadingOut;
			PhaseStart = Now;
		}
		break;
	}
	case EPhase::FadingOut:
	{
		const float Alpha = 1.f - static_cast<float>((Now - PhaseStart) / LoadingFadeSeconds);
		if (LoadingWidget.IsValid())
		{
			LoadingWidget->SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
		}
		if (Alpha <= 0.f)
		{
			HideOverlay();
			Phase = EPhase::Hidden;
			OnLoadingFinished.Broadcast();
		}
		break;
	}
	}
	return true;
}

//~ Saves

void UDCGameFlowSubsystem::OnPlanChanged()
{
	if (bApplyingPlan || bMenu || !bScreens || CurrentHeist.IsNone() || Phase == EPhase::Travelling || Phase == EPhase::Loading)
	{
		return;
	}
	if (PlanSignature() != SavedSignature)
	{
		AutosaveDue = FPlatformTime::Seconds() + AutosaveDelay;
	}
}

FString UDCGameFlowSubsystem::PlanSignature() const
{
	const UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>();
	FString Signature;
	for (const FDCCrewPlan& Plan : Plans ? Plans->GetPlans() : TArray<FDCCrewPlan>())
	{
		if (Plan.Tasks.IsEmpty())
		{
			continue;
		}
		Signature += Plan.Crew.ToString() + TEXT(":");
		for (const FDCPlanTask& Task : Plan.Tasks)
		{
			Signature += Task.Id.ToString(EGuidFormats::Short) + TEXT(",");
		}
		Signature += TEXT(";");
	}
	return Signature;
}

FString UDCGameFlowSubsystem::SlotPrefix() const
{
	return SelfTest == TEXT("Menu") ? FString(SelfTestSlotPrefix) : FString();
}

void UDCGameFlowSubsystem::FlushAutosave()
{
	AutosaveDue = -1.0;
	if (!bScreens || bMenu || bApplyingPlan || CurrentHeist.IsNone())
	{
		return;
	}
	const FString Signature = PlanSignature();
	if (Signature != SavedSignature && WriteSave(SlotPrefix() + TEXT("auto_") + CurrentHeist.ToString(), FString(), true))
	{
		SavedSignature = Signature;
	}
}

bool UDCGameFlowSubsystem::WriteSave(const FString& Slot, const FString& Name, bool bAuto) const
{
	const FDCHeistInfo* Heist = FindHeist(CurrentHeist);
	const UDCPlanSubsystem* Plans = GetGameInstance()->GetSubsystem<UDCPlanSubsystem>();
	UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
	if (!Heist || !Plans || !Save)
	{
		return false;
	}
	Save->DisplayName = Name;
	Save->bAuto = bAuto;
	Save->Heist = Heist->Id;
	Save->Map = Heist->Map;
	Save->Saved = FDateTime::Now();
	Save->Plans = Plans->GetPlans();
	Save->Speed = Plans->GetSpeed();
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	int32 Steps = 0;
	for (const FDCCrewPlan& Plan : Save->Plans)
	{
		Steps += Plan.Tasks.Num();
	}
	UE_LOG(LogDerClou, Display, TEXT("Game flow: %s %s (%d steps)%s"), bAuto ? TEXT("autosave") : TEXT("saved"), *Slot, Steps, bSaved ? TEXT("") : TEXT(" FAILED"));
	return bSaved;
}

FString UDCGameFlowSubsystem::SavePlan(const FString& Name)
{
	if (CurrentHeist.IsNone() || bMenu)
	{
		return FString();
	}
	FlushAutosave();
	const FString Base = SlotPrefix() + TEXT("save_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	FString Slot = Base;
	for (int32 Number = 2; UGameplayStatics::DoesSaveGameExist(Slot, 0); ++Number)
	{
		Slot = FString::Printf(TEXT("%s_%d"), *Base, Number);
	}
	const FString Trimmed = Name.TrimStartAndEnd();
	return WriteSave(Slot, Trimmed.IsEmpty() ? DefaultSaveName() : Trimmed, false) ? Slot : FString();
}

bool UDCGameFlowSubsystem::DeleteSave(const FString& Slot)
{
	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UE_LOG(LogDerClou, Display, TEXT("Game flow: deleted %s%s"), *Slot, bDeleted ? TEXT("") : TEXT(" FAILED"));
	return bDeleted;
}

TArray<FDCSaveInfo> UDCGameFlowSubsystem::ListSaves() const
{
	TArray<FDCSaveInfo> Saves;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / TEXT("*.sav")), true, false);
	const FString Prefix = SlotPrefix();
	for (const FString& File : Files)
	{
		const FString Slot = FPaths::GetBaseFilename(File);
		// The player's saves and the self-test's never mix.
		if (Prefix.IsEmpty() ? Slot.StartsWith(SelfTestSlotPrefix) : !Slot.StartsWith(Prefix))
		{
			continue;
		}
		const FString Own = Slot.RightChop(Prefix.Len());
		if (!Own.StartsWith(TEXT("auto_")) && !Own.StartsWith(TEXT("save_")))
		{
			continue;
		}
		const UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!Save)
		{
			continue;
		}
		FDCSaveInfo& Info = Saves.AddDefaulted_GetRef();
		Info.Slot = Slot;
		Info.Name = Save->DisplayName;
		Info.Heist = Save->Heist;
		Info.Saved = Save->Saved;
		Info.bAuto = Save->bAuto;
		for (const FDCCrewPlan& Plan : Save->Plans)
		{
			Info.Steps += Plan.Tasks.Num();
		}
	}
	Saves.Sort([](const FDCSaveInfo& A, const FDCSaveInfo& B) { return A.Saved > B.Saved; });
	return Saves;
}

TOptional<FDCSaveInfo> UDCGameFlowSubsystem::FindLatestSave() const
{
	const TArray<FDCSaveInfo> Saves = ListSaves();
	return Saves.IsEmpty() ? TOptional<FDCSaveInfo>() : TOptional<FDCSaveInfo>(Saves[0]);
}

TOptional<FDCSaveInfo> UDCGameFlowSubsystem::FindAutosave(FName Heist) const
{
	for (const FDCSaveInfo& Save : ListSaves())
	{
		if (Save.bAuto && Save.Heist == Heist)
		{
			return Save;
		}
	}
	return {};
}

FText UDCGameFlowSubsystem::DescribeSave(const FDCSaveInfo& Save)
{
	const FDCHeistInfo* Heist = FindHeist(Save.Heist);
	const FDateTime Today = FDateTime::Now().GetDate();
	const FText Clock = FText::FromString(Save.Saved.ToString(TEXT("%H:%M")));
	FText When;
	if (Save.Saved.GetDate() == Today)
	{
		When = FText::Format(LOCTEXT("Today", "heute {0}"), Clock);
	}
	else if (Save.Saved.GetDate() == Today - FTimespan::FromDays(1.0))
	{
		When = FText::Format(LOCTEXT("Yesterday", "gestern {0}"), Clock);
	}
	else
	{
		When = FText::FromString(Save.Saved.ToString(TEXT("%d.%m. %H:%M")));
	}
	return FText::Format(LOCTEXT("SaveLine", "{0} · {1} {1}|plural(one=Schritt,other=Schritte) · {2}"),
		Heist ? Heist->Name : FText::FromName(Save.Heist), Save.Steps, When);
}

FText UDCGameFlowSubsystem::SaveTitle(const FDCSaveInfo& Save)
{
	return Save.bAuto || Save.Name.IsEmpty() ? LOCTEXT("AutoTitle", "Automatisch gespeichert") : FText::FromString(Save.Name);
}

FString UDCGameFlowSubsystem::DefaultSaveName() const
{
	const FDCHeistInfo* Heist = FindHeist(CurrentHeist);
	return FString::Printf(TEXT("%s – %s"), Heist ? *Heist->Name.ToString() : *GetGameTitle().ToString(), *FDateTime::Now().ToString(TEXT("%d.%m. %H:%M")));
}

#undef LOCTEXT_NAMESPACE
