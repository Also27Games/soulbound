#pragma once

#include "CoreMinimal.h"
#include "Board/BoardState.h"
#include "Elects/Elects.h"
#include "Match/MatchParams.h"
#include "Match/WardenEvents.h"
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
	/**
	 * Inicio de turno: arranca el reloj de turno. Si el asiento está en el Vacío (S-013, R-TUR-3, R-VAC-2), aquí se decide
	 * su salida antes de nada: un Eco sale sin jugar; quien tiene Desvío elige si lo usa (elección Deflection, bajo el reloj
	 * de turno); si no, el minijuego de escape (fase SkillTrial) o, pasada la escalera, la salida sin jugar.
	 */
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
	 * Desde S-013 también el escape del Vacío al empezar el turno (PendingTrial VoidEscape). Nombre propuesto en S-008.
	 */
	SkillTrial,
	/**
	 * Trilero (glosario `ShellGame`, R-ROB-1, R-ROB-2): un asiento roba a otro. La máquina espera mientras dura, como espera
	 * cualquier elección: Koschei aparta una runa (elección SafeRune), muestra, mezcla y elección de esfera (ShellPick),
	 * con los plazos del reloj de partida (FShellGameState). Desde S-015 también entre turnos, como parte de un evento del
	 * Warden (FCurrentWardenEvent::bWaitsForAction): al acabar la máquina vuelve a BetweenTurns y el evento sigue. Nombre
	 * propuesto en S-010.
	 */
	ShellGame,
	/**
	 * Duelo (glosario "Duelo", R-DUE-1, S-012): retador y retado rinden y gana el mayor. La máquina espera el rendimiento de
	 * los dos (SubmitDuelPerformance, uno por participante y en cualquier orden), como espera el minijuego; sin plazo en
	 * S-012. Después, el botín con el trilero (fase ShellGame) o el fin del turno. Nombre propuesto en S-012.
	 */
	Duel,
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
	/**
	 * Escape del Vacío (glosario `VoidEscape`, R-VAC-2, S-013): la prueba de un asiento desterrado al empezar su turno, con la
	 * dificultad del peldaño de su escalera. Superada, vuelve a su hogar; en los dos casos su turno termina sin tirar.
	 */
	VoidEscape,
	/**
	 * Prueba del Warden (glosario `WardenTrial`, R-EFE-10, S-015): la prueba de un asiento con la Prueba pendiente (FSeatState::
	 * bWardenTrialPending) al empezar su siguiente turno jugado, antes de tirar, con la dificultad de datos
	 * (FMatchParams::WardenTrialDifficulty). Superada, el turno sigue; fallada, termina sin tirada y sin consumir el sorteo.
	 */
	WardenTrial,
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
	 * de la opción. Expulsión (R-POD-4), robo del Eco (R-ECO-4) y, desde S-012, duelo (R-POD-3, R-DUE-2). Con el reloj de
	 * elección.
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
	/**
	 * Desvío (glosario `PowerDeflection`, R-POD-6, S-013): un Elegido en el Vacío con el poder Desvío, al empezar su turno,
	 * elige si sacrifica la runa que se lo da para salir y jugar el turno; opción 1, "usar". Admite "no"; agotada, es "no",
	 * sin castigo. Bajo el reloj de turno, en la fase Start.
	 */
	Deflection,
	/**
	 * Dilema (glosario `Dilemma`, R-COM-2, S-014): qué opción del evento en curso elige quien lo dirige; las opciones son sus
	 * índices, de 0 en adelante. Obligatoria; agotada, el Warden resuelve en su contra (una opción al azar) y sube la
	 * insolencia. Con el reloj del dilema (base más incremento por opción). Entre turnos (BetweenTurns).
	 */
	Dilemma,
	/**
	 * Rival señalado (`EEffectTarget::ChosenRival`, R-DIR-1, S-014): a quién va un efecto dañino; las opciones son asientos
	 * válidos (R-DIR-2). La contesta quien dirige el evento. Obligatoria; agotada, el efecto cae sobre él (R-DIR-4) y sube la
	 * insolencia. Con el reloj corto. Entre turnos.
	 */
	ChosenRival,
	/**
	 * Runa regalada (`GiftRune`, R-EFE-16, S-014): qué tipo recibe el objetivo, que puede no tener el turno; las opciones son
	 * los tipos con alguna Fuente con reserva. Obligatoria; agotada, no recibe nada, burla e insolencia (R-DIR-4). Con el
	 * reloj corto. Entre turnos.
	 */
	GiftRune,
	/**
	 * Runa a perder (`LoseRune` "chosen", R-EFE-11, S-014): qué tipo de su Brazal pierde el objetivo; las opciones son los
	 * tipos que tiene. Obligatoria; agotada, una al azar e insolencia (R-DIR-4). Con el reloj corto. Entre turnos.
	 */
	LoseRune,
};

