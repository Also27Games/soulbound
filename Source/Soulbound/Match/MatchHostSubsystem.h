#pragma once

#include "CoreMinimal.h"
#include "Decider/Decider.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "StepClock.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "Warden/WardenBrain.h"
#include "MatchHostSubsystem.generated.h"

class ABoardViewActor;

#if !UE_BUILD_SHIPPING
/**
 * Qué pasó con una edición de depuración pedida al anfitrión (S-011, nombre propuesto): si se aplicó y la frase que
 * la describe (qué hizo, con todo el detalle, o por qué se rechazó). Solo existe fuera de la build de entrega.
 */
struct FDebugEditOutcome
{
	bool bApplied = false;
	FString Message;
};
#endif

/** Aviso de que el estado de la partida ha cambiado. Lo lanza solo el anfitrión de partida. */
DECLARE_MULTICAST_DELEGATE(FOnMatchStateChanged);

/**
 * Anfitrión de partida (glosario "MatchHost", nombre propuesto en S-005): UWorldSubsystem, solo en el servidor y
 * solo en mapas de partida (en el spike, los que usan ASpikeOnlineGameMode; en los menús de UGT no existe). Es el
 * dueño de la partida: carga el tablero y los parámetros de sus archivos, llama a StartMatch, avanza la máquina de
 * turnos con el reloj real, consulta a los decisores dummy y recibe la intención del humano. Es el único que escribe
 * el estado y el único que avisa de que cambió. No contiene reglas: todas están en SoulboundCore.
 * También genera la presentación (ABoardViewActor); el mapa no cambia.
 * Es quien lleva el reloj de partida y por eso es quien tiene el modo paso a paso (S-007, glosario "StepMode"): con él
 * activo, la partida se detiene al entrar en BetweenTurns y el reloj se congela hasta que se concede un paso. La regla
 * (cuándo detener, qué marca el reloj) está en FStepClock.
 * Desde S-022 (R-PRO-1) también lleva el prólogo: al empezar, el reloj de partida queda detenido (FStepClock::Hold) mientras el Warden dice la
 * apertura, los segundos de OpeningSeconds en tiempo real; al vencer se reanuda (FStepClock::Resume) y empieza el primer turno.
 */
