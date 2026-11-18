#include "GrimoirePanel.h"

#include "ChroniclePanel.h"
#include "LegendPanel.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/**
	 * Un texto del Grimorio con la letra del atributo; Scale la agranda (el título). Se parte con WrapTextAt a un ancho que no depende de lo
	 * pintado antes: con AutoWrapText el ancho sale del tamaño de la pasada anterior de pintado, y un texto recién creado cambiaba de tamaño de un
	 * fotograma a otro (S-029, R-VIS-21; el mismo motivo que la frase del Warden, R-HUD-3).
	 */
	TSharedRef<STextBlock> GrimoireText(const Soulbound::Presentation::FGrimoireFont& Font, const TAttribute<int32>& Size, const TAttribute<float>& Wrap, const FString& Text,
		const FLinearColor& Color, bool bBold, float Scale = 1.f)
	{
		// S-048 (corrección 4 del autor del 17/11/2026): con la fuente del Grimorio si la hay; sin ella, la de hoy.
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font_Lambda([Font, Size, bBold, Scale]() { return Soulbound::Presentation::GrimoireFontInfo(Font, bBold, FMath::RoundToInt(Size.Get() * Scale)); })
			.ColorAndOpacity(Color)
			.WrapTextAt_Lambda([Wrap]() { return Wrap.Get(); });
	}

	/** La línea de teclas del pie del Grimorio (S-048, corrección 5): como GrimoireText, con un texto que se lee al pintar, porque cambia con la pestaña. */
	TSharedRef<STextBlock> GrimoireFootText(const Soulbound::Presentation::FGrimoireFont& Font, const TAttribute<int32>& Size, const TAttribute<float>& Wrap,
		const TAttribute<FText>& Text, const FLinearColor& Color, bool bBold)
	{
		const TSharedRef<STextBlock> Block = GrimoireText(Font, Size, Wrap, FString(), Color, bBold);
		Block->SetText(Text);
		return Block;
	}

	/** Un SScrollBox de pestaña con la barra que reserva GrimoireColumnTextWidth (S-048, corrección 1): su grosor, su margen y el hueco hasta el texto. */
	TSharedRef<SWidget> GrimoireScroll(const TSharedRef<SWidget>& Content)
	{
		using namespace Soulbound::Presentation;
		return SNew(SScrollBox)
			.ScrollBarThickness(FVector2D(GrimoireScrollBarThickness, GrimoireScrollBarThickness))
			.ScrollBarPadding(FMargin(GrimoireScrollBarPadding))
			+ SScrollBox::Slot().Padding(0.f, 0.f, GrimoireScrollGap, 0.f)[Content];
	}

	/**
	 * El marco de un botón de página o de una pestaña (S-048, correcciones tras probar la maquetación del 18/11/2026, R-HUD-18 y R-HUD-10): una caja
	 * del color del marco con el contenido dentro, a GrimoireFrameWidth de su propia escala pintada; así cada lado mide al menos un píxel y se ve.
	 */
	TSharedRef<SBorder> GrimoireFrame(const TAttribute<FSlateColor>& Color, const TSharedRef<SWidget>& Content)
	{
		const TSharedRef<TWeakPtr<SWidget>> Self = MakeShared<TWeakPtr<SWidget>>();
		const TSharedRef<SBorder> Frame = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Color)
			.Padding_Lambda([Self]()
			{
				const TSharedPtr<SWidget> Widget = Self->Pin();
				return FMargin(Soulbound::Presentation::GrimoireFrameWidth(Widget.IsValid() ? Widget->GetCachedGeometry().Scale : 1.f));
			})
			[
				Content
			];
		*Self = Frame;
		return Frame;
	}

	/** La sangría de la tarjeta del jugador, izquierda y derecha sumadas (S-048, corrección 1). */
	constexpr float GrimoireCardIndent = 16.f;

	/**
	 * La sangría de la descripción de un poder bajo su runa, además de la del cuadrito (S-048, ronda de maquetación del 18/11/2026; valor inicial
	 * propuesto por el agente), y su gris, más claro que el de lo apagado para que se lea.
	 */
	constexpr float GrimoirePowerTextIndent = 6.f;
	const FLinearColor GrimoirePowerTextColor(0.68f, 0.68f, 0.68f);

	/**
	 * Los bloques de una página de "Cómo se juega" (S-048, ampliación del autor del 17/11/2026, valores iniciales propuestos por el agente, que se
	 * ajustan en PIE): el hueco entre dos bloques, el que hay entre dos elementos de lista seguidos y la sangría de la marca de la lista.
	 */
	constexpr float HowToPlayBlockGap = 8.f;
	constexpr float HowToPlayListGap = 2.f;
	constexpr float HowToPlayListIndent = 12.f;
	/** El hueco de más encima de un subtítulo (S-048, segunda ampliación del autor del 17/11/2026; valor inicial propuesto por el agente). */
	constexpr float HowToPlaySubtitleGap = 4.f;

	/** Los estilos del texto enriquecido de "Cómo se juega" (S-048, la negrita): el de los párrafos y el de la frase del Warden; los otros, sus marcas. */
	const FName HowToPlayNormalStyle(TEXT("Normal"));
	const FName HowToPlayWardenStyle(TEXT("Warden"));
	const FName HowToPlaySubtitleStyle(TEXT("Subtitle"));

	/** Cuánto más pequeña va la línea de teclas del pie del Grimorio (S-048, corrección 2 del autor del 17/11/2026). */
	constexpr float GrimoireFootScale = 0.85f;

	const FLinearColor GrimoireTitleColor(1.f, 0.85f, 0.4f);
	const FLinearColor GrimoireTextColor(0.9f, 0.9f, 0.9f);
	const FLinearColor GrimoireDimColor(0.55f, 0.55f, 0.55f);
	/** El color de la línea de teclas del pie (S-048, corrección 2): un gris claro, más discreto que el texto. */
	const FLinearColor GrimoireFootColor(0.7f, 0.7f, 0.7f);

	/**
	 * Los márgenes del panel sin la imagen del libro (16 a los lados y 12 arriba y abajo; desde S-048, en GrimoireBook.h, porque son la zona de contenido
	 * de hoy de GrimoireBookLayout) y lo que se deja a la barra de desplazamiento de una pestaña.
	 */
	const FVector2f GrimoireFlatPadding = Soulbound::Presentation::GrimoireFlatPadding;

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