/**
 * Acción contra un jugador (R-INT-2, S-010, nombre propuesto): lo que un asiento puede hacer a otro al terminar su
 * movimiento. Desde S-012 también el duelo.
 */
UENUM()
enum class EPlayerAction : uint8
{
	/** Expulsión (glosario `PowerExpel`, R-POD-4): el objetivo vuelve a su hogar. */
	Expel,
	/** Robo del Eco (R-ECO-4): el objetivo es la víctima de un trilero. */
	Steal,
	/**
	 * Duelo (S-012): el objetivo es el retado. Una sola acción para el duelo portátil (poder Duelo, R-POD-3) y el reto desde
	 * un Nodo de Duelo (R-DUE-2); de dónde sale el reto lo dice el registro (DuelChallenged). Decisión del autor del 06/10/2026.
	 */
	Duel,
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
 * Match/TurnRules.h y, desde S-014, las de los eventos del Warden (Match/WardenEventRules.h), que abren las suyas entre turnos.
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
	 * Plazo en segundos del reloj de partida: el del reloj de turno (DiceCount, Reroll, Deflection), el del reloj de elección
	 * (PlanarJump, PlayerAction, SafeRune), el tiempo de elección del trilero (ShellPick) o, desde S-014, el reloj del dilema
	 * (Dilemma) y el reloj corto (ChosenRival, GiftRune, LoseRune).
	 */
	UPROPERTY()
	double DeadlineSeconds = 0.0;

	bool IsOpen() const { return Kind != EChoiceKind::None; }

	/**
	 * Si admite "no" (INDEX_NONE): todas menos la elección de dados, la runa de Koschei, la esfera del trilero y, desde S-014,
	 * las de los eventos del Warden, donde no contestar es desatenderle (R-DIR-4). Desvío lo admite.
	 */
	bool CanDecline() const
	{
		return Kind != EChoiceKind::DiceCount && Kind != EChoiceKind::SafeRune && Kind != EChoiceKind::ShellPick && !IsWardenEventChoice();
	}

	/** Si es una elección de un evento del Warden (S-014): dilema, rival señalado, runa regalada o runa a perder. */
	bool IsWardenEventChoice() const
	{
		return Kind == EChoiceKind::Dilemma || Kind == EChoiceKind::ChosenRival || Kind == EChoiceKind::GiftRune || Kind == EChoiceKind::LoseRune;
	}

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

/**
 * Duelo en curso (glosario "Duelo", R-DUE-1, S-012, nombre propuesto): quién reta a quién, de dónde salió el reto y el
 * rendimiento de cada uno, que llega por su intención (SubmitDuelPerformance) en cualquier orden. Solo lo escriben las
 * reglas de Match/DuelRules.h. Termina al resolverse (ResolveDuel), antes del trilero del botín.
 */
USTRUCT()
struct SOULBOUNDCORE_API FDuelState
{
	GENERATED_BODY()

	/** Rendimiento que aún no ha llegado: los válidos van de 0 a 1. */
	static constexpr double NoPerformance = -1.0;

