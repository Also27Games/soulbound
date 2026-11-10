#pragma once

#include "CoreMinimal.h"
#include "Elects/Elects.h"

struct FMatchState;

/**
 * Lo que enseñan los paneles del HUD (S-027, R-HUD-5): el de estado, el de depuración y el Grimorio, como texto y color, y la escala de los
 * paneles con la altura de la pantalla. Funciones puras sobre el estado de la partida: sin actor, sin mundo y, salvo el cargador de
 * grimorio.json, sin disco, con tests como `BoardLayout`. Los paneles (SStatusPanel, SGrimoirePanel) solo pintan lo que esto devuelve.
 */
namespace Soulbound::Presentation
{
	/** Una línea de un panel de texto (S-027, nombre propuesto): el texto, su color y si va con la letra grande del panel. */
	struct FPanelLine
	{
		FString Text;
		FLinearColor Color = FLinearColor::White;
		/** La petición al jugador y el resultado de la partida (R-HUD-1): con la letra mayor del panel. */
		bool bLarge = false;

		bool operator==(const FPanelLine& Other) const { return Text == Other.Text && Color == Other.Color && bLarge == Other.bLarge; }
	};

	/**
	 * Lo que los paneles de estado y de depuración necesitan y no está en FMatchState (S-027, nombre propuesto): lo pone la vista con lo que
	 * sabe del anfitrión, del Warden y del perfil en cada refresco.
	 */
	struct FStatusContext
	{
		/** El asiento humano de esta máquina; INDEX_NONE si no hay. */
		int32 HumanSeat = INDEX_NONE;
		/** Si se enseña el texto de petición ("<- tu turno: ..."), según el ajuste de ayudas (S-021, ShowsRequestHints). */
		bool bHints = true;
		/** El prólogo del Warden en marcha (S-022). */
		bool bPrologue = false;
		/** La mesa espera a que el Warden termine de hablar (S-023). */
		bool bWardenSpeaking = false;
		/** El interruptor del anillo encendido: el humano juega sus pruebas y sus duelos con el anillo (S-026). */
		bool bHumanRing = false;
		/** El panel de decisión está a la vista (S-028, R-DEC-3): la línea de petición del humano no se repite aquí. */
		bool bDecisionPanelShown = false;
		/** El paso a paso espera a la tecla (S-007). */
		bool bStepWait = false;
		/** El paso a paso activado. */
		bool bStepMode = false;
		/** El Warden evalúa solo (SbWardenMode auto). */
		bool bWardenOn = true;
		/** Semilla de la partida. */
		int32 Seed = 0;
		/** Las teclas de la vista en una línea (DescribeViewKeys), para la línea del paso a paso. */
		FString ViewKeys;
		/** La tecla de volver al menú, para el resultado de la partida (MatchResultText). */
		FString ReturnKey;
		/** Color de cada asiento, el de la vista. */
		TArray<FLinearColor> SeatColors;

		/** El color del asiento Seat; blanco si no lo hay. */
		FLinearColor SeatColor(int32 Seat) const { return SeatColors.IsValidIndex(Seat) ? SeatColors[Seat] : FLinearColor::White; }
	};

	/**
	 * Las líneas del panel de estado (R-HUD-1): fase de la partida y prólogo, tirada y reloj de turno, Sellos activos, carrera, la petición al
	 * jugador, trilero, duelo, evento del Warden, Noche de los Ecos, Oráculo, votación, aspirantes y, con la partida terminada, el resultado.
	 * Los textos y colores de antes de S-027, sin las teclas de depuración. Van primero, con la letra grande, el resultado si la partida ha
	 * terminado y la petición al jugador: su elección pendiente o, en su turno de tirada, la línea de la tirada con "tu turno".
	 * @param State El estado de la partida.
	 * @param Context Lo que no está en el estado.
	 * @return Las líneas, de arriba abajo; ninguna lleva la semilla.
	 */
	TArray<FPanelLine> StatusLines(const FMatchState& State, const FStatusContext& Context);

