#include "StatusText.h"

#include "BoardViewState.h"
#include "Dom/JsonObject.h"
#include "EventText.h"
#include "RingPanelModel.h"
#include "Match/BondRaceRules.h"
#include "Match/MatchState.h"
#include "Match/RuneRules.h"
#include "Match/SeatGoalRules.h"
#include "Match/VictoryRules.h"
#include "Match/WardenEventRules.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Warden/WardenBrain.h"

// Los textos de las líneas de estado, de la petición, del trilero, del duelo, del evento y de la votación (de S-006 a S-026) vivían en el
// espacio anónimo de BoardViewActor.cpp; S-027 los trae aquí tal cual, para que los paneles y sus tests los usen sin el actor.
namespace Soulbound::Presentation
{
	const TCHAR* PhaseName(ETurnPhase Phase)
	{
		switch (Phase)
		{
		case ETurnPhase::BetweenTurns: return TEXT("entre turnos");
		case ETurnPhase::Start: return TEXT("inicio");
		case ETurnPhase::PreRoll: return TEXT("antes de tirar");
		case ETurnPhase::RollAndChoose: return TEXT("tirada y destino");
		case ETurnPhase::Moving: return TEXT("movimiento, por el camino");
		case ETurnPhase::MoveAndResolve: return TEXT("llegada y casilla");
		case ETurnPhase::SkillTrial: return TEXT("minijuego (Fuente, escape del Vacío o Prueba del Warden)");
		case ETurnPhase::ShellGame: return TEXT("trilero");
		case ETurnPhase::Duel: return TEXT("duelo");
		case ETurnPhase::End: return TEXT("fin de turno");
		case ETurnPhase::OracleConsult: return TEXT("consulta del Oráculo");
		}
		return TEXT("?");
	}

	FString ChoiceOptionKey(const FPendingChoice& Choice, int32 Index)
	{
		if (Index < 9)
		{
			return FString::FromInt(Index + 1);
		}
		return Index == 9 && !Choice.CanDecline() ? FString(TEXT("0")) : FString();
	}

	/** Tecla de la opción en la posición Index (S-010) entre corchetes, para la línea de petición: "[1]", "[0]" o "[sin tecla]". */
	FString OptionKey(const FPendingChoice& Choice, int32 Index)
	{
		const FString Key = ChoiceOptionKey(Choice, Index);
		return Key.IsEmpty() ? FString(TEXT("[sin tecla]")) : FString::Printf(TEXT("[%s]"), *Key);
	}

	FString OptionText(const FMatchState& State, int32 Index)
	{
		const FPendingChoice& Choice = State.PendingChoice;
		switch (Choice.Kind)
		{
		case EChoiceKind::DiceCount: return Choice.Options[Index] > 1 ? TEXT("dos dados") : TEXT("un dado");
		case EChoiceKind::PlanarJump: return Soulbound::Presentation::TileShortName(State.Board, Choice.Options[Index]);
		case EChoiceKind::PlayerAction:
		{
			const FPlayerActionOption& Action = Choice.ActionOptions[Index];
			if (Action.Action == EPlayerAction::Duel)
			{
				// S-012: con quien comparte casilla, duelo; con alguien de otra casilla, solo desde un Nodo de Duelo.
				const bool bSameTile = State.Seats.IsValidIndex(Choice.Seat) && State.Seats.IsValidIndex(Action.TargetSeat)
					&& State.Seats[Choice.Seat].TileId == State.Seats[Action.TargetSeat].TileId;
				return bSameTile ? FString::Printf(TEXT("Duelo con el asiento %d"), Action.TargetSeat) : FString::Printf(TEXT("Retar al asiento %d (Nodo)"), Action.TargetSeat);
			}
			return FString::Printf(TEXT("%s al asiento %d"), Action.Action == EPlayerAction::Steal ? TEXT("Robar") : TEXT("Expulsar"), Action.TargetSeat);
		}
		case EChoiceKind::SafeRune: return FString::Printf(TEXT("D%d"), Choice.Options[Index]);
		case EChoiceKind::ShellPick: return FString::Printf(TEXT("esfera %d"), Index + 1);
		case EChoiceKind::Deflection: return TEXT("usar Desvío");
		// S-014: las de los eventos del Warden. El dilema enseña lo que hace cada opción.
		case EChoiceKind::Dilemma:
		{
			const int32 EventIndex = State.CurrentEvent.EventIndex;
			const FWardenEventDef* Def = State.WardenEvents.Events.IsValidIndex(EventIndex) ? &State.WardenEvents.Events[EventIndex] : nullptr;
			return Def && Def->Options.IsValidIndex(Choice.Options[Index]) ? Soulbound::Presentation::DescribeOption(Def->Options[Choice.Options[Index]])
				: FString::Printf(TEXT("opción %d"), Index + 1);
		}
		case EChoiceKind::ChosenRival: return FString::Printf(TEXT("asiento %d"), Choice.Options[Index]);
		case EChoiceKind::GiftRune:
		case EChoiceKind::LoseRune: return FString::Printf(TEXT("D%d"), Choice.Options[Index]);
		default: return FString::FromInt(Choice.AnswerAt(Index));
		}
	}

	/**
	 * Efecto del evento del Warden que espera ahora a su duelo o a su trilero (S-015): la aplicación en curso de la opción que
	 * se aplica. Falso si no hay evento esperando una acción. Solo lee el estado.
	 */
	bool EventActionEffect(const FMatchState& State, EEffect& OutEffect)
	{
		const FCurrentWardenEvent& Event = State.CurrentEvent;
		if (!Event.IsActive() || !Event.bWaitsForAction || !State.WardenEvents.Events.IsValidIndex(Event.EventIndex))
		{
			return false;
		}
		const FWardenEventDef& Def = State.WardenEvents.Events[Event.EventIndex];
		if (!Def.Options.IsValidIndex(Event.Option) || !Def.Options[Event.Option].Applications.IsValidIndex(Event.NextApplication - 1))
		{
			return false;
		}
		OutEffect = Def.Options[Event.Option].Applications[Event.NextApplication - 1].Effect;
		return true;
	}

	FString ChosenRivalText(const FMatchState& State)
	{
		const FCurrentWardenEvent& Event = State.CurrentEvent;
		const FWardenEventDef* Def = State.WardenEvents.Events.IsValidIndex(Event.EventIndex) ? &State.WardenEvents.Events[Event.EventIndex] : nullptr;
		if (Def && Def->Options.IsValidIndex(Event.Option) && Def->Options[Event.Option].Applications.IsValidIndex(Event.NextApplication - 1))
		{
			const FEffectApplication& Application = Def->Options[Event.Option].Applications[Event.NextApplication - 1];
			if (Application.Target == EEffectTarget::TwoChosenRivals)
			{
				return Event.FirstRival == INDEX_NONE ? FString(TEXT("el primer rival, que reta al segundo"))
					: FString::Printf(TEXT("el segundo rival, al que reta el asiento %d"), Event.FirstRival);
			}
			switch (Application.Effect)
			{
			case EEffect::WardenTrial: return TEXT("a quién pones a prueba");
			case EEffect::DecreedTheft: return TEXT("a quién robas");
			case EEffect::ForcedDuel: return TEXT("con quién te bates");
			default: break;
			}
		}
		return TEXT("a quién señalas");
	}

