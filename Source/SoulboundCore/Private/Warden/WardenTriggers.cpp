#include "Warden/WardenTriggers.h"

#include "Match/EffectCatalog.h"
#include "Match/MatchState.h"

namespace
{
	/** Tipos de runa distintos en el Brazal de Seat. */
	int32 RuneTypes(const FSeatState& Seat)
	{
		return Seat.Runes.FilterByPredicate([](int32 Count) { return Count > 0; }).Num();
	}

	/** Si Seat causa algún disparador Trigger de Triggers. */
	bool Causes(const TArray<FActiveTrigger>& Triggers, EWardenTrigger Trigger, int32 Seat)
	{
		return Seat != INDEX_NONE && Triggers.Contains(FActiveTrigger{Trigger, Seat});
	}

	/** Si hay algún disparador Trigger activo en Triggers. */
	bool IsActive(const TArray<FActiveTrigger>& Triggers, EWardenTrigger Trigger)
	{
		return Triggers.ContainsByPredicate([Trigger](const FActiveTrigger& Active) { return Active.Trigger == Trigger; });
	}

	/** Las entradas de las tablas de los disparadores activos que nombran el evento EventIndex, con su disparador. */
	TArray<TPair<EWardenTrigger, const FTriggerEventEntry*>> ActiveEntries(const FMatchState& State, int32 EventIndex, const TArray<FActiveTrigger>& Triggers)
	{
		TArray<TPair<EWardenTrigger, const FTriggerEventEntry*>> Entries;
		for (const FTriggerTable& Table : State.WardenParams.Triggers.Tables)
		{
			if (!IsActive(Triggers, Table.Trigger))
			{
				continue;
			}
			for (const FTriggerEventEntry& Entry : Table.Events)
			{
				if (Entry.EventIndex == EventIndex)
				{
					Entries.Emplace(Table.Trigger, &Entry);
				}
			}
		}
		return Entries;
	}

	/** Los asientos que causan Trigger en Triggers, en su orden. */
	TArray<int32> CausesOf(const TArray<FActiveTrigger>& Triggers, EWardenTrigger Trigger)
	{
		TArray<int32> Seats;
		for (const FActiveTrigger& Active : Triggers)
		{
			if (Active.Trigger == Trigger && Active.Seat != INDEX_NONE)
			{
				Seats.Add(Active.Seat);
			}
		}
		return Seats;
	}
}

namespace Soulbound::Rules
{
	TArray<FActiveTrigger> ActiveTriggers(const FMatchState& State, double NowSeconds)
	{
		TArray<FActiveTrigger> Active;
		if (!State.WardenParams.HasWarden())
		{
			return Active;
		}
		const FWardenTriggerParams& Params = State.WardenParams.Triggers;
		const int32 SeatCount = State.Seats.Num();

		// Líder claro: quien tiene ClearLeaderGap tipos o más que el segundo (con un solo asiento, nadie es segundo).
		int32 Best = INDEX_NONE;
		int32 BestTypes = -1;
		int32 SecondTypes = -1;
		for (int32 Seat = 0; Seat < SeatCount; ++Seat)
		{
			const int32 Types = RuneTypes(State.Seats[Seat]);
			if (Types > BestTypes)
			{
				SecondTypes = BestTypes;
				BestTypes = Types;
				Best = Seat;
			}
			else
			{
				SecondTypes = FMath::Max(SecondTypes, Types);
			}
		}
		if (Best != INDEX_NONE && SecondTypes >= 0 && BestTypes - SecondTypes >= Params.ClearLeaderGap)
		{
			Active.Add({EWardenTrigger::ClearLeader, Best});
		}

		// Victoria inminente: ImminentVictoryTypes tipos, o en el Altar con los seis.
		for (int32 Seat = 0; Seat < SeatCount; ++Seat)
		{
			const FSeatState& SeatState = State.Seats[Seat];
			const int32 Types = RuneTypes(SeatState);
			const bool bOnAltarWithAll = Types == Soulbound::Board::DomainCount && State.Board.IsAltar(SeatState.TileId);
			if (Types >= Params.ImminentVictoryTypes || bOnAltarWithAll)
			{
				Active.Add({EWardenTrigger::ImminentVictory, Seat});
			}
		}

		// Jugador ignorado: fuera del Vacío, más de IgnoredMinutes sin ser objetivo ni protagonista de un golpe.
		for (int32 Seat = 0; Seat < SeatCount; ++Seat)
		{
			const FSeatState& SeatState = State.Seats[Seat];
			if (!SeatState.IsInVoid() && NowSeconds - SeatState.LastTargetedSeconds > Params.IgnoredMinutes * 60.0)
			{
				Active.Add({EWardenTrigger::IgnoredPlayer, Seat});
			}
		}

		// Partida estancada: fuera de la carrera (decisión del autor del 23/10/2026), StalledMinutes sin runas obtenidas ni duelos.
		if (State.MatchPhase == EMatchPhase::RuneHunt && NowSeconds - State.LastProgressSeconds > Params.StalledMinutes * 60.0)
		{
			Active.Add({EWardenTrigger::StalledMatch, INDEX_NONE});
		}

		// Acaparador: HoarderCount runas o más de un mismo tipo.
		for (int32 Seat = 0; Seat < SeatCount; ++Seat)
		{
			if (State.Seats[Seat].Runes.ContainsByPredicate([&Params](int32 Count) { return Count >= Params.HoarderCount; }))
			{
				Active.Add({EWardenTrigger::Hoarder, Seat});
			}
		}
		return Active;
	}

