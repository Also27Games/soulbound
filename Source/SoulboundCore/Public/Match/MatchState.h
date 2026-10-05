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
	/**
	 * Decisiones previas a la tirada: con Celeridad, el asiento elige uno o dos dados (R-TUR-4, elección pendiente
	 * DiceCount, bajo el reloj de turno); sin Celeridad se pasa sin decisión (S-009).
	 */
	PreRoll,
	/** Tirada y elección de destino, un solo paso (R-TUR-7): espera la intención del asiento activo. */
	RollAndChoose,
	/**
	 * Movimiento y resolución de la casilla de destino (R-INT-1): primero el efecto de la casilla (Nicho, Fuente y su
	 * minijuego), después el salto planar y después, como mucho, una acción contra un jugador. El salto y la acción son
	 * elecciones pendientes con su propio reloj (S-009).
	 */
	MoveAndResolve,
	/**
	 * Un Elegido en la Fuente de un Dominio Olvidado juega el minijuego de habilidad (R-RUN-2, R-TAB-7): la máquina espera
	 * el resultado, como espera el destino, y AdvanceTurn no avanza hasta SubmitSkillTrialResult. Sin plazo en S-008.
	 * Nombre propuesto en S-008.
	 */
	SkillTrial,
	/**
	 * Trilero (glosario `ShellGame`, R-ROB-1, R-ROB-2): un asiento roba a otro. La máquina espera mientras dura, como espera
	 * cualquier elección: Koschei aparta una runa (elección SafeRune), muestra, mezcla y elección de esfera (ShellPick),
	 * con los plazos del reloj de partida (FShellGameState). Nombre propuesto en S-010.
	 */
	ShellGame,
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

/**
 * Tipo de una elección pendiente (S-009, nombre propuesto): qué decide el asiento activo y qué significa cada opción.
 * Mitad del motor del componente de selección (glosario "SelectionPrompt", GDD 07): opciones y plazo.
 */
UENUM()
enum class EChoiceKind : uint8
{
	/** Ninguna: no hay elección pendiente. */
	None,
	/** Celeridad (R-TUR-4): cuántos dados tirar; opciones 1 y 2. Obligatoria: no admite "no". Bajo el reloj de turno. */
	DiceCount,
	/** Afinidad de Apep (R-TUR-6, R-ELE-2): repetir la tirada; opción 1, "repetir". Bajo el reloj de turno. */
	Reroll,
	/** Vuelo Planar (R-POD-5, R-TAB-15): saltar a otro Nodo Planar; las opciones son casillas. Con el reloj de elección. */
	PlanarJump,
	/**
	 * Acción contra un jugador (R-INT-2, S-010; sustituye a Expel de S-009): qué hacer y a quién, en una sola decisión. Las
	 * opciones son pares (acción, asiento objetivo) en FPendingChoice::ActionOptions y la intención contesta con el índice
	 * de la opción. Expulsión (R-POD-4) y robo del Eco (R-ECO-4). Con el reloj de elección.
	 */
	PlayerAction,
	/**
	 * Afinidad de Koschei (R-ROB-5, R-ELE-2): qué runa aparta antes del trilero; las opciones son los tipos de runa de su
	 * Brazal. La contesta la víctima, no el asiento activo. Obligatoria: no admite "no"; agotada, se juega con todas. Con el
	 * reloj de elección. S-010.
	 */
	SafeRune,
	/**
	 * Elección de esfera del trilero (R-ROB-2, R-ROB-3): las opciones son las posiciones finales de las esferas, de 0 en
	 * adelante. La contesta el ladrón. Obligatoria: no admite "no"; agotada, no roba nada. Con el tiempo de elección del
	 * trilero (FMatchParams::ShellPickSeconds). S-010.
	 */
	ShellPick,
};

/**
 * Acción contra un jugador (R-INT-2, S-010, nombre propuesto): lo que un asiento puede hacer a otro al terminar su
 * movimiento. Duelo y reto de duelo llegarán en S-011.
 */
