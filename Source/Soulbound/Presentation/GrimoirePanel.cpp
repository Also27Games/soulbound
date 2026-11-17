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

	/** Las sangrías de las líneas del Grimorio, izquierda y derecha sumadas (S-048, corrección 1): las de la pestaña "Tú" y la de la tarjeta. */
	constexpr float GrimoireLineIndent = 8.f;
	constexpr float GrimoirePowerIndent = 16.f;
	constexpr float GrimoireCardIndent = 16.f;

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

TSharedRef<SWidget> Soulbound::Presentation::MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth,
	const FGrimoireFont& Font, bool bBackground)
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
	ActiveTab = InArgs._ActiveTab.IsSet() ? InArgs._ActiveTab : TAttribute<EGrimoireTab>(EGrimoireTab::You);
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
	// Corrección 7: un margen entre el final de la zona de contenido y el pie, el mismo en las cinco pestañas.
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
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.88f))
				.Padding(FMargin(GrimoireFlatPadding.X, GrimoireFlatPadding.Y))
				// S-044: el panel se queda los clics que le caen encima; ninguno llega al tablero.
				.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
					[
						GrimoireText(TextFont, Size, TitleWidth, Soulbound::Presentation::GrimoireTitleText, FLinearColor::White, true, 1.3f)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
					[
						MakeTabHeader(Soulbound::Presentation::GrimoireTabs())
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda(WidgetIndex)
						+ SWidgetSwitcher::Slot()[Page(SAssignNew(YouBody, SVerticalBox))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(TablePanel, false))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(BoardPanel, false))]
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
		return;
	}

	// S-048 (R-HUD-20): sobre la imagen del libro abierto. El contenido va dentro de sus páginas (GrimoireBookLayout) y se reparte en dos columnas,
	// una por página, con el hueco del lomo entre las dos: nada lo cruza. La cabecera también se reparte (precisión del autor del 17/11/2026): el
	// título y "Tú · La mesa · El tablero" a la izquierda, "Cómo se juega · Crónica" a la derecha, a la misma altura.
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
	const TArray<EGrimoireTab> LeftTabs{EGrimoireTab::You, EGrimoireTab::Table, EGrimoireTab::Board};
	const TArray<EGrimoireTab> RightTabs{EGrimoireTab::HowToPlay, EGrimoireTab::Chronicle};
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride_Lambda([Width]() { return FOptionalSize(Width.Get()); })
		.HeightOverride_Lambda([this]() { return FOptionalSize(FMath::Max(PanelHeight.Get(), 1.f)); })
		[
			SNew(SBorder)
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
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
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
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					Columns(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							GrimoireText(TextFont, Size, TitleWidth, Soulbound::Presentation::GrimoireTitleText, FLinearColor::White, true, 1.3f)
						]
						+ SVerticalBox::Slot().AutoHeight()[MakeTabHeader(LeftTabs)],
						MakeTabHeader(RightTabs),
						VAlign_Bottom)
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					Columns(
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda(WidgetIndex)
						+ SWidgetSwitcher::Slot()[Page(SAssignNew(YouBody, SVerticalBox))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(TablePanel, false))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(BoardPanel, false))]
						+ SWidgetSwitcher::Slot()[HowToPlayLeft.ToSharedRef()]
						// Precisión del autor del 17/11/2026: la crónica, entera en la página izquierda.
						+ SWidgetSwitcher::Slot()[Chronicle.ToSharedRef()],
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda(WidgetIndex)
						+ SWidgetSwitcher::Slot()[Page(SAssignNew(YouSide, SVerticalBox))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(TableSide, true))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(BoardSide, true))]
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
			.OnMouseButtonDown_Lambda([this, Tab](const FGeometry&, const FPointerEvent&)
			{
				OnTabClicked.ExecuteIfBound(Tab);
				return FReply::Handled();
			})
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

void SGrimoirePanel::SetYou(const Soulbound::Presentation::FGrimoireYouView& View)
{
	if (!YouBody.IsValid() || (bHasYou && View == LastYou))
	{
		return;
	}
	bHasYou = true;
	LastYou = View;
	// S-048 (R-HUD-20, precisión del autor del 17/11/2026): con el libro, la tarjeta en la página izquierda y lo demás en la derecha.
	const TSharedRef<SVerticalBox> Side = YouSide.IsValid() ? YouSide.ToSharedRef() : YouBody.ToSharedRef();
	YouBody->ClearChildren();
	Side->ClearChildren();
	const TAttribute<int32> Size = FontSize;
	// Corrección 1 del autor del 17/11/2026: cada texto se parte al ancho de su columna (la tarjeta, la izquierda; lo demás, la derecha con el libro),
	// menos su sangría: la tarjeta, sus 8 a cada lado; la insolencia, el hogar y el título de los poderes, 4 y 4; cada poder, 12 y 4.
	const bool bSideRight = YouSide.IsValid();
	if (View.Row.Seat != INDEX_NONE)
	{
		YouBody->AddSlot().AutoHeight()
		[
			Soulbound::Presentation::MakeGrimoireCard(View.Row, Size, DomainColors, TextWidth(false, GrimoireCardIndent), TextFont, !BookBrush.IsSet())
		];
	}
	const TAttribute<float> LineWidth = TextWidth(bSideRight, GrimoireLineIndent);
	if (!View.InsolenceLine.IsEmpty())
	{
		Side->AddSlot().AutoHeight().Padding(4.f, 6.f, 4.f, 0.f)[GrimoireText(TextFont, Size, LineWidth, View.InsolenceLine, GrimoireTextColor, true)];
	}
	if (!View.HomeRuneLine.IsEmpty())
	{
		Side->AddSlot().AutoHeight().Padding(4.f, 4.f, 4.f, 0.f)[GrimoireText(TextFont, Size, LineWidth, View.HomeRuneLine, FLinearColor::White, true)];
	}
	Side->AddSlot().AutoHeight().Padding(4.f, 10.f, 4.f, 2.f)[GrimoireText(TextFont, Size, LineWidth, TEXT("PODERES (los da cada runa a un Elegido, según su mapa)"), GrimoireTitleColor, true)];
	const TAttribute<float> PowerWidth = TextWidth(bSideRight, GrimoirePowerIndent);
	for (const FString& Line : View.PowerLines)
	{
		Side->AddSlot().AutoHeight().Padding(12.f, 0.f, 4.f, 0.f)[GrimoireText(TextFont, Size, PowerWidth, Line, GrimoireTextColor, false)];
	}
}

