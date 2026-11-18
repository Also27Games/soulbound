#pragma once

#include "CoreMinimal.h"
#include "DecisionText.h"
#include "HowToPlay.h"
#include "Layout/Margin.h"
#include "LegendPanel.h"
#include "StatusText.h"

struct FMatchState;

/**
 * El Grimorio como libro del juego (S-044, R-HUD-10 a R-HUD-12): sus pestañas (tres desde la ronda de maquetación de S-048), lo que lleva cada una,
 * cuándo se cierra solo y su botón.
 * Lógica pura sobre el estado de la partida y lo que ya calculan las funciones de la leyenda, del Grimorio de S-027 y de "Cómo se juega": sin
 * Slate, sin actor y sin disco, con tests. SGrimoirePanel solo pinta lo que esto devuelve; ABoardViewActor lo alimenta.
 */
namespace Soulbound::Presentation
{
	/**
	 * Las pestañas del Grimorio (S-044, R-HUD-10, nombre propuesto), en su orden. Desde la ronda de maquetación de S-048 (18/11/2026), tres: "Tú" se
	 * juntó con "La mesa" (la hoja del jugador, a la izquierda) y "El tablero" se fue (su leyenda la explican el libro y su lámina rotulada).
	 */
	enum class EGrimoireTab : uint8
	{
		Table,
		HowToPlay,
		Chronicle,
	};

	/** Las tres pestañas, en el orden en que se enseñan: La mesa, Cómo se juega y Crónica. */
	const TArray<EGrimoireTab>& GrimoireTabs();

	/** El nombre de una pestaña tal como se lee en su cabecera ("La mesa", "Cómo se juega", "Crónica"). */
	FString GrimoireTabTitle(EGrimoireTab Tab);

	/**
	 * La pestaña activa del Grimorio (S-044, R-HUD-10, nombre propuesto). Al abrirse vuelve siempre a la primera, "La mesa" (decisión del autor del
	 * 16/11/2026: no recuerda la última; hasta la ronda de maquetación de S-048, "Tú"). Con las flechas pasa a la siguiente o a la anterior de forma
	 * cíclica (aceptado por el autor el 17/11/2026). Desde S-048 (R-HUD-18) lleva también la página de "Cómo se juega", que sí se recuerda durante la
	 * partida: abrir el Grimorio no la toca.
	 */
	struct FGrimoireTabState
	{
		EGrimoireTab Active = EGrimoireTab::Table;

		/** S-048 (R-HUD-18): la página de "Cómo se juega" que se ve, desde 0. Una partida nueva empieza en la primera. */
		int32 Page = 0;

		/** El Grimorio se abre: la pestaña activa es "La mesa"; la página de "Cómo se juega" se queda donde estaba (S-048, R-HUD-18). */
		void Open();
		/**
		 * S-048 (R-HUD-18): la vista siguiente; en la última, se queda (no es cíclico: un libro tiene principio y final). Desde la segunda ampliación
		 * del autor del 17/11/2026, una vista son PagesPerView páginas y Page queda en la primera de la suya: lo que se recuerda es la vista.
		 * @param PageCount Cuántas páginas tiene el libro, la de teclas incluida.
		 * @param PagesPerView Cuántas se ven a la vez (HowToPlayPagesPerView): 2 con el libro, 1 sin él.
		 */
		void NextPage(int32 PageCount, int32 PagesPerView);
		/**
		 * S-048 (R-HUD-18): la vista anterior; en la primera, se queda.
		 * @param PageCount Cuántas páginas tiene el libro, la de teclas incluida.
		 * @param PagesPerView Cuántas se ven a la vez (HowToPlayPagesPerView).
		 */
		void PreviousPage(int32 PageCount, int32 PagesPerView);
		/** La siguiente pestaña; desde "Crónica", "La mesa". */
		void Next();
		/** La anterior; desde "La mesa", "Crónica". */
		void Previous();
		/** Una pestaña concreta (el clic en su nombre). */
		void Select(EGrimoireTab Tab);
	};