void Soulbound::Presentation::GrimoireOnPress(const TSharedRef<SWidget>& Widget, TFunction<void()> OnPress)
{
	// Retoques del autor del 18/11/2026: con dos clics rápidos, Slate entrega el segundo solo como doble clic (FSlateApplication::
	// ProcessMouseButtonDoubleClickEvent no lo repite como pulsación); los dos llevan a la misma acción. Cualquier botón del ratón, como antes.
	const FPointerEventHandler Press = FPointerEventHandler::CreateLambda([OnPress](const FGeometry&, const FPointerEvent&)
	{
		if (OnPress)
		{
			OnPress();
		}
		return FReply::Handled();
	});
	Widget->SetOnMouseButtonDown(Press);
	Widget->SetOnMouseDoubleClick(Press);
}

TSharedRef<SWidget> Soulbound::Presentation::MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth,
	const FGrimoireFont& Font, bool bBackground, const FString& InsolenceLine, bool bPowerTexts)
{
	const FString Who = Row.bHuman ? FString::Printf(TEXT("Asiento %d (tú)"), Row.Seat) : FString::Printf(TEXT("Asiento %d"), Row.Seat);
	const FString Domain = Row.DomainId == INDEX_NONE ? FString() : FString::Printf(TEXT("D%d %s"), Row.DomainId, *Row.DomainName).TrimEnd();
	const FString Header = Row.bElect ? FString::Printf(TEXT("%s · ELEGIDO: %s · %s"), *Who, *Row.ElectName, *Domain)
		: Domain.IsEmpty() ? FString::Printf(TEXT("%s · ECO, sin Elegido"), *Who)
		: FString::Printf(TEXT("%s · ECO, sin Elegido · hogar en %s"), *Who, *Domain);
	// Las líneas de las runas llevan el cuadrito del Dominio y una sangría delante: su texto se parte un poco antes.
	const TAttribute<float> RuneWidth = TAttribute<float>::CreateLambda([FontSize, TextWidth]() { return FMath::Max(TextWidth.Get() - FontSize.Get() - 14.f, 1.f); });

	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox);
	Card->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f)[GrimoireText(Font, FontSize, TextWidth, Header, Row.Color, true)];
	Card->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
	[
		Row.bElect ? GrimoireText(Font, FontSize, TextWidth, FString::Printf(TEXT("Afinidad: %s. %s"), *Row.AffinityName, *Row.AffinityText), GrimoireTextColor, false)
			: GrimoireText(Font, FontSize, TextWidth, TEXT("Sin afinidad."), GrimoireDimColor, false)
	];
	// S-048 (ronda de maquetación del 18/11/2026): en la hoja del jugador, la insolencia tras la afinidad.
	if (!InsolenceLine.IsEmpty())
	{
		Card->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)[GrimoireText(Font, FontSize, TextWidth, InsolenceLine, GrimoireTextColor, true)];
	}
	// La descripción de un poder va sangrada bajo el texto de su runa: el cuadrito, su hueco y un poco más.
	const TAttribute<float> PowerTextWidth = TAttribute<float>::CreateLambda([RuneWidth]() { return FMath::Max(RuneWidth.Get() - GrimoirePowerTextIndent, 1.f); });
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
				GrimoireText(Font, FontSize, RuneWidth, FString::Printf(TEXT("D%d%s%s"), Rune.RuneType, *(Count.IsEmpty() ? Count : FString(TEXT(" ")) + Count), *Power),
					Rune.bActive ? FLinearColor::White : Row.bTypesHidden || Rune.Count > 0 ? GrimoireTextColor : GrimoireDimColor, Rune.bActive)
			]
		];
		// S-048 (ronda de maquetación, decisión del autor del 18/11/2026): debajo de su runa, sangrada, en gris y al tamaño de siempre, qué hace el poder.
		// Una runa sin poder no lleva nada.
		if (bPowerTexts && Rune.bHasPower && !Rune.PowerText.IsEmpty())
		{
			Card->AddSlot().AutoHeight().Padding(8.f, 0.f, 0.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride_Lambda([Size]() { return FOptionalSize(Size.Get() + 2.f + 4.f + GrimoirePowerTextIndent); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					GrimoireText(Font, FontSize, PowerTextWidth, Rune.PowerText, GrimoirePowerTextColor, false)
				]
			];
		}
	}
	Card->AddSlot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
	[
		GrimoireText(Font, FontSize, TextWidth, Row.bTypesHidden ? FString::Printf(TEXT("%d runa%s; sus tipos están ocultos"), Row.TotalRunes, Row.TotalRunes == 1 ? TEXT("") : TEXT("s"))
			: FString::Printf(TEXT("Poderes activos: %s"), Row.ActivePowers.IsEmpty() ? TEXT("ninguno") : *FString::Join(Row.ActivePowers, TEXT(", "))),
			GrimoireTitleColor, false)
	];
	// S-048 (corrección 3 del autor del 17/11/2026): sobre la imagen del libro, sin fondo; el margen se queda, para que el texto se parta igual.
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(bBackground ? FLinearColor(1.f, 1.f, 1.f, 0.05f) : FLinearColor::Transparent)
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
	ActiveTab = InArgs._ActiveTab.IsSet() ? InArgs._ActiveTab : TAttribute<EGrimoireTab>(EGrimoireTab::Table);
	OnTabClicked = InArgs._OnTabClicked;
	DomainColors = InArgs._DomainColors;
	BookBrush = InArgs._BookBrush;
	if (BookBrush.IsSet())
	{
		// S-048 (R-HUD-20): el tamaño de la textura y sus márgenes; al pintar, el pincel se ajusta al tamaño que da GrimoireBookLayout.
		BookImageSize = FVector2f(BookBrush->ImageSize.X, BookBrush->ImageSize.Y);
		BookMargin = BookBrush->Margin;
		BookBrush->DrawAs = ESlateBrushDrawType::Box;
	}
	PageInset = InArgs._PageInset;
	SpineGap = InArgs._SpineGap;
	ImageMaxHeight = InArgs._ImageMaxHeight.IsSet() ? InArgs._ImageMaxHeight : TAttribute<float>(0.6f);
	WatermarkBrush = InArgs._Watermark;
	WatermarkOpacity = InArgs._WatermarkOpacity;
	WatermarkSize = InArgs._WatermarkSize;
	WardenColor = InArgs._WardenColor;
	ImageForPage = InArgs._ImageForPage;
	OnPreviousPage = InArgs._OnPreviousPage;
	OnNextPage = InArgs._OnNextPage;
	TextFont = InArgs._Font;
	const bool bBook = BookBrush.IsSet();

	const TAttribute<float> Width = PanelWidth;
	const TAttribute<int32> Size = FontSize;
	const TAttribute<int32> FootSize = TAttribute<int32>::CreateLambda([Size]() { return FMath::Max(FMath::RoundToInt(Size.Get() * GrimoireFootScale), 1); });
	// Corrección 5 del autor del 17/11/2026: las teclas del pie, según la pestaña activa (las de página, solo en "Cómo se juega").
	const Soulbound::Presentation::FGrimoireKeyNames Keys = InArgs._KeyNames;
	const TAttribute<EGrimoireTab> Tab = ActiveTab;
	const auto FootText = [Keys, Tab](TFunction<FString(const Soulbound::Presentation::FGrimoireFootHints&)> Part)
	{
		return TAttribute<FText>::CreateLambda([Keys, Tab, Part]() { return FText::FromString(Part(Soulbound::Presentation::GrimoireFootHints(Keys, Tab.Get()))); });
	};
	// Corrección 7: un margen entre el final de la zona de contenido y el pie, el mismo en todas las pestañas.
	const float FootGap = Soulbound::Presentation::GrimoireFootGap;

	// Una página por pestaña, en el orden de GrimoireTabs. Cada una con su desplazamiento (la crónica ya lleva el suyo).
	const auto Page = [](const TSharedRef<SWidget>& Content) -> TSharedRef<SWidget> { return GrimoireScroll(Content); };
	// S-048 (corrección 1): la leyenda parte sus textos al ancho que le queda dentro de su columna, la izquierda o la derecha.
	const auto MakeLegend = [this, Size](TSharedPtr<SLegendPanel>& Out, bool bRight) -> TSharedRef<SWidget>
	{
		return SAssignNew(Out, SLegendPanel).FontSize(Size).WrapWidth(TextWidth(bRight, 0.f)).Heading(FString()).Background(false).Font(TextFont);
	};
	const auto WidgetIndex = [this]() { return FMath::Max(Soulbound::Presentation::GrimoireTabs().Find(ActiveTab.Get()), 0); };
	SAssignNew(HowToPlayLeft, SBox);
	// S-048 (corrección 3 del autor del 17/11/2026): sobre la imagen del libro, la crónica sin su caja oscura.
	SAssignNew(Chronicle, SChroniclePanel).FontSize(Size).Background(!bBook).Font(TextFont);

	// El fondo del panel (S-044): se queda los clics que le caen encima, para que ninguno llegue al tablero; desde los retoques del 18/11/2026, también
	// los dobles clics (decisión del autor), con GrimoireOnPress y una acción vacía.
	TSharedPtr<SBorder> Background;
	if (!bBook)
	{
		// Sin la imagen del libro, como antes (S-044): una columna, sobre el fondo negro. Desde la corrección 2 del autor del 17/11/2026, el título
		// dice solo "Grimorio" y las teclas van en una línea pequeña al pie, entera.
		const TAttribute<float> TitleWidth = TAttribute<float>::CreateLambda([Width]() { return FMath::Max(Width.Get() - 2.f * GrimoireFlatPadding.X, 1.f); });
		ChildSlot
		[
			// Ancho y alto fijos (S-029, R-VIS-21; S-044): el panel mide lo que dice la capa, no lo que ocupen sus textos ni la pestaña activa.
			SNew(SBox)
			.WidthOverride_Lambda([Width]() { return FOptionalSize(Width.Get()); })
			.HeightOverride_Lambda([this]() { return FOptionalSize(FMath::Max(PanelHeight.Get(), 1.f)); })
			[
				SAssignNew(Background, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.88f))
				.Padding(FMargin(GrimoireFlatPadding.X, GrimoireFlatPadding.Y))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
					[
						GrimoireText(TextFont, Size, TitleWidth, Soulbound::Presentation::GrimoireTitleText, FLinearColor::White, true, 1.3f)
					]
					// Correcciones tras probar la maquetación (18/11/2026): entre las pestañas y el contenido, GrimoireHeaderGap.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, Soulbound::Presentation::GrimoireHeaderGap)
					[
						MakeTabHeader(Soulbound::Presentation::GrimoireTabs())
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda(WidgetIndex)
						// Ronda de maquetación de S-048 (18/11/2026): sin el libro, una columna; primero el jugador y debajo la mesa.
						+ SWidgetSwitcher::Slot()
						[
							Page(SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SAssignNew(PlayerBody, SVerticalBox)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[MakeLegend(TablePanel, false)])
						]
						+ SWidgetSwitcher::Slot()[HowToPlayLeft.ToSharedRef()]
						+ SWidgetSwitcher::Slot()[Chronicle.ToSharedRef()]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, FootGap, 0.f, 0.f)
					[
						GrimoireFootText(TextFont, FootSize, TitleWidth, FootText([](const Soulbound::Presentation::FGrimoireFootHints& Foot) { return Foot.OneLine(); }),
							GrimoireFootColor, false)
					]
				]
			]
		];
		Soulbound::Presentation::GrimoireOnPress(Background.ToSharedRef(), TFunction<void()>());
		return;
	}

	// S-048 (R-HUD-20): sobre la imagen del libro abierto. El contenido va dentro de sus páginas (GrimoireBookLayout) y se reparte en dos columnas,
	// una por página, con el hueco del lomo entre las dos: nada lo cruza. La cabecera también se reparte (ronda de maquetación, decisión del autor del
	// 18/11/2026): el título a la izquierda y las tres pestañas a la derecha, a la misma altura.
	SAssignNew(HowToPlayRight, SBox);
	const TAttribute<float> LeftWidth = TAttribute<float>::CreateLambda([this]() { return ColumnWidth(false); });
	const TAttribute<float> RightWidth = TAttribute<float>::CreateLambda([this]() { return ColumnWidth(true); });
	const TAttribute<float> SpineWidth = TAttribute<float>::CreateLambda([this]()
	{
		const Soulbound::Presentation::FGrimoireBookLayout Book = Layout();
		return FMath::Max(Book.RightPage.Min.X - Book.LeftPage.Max.X, 0.f);
	});
	// Las dos columnas de una fila: la de la izquierda mide la página izquierda; el hueco, el lomo; la derecha, lo que queda.
	const auto Columns = [LeftWidth, SpineWidth](const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right, EVerticalAlignment RightAlign) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride_Lambda([LeftWidth]() { return FOptionalSize(LeftWidth.Get()); })[Left]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride_Lambda([SpineWidth]() { return FOptionalSize(SpineWidth.Get()); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(RightAlign)[Right];
	};
	// Corrección 2 del autor del 17/11/2026: el título, solo "Grimorio"; sin barra de desplazamiento, se parte al ancho de su página.
	const TAttribute<float> TitleWidth = LeftWidth;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride_Lambda([Width]() { return FOptionalSize(Width.Get()); })
		.HeightOverride_Lambda([this]() { return FOptionalSize(FMath::Max(PanelHeight.Get(), 1.f)); })
		[
			SAssignNew(Background, SBorder)
			.BorderImage_Lambda([this]() -> const FSlateBrush*
			{
				// La imagen se pinta a su escala uniforme: sus bordes miden lo mismo en los dos ejes y no se deforman.
				BookBrush->ImageSize = Layout().BrushSize;
				return &BookBrush.GetValue();
			})
			.Padding_Lambda([this]()
			{
				const FVector2f Panel = PanelSize();
				const FBox2f Content = Layout().Content;
				return FMargin(Content.Min.X, Content.Min.Y, FMath::Max(Panel.X - Content.Max.X, 0.f), FMath::Max(Panel.Y - Content.Max.Y, 0.f));
			})
			[
				// Segunda ampliación del autor del 17/11/2026 (R-HUD-20): detrás de todo, la marca de agua de cada hoja, en "Cómo se juega".
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SOverlay)
					.Visibility(EVisibility::HitTestInvisible)
					+ SOverlay::Slot()[MakeWatermark(false)]
					+ SOverlay::Slot()[MakeWatermark(true)]
				]
				+ SOverlay::Slot()
				[
				SNew(SVerticalBox)
				// Correcciones tras probar la maquetación (18/11/2026): entre la fila del título y las pestañas y las hojas, GrimoireHeaderGap.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, Soulbound::Presentation::GrimoireHeaderGap)
				[
					Columns(
						GrimoireText(TextFont, Size, TitleWidth, Soulbound::Presentation::GrimoireTitleText, FLinearColor::White, true, 1.3f),
						MakeTabHeader(Soulbound::Presentation::GrimoireTabs()),
						VAlign_Bottom)
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					Columns(
						// Ronda de maquetación de S-048 (18/11/2026): "La mesa", con la hoja del jugador a la izquierda y la mesa a la derecha.
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda(WidgetIndex)
						+ SWidgetSwitcher::Slot()[Page(SAssignNew(PlayerBody, SVerticalBox))]
						+ SWidgetSwitcher::Slot()[HowToPlayLeft.ToSharedRef()]
						// Precisión del autor del 17/11/2026: la crónica, entera en la página izquierda.
						+ SWidgetSwitcher::Slot()[Chronicle.ToSharedRef()],
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda(WidgetIndex)
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(TablePanel, true))]
						+ SWidgetSwitcher::Slot()[HowToPlayRight.ToSharedRef()]
						+ SWidgetSwitcher::Slot()[SNew(SSpacer)],
						VAlign_Fill)
				]
				// Corrección 2 del autor del 17/11/2026: las teclas, en una línea pequeña al pie; cerrar y pestañas a la izquierda, páginas a la derecha
				// (corrección 5: solo en "Cómo se juega"; en las demás, vacío).
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, FootGap, 0.f, 0.f)
				[
					Columns(
						GrimoireFootText(TextFont, FootSize, LeftWidth, FootText([](const Soulbound::Presentation::FGrimoireFootHints& Foot) { return Foot.Left; }), GrimoireFootColor, false),
						SNew(SBox).HAlign(HAlign_Right)
						[
							GrimoireFootText(TextFont, FootSize, RightWidth, FootText([](const Soulbound::Presentation::FGrimoireFootHints& Foot) { return Foot.Right; }), GrimoireFootColor, false)
						],
						VAlign_Top)
				]
				]
			]
		]
	];
	Soulbound::Presentation::GrimoireOnPress(Background.ToSharedRef(), TFunction<void()>());
}