	/**
	 * Las líneas del panel de depuración (R-HUD-2): paso a paso, evaluación del Warden, semilla con el tiempo y la ronda, y los asientos con su
	 * insolencia. Ninguna está también en el panel de estado.
	 * @param State El estado de la partida.
	 * @param Context Lo que no está en el estado.
	 * @return Las líneas, de arriba abajo.
	 */
	TArray<FPanelLine> DebugLines(const FMatchState& State, const FStatusContext& Context);

	/**
	 * Tamaño de la letra de los paneles, en píxeles de pantalla, para una pantalla de ViewportHeight de alto (R-HUD-3): FontSizeAt1080 a 1080 y
	 * en proporción a la altura, nunca menos de MinFontSize. La vista lo divide por la escala de DPI que Unreal ya aplica a la interfaz.
	 * @param ViewportHeight Altura del viewport en píxeles.
	 * @param FontSizeAt1080 El tamaño a 1080 de alto (PanelFontSize).
	 * @param MinFontSize El mínimo (PanelMinFontSize).
	 * @return El tamaño, redondeado al entero más cercano.
	 */
	int32 ScaledPanelFontSize(float ViewportHeight, int32 FontSizeAt1080, int32 MinFontSize);

	/**
	 * Ancho de la columna de paneles de la derecha, en píxeles de pantalla, para una pantalla de ViewportHeight de alto (R-HUD-3): WidthAt1080
	 * a 1080 y en proporción a la altura.
	 * @param ViewportHeight Altura del viewport en píxeles.
	 * @param WidthAt1080 El ancho a 1080 de alto (PanelWidthPixels).
	 * @return El ancho; 0 si alguno de los dos no es positivo.
	 */
	float ScaledPanelWidth(float ViewportHeight, float WidthAt1080);

	// ---- Textos de asiento que comparten los paneles y la leyenda (de S-006 a S-016; en BoardViewActor.cpp hasta S-027) ----

	/** Nombre del Elegido del Dominio, de elegidos.json (S-008); vacío si el Dominio no tiene Elegido. */
	FString ElectName(const FMatchState& State, int32 DomainId);

	/** "El Banquete · Shuten-dōji": el nombre del Dominio (del tablero) y el de su Elegido (de elegidos.json). */
	FString DomainTitle(const FMatchState& State, int32 DomainId);

	/**
	 * Estado del Vacío del asiento Seat para la vista (S-013): "EN EL VACÍO, turno n", con n el turno dentro que juega o, si no lo está jugando,
	 * el siguiente. Vacío si no está en el Vacío. Solo lee el estado.
	 */
	FString VoidText(const FMatchState& State, int32 Seat);

	/**
	 * Estados del asiento Seat que dejan los efectos del Warden (S-014): insolencia, castigos pendientes, Prueba del Warden, Marcado y Brazal
	 * revelado. Solo lee el estado.
	 */
	FString EffectStatusText(const FMatchState& State, int32 Seat);

	/**
	 * Brazal de un asiento (S-008, solo lectura): sus runas por tipo (D<n>×cantidad, solo las que tiene) y sus poderes activos. Paso Libre por
	 * afinidad (Fenrir) se marca aparte, porque no depende de ninguna runa.
	 */
	FString BracerText(const FMatchState& State, int32 Seat);

	/** Nombre de un poder (glosario): "Duelo", "Celeridad", "Paso Libre", "Expulsión", "Vuelo Planar", "Desvío". */
	const TCHAR* PowerName(EPower Power);

	// ---- El Grimorio (R-HUD-4) ----

	/** Ruta de los textos del Grimorio, relativa a Content/ (se empaqueta con el resto de Data/). */
	inline const FString GrimoireRelativePath = TEXT("Data/Textos/grimorio.json");

	/**
	 * Los textos del Grimorio (S-027, nombre propuesto), de grimorio.json: nombre y frase de cada afinidad, frase de cada poder. Lo que falta no
	 * está en los mapas; las funciones de acceso lo sustituyen por el nombre del enumerado.
	 */
	struct FGrimoireTexts
	{
		TMap<EAffinity, FString> AffinityNames;
		TMap<EAffinity, FString> AffinityTexts;
		TMap<EPower, FString> PowerTexts;
	};

	/** Resultado de leer grimorio.json: los textos que haya y, si falta el archivo o alguna clave, el motivo (vacío si está todo). Nunca falla. */
	struct FGrimoireTextsLoadResult
	{
		FGrimoireTexts Texts;
		FString Message;

