#include "GrimoirePanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/**
	 * Un texto del Grimorio con la letra del atributo; Scale la agranda (el título). Se parte con WrapTextAt a un ancho que no depende de lo
	 * pintado antes: con AutoWrapText el ancho sale del tamaño de la pasada anterior de pintado, y un texto recién creado cambiaba de tamaño de un
	 * fotograma a otro (S-029, R-VIS-21; el mismo motivo que la frase del Warden, R-HUD-3).
	 */
	TSharedRef<STextBlock> GrimoireText(const TAttribute<int32>& Size, const TAttribute<float>& Wrap, const FString& Text, const FLinearColor& Color, bool bBold, float Scale = 1.f)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font_Lambda([Size, bBold, Scale]() { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::Max(FMath::RoundToInt(Size.Get() * Scale), 1)); })
			.ColorAndOpacity(Color)
			.WrapTextAt_Lambda([Wrap]() { return Wrap.Get(); });
	}

	const FLinearColor GrimoireTitleColor(1.f, 0.85f, 0.4f);
	const FLinearColor GrimoireTextColor(0.9f, 0.9f, 0.9f);
	const FLinearColor GrimoireDimColor(0.55f, 0.55f, 0.55f);
}

TSharedRef<SWidget> Soulbound::Presentation::MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth)
{
	const FString Who = Row.bHuman ? FString::Printf(TEXT("Asiento %d (tú)"), Row.Seat) : FString::Printf(TEXT("Asiento %d"), Row.Seat);
	const FString Domain = Row.DomainId == INDEX_NONE ? FString() : FString::Printf(TEXT("D%d %s"), Row.DomainId, *Row.DomainName).TrimEnd();
	const FString Header = Row.bElect ? FString::Printf(TEXT("%s · ELEGIDO: %s · %s"), *Who, *Row.ElectName, *Domain)
		: Domain.IsEmpty() ? FString::Printf(TEXT("%s · ECO, sin Elegido"), *Who)
		: FString::Printf(TEXT("%s · ECO, sin Elegido · hogar en %s"), *Who, *Domain);
	// Las líneas de las runas llevan el cuadrito del Dominio y una sangría delante: su texto se parte un poco antes.
	const TAttribute<float> RuneWidth = TAttribute<float>::CreateLambda([FontSize, TextWidth]() { return FMath::Max(TextWidth.Get() - FontSize.Get() - 14.f, 1.f); });

	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox);
	Card->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f)[GrimoireText(FontSize, TextWidth, Header, Row.Color, true)];
	Card->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
	[
		Row.bElect ? GrimoireText(FontSize, TextWidth, FString::Printf(TEXT("Afinidad: %s. %s"), *Row.AffinityName, *Row.AffinityText), GrimoireTextColor, false)
			: GrimoireText(FontSize, TextWidth, TEXT("Sin afinidad."), GrimoireDimColor, false)
	];
	for (const FGrimoireRune& Rune : Row.Runes)
	{
		// S-035 (R-RUN-7): con los tipos ocultos no hay cantidad que decir (un guion diría "ninguna"): solo el mapa de runa a poder, que es público.
		const FString Count = Row.bTypesHidden ? FString(TEXT("")) : Rune.Count > 0 ? FString::Printf(TEXT("×%d"), Rune.Count) : FString(TEXT("—"));
		const FString Power = !Rune.bHasPower ? FString() : FString::Printf(TEXT(": %s%s"), *Rune.PowerName, Rune.bActive ? TEXT(" (ACTIVO)") : TEXT(""));
		const FLinearColor Chip = DomainColors.IsValidIndex(Rune.RuneType) ? DomainColors[Rune.RuneType] : FLinearColor::Gray;
		const TAttribute<int32> Size = FontSize;
		Card->AddSlot().AutoHeight().Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride_Lambda([Size]() { return FOptionalSize(Size.Get() + 2.f); })
				.HeightOverride_Lambda([Size]() { return FOptionalSize(Size.Get() + 2.f); })
				[
					SNew(SColorBlock).Color(Chip)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				GrimoireText(FontSize, RuneWidth, FString::Printf(TEXT("D%d%s%s"), Rune.RuneType, *(Count.IsEmpty() ? Count : FString(TEXT(" ")) + Count), *Power),
					Rune.bActive ? FLinearColor::White : Row.bTypesHidden || Rune.Count > 0 ? GrimoireTextColor : GrimoireDimColor, Rune.bActive)
			]
		];
	}
	Card->AddSlot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
	[
		GrimoireText(FontSize, TextWidth, Row.bTypesHidden ? FString::Printf(TEXT("%d runa%s; sus tipos están ocultos"), Row.TotalRunes, Row.TotalRunes == 1 ? TEXT("") : TEXT("s"))
			: FString::Printf(TEXT("Poderes activos: %s"), Row.ActivePowers.IsEmpty() ? TEXT("ninguno") : *FString::Join(Row.ActivePowers, TEXT(", "))),
			GrimoireTitleColor, false)
	];
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.05f))
		.Padding(FMargin(8.f, 6.f))
		[
			Card
		];
}

void SGrimoirePanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	PanelWidth = InArgs._PanelWidth.IsSet() ? InArgs._PanelWidth : TAttribute<float>(Soulbound::Presentation::GrimoireMaxWidth);
	DomainColors = InArgs._DomainColors;
	const TAttribute<float> Width = PanelWidth;
	const TAttribute<float> TitleWidth = TAttribute<float>::CreateLambda([Width]() { return Soulbound::Presentation::GrimoireCardTextWidth(Width.Get(), 1); });
	// Ancho fijo (S-029, R-VIS-21): el panel mide lo que dice la capa, no lo que ocupen sus textos.
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride_Lambda([Width]() { return FOptionalSize(Width.Get()); })
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.88f))
			.Padding(FMargin(16.f, 12.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					GrimoireText(FontSize, TitleWidth, FString::Printf(TEXT("GRIMORIO  (%s para cerrar; la partida sigue)"), *InArgs._CloseKeys), FLinearColor::White, true, 1.3f)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(Body, SVerticalBox)
				]
			]
		]
	];
}

void SGrimoirePanel::SetContent(const TArray<Soulbound::Presentation::FGrimoireRow>& Rows, const TArray<FString>& PowerLines)
{
	if (!Body.IsValid())
	{
		return;
	}
	// R-VIS-21: lo mismo que ya se pinta no se toca (como SDecisionPanel::SameLook). Cada reconstrucción estrenaba textos sin ancho conocido.
	if (bHasContent && Rows == LastRows && PowerLines == LastPowerLines)
	{
		return;
	}
	bHasContent = true;
	LastRows = Rows;
	LastPowerLines = PowerLines;
	Body->ClearChildren();
	const TAttribute<float> Width = PanelWidth;
	const TAttribute<float> CardText = TAttribute<float>::CreateLambda([Width]() { return Soulbound::Presentation::GrimoireCardTextWidth(Width.Get(), 2); });
	const TAttribute<float> FullText = TAttribute<float>::CreateLambda([Width]() { return Soulbound::Presentation::GrimoireCardTextWidth(Width.Get(), 1); });
	// Dos tarjetas por fila: cuatro asientos caben a 1024x768.
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4.f));
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		Grid->AddSlot(Index % 2, Index / 2)[Soulbound::Presentation::MakeGrimoireCard(Rows[Index], FontSize, DomainColors, CardText)];
	}
	Body->AddSlot().AutoHeight()[Grid];
	Body->AddSlot().AutoHeight().Padding(4.f, 8.f, 4.f, 2.f)[GrimoireText(FontSize, FullText, TEXT("PODERES (los da cada runa a un Elegido, según su mapa)"), GrimoireTitleColor, true)];
	for (const FString& Line : PowerLines)
	{
		Body->AddSlot().AutoHeight().Padding(12.f, 0.f, 4.f, 0.f)[GrimoireText(FontSize, FullText, Line, GrimoireTextColor, false)];
	}
}