TSharedRef<SWidget> SGrimoirePanel::MakeWatermark(bool bRight)
{
	if (!WatermarkBrush.IsSet())
	{
		return SNullWidget::NullWidget;
	}
	// El sitio de la marca, en coordenadas del panel (GrimoireWatermarks); el contenido empieza en Content.Min, así que se resta.
	const TWeakPtr<SGrimoirePanel> Weak = SharedThis(this);
	const auto Mark = [Weak, bRight]() -> TOptional<FBox2f>
	{
		const TSharedPtr<SGrimoirePanel> Panel = Weak.Pin();
		if (!Panel.IsValid() || !Panel->WatermarkBrush.IsSet())
		{
			return TOptional<FBox2f>();
		}
		const Soulbound::Presentation::FGrimoireBookLayout Book = Panel->Layout();
		const FVector2f Texture(Panel->WatermarkBrush->ImageSize.X, Panel->WatermarkBrush->ImageSize.Y);
		const Soulbound::Presentation::FGrimoireWatermarks Marks = Soulbound::Presentation::GrimoireWatermarks(Book, Texture, Panel->WatermarkSize);
		if (!Marks.bShown)
		{
			return TOptional<FBox2f>();
		}
		const FBox2f Box = bRight ? Marks.Right : Marks.Left;
		return FBox2f(Box.Min - Book.Content.Min, Box.Max - Book.Content.Min);
	};
	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding_Lambda([Mark]() { const TOptional<FBox2f> Box = Mark(); return Box.IsSet() ? FMargin(Box->Min.X, Box->Min.Y, 0.f, 0.f) : FMargin(0.f); })
		// Solo en "Cómo se juega"; siempre, lleve la página imágenes o no, también en la hoja vacía de la última vista.
		.Visibility_Lambda([this, Mark]()
		{
			return ActiveTab.Get() == Soulbound::Presentation::EGrimoireTab::HowToPlay && Mark().IsSet() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SBox)
			.WidthOverride_Lambda([Mark]() { const TOptional<FBox2f> Box = Mark(); return FOptionalSize(Box.IsSet() ? Box->GetSize().X : 0.f); })
			.HeightOverride_Lambda([Mark]() { const TOptional<FBox2f> Box = Mark(); return FOptionalSize(Box.IsSet() ? Box->GetSize().Y : 0.f); })
			[
				SNew(SImage)
				.Image(&WatermarkBrush.GetValue())
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(WatermarkOpacity, 0.f, 1.f)))
			]
		];
}

