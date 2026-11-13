#include "ElectCardPanel.h"

#include "GrimoirePanel.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Un texto de la carta, partido con WrapTextAt a un ancho que no depende de lo pintado antes (como el Grimorio, R-VIS-21). */
	TSharedRef<STextBlock> CardText(const TAttribute<int32>& Size, const TAttribute<float>& Wrap, const FString& Text, const FLinearColor& Color, bool bBold, float Scale)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font_Lambda([Size, bBold, Scale]() { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::Max(FMath::RoundToInt(Size.Get() * Scale), 1)); })
			.ColorAndOpacity(Color)
			.WrapTextAt_Lambda([Wrap]() { return Wrap.Get(); });
	}
}

void SElectCardPanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	PanelWidth = InArgs._PanelWidth.IsSet() ? InArgs._PanelWidth : TAttribute<float>(480.f);
	DomainColors = InArgs._DomainColors;
	const TAttribute<float> Width = PanelWidth;
	const TAttribute<float> Text = TAttribute<float>::CreateLambda([Width]() { return Soulbound::Presentation::GrimoireCardTextWidth(Width.Get(), 1); });
	const TAttribute<int32> Size = FontSize;
	const FSimpleDelegate OnDismissed = InArgs._OnDismissed;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride_Lambda([Width]() { return FOptionalSize(Width.Get()); })
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			.Padding(FMargin(16.f, 12.f))
			// Un clic en el panel lo cierra y no pasa al tablero.
			.OnMouseButtonDown_Lambda([OnDismissed](const FGeometry&, const FPointerEvent&)
			{
				OnDismissed.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					CardText(Size, Text, TEXT("TE HAS VINCULADO"), FLinearColor(1.f, 0.85f, 0.4f), true, 1.3f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
				[
					SAssignNew(Body, SVerticalBox)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					CardText(Size, Text, FString::Printf(TEXT("Clic para cerrar · %s abre el Grimorio"), *InArgs._GrimoireKey), FLinearColor(0.55f, 0.55f, 0.55f), false, 1.f)
				]
			]
		]
	];
}

void SElectCardPanel::SetRow(const Soulbound::Presentation::FGrimoireRow& Row)
{
	if (!Body.IsValid() || (bHasRow && Row == LastRow))
	{
		return;
	}
	bHasRow = true;
	LastRow = Row;
	Body->ClearChildren();
	const TAttribute<float> Width = PanelWidth;
	const TAttribute<float> Text = TAttribute<float>::CreateLambda([Width]() { return Soulbound::Presentation::GrimoireCardTextWidth(Width.Get(), 1); });
	Body->AddSlot().AutoHeight()[Soulbound::Presentation::MakeGrimoireCard(Row, FontSize, DomainColors, Text)];
}
