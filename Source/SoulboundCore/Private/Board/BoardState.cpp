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

void FBoardState::Disconnect(int32 TileA, int32 TileB)
{
	if (!IsValidTile(TileA) || !IsValidTile(TileB))
	{
		return;
	}
	Tiles[TileA].Neighbours.Remove(TileB);
	Tiles[TileB].Neighbours.Remove(TileA);
}

int32 FBoardState::FindAltar() const
{
	const FTileState* Altar = Tiles.FindByPredicate([](const FTileState& Tile) { return Tile.Type == ETileType::Altar; });
	return Altar ? Altar->TileId : INDEX_NONE;
}

int32 FBoardState::FindNiche(int32 DomainId) const
{
	const FTileState* Niche = Tiles.FindByPredicate([DomainId](const FTileState& Tile)
	{
		return Tile.Type == ETileType::Niche && Tile.DomainId == DomainId;
	});
	return Niche ? Niche->TileId : INDEX_NONE;
}

int32 FBoardState::FindPowerSource(int32 DomainId) const
{
	const FTileState* Source = Tiles.FindByPredicate([DomainId](const FTileState& Tile)
	{
		return Tile.Type == ETileType::PowerSource && Tile.DomainId == DomainId;
	});
	return Source ? Source->TileId : INDEX_NONE;
}
