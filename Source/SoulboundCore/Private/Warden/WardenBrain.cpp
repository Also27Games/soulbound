#include "Warden/WardenBrain.h"

#include "Match/EffectCatalog.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Warden/WardenTriggers.h"

namespace
{
	/** Fin por tiempo del acto Act (R-WAR-2): el inicio del siguiente, o el fin de la partida en el último. */
	double ActEndSeconds(const FWardenParams& Params, EAct Act)
	{
		const int32 Next = static_cast<int32>(Act) + 1;
		return Params.Acts.IsValidIndex(Next) ? Params.Acts[Next].StartSeconds : Params.MatchEndSeconds;
	}

	/**
	 * Lo más tarde que podría llegar la próxima intervención si en esta evaluación no se interviene (garantía anticipada,
	 * decisión del autor del 19/10/2026): la siguiente evaluación, Next; si Next cae en el margen de un acto, ahí no se inicia
	 * nada y la primera que puede intervenir llega, como mucho, un intervalo máximo después del fin del acto; si cae en el
	 * margen del final de la partida, ya no hay otra y el tramo acaba con la partida.
	 */
	double LatestOpportunity(const FMatchState& State, double Next)
	{
		const FWardenParams& Params = State.WardenParams;
		if (Next >= Params.MatchEndSeconds - Params.MarginSeconds)
		{
			return Params.MatchEndSeconds;
		}
		if (Soulbound::Rules::IsInWardenMargin(State, Next))
		{
			return ActEndSeconds(Params, Soulbound::Rules::CurrentAct(State, Next)) + Params.EvaluationIntervalSeconds + Params.EvaluationVariationSeconds;
		}
		return Next;
	}

	/**
	 * Canal por los pesos del acto (R-WAR-3) con el sorteo Draw, de 0 a 1. Forzado por la garantía (R-WAR-5), solo entre los
	 * canales con efecto; si los dos pesan cero, el golpe (y el cerebro probará la escena si no hay golpes).
	 */
	EWardenChannel PickChannel(const TArray<double>& Weights, double Draw, bool bOnlyWithEffect)
	{
		const int32 Count = bOnlyWithEffect ? static_cast<int32>(EWardenChannel::Remark) : Soulbound::Warden::ChannelCount;
		double Sum = 0.0;
		for (int32 Channel = 0; Channel < Count && Channel < Weights.Num(); ++Channel)
		{
			Sum += Weights[Channel];
		}
		if (Sum <= 0.0)
		{
			return EWardenChannel::Strike;
		}
		double Point = Draw * Sum;
		for (int32 Channel = 0; Channel < Count && Channel < Weights.Num(); ++Channel)
		{
			if (Point < Weights[Channel])
			{
				return static_cast<EWardenChannel>(Channel);
			}
			Point -= Weights[Channel];
		}
		return static_cast<EWardenChannel>(Count - 1);
	}

	/**
	 * Evento de Candidates por el peso del acto Act (R-WAR-8, R-SAC-1) con el sorteo Draw, de 0 a 1. Desde S-017, el peso con los
	 * disparadores Triggers (R-DIS-1), si lo dirigiera Director.
	 */
	int32 PickEvent(const FMatchState& State, const TArray<int32>& Candidates, EAct Act, int32 Director, const TArray<FActiveTrigger>& Triggers, double Draw)
	{
		double Sum = 0.0;
		for (const int32 Index : Candidates)
		{
			Sum += Soulbound::Rules::TriggeredEventWeight(State, Index, Act, Director, Triggers);
		}
		double Point = Draw * Sum;
		for (const int32 Index : Candidates)
		{
			const double Weight = Soulbound::Rules::TriggeredEventWeight(State, Index, Act, Director, Triggers);
			if (Point < Weight)
			{
				return Index;
			}
			Point -= Weight;
		}
		return Candidates.Last();
	}

	/** Si una opción tiene algún objetivo válido (R-DIR-2, R-COM-3: cada aplicación se valida por separado). */
	bool IsValidOption(const FMatchState& View, const FWardenEventOption& Option, int32 Director, bool bCuts)
	{
		return Option.Applications.ContainsByPredicate([&](const FEffectApplication& Application)
		{
			// R-TAB-28: el Alto solo vale cortando un turno.
			return Application.Effect == EEffect::Halt ? bCuts : Soulbound::Rules::HasValidTargets(View, Application, Director);
		});
	}
}

