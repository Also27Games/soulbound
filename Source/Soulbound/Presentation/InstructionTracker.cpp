#include "InstructionTracker.h"

#include "Match/MatchState.h"
#include "Match/VictoryRules.h"

namespace
{
	constexpr int32 InstructionMomentCount = static_cast<int32>(EInstructionMoment::Insolence) + 1;

	uint32 Bit(EInstructionMoment Moment)
	{
		return 1u << static_cast<uint32>(Moment);
	}

	/** Si hay un Nicho libre entre las casillas a las que puede ir quien tira ahora. */
	bool FreeNicheInReach(const FMatchState& State)
	{
		for (const int32 TileId : State.ReachableOptions)
		{
			if (!State.Board.Tiles.IsValidIndex(TileId) || State.Board.Tiles[TileId].Type != ETileType::Niche)
			{
				continue;
			}
			const int32 DomainId = State.Board.Tiles[TileId].DomainId;
			if (State.Board.Domains.IsValidIndex(DomainId) && State.Board.Domains[DomainId].NicheState == ENicheState::Free)
			{
				return true;
			}
		}
		return false;
	}
}

namespace Soulbound::Presentation
{
	TArray<FInstructionCue> FInstructionTracker::Observe(const FMatchState& State, const TArray<int32>& HumanSeats)
	{
		TArray<FInstructionCue> Cues;
		// Una partida nueva: el registro es más corto que lo recorrido.
		if (Scanned > State.EventLog.Num())
		{
			Scanned = 0;
			Given = 0;
		}
		const int32 FirstHuman = HumanSeats.IsEmpty() ? INDEX_NONE : HumanSeats[0];
		const auto IsHuman = [&HumanSeats](int32 Seat) { return Seat != INDEX_NONE && HumanSeats.Contains(Seat); };
		const auto Give = [this, &Cues](EInstructionMoment Moment, int32 Seat)
		{
			if (Seat != INDEX_NONE && (Given & Bit(Moment)) == 0)
			{
				Given |= Bit(Moment);
				Cues.Add({Moment, Seat});
			}
		};

		// Lo que cuenta el registro, en el orden en que ocurrió.
		for (; Scanned < State.EventLog.Num(); ++Scanned)
		{
			const FMatchEvent& Event = State.EventLog[Scanned];
			switch (Event.Type)
			{
			case EMatchEventType::TurnStarted:
				if (IsHuman(Event.Seat))
				{
					Give(EInstructionMoment::Roll, Event.Seat);
				}
				break;
			case EMatchEventType::Rolled:
				if (IsHuman(Event.Seat))
				{
					Give(EInstructionMoment::ChooseTile, Event.Seat);
				}
				break;
			case EMatchEventType::Moved:
				if (IsHuman(Event.Seat) && State.Board.Tiles.IsValidIndex(Event.Value) && State.Board.Tiles[Event.Value].Type == ETileType::PowerSource)
				{
					Give(EInstructionMoment::Source, Event.Seat);
				}
				break;
			case EMatchEventType::DuelChallenged:
				// El retador o el retado: el primero que sea humano.
				Give(EInstructionMoment::Duel, IsHuman(Event.Seat) ? Event.Seat : (IsHuman(Event.OtherSeat) ? Event.OtherSeat : INDEX_NONE));
				break;
			case EMatchEventType::OracleOpened:
				if (IsHuman(Event.Seat))
				{
					Give(EInstructionMoment::Orb, Event.Seat);
				}
				break;
			case EMatchEventType::VoteStarted:
				Give(EInstructionMoment::Vote, FirstHuman);
				break;
			case EMatchEventType::InsolencePunished:
				if (IsHuman(Event.Seat))
				{
					Give(EInstructionMoment::Insolence, Event.Seat);
				}
				break;
			default:
				break;
			}
		}

		// Lo que cuenta el estado de ahora: lo que se le pide al humano y lo que hay en la mesa.
		if (IsHuman(State.ActiveSeat) && State.TurnPhase == ETurnPhase::RollAndChoose && FreeNicheInReach(State))
		{
			Give(EInstructionMoment::Niche, State.ActiveSeat);
		}
		if (State.PendingChoice.IsWardenEventChoice() && IsHuman(State.PendingChoice.Seat))
		{
			Give(EInstructionMoment::Dilemma, State.PendingChoice.Seat);
		}
		for (const FSeatState& Seat : State.Seats)
		{
			if (Soulbound::Rules::RuneTypeCount(Seat) == Soulbound::Board::DomainCount)
			{
				Give(EInstructionMoment::SixRunes, FirstHuman);
				break;
			}
		}
		return Cues;
	}

