#include "Match/DebugEdits.h"

#include "Match/BondRaceRules.h"
#include "Match/MatchEvents.h"
#include "Match/MatchState.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Board/SealRules.h"

namespace
{
	using Soulbound::Rules::LogEvent;

	/** Deja el evento de una edición aplicada. Se llama justo antes de aplicarla, para que preceda a los eventos de las reglas. */
	void LogEdit(FMatchState& State, EDebugEditKind Kind, double NowSeconds, int32 Seat)
	{
		LogEvent(State, EMatchEventType::DebugEdit, NowSeconds, Seat, static_cast<int32>(Kind));
	}

	bool IsValidRuneType(int32 RuneType)
	{
		return RuneType >= 0 && RuneType < Soulbound::Board::DomainCount;
	}
}

namespace Soulbound::Debug
{
	int32 MaxFixedRoll()
	{
		return Soulbound::Dice::MaxCount * Soulbound::Dice::Faces;
	}

	EDebugEditResult GiveRune(FMatchState& State, int32 Seat, int32 RuneType, int32 Count, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		if (!IsValidRuneType(RuneType))
		{
			return EDebugEditResult::InvalidDomain;
		}
		if (Count <= 0)
		{
			return EDebugEditResult::InvalidAmount;
		}
		if (State.Board.Phase == EBoardPhase::Primordial)
		{
			return EDebugEditResult::BoardPrimordial;
		}
		const int32 Source = State.Board.FindPowerSource(RuneType);
		if (Source == INDEX_NONE)
		{
			return EDebugEditResult::NoSource;
		}
		FTileState& SourceTile = State.Board.Tiles[Source];
		if (SourceTile.RuneReserve < Count)
		{
			return EDebugEditResult::NotEnoughReserve;
		}
		LogEdit(State, EDebugEditKind::GiveRune, NowSeconds, Seat);
		// Ninguna regla saca runas de la reserva a un Brazal sin llegar a una Fuente: se escribe a mano (S-011).
		SourceTile.RuneReserve -= Count;
		State.Seats[Seat].Runes[RuneType] += Count;
		return EDebugEditResult::Applied;
	}

