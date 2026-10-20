#include "Match/TurnRules.h"

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/DuelRules.h"
#include "Match/EffectRules.h"
#include "Match/InsolenceRules.h"
#include "Match/RuneRules.h"
#include "Match/MatchEvents.h"
#include "Match/ShellGameRules.h"
#include "Match/VoidRules.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Warden/WardenRules.h"

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

	/**
	 * Abre una elección pendiente para el asiento Seat (S-010: el que la contesta; INDEX_NONE, el activo) con sus opciones
	 * y su plazo. Las de la acción contra un jugador van aparte, en ActionOptions.
	 */
	void OpenChoice(FMatchState& State, EChoiceKind Kind, TArray<int32> Options, double DeadlineSeconds, int32 Seat = INDEX_NONE)
	{
		State.PendingChoice = FPendingChoice();
		State.PendingChoice.Kind = Kind;
		State.PendingChoice.Seat = Seat == INDEX_NONE ? State.ActiveSeat : Seat;
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
		case EChoiceKind::Deflection: return ETurnPhase::Start;
		case EChoiceKind::DiceCount: return ETurnPhase::PreRoll;
		case EChoiceKind::Reroll: return ETurnPhase::RollAndChoose;
		case EChoiceKind::PlanarJump:
		case EChoiceKind::PlayerAction: return ETurnPhase::MoveAndResolve;
		case EChoiceKind::SafeRune:
		case EChoiceKind::ShellPick: return ETurnPhase::ShellGame;
		// S-014: las de los eventos del Warden, que se resuelven entre turnos.
		case EChoiceKind::Dilemma:
		case EChoiceKind::ChosenRival:
		case EChoiceKind::GiftRune:
		case EChoiceKind::LoseRune: return ETurnPhase::BetweenTurns;
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

	/**
	 * El turno pasa al siguiente del orden (R-TUR-1). Si el orden da la vuelta (llega a su primer asiento, juegue o se salte su
	 * turno), empieza una ronda (decisión del autor del 10/10/2026, S-014): caducan marcas, Brazales revelados y la Noche.
	 */
	void MoveToNextSeat(FMatchState& State, double NowSeconds)
	{
		State.ActiveSeat = NextInTurnOrder(State, State.ActiveSeat);
		if (State.ActiveSeat == State.TurnOrder[0])
		{
			Soulbound::Rules::StartRound(State, NowSeconds);
		}
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
		// S-011: una edición de depuración pidió el asiento de este turno; el orden sigue desde él. Un turno extra pendiente
		// del asiento anterior ya se consumió al terminar su turno y se pierde. Los turnos perdidos del pedido se respetan.
		if (State.ForcedNextSeat != INDEX_NONE)
		{
			if (State.TurnOrder.Contains(State.ForcedNextSeat))
			{
				State.ActiveSeat = State.ForcedNextSeat;
			}
			State.ForcedNextSeat = INDEX_NONE;
		}
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
			MoveToNextSeat(State, NowSeconds);
		}

		// R-TUR-3: la salida del Vacío se decide al pasar de Start a PreRoll (EnterVoidTurn). Un turno saltado arriba no es
		// un turno empezado en el Vacío y no sube su cuenta (decisión del autor del 08/10/2026).
		State.TurnPhase = ETurnPhase::Start;
		State.TurnDeadlineSeconds = NowSeconds + State.Params.TurnClockSeconds;
		State.DiceCount = 0;
		State.Roll = 0;
		State.ReachableOptions.Reset();
		CloseChoice(State);
		State.bRerollValuesPending = false;
		// R-RUN-3: el turno que empieza aún no ha movido la ficha (los turnos extra también empiezan así).
		State.bActiveSeatMoved = false;
		// S-014: "el último que jugó", el activo de un evento del Warden que llegue entre turnos (R-DIR-1).
		State.LastTurnSeat = State.ActiveSeat;
		// S-016 (R-WAR-4): el hueco entre turnos que acaba aquí ya no puede tener otra intervención; el siguiente, sí.
		State.Warden.bGapUsed = false;
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
		// S-011: con una tirada fijada por una edición de depuración, el sorteo se consume igual (DrawDice) y solo cambia la suma.
		const int32 Drawn = SumDice(DrawDice(Random), State.DiceCount);
		State.Roll = State.ForcedRoll > 0 ? State.ForcedRoll : Drawn;
		State.ForcedRoll = 0;
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
	 * Acaba un duelo o un trilero (S-015). Si lo jugaba un evento del Warden (bWaitsForAction), la máquina vuelve a
	 * BetweenTurns y el evento sigue en la siguiente llamada, sin terminar ningún turno; si no, termina el turno del activo.
	 */
	void FinishAction(FMatchState& State, double NowSeconds)
	{
		FCurrentWardenEvent& Event = State.CurrentEvent;
		if (Event.IsActive() && Event.bWaitsForAction)
		{
			CloseChoice(State);
			Event.bWaitsForAction = false;
			State.TurnPhase = ETurnPhase::BetweenTurns;
			// R-INS-2 (S-016): el castigo de quien desatendió y acabó en este duelo o trilero, ahora que ya no está en juego.
			Soulbound::Rules::DischargePendingPunishments(State, NowSeconds);
			return;
		}
		EnterEnd(State, NowSeconds);
	}

	/**
	 * Start → PreRoll o Prueba del Warden (R-EFE-10): quien la tiene pendiente la juega antes de tirar, con la dificultad de
	 * los datos y sin plazo (TODO(R-MIN), como el resto de minijuegos). Se llama después de la salida del Vacío, que la anula
	 * al entrar, así que solo la ve quien juega el turno.
	 */
	void EnterAfterStart(FMatchState& State)
	{
		if (State.Seats[State.ActiveSeat].bWardenTrialPending)
		{
			State.PendingTrial = ESkillTrial::WardenTrial;
			State.PendingDifficulty = State.Params.WardenTrialDifficulty;
			State.TurnPhase = ETurnPhase::SkillTrial;
			return;
		}
		EnterPreRoll(State);
	}

	/**
	 * El paso de la escalera de escape del turno en curso (R-VAC-2), después de Eco y Desvío: el minijuego con la dificultad
	 * del peldaño (fase SkillTrial) o, pasado el último, la salida sin jugar (R-VAC-3), que termina el turno.
	 */
	void ContinueVoidEscape(FMatchState& State, double NowSeconds)
	{
		const int32 Seat = State.ActiveSeat;
		const TArray<ESkillDifficulty>& Ladder = Soulbound::Rules::VoidEscapeLadderFor(State, Seat);
		const int32 Turn = State.Seats[Seat].VoidTurns;
		if (Ladder.IsValidIndex(Turn - 1))
		{
			// TODO(R-MIN): el escape espera sin plazo, como el minijuego de la Fuente; su tiempo llega con la story del minijuego.
			State.PendingTrial = ESkillTrial::VoidEscape;
			State.PendingDifficulty = Ladder[Turn - 1];
			State.TurnPhase = ETurnPhase::SkillTrial;
			return;
		}
		Soulbound::Rules::LeaveVoid(State, Seat, EVoidExit::TurnLimit, NowSeconds);
		EnterEnd(State, NowSeconds);
	}

	/**
	 * Start de un asiento en el Vacío (R-TUR-3, R-VAC-2), antes de nada y en este orden: su cuenta sube uno; un Eco sale sin
	 * jugar (R-ECO-5), salvo en el Vacío de la Furia (S-016); quien tiene Desvío elige si lo usa (R-POD-6), bajo el reloj de turno y también en el turno en que
	 * saldría solo (decisión del autor del 08/10/2026); si no, la escalera. Sin dados: nada de esto toca el sorteo.
	 */
	void EnterVoidTurn(FMatchState& State, double NowSeconds)
	{
		const int32 Seat = State.ActiveSeat;
		++State.Seats[Seat].VoidTurns;
		// R-INS-3 (decisión del autor del 20/10/2026): en el Vacío de la Furia no vale la salida del Eco; juega la escalera.
		if (State.Seats[Seat].Role == ESeatRole::LostEcho && !State.Seats[Seat].bFuryVoid)
		{
			Soulbound::Rules::LeaveVoid(State, Seat, EVoidExit::Echo, NowSeconds);
			EnterEnd(State, NowSeconds);
			return;
		}
		if (Soulbound::Rules::DeflectionRuneType(State, Seat) != INDEX_NONE)
		{
			OpenChoice(State, EChoiceKind::Deflection, {1}, State.TurnDeadlineSeconds);
			return;
		}
		ContinueVoidEscape(State, NowSeconds);
	}

	/**
	 * Si el asiento activo espera en su turno del Vacío (elección de Desvío o escape) pero ya no está dentro: lo sacó una
	 * Liberación (R-VAC-5) y la espera ya no contesta nada (decisión aceptada por el autor el 08/10/2026).
	 */
	bool IsVoidWaitObsolete(const FMatchState& State)
	{
		const bool bWaitsInVoid = (State.TurnPhase == ETurnPhase::Start && State.PendingChoice.Kind == EChoiceKind::Deflection)
			|| (State.TurnPhase == ETurnPhase::SkillTrial && State.PendingTrial == ESkillTrial::VoidEscape);
		return bWaitsInVoid && State.Seats.IsValidIndex(State.ActiveSeat) && !State.Seats[State.ActiveSeat].IsInVoid();
	}

	/**
	 * Lo que sigue al efecto de la casilla en MoveAndResolve (R-INT-1), desde el paso Step: el salto planar (PlanarJump),
	 * después como mucho una acción contra un jugador (PlayerAction: expulsar o robar, S-010) y después el fin del turno.
	 * Abre la primera elección que tenga opciones, con el reloj de elección; si no queda ninguna, termina el turno.
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
			Step = EChoiceKind::PlayerAction;
		}
		if (Step == EChoiceKind::PlayerAction)
		{
			// R-INT-2: qué y a quién, en una sola elección con un solo reloj; la intención contesta con el índice de la opción.
			TArray<FPlayerActionOption> Actions = Soulbound::Rules::PlayerActionOptions(State, Seat);
			if (!Actions.IsEmpty())
			{
				OpenChoice(State, EChoiceKind::PlayerAction, {}, Deadline);
				State.PendingChoice.ActionOptions = MoveTemp(Actions);
				return;
			}
		}
		EnterEnd(State, NowSeconds);
	}

	/**
	 * El asiento Thief roba al asiento Victim (R-ECO-4, R-ROB-1; desde S-012 también el ganador de un duelo, R-DUE-5, que
	 * puede no tener el turno): sin runas o con el robo anulado, termina el turno; si no, la máquina pasa al trilero (fase
	 * ShellGame), con la elección de Koschei abierta para la víctima si toca (R-ROB-5), con el reloj de elección. El
	 * trilero termina el turno del asiento activo o, si lo juega un evento del Warden (S-015), devuelve la máquina al evento.
	 */
	void BeginStealAction(FMatchState& State, int32 Thief, int32 Victim, double NowSeconds)
	{
		switch (Soulbound::Rules::BeginSteal(State, Thief, Victim, NowSeconds))
		{
		case EStealStart::NoRunes:
		case EStealStart::Cancelled:
			FinishAction(State, NowSeconds);
			break;
		case EStealStart::SafeRunePending:
			State.TurnPhase = ETurnPhase::ShellGame;
			// Elección de un asiento sin turno (S-010): la contesta Koschei, no el ladrón.
			OpenChoice(State, EChoiceKind::SafeRune, Soulbound::Rules::SafeRuneOptions(State), NowSeconds + State.Params.ChoiceClockSeconds, Victim);
			break;
		case EStealStart::Started:
			State.TurnPhase = ETurnPhase::ShellGame;
			break;
		}
	}

	/**
	 * Avanza el trilero en curso (fase ShellGame, R-ROB-2) si su etapa ha vencido: la runa de Koschei sin elegir (se juega
	 * con todas), el fin de la muestra, el fin de la mezcla (abre la elección de esfera para el ladrón, con el tiempo de
	 * elección del trilero) o la esfera sin elegir (no roba y termina el turno). Devuelve false si todavía espera.
	 */
	bool AdvanceShellGame(FMatchState& State, double NowSeconds)
	{
		const FShellGameState& Game = State.ShellGame;
		switch (Game.Stage)
		{
		case EShellGameStage::SafeRune:
			if (NowSeconds < State.PendingChoice.DeadlineSeconds)
			{
				return false;
			}
			// Koschei no elige a tiempo: no aparta ninguna (decisión del autor del 05/10/2026) y queda registrado.
			LogEvent(State, EMatchEventType::ChoiceExpired, NowSeconds, State.PendingChoice.Seat, static_cast<int32>(EChoiceKind::SafeRune));
			CloseChoice(State);
			Soulbound::Rules::SetAsideSafeRune(State, INDEX_NONE, NowSeconds);
			return true;
		case EShellGameStage::Reveal:
			if (NowSeconds < Game.StageDeadlineSeconds)
			{
				return false;
			}
			Soulbound::Rules::StartShellShuffle(State, NowSeconds);
			return true;
		case EShellGameStage::Shuffle:
		{
			if (NowSeconds < Game.StageDeadlineSeconds)
			{
				return false;
			}
			TArray<int32> Positions;
			for (int32 Position = 0; Position < Game.Orbs.Num(); ++Position)
			{
				Positions.Add(Position);
			}
			const int32 Thief = Game.Thief;
			Soulbound::Rules::StartShellPick(State);
			OpenChoice(State, EChoiceKind::ShellPick, MoveTemp(Positions), NowSeconds + State.Params.ShellPickSeconds, Thief);
			return true;
		}
		case EShellGameStage::Pick:
			if (NowSeconds < State.PendingChoice.DeadlineSeconds)
			{
				return false;
			}
			CloseChoice(State);
			Soulbound::Rules::ExpireShellPick(State, NowSeconds);
			FinishAction(State, NowSeconds);
			return true;
		case EShellGameStage::None:
		default:
			// Fase ShellGame sin trilero: no debería pasar; el turno sigue para no quedarse esperando.
			FinishAction(State, NowSeconds);
			return true;
		}
	}

	/**
	 * El asiento activo reta al asiento Challenged (R-POD-3, R-DUE-2): sin runas el retado, la acción se pierde y el turno
	 * termina (R-DUE-4); si no, la máquina pasa a la fase Duel y espera los dos rendimientos (R-DUE-3: el retado no elige).
	 */
	void BeginDuelAction(FMatchState& State, int32 Challenged, double NowSeconds)
	{
		if (Soulbound::Rules::BeginDuel(State, State.ActiveSeat, Challenged, NowSeconds) == EDuelStart::NoRunes)
		{
			EnterEnd(State, NowSeconds);
			return;
		}
		State.TurnPhase = ETurnPhase::Duel;
	}

	/** El paso que sigue a cerrar una elección de MoveAndResolve: tras el salto, la acción; tras la acción, el fin. */
	EChoiceKind StepAfter(EChoiceKind Closed)
	{
		return Closed == EChoiceKind::PlanarJump ? EChoiceKind::PlayerAction : EChoiceKind::None;
	}

	/**
	 * R-CAR-1, R-TAB-27: agotado el tiempo de la carrera, termina al acabar el turno en curso. Se mira al entrar en
	 * BetweenTurns y antes de empezar un turno, nunca a mitad de uno. Devuelve true si la carrera terminó.
	 */
	bool EndBondRaceIfTimeIsUp(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		// TODO(R-TAB-26): el fin de carrera que corta el turno en curso es un evento programado (de los que siempre cortan), y
		// llega con los eventos programados de S-017 (decisión del autor del 19/10/2026); hasta entonces la carrera termina al
		// acabar el turno en curso. El corte ya existe (InterruptTurn): faltará llamarlo desde el evento programado.
		return State.MatchPhase == EMatchPhase::BondRace && NowSeconds >= State.Params.RaceDurationSeconds
			&& Soulbound::Rules::EndBondRace(State, NowSeconds);
	}

	/** End → BetweenTurns: turno extra del mismo asiento (R-TUR-9) o siguiente del orden (R-TUR-1). */
	void LeaveEnd(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		FSeatState& Seat = State.Seats[State.ActiveSeat];
		SettleExtraAgainstLost(Seat);
		// Entre turnos antes de pasar al siguiente: lo que caduca con la ronda (S-014) se aplica con nadie en juego.
		State.TurnPhase = ETurnPhase::BetweenTurns;
		if (Seat.ExtraTurns > 0)
		{
			--Seat.ExtraTurns;
		}
		else
		{
			MoveToNextSeat(State, NowSeconds);
		}
		State.DiceCount = 0;
		State.Roll = 0;
		State.ReachableOptions.Reset();
		// R-INS-2 (S-016): el castigo del reloj de turno, y lo que esperaba a que el asiento dejara de estar en juego, ahora.
		Soulbound::Rules::DischargePendingPunishments(State, NowSeconds);
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
			// S-014: un evento del Warden en curso se resuelve entero antes de empezar el siguiente turno; mientras espera una
			// elección, la máquina espera, y el turno siguiente empieza en una llamada posterior.
			if (State.CurrentEvent.IsActive())
			{
				return Soulbound::Rules::AdvanceWardenEvent(State, NowSeconds);
			}
			// S-016 (R-WAR-4): una escena que el Warden decidió con un turno en curso entra en este hueco, antes del turno siguiente.
			if (State.Warden.PendingScene != INDEX_NONE)
			{
				const int32 Scene = State.Warden.PendingScene;
				State.Warden.PendingScene = INDEX_NONE;
				Soulbound::Rules::LaunchWardenEvent(State, Scene, false, NowSeconds);
				return true;
			}
			// Si el tiempo de carrera se agotó sin turno en curso, termina antes de empezar el siguiente.
			if (!EndBondRaceIfTimeIsUp(State, NowSeconds, Random))
			{
				EnterStart(State, NowSeconds);
			}
			return true;
		case ETurnPhase::Start:
			if (State.PendingChoice.Kind == EChoiceKind::Deflection)
			{
				// R-VAC-3: si una Liberación lo sacó mientras elegía, ya salió por ella y su turno termina sin tirar.
				if (IsVoidWaitObsolete(State))
				{
					EnterEnd(State, NowSeconds);
					return true;
				}
				// R-POD-6: agotar la elección de Desvío es "no", sin castigo; sigue la escalera.
				if (NowSeconds < State.PendingChoice.DeadlineSeconds)
				{
					return false;
				}
				LogEvent(State, EMatchEventType::ChoiceExpired, NowSeconds, State.ActiveSeat, static_cast<int32>(EChoiceKind::Deflection));
				CloseChoice(State);
				ContinueVoidEscape(State, NowSeconds);
				return true;
			}
			// R-TUR-3: quien empieza su turno en el Vacío decide su salida antes de nada.
			if (State.Seats[State.ActiveSeat].IsInVoid())
			{
				EnterVoidTurn(State, NowSeconds);
				return true;
			}
			// R-EFE-10: la Prueba del Warden pendiente, antes de tirar y después de la salida del Vacío.
			EnterAfterStart(State);
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
					RaiseInsolence(State, State.ActiveSeat, NowSeconds);
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
				RaiseInsolence(State, State.ActiveSeat, NowSeconds);
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
			// TODO(R-TUR-2): Nodo del Oráculo, en la story del Warden. El Nodo de Duelo entra en la acción contra un jugador (S-012).
			// R-INT-1: después de la casilla, el salto planar y, como mucho, una acción contra un jugador.
			OfferFrom(State, NowSeconds, EChoiceKind::PlanarJump);
			return true;
		case ETurnPhase::SkillTrial:
			// R-VAC-3: si una Liberación sacó del Vacío a quien jugaba el escape, la prueba ya no cuenta y su turno termina.
			if (IsVoidWaitObsolete(State))
			{
				State.PendingTrial = ESkillTrial::None;
				EnterEnd(State, NowSeconds);
				return true;
			}
			// TODO(R-MIN): el minijuego espera sin plazo; su tiempo propio llega con su story.
			return false;
		case ETurnPhase::Duel:
			// TODO(R-MIN): el duelo espera los dos rendimientos sin plazo (SubmitDuelPerformance); el tiempo del minijuego de
			// duelo, y qué pasa si un participante no responde, llegan con su story.
			return false;
		case ETurnPhase::ShellGame:
			// R-ROB-2: el trilero espera sus plazos (muestra, mezcla) y sus elecciones (runa de Koschei, esfera).
			return AdvanceShellGame(State, NowSeconds);
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
		// R-TUR-11 (S-016): el Warden no sella a mitad de turno sin cortarlo (un golpe corta; una escena espera al hueco), así
		// que las opciones calculadas al tirar siguen valiendo hasta confirmar. Solo SbSeal puede sellar en medio, y es depuración.
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
		if (State.TurnPhase != ETurnPhase::SkillTrial || State.PendingTrial == ESkillTrial::None || IsVoidWaitObsolete(State))
		{
			return EIntentResult::WrongPhase;
		}
		// TODO(R-MIN): sin plazo en S-008; el minijuego real tendrá su propio tiempo y su propio fallo por agotarlo.
		if (State.PendingTrial == ESkillTrial::WardenTrial)
		{
			// R-EFE-10: superada, el turno sigue con normalidad; fallada, termina sin tirada y sin consumir el sorteo.
			// TODO(R-EFE-10): el reloj de turno corre durante la Prueba; si debe reiniciarse al superarla es decisión del autor.
			if (Soulbound::Rules::ApplyWardenTrialResult(State, Seat, bPassed, NowSeconds))
			{
				EnterPreRoll(State);
			}
			else
			{
				EnterEnd(State, NowSeconds);
			}
			return EIntentResult::Accepted;
		}
		if (State.PendingTrial == ESkillTrial::VoidEscape)
		{
			// R-VAC-2, R-VAC-3: superado, a su hogar; fallado, sigue dentro. En los dos casos el turno termina sin tirar.
			Soulbound::Rules::ApplyVoidEscapeResult(State, Seat, bPassed, NowSeconds);
			EnterEnd(State, NowSeconds);
			return EIntentResult::Accepted;
		}
		Soulbound::Rules::ApplySkillTrialResult(State, Seat, bPassed, NowSeconds);
		// R-INT-1 (S-009): tras la casilla y su minijuego, el salto planar y la acción; sin ellos, el turno termina.
		State.TurnPhase = ETurnPhase::MoveAndResolve;
		OfferFrom(State, NowSeconds, EChoiceKind::PlanarJump);
		return EIntentResult::Accepted;
	}

	EIntentResult SubmitChoiceIntent(FMatchState& State, int32 Seat, EChoiceKind Kind, int32 Option, double NowSeconds, FRandomStream& Random)
	{
		// Glosario "Intención": asiento, tipo de elección, plazo y opción; si no vale, se descarta sin castigo. El asiento es
		// el de la elección abierta, que puede no tener el turno (S-010: Koschei víctima); sin elección, el activo.
		const FPendingChoice& Choice = State.PendingChoice;
		if (!HasTurnOrder(State) || Seat != (Choice.IsOpen() ? Choice.Seat : State.ActiveSeat))
		{
			return EIntentResult::WrongSeat;
		}
		if (!Choice.IsOpen() || Choice.Kind != Kind || State.TurnPhase != PhaseOf(Kind) || IsVoidWaitObsolete(State))
		{
			return EIntentResult::WrongPhase;
		}
		if (NowSeconds >= Choice.DeadlineSeconds)
		{
			return EIntentResult::ClockExpired;
		}
		if (Option == INDEX_NONE ? !Choice.CanDecline() : !Choice.IsValidAnswer(Option))
		{
			return EIntentResult::InvalidOption;
		}

		// Las opciones de la acción contra un jugador se leen después de cerrar la elección: se copian antes.
		const TArray<FPlayerActionOption> Actions = Choice.ActionOptions;
		CloseChoice(State);
		switch (Kind)
		{
		case EChoiceKind::Deflection:
			// R-POD-6: usarlo sacrifica la runa, sale al hogar y juega el turno entero (R-VAC-3); "no" sigue con la escalera, y en
			// el turno de salida eso es salir sin jugar.
			if (Option != INDEX_NONE && Soulbound::Rules::UseDeflection(State, Seat, NowSeconds))
			{
				EnterPreRoll(State);
				break;
			}
			ContinueVoidEscape(State, NowSeconds);
			break;
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
		case EChoiceKind::PlayerAction:
		{
			// R-INT-2: como mucho una acción, contra uno o contra ninguno. Expulsar (R-POD-4) y después el fin del turno; robar
			// (R-ECO-4) abre el trilero, que termina el turno al resolverse; retar (S-012) abre el duelo, con su botín.
			const FPlayerActionOption* Action = Option != INDEX_NONE ? &Actions[Option] : nullptr;
			if (Action && Action->Action == EPlayerAction::Steal)
			{
				BeginStealAction(State, Seat, Action->TargetSeat, NowSeconds);
				break;
			}
			if (Action && Action->Action == EPlayerAction::Duel)
			{
				BeginDuelAction(State, Action->TargetSeat, NowSeconds);
				break;
			}
			if (Action && Action->Action == EPlayerAction::Expel)
			{
				ApplyExpel(State, Seat, Action->TargetSeat, NowSeconds);
			}
			OfferFrom(State, NowSeconds, StepAfter(Kind));
			break;
		}
		case EChoiceKind::SafeRune:
			// R-ROB-5: Koschei aparta la runa y empieza la muestra.
			Soulbound::Rules::SetAsideSafeRune(State, Option, NowSeconds);
			break;
		case EChoiceKind::ShellPick:
			// R-ROB-3: la runa de la esfera elegida pasa al ladrón; el robo es la acción del turno, que termina (o, en un
			// evento del Warden, devuelve la máquina al evento).
			Soulbound::Rules::ResolveShellPick(State, Option, NowSeconds);
			FinishAction(State, NowSeconds);
			break;
		case EChoiceKind::Dilemma:
		case EChoiceKind::ChosenRival:
		case EChoiceKind::GiftRune:
		case EChoiceKind::LoseRune:
			// S-014: las del evento del Warden en curso; el evento sigue hasta la siguiente elección o el final.
			Soulbound::Rules::AnswerWardenEventChoice(State, Kind, Option, NowSeconds);
			break;
		case EChoiceKind::None:
			break;
		}
		return EIntentResult::Accepted;
	}

	EIntentResult SubmitDuelPerformance(FMatchState& State, int32 Seat, double Performance, double NowSeconds)
	{
		// Glosario "Intención": fase, asiento y valor; si no vale, se descarta sin castigo y sin tocar el estado.
		const FDuelState& Duel = State.Duel;
		if (!HasTurnOrder(State) || State.TurnPhase != ETurnPhase::Duel || !Duel.IsActive())
		{
			return EIntentResult::WrongPhase;
		}
		if (!Duel.IsParticipant(Seat))
		{
			return EIntentResult::WrongSeat;
		}
		if (!(Performance >= 0.0 && Performance <= 1.0))
		{
			return EIntentResult::InvalidOption;
		}
		// Cada participante rinde una vez; una segunda intención no contesta nada.
		if (Duel.HasPerformed(Seat))
		{
			return EIntentResult::WrongPhase;
		}
		// TODO(R-MIN): sin plazo en S-012; el minijuego de duelo tendrá su propio tiempo.
		if (!Soulbound::Rules::RecordDuelPerformance(State, Seat, Performance))
		{
			return EIntentResult::Accepted;
		}
		// R-DUE-1: con los dos rendimientos, el duelo se resuelve. R-DUE-6: el perdedor sin runas ya está en su hogar y la
		// acción termina. R-DUE-5: si no, el ganador le roba con el trilero, sea retador o retado; el trilero termina el turno
		// del retador, que es el activo. S-015: un duelo de un evento no termina ningún turno (FinishAction); contra el
		// Warden, lo que pierde el jugador no es un trilero (el Warden no tiene Brazal), y con todas las runas en juego
		// tampoco lo es (R-EFE-13).
		const FDuelResult Result = Soulbound::Rules::ResolveDuel(State, NowSeconds);
		if (Result.bAgainstWarden)
		{
			Soulbound::Rules::SettleWardenDuel(State, Result, NowSeconds);
			FinishAction(State, NowSeconds);
		}
		else if (Result.bLoserSentHome)
		{
			FinishAction(State, NowSeconds);
		}
		else if (Result.bAllRunes)
		{
			Soulbound::Rules::SeizeAllRunes(State, Result.Winner, Result.Loser, NowSeconds);
			FinishAction(State, NowSeconds);
		}
		else
		{
			BeginStealAction(State, Result.Winner, Result.Loser, NowSeconds);
		}
		return EIntentResult::Accepted;
	}

	void BeginEventSteal(FMatchState& State, int32 Thief, int32 Victim, double NowSeconds)
	{
		State.CurrentEvent.bWaitsForAction = true;
		BeginStealAction(State, Thief, Victim, NowSeconds);
	}

	void BeginEventDuel(FMatchState& State, int32 Challenger, int32 Challenged, bool bAllRunes, double NowSeconds)
	{
		State.CurrentEvent.bWaitsForAction = true;
		if (Soulbound::Rules::BeginForcedDuel(State, Challenger, Challenged, bAllRunes, NowSeconds) == EDuelStart::NoRunes)
		{
			FinishAction(State, NowSeconds);
			return;
		}
		State.TurnPhase = ETurnPhase::Duel;
	}

	void BeginEventWardenDuel(FMatchState& State, int32 Challenged, bool bAllRunes, double NowSeconds)
	{
		State.CurrentEvent.bWaitsForAction = true;
		if (Soulbound::Rules::BeginWardenDuel(State, Challenged, bAllRunes, NowSeconds) == EDuelStart::NoRunes)
		{
			FinishAction(State, NowSeconds);
			return;
		}
		State.TurnPhase = ETurnPhase::Duel;
	}

	bool CanInterruptTurn(const FMatchState& State)
	{
		if (!HasTurnOrder(State) || State.IsMatchOver())
		{
			return false;
		}
		// R-WAR-7: en Duel, ShellGame y SkillTrial no se corta (el Warden aplaza); entre turnos y en End no hay turno que cortar.
		switch (State.TurnPhase)
		{
		case ETurnPhase::Start:
		case ETurnPhase::PreRoll:
		case ETurnPhase::RollAndChoose:
		case ETurnPhase::MoveAndResolve:
			return true;
		default:
			return false;
		}
	}

	bool InterruptTurn(FMatchState& State, double NowSeconds, FRandomStream& Random)
	{
		if (!CanInterruptTurn(State))
		{
			return false;
		}
		const ETurnPhase Phase = State.TurnPhase;
		// Consumo fijo (decisión del autor del 04/10/2026): un turno que llegó a PreRoll saca cuatro valores. En PreRoll aún no
		// tiró: los cuatro; después, los dos de la repetición si siguen pendientes. En Start no llegó: nada.
		if (Phase == ETurnPhase::PreRoll)
		{
			DrawDice(Random);
			DrawDice(Random);
		}
		else
		{
			SettleRerollValues(State, Random);
		}
		LogEvent(State, EMatchEventType::TurnInterrupted, NowSeconds, State.ActiveSeat, static_cast<int32>(Phase));
		// R-TUR-11: la ficha se queda donde está; el turno termina como cualquier otro (turno extra, siguiente, fin de carrera), y
		// el cortado es el activo del evento que lo cortó (el último que jugó).
		State.PendingTrial = ESkillTrial::None;
		State.LastTurnSeat = State.ActiveSeat;
		EnterEnd(State, NowSeconds);
		LeaveEnd(State, NowSeconds, Random);
		return true;
	}

	bool GrantExtraTurns(FMatchState& State, int32 Seat, int32 Count)
	{
		return GrantCapped(State, Seat, Count, &FSeatState::ExtraTurns, State.Params.MaxExtraTurns);
	}

	bool GrantLostTurns(FMatchState& State, int32 Seat, int32 Count)
	{
		return GrantCapped(State, Seat, Count, &FSeatState::LostTurns, State.Params.MaxLostTurns);
	}

	bool RaiseInsolence(FMatchState& State, int32 Seat, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return false;
		}
		int32& Insolence = State.Seats[Seat].Insolence;
		const int32 LevelBefore = Insolence;
		Insolence = FMath::Min(Insolence + 1, Soulbound::Match::MaxInsolence);
		// S-016 (decisión del autor del 19/10/2026): sin Warden en la partida, sin tabla, no hay castigos: la insolencia solo cuenta.
		if (State.WardenParams.InsolenceTable.IsEmpty())
		{
			return true;
		}
		// R-INS-2, R-INS-3: el castigo se sortea ahora, con el nivel anterior, y se aplica cuando el asiento deja de estar en
		// juego (o a su salida del Vacío, salvo la Furia).
		FRandomStream Draw = WardenDraw(State);
		const FPendingPunishment Punishment{Insolence, DrawInsolencePunishment(State.WardenParams, LevelBefore, Draw)};
		PunishOrDefer(State, Seat, Punishment, NowSeconds);
		return true;
	}
}
