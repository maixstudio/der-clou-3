#include "DCItemTypes.h"

#define LOCTEXT_NAMESPACE "DCItems"

namespace
{
	/** Small fluent helper so the catalog below stays readable. */
	struct FDef
	{
		FDCItemDef Def;

		FDef(const TCHAR* Id, const FText& Name, const FText& Kind, float MassKg, FIntPoint Cells, EDCItemSize Size, int32 Value = 0)
		{
			Def.Id = Id;
			Def.Name = Name;
			Def.Kind = Kind;
			Def.MassKg = MassKg;
			Def.GridSize = Cells;
			Def.Size = Size;
			Def.Value = Value;
		}
		FDef& Look(EDCItemShape Shape, const FVector& Size, const FLinearColor& Color) { Def.Shape = Shape; Def.ShapeSize = Size; Def.Color = Color; return *this; }
		FDef& Wear(EDCEquipSlot Slot) { Def.WearSlot = Slot; return *this; }
		FDef& Holds(FIntPoint Grid, EDCItemSize MaxSize) { Def.Grid = Grid; Def.MaxContentSize = MaxSize; return *this; }
		FDef& Short(const FText& Text) { Def.ShortName = Text; return *this; }
		FDef& Openable() { Def.bOpenable = true; return *this; }
		/** Lock: pick time and difficulty; ForceDifficulty < 0 = cannot be broken open. */
		FDef& Lock(float PickSeconds, int32 Difficulty, int32 ForceDifficulty, EDCSkill Skill = EDCSkill::Locks, const TCHAR* Tool = TEXT("lockpicks"))
		{
			Def.LockPickSeconds = PickSeconds;
			Def.LockDifficulty = Difficulty;
			Def.bForceable = ForceDifficulty >= 0;
			Def.ForceDifficulty = FMath::Max(0, ForceDifficulty);
			Def.LockSkill = Skill;
			Def.LockTool = Tool;
			return *this;
		}
		FDef& Fixed() { Def.bMovable = false; return *this; }
		FDef& Glass() { Def.bBreakable = true; return *this; }
		FDef& Parts(TArray<FName> Parts) { Def.FixedParts = MoveTemp(Parts); return *this; }
		FDef& Note(const FText& Text) { Def.Description = Text; return *this; }
	};

