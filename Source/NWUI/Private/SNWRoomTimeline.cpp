#include "SNWRoomTimeline.h"

#include "NWRoomSession.h"
#include "Rendering/DrawElements.h"
#include "Sim/RoomSim.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "NWRoomTimeline"

namespace NWRoomTimelineStyle
{
	constexpr float QueuedWidth = 90.f;
	constexpr float MinBlockWidth = 8.f;

	const FLinearColor LaneColor(0.09f, 0.1f, 0.13f, 1.f);
	const FLinearColor SelectedLaneColor(0.13f, 0.16f, 0.24f, 1.f);
	const FLinearColor TextColor(0.92f, 0.93f, 0.95f, 1.f);
	const FLinearColor DimColor(0.55f, 0.58f, 0.63f, 1.f);
	const FLinearColor FailedColor(0.85f, 0.2f, 0.18f, 1.f);
	const FLinearColor QueuedColor(0.32f, 0.33f, 0.37f, 1.f);

	FLinearColor CommandColor(nw::room::CommandType Type)
	{
		switch (Type)
		{
		case nw::room::CommandType::MoveTo: return FLinearColor(0.2f, 0.45f, 0.78f);
		case nw::room::CommandType::Open: return FLinearColor(0.2f, 0.62f, 0.4f);
		case nw::room::CommandType::Close: return FLinearColor(0.35f, 0.5f, 0.3f);
		case nw::room::CommandType::PickLock: return FLinearColor(0.85f, 0.6f, 0.15f);
		case nw::room::CommandType::GoThrough: return FLinearColor(0.15f, 0.62f, 0.62f);
		case nw::room::CommandType::ClimbThrough: return FLinearColor(0.55f, 0.4f, 0.8f);
		case nw::room::CommandType::Wait: return FLinearColor(0.42f, 0.42f, 0.46f);
		}
		return FLinearColor::Gray;
	}

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FString Clock(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
	}

	float ToSeconds(nw::room::Tick Ticks)
	{
		return static_cast<float>(Ticks) / static_cast<float>(nw::room::TicksPerSecond);
	}
}

void SNWRoomTimeline::Construct(const FArguments& InArgs)
{
	Session = InArgs._Session;
	SetToolTipText(TAttribute<FText>::CreateSP(this, &SNWRoomTimeline::GetHoverText));
}

FVector2D SNWRoomTimeline::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const int32 Lanes = Session.IsValid() ? FMath::Max(1, Session->GetAgentCount()) : 1;
	return FVector2D(600.f, RulerHeight + static_cast<float>(Lanes) * (LaneHeight + LaneGap));
}

void SNWRoomTimeline::ComputeScale(float Width, float& OutSeconds, float& OutPixelsPerSecond) const
{
	const UNWRoomSession* Current = Session.Get();
	float End = Current ? Current->GetSeconds() : 0.f;
	if (Current)
	{
		for (const nw::room::CommandRecord& Record : Current->GetSim().GetRecords())
		{
			if (Record.End != nw::room::NoTick)
			{
				End = FMath::Max(End, NWRoomTimelineStyle::ToSeconds(Record.End));
			}
		}
	}
	// Show at least 30 s, and always some room after the end of the plan.
	OutSeconds = FMath::Max(30.f, End * 1.15f + 5.f);
	OutPixelsPerSecond = FMath::Max(1.f, (Width - NameWidth - 8.f) / OutSeconds);
}