	double TriggeredEventWeight(const FMatchState& State, int32 EventIndex, EAct Act, int32 Director, const TArray<FActiveTrigger>& Triggers)
	{
		if (!State.WardenEvents.Events.IsValidIndex(EventIndex))
		{
			return 0.0;
		}
		double Weight = State.WardenEvents.Events[EventIndex].WeightIn(Act);
		// R-DIS-1: varios activos a la vez se multiplican; cada tabla cuenta una vez, tenga los causantes que tenga.
		for (const TPair<EWardenTrigger, const FTriggerEventEntry*>& Entry : ActiveEntries(State, EventIndex, Triggers))
		{
			if (!Entry.Value->bWhenActiveIsCause || Causes(Triggers, Entry.Key, Director))
			{
				Weight *= Entry.Value->Multiplier;
			}
		}
		if (IsStrikeAgainstActive(State, EventIndex))
		{
			for (const FTriggerTable& Table : State.WardenParams.Triggers.Tables)
			{
				if (Causes(Triggers, Table.Trigger, Director))
				{
					Weight *= Table.StrikesAgainstActiveMultiplier;
				}
			}
		}
		return Weight;
	}

	double TriggeredInterveneProbability(const FMatchState& State, EAct Act, const TArray<FActiveTrigger>& Triggers)
	{
		const int32 Index = static_cast<int32>(Act);
		if (!State.WardenParams.Acts.IsValidIndex(Index))
		{
			return 0.0;
		}
		const double Base = State.WardenParams.Acts[Index].InterveneProbability;
		const double Multiplier = IsActive(Triggers, EWardenTrigger::ImminentVictory) ? State.WardenParams.Triggers.ImminentVictoryInterveneMultiplier : 1.0;
		return FMath::Min(1.0, Base * Multiplier);
	}

	bool IsStrikeAgainstActive(const FMatchState& State, int32 EventIndex)
	{
		if (!State.WardenEvents.Events.IsValidIndex(EventIndex))
		{
			return false;
		}
		const FWardenEventDef& Event = State.WardenEvents.Events[EventIndex];
		return Event.Channel == EWardenChannel::Strike && Event.Options.ContainsByPredicate([](const FWardenEventOption& Option)
		{
			return Option.Applications.ContainsByPredicate([](const FEffectApplication& Application)
			{
				return Application.Target == EEffectTarget::ActivePlayer && !IsGift(Application.Effect);
			});
		});
	}

	double RandomTargetWeight(const FMatchState& State, int32 Seat, double NowSeconds, const TArray<FActiveTrigger>& Triggers)
	{
		const FWardenTriggerParams& Params = State.WardenParams.Triggers;
		if (!State.WardenParams.HasWarden() || !State.Seats.IsValidIndex(Seat) || Params.IgnoredMinutes <= 0.0)
		{
			return 1.0;
		}
		// R-DIS-3 (decisión del autor del 23/10/2026): crece con el tiempo sin ser objetivo, en unidades del umbral de Jugador ignorado.
		const double Weight = 1.0 + FMath::Max(0.0, NowSeconds - State.Seats[Seat].LastTargetedSeconds) / (Params.IgnoredMinutes * 60.0);
		return Causes(Triggers, EWardenTrigger::IgnoredPlayer, Seat) ? Weight * Params.IgnoredRandomMultiplier : Weight;
	}

	TArray<double> SuggestedRivalWeights(const FMatchState& State, int32 EventIndex, const TArray<int32>& Rivals, const TArray<FActiveTrigger>& Triggers)
	{
		TArray<double> Weights;
		Weights.Init(0.0, Rivals.Num());
		auto Suggest = [&](int32 Seat, double Multiplier)
		{
			const int32 Position = Rivals.Find(Seat);
			if (Position != INDEX_NONE)
			{
				Weights[Position] = Weights[Position] > 0.0 ? Weights[Position] * Multiplier : Multiplier;
			}
		};
		for (const TPair<EWardenTrigger, const FTriggerEventEntry*>& Entry : ActiveEntries(State, EventIndex, Triggers))
		{
			if (Entry.Value->bSuggestCause)
			{
				for (const int32 Seat : CausesOf(Triggers, Entry.Key))
				{
					Suggest(Seat, Entry.Value->Multiplier);
				}
			}
		}
		// Jugador ignorado sugiere en cualquier rival señalado (decisión del autor del 23/10/2026).
		for (const int32 Seat : CausesOf(Triggers, EWardenTrigger::IgnoredPlayer))
		{
			Suggest(Seat, State.WardenParams.Triggers.IgnoredSuggestMultiplier);
		}
		return Weights;
	}

	void ApplyTriggersToEvent(FMatchState& State, const TArray<FActiveTrigger>& Triggers)
	{
		FCurrentWardenEvent& Event = State.CurrentEvent;
		for (const TPair<EWardenTrigger, const FTriggerEventEntry*>& Entry : ActiveEntries(State, Event.EventIndex, Triggers))
		{
			// R-DIS-2: la dirección preferida que dice la tabla (Enséñame al líder) y el Sello hacia el Altar del primer causante.
			if (Entry.Value->bHasTargetOverride && !Event.bHasTargetOverride)
			{
				Event.bHasTargetOverride = true;
				Event.TargetOverride = Entry.Value->TargetOverride;
			}
			const TArray<int32> Causing = CausesOf(Triggers, Entry.Key);
			if (Entry.Value->bSealTowardsAltar && Event.SealTowardsAltarSeat == INDEX_NONE && !Causing.IsEmpty())
			{
				Event.SealTowardsAltarSeat = Causing[0];
			}
		}
	}
}
