#include "DCItemTypes.h"

#define LOCTEXT_NAMESPACE "DCItems"

namespace
{
	FDCItemDef Make(const TCHAR* Id, const FText& Name, const FText& Kind, float MassKg, float VolumeLiters, EDCItemSize Size, int32 Value,
		EDCItemShape Shape, const FVector& ShapeSize, const FLinearColor& Color, const FText& Description)
	{
		FDCItemDef Def;
		Def.Id = Id;
		Def.Name = Name;
		Def.Kind = Kind;
		Def.MassKg = MassKg;
		Def.VolumeLiters = VolumeLiters;
		Def.Size = Size;
		Def.Value = Value;
		Def.Shape = Shape;
		Def.ShapeSize = ShapeSize;
		Def.Color = Color;
		Def.Description = Description;
		return Def;
	}

	FDCItemDef MakeWearable(FDCItemDef Def, EDCEquipSlot Slot, float CapacityLiters = 0.f, EDCItemSize MaxContentSize = EDCItemSize::Small)
	{
		Def.WearSlot = Slot;
		Def.CapacityLiters = CapacityLiters;
		Def.MaxContentSize = MaxContentSize;
		return Def;
	}

	TArray<FDCItemDef> BuildCatalog()
	{
		const FText Gear = LOCTEXT("KindGear", "Ausrüstung");
		const FText Tool = LOCTEXT("KindTool", "Werkzeug");
		const FText Loot = LOCTEXT("KindLoot", "Beute");
		using S = EDCItemSize;
		using P = EDCItemShape;

		// Small items are shown a bit larger than life so they stay clickable from above.
		TArray<FDCItemDef> Items;
		Items.Add(MakeWearable(Make(TEXT("work_clothes"), LOCTEXT("WorkClothes", "Arbeitskleidung"), Gear, 1.5f, 4.f, S::Medium, 0,
			P::Box, FVector(40, 30, 10), FLinearColor(0.15f, 0.2f, 0.35f), LOCTEXT("WorkClothesDesc", "Unauffällige Kleidung mit ein paar Taschen.")),
			EDCEquipSlot::Body, 3.f, S::Small));
		Items.Add(MakeWearable(Make(TEXT("beanie"), LOCTEXT("Beanie", "Strickmütze"), Gear, 0.1f, 0.5f, S::Small, 0,
			P::Sphere, FVector(22, 22, 14), FLinearColor(0.1f, 0.1f, 0.1f), LOCTEXT("BeanieDesc", "Hält warm und verdeckt die Haare.")),
			EDCEquipSlot::Head));
		Items.Add(MakeWearable(Make(TEXT("tool_vest"), LOCTEXT("ToolVest", "Werkzeugweste"), Gear, 1.0f, 3.f, S::Medium, 0,
			P::Box, FVector(40, 35, 8), FLinearColor(0.35f, 0.38f, 0.2f), LOCTEXT("ToolVestDesc", "Viele kleine Taschen für Werkzeug.")),
			EDCEquipSlot::Vest, 6.f, S::Small));
		Items.Add(MakeWearable(Make(TEXT("backpack"), LOCTEXT("Backpack", "Rucksack"), Gear, 1.2f, 5.f, S::Medium, 0,
			P::Box, FVector(35, 25, 50), FLinearColor(0.2f, 0.3f, 0.15f), LOCTEXT("BackpackDesc", "Fasst auch mittelgroße Dinge.")),
			EDCEquipSlot::Back, 25.f, S::Medium));

		Items.Add(Make(TEXT("lockpicks"), LOCTEXT("Lockpicks", "Dietrich-Set"), Tool, 0.2f, 0.3f, S::Small, 0,
			P::Box, FVector(18, 10, 4), FLinearColor(0.7f, 0.7f, 0.75f), LOCTEXT("LockpicksDesc", "Zum Öffnen einfacher Schlösser.")));
		Items.Add(Make(TEXT("flashlight"), LOCTEXT("Flashlight", "Taschenlampe"), Tool, 0.4f, 0.6f, S::Small, 0,
			P::Cylinder, FVector(8, 8, 25), FLinearColor(0.9f, 0.75f, 0.1f), LOCTEXT("FlashlightDesc", "Licht für dunkle Räume.")));
		Items.Add(Make(TEXT("crowbar"), LOCTEXT("Crowbar", "Brecheisen"), Tool, 2.5f, 2.f, S::Medium, 0,
			P::Box, FVector(70, 6, 6), FLinearColor(0.75f, 0.1f, 0.1f), LOCTEXT("CrowbarDesc", "Hebelt Fenster und Kisten auf. Laut.")));

		Items.Add(Make(TEXT("necklace"), LOCTEXT("Necklace", "Goldkette"), Loot, 0.1f, 0.1f, S::Small, 4500,
			P::Box, FVector(16, 16, 4), FLinearColor(1.f, 0.78f, 0.15f), LOCTEXT("NecklaceDesc", "Schwer vergoldet, leicht zu verstecken.")));
		Items.Add(Make(TEXT("laptop"), LOCTEXT("Laptop", "Laptop"), Loot, 2.0f, 3.f, S::Medium, 800,
			P::Box, FVector(35, 25, 4), FLinearColor(0.25f, 0.25f, 0.28f), LOCTEXT("LaptopDesc", "Neues Modell, vielleicht mit interessanten Daten.")));
		Items.Add(Make(TEXT("painting"), LOCTEXT("Painting", "Ölgemälde"), Loot, 8.f, 40.f, S::Large, 60000,
			P::Box, FVector(8, 100, 80), FLinearColor(0.55f, 0.35f, 0.1f), LOCTEXT("PaintingDesc", "Schwerer Goldrahmen. Passt in keine Tasche – nur mit beiden Händen.")));
		Items.Add(Make(TEXT("statue"), LOCTEXT("Statue", "Bronzefigur"), Loot, 35.f, 30.f, S::Large, 12000,
			P::Cylinder, FVector(30, 30, 80), FLinearColor(0.55f, 0.33f, 0.15f), LOCTEXT("StatueDesc", "Massiv und schwer. Mit beiden Händen gerade noch tragbar.")));
		Items.Add(Make(TEXT("piano"), LOCTEXT("Piano", "Klavier"), Loot, 250.f, 900.f, S::Huge, 18000,
			P::Box, FVector(150, 60, 130), FLinearColor(0.04f, 0.04f, 0.05f), LOCTEXT("PianoDesc", "Wertvoll, aber viel zu schwer für eine Person.")));
		return Items;
	}

	FString FormatDecimal(float Value)
	{
		// One decimal place when needed, German decimal comma.
		FString Text = FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value), 0.05f)
			? FString::Printf(TEXT("%d"), FMath::RoundToInt(Value))
			: FString::Printf(TEXT("%.1f"), Value);
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

	FText FormatKg(float Kg)
	{
		return FText::FromString(FormatDecimal(Kg) + TEXT(" kg"));
	}

	FText FormatLiters(float Liters)
	{
		return FText::FromString(FormatDecimal(Liters) + TEXT(" L"));
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
