#include "Match/TurnRules.h"

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Math/RandomStream.h"

namespace
{
	void LogEvent(FMatchState& State, EMatchEventType Type, double NowSeconds, int32 Seat, int32 Value = INDEX_NONE)
	{
		FMatchEvent& Event = State.EventLog.AddDefaulted_GetRef();
		Event.Type = Type;
		Event.ClockSeconds = NowSeconds;
		Event.Seat = Seat;
		Event.Value = Value;
	}

	/** Asiento que sigue a Seat en el orden de turno (R-TUR-1). */
	int32 NextInTurnOrder(const FMatchState& State, int32 Seat)
	{
		const int32 Index = State.TurnOrder.Find(Seat);
		return State.TurnOrder[(Index + 1) % State.TurnOrder.Num()];
	}

	/** Caso límite del GDD 02: los turnos perdidos se descuentan contra los extra, uno a uno; el resto se aplica. */
	void SettleExtraAgainstLost(FSeatState& Seat)
	{
		const int32 Cancelled = FMath::Min(Seat.ExtraTurns, Seat.LostTurns);
		Seat.ExtraTurns -= Cancelled;
		Seat.LostTurns -= Cancelled;
	}

	/** BetweenTurns → Start: salta los turnos perdidos (R-TUR-10) y empieza el turno del asiento al que le toca. */
	void EnterStart(FMatchState& State, double NowSeconds)
	{
		// Termina: cada salto descuenta un turno perdido, y los contadores son finitos.
		while (true)
		{
			FSeatState& Seat = State.Seats[State.ActiveSeat];
			SettleExtraAgainstLost(Seat);
			if (Seat.LostTurns == 0)
			{
				break;
			}
			// El anuncio del salto es del Warden; el motor deja el evento.
			--Seat.LostTurns;
			LogEvent(State, EMatchEventType::TurnSkipped, NowSeconds, State.ActiveSeat);
			State.ActiveSeat = NextInTurnOrder(State, State.ActiveSeat);
		}

		// TODO(R-TUR-3): salida del Vacío al inicio del turno, en la story de castigos y Vacío.
		State.TurnPhase = ETurnPhase::Start;
		State.TurnDeadlineSeconds = NowSeconds + State.Params.TurnClockSeconds;
		State.Roll = 0;
		State.ReachableOptions.Reset();
		LogEvent(State, EMatchEventType::TurnStarted, NowSeconds, State.ActiveSeat);
	}

	/** PreRoll → RollAndChoose: tirada y casillas alcanzables, un solo paso (R-TUR-5, R-TUR-7). */
	void EnterRollAndChoose(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		// TODO(R-TUR-4): con Celeridad el jugador elige uno o dos dados en PreRoll; hoy siempre el valor por defecto.
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.Roll = Soulbound::Rules::RollDice(Random, Soulbound::Dice::DefaultCount);
		// R-TAB-21: los rasgos del asiento salen del estado con la misma función que puede usar cualquiera (MakeMoverTraits).
		const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(State, State.ActiveSeat);
		State.ReachableOptions = Soulbound::Rules::ReachableTiles(State.Board, State.Seats[State.ActiveSeat].TileId, State.Roll, Mover);
		LogEvent(State, EMatchEventType::Rolled, NowSeconds, State.ActiveSeat, State.Roll);
	}

	void EnterEnd(FMatchState& State, double NowSeconds)
	{
		State.TurnPhase = ETurnPhase::End;
		++State.TurnCount;
		LogEvent(State, EMatchEventType::TurnEnded, NowSeconds, State.ActiveSeat);
	}

	/**
	 * R-CAR-1, R-TAB-27: agotado el tiempo de la carrera, termina al acabar el turno en curso. Se mira al entrar en
	 * BetweenTurns y antes de empezar un turno, nunca a mitad de uno. Devuelve true si la carrera terminó.
	 */
	bool EndBondRaceIfTimeIsUp(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		// TODO(R-TAB-26): el fin de carrera como interrupción del turno en curso es de la story del Warden.
		return State.MatchPhase == EMatchPhase::BondRace && NowSeconds >= State.Params.RaceDurationSeconds
			&& Soulbound::Rules::EndBondRace(State, NowSeconds, Random);
	}

	/** End → BetweenTurns: turno extra del mismo asiento (R-TUR-9) o siguiente del orden (R-TUR-1). */
	void LeaveEnd(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		FSeatState& Seat = State.Seats[State.ActiveSeat];
		SettleExtraAgainstLost(Seat);
		if (Seat.ExtraTurns > 0)
		{
			--Seat.ExtraTurns;
		}
		else
		{
			State.ActiveSeat = NextInTurnOrder(State, State.ActiveSeat);
		}
		State.TurnPhase = ETurnPhase::BetweenTurns;
		State.Roll = 0;
		State.ReachableOptions.Reset();
		// R-TUR-9 antes que R-CAR-1: el fin de carrera se mira al acabar cada turno, extra o no. Un turno extra concedido
		// y empezado antes del límite se juega entero, y la carrera termina al acabarlo. Si el turno que acaba aquí ya
		// pasó el límite y deja un turno extra pendiente, la carrera termina ahora y ese turno extra se juega después,
		// ya fuera de la carrera.
		EndBondRaceIfTimeIsUp(State, NowSeconds, Random);
	}