TSharedRef<SWidget> SGrimoirePanel::MakeTabHeader(const TArray<Soulbound::Presentation::EGrimoireTab>& Tabs)
{
	using Soulbound::Presentation::EGrimoireTab;
	const TAttribute<int32> Size = FontSize;
	// La cabecera: una caja por pestaña, todas del mismo ancho, como la de la columna (S-035). La caja se queda el clic.
	TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox);
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
			[
				SNew(STextBlock)
				.Text(FText::FromString(Soulbound::Presentation::GrimoireTabTitle(Tab)))
				.Font_Lambda([this, Size]() { return Soulbound::Presentation::GrimoireFontInfo(TextFont, true, Size.Get()); })
				.ColorAndOpacity_Lambda([Active, Tab, IsHot]()
				{
					return Active.Get() == Tab ? FSlateColor(FLinearColor::Black) : IsHot() ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f));
				})
			];
		*Inner = Box;
		// Retoques del 18/11/2026: cada clic elige la pestaña, también los rápidos (el segundo llega como doble clic).
		Soulbound::Presentation::GrimoireOnPress(Box, [this, Tab]() { OnTabClicked.ExecuteIfBound(Tab); });
		Header->AddSlot().FillWidth(1.f).Padding(Index == 0 ? 0.f : 2.f, 0.f, Index + 1 == Tabs.Num() ? 0.f : 2.f, 0.f)
		[
			// El borde de una caja inactiva: un marco del gris claro (en la activa, del mismo color que el fondo), de al menos un píxel de pantalla
			// (correcciones tras probar la maquetación, GrimoireFrame).
			GrimoireFrame(FSlateColor(TabLight), Box)
		];
	}
	return Header;
}

