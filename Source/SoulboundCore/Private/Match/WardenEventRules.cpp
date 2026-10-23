#include "Match/WardenEventRules.h"

#include "Match/BondRaceRules.h"
#include "Match/EffectCatalog.h"
#include "Match/MatchEvents.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Templates/TypeHash.h"

using Soulbound::Rules::LogEvent;

namespace
{
	/** El evento en curso, o nullptr si no hay. */
	const FWardenEventDef* CurrentDef(const FMatchState& State)
	{
		const int32 Index = State.CurrentEvent.EventIndex;
		return State.WardenEvents.Events.IsValidIndex(Index) ? &State.WardenEvents.Events[Index] : nullptr;
	}

	/** Aplicaciones de la opción en curso; vacío mientras el dilema espera. */
	const TArray<FEffectApplication>& CurrentApplications(const FMatchState& State)
	{
		static const TArray<FEffectApplication> None;
		const FWardenEventDef* Def = CurrentDef(State);
		return Def && Def->Options.IsValidIndex(State.CurrentEvent.Option) ? Def->Options[State.CurrentEvent.Option].Applications : None;
	}

	/** La aplicación que se está resolviendo: la anterior a NextApplication, que avanza al empezarla. */
	FEffectApplication ApplicationInProgress(const FMatchState& State)
	{
		const TArray<FEffectApplication>& Applications = CurrentApplications(State);
		const int32 Index = State.CurrentEvent.NextApplication - 1;
		return Applications.IsValidIndex(Index) ? Applications[Index] : FEffectApplication();
	}

	/**
	 * El siguiente sorteo del evento: un generador con la semilla del evento combinada con el número de sorteo. La semilla salió
	 * una vez del sorteo del Warden; nada de esto toca el sorteo de la partida.
	 */
	FRandomStream NextDraw(FMatchState& State)
	{
		FCurrentWardenEvent& Event = State.CurrentEvent;
		return FRandomStream(static_cast<int32>(HashCombineFast(GetTypeHash(Event.Seed), GetTypeHash(Event.Draws++))));
	}

	/** Abre una elección del evento para el asiento Seat, con sus opciones y su plazo. */
	void OpenEventChoice(FMatchState& State, EChoiceKind Kind, int32 Seat, TArray<int32> Options, double DeadlineSeconds)
	{
		State.PendingChoice = FPendingChoice();
		State.PendingChoice.Kind = Kind;
		State.PendingChoice.Seat = Seat;
		State.PendingChoice.Options = MoveTemp(Options);
		State.PendingChoice.DeadlineSeconds = DeadlineSeconds;
	}

	/** Burla del Warden (R-DIR-3): la aplicación Application no se aplica; Seat, a quien se dirige la burla. */
	void Taunt(FMatchState& State, const FEffectApplication& Application, int32 Seat, double NowSeconds)
	{
		LogEvent(State, EMatchEventType::WardenTaunt, NowSeconds, Seat, static_cast<int32>(Application.Effect), INDEX_NONE, static_cast<int32>(Application.Target));
	}

	/** Asiento que sigue a Seat en el orden de turno (R-TUR-1). */
	int32 NextInTurnOrder(const FMatchState& State, int32 Seat)
	{
		const int32 Index = State.TurnOrder.Find(Seat);
		return Index == INDEX_NONE ? INDEX_NONE : State.TurnOrder[(Index + 1) % State.TurnOrder.Num()];
	}

	/**
	 * Si la aplicación se resuelve objetivo a objetivo, uno tras otro y en orden de asiento: pide al objetivo una elección (la
	 * runa regalada y la runa a perder a elección, R-EFE-11, R-EFE-16) o juega un trilero con cada víctima (el Robo decretado
	 * "a todos", R-EFE-12, S-015), que la máquina juega entre turnos.
	 */
	bool AsksTarget(const FEffectApplication& Application)
	{
		return Application.Effect == EEffect::GiftRune || Application.Effect == EEffect::DecreedTheft
			|| (Application.Effect == EEffect::LoseRune && Application.Variant == EEffectVariant::Chosen);
	}

	/** Si el asiento tiene alguna runa en el Brazal. */
	bool HasRunes(const FSeatState& Seat)
	{
		return Seat.Runes.ContainsByPredicate([](int32 Count) { return Count > 0; });
	}

	/**
	 * Si Seat puede ser objeto de lo que el Warden hace por su cuenta cuando se le desatiende (R-DIR-4, S-015): está en el
	 * tablero y tiene alguna runa. Sin runas no hay duelo ni robo: burla.
	 */
	bool CanWardenTakeFrom(const FMatchState& State, int32 Seat)
	{
		return State.Seats.IsValidIndex(Seat) && !State.Seats[Seat].IsInVoid() && HasRunes(State.Seats[Seat]);
	}

