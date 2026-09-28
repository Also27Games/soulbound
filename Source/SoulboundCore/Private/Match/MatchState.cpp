#include "Match/MatchState.h"

namespace
{
	/**
	 * Máximo de Sellos del montaje del spike: valor inicial del tablero de pruebas; el montaje del spike desaparece
	 * cuando el anfitrión cargue datos (S-005).
	 */
	constexpr int32 SpikeMaxSealedTiles = 3;
}

FMatchState FMatchState::MakeSpikeMatch()
{
	FMatchState State;
	for (int32 i = 0; i < 6; ++i)
	{
		State.Board.AddTile();
	}
	State.Board.Tiles[0].Type = ETileType::Altar;
	State.Board.Params.MaxSealedTiles = SpikeMaxSealedTiles;
	State.Board.Connect(0, 1);
	State.Board.Connect(1, 2);
	State.Board.Connect(2, 3);
	State.Board.Connect(1, 4);
	State.Board.Connect(4, 5);
	State.Board.Connect(5, 3);

	State.Seats.SetNum(Soulbound::Match::SeatCount);
	for (FSeatState& Seat : State.Seats)
	{
		Seat.TileId = 0;
	}
	return State;
}

bool FMatchState::IsTileOccupied(int32 TileId) const
{
	for (const FSeatState& Seat : Seats)
	{
		if (Seat.TileId == TileId)
		{
			return true;
		}
	}
	return false;
}
