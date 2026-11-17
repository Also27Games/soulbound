#include "ChroniclePanel.h"

#include "ChronicleLines.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SChroniclePanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	TextFont = InArgs._Font;
	const TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Soulbound::Presentation::ChronicleHeadingText))
			.Font_Lambda([this]() { return Soulbound::Presentation::GrimoireFontInfo(TextFont, true, FontSize.Get()); })
			.ColorAndOpacity(FLinearColor::White)
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ScrollBox, SScrollBox)
		];
	// S-048 (corrección 3 del autor del 17/11/2026): sobre la imagen del libro, sin la caja oscura; el texto va directamente sobre la página.
	if (!InArgs._Background)
	{
		ChildSlot[Content];
		return;
	}
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
		.Padding(FMargin(8.f, 6.f))
		[
			Content
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
			.Font_Lambda([this]() { return Soulbound::Presentation::GrimoireFontInfo(TextFont, false, FontSize.Get()); })
			.ColorAndOpacity(Line.Color)
			.AutoWrapText(true)
		];
	}
	ScrollBox->SetScrollOffset(Offset);
}