	/**
	 * Asientos a los que apunta la dirección Target en un evento que dirige Director (R-DIR-1), antes del filtro de válidos:
	 * Activo, Siguiente, Líder, Todos y Ecos. Rival señalado y Al azar se resuelven aparte (elección y sorteo entre válidos).
	 */
	TArray<int32> PointedSeats(const FMatchState& State, EEffectTarget Target, int32 Director)
	{
		TArray<int32> Seats;
		switch (Target)
		{
		case EEffectTarget::ActivePlayer:
			Seats.Add(Director);
			break;
		case EEffectTarget::NextPlayer:
			Seats.Add(NextInTurnOrder(State, Director));
			break;
		case EEffectTarget::Leader:
			Seats = Soulbound::Rules::LeaderSeats(State);
			break;
		case EEffectTarget::Everyone:
		case EEffectTarget::RandomPlayer:
			for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
			{
				Seats.Add(Seat);
			}
			break;
		case EEffectTarget::Echoes:
			for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
			{
				if (State.Seats[Seat].Role == ESeatRole::LostEcho)
				{
					Seats.Add(Seat);
				}
			}
			break;
		case EEffectTarget::ChosenRival:
		case EEffectTarget::TwoChosenRivals:
		case EEffectTarget::None:
			break;
		}
		return Seats;
	}

	/** De Seats, los que son objetivo válido del efecto (R-DIR-2, R-DIR-3), en el mismo orden. */
	TArray<int32> ValidTargets(const FMatchState& State, EEffect Effect, const TArray<int32>& Seats, int32 Director)
	{
		return Seats.FilterByPredicate([&](int32 Seat) { return Soulbound::Rules::CanApplyEffect(State, Effect, Seat, Director) == EEffectResult::Applied; });
	}

	/** Aplica Application a Seat con un sorteo del evento; si ya no vale, burla. */
	void ApplyOrTaunt(FMatchState& State, const FEffectApplication& Application, int32 Seat, int32 Pointer, int32 Rune, double NowSeconds)
	{
		FRandomStream Draw = NextDraw(State);
		if (Soulbound::Rules::ApplyEffect(State, Application, Seat, State.CurrentEvent.Director, Pointer, Rune, NowSeconds, Draw) != EEffectResult::Applied)
		{
			Taunt(State, Application, State.CurrentEvent.Director, NowSeconds);
		}
	}

	/** Un Duelo forzado desatendido (R-DIR-4): Seat se bate contra el Warden; sin runas, burla. */
	void StartWardenDuel(FMatchState& State, const FEffectApplication& Application, int32 Seat, double NowSeconds)
	{
		if (!CanWardenTakeFrom(State, Seat))
		{
			Taunt(State, Application, Seat, NowSeconds);
			return;
		}
		Soulbound::Rules::BeginEventWardenDuel(State, Seat, Application.Variant == EEffectVariant::AllRunes, NowSeconds);
	}

	/** Un Robo decretado desatendido (R-DIR-4): el Warden roba a Seat, una runa suya al azar vuelve a su Fuente; sin runas, burla. */
	void WardenTakesRune(FMatchState& State, const FEffectApplication& Application, int32 Seat, double NowSeconds)
	{
		if (!CanWardenTakeFrom(State, Seat))
		{
			Taunt(State, Application, Seat, NowSeconds);
			return;
		}
		FRandomStream Draw = NextDraw(State);
		const int32 RuneType = Soulbound::Rules::RandomRuneType(State, Seat, Draw);
		LogEvent(State, EMatchEventType::RuneTakenByWarden, NowSeconds, Seat, RuneType);
		Soulbound::Rules::ReturnRuneToSource(State, Seat, RuneType, State.Board.FindPowerSource(RuneType), NowSeconds);
	}

	/**
	 * Resolución en contra de quien dirige (R-COM-2, decisión del autor del 12/10/2026), sin ninguna elección dentro:
	 *  - los regalos (Turno extra, Runa regalada, Liberación, Noche de los Ecos) se descartan;
	 *  - los dañinos con asiento caen sobre quien dirige, y si no valen sobre él, burla (la runa a perder, al azar);
	 *  - los dañinos sin asiento (Sello, Ley de Sombras) se aplican tal cual;
	 *  - el Duelo forzado es un Duelo contra el Warden, y el Robo decretado, que el Warden le roba a él (R-DIR-4, S-015).
	 */
	void ApplyAgainstDirector(FMatchState& State, const FEffectApplication& Application, double NowSeconds)
	{
		if (Soulbound::Rules::IsGift(Application.Effect))
		{
			return;
		}
		if (Application.Effect == EEffect::ForcedDuel)
		{
			StartWardenDuel(State, Application, State.CurrentEvent.Director, NowSeconds);
			return;
		}
		if (Application.Effect == EEffect::DecreedTheft)
		{
			WardenTakesRune(State, Application, State.CurrentEvent.Director, NowSeconds);
			return;
		}
		const int32 Seat = Soulbound::Rules::IsGlobalEffect(Application.Effect) ? INDEX_NONE : State.CurrentEvent.Director;
		ApplyOrTaunt(State, Application, Seat, INDEX_NONE, INDEX_NONE, NowSeconds);
	}

