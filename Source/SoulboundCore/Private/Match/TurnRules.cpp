#include "Match/TurnRules.h"

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/RuneRules.h"
#include "Match/MatchEvents.h"
#include "Math/RandomStream.h"

using Soulbound::Rules::LogEvent;

namespace
{
	/** Valores de los dados de una tirada: siempre Dice::MaxCount. */
	using FDiceDraw = TArray<int32, TInlineAllocator<Soulbound::Dice::MaxCount>>;

	/**
	 * Saca del sorteo de la partida los valores de Dice::MaxCount dados, se tiren uno o dos: así una tirada consume
	 * siempre lo mismo y los dados no dependen de lo que elija cada decisor (decisión del autor del 04/10/2026).
	 */
	FDiceDraw DrawDice(FRandomStream& Random)
	{
		FDiceDraw Dice;
		for (int32 Die = 0; Die < Soulbound::Dice::MaxCount; ++Die)
		{
			Dice.Add(Random.RandRange(1, Soulbound::Dice::Faces));
		}
		return Dice;
	}

	/** Suma de los primeros Count dados de Dice (R-TUR-5: el resultado es la suma). */
	int32 SumDice(const FDiceDraw& Dice, int32 Count)
	{
		int32 Sum = 0;
		for (int32 Die = 0; Die < FMath::Min(Count, Dice.Num()); ++Die)
		{
			Sum += Dice[Die];
		}
		return Sum;
	}

	/** Casillas alcanzables por el asiento activo con la tirada del turno (R-TUR-7). */
	void ComputeReachable(FMatchState& State)
	{
		// R-TAB-21: los rasgos del asiento salen del estado con la misma función que puede usar cualquiera (MakeMoverTraits).
		const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(State, State.ActiveSeat);
		State.ReachableOptions = Soulbound::Rules::ReachableTiles(State.Board, State.Seats[State.ActiveSeat].TileId, State.Roll, Mover);
	}

	void OpenChoice(FMatchState& State, EChoiceKind Kind, TArray<int32> Options, double DeadlineSeconds)
	{
		State.PendingChoice.Kind = Kind;
		State.PendingChoice.Options = MoveTemp(Options);
		State.PendingChoice.DeadlineSeconds = DeadlineSeconds;
	}

	void CloseChoice(FMatchState& State)
	{
		State.PendingChoice = FPendingChoice();
	}

	/**
	 * Saca del sorteo los dos valores de la repetición del turno si siguen pendientes, y los descarta. Se llama al cerrarse
	 * la oferta sin repetir (o justo tras la tirada si no hay oferta): todo turno que llega a PreRoll consume cuatro
	 * valores, dos de la tirada y dos de la repetición (decisión del autor del 04/10/2026).
	 */
	void SettleRerollValues(FMatchState& State, FRandomStream& Random)
	{
		if (State.bRerollValuesPending)
		{
			DrawDice(Random);
			State.bRerollValuesPending = false;
		}
	}

	/** Fase en la que se contesta cada tipo de elección. */
	ETurnPhase PhaseOf(EChoiceKind Kind)
	{
		switch (Kind)
		{
		case EChoiceKind::DiceCount: return ETurnPhase::PreRoll;
		case EChoiceKind::Reroll: return ETurnPhase::RollAndChoose;
		case EChoiceKind::PlanarJump:
		case EChoiceKind::Expel: return ETurnPhase::MoveAndResolve;
		case EChoiceKind::None:
		default: return ETurnPhase::BetweenTurns;
		}
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
		State.DiceCount = 0;
		State.Roll = 0;
		State.ReachableOptions.Reset();
		CloseChoice(State);
		State.bRerollValuesPending = false;
		// R-RUN-3: el turno que empieza aún no ha movido la ficha (los turnos extra también empiezan así).
		State.bActiveSeatMoved = false;
		LogEvent(State, EMatchEventType::TurnStarted, NowSeconds, State.ActiveSeat);
	}

