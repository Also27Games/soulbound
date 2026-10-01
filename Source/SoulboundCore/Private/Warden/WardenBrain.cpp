#include "Warden/WardenBrain.h"

#include "Board/SealRules.h"
#include "Match/MatchState.h"

FWardenDecision FWardenBrain::Evaluate(const FMatchState& State, FRandomStream& Rng) const
{
	FWardenDecision Decision;
	// Con la partida terminada (victoria del Warden, R-CAR-8) el Warden ya no actúa (decisión del autor del 01/10/2026).
	if (State.IsMatchOver())
	{
		return Decision;
	}

	// Consumimos siempre el mismo número de valores del generador para que la secuencia sea reproducible.
	const double Dice = Rng.FRand();
	const int32 Pick = Rng.RandRange(0, FMath::Max(State.Board.Tiles.Num() - 1, 0));

	if (Dice > Params.InterveneProbability)
	{
		return Decision;
	}

	// Candidatas a Sello: las que acepta la regla del tablero (R-TAB-22), así ninguna intervención se pierde.
	TArray<int32> Sealable;
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (Soulbound::Rules::CanSealTile(State, Tile.TileId) == ESealResult::Accepted)
		{
			Sealable.Add(Tile.TileId);
		}
	}
	const TArray<int32>& Sealed = State.Board.SealedTiles;

	if (Sealable.Num() > 0 && (Sealed.Num() == 0 || Pick % 2 == 0))
	{
		Decision.Action = EWardenAction::SealTile;
		Decision.TileId = Sealable[Pick % Sealable.Num()];
		Decision.Line = FString::Printf(TEXT("Esa casilla, la %d, ya no es vuestra."), Decision.TileId);
	}
	else if (Sealed.Num() > 0)
	{
		Decision.Action = EWardenAction::ReleaseTile;
		Decision.TileId = Sealed[Pick % Sealed.Num()];
		Decision.Line = FString::Printf(TEXT("Os devuelvo la %d. No os acostumbreis."), Decision.TileId);
	}
	else
	{
		Decision.Action = EWardenAction::Comment;
		Decision.Line = TEXT("Que lentos sois.");
	}
	return Decision;
}

void FWardenBrain::Apply(FMatchState& State, const FWardenDecision& Decision)
{
	// Las reglas de Sello son del tablero: el tope, las prohibiciones y el registro los pone el motor.
	switch (Decision.Action)
	{
	case EWardenAction::SealTile:
		Soulbound::Rules::SealTile(State, Decision.TileId, State.ClockSeconds);
		break;
	case EWardenAction::ReleaseTile:
		Soulbound::Rules::ReleaseSeal(State, Decision.TileId, State.ClockSeconds);
		break;
	default:
		break;
	}
}
