#include "GrimoirePanel.h"

#include "ChroniclePanel.h"
#include "LegendPanel.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
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

	/** Los márgenes del panel (16 a cada lado) y lo que se deja a la barra de desplazamiento de una pestaña. */
	constexpr float GrimoirePanelPadding = 16.f;
	constexpr float GrimoireScrollBarRoom = 14.f;

	/**
	 * Los colores de las pestañas: los de la cabecera de la columna (S-035), que repiten el gris del panel de decisión y su fondo negro. Activa: fondo
	 * claro y texto oscuro en negrita; inactiva: fondo oscuro con borde, que se ilumina al pasar el ratón.
	 * TODO(R-HUD-4): el mismo gris está ya en DecisionPanel.cpp (la cabecera de la columna, que también lo repetía, se fue en S-045); con este, dos:
	 * si un tercer panel lo necesita, extraerlo a un sitio común de Presentation y que lo lean de allí.
	 */
	const FLinearColor TabLight = FLinearColor(FColor(190, 190, 190));
	const FLinearColor TabDark = FLinearColor(0.04f, 0.04f, 0.04f, 1.f);
	const FLinearColor TabHover = FLinearColor(TabLight.R * 0.35f, TabLight.G * 0.35f, TabLight.B * 0.35f, 1.f);
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
	using Soulbound::Presentation::EGrimoireTab;
	FontSize = InArgs._FontSize;
	PanelWidth = InArgs._PanelWidth.IsSet() ? InArgs._PanelWidth : TAttribute<float>(Soulbound::Presentation::GrimoireMaxWidth);
	PanelHeight = InArgs._PanelHeight.IsSet() ? InArgs._PanelHeight : TAttribute<float>(600.f);
	ActiveTab = InArgs._ActiveTab.IsSet() ? InArgs._ActiveTab : TAttribute<EGrimoireTab>(EGrimoireTab::You);
	OnTabClicked = InArgs._OnTabClicked;
	DomainColors = InArgs._DomainColors;
	const TAttribute<float> Width = PanelWidth;
	const TAttribute<float> TitleWidth = TAttribute<float>::CreateLambda([Width]() { return FMath::Max(Width.Get() - 2.f * GrimoirePanelPadding, 1.f); });
	const TAttribute<float> PageWidth = TAttribute<float>::CreateSP(this, &SGrimoirePanel::PageTextWidth);
	const TAttribute<int32> Size = FontSize;

	// La cabecera: una caja por pestaña, todas del mismo ancho, como la de la columna (S-035). La caja se queda el clic.
	TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox);
	const TArray<EGrimoireTab>& Tabs = Soulbound::Presentation::GrimoireTabs();
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		const EGrimoireTab Tab = Tabs[Index];
		// La caja interior, para saber si el ratón está encima sin que la caja se tenga a sí misma (un puntero débil).
		const TSharedRef<TWeakPtr<SWidget>> Inner = MakeShared<TWeakPtr<SWidget>>();
		const auto IsHot = [Inner]()
		{
			const TSharedPtr<SWidget> Widget = Inner->Pin();
			return Widget.IsValid() && Widget->IsHovered();
		};
		const TAttribute<EGrimoireTab> Active = ActiveTab;
		const TSharedRef<SBorder> Box = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([Active, Tab, IsHot]() -> FSlateColor { return Active.Get() == Tab ? TabLight : IsHot() ? TabHover : TabDark; })
			.Padding(FMargin(6.f, 4.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Cursor(EMouseCursor::Hand)
			.OnMouseButtonDown_Lambda([this, Tab](const FGeometry&, const FPointerEvent&)
			{
				OnTabClicked.ExecuteIfBound(Tab);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Soulbound::Presentation::GrimoireTabTitle(Tab)))
				.Font_Lambda([Size]() { return FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(Size.Get(), 1)); })
				.ColorAndOpacity_Lambda([Active, Tab, IsHot]()
				{
					return Active.Get() == Tab ? FSlateColor(FLinearColor::Black) : IsHot() ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f));
				})
			];
		*Inner = Box;
		Header->AddSlot().FillWidth(1.f).Padding(Index == 0 ? 0.f : 2.f, 0.f, Index + 1 == Tabs.Num() ? 0.f : 2.f, 0.f)
		[
			// El borde de una caja inactiva: un marco de un píxel del gris claro (en la activa, del mismo color que el fondo).
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(TabLight)
			.Padding(FMargin(1.f))
			[
				Box
			]
		];
	}

	// Una página por pestaña, en el orden de GrimoireTabs. Cada una con su desplazamiento (la crónica ya lleva el suyo).
	const auto Page = [](const TSharedRef<SWidget>& Content) -> TSharedRef<SWidget>
	{
		return SNew(SScrollBox) + SScrollBox::Slot().Padding(0.f, 0.f, GrimoireScrollBarRoom, 0.f)[Content];
	};
	const TAttribute<float> LegendWidth = TAttribute<float>::CreateLambda([PageWidth]() { return PageWidth.Get() + 16.f; });
	ChildSlot
	[
		// Ancho y alto fijos (S-029, R-VIS-21; S-044): el panel mide lo que dice la capa, no lo que ocupen sus textos ni la pestaña activa.
		SNew(SBox)
		.WidthOverride_Lambda([Width]() { return FOptionalSize(Width.Get()); })
		.HeightOverride_Lambda([this]() { return FOptionalSize(FMath::Max(PanelHeight.Get(), 1.f)); })
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.88f))
			.Padding(FMargin(GrimoirePanelPadding, 12.f))
			// S-044: el panel se queda los clics que le caen encima; ninguno llega al tablero.
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					GrimoireText(Size, TitleWidth, FString::Printf(TEXT("GRIMORIO  (%s; la partida sigue)"), *InArgs._KeysHint), FLinearColor::White, true, 1.3f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					Header
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return FMath::Max(Soulbound::Presentation::GrimoireTabs().Find(ActiveTab.Get()), 0); })
					+ SWidgetSwitcher::Slot()[Page(SAssignNew(YouBody, SVerticalBox))]
					+ SWidgetSwitcher::Slot()[Page(SAssignNew(TablePanel, SLegendPanel).FontSize(Size).WrapWidth(LegendWidth).Heading(FString()).Background(false))]
					+ SWidgetSwitcher::Slot()[Page(SAssignNew(BoardPanel, SLegendPanel).FontSize(Size).WrapWidth(LegendWidth).Heading(FString()).Background(false))]
					+ SWidgetSwitcher::Slot()[Page(SAssignNew(HowToPlayText, STextBlock)
						.Font_Lambda([Size]() { return FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(Size.Get(), 1)); })
						.ColorAndOpacity(GrimoireTextColor)
						.WrapTextAt_Lambda([PageWidth]() { return PageWidth.Get(); }))]
					+ SWidgetSwitcher::Slot()[SAssignNew(Chronicle, SChroniclePanel).FontSize(Size)]
				]
			]
		]
	];
}