	/**
	 * Estados del asiento Seat que dejan los efectos del Warden (S-014): Marcado y Brazal revelado, con la ronda en que
	 * caducan, para la leyenda de asientos. Vacío si no tiene ninguno. Solo lee el estado.
	 */
	FString EffectStatusLine(const FMatchState& State, int32 Seat, bool bWithInsolence)
	{
		FString Text;
		// S-035 (R-HUD-6): la insolencia y los castigos que esperan son de quien mira y de la depuración; los de los demás no se enseñan. Esto
		// sustituye a la decisión del autor del 05/10/2026 (S-016, B), que los ponía a la vista de todos.
		if (bWithInsolence && State.Seats.IsValidIndex(Seat))
		{
			const FSeatState& SeatState = State.Seats[Seat];
			const int32 Waiting = SeatState.PendingPunishments.Num() + SeatState.VoidPunishments.Num();
			Text += FString::Printf(TEXT(" | insolencia %d de %d"), SeatState.Insolence, Soulbound::Match::MaxInsolence);
			if (Waiting > 0)
			{
				Text += FString::Printf(TEXT(" | %d castigo%s pendiente%s"), Waiting, Waiting > 1 ? TEXT("s") : TEXT(""), Waiting > 1 ? TEXT("s") : TEXT(""));
			}
		}
		if (State.Seats.IsValidIndex(Seat) && State.Seats[Seat].bWardenTrialPending)
		{
			// S-015: la Prueba del Warden pendiente, hasta que la juegue al empezar su siguiente turno.
			Text += TEXT(" | PRUEBA DEL WARDEN");
		}
		if (Soulbound::Rules::IsMarked(State, Seat))
		{
			Text += FString::Printf(TEXT(" | MARCADO hasta la ronda %d"), State.Seats[Seat].MarkedUntilRound);
		}
		if (Soulbound::Rules::IsBracerRevealed(State, Seat))
		{
			Text += FString::Printf(TEXT(" | BRAZAL REVELADO hasta la ronda %d"), State.Seats[Seat].RevealedUntilRound);
		}
		return Text;
	}

	/**
	 * Línea del evento del Warden en curso (S-014): su texto y quién lo dirige; si es un dilema que espera, lo dice (las
	 * opciones, numeradas, van en la línea de petición). Vacía sin evento. Solo lee el estado.
	 */
	FString WardenEventLine(const FMatchState& State)
	{
		const FCurrentWardenEvent& Event = State.CurrentEvent;
		if (!Event.IsActive() || !State.WardenEvents.Events.IsValidIndex(Event.EventIndex))
		{
			return FString();
		}
		const FWardenEventDef& Def = State.WardenEvents.Events[Event.EventIndex];
		// S-017 (R-VOT-1): en una votación no dirige nadie; lo que se vota va en su propia línea.
		if (Def.bVote)
		{
			return FString::Printf(TEXT("EL WARDEN (VOTACIÓN): \"%s\""), *Def.Text);
		}
		const TCHAR* Kind = Def.IsDilemma() ? (Event.Option == INDEX_NONE ? TEXT("DILEMA, elige una opción") : TEXT("DILEMA")) : TEXT("DECRETO");
		const FString Against = Event.bAgainst ? FString(TEXT(" | resuelto EN SU CONTRA")) : FString();
		return FString::Printf(TEXT("EL WARDEN (%s): \"%s\" | dirige el asiento %d%s"), Kind, *Def.Text, Event.Director, *Against);
	}

	/**
	 * Línea de la votación abierta (S-017, R-VOT-1 a 6): qué se vota, quién ha votado ya (a quién, no: los votos se cuentan al
	 * resolverse, decisión del autor) y el tiempo que queda. Al humano, si aún no ha votado y tiene a quién, le dice cómo votar.
	 * Vacía sin votación. Solo lee el estado.
	 */
	FString VoteLine(const FMatchState& State, int32 HumanSeat, bool bHints)
	{
		const FVoteState& Vote = State.Vote;
		if (!Vote.IsOpen())
		{
			return FString();
		}
		TArray<FString> Voted;
		for (int32 Seat = 0; Seat < Vote.Votes.Num(); ++Seat)
		{
			if (Vote.Votes[Seat] != INDEX_NONE)
			{
				Voted.Add(FString::FromInt(Seat));
			}
		}
		const bool bHumanPending = Vote.Votes.IsValidIndex(HumanSeat) && Vote.Votes[HumanSeat] == INDEX_NONE
			&& !Soulbound::Rules::VoteOptions(State, HumanSeat).IsEmpty();
		return FString::Printf(TEXT("VOTACIÓN: %d se pueden votar | han votado: %s | quedan %.0f s%s"), Vote.Votables.Num(),
			Voted.IsEmpty() ? TEXT("nadie") : *FString::Join(Voted, TEXT(", ")), FMath::Max(0.0, Vote.DeadlineSeconds - State.ClockSeconds),
			bHumanPending && bHints ? TEXT("  <- clic en la ficha del asiento al que votas") : TEXT(""));
	}

	/**
	 * Estado del Vacío del asiento Seat para la vista (S-013): "EN EL VACÍO, turno n", con n el turno dentro que juega o, si
	 * no lo está jugando, el siguiente. Vacío si no está en el Vacío. Solo lee el estado.
	 */
	FString VoidText(const FMatchState& State, int32 Seat)
	{
		if (!State.Seats.IsValidIndex(Seat) || !State.Seats[Seat].IsInVoid())
		{
			return FString();
		}
		const bool bPlayingVoidTurn = Seat == State.ActiveSeat && (State.TurnPhase == ETurnPhase::Start || State.TurnPhase == ETurnPhase::SkillTrial)
			&& State.Seats[Seat].VoidTurns > 0;
		// S-016 (B): el Vacío de la Furia, donde no funcionan Desvío ni Liberación.
		return FString::Printf(TEXT("%s, turno %d"), State.Seats[Seat].bFuryVoid ? TEXT("EN EL VACÍO DE LA FURIA (sin Desvío ni Liberación)") : TEXT("EN EL VACÍO"),
			State.Seats[Seat].VoidTurns + (bPlayingVoidTurn ? 0 : 1));
	}

	FString ChoiceWhat(const FMatchState& State)
	{
		switch (State.PendingChoice.Kind)
		{
		case EChoiceKind::Dilemma: return TEXT("una opción del dilema");
		case EChoiceKind::ChosenRival: return ChosenRivalText(State);
		case EChoiceKind::GiftRune: return TEXT("qué runa te regala el Warden");
		case EChoiceKind::LoseRune: return TEXT("qué runa pierdes");
		case EChoiceKind::DiceCount: return TEXT("dados");
		case EChoiceKind::PlanarJump: return TEXT("salto planar");
		case EChoiceKind::SafeRune: return TEXT("la runa que Koschei pone a salvo del trilero");
		case EChoiceKind::ShellPick: return TEXT("esfera");
		case EChoiceKind::Deflection: return TEXT("si sales del Vacío con Desvío (la runa vuelve a su Fuente y juegas el turno)");
		default: return TEXT("accion contra un jugador");
		}
	}