	/** Asiento que reta (el activo, o quien dirige un Duelo forzado). INDEX_NONE si no hay duelo o reta el Warden. */
	UPROPERTY()
	int32 Challenger = INDEX_NONE;

	/** Asiento retado: no elige nada (R-DUE-3). */
	UPROPERTY()
	int32 Challenged = INDEX_NONE;

	/** Si el reto salió de un Nodo de Duelo (R-DUE-2), aunque el retador tenga además el poder Duelo. */
	UPROPERTY()
	bool bFromDuelNode = false;

	/** Rendimiento del retador, de 0 a 1; NoPerformance mientras no llega. */
	UPROPERTY()
	double ChallengerPerformance = NoPerformance;

	/** Rendimiento del retado, de 0 a 1; NoPerformance mientras no llega. */
	UPROPERTY()
	double ChallengedPerformance = NoPerformance;

	/**
	 * Duelo contra el Warden (S-015, nombre propuesto `WardenDuel`): quien reta es el Warden, no un asiento. Challenger vale
	 * INDEX_NONE y ChallengerPerformance es el rendimiento fijo de los datos (FMatchParams::WardenDuelPerformance), desde que
	 * se abre; Challenged es el jugador al que se bate.
	 */
	UPROPERTY()
	bool bAgainstWarden = false;

	/**
	 * Lo que está en juego en un Duelo forzado (S-015, variante AllRunes): todas las runas del perdedor. Falso: una runa, con
	 * el trilero (R-DUE-5). Sin efecto en un duelo que no nace de un evento.
	 */
	UPROPERTY()
	bool bAllRunes = false;

	bool IsActive() const { return Challenger != INDEX_NONE || bAgainstWarden; }

	/** Si Seat participa en el duelo en curso. */
	bool IsParticipant(int32 Seat) const { return IsActive() && (Seat == Challenger || Seat == Challenged); }

