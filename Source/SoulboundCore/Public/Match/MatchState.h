#pragma once

#include "CoreMinimal.h"
#include "Board/BoardState.h"
#include "Elects/Elects.h"
#include "Match/MatchParams.h"
#include "MatchState.generated.h"

namespace Soulbound::Match
{
	/** Asientos de toda partida: siempre cuatro, los huecos los ocupan bots (glosario "Jugador"). */
	inline constexpr int32 SeatCount = 4;

	/** Jugadores admitidos en una partida: de uno a cuatro (R-TAB-6: con N jugadores, 6 menos N Dominios Olvidados). */
	inline constexpr int32 MinPlayers = 1;
	inline constexpr int32 MaxPlayers = SeatCount;

	/**
	 * Tope del contador de insolencia (glosario "Insolencia": de 0 a 5; al quinto, la Furia). Es la estructura
	 * de la tabla de castigos del Warden, no un valor de equilibrado: por eso es una constante y no un dato.
	 */
	inline constexpr int32 MaxInsolence = 5;
}

/**
 * Fase del turno (R-TUR-2), en el orden en que se recorren. Entre dos turnos, BetweenTurns.
 * Nombre propuesto en S-003 para el glosario.
 */
UENUM()
enum class ETurnPhase : uint8
{
	/** Nadie juega: antes del primer turno o después de terminar uno. El siguiente empieza al avanzar. */
	BetweenTurns,
	/** Inicio de turno: arranca el reloj de turno. Salida del Vacío (R-TUR-3), en su story. */
	Start,
	/** Decisiones previas a la tirada. Hoy se pasa sin decisión; Celeridad (R-TUR-4), en la story de runas. */
	PreRoll,
	/** Tirada y elección de destino, un solo paso (R-TUR-7): espera la intención del asiento activo. */
	RollAndChoose,
	/** Movimiento y resolución de la casilla de destino. */
	MoveAndResolve,
	/**
	 * Un Elegido en la Fuente de un Dominio Olvidado juega el minijuego de habilidad (R-RUN-2, R-TAB-7): la máquina espera
	 * el resultado, como espera el destino, y AdvanceTurn no avanza hasta SubmitSkillTrialResult. Sin plazo en S-008.
	 * Nombre propuesto en S-008.
	 */
	SkillTrial,
	/** Fin de turno. */
	End,
};

/** Qué prueba de habilidad espera el motor (glosario "Minijuego de habilidad"). Nombre propuesto en S-008. */
UENUM()
enum class ESkillTrial : uint8
{
	/** Ninguna: no hay prueba pendiente. */
	None,
	/** Fuente de Olvidado (R-TAB-7): la prueba de un Elegido para obtener una runa. */
	ForgottenSource,
};

/** Dificultad de una prueba (glosario "Minijuego de habilidad"). Nombre propuesto en S-008; hoy solo se usa Easy. */
UENUM()
enum class ESkillDifficulty : uint8
{
	Easy,
	Normal,
	Hard,
};

/** Rol de un asiento (R-CAR-2, R-CAR-5): todos empiezan Eco Perdido; quien reclama un Nicho es Elegido para siempre. Nombre propuesto en S-006. */
UENUM()
enum class ESeatRole : uint8
{
	/** Eco Perdido (glosario `LostEcho`): el estado inicial; quien no se vincula en la carrera se queda así. */
	LostEcho,
	/** Elegido (glosario `Elect`): vinculado al Elegido del Dominio cuyo Nicho reclamó (FSeatState::DomainId). */
	Elect,
};

/** Fase de la partida. Nombre propuesto en S-006. */
UENUM()
enum class EMatchPhase : uint8
{
	/** Carrera por el vínculo (glosario `BondRace`): empieza con la partida (R-CAR-1). */
	BondRace,
	/** Caza de runas (glosario `RuneHunt`): desde el fin de la carrera, si alguien se vinculó (R-CAR-8). */
	RuneHunt,
	/** Partida terminada: la máquina de turnos no avanza más y el Warden no actúa. Resultado en EMatchOutcome. */
	MatchOver,
};

/** Resultado de la partida. Nombre propuesto en S-006. */
UENUM()
enum class EMatchOutcome : uint8
{
	/** La partida sigue. */
	Undecided,
	/** Victoria del Warden (glosario `WardenVictory`): derrota colectiva; en S-006, nadie se vinculó en la carrera (R-CAR-8). */
	WardenVictory,
};