	/**
	 * Línea de petición de la elección pendiente (S-009): qué se elige, las opciones numeradas como las contestan las
	 * teclas (1 a 9, y 0 para "no" o, si no se admite, la décima) y el tiempo que queda. Para el humano, en imperativo y con
	 * sus teclas; para un dummy, qué está decidiendo. Desde S-010 la elección puede ser de un asiento sin turno (Koschei
	 * víctima): bHuman es si se le pide al humano, tenga o no el turno. Solo lee el estado.
	 */
	FString ChoicePrompt(const FMatchState& State, bool bHuman, bool bHints, int32 ViewerSeat)
	{
		const FPendingChoice& Choice = State.PendingChoice;
		const FString Left = FString::Printf(TEXT("quedan %.0f s"), FMath::Max(0.0, Choice.DeadlineSeconds - State.ClockSeconds));
		if (Choice.Kind == EChoiceKind::Reroll)
		{
			// S-021 (A3): lo que ofrece (la tecla R) y el tiempo se muestran siempre; "o clic en una casilla que late" es explicación y sigue el ajuste.
			return bHuman ? FString::Printf(TEXT("REPETIR: [R] repite la tirada (una vez por turno)%s | %s"), bHints ? TEXT(", o clic en una casilla que late") : TEXT(""), *Left)
				: FString::Printf(TEXT("el asiento %d decide si repite la tirada | %s"), Choice.Seat, *Left);
		}
		const FString WhatText = ChoiceWhat(State);
		const TCHAR* What = *WhatText;
		// S-035 (R-RUN-7): las opciones de runa de un dummy (la que pierde, la que regala el Warden, la que pone a salvo) son tipos de su Brazal: no
		// se dicen mientras no se pueda ver. Se dice qué elige y cuánto le queda.
		const bool bRuneOptions = Choice.Kind == EChoiceKind::LoseRune || Choice.Kind == EChoiceKind::GiftRune || Choice.Kind == EChoiceKind::SafeRune;
		if (!bHuman && bRuneOptions && !CanSeeBracer(State, Choice.Seat, ViewerSeat))
		{
			return FString::Printf(TEXT("el asiento %d elige %s | %s"), Choice.Seat, What, *Left);
		}
		const TCHAR* No = Choice.Kind == EChoiceKind::PlanarJump ? TEXT("no saltar") : Choice.Kind == EChoiceKind::Deflection ? TEXT("no") : TEXT("nada");
		TArray<FString> Parts;
		for (int32 Index = 0; Index < Choice.AnswerCount(); ++Index)
		{
			Parts.Add(FString::Printf(TEXT("%s %s"), *OptionKey(Choice, Index), *OptionText(State, Index)));
		}
		if (Choice.CanDecline())
		{
			Parts.Add(FString::Printf(TEXT("[0] %s"), No));
		}
		const FString Hint = bHints && bHuman && Choice.Kind == EChoiceKind::PlanarJump ? TEXT(" (o clic en un Nodo con halo)") : TEXT("");
		return bHuman ? FString::Printf(TEXT("ELIGE %s: %s%s | %s"), What, *FString::Join(Parts, TEXT(" ")), *Hint, *Left)
			: FString::Printf(TEXT("el asiento %d elige %s: %s | %s"), Choice.Seat, What, *FString::Join(Parts, TEXT(" ")), *Left);
	}

	/**
	 * Línea del trilero en curso (S-010), sin animación (decisión del autor del 05/10/2026): quién roba a quién; durante la
	 * muestra, las runas de la víctima a la vista (y la que apartó Koschei); durante la mezcla, solo cuántas esferas; en la
	 * elección, las esferas numeradas. Con el tiempo de cada etapa. Vacía si no hay trilero. Solo lee el estado.
	 */
	FString ShellGameLine(const FMatchState& State)
	{
		const FShellGameState& Game = State.ShellGame;
		// S-015: el de un evento del Warden lo dice: el Robo decretado, o el botín de un Duelo forzado.
		EEffect EventEffect = EEffect::Halt;
		const bool bFromEvent = EventActionEffect(State, EventEffect);
		const TCHAR* Header = !bFromEvent ? TEXT("TRILERO") : EventEffect == EEffect::DecreedTheft ? TEXT("ROBO DECRETADO") : TEXT("DUELO FORZADO (botín: trilero)");
		const FString Who = FString::Printf(TEXT("%s: el asiento %d roba al asiento %d"), Header, Game.Thief, Game.Victim);
		const double StageLeft = FMath::Max(0.0, Game.StageDeadlineSeconds - State.ClockSeconds);
		switch (Game.Stage)
		{
		case EShellGameStage::SafeRune:
			return Who + TEXT(" (Koschei): antes aparta una runa, que no entra en la mezcla");
		case EShellGameStage::Reveal:
		{
			const FString Runes = FString::JoinBy(Game.Orbs, TEXT(" "), [](int32 Rune) { return FString::Printf(TEXT("D%d"), Rune); });
			const FString Aside = Game.SetAsideRune != INDEX_NONE ? FString::Printf(TEXT(" (a salvo: D%d)"), Game.SetAsideRune) : FString();
			return FString::Printf(TEXT("%s | runas a la vista: %s%s | muestra: quedan %.0f s"), *Who, *Runes, *Aside, StageLeft);
		}
		case EShellGameStage::Shuffle:
			return FString::Printf(TEXT("%s | mezclando %d esferas | quedan %.0f s"), *Who, Game.Orbs.Num(), StageLeft);
		case EShellGameStage::Pick:
		{
			TArray<FString> Orbs;
			for (int32 Index = 0; Index < Game.Orbs.Num(); ++Index)
			{
				Orbs.Add(FString::Printf(TEXT("(%d)"), Index + 1));
			}
			return FString::Printf(TEXT("%s | esferas: %s | el ladron elige a ciegas"), *Who, *FString::Join(Orbs, TEXT(" ")));
		}
		case EShellGameStage::None:
		default:
			return FString();
		}
	}

	/**
	 * Línea del duelo (S-012): mientras dura, quién reta a quién, de dónde sale el reto y quién ha rendido ya; resuelto, el
	 * resultado del último duelo del turno en curso, hasta que acabe. Vacía si el turno no tiene duelo. Solo lee el estado.
	 */
	FString DuelLine(const FMatchState& State, int32 HumanSeat, bool bHumanRing)
	{
		const FDuelState& Duel = State.Duel;
		// S-026: con el interruptor encendido, el duelo del humano se juega con el anillo; el de dos dummies sigue al azar. Apagado, los
		// textos de siempre.
		const bool bRingDuel = bHumanRing && Duel.IsParticipant(HumanSeat);
		const TCHAR* const How = bRingDuel ? TEXT("con el anillo: clic izquierdo") : bHumanRing ? TEXT("resuelto al azar") : TEXT("resuelto al azar (sin minijuego aún)");
		auto Who = [HumanSeat](int32 Seat) { return Seat == HumanSeat ? FString::Printf(TEXT("%d (tú)"), Seat) : FString::FromInt(Seat); };
		if (Duel.IsActive())
		{
			auto Status = [&Duel](int32 Seat) { return Duel.HasPerformed(Seat) ? TEXT("ha rendido") : TEXT("rindiendo"); };
			// S-015: el Duelo contra el Warden (rinde un valor fijo) y el Duelo forzado de un evento, entre turnos.
			if (Duel.bAgainstWarden)
			{
				return FString::Printf(TEXT("DUELO CONTRA EL WARDEN: el asiento %s se bate contra el Warden (rinde %.2f) por %s | %s | %s"),
					*Who(Duel.Challenged), Duel.ChallengerPerformance, Duel.bAllRunes ? TEXT("todas sus runas") : TEXT("una de sus runas"), Status(Duel.Challenged), How);
			}
			if (State.CurrentEvent.IsActive() && State.CurrentEvent.bWaitsForAction)
			{
				return FString::Printf(TEXT("DUELO FORZADO: el asiento %s reta al asiento %s por %s | retador %s, retado %s | %s"),
					*Who(Duel.Challenger), *Who(Duel.Challenged), Duel.bAllRunes ? TEXT("todas sus runas") : TEXT("una runa"),
					Status(Duel.Challenger), Status(Duel.Challenged), How);
			}
			return FString::Printf(TEXT("DUELO: el asiento %s reta al asiento %s%s | retador %s, retado %s | %s"),
				*Who(Duel.Challenger), *Who(Duel.Challenged), Duel.bFromDuelNode ? TEXT(" desde un Nodo de Duelo") : TEXT(""),
				Status(Duel.Challenger), Status(Duel.Challenged), How);
		}
		if (State.TurnPhase == ETurnPhase::BetweenTurns)
		{
			return FString();
		}
		// El último resultado, si es de este turno: nada desde el último TurnStarted.
		for (int32 Index = State.EventLog.Num() - 1; Index >= 0; --Index)
		{
			const FMatchEvent& Event = State.EventLog[Index];
			if (Event.Type == EMatchEventType::TurnStarted)
			{
				break;
			}
			if (Event.Type == EMatchEventType::DuelWon)
			{
				const bool bRingResult = bHumanRing && (Event.Seat == HumanSeat || Event.OtherSeat == HumanSeat);
				return FString::Printf(TEXT("DUELO: gana el asiento %s al asiento %s, siendo el %s (%s)"), *Who(Event.Seat), *Who(Event.OtherSeat),
					Event.Value == 1 ? TEXT("retado") : TEXT("retador"), bRingResult ? TEXT("con el anillo") : TEXT("resuelto al azar"));
			}
		}
		return FString();
	}