	/** Si ya llegó el rendimiento de Seat. Falso si no participa. */
	bool HasPerformed(int32 Seat) const
	{
		return (Seat == Challenger && ChallengerPerformance != NoPerformance) || (Seat == Challenged && ChallengedPerformance != NoPerformance);
	}
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

/** Por qué sale un asiento del Vacío (R-VAC-3, S-013, nombre propuesto): es el Value del evento LeftVoid. */
UENUM()
enum class EVoidExit : uint8
{
	/** Supera el minijuego de escape (R-VAC-2): su turno termina sin tirar. */
	Escaped,
	/** Usa Desvío (R-POD-6): sacrifica la runa y juega el turno. */
	Deflected,
	/** Pasa el último peldaño de su escalera (el cuarto turno; el segundo de Mictlantecuhtli): sale sin jugar. */
	TurnLimit,
	/** Es un Eco (R-ECO-5): sale en su primer turno sin jugar. */
	Echo,
	/** Liberación (R-VAC-5, R-EFE-17): todos los del Vacío salen a la vez, sin esperar su turno. */
	Released,
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
	/**
	 * Edición de depuración aplicada (S-011): Value es el tipo (EDebugEditKind), Seat el asiento afectado o INDEX_NONE.
	 * Va justo antes de los eventos de las reglas que la edición reutiliza. Nombre propuesto en S-011.
	 */
	DebugEdit,
	/**
	 * Un asiento reta a otro a duelo (R-POD-3, R-DUE-2); Seat el retador, OtherSeat el retado, Value 1 si el reto sale de un
	 * Nodo de Duelo (aunque el retador tenga además el poder), 0 si sale del poder Duelo y 2 si es un Duelo forzado del Warden
	 * (R-EFE-13, S-015). Nombre propuesto en S-012.
	 */
	DuelChallenged,
	/** Reto a quien no tiene runas: no hay duelo y la acción se pierde (R-DUE-4); Seat el retador, OtherSeat el retado. S-012. */
	DuelNoRunes,
	/**
	 * Se resuelve un duelo (R-DUE-1); Seat el ganador, OtherSeat el perdedor, Value 1 si gana el retado y 0 si gana el
	 * retador. Va antes del botín (trilero) o de la vuelta al hogar del perdedor. Nombre propuesto en S-012.
	 */
	DuelWon,
	/**
	 * El perdedor de un duelo no tiene runas y vuelve a su hogar (R-DUE-6): Seat el ganador, OtherSeat el perdedor, Value la
	 * casilla del hogar. Nombre propuesto en S-012.
	 */
	DuelLoserSentHome,
	/**
	 * Un asiento es desterrado al Vacío (R-VAC-1, R-EFE-14): su ficha sale del tablero. Seat el desterrado, Value 1 si lo
	 * destierra la Furia y 0 si no. Nombre propuesto en S-013.
	 */
	Banished,
	/** Un asiento supera el minijuego de escape del Vacío (R-VAC-2); Value es la dificultad (ESkillDifficulty). S-013. */
	VoidEscapePassed,
	/** Un asiento falla el minijuego de escape del Vacío (R-VAC-2) y sigue dentro; Value es la dificultad. S-013. */
	VoidEscapeFailed,
	/**
	 * Un asiento sale del Vacío y reaparece en su hogar (R-VAC-3); Value es el motivo (EVoidExit). Con Desvío va después del
	 * RuneReturned de la runa sacrificada. Nombre propuesto en S-013.
	 */
	LeftVoid,
	/** Vuelta al hogar (glosario `SendHome`, R-EFE-4): la ficha del asiento va a su hogar; Value es la casilla. S-013. */
	SentHome,
	/**
	 * Empieza un evento del Warden (R-COM-1, S-014): Seat quien lo dirige, Value su posición en FMatchState::WardenEvents y
	 * Detail su número de opciones (1, decreto; más, dilema presentado). Nombre propuesto en S-014.
	 */
	WardenEventStarted,
	/**
	 * Opción del dilema en curso (R-COM-2): Seat quien lo dirige, Value el índice de la opción, Detail 0 si la eligió y 1 si
	 * el Warden la sorteó en su contra al agotarse el reloj. Nombre propuesto en S-014.
	 */
	DilemmaChosen,
	/**
	 * Desatender al Warden (R-DIR-4, S-014): Seat no contestó a tiempo una elección de un evento y sube su insolencia; Value
	 * es el tipo de elección (EChoiceKind). Va antes de lo que el Warden hace en su lugar. Nombre propuesto en S-014.
	 */
	WardenIgnored,
	/**
	 * Aplicación de un efecto (R-EFE-0, S-014): Seat el objetivo (INDEX_NONE en los globales: Sello, Ley de Sombras, Noche,
	 * Liberación), OtherSeat quien lo señaló (rival señalado; INDEX_NONE si nadie), Value el efecto (EEffect) y Detail la
	 * dirección (EEffectTarget). La causa es el evento anterior del registro (WardenEventStarted, DilemmaChosen o
	 * WardenIgnored). Va antes de los eventos que deja el efecto. Nombre propuesto en S-014.
	 */
	EffectApplied,
	/**
	 * Burla del Warden (R-DIR-3, S-014): una aplicación sin objetivos válidos, o un regalo perdido por no elegir; no se aplica.
	 * Seat quien dirige el evento (o el objetivo, en un regalo perdido), Value el efecto y Detail la dirección.
	 * Nombre propuesto en S-014.
	 */
	WardenTaunt,
	/** Un asiento queda Marcado (R-EFE-8): Value la ronda en que caduca, OtherSeat quien lo señaló o INDEX_NONE. S-014. */
	Marked,
	/** Se cobra una marca (R-EFE-8): Seat quien la cobra y gana un turno extra, OtherSeat el Marcado. S-014. */
	MarkCollected,
	/** Una marca caduca sin cobrarse (R-EFE-8): Seat el que estaba Marcado. S-014. */
	MarkExpired,
	/** El Brazal del asiento queda revelado, o se prolonga (R-EFE-2): Value la ronda en que se oculta. S-014. */
	BracerRevealed,
	/** El Brazal del asiento vuelve a ocultarse al caducar (R-EFE-2). S-014. */
	BracerHidden,
	/** Empieza la Noche de los Ecos (R-EFE-18): Value la ronda en que termina por el tope; sin asiento. S-014. */
	EchoNightBegan,
	/** Termina la Noche de los Ecos: Detail el motivo (EEchoNightEnd: Ley de Sombras o tope de rondas); sin asiento. S-014. */
	EchoNightEnded,
	/** Una Prueba del Warden queda pendiente en el asiento (R-EFE-10, S-015): Seat el objetivo. Nombre propuesto en S-015. */
	WardenTrialPending,
	/** Un asiento supera su Prueba del Warden y su turno sigue (R-EFE-10): Value la dificultad (ESkillDifficulty). S-015. */
	WardenTrialPassed,
	/** Un asiento falla su Prueba del Warden y su turno termina sin tirar (R-EFE-10): Value la dificultad. S-015. */
	WardenTrialFailed,
	/** La Prueba pendiente de un asiento se anula porque entra en el Vacío (R-EFE-10): Seat el desterrado. S-015. */
	WardenTrialCancelled,
	/** Una runa pasa del perdedor al ganador de un Duelo forzado de todas (R-EFE-13): Seat el ganador, OtherSeat el perdedor, Value el tipo. S-015. */
	RunesSeized,
	/** Empieza un Duelo contra el Warden (S-015): Seat el jugador, Value el rendimiento del Warden en centésimas y Detail 1 si están en juego todas las runas. */
	WardenDuelStarted,
	/** El Warden gana su duelo (S-015): Seat el jugador, que pierde lo que estaba en juego (vuelve a su Fuente, R-RUN-6). */
	WardenDuelWon,
	/** El Warden pierde su duelo (S-015): Seat el jugador, que no pierde nada. */
	WardenDuelLost,
	/** El Warden roba a quien no señaló a tiempo en un Robo decretado (R-DIR-4, S-015): Seat la víctima, Value el tipo; la runa vuelve a su Fuente. */
	RuneTakenByWarden,
	/**
	 * Un golpe corta el turno en curso (R-TUR-11, S-016): Seat el asiento cortado, Value la fase en que estaba (ETurnPhase). Va
	 * antes del TurnEnded de ese turno y del evento que lo cortó. Nombre propuesto en S-016.
	 */
	TurnInterrupted,
	/**
	 * Evaluación del cerebro del Warden (R-WAR-10, S-016): Value el acto (EAct), Detail 1 si la garantía la obliga a intervenir
	 * (R-WAR-5) y 0 si no. Sin asiento. Nombre propuesto en S-016.
	 */
	WardenEvaluated,
	/**
	 * Decisión de una evaluación (R-WAR-10, S-016): Value la posición del evento en FMatchState::WardenEvents (INDEX_NONE sin
	 * evento: nada, comentario o aplazada) y Detail el tipo de decisión (EWardenDecisionKind). Va justo después de su
	 * WardenEvaluated. Sin asiento. Nombre propuesto en S-016.
	 */
	WardenDecided,
	/** Comentario del Warden (glosario `Remark`, R-WAR-4, S-016): sin texto ni efecto; el tipo y la frase son de S-017. Sin asiento. */
	WardenRemark,
};

/** Por qué termina la Noche de los Ecos (R-EFE-9, R-EFE-18, S-014, nombre propuesto): el Detail del evento EchoNightEnded. */
UENUM()
enum class EEchoNightEnd : uint8
{
	/** La Ley de Sombras. */
	ShadowLaw,
	/** El tope de rondas (FMatchParams::EchoNightMaxRounds): los Ecos vuelven a su hogar, igual que con la Ley. */
	RoundLimit,
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
	 * y HomeAssigned; los Elegidos en BondRaceEnded; los de los eventos de S-008 a S-012, en su tipo; INDEX_NONE en el
	 * resto. Nunca un asiento: el segundo asiento va en OtherSeat (S-010).
	 */
	UPROPERTY()
	int32 Value = INDEX_NONE;

