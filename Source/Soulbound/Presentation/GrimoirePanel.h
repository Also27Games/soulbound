#pragma once

#include "CoreMinimal.h"
#include "ChronicleLines.h"
#include "GrimoireBook.h"
#include "GrimoireFont.h"
#include "StatusText.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SChroniclePanel;
class SLegendPanel;
class STextBlock;
class SVerticalBox;

namespace Soulbound::Presentation
{
	/**
	 * La tarjeta de un asiento del Grimorio (S-027, R-HUD-4), la misma que reutiliza la carta del Elegido (S-029, R-VIS-13): papel, Elegido y
	 * Dominio, afinidad, las seis runas con su poder y los poderes activos. Sus textos se parten con WrapTextAt a TextWidth, que no depende de lo
	 * pintado antes (S-029, R-VIS-21). Desde S-044 es la tarjeta de la pestaña "Tú".
	 * @param Row La fila de GrimoireRows.
	 * @param FontSize Tamaño de la letra, se lee cada vez que se pinta.
	 * @param DomainColors Color de cada Dominio, para el cuadrito de cada runa.
	 * @param TextWidth Ancho al que se parten los textos (GrimoireCardTextWidth), se lee cada vez que se pinta.
	 * @param Font S-048 (corrección 4 del autor del 17/11/2026): la fuente del Grimorio; sin asset, la de hoy (la carta del Elegido la deja así).
	 * @param bBackground S-048 (corrección 3): si lleva su fondo; sobre la imagen del libro, no (el margen se queda).
	 */
	TSharedRef<SWidget> MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth,
		const FGrimoireFont& Font = FGrimoireFont(), bool bBackground = true);
}

/**
 * El Grimorio en PC (glosario "Grimorio", `Grimoire`): un panel centrado, bajo demanda, que no pausa nada (S-027, R-HUD-4). Desde S-044 (R-HUD-10) es
 * el libro del juego, con cinco pestañas: "Tú" (la tarjeta del humano, su insolencia, la runa de su hogar si es un Eco y, al pie, una línea por poder),
 * "La mesa" y "El tablero" (filas de leyenda), "Cómo se juega" y "Crónica". Cada pestaña se desplaza con la rueda; el panel tiene ancho y alto fijos
 * (los de la capa) para no cambiar de tamaño al cambiar de pestaña. Las pestañas se cambian con un clic en su nombre (OnTabClicked); cuál está activa
 * lo dice el atributo ActiveTab. Slate puro: solo pinta lo que le dan.
 * Desde S-048: "Cómo se juega" es un libro que se lee una página cada vez, con "Anterior", "N de M" y "Siguiente" (R-HUD-18); y, con la imagen del libro
 * abierto (BookBrush), el Grimorio se pinta sobre ella, con el contenido dentro de sus páginas y repartido en dos columnas, una por página, de modo
 * que nada cruza el lomo (R-HUD-20, GrimoireBookLayout). Sin ella, se ve como antes, de una columna.
 */