	/** Nombre del Elegido del Dominio, de elegidos.json (S-008); vacío si el Dominio no tiene Elegido. */
	FString ElectName(const FMatchState& State, int32 DomainId)
	{
		return State.Elects.IsValidDomain(DomainId) ? State.Elects.Elects[DomainId].Name : FString();
	}

	/** "El Banquete · Shuten-dōji": el nombre del Dominio (del tablero) y el de su Elegido (de elegidos.json). */
	FString DomainTitle(const FMatchState& State, int32 DomainId)
	{
		const FString Domain = State.Board.Domains.IsValidIndex(DomainId) ? State.Board.Domains[DomainId].Name : FString();
		const FString Elect = ElectName(State, DomainId);
		return Elect.IsEmpty() ? Domain : FString::Printf(TEXT("%s · %s"), *Domain, *Elect);
	}

	const TCHAR* PowerName(EPower Power)
	{
		switch (Power)
		{
		case EPower::Duel: return TEXT("Duelo");
		case EPower::Haste: return TEXT("Celeridad");
		case EPower::FreePassage: return TEXT("Paso Libre");
		case EPower::Expel: return TEXT("Expulsión");
		case EPower::PlanarFlight: return TEXT("Vuelo Planar");
		case EPower::Deflection: return TEXT("Desvío");
		}
		return TEXT("?");
	}

	/**
	 * Brazal de un asiento (S-008, solo lectura): sus runas por tipo (D<n>×cantidad, solo las que tiene) y sus poderes
	 * activos. Paso Libre por afinidad (Fenrir) se marca aparte, porque no depende de ninguna runa.
	 */
	bool CanSeeBracer(const FMatchState& State, int32 Seat, int32 ViewerSeat)
	{
		return (State.Seats.IsValidIndex(ViewerSeat) && Seat == ViewerSeat) || Soulbound::Rules::IsBracerRevealed(State, Seat);
	}

	FString EffectStatusText(const FMatchState& State, int32 Seat, int32 ViewerSeat)
	{
		return EffectStatusLine(State, Seat, State.Seats.IsValidIndex(ViewerSeat) && Seat == ViewerSeat);
	}

	FString EffectStatusTextAll(const FMatchState& State, int32 Seat)
	{
		return EffectStatusLine(State, Seat, true);
	}

	FString BracerText(const FMatchState& State, int32 Seat, int32 ViewerSeat)
	{
		const FSeatState& SeatState = State.Seats[Seat];
		TArray<FString> Runes;
		for (int32 RuneType = 0; RuneType < SeatState.Runes.Num(); ++RuneType)
		{
			if (SeatState.Runes[RuneType] > 0)
			{
				Runes.Add(FString::Printf(TEXT("D%d×%d"), RuneType, SeatState.Runes[RuneType]));
			}
		}
		TArray<FString> Powers;
		const bool bSees = CanSeeBracer(State, Seat, ViewerSeat);
		if (bSees)
		{
			for (const EPower Power : Soulbound::Rules::GetActivePowers(State, Seat))
			{
				Powers.Add(PowerName(Power));
			}
		}
		// La afinidad de un Elegido es pública: Paso Libre por afinidad se dice aunque el Brazal esté oculto.
		if (Soulbound::Rules::MakeMoverTraits(State, Seat).bFreePassage && !Soulbound::Rules::IsPowerActive(State, Seat, EPower::FreePassage))
		{
			Powers.Add(TEXT("Paso Libre (afinidad)"));
		}
		if (!bSees)
		{
			// S-035 (R-RUN-7): de un Brazal oculto, solo el total.
			int32 Total = 0;
			for (const int32 Count : SeatState.Runes)
			{
				Total += Count;
			}
			return FString::Printf(TEXT("%d: %d runa%s%s"), Seat, Total, Total == 1 ? TEXT("") : TEXT("s"),
				Powers.IsEmpty() ? TEXT("") : *(FString(TEXT(" | poderes: ")) + FString::Join(Powers, TEXT(", "))));
		}
		// Textos de la leyenda revisados por el autor (13/11/2026): el propio lleva "(tú)"; el de otro, visible solo por la revelación, "(Brazal revelado)".
		const bool bOwn = State.Seats.IsValidIndex(ViewerSeat) && Seat == ViewerSeat;
		// Retoque del autor (13/11/2026): sin runas, "0 runas" y no "-"; sin poderes, se omite el trozo.
		return FString::Printf(TEXT("%d%s: %s%s"), Seat, bOwn ? TEXT(" (tú)") : TEXT(" (Brazal revelado)"),
			Runes.IsEmpty() ? TEXT("0 runas") : *FString::Join(Runes, TEXT(" ")),
			Powers.IsEmpty() ? TEXT("") : *(FString(TEXT(" | poderes: ")) + FString::Join(Powers, TEXT(", "))));
	}

	FString DomainLegendText(const FMatchState& State, int32 DomainId, bool bForgotten)
	{
		if (!State.Board.Domains.IsValidIndex(DomainId))
		{
			return FString::Printf(TEXT("D%d"), DomainId);
		}
		const FString Title = DomainTitle(State, DomainId);
		if (bForgotten)
		{
			return FString::Printf(TEXT("D%d %s: Olvidado"), DomainId, *Title);
		}
		const int32 Owner = State.Board.Domains[DomainId].OwnerSeat;
		return Owner == INDEX_NONE ? FString::Printf(TEXT("D%d %s: libre"), DomainId, *Title)
			: FString::Printf(TEXT("D%d %s: del asiento %d"), DomainId, *Title, Owner);
	}