	/**
	 * La hoja del jugador de "La mesa" (S-048, ronda de maquetación del 18/11/2026, R-HUD-10, nombre propuesto): su fila del Grimorio, con la frase de
	 * cada poder en sus runas, y su insolencia.
	 */
	struct FGrimoirePlayerSheet
	{
		/** Su fila de GrimoireRows; si es un Eco con hogar, la runa de su hogar le da Celeridad (R-ECO-7). */
		FGrimoireRow Row;
		/** "Insolencia: N de 5". */
		FString InsolenceLine;

		/** Igualdad de todo lo que se pinta (R-VIS-21): el panel solo se reconstruye si cambia. */
		bool operator==(const FGrimoirePlayerSheet& Other) const
		{
			return Row == Other.Row && InsolenceLine == Other.InsolenceLine;
		}
	};

	/**
	 * La hoja del jugador (S-048, ronda de maquetación del 18/11/2026, R-HUD-10): asiento y papel, afinidad, insolencia, sus seis runas, cada una con
	 * el poder que le da y qué hace ese poder, y sus poderes activos.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano de esta máquina.
	 * @param Texts Los textos de grimorio.json.
	 * @param SeatColors El color de cada asiento, el de la vista.
	 * @return Lo que se pinta; con un asiento humano no válido, una fila vacía y sin insolencia.
	 */
	FGrimoirePlayerSheet GrimoirePlayerSheet(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors);

	/** El texto que marca a un aspirante en "La mesa" (S-044, R-HUD-10). */
	inline const TCHAR* const TableAspirantMark = TEXT(" | ASPIRANTE");

	/** El encabezado de los jugadores de "La mesa" (S-048, ronda de maquetación del 18/11/2026: solo "Jugadores"). */
	inline const TCHAR* const TablePlayersTitle = TEXT("Jugadores");

	/**
	 * La mesa de la pestaña "La mesa" (S-044, R-HUD-10; desde la ronda de maquetación de S-048, su hoja derecha): de cada asiento, lo mismo que
	 * enseñaba la leyenda, con los filtros de R-HUD-6 y R-RUN-7, y la marca de aspirante. Dos filas: "Jugadores", una muestra por asiento de su color,
	 * una debajo de otra (bOnePerLine), cada una en una línea: SeatLegendText, la marca de aspirante y " | " con BracerSummary (sin "Runas: "); la del
	 * humano es como la de los demás, con "(tú)" y "N runas" (decisión del autor del 18/11/2026: sus tipos, sus poderes y su insolencia van en su
	 * hoja, GrimoirePlayerSheet). Y "Los seis Dominios y su Elegido", con una muestra por Dominio de su color (DomainLegendText), como antes.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano de esta máquina (quien mira).
	 * @param ForgottenShown Si la vista pinta cada Dominio como Olvidado (en el prólogo va con la frase de corrupción); falta, no Olvidado.
	 * @param SeatColors El color de cada asiento.
	 * @param DomainColors El color de cada Dominio.
	 * @return Las dos filas, sin muestras con TextGetter (el texto ya va puesto).
	 */
	TArray<FLegendRow> GrimoireTableRows(const FMatchState& State, int32 HumanSeat, const TArray<bool>& ForgottenShown, const TArray<FLinearColor>& SeatColors,
		const TArray<FLinearColor>& DomainColors);

	/**
	 * Los nombres de las teclas que dice el Grimorio en "Cómo se juega" (S-044, R-HUD-10, nombre propuesto); los pone la vista. Desde S-045
	 * (precisión de R-CAM) sin la de la columna ni la de ocultar paneles, y con la de las etiquetas y la de volver al menú. Desde S-048 (R-HUD-18),
	 * con las de página, y son la última página del libro (HowToPlayKeysPage).
	 */
	struct FGrimoireKeyNames
	{
		FString Grimoire;
		FString PreviousTab;
		FString NextTab;
		FString View;
		FString Overlay;
		FString RingStart;
		FString ReturnToMenu;
		/** S-048 (R-HUD-18): las teclas de la página anterior y la siguiente de "Cómo se juega". */
		FString PreviousPage;
		FString NextPage;
	};

