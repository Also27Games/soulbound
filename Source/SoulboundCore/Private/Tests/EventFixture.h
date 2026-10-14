#pragma once

// Montaje de los tests de efectos y eventos del Warden (S-014): la partida de las elecciones (ChoiceFixture.h, tablero de 35
// casillas) con los eventos de Content/Data/Events/eventos.json, acabada la carrera, con ayudantes para lanzar eventos
// hechos a medida, contestar y agotar sus elecciones y fijar quién dirige. Los eventos de un test se añaden a la tabla del
// estado a mano: es lo que el test necesita, no lo que comprueba. Solo para tests.

#include "ChoiceFixture.h"
#include "Match/EffectRules.h"
#include "Match/WardenEventRules.h"
#include "Match/WardenEvents.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Soulbound::Tests
{
	/** Ruta de Content/Data/Events/eventos.json. */
	inline FString WardenEventsPath()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Events/eventos.json"));
	}

	/** Carga Content/Data/Events/eventos.json; si falla, lo apunta como error del test y devuelve false. */
	inline bool LoadTestWardenEvents(FAutomationTestBase& Test, FWardenEventTable& OutTable)
	{
		const FWardenEventsLoadResult Result = Soulbound::Rules::LoadWardenEventsFromFile(WardenEventsPath());
		OutTable = Result.Table;
		return Test.TestTrue(FString::Printf(TEXT("Los eventos del Warden cargan (%s)"), *Result.Message), Result.IsSuccess());
	}

	/** Una aplicación de efecto: efecto, dirección, cantidad y variante. */
	inline FEffectApplication App(EEffect Effect, EEffectTarget Target, int32 Amount = 1, EEffectVariant Variant = EEffectVariant::None)
	{
		FEffectApplication Application;
		Application.Effect = Effect;
		Application.Target = Target;
		Application.Amount = Amount;
		Application.Variant = Variant;
		return Application;
	}

	/**
	 * Partida de cuatro tras la carrera, con los eventos del archivo: el asiento 0 se vincula a Fenrir (Dominio 1) y el 1 a
	 * Koschei (Dominio 5); el 2 y el 3 quedan Ecos con hogar. Nadie tiene runas; todos en su hogar; entre turnos.
	 */
	struct FEventFixture : FChoiceFixture
	{
		static constexpr int32 FenrirDomain = 1;
		static constexpr int32 KoscheiDomain = 5;
		static constexpr int32 Fenrir = 0;
		static constexpr int32 Koschei = 1;
		static constexpr int32 EchoA = 2;
		static constexpr int32 EchoB = 3;

		/** El sorteo del Warden: el que reciben los eventos. Nunca el de la partida (FChoiceFixture::Random). */
		FRandomStream WardenRandom{7};

		bool StartAfterRace(FAutomationTestBase& Test)
		{
			FBoardState Board;
			FMatchParams Params;
			FElectsTable Elects;
			FWardenEventTable Events;
			if (!LoadBoardFile(Test, RaceBoardFileName(), Board) || !LoadTestMatchParams(Test, Params) || !LoadTestElects(Test, Elects)
				|| !LoadTestWardenEvents(Test, Events))
			{
				return false;
			}
			FMatchStartResult Result = Soulbound::Rules::StartMatch(Board, Params, Elects, Soulbound::Match::MaxPlayers, Random, Events);
			State = MoveTemp(Result.State);
			if (!Test.TestTrue(FString::Printf(TEXT("La partida empieza (%s)"), *Result.Message), Result.IsSuccess())
				|| !Bond(Test, Fenrir, FenrirDomain) || !Bond(Test, Koschei, KoscheiDomain))
			{
				return false;
			}
			if (!Test.TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(State, Now, Random)))
			{
				return false;
			}
			for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
			{
				State.Seats[Seat].Runes.Init(0, Soulbound::Board::DomainCount);
				State.Seats[Seat].TileId = Soulbound::Rules::HomeTile(State, Seat);
			}
			State.TurnPhase = ETurnPhase::BetweenTurns;
			return Test.TestTrue(TEXT("Los asientos 2 y 3 quedan Ecos con hogar"), State.Seats[EchoA].Role == ESeatRole::LostEcho
				&& State.Seats[EchoA].DomainId != INDEX_NONE && State.Seats[EchoB].Role == ESeatRole::LostEcho && State.Seats[EchoB].DomainId != INDEX_NONE);
		}

		/** Asiento que sigue a Seat en el orden de turno. */
		int32 NextOf(int32 Seat) const
		{
			const int32 Index = State.TurnOrder.Find(Seat);
			return State.TurnOrder[(Index + 1) % State.TurnOrder.Num()];
		}

		/** Asiento anterior a Seat en el orden de turno: el que tiene a Seat de siguiente. */
		int32 PreviousOf(int32 Seat) const
		{
			const int32 Index = State.TurnOrder.Find(Seat);
			return State.TurnOrder[(Index + State.TurnOrder.Num() - 1) % State.TurnOrder.Num()];
		}

		/** Seat acaba de jugar: entre turnos, es quien dirige el evento (R-DIR-1) y le toca al siguiente del orden. */
		void Director(int32 Seat)
		{
			State.TurnPhase = ETurnPhase::BetweenTurns;
			State.LastTurnSeat = Seat;
			State.ActiveSeat = NextOf(Seat);
		}

		/** Añade a la tabla del estado un evento con las opciones dadas (cada una, sus aplicaciones). Devuelve su identificador. */
		FString AddEvent(const FString& Id, const TArray<TArray<FEffectApplication>>& Options)
		{
			FWardenEventDef& Event = State.WardenEvents.Events.AddDefaulted_GetRef();
			Event.Id = Id;
			Event.Text = Id;
			for (const TArray<FEffectApplication>& Applications : Options)
			{
				Event.Options.AddDefaulted_GetRef().Applications = Applications;
			}
			return Id;
		}

		/** Lanza el evento Id un segundo después del anterior, con el sorteo del Warden. */
		EWardenEventStart Launch(const FString& Id)
		{
			Now += 1.0;
			return Soulbound::Rules::ApplyEvent(State, Id, Now, WardenRandom);
		}

		/** Lanza un decreto de una sola aplicación, con identificador único, dirigido por DirectorSeat. */
		EWardenEventStart Decree(int32 DirectorSeat, const FEffectApplication& Application)
		{
			Director(DirectorSeat);
			return Launch(AddEvent(FString::Printf(TEXT("decreto-%d"), State.WardenEvents.Events.Num()), {{Application}}));
		}

		/** Contesta la elección abierta con Option, por el asiento al que se le pide. */
		EIntentResult Answer(int32 Option)
		{
			return Choose(State.PendingChoice.Seat, State.PendingChoice.Kind, Option);
		}

		/** Deja pasar el plazo de la elección abierta y avanza la máquina. Devuelve si había elección abierta. */
		bool Expire()
		{
			if (!State.PendingChoice.IsOpen())
			{
				return false;
			}
			Now = FMath::Max(Now, State.PendingChoice.DeadlineSeconds);
			Advance();
			return true;
		}

		/** Una casilla normal del Dominio del hogar de Seat, que no es su hogar: para sacarlo de casa sin salir de su Dominio. */
		int32 AwayTile(int32 Seat) const
		{
			const TArray<int32> Tiles = NormalTiles(State.Seats[Seat].DomainId);
			return Tiles.IsEmpty() ? INDEX_NONE : Tiles[0];
		}

		/** Saca a Seat de su hogar, a AwayTile. */
		void MoveAway(int32 Seat)
		{
			State.Seats[Seat].TileId = AwayTile(Seat);
		}

		/** Eventos de un tipo desde la posición From del registro. */
		TArray<FMatchEvent> Since(int32 From, EMatchEventType Type) const
		{
			TArray<FMatchEvent> Events;
			for (int32 Index = From; Index < State.EventLog.Num(); ++Index)
			{
				if (State.EventLog[Index].Type == Type)
				{
					Events.Add(State.EventLog[Index]);
				}
			}
			return Events;
		}

		/** Número total de runas de Seat. */
		int32 RuneTotal(int32 Seat) const
		{
			int32 Total = 0;
			for (const int32 Count : State.Seats[Seat].Runes)
			{
				Total += Count;
			}
			return Total;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
