#pragma once

#include "CoreMinimal.h"
#include "Decider/Decider.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "StepClock.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "MatchHostSubsystem.generated.h"

class ABoardViewActor;
struct FWardenDecision;

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

	/** Última línea que dijo el Warden, o vacía si aún no ha intervenido. */
	const FString& GetLastWardenLine() const { return LastWardenLine; }

	/**
	 * Intención de destino del asiento humano (glosario "Intención"). Se entrega tal cual a SubmitDestinationIntent:
	 * el motor decide si vale; una intención inválida se descarta sin castigo. Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanIntent(int32 TileId);

	/**
	 * Respuesta del asiento humano a su elección pendiente (S-009): dados, repetir, salto, acción contra un jugador, runa
	 * de Koschei o esfera del trilero (S-010), también cuando no tiene el turno. Se entrega tal cual a SubmitChoiceIntent,
	 * como el destino: el motor decide si vale. Kind: el tipo de elección que contesta. Option: una respuesta válida
	 * (FPendingChoice::AnswerAt), o INDEX_NONE para "no". Devuelve lo que dijo el motor.
	 */
	EIntentResult SubmitHumanChoice(EChoiceKind Kind, int32 Option);

	/** El anfitrión del Warden pide aplicar una decisión sobre el estado; aquí se escribe y se avisa. */
	void ApplyWardenDecision(const FWardenDecision& Decision);

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

	/** Si la partida está detenida esperando el paso: el reloj no corre y el Warden no evalúa. */
	UFUNCTION(BlueprintPure, Category = "Partida")
	bool IsWaitingForStep() const { return StepClock.bHeld; }

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

	/**
	 * Lanza el evento del Warden EventId como entrega directa, entre turnos (S-014, Soulbound::Debug::LaunchEvent), con el
	 * sorteo del Warden (WardenRandom: el anfitrión del Warden pasa el suyo). Si la partida estaba detenida en el paso a paso
	 * y el evento espera una elección, el reloj se reanuda sin conceder turno: las elecciones del evento tienen plazo, y la
	 * partida vuelve a detenerse al acabar el evento, antes del siguiente turno.
	 */
	FDebugEditOutcome DebugLaunchEvent(const FString& EventId, FRandomStream& WardenRandom);
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

	/** Semilla de la partida. Valor inicial -1: aleatoria. Un valor de 0 en adelante la fija para repetir una partida. */
	UPROPERTY(Config)
	int32 FixedSeed = -1;

	/** Asiento del humano. Valor inicial 0 (story S-005). */
	UPROPERTY(Config)
	int32 HumanSeat = 0;

	/**
	 * Segundos que tarda un dummy en confirmar su destino desde que tiene la tirada, y en contestar cada elección (S-009).
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
#if !UE_BUILD_SHIPPING
	/** Resultado fijado del próximo minijuego del humano (S-011); sin valor, lo resuelve el dummy. Se gasta al usarlo. */
	TOptional<bool> ForcedTrialResult;

	/** Resultado fijado del próximo duelo del humano (S-012): true gana, false pierde. Se gasta al usarlo. */
	TOptional<bool> ForcedDuelResult;

	/** Cierra una edición de depuración: la deja en el log, avisa a la vista si se aplicó y devuelve su resultado. */
	FDebugEditOutcome FinishDebugEdit(bool bApplied, const FString& Message);
#endif
	FTimerHandle AdvanceTimer;
	FString LastWardenLine;
	FStepClock StepClock;
	int32 UsedSeed = 0;
	bool bHasMatch = false;

	UPROPERTY()
	TObjectPtr<ABoardViewActor> View;
};
