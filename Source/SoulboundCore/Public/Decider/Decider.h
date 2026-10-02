#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"
#include "Math/RandomStream.h"

namespace Soulbound::Decider
{
	/**
	 * Probabilidad con la que un dummy supera el minijuego de habilidad (decisión del autor, 02/10/2026). Valor inicial
	 * 0,5; es el comportamiento de un doble de prueba hasta que exista el minijuego real, no una regla de juego.
	 */
	inline constexpr double DummySkillSuccessChance = 0.5;
}

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

	/**
	 * Resultado del minijuego de habilidad (glosario "Minijuego de habilidad"): true si el asiento lo supera. Lo pregunta
	 * quien avanza la partida cuando el motor espera en SkillTrial, y entrega la respuesta con SubmitSkillTrialResult,
	 * igual que hace con el destino (S-008). Trial y Difficulty son los datos de la prueba (FMatchState::PendingTrial y
	 * PendingDifficulty). El minijuego real de un humano llegará con su story; hasta entonces los dummies responden con
	 * una probabilidad fija (0,5) sacada del generador que les pasan.
	 */
	virtual bool PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng) = 0;
};

/** Dummy: elige al azar con el generador que le pasan, así la simulación sigue siendo determinista por semilla. */
class SOULBOUNDCORE_API FDummyDecider : public IDecider
{
public:
	virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override;
	virtual bool PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng) override;
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
	virtual bool PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng) override;
};
