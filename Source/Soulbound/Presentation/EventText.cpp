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
		case EMatchEventType::Expelled: return FString::Printf(TEXT("el asiento %d EXPULSA al asiento %d a su hogar"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::ChoiceExpired:
		{
			const EChoiceKind Kind = static_cast<EChoiceKind>(Event.Value);
			const TCHAR* What = Kind == EChoiceKind::Reroll ? TEXT("repetir")
				: Kind == EChoiceKind::PlanarJump ? TEXT("el salto planar")
				: Kind == EChoiceKind::SafeRune ? TEXT("apartar una runa: el trilero se juega con todas")
				: TEXT("la acción contra un jugador");
			return FString::Printf(TEXT("el asiento %d deja pasar %s: se agota su elección"), Event.Seat, What);
		}
		// S-010: el robo del Eco y el trilero.
		case EMatchEventType::ShellGameStarted:
			return FString::Printf(TEXT("TRILERO: el asiento %d roba al asiento %d; sus runas salen a la vista en %d esferas"), Event.Seat, Event.OtherSeat, Event.Value);
		case EMatchEventType::SafeRuneSetAside:
			return FString::Printf(TEXT("el asiento %d (Koschei) pone a salvo la runa del Dominio %d: no entra en la mezcla"), Event.Seat, Event.Value);
		case EMatchEventType::RuneStolen:
			return FString::Printf(TEXT("el asiento %d ROBA la runa del Dominio %d al asiento %d"), Event.Seat, Event.Value, Event.OtherSeat);
		case EMatchEventType::ShellGameExpired:
			return FString::Printf(TEXT("el asiento %d no elige esfera a tiempo: no roba nada al asiento %d"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::StealNoRunes:
			return FString::Printf(TEXT("el asiento %d intenta robar al asiento %d, que no tiene runas: no hay trilero"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::StealCancelled:
			return FString::Printf(TEXT("el asiento %d intenta robar a Koschei (asiento %d), que solo tiene una runa: robo anulado"), Event.Seat, Event.OtherSeat);
		case EMatchEventType::DebugEdit:
			return Event.Seat == INDEX_NONE ? FString::Printf(TEXT("EDICIÓN DE DEPURACIÓN: %s"), *DebugKindName(Event.Value))
				: FString::Printf(TEXT("EDICIÓN DE DEPURACIÓN: %s (asiento %d)"), *DebugKindName(Event.Value), Event.Seat);
		}
		return FString();
	}

	FString DebugKindName(int32 Kind)
	{
		switch (static_cast<EDebugEditKind>(Kind))
		{
		case EDebugEditKind::GiveRune: return TEXT("dar runas");
		case EDebugEditKind::TakeRune: return TEXT("quitar runas");
		case EDebugEditKind::Move: return TEXT("colocar ficha");
		case EDebugEditKind::Bond: return TEXT("vincular");
		case EDebugEditKind::Seal: return TEXT("sellar");
		case EDebugEditKind::Unseal: return TEXT("liberar Sello");
		case EDebugEditKind::EndRace: return TEXT("terminar la carrera");
		case EDebugEditKind::FixRoll: return TEXT("fijar la tirada");
		case EDebugEditKind::FixNextSeat: return TEXT("fijar el siguiente asiento");
		case EDebugEditKind::FixTrial: return TEXT("fijar el resultado del minijuego");
		}
		return TEXT("edición desconocida");
	}

	FString DescribeDebugResult(EDebugEditResult Result)
	{
		switch (Result)
		{
		case EDebugEditResult::Applied: return TEXT("aplicada");
		case EDebugEditResult::InvalidSeat: return TEXT("ese asiento no existe (0 a 3)");
		case EDebugEditResult::InvalidTile: return TEXT("esa casilla no existe");
		case EDebugEditResult::InvalidDomain: return TEXT("ese Dominio o tipo de runa no existe (0 a 5)");
		case EDebugEditResult::InvalidAmount: return TEXT("la cantidad debe ser 1 o más");
		case EDebugEditResult::BoardPrimordial: return TEXT("el tablero es Primordial: aún no hay reservas de runas");
		case EDebugEditResult::NoSource: return TEXT("ese Dominio no tiene Fuente de Poder");
		case EDebugEditResult::NotEnoughReserve: return TEXT("la reserva de la Fuente no llega a esa cantidad");
		case EDebugEditResult::NotEnoughRunes: return TEXT("el asiento no tiene tantas runas de ese tipo");
		case EDebugEditResult::NotInBondRace: return TEXT("la partida no está en la carrera por el vínculo");
		case EDebugEditResult::NotLostEcho: return TEXT("ese asiento ya es Elegido");
		case EDebugEditResult::NicheNotFree: return TEXT("el Nicho de ese Dominio no está Libre");
		case EDebugEditResult::NotSealed: return TEXT("esa casilla no está sellada");
		case EDebugEditResult::InvalidRoll: return FString::Printf(TEXT("la tirada debe estar entre 1 y %d"), Soulbound::Debug::MaxFixedRoll());
		}
		return TEXT("rechazada");
	}

	FString DescribeSealResult(ESealResult Result)
	{
		switch (Result)
		{
		case ESealResult::Accepted: return TEXT("aceptado");
		case ESealResult::InvalidTile: return TEXT("esa casilla no existe");
		case ESealResult::NoSealsAllowed: return TEXT("este tablero no admite Sellos (máximo 0)");
		case ESealResult::AlreadySealed: return TEXT("esa casilla ya está sellada");
		case ESealResult::Altar: return TEXT("no se sella el Altar");
		case ESealResult::Niche: return TEXT("no se sella un Nicho");
		case ESealResult::PowerSource: return TEXT("no se sella una Fuente de Poder");
		case ESealResult::Occupied: return TEXT("la casilla está ocupada");
		case ESealResult::CutsOffAltar: return TEXT("dejaría casillas sin camino al Altar");
		}
		return TEXT("rechazado");
	}

	EEventSource EventSource(EMatchEventType Type)
	{
		switch (Type)
		{
		case EMatchEventType::DebugEdit:
			return EEventSource::Debug;
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