	/** El título de la última página del libro, la que pone el código (S-048, R-HUD-18). */
	inline const TCHAR* const HowToPlayKeysTitle = TEXT("Teclas");

	/**
	 * El libro de "Cómo se juega" tal como lo leen el Grimorio y el menú (S-048, R-HUD-18 y R-HUD-19, nombre propuesto): el mismo para los dos, que
	 * se escribe una vez. Si el archivo no cargó, sin páginas y con el mensaje del cargador.
	 */
	struct FHowToPlayBook
	{
		/** El título del libro (el "# " del archivo); vacío si no lo trae. */
		FString Title;
		/** La frase de la portada, del Warden; vacía si no la trae. */
		FString CoverLine;
		/** Las páginas del autor y, al final, la de teclas. Vacío si el archivo no cargó. */
		TArray<FHowToPlayPage> Pages;
		/** El mensaje del cargador si el archivo no cargó; vacío si cargó. */
		FString ErrorMessage;

		bool IsLoaded() const { return ErrorMessage.IsEmpty(); }
	};

	/**
	 * La página "Teclas" (S-048, R-HUD-18; antes, el bloque del final del texto de la pestaña, S-044): la pone el código con los nombres de las
	 * teclas configuradas, en español donde lo haya, para que nunca se contradiga con la configuración. Las del Grimorio, sus pestañas y sus páginas,
	 * 1 a 9 y 0, la de empezar la prueba del anillo y la de volver al menú (una línea si son la misma), la de la vista y la de las etiquetas. Desde
	 * S-045 (precisión de R-CAM), ni las de depuración (T y D) ni las que se fueron con la columna (H y P).
	 * @param Keys Los nombres que da Unreal a las teclas configuradas.
	 * @return La página, sin imagen: desde la ampliación del autor del 17/11/2026, una lista, una tecla por elemento.
	 */
	FHowToPlayPage HowToPlayKeysPage(const FGrimoireKeyNames& Keys);

	/**
	 * El libro (S-048, R-HUD-18 y R-HUD-19): las páginas del archivo y, al final, la de teclas; o, si el cargador falló, ninguna página y su mensaje.
	 * @param Loaded Lo que devolvió LoadHowToPlayText.
	 * @param Keys Los nombres de las teclas configuradas.
	 * @return El libro.
	 */
	FHowToPlayBook HowToPlayBook(const FHowToPlayLoadResult& Loaded, const FGrimoireKeyNames& Keys);

	/**
	 * El pie de una vista (S-048, R-HUD-18): "N de M", con la página contada desde 1. Desde la segunda ampliación del autor del 17/11/2026, a doble
	 * página, "N-N+1 de M", o "N de M" si la vista solo tiene una (la última, con un total impar).
	 * @param Page La página, desde 0; cuenta la vista en que está.
	 * @param PageCount Cuántas hay.
	 * @param PagesPerView Cuántas se ven a la vez (HowToPlayPagesPerView).
	 * @return "2 de 5", "1-2 de 5" o "5 de 5"; sin páginas, vacío.
	 */
	FString HowToPlayPageFooter(int32 Page, int32 PageCount, int32 PagesPerView);

	/**
	 * La primera página de la vista en que está una página (S-048, segunda ampliación del autor del 17/11/2026, R-HUD-18, nombre propuesto).
	 * @param Page La página, desde 0.
	 * @param PagesPerView Cuántas se ven a la vez.
	 * @return La primera de su vista; con una por vista, la misma.
	 */
	int32 HowToPlayViewStart(int32 Page, int32 PagesPerView);

	/**
	 * Cuántas páginas de "Cómo se juega" se ven a la vez (S-048, segunda ampliación del autor del 17/11/2026, R-HUD-18, nombre propuesto): dos con
	 * la imagen del libro, una en cada hoja; una sin ella.
	 */
	int32 HowToPlayPagesPerView(bool bBook);