	FString SeatLegendText(const FMatchState& State, int32 Seat, int32 ViewerSeat)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return FString::Printf(TEXT("%d"), Seat);
		}
		const FSeatState& SeatState = State.Seats[Seat];
		const FString Who = State.Seats.IsValidIndex(ViewerSeat) && Seat == ViewerSeat ? FString::Printf(TEXT("%d (tú)"), Seat) : FString::FromInt(Seat);
		// S-013: la ficha desterrada no está en el tablero; aquí se dice dónde está. S-014: Marcado y Brazal revelado van detrás del Vacío.
		const FString Void = VoidText(State, Seat);
		const FString After = (Void.IsEmpty() ? FString() : TEXT(" | ") + Void) + EffectStatusText(State, Seat, ViewerSeat);
		if (SeatState.Role == ESeatRole::Elect)
		{
			const FString Name = ElectName(State, SeatState.DomainId);
			return FString::Printf(TEXT("%s: Elegido, %s%s"), *Who, Name.IsEmpty() ? *FString::Printf(TEXT("D%d"), SeatState.DomainId) : *Name, *After);
		}
		return SeatState.DomainId == INDEX_NONE ? FString::Printf(TEXT("%s: Eco%s"), *Who, *After)
			: FString::Printf(TEXT("%s: Eco, con hogar en D%d%s"), *Who, SeatState.DomainId, *After);
	}
}

namespace
{
	/** Los colores de las líneas de antes de S-027 (FColor de los mensajes de pantalla), para el panel. */
	FLinearColor StatusLineColor(const FColor& Color)
	{
		return FLinearColor(Color);
	}

	const FLinearColor StatusOrange = StatusLineColor(FColor::Orange);
	const FLinearColor StatusYellow = StatusLineColor(FColor::Yellow);
	const FLinearColor StatusGreen = StatusLineColor(FColor::Green);
	const FLinearColor StatusGray = StatusLineColor(FColor(190, 190, 190));

	/** Altura de pantalla a la que PanelFontSize y PanelWidthPixels valen lo que dicen (R-HUD-3). */
	constexpr float StatusReferenceHeight = 1080.f;

	/** La clave de un enumerado en los datos: su nombre con la inicial en minúscula, como en elegidos.json ("FreePassage" pasa a "freePassage"). */
	FString GrimoireDataKey(const FString& GrimoireEnumName)
	{
		return GrimoireEnumName.IsEmpty() ? GrimoireEnumName : GrimoireEnumName.Left(1).ToLower() + GrimoireEnumName.Mid(1);
	}

	FString GrimoireEnumName(EAffinity Affinity)
	{
		return StaticEnum<EAffinity>()->GetNameStringByValue(static_cast<int64>(Affinity));
	}

	FString GrimoireEnumName(EPower Power)
	{
		return StaticEnum<EPower>()->GetNameStringByValue(static_cast<int64>(Power));
	}

	constexpr EAffinity GrimoireAffinities[] = {EAffinity::DuelAdvantage, EAffinity::FreePassage, EAffinity::RerollOnce, EAffinity::VoidEscape, EAffinity::RefuseOracle,
		EAffinity::SafeRune};
	constexpr EPower GrimoirePowers[] = {EPower::Duel, EPower::Haste, EPower::FreePassage, EPower::Expel, EPower::PlanarFlight, EPower::Deflection};
	static_assert(UE_ARRAY_COUNT(GrimoireAffinities) == static_cast<int32>(EAffinity::SafeRune) + 1, "Todas las afinidades");
	static_assert(UE_ARRAY_COUNT(GrimoirePowers) == static_cast<int32>(EPower::Deflection) + 1, "Todos los poderes");
}

namespace Soulbound::Presentation
{
	FString GoalLineText(const FMatchState& State, int32 HumanSeat)
	{
		// R-HUD-7: un texto por meta (R-VIC-5); N son las runas distintas del Brazal del humano.
		const FSeatGoal Goal = Soulbound::Rules::SeatGoal(State, HumanSeat);
		switch (Goal.Kind)
		{
		case ESeatGoal::Niche:
			return TEXT("Llega a un Nicho libre");
		case ESeatGoal::Runes:
		{
			const FSeatState& Seat = State.Seats[HumanSeat];
			const int32 Count = Soulbound::Rules::RuneTypeCount(Seat);
			return Seat.Role == ESeatRole::Elect
				? FString::Printf(TEXT("Consigue runas en las Fuentes (%d/%d)"), Count, Soulbound::Board::DomainCount)
				: FString::Printf(TEXT("Consigue runas en las Fuentes de los Olvidados, o róbalas (%d/%d)"), Count, Soulbound::Board::DomainCount);
		}
		case ESeatGoal::Altar:
			// Corrección del autor del 16/11/2026 tras PIE: ya en el Altar con los seis (aspirante), lo que falta es el desafío final. SeatGoal no cambia.
			return Soulbound::Rules::IsAspirant(State, HumanSeat) ? TEXT("Supera el desafío final en tu próximo turno") : TEXT("Vuelve al Altar");
		default:
			return FString();
		}
	}

	TArray<FPanelLine> GoalPanelLines(const FMatchState& State, const FStatusContext& Context)
	{
		// R-HUD-7: una sola línea, grande y con su color; en el prólogo no sale (precisión del 16/11/2026), y con la meta None GoalLineText está vacía.
		TArray<FPanelLine> Lines;
		if (!Context.bPrologue)
		{
			const FString Goal = GoalLineText(State, Context.HumanSeat);
			if (!Goal.IsEmpty())
			{
				Lines.Add({Goal, Context.GoalLineColor, true});
			}
		}
		return Lines;
	}

	bool IsGoalPanelShown(bool bPrologue, ESeatGoal Goal, bool bWardenLineOnScreen)
	{
		// R-HUD-7 (corrección del 16/11/2026): en partida, con meta, y sin frase del Warden en pantalla; ninguna tecla.
		return !bPrologue && Goal != ESeatGoal::None && !bWardenLineOnScreen;
	}

