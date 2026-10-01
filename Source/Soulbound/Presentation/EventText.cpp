#include "EventText.h"

namespace Soulbound::Presentation
{
	FString TileShortName(const FBoardState& Board, int32 TileId)
	{
		if (!Board.IsValidTile(TileId))
		{
			return FString::Printf(TEXT("casilla %d"), TileId);
		}
		const FTileState& Tile = Board.Tiles[TileId];
		return Tile.Type == ETileType::Altar ? FString(TEXT("ALTAR")) : FString::Printf(TEXT("D%d·%d"), Tile.DomainId, Tile.TileId);
	}

	FString DescribeEvent(const FMatchEvent& Event, const FBoardState& Board)
	{
		switch (Event.Type)
		{
		case EMatchEventType::TurnStarted: return FString::Printf(TEXT("empieza el turno del asiento %d"), Event.Seat);
		case EMatchEventType::Rolled: return FString::Printf(TEXT("el asiento %d tira %d"), Event.Seat, Event.Value);
		case EMatchEventType::TurnClockExpired: return FString::Printf(TEXT("el asiento %d AGOTA el reloj de turno: no se mueve"), Event.Seat);
		case EMatchEventType::TurnSkipped: return FString::Printf(TEXT("el asiento %d pierde el turno"), Event.Seat);
		case EMatchEventType::TurnEnded: return FString::Printf(TEXT("termina el turno del asiento %d"), Event.Seat);
		case EMatchEventType::Moved: return FString::Printf(TEXT("el asiento %d se mueve a %s"), Event.Seat, *TileShortName(Board, Event.Value));
		case EMatchEventType::SealPlaced: return FString::Printf(TEXT("el Warden sella la casilla %d"), Event.Value);
		case EMatchEventType::SealReleased: return FString::Printf(TEXT("se libera el Sello de la casilla %d"), Event.Value);
		case EMatchEventType::Bonded: return FString::Printf(TEXT("el asiento %d se vincula al Elegido del Dominio %d"), Event.Seat, Event.Value);
		case EMatchEventType::BoardAwakening: return FString::Printf(TEXT("el asiento %d despierta el tablero"), Event.Seat);
		case EMatchEventType::HomeAssigned: return FString::Printf(TEXT("el asiento %d queda Eco, con hogar en el Dominio %d"), Event.Seat, Event.Value);
		case EMatchEventType::ReturnedHome: return FString::Printf(TEXT("el asiento %d vuelve a su hogar, la casilla %d"), Event.Seat, Event.Value);
		case EMatchEventType::WardenVictory: return FString(TEXT("VICTORIA DEL WARDEN: nadie se ha vinculado"));
		case EMatchEventType::BondRaceEnded: return FString::Printf(TEXT("fin de la carrera: %d Elegidos"), Event.Value);
		}
		return FString();
	}

	EEventSource EventSource(EMatchEventType Type)
	{
		switch (Type)
		{
		case EMatchEventType::SealPlaced:
		case EMatchEventType::SealReleased:
			return EEventSource::Warden;
		case EMatchEventType::Bonded:
		case EMatchEventType::BoardAwakening:
		case EMatchEventType::HomeAssigned:
		case EMatchEventType::ReturnedHome:
		case EMatchEventType::WardenVictory:
		case EMatchEventType::BondRaceEnded:
			return EEventSource::Race;
		default:
			return EEventSource::Seat;
		}
	}
}
