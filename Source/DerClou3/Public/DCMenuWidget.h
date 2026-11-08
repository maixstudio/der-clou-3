#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCGameFlow.h"
#include "DCOptions.h"

#include "DCMenuWidget.generated.h"

class SBox;
class SEditableTextBox;

/** A page next to the menu column. */
UENUM()
enum class EDCMenuPage : uint8
{
	None,
	/** "Neues Spiel": which heist. */
	Heists,
	Load,
	/** Only in the game: save the plan under a name. */
	Save,
	Options
};

/**
 * The start menu (over the villa at night) and the game menu (Esc / "Menü" over a heist): a column with
 * the title and the main buttons on the left, the open page next to it. Pages are rebuilt in the next
 * tick after a change, never inside a click.
 */
UCLASS()
class DERCLOU3_API UDCMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** bInGame: the game menu (Weiter, Plan speichern, Hauptmenü …); else the start menu. */
	void Setup(bool bInGame);
	bool IsInGame() const { return bInGame; }

	void ShowPage(EDCMenuPage InPage);
	EDCMenuPage GetPage() const { return Page; }
	/** Esc: closes the open page; false if none was open. */
	bool Back();
	/** Reads the saves again (the menu was opened again). */
	void Refresh();
	/** The game menu wants to close ("Weiter"). */
	TFunction<void()> OnCloseRequested;

	//~ What the buttons do, also for self-tests
	void SelectHeist(FName Id);
	void SelectSave(const FString& SaveSlot);
	FString GetSelectedSave() const { return SelectedSave; }
	const TArray<FDCSaveInfo>& GetSaves() const { return Saves; }
	void SetSaveName(const FString& Name) { SaveName = Name; }
	/** Save page: saves the plan under the name; false if that did not work. */
	bool SaveNow();
	/** Options as edited (applied by "Übernehmen"). */
	FDCOptionValues& EditOptions() { return PendingOptions; }
	/** The last short message ("Gespeichert: …"). */
	FText GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedRef<SWidget> MakeColumn();
	TSharedRef<SWidget> MakePage();
	TSharedRef<SWidget> MakeHeistsPage();
	TSharedRef<SWidget> MakeLoadPage();
	TSharedRef<SWidget> MakeSavePage();
	TSharedRef<SWidget> MakeOptionsPage();
	/** A page: title, content, buttons at the bottom right. */
	TSharedRef<SWidget> MakeWindow(const FText& Title, TSharedRef<SWidget> Body, TSharedRef<SWidget> Buttons);
	void ReadSaves();
	void SetMessage(const FText& Text);
	UDCGameFlowSubsystem* Flow() const;

	bool bInGame = false;
	EDCMenuPage Page = EDCMenuPage::None;
	bool bDirty = true;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SBox> ColumnHost;
	TSharedPtr<SBox> PageHost;
	TSharedPtr<SEditableTextBox> NameBox;

	TArray<FDCSaveInfo> Saves;
	FName SelectedHeist;
	FString SelectedSave;
	bool bConfirmDelete = false;
	FString SaveName;
	FDCOptionValues PendingOptions;
	FText Message;
	double MessageTime = -100.0;
};