	/**
	 * Segundo dato del evento (S-014): en EffectApplied y WardenTaunt, la dirección; en WardenEventStarted, el número de
	 * opciones; en DilemmaChosen, si la sorteó el Warden; en EchoNightEnded, el motivo. Nunca un asiento. INDEX_NONE en el
	 * resto. Nombre propuesto en S-014.
	 */
	UPROPERTY()
	int32 Detail = INDEX_NONE;
};

/** Estado de un asiento (Seat): quien lo ocupe (humano, bot o dummy) decide; el estado es el mismo. */
USTRUCT()
struct SOULBOUNDCORE_API FSeatState
{
	GENERATED_BODY()

	/**
	 * Casilla en la que está la ficha. INDEX_NONE mientras está en el Vacío (R-TAB-24, S-013): no ocupa casilla. Solo las
	 * reglas del Vacío (Match/VoidRules.h, Match/EffectRules.h) la ponen y la quitan de INDEX_NONE.
	 */
	UPROPERTY()
	int32 TileId = 0;

	/**
	 * Turnos propios empezados en el Vacío desde el último destierro (R-VAC-2, R-VAC-4, S-013): el destierro la pone a 0 y
	 * cada inicio de turno dentro la sube uno antes de decidir la salida, así que es el número del turno en curso. Un turno
	 * saltado por turno perdido no cuenta (decisión del autor del 08/10/2026). 0 fuera del Vacío. Nombre propuesto en S-013.
	 */
	UPROPERTY()
	int32 VoidTurns = 0;

