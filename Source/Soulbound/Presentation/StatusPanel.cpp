#include "StatusPanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SStatusPanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	WrapWidth = InArgs._WrapWidth;
	LargeScale = FMath::Max(InArgs._LargeScale, 1.f);
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
	if (!InArgs._Title.IsEmpty())
	{
		TAttribute<int32> Size = FontSize;
		Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InArgs._Title))
			.Font_Lambda([Size]() { return FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(Size.Get(), 1)); })
			.ColorAndOpacity(FLinearColor::White)
		];
	}
	Column->AddSlot().AutoHeight()
	[
		SAssignNew(LinesBox, SVerticalBox)
	];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
		.Padding(FMargin(8.f, 6.f))
		// Sin líneas no hay panel: ni el fondo se ve.
		.Visibility_Lambda([this]() { return Lines.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		[
			Column
		]
	];
}

void SStatusPanel::SetLines(const TArray<Soulbound::Presentation::FPanelLine>& NewLines)
{
	if (!LinesBox.IsValid() || NewLines == Lines)
	{
		return;
	}
	Lines = NewLines;
	LinesBox->ClearChildren();
	for (const Soulbound::Presentation::FPanelLine& Line : Lines)
	{
		const TAttribute<int32> Size = FontSize;
		const float Scale = Line.bLarge ? LargeScale : 1.f;
		const bool bLarge = Line.bLarge;
		// S-040: con WrapWidth, el texto se parte a ese ancho y no al de lo pintado antes; sin él, como siempre. Nunca los dos a la vez.
		const TAttribute<float> Wrap = WrapWidth;
		const bool bFixedWrap = Wrap.IsBound() || Wrap.Get() > 0.f;
		LinesBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Line.Text))
			.Font_Lambda([Size, Scale, bLarge]() { return FCoreStyle::GetDefaultFontStyle(bLarge ? "Bold" : "Regular", FMath::Max(FMath::RoundToInt(Size.Get() * Scale), 1)); })
			.ColorAndOpacity(Line.Color)
			.AutoWrapText(!bFixedWrap)
			.WrapTextAt_Lambda([Wrap, bFixedWrap]() { return bFixedWrap ? FMath::Max(Wrap.Get(), 1.f) : 0.f; })
		];
	}
}
