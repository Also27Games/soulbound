#pragma once

#include "CoreMinimal.h"
#include "ChronicleLines.h"
#include "GrimoireBook.h"
#include "GrimoireFont.h"
// S-049: GrimoireOnPress y las demás piezas que comparte con el libro de "Cómo se juega".
#include "GrimoireWidgets.h"
#include "StatusText.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"

class SChroniclePanel;
class SHowToPlayBookPanel;
class SLegendPanel;
class SVerticalBox;

namespace Soulbound::Presentation
{
	/**
	 * La tarjeta de un asiento del Grimorio (S-027, R-HUD-4), la misma que reutiliza la carta del Elegido (S-029, R-VIS-13): papel, Elegido y
	 * Dominio, afinidad, las seis runas con su poder y los poderes activos. Sus textos se parten con WrapTextAt a TextWidth, que no depende de lo
	 * pintado antes (S-029, R-VIS-21). Desde S-044 es la tarjeta de la pestaña "Tú"; desde la ronda de maquetación de S-048 (18/11/2026), la hoja del
	 * jugador de "La mesa", con su insolencia y qué hace cada poder.
	 * @param Row La fila de GrimoireRows.
	 * @param FontSize Tamaño de la letra, se lee cada vez que se pinta.
	 * @param DomainColors Color de cada Dominio, para el cuadrito de cada runa.
	 * @param TextWidth Ancho al que se parten los textos (GrimoireCardTextWidth), se lee cada vez que se pinta.
	 * @param Font S-048 (corrección 4 del autor del 17/11/2026): la fuente del Grimorio; sin asset, la de hoy (la carta del Elegido la deja así).
	 * @param bBackground S-048 (corrección 3): si lleva su fondo; sobre la imagen del libro, no (el margen se queda).
	 * @param InsolenceLine S-048 (ronda de maquetación del 18/11/2026): la insolencia, tras la afinidad; vacía, ninguna (la carta del Elegido).
	 * @param bPlayerSheet S-048: si es la hoja del jugador de "La mesa". Desde la ronda de maquetación, cada runa con poder lleva debajo, sangrado y en
	 * gris, qué hace (FGrimoireRune::PowerText); desde los retoques de "La mesa" del 18/11/2026, la cabecera en una línea sin "ELEGIDO:"
	 * (GrimoireCardHeader) y sin la línea final "Poderes activos", que ya dice "(ACTIVO)" en cada runa. Antes se llamaba bPowerTexts. Si no, la carta
	 * del Elegido, como siempre.
	 */
	TSharedRef<SWidget> MakeGrimoireCard(const FGrimoireRow& Row, const TAttribute<int32>& FontSize, const TArray<FLinearColor>& DomainColors, const TAttribute<float>& TextWidth,
		const FGrimoireFont& Font = FGrimoireFont(), bool bBackground = true, const FString& InsolenceLine = FString(), bool bPlayerSheet = false);
}

/**
 * El Grimorio en PC (glosario "Grimorio", `Grimoire`): un panel centrado, bajo demanda, que no pausa nada (S-027, R-HUD-4). Desde S-044 (R-HUD-10) es
 * el libro del juego, con sus pestañas; desde la ronda de maquetación de S-048 (18/11/2026), tres: "La mesa" (a la izquierda la hoja del jugador, su
 * tarjeta con su insolencia y qué hace cada poder; a la derecha, la mesa, con las filas de GrimoireTableRows; sin el libro, una debajo de otra),
 * "Cómo se juega" y "Crónica". Cada pestaña se desplaza con la rueda; el panel tiene ancho y alto fijos
 * (los de la capa) para no cambiar de tamaño al cambiar de pestaña. Las pestañas se cambian con un clic en su nombre (OnTabClicked); cuál está activa
 * lo dice el atributo ActiveTab. Slate puro: solo pinta lo que le dan.
 * Desde S-048: "Cómo se juega" es un libro que se lee una página cada vez, con "Anterior", "N de M" y "Siguiente" (R-HUD-18); y, con la imagen del libro
 * abierto (BookBrush), el Grimorio se pinta sobre ella, con el contenido dentro de sus páginas y repartido en dos columnas, una por página, de modo
 * que nada cruza el lomo (R-HUD-20, GrimoireBookLayout). Sin ella, se ve como antes, de una columna.
 * Desde S-049 (R-HUD-21): la pestaña "Cómo se juega" la pinta una pieza propia, SHowToPlayBookPanel, la misma que el libro del menú. Las hojas van en un
 * solo cambiador, una ranura por pestaña, y cada pestaña reparte las suyas en las dos columnas (antes, un cambiador por columna); lo que se ve no cambia.
 */