	TArray<FDCItemDef> BuildCatalog()
	{
		const FText Gear = LOCTEXT("KindGear", "Ausrüstung");
		const FText Tool = LOCTEXT("KindTool", "Werkzeug");
		const FText Loot = LOCTEXT("KindLoot", "Beute");
		const FText Furniture = LOCTEXT("KindFurniture", "Möbel");
		const FText Part = LOCTEXT("KindPart", "Möbelteil");
		const FText Box = LOCTEXT("KindBox", "Behälter");
		const FText Stuff = LOCTEXT("KindStuff", "Gegenstand");
		using S = EDCItemSize;
		using P = EDCItemShape;
		using E = EDCEquipSlot;

		// Small items are shown a bit larger than life so they stay clickable from above.
		// Grid footprints (cells): small things 1 x 1, long things stand upright (crowbar 1 x 4).
		// 0 x 0 = fits in no grid: only in both hands (painting, statue) or not at all (furniture).
		const FIntPoint None(0, 0);
		const TArray<FDef> Defs = {
			// Wearables: worn they are a slot with their own grid; packed away they are an item like any other.
			FDef(TEXT("work_clothes"), LOCTEXT("WorkClothes", "Arbeitskleidung"), Gear, 1.5f, { 2, 2 }, S::Medium).Wear(E::Body).Holds({ 3, 2 }, S::Small).Short(LOCTEXT("WorkClothesShort", "Kleidung"))
				.Look(P::Box, FVector(40, 30, 10), FLinearColor(0.15f, 0.2f, 0.35f)).Note(LOCTEXT("WorkClothesDesc", "Unauffällige Kleidung mit ein paar Taschen.")),
			FDef(TEXT("beanie"), LOCTEXT("Beanie", "Strickmütze"), Gear, 0.1f, { 1, 1 }, S::Small).Wear(E::Head).Short(LOCTEXT("BeanieShort", "Mütze"))
				.Look(P::Sphere, FVector(22, 22, 14), FLinearColor(0.1f, 0.1f, 0.1f)).Note(LOCTEXT("BeanieDesc", "Hält warm und verdeckt die Haare.")),
			FDef(TEXT("tool_vest"), LOCTEXT("ToolVest", "Werkzeugweste"), Gear, 1.0f, { 2, 2 }, S::Medium).Wear(E::Vest).Holds({ 4, 2 }, S::Small).Short(LOCTEXT("ToolVestShort", "Weste"))
				.Look(P::Box, FVector(40, 35, 8), FLinearColor(0.35f, 0.38f, 0.2f)).Note(LOCTEXT("ToolVestDesc", "Viele kleine Taschen für Werkzeug.")),
			FDef(TEXT("backpack"), LOCTEXT("Backpack", "Rucksack"), Gear, 1.2f, { 3, 4 }, S::Medium).Wear(E::Back).Holds({ 5, 4 }, S::Medium)
				.Look(P::Box, FVector(35, 25, 50), FLinearColor(0.2f, 0.3f, 0.15f)).Note(LOCTEXT("BackpackDesc", "Fasst auch mittelgroße Dinge.")),
			FDef(TEXT("coat"), LOCTEXT("Coat", "Mantel"), Gear, 1.8f, { 3, 4 }, S::Medium, 120).Holds({ 2, 1 }, S::Small)
				.Look(P::Box, FVector(50, 35, 10), FLinearColor(0.3f, 0.22f, 0.15f)).Note(LOCTEXT("CoatDesc", "Wollmantel mit Innentasche.")),

			// Tools.
			FDef(TEXT("lockpicks"), LOCTEXT("Lockpicks", "Dietrich-Set"), Tool, 0.2f, { 1, 1 }, S::Small).Short(LOCTEXT("LockpicksShort", "Dietr."))
				.Look(P::Box, FVector(18, 10, 4), FLinearColor(0.7f, 0.7f, 0.75f)).Note(LOCTEXT("LockpicksDesc", "Öffnet Schlösser leise – braucht aber Zeit und beide Hände.")),
			FDef(TEXT("stethoscope"), LOCTEXT("Stethoscope", "Stethoskop"), Tool, 0.3f, { 2, 1 }, S::Small)
				.Look(P::Box, FVector(16, 12, 5), FLinearColor(0.2f, 0.55f, 0.6f)).Note(LOCTEXT("StethoscopeDesc", "Zum Abhören von Tresorschlössern.")),
			FDef(TEXT("flashlight"), LOCTEXT("Flashlight", "Taschenlampe"), Tool, 0.4f, { 1, 2 }, S::Small).Short(LOCTEXT("FlashlightShort", "Lampe"))
				.Look(P::Cylinder, FVector(8, 8, 25), FLinearColor(0.9f, 0.75f, 0.1f)).Note(LOCTEXT("FlashlightDesc", "Licht für dunkle Räume.")),
			FDef(TEXT("crowbar"), LOCTEXT("Crowbar", "Brecheisen"), Tool, 2.5f, { 1, 4 }, S::Medium)
				.Look(P::Box, FVector(70, 6, 6), FLinearColor(0.75f, 0.1f, 0.1f)).Note(LOCTEXT("CrowbarDesc", "Bricht Schubladen und Kisten schnell auf. Das Schloss ist danach kaputt.")),

			// Loot.
			FDef(TEXT("necklace"), LOCTEXT("Necklace", "Goldkette"), Loot, 0.1f, { 1, 1 }, S::Small, 4500).Short(LOCTEXT("NecklaceShort", "Kette"))
				.Look(P::Box, FVector(16, 16, 4), FLinearColor(1.f, 0.78f, 0.15f)).Note(LOCTEXT("NecklaceDesc", "Schwer vergoldet, leicht zu verstecken.")),
			FDef(TEXT("ring"), LOCTEXT("Ring", "Brillantring"), Loot, 0.01f, { 1, 1 }, S::Small, 9000).Short(LOCTEXT("RingShort", "Ring"))
				.Look(P::Sphere, FVector(10, 10, 6), FLinearColor(0.95f, 0.95f, 1.f)).Note(LOCTEXT("RingDesc", "Klein, aber sehr wertvoll.")),
			FDef(TEXT("watch"), LOCTEXT("Watch", "Armbanduhr"), Loot, 0.15f, { 1, 1 }, S::Small, 3800).Short(LOCTEXT("WatchShort", "Uhr"))
				.Look(P::Cylinder, FVector(10, 10, 4), FLinearColor(0.8f, 0.8f, 0.85f)).Note(LOCTEXT("WatchDesc", "Schweizer Uhrwerk.")),
			FDef(TEXT("cash"), LOCTEXT("Cash", "Bündel Geldscheine"), Loot, 0.1f, { 1, 1 }, S::Small, 5000).Short(LOCTEXT("CashShort", "Geld"))
				.Look(P::Box, FVector(16, 8, 4), FLinearColor(0.35f, 0.6f, 0.35f)).Note(LOCTEXT("CashDesc", "Bar und nicht zurückverfolgbar.")),
			FDef(TEXT("wallet"), LOCTEXT("Wallet", "Brieftasche"), Loot, 0.15f, { 1, 1 }, S::Small, 300).Holds({ 1, 1 }, S::Small).Short(LOCTEXT("WalletShort", "Börse"))
				.Look(P::Box, FVector(12, 9, 3), FLinearColor(0.25f, 0.12f, 0.05f)).Note(LOCTEXT("WalletDesc", "Etwas Bargeld und Karten.")),
			FDef(TEXT("laptop"), LOCTEXT("Laptop", "Laptop"), Loot, 2.0f, { 3, 2 }, S::Medium, 800)
				.Look(P::Box, FVector(35, 25, 4), FLinearColor(0.25f, 0.25f, 0.28f)).Note(LOCTEXT("LaptopDesc", "Neues Modell, vielleicht mit interessanten Daten.")),
			FDef(TEXT("files"), LOCTEXT("Files", "Aktenordner"), Stuff, 1.5f, { 2, 3 }, S::Medium).Short(LOCTEXT("FilesShort", "Ordner"))
				.Look(P::Box, FVector(30, 8, 32), FLinearColor(0.2f, 0.35f, 0.6f)).Note(LOCTEXT("FilesDesc", "Geschäftsunterlagen. Vielleicht steht etwas Brauchbares drin.")),
			FDef(TEXT("painting"), LOCTEXT("Painting", "Ölgemälde"), Loot, 8.f, None, S::Large, 60000)
				.Look(P::Box, FVector(8, 100, 80), FLinearColor(0.55f, 0.35f, 0.1f)).Note(LOCTEXT("PaintingDesc", "Schwerer Goldrahmen. Passt in keine Tasche – nur mit beiden Händen.")),
			FDef(TEXT("statue"), LOCTEXT("Statue", "Bronzefigur"), Loot, 35.f, None, S::Large, 12000)
				.Look(P::Cylinder, FVector(30, 30, 80), FLinearColor(0.55f, 0.33f, 0.15f)).Note(LOCTEXT("StatueDesc", "Massiv und schwer. Mit beiden Händen gerade noch tragbar.")),
			FDef(TEXT("brooch"), LOCTEXT("Brooch", "Diamantbrosche"), Loot, 0.05f, { 1, 1 }, S::Small, 15000).Short(LOCTEXT("BroochShort", "Brosche"))
				.Look(P::Box, FVector(10, 10, 4), FLinearColor(0.85f, 0.9f, 1.f)).Note(LOCTEXT("BroochDesc", "Das Prunkstück der Sammlung.")),
			FDef(TEXT("figurine"), LOCTEXT("Figurine", "Porzellanfigur"), Loot, 0.6f, { 1, 2 }, S::Medium, 2500).Short(LOCTEXT("FigurineShort", "Figur"))
				.Look(P::Cylinder, FVector(10, 10, 22), FLinearColor(0.95f, 0.95f, 0.92f)).Note(LOCTEXT("FigurineDesc", "Meißner Porzellan, zerbrechlich.")),
			FDef(TEXT("piano"), LOCTEXT("Piano", "Klavier"), Loot, 250.f, None, S::Huge, 18000)
				.Look(P::Box, FVector(150, 60, 130), FLinearColor(0.04f, 0.04f, 0.05f)).Note(LOCTEXT("PianoDesc", "Wertvoll, aber viel zu schwer für eine Person.")),

			// Boxes.
			FDef(TEXT("jewelry_box"), LOCTEXT("JewelryBox", "Schmuckschatulle"), Box, 0.3f, { 2, 1 }, S::Small, 150).Holds({ 2, 1 }, S::Small).Openable().Lock(3.f, 2, 1).Short(LOCTEXT("JewelryBoxShort", "Schatulle"))
				.Look(P::Box, FVector(20, 15, 10), FLinearColor(0.45f, 0.1f, 0.15f)).Note(LOCTEXT("JewelryBoxDesc", "Kleine Schatulle mit Samtfutter.")),
			FDef(TEXT("cardboard_box"), LOCTEXT("CardboardBox", "Umzugskarton"), Box, 0.6f, None, S::Large).Holds({ 6, 4 }, S::Medium).Openable()
				.Look(P::Box, FVector(55, 40, 38), FLinearColor(0.65f, 0.48f, 0.28f)).Note(LOCTEXT("CardboardBoxDesc", "Offen zugeklappt, nichts verklebt.")),

			// Furniture. Parts (drawers) are separate items fixed inside their furniture.
			FDef(TEXT("desk_drawer"), LOCTEXT("DeskDrawer", "Schublade"), Part, 3.f, None, S::Large).Holds({ 6, 3 }, S::Medium).Openable().Lock(5.f, 3, 3)
				.Note(LOCTEXT("DeskDrawerDesc", "Mit einfachem Möbelschloss.")),
			FDef(TEXT("desk"), LOCTEXT("Desk", "Schreibtisch"), Furniture, 55.f, None, S::Huge, 400).Parts({ TEXT("desk_drawer") })
				.Look(P::Box, FVector(120, 60, 75), FLinearColor(0.38f, 0.22f, 0.1f)).Note(LOCTEXT("DeskDesc", "Massivholz mit einer abschließbaren Schublade.")),
			FDef(TEXT("safe"), LOCTEXT("Safe", "Wandtresor"), Furniture, 120.f, None, S::Huge).Fixed().Holds({ 4, 4 }, S::Medium).Openable().Lock(20.f, 6, -1, EDCSkill::Safes, TEXT("stethoscope"))
				.Look(P::Box, FVector(60, 60, 80), FLinearColor(0.22f, 0.23f, 0.25f)).Note(LOCTEXT("SafeDesc", "Fest verankert. Mit dem Brecheisen ist nichts zu machen – knacken nur mit Stethoskop und Tresor-Erfahrung.")),
			FDef(TEXT("display_case"), LOCTEXT("DisplayCase", "Vitrine"), Furniture, 45.f, None, S::Huge, 600).Holds({ 6, 2 }, S::Medium).Openable().Lock(6.f, 4, -1).Glass()
				.Look(P::Box, FVector(90, 50, 120), FLinearColor(0.55f, 0.75f, 0.85f)).Note(LOCTEXT("DisplayCaseDesc", "Glasvitrine mit kleinem Schloss. Das Glas lässt sich einschlagen – das hört man weit.")),
			FDef(TEXT("wardrobe"), LOCTEXT("Wardrobe", "Kleiderschrank"), Furniture, 80.f, None, S::Huge, 200).Holds({ 8, 6 }, S::Medium).Openable()
				.Look(P::Box, FVector(100, 55, 190), FLinearColor(0.45f, 0.3f, 0.18f)).Note(LOCTEXT("WardrobeDesc", "Groß genug, um sich darin zu verstecken.")),
			FDef(TEXT("sofa"), LOCTEXT("Sofa", "Sofa"), Furniture, 60.f, None, S::Huge, 300).Holds({ 3, 1 }, S::Small)
				.Look(P::Box, FVector(200, 85, 80), FLinearColor(0.25f, 0.32f, 0.45f)).Note(LOCTEXT("SofaDesc", "Zwischen den Polstern lässt sich Kleines verstecken.")),
			FDef(TEXT("armchair"), LOCTEXT("Armchair", "Sessel"), Furniture, 18.f, None, S::Large, 150)
				.Look(P::Box, FVector(80, 80, 90), FLinearColor(0.55f, 0.15f, 0.12f)).Note(LOCTEXT("ArmchairDesc", "Bequem, aber sperrig.")),
			FDef(TEXT("dining_table"), LOCTEXT("DiningTable", "Esstisch"), Furniture, 35.f, None, S::Huge, 250)
				.Look(P::Box, FVector(160, 90, 75), FLinearColor(0.5f, 0.34f, 0.18f)).Note(LOCTEXT("DiningTableDesc", "Eiche, für sechs Personen.")),
			FDef(TEXT("chair"), LOCTEXT("Chair", "Stuhl"), Furniture, 6.f, None, S::Large, 40)
				.Look(P::Box, FVector(45, 45, 90), FLinearColor(0.45f, 0.3f, 0.16f)).Note(LOCTEXT("ChairDesc", "Ein einfacher Holzstuhl.")),
		};

		TArray<FDCItemDef> Items;
		for (const FDef& Entry : Defs)
		{
			Items.Add(Entry.Def);
		}
		return Items;
	}