	/**
	 * Empieza la aplicación Application de la opción en curso (R-COM-3: cada una se valida por separado). Sin objetivos
	 * válidos, burla (R-DIR-3). Un rival señalado abre su elección; la runa regalada y la runa a perder a elección dejan sus
	 * objetivos en PendingTargets; el resto se aplica ya.
	 */
	void BeginApplication(FMatchState& State, const FEffectApplication& Application, double NowSeconds)
	{
		const int32 Director = State.CurrentEvent.Director;
		if (State.CurrentEvent.bAgainst)
		{
			ApplyAgainstDirector(State, Application, NowSeconds);
			return;
		}
		if (Soulbound::Rules::IsGlobalEffect(Application.Effect))
		{
			ApplyOrTaunt(State, Application, INDEX_NONE, INDEX_NONE, INDEX_NONE, NowSeconds);
			return;
		}
		if (Application.Target == EEffectTarget::ChosenRival)
		{
			// R-DIR-1, R-DIR-2: el activo elige entre los rivales válidos, con el reloj corto.
			TArray<int32> Rivals = ValidTargets(State, Application.Effect, PointedSeats(State, EEffectTarget::Everyone, Director), Director);
			Rivals.Remove(Director);
			if (Rivals.IsEmpty())
			{
				Taunt(State, Application, Director, NowSeconds);
				return;
			}
			OpenEventChoice(State, EChoiceKind::ChosenRival, Director, MoveTemp(Rivals), NowSeconds + State.Params.ShortChoiceClockSeconds);
			return;
		}
		if (Application.Target == EEffectTarget::TwoChosenRivals)
		{
			// R-DIR-1, R-EFE-13: quien dirige señala dos rivales válidos, uno tras otro; el primero es el retador. Con menos de
			// dos válidos, burla (R-DIR-3). Las dos elecciones son ChosenRival; FirstRival dice cuál es cuál.
			TArray<int32> Rivals = ValidTargets(State, Application.Effect, PointedSeats(State, EEffectTarget::Everyone, Director), Director);
			Rivals.Remove(Director);
			if (Rivals.Num() < 2)
			{
				Taunt(State, Application, Director, NowSeconds);
				return;
			}
			State.CurrentEvent.FirstRival = INDEX_NONE;
			OpenEventChoice(State, EChoiceKind::ChosenRival, Director, MoveTemp(Rivals), NowSeconds + State.Params.ShortChoiceClockSeconds);
			return;
		}
		TArray<int32> Targets = ValidTargets(State, Application.Effect, PointedSeats(State, Application.Target, Director), Director);
		if (Application.Effect == EEffect::DecreedTheft)
		{
			// R-EFE-12: cuando elige el Warden ("a todos", "al azar") se salta a los que no tienen runas (cambia R-DIR-2).
			Targets = Targets.FilterByPredicate([&State](int32 Seat) { return HasRunes(State.Seats[Seat]); });
		}
		if (Targets.IsEmpty())
		{
			Taunt(State, Application, Director, NowSeconds);
			return;
		}
		if (Application.Effect == EEffect::ForcedDuel)
		{
			// R-EFE-13, "dos al azar": dos válidos distintos con el sorteo del evento, y el retador se sortea entre ellos. Quien
			// dirige puede ser uno de los dos (decisión del autor del 14/10/2026: "al azar" puede caer sobre quien dirige).
			if (Targets.Num() < 2)
			{
				Taunt(State, Application, Director, NowSeconds);
				return;
			}
			const int32 First = Targets[NextDraw(State).RandRange(0, Targets.Num() - 1)];
			Targets.Remove(First);
			const int32 Second = Targets[NextDraw(State).RandRange(0, Targets.Num() - 1)];
			const bool bFirstChallenges = NextDraw(State).RandRange(0, 1) == 0;
			ApplyOrTaunt(State, Application, bFirstChallenges ? Second : First, bFirstChallenges ? First : Second, INDEX_NONE, NowSeconds);
			return;
		}
		if (Application.Target == EEffectTarget::RandomPlayer)
		{
			// R-DIR-1: uno de los válidos, con el sorteo del evento (el del Warden).
			const int32 Pick = NextDraw(State).RandRange(0, Targets.Num() - 1);
			Targets = {Targets[Pick]};
		}
		if (AsksTarget(Application))
		{
			State.CurrentEvent.PendingTargets = MoveTemp(Targets);
			return;
		}
		for (const int32 Seat : Targets)
		{
			ApplyOrTaunt(State, Application, Seat, INDEX_NONE, INDEX_NONE, NowSeconds);
		}
	}