class SGrimoirePanel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnTabClicked, Soulbound::Presentation::EGrimoireTab);
	/** S-048 (R-HUD-18): la imagen de una página de "Cómo se juega" por su nombre; nula si no hay. */
	using FImageForPage = TFunction<const FSlateBrush*(const FString& ImageName)>;

	SLATE_BEGIN_ARGS(SGrimoirePanel) : _FontSize(13), _PageInset(12.f), _SpineGap(0.06f), _WardenColor(FLinearColor(1.f, 0.45f, 0.05f)) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Color de cada Dominio, el de la vista, para el cuadrito de cada runa. */
		SLATE_ARGUMENT(TArray<FLinearColor>, DomainColors)
		/**
		 * S-048 (corrección 2 del autor del 17/11/2026): la línea pequeña de teclas del pie (GrimoireFootHints): con el libro, cerrar y pestañas al pie
		 * de la página izquierda y páginas al de la derecha; sin él, las dos en una línea. El título dice solo "Grimorio".
		 */
		SLATE_ARGUMENT(Soulbound::Presentation::FGrimoireFootHints, FootHints)
		/** S-048 (corrección 4): la fuente de todo el texto del Grimorio; sin asset, la de hoy. */
		SLATE_ARGUMENT(Soulbound::Presentation::FGrimoireFont, Font)
		/** Ancho del panel (GrimoirePanelWidth de la capa); se lee cada vez que se pinta y no depende de lo pintado (S-029, R-VIS-21). */
		SLATE_ATTRIBUTE(float, PanelWidth)
		/** S-044: alto del panel, fijo; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(float, PanelHeight)
		/** S-044: la pestaña activa. */
		SLATE_ATTRIBUTE(Soulbound::Presentation::EGrimoireTab, ActiveTab)
		/** S-044: el clic en el nombre de una pestaña. */
		SLATE_EVENT(FOnTabClicked, OnTabClicked)
		/**
		 * S-048 (R-HUD-20): la imagen del libro abierto, con su textura, dibujada como caja (Box) con los márgenes del borde (GrimoireBackgroundMargin)
		 * y su tamaño en píxeles como ImageSize. Sin ella, el fondo negro de siempre. Se decide al construir.
		 */
		SLATE_ARGUMENT(TOptional<FSlateBrush>, BookBrush)
		/** S-048 (R-HUD-20): margen dentro de los bordes del libro (GrimoirePageInset). */
		SLATE_ARGUMENT(float, PageInset)
		/** S-048 (R-HUD-20): hueco del lomo, en fracción del ancho del contenido (GrimoireSpineGap). */
		SLATE_ARGUMENT(float, SpineGap)
		/** S-048 (R-HUD-18): ancho mínimo del texto de una página con la imagen al lado, sin el libro (HowToPlayMinTextWidth). */
		SLATE_ATTRIBUTE(float, MinTextWidth)
		/** S-048 (R-HUD-18): el color del comentario del Warden, el de su línea (WardenLineColor). */
		SLATE_ARGUMENT(FLinearColor, WardenColor)
		/** S-048 (R-HUD-18): la imagen de una página por su nombre (las carga y las mantiene la vista). */
		SLATE_ARGUMENT(FImageForPage, ImageForPage)
		/** S-048 (R-HUD-18): los botones "Anterior" y "Siguiente". */
		SLATE_EVENT(FSimpleDelegate, OnPreviousPage)
		SLATE_EVENT(FSimpleDelegate, OnNextPage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** S-048 (R-HUD-18): sin el libro, si la imagen de la página cabe al lado del texto cambia con el ancho; si cambia, se rehace la página. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** "Tú": se rehace solo si cambia respecto a lo último que pintó (R-VIS-21). */
	void SetYou(const Soulbound::Presentation::FGrimoireYouView& View);
	/** "La mesa", con las filas de GrimoireTableRows; solo se rehace si cambian. */
	void SetTable(const TArray<FLegendRow>& Rows);
	/** "El tablero", con las filas de BoardLegendRows; solo se rehace si cambian. */
	void SetBoard(const TArray<FLegendRow>& Rows);
	/**
	 * "Cómo se juega" (S-048, R-HUD-18): el libro (HowToPlayBook) y la página que se ve, desde 0. Si el libro no cargó, su mensaje. Se rehace solo si
	 * cambia el libro o la página.
	 */
	void SetHowToPlay(const Soulbound::Presentation::FHowToPlayBook& Book, int32 Page);
	/** "Crónica", con las líneas de ChronicleLines (la más reciente arriba). */
	void SetChronicle(const TArray<FChronicleLine>& NewestFirst);

private:
	/** El tamaño del panel, el de la capa. */
	FVector2f PanelSize() const;
	/** S-048 (R-HUD-20): dónde va el contenido, con el libro o sin él. */
	Soulbound::Presentation::FGrimoireBookLayout Layout() const;
	/** El ancho de una columna: con el libro, la página izquierda o la derecha; sin él, la única (S-048, R-HUD-20). */
	float ColumnWidth(bool bRight) const;
	/**
	 * El ancho al que se parte un texto de una columna (S-048, corrección 1 del autor del 17/11/2026): GrimoireColumnTextWidth con el de su propia
	 * columna y la sangría de la línea; se lee cada vez que se pinta.
	 */
	TAttribute<float> TextWidth(bool bRight, float Indent) const;
	/** Las cajas de las pestañas dadas, todas del mismo ancho (S-044); con el libro, la cabecera se reparte entre las dos páginas (S-048). */
	TSharedRef<SWidget> MakeTabHeader(const TArray<Soulbound::Presentation::EGrimoireTab>& Tabs);
	/** S-048 (R-HUD-18): rehace la página de "Cómo se juega" con lo último que se le dio. */
	void RebuildHowToPlay();
	/** S-048 (R-HUD-18): un botón de pasar página; apagado, se ve gris y no hace nada. */
	TSharedRef<SWidget> MakePageButton(const FString& Label, bool bEnabled, const FSimpleDelegate& OnClicked);
	/** S-048 (R-HUD-18): sin el libro, si la imagen de la página va al lado del texto con el ancho de ahora. */
	bool ImageBesideNow() const;

	TAttribute<int32> FontSize;
	TAttribute<float> PanelWidth;
	TAttribute<float> PanelHeight;
	TAttribute<Soulbound::Presentation::EGrimoireTab> ActiveTab;
	FOnTabClicked OnTabClicked;
	TArray<FLinearColor> DomainColors;
	/** Lo último que se pintó en "Tú", para no rehacer lo que no cambia. */
	Soulbound::Presentation::FGrimoireYouView LastYou;
	bool bHasYou = false;
	TSharedPtr<SVerticalBox> YouBody;
	TSharedPtr<SLegendPanel> TablePanel;
	TSharedPtr<SLegendPanel> BoardPanel;
	TSharedPtr<SChroniclePanel> Chronicle;

	/** S-048 (R-HUD-20): la imagen del libro, si la hay; su ImageSize se ajusta al pintar (FGrimoireBookLayout::BrushSize). */
	TOptional<FSlateBrush> BookBrush;
	/** El tamaño de la textura del libro, en píxeles; cero sin libro. */
	FVector2f BookImageSize = FVector2f::ZeroVector;
	FMargin BookMargin;
	float PageInset = 12.f;
	float SpineGap = 0.06f;
	/** Con el libro, la otra mitad de cada pestaña, en la página derecha (S-048, R-HUD-20). Sin él, no existen y todo va en la columna única. */
	TSharedPtr<SVerticalBox> YouSide;
	TSharedPtr<SLegendPanel> TableSide;
	TSharedPtr<SLegendPanel> BoardSide;

	/** S-048 (R-HUD-18): "Cómo se juega". Sin el libro, todo en HowToPlayLeft. */
	TSharedPtr<SBox> HowToPlayLeft;
	TSharedPtr<SBox> HowToPlayRight;
	TAttribute<float> MinTextWidth;
	/** S-048 (corrección 4): la fuente del Grimorio. */
	Soulbound::Presentation::FGrimoireFont TextFont;
	FLinearColor WardenColor;
	FImageForPage ImageForPage;
	FSimpleDelegate OnPreviousPage;
	FSimpleDelegate OnNextPage;
	Soulbound::Presentation::FHowToPlayBook HowToPlayBook;
	int32 HowToPlayPage = 0;
	bool bHasHowToPlay = false;
	/** Sin el libro: si la imagen iba al lado la última vez que se rehízo la página. */
	bool bLastImageBeside = true;
};
