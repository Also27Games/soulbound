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
		case EMatchEventType::ReturnedHome: return FString::Printf(TEXT("el asiento %d vuelve a su hogar, %s"), Event.Seat, *TileShortName(Board, Event.Value));
		case EMatchEventType::WardenVictory: return FString(TEXT("VICTORIA DEL WARDEN: nadie se ha vinculado"));
		case EMatchEventType::BondRaceEnded: return FString::Printf(TEXT("fin de la carrera: %d Elegidos"), Event.Value);
		case EMatchEventType::RuneObtained: return FString::Printf(TEXT("el asiento %d obtiene la runa del Dominio %d"), Event.Seat, Event.Value);
		// El minijuego real no existe aún (su story): el decisor del asiento lo resuelve al azar, y la crónica lo dice.
		case EMatchEventType::SkillMinigamePassed: return FString::Printf(TEXT("el asiento %d SUPERA el minijuego de la Fuente del Dominio %d (resuelto al azar)"), Event.Seat, Event.Value);
		case EMatchEventType::SkillMinigameFailed: return FString::Printf(TEXT("el asiento %d FALLA el minijuego de la Fuente del Dominio %d (resuelto al azar)"), Event.Seat, Event.Value);
		case EMatchEventType::SourceDepleted: return FString::Printf(TEXT("el asiento %d llega a la Fuente del Dominio %d, AGOTADA: sin runa"), Event.Seat, Event.Value);
		case EMatchEventType::RuneReturned: return FString::Printf(TEXT("el asiento %d devuelve la runa del Dominio %d a su Fuente"), Event.Seat, Event.Value);
		case EMatchEventType::DiceChosen: return FString::Printf(TEXT("el asiento %d elige tirar %s"), Event.Seat, Event.Value > 1 ? TEXT("DOS dados") : TEXT("un dado"));
		case EMatchEventType::Rerolled: return FString::Printf(TEXT("el asiento %d REPITE la tirada: %d"), Event.Seat, Event.Value);
		case EMatchEventType::PlanarJumped: return FString::Printf(TEXT("el asiento %d SALTA por Vuelo Planar a %s"), Event.Seat, *TileShortName(Board, Event.Value));
		case EMatchEventType::Expelled: return FString::Printf(TEXT("el asiento %d EXPULSA al asiento %d a su hogar"), Event.Seat, Event.Value);
		case EMatchEventType::ChoiceExpired:
		{
			const EChoiceKind Kind = static_cast<EChoiceKind>(Event.Value);
			const TCHAR* What = Kind == EChoiceKind::Reroll ? TEXT("repetir") : Kind == EChoiceKind::PlanarJump ? TEXT("el salto planar") : TEXT("la expulsión");
			return FString::Printf(TEXT("el asiento %d deja pasar %s: se agota su elección"), Event.Seat, What);
		}
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