	/**
	 * Abre la elección del primer objetivo pendiente (runa regalada o runa a perder), con el reloj corto. Si ya no vale (la
	 * reserva se agotó con el anterior, por ejemplo), burla y se pasa al siguiente.
	 */
	void AskNextTarget(FMatchState& State, double NowSeconds)
	{
		const FEffectApplication Application = ApplicationInProgress(State);
		const int32 Seat = State.CurrentEvent.PendingTargets[0];
		if (Application.Effect == EEffect::DecreedTheft)
		{
			// R-EFE-12, "a todos": el trilero de esta víctima, entre turnos; el evento sigue con el siguiente cuando acaba.
			State.CurrentEvent.PendingTargets.RemoveAt(0);
			ApplyOrTaunt(State, Application, Seat, INDEX_NONE, INDEX_NONE, NowSeconds);
			return;
		}
		const bool bGift = Application.Effect == EEffect::GiftRune;
		TArray<int32> Options = bGift ? Soulbound::Rules::GiftRuneOptions(State) : Soulbound::Rules::LoseRuneOptions(State, Seat);
		if (Options.IsEmpty() || Soulbound::Rules::CanApplyEffect(State, Application.Effect, Seat, State.CurrentEvent.Director) != EEffectResult::Applied)
		{
			Taunt(State, Application, State.CurrentEvent.Director, NowSeconds);
			State.CurrentEvent.PendingTargets.RemoveAt(0);
			return;
		}
		OpenEventChoice(State, bGift ? EChoiceKind::GiftRune : EChoiceKind::LoseRune, Seat, MoveTemp(Options), NowSeconds + State.Params.ShortChoiceClockSeconds);
	}

	/** Sigue el evento en curso hasta la siguiente elección o hasta el final (R-COM-3: las aplicaciones, en orden). */
	void ContinueEvent(FMatchState& State, double NowSeconds)
	{
		// S-015: con un duelo o un trilero del evento en curso (bWaitsForAction) la máquina espera; sigue al acabar.
		while (State.CurrentEvent.IsActive() && !State.PendingChoice.IsOpen() && State.CurrentEvent.Option != INDEX_NONE && !State.CurrentEvent.bWaitsForAction)
		{
			FCurrentWardenEvent& Event = State.CurrentEvent;
			if (!Event.PendingTargets.IsEmpty())
			{
				AskNextTarget(State, NowSeconds);
				continue;
			}
			const TArray<FEffectApplication>& Applications = CurrentApplications(State);
			if (Event.NextApplication >= Applications.Num())
			{
				State.CurrentEvent = FCurrentWardenEvent();
				return;
			}
			const FEffectApplication Application = Applications[Event.NextApplication++];
			BeginApplication(State, Application, NowSeconds);
		}
	}
}