		bool IsComplete() const { return Message.IsEmpty(); }
	};

	/**
	 * Lee los textos del Grimorio de un JSON: `{"afinidades": {"<afinidad>": {"nombre": "...", "frase": "..."}}, "poderes": {"<poder>": "..."}}`,
	 * con las claves de elegidos.json (el nombre del enumerado con la inicial en minúscula: "freePassage", "planarFlight").
	 * @param Json El texto del archivo.
	 * @return Los textos leídos y, si falta algo o el JSON no se entiende, el motivo con las claves que faltan.
	 */
	FGrimoireTextsLoadResult ParseGrimoireTexts(const FString& Json);

	/**
	 * Lee grimorio.json de un archivo (ruta absoluta) con ParseGrimoireTexts. Sin archivo, textos vacíos y el motivo.
	 * @param FilePath Ruta del archivo.
	 * @return Lo que diga ParseGrimoireTexts, o textos vacíos y el motivo si no se puede leer.
	 */
	FGrimoireTextsLoadResult LoadGrimoireTexts(const FString& FilePath);

	/** El nombre de la afinidad, de los textos; sin él, el nombre del enumerado ("FreePassage"). */
	FString AffinityName(const FGrimoireTexts& Texts, EAffinity Affinity);

	/** La frase de la afinidad, de los textos; sin ella, el nombre del enumerado. */
	FString AffinityText(const FGrimoireTexts& Texts, EAffinity Affinity);

	/** La frase del poder, de los textos; sin ella, el nombre del enumerado ("PlanarFlight"). */
	FString PowerText(const FGrimoireTexts& Texts, EPower Power);

	/** Una runa en una fila del Grimorio: su tipo (el Dominio), cuántas tiene y, si es Elegido, el poder que le da según su mapa. */
	struct FGrimoireRune
	{
		int32 RuneType = INDEX_NONE;
		int32 Count = 0;
		/** Si la runa da un poder a este asiento: solo a un Elegido, según su mapa (R-ELE-3). */
		bool bHasPower = false;
		EPower Power = EPower::Duel;
		/** Nombre del poder (glosario); vacío si no da ninguno. */
		FString PowerName;
		/** Si el poder está activo ahora (IsPowerActive). */
		bool bActive = false;
	};

	/** Una fila del Grimorio (S-027, nombre propuesto): un asiento, su papel, su Elegido y afinidad, sus seis runas y sus poderes activos. */
	struct FGrimoireRow
	{
		int32 Seat = INDEX_NONE;
		bool bHuman = false;
		bool bElect = false;
		/** Nombre del Elegido; vacío si es Eco. */
		FString ElectName;
		/** El Dominio del asiento: el de su Elegido, el hogar de un Eco tras la carrera o INDEX_NONE. */
		int32 DomainId = INDEX_NONE;
		/** Nombre del Dominio (del tablero); vacío sin Dominio. */
		FString DomainName;
		/** La afinidad del Elegido; vacías las dos si es Eco. */
		FString AffinityName;
		FString AffinityText;
		/** Las seis runas, en orden de Dominio. */
		TArray<FGrimoireRune> Runes;
		/** Nombres de los poderes activos (GetActivePowers), en el orden de EPower. */
		TArray<FString> ActivePowers;
		/** El color del asiento. */
		FLinearColor Color = FLinearColor::White;
	};

	/**
	 * Las filas del Grimorio (R-HUD-4), una por asiento, con los textos de afinidad de Texts.
	 * @param State El estado de la partida (Elects, asientos y Brazales).
	 * @param HumanSeat El asiento humano de esta máquina.
	 * @param Texts Los textos de grimorio.json.
	 * @param SeatColors El color de cada asiento, el de la vista.
	 * @return Una fila por asiento, en orden de asiento.
	 */
	TArray<FGrimoireRow> GrimoireRows(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors);

	/**
	 * Las líneas del pie del Grimorio: un poder por línea, "Nombre: frase", en el orden de EPower, con la frase de Texts.
	 * @param Texts Los textos de grimorio.json.
	 * @return Seis líneas.
	 */
	TArray<FString> GrimoirePowerLines(const FGrimoireTexts& Texts);
}
