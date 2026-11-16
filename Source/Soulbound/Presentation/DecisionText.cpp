#include "DecisionText.h"

#include "EventText.h"
#include "StatusText.h"
#include "Match/WardenEventRules.h"

namespace
{
	using Soulbound::Presentation::EDecisionAnswer;
	using Soulbound::Presentation::EDecisionInput;
	using Soulbound::Presentation::FDecisionOption;
	using Soulbound::Presentation::FDecisionView;

	/** El texto con la primera letra en mayúscula: los botones empiezan como una frase ("Un dado", "Asiento 2"). */
	FString UpperFirst(const FString& Text)
	{
		return Text.IsEmpty() ? Text : Text.Left(1).ToUpper() + Text.Mid(1);
	}

	/** El evento del Warden en curso, o null si no hay o no está en la tabla. */
	const FWardenEventDef* CurrentEventDef(const FMatchState& State)
	{
		const int32 Index = State.CurrentEvent.EventIndex;
		return State.WardenEvents.Events.IsValidIndex(Index) ? &State.WardenEvents.Events[Index] : nullptr;
	}

	/** Una opción que entrega SubmitHumanChoice(Kind, Value). */
	FDecisionOption ChoiceOption(const FString& Text, const FString& Key, EChoiceKind Kind, int32 Value)
	{
		FDecisionOption Option;
		Option.Text = Text;
		Option.Key = Key;
		Option.Answer = EDecisionAnswer::Choice;
		Option.ChoiceKind = Kind;
		Option.Value = Value;
		return Option;
	}

	/** El "0  No" de las elecciones que admiten "no" (decisión 3 del autor): al final, con la tecla 0, y contesta INDEX_NONE. */
	FDecisionOption DeclineOption(EChoiceKind Kind)
	{
		return ChoiceOption(TEXT("No"), TEXT("0"), Kind, INDEX_NONE);
	}

	/** El contexto de la tirada (R-DEC-15): "Has sacado" o "Ha sacado" si hay valores de dados (las caras van detrás), o con "un N" si no los hay. */
	FString RollContext(const FMatchState& State, const TCHAR* Verb)
	{
		return State.DiceValues.IsEmpty() ? FString::Printf(TEXT("%s un %d"), Verb, State.Roll) : FString(Verb);
	}

	/** La instrucción y el título del destino del humano en su turno de tirada. */
	void FillTile(const FMatchState& State, FDecisionView& View)
	{
		View.Input = EDecisionInput::Tile;
		// R-DEC-15 (S-039): la tirada se ve en dados. Sin valores de dados (tirada fijada por depuración) no hay caras y el contexto dice el número.
		View.Title = TEXT("Tu turno");
		View.Context = RollContext(State, TEXT("Has sacado"));
		View.DiceValues = State.DiceValues;
		View.Instruction = TEXT("Elige una casilla que late.");
		View.DeadlineSeconds = State.TurnDeadlineSeconds;
	}