	EDebugEditResult TakeRune(FMatchState& State, int32 Seat, int32 RuneType, int32 Count, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		if (!IsValidRuneType(RuneType))
		{
			return EDebugEditResult::InvalidDomain;
		}
		if (Count <= 0)
		{
			return EDebugEditResult::InvalidAmount;
		}
		if (State.Seats[Seat].RuneCountOf(RuneType) < Count)
		{
			return EDebugEditResult::NotEnoughRunes;
		}
		const int32 Source = State.Board.FindPowerSource(RuneType);
		if (Source == INDEX_NONE)
		{
			return EDebugEditResult::NoSource;
		}
		LogEdit(State, EDebugEditKind::TakeRune, NowSeconds, Seat);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Soulbound::Rules::ReturnRuneToSource(State, Seat, RuneType, Source, NowSeconds);
		}
		return EDebugEditResult::Applied;
	}

	EDebugEditResult PlaceToken(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		if (!State.Board.IsValidTile(TileId))
		{
			return EDebugEditResult::InvalidTile;
		}
		LogEdit(State, EDebugEditKind::Move, NowSeconds, Seat);
		State.Seats[Seat].TileId = TileId;
		return EDebugEditResult::Applied;
	}

	EDebugEditResult BondSeat(FMatchState& State, int32 Seat, int32 DomainId, double NowSeconds, FRandomStream& Random)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		if (!State.Board.Domains.IsValidIndex(DomainId))
		{
			return EDebugEditResult::InvalidDomain;
		}
		if (State.MatchPhase != EMatchPhase::BondRace)
		{
			return EDebugEditResult::NotInBondRace;
		}
		if (State.Seats[Seat].Role != ESeatRole::LostEcho)
		{
			return EDebugEditResult::NotLostEcho;
		}
		if (State.Board.Domains[DomainId].NicheState != ENicheState::Free)
		{
			return EDebugEditResult::NicheNotFree;
		}
		LogEdit(State, EDebugEditKind::Bond, NowSeconds, Seat);
		Soulbound::Rules::ClaimNiche(State, Seat, DomainId, NowSeconds, Random);
		return EDebugEditResult::Applied;
	}

	ESealResult SealTileEdit(FMatchState& State, int32 TileId, double NowSeconds)
	{
		const ESealResult Check = Soulbound::Rules::CanSealTile(State, TileId);
		if (Check != ESealResult::Accepted)
		{
			return Check;
		}
		LogEdit(State, EDebugEditKind::Seal, NowSeconds, INDEX_NONE);
		return Soulbound::Rules::SealTile(State, TileId, NowSeconds);
	}

	EDebugEditResult UnsealTile(FMatchState& State, int32 TileId, double NowSeconds)
	{
		if (!State.Board.IsValidTile(TileId))
		{
			return EDebugEditResult::InvalidTile;
		}
		if (!State.Board.Tiles[TileId].bSealed)
		{
			return EDebugEditResult::NotSealed;
		}
		LogEdit(State, EDebugEditKind::Unseal, NowSeconds, INDEX_NONE);
		Soulbound::Rules::ReleaseSeal(State, TileId, NowSeconds);
		return EDebugEditResult::Applied;
	}

	EDebugEditResult EndRace(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		if (State.MatchPhase != EMatchPhase::BondRace)
		{
			return EDebugEditResult::NotInBondRace;
		}
		LogEdit(State, EDebugEditKind::EndRace, NowSeconds, INDEX_NONE);
		Soulbound::Rules::EndBondRace(State, NowSeconds);
		return EDebugEditResult::Applied;
	}

	EDebugEditResult FixNextRoll(FMatchState& State, int32 Value, double NowSeconds)
	{
		if (Value < 1 || Value > MaxFixedRoll())
		{
			return EDebugEditResult::InvalidRoll;
		}
		LogEdit(State, EDebugEditKind::FixRoll, NowSeconds, State.ActiveSeat);
		State.ForcedRoll = Value;
		return EDebugEditResult::Applied;
	}

	EDebugEditResult FixNextSeat(FMatchState& State, int32 Seat, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		LogEdit(State, EDebugEditKind::FixNextSeat, NowSeconds, Seat);
		State.ForcedNextSeat = Seat;
		return EDebugEditResult::Applied;
	}

	EDebugEditResult FixTrial(FMatchState& State, int32 Seat, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		LogEdit(State, EDebugEditKind::FixTrial, NowSeconds, Seat);
		return EDebugEditResult::Applied;
	}

	EDebugEditResult FixDuel(FMatchState& State, int32 Seat, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		LogEdit(State, EDebugEditKind::FixDuel, NowSeconds, Seat);
		return EDebugEditResult::Applied;
	}

	EEffectResult BanishSeat(FMatchState& State, int32 Seat, double NowSeconds)
	{
		// Como el Sello: la comprobación de la regla decide; aplicada, el evento de depuración va antes que el de la regla. Los
		// comandos no destierran con la Furia (S-013): durante la carrera, la regla los rechaza.
		const EEffectResult Check = Soulbound::Rules::CanBanish(State, Seat, false);
		if (Check != EEffectResult::Applied)
		{
			return Check;
		}
		LogEdit(State, EDebugEditKind::Banish, NowSeconds, Seat);
		return Soulbound::Rules::Banish(State, Seat, false, NowSeconds);
	}

	EEffectResult ReleaseVoidEdit(FMatchState& State, double NowSeconds)
	{
		const EEffectResult Check = Soulbound::Rules::CanReleaseVoid(State);
		if (Check != EEffectResult::Applied)
		{
			return Check;
		}
		LogEdit(State, EDebugEditKind::ReleaseVoid, NowSeconds, INDEX_NONE);
		return Soulbound::Rules::ReleaseVoid(State, NowSeconds);
	}

	EEffectResult SendHomeEdit(FMatchState& State, int32 Seat, double NowSeconds)
	{
		const EEffectResult Check = Soulbound::Rules::CanSendHome(State, Seat);
		if (Check != EEffectResult::Applied)
		{
			return Check;
		}
		LogEdit(State, EDebugEditKind::SendHome, NowSeconds, Seat);
		return Soulbound::Rules::SendHome(State, Seat, NowSeconds);
	}
}

namespace Soulbound::Debug
{
	EWardenEventStart LaunchEvent(FMatchState& State, const FString& EventId, double NowSeconds, FRandomStream& Random)
	{
		// Como los efectos de S-013: la comprobación de la regla decide; aplicada, el evento de depuración va antes que los del
		// evento. Siempre como entrega directa del Warden, entre turnos (S-014).
		const EWardenEventStart Check = Soulbound::Rules::CanApplyEvent(State, EventId);
		if (Check != EWardenEventStart::Started)
		{
			return Check;
		}
		LogEdit(State, EDebugEditKind::LaunchEvent, NowSeconds, INDEX_NONE);
		return Soulbound::Rules::ApplyEvent(State, EventId, NowSeconds, Random);
	}

	EWardenEventStart StrikeEvent(FMatchState& State, const FString& EventId, double NowSeconds, FRandomStream& MatchRandom, FRandomStream& WardenRandom)
	{
		return EWardenEventStart::NotBetweenTurns;
	}

	EDebugEditResult SetInsolence(FMatchState& State, int32 Seat, int32 Level, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return EDebugEditResult::InvalidSeat;
		}
		// 5 no es un nivel en el que se quede nadie: la quinta subida es la Furia y la devuelve a 0 (R-INS-3).
		if (Level < 0 || Level >= Soulbound::Match::MaxInsolence)
		{
			return EDebugEditResult::InvalidAmount;
		}
		LogEdit(State, EDebugEditKind::SetInsolence, NowSeconds, Seat);
		State.Seats[Seat].Insolence = Level;
		return EDebugEditResult::Applied;
	}
}
