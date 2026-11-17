#include "GrimoirePanel.h"

#include "ChroniclePanel.h"
#include "LegendPanel.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
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

	/**
	 * Los márgenes del panel sin la imagen del libro (16 a los lados y 12 arriba y abajo; desde S-048, en GrimoireBook.h, porque son la zona de contenido
	 * de hoy de GrimoireBookLayout) y lo que se deja a la barra de desplazamiento de una pestaña.
	 */
	const FVector2f GrimoireFlatPadding = Soulbound::Presentation::GrimoireFlatPadding;
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
	MinTextWidth = InArgs._MinTextWidth.IsSet() ? InArgs._MinTextWidth : TAttribute<float>(300.f);
	WardenColor = InArgs._WardenColor;
	ImageForPage = InArgs._ImageForPage;
	OnPreviousPage = InArgs._OnPreviousPage;
	OnNextPage = InArgs._OnNextPage;

	const TAttribute<float> Width = PanelWidth;
	const TAttribute<float> PageWidth = TAttribute<float>::CreateSP(this, &SGrimoirePanel::PageTextWidth);
	const TAttribute<int32> Size = FontSize;

	// Una página por pestaña, en el orden de GrimoireTabs. Cada una con su desplazamiento (la crónica ya lleva el suyo).
	const auto Page = [](const TSharedRef<SWidget>& Content) -> TSharedRef<SWidget>
	{
		return SNew(SScrollBox) + SScrollBox::Slot().Padding(0.f, 0.f, GrimoireScrollBarRoom, 0.f)[Content];
	};
	const TAttribute<float> LegendWidth = TAttribute<float>::CreateLambda([PageWidth]() { return PageWidth.Get() + 16.f; });
	const auto MakeLegend = [Size, LegendWidth](TSharedPtr<SLegendPanel>& Out) -> TSharedRef<SWidget>
	{
		return SAssignNew(Out, SLegendPanel).FontSize(Size).WrapWidth(LegendWidth).Heading(FString()).Background(false);
	};
	const auto WidgetIndex = [this]() { return FMath::Max(Soulbound::Presentation::GrimoireTabs().Find(ActiveTab.Get()), 0); };
	SAssignNew(HowToPlayLeft, SBox);

	if (!BookBrush.IsSet())
	{
		// Sin la imagen del libro, como antes (S-044): una columna, sobre el fondo negro.
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
						GrimoireText(Size, TitleWidth, FString::Printf(TEXT("GRIMORIO  (%s; la partida sigue)"), *InArgs._KeysHint), FLinearColor::White, true, 1.3f)
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
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(TablePanel))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(BoardPanel))]
						+ SWidgetSwitcher::Slot()[HowToPlayLeft.ToSharedRef()]
						+ SWidgetSwitcher::Slot()[SAssignNew(Chronicle, SChroniclePanel).FontSize(Size)]
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
	const TAttribute<float> LeftWidth = TAttribute<float>::CreateLambda([this]()
	{
		const Soulbound::Presentation::FGrimoireBookLayout Book = Layout();
		return FMath::Max(Book.LeftPage.Max.X - Book.LeftPage.Min.X, 1.f);
	});
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
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					Columns(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							GrimoireText(Size, TitleWidth, FString::Printf(TEXT("GRIMORIO  (%s; la partida sigue)"), *InArgs._KeysHint), FLinearColor::White, true, 1.3f)
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
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(TablePanel))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(BoardPanel))]
						+ SWidgetSwitcher::Slot()[HowToPlayLeft.ToSharedRef()]
						// Precisión del autor del 17/11/2026: la crónica, entera en la página izquierda.
						+ SWidgetSwitcher::Slot()[SAssignNew(Chronicle, SChroniclePanel).FontSize(Size)],
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda(WidgetIndex)
						+ SWidgetSwitcher::Slot()[Page(SAssignNew(YouSide, SVerticalBox))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(TableSide))]
						+ SWidgetSwitcher::Slot()[Page(MakeLegend(BoardSide))]
						+ SWidgetSwitcher::Slot()[HowToPlayRight.ToSharedRef()]
						+ SWidgetSwitcher::Slot()[SNew(SSpacer)],
						VAlign_Fill)
				]
			]
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