UENUM()
enum class EPlayerAction : uint8
{
	/** Expulsión (glosario `PowerExpel`, R-POD-4): el objetivo vuelve a su hogar. */
	Expel,
	/** Robo del Eco (R-ECO-4): el objetivo es la víctima de un trilero. */
	Steal,
};

/** Una opción de la acción contra un jugador (S-010, nombre propuesto): qué acción y contra qué asiento. */
USTRUCT()
struct SOULBOUNDCORE_API FPlayerActionOption
{
	GENERATED_BODY()

	UPROPERTY()
	EPlayerAction Action = EPlayerAction::Expel;

	UPROPERTY()
	int32 TargetSeat = INDEX_NONE;

	bool operator==(const FPlayerActionOption& Other) const { return Action == Other.Action && TargetSeat == Other.TargetSeat; }
};

/**
 * Elección pendiente (S-009, nombre propuesto): a qué asiento se le pide, tipo, opciones y plazo. La máquina de turnos la
 * abre y espera la intención (SubmitChoiceIntent), como espera el destino; agotado el plazo, la resuelve ella. Desde S-010
 * puede pedírsele a un asiento que no tiene el turno (Koschei víctima aparta una runa). Solo la escriben las reglas de
 * Match/TurnRules.h.
 */
USTRUCT()
struct SOULBOUNDCORE_API FPendingChoice
{
	GENERATED_BODY()

	UPROPERTY()
	EChoiceKind Kind = EChoiceKind::None;

	/**
	 * Asiento al que se le pide la elección (S-010): el activo en todas menos SafeRune, que es de la víctima, y ShellPick,
	 * que es del ladrón. SubmitChoiceIntent valida este asiento, no el activo. INDEX_NONE si no hay elección.
	 */
	UPROPERTY()
	int32 Seat = INDEX_NONE;

	/**
	 * Opciones válidas en todas menos PlayerAction; su significado depende de Kind (dados, 1 para repetir, casillas, tipos
	 * de runa o posiciones de esfera). La intención contesta con una de ellas.
	 */
	UPROPERTY()
	TArray<int32> Options;

	/** Opciones de PlayerAction (S-010): pares (acción, asiento objetivo). La intención contesta con el índice de una. */
	UPROPERTY()
	TArray<FPlayerActionOption> ActionOptions;

	/**
	 * Plazo en segundos del reloj de partida: el del reloj de turno (DiceCount, Reroll), el del reloj de elección
	 * (PlanarJump, PlayerAction, SafeRune) o el tiempo de elección del trilero (ShellPick).
	 */
	UPROPERTY()
	double DeadlineSeconds = 0.0;

	bool IsOpen() const { return Kind != EChoiceKind::None; }

	/** Si admite "no" (INDEX_NONE): todas menos la elección de dados, la runa de Koschei y la esfera del trilero. */
	bool CanDecline() const { return Kind != EChoiceKind::DiceCount && Kind != EChoiceKind::SafeRune && Kind != EChoiceKind::ShellPick; }

	/** Cuántas respuestas distintas de "no" admite (S-010): las de ActionOptions en PlayerAction, las de Options en el resto. */
	int32 AnswerCount() const { return Kind == EChoiceKind::PlayerAction ? ActionOptions.Num() : Options.Num(); }

	/**
	 * Respuesta que se entrega por la opción en la posición Index (de 0 a AnswerCount - 1), la que numera la vista: en
	 * PlayerAction el propio índice, en el resto el valor de Options. INDEX_NONE si la posición no existe.
	 */
	int32 AnswerAt(int32 Index) const
	{
		if (Index < 0 || Index >= AnswerCount())
		{
			return INDEX_NONE;
		}
		return Kind == EChoiceKind::PlayerAction ? Index : Options[Index];
	}

	/** Si Answer es una respuesta válida distinta de "no". */
	bool IsValidAnswer(int32 Answer) const { return Kind == EChoiceKind::PlayerAction ? ActionOptions.IsValidIndex(Answer) : Options.Contains(Answer); }
};