FWardenDecision FWardenBrain::Evaluate(const FMatchState& State, double NowSeconds, FRandomStream& Random) const
{
	// Siempre los mismos cinco valores del sorteo, en este orden: así la secuencia del Warden no depende de lo que decida. El quinto
	// (S-017) elige la entrega de un golpe con "any".
	const double Variation = Random.FRandRange(-1.0, 1.0);
	const double InterveneDraw = Random.FRand();
	const double ChannelDraw = Random.FRand();
	const double EventDraw = Random.FRand();
	const double DeliveryDraw = Random.FRand();

	FWardenDecision Decision;
	const FWardenParams& Params = State.WardenParams;
	Decision.NextEvaluationSeconds = NowSeconds + Params.EvaluationIntervalSeconds + Variation * Params.EvaluationVariationSeconds;
	// Con la partida terminada (decisión del autor del 01/10/2026) o sin Warden, nada.
	if (!Params.HasWarden() || State.IsMatchOver())
	{
		return Decision;
	}
	Decision.Act = Soulbound::Rules::CurrentAct(State, NowSeconds);
	// R-DIS-1, R-WAR-10 (S-017): los disparadores activos, en cada evaluación; quedan en el registro con la decisión.
	Decision.Triggers = Soulbound::Rules::ActiveTriggers(State, NowSeconds);
	// R-WAR-7: durante un duelo, un trilero, un minijuego, un evento o, desde S-017, una consulta del Oráculo, se aplaza.
	if (Soulbound::Rules::IsWardenDeferred(State))
	{
		Decision.Kind = EWardenDecisionKind::Deferred;
		return Decision;
	}
	// R-WAR-4d: en el margen no se inicia nada, ni un comentario.
	if (Soulbound::Rules::IsInWardenMargin(State, NowSeconds))
	{
		return Decision;
	}
	// R-WAR-5, garantía anticipada (decisión del autor del 19/10/2026): si esperar dejaría pasar más de la garantía sin
	// golpe ni escena, se interviene ya, con efecto.
	Decision.bForcedByGuarantee = LatestOpportunity(State, Decision.NextEvaluationSeconds) - State.Warden.LastEffectSeconds > Params.GuaranteeSeconds;
	const FWardenActParams& Act = Params.Acts[static_cast<int32>(Decision.Act)];
	// R-DIS-1 (S-017): con Victoria inminente, la probabilidad de intervenir se multiplica (datos, 1,5).
	if (!Decision.bForcedByGuarantee && InterveneDraw >= Soulbound::Rules::TriggeredInterveneProbability(State, Decision.Act, Decision.Triggers))
	{
		return Decision;
	}

	// R-WAR-4: como mucho una intervención con efecto por hueco entre turnos; una escena que espera al hueco ya es la suya.
	const bool bEffectAllowed = State.Warden.PendingScene == INDEX_NONE && !(State.TurnPhase == ETurnPhase::BetweenTurns && State.Warden.bGapUsed);
	const EWardenChannel Channel = PickChannel(Act.ChannelWeights, ChannelDraw, Decision.bForcedByGuarantee);
	if (Channel != EWardenChannel::Remark && bEffectAllowed)
	{
		// R-WAR-8: sin candidato en el canal elegido, el otro canal con efecto.
		const EWardenChannel Other = Channel == EWardenChannel::Strike ? EWardenChannel::Scene : EWardenChannel::Strike;
		// R-WAR-5: forzada por la garantía con un turno en curso, solo vale lo que entra ya; una escena que no corta esperaría
		// al hueco y dejaría pasar la garantía.
		const bool bOnlyImmediate = Decision.bForcedByGuarantee && State.TurnPhase != ETurnPhase::BetweenTurns;
		for (const EWardenChannel Tried : {Channel, Other})
		{
			TArray<int32> Candidates = Soulbound::Rules::EventCandidates(State, Tried, NowSeconds);
			if (bOnlyImmediate && Tried == EWardenChannel::Scene)
			{
				Candidates = Candidates.FilterByPredicate([&State](int32 Index) { return State.WardenEvents.Events[Index].bCutsTurn; });
			}
			if (!Candidates.IsEmpty())
			{
				Decision.Kind = Tried == EWardenChannel::Strike ? EWardenDecisionKind::Strike : EWardenDecisionKind::Scene;
				const int32 Director = State.TurnPhase != ETurnPhase::BetweenTurns ? State.ActiveSeat : Soulbound::Rules::EventDirector(State);
				Decision.EventIndex = PickEvent(State, Candidates, Decision.Act, Director, Decision.Triggers, EventDraw);
				// R-ORA-1 (S-017): un golpe con "oracle" llega por el Oráculo; con "any", a partes iguales. Las escenas, directas.
				const EEventDelivery Delivery = State.WardenEvents.Events[Decision.EventIndex].Delivery;
				Decision.bViaOracle = Decision.Kind == EWardenDecisionKind::Strike
					&& (Delivery == EEventDelivery::Oracle || (Delivery == EEventDelivery::Any && DeliveryDraw < 0.5));
				return Decision;
			}
		}
	}
	// Sin ningún candidato con efecto, o con el comentario elegido: comentario. La garantía sigue contando.
	Decision.Kind = EWardenDecisionKind::Remark;
	return Decision;
}