	/** Si la partida tiene lo que la máquina de turnos necesita: orden de turno sobre asientos existentes y turno activo en él. */
	bool HasTurnOrder(const FMatchState& State)
	{
		return State.TurnOrder.Num() == State.Seats.Num() && State.Seats.Num() > 0 && State.TurnOrder.Contains(State.ActiveSeat);
	}

	/** Suma Count al contador sin pasar del tope. Devuelve false, sin cambiar nada, si el asiento o Count no valen. */
	bool GrantCapped(FMatchState& State, int32 Seat, int32 Count, int32 FSeatState::*Counter, int32 Cap)
	{
		if (!State.Seats.IsValidIndex(Seat) || Count <= 0)
		{
			return false;
		}
		int32& Value = State.Seats[Seat].*Counter;
		Value = FMath::Clamp(Value + Count, 0, FMath::Max(Cap, 0));
		return true;
	}
}

namespace Soulbound::Rules
{
	int32 RollDice(FRandomStream& Random, int32 DiceCount)
	{
		int32 Sum = 0;
		for (int32 Die = 0; Die < DiceCount; ++Die)
		{
			Sum += Random.RandRange(1, Soulbound::Dice::Faces);
		}
		return Sum;
	}

	bool AdvanceTurn(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		// R-CAR-8: con la partida terminada, la máquina de turnos no avanza más.
		if (!HasTurnOrder(State) || State.IsMatchOver())
		{
			return false;
		}
		switch (State.TurnPhase)
		{
		case ETurnPhase::BetweenTurns:
			// Si el tiempo de carrera se agotó sin turno en curso, termina antes de empezar el siguiente.
			if (!EndBondRaceIfTimeIsUp(State, NowSeconds, Random))
			{
				EnterStart(State, NowSeconds);
			}
			return true;
		case ETurnPhase::Start:
			State.TurnPhase = ETurnPhase::PreRoll;
			return true;
		case ETurnPhase::PreRoll:
			EnterRollAndChoose(State, NowSeconds, Random);
			return true;
		case ETurnPhase::RollAndChoose:
			// R-TAB-18: sin casillas alcanzables el jugador no se mueve; no hay nada que confirmar, así que el turno
			// sigue sin esperar al reloj ni castigar (decisión del autor, 28/09/2026).
			if (State.ReachableOptions.IsEmpty())
			{
				State.TurnPhase = ETurnPhase::MoveAndResolve;
				return true;
			}
			// R-TUR-8: agotado el reloj antes de confirmar destino, el turno termina sin mover y sube la insolencia.
			if (NowSeconds >= State.TurnDeadlineSeconds)
			{
				LogEvent(State, EMatchEventType::TurnClockExpired, NowSeconds, State.ActiveSeat);
				RaiseInsolence(State, State.ActiveSeat);
				EnterEnd(State, NowSeconds);
				return true;
			}
			return false;
		case ETurnPhase::MoveAndResolve:
			// R-CAR-4: el Nicho se resuelve aquí. Sin movimiento (R-TAB-18) el asiento sigue donde terminó el anterior,
			// que nunca es un Nicho Libre para un Eco: lo habría reclamado al llegar.
			Soulbound::Rules::ResolveNicheLanding(State, State.ActiveSeat, NowSeconds, Random);
			// TODO(R-TUR-2): resolución de Fuentes y Nodos, en las stories de runas y de Nodos.
			EnterEnd(State, NowSeconds);
			return true;
		case ETurnPhase::End:
			LeaveEnd(State, NowSeconds, Random);
			return true;
		}
		return false;
	}

	EIntentResult SubmitDestinationIntent(FMatchState& State, int32 Seat, int32 TileId, double NowSeconds)
	{
		// Glosario "Intención": asiento correcto, fase correcta, casilla alcanzable; si no, se descarta sin castigo.
		if (!HasTurnOrder(State) || Seat != State.ActiveSeat)
		{
			return EIntentResult::WrongSeat;
		}
		if (State.TurnPhase != ETurnPhase::RollAndChoose)
		{
			return EIntentResult::WrongPhase;
		}
		if (NowSeconds >= State.TurnDeadlineSeconds)
		{
			return EIntentResult::ClockExpired;
		}
		// TODO(R-TUR-11): si el Warden sella una casilla entre la tirada y la confirmación, las opciones calculadas
		// al tirar siguen admitiéndola. Las interrupciones y su efecto sobre el turno son de la story del Warden.
		if (!State.ReachableOptions.Contains(TileId))
		{
			return EIntentResult::Unreachable;
		}
		State.Seats[Seat].TileId = TileId;
		State.TurnPhase = ETurnPhase::MoveAndResolve;
		return EIntentResult::Accepted;
	}

	bool GrantExtraTurns(FMatchState& State, int32 Seat, int32 Count)
	{
		return GrantCapped(State, Seat, Count, &FSeatState::ExtraTurns, State.Params.MaxExtraTurns);
	}

	bool GrantLostTurns(FMatchState& State, int32 Seat, int32 Count)
	{
		return GrantCapped(State, Seat, Count, &FSeatState::LostTurns, State.Params.MaxLostTurns);
	}

	bool RaiseInsolence(FMatchState& State, int32 Seat)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return false;
		}
		// TODO(R-TUR-8): cada subida trae un castigo sorteado por nivel y al quinto la Furia, que devuelve el contador
		// a 0. Es de la story del Warden; aquí solo el contador y su tope.
		int32& Insolence = State.Seats[Seat].Insolence;
		Insolence = FMath::Min(Insolence + 1, Soulbound::Match::MaxInsolence);
		return true;
	}
}
