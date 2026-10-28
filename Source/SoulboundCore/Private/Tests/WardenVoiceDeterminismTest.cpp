// Test de que la voz no cambia la partida (S-021, CA-VOZ-4, R-WAR-9): con las frases cargadas, la secuencia de eventos de una partida
// simulada, sin contar WardenSpoke ni MatchEndWarning, es la de la partida sin voz; y el informe del tablero de 86 da las cifras de
// S-018 (corte B), la línea base de la bitácora, a los dos ritmos. La simulación es determinista, así que son igualdades exactas (las
// cifras de la bitácora están redondeadas a la precisión con que se publicaron). Filtro: "Soulbound.Core.Voice".

#include "Decider/Decider.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"
#include "Simulation/MatchSimulation.h"
#include "VoiceFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 Games = 50;
	const TCHAR* const Board86 = TEXT("tablero_86.json");

	TArray<TSharedPtr<IDecider>> FourRaceDummies()
	{
		TArray<TSharedPtr<IDecider>> Deciders;
		for (int32 i = 0; i < Soulbound::Match::SeatCount; ++i)
		{
			Deciders.Add(MakeShared<FRaceDummyDecider>());
		}
		return Deciders;
	}

	/** Parámetros de la simulación del tablero de 86 con el ritmo dado, con o sin las frases. */
	bool MakeParams(FAutomationTestBase& Test, const FSimulationPace& Pace, const FWardenLineTable* Lines, FSimulationParams& OutParams)
	{
		if (!Soulbound::Tests::LoadBoardFile(Test, Board86, OutParams.Board) || !Soulbound::Tests::LoadTestMatchParams(Test, OutParams.MatchParams)
			|| !Soulbound::Tests::LoadTestElects(Test, OutParams.Elects) || !Soulbound::Tests::LoadTestWardenEvents(Test, OutParams.WardenEvents)
			|| !Soulbound::Tests::LoadTestWardenParams(Test, OutParams.WardenParams))
		{
			return false;
		}
		OutParams.Pace = Pace;
		if (Lines)
		{
			OutParams.WardenLines = *Lines;
		}
		return true;
	}

	/** El registro sin la voz: ni WardenSpoke ni MatchEndWarning. Del comentario no se compara el tipo ni el asiento (lo de A2). */
	TArray<FMatchEvent> WithoutVoice(const FMatchState& State)
	{
		TArray<FMatchEvent> Events = State.EventLog.FilterByPredicate([](const FMatchEvent& Event)
		{
			return Event.Type != EMatchEventType::WardenSpoke && Event.Type != EMatchEventType::MatchEndWarning;
		});
		for (FMatchEvent& Event : Events)
		{
			if (Event.Type == EMatchEventType::WardenRemark)
			{
				Event.Seat = INDEX_NONE;
				Event.Value = INDEX_NONE;
			}
		}
		return Events;
	}

	bool SameEvents(const TArray<FMatchEvent>& A, const TArray<FMatchEvent>& B, FString& OutWhere)
	{
		if (A.Num() != B.Num())
		{
			OutWhere = FString::Printf(TEXT("%d eventos contra %d"), A.Num(), B.Num());
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Type != B[Index].Type || A[Index].ClockSeconds != B[Index].ClockSeconds || A[Index].Seat != B[Index].Seat
				|| A[Index].OtherSeat != B[Index].OtherSeat || A[Index].Value != B[Index].Value || A[Index].Detail != B[Index].Detail)
			{
				OutWhere = FString::Printf(TEXT("el evento %d (tipo %d)"), Index, static_cast<int32>(A[Index].Type));
				return false;
			}
		}
		return true;
	}

	/** Redondea a un decimal, la precisión con que se publicó la cifra. */
	double OneDecimal(double Value) { return FMath::RoundToDouble(Value * 10.0) / 10.0; }

	/** Las cifras del informe de un ritmo (50 partidas, semillas 1 a 50), las que la bitácora de S-018 (B) publica. */
	struct FReportFigures
	{
		int32 PlayerWins = 0;
		int32 TimeOuts = 0;
		double WinMinuteSum = 0.0;
		int32 Challenges = 0;
		int32 EchoSeats = 0;
		int32 Seats = 0;
		int32 Spoken = 0;
		TArray<int32> TurnsStarted;
		TArray<int32> TurnsCut;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceDoesNotChangeTheMatchTest, "Soulbound.Core.Voice.LaVozNoCambiaLaPartida",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoiceDoesNotChangeTheMatchTest::RunTest(const FString& Parameters)
{
	// CA-VOZ-4 (R-WAR-9): con la misma semilla, la secuencia de eventos de una partida simulada sin contar WardenSpoke ni MatchEndWarning
	// (ni el tipo y asiento de WardenRemark) es idéntica a la de la partida sin voz; el informe del tablero de 86 da, con voz y a los dos
	// ritmos, las cifras de S-018 (B): rápido, gana un jugador el 58 % (minuto 19,5) y 2,00 desafíos por partida; humano, el 6 % (minuto
	// 29,0), el 94 % el Warden por tiempo, 0,26 desafíos, Ecos 18,5 % y turnos cortados por acto 14,8 / 21,3 / 31,5 %.
	FWardenLineTable Lines;
	if (!Soulbound::Tests::LoadTestWardenLines(*this, Lines))
	{
		return false;
	}
	const TArray<TPair<const TCHAR*, FSimulationPace>> Paces = {{TEXT("rápido"), FSimulationPace::Fast()}, {TEXT("humano"), FSimulationPace::Human()}};
	TArray<FReportFigures> Figures;
	for (const TPair<const TCHAR*, FSimulationPace>& Pace : Paces)
	{
		FSimulationParams Silent;
		FSimulationParams Voiced;
		if (!MakeParams(*this, Pace.Value, nullptr, Silent) || !MakeParams(*this, Pace.Value, &Lines, Voiced))
		{
			return false;
		}
		FReportFigures& Report = Figures.AddDefaulted_GetRef();
		Report.TurnsStarted.Init(0, Soulbound::Warden::ActCount);
		Report.TurnsCut.Init(0, Soulbound::Warden::ActCount);
		for (int32 Seed = 1; Seed <= Games; ++Seed)
		{
			Silent.Seed = Seed;
			Voiced.Seed = Seed;
			const FSimulationResult Without = FMatchSimulation(Silent, FourRaceDummies()).Run();
			const FSimulationResult With = FMatchSimulation(Voiced, FourRaceDummies()).Run();

			FString Where;
			TestTrue(FString::Printf(TEXT("Ritmo %s, semilla %d: sin contar la voz, el mismo registro (%s)"), Pace.Key, Seed, *Where),
				SameEvents(WithoutVoice(Without.FinalState), WithoutVoice(With.FinalState), Where));
			TestEqual(FString::Printf(TEXT("Ritmo %s, semilla %d: sin frases no habla"), Pace.Key, Seed),
				Without.FinalState.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::WardenSpoke; }).Num(), 0);
			TestEqual(FString::Printf(TEXT("Ritmo %s, semilla %d: el mismo resultado"), Pace.Key, Seed), static_cast<int32>(With.FinalState.Outcome), static_cast<int32>(Without.FinalState.Outcome));
			TestEqual(FString::Printf(TEXT("Ritmo %s, semilla %d: el mismo final"), Pace.Key, Seed), With.FinalState.ClockSeconds, Without.FinalState.ClockSeconds);
			TestEqual(FString::Printf(TEXT("Ritmo %s, semilla %d: el mismo sorteo del Warden"), Pace.Key, Seed), With.FinalState.Warden.Draws, Without.FinalState.Warden.Draws);

			const FMatchState& State = With.FinalState;
			Report.Spoken += State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::WardenSpoke; }).Num();
			Report.PlayerWins += State.Outcome == EMatchOutcome::PlayerVictory ? 1 : 0;
			Report.TimeOuts += State.Outcome == EMatchOutcome::WardenVictory && State.EndReason == EMatchEndReason::TimeOut ? 1 : 0;
			Report.WinMinuteSum += State.Outcome == EMatchOutcome::PlayerVictory ? State.ClockSeconds / 60.0 : 0.0;
			Report.Challenges += State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::FinalChallengeStarted; }).Num();
			for (const FSeatState& Seat : State.Seats)
			{
				Report.EchoSeats += Seat.Role == ESeatRole::LostEcho ? 1 : 0;
				++Report.Seats;
			}
			if (TestEqual(TEXT("Las cifras del Warden de los tres actos"), With.WardenActs.Num(), Soulbound::Warden::ActCount))
			{
				for (int32 Act = 0; Act < Soulbound::Warden::ActCount; ++Act)
				{
					Report.TurnsStarted[Act] += With.WardenActs[Act].TurnsStarted;
					Report.TurnsCut[Act] += With.WardenActs[Act].TurnsCut;
				}
			}
		}
		// La voz habla de verdad en estas partidas: al menos las siete del inicio por partida.
		TestTrue(FString::Printf(TEXT("Ritmo %s: con frases, la voz habla (%d frases)"), Pace.Key, Report.Spoken), Report.Spoken >= 7 * Games);
	}
	if (Figures.Num() != 2)
	{
		return false;
	}

	// Rápido: gana un jugador el 58 % (29 de 50), en el minuto 19,5, y 2,00 desafíos por partida.
	const FReportFigures& Fast = Figures[0];
	TestEqual(TEXT("Rápido: gana un jugador el 58 % (29 de 50)"), Fast.PlayerWins, 29);
	TestEqual(TEXT("Rápido: gana el Warden por tiempo el 42 % (21 de 50)"), Fast.TimeOuts, 21);
	TestEqual(TEXT("Rápido: minuto medio de la victoria 19,5"), OneDecimal(Fast.WinMinuteSum / FMath::Max(1, Fast.PlayerWins)), 19.5);
	TestEqual(TEXT("Rápido: 2,00 desafíos por partida (100 de 50)"), Fast.Challenges, 100);

	// Humano: 6 % (3 de 50), minuto 29,0, 94 % el Warden por tiempo, 0,26 desafíos, Ecos 18,5 % y cortes por acto 14,8 / 21,3 / 31,5 %.
	const FReportFigures& Human = Figures[1];
	TestEqual(TEXT("Humano: gana un jugador el 6 % (3 de 50)"), Human.PlayerWins, 3);
	TestEqual(TEXT("Humano: gana el Warden por tiempo el 94 % (47 de 50)"), Human.TimeOuts, 47);
	TestEqual(TEXT("Humano: minuto medio de la victoria 29,0"), OneDecimal(Human.WinMinuteSum / FMath::Max(1, Human.PlayerWins)), 29.0);
	TestEqual(TEXT("Humano: 0,26 desafíos por partida (13 de 50)"), Human.Challenges, 13);
	TestEqual(TEXT("Humano: Ecos 18,5 %"), OneDecimal(100.0 * Human.EchoSeats / FMath::Max(1, Human.Seats)), 18.5);
	const double ExpectedCuts[Soulbound::Warden::ActCount] = {14.8, 21.3, 31.5};
	for (int32 Act = 0; Act < Soulbound::Warden::ActCount; ++Act)
	{
		TestEqual(FString::Printf(TEXT("Humano: turnos cortados en el acto %d, %.1f %%"), Act + 1, ExpectedCuts[Act]),
			OneDecimal(Human.TurnsStarted[Act] > 0 ? 100.0 * Human.TurnsCut[Act] / Human.TurnsStarted[Act] : 0.0), ExpectedCuts[Act]);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