	/** Si el asiento está en el Vacío (glosario "El Vacío"): fuera del tablero. */
	bool IsInVoid() const { return TileId == INDEX_NONE; }

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

	/**
	 * Ronda (FMatchState::Round) en que caduca su marca (R-EFE-8, S-014): Marcado mientras sea mayor que la ronda en curso;
	 * 0 si nunca lo estuvo. Solo la escriben las reglas de Match/EffectRules.h. Nombre propuesto en S-014.
	 */
	UPROPERTY()
	int32 MarkedUntilRound = 0;

	/**
	 * Ronda en que vuelve a ocultarse su Brazal (R-EFE-2, S-014): revelado mientras sea mayor que la ronda en curso; 0 si
	 * nunca lo estuvo. Solo la escriben las reglas de Match/EffectRules.h. Nombre propuesto en S-014.
	 */
	UPROPERTY()
	int32 RevealedUntilRound = 0;

	/**
	 * Prueba del Warden pendiente (R-EFE-10, S-015): el asiento la juega al empezar su siguiente turno jugado, antes de tirar.
	 * Como mucho una; la anula entrar en el Vacío. Solo la escriben las reglas de Match/EffectRules.h y la máquina de turnos.
	 * Nombre propuesto en S-015.
	 */
	UPROPERTY()
	bool bWardenTrialPending = false;
};

/**
 * Evento del Warden en curso (S-014, nombre propuesto): cuál, quién lo dirige, qué opción se aplica y por dónde va. Lo
 * arranca ApplyEvent y lo lleva la máquina de turnos entre turnos (AdvanceTurn en BetweenTurns); cada elección que haga
 * falta es la elección pendiente (FMatchState::PendingChoice). Solo lo escriben las reglas de Match/WardenEventRules.h.
 */
USTRUCT()
struct SOULBOUNDCORE_API FCurrentWardenEvent
{
	GENERATED_BODY()

	/** Posición del evento en FMatchState::WardenEvents; INDEX_NONE si no hay evento en curso. */
	UPROPERTY()
	int32 EventIndex = INDEX_NONE;