	TArray<FInstructionCue> FInstructionTracker::RingOpened(const TArray<int32>& HumanSeats)
	{
		// TODO(R-INS-1): esqueleto para ver el test en rojo.
		return {};
	}

	bool TeachAndRecord(EHintsMode Mode, TArray<FString>& Seen, EInstructionMoment Moment)
	{
		if (Mode == EHintsMode::Never)
		{
			return false;
		}
		const FString Key = Soulbound::Rules::InstructionKey(Moment);
		if (Mode == EHintsMode::Automatic && Seen.Contains(Key))
		{
			return false;
		}
		Seen.AddUnique(Key);
		return true;
	}

	bool ShowsRequestHints(EHintsMode Mode, const TArray<FString>& Seen)
	{
		if (Mode != EHintsMode::Automatic)
		{
			return Mode == EHintsMode::Always;
		}
		for (int32 Moment = 0; Moment < InstructionMomentCount; ++Moment)
		{
			if (!Seen.Contains(Soulbound::Rules::InstructionKey(static_cast<EInstructionMoment>(Moment))))
			{
				return true;
			}
		}
		return false;
	}

	FString InstructionText(const FWardenLineTable& Lines, EInstructionMoment Moment)
	{
		const FString Key = Soulbound::Rules::InstructionKey(Moment);
		for (const FWardenLineEntry& Entry : Lines.Entries)
		{
			if (Entry.Id == Key)
			{
				return Entry.Lines.IsEmpty() ? FString() : Soulbound::Rules::StripLineCues(Entry.Lines[0]);
			}
		}
		return FString();
	}

	bool ParseHintsMode(const FString& Text, EHintsMode& OutMode)
	{
		const FString Clean = Text.TrimStartAndEnd().ToLower();
		if (Clean == TEXT("auto") || Clean == TEXT("automatico") || Clean == TEXT("automático"))
		{
			OutMode = EHintsMode::Automatic;
			return true;
		}
		if (Clean == TEXT("siempre"))
		{
			OutMode = EHintsMode::Always;
			return true;
		}
		if (Clean == TEXT("nunca"))
		{
			OutMode = EHintsMode::Never;
			return true;
		}
		return false;
	}

	FString HintsModeText(EHintsMode Mode)
	{
		switch (Mode)
		{
		case EHintsMode::Always: return TEXT("siempre");
		case EHintsMode::Never: return TEXT("nunca");
		case EHintsMode::Automatic:
		default: return TEXT("automático");
		}
	}

	namespace
	{
		/** El orden del combo de opciones de UGT: la posición de cada ajuste. */
		constexpr EHintsMode HintsComboOrder[] = {EHintsMode::Automatic, EHintsMode::Always, EHintsMode::Never};
	}

	int32 HintsModeToIndex(EHintsMode Mode)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(HintsComboOrder); ++Index)
		{
			if (HintsComboOrder[Index] == Mode)
			{
				return Index;
			}
		}
		return 0;
	}

	EHintsMode HintsModeFromIndex(int32 Index)
	{
		return Index >= 0 && Index < UE_ARRAY_COUNT(HintsComboOrder) ? HintsComboOrder[Index] : EHintsMode::Automatic;
	}
}
