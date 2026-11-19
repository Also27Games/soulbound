#include "GrimoirePanel.h"

#include "ChroniclePanel.h"
#include "HowToPlayBookPanel.h"
#include "LegendPanel.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// S-049: los textos, el desplazamiento, el marco y los colores que comparte con el libro de "Cómo se juega" viven en GrimoireWidgets.h, sin cambios.
	using Soulbound::Presentation::GrimoireFootColor;
	using Soulbound::Presentation::GrimoireFootScale;
	using Soulbound::Presentation::GrimoireFootText;
	using Soulbound::Presentation::GrimoireFrame;
	using Soulbound::Presentation::GrimoireScroll;
	using Soulbound::Presentation::GrimoireText;
	using Soulbound::Presentation::GrimoireTextColor;
	using Soulbound::Presentation::GrimoireTitleColor;
	using Soulbound::Presentation::TabDark;
	using Soulbound::Presentation::TabLight;

	/** La sangría de la tarjeta del jugador, izquierda y derecha sumadas (S-048, corrección 1). */
	constexpr float GrimoireCardIndent = 16.f;

	/**
	 * La sangría de la descripción de un poder bajo su runa, además de la del cuadrito (S-048, ronda de maquetación del 18/11/2026; valor inicial
	 * propuesto por el agente), y su gris, más claro que el de lo apagado para que se lea.
	 */
	constexpr float GrimoirePowerTextIndent = 6.f;
	const FLinearColor GrimoirePowerTextColor(0.68f, 0.68f, 0.68f);

	// S-049 (R-HUD-21): los huecos y los estilos de los bloques de "Cómo se juega" se fueron con la pieza del libro (HowToPlayBookPanel.cpp).

	const FLinearColor GrimoireDimColor(0.55f, 0.55f, 0.55f);

	/**
	 * Los márgenes del panel sin la imagen del libro (16 a los lados y 12 arriba y abajo; desde S-048, en GrimoireBook.h, porque son la zona de contenido
	 * de hoy de GrimoireBookLayout) y lo que se deja a la barra de desplazamiento de una pestaña.
	 */
	const FVector2f GrimoireFlatPadding = Soulbound::Presentation::GrimoireFlatPadding;

	/** Al pasar el ratón por una pestaña inactiva, su fondo se ilumina (S-035); los demás colores de las pestañas, en GrimoireWidgets.h. */
	const FLinearColor TabHover = FLinearColor(TabLight.R * 0.35f, TabLight.G * 0.35f, TabLight.B * 0.35f, 1.f);
}