	/** Quien lo dirige: el activo o, entre turnos, el último que jugó (R-DIR-1). */
	UPROPERTY()
	int32 Director = INDEX_NONE;

	/** Opción que se aplica; INDEX_NONE mientras el dilema espera. */
	UPROPERTY()
	int32 Option = INDEX_NONE;

	/** Aplicación de la opción que toca (R-COM-3: en orden). */
	UPROPERTY()
	int32 NextApplication = 0;

	/**
	 * Objetivos de la aplicación en curso que aún tienen que elegir (runa regalada, runa a perder a elección), en orden de
	 * asiento; el primero tiene la elección abierta.
	 */
	UPROPERTY()
	TArray<int32> PendingTargets;

	/** Si el dilema se resuelve en contra de quien lo dirige (R-COM-2): regalos descartados, daños sobre él, sin elecciones. */
	UPROPERTY()
	bool bAgainst = false;

	/**
	 * Semilla de los sorteos del evento (direcciones al azar, Sello, opción y runa al azar): se saca una vez del sorteo del
	 * Warden en ApplyEvent; cada sorteo la combina con Draws. Así el evento nunca toca el sorteo de la partida.
	 */
	UPROPERTY()
	int32 Seed = 0;

	/** Sorteos hechos con la semilla. */
	UPROPERTY()
	int32 Draws = 0;

	/**
	 * Si el evento espera a un duelo o un trilero suyo (S-015): la máquina está en la fase Duel o ShellGame entre turnos y,
	 * al acabar, vuelve a BetweenTurns y el evento sigue por su siguiente aplicación. Nada de esto termina un turno.
	 */
	UPROPERTY()
	bool bWaitsForAction = false;

	/**
	 * Primer rival señalado de un Duelo forzado a "dos rivales señalados" (S-015), que será el retador; INDEX_NONE mientras
	 * no ha señalado a ninguno. Las dos elecciones son ChosenRival seguidas; esto dice cuál es cuál.
	 */
	UPROPERTY()
	int32 FirstRival = INDEX_NONE;

	/**
	 * Si el evento llega con el corte de un turno (S-016, R-TUR-11): un golpe, o una escena cortante, sobre un turno en curso.
	 * Solo así vale el Alto (R-TAB-28), y solo sobre quien dirige, el cortado. Nombre propuesto en S-016.
	 */
	UPROPERTY()
	bool bInterruptedTurn = false;

	bool IsActive() const { return EventIndex != INDEX_NONE; }
};

/**
 * Estado del Warden en la partida (R-WAR-10, S-016, nombre propuesto): lo que el cerebro necesita recordar entre
 * evaluaciones para que una partida se reproduzca con su semilla. Solo lo escriben las reglas de Warden/WardenRules.h y, al
 * empezar cada turno, la máquina de turnos (bGapUsed).
 */
USTRUCT()
struct SOULBOUNDCORE_API FWardenState
{
	GENERATED_BODY()

	/**
	 * Semilla del sorteo propio del Warden: la de la partida más 2, fijada por StartMatch (como el anfitrión y la simulación
	 * hasta S-015). Cada sorteo la combina con Draws (WardenDraw); nada toca el sorteo de los dados.
	 */
	UPROPERTY()
	int32 Seed = 0;

	/** Sorteos hechos con Seed. */
	UPROPERTY()
	int32 Draws = 0;

	/** Usos que le quedan a cada evento de FMatchState::WardenEvents, en su orden (glosario `MaxUses`); -1, sin límite. */
	UPROPERTY()
	TArray<int32> UsesLeft;

	/** Instante de la última intervención con efecto (golpe o escena aplicados), o 0 si aún no hubo: de aquí cuenta la garantía. */
	UPROPERTY()
	double LastEffectSeconds = 0.0;

	/** Instante de la siguiente evaluación, el que devolvió el cerebro en la anterior (R-WAR-3). */
	UPROPERTY()
	double NextEvaluationSeconds = 0.0;

