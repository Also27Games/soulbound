#pragma once

#include "CoreMinimal.h"
#include "Decider/Decider.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
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
 */
UCLASS(Config = Game)
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

	/** El anfitrión del Warden pide aplicar una decisión sobre el estado; aquí se escribe y se avisa. */
	void ApplyWardenDecision(const FWardenDecision& Decision);

	/** Se lanza tras cada cambio del estado. */
	FOnMatchStateChanged OnMatchStateChanged;

	/** Ruta del tablero, relativa a Content/. Valor inicial: el tablero de 15 casillas por Dominio (S-005). */
	UPROPERTY(Config)
	FString BoardRelativePath = TEXT("Data/Boards/tablero_15.json");

	/** Ruta de los parámetros de partida, relativa a Content/. */
	UPROPERTY(Config)
	FString MatchParamsRelativePath = TEXT("Data/Match/partida.json");

	/** Semilla de la partida. Valor inicial -1: aleatoria. Un valor de 0 en adelante la fija para repetir una partida. */
	UPROPERTY(Config)
	int32 FixedSeed = -1;

	/** Asiento del humano. Valor inicial 0 (story S-005). */
	UPROPERTY(Config)
	int32 HumanSeat = 0;

	/** Segundos que tarda un dummy en confirmar su destino desde que tiene la tirada. Valor inicial 5 (como en la simulación, S-003). */
	UPROPERTY(Config)
	float DummyResponseSeconds = 5.f;

	/** Cada cuántos segundos reales se avanza la máquina de turnos. Valor inicial 0,1. */
	UPROPERTY(Config)
	float AdvanceIntervalSeconds = 0.1f;

private:
	/** Segundos de partida: tiempo del mundo desde que empezó la partida (sin pausa en S-005). */
	double MatchSeconds() const;

	/** Avanza la máquina de turnos hasta que haya que esperar a alguien, consultando a los dummies. */
	void Advance();

	/** Carga los archivos de datos e inicia la partida. Devuelve false, con el motivo en el log, si no puede. */
	bool StartFromData(UWorld& World);

	/**
	 * Respuesta ya decidida de un dummy, a la espera de su hora. Va atada al turno que la originó (asiento y
	 * TurnCount): si el turno cambia por cualquier otra vía (reloj agotado, turno extra del mismo asiento), se descarta.
	 */
	struct FPendingDecision
	{
		bool bPending = false;
		int32 Seat = INDEX_NONE;
		int32 TurnCount = INDEX_NONE;
		double ReadyAtSeconds = 0.0;
		int32 TileId = INDEX_NONE;
	};

	FMatchState State;
	FRandomStream MatchRng;
	FRandomStream DeciderRng;
	TArray<TSharedPtr<IDecider>> Deciders;
	FPendingDecision Pending;
	FTimerHandle AdvanceTimer;
	FString LastWardenLine;
	double OriginWorldSeconds = 0.0;
	int32 UsedSeed = 0;
	bool bHasMatch = false;

	UPROPERTY()
	TObjectPtr<ABoardViewActor> View;
};
