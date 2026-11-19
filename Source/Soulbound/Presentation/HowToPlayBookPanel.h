#pragma once

#include "CoreMinimal.h"
#include "GrimoireBook.h"
#include "GrimoireFont.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/**
 * El libro de "Cómo se juega" (S-049, R-HUD-21, nombre aprobado): la pieza que pinta una vista del libro, la misma en la pestaña del Grimorio y en el
 * menú. Las hojas de la vista (HowToPlayView), cada una con su título, sus bloques en texto enriquecido y sus láminas, "Anterior", "N de M" y
 * "Siguiente" (ocultos en la primera y en la última vista, guardando su sitio); con la imagen del libro, a doble página y sin cruzar el lomo
 * (GrimoireBookLayout); sin ella, de una en una. Hasta S-049 lo pintaba SGrimoirePanel; se sacó sin cambiar su aspecto. Slate puro: solo pinta lo
 * que le dan.
 * En el Grimorio, la pieza ocupa la fila de las hojas y el panel le da su zona (Layout); la cabecera, el pie de teclas, el fondo y la marca de agua son
 * del panel. En el menú (Framed, R-HUD-22) se basta sola: pinta el fondo del libro, la marca de agua y la línea de las teclas de página, y mide su
 * zona con su propio tamaño.
 */
class SHowToPlayBookPanel : public SCompoundWidget
{
public:
	/** La imagen de una lámina por su nombre; nula si no hay. */
	using FImageForPage = TFunction<const FSlateBrush*(const FString& ImageName)>;

	SLATE_BEGIN_ARGS(SHowToPlayBookPanel) : _FontSize(13), _WardenColor(FLinearColor(1.f, 0.45f, 0.05f)), _DoublePage(false), _Framed(false), _PageInset(12.f),
		_SpineGap(0.06f), _WatermarkOpacity(0.25f), _WatermarkSize(0.7f), _PageKeys(false) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta. Si cambia, se rehacen los estilos y la vista. */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** La fuente del Grimorio; sin asset, la de siempre. */
		SLATE_ARGUMENT(Soulbound::Presentation::FGrimoireFont, Font)
		/** El color de las frases del Warden (WardenLineColor). */
		SLATE_ARGUMENT(FLinearColor, WardenColor)
		/** El alto máximo de una lámina dentro del texto, en fracción del alto de su hoja (HowToPlayImageMaxHeight). */
		SLATE_ATTRIBUTE(float, ImageMaxHeight)
		/** La imagen de un bloque por su nombre (las carga y las mantiene quien la usa: la vista o el widget del menú). */
		SLATE_ARGUMENT(FImageForPage, ImageForPage)
		/** Los botones "Anterior" y "Siguiente" (y, en el menú, las teclas de página). */
		SLATE_EVENT(FSimpleDelegate, OnPreviousPage)
		SLATE_EVENT(FSimpleDelegate, OnNextPage)
		/** Dos hojas por vista (con la imagen del libro) o una (sin ella). Se decide al construir; en el menú, lo dice BookBrush. */
		SLATE_ARGUMENT(bool, DoublePage)
		/** En el Grimorio: la zona de contenido del panel (sus hojas y el lomo), que se lee cada vez que se pinta. En el menú no se usa. */
		SLATE_ATTRIBUTE(Soulbound::Presentation::FGrimoireBookLayout, Layout)
		/**
		 * En el menú (R-HUD-22): la pieza pinta su propio libro, a su tamaño (el del hueco donde se coloca): el fondo (BookBrush; sin él, el negro del
		 * Grimorio), la marca de agua de cada hoja (Watermark) y, al pie, FootHint. Sin pestañas, sin título y sin las teclas de cerrar ni de pestañas.
		 */
		SLATE_ARGUMENT(bool, Framed)
		SLATE_ARGUMENT(TOptional<FSlateBrush>, BookBrush)
		SLATE_ARGUMENT(float, PageInset)
		SLATE_ARGUMENT(float, SpineGap)
		SLATE_ARGUMENT(TOptional<FSlateBrush>, Watermark)
		SLATE_ARGUMENT(float, WatermarkOpacity)
		SLATE_ARGUMENT(float, WatermarkSize)
		/** La línea de teclas del pie, en el menú (HowToPlayMenuFootHint); vacía, ninguna. */
		SLATE_ARGUMENT(FString, FootHint)
		/** Si las teclas de página pasan de vista cuando la pieza tiene el foco del teclado (en el menú; en el Grimorio las atiende la vista). */
		SLATE_ARGUMENT(bool, PageKeys)
		SLATE_ARGUMENT(FKey, PreviousPageKey)
		SLATE_ARGUMENT(FKey, NextPageKey)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Si cambia el tamaño de letra, se rehacen los estilos del texto enriquecido y la vista (la negrita, S-048). */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** Con PageKeys, la pieza acepta el foco del teclado; sin ellas, no. */
	virtual bool SupportsKeyboardFocus() const override;
	/** Con PageKeys y el foco, las teclas de página pasan de vista; las demás siguen su camino (la navegación del menú). */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/**
	 * El libro (HowToPlayBook) y la página que se ve, desde 0; si el libro no cargó, su mensaje. Se rehace solo si cambia el libro o la página. Con el
	 * libro se ve la vista de esa página (HowToPlayView): ella en la hoja izquierda y la siguiente en la derecha.
	 */
	void SetHowToPlay(const Soulbound::Presentation::FHowToPlayBook& Book, int32 Page);

private:
	/** La zona de contenido: la del panel en el Grimorio; en el menú, la de su propio tamaño. */
	Soulbound::Presentation::FGrimoireBookLayout CurrentLayout() const;
	/** El ancho de una hoja: con el libro, la izquierda o la derecha; sin él, la única. */
	float ColumnWidth(bool bRight) const;
	/** El ancho al que se parte un texto de una hoja: GrimoireColumnTextWidth con el de su hoja y la sangría de la línea; se lee al pintar. */
	TAttribute<float> TextWidth(bool bRight, float Indent) const;
	/** Las hojas y su pie, sin marco: lo que pone el Grimorio en su pestaña. */
	TSharedRef<SWidget> MakePages();
	/** En el menú: el libro entero, con su fondo, su marca de agua y su línea de teclas, alrededor de las hojas. */
	TSharedRef<SWidget> MakeFrame(const TSharedRef<SWidget>& Pages);
	/** Rehace la vista con lo último que se le dio. */
	void Rebuild();
	/** Un botón de pasar página; si no se ve (HowToPlayPageButtons), no se pinta pero conserva su hueco. */
	TSharedRef<SWidget> MakePageButton(const FString& Label, bool bShown, const FSimpleDelegate& OnClicked);
	/** Una hoja: el título de la página, si lo tiene, y sus bloques en su orden, con su propio desplazamiento. Sin página, vacía. */
	TSharedRef<SWidget> MakeSheet(const FHowToPlayPage* Page, bool bRight);
	/** Rehace los estilos del texto enriquecido con el tamaño de letra de ahora. */
	void UpdateStyles();

