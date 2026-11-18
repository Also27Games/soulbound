#include "HowToPlayBookPanel.h"

#include "GrimoireWidgets.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/**
	 * Los bloques de una página de "Cómo se juega" (S-048, ampliación del autor del 17/11/2026, valores iniciales propuestos por el agente, que se
	 * ajustan en PIE): el hueco entre dos bloques, el que hay entre dos elementos de lista seguidos y la sangría de la marca de la lista. Desde S-049,
	 * aquí y no en GrimoirePanel.cpp, sin cambiarlos.
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
}

void SHowToPlayBookPanel::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	TextFont = InArgs._Font;
	WardenColor = InArgs._WardenColor;
	ImageMaxHeight = InArgs._ImageMaxHeight.IsSet() ? InArgs._ImageMaxHeight : TAttribute<float>(0.6f);
	ImageForPage = InArgs._ImageForPage;
	OnPreviousPage = InArgs._OnPreviousPage;
	OnNextPage = InArgs._OnNextPage;
	Layout = InArgs._Layout;
	bFramed = InArgs._Framed;
	BookBrush = InArgs._BookBrush;
	if (BookBrush.IsSet())
	{
		// S-048 (R-HUD-20): el tamaño de la textura y sus márgenes; al pintar, el pincel se ajusta al tamaño que da GrimoireBookLayout.
		BookImageSize = FVector2f(BookBrush->ImageSize.X, BookBrush->ImageSize.Y);
		BookMargin = BookBrush->Margin;
		BookBrush->DrawAs = ESlateBrushDrawType::Box;
	}
	// En el menú, la doble página la decide la imagen del libro; en el Grimorio, el panel.
	bDoublePage = bFramed ? BookBrush.IsSet() : InArgs._DoublePage;
	PageInset = InArgs._PageInset;
	SpineGap = InArgs._SpineGap;
	WatermarkBrush = InArgs._Watermark;
	WatermarkOpacity = InArgs._WatermarkOpacity;
	WatermarkSize = InArgs._WatermarkSize;
	FootHint = InArgs._FootHint;
	bPageKeys = InArgs._PageKeys;
	PreviousPageKey = InArgs._PreviousPageKey;
	NextPageKey = InArgs._NextPageKey;

	const TSharedRef<SWidget> Pages = MakePages();
	if (!bFramed)
	{
		ChildSlot[Pages];
		return;
	}
	ChildSlot[MakeFrame(Pages)];
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakePages()
{
	SAssignNew(LeftSheet, SBox);
	if (!bDoublePage)
	{
		// Sin la imagen del libro, como antes: una hoja, a todo el ancho.
		return LeftSheet.ToSharedRef();
	}
	// S-048 (R-HUD-20): a doble página, una hoja a cada lado del lomo; nada lo cruza.
	SAssignNew(RightSheet, SBox);
	const TAttribute<float> LeftWidth = TAttribute<float>::CreateLambda([this]() { return ColumnWidth(false); });
	const TAttribute<float> SpineWidth = TAttribute<float>::CreateLambda([this]()
	{
		const Soulbound::Presentation::FGrimoireBookLayout Zone = CurrentLayout();
		return FMath::Max(Zone.RightPage.Min.X - Zone.LeftPage.Max.X, 0.f);
	});
	return Soulbound::Presentation::GrimoireColumns(LeftSheet.ToSharedRef(), RightSheet.ToSharedRef(), LeftWidth, SpineWidth, VAlign_Fill);
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakeFrame(const TSharedRef<SWidget>& Pages)
{
	// Declaraciones una a una, no "using namespace": en una compilación unity, otro archivo puede tener en su espacio anónimo un nombre igual.
	using Soulbound::Presentation::FGrimoireBookLayout;
	using Soulbound::Presentation::GrimoireColumns;
	using Soulbound::Presentation::GrimoireFootColor;
	using Soulbound::Presentation::GrimoireFootGap;
	using Soulbound::Presentation::GrimoireFootScale;
	using Soulbound::Presentation::GrimoireOnPress;
	using Soulbound::Presentation::GrimoireText;
	using Soulbound::Presentation::GrimoireWatermark;
	// R-HUD-22: el libro del menú, como el Grimorio pero sin cabecera ni teclas de cerrar y de pestañas. Al pie, si la hay, la línea de las teclas de
	// página, del tamaño y el color de la del Grimorio.
	const TAttribute<int32> Size = FontSize;
	const TAttribute<int32> FootSize = TAttribute<int32>::CreateLambda([Size]() { return FMath::Max(FMath::RoundToInt(Size.Get() * GrimoireFootScale), 1); });
	const TAttribute<float> LeftWidth = TAttribute<float>::CreateLambda([this]() { return ColumnWidth(false); });
	const TAttribute<float> RightWidth = TAttribute<float>::CreateLambda([this]() { return ColumnWidth(true); });
	const TAttribute<float> SpineWidth = TAttribute<float>::CreateLambda([this]()
	{
		const FGrimoireBookLayout Zone = CurrentLayout();
		return FMath::Max(Zone.RightPage.Min.X - Zone.LeftPage.Max.X, 0.f);
	});
	const TSharedRef<SWidget> Hint = FootHint.IsEmpty() ? SNullWidget::NullWidget
		: bDoublePage
		? GrimoireColumns(SNew(SSpacer),
			SNew(SBox).HAlign(HAlign_Right)[GrimoireText(TextFont, FootSize, RightWidth, FootHint, GrimoireFootColor, false)], LeftWidth, SpineWidth, VAlign_Top)
		: StaticCastSharedRef<SWidget>(GrimoireText(TextFont, FootSize, LeftWidth, FootHint, GrimoireFootColor, false));
	const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)[Pages]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, FootHint.IsEmpty() ? 0.f : GrimoireFootGap, 0.f, 0.f)[Hint];

	// Lo que cae sobre el libro no pasa a lo que haya debajo, como en el Grimorio (GrimoireOnPress, con una acción vacía).
	TSharedPtr<SBorder> Background;
	if (!BookBrush.IsSet())
	{
		// Sin la imagen del libro, como el Grimorio sin ella: el fondo negro y el margen de siempre.
		SAssignNew(Background, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.88f))
			.Padding(FMargin(Soulbound::Presentation::GrimoireFlatPadding.X, Soulbound::Presentation::GrimoireFlatPadding.Y))
			[
				Content
			];
		GrimoireOnPress(Background.ToSharedRef(), TFunction<void()>());
		return Background.ToSharedRef();
	}

	// Con la imagen del libro: el contenido dentro de sus páginas y, detrás, la marca de agua de cada hoja.
	const TWeakPtr<SHowToPlayBookPanel> Weak = SharedThis(this);
	const TFunction<FGrimoireBookLayout()> BookLayout = [Weak]()
	{
		const TSharedPtr<SHowToPlayBookPanel> Self = Weak.Pin();
		return Self.IsValid() ? Self->CurrentLayout() : FGrimoireBookLayout();
	};
	const FSlateBrush* Mark = WatermarkBrush.IsSet() ? &WatermarkBrush.GetValue() : nullptr;
	SAssignNew(Background, SBorder)
		.BorderImage_Lambda([this]() -> const FSlateBrush*
		{
			// La imagen se pinta a su escala uniforme: sus bordes miden lo mismo en los dos ejes y no se deforman.
			BookBrush->ImageSize = CurrentLayout().BrushSize;
			return &BookBrush.GetValue();
		})
		.Padding_Lambda([this]()
		{
			const FVector2f Panel = GetCachedGeometry().GetLocalSize();
			const FBox2f Zone = CurrentLayout().Content;
			return FMargin(Zone.Min.X, Zone.Min.Y, FMath::Max(Panel.X - Zone.Max.X, 0.f), FMath::Max(Panel.Y - Zone.Max.Y, 0.f));
		})
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SOverlay)
				.Visibility(EVisibility::HitTestInvisible)
				+ SOverlay::Slot()[GrimoireWatermark(Mark, WatermarkOpacity, WatermarkSize, false, BookLayout, TFunction<bool()>())]
				+ SOverlay::Slot()[GrimoireWatermark(Mark, WatermarkOpacity, WatermarkSize, true, BookLayout, TFunction<bool()>())]
			]
			+ SOverlay::Slot()[Content]
		];
	GrimoireOnPress(Background.ToSharedRef(), TFunction<void()>());
	return Background.ToSharedRef();
}