TSharedRef<SWidget> Soulbound::Presentation::MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth,
	const FGrimoireFont& Font, bool bBackground, const FString& InsolenceLine, bool bPlayerSheet)
{
	// S-048 (retoques de "La mesa" del 18/11/2026): en la hoja del jugador, en una línea y sin "ELEGIDO:"; en la carta del Elegido, como siempre.
	const FString Header = GrimoireCardHeader(Row, bPlayerSheet);
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
		// S-048 (retoques de "La mesa" del 18/11/2026): el texto de la runa, el mismo que el de las muestras de un Brazal revelado (GrimoireRuneLabel).
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
				GrimoireText(Font, FontSize, RuneWidth, GrimoireRuneLabel(Rune, Row.bTypesHidden, true),
					Rune.bActive ? FLinearColor::White : Row.bTypesHidden || Rune.Count > 0 ? GrimoireTextColor : GrimoireDimColor, Rune.bActive)
			]
		];
		// S-048 (ronda de maquetación, decisión del autor del 18/11/2026): debajo de su runa, sangrada, en gris y al tamaño de siempre, qué hace el poder.
		// Una runa sin poder no lleva nada.
		if (bPlayerSheet && Rune.bHasPower && !Rune.PowerText.IsEmpty())
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
	// S-048 (retoques de "La mesa" del 18/11/2026): la hoja del jugador no lleva la línea final, porque "(ACTIVO)" ya va en cada runa (con las seis runas
	// no cabía a 1024x768); la carta del Elegido, sí.
	if (!bPlayerSheet)
	{
		Card->AddSlot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
		[
			GrimoireText(Font, FontSize, TextWidth, Row.bTypesHidden ? FString::Printf(TEXT("%d runa%s; sus tipos están ocultos"), Row.TotalRunes, Row.TotalRunes == 1 ? TEXT("") : TEXT("s"))
				: FString::Printf(TEXT("Poderes activos: %s"), Row.ActivePowers.IsEmpty() ? TEXT("ninguno") : *FString::Join(Row.ActivePowers, TEXT(", "))),
				GrimoireTitleColor, false)
		];
	}
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
	WatermarkBrush = InArgs._Watermark;
	WatermarkOpacity = InArgs._WatermarkOpacity;
	WatermarkSize = InArgs._WatermarkSize;
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
	// S-049 (R-HUD-21): "Cómo se juega" es la pieza del libro, la misma del menú. El panel le da su zona de contenido (con el libro, sus dos hojas y el
	// lomo; sin él, la columna única) y le pasa lo que hasta ahora pintaba él: la letra, la fuente, el color del Warden, las láminas y los botones.
	const TWeakPtr<SGrimoirePanel> WeakPanel = SharedThis(this);
	SAssignNew(HowToPlay, SHowToPlayBookPanel)
		.FontSize(Size)
		.Font(TextFont)
		.WardenColor(InArgs._WardenColor)
		.ImageMaxHeight(InArgs._ImageMaxHeight.IsSet() ? InArgs._ImageMaxHeight : TAttribute<float>(0.6f))
		.ImageForPage(InArgs._ImageForPage)
		.OnPreviousPage(InArgs._OnPreviousPage)
		.OnNextPage(InArgs._OnNextPage)
		.DoublePage(bBook)
		.Layout_Lambda([WeakPanel]()
		{
			const TSharedPtr<SGrimoirePanel> Panel = WeakPanel.Pin();
			return Panel.IsValid() ? Panel->Layout() : Soulbound::Presentation::FGrimoireBookLayout();
		});
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
						+ SWidgetSwitcher::Slot()[HowToPlay.ToSharedRef()]
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
	const TAttribute<float> LeftWidth = TAttribute<float>::CreateLambda([this]() { return ColumnWidth(false); });
	const TAttribute<float> RightWidth = TAttribute<float>::CreateLambda([this]() { return ColumnWidth(true); });
	const TAttribute<float> SpineWidth = TAttribute<float>::CreateLambda([this]()
	{
		const Soulbound::Presentation::FGrimoireBookLayout Book = Layout();
		return FMath::Max(Book.RightPage.Min.X - Book.LeftPage.Max.X, 0.f);
	});
	// Las dos columnas de una fila: la de la izquierda mide la página izquierda; el hueco, el lomo; la derecha, lo que queda (desde S-049, GrimoireColumns).
	const auto Columns = [LeftWidth, SpineWidth](const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right, EVerticalAlignment RightAlign) -> TSharedRef<SWidget>
	{
		return Soulbound::Presentation::GrimoireColumns(Left, Right, LeftWidth, SpineWidth, RightAlign);
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
				// S-049 (R-HUD-21, opción A aprobada por el autor el 18/11/2026): un solo cambiador, una ranura por pestaña, y cada pestaña reparte sus
				// hojas en las dos columnas; antes, un cambiador por columna. Las hojas de "La mesa" y de "Crónica" son las mismas que antes, con los
				// mismos anchos; "Cómo se juega" es la pieza, que reparte las suyas igual.
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda(WidgetIndex)
					// Ronda de maquetación de S-048 (18/11/2026): "La mesa", con la hoja del jugador a la izquierda y la mesa a la derecha.
					+ SWidgetSwitcher::Slot()[Columns(Page(SAssignNew(PlayerBody, SVerticalBox)), Page(MakeLegend(TablePanel, true)), VAlign_Fill)]
					+ SWidgetSwitcher::Slot()[HowToPlay.ToSharedRef()]
					// Precisión del autor del 17/11/2026: la crónica, entera en la página izquierda.
					+ SWidgetSwitcher::Slot()[Columns(Chronicle.ToSharedRef(), SNew(SSpacer), VAlign_Fill)]
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
	// S-049: con la función que comparte con el libro del menú (GrimoireWatermark). Solo en "Cómo se juega"; siempre, lleve la página imágenes o no,
	// también en la hoja vacía de la última vista.
	const TWeakPtr<SGrimoirePanel> Weak = SharedThis(this);
	return Soulbound::Presentation::GrimoireWatermark(WatermarkBrush.IsSet() ? &WatermarkBrush.GetValue() : nullptr, WatermarkOpacity, WatermarkSize, bRight,
		[Weak]()
		{
			const TSharedPtr<SGrimoirePanel> Panel = Weak.Pin();
			return Panel.IsValid() ? Panel->Layout() : Soulbound::Presentation::FGrimoireBookLayout();
		},
		[Weak]()
		{
			const TSharedPtr<SGrimoirePanel> Panel = Weak.Pin();
			return Panel.IsValid() && Panel->ActiveTab.Get() == Soulbound::Presentation::EGrimoireTab::HowToPlay;
		});
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
	// S-049 (R-HUD-21): lo pinta la pieza del libro, que solo se rehace si cambia el libro o la página.
	if (HowToPlay.IsValid())
	{
		HowToPlay->SetHowToPlay(Book, Page);
	}
}

void SGrimoirePanel::SetChronicle(const TArray<FChronicleLine>& NewestFirst)
{
	if (Chronicle.IsValid())
	{
		Chronicle->SetLines(NewestFirst);
	}
}
