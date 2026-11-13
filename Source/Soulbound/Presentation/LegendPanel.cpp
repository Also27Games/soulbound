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
	// S-027 (R-HUD-3): la letra se lee del atributo cada vez que se pinta, para que cambie con la resolución sin rehacer el panel.
	const TAttribute<int32> FontSize = InArgs._FontSize;
	// S-035: el ancho al que se parten los textos. Sale del de la columna menos el margen del panel (8 a cada lado) y, en una ficha, la sangría
	// de la fila (8) y el cuadro de color con su separación (letra + 2 + 3). Con WrapTextAt: AutoWrapText dentro de un SWrapBox cortaba por la derecha.
	const TAttribute<float> ColumnWidth = InArgs._WrapWidth;
	const auto TextWidth = [ColumnWidth](const TAttribute<int32>& Size, bool bChip)
	{
		return [ColumnWidth, Size, bChip]()
		{
			const float Column = ColumnWidth.Get(0.f);
			return Column > 0.f ? FMath::Max(Column - 16.f - (bChip ? 8.f + static_cast<float>(Size.Get()) + 5.f : 0.f), 1.f) : 0.f;
		};
	};
	const auto Font = [FontSize](const ANSICHAR* Style) { return FCoreStyle::GetDefaultFontStyle(Style, FMath::Max(FontSize.Get(), 1)); };
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
	Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("LEYENDA")))
		.Font_Lambda([Font]() { return Font("Bold"); })
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
					SNew(SBox)
					.WidthOverride_Lambda([FontSize]() { return FOptionalSize(FontSize.Get() + 2.f); })
					.HeightOverride_Lambda([FontSize]() { return FOptionalSize(FontSize.Get() + 2.f); })
					[
						SNew(SColorBlock).Color(Chip.Color)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([Chip]() { return FText::FromString(Chip.TextGetter ? Chip.TextGetter() : Chip.Text); })
					.WrapTextAt_Lambda(TextWidth(FontSize, true))
					.Font_Lambda([Font]() { return Font("Regular"); })
					.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
				]
			];
		}
		Column->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				// S-027 (R-HUD-3): el título parte en líneas en vez de cortarse por la derecha cuando la columna es estrecha (1024x768).
				SNew(STextBlock)
				.Text(FText::FromString(Row.Title))
				.Font_Lambda([Font]() { return Font("Bold"); })
				.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.4f))
				.WrapTextAt_Lambda(TextWidth(FontSize, false))
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