	/**
	 * Start → PreRoll (R-TUR-4): con Celeridad, el asiento elige uno o dos dados dentro del reloj de turno; sin ella, uno
	 * sin elección. Celeridad se mira al empezar el turno: quien pierde su última runa tira un dado en el siguiente.
	 */
	void EnterPreRoll(FMatchState& State)
	{
		State.TurnPhase = ETurnPhase::PreRoll;
		if (Soulbound::Rules::IsPowerActive(State, State.ActiveSeat, EPower::Haste))
		{
			OpenChoice(State, EChoiceKind::DiceCount, {1, Soulbound::Dice::MaxCount}, State.TurnDeadlineSeconds);
		}
		else
		{
			State.DiceCount = Soulbound::Dice::DefaultCount;
		}
	}

	/**
	 * PreRoll → RollAndChoose: tirada y casillas alcanzables, un solo paso (R-TUR-5, R-TUR-7). Con la afinidad de Apep,
	 * se abre la oferta de repetir, dentro del reloj de turno (R-TUR-6); sin ella, se sacan ya los valores de la repetición.
	 */
	void EnterRollAndChoose(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.Roll = SumDice(DrawDice(Random), State.DiceCount);
		ComputeReachable(State);
		LogEvent(State, EMatchEventType::Rolled, NowSeconds, State.ActiveSeat, State.Roll);
		State.bRerollValuesPending = true;
		if (Soulbound::Rules::HasAffinity(State, State.ActiveSeat, EAffinity::RerollOnce))
		{
			OpenChoice(State, EChoiceKind::Reroll, {1}, State.TurnDeadlineSeconds);
		}
		else
		{
			SettleRerollValues(State, Random);
		}
	}

	void EnterEnd(FMatchState& State, double NowSeconds)
	{
		CloseChoice(State);
		State.TurnPhase = ETurnPhase::End;
		++State.TurnCount;
		LogEvent(State, EMatchEventType::TurnEnded, NowSeconds, State.ActiveSeat);
	}

	/**
	 * Lo que sigue al efecto de la casilla en MoveAndResolve (R-INT-1), desde el paso Step: el salto planar (PlanarJump),
	 * después como mucho una acción contra un jugador (Expel; en S-009 la única) y después el fin del turno. Abre la
	 * primera elección que tenga opciones, con el reloj de elección; si no queda ninguna, termina el turno.
	 */
	void OfferFrom(FMatchState& State, double NowSeconds, EChoiceKind Step)
	{
		const int32 Seat = State.ActiveSeat;
		const double Deadline = NowSeconds + State.Params.ChoiceClockSeconds;
		if (Step == EChoiceKind::PlanarJump)
		{
			TArray<int32> Jumps = Soulbound::Rules::PlanarJumpOptions(State, Seat);
			if (!Jumps.IsEmpty())
			{
				OpenChoice(State, EChoiceKind::PlanarJump, MoveTemp(Jumps), Deadline);
				return;
			}
			Step = EChoiceKind::Expel;
		}
		if (Step == EChoiceKind::Expel)
		{
			TArray<int32> Targets = Soulbound::Rules::ExpelTargets(State, Seat);
			if (!Targets.IsEmpty())
			{
				OpenChoice(State, EChoiceKind::Expel, MoveTemp(Targets), Deadline);
				return;
			}
		}
		EnterEnd(State, NowSeconds);
	}