FVector2f SGrimoirePanel::PanelSize() const
{
	return FVector2f(FMath::Max(PanelWidth.Get(), 1.f), FMath::Max(PanelHeight.Get(), 1.f));
}

Soulbound::Presentation::FGrimoireBookLayout SGrimoirePanel::Layout() const
{
	return Soulbound::Presentation::GrimoireBookLayout(PanelSize(), BookBrush.IsSet() ? BookImageSize : FVector2f::ZeroVector, BookMargin, PageInset, SpineGap);
}

float SGrimoirePanel::ColumnWidth(bool bRight) const
{
	// Con el libro, la página en que va el texto; sin él, la columna única (LeftPage y RightPage son la misma, Content).
	const Soulbound::Presentation::FGrimoireBookLayout Book = Layout();
	const FBox2f& Column = bRight ? Book.RightPage : Book.LeftPage;
	return FMath::Max(Column.Max.X - Column.Min.X, 1.f);
}

TAttribute<float> SGrimoirePanel::TextWidth(bool bRight, float Indent) const
{
	// Corrección 1 del autor del 17/11/2026: el ancho de su propia columna, menos la barra de desplazamiento y la sangría de la línea.
	const TWeakPtr<const SGrimoirePanel> Weak = SharedThis(this);
	return TAttribute<float>::CreateLambda([Weak, bRight, Indent]()
	{
		const TSharedPtr<const SGrimoirePanel> Panel = Weak.Pin();
		return Soulbound::Presentation::GrimoireColumnTextWidth(Panel.IsValid() ? Panel->ColumnWidth(bRight) : 1.f, Indent);
	});
}

void SGrimoirePanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// La negrita (ampliación del autor del 17/11/2026): los estilos del texto enriquecido llevan el tamaño de letra; si cambia, se rehace la página.
	if (bHasHowToPlay && HowToPlayStyleSize >= 0 && FMath::Max(FontSize.Get(), 1) != HowToPlayStyleSize)
	{
		RebuildHowToPlay();
	}
}

void SGrimoirePanel::UpdateHowToPlayStyles()
{
	const int32 Size = FMath::Max(FontSize.Get(), 1);
	if (!HowToPlayStyles.IsValid())
	{
		HowToPlayStyles = MakeShared<FSlateStyleSet>(TEXT("GrimoireHowToPlay"));
	}
	const auto Style = [this, Size](bool bBold, bool bItalic, const FLinearColor& Color)
	{
		return FTextBlockStyle().SetFont(Soulbound::Presentation::GrimoireFontInfo(TextFont, bBold, Size, bItalic)).SetColorAndOpacity(FSlateColor(Color));
	};
	// Una entrada por marca del texto enriquecido, más la de los párrafos sin marca.
	HowToPlayStyles->Set(HowToPlayNormalStyle, Style(false, false, GrimoireTextColor));
	HowToPlayStyles->Set(TEXT("Bold"), Style(true, false, GrimoireTextColor));
	HowToPlayStyles->Set(HowToPlayWardenStyle, Style(false, true, WardenColor));
	HowToPlayStyles->Set(TEXT("WardenBold"), Style(true, true, WardenColor));
	// Segunda ampliación del autor del 17/11/2026: el subtítulo, en negrita y con el dorado del título, al tamaño del texto.
	HowToPlayStyles->Set(HowToPlaySubtitleStyle, Style(true, false, GrimoireTitleColor));
	HowToPlayStyleSize = Size;
}

void SGrimoirePanel::SetPlayer(const Soulbound::Presentation::FGrimoirePlayerSheet& Sheet)
{
	if (!PlayerBody.IsValid() || (bHasPlayer && Sheet == LastPlayer))
	{
		return;
	}
	bHasPlayer = true;
	LastPlayer = Sheet;
	PlayerBody->ClearChildren();
	// Ronda de maquetación de S-048 (18/11/2026): la tarjeta del jugador con su insolencia y, bajo cada runa con poder, qué hace; sin el bloque
	// "PODERES" ni la línea aparte de la runa del hogar. Se parte al ancho de la página izquierda (sin el libro, la columna única) menos su sangría.
	if (Sheet.Row.Seat != INDEX_NONE)
	{
		PlayerBody->AddSlot().AutoHeight()
		[
			Soulbound::Presentation::MakeGrimoireCard(Sheet.Row, FontSize, DomainColors, TextWidth(false, GrimoireCardIndent), TextFont, !BookBrush.IsSet(), Sheet.InsolenceLine, true)
		];
	}
}

void SGrimoirePanel::SetTable(const TArray<FLegendRow>& Rows)
{
	// Ronda de maquetación de S-048: con el libro, la mesa entera en la página derecha; sin él, bajo la hoja del jugador.
	if (TablePanel.IsValid())
	{
		TablePanel->SetRows(Rows);
	}
}

void SGrimoirePanel::SetHowToPlay(const Soulbound::Presentation::FHowToPlayBook& Book, int32 Page)
{
	if (bHasHowToPlay && Page == HowToPlayPage && Book.Pages == HowToPlayBook.Pages && Book.ErrorMessage == HowToPlayBook.ErrorMessage)
	{
		return;
	}
	bHasHowToPlay = true;
	HowToPlayBook = Book;
	HowToPlayPage = Page;
	RebuildHowToPlay();
}

TSharedRef<SWidget> SGrimoirePanel::MakePageButton(const FString& Label, bool bShown, const FSimpleDelegate& OnClicked)
{
	const TAttribute<int32> Size = FontSize;
	// Como una pestaña inactiva. Correcciones tras probar la maquetación (18/11/2026): el marco, de al menos un píxel de pantalla, para que se vean sus
	// cuatro bordes (GrimoireFrame).
	const TSharedRef<SBorder> Box = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(TabDark)
		.Padding(FMargin(10.f, 4.f))
		.Cursor(EMouseCursor::Hand)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Label))
			.Font_Lambda([this, Size]() { return Soulbound::Presentation::GrimoireFontInfo(TextFont, true, Size.Get()); })
			.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
		];
	// Retoques del 18/11/2026: cada clic pasa una vista, también los rápidos (el segundo llega como doble clic).
	Soulbound::Presentation::GrimoireOnPress(Box, [OnClicked]() { OnClicked.ExecuteIfBound(); });
	const TSharedRef<SBorder> Frame = GrimoireFrame(FSlateColor(TabLight), Box);
	// Retoques del 18/11/2026 (R-HUD-18, HowToPlayPageButtons): el que no lleva a ninguna vista no se pinta ni recibe clics, pero conserva su hueco,
	// para que "N de M" y el otro botón no cambien de sitio. Antes salía apagado.
	Frame->SetVisibility(bShown ? EVisibility::Visible : EVisibility::Hidden);
	return Frame;
}