void SNWRoomTimeline::LayoutBlocks(float Width, TArray<FBlock>& OutBlocks) const
{
	const UNWRoomSession* Current = Session.Get();
	if (!Current)
	{
		return;
	}
	float Seconds = 0.f;
	float PixelsPerSecond = 1.f;
	ComputeScale(Width, Seconds, PixelsPerSecond);

	const nw::room::RoomSim& Sim = Current->GetSim();
	const float Now = Current->GetSeconds();
	const std::vector<nw::room::CommandRecord>& Records = Sim.GetRecords();
	TArray<float> QueuedX;
	QueuedX.Init(0.f, Current->GetAgentCount());

	for (int32 Index = 0; Index < static_cast<int32>(Records.size()); ++Index)
	{
		const nw::room::CommandRecord& Record = Records[static_cast<size_t>(Index)];
		const int32 Agent = Record.Cmd.Agent;
		if (Agent < 0 || Agent >= Current->GetAgentCount())
		{
			continue;
		}
		const float LaneY = RulerHeight + static_cast<float>(Agent) * (LaneHeight + LaneGap);
		FBlock& Block = OutBlocks.AddDefaulted_GetRef();
		Block.Record = Index;
		if (Record.Start == nw::room::NoTick)
		{
			// Queued: grey chips after the current time.
			const float X = FMath::Max(QueuedX[Agent], NameWidth + Now * PixelsPerSecond + 4.f);
			Block.Position = FVector2D(X, LaneY + 6.f);
			Block.Size = FVector2D(NWRoomTimelineStyle::QueuedWidth, LaneHeight - 12.f);
		}
		else
		{
			const float Start = NWRoomTimelineStyle::ToSeconds(Record.Start);
			const float End = Record.End == nw::room::NoTick ? Now : NWRoomTimelineStyle::ToSeconds(Record.End);
			Block.Position = FVector2D(NameWidth + Start * PixelsPerSecond, LaneY);
			Block.Size = FVector2D(FMath::Max(NWRoomTimelineStyle::MinBlockWidth, (End - Start) * PixelsPerSecond - 2.f), LaneHeight);
		}
		QueuedX[Agent] = FMath::Max(QueuedX[Agent], static_cast<float>(Block.Position.X + Block.Size.X) + 4.f);
	}
}

int32 SNWRoomTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace NWRoomTimelineStyle;
	const UNWRoomSession* Current = Session.Get();
	if (!Current || !Current->IsValid())
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	float Seconds = 0.f;
	float PixelsPerSecond = 1.f;
	ComputeScale(static_cast<float>(Size.X), Seconds, PixelsPerSecond);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo BlockFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);

	auto Rect = [&](const FVector2D& Position, const FVector2D& RectSize, const FLinearColor& Color, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(RectSize, FSlateLayoutTransform(Position)), White(), ESlateDrawEffect::None, Color);
	};
	auto Label = [&](const FVector2D& Position, const FString& String, const FSlateFontInfo& Font, const FLinearColor& Color, int32 Layer)
	{
		FSlateDrawElement::MakeText(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2D(400.0, 30.0), FSlateLayoutTransform(Position)), String, Font, ESlateDrawEffect::None, Color);
	};

	// Ruler.
	const float Step = Seconds <= 40.f ? 5.f : (Seconds <= 120.f ? 10.f : (Seconds <= 360.f ? 30.f : 60.f));
	for (float T = 0.f; T <= Seconds; T += Step)
	{
		const float X = NameWidth + T * PixelsPerSecond;
		Rect(FVector2D(X, RulerHeight - 6.f), FVector2D(1.f, 6.f), DimColor, LayerId);
		Label(FVector2D(X + 3.f, 0.f), Clock(T), SmallFont, DimColor, LayerId);
	}

	// Lanes with names.
	for (int32 Agent = 0; Agent < Current->GetAgentCount(); ++Agent)
	{
		const float LaneY = RulerHeight + static_cast<float>(Agent) * (LaneHeight + LaneGap);
		const bool bSelected = Agent == Current->GetSelectedAgent();
		Rect(FVector2D(0.f, LaneY), FVector2D(Size.X, LaneHeight), bSelected ? SelectedLaneColor : LaneColor, LayerId);
		Label(FVector2D(10.f, LaneY + 11.f), Current->GetAgentName(Agent).ToString(), NameFont, TextColor, LayerId + 1);
	}

	// Command blocks.
	TArray<FBlock> Blocks;
	LayoutBlocks(static_cast<float>(Size.X), Blocks);
	const std::vector<nw::room::CommandRecord>& Records = Current->GetSim().GetRecords();
	for (const FBlock& Block : Blocks)
	{
		const nw::room::CommandRecord& Record = Records[static_cast<size_t>(Block.Record)];
		const bool bQueued = Record.Start == nw::room::NoTick;
		const bool bRunning = !bQueued && Record.End == nw::room::NoTick;
		FLinearColor Color = bQueued ? QueuedColor : (Record.bFailed ? FailedColor : CommandColor(Record.Cmd.Type));
		if (Block.Record == HoveredRecord)
		{
			Color = Color * 1.25f;
			Color.A = 1.f;
		}
		if (bRunning)
		{
			Rect(Block.Position - FVector2D(2.f, 2.f), Block.Size + FVector2D(4.f, 4.f), FLinearColor::White, LayerId + 1);
		}
		Rect(Block.Position, Block.Size, Color, LayerId + 2);

		// Label, clipped to the block.
		FString BlockText = Current->DescribeCommand(Record.Cmd).ToString();
		if (!bQueued)
		{
			const float End = Record.End == nw::room::NoTick ? Current->GetSeconds() : NWRoomTimelineStyle::ToSeconds(Record.End);
			BlockText += FString::Printf(TEXT("  %.1f s"), End - NWRoomTimelineStyle::ToSeconds(Record.Start));
		}
		if (Record.bFailed)
		{
			BlockText = TEXT("! ") + BlockText;
		}
		const FGeometry BlockGeometry = AllottedGeometry.MakeChild(Block.Size, FSlateLayoutTransform(Block.Position));
		OutDrawElements.PushClip(FSlateClippingZone(BlockGeometry));
		Label(Block.Position + FVector2D(6.f, Block.Size.Y * 0.5f - 9.f), BlockText, BlockFont, FLinearColor::White, LayerId + 3);
		OutDrawElements.PopClip();
	}

	// Playhead.
	const float NowX = NameWidth + Current->GetSeconds() * PixelsPerSecond;
	const float LanesBottom = RulerHeight + static_cast<float>(Current->GetAgentCount()) * (LaneHeight + LaneGap);
	Rect(FVector2D(NowX - 1.f, RulerHeight - 4.f), FVector2D(2.f, LanesBottom - RulerHeight + 4.f), FLinearColor::White, LayerId + 4);
	Label(FVector2D(NowX + 4.f, RulerHeight - 2.f + LanesBottom - RulerHeight - 16.f), Clock(Current->GetSeconds()), SmallFont, FLinearColor::White, LayerId + 4);

	return LayerId + 5;
}