float SGrimoirePanel::PageTextWidth() const
{
	// Sin el libro, la columna única; con él, la más estrecha de las dos páginas, para que nada cruce el lomo.
	const Soulbound::Presentation::FGrimoireBookLayout Book = Layout();
	const float Column = Book.bBook ? FMath::Min(Book.LeftPage.Max.X - Book.LeftPage.Min.X, Book.RightPage.Max.X - Book.RightPage.Min.X)
		: Book.Content.Max.X - Book.Content.Min.X;
	return FMath::Max(Column - GrimoireScrollBarRoom, 1.f);
}

void SGrimoirePanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// Sin el libro, la imagen va al lado o debajo según el ancho (R-HUD-18); si la ventana cambia y la decisión con ella, se rehace la página.
	if (!BookBrush.IsSet() && bHasHowToPlay && HowToPlayBook.Pages.IsValidIndex(HowToPlayPage) && !HowToPlayBook.Pages[HowToPlayPage].ImageName.IsEmpty()
		&& ImageBesideNow() != bLastImageBeside)
	{
		RebuildHowToPlay();
	}
}

bool SGrimoirePanel::ImageBesideNow() const
{
	const FBox2f Content = Layout().Content;
	return Soulbound::Presentation::HowToPlayImageBeside(Content.Max.X - Content.Min.X, MinTextWidth.Get());
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
	const TAttribute<float> PageWidth = TAttribute<float>::CreateSP(this, &SGrimoirePanel::PageTextWidth);
	// La tarjeta ocupa el ancho de la página: su texto, el de la página menos los 16 de margen de la tarjeta.
	const TAttribute<float> CardText = TAttribute<float>::CreateLambda([PageWidth]() { return FMath::Max(PageWidth.Get() - 16.f, 1.f); });
	if (View.Row.Seat != INDEX_NONE)
	{
		YouBody->AddSlot().AutoHeight()[Soulbound::Presentation::MakeGrimoireCard(View.Row, Size, DomainColors, CardText)];
	}
	if (!View.InsolenceLine.IsEmpty())
	{
		Side->AddSlot().AutoHeight().Padding(4.f, 6.f, 4.f, 0.f)[GrimoireText(Size, PageWidth, View.InsolenceLine, GrimoireTextColor, true)];
	}
	if (!View.HomeRuneLine.IsEmpty())
	{
		Side->AddSlot().AutoHeight().Padding(4.f, 4.f, 4.f, 0.f)[GrimoireText(Size, PageWidth, View.HomeRuneLine, FLinearColor::White, true)];
	}
	Side->AddSlot().AutoHeight().Padding(4.f, 10.f, 4.f, 2.f)[GrimoireText(Size, PageWidth, TEXT("PODERES (los da cada runa a un Elegido, según su mapa)"), GrimoireTitleColor, true)];
	for (const FString& Line : View.PowerLines)
	{
		Side->AddSlot().AutoHeight().Padding(12.f, 0.f, 4.f, 0.f)[GrimoireText(Size, PageWidth, Line, GrimoireTextColor, false)];
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
				.Font_Lambda([Size]() { return FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(Size.Get(), 1)); })
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
	const TAttribute<float> PageWidth = TAttribute<float>::CreateSP(this, &SGrimoirePanel::PageTextWidth);
	const auto Scroll = [](const TSharedRef<SWidget>& Content) -> TSharedRef<SWidget>
	{
		return SNew(SScrollBox) + SScrollBox::Slot().Padding(0.f, 0.f, GrimoireScrollBarRoom, 0.f)[Content];
	};

	// R-HUD-18: si el archivo no carga, la pestaña enseña el mensaje del cargador.
	if (!HowToPlayBook.IsLoaded() || HowToPlayBook.Pages.IsEmpty())
	{
		HowToPlayLeft->SetContent(Scroll(GrimoireText(Size, PageWidth, HowToPlayBook.ErrorMessage, GrimoireTextColor, false)));
		if (HowToPlayRight.IsValid())
		{
			HowToPlayRight->SetContent(SNullWidget::NullWidget);
		}
		return;
	}

	const int32 Count = HowToPlayBook.Pages.Num();
	const int32 Index = FMath::Clamp(HowToPlayPage, 0, Count - 1);
	const FHowToPlayPage& Shown = HowToPlayBook.Pages[Index];
	const FSlateBrush* Image = Shown.ImageName.IsEmpty() || !ImageForPage ? nullptr : ImageForPage(Shown.ImageName);
	const bool bBook = HowToPlayRight.IsValid();
	const bool bBeside = bBook || ImageBesideNow();
	bLastImageBeside = bBeside;
	// Sin el libro y con la imagen al lado, el texto ocupa la mitad de la columna menos el hueco entre los dos.
	constexpr float ImageGap = 12.f;
	const bool bHalf = !bBook && Image && bBeside;
	const TAttribute<float> BodyWidth = TAttribute<float>::CreateLambda([PageWidth, bHalf]()
	{
		return bHalf ? FMath::Max(PageWidth.Get() * 0.5f - ImageGap, 1.f) : PageWidth.Get();
	});

	const TSharedRef<SWidget> Title = GrimoireText(Size, PageWidth, Shown.Title, GrimoireTitleColor, true, 1.15f);
	const TSharedRef<SWidget> Body = Scroll(GrimoireText(Size, BodyWidth, Shown.Body, GrimoireTextColor, false));
	const TSharedRef<SWidget> Comment = Shown.WardenComment.IsEmpty() ? SNullWidget::NullWidget
		: StaticCastSharedRef<SWidget>(GrimoireText(Size, PageWidth, Shown.WardenComment, WardenColor, false));
	const TSharedRef<SWidget> Picture = !Image ? SNullWidget::NullWidget
		: StaticCastSharedRef<SWidget>(SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[SNew(SImage).Image(Image)]);
	const TSharedRef<SWidget> Previous = MakePageButton(TEXT("Anterior"), Index > 0, OnPreviousPage);
	const TSharedRef<SWidget> Next = MakePageButton(TEXT("Siguiente"), Index < Count - 1, OnNextPage);
	const TSharedRef<SWidget> Footer = SNew(STextBlock)
		.Text(FText::FromString(Soulbound::Presentation::HowToPlayPageFooter(Index, Count)))
		.Font_Lambda([Size]() { return FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(Size.Get(), 1)); })
		.ColorAndOpacity(GrimoireTextColor);
	const float CommentGap = Shown.WardenComment.IsEmpty() ? 0.f : 8.f;

	if (bBook)
	{
		// Precisión del autor del 17/11/2026: a la izquierda, la regla y, al pie, el comentario del Warden y "Anterior"; a la derecha, la imagen y, al
		// pie, "N de M · Siguiente".
		HowToPlayLeft->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[Title]
			+ SVerticalBox::Slot().FillHeight(1.f)[Body]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, CommentGap, GrimoireScrollBarRoom, 0.f)[Comment]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Left)[Previous]);
		HowToPlayRight->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Picture]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[Footer]
				+ SHorizontalBox::Slot().AutoWidth()[Next]
			]);
		return;
	}

	// Sin el libro: el texto a la izquierda y la imagen a la derecha o, si no caben, debajo; sin imagen, el texto a todo el ancho, sin hueco. El
	// comentario, a todo el ancho; el pie, centrado.
	TSharedRef<SWidget> Middle = Body;
	if (Image && bBeside)
	{
		Middle = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Body]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(ImageGap, 0.f, 0.f, 0.f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Picture];
	}
	else if (Image)
	{
		Middle = SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)[Body]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Center)
			[
				SNew(SBox)
				.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(FMath::Max(PanelHeight.Get() * 0.4f, 1.f)); })
				[
					Picture
				]
			];
	}
	HowToPlayLeft->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[Title]
		+ SVerticalBox::Slot().FillHeight(1.f)[Middle]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, CommentGap, GrimoireScrollBarRoom, 0.f)[Comment]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Previous]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)[Footer]
			+ SHorizontalBox::Slot().AutoWidth()[Next]
		]);
}

void SGrimoirePanel::SetChronicle(const TArray<FChronicleLine>& NewestFirst)
{
	if (Chronicle.IsValid())
	{
		Chronicle->SetLines(NewestFirst);
	}
}