	/** El paso que sigue a cerrar una elección de MoveAndResolve: tras el salto, la acción; tras la acción, el fin. */
	EChoiceKind StepAfter(EChoiceKind Closed)
	{
		return Closed == EChoiceKind::PlanarJump ? EChoiceKind::Expel : EChoiceKind::None;
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
		State.DiceCount = 0;
		State.Roll = 0;
		State.ReachableOptions.Reset();
		// R-TUR-9 antes que R-CAR-1: el fin de carrera se mira al acabar cada turno, extra o no. Un turno extra concedido
		// y empezado antes del límite se juega entero, y la carrera termina al acabarlo. Si el turno que acaba aquí ya
		// pasó el límite y deja un turno extra pendiente, la carrera termina ahora y ese turno extra se juega después,
		// ya fuera de la carrera y con sus condiciones (confinamiento de los Ecos): decisión del autor del 01/10/2026.
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
			EnterPreRoll(State);
			return true;
		case ETurnPhase::PreRoll:
			if (State.PendingChoice.Kind == EChoiceKind::DiceCount)
			{
				// R-TUR-8: la elección de dados cuenta dentro del reloj de turno; agotado, el turno termina sin tirar ni
				// mover y sube la insolencia. Consume los cuatro valores igualmente (decisión del autor del 04/10/2026).
				if (NowSeconds >= State.TurnDeadlineSeconds)
				{
					DrawDice(Random);
					DrawDice(Random);
					CloseChoice(State);
					LogEvent(State, EMatchEventType::TurnClockExpired, NowSeconds, State.ActiveSeat);
					RaiseInsolence(State, State.ActiveSeat);
					EnterEnd(State, NowSeconds);
					return true;
				}
				return false;
			}
			EnterRollAndChoose(State, NowSeconds, Random);
			return true;
		case ETurnPhase::RollAndChoose:
		{
			const bool bRerollOffered = State.PendingChoice.Kind == EChoiceKind::Reroll;
			// R-TAB-18: sin casillas alcanzables el jugador no se mueve; no hay nada que confirmar, así que el turno
			// sigue sin esperar al reloj ni castigar (decisión del autor, 28/09/2026). Con la oferta de repetir abierta,
			// se espera dentro del reloj de turno; agotado, es un "no" sin castigo (decisión del autor del 04/10/2026).
			if (State.ReachableOptions.IsEmpty())
			{
				if (bRerollOffered)
				{
					if (NowSeconds < State.TurnDeadlineSeconds)
					{
						return false;
					}
					LogEvent(State, EMatchEventType::ChoiceExpired, NowSeconds, State.ActiveSeat, static_cast<int32>(EChoiceKind::Reroll));
					CloseChoice(State);
					SettleRerollValues(State, Random);
				}
				State.TurnPhase = ETurnPhase::MoveAndResolve;
				return true;
			}
			// R-TUR-8: agotado el reloj antes de confirmar destino, el turno termina sin mover y sube la insolencia.
			if (NowSeconds >= State.TurnDeadlineSeconds)
			{
				CloseChoice(State);
				SettleRerollValues(State, Random);
				LogEvent(State, EMatchEventType::TurnClockExpired, NowSeconds, State.ActiveSeat);
				RaiseInsolence(State, State.ActiveSeat);
				EnterEnd(State, NowSeconds);
				return true;
			}
			return false;
		}
		case ETurnPhase::MoveAndResolve:
			// Salto planar o acción contra jugador pendientes: se espera su intención; agotado el reloj de elección, es un
			// "no" sin castigo, se registra y el turno sigue (decisión del autor del 04/10/2026).
			if (State.PendingChoice.IsOpen())
			{
				if (NowSeconds < State.PendingChoice.DeadlineSeconds)
				{
					return false;
				}
				const EChoiceKind Expired = State.PendingChoice.Kind;
				LogEvent(State, EMatchEventType::ChoiceExpired, NowSeconds, State.ActiveSeat, static_cast<int32>(Expired));
				CloseChoice(State);
				OfferFrom(State, NowSeconds, StepAfter(Expired));
				return true;
			}
			// Si Apep confirmó destino sin contestar la oferta de repetir, sus dos valores se sacan aquí.
			SettleRerollValues(State, Random);
			// R-CAR-4: el Nicho se resuelve aquí. Sin movimiento (R-TAB-18) el asiento sigue donde terminó el anterior,
			// que nunca es un Nicho Libre para un Eco: lo habría reclamado al llegar.
			Soulbound::Rules::ResolveNicheLanding(State, State.ActiveSeat, NowSeconds, Random);
			// R-RUN-2: la Fuente se resuelve junto al Nicho, solo si el asiento se ha movido (R-RUN-3). Si un Elegido debe
			// jugar el minijuego, la máquina espera su resultado como espera el destino (SubmitSkillTrialResult).
			if (Soulbound::Rules::ResolvePowerSourceLanding(State, State.ActiveSeat, NowSeconds) == ERuneLanding::TrialPending)
			{
				State.TurnPhase = ETurnPhase::SkillTrial;
				return true;
			}
			// TODO(R-TUR-2): Nodos de Duelo y del Oráculo, en S-010 y en la story del Warden.
			// R-INT-1: después de la casilla, el salto planar y, como mucho, una acción contra un jugador.
			OfferFrom(State, NowSeconds, EChoiceKind::PlanarJump);
			return true;
		case ETurnPhase::SkillTrial:
			// TODO(R-MIN): el minijuego espera sin plazo; su tiempo propio llega con su story.
			return false;
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
		// R-TUR-6: confirmar destino cierra la oferta de repetir; sus valores del sorteo se sacan en MoveAndResolve.
		if (State.PendingChoice.Kind == EChoiceKind::Reroll)
		{
			CloseChoice(State);
		}
		State.Seats[Seat].TileId = TileId;
		State.bActiveSeatMoved = true;
		LogEvent(State, EMatchEventType::Moved, NowSeconds, Seat, TileId);
		State.TurnPhase = ETurnPhase::MoveAndResolve;
		return EIntentResult::Accepted;
	}