	/** Si la última evaluación se aplazó (R-WAR-7): se vuelve a evaluar en cuanto termina lo que la aplazó. */
	UPROPERTY()
	bool bDeferred = false;

	/** Escena decidida con un turno en curso que espera al hueco entre turnos (R-WAR-4); INDEX_NONE si no hay. */
	UPROPERTY()
	int32 PendingScene = INDEX_NONE;

	/** Si el hueco entre turnos en curso ya tuvo su intervención con efecto (R-WAR-4: como mucho una). Lo limpia el inicio de turno. */
	UPROPERTY()
	bool bGapUsed = false;
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

	/** Duelo en curso (S-012); sin retador fuera de la fase Duel. */
	UPROPERTY()
	FDuelState Duel;

	/**
	 * Eventos del Warden de la partida (S-014): los de Content/Data/Events/eventos.json, que recibe StartMatch. Vacío en una
	 * partida sin eventos (la simulación). Nombre del autor (12/10/2026): WardenEvents, para no confundirlo con EventLog.
	 */
	UPROPERTY()
	FWardenEventTable WardenEvents;

	/** Evento del Warden en curso (S-014); sin evento, EventIndex INDEX_NONE. */
	UPROPERTY()
	FCurrentWardenEvent CurrentEvent;

	/**
	 * Parámetros del Warden (S-016): los de Content/Data/Warden/warden.json, que recibe StartMatch. Sin actos, la partida no
	 * tiene Warden (FWardenParams::HasWarden).
	 */
	UPROPERTY()
	FWardenParams WardenParams;

	/** Estado del Warden (S-016): sorteo propio, usos, última intervención, siguiente evaluación, aplazamiento y escena en espera. */
	UPROPERTY()
	FWardenState Warden;

	/**
	 * Ronda (decisión del autor del 10/10/2026: cada vuelta completa del orden de turno): vueltas completas desde el inicio.
	 * Sube cuando el orden pasa por su primer asiento, juegue o se salte su turno, y nunca con un turno extra. Marcas, Brazales
	 * revelados y la Noche caducan por ella. Solo la escriben las reglas de Match/TurnRules.h. Nombre propuesto en S-014.
	 */
	UPROPERTY()
	int32 Round = 0;

	/**
	 * Asiento del último turno empezado (S-014): "el último que jugó", el activo de un evento que llega entre turnos (R-DIR-1).
	 * INDEX_NONE antes del primer turno. Nombre propuesto en S-014.
	 */
	UPROPERTY()
	int32 LastTurnSeat = INDEX_NONE;

	/**
	 * Ronda en que termina por el tope la Noche de los Ecos (R-EFE-18, S-014): activa mientras sea mayor que Round; 0 sin
	 * Noche. Solo la escriben las reglas de Match/EffectRules.h. Nombre propuesto en S-014.
	 */
	UPROPERTY()
	int32 EchoNightUntilRound = 0;

	/** Si la Noche de los Ecos está activa (R-EFE-18). */
	bool IsEchoNight() const { return EchoNightUntilRound > Round; }

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

	/**
	 * Tirada fijada por una edición de depuración (S-011): la próxima tirada, sea de quien sea, vale esto en lugar de la
	 * suma de los dados (el sorteo se consume igual). 0 si no hay. La limpia la tirada que la usa. Solo la escriben
	 * Match/DebugEdits.h y la máquina de turnos. Nombre propuesto en S-011.
	 */
	UPROPERTY()
	int32 ForcedRoll = 0;

	/**
	 * Asiento fijado para el siguiente turno por una edición de depuración (S-011): el próximo turno que empiece es suyo y
	 * el orden sigue desde él. INDEX_NONE si no hay. Lo limpia el inicio de turno que lo usa. Nombre propuesto en S-011.
	 */
	UPROPERTY()
	int32 ForcedNextSeat = INDEX_NONE;

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