	FString FormatDecimal(float Value)
	{
		// Up to two decimal places below 1, one above; no trailing zeros; German decimal comma.
		FString Text = Value < 1.f ? FString::Printf(TEXT("%.2f"), Value) : FString::Printf(TEXT("%.1f"), Value);
		while (Text.Contains(TEXT(".")) && (Text.EndsWith(TEXT("0")) || Text.EndsWith(TEXT("."))))
		{
			Text.LeftChopInline(1);
		}
		return Text.Replace(TEXT("."), TEXT(","));
	}
}

namespace DCItems
{
	const TArray<FDCItemDef>& GetAll()
	{
		static const TArray<FDCItemDef> Catalog = BuildCatalog();
		return Catalog;
	}

	const FDCItemDef* Find(FName Id)
	{
		return GetAll().FindByPredicate([Id](const FDCItemDef& Def) { return Def.Id == Id; });
	}

	FText DescribeSize(EDCItemSize Size)
	{
		switch (Size)
		{
		case EDCItemSize::Small: return LOCTEXT("SizeSmall", "klein");
		case EDCItemSize::Medium: return LOCTEXT("SizeMedium", "mittel");
		case EDCItemSize::Large: return LOCTEXT("SizeLarge", "groß");
		case EDCItemSize::Huge: return LOCTEXT("SizeHuge", "sperrig");
		}
		return FText::GetEmpty();
	}

