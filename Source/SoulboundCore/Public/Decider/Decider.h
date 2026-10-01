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

/**
 * Dummy de carrera (S-006, nombre propuesto): doble de prueba para la simulación y la vista de depuración, no el bot
 * básico del GDD 08. Si es Eco y la carrera sigue, elige la opción que deja más cerca, en pasos del grafo (sin mirar
 * Sellos ni el Altar), el Nicho Libre más cercano; los empates, al azar. Si es Elegido, la carrera acabó o no queda
 * ningún Nicho Libre al que llegar, elige al azar como FDummyDecider. Siempre con el generador que le pasan.
 */
class SOULBOUNDCORE_API FRaceDummyDecider : public IDecider
{
public:
	virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override;
};