float SGrimoirePanel::PageTextWidth() const
{
	return FMath::Max(PanelWidth.Get() - 2.f * GrimoirePanelPadding - GrimoireScrollBarRoom, 1.f);
}

void SGrimoirePanel::SetYou(const Soulbound::Presentation::FGrimoireYouView& View)
{
	if (!YouBody.IsValid() || (bHasYou && View == LastYou))
	{
		return;
	}
	bHasYou = true;
	LastYou = View;
	YouBody->ClearChildren();
	const TAttribute<int32> Size = FontSize;
	const TAttribute<float> PageWidth = TAttribute<float>::CreateSP(this, &SGrimoirePanel::PageTextWidth);
	// La tarjeta ocupa el ancho de la página: su texto, el de la página menos los 16 de margen de la tarjeta.
	const TAttribute<float> CardText = TAttribute<float>::CreateLambda([PageWidth]() { return FMath::Max(PageWidth.Get() - 16.f, 1.f); });
	if (View.Row.Seat != INDEX_NONE)
	{
		YouBody->AddSlot().AutoHeight()[Soulbound::Presentation::MakeGrimoireCard(View.Row, Size, DomainColors, CardText)];
	}
	if (!View.InsolenceLine.IsEmpty())
	{
		YouBody->AddSlot().AutoHeight().Padding(4.f, 6.f, 4.f, 0.f)[GrimoireText(Size, PageWidth, View.InsolenceLine, GrimoireTextColor, true)];
	}
	if (!View.HomeRuneLine.IsEmpty())
	{
		YouBody->AddSlot().AutoHeight().Padding(4.f, 4.f, 4.f, 0.f)[GrimoireText(Size, PageWidth, View.HomeRuneLine, FLinearColor::White, true)];
	}
	YouBody->AddSlot().AutoHeight().Padding(4.f, 10.f, 4.f, 2.f)[GrimoireText(Size, PageWidth, TEXT("PODERES (los da cada runa a un Elegido, según su mapa)"), GrimoireTitleColor, true)];
	for (const FString& Line : View.PowerLines)
	{
		YouBody->AddSlot().AutoHeight().Padding(12.f, 0.f, 4.f, 0.f)[GrimoireText(Size, PageWidth, Line, GrimoireTextColor, false)];
	}
}

void SGrimoirePanel::SetTable(const TArray<FLegendRow>& Rows)
{
	if (TablePanel.IsValid())
	{
		TablePanel->SetRows(Rows);
	}
}

void SGrimoirePanel::SetBoard(const TArray<FLegendRow>& Rows)
{
	if (BoardPanel.IsValid())
	{
		BoardPanel->SetRows(Rows);
	}
}

void SGrimoirePanel::SetHowToPlay(const FString& Text)
{
	if (HowToPlayText.IsValid())
	{
		HowToPlayText->SetText(FText::FromString(Text));
	}
}

void SGrimoirePanel::SetChronicle(const TArray<FChronicleLine>& NewestFirst)
{
	if (Chronicle.IsValid())
	{
		Chronicle->SetLines(NewestFirst);
	}
}