	/** La vista de la elección pendiente del humano (FPendingChoice): las opciones del motor, en su orden, con las teclas de OptionKey. */
	FDecisionView ChoiceView(const FMatchState& State)
	{
		const FPendingChoice& Choice = State.PendingChoice;
		FDecisionView View;
		View.DeadlineSeconds = Choice.DeadlineSeconds;

		// Decisión 4 del autor: la tirada repetible es el destino, con un solo botón para repetir (la tecla R, como hoy).
		if (Choice.Kind == EChoiceKind::Reroll)
		{
			FillTile(State, View);
			View.Instruction = TEXT("Elige una casilla que late o repite la tirada.");
			View.DeadlineSeconds = Choice.DeadlineSeconds;
			View.Options.Add(ChoiceOption(TEXT("Repetir la tirada"), TEXT("R"), EChoiceKind::Reroll, Choice.AnswerAt(0) != INDEX_NONE ? Choice.AnswerAt(0) : 1));
			return View;
		}

		// Añadido a del autor: el salto planar se contesta con clic en un Nodo; en el panel, su texto y el "0  No".
		if (Choice.Kind == EChoiceKind::PlanarJump)
		{
			View.Input = EDecisionInput::Tile;
			View.Title = UpperFirst(Soulbound::Presentation::ChoiceWhat(State));
			View.Instruction = TEXT("Haz clic en un Nodo con halo para saltar");
			View.Options.Add(DeclineOption(Choice.Kind));
			return View;
		}

		View.Input = EDecisionInput::Buttons;
		if (Choice.IsWardenEventChoice())
		{
			// Las de un evento del Warden: su nombre y su texto; qué se elige, en la instrucción (en el dilema, las opciones lo dicen solas).
			if (const FWardenEventDef* Def = CurrentEventDef(State))
			{
				View.Title = Def->DisplayName();
				View.Context = Def->Text;
			}
			else
			{
				View.Title = TEXT("El Warden");
			}
			if (Choice.Kind != EChoiceKind::Dilemma)
			{
				View.Instruction = TEXT("Elige ") + Soulbound::Presentation::ChoiceWhat(State);
			}
		}
		else
		{
			// Corrección del autor del 11/11/2026: la de dados pregunta; la línea del panel de estado sigue diciendo "ELIGE dados".
			View.Title = Choice.Kind == EChoiceKind::DiceCount ? FString(TEXT("¿Cuántos dados tiras?")) : TEXT("Elige ") + Soulbound::Presentation::ChoiceWhat(State);
			if (Choice.Kind == EChoiceKind::ShellPick && State.ShellGame.IsActive())
			{
				View.Context = FString::Printf(TEXT("Robas al asiento %d: una esfera, a ciegas"), State.ShellGame.Victim);
			}
		}
		for (int32 Index = 0; Index < Choice.AnswerCount(); ++Index)
		{
			// R-DEC-11: en el dilema, el texto de la opción para el jugador, sin la dirección cuando recae en quien elige.
			const FWardenEventDef* DilemmaDef = Choice.Kind == EChoiceKind::Dilemma ? CurrentEventDef(State) : nullptr;
			const FString OptionLabel = DilemmaDef && DilemmaDef->Options.IsValidIndex(Choice.Options[Index])
				? Soulbound::Presentation::DescribeOptionForPlayer(DilemmaDef->Options[Choice.Options[Index]]) : Soulbound::Presentation::OptionText(State, Index);
			View.Options.Add(ChoiceOption(UpperFirst(OptionLabel), Soulbound::Presentation::ChoiceOptionKey(Choice, Index),
				Choice.Kind, Choice.AnswerAt(Index)));
		}
		if (Choice.CanDecline())
		{
			View.Options.Add(DeclineOption(Choice.Kind));
		}
		return View;
	}

	/** La vista de la consulta del Oráculo del humano: consultar con el 1 y, si puede (Tifón), rechazar con el 2 (decisión 1 del autor). */
	FDecisionView OracleView(const FMatchState& State)
	{
		FDecisionView View;
		View.Input = EDecisionInput::Orb;
		View.Title = TEXT("Oráculo");
		View.Context = UpperFirst(Soulbound::Presentation::OracleOriginText(static_cast<int32>(State.Oracle.Origin)));
		View.Instruction = State.Oracle.bRefusable ? TEXT("o haz clic en el orbe sobre tu ficha; clic derecho para rechazar")
			: TEXT("o haz clic en el orbe sobre tu ficha");
		View.DeadlineSeconds = State.Oracle.DeadlineSeconds;
		FDecisionOption Consult;
		Consult.Text = TEXT("Consultar");
		Consult.Key = TEXT("1");
		Consult.Answer = EDecisionAnswer::OracleConsult;
		View.Options.Add(Consult);
		if (State.Oracle.bRefusable)
		{
			FDecisionOption Refuse;
			Refuse.Text = TEXT("Rechazar");
			Refuse.Key = TEXT("2");
			Refuse.Answer = EDecisionAnswer::OracleRefuse;
			View.Options.Add(Refuse);
		}
		return View;
	}

	/** "Asiento 2 · Shuten-dōji" si es Elegido, "Asiento 2 (Eco)" si no (decisión 2 del autor). */
	FString SeatLabel(const FMatchState& State, int32 Seat)
	{
		const FString Elect = State.Seats.IsValidIndex(Seat) && State.Seats[Seat].Role == ESeatRole::Elect
			? Soulbound::Presentation::ElectName(State, State.Seats[Seat].DomainId) : FString();
		return Elect.IsEmpty() ? FString::Printf(TEXT("Asiento %d (Eco)"), Seat) : FString::Printf(TEXT("Asiento %d · %s"), Seat, *Elect);
	}