UCLASS(Config = Game, BlueprintType)
class SOULBOUND_API UMatchHostSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Si este mundo es una partida con autoridad: no es un cliente y usa el GameMode del spike. */
	static bool IsMatchWorld(const UWorld& World);

	/** Si la partida se inició. Falso si faltó un archivo de datos (el motivo está en el log). */
	bool HasMatch() const { return bHasMatch; }

	/** Estado de la partida, en solo lectura. Solo tiene sentido si HasMatch(). */
	const FMatchState& GetState() const { return State; }

	/** Asiento que juega el humano con el ratón. */
	int32 GetHumanSeat() const { return HumanSeat; }

	/** Semilla con la que se inició la partida (la fijada, o la aleatoria elegida). Repetir una partida es fijarla. */
	int32 GetSeed() const { return UsedSeed; }

	/** Segundos de partida ahora: los que marca el reloj (congelado en el paso a paso, S-007). El anfitrión del Warden los mira (S-016). */
	double GetMatchSeconds() const { return MatchSeconds(); }

	/**
	 * Intención de destino del asiento humano (glosario "Intención"). Se entrega tal cual a SubmitDestinationIntent:
	 * el motor decide si vale; una intención inválida se descarta sin castigo. Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanIntent(int32 TileId);

	/**
	 * Intención de avance del asiento Seat (S-020), sea humano o dummy (decisión del autor del 21/10/2026): la vista la entrega al
	 * ver entrar su ficha en cada casilla del camino. Se entrega tal cual a SubmitMoveProgress, que solo acepta la del asiento
	 * activo; una que no vale (la salvaguarda ya entró en esa casilla, o el turno se cortó) se descarta sin castigo. Devuelve lo
	 * que dijo el motor.
	 */
	// TODO(R-RED-1): con red, el avance lo entrega la presentación del servidor; si un cliente llega a entregarlo, el servidor tendrá
	// que comprobar que el asiento es suyo, como con el resto de intenciones.
	EIntentResult SubmitMoveProgress(int32 Seat, int32 TileId);

	/**
	 * Respuesta del asiento humano a su elección pendiente (S-009): dados, repetir, salto, acción contra un jugador, runa
	 * de Koschei o esfera del trilero (S-010), también cuando no tiene el turno. Se entrega tal cual a SubmitChoiceIntent,
	 * como el destino: el motor decide si vale. Kind: el tipo de elección que contesta. Option: una respuesta válida
	 * (FPendingChoice::AnswerAt), o INDEX_NONE para "no". Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanChoice(EChoiceKind Kind, int32 Option);

	/**
	 * Consulta del Oráculo del asiento humano (S-017, R-ORA-3): la vista la entrega con un clic en el orbe. Se entrega tal cual a
	 * SubmitOracleConsult: el motor decide si vale (consulta abierta, suya y en plazo). Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanOracleConsult();

	/** Rechazo de Tifón del asiento humano (S-017, R-ORA-5), con el clic derecho en el orbe; como SubmitHumanOracleConsult. */
	EIntentResult SubmitHumanOracleRefuse();

	/**
	 * Voto del asiento humano por Voted (S-017, R-VOT-1): la vista lo entrega con un clic en la ficha del votado. Se entrega tal cual a
	 * SubmitVote: el motor decide si vale. Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanVote(int32 Voted);

	/**
	 * El anfitrión del Warden pide una evaluación (S-016): el motor la hace y la aplica sobre el estado
	 * (Soulbound::Rules::EvaluateWarden), aquí se escribe y se avisa, y la partida sigue (un golpe deja el turno entre turnos
	 * con el evento en marcha). Devuelve la decisión, para el log del anfitrión del Warden.
	 */
	FWardenDecision EvaluateWarden();

	/**
	 * Modo paso a paso (S-007). Activo, la partida se detiene al acabar cada turno y el reloj se congela hasta que se
	 * concede un paso (StepOnce). Cambiable en caliente: al desactivarlo la partida sigue sola.
	 */
	UFUNCTION(BlueprintCallable, Category = "Partida")
	void SetStepMode(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsStepMode() const { return bStepMode; }

	/** Suelta exactamente un turno. Solo hace algo si la partida está detenida esperando; devuelve si lo hizo. */
	UFUNCTION(BlueprintCallable, Category = "Partida")
	bool StepOnce();

	/**
	 * Si la partida está detenida esperando el paso, o en el prólogo (S-022): el reloj no corre y el Warden no evalúa. Para distinguir el
	 * prólogo, IsInPrologue.
	 */
	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsWaitingForStep() const { return StepClock.bHeld; }

	/**
	 * Si la partida está en el prólogo (S-022, R-PRO-1; nombre aprobado): el Warden dice la apertura y el reloj de partida sigue en cero hasta
	 * que termina. Ni Espacio ni SbStepMode 0 lo acortan; SbEvent, SbStrike y SbClock lo acaban antes de aplicarse.
	 */
	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsInPrologue() const { return bInPrologue; }

#if !UE_BUILD_SHIPPING
	// Herramientas de depuración (S-011, glosario "DebugEdit"). Son las únicas vías por las que los comandos Sb… cambian
	// el estado: cada una pide la edición al motor (Soulbound::Debug), avisa a la vista si se aplicó y deja el detalle en
	// el log y en el resultado. No existen en la build de entrega (UE_BUILD_SHIPPING); los comandos de consola del
	// controlador siguen declarados, pero su cuerpo no hace nada.

	/** Da Count runas de tipo RuneType al asiento Seat, sacadas de la reserva de su Fuente. */
	FDebugEditOutcome DebugGiveRune(int32 Seat, int32 RuneType, int32 Count);

	/** Quita Count runas de tipo RuneType al asiento Seat y las devuelve a la reserva de su Fuente. */
	FDebugEditOutcome DebugTakeRune(int32 Seat, int32 RuneType, int32 Count);

	/** Coloca la ficha del asiento Seat en la casilla TileId, sin resolverla. */
	FDebugEditOutcome DebugMove(int32 Seat, int32 TileId);

	/** Vincula al asiento Seat con el Elegido del Dominio DomainId, como si hubiera reclamado su Nicho. */
	FDebugEditOutcome DebugBond(int32 Seat, int32 DomainId);

	/** Sella la casilla TileId con las reglas de Sello. */
	FDebugEditOutcome DebugSeal(int32 TileId);

	/** Libera el Sello de la casilla TileId. */
	FDebugEditOutcome DebugUnseal(int32 TileId);

	/** Termina la carrera por el vínculo ahora. */
	FDebugEditOutcome DebugEndRace();

	/** Fija la próxima tirada de la partida. */
	FDebugEditOutcome DebugFixRoll(int32 Value);

	/** Fija el asiento del siguiente turno que empiece. */
	FDebugEditOutcome DebugFixNextSeat(int32 Seat);

	/**
	 * Fija el resultado del próximo minijuego de habilidad del humano (hoy lo resuelve un dummy al azar, S-008). Vive aquí
	 * y no en el motor porque el anfitrión es quien responde por el humano. El motor solo deja el evento de la edición
	 * (Soulbound::Debug::FixTrial).
	 */
	FDebugEditOutcome DebugFixTrial(bool bPass);

	/**
	 * Fija el resultado del próximo duelo en que participe el humano (S-012): al resolverse, el humano rinde 1 y su rival 0
	 * para ganar, o al revés para perder, sea quien sea Shuten-dōji (el margen es menor que 1). Como DebugFixTrial, vive
	 * aquí y el motor solo deja el evento de la edición (Soulbound::Debug::FixDuel).
	 */
	FDebugEditOutcome DebugFixDuel(bool bWin);

	/** Destierra al asiento Seat al Vacío, sin la Furia (S-013, Soulbound::Debug::BanishSeat). */
	FDebugEditOutcome DebugBanish(int32 Seat);

	/** Liberación: todos los del Vacío salen a su hogar (S-013). */
	FDebugEditOutcome DebugRelease();

	/** Vuelta al hogar del asiento Seat (S-013). */
	FDebugEditOutcome DebugSendHome(int32 Seat);

	/** Fija la insolencia del asiento Seat en Level, de 0 a 4, sin castigo (S-016, Soulbound::Debug::SetInsolence). */
	FDebugEditOutcome DebugSetInsolence(int32 Seat, int32 Level);

	/**
	 * Lanza el evento del Warden EventId como entrega directa, entre turnos (S-014, Soulbound::Debug::LaunchEvent), con el
	 * sorteo del Warden, que desde S-016 vive en el estado (WardenDraw). Si la partida estaba detenida en el paso a paso y el
	 * evento espera una elección, el reloj se reanuda sin conceder turno: las elecciones del evento tienen plazo, y la partida
	 * vuelve a detenerse al acabar el evento, antes del siguiente turno. Con el prólogo en marcha, primero lo acaba (S-022), y lo deja en
	 * el log, aunque luego el evento se rechace.
	 */
	FDebugEditOutcome DebugLaunchEvent(const FString& EventId);

	/**
	 * Lanza el evento del Warden EventId como golpe ahora (S-020, SbStrike, Soulbound::Debug::StrikeEvent): corta el turno en curso,
	 * con el sorteo de la partida para el consumo fijo, y el evento entra en el hueco con el sorteo del Warden; sin turno en curso,
	 * como DebugLaunchEvent. En un duelo, un trilero o un minijuego se rechaza. Como DebugLaunchEvent, reanuda el reloj del paso a
	 * paso si el evento espera una elección, y acaba el prólogo antes de golpear (S-022).
	 */
	FDebugEditOutcome DebugStrikeEvent(const FString& EventId);

	/**
	 * Adelanta el reloj de partida a Seconds (S-018, SbClock, Soulbound::Debug::SetClock): el estado y el reloj del anfitrión
	 * (FStepClock::Skip). Lo que venza por ello (fin de carrera, evaluaciones del Warden, fin de la partida a los 40 minutos) llega en
	 * el siguiente paso, como si el tiempo hubiera pasado. Hacia atrás se rechaza. Con el prólogo en marcha, primero lo acaba (S-022).
	 */
	FDebugEditOutcome DebugSetClock(double Seconds);

	/**
	 * Dice la frase Key de la tabla del Warden ahora (S-021, SbSay, Soulbound::Debug::SayLine), para probar los textos sin esperar al
	 * suceso. Una clave que no está en la tabla se rechaza.
	 */
	FDebugEditOutcome DebugSay(const FString& Key);
#endif

	/** Se lanza tras cada cambio del estado. */
	FOnMatchStateChanged OnMatchStateChanged;

	/** Ruta del tablero, relativa a Content/. Valor inicial: el tablero de 15 casillas por Dominio (S-005). */
	UPROPERTY(Config)
	FString BoardRelativePath = TEXT("Data/Boards/tablero_15.json");

	/** Ruta de los parámetros de partida, relativa a Content/. */
	UPROPERTY(Config)
	FString MatchParamsRelativePath = TEXT("Data/Match/partida.json");

	/** Ruta de los Elegidos, relativa a Content/ (S-008). */
	UPROPERTY(Config)
	FString ElectsRelativePath = TEXT("Data/Elects/elegidos.json");

	/** Ruta de los eventos del Warden, relativa a Content/ (S-014). */
	UPROPERTY(Config)
	FString WardenEventsRelativePath = TEXT("Data/Events/eventos.json");

	/** Ruta de los parámetros del Warden, relativa a Content/ (S-016). */
	UPROPERTY(Config)
	FString WardenParamsRelativePath = TEXT("Data/Warden/warden.json");

	/** Ruta de las frases del Warden, relativa a Content/ (S-021). */
	UPROPERTY(Config)
	FString WardenLinesRelativePath = TEXT("Data/Warden/frases.json");

	/** Semilla de la partida. Valor inicial -1: aleatoria. Un valor de 0 en adelante la fija para repetir una partida. */
	UPROPERTY(Config)
	int32 FixedSeed = -1;

	/** Asiento del humano. Valor inicial 0 (story S-005). */
	UPROPERTY(Config)
	int32 HumanSeat = 0;

	/**
	 * Segundos que tarda un dummy en confirmar su destino desde que tiene la tirada, y en contestar cada elección (S-009). Su
	 * camino lo recorre su ficha, que entrega el avance de cada casilla como la del humano (S-020, decisión del autor del 21/10/2026).
	 * Valor inicial 5 (como en la simulación, S-003); menor que el reloj de elección, así un dummy nunca lo agota. Si el
	 * plazo de una elección es más corto (la esfera del trilero, S-010), contesta a la mitad de lo que queda.
	 */
	UPROPERTY(Config)
	float DummyResponseSeconds = 5.f;

	/** Modo paso a paso al iniciar la partida. Valor inicial false (S-007). Se cambia en juego con SetStepMode o SbStepMode. */
	UPROPERTY(Config, BlueprintGetter = IsStepMode, BlueprintSetter = SetStepMode, Category = "Partida")
	bool bStepMode = false;

	/** Cada cuántos segundos reales se avanza la máquina de turnos. Valor inicial 0,1. */
	UPROPERTY(Config)
	float AdvanceIntervalSeconds = 0.1f;

private:
	/** Segundos de partida: los que marca el reloj (congelado en el paso a paso, S-007). */
	double MatchSeconds() const;

	/** Avanza la máquina de turnos hasta que haya que esperar a alguien, consultando a los dummies. */
	void Advance();

	/** Carga los archivos de datos e inicia la partida. Devuelve false, con el motivo en el log, si no puede. */
	bool StartFromData(UWorld& World);

	/** Acaba el prólogo, si dura: el reloj de partida se reanuda desde cero (FStepClock::Resume). Why dice por qué, para el log. */
	void EndPrologue(const TCHAR* Why);

	/**
	 * Respuesta ya decidida de un dummy, a la espera de su hora. Va atada al turno que la originó (asiento y
	 * TurnCount) y a lo que contesta (ChoiceKind, None para el destino y el minijuego): si cambia por cualquier otra vía
	 * (reloj agotado, turno extra del mismo asiento, otra elección), se descarta.
	 */
	struct FPendingDecision
	{
		bool bPending = false;
		int32 Seat = INDEX_NONE;
		int32 TurnCount = INDEX_NONE;
		double ReadyAtSeconds = 0.0;
		int32 TileId = INDEX_NONE;
		/** Resultado del minijuego que confirmará, si lo que espera es la prueba de una Fuente (S-008). */
		bool bPassedTrial = false;
		/** Elección que contesta (S-009), o None si es el destino, el minijuego o el duelo. */
		EChoiceKind ChoiceKind = EChoiceKind::None;
		/** Opción que entregará, si contesta una elección (S-009). */
		int32 Option = INDEX_NONE;
		/** Si lo que espera son los rendimientos de un duelo (S-012): entonces los dos se entregan juntos, a la misma hora. */
		bool bDuel = false;
		/** Rendimientos del retador y del retado que entregará, si espera un duelo (S-012). */
		double ChallengerPerformance = 0.0;
		double ChallengedPerformance = 0.0;
		/** Si lo que espera es una consulta del Oráculo (S-017), y si la rechazará (Tifón; los dummies nunca). */
		bool bOracle = false;
		bool bRefuseOracle = false;
	};

	FMatchState State;
	FRandomStream MatchRng;
	FRandomStream DeciderRng;
	TArray<TSharedPtr<IDecider>> Deciders;
	/**
	 * Responde por el asiento humano lo que aún no tiene interfaz: las pruebas de habilidad (S-008) y el rendimiento en un
	 * duelo (S-012), al azar, hasta que exista el minijuego real.
	 */
	FDummyDecider HumanTrialDecider;
	FPendingDecision Pending;
	/**
	 * Votos de los dummies en la votación abierta (S-017): a quién votará cada asiento y cuándo (su retardo, o la mitad de lo que
	 * queda). Indexados por asiento; se vacían al cerrarse la votación.
	 */
	TArray<TOptional<TPair<double, int32>>> PendingVotes;
	/** Plazo de la votación a la que pertenecen PendingVotes: si cambia, es otra votación y se vacían. */
	double PendingVotesDeadline = -1.0;
#if !UE_BUILD_SHIPPING
	/** Resultado fijado del próximo minijuego del humano (S-011); sin valor, lo resuelve el dummy. Se gasta al usarlo. */
	TOptional<bool> ForcedTrialResult;

	/** Resultado fijado del próximo duelo del humano (S-012): true gana, false pierde. Se gasta al usarlo. */
	TOptional<bool> ForcedDuelResult;

	/** Cierra una edición de depuración: la deja en el log, avisa a la vista si se aplicó y devuelve su resultado. */
	FDebugEditOutcome FinishDebugEdit(bool bApplied, const FString& Message);
#endif
	FTimerHandle AdvanceTimer;
	FStepClock StepClock;
	/** Si dura el prólogo (S-022): el reloj está detenido por él, no por el paso a paso. */
	bool bInPrologue = false;
	/** Tiempo del mundo en que acaba el prólogo. */
	double PrologueEndsWorldSeconds = 0.0;
	int32 UsedSeed = 0;
	bool bHasMatch = false;

	UPROPERTY()
	TObjectPtr<ABoardViewActor> View;
};
