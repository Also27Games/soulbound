#pragma once

#include "CoreMinimal.h"
#include "Board/BoardState.h"
#include "MatchState.generated.h"

/** Estado de un asiento (Seat): quien lo ocupe (humano, bot o dummy) decide; el estado es el mismo. */
USTRUCT()
struct SOULBOUNDCORE_API FSeatState
{
	GENERATED_BODY()

	/** Casilla en la que está la ficha. INDEX_NONE si no está en el tablero (Vacío). */
	UPROPERTY()
	int32 TileId = 0;

	UPROPERTY()
	int32 RuneCount = 0;
};

/**
 * Estado de partida: datos puros, sin mundo. Lo mismo que replica el servidor, lo que simula el bucle
 * con reloj virtual y lo que reciben el cerebro del Warden y los decisores.
 * Spike, pasos 3 y 4: solo lo necesario para mover fichas y sellar casillas.
 */
USTRUCT()
struct SOULBOUNDCORE_API FMatchState
{
	GENERATED_BODY()

	UPROPERTY()
	FBoardState Board;

	/** Siempre cuatro asientos (glosario "Jugador"). */
	UPROPERTY()
	TArray<FSeatState> Seats;

	/** Reloj de partida en segundos. Virtual en simulación, real en juego. Nunca se detiene (R-WAR-4c). */
	UPROPERTY()
	double ClockSeconds = 0.0;

	UPROPERTY()
	int32 ActiveSeat = 0;

	UPROPERTY()
	int32 TurnCount = 0;

	/** Tablero de prueba del spike: seis casillas con un ciclo, la 0 es el Altar y los cuatro asientos empiezan en ella. */
	static FMatchState MakeSpikeMatch();

	bool IsTileOccupied(int32 TileId) const;
};
