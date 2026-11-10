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

	/** La instrucción y el título del destino del humano en su turno de tirada. */
	void FillTile(const FMatchState& State, FDecisionView& View)
	{
		View.Input = EDecisionInput::Tile;
		View.Title = FString::Printf(TEXT("Tu turno: tirada %d%s"), State.Roll, State.DiceCount > 1 ? TEXT(" (dos dados)") : TEXT(""));
		View.Instruction = TEXT("Elige una casilla iluminada");
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
			View.Instruction = TEXT("Elige una casilla iluminada o repite la tirada");
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
			View.Title = TEXT("Elige ") + Soulbound::Presentation::ChoiceWhat(State);
			if (Choice.Kind == EChoiceKind::ShellPick && State.ShellGame.IsActive())
			{
				View.Context = FString::Printf(TEXT("Robas al asiento %d: una esfera, a ciegas"), State.ShellGame.Victim);
			}
		}
		for (int32 Index = 0; Index < Choice.AnswerCount(); ++Index)
		{
			View.Options.Add(ChoiceOption(UpperFirst(Soulbound::Presentation::OptionText(State, Index)), Soulbound::Presentation::ChoiceOptionKey(Choice, Index),
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
}