namespace Soulbound::Rules
{
	EAct CurrentAct(const FMatchState& State, double NowSeconds)
	{
		const FWardenParams& Params = State.WardenParams;
		if (Params.Acts.Num() < Soulbound::Warden::ActCount)
		{
			return EAct::ActOne;
		}
		if (NowSeconds >= Params.Acts[static_cast<int32>(EAct::ActThree)].StartSeconds)
		{
			return EAct::ActThree;
		}
		// R-WAR-2: el fin de la carrera adelanta el Acto II; si la carrera se alarga, el tiempo lo trae igual.
		if (State.MatchPhase == EMatchPhase::BondRace && NowSeconds < Params.Acts[static_cast<int32>(EAct::ActTwo)].StartSeconds)
		{
			return EAct::ActOne;
		}
		return EAct::ActTwo;
	}

	bool IsInWardenMargin(const FMatchState& State, double NowSeconds)
	{
		const FWardenParams& Params = State.WardenParams;
		if (!Params.HasWarden())
		{
			return false;
		}
		if (NowSeconds >= Params.MatchEndSeconds - Params.MarginSeconds)
		{
			return true;
		}
		const double End = ActEndSeconds(Params, CurrentAct(State, NowSeconds));
		return NowSeconds >= End - Params.MarginSeconds && NowSeconds < End;
	}

	bool IsWardenDeferred(const FMatchState& State)
	{
		// S-017 (R-WAR-7): una consulta del Oráculo abierta, aplicándose o esperando es una escena en curso.
		if (State.CurrentEvent.IsActive() || State.Oracle.IsBusy())
		{
			return true;
		}
		switch (State.TurnPhase)
		{
		case ETurnPhase::Duel:
		case ETurnPhase::ShellGame:
		case ETurnPhase::SkillTrial:
		case ETurnPhase::OracleConsult:
		// End es un instante: la máquina pasa de él a BetweenTurns en el mismo avance.
		case ETurnPhase::End:
			return true;
		case ETurnPhase::MoveAndResolve:
			// Sin elección abierta, la casilla de destino aún no ha resuelto: un instante, el siguiente avance la resuelve.
			return !State.PendingChoice.IsOpen();
		default:
			return false;
		}
	}

	TArray<int32> EventCandidates(const FMatchState& State, EWardenChannel Channel, double NowSeconds)
	{
		TArray<int32> Candidates;
		const EAct Act = CurrentAct(State, NowSeconds);
		const bool bTurnInProgress = State.TurnPhase != ETurnPhase::BetweenTurns;
		// Quien lo dirigiría: con un turno en curso, el activo (tras el corte, o en el hueco siguiente, el último que jugó);
		// entre turnos, EventDirector. La validez se mira como quedaría tras cortar: el activo ya no está en juego.
		const int32 Director = bTurnInProgress ? State.ActiveSeat : EventDirector(State);
		FMatchState AfterCut;
		const bool bCanCut = bTurnInProgress && CanInterruptTurn(State);
		if (bCanCut)
		{
			AfterCut = State;
			FRandomStream Unused(0);
			InterruptTurn(AfterCut, NowSeconds, Unused);
		}
		const FMatchState& View = bCanCut ? AfterCut : State;

		const TArray<FWardenEventDef>& Events = State.WardenEvents.Events;
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			const FWardenEventDef& Event = Events[Index];
			// R-SAC-1: de los sacos de este acto y los anteriores, del canal, con peso en el acto, en su ventana y con usos.
			const bool bUsesLeft = !State.Warden.UsesLeft.IsValidIndex(Index) || State.Warden.UsesLeft[Index] != 0;
			if (!Event.bInBag || Event.Act > Act || Event.Channel != Channel || Event.WeightIn(Act) <= 0.0 || !Event.IsInWindow(NowSeconds) || !bUsesLeft)
			{
				continue;
			}
			// R-ORA-1 (S-017): un golpe llega directo o por el Oráculo; una escena solo por "direct" o "any" (que va directa).
			const bool bImplemented = !Event.Options.ContainsByPredicate([](const FWardenEventOption& Option)
			{
				return Option.Applications.ContainsByPredicate([](const FEffectApplication& Application) { return !IsEffectImplemented(Application.Effect); });
			});
			if ((Event.Delivery == EEventDelivery::Oracle && Channel != EWardenChannel::Strike) || !bImplemented)
			{
				continue;
			}
			// R-WAR-8, R-DIR-2, R-DIR-3: con objetivos válidos; un dilema, con al menos dos opciones válidas.
			const bool bCuts = bCanCut && (Event.Channel == EWardenChannel::Strike || Event.bCutsTurn);
			const int32 ValidOptions = Event.Options.FilterByPredicate([&](const FWardenEventOption& Option) { return IsValidOption(View, Option, Director, bCuts); }).Num();
			if (ValidOptions >= (Event.IsDilemma() ? 2 : 1))
			{
				Candidates.Add(Index);
			}
		}
		return Candidates;
	}
}
