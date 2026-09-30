#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

struct FMatchState;

/** Lo que el cerebro decide en una evaluación. Spike: sellar o liberar una casilla, o solo un comentario. */
enum class EWardenAction : uint8
{
	None,
	SealTile,
	ReleaseTile,
	Comment
};

struct SOULBOUNDCORE_API FWardenDecision
{
	EWardenAction Action = EWardenAction::None;
	int32 TileId = INDEX_NONE;
	FString Line;

	bool IsIntervention() const { return Action != EWardenAction::None; }
};

/**
 * IA directora del Warden (glosario "WardenBrain"): clase sin mundo. Recibe el estado y un generador con semilla,
 * devuelve una decisión; no aplica nada ni conoce el reloj. El anfitrión (UWardenHostSubsystem, en juego) y el
 * bucle de simulación la llaman igual (R-WAR-1). Spike, paso 4: reglas de mentira; lo que se prueba es la forma.
 */
class SOULBOUNDCORE_API FWardenBrain
{
public:
	struct FParams
	{
		/** Probabilidad de intervenir en cada evaluación (R-WAR-3: 20/35/50 % por acto; aquí un valor fijo). */
		double InterveneProbability = 0.35;
	};

	explicit FWardenBrain(const FParams& InParams = FParams()) : Params(InParams) {}

	/** Evalúa el estado y decide. Determinista para un mismo estado y un mismo generador. */
	FWardenDecision Evaluate(const FMatchState& State, FRandomStream& Rng) const;

	/**
	 * Aplica una decisión sobre el estado con las reglas de Sello del motor (Board/SealRules.h); la marca de tiempo
	 * de sus eventos es State.ClockSeconds. Lo usa quien tenga la autoridad: el anfitrión en juego, el bucle en simulación.
	 */
	static void Apply(FMatchState& State, const FWardenDecision& Decision);

private:
	FParams Params;
};