	/**
	 * El tamaño de una imagen de "Cómo se juega" dentro de una hoja (S-048, segunda ampliación del autor del 17/11/2026, R-HUD-18, nombre propuesto):
	 * el mayor que cabe en el ancho de la hoja y en el alto máximo sin deformarla, agrandándola si es pequeña.
	 * @param SheetWidth El ancho de la hoja.
	 * @param MaxHeight El alto máximo (HowToPlayImageMaxHeight por el alto de la hoja; a hoja entera, el alto de la hoja).
	 * @param ImageSize El tamaño de la textura, en píxeles; sus proporciones.
	 * @return El tamaño; cero si la imagen o los límites no tienen tamaño.
	 */
	FVector2f HowToPlayImageSize(float SheetWidth, float MaxHeight, const FVector2f& ImageSize);

	/**
	 * El sitio de una imagen a hoja entera (S-048, ronda de maquetación del 18/11/2026, R-HUD-18, nombre propuesto): la de una página cuyo único bloque
	 * es una imagen. Centrada en horizontal y en vertical, al mayor tamaño que cabe en la hoja sin deformarse, agrandándose si es pequeña.
	 * @param SheetSize El ancho y el alto que hay de verdad en la hoja.
	 * @param ImageSize El tamaño de la textura, en píxeles; sus proporciones.
	 * @return El rectángulo, en coordenadas de la hoja (el origen, arriba a la izquierda); vacío en el origen si la hoja o la imagen no tienen tamaño.
	 */
	FBox2f HowToPlayFullSheetImage(const FVector2f& SheetSize, const FVector2f& ImageSize);

	/**
	 * El libro aplanado para el bloque de texto del menú de hoy (S-048, precisión del autor de R-HUD-19): el título y el texto de cada página,
	 * separados por una línea en blanco; si el archivo no cargó, vacío.
	 * TODO(R-HUD-19): quitarlo, con UHowToPlayLibrary::HowToPlayText, cuando el widget nuevo del menú lea las páginas.
	 */
	FString HowToPlayFlatText(const FHowToPlayBook& Book);

	/** El margen del Grimorio sin la imagen del libro (S-044; desde S-048, R-HUD-20, la zona de contenido de hoy): 16 a los lados y 12 arriba y abajo. */
	inline const FVector2f GrimoireFlatPadding(16.f, 12.f);

	/**
	 * Dónde va el contenido del Grimorio dentro del panel (S-048, R-HUD-20, nombre propuesto), en coordenadas del panel (el origen, arriba a la
	 * izquierda). Con la imagen del libro, dentro de sus páginas y partido en dos por el lomo; sin ella, la zona de hoy, de una columna.
	 */
	struct FGrimoireBookLayout
	{
		/** Si hay libro: con la imagen del fondo cargada. */
		bool bBook = false;
		/** La zona de contenido: dentro del panel y fuera de los bordes del libro. */
		FBox2f Content = FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
		/** La página izquierda y la derecha, a cada lado del lomo; sin libro, las dos son Content. */
		FBox2f LeftPage = FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
		FBox2f RightPage = FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
		/** El tamaño al que se pinta la imagen del libro para que sus bordes no se deformen (escala uniforme); cero sin libro. */
		FVector2f BrushSize = FVector2f::ZeroVector;
	};

	/**
	 * La zona de contenido del Grimorio (S-048, R-HUD-20). La imagen se estira al panel conservando sus bordes: se escala igual en los dos ejes (la
	 * menor de las dos escalas que la llevan al panel), y sus bordes miden la fracción de la imagen escalada que dice BorderFraction. El contenido
	 * queda dentro de esos bordes con PageInset más de margen, y el lomo (el centro del panel) deja un hueco de SpineGapFraction del ancho del
	 * contenido que ninguna página pisa.
	 * @param PanelSize El tamaño del panel.
	 * @param ImageSize El tamaño de la imagen del libro, en píxeles; cero si no hay textura (entonces, la zona de hoy, GrimoireFlatPadding).
	 * @param BorderFraction Los márgenes del borde de la imagen, como fracción de la imagen (GrimoireBackgroundMargin).
	 * @param PageInset Margen añadido dentro de los bordes, en unidades de Slate (GrimoirePageInset).
	 * @param SpineGapFraction Hueco del lomo, en fracción del ancho del contenido (GrimoireSpineGap).
	 * @return Las zonas; nunca de tamaño negativo.
	 */
	FGrimoireBookLayout GrimoireBookLayout(const FVector2f& PanelSize, const FVector2f& ImageSize, const FMargin& BorderFraction, float PageInset, float SpineGapFraction);