	EIntentResult SubmitSkillTrialResult(FMatchState& State, int32 Seat, bool bPassed, double NowSeconds)
	{
		if (!HasTurnOrder(State) || Seat != State.ActiveSeat)
		{
			return EIntentResult::WrongSeat;
		}
		if (State.TurnPhase != ETurnPhase::SkillTrial || State.PendingTrial == ESkillTrial::None)
		{
			return EIntentResult::WrongPhase;
		}
		// TODO(R-MIN): sin plazo en S-008; el minijuego real tendrá su propio tiempo y su propio fallo por agotarlo.
		Soulbound::Rules::ApplySkillTrialResult(State, Seat, bPassed, NowSeconds);
		// R-INT-1 (S-009): tras la casilla y su minijuego, el salto planar y la acción; sin ellos, el turno termina.
		State.TurnPhase = ETurnPhase::MoveAndResolve;
		OfferFrom(State, NowSeconds, EChoiceKind::PlanarJump);
		return EIntentResult::Accepted;
	}

	EIntentResult SubmitChoiceIntent(FMatchState& State, int32 Seat, EChoiceKind Kind, int32 Option, double NowSeconds, FRandomStream& Random)
	{
		// Glosario "Intención": asiento, tipo de elección, plazo y opción; si no vale, se descarta sin castigo.
		if (!HasTurnOrder(State) || Seat != State.ActiveSeat)
		{
			return EIntentResult::WrongSeat;
		}
		const FPendingChoice& Choice = State.PendingChoice;
		if (!Choice.IsOpen() || Choice.Kind != Kind || State.TurnPhase != PhaseOf(Kind))
		{
			return EIntentResult::WrongPhase;
		}
		if (NowSeconds >= Choice.DeadlineSeconds)
		{
			return EIntentResult::ClockExpired;
		}
		if (Option == INDEX_NONE ? !Choice.CanDecline() : !Choice.Options.Contains(Option))
		{
			return EIntentResult::InvalidOption;
		}

		CloseChoice(State);
		switch (Kind)
		{
		case EChoiceKind::DiceCount:
			// R-TUR-4: los dados del turno; la tirada la hace AdvanceTurn al salir de PreRoll.
			State.DiceCount = Option;
			LogEvent(State, EMatchEventType::DiceChosen, NowSeconds, Seat, Option);
			break;
		case EChoiceKind::Reroll:
			// R-TUR-6: la tirada nueva sustituye a la anterior, con los mismos dados, y las alcanzables se recalculan. El
			// reloj de turno sigue corriendo. Con "no", los valores de la repetición se sacan y se descartan.
			if (Option == INDEX_NONE)
			{
				SettleRerollValues(State, Random);
				break;
			}
			State.Roll = SumDice(DrawDice(Random), State.DiceCount);
			State.bRerollValuesPending = false;
			ComputeReachable(State);
			LogEvent(State, EMatchEventType::Rerolled, NowSeconds, Seat, State.Roll);
			break;
		case EChoiceKind::PlanarJump:
			// R-POD-5: un salto por movimiento; después, la acción contra un jugador en el destino (R-INT-1).
			if (Option != INDEX_NONE)
			{
				ApplyPlanarJump(State, Seat, Option, NowSeconds);
			}
			OfferFrom(State, NowSeconds, StepAfter(Kind));
			break;
		case EChoiceKind::Expel:
			// R-POD-4, R-INT-2: a uno o a ninguno; después, el fin del turno.
			if (Option != INDEX_NONE)
			{
				ApplyExpel(State, Seat, Option, NowSeconds);
			}
			OfferFrom(State, NowSeconds, StepAfter(Kind));
			break;
		case EChoiceKind::None:
			break;
		}
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