	/** El turno de otro asiento en su fase de tirada: de quién es y su tirada (R-DEC-5, caso 2). */
	void FillTurnWait(const FMatchState& State, FDecisionView& View, double DeadlineSeconds)
	{
		View.Title = FString::Printf(TEXT("Turno de %s"), *SeatLabel(State, State.ActiveSeat));
		View.Context = RollContext(State, TEXT("Ha sacado"));
		View.DiceValues = State.DiceValues;
		View.DeadlineSeconds = DeadlineSeconds;
		View.Identity = FString::Printf(TEXT("W|T|%d"), State.ActiveSeat);
	}

	/** Qué hace quien elige, en tercera persona, para la línea de título de la espera (R-DEC-5, caso 1). */
	const TCHAR* WaitChoiceVerb(EChoiceKind Kind)
	{
		switch (Kind)
		{
		case EChoiceKind::DiceCount: return TEXT("decide cuántos dados tira");
		case EChoiceKind::PlanarJump: return TEXT("decide si hace un salto planar");
		case EChoiceKind::SafeRune: return TEXT("pone una runa a salvo del trilero");
		case EChoiceKind::ShellPick: return TEXT("elige una esfera del trilero");
		case EChoiceKind::Deflection: return TEXT("decide si sale del Vacío con Desvío");
		default: return TEXT("decide si actúa contra un jugador");
		}
	}

	/** La elección pendiente de otro asiento: quién elige y qué; en las de un evento del Warden, su nombre y su texto (R-DEC-5, caso 1). */
	void FillChoiceWait(const FMatchState& State, FDecisionView& View)
	{
		const FPendingChoice& Choice = State.PendingChoice;
		const FString Who = SeatLabel(State, Choice.Seat);
		View.DeadlineSeconds = Choice.DeadlineSeconds;
		View.Identity = FString::Printf(TEXT("W|C|%d|%d"), Choice.Seat, static_cast<int32>(Choice.Kind));
		if (Choice.Kind == EChoiceKind::Reroll)
		{
			// Repetir la tirada es parte de su destino: se enseña como su turno, con el plazo de la elección.
			FillTurnWait(State, View, Choice.DeadlineSeconds);
			return;
		}
		if (Choice.IsWardenEventChoice())
		{
			if (const FWardenEventDef* Def = CurrentEventDef(State))
			{
				View.Title = Def->DisplayName();
				View.Context = Def->Text;
			}
			else
			{
				View.Title = TEXT("El Warden");
			}
			View.Instruction = TEXT("Le toca a ") + Who;
			return;
		}
		// Corrección del autor del 11/11/2026: una línea, en tercera persona (ChoiceWhat está en segunda y lo usan las decisiones del humano).
		View.Title = Who + TEXT(" ") + WaitChoiceVerb(Choice.Kind);
	}

	/** Qué prueba de habilidad espera el motor, para la espera de los demás. */
	FString SkillTrialName(ESkillTrial Trial)
	{
		switch (Trial)
		{
		case ESkillTrial::ForgottenSource: return TEXT("Prueba de la Fuente");
		case ESkillTrial::WardenTrial: return TEXT("Prueba del Warden");
		case ESkillTrial::VoidEscape: return TEXT("Escape del Vacío");
		case ESkillTrial::FinalChallenge: return TEXT("Desafío final");
		default: return TEXT("Prueba de habilidad");
		}
	}

	/** La vista del voto del humano: un botón por asiento que puede votar (VoteOptions, del motor), con las teclas 1 a n (decisión 1 del autor). */
	FDecisionView VoteView(const FMatchState& State, const TArray<int32>& Votables)
	{
		FDecisionView View;
		View.Input = EDecisionInput::Token;
		if (const FWardenEventDef* Def = CurrentEventDef(State))
		{
			View.Title = Def->DisplayName();
			View.Context = Def->Text;
		}
		else
		{
			View.Title = TEXT("Votación");
		}
		View.Instruction = TEXT("Vota con un botón o haz clic en su ficha");
		View.DeadlineSeconds = State.Vote.DeadlineSeconds;
		for (int32 Index = 0; Index < Votables.Num(); ++Index)
		{
			FDecisionOption Option;
			Option.Text = SeatLabel(State, Votables[Index]);
			// 1 a n en el orden de los votables; con cuatro asientos, nunca pasa de 3.
			Option.Key = Index < 9 ? FString::FromInt(Index + 1) : FString();
			Option.Answer = EDecisionAnswer::Vote;
			Option.Value = Votables[Index];
			View.Options.Add(Option);
		}
		return View;
	}
}

