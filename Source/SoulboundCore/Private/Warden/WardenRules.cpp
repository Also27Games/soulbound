#include "Warden/WardenRules.h"

#include "Match/MatchEvents.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "Templates/TypeHash.h"

using Soulbound::Rules::LogEvent;

namespace Soulbound::Rules
{
	FRandomStream WardenDraw(FMatchState& State)
	{
		// Como el sorteo del evento en curso: la semilla combinada con el número de sorteo. Nada de esto toca los dados.
		FWardenState& Warden = State.Warden;
		return FRandomStream(static_cast<int32>(HashCombineFast(GetTypeHash(Warden.Seed), GetTypeHash(Warden.Draws++))));
	}

	bool IsWardenEvaluationDue(const FMatchState& State, double NowSeconds)
	{
		if (!State.WardenParams.HasWarden() || State.IsMatchOver())
		{
			return false;
		}
		// R-WAR-7: una aplazada se vuelve a evaluar en cuanto termina lo que la aplazó, sin esperar al intervalo.
		if (State.Warden.bDeferred)
		{
			return !IsWardenDeferred(State);
		}
		return NowSeconds >= State.Warden.NextEvaluationSeconds;
	}

	FWardenDecision EvaluateWarden(FMatchState& State, double NowSeconds, FRandomStream& MatchRandom)
	{
		FRandomStream Draw = WardenDraw(State);
		const FWardenDecision Decision = FWardenBrain().Evaluate(State, NowSeconds, Draw);
		// R-WAR-10: cada evaluación y cada decisión dejan evento.
		LogEvent(State, EMatchEventType::WardenEvaluated, NowSeconds, INDEX_NONE, static_cast<int32>(Decision.Act), INDEX_NONE, Decision.bForcedByGuarantee ? 1 : 0);
		LogEvent(State, EMatchEventType::WardenDecided, NowSeconds, INDEX_NONE, Decision.EventIndex, INDEX_NONE, static_cast<int32>(Decision.Kind));
		// R-DIS-1, R-WAR-10 (S-017): los disparadores activos y quien los causa, uno por disparador (decisión del autor del 23/10/2026).
		for (const FActiveTrigger& Trigger : Decision.Triggers)
		{
			LogEvent(State, EMatchEventType::WardenTriggered, NowSeconds, Trigger.Seat, static_cast<int32>(Trigger.Trigger));
		}
		// R-WAR-3, R-WAR-7: el instante siguiente vive en el estado; una aplazada no lo cambia, se re-evalúa al terminar.
		State.Warden.bDeferred = Decision.Kind == EWardenDecisionKind::Deferred;
		if (!State.Warden.bDeferred)
		{
			State.Warden.NextEvaluationSeconds = Decision.NextEvaluationSeconds;
		}
		ApplyWardenDecision(State, Decision, NowSeconds, MatchRandom);
		return Decision;
	}

	void ApplyWardenDecision(FMatchState& State, const FWardenDecision& Decision, double NowSeconds, FRandomStream& MatchRandom)
	{
		const int32 Index = Decision.EventIndex;
		switch (Decision.Kind)
		{
		case EWardenDecisionKind::Remark:
			// TODO(R-WAR-4): el comentario tiene tipo y frase en la story "La voz del Warden" (decisión del autor del 22/10/2026);
			// aquí solo queda registrado.
			LogEvent(State, EMatchEventType::WardenRemark, NowSeconds, INDEX_NONE);
			break;
		case EWardenDecisionKind::Strike:
		{
			// R-TUR-11: el golpe corta el turno en curso y su evento entra, en la misma llamada, en el hueco que deja, con el
			// cortado como activo. Sin turno en curso, entra sin cortar (decisión aceptada por el autor el 19/10/2026).
			const bool bCut = InterruptTurn(State, NowSeconds, MatchRandom);
			if (!bCut && State.TurnPhase != ETurnPhase::BetweenTurns)
			{
				break;
			}
			// R-DIS-3 (S-017): el protagonista de un golpe deja de estar ignorado.
			const int32 Protagonist = EventDirector(State);
			if (State.Seats.IsValidIndex(Protagonist))
			{
				State.Seats[Protagonist].LastTargetedSeconds = NowSeconds;
			}
			if (Decision.bViaOracle)
			{
				// R-ORA-1, R-ORA-2 (S-017): por el Oráculo, el decreto que eligió el cerebro es la sentencia del cortado. Cuenta para la
				// garantía y ocupa el hueco ya (decisión del autor del 23/10/2026); el uso lo gasta al revelarse.
				if (OpenOracle(State, Protagonist, EOracleOrigin::Strike, Index, bCut, NowSeconds))
				{
					State.Warden.LastEffectSeconds = NowSeconds;
					State.Warden.bGapUsed = true;
				}
				break;
			}
			LaunchWardenEvent(State, Index, bCut, NowSeconds);
			break;
		}
		case EWardenDecisionKind::Scene:
		{
			if (!State.WardenEvents.Events.IsValidIndex(Index))
			{
				break;
			}
			// R-WAR-4: la escena entra entre turnos; con un turno en curso, corta solo si su evento corta, y si no espera al hueco.
			if (State.TurnPhase == ETurnPhase::BetweenTurns)
			{
				LaunchWardenEvent(State, Index, false, NowSeconds);
			}
			else if (State.WardenEvents.Events[Index].bCutsTurn && InterruptTurn(State, NowSeconds, MatchRandom))
			{
				LaunchWardenEvent(State, Index, true, NowSeconds);
			}
			else
			{
				State.Warden.PendingScene = Index;
			}
			break;
		}
		case EWardenDecisionKind::None:
		case EWardenDecisionKind::Deferred:
			break;
		}
	}

	EWardenEventStart LaunchWardenEvent(FMatchState& State, int32 EventIndex, bool bInterruptedTurn, double NowSeconds)
	{
		if (!State.WardenEvents.Events.IsValidIndex(EventIndex))
		{
			return EWardenEventStart::UnknownEvent;
		}
		// Los sorteos del evento salen de un valor del sorteo del Warden (S-014), ahora el del estado.
		FRandomStream Draw = WardenDraw(State);
		const EWardenEventStart Result = ApplyEvent(State, State.WardenEvents.Events[EventIndex].Id, NowSeconds, Draw, bInterruptedTurn);
		if (Result == EWardenEventStart::Started)
		{
			// R-SAC-1: un uso menos; R-WAR-5: cuenta para la garantía; R-WAR-4: el hueco ya tuvo su intervención.
			if (State.Warden.UsesLeft.IsValidIndex(EventIndex) && State.Warden.UsesLeft[EventIndex] > 0)
			{
				--State.Warden.UsesLeft[EventIndex];
			}
			State.Warden.LastEffectSeconds = NowSeconds;
			State.Warden.bGapUsed = true;
		}
		return Result;
	}
}