/** Tipo de un evento del registro de partida (R-TUR-15). Nombre propuesto en S-003 para el glosario. */
UENUM()
enum class EMatchEventType : uint8
{
	/** Empieza el turno de un asiento. */
	TurnStarted,
	/** Un asiento tira; Value es la tirada. */
	Rolled,
	/** Se agota el reloj de turno antes de confirmar destino (R-TUR-8). */
	TurnClockExpired,
	/** Se salta el turno de un asiento con turnos perdidos (R-TUR-10). */
	TurnSkipped,
	/** Termina el turno de un asiento. */
	TurnEnded,
	/** El Warden sella una casilla (R-TAB-22); Value es la casilla, sin asiento. Nombre propuesto en S-004. */
	SealPlaced,
	/**
	 * Se libera un Sello, por el tope (R-TAB-22) o por el Warden (R-TAB-23); Value es la casilla, sin asiento.
	 * El liberado por tope va justo antes del SealPlaced que lo provoca. Nombre propuesto en S-004.
	 */
	SealReleased,
	/** Un Eco reclama un Nicho y se vincula a su Elegido (R-CAR-4, R-CAR-5); Value es el Dominio. Nombre propuesto en S-006. */
	Bonded,
	/** Despertar del tablero (R-TAB-9): justo después del primer Bonded, con el mismo asiento. Nombre propuesto en S-006. */
	BoardAwakening,
	/** Un Eco recibe su Dominio Olvidado como hogar al acabar la carrera (R-TAB-12); Value es el Dominio. Nombre propuesto en S-006. */
	HomeAssigned,
	/**
	 * Un Eco que acaba la carrera dentro de un Dominio con dueño vuelve a su hogar (decisión del autor del 01/10/2026,
	 * para que el confinamiento de R-TAB-20 no lo deje encerrado); Value es la casilla. Nombre propuesto en S-006.
	 */
	ReturnedHome,
	/** Victoria del Warden (R-CAR-8): nadie se vinculó; sin asiento, afecta a todos. Nombre propuesto en S-006. */
	WardenVictory,
	/** Fin de la carrera (R-CAR-1, R-CAR-8); Value es el número de Elegidos; sin asiento. Nombre propuesto en S-006. */
	BondRaceEnded,
	/**
	 * Un asiento mueve su ficha: el motor acepta su intención de destino (SubmitDestinationIntent); Value es la casilla
	 * de destino. Va antes de los eventos de resolver la casilla (Bonded...). Añadido por el autor en S-007 para la
	 * vista y la crónica; el camino casilla a casilla queda para la story del Warden. Nombre propuesto en S-007.
	 */
	Moved,
	/** Un asiento obtiene una runa (R-RUN-2); Value es el tipo de runa. Nombre propuesto en S-008. */
	RuneObtained,
	/** Un Elegido supera el minijuego de una Fuente de Olvidado (R-TAB-7); Value es el tipo de runa. Nombre propuesto en S-008. */
	SkillMinigamePassed,
	/** Un Elegido falla el minijuego de una Fuente de Olvidado (R-TAB-7); Value es el tipo de runa. Nombre propuesto en S-008. */
	SkillMinigameFailed,
	/** Un asiento termina en una Fuente sin reserva (R-RUN-4); Value es el tipo de runa. Nombre propuesto en S-008. */
	SourceDepleted,
	/** Una runa de un asiento vuelve a la reserva de su Fuente (R-RUN-6); Value es el tipo de runa. Nombre propuesto en S-008. */
	RuneReturned,
};

/**
 * Evento del registro de partida (R-TUR-15): qué pasó, cuándo en el reloj de partida y a qué asiento.
 * Base de la métrica M1 y de la Crónica. Barato a propósito: sin texto formateado. Nombre propuesto en S-003.
 */
USTRUCT()
struct SOULBOUNDCORE_API FMatchEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EMatchEventType Type = EMatchEventType::TurnStarted;

	/** Marca de tiempo en segundos del reloj de partida. */
	UPROPERTY()
	double ClockSeconds = 0.0;

	/** Asiento afectado; INDEX_NONE en los eventos de Sello, WardenVictory y BondRaceEnded. */
	UPROPERTY()
	int32 Seat = INDEX_NONE;

	/**
	 * Dato del evento: la tirada en Rolled; la casilla en SealPlaced, SealReleased y ReturnedHome; el Dominio en Bonded
	 * y HomeAssigned; los Elegidos en BondRaceEnded; INDEX_NONE en el resto.
	 */
	UPROPERTY()
	int32 Value = INDEX_NONE;
};

/** Estado de un asiento (Seat): quien lo ocupe (humano, bot o dummy) decide; el estado es el mismo. */
USTRUCT()
struct SOULBOUNDCORE_API FSeatState
{
	GENERATED_BODY()

	/** Casilla en la que está la ficha. INDEX_NONE si no está en el tablero (Vacío). */
	UPROPERTY()
	int32 TileId = 0;

	/**
	 * Brazal (glosario "Brazal Arcano"): runas por tipo, seis contadores indexados por el Dominio de la runa (R-RUN-1),
	 * sin límite. Solo las reglas de Match/RuneRules.h los escriben. Sustituye a RuneCount (S-008).
	 */
	UPROPERTY()
	TArray<int32> Runes;

	FSeatState() { Runes.Init(0, Soulbound::Board::DomainCount); }

	/** Runas del tipo RuneType en el Brazal; 0 si el tipo no existe. */
	int32 RuneCountOf(int32 RuneType) const { return Runes.IsValidIndex(RuneType) ? Runes[RuneType] : 0; }

	/** Insolencia (glosario): de 0 a Soulbound::Match::MaxInsolence. */
	UPROPERTY()
	int32 Insolence = 0;

