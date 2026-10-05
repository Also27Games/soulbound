#pragma once

#include "CoreMinimal.h"
#include "Match/MatchState.h"

/** Qué pasa cuando un asiento elige robar a otro (R-ROB-1, R-ROB-4, R-ROB-5). Nombre propuesto en S-010. */
enum class EStealStart : uint8
{
	/** La víctima no tiene runas: no hay trilero y se registra StealNoRunes (R-ROB-4). */
	NoRunes,
	/** La víctima es Koschei con una sola runa: el robo se anula y se registra StealCancelled (R-ROB-5). */
	Cancelled,
	/** La víctima es Koschei con más de una runa: antes aparta una (etapa SafeRune; la elección la abre la máquina de turnos). */
	SafeRunePending,
	/** Empieza la muestra (etapa Reveal) y se registra ShellGameStarted (R-ROB-2). */
	Started,
};

/**
 * Trilero (glosario `ShellGame`, GDD 04, R-ROB-1 a R-ROB-6): el mecanismo con que se resuelve todo robo. Estado en
 * FMatchState::ShellGame; solo estas reglas lo escriben. La máquina de turnos (Match/TurnRules.h) lleva la fase ShellGame,
 * los plazos y las elecciones (SafeRune, ShellPick) y llama a estas reglas en cada paso. La mezcla es una función pura de
 * la semilla: la interfaz podrá animarla después con los mismos intercambios que usa el motor.
 */
namespace Soulbound::Rules
{
	/**
	 * Semilla de la mezcla del trilero número GameIndex de una partida (R-ROB-6): combina SeedBase (FMatchState::ShellSeedBase,
	 * sacado una vez en StartMatch) con el contador. No toca el sorteo de la partida.
	 */
	SOULBOUNDCORE_API int32 ShellGameSeed(int32 SeedBase, int32 GameIndex);

	/**
	 * Intercambios de la mezcla (R-ROB-2, R-ROB-6), en orden: cada uno cambia de sitio las esferas de las posiciones X e Y
	 * (X < Y). Función pura: misma semilla y mismos números, misma secuencia. Seed: semilla de la mezcla (ShellGameSeed).
	 * OrbCount: esferas. SwapCount: intercambios (FMatchParams::ShellSwapCount). Vacía con menos de dos esferas.
	 */
	SOULBOUNDCORE_API TArray<FIntPoint> ShellGameSwaps(int32 Seed, int32 OrbCount, int32 SwapCount);

	/**
	 * Posición final de cada esfera tras los intercambios de ShellGameSwaps (R-ROB-6): el elemento i es dónde acaba la
	 * esfera que empezó en la posición i. Una permutación de 0 a OrbCount - 1.
	 */
	SOULBOUNDCORE_API TArray<int32> ShellGameFinalPositions(int32 Seed, int32 OrbCount, int32 SwapCount);

	/**
	 * Tipo de runa de la esfera que acaba en la posición Position del trilero en curso (R-ROB-3), según su semilla y
	 * FMatchParams::ShellSwapCount. INDEX_NONE si no hay trilero con esferas o la posición no existe.
	 */
	SOULBOUNDCORE_API int32 ShellRuneAtPosition(const FMatchState& State, int32 Position);

	/**
	 * El asiento Thief roba al asiento Victim (R-ROB-1): sin runas, StealNoRunes (R-ROB-4); Koschei con una runa,
	 * StealCancelled (R-ROB-5); Koschei con más, etapa SafeRune a la espera de que aparte una; si no, empieza la muestra
	 * (StartShellReveal). No valida que el robo sea legal: los objetivos los da StealTargets.
	 * NowSeconds: marca de los eventos e inicio de la muestra. Devuelve qué pasó.
	 */
	SOULBOUNDCORE_API EStealStart BeginSteal(FMatchState& State, int32 Thief, int32 Victim, double NowSeconds);

	/** Tipos de runa que puede apartar la víctima del trilero en curso (R-ROB-5): los de su Brazal, en orden de tipo. */
	SOULBOUNDCORE_API TArray<int32> SafeRuneOptions(const FMatchState& State);

	/**
	 * Koschei aparta una runa del tipo RuneType (R-ROB-5), que no entra en la mezcla y sigue en su Brazal, y se registra
	 * SafeRuneSetAside; con INDEX_NONE (no eligió a tiempo) no aparta ninguna. En los dos casos empieza la muestra.
	 */
	SOULBOUNDCORE_API void SetAsideSafeRune(FMatchState& State, int32 RuneType, double NowSeconds);

	/**
	 * Empieza la muestra (R-ROB-2): las runas de la víctima, duplicados incluidos y menos la apartada, van a las esferas en
	 * orden de tipo; se fija la semilla de la mezcla (ShellGameSeed con el contador de trileros, que sube uno); la etapa
	 * Reveal dura FMatchParams::ShellRevealSeconds. Registra ShellGameStarted.
	 */
	SOULBOUNDCORE_API void StartShellReveal(FMatchState& State, double NowSeconds);

	/** Acaba la muestra y empieza la mezcla (R-ROB-2), que dura FMatchParams::ShellShuffleSeconds. */
	SOULBOUNDCORE_API void StartShellShuffle(FMatchState& State, double NowSeconds);

	/** Acaba la mezcla: el ladrón elige esfera (etapa Pick; la elección ShellPick la abre la máquina de turnos). */
	SOULBOUNDCORE_API void StartShellPick(FMatchState& State);

	/**
	 * El ladrón elige la esfera de la posición Position (R-ROB-3): la runa de esa esfera pasa a su Brazal, aunque ya tenga
	 * de ese tipo, y el resto se queda en el de la víctima; se registra RuneStolen y el trilero termina. Si la víctima pierde
	 * así su última runa de un tipo, pierde el poder en el acto (R-ELE-5: IsPowerActive lo calcula del Brazal).
	 * Devuelve true si una runa cambió de mano.
	 */
	SOULBOUNDCORE_API bool ResolveShellPick(FMatchState& State, int32 Position, double NowSeconds);

	/** El ladrón no elige a tiempo (R-ROB-3): no roba nada, se registra ShellGameExpired y el trilero termina. */
	SOULBOUNDCORE_API void ExpireShellPick(FMatchState& State, double NowSeconds);
}