	FText DescribeSlot(EDCEquipSlot Slot)
	{
		switch (Slot)
		{
		case EDCEquipSlot::None: return FText::GetEmpty();
		case EDCEquipSlot::Head: return LOCTEXT("SlotHead", "Kopf");
		case EDCEquipSlot::Body: return LOCTEXT("SlotBody", "Kleidung");
		case EDCEquipSlot::Vest: return LOCTEXT("SlotVest", "Weste");
		case EDCEquipSlot::Back: return LOCTEXT("SlotBack", "Rücken");
		case EDCEquipSlot::LeftHand: return LOCTEXT("SlotLeftHand", "Linke Hand");
		case EDCEquipSlot::RightHand: return LOCTEXT("SlotRightHand", "Rechte Hand");
		}
		return FText::GetEmpty();
	}

	FText DescribeAction(EDCItemAction Action)
	{
		switch (Action)
		{
		case EDCItemAction::Open: return LOCTEXT("ActionOpen", "öffnen");
		case EDCItemAction::Close: return LOCTEXT("ActionClose", "schließen");
		case EDCItemAction::PickLock: return LOCTEXT("ActionPick", "knacken");
		case EDCItemAction::Force: return LOCTEXT("ActionForce", "aufbrechen");
		case EDCItemAction::Take: return LOCTEXT("ActionTake", "nehmen");
		case EDCItemAction::Drop: return LOCTEXT("ActionDrop", "ablegen");
		case EDCItemAction::Smash: return LOCTEXT("ActionSmash", "einschlagen");
		}
		return FText::GetEmpty();
	}

	FText FormatKg(float Kg)
	{
		return FText::FromString(FormatDecimal(Kg) + TEXT(" kg"));
	}

	FText FormatCells(FIntPoint Cells)
	{
		return FText::FromString(FString::Printf(TEXT("%d × %d"), Cells.X, Cells.Y));
	}

	FText FormatEuro(int32 Euro)
	{
		// Thousands separated by dots: 60.000 €
		FString Digits = FString::FromInt(FMath::Abs(Euro));
		for (int32 Index = Digits.Len() - 3; Index > 0; Index -= 3)
		{
			Digits.InsertAt(Index, TEXT('.'));
		}
		return FText::FromString((Euro < 0 ? TEXT("-") : TEXT("")) + Digits + TEXT(" €"));
	}
}

#undef LOCTEXT_NAMESPACE