namespace
{
	/** Si un suceso del registro es de la lista cerrada de desenlaces (R-DEC-10). */
	bool IsOutcomeEvent(const FMatchEvent& Event)
	{
		switch (Event.Type)
		{
		// Señalar: un efecto con rival señalado (OtherSeat, quien señaló) distinto del señalado (Seat). Los demás EffectApplied no son desenlace.
		case EMatchEventType::EffectApplied:
			return Event.OtherSeat != INDEX_NONE && Event.OtherSeat != Event.Seat;
		case EMatchEventType::DuelWon:
		case EMatchEventType::WardenDuelWon:
		case EMatchEventType::WardenDuelLost:
		case EMatchEventType::DilemmaChosen:
		case EMatchEventType::RuneStolen:
		case EMatchEventType::ShellGameExpired:
		case EMatchEventType::SkillMinigamePassed:
		case EMatchEventType::SkillMinigameFailed:
		case EMatchEventType::WardenTrialPassed:
		case EMatchEventType::WardenTrialFailed:
		case EMatchEventType::VoidEscapePassed:
		case EMatchEventType::VoidEscapeFailed:
		case EMatchEventType::FinalChallengeFailed:
			return true;
		default:
			return false;
		}
	}

	/** La posición en el registro del último suceso de la lista de desenlaces, o INDEX_NONE. */
	int32 LastOutcomeIndex(const FMatchState& State)
	{
		for (int32 Index = State.EventLog.Num() - 1; Index >= 0; --Index)
		{
			if (IsOutcomeEvent(State.EventLog[Index]))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** La posición en el registro del último VoteResolved, o INDEX_NONE. */
	int32 LastVoteResolvedIndex(const FMatchState& State)
	{
		for (int32 Index = State.EventLog.Num() - 1; Index >= 0; --Index)
		{
			if (State.EventLog[Index].Type == EMatchEventType::VoteResolved)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

namespace Soulbound::Presentation
{
	TOptional<FDecisionView> DecisionView(const FMatchState& State, int32 HumanSeat, bool bWardenSpeaking)
	{
		if (State.IsMatchOver() || !State.Seats.IsValidIndex(HumanSeat))
		{
			return {};
		}
		// Añadido b del autor: con dos decisiones abiertas, la que el panel de estado pone primero (StatusLines): la elección pendiente y el
		// destino van arriba, y en ese orden; después, la línea del Oráculo y la de la votación.
		TOptional<FDecisionView> View;
		if (State.PendingChoice.IsOpen() && State.PendingChoice.Seat == HumanSeat)
		{
			View = ChoiceView(State);
		}
		else if (State.TurnPhase == ETurnPhase::RollAndChoose && State.ActiveSeat == HumanSeat)
		{
			FDecisionView Tile;
			FillTile(State, Tile);
			View = Tile;
		}
		else if (State.Oracle.IsWaiting() && State.Oracle.Consultant == HumanSeat)
		{
			View = OracleView(State);
		}
		else if (State.Vote.IsOpen() && State.Vote.Votes.IsValidIndex(HumanSeat) && State.Vote.Votes[HumanSeat] == INDEX_NONE)
		{
			// Si no tiene a quién votar, no vota (R-VOT-2); si ya votó, para él la decisión está resuelta (decisión 5 del autor).
			const TArray<int32> Votables = Soulbound::Rules::VoteOptions(State, HumanSeat);
			if (!Votables.IsEmpty())
			{
				View = VoteView(State, Votables);
			}
		}
		if (View.IsSet())
		{
			View->ClockSeconds = State.ClockSeconds;
			View->bBlocked = bWardenSpeaking;
			// Qué decisión es, para el reloj de la barra: la entrada y, con botones, el tipo de elección. El destino y la tirada repetible son la
			// misma (el mismo reloj de turno); dos decisiones seguidas del mismo tipo las separa el plazo, que se alarga (AdvanceDecisionClock).
			const EChoiceKind Kind = View->Input == EDecisionInput::Buttons ? State.PendingChoice.Kind : EChoiceKind::None;
			View->Identity = FString::Printf(TEXT("%d|%d"), static_cast<int32>(View->Input), static_cast<int32>(Kind));
		}
		return View;
	}

	TOptional<FDecisionView> WaitView(const FMatchState& State, int32 HumanSeat)
	{
		if (State.IsMatchOver() || !State.Seats.IsValidIndex(HumanSeat))
		{
			return {};
		}
		// Los casos, en el orden del panel de estado (R-DEC-5): elección de otro, destino de otro, Oráculo de otro, votación y, al final, la prueba
		// de habilidad o el duelo de otros. Cada uno se mira solo si no es del humano: lo propio lo enseña DecisionView.
		FDecisionView View;
		View.Input = EDecisionInput::Wait;
		bool bFound = false;
		if (State.PendingChoice.IsOpen() && State.PendingChoice.Seat != HumanSeat)
		{
			FillChoiceWait(State, View);
			bFound = true;
		}
		else if (State.TurnPhase == ETurnPhase::RollAndChoose && State.ActiveSeat != HumanSeat)
		{
			FillTurnWait(State, View, State.TurnDeadlineSeconds);
			bFound = true;
		}
		else if (State.Oracle.IsWaiting() && State.Oracle.Consultant != HumanSeat)
		{
			View.Title = FString::Printf(TEXT("%s ante el Oráculo"), *SeatLabel(State, State.Oracle.Consultant));
			View.DeadlineSeconds = State.Oracle.DeadlineSeconds;
			View.Identity = FString::Printf(TEXT("W|O|%d"), State.Oracle.Consultant);
			bFound = true;
		}
		else if (State.Vote.IsOpen())
		{
			// Faltan los que pueden votar y aún no lo han hecho. Si el humano es uno de ellos, ese voto es su decisión, no una espera.
			const bool bHumanPending = State.Vote.Votes.IsValidIndex(HumanSeat) && State.Vote.Votes[HumanSeat] == INDEX_NONE
				&& !Soulbound::Rules::VoteOptions(State, HumanSeat).IsEmpty();
			TArray<FString> Missing;
			for (int32 Seat = 0; Seat < State.Vote.Votes.Num(); ++Seat)
			{
				if (State.Vote.Votes[Seat] == INDEX_NONE && !Soulbound::Rules::VoteOptions(State, Seat).IsEmpty())
				{
					Missing.Add(SeatLabel(State, Seat));
				}
			}
			if (!bHumanPending && !Missing.IsEmpty())
			{
				const FWardenEventDef* Def = CurrentEventDef(State);
				View.Title = Def ? Def->DisplayName() : FString(TEXT("Votación"));
				View.Context = TEXT("Faltan por votar: ") + FString::Join(Missing, TEXT(", "));
				View.DeadlineSeconds = State.Vote.DeadlineSeconds;
				View.Identity = TEXT("W|V");
				bFound = true;
			}
		}
		if (!bFound && State.TurnPhase == ETurnPhase::SkillTrial && State.ActiveSeat != HumanSeat && State.Seats.IsValidIndex(State.ActiveSeat))
		{
			View.Title = FString::Printf(TEXT("%s juega"), *SeatLabel(State, State.ActiveSeat));
			View.Context = SkillTrialName(State.PendingTrial);
			View.bHasDeadline = false;
			View.Identity = FString::Printf(TEXT("W|S|%d|%d"), State.ActiveSeat, static_cast<int32>(State.PendingTrial));
			bFound = true;
		}
		if (!bFound && State.TurnPhase == ETurnPhase::Duel && State.Duel.IsActive() && !State.Duel.IsParticipant(HumanSeat))
		{
			const FString Challenger = State.Duel.bAgainstWarden ? FString(TEXT("El Warden")) : SeatLabel(State, State.Duel.Challenger);
			View.Title = FString::Printf(TEXT("Duelo: %s contra %s"), *Challenger, *SeatLabel(State, State.Duel.Challenged));
			View.bHasDeadline = false;
			View.Identity = FString::Printf(TEXT("W|D|%d|%d"), State.Duel.Challenger, State.Duel.Challenged);
			bFound = true;
		}
		if (!bFound)
		{
			return {};
		}
		View.ClockSeconds = State.ClockSeconds;
		return View;
	}

	TOptional<FDecisionView> VoteResultView(const FMatchState& State, double ResultSeconds)
	{
		int32 Resolved = INDEX_NONE;
		for (int32 Index = State.EventLog.Num() - 1; Index >= 0; --Index)
		{
			if (State.EventLog[Index].Type == EMatchEventType::VoteResolved)
			{
				Resolved = Index;
				break;
			}
		}
		if (Resolved == INDEX_NONE || State.ClockSeconds - State.EventLog[Resolved].ClockSeconds >= ResultSeconds)
		{
			return {};
		}
		const FMatchEvent& Outcome = State.EventLog[Resolved];

		// Los votos de esa votación: los VoteCast justo antes de su VoteResolved, hasta el VoteStarted (que dice de qué evento) o la votación anterior.
		FDecisionView View;
		View.Input = EDecisionInput::Result;
		View.Title = TEXT("Votación");
		for (int32 Index = Resolved - 1; Index >= 0; --Index)
		{
			const FMatchEvent& Event = State.EventLog[Index];
			if (Event.Type == EMatchEventType::VoteResolved)
			{
				break;
			}
			if (Event.Type == EMatchEventType::VoteStarted)
			{
				if (State.WardenEvents.Events.IsValidIndex(Event.Value))
				{
					View.Title = State.WardenEvents.Events[Event.Value].DisplayName();
				}
				break;
			}
			if (Event.Type == EMatchEventType::VoteCast)
			{
				View.Lines.Insert(FString::Printf(TEXT("%s votó a %s%s"), *SeatLabel(State, Event.Seat), *SeatLabel(State, Event.OtherSeat),
					Event.Detail == 1 ? TEXT(" (al azar: no votó a tiempo)") : TEXT("")), 0);
			}
		}
		View.Context = FString::Printf(TEXT("El más votado: %s, con %d %s%s"), *SeatLabel(State, Outcome.Seat), Outcome.Value, Outcome.Value == 1 ? TEXT("voto") : TEXT("votos"),
			Outcome.Detail == 1 ? TEXT(" (desempate del Warden)") : TEXT(""));
		View.bHasDeadline = false;
		View.ClockSeconds = State.ClockSeconds;
		View.Identity = FString::Printf(TEXT("R|%.3f"), Outcome.ClockSeconds);
		return View;
	}

	TOptional<FDecisionView> OutcomeView(const FMatchState& State, int32 HumanSeat, double OutcomeSeconds)
	{
		const int32 Index = LastOutcomeIndex(State);
		if (Index == INDEX_NONE || State.ClockSeconds - State.EventLog[Index].ClockSeconds >= OutcomeSeconds)
		{
			return {};
		}
		const FMatchEvent& Event = State.EventLog[Index];
		FDecisionView View;
		View.Input = EDecisionInput::Result;
		View.bHasDeadline = false;
		View.ClockSeconds = State.ClockSeconds;
		View.Identity = FString::Printf(TEXT("O|%d"), Index);
		switch (Event.Type)
		{
		case EMatchEventType::DuelWon:
			View.Title = TEXT("Duelo");
			View.Context = FString::Printf(TEXT("Gana %s a %s"), *SeatLabel(State, Event.Seat), *SeatLabel(State, Event.OtherSeat));
			break;
		case EMatchEventType::WardenDuelWon:
			View.Title = TEXT("Duelo contra el Warden");
			View.Context = FString::Printf(TEXT("Gana el Warden: %s pierde lo que estaba en juego"), *SeatLabel(State, Event.Seat));
			break;
		case EMatchEventType::WardenDuelLost:
			View.Title = TEXT("Duelo contra el Warden");
			View.Context = FString::Printf(TEXT("Gana %s: no pierde nada"), *SeatLabel(State, Event.Seat));
			break;
		case EMatchEventType::DilemmaChosen:
		{
			// La opción que eligió el propio humano ya la vio en su panel; la que le sorteó el Warden, no.
			const bool bDrawn = Event.Detail == 1;
			if (Event.Seat == HumanSeat && !bDrawn)
			{
				return {};
			}
			// El evento es el del WardenEventStarted anterior en el registro (Value es su posición en la tabla).
			const FWardenEventDef* Def = nullptr;
			for (int32 Back = Index - 1; Back >= 0; --Back)
			{
				if (State.EventLog[Back].Type == EMatchEventType::WardenEventStarted)
				{
					Def = State.WardenEvents.Events.IsValidIndex(State.EventLog[Back].Value) ? &State.WardenEvents.Events[State.EventLog[Back].Value] : nullptr;
					break;
				}
			}
			View.Title = Def ? Def->DisplayName() : FString(TEXT("Dilema"));
			// El texto de la opción es el del botón de S-028 (DescribeOptionForPlayer sobre la opción del evento, R-DEC-11).
			const FString Option = Def && Def->Options.IsValidIndex(Event.Value) ? UpperFirst(Soulbound::Presentation::DescribeOptionForPlayer(Def->Options[Event.Value]))
				: FString::Printf(TEXT("Opción %d"), Event.Value + 1);
			View.Context = bDrawn ? FString::Printf(TEXT("El Warden elige por %s: %s"), *SeatLabel(State, Event.Seat), *Option)
				: FString::Printf(TEXT("%s elige: %s"), *SeatLabel(State, Event.Seat), *Option);
			break;
		}
		case EMatchEventType::EffectApplied:
			// Señalar: quien señaló no es el humano (lo hizo él y ya lo sabe); sí si el señalado es él.
			if (Event.OtherSeat == HumanSeat)
			{
				return {};
			}
			View.Title = EffectName(Event.Value);
			View.Context = FString::Printf(TEXT("%s señala a %s"), *SeatLabel(State, Event.OtherSeat), *SeatLabel(State, Event.Seat));
			break;
		case EMatchEventType::RuneStolen:
			// La runa se nombra como en los botones de S-028 y en el Grimorio: "D2" (decisión del autor).
			View.Title = TEXT("Trilero");
			View.Context = FString::Printf(TEXT("%s roba a %s la runa D%d"), *SeatLabel(State, Event.Seat), *SeatLabel(State, Event.OtherSeat), Event.Value);
			break;
		case EMatchEventType::ShellGameExpired:
			View.Title = TEXT("Trilero");
			View.Context = FString::Printf(TEXT("%s no elige a tiempo y no roba nada"), *SeatLabel(State, Event.Seat));
			break;
		default:
		{
			// Las pruebas: las del humano no, su resultado ya lo enseña el anillo.
			if (Event.Seat == HumanSeat)
			{
				return {};
			}
			bool bPassed = false;
			ESkillTrial Trial = ESkillTrial::ForgottenSource;
			switch (Event.Type)
			{
			case EMatchEventType::SkillMinigamePassed: bPassed = true; Trial = ESkillTrial::ForgottenSource; break;
			case EMatchEventType::SkillMinigameFailed: Trial = ESkillTrial::ForgottenSource; break;
			case EMatchEventType::WardenTrialPassed: bPassed = true; Trial = ESkillTrial::WardenTrial; break;
			case EMatchEventType::WardenTrialFailed: Trial = ESkillTrial::WardenTrial; break;
			case EMatchEventType::VoidEscapePassed: bPassed = true; Trial = ESkillTrial::VoidEscape; break;
			case EMatchEventType::VoidEscapeFailed: Trial = ESkillTrial::VoidEscape; break;
			default: Trial = ESkillTrial::FinalChallenge; break;
			}
			View.Title = SkillTrialName(Trial);
			View.Context = FString::Printf(TEXT("%s %s"), *SeatLabel(State, Event.Seat), bPassed ? TEXT("la supera") : TEXT("falla"));
			break;
		}
		}
		return View;
	}

	TOptional<FDecisionView> PanelView(const FMatchState& State, int32 HumanSeat, bool bWardenSpeaking, double VoteResultSeconds, double OutcomeSeconds)
	{
		if (State.IsMatchOver() || !State.Seats.IsValidIndex(HumanSeat))
		{
			return {};
		}
		// R-DEC-6 y R-DEC-8: lo propio manda; después el resultado de una votación (también mientras habla el Warden); después la espera, que
		// calla mientras habla el Warden porque su frase ya está en pantalla.
		if (TOptional<FDecisionView> Own = DecisionView(State, HumanSeat, bWardenSpeaking))
		{
			return Own;
		}
		// Resultado de una votación y desenlace: si hay los dos, el suceso más reciente del registro (R-DEC-10).
		const TOptional<FDecisionView> Result = VoteResultView(State, VoteResultSeconds);
		const TOptional<FDecisionView> Outcome = OutcomeView(State, HumanSeat, OutcomeSeconds);
		if (Result.IsSet() && Outcome.IsSet())
		{
			return LastVoteResolvedIndex(State) > LastOutcomeIndex(State) ? Result : Outcome;
		}
		if (Result.IsSet() || Outcome.IsSet())
		{
			return Result.IsSet() ? Result : Outcome;
		}
		return bWardenSpeaking ? TOptional<FDecisionView>() : WaitView(State, HumanSeat);
	}

	bool IsOwnDecisionShown(const TOptional<FDecisionView>& View, bool bRingOpen)
	{
		return View.IsSet() && !bRingOpen && View->Input != EDecisionInput::Wait && View->Input != EDecisionInput::Result;
	}

	bool IsDecisionPanelShown(const TOptional<FDecisionView>& View, bool bRingOpen)
	{
		// Mientras el anillo está abierto, el anillo es la decisión (S-026). Bloqueada por la espera del Warden, se enseña igual (R-DEC-3).
		return View.IsSet() && !bRingOpen;
	}

	const FDecisionOption* FindDecisionOption(const FDecisionView& View, const FString& Key)
	{
		if (Key.IsEmpty())
		{
			return nullptr;
		}
		return View.Options.FindByPredicate([&Key](const FDecisionOption& Option) { return Option.Key == Key; });
	}

	double AdvanceDecisionClock(FDecisionClock& Clock, const TOptional<FDecisionView>& View)
	{
		if (!View.IsSet())
		{
			Clock = FDecisionClock();
			return 0.0;
		}
		const double Remaining = FMath::Max(0.0, View->DeadlineSeconds - View->ClockSeconds);
		if (Clock.Identity != View->Identity)
		{
			// Una decisión nueva: su total es lo que queda al verla aparecer (decisión 6 del autor).
			Clock.Identity = View->Identity;
			Clock.TotalSeconds = Remaining;
		}
		else if (Remaining > Clock.TotalSeconds)
		{
			// El plazo se ha alargado (la espera del Warden desplaza los plazos abiertos, S-023): la barra vuelve a llenarse.
			Clock.TotalSeconds = Remaining;
		}
		return DecisionTimeFraction(Clock.TotalSeconds, View->DeadlineSeconds, View->ClockSeconds);
	}

	double DecisionTimeFraction(double TotalSeconds, double DeadlineSeconds, double NowSeconds)
	{
		if (TotalSeconds <= 0.0)
		{
			return 0.0;
		}
		return FMath::Clamp((DeadlineSeconds - NowSeconds) / TotalSeconds, 0.0, 1.0);
	}

	bool DecisionButtonsInRow(int32 Count, float WidestButton, float SlotGap, float AvailableWidth)
	{
		return Count <= 0 || Count * (WidestButton + SlotGap) <= AvailableWidth;
	}

	float BoardFreeWidth(float ViewportSlateWidth, float PanelColumnWidth, float Cap)
	{
		return FMath::Clamp(ViewportSlateWidth - PanelColumnWidth - 32.f, 0.f, FMath::Max(Cap, 0.f));
	}

	float WardenLineBandHeight(float ReservedHeight, bool bHasLine, float NeededHeight)
	{
		// S-041: cuerpo vacío para ver el test en rojo.
		return 0.f;
	}
}

FString Soulbound::Presentation::UnreachableNoticeText(EUnreachableReason Reason, int32 Roll)
{
	// Los tres textos son del autor (15/11/2026) y van tal cual.
	switch (Reason)
	{
	case EUnreachableReason::Sealed: return TEXT("Esa casilla la he sellado yo. Busca otro camino.");
	case EUnreachableReason::Confined: return TEXT("Los Ecos no pisan el Dominio de un Elegido. Por ahora.");
	case EUnreachableReason::Roll: break;
	}
	return FString::Printf(TEXT("Con un %d no llegas ahí. Qué lástima."), Roll);
}
