#include "Board/BoardState.h"

int32 FBoardState::AddTile()
{
	FTileState& Tile = Tiles.AddDefaulted_GetRef();
	Tile.TileId = Tiles.Num() - 1;
	return Tile.TileId;
}

void FBoardState::Connect(int32 TileA, int32 TileB)
{
	if (!IsValidTile(TileA) || !IsValidTile(TileB) || TileA == TileB)
	{
		return;
	}
	Tiles[TileA].Neighbours.AddUnique(TileB);
	Tiles[TileB].Neighbours.AddUnique(TileA);
}