	/** Turnos extra pendientes (R-TUR-9), de 0 al tope de los parámetros de partida. */
	UPROPERTY()
	int32 ExtraTurns = 0;

	/** Turnos perdidos pendientes (R-TUR-10), de 0 al tope de los parámetros de partida. */
	UPROPERTY()
	int32 LostTurns = 0;

	/** Eco Perdido o Elegido (R-CAR-2, R-CAR-5). Solo lo escriben las reglas de Match/BondRaceRules.h. */
	UPROPERTY()
	ESeatRole Role = ESeatRole::LostEcho;

	/**
	 * Dominio del asiento: el de su Elegido si es Elegido (R-CAR-5); el Olvidado asignado como hogar si es Eco tras la
	 * carrera (R-TAB-12); INDEX_NONE para un Eco durante la carrera. Su Nicho es el hogar (R-TAB-13, HomeTile).
	 * Solo lo escriben las reglas de Match/BondRaceRules.h. Nombre propuesto en S-006.
	 */
	UPROPERTY()
	int32 DomainId = INDEX_NONE;
};

/**
 * Estado de partida: datos puros, sin mundo. Lo mismo que replica el servidor, lo que simula el bucle
 * con reloj virtual y lo que reciben el cerebro del Warden y los decisores.
 * Los campos de turno los prepara StartMatch y después solo los escriben las reglas de Match/TurnRules.h.
 */
USTRUCT()
struct SOULBOUNDCORE_API FMatchState
{
	GENERATED_BODY()

	UPROPERTY()
	FBoardState Board;

	/** Parámetros de partida con los que se inició (StartMatch). */
	UPROPERTY()
	FMatchParams Params;

	/** Siempre cuatro asientos (glosario "Jugador"). */
	UPROPERTY()
	TArray<FSeatState> Seats;

	/** Reloj de partida en segundos. Virtual en simulación, real en juego. Nunca se detiene (R-WAR-4c). */
	UPROPERTY()
	double ClockSeconds = 0.0;

	/**
	 * Orden de turno (R-TUR-1): permutación de los asientos, sorteada una vez en StartMatch y fija toda la partida.
	 * Nombre propuesto en S-003.
	 */
	UPROPERTY()
	TArray<int32> TurnOrder;

	/** Asiento al que le toca, o que está jugando su turno. */
	UPROPERTY()
	int32 ActiveSeat = 0;

	/** Fase del turno en curso (R-TUR-2). */
	UPROPERTY()
	ETurnPhase TurnPhase = ETurnPhase::BetweenTurns;

	/** Límite del reloj de turno en segundos del reloj de partida (R-TUR-8). Solo vale mientras hay un turno en curso. */
	UPROPERTY()
	double TurnDeadlineSeconds = 0.0;

	/** Tirada del turno en curso (R-TUR-5); 0 antes de tirar. */
	UPROPERTY()
	int32 Roll = 0;

	/** Casillas alcanzables con la tirada del turno en curso (R-TUR-7): contra ellas se valida la intención de destino. */
	UPROPERTY()
	TArray<int32> ReachableOptions;

	/** Turnos terminados, incluidos los agotados; los saltados por turnos perdidos no cuentan. */
	UPROPERTY()
	int32 TurnCount = 0;

	/** Registro de eventos (R-TUR-15), en orden de llegada. */
	UPROPERTY()
	TArray<FMatchEvent> EventLog;

	/** Los seis Elegidos: nombre, afinidad y mapa de runa a poder (R-ELE-1, R-ELE-3). Los carga StartMatch (S-008). */
	UPROPERTY()
	FElectsTable Elects;

	/**
	 * Si el asiento activo ha movido su ficha en el turno en curso (R-RUN-3: quedarse en una Fuente sin casillas
	 * alcanzables no cuenta como llegada). Lo pone SubmitDestinationIntent y lo limpia el inicio de cada turno, los
	 * extra incluidos. Nombre propuesto en S-008.
	 */
	UPROPERTY()
	bool bActiveSeatMoved = false;

	/** Prueba de habilidad que espera el motor en la fase SkillTrial; None fuera de ella. Nombre propuesto en S-008. */
	UPROPERTY()
	ESkillTrial PendingTrial = ESkillTrial::None;

	/** Dificultad de la prueba pendiente. Solo vale con PendingTrial distinto de None. */
	UPROPERTY()
	ESkillDifficulty PendingDifficulty = ESkillDifficulty::Easy;

	/** Fase de la partida (S-006). Solo la escriben las reglas de Match/BondRaceRules.h. */
	UPROPERTY()
	EMatchPhase MatchPhase = EMatchPhase::BondRace;

	/** Resultado de la partida; Undecided mientras sigue. Solo lo escriben las reglas de Match/BondRaceRules.h. */
	UPROPERTY()
	EMatchOutcome Outcome = EMatchOutcome::Undecided;

	bool IsMatchOver() const { return MatchPhase == EMatchPhase::MatchOver; }

	/** Tablero de prueba del spike: seis casillas con un ciclo, la 0 es el Altar y los cuatro asientos empiezan en ella. */
	static FMatchState MakeSpikeMatch();

	bool IsTileOccupied(int32 TileId) const;
};