namespace
{
	/** Reparte filas entre las dos páginas del libro: las First primeras a la izquierda y el resto a la derecha (S-048, R-HUD-20). */
	void SetRowsOnPages(const TSharedPtr<SLegendPanel>& Left, const TSharedPtr<SLegendPanel>& Right, const TArray<FLegendRow>& Rows, int32 First)
	{
		if (!Left.IsValid())
		{
			return;
		}
		if (!Right.IsValid())
		{
			Left->SetRows(Rows);
			return;
		}
		const int32 Split = FMath::Clamp(First, 0, Rows.Num());
		Left->SetRows(TArray<FLegendRow>(Rows.GetData(), Split));
		Right->SetRows(TArray<FLegendRow>(Rows.GetData() + Split, Rows.Num() - Split));
	}
}

void SGrimoirePanel::SetTable(const TArray<FLegendRow>& Rows)
{
	// Precisión del autor del 17/11/2026: con el libro, Jugadores a la izquierda y Dominios a la derecha.
	SetRowsOnPages(TablePanel, TableSide, Rows, 1);
}

void SGrimoirePanel::SetBoard(const TArray<FLegendRow>& Rows)
{
	// Precisión del autor del 17/11/2026: con el libro, Casillas y Nichos a la izquierda, Tablero y destellos a la derecha.
	SetRowsOnPages(BoardPanel, BoardSide, Rows, 2);
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

TSharedRef<SWidget> SGrimoirePanel::MakePageButton(const FString& Label, bool bEnabled, const FSimpleDelegate& OnClicked)
{
	const TAttribute<int32> Size = FontSize;
	// Como una pestaña inactiva; apagado (en la primera o en la última página), gris y sin efecto (R-HUD-18: no es cíclico).
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(bEnabled ? TabLight : GrimoireDimColor)
		.Padding(FMargin(1.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(TabDark)
			.Padding(FMargin(10.f, 4.f))
			.Cursor(bEnabled ? EMouseCursor::Hand : EMouseCursor::Default)
			.OnMouseButtonDown_Lambda([bEnabled, OnClicked](const FGeometry&, const FPointerEvent&)
			{
				if (bEnabled)
				{
					OnClicked.ExecuteIfBound();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font_Lambda([this, Size]() { return Soulbound::Presentation::GrimoireFontInfo(TextFont, true, Size.Get()); })
				.ColorAndOpacity(bEnabled ? FLinearColor(0.9f, 0.9f, 0.9f) : GrimoireDimColor)
			]
		];
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
	const TSharedRef<SWidget> Previous = MakePageButton(TEXT("Anterior"), First > 0, OnPreviousPage);
	const TSharedRef<SWidget> Next = MakePageButton(TEXT("Siguiente"), First + PerView < Count, OnNextPage);
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
	// El alto de la hoja y el de su zona que se desplaza, de lo pintado: el alto máximo de una imagen es una fracción del de la hoja; una página con
	// solo una imagen la pinta a hoja entera, el alto de la zona que se desplaza (segunda ampliación del autor del 17/11/2026).
	const TSharedRef<TWeakPtr<SWidget>> ScrollArea = MakeShared<TWeakPtr<SWidget>>();
	const TWeakPtr<SWidget> SheetBox = bRight ? StaticCastSharedPtr<SWidget>(HowToPlayRight) : StaticCastSharedPtr<SWidget>(HowToPlayLeft);
	const bool bOnlyImage = Page->Blocks.Num() == 1 && Page->Blocks[0].Kind == EHowToPlayBlockKind::Image;
	const TAttribute<float> Fraction = ImageMaxHeight;
	const TAttribute<float> MaxHeight = TAttribute<float>::CreateLambda([ScrollArea, SheetBox, bOnlyImage, Fraction]()
	{
		const TSharedPtr<SWidget> Widget = bOnlyImage ? ScrollArea->Pin() : SheetBox.Pin();
		const float Height = Widget.IsValid() ? Widget->GetCachedGeometry().GetLocalSize().Y : 0.f;
		return bOnlyImage ? Height : Height * FMath::Clamp(Fraction.Get(), 0.f, 1.f);
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
	const TSharedRef<SWidget> Scroll = GrimoireScroll(Blocks);
	*ScrollArea = Scroll;

	// Segunda ampliación del autor: una página sin título ("##" sola) no lo pinta.
	const TSharedRef<SVerticalBox> Sheet = SNew(SVerticalBox);
	if (!Page->Title.IsEmpty())
	{
		Sheet->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[GrimoireText(TextFont, Size, SheetWidth, Page->Title, GrimoireTitleColor, true, 1.15f)];
	}
	Sheet->AddSlot().FillHeight(1.f)[Scroll];
	return Sheet;
}

void SGrimoirePanel::SetChronicle(const TArray<FChronicleLine>& NewestFirst)
{
	if (Chronicle.IsValid())
	{
		Chronicle->SetLines(NewestFirst);
	}
}
