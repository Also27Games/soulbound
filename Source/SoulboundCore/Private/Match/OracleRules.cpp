#include "Match/OracleRules.h"

#include "Match/EffectCatalog.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Warden/WardenBrain.h"
#include "Warden/WardenTriggers.h"

namespace Soulbound::Rules
{
	TArray<int32> OracleSentenceCandidates(const FMatchState& State, int32 Consultant, double NowSeconds)
	{
		TArray<int32> Candidates;
		if (!State.Seats.IsValidIndex(Consultant))
		{
			return Candidates;
		}
		const EAct Act = CurrentAct(State, NowSeconds);
		const TArray<FWardenEventDef>& Events = State.WardenEvents.Events;
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			const FWardenEventDef& Event = Events[Index];
			// R-ORA-4: del saco del acto y los anteriores, con peso, en su ventana y con usos (como los candidatos del cerebro).
			const bool bUsesLeft = !State.Warden.UsesLeft.IsValidIndex(Index) || State.Warden.UsesLeft[Index] != 0;
			if (!Event.bInBag || Event.Act > Act || Event.WeightIn(Act) <= 0.0 || !Event.IsInWindow(NowSeconds) || !bUsesLeft)
			{
				continue;
			}
			// R-ORA-1: entregable por el Oráculo y un decreto. Sin Consulta forzada: una sentencia no abre otra consulta.
			if (Event.Delivery == EEventDelivery::Direct || Event.IsDilemma() || Event.Options.IsEmpty())
			{
				continue;
			}
			const TArray<FEffectApplication>& Applications = Event.Options[0].Applications;
			const bool bUsable = !Applications.ContainsByPredicate([](const FEffectApplication& Application)
			{
				return !IsEffectImplemented(Application.Effect) || Application.Effect == EEffect::ForcedOracle;
			});
			// R-DIR-2, R-DIR-3: con algún objetivo válido para el consultante como quien lo dirige. El Alto nunca: una sentencia
			// sorteada no corta ningún turno.
			const bool bValid = Applications.ContainsByPredicate([&](const FEffectApplication& Application)
			{
				return Application.Effect != EEffect::Halt && HasValidTargets(State, Application, Consultant);
			});
			if (bUsable && bValid)
			{
				Candidates.Add(Index);
			}
		}
		return Candidates;
	}

	TArray<TPair<int32, double>> OracleSentenceProbabilities(const FMatchState& State, int32 Consultant, double NowSeconds, const TArray<FActiveTrigger>& Triggers)
	{
		TArray<TPair<int32, double>> Probabilities;
		const TArray<int32> Candidates = OracleSentenceCandidates(State, Consultant, NowSeconds);
		if (Candidates.IsEmpty())
		{
			return Probabilities;
		}
		const EAct Act = CurrentAct(State, NowSeconds);
		TArray<double> Weights;
		double Sum = 0.0;
		for (const int32 Index : Candidates)
		{
			Weights.Add(TriggeredEventWeight(State, Index, Act, Consultant, Triggers));
			Sum += Weights.Last();
		}
		// R-ORA-4: (1 − sesgo) × uniforme + sesgo × el criterio del Warden (el peso del saco por los disparadores).
		const double Bias = FMath::Clamp(State.WardenParams.OracleBias, 0.0, 1.0);
		const double Uniform = 1.0 / Candidates.Num();
		for (int32 Position = 0; Position < Candidates.Num(); ++Position)
		{
			const double Brain = Sum > 0.0 ? Weights[Position] / Sum : Uniform;
			Probabilities.Emplace(Candidates[Position], (1.0 - Bias) * Uniform + Bias * Brain);
		}
		return Probabilities;
	}

	int32 DrawOracleSentence(const FMatchState& State, int32 Consultant, double NowSeconds, const TArray<FActiveTrigger>& Triggers, FRandomStream& Random)
	{
		const TArray<TPair<int32, double>> Probabilities = OracleSentenceProbabilities(State, Consultant, NowSeconds, Triggers);
		if (Probabilities.IsEmpty())
		{
			return INDEX_NONE;
		}
		double Point = Random.FRand();
		for (const TPair<int32, double>& Candidate : Probabilities)
		{
			if (Point < Candidate.Value)
			{
				return Candidate.Key;
			}
			Point -= Candidate.Value;
		}
		return Probabilities.Last().Key;
	}
}
