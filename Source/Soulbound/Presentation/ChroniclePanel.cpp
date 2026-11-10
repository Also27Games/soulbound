#include "ChroniclePanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SChroniclePanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
		.Padding(FMargin(8.f, 6.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("CRONICA (la ultima arriba; rueda para ir al inicio)")))
				.Font_Lambda([this]() { return FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(FontSize.Get(), 1)); })
				.ColorAndOpacity(FLinearColor::White)
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(ScrollBox, SScrollBox)
			]
		]
	];
}

void SChroniclePanel::SetLines(const TArray<FChronicleLine>& NewestFirst)
{
	if (!ScrollBox.IsValid())
	{
		return;
	}
	const float Offset = ScrollBox->GetScrollOffset();
	ScrollBox->ClearChildren();
	for (const FChronicleLine& Line : NewestFirst)
	{
		ScrollBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Line.Text))
			.Font_Lambda([this]() { return FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(FontSize.Get(), 1)); })
			.ColorAndOpacity(Line.Color)
			.AutoWrapText(true)
		];
	}
	ScrollBox->SetScrollOffset(Offset);
}
