#include "Warden/WardenBrain.h"

#include "Match/MatchState.h"

FWardenDecision FWardenBrain::Evaluate(const FMatchState& State, FRandomStream& Rng) const
{
	FWardenDecision Decision;

	// Consumimos siempre el mismo número de valores del generador para que la secuencia sea reproducible.
	const double Dice = Rng.FRand();
	const int32 Pick = Rng.RandRange(0, FMath::Max(State.Board.Tiles.Num() - 1, 0));

	if (Dice > Params.InterveneProbability)
	{
		return Decision;
	}

	// Candidatas a Sello: casillas sin sellar, no ocupadas y que no sean el Altar (glosario "Sello").
	TArray<int32> Sealable;
	TArray<int32> Sealed;
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (Tile.bSealed)
		{
			Sealed.Add(Tile.TileId);
		}
		else if (!State.Board.IsAltar(Tile.TileId) && !State.IsTileOccupied(Tile.TileId))
		{
			Sealable.Add(Tile.TileId);
		}
	}

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
	switch (Decision.Action)
	{
	case EWardenAction::SealTile:
		if (State.Board.IsValidTile(Decision.TileId))
		{
			State.Board.Tiles[Decision.TileId].bSealed = true;
		}
		break;
	case EWardenAction::ReleaseTile:
		if (State.Board.IsValidTile(Decision.TileId))
		{
			State.Board.Tiles[Decision.TileId].bSealed = false;
		}
		break;
	default:
		break;
	}
}