FReply SNWRoomTimeline::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	TArray<FBlock> Blocks;
	LayoutBlocks(static_cast<float>(MyGeometry.GetLocalSize().X), Blocks);
	HoveredRecord = INDEX_NONE;
	for (const FBlock& Block : Blocks)
	{
		if (Local.X >= Block.Position.X && Local.X <= Block.Position.X + Block.Size.X && Local.Y >= Block.Position.Y && Local.Y <= Block.Position.Y + Block.Size.Y)
		{
			HoveredRecord = Block.Record;
		}
	}
	return FReply::Unhandled();
}

void SNWRoomTimeline::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseLeave(MouseEvent);
	HoveredRecord = INDEX_NONE;
}

FText SNWRoomTimeline::GetHoverText() const
{
	const UNWRoomSession* Current = Session.Get();
	if (!Current || HoveredRecord == INDEX_NONE || HoveredRecord >= static_cast<int32>(Current->GetSim().GetRecords().size()))
	{
		return FText::GetEmpty();
	}
	const nw::room::CommandRecord& Record = Current->GetSim().GetRecords()[static_cast<size_t>(HoveredRecord)];
	const FText What = Current->DescribeCommand(Record.Cmd);
	if (Record.Start == nw::room::NoTick)
	{
		return FText::Format(LOCTEXT("Queued", "{0}\ngeplant, noch nicht begonnen"), What);
	}
	const FText From = FText::FromString(NWRoomTimelineStyle::Clock(NWRoomTimelineStyle::ToSeconds(Record.Start)));
	const FText To = Record.End == nw::room::NoTick ? LOCTEXT("Running", "läuft") : FText::FromString(NWRoomTimelineStyle::Clock(NWRoomTimelineStyle::ToSeconds(Record.End)));
	if (Record.bFailed)
	{
		return FText::Format(LOCTEXT("FailedTip", "{0}\n{1} – {2}\nGescheitert: {3}"), What, From, To, FText::FromString(UTF8_TO_TCHAR(Record.FailReason.c_str())));
	}
	return FText::Format(LOCTEXT("Tip", "{0}\n{1} – {2}"), What, From, To);
}

#undef LOCTEXT_NAMESPACE
