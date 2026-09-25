#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

struct FMatchState;

/**
 * Decisor (glosario "Decisor"): quien decide por un asiento. Humano, bot básico, dummy o dummy insolente
 * implementan la misma interfaz; el motor no distingue. Spike: solo la elección de destino.
 */
class SOULBOUNDCORE_API IDecider
{
public:
	virtual ~IDecider() = default;

	/** Elige una casilla de destino entre las alcanzables. Devuelve INDEX_NONE si no hay opciones. */
	virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) = 0;
};

/** Dummy: elige al azar con el generador que le pasan, así la simulación sigue siendo determinista por semilla. */
class SOULBOUNDCORE_API FDummyDecider : public IDecider
{
public:
	virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override;
};
