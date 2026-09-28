#include "Match/MatchStart.h"

#include "Board/BoardLoader.h"
#include "Math/RandomStream.h"

namespace
{
	/** Baraja Items con el sorteo de la partida (Fisher-Yates): misma semilla, mismo orden. */
	template <typename ItemType>
	void Shuffle(TArray<ItemType>& Items, FRandomStream& Random)
	{
		for (int32 Last = Items.Num() - 1; Last > 0; --Last)
		{
			Items.Swap(Last, Random.RandRange(0, Last));
		}
	}

	FMatchStartResult Fail(EMatchStartError Error, const FString& Message)
	{
		FMatchStartResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}
}

namespace Soulbound::Rules
{
	FMatchStartResult StartMatch(const FBoardState& Board, int32 PlayerCount, FRandomStream& Random)
	{
		using Soulbound::Board::DomainCount;

		if (PlayerCount < Soulbound::Match::MinPlayers || PlayerCount > Soulbound::Match::MaxPlayers)
		{
			return Fail(EMatchStartError::InvalidPlayerCount, FString::Printf(TEXT("Una partida admite de %d a %d jugadores, no %d."),
				Soulbound::Match::MinPlayers, Soulbound::Match::MaxPlayers, PlayerCount));
		}
		const FBoardLoadResult Validation = ValidateBoard(Board);
		if (!Validation.IsSuccess())
		{
			return Fail(EMatchStartError::InvalidBoard, TEXT("El tablero no es válido: ") + Validation.Message);
		}

		FMatchStartResult Result;
		FMatchState& State = Result.State;
		State.Board = Board;

		// R-TAB-5: una bolsa con tantos efectos de cada tipo como piden los datos, barajada y repartida a los Nodos
		// en orden de casilla. ValidateBoard garantiza que hay tantos Nodos como efectos en la bolsa.
		TArray<EPowerNodeEffect> Effects;
		Effects.Reserve(Board.Params.TotalPowerNodeCount());
		const TPair<EPowerNodeEffect, int32> Counts[] = {
			{EPowerNodeEffect::Duel, Board.Params.DuelNodeCount},
			{EPowerNodeEffect::Oracle, Board.Params.OracleNodeCount},
			{EPowerNodeEffect::Planar, Board.Params.PlanarNodeCount},
		};
		for (const TPair<EPowerNodeEffect, int32>& Count : Counts)
		{
			for (int32 i = 0; i < Count.Value; ++i)
			{
				Effects.Add(Count.Key);
			}
		}
		Shuffle(Effects, Random);
		int32 NextEffect = 0;
		for (FTileState& Tile : State.Board.Tiles)
		{
			Tile.PowerNodeEffect = Tile.Type == ETileType::PowerNode ? Effects[NextEffect++] : EPowerNodeEffect::None;
		}

		// R-TAB-6: 6 menos N Dominios Olvidados al azar, con su Nicho Cerrado; el resto con dueño (su Elegido) y con el Nicho Libre.
		TArray<int32> DomainOrder;
		for (int32 D = 0; D < DomainCount; ++D)
		{
			DomainOrder.Add(D);
		}
		Shuffle(DomainOrder, Random);
		const int32 ForgottenCount = DomainCount - PlayerCount;
		for (int32 Index = 0; Index < DomainOrder.Num(); ++Index)
		{
			FDomainState& Domain = State.Board.Domains[DomainOrder[Index]];
			const bool bForgotten = Index < ForgottenCount;
			Domain.State = bForgotten ? EDomainState::Forgotten : EDomainState::Owned;
			Domain.NicheState = bForgotten ? ENicheState::Closed : ENicheState::Free;
		}

		// R-TAB-2: siempre cuatro asientos, todos en el Altar.
		State.Seats.SetNum(Soulbound::Match::SeatCount);
		const int32 Altar = State.Board.FindAltar();
		for (FSeatState& Seat : State.Seats)
		{
			Seat.TileId = Altar;
		}
		return Result;
	}
}
