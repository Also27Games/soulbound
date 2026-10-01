#include "LegendPanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SLegendPanel::Construct(const FArguments& InArgs)
{
	const int32 FontSize = InArgs._FontSize;
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
	Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("LEYENDA")))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", FontSize))
		.ColorAndOpacity(FLinearColor::White)
	];

	for (const FLegendRow& Row : InArgs._Rows)
	{
		TSharedRef<SWrapBox> Chips = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, 1.f));
		for (const FLegendChip& Chip : Row.Chips)
		{
			Chips->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f, 0.f)
				[
					SNew(SBox).WidthOverride(FontSize + 2.f).HeightOverride(FontSize + 2.f)
					[
						SNew(SColorBlock).Color(Chip.Color)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([Chip]() { return FText::FromString(Chip.TextGetter ? Chip.TextGetter() : Chip.Text); })
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", FontSize))
					.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
				]
			];
		}
		Column->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Row.Title))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", FontSize))
				.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.4f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 0.f, 0.f)
			[
				Chips
			]
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
		.Padding(FMargin(8.f, 6.f))
		[
			Column
		]
	];
}
