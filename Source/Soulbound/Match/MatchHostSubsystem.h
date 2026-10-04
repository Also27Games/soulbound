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
	 * Respuesta del asiento humano a su elección pendiente (S-009): dados, repetir, salto o expulsión. Se entrega tal cual
	 * a SubmitChoiceIntent, como el destino: el motor decide si vale. Kind: el tipo de elección que contesta. Option: una
	 * de sus opciones, o INDEX_NONE para "no". Devuelve lo que dijo el motor.
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

	/** Semilla de la partida. Valor inicial -1: aleatoria. Un valor de 0 en adelante la fija para repetir una partida. */
	UPROPERTY(Config)
	int32 FixedSeed = -1;

	/** Asiento del humano. Valor inicial 0 (story S-005). */
	UPROPERTY(Config)
	int32 HumanSeat = 0;

	/**
	 * Segundos que tarda un dummy en confirmar su destino desde que tiene la tirada, y en contestar cada elección (S-009).
	 * Valor inicial 5 (como en la simulación, S-003); menor que el reloj de elección, así un dummy nunca lo agota.
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
		/** Elección que contesta (S-009), o None si es el destino o el minijuego. */
		EChoiceKind ChoiceKind = EChoiceKind::None;
		/** Opción que entregará, si contesta una elección (S-009). */
		int32 Option = INDEX_NONE;
	};

	FMatchState State;
	FRandomStream MatchRng;
	FRandomStream DeciderRng;
	TArray<TSharedPtr<IDecider>> Deciders;
	/** Responde las pruebas de habilidad del asiento humano hasta que exista el minijuego real (S-008). */
	FDummyDecider HumanTrialDecider;
	FPendingDecision Pending;
	FTimerHandle AdvanceTimer;
	FString LastWardenLine;
	FStepClock StepClock;
	int32 UsedSeed = 0;
	bool bHasMatch = false;

	UPROPERTY()
	TObjectPtr<ABoardViewActor> View;
};