void SGrimoirePanel::RebuildHowToPlay()
{
	if (!HowToPlayLeft.IsValid())
	{
		return;
	}
	const TAttribute<int32> Size = FontSize;
	// Corrección 1 del autor del 17/11/2026: el mensaje del cargador se parte al ancho de la página izquierda (sin el libro, la columna única).
	const TAttribute<float> PageWidth = TextWidth(false, 0.f);
	const auto Scroll = [](const TSharedRef<SWidget>& Content) -> TSharedRef<SWidget> { return GrimoireScroll(Content); };

	// R-HUD-18: si el archivo no carga, la pestaña enseña el mensaje del cargador.
	if (!HowToPlayBook.IsLoaded() || HowToPlayBook.Pages.IsEmpty())
	{
		HowToPlayLeft->SetContent(Scroll(GrimoireText(TextFont, Size, PageWidth, HowToPlayBook.ErrorMessage, GrimoireTextColor, false)));
		if (HowToPlayRight.IsValid())
		{
			HowToPlayRight->SetContent(SNullWidget::NullWidget);
		}
		return;
	}

	// Segunda ampliación del autor del 17/11/2026 (R-HUD-18): con el libro, a doble página: la vista de la página, ella a la izquierda y la siguiente
	// a la derecha (la última de un total impar, sola); sin él, de una en una.
	UpdateHowToPlayStyles();
	const bool bBook = HowToPlayRight.IsValid();
	const int32 PerView = Soulbound::Presentation::HowToPlayPagesPerView(bBook);
	const int32 Count = HowToPlayBook.Pages.Num();
	const int32 First = Soulbound::Presentation::HowToPlayViewStart(FMath::Clamp(HowToPlayPage, 0, Count - 1), PerView);
	const FHowToPlayPage* LeftPage = &HowToPlayBook.Pages[First];
	const FHowToPlayPage* RightPage = bBook && HowToPlayBook.Pages.IsValidIndex(First + 1) ? &HowToPlayBook.Pages[First + 1] : nullptr;
	const Soulbound::Presentation::FHowToPlayPageButtons Buttons = Soulbound::Presentation::HowToPlayPageButtons(First, Count, PerView);
	const TSharedRef<SWidget> Previous = MakePageButton(TEXT("Anterior"), Buttons.bPrevious, OnPreviousPage);
	const TSharedRef<SWidget> Next = MakePageButton(TEXT("Siguiente"), Buttons.bNext, OnNextPage);
	const TSharedRef<SWidget> Footer = SNew(STextBlock)
		.Text(FText::FromString(Soulbound::Presentation::HowToPlayPageFooter(First, Count, PerView)))
		.Font_Lambda([this, Size]() { return Soulbound::Presentation::GrimoireFontInfo(TextFont, false, Size.Get()); })
		.ColorAndOpacity(GrimoireTextColor);
	if (bBook)
	{
		// Precisión del autor del 17/11/2026: "Anterior" al pie de la hoja izquierda y "1-2 de 5 · Siguiente" al pie de la derecha.
		HowToPlayLeft->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)[MakeHowToPlaySheet(LeftPage, false)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Left)[Previous]);
		HowToPlayRight->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)[MakeHowToPlaySheet(RightPage, true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[Footer]
				+ SHorizontalBox::Slot().AutoWidth()[Next]
			]);
		return;
	}

	// Sin el libro: una página, a todo el ancho, con las imágenes en su sitio; el pie, centrado.
	HowToPlayLeft->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)[MakeHowToPlaySheet(LeftPage, false)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Previous]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)[Footer]
			+ SHorizontalBox::Slot().AutoWidth()[Next]
		]);
}