Soulbound::Presentation::FGrimoireBookLayout SHowToPlayBookPanel::CurrentLayout() const
{
	if (!bFramed)
	{
		return Layout.Get();
	}
	// R-HUD-22: en el menú, el libro mide lo que el hueco donde se coloca (lo pintado la última vez; antes de pintarse, nada).
	const FVector2f Size = GetCachedGeometry().GetLocalSize();
	const FVector2f Panel(FMath::Max(Size.X, 1.f), FMath::Max(Size.Y, 1.f));
	return Soulbound::Presentation::GrimoireBookLayout(Panel, BookBrush.IsSet() ? BookImageSize : FVector2f::ZeroVector, BookMargin, PageInset, SpineGap);
}

float SHowToPlayBookPanel::ColumnWidth(bool bRight) const
{
	// Con el libro, la hoja en que va el texto; sin él, la única (LeftPage y RightPage son la misma, Content).
	const Soulbound::Presentation::FGrimoireBookLayout Zone = CurrentLayout();
	const FBox2f& Column = bRight ? Zone.RightPage : Zone.LeftPage;
	return FMath::Max(Column.Max.X - Column.Min.X, 1.f);
}

TAttribute<float> SHowToPlayBookPanel::TextWidth(bool bRight, float Indent) const
{
	// Corrección 1 del autor del 17/11/2026: el ancho de su propia hoja, menos la barra de desplazamiento y la sangría de la línea.
	const TWeakPtr<const SHowToPlayBookPanel> Weak = SharedThis(this);
	return TAttribute<float>::CreateLambda([Weak, bRight, Indent]()
	{
		const TSharedPtr<const SHowToPlayBookPanel> Panel = Weak.Pin();
		return Soulbound::Presentation::GrimoireColumnTextWidth(Panel.IsValid() ? Panel->ColumnWidth(bRight) : 1.f, Indent);
	});
}

void SHowToPlayBookPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// La negrita (ampliación del autor del 17/11/2026): los estilos del texto enriquecido llevan el tamaño de letra; si cambia, se rehace la vista.
	if (bHasBook && StyleSize >= 0 && FMath::Max(FontSize.Get(), 1) != StyleSize)
	{
		Rebuild();
	}
}

bool SHowToPlayBookPanel::SupportsKeyboardFocus() const
{
	return bPageKeys;
}

FReply SHowToPlayBookPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// R-HUD-22: en el menú, con el foco, las teclas de página pasan de vista; las demás siguen su camino (la navegación de la plantilla).
	if (bPageKeys)
	{
		const FKey Key = InKeyEvent.GetKey();
		if (PreviousPageKey.IsValid() && Key == PreviousPageKey)
		{
			OnPreviousPage.ExecuteIfBound();
			return FReply::Handled();
		}
		if (NextPageKey.IsValid() && Key == NextPageKey)
		{
			OnNextPage.ExecuteIfBound();
			return FReply::Handled();
		}
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SHowToPlayBookPanel::UpdateStyles()
{
	const int32 Size = FMath::Max(FontSize.Get(), 1);
	if (!Styles.IsValid())
	{
		Styles = MakeShared<FSlateStyleSet>(TEXT("GrimoireHowToPlay"));
	}
	const auto Style = [this, Size](bool bBold, bool bItalic, const FLinearColor& Color)
	{
		return FTextBlockStyle().SetFont(Soulbound::Presentation::GrimoireFontInfo(TextFont, bBold, Size, bItalic)).SetColorAndOpacity(FSlateColor(Color));
	};
	// Una entrada por marca del texto enriquecido, más la de los párrafos sin marca.
	Styles->Set(HowToPlayNormalStyle, Style(false, false, Soulbound::Presentation::GrimoireTextColor));
	Styles->Set(TEXT("Bold"), Style(true, false, Soulbound::Presentation::GrimoireTextColor));
	Styles->Set(HowToPlayWardenStyle, Style(false, true, WardenColor));
	Styles->Set(TEXT("WardenBold"), Style(true, true, WardenColor));
	// Segunda ampliación del autor del 17/11/2026: el subtítulo, en negrita y con el dorado del título, al tamaño del texto.
	Styles->Set(HowToPlaySubtitleStyle, Style(true, false, Soulbound::Presentation::GrimoireTitleColor));
	StyleSize = Size;
}

void SHowToPlayBookPanel::SetHowToPlay(const Soulbound::Presentation::FHowToPlayBook& InBook, int32 InPage)
{
	if (bHasBook && InPage == Page && InBook.Pages == Book.Pages && InBook.ErrorMessage == Book.ErrorMessage)
	{
		return;
	}
	bHasBook = true;
	Book = InBook;
	Page = InPage;
	Rebuild();
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakePageButton(const FString& Label, bool bShown, const FSimpleDelegate& OnClicked)
{
	const TAttribute<int32> Size = FontSize;
	// Como una pestaña inactiva. Correcciones tras probar la maquetación (18/11/2026): el marco, de al menos un píxel de pantalla, para que se vean sus
	// cuatro bordes (GrimoireFrame).
	const TSharedRef<SBorder> Box = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Soulbound::Presentation::TabDark)
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
	const TSharedRef<SBorder> Frame = Soulbound::Presentation::GrimoireFrame(FSlateColor(Soulbound::Presentation::TabLight), Box);
	// Retoques del 18/11/2026 (R-HUD-18, HowToPlayPageButtons): el que no lleva a ninguna vista no se pinta ni recibe clics, pero conserva su hueco,
	// para que "N de M" y el otro botón no cambien de sitio. Antes salía apagado.
	Frame->SetVisibility(bShown ? EVisibility::Visible : EVisibility::Hidden);
	return Frame;
}

void SHowToPlayBookPanel::Rebuild()
{
	if (!LeftSheet.IsValid())
	{
		return;
	}
	using Soulbound::Presentation::GrimoireText;
	using Soulbound::Presentation::GrimoireTextColor;
	const TAttribute<int32> Size = FontSize;
	// Corrección 1 del autor del 17/11/2026: el mensaje del cargador se parte al ancho de la hoja izquierda (sin el libro, la única).
	const TAttribute<float> PageWidth = TextWidth(false, 0.f);

	// R-HUD-18: si el archivo no carga, la pieza enseña el mensaje del cargador.
	if (!Book.IsLoaded() || Book.Pages.IsEmpty())
	{
		LeftSheet->SetContent(Soulbound::Presentation::GrimoireScroll(GrimoireText(TextFont, Size, PageWidth, Book.ErrorMessage, GrimoireTextColor, false)));
		if (RightSheet.IsValid())
		{
			RightSheet->SetContent(SNullWidget::NullWidget);
		}
		return;
	}

	// Segunda ampliación del autor del 17/11/2026 (R-HUD-18): con el libro, a doble página: la vista de la página, ella a la izquierda y la siguiente
	// a la derecha (la última de un total impar, sola); sin él, de una en una. Desde S-049 (R-HUD-21), lo dice HowToPlayView.
	UpdateStyles();
	const Soulbound::Presentation::FHowToPlayView View = Soulbound::Presentation::HowToPlayView(Page, Book.Pages.Num(), bDoublePage);
	const FHowToPlayPage* LeftPage = Book.Pages.IsValidIndex(View.LeftPage) ? &Book.Pages[View.LeftPage] : nullptr;
	const FHowToPlayPage* RightPage = Book.Pages.IsValidIndex(View.RightPage) ? &Book.Pages[View.RightPage] : nullptr;
	const TSharedRef<SWidget> Previous = MakePageButton(TEXT("Anterior"), View.Buttons.bPrevious, OnPreviousPage);
	const TSharedRef<SWidget> Next = MakePageButton(TEXT("Siguiente"), View.Buttons.bNext, OnNextPage);
	const TSharedRef<SWidget> Footer = SNew(STextBlock)
		.Text(FText::FromString(View.Footer))
		.Font_Lambda([this, Size]() { return Soulbound::Presentation::GrimoireFontInfo(TextFont, false, Size.Get()); })
		.ColorAndOpacity(GrimoireTextColor);
	if (bDoublePage)
	{
		// Precisión del autor del 17/11/2026: "Anterior" al pie de la hoja izquierda y "1-2 de 5 · Siguiente" al pie de la derecha.
		LeftSheet->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)[MakeSheet(LeftPage, false)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Left)[Previous]);
		RightSheet->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)[MakeSheet(RightPage, true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[Footer]
				+ SHorizontalBox::Slot().AutoWidth()[Next]
			]);
		return;
	}

	// Sin el libro: una página, a todo el ancho, con las imágenes en su sitio; el pie, centrado.
	LeftSheet->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)[MakeSheet(LeftPage, false)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f).HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Previous]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)[Footer]
			+ SHorizontalBox::Slot().AutoWidth()[Next]
		]);
}