	TAttribute<int32> FontSize;
	Soulbound::Presentation::FGrimoireFont TextFont;
	FLinearColor WardenColor;
	TAttribute<float> ImageMaxHeight;
	FImageForPage ImageForPage;
	FSimpleDelegate OnPreviousPage;
	FSimpleDelegate OnNextPage;
	bool bDoublePage = false;
	TAttribute<Soulbound::Presentation::FGrimoireBookLayout> Layout;

	bool bFramed = false;
	/** En el menú: la imagen del libro, si la hay; su ImageSize se ajusta al pintar (FGrimoireBookLayout::BrushSize). */
	TOptional<FSlateBrush> BookBrush;
	FVector2f BookImageSize = FVector2f::ZeroVector;
	FMargin BookMargin;
	float PageInset = 12.f;
	float SpineGap = 0.06f;
	TOptional<FSlateBrush> WatermarkBrush;
	float WatermarkOpacity = 0.25f;
	float WatermarkSize = 0.7f;
	FString FootHint;

	bool bPageKeys = false;
	FKey PreviousPageKey;
	FKey NextPageKey;

	/** Las dos hojas; sin el libro, todo en LeftSheet. */
	TSharedPtr<SBox> LeftSheet;
	TSharedPtr<SBox> RightSheet;
	Soulbound::Presentation::FHowToPlayBook Book;
	int32 Page = 0;
	bool bHasBook = false;
	/**
	 * Los estilos del texto enriquecido, uno por marca (Bold, Warden, WardenBold y Subtitle) con la fuente del Grimorio y el tamaño de letra de
	 * ahora; si el tamaño cambia, se rehacen y se rehace la vista.
	 */
	TSharedPtr<class FSlateStyleSet> Styles;
	int32 StyleSize = -1;
};