namespace Soulbound::Rules
{
	TArray<int32> LeaderSeats(const FMatchState& State)
	{
		// R-DIR-1: más tipos distintos; con empate, más runas; con empate total, todos los empatados.
		TArray<int32> Leaders;
		TPair<int32, int32> Best(-1, -1);
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			TPair<int32, int32> Score(0, 0);
			for (const int32 Count : State.Seats[Seat].Runes)
			{
				Score.Key += Count > 0 ? 1 : 0;
				Score.Value += Count;
			}
			if (Score.Key > Best.Key || (Score.Key == Best.Key && Score.Value > Best.Value))
			{
				Best = Score;
				Leaders.Reset();
			}
			if (Score == Best)
			{
				Leaders.Add(Seat);
			}
		}
		// TODO(R-DIR-1): sin runas empatan todos y todos son líderes (decisión del autor del 12/10/2026); duda para el GDD 05.
		return Leaders;
	}

	int32 EventDirector(const FMatchState& State)
	{
		// R-DIR-1: el activo; si el evento llega entre turnos, el último que jugó (o el que va a jugar, si aún no jugó nadie).
		if (State.TurnPhase != ETurnPhase::BetweenTurns || State.LastTurnSeat == INDEX_NONE)
		{
			return State.ActiveSeat;
		}
		return State.LastTurnSeat;
	}

	EEffectResult CanApplyEffect(const FMatchState& State, EEffect Effect, int32 Seat, int32 Director)
	{
		if (!IsEffectImplemented(Effect))
		{
			return EEffectResult::NotImplemented;
		}
		// R-DIR-2: en el Vacío solo Liberación; cada comprobación lo mira (InVoid) junto a sus propios motivos (R-DIR-3).
		switch (Effect)
		{
		case EEffect::Halt:
			// R-TAB-28 (S-016): el Alto es el corte; solo vale en un evento que llegó cortando un turno, y sobre el cortado.
			return State.CurrentEvent.IsActive() && State.CurrentEvent.bInterruptedTurn && Seat == Director && State.Seats.IsValidIndex(Seat)
				? EEffectResult::Applied : EEffectResult::NoTurnToCut;
		case EEffect::RevealBracer: return CanRevealBracer(State, Seat);
		case EEffect::SendHome: return CanSendHome(State, Seat);
		case EEffect::LoseTurn:
		case EEffect::ExtraTurn: return CanChangeTurns(State, Seat);
		case EEffect::PlaceSeal: return SealableTiles(State).IsEmpty() ? EEffectResult::NoValidTile : EEffectResult::Applied;
		case EEffect::SealAgainstRival: return CanSealAgainstRival(State, Seat);
		case EEffect::Mark: return CanMark(State, Seat, Director);
		case EEffect::ShadowLaw: return CanShadowLaw(State);
		case EEffect::LoseRune: return CanLoseRune(State, Seat);
		case EEffect::Banish: return CanBanish(State, Seat, false);
		case EEffect::GiftRune: return CanGiftRune(State, Seat);
		case EEffect::WardenTrial: return CanWardenTrial(State, Seat);
		case EEffect::DecreedTheft: return CanDecreeTheft(State, Seat, Director);
		case EEffect::ForcedDuel:
			// R-INT-3: quien dirige no puede estar en el Vacío. Seat puede ser el propio Director (con "dos al azar", 14/10/2026);
			// que el retador y el retado sean distintos lo comprueba ApplyEffect con el retador real.
			if (!State.Seats.IsValidIndex(Director))
			{
				return EEffectResult::InvalidSeat;
			}
			return State.Seats[Director].IsInVoid() ? EEffectResult::InVoid : CanForceDuelTarget(State, Seat);
		case EEffect::ReleaseVoid:
			// Global: con alguien dentro. Sobre un asiento: solo si está dentro (filtro de "los del Vacío", decisión del autor).
			if (Seat == INDEX_NONE)
			{
				return CanReleaseVoid(State);
			}
			// R-INS-3 (S-016): a quien está en el Vacío de la Furia la Liberación no le saca.
			return State.Seats.IsValidIndex(Seat) && State.Seats[Seat].IsInVoid() && !State.Seats[Seat].bFuryVoid ? EEffectResult::Applied : EEffectResult::NobodyInVoid;
		case EEffect::EchoNight: return CanEchoNight(State);
		default: return EEffectResult::NotImplemented;
		}
	}

	EEffectResult ApplyEffect(FMatchState& State, const FEffectApplication& Application, int32 Seat, int32 Director, int32 Pointer, int32 Rune,
		double NowSeconds, FRandomStream& Random)
	{
		const EEffect Effect = Application.Effect;
		const EEffectResult Check = CanApplyEffect(State, Effect, Seat, Director);
		if (Check != EEffectResult::Applied)
		{
			return Check;
		}
		// R-EFE-13: con "dos rivales señalados", el retador es el primero señalado (Pointer), no quien dirige.
		const int32 Challenger = Pointer != INDEX_NONE ? Pointer : Director;
		if (Effect == EEffect::ForcedDuel)
		{
			const EEffectResult DuelCheck = CanForceDuel(State, Seat, Challenger);
			if (DuelCheck != EEffectResult::Applied)
			{
				return DuelCheck;
			}
		}
		const bool bRuneMissing = Effect == EEffect::GiftRune ? !GiftRuneOptions(State).Contains(Rune)
			: Effect == EEffect::LoseRune && Rune != INDEX_NONE && !LoseRuneOptions(State, Seat).Contains(Rune);
		if (bRuneMissing)
		{
			return EEffectResult::InvalidRune;
		}
		const int32 Target = IsGlobalEffect(Effect) ? INDEX_NONE : Seat;
		LogEvent(State, EMatchEventType::EffectApplied, NowSeconds, Target, static_cast<int32>(Effect), Pointer, static_cast<int32>(Application.Target));
		// TODO(R-EFE-0): un switch que crece con cada efecto; el que falta (Consulta forzada, S-017) entra aquí.
		switch (Effect)
		{
		// R-TAB-28 (S-016): el Alto no hace nada más que el corte, que ya hizo quien lanzó el evento (InterruptTurn).
		case EEffect::Halt: break;
		case EEffect::RevealBracer: RevealBracer(State, Seat, NowSeconds); break;
		case EEffect::SendHome: SendHome(State, Seat, NowSeconds); break;
		case EEffect::LoseTurn: LoseTurn(State, Seat, Application.Amount, NowSeconds); break;
		case EEffect::ExtraTurn: ExtraTurn(State, Seat, Application.Amount, NowSeconds); break;
		case EEffect::PlaceSeal: PlaceSeal(State, NowSeconds, Random); break;
		case EEffect::SealAgainstRival: SealAgainstRival(State, Seat, NowSeconds); break;
		case EEffect::Mark: Mark(State, Seat, Director, Pointer, NowSeconds); break;
		case EEffect::ShadowLaw: ShadowLaw(State, NowSeconds); break;
		case EEffect::LoseRune: LoseRune(State, Seat, Rune, NowSeconds, Random); break;
		case EEffect::Banish: Banish(State, Seat, false, NowSeconds); break;
		case EEffect::GiftRune: GiftRune(State, Seat, Rune, NowSeconds); break;
		case EEffect::ReleaseVoid: ReleaseVoid(State, NowSeconds); break;
		case EEffect::EchoNight: EchoNight(State, NowSeconds); break;
		case EEffect::WardenTrial: WardenTrial(State, Seat, NowSeconds); break;
		case EEffect::DecreedTheft: DecreeTheft(State, Seat, Director, NowSeconds); break;
		case EEffect::ForcedDuel: ForceDuel(State, Seat, Challenger, Application.Variant == EEffectVariant::AllRunes, NowSeconds); break;
		default: break;
		}
		// R-EFE-8: mandar a casa o al Vacío a un Marcado con un efecto que uno señaló cobra su marca.
		if ((Effect == EEffect::SendHome || Effect == EEffect::Banish) && Pointer != INDEX_NONE)
		{
			CollectMark(State, Pointer, Seat, NowSeconds);
		}
		return EEffectResult::Applied;
	}

	bool HasValidTargets(const FMatchState& State, const FEffectApplication& Application, int32 Director)
	{
		// El mismo filtro que BeginApplication, sin abrir elecciones ni sortear.
		if (IsGlobalEffect(Application.Effect))
		{
			return CanApplyEffect(State, Application.Effect, INDEX_NONE, Director) == EEffectResult::Applied;
		}
		if (Application.Target == EEffectTarget::ChosenRival || Application.Target == EEffectTarget::TwoChosenRivals)
		{
			TArray<int32> Rivals = ValidTargets(State, Application.Effect, PointedSeats(State, EEffectTarget::Everyone, Director), Director);
			Rivals.Remove(Director);
			return Rivals.Num() >= (Application.Target == EEffectTarget::TwoChosenRivals ? 2 : 1);
		}
		TArray<int32> Targets = ValidTargets(State, Application.Effect, PointedSeats(State, Application.Target, Director), Director);
		if (Application.Effect == EEffect::DecreedTheft)
		{
			Targets = Targets.FilterByPredicate([&State](int32 Seat) { return HasRunes(State.Seats[Seat]); });
		}
		return Targets.Num() >= (Application.Effect == EEffect::ForcedDuel ? 2 : 1);
	}

	double DilemmaClockSeconds(const FMatchParams& Params, int32 OptionCount)
	{
		// R-COM-2: base más un incremento por cada opción a partir de la segunda.
		return Params.DilemmaClockSeconds + Params.DilemmaClockPerOptionSeconds * FMath::Max(OptionCount - 1, 0);
	}

	EWardenEventStart CanApplyEvent(const FMatchState& State, const FString& EventId)
	{
		const bool bHasTurnOrder = State.TurnOrder.Num() == State.Seats.Num() && State.Seats.Num() > 0;
		if (!bHasTurnOrder || State.IsMatchOver())
		{
			return EWardenEventStart::MatchOver;
		}
		// Decisión del autor del 10/10/2026: los eventos se aplican entre turnos. Desde S-016 un golpe llega a mitad de turno,
		// pero lo corta antes (InterruptTurn) y el evento se aplica en el hueco que deja: también entre turnos.
		if (State.TurnPhase != ETurnPhase::BetweenTurns)
		{
			return EWardenEventStart::NotBetweenTurns;
		}
		if (State.CurrentEvent.IsActive())
		{
			return EWardenEventStart::EventInProgress;
		}
		const int32 Index = State.WardenEvents.IndexOf(EventId);
		if (Index == INDEX_NONE)
		{
			return EWardenEventStart::UnknownEvent;
		}
		// Decisión del autor del 12/10/2026: un evento con un efecto de otra story se rechaza entero, sin tocar nada.
		for (const FWardenEventOption& Option : State.WardenEvents.Events[Index].Options)
		{
			for (const FEffectApplication& Application : Option.Applications)
			{
				if (!IsEffectImplemented(Application.Effect))
				{
					return EWardenEventStart::NotImplemented;
				}
			}
		}
		return EWardenEventStart::Started;
	}

	EWardenEventStart ApplyEvent(FMatchState& State, const FString& EventId, double NowSeconds, FRandomStream& Random, bool bInterruptedTurn, int32 Director)
	{
		const EWardenEventStart Result = CanApplyEvent(State, EventId);
		if (Result != EWardenEventStart::Started)
		{
			return Result;
		}
		// TODO(R-COM-1): la entrega por el Oráculo es de su story; hasta entonces todo evento se aplica como entrega directa.
		FCurrentWardenEvent& Event = State.CurrentEvent;
		Event = FCurrentWardenEvent();
		Event.EventIndex = State.WardenEvents.IndexOf(EventId);
		Event.Director = EventDirector(State);
		// S-016: si llega cortando un turno (un golpe), el Alto vale sobre el cortado, que es quien lo dirige.
		Event.bInterruptedTurn = bInterruptedTurn;
		// Un valor del sorteo del Warden; los sorteos del evento salen de él, nunca del de la partida.
		Event.Seed = Random.RandRange(0, MAX_int32 - 1);
		const int32 OptionCount = State.WardenEvents.Events[Event.EventIndex].Options.Num();
		LogEvent(State, EMatchEventType::WardenEventStarted, NowSeconds, Event.Director, Event.EventIndex, INDEX_NONE, OptionCount);
		if (OptionCount == 1)
		{
			// R-COM-2: una opción es un decreto, que se aplica sin elegir.
			Event.Option = 0;
		}
		else
		{
			// R-COM-2: varias, un dilema que quien dirige elige dentro de su reloj.
			TArray<int32> Options;
			for (int32 Option = 0; Option < OptionCount; ++Option)
			{
				Options.Add(Option);
			}
			OpenEventChoice(State, EChoiceKind::Dilemma, Event.Director, MoveTemp(Options), NowSeconds + DilemmaClockSeconds(State.Params, OptionCount));
		}
		ContinueEvent(State, NowSeconds);
		return EWardenEventStart::Started;
	}

	void SettleWardenDuel(FMatchState& State, const FDuelResult& Result, double NowSeconds)
	{
		if (!Result.bAgainstWarden)
		{
			return;
		}
		// Contra el Warden, quien gana es INDEX_NONE (el Warden) o el jugador; el jugador es el otro asiento del duelo.
		const bool bWardenWins = Result.Winner == INDEX_NONE;
		const int32 Player = bWardenWins ? Result.Loser : Result.Winner;
		if (!State.Seats.IsValidIndex(Player))
		{
			return;
		}
		if (!bWardenWins)
		{
			LogEvent(State, EMatchEventType::WardenDuelLost, NowSeconds, Player);
			return;
		}
		LogEvent(State, EMatchEventType::WardenDuelWon, NowSeconds, Player);
		// R-RUN-6: lo que estaba en juego vuelve a su Fuente; el Warden no tiene Brazal, no hay trilero.
		if (Result.bAllRunes)
		{
			for (int32 RuneType = 0; RuneType < State.Seats[Player].Runes.Num(); ++RuneType)
			{
				while (State.Seats[Player].RuneCountOf(RuneType) > 0
					&& Soulbound::Rules::ReturnRuneToSource(State, Player, RuneType, State.Board.FindPowerSource(RuneType), NowSeconds))
				{
				}
			}
			return;
		}
		FRandomStream Draw = NextDraw(State);
		const int32 RuneType = Soulbound::Rules::RandomRuneType(State, Player, Draw);
		if (RuneType != INDEX_NONE)
		{
			Soulbound::Rules::ReturnRuneToSource(State, Player, RuneType, State.Board.FindPowerSource(RuneType), NowSeconds);
		}
	}

	bool AdvanceWardenEvent(FMatchState& State, double NowSeconds)
	{
		if (!State.CurrentEvent.IsActive())
		{
			return false;
		}
		if (State.PendingChoice.IsOpen())
		{
			if (NowSeconds < State.PendingChoice.DeadlineSeconds)
			{
				return false;
			}
			// R-DIR-4: desatender al Warden nunca sale gratis. Se registra, sube la insolencia y el Warden resuelve en su lugar.
			const FPendingChoice Expired = State.PendingChoice;
			State.PendingChoice = FPendingChoice();
			LogEvent(State, EMatchEventType::WardenIgnored, NowSeconds, Expired.Seat, static_cast<int32>(Expired.Kind));
			const FEffectApplication Application = ApplicationInProgress(State);
			switch (Expired.Kind)
			{
			case EChoiceKind::Dilemma:
			{
				// R-COM-2: una opción al azar, resuelta en contra de quien dirige.
				const int32 Option = Expired.Options[NextDraw(State).RandRange(0, Expired.Options.Num() - 1)];
				State.CurrentEvent.Option = Option;
				State.CurrentEvent.bAgainst = true;
				LogEvent(State, EMatchEventType::DilemmaChosen, NowSeconds, Expired.Seat, Option, INDEX_NONE, 1);
				break;
			}
			case EChoiceKind::ChosenRival:
				// R-DIR-4: el efecto dañino cae sobre quien no señaló; si no vale sobre él, burla. S-015: un Duelo forzado
				// desatendido, en la primera señal o en la segunda, es un Duelo contra el Warden; un Robo decretado, que el
				// Warden le roba a él.
				State.CurrentEvent.FirstRival = INDEX_NONE;
				if (Application.Effect == EEffect::ForcedDuel)
				{
					StartWardenDuel(State, Application, Expired.Seat, NowSeconds);
				}
				else if (Application.Effect == EEffect::DecreedTheft)
				{
					WardenTakesRune(State, Application, Expired.Seat, NowSeconds);
				}
				else
				{
					ApplyOrTaunt(State, Application, Expired.Seat, INDEX_NONE, INDEX_NONE, NowSeconds);
				}
				break;
			case EChoiceKind::GiftRune:
				// R-DIR-4: el regalo se pierde, con burla.
				Taunt(State, Application, Expired.Seat, NowSeconds);
				State.CurrentEvent.PendingTargets.RemoveAt(0);
				break;
			case EChoiceKind::LoseRune:
				// R-DIR-4: la variante de un castigo propio, al azar.
				ApplyOrTaunt(State, Application, Expired.Seat, INDEX_NONE, INDEX_NONE, NowSeconds);
				State.CurrentEvent.PendingTargets.RemoveAt(0);
				break;
			default:
				break;
			}
			// R-INS-2 (decisión del autor del 19/10/2026): primero lo que el Warden hace en su lugar (R-DIR-4), después, en el mismo
			// instante, la insolencia y su castigo; si eso lo deja en juego (su Duelo contra el Warden), el castigo espera a que acabe.
			ContinueEvent(State, NowSeconds);
			RaiseInsolence(State, Expired.Seat, NowSeconds);
			return true;
		}
		ContinueEvent(State, NowSeconds);
		return true;
	}

	void AnswerWardenEventChoice(FMatchState& State, EChoiceKind Kind, int32 Option, double NowSeconds)
	{
		if (!State.CurrentEvent.IsActive())
		{
			return;
		}
		const FEffectApplication Application = ApplicationInProgress(State);
		switch (Kind)
		{
		case EChoiceKind::Dilemma:
			State.CurrentEvent.Option = Option;
			LogEvent(State, EMatchEventType::DilemmaChosen, NowSeconds, State.CurrentEvent.Director, Option, INDEX_NONE, 0);
			break;
		case EChoiceKind::ChosenRival:
		{
			FCurrentWardenEvent& Event = State.CurrentEvent;
			if (Application.Target == EEffectTarget::TwoChosenRivals && Event.FirstRival == INDEX_NONE)
			{
				// R-EFE-13: el primero señalado será el retador; la segunda señal no lo ofrece ni se ofrece a sí misma.
				Event.FirstRival = Option;
				TArray<int32> Rivals = ValidTargets(State, Application.Effect, PointedSeats(State, EEffectTarget::Everyone, Event.Director), Event.Director);
				Rivals.Remove(Option);
				Rivals.Remove(Event.Director);
				if (Rivals.IsEmpty())
				{
					Event.FirstRival = INDEX_NONE;
					Taunt(State, Application, Event.Director, NowSeconds);
					break;
				}
				OpenEventChoice(State, EChoiceKind::ChosenRival, Event.Director, MoveTemp(Rivals), NowSeconds + State.Params.ShortChoiceClockSeconds);
				break;
			}
			// Quien dirige señaló: el efecto es suyo, y una Vuelta al hogar o un Vacío cobran la marca (R-EFE-8). Con dos
			// rivales señalados, el primero reta al segundo (Pointer es el retador).
			const int32 Pointer = Application.Target == EEffectTarget::TwoChosenRivals ? Event.FirstRival : Event.Director;
			Event.FirstRival = INDEX_NONE;
			ApplyOrTaunt(State, Application, Option, Pointer, INDEX_NONE, NowSeconds);
			break;
		}
		case EChoiceKind::GiftRune:
		case EChoiceKind::LoseRune:
		{
			const int32 Seat = State.CurrentEvent.PendingTargets[0];
			State.CurrentEvent.PendingTargets.RemoveAt(0);
			ApplyOrTaunt(State, Application, Seat, INDEX_NONE, Option, NowSeconds);
			break;
		}
		default:
			break;
		}
		ContinueEvent(State, NowSeconds);
	}
}