/** Etapa del trilero (R-ROB-2, S-010, nombre propuesto), en el orden en que se recorren. */
UENUM()
enum class EShellGameStage : uint8
{
	/** No hay trilero. */
	None,
	/** Koschei víctima elige qué runa aparta (R-ROB-5), con la elección SafeRune. */
	SafeRune,
	/** Muestra: las runas de la víctima, a la vista hasta StageDeadlineSeconds (FMatchParams::ShellRevealSeconds). */
	Reveal,
	/** Mezcla: las esferas se intercambian hasta StageDeadlineSeconds (FMatchParams::ShellShuffleSeconds). */
	Shuffle,
	/** El ladrón elige una esfera, con la elección ShellPick. */
	Pick,
};

/**
 * Trilero en curso (glosario `ShellGame`, R-ROB-1 a R-ROB-6, S-010, nombre propuesto): quién roba a quién, qué runas van en
 * las esferas y con qué semilla se mezclan. La secuencia de intercambios no se guarda: es ShellGameSwaps(Seed, número de
 * esferas, FMatchParams::ShellSwapCount), una función pura que la interfaz podrá animar. Solo lo escriben las reglas de
 * Match/ShellGameRules.h. Las runas no salen del Brazal de la víctima mientras dura: las esferas son una copia.
 */
// TODO(R-RED-1): Orbs y Seed permiten a un cliente calcular el resultado de la mezcla (qué runa acaba en cada esfera) antes
// de elegir. Qué se replica del trilero y cuándo (por ejemplo, la secuencia de intercambios sin las runas, y la runa solo al
// resolverse) se decide en la story de multijugador.
USTRUCT()
struct SOULBOUNDCORE_API FShellGameState
{
	GENERATED_BODY()

	UPROPERTY()
	EShellGameStage Stage = EShellGameStage::None;

	/** Asiento que roba (el ladrón). */
	UPROPERTY()
	int32 Thief = INDEX_NONE;

	/** Asiento al que se roba (la víctima). */
	UPROPERTY()
	int32 Victim = INDEX_NONE;

	/**
	 * Tipo de runa de cada esfera, en el orden de la muestra (la esfera i empieza en la posición i): todas las de la víctima,
	 * duplicados incluidos, en orden de tipo, menos la que aparta Koschei (R-ROB-2, R-ROB-5). Vacío antes de la muestra.
	 */
	UPROPERTY()
	TArray<int32> Orbs;

	/** Tipo de runa que apartó Koschei (R-ROB-5); INDEX_NONE si no apartó ninguna. */
	UPROPERTY()
	int32 SetAsideRune = INDEX_NONE;

	/** Semilla de la mezcla (R-ROB-6): sale de FMatchState::ShellSeedBase y del número de trilero, no del sorteo de los dados. */
	UPROPERTY()
	int32 Seed = 0;

	/** Fin de la etapa Reveal o Shuffle, en segundos del reloj de partida; las etapas con elección usan el plazo de la elección. */
	UPROPERTY()
	double StageDeadlineSeconds = 0.0;

