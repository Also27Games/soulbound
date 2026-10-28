#include "Match/InsolenceRules.h"

#include "Match/EffectRules.h"
#include "Match/MatchEvents.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Warden/WardenBrain.h"
#include "Warden/WardenRules.h"
#include "Warden/WardenVoice.h"

using Soulbound::Rules::LogEvent;

namespace
{
	/** La gravedad del catálogo que corresponde a un castigo de la tabla (Fury no tiene: va aparte). */
	EEffectSeverity SeverityOf(EInsolencePunishment Kind)
	{
		switch (Kind)
		{
		case EInsolencePunishment::Medium: return EEffectSeverity::Medium;
		case EInsolencePunishment::Severe: return EEffectSeverity::Severe;
		default: return EEffectSeverity::Light;
		}
	}
}

namespace Soulbound::Rules
{
	TArray<EEffect> InsolencePunishmentEffects(EEffectSeverity Severity)
	{
		// R-INS-2 (decisión del autor del 19/10/2026): del catálogo, sin lista propia. Los regalos se quedan fuera por su gravedad
		// y los que necesitan a otro jugador, porque no admiten la dirección activo.
		TArray<EEffect> Effects;
		if (Severity == EEffectSeverity::Gift)
		{
			return Effects;
		}
		for (const FEffectDef& Def : EffectCatalog())
		{
			if (Def.Severity == Severity && Def.bImplemented && Def.AllowedTargets.Contains(EEffectTarget::ActivePlayer))
			{
				Effects.Add(Def.Effect);
			}
		}
		return Effects;
	}

	EInsolencePunishment DrawInsolencePunishment(const FWardenParams& Params, int32 LevelBefore, FRandomStream& Random)
	{
		const FInsolenceRow* Row = Params.InsolenceTable.FindByPredicate([LevelBefore](const FInsolenceRow& Candidate) { return Candidate.Levels.Contains(LevelBefore); });
		if (!Row)
		{
			return EInsolencePunishment::Light;
		}
		// R-INS-3: desde el nivel de la Furia, sin sorteo.
		if (Row->bFury)
		{
			return EInsolencePunishment::Fury;
		}
		// R-INS-2: leve, media o grave por los porcentajes de la fila, que suman 100 (ValidateWardenParams).
		int32 Point = Random.RandRange(0, 99);
		for (int32 Severity = 0; Severity < Row->SeverityPercents.Num(); ++Severity)
		{
			if (Point < Row->SeverityPercents[Severity])
			{
				return static_cast<EInsolencePunishment>(Severity);
			}
			Point -= Row->SeverityPercents[Severity];
		}
		return EInsolencePunishment::Severe;
	}