TSharedRef<SWidget> SHowToPlayBookPanel::MakeSheet(const FHowToPlayPage* InPage, bool bRight)
{
	using Soulbound::Presentation::GrimoireText;
	using Soulbound::Presentation::GrimoireTextColor;
	if (!InPage)
	{
		// La hoja derecha de la última vista de un total impar: vacía (con su marca de agua, que va detrás).
		return SNew(SSpacer);
	}
	const TAttribute<int32> Size = FontSize;
	// Corrección 1 del autor del 17/11/2026: cada hoja parte su texto al ancho de su propia columna.
	const TAttribute<float> SheetWidth = TextWidth(bRight, 0.f);
	const TAttribute<float> ItemWidth = TAttribute<float>::CreateLambda([SheetWidth]() { return FMath::Max(SheetWidth.Get() - HowToPlayListIndent, 1.f); });
	const ISlateStyle* StyleSet = Styles.Get();
	// Ampliación del autor del 17/11/2026 (R-HUD-18): cada bloque es texto enriquecido (HowToPlayBlockRichText, el mismo del menú) con los estilos de
	// sus marcas: la frase del Warden en cursiva y con su color, la negrita en negrita y el subtítulo como un título pequeño.
	const auto Rich = [StyleSet](const FHowToPlayBlock& Item, const TAttribute<float>& Wrap) -> TSharedRef<SWidget>
	{
		return SNew(SRichTextBlock)
			.Text(FText::FromString(Soulbound::Presentation::HowToPlayBlockRichText(Item)))
			.TextStyle(StyleSet, Item.Kind == EHowToPlayBlockKind::WardenLine ? HowToPlayWardenStyle : HowToPlayNormalStyle)
			.DecoratorStyleSet(StyleSet)
			.WrapTextAt(Wrap);
	};
	// Segunda ampliación del autor: una página sin título ("##" sola) no lo pinta.
	const TSharedRef<SVerticalBox> Sheet = SNew(SVerticalBox);
	if (!InPage->Title.IsEmpty())
	{
		Sheet->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			GrimoireText(TextFont, Size, SheetWidth, InPage->Title, Soulbound::Presentation::GrimoireTitleColor, true, 1.15f)
		];
	}

	// Ronda de maquetación del 18/11/2026: una página cuyo único bloque es una imagen la pinta en el hueco que queda de verdad en la hoja, centrada en
	// los dos ejes y sin barra de desplazamiento (HowToPlayFullSheetImage, con el tamaño pintado de ese hueco); antes iba dentro del desplazamiento y
	// una lámina vertical lo sacaba por poco. Si su textura no existe, nada.
	if (InPage->Blocks.Num() == 1 && InPage->Blocks[0].Kind == EHowToPlayBlockKind::Image)
	{
		const FSlateBrush* Brush = ImageForPage ? ImageForPage(InPage->Blocks[0].ImageName) : nullptr;
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
	const TWeakPtr<SWidget> SheetBox = bRight ? StaticCastSharedPtr<SWidget>(RightSheet) : StaticCastSharedPtr<SWidget>(LeftSheet);
	const TAttribute<float> Fraction = ImageMaxHeight;
	const TAttribute<float> MaxHeight = TAttribute<float>::CreateLambda([SheetBox, Fraction]()
	{
		const TSharedPtr<SWidget> Widget = SheetBox.Pin();
		const float Height = Widget.IsValid() ? Widget->GetCachedGeometry().GetLocalSize().Y : 0.f;
		return Height * FMath::Clamp(Fraction.Get(), 0.f, 1.f);
	});

	const TSharedRef<SVerticalBox> Blocks = SNew(SVerticalBox);
	bool bFirst = true;
	for (int32 Block = 0; Block < InPage->Blocks.Num(); ++Block)
	{
		const FHowToPlayBlock& Item = InPage->Blocks[Block];
		const bool bItem = Item.Kind == EHowToPlayBlockKind::ListItem;
		const bool bListRun = bItem && Block > 0 && InPage->Blocks[Block - 1].Kind == EHowToPlayBlockKind::ListItem;
		TSharedRef<SWidget> Content = SNullWidget::NullWidget;
		if (Item.Kind == EHowToPlayBlockKind::Image)
		{
			// La imagen, centrada, al mayor tamaño que cabe en el ancho de la hoja y en el alto máximo; si su textura no existe, no se pinta (quien la
			// carga lo dice una vez en el registro).
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
	Sheet->AddSlot().FillHeight(1.f)[Soulbound::Presentation::GrimoireScroll(Blocks)];
	return Sheet;
}