	/** El sitio de la marca de agua de "Cómo se juega" en cada hoja (S-048, segunda ampliación del autor del 17/11/2026, R-HUD-20, nombre propuesto). */
	struct FGrimoireWatermarks
	{
		/** Si hay marca: con la imagen del libro y con la textura de la marca. */
		bool bShown = false;
		/** La de la hoja izquierda y la de la derecha, en coordenadas del panel. */
		FBox2f Left = FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
		FBox2f Right = FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
	};

	/**
	 * Dónde va la marca de agua de cada hoja (S-048, segunda ampliación, corrección del autor del 17/11/2026: en las dos hojas): centrada en su hoja,
	 * al mayor tamaño que cabe sin deformarse en la fracción dada del ancho y del alto de la hoja.
	 * @param Book La zona de contenido del libro (GrimoireBookLayout): sus dos hojas.
	 * @param TextureSize El tamaño de la textura de la marca; cero si no hay.
	 * @param SizeFraction La fracción de la hoja (HowToPlayWatermarkSize).
	 * @return Las dos marcas; sin textura o sin el libro, ninguna.
	 */
	FGrimoireWatermarks GrimoireWatermarks(const FGrimoireBookLayout& Book, const FVector2f& TextureSize, float SizeFraction);

	/**
	 * La barra de desplazamiento de las pestañas del Grimorio (S-048, corrección 1 del autor del 17/11/2026, R-HUD-20): su grosor y su margen, que se
	 * le dan a cada SScrollBox, y el hueco entre el texto y la barra. Lo que reservan entre los tres, GrimoireScrollRoom, se resta siempre, se vea la
	 * barra o no, para que el texto no cambie de ancho al aparecer (R-VIS-21).
	 */
	inline constexpr float GrimoireScrollBarThickness = 8.f;
	inline constexpr float GrimoireScrollBarPadding = 2.f;
	inline constexpr float GrimoireScrollGap = 4.f;
	inline constexpr float GrimoireScrollRoom = GrimoireScrollBarThickness + 2.f * GrimoireScrollBarPadding + GrimoireScrollGap;

	/**
	 * El ancho al que se parte un texto del Grimorio dentro de su columna (S-048, corrección 1 del autor del 17/11/2026, R-HUD-20): el de su propia
	 * columna (con el libro, la página en que va; sin él, la única), menos lo que reserva la barra de desplazamiento y menos la sangría de la línea
	 * (sus márgenes a izquierda y derecha). Así nada se corta por el borde.
	 * @param ColumnWidth El ancho de la columna (FGrimoireBookLayout::LeftPage, RightPage o Content).
	 * @param Indent Los márgenes de la línea, izquierdo y derecho sumados.
	 * @return El ancho de partir; nunca menos de 1.
	 */
	float GrimoireColumnTextWidth(float ColumnWidth, float Indent);

	/**
	 * El margen entre el final de la zona de contenido del Grimorio y la línea de teclas del pie (S-048, corrección 7 del autor del 17/11/2026,
	 * nombre propuesto), en unidades de Slate: el mismo en todas las pestañas, con el libro y sin él, para que la última línea no quede pegada al pie.
	 * Antes, 6.
	 */
	inline constexpr float GrimoireFootGap = 12.f;

	/** El título del Grimorio (S-048, corrección 2 del autor del 17/11/2026): solo "Grimorio"; las teclas van al pie (GrimoireFootHints). */
	inline const TCHAR* const GrimoireTitleText = TEXT("Grimorio");