	void PunishInsolence(FMatchState& State, int32 Seat, const FPendingPunishment& Punishment, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return;
		}
		LogEvent(State, EMatchEventType::InsolencePunished, NowSeconds, Seat, Punishment.Level, INDEX_NONE, static_cast<int32>(Punishment.Kind));
		// S-021 (R-WAR-10): lo que dice al castigar, con la gravedad; la Furia dice la suya al desatarse (UnleashFury).
		if (Punishment.Kind != EInsolencePunishment::Fury)
		{
			SpeakWardenLine(State, InsolenceLineKey(Punishment.Kind), NowSeconds, Seat);
		}
		if (Punishment.Kind == EInsolencePunishment::Fury)
		{
			UnleashFury(State, Seat, NowSeconds);
			return;
		}
		// R-CAR-7: en el Acto I nadie va al Vacío salvo por la Furia; lo grave es Vuelta al hogar.
		TArray<EEffect> Effects = Punishment.Kind == EInsolencePunishment::Severe && CurrentAct(State, NowSeconds) == EAct::ActOne
			? TArray<EEffect>{EEffect::SendHome} : InsolencePunishmentEffects(SeverityOf(Punishment.Kind));
		// R-INS-2: se sortea uno; si no vale sobre él, otro de la misma gravedad; sin ninguno válido, burla.
		FRandomStream Draw = WardenDraw(State);
		EEffect Tried = Effects.IsEmpty() ? EEffect::RevealBracer : Effects[0];
		while (!Effects.IsEmpty())
		{
			const int32 Pick = Draw.RandRange(0, Effects.Num() - 1);
			Tried = Effects[Pick];
			// R-TAB-28: un castigo no es un golpe y nunca corta un turno: el Alto no vale nunca como castigo, tampoco dentro de un
			// evento que llegó cortando el turno de este mismo asiento.
			if (Tried != EEffect::Halt && CanApplyEffect(State, Tried, Seat, Seat) == EEffectResult::Applied)
			{
				const FEffectDef& Def = EffectDefOf(Tried);
				FEffectApplication Application;
				Application.Effect = Tried;
				Application.Target = EEffectTarget::ActivePlayer;
				Application.Amount = 1;
				Application.Variant = Def.Variants.IsEmpty() ? EEffectVariant::None : Def.Variants[0];
				ApplyEffect(State, Application, Seat, Seat, INDEX_NONE, INDEX_NONE, NowSeconds, Draw);
				return;
			}
			Effects.RemoveAt(Pick);
		}
		LogEvent(State, EMatchEventType::WardenTaunt, NowSeconds, Seat, static_cast<int32>(Tried), INDEX_NONE, static_cast<int32>(EEffectTarget::ActivePlayer));
	}

	void UnleashFury(FMatchState& State, int32 Seat, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return;
		}
		LogEvent(State, EMatchEventType::FuryUnleashed, NowSeconds, Seat);
		SpeakWardenLine(State, Soulbound::Warden::LineFury, NowSeconds, Seat);
		FSeatState& SeatState = State.Seats[Seat];
		SeatState.Insolence = 0;
		// R-INS-3: sobre quien ya está en el Vacío, su Vacío pasa a ser de la Furia y su cuenta de turnos dentro vuelve a 0.
		if (SeatState.IsInVoid())
		{
			SeatState.bFuryVoid = true;
			SeatState.VoidTurns = 0;
			return;
		}
		// R-INS-3, R-CAR-7: la Furia destierra también durante la carrera.
		if (Banish(State, Seat, true, NowSeconds) == EEffectResult::Applied)
		{
			State.Seats[Seat].bFuryVoid = true;
		}
	}

	void PunishOrDefer(FMatchState& State, int32 Seat, const FPendingPunishment& Punishment, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat))
		{
			return;
		}
		FSeatState& SeatState = State.Seats[Seat];
		// R-INS-2: en el Vacío, a su salida, salvo la Furia; en juego, cuando deje de estarlo; si no, en el acto.
		if (SeatState.IsInVoid() && Punishment.Kind != EInsolencePunishment::Fury)
		{
			SeatState.VoidPunishments.Add(Punishment);
		}
		else if (IsSeatInPlay(State, Seat))
		{
			SeatState.PendingPunishments.Add(Punishment);
		}
		else
		{
			PunishInsolence(State, Seat, Punishment, NowSeconds);
		}
	}

	void DischargePendingPunishments(FMatchState& State, double NowSeconds)
	{
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			if (State.Seats[Seat].PendingPunishments.IsEmpty() || IsSeatInPlay(State, Seat))
			{
				continue;
			}
			const TArray<FPendingPunishment> Waiting = MoveTemp(State.Seats[Seat].PendingPunishments);
			State.Seats[Seat].PendingPunishments.Reset();
			for (const FPendingPunishment& Punishment : Waiting)
			{
				PunishOrDefer(State, Seat, Punishment, NowSeconds);
			}
		}
	}

	void DischargeVoidPunishments(FMatchState& State, int32 Seat, double NowSeconds)
	{
		if (!State.Seats.IsValidIndex(Seat) || State.Seats[Seat].VoidPunishments.IsEmpty())
		{
			return;
		}
		const TArray<FPendingPunishment> Waiting = MoveTemp(State.Seats[Seat].VoidPunishments);
		State.Seats[Seat].VoidPunishments.Reset();
		for (const FPendingPunishment& Punishment : Waiting)
		{
			PunishOrDefer(State, Seat, Punishment, NowSeconds);
		}
	}
}
