#pragma once

#include "CoreMinimal.h"
#include "Elects/Elects.h"

struct FMatchState;
struct FPendingChoice;

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
		/** Color de la línea de objetivo (S-040, R-HUD-7): ABoardViewActor::GoalLineColor. */
		FLinearColor GoalLineColor = FLinearColor::White;

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
	 * La línea de objetivo del asiento humano (S-040, R-HUD-7, nombre propuesto): su meta (SeatGoal, R-VIC-5) con palabras. "Llega a un Nicho libre";
	 * "Consigue runas en las Fuentes (N/6)" para un Elegido y "Consigue runas en las Fuentes de los Olvidados, o róbalas (N/6)" para un Eco, con N
	 * los tipos de runa distintos que tiene (RuneTypeCount); con los seis, "Supera el desafío final en tu próximo turno" si ya es aspirante (IsAspirant:
	 * en el Altar) y "Vuelve al Altar" si no (corrección del autor del 16/11/2026 tras PIE). Vacía con la meta None. La enseña GoalPanelLines.
	 * @param State El estado de la partida.
	 * @param HumanSeat El asiento humano.
	 * @return La línea, o vacía.
	 */
	FString GoalLineText(const FMatchState& State, int32 HumanSeat);

	/**
	 * Lo que pinta la caja de la línea de objetivo (S-040, R-HUD-7, nombre propuesto), arriba a la izquierda y encima del panel de estado (corrección del
	 * autor del 16/11/2026 tras PIE: entre las líneas del panel quedaba escondida). Una sola línea, GoalLineText, con la letra grande y el color
	 * Context.GoalLineColor; ninguna en el prólogo (Context.bPrologue) ni con la meta None. Sin líneas, la vista oculta la caja.
	 * @param State El estado de la partida.
	 * @param Context Lo que no está en el estado: el asiento humano, el prólogo y el color.
	 * @return La línea de la caja, o ninguna.
	 */
	TArray<FPanelLine> GoalPanelLines(const FMatchState& State, const FStatusContext& Context);

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

	// ---- Textos de la elección pendiente, que comparten el panel de estado y el de decisión (S-028; en el .cpp desde S-027) ----

	/**
	 * Tecla de la opción en la posición Index de Choice (S-010), como la contestan las teclas: "1" a "9" y, si la elección no admite "no", "0"
	 * para la décima. Vacía si no tiene tecla. La línea de petición la enseña entre corchetes (OptionKey).
	 */
	FString ChoiceOptionKey(const FPendingChoice& Choice, int32 Index);

	/** Texto de la opción en la posición Index de la elección pendiente de State (S-009, S-010): "dos dados", "Robar al asiento 2"... */
	FString OptionText(const FMatchState& State, int32 Index);

	/**
	 * Qué se elige al señalar a un rival en el evento en curso (S-015): a quién se pone a prueba, a quién se roba, con quién se bate o, con dos
	 * rivales, quién reta y a quién. Solo lee el estado.
	 */
	FString ChosenRivalText(const FMatchState& State);

	/** Qué se elige en la elección pendiente de State, como lo dice la línea de petición tras "ELIGE": "dados", "salto planar", "esfera"... */
	FString ChoiceWhat(const FMatchState& State);

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
	 * Si quien mira puede ver los tipos de runa del Brazal de un asiento (S-035, R-RUN-7): sí si es el suyo o si ese Brazal está revelado
	 * (IsBracerRevealed); no en el resto. Un asiento que mira no válido (INDEX_NONE) no es ningún asiento: solo ve lo revelado.
	 * @param State El estado de la partida.
	 * @param Seat El asiento cuyo Brazal se mira.
	 * @param ViewerSeat El asiento de quien mira (el humano de esta máquina).
	 * @return Verdadero si puede ver los tipos de runa y los poderes activos de Seat.
	 */
	bool CanSeeBracer(const FMatchState& State, int32 Seat, int32 ViewerSeat);

	/**
	 * Estados del asiento Seat que dejan los efectos del Warden (S-014), tal como los ve el asiento ViewerSeat (S-035, R-HUD-6): la insolencia y
	 * los castigos pendientes solo si Seat es ViewerSeat; la Prueba del Warden, el Marcado y el Brazal revelado, de todos. Solo lee el estado.
	 */
	FString EffectStatusText(const FMatchState& State, int32 Seat, int32 ViewerSeat);

	/**
	 * Lo mismo que EffectStatusText pero entero, sin filtrar por quien mira: insolencia y castigos de todos (S-035). Es para el panel de
	 * depuración, que es del autor. Una función aparte y no un asiento especial, para que un asiento humano no válido nunca enseñe todo.
	 */
	FString EffectStatusTextAll(const FMatchState& State, int32 Seat);

	/**
	 * Brazal de un asiento (S-008, solo lectura) tal como lo ve ViewerSeat (S-035, R-RUN-7): si CanSeeBracer, sus runas por tipo
	 * (D<n>×cantidad, solo las que tiene) y sus poderes activos; si no, solo cuántas runas tiene. Paso Libre por afinidad (Fenrir) se dice en
	 * los dos casos: la afinidad de un Elegido es pública.
	 */
	FString BracerText(const FMatchState& State, int32 Seat, int32 ViewerSeat);

	/**
	 * Línea de un Dominio en la leyenda: "D0 El Banquete · Shuten-dōji: del asiento 0" (con dueño), "...: libre" o "...: Olvidado". Un Dominio
	 * que no existe, solo "D<n>".
	 * @param bForgotten Si la vista lo pinta como Olvidado (en el prólogo, con la frase de corrupción).
	 */
	FString DomainLegendText(const FMatchState& State, int32 DomainId, bool bForgotten);

	/**
	 * Línea de un asiento en la leyenda: "0 (tú): Elegido, Shuten-dōji | insolencia 0 de 5", "1: Elegido, Tifón" o "2: Eco, con hogar en D2",
	 * y detrás lo del Vacío, Marcado y Brazal revelado. "(tú)" es el asiento ViewerSeat; la insolencia, solo la suya (EffectStatusText).
	 */
	FString SeatLegendText(const FMatchState& State, int32 Seat, int32 ViewerSeat);

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

		bool operator==(const FGrimoireRune& Other) const
		{
			return RuneType == Other.RuneType && Count == Other.Count && bHasPower == Other.bHasPower && Power == Other.Power && PowerName == Other.PowerName && bActive == Other.bActive;
		}
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
		/** S-035 (R-RUN-7): los tipos de runa y los poderes activos de este asiento no se enseñan a quien mira; Runes va sin cantidades ni activos. */
		bool bTypesHidden = false;
		/** S-035: cuántas runas tiene el asiento en total, sean de los tipos que sean. Es lo único que se dice de un Brazal oculto. */
		int32 TotalRunes = 0;

		/** Igualdad de todo lo que se pinta (S-029, R-VIS-21): el panel solo se reconstruye si cambia. */
		bool operator==(const FGrimoireRow& Other) const
		{
			return Seat == Other.Seat && bHuman == Other.bHuman && bElect == Other.bElect && ElectName == Other.ElectName && DomainId == Other.DomainId
				&& DomainName == Other.DomainName && AffinityName == Other.AffinityName && AffinityText == Other.AffinityText && Runes == Other.Runes
				&& ActivePowers == Other.ActivePowers && Color == Other.Color && bTypesHidden == Other.bTypesHidden && TotalRunes == Other.TotalRunes;
		}
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

	/** Ancho máximo del panel del Grimorio, en unidades de Slate. */
	inline constexpr float GrimoireMaxWidth = 1100.f;

	/**
	 * Ancho del panel del Grimorio (S-029, R-VIS-21): el de la capa menos 32 de margen, con tope GrimoireMaxWidth. Solo depende del ancho de la
	 * capa, nunca de lo pintado antes, para que el panel no cambie de tamaño de un fotograma a otro.
	 * @param LayerWidth Ancho de la capa que lo contiene, en unidades de Slate.
	 * @return El ancho del panel, al menos 1.
	 */
	float GrimoirePanelWidth(float LayerWidth);

	/**
	 * Ancho al que se parten los textos de una tarjeta del Grimorio (WrapTextAt, no AutoWrapText: S-029, R-VIS-21). Es el de una celda del panel
	 * (su ancho menos el margen de 32 entre Columns) menos los 24 de margen de la tarjeta. Dos tarjetas por fila caben mientras el ancho del panel
	 * sea el de GrimoirePanelWidth.
	 * @param PanelWidth Ancho del panel.
	 * @param Columns Tarjetas por fila: 2 en el Grimorio, 1 en la carta del Elegido.
	 * @return El ancho de texto, al menos 1.
	 */
	float GrimoireCardTextWidth(float PanelWidth, int32 Columns);

	/**
	 * Las líneas del pie del Grimorio: un poder por línea, "Nombre: frase", en el orden de EPower, con la frase de Texts.
	 * @param Texts Los textos de grimorio.json.
	 * @return Seis líneas.
	 */
	TArray<FString> GrimoirePowerLines(const FGrimoireTexts& Texts);
}