	/** La línea pequeña de teclas al pie del Grimorio (S-048, corrección 2, nombre propuesto): una mitad por página, para no cruzar el lomo. */
	struct FGrimoireFootHints
	{
		/** Al pie de la página izquierda: cerrar y pestañas ("G para cerrar; flechas izquierda y derecha, pestañas", GrimoireKeysHint). */
		FString Left;
		/** Al pie de la derecha: páginas ("flechas arriba y abajo, páginas"), solo en la pestaña con páginas, "Cómo se juega"; en las demás, vacío. */
		FString Right;

		/**
		 * La línea única del pie sin el libro (S-048, corrección 5 del autor del 17/11/2026, nombre propuesto): las dos partes separadas por "; ", o
		 * solo la izquierda si la derecha está vacía.
		 */
		FString OneLine() const;
	};

	/**
	 * Las teclas del pie del Grimorio (S-048, correcciones 2 y 5 del autor del 17/11/2026), con los nombres de las configuradas en español donde lo
	 * haya. La parte de páginas, solo en "Cómo se juega", la única pestaña con páginas.
	 * @param Keys Los nombres que da Unreal a las teclas configuradas.
	 * @param Tab La pestaña que se enseña.
	 */
	FGrimoireFootHints GrimoireFootHints(const FGrimoireKeyNames& Keys, EGrimoireTab Tab);

	/**
	 * Cuándo se cierra solo el Grimorio (S-044, R-HUD-11, nombre propuesto). Se mira en cada refresco del HUD, esté abierto o no, y dice si acaba
	 * de aparecer algo que lo cierra: una decisión propia nueva (también bloqueada porque habla el Warden) o un anillo del humano. Lo que ya estaba
	 * a la vista no lo cierra: con una decisión propia abierta, el Grimorio se puede abrir para consultar (precisión del autor del 17/11/2026). La
	 * espera a otro asiento, un resultado o un desenlace no lo cierran.
	 */
	class FGrimoireCloseWatch
	{
	public:
		/**
		 * @param Panel Lo que enseña ahora el panel de decisión (PanelView).
		 * @param bRingOpen Si el anillo del humano está abierto.
		 * @return Verdadero si desde la última vez apareció una decisión propia nueva o se abrió el anillo.
		 */
		bool Observe(const TOptional<FDecisionView>& Panel, bool bRingOpen);

	private:
		/** La identidad de la decisión propia que se veía la última vez; vacía si no había. */
		FString LastOwnDecision;
		bool bLastRingOpen = false;
	};

	/** Si el Grimorio se puede abrir (S-044, R-HUD-11): no durante un anillo del humano, ni con el botón ni con la tecla (decisión del autor del 17/11/2026). */
	bool CanOpenGrimoire(bool bRingOpen);

	/**
	 * Lo que dice el título del Grimorio sobre sus teclas (S-044, corrección del autor del 17/11/2026): "G para cerrar; flechas izquierda y derecha,
	 * pestañas" (desde S-045, R-HUD-16, sin la H, que ya no existe). Con otras teclas de pestaña, sus nombres en español donde lo haya
	 * (SpanishKeyName): "Q y E, pestañas".
	 * @param GrimoireKey, PreviousTabKey, NextTabKey Los nombres que da Unreal a las teclas configuradas.
	 */
	FString GrimoireKeysHint(const FString& GrimoireKey, const FString& PreviousTabKey, const FString& NextTabKey);

	/** El texto del botón del Grimorio (S-044, R-HUD-12): "Grimorio (G)", con el nombre de la tecla configurada, en español donde lo haya. */
	FString GrimoireButtonLabel(const FString& KeyName);

	/**
	 * Si el botón del Grimorio se ve (S-044, R-HUD-12, corrección del autor del 17/11/2026): con la regla de la caja de la línea de objetivo, no en
	 * el prólogo, ni con la partida terminada, ni mientras hay una frase del Warden en pantalla. No depende de ninguna tecla de ocultar paneles: no
	 * las recibe. El telón lo mira su capa, como en las demás.
	 */
	bool IsGrimoireButtonShown(bool bPrologue, bool bMatchOver, bool bWardenLineOnScreen);
}