	TArray<FPanelLine> StatusLines(const FMatchState& State, const FStatusContext& Context)
	{
		// Lo de antes de S-027 (ABoardViewActor::TickHud, líneas 0, 1, 3, 6 y de la 8 a la 19), en el mismo orden y con los mismos colores. Arriba
		// y grandes, el resultado y la petición al jugador (R-HUD-1; decisión del autor del 9/11/2026 para el resultado).
		TArray<FPanelLine> Top;
		TArray<FPanelLine> Lines;
		const int32 HumanSeat = Context.HumanSeat;
		const bool bOver = State.IsMatchOver();
		const bool bWaiting = State.TurnPhase == ETurnPhase::RollAndChoose;
		const bool bHumanTurn = bWaiting && State.ActiveSeat == HumanSeat;
		const bool bHints = Context.bHints;
		const int32 TurnNumber = State.TurnPhase == ETurnPhase::BetweenTurns ? State.TurnCount : State.TurnCount + 1;

		// Fase de la partida (línea 0): la última que aplica manda, como cuando una sustituía a otra en la misma clave. Sin las teclas de la vista:
		// están en "Cómo se juega" y en SbHelp.
		const bool bSomeoneActive = !bOver && State.TurnPhase != ETurnPhase::BetweenTurns;
		FPanelLine Phase;
		if (Context.bPrologue)
		{
			// S-022 (R-PRO-1): el Warden abre la partida; aún no hay turno.
			Phase = {TEXT("Prólogo: el Warden abre la partida; el primer turno empieza cuando termine"), StatusOrange};
		}
		else
		{
			Phase = {FString::Printf(TEXT("Turno %d | juega: asiento %d (%s) | fase: %s"), TurnNumber, State.ActiveSeat,
				State.ActiveSeat == HumanSeat ? TEXT("humano") : TEXT("dummy"), PhaseName(State.TurnPhase)), bSomeoneActive ? Context.SeatColor(State.ActiveSeat) : FLinearColor::White};
		}
		// S-015: un duelo o un trilero de un evento se juega entre turnos; el asiento activo es a quien le toca después.
		if (State.CurrentEvent.IsActive() && State.CurrentEvent.bWaitsForAction && !bOver)
		{
			Phase = {FString::Printf(TEXT("Entre turnos | evento del Warden en curso | fase: %s | el siguiente turno es del asiento %d"), PhaseName(State.TurnPhase),
				State.ActiveSeat), StatusOrange};
		}
		// S-023: la mesa espera al Warden; el reloj de partida sigue, el de turno no.
		if (Context.bWardenSpeaking && !bOver)
		{
			Phase = {TEXT("El Warden habla: la mesa espera a que termine"), StatusOrange};
		}
		Lines.Add(Phase);

		// Prueba de habilidad o tirada (línea 1). S-026: con el interruptor encendido, la prueba del humano se juega con el anillo.
		const bool bHumanRingTrial = Context.bHumanRing && State.ActiveSeat == HumanSeat;
		const TCHAR* const RingHow = TEXT("con el anillo: clic izquierdo, o con SbTrial");
		if (State.TurnPhase == ETurnPhase::SkillTrial && State.PendingTrial == ESkillTrial::FinalChallenge)
		{
			Lines.Add({FString::Printf(TEXT("DESAFÍO FINAL: el asiento %d juega en vez de su turno, sin tirada, y espera el resultado (%s)"), State.ActiveSeat,
				bHumanRingTrial ? RingHow : TEXT("resuelto al azar, o con SbTrial")), Context.SeatColor(State.ActiveSeat)});
		}
		else if (State.TurnPhase == ETurnPhase::SkillTrial && State.PendingTrial == ESkillTrial::VoidEscape)
		{
			Lines.Add({FString::Printf(TEXT("Escape del Vacío: el asiento %d juega (%s) y espera el resultado (%s)"), State.ActiveSeat,
				*VoidText(State, State.ActiveSeat), bHumanRingTrial ? RingHow : TEXT("resuelto al azar, o con SbTrial")), StatusYellow});
		}
		else if (State.TurnPhase == ETurnPhase::SkillTrial && State.PendingTrial == ESkillTrial::WardenTrial)
		{
			Lines.Add({FString::Printf(TEXT("PRUEBA DEL WARDEN: el asiento %d juega antes de tirar y espera el resultado (%s)"), State.ActiveSeat,
				bHumanRingTrial ? RingHow : TEXT("resuelta al azar, o con SbTrial")), StatusYellow});
		}
		else if (State.TurnPhase == ETurnPhase::SkillTrial)
		{
			Lines.Add({FString::Printf(TEXT("Minijuego de la Fuente: el asiento %d espera el resultado (%s)"), State.ActiveSeat,
				bHumanRingTrial ? RingHow : TEXT("resuelto al azar, probabilidad 0,5")), StatusYellow});
		}
		else
		{
			const FString Clock = bWaiting ? FString::Printf(TEXT("%.0f s"), FMath::Max(0.0, State.TurnDeadlineSeconds - State.ClockSeconds)) : FString(TEXT("-"));
			// S-039 (R-HUD-1): "Dado: N" con un dado y "Dados: N" con dos (N es la suma): "Tirada 1" se leía como el contador de turno.
			const FPanelLine Roll = {FString::Printf(TEXT("%s: %d | reloj de turno: %s%s"), State.DiceCount > 1 ? TEXT("Dados") : TEXT("Dado"), State.Roll, *Clock,
				bHumanTurn && bHints ? TEXT("  <- tu turno: clic en una casilla que late") : TEXT("")), bHumanTurn ? StatusGreen : FLinearColor::White, bHumanTurn};
			// En su turno de tirada, la tirada es la petición al jugador: elegir casilla. Con el panel de decisión a la vista ya está allí (R-DEC-3, S-028;
			// corrección del autor del 15/11/2026, S-039): la línea del humano no se repite; la de otro asiento sigue.
			if (!(bHumanTurn && Context.bDecisionPanelShown))
			{
				(bHumanTurn ? Top : Lines).Add(Roll);
			}
		}

		// Sellos activos (línea 3).
		const FString Sealed = State.Board.SealedTiles.IsEmpty()
			? FString(TEXT("ninguno"))
			: FString::JoinBy(State.Board.SealedTiles, TEXT(", "), [](int32 Id) { return FString::FromInt(Id); });
		Lines.Add({FString::Printf(TEXT("Sellos activos: %d (casillas: %s)"), State.Board.SealedTiles.Num(), *Sealed), StatusLineColor(FColor::Red)});

		// Carrera por el vínculo (línea 6, S-006 y S-018): tiempo que queda, fin, o el resultado.
		FString Race;
		FLinearColor RaceColor = StatusLineColor(FColor::Cyan);
		if (State.Outcome == EMatchOutcome::PlayerVictory)
		{
			Race = FString::Printf(TEXT("VICTORIA DEL ASIENTO %d: supera el desafío final. La partida ha terminado."), State.WinnerSeat);
			RaceColor = Context.SeatColor(State.WinnerSeat);
		}
		else if (State.Outcome == EMatchOutcome::WardenVictory)
		{
			Race = State.EndReason == EMatchEndReason::TimeOut ? TEXT("VICTORIA DEL WARDEN: se acaba el tiempo. La partida ha terminado.")
				: TEXT("VICTORIA DEL WARDEN: nadie se ha vinculado. La partida ha terminado.");
			RaceColor = StatusLineColor(FColor::Red);
		}
		else if (State.MatchPhase == EMatchPhase::BondRace)
		{
			const double Left = State.Params.RaceDurationSeconds - State.ClockSeconds;
			Race = Left > 0.0
				? FString::Printf(TEXT("Carrera por el vinculo: quedan %.0f s"), Left)
				: FString(TEXT("Carrera por el vinculo: tiempo agotado, termina al acabar este turno"));
		}
		else
		{
			const int32 Elects = State.Seats.FilterByPredicate([](const FSeatState& Seat) { return Seat.Role == ESeatRole::Elect; }).Num();
			Race = FString::Printf(TEXT("FIN DE LA CARRERA: %d Elegidos. Los Ecos quedan confinados a los Dominios Olvidados."), Elects);
			if (State.WardenParams.HasWarden())
			{
				const int32 Left = FMath::Max(0, FMath::CeilToInt32(State.WardenParams.MatchEndSeconds - State.ClockSeconds));
				Race += FString::Printf(TEXT(" | fin de la partida en %02d:%02d"), Left / 60, Left % 60);
			}
		}
		Lines.Add({Race, RaceColor});

		// Elección pendiente (línea 8, S-009): la del humano es la petición; la de un dummy, en gris y en su sitio.
		if (State.PendingChoice.IsOpen() && !bOver)
		{
			const bool bHumanChoice = State.PendingChoice.Seat == HumanSeat;
			if (bHumanChoice)
			{
				// S-028 (R-DEC-3): con el panel de decisión a la vista, la petición ya está allí; aquí no se repite.
				if (!Context.bDecisionPanelShown)
				{
					Top.Insert({ChoicePrompt(State, true, bHints, HumanSeat), StatusGreen, true}, 0);
				}
			}
			else
			{
				Lines.Add({ChoicePrompt(State, false, bHints, HumanSeat), StatusGray});
			}
		}

		// Trilero (línea 9, S-010).
		if (State.ShellGame.IsActive() && !bOver)
		{
			Lines.Add({ShellGameLine(State), StatusLineColor(FColor::Magenta)});
		}

		// Duelo (línea 10, S-012).
		const FString Duel = bOver ? FString() : DuelLine(State, HumanSeat, Context.bHumanRing);
		if (!Duel.IsEmpty())
		{
			Lines.Add({Duel, StatusOrange});
		}

		// Evento del Warden (línea 11, S-014).
		const FString Event = bOver ? FString() : WardenEventLine(State);
		if (!Event.IsEmpty())
		{
			Lines.Add({Event, StatusOrange});
		}

		// Noche de los Ecos (línea 12, S-014).
		if (State.IsEchoNight() && !bOver)
		{
			Lines.Add({FString::Printf(TEXT("NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio (como mucho hasta la ronda %d)"), State.EchoNightUntilRound),
				StatusLineColor(FColor(150, 120, 255))});
		}

		// Oráculo (línea 13, S-017).
		if (State.Oracle.IsWaiting() && !bOver)
		{
			const int32 Consultant = State.Oracle.Consultant;
			const bool bHumanOracle = Consultant == HumanSeat;
			const FString Action = bHumanOracle ? (!bHints ? FString() : FString::Printf(TEXT("  <- clic en el orbe sobre tu ficha para consultar%s"), State.Oracle.bRefusable ? TEXT(", clic derecho para rechazar (Tifón)") : TEXT("")))
				: FString(TEXT(" | el orbe sobre su ficha"));
			Lines.Add({FString::Printf(TEXT("ORÁCULO: consulta el asiento %d (%s) | quedan %.0f s%s"), Consultant, *OracleOriginText(static_cast<int32>(State.Oracle.Origin)),
				FMath::Max(0.0, State.Oracle.DeadlineSeconds - State.ClockSeconds), *Action), bHumanOracle ? StatusGreen : StatusOrange});
		}

		// Votación (línea 14, S-017).
		if (State.Vote.IsOpen() && !bOver)
		{
			Lines.Add({VoteLine(State, HumanSeat, bHints), StatusOrange});
		}

		// Aspirantes (líneas 15 a 18, S-018, R-PRO-4).
		for (int32 Seat = 0; Seat < State.Seats.Num() && !bOver; ++Seat)
		{
			if (Soulbound::Rules::IsAspirant(State, Seat))
			{
				Lines.Add({FString::Printf(TEXT("ASPIRANTE: el asiento %d está en el Altar con las seis runas; en su próximo turno, el desafío final"), Seat), Context.SeatColor(Seat)});
			}
		}

		// El resultado (línea 19, S-018 y S-022): arriba del todo y grande.
		if (bOver)
		{
			Top.Insert({MatchResultText(State, Context.ReturnKey), RaceColor, true}, 0);
		}

		// La línea de objetivo (R-HUD-7, S-040) no va aquí desde la corrección del 16/11/2026 tras PIE: tiene su caja encima (GoalPanelLines).

		Top.Append(Lines);
		return Top;
	}