class SGrimoirePanel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnTabClicked, Soulbound::Presentation::EGrimoireTab);
	/** S-048 (R-HUD-18): la imagen de una página de "Cómo se juega" por su nombre; nula si no hay. */
	using FImageForPage = TFunction<const FSlateBrush*(const FString& ImageName)>;

	SLATE_BEGIN_ARGS(SGrimoirePanel) : _FontSize(13), _PageInset(12.f), _SpineGap(0.06f), _WatermarkOpacity(0.25f), _WatermarkSize(0.7f),
		_WardenColor(FLinearColor(1.f, 0.45f, 0.05f)) {}
		/** Tamaño de la letra, en unidades de Slate; se lee cada vez que se pinta. */
		SLATE_ATTRIBUTE(int32, FontSize)
		/** Color de cada Dominio, el de la vista, para el cuadrito de cada runa. */
		SLATE_ARGUMENT(TArray<FLinearColor>, DomainColors)
		/**
		 * S-048 (correcciones 2 y 5 del autor del 17/11/2026): los nombres de las teclas configuradas, para la línea pequeña de teclas del pie
		 * (GrimoireFootHints, según la pestaña activa): con el libro, cerrar y pestañas al pie de la página izquierda y páginas al de la derecha, solo
		 * en "Cómo se juega"; sin él, en una línea (FGrimoireFootHints::OneLine). El título dice solo "Grimorio".
		 */
		SLATE_ARGUMENT(Soulbound::Presentation::FGrimoireKeyNames, KeyNames)
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
		/**
		 * S-048 (segunda ampliación del autor del 17/11/2026, R-HUD-18): el alto máximo de una imagen de "Cómo se juega", en fracción del alto de su
		 * hoja (HowToPlayImageMaxHeight). Una página con solo una imagen la pinta a hoja entera.
		 */
		SLATE_ATTRIBUTE(float, ImageMaxHeight)
		/**
		 * S-048 (segunda ampliación y corrección del autor del 17/11/2026, R-HUD-20): la marca de agua de cada hoja de "Cómo se juega" con el libro,
		 * su opacidad y su tamaño en fracción de la hoja (HowToPlayWatermarkTexture, HowToPlayWatermarkOpacity, HowToPlayWatermarkSize). Sin ella,
		 * ninguna marca.
		 */
		SLATE_ARGUMENT(TOptional<FSlateBrush>, Watermark)
		SLATE_ARGUMENT(float, WatermarkOpacity)
		SLATE_ARGUMENT(float, WatermarkSize)
		/** S-048 (R-HUD-18): el color de las frases del Warden de "Cómo se juega", el de su línea (WardenLineColor). */
		SLATE_ARGUMENT(FLinearColor, WardenColor)
		/** S-048 (R-HUD-18): la imagen de un bloque por su nombre (las carga y las mantiene la vista). */
		SLATE_ARGUMENT(FImageForPage, ImageForPage)
		/** S-048 (R-HUD-18): los botones "Anterior" y "Siguiente". */
		SLATE_EVENT(FSimpleDelegate, OnPreviousPage)
		SLATE_EVENT(FSimpleDelegate, OnNextPage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** La hoja del jugador de "La mesa" (S-048, ronda de maquetación): se rehace solo si cambia respecto a lo último que pintó (R-VIS-21). */
	void SetPlayer(const Soulbound::Presentation::FGrimoirePlayerSheet& Sheet);
	/** La mesa de "La mesa", con las filas de GrimoireTableRows; solo se rehace si cambian. */
	void SetTable(const TArray<FLegendRow>& Rows);
	/**
	 * "Cómo se juega" (S-048, R-HUD-18): el libro (HowToPlayBook) y la página que se ve, desde 0. Si el libro no cargó, su mensaje. Se rehace solo si
	 * cambia el libro o la página. Desde la segunda ampliación del autor del 17/11/2026, con el libro se ve la vista de esa página: ella en la hoja
	 * izquierda y la siguiente en la derecha. Desde S-049 (R-HUD-21), lo pinta la pieza del libro (SHowToPlayBookPanel), que también cambia de estilos
	 * si cambia el tamaño de letra.
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
	/**
	 * Las cajas de las pestañas dadas, todas del mismo ancho (S-044). Con el libro, el título va en la hoja izquierda y las pestañas en la derecha, a
	 * la misma altura (ronda de maquetación de S-048, decisión del autor del 18/11/2026).
	 */
	TSharedRef<SWidget> MakeTabHeader(const TArray<Soulbound::Presentation::EGrimoireTab>& Tabs);
	/** S-048 (segunda ampliación, R-HUD-20): la marca de agua de una hoja, detrás de su contenido; solo en "Cómo se juega" y con el libro (GrimoireWatermark). */
	TSharedRef<SWidget> MakeWatermark(bool bRight);

	TAttribute<int32> FontSize;
	TAttribute<float> PanelWidth;
	TAttribute<float> PanelHeight;
	TAttribute<Soulbound::Presentation::EGrimoireTab> ActiveTab;
	FOnTabClicked OnTabClicked;
	TArray<FLinearColor> DomainColors;
	/** Lo último que se pintó en la hoja del jugador, para no rehacer lo que no cambia. */
	Soulbound::Presentation::FGrimoirePlayerSheet LastPlayer;
	bool bHasPlayer = false;
	TSharedPtr<SVerticalBox> PlayerBody;
	TSharedPtr<SLegendPanel> TablePanel;
	TSharedPtr<SChroniclePanel> Chronicle;

	/** S-048 (R-HUD-20): la imagen del libro, si la hay; su ImageSize se ajusta al pintar (FGrimoireBookLayout::BrushSize). */
	TOptional<FSlateBrush> BookBrush;
	/** El tamaño de la textura del libro, en píxeles; cero sin libro. */
	FVector2f BookImageSize = FVector2f::ZeroVector;
	FMargin BookMargin;
	float PageInset = 12.f;
	float SpineGap = 0.06f;

	/**
	 * "Cómo se juega" (S-048, R-HUD-18): desde S-049 (R-HUD-21), el libro es una pieza propia, la misma que la del menú; el panel le da su zona de
	 * contenido (Layout) y le pasa el libro y la página.
	 */
	TSharedPtr<SHowToPlayBookPanel> HowToPlay;
	/** S-048 (segunda ampliación, R-HUD-20): la marca de agua de cada hoja, que pinta el panel detrás de la cabecera y de las hojas. */
	TOptional<FSlateBrush> WatermarkBrush;
	float WatermarkOpacity = 0.25f;
	float WatermarkSize = 0.7f;
	/** S-048 (corrección 4): la fuente del Grimorio. */
	Soulbound::Presentation::FGrimoireFont TextFont;
};
