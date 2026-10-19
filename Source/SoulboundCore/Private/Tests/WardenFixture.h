#pragma once

// Montaje de los tests del Warden (S-016): la partida de los eventos (EventFixture.h) con los parámetros del Warden de
// Content/Data/Warden/warden.json, con ayudantes para dejar en los sacos solo los eventos que el test mira, hacer que el
// cerebro intervenga siempre por un canal y aplicar decisiones hechas a mano. Lo que se fija a mano es lo que el test
// necesita, no lo que comprueba. Solo para tests.

#include "EventFixture.h"
#include "Warden/WardenBrain.h"
#include "Warden/WardenParams.h"
#include "Warden/WardenRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Soulbound::Tests
{
	/** Ruta de Content/Data/Warden/warden.json. */
	inline FString WardenParamsPath()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Warden/warden.json"));
	}

	/** Carga Content/Data/Warden/warden.json; si falla, lo apunta como error del test y devuelve false. */
	inline bool LoadTestWardenParams(FAutomationTestBase& Test, FWardenParams& OutParams)
	{
		const FWardenParamsLoadResult Result = Soulbound::Rules::LoadWardenParamsFromFile(WardenParamsPath());
		OutParams = Result.Params;
		return Test.TestTrue(FString::Printf(TEXT("Los parámetros del Warden cargan (%s)"), *Result.Message), Result.IsSuccess());
	}

	struct FWardenFixture : FEventFixture
	{
		/**
		 * Partida tras la carrera (acto II) con los eventos y el Warden de los archivos: entre turnos, con el asiento 0 (Fenrir)
		 * como el último que jugó.
		 */
		bool StartWithWarden(FAutomationTestBase& Test)
		{
			if (!LoadTestWardenParams(Test, WardenParams) || !StartAfterRace(Test))
			{
				return false;
			}
			Director(Fenrir);
			return Test.TestTrue(TEXT("La partida tiene Warden"), State.WardenParams.HasWarden() && State.Warden.UsesLeft.Num() == State.WardenEvents.Events.Num());
		}

		/** Partida en la carrera (acto I) con los eventos y el Warden de los archivos, antes del primer turno. */
		bool StartInRace(FAutomationTestBase& Test)
		{
			FBoardState Board;
			FMatchParams Params;
			FElectsTable Elects;
			FWardenEventTable Events;
			if (!LoadTestWardenParams(Test, WardenParams) || !LoadBoardFile(Test, RaceBoardFileName(), Board) || !LoadTestMatchParams(Test, Params)
				|| !LoadTestElects(Test, Elects) || !LoadTestWardenEvents(Test, Events))
			{
				return false;
			}
			FMatchStartResult Result = Soulbound::Rules::StartMatch(Board, Params, Elects, Soulbound::Match::MaxPlayers, Random, Events, WardenParams);
			State = MoveTemp(Result.State);
			return Test.TestTrue(FString::Printf(TEXT("La partida empieza (%s)"), *Result.Message), Result.IsSuccess());
		}

		/** Posición del evento Id en la tabla. */
		int32 IndexOf(const FString& Id) const { return State.WardenEvents.IndexOf(Id); }

		/** Deja en los sacos solo los eventos Ids: el resto pasa a estar fuera, como los de prueba. */
		void KeepOnly(const TArray<FString>& Ids)
		{
			for (FWardenEventDef& Event : State.WardenEvents.Events)
			{
				Event.bInBag = Event.bInBag && Ids.Contains(Event.Id);
			}
		}

		/**
		 * Añade al saco del acto InAct un evento del canal Channel con las opciones dadas, peso 1 en su acto y los siguientes, sin
		 * límite de usos y sin ventana. Corta si bCuts. Devuelve su posición.
		 */
		int32 AddBagEvent(const FString& Id, EAct InAct, EWardenChannel Channel, const TArray<TArray<FEffectApplication>>& Options, bool bCuts)
		{
			AddEvent(Id, Options);
			FWardenEventDef& Event = State.WardenEvents.Events.Last();
			Event.Name = Id;
			Event.bInBag = true;
			Event.Act = InAct;
			Event.Channel = Channel;
			Event.bCutsTurn = bCuts;
			Event.Weights.Reset();
			for (int32 Act = 0; Act < Soulbound::Warden::ActCount; ++Act)
			{
				Event.Weights.Add(Act >= static_cast<int32>(InAct) ? 1.0 : 0.0);
			}
			State.Warden.UsesLeft.SetNum(State.WardenEvents.Events.Num());
			State.Warden.UsesLeft.Last() = -1;
			return State.WardenEvents.Events.Num() - 1;
		}

		/** En todos los actos, el cerebro interviene siempre y solo por el canal Channel. */
		void AlwaysIntervene(EWardenChannel Channel)
		{
			for (FWardenActParams& Act : State.WardenParams.Acts)
			{
				Act.InterveneProbability = 1.0;
				for (int32 Index = 0; Index < Act.ChannelWeights.Num(); ++Index)
				{
					Act.ChannelWeights[Index] = Index == static_cast<int32>(Channel) ? 1.0 : 0.0;
				}
			}
		}

		/** En todos los actos, el cerebro nunca interviene por probabilidad (solo la garantía puede obligarle). */
		void NeverIntervene()
		{
			for (FWardenActParams& Act : State.WardenParams.Acts)
			{
				Act.InterveneProbability = 0.0;
			}
		}

		/** Evaluación del cerebro en At con un sorteo de semilla Seed, sin aplicar nada. */
		FWardenDecision Evaluate(double At, int32 Seed = 11) const
		{
			FRandomStream Draw(Seed);
			return FWardenBrain().Evaluate(State, At, Draw);
		}

		/** Decisión hecha a mano: Kind con el evento Id (vacío: sin evento). */
		FWardenDecision MakeDecision(EWardenDecisionKind Kind, const FString& Id = FString()) const
		{
			FWardenDecision Decision;
			Decision.Kind = Kind;
			Decision.EventIndex = Id.IsEmpty() ? INDEX_NONE : IndexOf(Id);
			Decision.Act = Soulbound::Rules::CurrentAct(State, Now);
			Decision.NextEvaluationSeconds = Now + 15.0;
			return Decision;
		}

		/** Aplica la decisión un segundo después del anterior, con el sorteo de la partida (el del consumo fijo). */
		void Apply(const FWardenDecision& Decision)
		{
			Now += 1.0;
			Soulbound::Rules::ApplyWardenDecision(State, Decision, Now, Random);
		}

		/** Asientos de los eventos de un tipo desde la posición From del registro. */
		TArray<int32> SeatsSince(int32 From, EMatchEventType Type) const
		{
			TArray<int32> Seats;
			for (const FMatchEvent& Event : Since(From, Type))
			{
				Seats.Add(Event.Seat);
			}
			return Seats;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