TSharedRef<SWidget> SGrimoirePanel::MakeHowToPlaySheet(const FHowToPlayPage* Page, bool bRight)
{
	if (!Page)
	{
		// La hoja derecha de la última vista de un total impar: vacía (con su marca de agua, que va detrás).
		return SNew(SSpacer);
	}
	const TAttribute<int32> Size = FontSize;
	// Corrección 1 del autor del 17/11/2026: cada hoja parte su texto al ancho de su propia columna.
	const TAttribute<float> SheetWidth = TextWidth(bRight, 0.f);
	const TAttribute<float> ItemWidth = TAttribute<float>::CreateLambda([SheetWidth]() { return FMath::Max(SheetWidth.Get() - HowToPlayListIndent, 1.f); });
	const ISlateStyle* Styles = HowToPlayStyles.Get();
	// Ampliación del autor del 17/11/2026 (R-HUD-18): cada bloque es texto enriquecido (HowToPlayBlockRichText, el mismo del menú) con los estilos de
	// sus marcas: la frase del Warden en cursiva y con su color, la negrita en negrita y el subtítulo como un título pequeño.
	const auto Rich = [Styles](const FHowToPlayBlock& Item, const TAttribute<float>& Wrap) -> TSharedRef<SWidget>
	{
		return SNew(SRichTextBlock)
			.Text(FText::FromString(Soulbound::Presentation::HowToPlayBlockRichText(Item)))
			.TextStyle(Styles, Item.Kind == EHowToPlayBlockKind::WardenLine ? HowToPlayWardenStyle : HowToPlayNormalStyle)
			.DecoratorStyleSet(Styles)
			.WrapTextAt(Wrap);
	};
	// Segunda ampliación del autor: una página sin título ("##" sola) no lo pinta.
	const TSharedRef<SVerticalBox> Sheet = SNew(SVerticalBox);
	if (!Page->Title.IsEmpty())
	{
		Sheet->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[GrimoireText(TextFont, Size, SheetWidth, Page->Title, GrimoireTitleColor, true, 1.15f)];
	}

	// Ronda de maquetación del 18/11/2026: una página cuyo único bloque es una imagen la pinta en el hueco que queda de verdad en la hoja, centrada en
	// los dos ejes y sin barra de desplazamiento (HowToPlayFullSheetImage, con el tamaño pintado de ese hueco); antes iba dentro del desplazamiento y
	// una lámina vertical lo sacaba por poco. Si su textura no existe, nada.
	if (Page->Blocks.Num() == 1 && Page->Blocks[0].Kind == EHowToPlayBlockKind::Image)
	{
		const FSlateBrush* Brush = ImageForPage ? ImageForPage(Page->Blocks[0].ImageName) : nullptr;
		if (!Brush)
		{
			Sheet->AddSlot().FillHeight(1.f)[SNew(SSpacer)];
			return Sheet;
		}
		const FVector2f Pixels(Brush->ImageSize.X, Brush->ImageSize.Y);
		const TSharedRef<TWeakPtr<SWidget>> Area = MakeShared<TWeakPtr<SWidget>>();
		const auto Place = [Area, Pixels]()
		{
			const TSharedPtr<SWidget> Widget = Area->Pin();
			const FVector2f Room = Widget.IsValid() ? FVector2f(Widget->GetCachedGeometry().GetLocalSize().X, Widget->GetCachedGeometry().GetLocalSize().Y) : FVector2f::ZeroVector;
			return Soulbound::Presentation::HowToPlayFullSheetImage(Room, Pixels);
		};
		const TSharedRef<SBox> Room = SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding_Lambda([Place]() { const FBox2f Box = Place(); return FMargin(Box.Min.X, Box.Min.Y, 0.f, 0.f); })
			[
				SNew(SBox)
				.WidthOverride_Lambda([Place]() { return FOptionalSize(Place().GetSize().X); })
				.HeightOverride_Lambda([Place]() { return FOptionalSize(Place().GetSize().Y); })
				[
					SNew(SImage).Image(Brush)
				]
			];
		*Area = Room;
		Sheet->AddSlot().FillHeight(1.f)[Room];
		return Sheet;
	}

	// El alto de la hoja, de lo pintado: el alto máximo de una imagen entre otros bloques es una fracción del de la hoja (segunda ampliación).
	const TWeakPtr<SWidget> SheetBox = bRight ? StaticCastSharedPtr<SWidget>(HowToPlayRight) : StaticCastSharedPtr<SWidget>(HowToPlayLeft);
	const TAttribute<float> Fraction = ImageMaxHeight;
	const TAttribute<float> MaxHeight = TAttribute<float>::CreateLambda([SheetBox, Fraction]()
	{
		const TSharedPtr<SWidget> Widget = SheetBox.Pin();
		const float Height = Widget.IsValid() ? Widget->GetCachedGeometry().GetLocalSize().Y : 0.f;
		return Height * FMath::Clamp(Fraction.Get(), 0.f, 1.f);
	});

	const TSharedRef<SVerticalBox> Blocks = SNew(SVerticalBox);
	bool bFirst = true;
	for (int32 Block = 0; Block < Page->Blocks.Num(); ++Block)
	{
		const FHowToPlayBlock& Item = Page->Blocks[Block];
		const bool bItem = Item.Kind == EHowToPlayBlockKind::ListItem;
		const bool bListRun = bItem && Block > 0 && Page->Blocks[Block - 1].Kind == EHowToPlayBlockKind::ListItem;
		TSharedRef<SWidget> Content = SNullWidget::NullWidget;
		if (Item.Kind == EHowToPlayBlockKind::Image)
		{
			// La imagen, centrada, al mayor tamaño que cabe en el ancho de la hoja y en el alto máximo; si su textura no existe, no se pinta (la vista
			// lo dice una vez en el registro).
			const FSlateBrush* Brush = ImageForPage ? ImageForPage(Item.ImageName) : nullptr;
			if (!Brush)
			{
				continue;
			}
			const FVector2f Pixels(Brush->ImageSize.X, Brush->ImageSize.Y);
			const auto ImageSize = [SheetWidth, MaxHeight, Pixels]() { return Soulbound::Presentation::HowToPlayImageSize(SheetWidth.Get(), MaxHeight.Get(), Pixels); };
			Content = SNew(SBox)
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.WidthOverride_Lambda([ImageSize]() { return FOptionalSize(ImageSize().X); })
					.HeightOverride_Lambda([ImageSize]() { return FOptionalSize(ImageSize().Y); })
					[
						SNew(SImage).Image(Brush)
					]
				];
		}
		else if (bItem)
		{
			Content = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(HowToPlayListIndent)[GrimoireText(TextFont, Size, SheetWidth, Soulbound::Presentation::HowToPlayListMark, GrimoireTextColor, false)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Rich(Item, ItemWidth)];
		}
		else
		{
			Content = Rich(Item, SheetWidth);
		}
		const float Gap = bFirst ? 0.f : bListRun ? HowToPlayListGap
			: Item.Kind == EHowToPlayBlockKind::Subtitle ? HowToPlayBlockGap + HowToPlaySubtitleGap : HowToPlayBlockGap;
		Blocks->AddSlot().AutoHeight().Padding(0.f, Gap, 0.f, 0.f)[Content];
		bFirst = false;
	}
	Sheet->AddSlot().FillHeight(1.f)[GrimoireScroll(Blocks)];
	return Sheet;
}

void SGrimoirePanel::SetChronicle(const TArray<FChronicleLine>& NewestFirst)
{
	if (Chronicle.IsValid())
	{
		Chronicle->SetLines(NewestFirst);
	}
}