	TArray<FPanelLine> DebugLines(const FMatchState& State, const FStatusContext& Context)
	{
		// Lo de antes de S-027 que era de depuración (ABoardViewActor::TickHud, líneas 2, 4, 5 y 7): mismos textos y colores.
		TArray<FPanelLine> Lines;

		// Paso a paso (línea 2, S-007). En el prólogo (S-022) el reloj también espera, pero no al paso.
		if (Context.bStepWait && !Context.bPrologue)
		{
			Lines.Add({FString::Printf(TEXT("PASO A PASO: espacio para el siguiente turno | teclas: %s"), *Context.ViewKeys), StatusYellow});
		}
		else if (Context.bStepMode)
		{
			Lines.Add({FString::Printf(TEXT("PASO A PASO activo: la partida se detendra al acabar este turno | teclas: %s"), *Context.ViewKeys), StatusLineColor(FColor(200, 200, 90))});
		}

		// El Warden en una línea (línea 4, S-016): acto, segundos hasta la siguiente evaluación y desde la última intervención, y su modo.
		static const TCHAR* const ActNames[] = {TEXT("I"), TEXT("II"), TEXT("III")};
		const int32 Act = static_cast<int32>(Soulbound::Rules::CurrentAct(State, State.ClockSeconds));
		const FString Next = !State.WardenParams.HasWarden() ? FString(TEXT("sin Warden"))
			: !Context.bWardenOn ? FString(TEXT("apagado (SbWardenMode auto)"))
			: State.Warden.bDeferred ? FString(TEXT("evaluación aplazada hasta que acabe lo que está en curso"))
			// Vencida (la partida detenida en el paso a paso, que no evalúa): evaluará en cuanto siga.
			: State.Warden.NextEvaluationSeconds <= State.ClockSeconds ? FString(TEXT("siguiente evaluación: pendiente"))
			: FString::Printf(TEXT("siguiente evaluación en %.0f s"), State.Warden.NextEvaluationSeconds - State.ClockSeconds);
		Lines.Add({FString::Printf(TEXT("Warden: Acto %s | %s | última intervención hace %.0f s%s"), ActNames[FMath::Clamp(Act, 0, 2)], *Next,
			State.ClockSeconds - State.Warden.LastEffectSeconds, State.Warden.PendingScene != INDEX_NONE ? TEXT(" | una escena espera al hueco") : TEXT("")), StatusOrange});

		// S-036 (R-HUD-1): el nivel de la prueba ya no se lee en el anillo ni en el panel de estado; el autor lo ve aquí mientras hay una prueba pendiente.
		if (State.TurnPhase == ETurnPhase::SkillTrial)
		{
			Lines.Add({FString::Printf(TEXT("Prueba pendiente: %s, nivel %s"), *Soulbound::Presentation::RingTitleText(State.PendingTrial, false),
				*DifficultyText(static_cast<int32>(State.PendingDifficulty))), StatusYellow});
		}

		// Semilla, tiempo y ronda (línea 5).
		Lines.Add({FString::Printf(TEXT("Semilla: %d (fijala con FixedSeed para repetir la partida) | t=%.0f s | ronda %d"), Context.Seed, State.ClockSeconds, State.Round),
			StatusLineColor(FColor::Silver)});

		// Los asientos con su rol, su Vacío y sus estados, la insolencia incluida (línea 7).
		TArray<FString> Roles;
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			const FSeatState& SeatState = State.Seats[Seat];
			const FString Void = VoidText(State, Seat);
			Roles.Add((SeatState.Role == ESeatRole::Elect
				? FString::Printf(TEXT("%d Elegido (Dominio %d)"), Seat, SeatState.DomainId)
				: SeatState.DomainId == INDEX_NONE
					? FString::Printf(TEXT("%d Eco"), Seat)
					: FString::Printf(TEXT("%d Eco, hogar: Nicho del Dominio %d"), Seat, SeatState.DomainId))
				+ (Void.IsEmpty() ? FString() : TEXT(", ") + Void) + EffectStatusTextAll(State, Seat));
		}
		Lines.Add({FString::Printf(TEXT("Asientos: %s"), *FString::Join(Roles, TEXT(" | "))), StatusLineColor(FColor::Cyan)});
		return Lines;
	}

	int32 ScaledPanelFontSize(float ViewportHeight, int32 FontSizeAt1080, int32 MinFontSize)
	{
		const float Scaled = ViewportHeight > 0.f ? static_cast<float>(FontSizeAt1080) * ViewportHeight / StatusReferenceHeight : 0.f;
		return FMath::Max(MinFontSize, FMath::RoundToInt(Scaled));
	}

	float ScaledPanelWidth(float ViewportHeight, float WidthAt1080)
	{
		return ViewportHeight > 0.f && WidthAt1080 > 0.f ? WidthAt1080 * ViewportHeight / StatusReferenceHeight : 0.f;
	}

	FGrimoireTextsLoadResult ParseGrimoireTexts(const FString& Json)
	{
		FGrimoireTextsLoadResult Result;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			Result.Message = TEXT("grimorio.json no es un objeto JSON: el Grimorio enseña los nombres de los enumerados.");
			return Result;
		}

		TArray<FString> Missing;
		const TSharedPtr<FJsonObject>* Affinities = nullptr;
		Root->TryGetObjectField(TEXT("afinidades"), Affinities);
		for (const EAffinity Affinity : GrimoireAffinities)
		{
			const FString Key = GrimoireDataKey(GrimoireEnumName(Affinity));
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			FString Name;
			FString Text;
			if (Affinities && (*Affinities)->TryGetObjectField(Key, Entry))
			{
				(*Entry)->TryGetStringField(TEXT("nombre"), Name);
				(*Entry)->TryGetStringField(TEXT("frase"), Text);
			}
			if (Name.IsEmpty())
			{
				Missing.Add(FString::Printf(TEXT("afinidades.%s.nombre"), *Key));
			}
			else
			{
				Result.Texts.AffinityNames.Add(Affinity, Name);
			}
			if (Text.IsEmpty())
			{
				Missing.Add(FString::Printf(TEXT("afinidades.%s.frase"), *Key));
			}
			else
			{
				Result.Texts.AffinityTexts.Add(Affinity, Text);
			}
		}

		const TSharedPtr<FJsonObject>* Powers = nullptr;
		Root->TryGetObjectField(TEXT("poderes"), Powers);
		for (const EPower Power : GrimoirePowers)
		{
			const FString Key = GrimoireDataKey(GrimoireEnumName(Power));
			FString Text;
			if (Powers)
			{
				(*Powers)->TryGetStringField(Key, Text);
			}
			if (Text.IsEmpty())
			{
				Missing.Add(FString::Printf(TEXT("poderes.%s"), *Key));
			}
			else
			{
				Result.Texts.PowerTexts.Add(Power, Text);
			}
		}

		if (!Missing.IsEmpty())
		{
			Result.Message = FString::Printf(TEXT("grimorio.json: faltan %s; el Grimorio enseña el nombre del enumerado en su lugar."), *FString::Join(Missing, TEXT(", ")));
		}
		return Result;
	}

	FGrimoireTextsLoadResult LoadGrimoireTexts(const FString& FilePath)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			FGrimoireTextsLoadResult Result;
			Result.Message = FString::Printf(TEXT("No se puede leer %s: el Grimorio enseña los nombres de los enumerados."), *FilePath);
			return Result;
		}
		return ParseGrimoireTexts(Json);
	}

	FString AffinityName(const FGrimoireTexts& Texts, EAffinity Affinity)
	{
		const FString* Name = Texts.AffinityNames.Find(Affinity);
		return Name ? *Name : GrimoireEnumName(Affinity);
	}

	FString AffinityText(const FGrimoireTexts& Texts, EAffinity Affinity)
	{
		const FString* Text = Texts.AffinityTexts.Find(Affinity);
		return Text ? *Text : GrimoireEnumName(Affinity);
	}

	FString PowerText(const FGrimoireTexts& Texts, EPower Power)
	{
		const FString* Text = Texts.PowerTexts.Find(Power);
		return Text ? *Text : GrimoireEnumName(Power);
	}

	TArray<FGrimoireRow> GrimoireRows(const FMatchState& State, int32 HumanSeat, const FGrimoireTexts& Texts, const TArray<FLinearColor>& SeatColors)
	{
		TArray<FGrimoireRow> Rows;
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			const FSeatState& SeatState = State.Seats[Seat];
			FGrimoireRow& Row = Rows.AddDefaulted_GetRef();
			Row.Seat = Seat;
			Row.bHuman = Seat == HumanSeat;
			Row.bElect = SeatState.Role == ESeatRole::Elect;
			Row.DomainId = SeatState.DomainId;
			Row.DomainName = State.Board.Domains.IsValidIndex(SeatState.DomainId) ? State.Board.Domains[SeatState.DomainId].Name : FString();
			Row.Color = SeatColors.IsValidIndex(Seat) ? SeatColors[Seat] : FLinearColor::White;
			// S-035 (R-RUN-7): de otro asiento con el Brazal oculto, lo público (Elegido, afinidad, mapa) y el total; ni cantidades ni activos.
			Row.bTypesHidden = !CanSeeBracer(State, Seat, HumanSeat);
			for (const int32 Count : SeatState.Runes)
			{
				Row.TotalRunes += Count;
			}
			// R-ELE-3: el mapa de runa a poder es del Elegido; un Eco no tiene mapa ni afinidad.
			const FElectDef* Elect = Row.bElect && State.Elects.IsValidDomain(SeatState.DomainId) ? &State.Elects.Elects[SeatState.DomainId] : nullptr;
			if (Elect)
			{
				Row.ElectName = Elect->Name;
				Row.AffinityName = AffinityName(Texts, Elect->Affinity);
				Row.AffinityText = AffinityText(Texts, Elect->Affinity);
			}
			for (int32 RuneType = 0; RuneType < Soulbound::Board::DomainCount; ++RuneType)
			{
				FGrimoireRune& Rune = Row.Runes.AddDefaulted_GetRef();
				Rune.RuneType = RuneType;
				Rune.Count = Row.bTypesHidden ? 0 : SeatState.RuneCountOf(RuneType);
				if (Elect && Elect->RuneToPower.IsValidIndex(RuneType))
				{
					Rune.bHasPower = true;
					Rune.Power = Elect->RuneToPower[RuneType];
					Rune.PowerName = PowerName(Rune.Power);
					// R-ELE-5: activo mientras tenga una runa del tipo; esta runa lo activa si la tiene.
					Rune.bActive = !Row.bTypesHidden && Rune.Count > 0 && Soulbound::Rules::IsPowerActive(State, Seat, Rune.Power);
				}
			}
			if (!Row.bTypesHidden)
			{
				for (const EPower Power : Soulbound::Rules::GetActivePowers(State, Seat))
				{
					Row.ActivePowers.Add(PowerName(Power));
				}
			}
		}
		return Rows;
	}

	TArray<FString> GrimoirePowerLines(const FGrimoireTexts& Texts)
	{
		TArray<FString> Lines;
		for (const EPower Power : GrimoirePowers)
		{
			Lines.Add(FString::Printf(TEXT("%s: %s"), PowerName(Power), *PowerText(Texts, Power)));
		}
		return Lines;
	}
}

float Soulbound::Presentation::GrimoirePanelWidth(float LayerWidth)
{
	return FMath::Clamp(LayerWidth - 32.f, 1.f, GrimoireMaxWidth);
}

float Soulbound::Presentation::GrimoireCardTextWidth(float PanelWidth, int32 Columns)
{
	return FMath::Max((PanelWidth - 32.f) / FMath::Max(Columns, 1) - 24.f, 1.f);
}