	bool IsActive() const { return Stage != EShellGameStage::None; }
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
	/** Un asiento con Celeridad elige cuántos dados tira (R-TUR-4); Value es el número de dados. Nombre propuesto en S-009. */
	DiceChosen,
	/** Apep repite la tirada (R-TUR-6); Value es la tirada nueva. Nombre propuesto en S-009. */
	Rerolled,
	/** Un asiento salta de un Nodo Planar a otro (R-POD-5); Value es la casilla de destino. Nombre propuesto en S-009. */
	PlanarJumped,
	/**
	 * Un asiento expulsa a otro a su hogar (R-POD-4); Seat es quien expulsa y OtherSeat el expulsado (desde S-010; antes,
	 * Value), sin Value. La casilla de llegada es el hogar del expulsado (HomeTile). Nombre propuesto en S-009.
	 */
	Expelled,
	/**
	 * Se agota el plazo de una elección que admite "no" y se resuelve como "no", sin castigo (S-009); Value es el tipo de
	 * elección (EChoiceKind). La elección de dados no lo deja: su agotamiento es TurnClockExpired. Nombre propuesto en S-009.
	 */
	ChoiceExpired,
	/**
	 * Empieza el trilero: las runas salen a la vista (R-ROB-2); Seat es el ladrón, OtherSeat la víctima y Value el número de
	 * esferas. Va después de la runa que aparta Koschei, si la hay. Nombre propuesto en S-010.
	 */
	ShellGameStarted,
	/** Koschei aparta una runa antes del trilero (R-ROB-5); Seat es Koschei, OtherSeat el ladrón, Value el tipo. S-010. */
	SafeRuneSetAside,
	/** El ladrón se queda la runa de la esfera que eligió (R-ROB-3); Seat el ladrón, OtherSeat la víctima, Value el tipo. S-010. */
	RuneStolen,
	/** El ladrón no elige esfera a tiempo y no roba nada (R-ROB-3); Seat el ladrón, OtherSeat la víctima. S-010. */
	ShellGameExpired,
	/** Robo a quien no tiene runas: no hay trilero (R-ROB-4); Seat el ladrón, OtherSeat la víctima. S-010. */
	StealNoRunes,
	/**
	 * Robo anulado: Koschei víctima con una sola runa (R-ROB-5); Seat el ladrón, OtherSeat Koschei y Value el tipo de esa
	 * runa. Nombre propuesto en S-010.
	 */
	StealCancelled,
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
	 * Segundo asiento implicado (S-010): el afectado, cuando Seat es quien actúa (víctima del robo, expulsado). INDEX_NONE
	 * en los eventos de un solo asiento.
	 */
	UPROPERTY()
	int32 OtherSeat = INDEX_NONE;

	/**
	 * Dato del evento: la tirada en Rolled; la casilla en SealPlaced, SealReleased y ReturnedHome; el Dominio en Bonded
	 * y HomeAssigned; los Elegidos en BondRaceEnded; los de los eventos de S-008, S-009 y S-010, en su tipo; INDEX_NONE en el
	 * resto. Nunca un asiento: el segundo asiento va en OtherSeat (S-010).
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

	/**
	 * Dados del turno en curso (R-TUR-4, R-TUR-5): 1, o 2 si el asiento lo eligió con Celeridad; 0 mientras no se sabe.
	 * La repetición de Apep tira los mismos (R-TUR-6). Nombre propuesto en S-009.
	 */
	UPROPERTY()
	int32 DiceCount = 0;

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

	/** Elección pendiente (S-009); Kind None si no hay. Desde S-010 puede ser de un asiento sin turno (PendingChoice.Seat). */
	UPROPERTY()
	FPendingChoice PendingChoice;

	/** Trilero en curso (S-010); Stage None fuera de la fase ShellGame. */
	UPROPERTY()
	FShellGameState ShellGame;

	/**
	 * Valor base de las semillas de mezcla del trilero (R-ROB-6, S-010): se saca una vez del sorteo de la partida al final
	 * de StartMatch. La semilla de cada trilero lo combina con ShellGameCount; así ningún robo consume el sorteo de los dados
	 * (consumo fijo de S-009). No es la semilla del sorteo de la partida, que nunca va al estado.
	 */
	UPROPERTY()
	int32 ShellSeedBase = 0;

	/** Trileros jugados en la partida (S-010): el contador que se combina con ShellSeedBase. */
	UPROPERTY()
	int32 ShellGameCount = 0;

	/**
	 * Si faltan por sacar del sorteo de la partida los dos valores reservados para la repetición del turno en curso
	 * (decisión del autor del 04/10/2026: todo turno que llega a PreRoll consume exactamente cuatro valores, dos de la
	 * tirada y dos de la repetición, se repita o no). Se sacan al cerrarse la oferta de repetición, o justo después de
	 * la tirada si no hay oferta. No guarda ningún valor futuro. Nombre propuesto en S-009.
	 */
	UPROPERTY()
	bool bRerollValuesPending = false;

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
