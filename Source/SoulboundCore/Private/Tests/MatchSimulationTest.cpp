// Tests de la simulación con reloj virtual (R-SIM-1 a 5 del GDD 08). Desde S-003 la simulación inicia la partida
// con StartMatch sobre el tablero de pruebas y juega los turnos con la máquina de turnos del motor.
// Determinismo por semilla, coste por partida y uso de la máquina de turnos. Desde S-004, los Sellos de partidas
// enteras cumplen R-TAB-22 y la crónica cuenta los que caen por el tope. Desde S-006, la carrera por el vínculo con
// cuatro dummies de carrera: simulación y crónica sobre el tablero de 35 casillas, e informe de 50 semillas por cada
// tablero de Content/Data/Boards. Los tests del motor no leen la configuración del juego.
// Se lanzan con Scripts/test.ps1.

#include "Decider/Decider.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"
#include "Simulation/MatchSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundTest, Log, All);

namespace
{
	TArray<TSharedPtr<IDecider>> FourDummies()
	{
		TArray<TSharedPtr<IDecider>> Deciders;
		for (int32 i = 0; i < Soulbound::Match::SeatCount; ++i)
		{
			Deciders.Add(MakeShared<FDummyDecider>());
		}
		return Deciders;
	}

	/** Cuatro dummies de carrera (S-006): buscan el Nicho Libre más cercano mientras son Ecos en carrera. */
	TArray<TSharedPtr<IDecider>> FourRaceDummies()
	{
		TArray<TSharedPtr<IDecider>> Deciders;
		for (int32 i = 0; i < Soulbound::Match::SeatCount; ++i)
		{
			Deciders.Add(MakeShared<FRaceDummyDecider>());
		}
		return Deciders;
	}

	/**
	 * Decisor de prueba que no se parece al dummy: gasta tres valores del generador que recibe y elige la última
	 * casilla alcanzable. Si la simulación le diera el sorteo de la partida, las tiradas de los demás cambiarían.
	 */
	class FLastOptionDecider : public IDecider
	{
	public:
		virtual int32 ChooseDestination(const FMatchState& State, int32 Seat, const TArray<int32>& Options, FRandomStream& Rng) override
		{
			for (int32 Draw = 0; Draw < 3; ++Draw)
			{
				Rng.GetFraction();
			}
			return Options.IsEmpty() ? INDEX_NONE : Options.Last();
		}
	};

	/** Tiradas de la partida en orden, tal como las registró el motor. */
	TArray<int32> RollSequence(const FSimulationResult& Result)
	{
		TArray<int32> Rolls;
		for (const FMatchEvent& Event : Result.FinalState.EventLog)
		{
			if (Event.Type == EMatchEventType::Rolled)
			{
				Rolls.Add(Event.Value);
			}
		}
		return Rolls;
	}

	/** Parámetros de simulación con el tablero de pruebas y los parámetros de partida de sus archivos de datos. */
	bool MakeParams(FAutomationTestBase& Test, int32 Seed, FSimulationParams& OutParams)
	{
		OutParams.Seed = Seed;
		return Soulbound::Tests::LoadTestBoard(Test, OutParams.Board) && Soulbound::Tests::LoadTestMatchParams(Test, OutParams.MatchParams);
	}

	/** Parámetros de simulación con el tablero BoardFileName de Content/Data/Boards y los parámetros de partida del archivo. */
	bool MakeBoardParams(FAutomationTestBase& Test, const FString& BoardFileName, int32 Seed, FSimulationParams& OutParams)
	{
		OutParams.Seed = Seed;
		return Soulbound::Tests::LoadBoardFile(Test, BoardFileName, OutParams.Board) && Soulbound::Tests::LoadTestMatchParams(Test, OutParams.MatchParams);
	}

	int32 CountEvents(const FMatchState& State, EMatchEventType Type)
	{
		return Soulbound::Tests::EventsOfType(State, Type).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationDeterminismTest,
	"Soulbound.Core.Simulation.DeterminismoPorSemilla",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationDeterminismTest::RunTest(const FString& Parameters)
{
	// Semilla fija 42.
	FSimulationParams Params;
	if (!MakeParams(*this, 42, Params))
	{
		return false;
	}

	const FSimulationResult A = FMatchSimulation(Params, FourDummies()).Run();
	const FSimulationResult B = FMatchSimulation(Params, FourDummies()).Run();

	// R-SIM-2: misma semilla, mismo registro, línea a línea, y mismo estado final.
	TestEqual(TEXT("Dos ejecuciones con la misma semilla tienen el mismo numero de lineas"), A.Log.Num(), B.Log.Num());
	for (int32 i = 0; i < FMath::Min(A.Log.Num(), B.Log.Num()); ++i)
	{
		if (A.Log[i] != B.Log[i])
		{
			AddError(FString::Printf(TEXT("Divergencia en la linea %d: '%s' frente a '%s'"), i, *A.Log[i], *B.Log[i]));
			break;
		}
	}
	TestTrue(TEXT("Misma semilla, mismo estado final"), Soulbound::Tests::SameState(A.FinalState, B.FinalState));

	// Misma semilla, mismos dados, ocupe quien ocupe cada asiento (grabación y reproducción, GDD 08): los decisores
	// no tocan el sorteo de la partida. Se cambia el decisor del asiento 2 por uno que gasta y elige distinto.
	TArray<TSharedPtr<IDecider>> Mixed = FourDummies();
	Mixed[2] = MakeShared<FLastOptionDecider>();
	const FSimulationResult Other = FMatchSimulation(Params, MoveTemp(Mixed)).Run();
	const TArray<int32> RollsA = RollSequence(A);
	const TArray<int32> RollsOther = RollSequence(Other);
	TestTrue(TEXT("Hay tiradas"), RollsA.Num() > 0);
	TestEqual(TEXT("Con otro decisor en un asiento, el mismo número de tiradas"), RollsOther.Num(), RollsA.Num());
	for (int32 i = 0; i < FMath::Min(RollsA.Num(), RollsOther.Num()); ++i)
	{
		if (RollsA[i] != RollsOther[i])
		{
			AddError(FString::Printf(TEXT("Con otro decisor en el asiento 2, la tirada %d cambia: %d frente a %d"), i, RollsA[i], RollsOther[i]));
			break;
		}
	}
	TestFalse(TEXT("Y la partida es otra: el decisor distinto sí cambia los destinos"), Soulbound::Tests::SameState(A.FinalState, Other.FinalState));

	// Los turnos no son una constante: salen de los parámetros. El dummy confirma a los DummyResponseSeconds de
	// empezar su turno, antes del reloj de turno, así que caben duración entre respuesta (margen de uno por redondeo).
	// Hasta S-003 aquí se exigían 80 turnos: el turno de mentira del spike duraba TurnPeriodSeconds (30 s).
	const double ExpectedTurns = Params.DurationSeconds / Params.DummyResponseSeconds;
	TestTrue(TEXT("Se juegan turnos"), A.Turns > 0);
	TestTrue(FString::Printf(TEXT("Turnos derivados de los parametros: %d frente a %.1f"), A.Turns, ExpectedTurns), FMath::Abs(A.Turns - ExpectedTurns) <= 1.0);
	TestEqual(TEXT("Evaluaciones del Warden"), A.Evaluations, FMath::FloorToInt32(Params.DurationSeconds / Params.EvaluationPeriodSeconds));
	TestTrue(TEXT("El Warden intervino alguna vez"), A.Interventions > 0);
	TestTrue(TEXT("El Warden no intervino siempre"), A.Interventions < A.Evaluations);

	// Otra semilla, otro registro.
	Params.Seed = 43;
	const FSimulationResult C = FMatchSimulation(Params, FourDummies()).Run();
	bool bDifferent = C.Log.Num() != A.Log.Num();
	for (int32 i = 0; !bDifferent && i < A.Log.Num(); ++i)
	{
		bDifferent = A.Log[i] != C.Log[i];
	}
	TestTrue(TEXT("Semillas distintas dan registros distintos"), bDifferent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationPerformanceTest,
	"Soulbound.Core.Simulation.CostePorPartida",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationPerformanceTest::RunTest(const FString& Parameters)
{
	FSimulationParams Params;
	if (!MakeParams(*this, 7, Params))
	{
		return false;
	}

	// R-SIM-1: una partida en menos de un segundo, ya con la máquina de turnos del motor.
	const FSimulationResult One = FMatchSimulation(Params, FourDummies()).Run();
	const FString OneLine = FString::Printf(TEXT("Una partida de 40 min virtuales: %.4f s reales, %d turnos, %d lineas de registro"), One.WallSeconds, One.Turns, One.Log.Num());
	AddInfo(OneLine);
	UE_LOG(LogSoulboundTest, Display, TEXT("%s"), *OneLine);
	TestTrue(TEXT("Una partida tarda menos de un segundo"), One.WallSeconds < 1.0);

	double Total = 0.0;
	for (int32 i = 0; i < 100; ++i)
	{
		Params.Seed = 1000 + i;
		Total += FMatchSimulation(Params, FourDummies()).Run().WallSeconds;
	}
	const FString HundredLine = FString::Printf(TEXT("Cien partidas: %.3f s reales (%.4f s por partida)"), Total, Total / 100.0);
	AddInfo(HundredLine);
	UE_LOG(LogSoulboundTest, Display, TEXT("%s"), *HundredLine);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationTurnMachineTest,
	"Soulbound.Core.Simulation.UsaLaMaquinaDeTurnos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationTurnMachineTest::RunTest(const FString& Parameters)
{
	// Criterio "Simulación" (S-003): la simulación juega con la máquina de turnos del motor. Lo que queda en el
	// registro de eventos del estado final solo lo escriben las reglas de turno: inicios en el orden sorteado,
	// una tirada por turno, un fin por turno jugado y ningún agotamiento (el dummy responde antes del reloj).
	FSimulationParams Params;
	if (!MakeParams(*this, 11, Params))
	{
		return false;
	}
	const FSimulationResult Result = FMatchSimulation(Params, FourDummies()).Run();
	const FMatchState& State = Result.FinalState;
	if (!TestEqual(TEXT("La partida tiene orden de turno"), State.TurnOrder.Num(), Soulbound::Match::SeatCount))
	{
		return false;
	}

	int32 Started = 0, Rolled = 0, Ended = 0, Expired = 0;
	for (const FMatchEvent& Event : State.EventLog)
	{
		switch (Event.Type)
		{
		case EMatchEventType::TurnStarted:
			if (Event.Seat != State.TurnOrder[Started % State.TurnOrder.Num()])
			{
				AddError(FString::Printf(TEXT("El turno %d lo empieza el asiento %d, fuera del orden sorteado"), Started, Event.Seat));
			}
			++Started;
			break;
		case EMatchEventType::Rolled: ++Rolled; break;
		case EMatchEventType::TurnEnded: ++Ended; break;
		case EMatchEventType::TurnClockExpired: ++Expired; break;
		default: break;
		}
	}
	TestTrue(TEXT("Hay turnos en el registro"), Ended > 0);
	TestEqual(TEXT("Un fin por turno jugado"), Ended, Result.Turns);
	TestEqual(TEXT("Turnos jugados del estado"), State.TurnCount, Result.Turns);
	TestTrue(TEXT("Cada inicio tiene su tirada (el último turno puede quedar a medias)"), Started == Rolled && (Started == Ended || Started == Ended + 1));
	TestEqual(TEXT("El dummy nunca agota el reloj"), Expired, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationChronicleTest,
	"Soulbound.Core.Simulation.CronicaDeUnaPartida",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationChronicleTest::RunTest(const FString& Parameters)
{
	// Herramienta de lectura, no un criterio: el registro de una partida entera con dummies de carrera sobre el tablero
	// de 35 casillas (semilla fija 42; hasta S-005, dummies al azar sobre el tablero de pruebas), para leer el
	// ritmo turno a turno: quién juega, qué tira, adónde va, quién se vincula, cuándo acaba la carrera y qué hace el
	// Warden. Siempre comprueba que el registro cuenta la partida entera; solo lo imprime con la variable de entorno
	// SOULBOUND_CRONICA=1, que pone Scripts/test.ps1 -Cronica (líneas "info" de la consola). Sin ella, una línea de
	// resumen.
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 42, Params))
	{
		return false;
	}
	const FSimulationResult Result = FMatchSimulation(Params, FourRaceDummies()).Run();

	auto CountLines = [&Result](const TCHAR* Fragment)
	{
		return Result.Log.FilterByPredicate([Fragment](const FString& Line) { return Line.Contains(Fragment, ESearchCase::CaseSensitive); }).Num();
	};
	const int32 Started = Result.FinalState.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::TurnStarted; }).Num();
	TestEqual(TEXT("Una línea de turno por cada turno empezado"), CountLines(TEXT("] Turno ")), Started);
	TestEqual(TEXT("Cada turno terminado cuenta cómo acabó"), CountLines(TEXT(" va a la casilla ")) + CountLines(TEXT(" no puede moverse")) + CountLines(TEXT(" agota el reloj")), Result.Turns);
	TestEqual(TEXT("Una línea por intervención del Warden"), CountLines(TEXT("] Warden ")), Result.Interventions);

	TestEqual(TEXT("Cada Sello que cae por el tope tiene su línea"), CountLines(TEXT(" cae el Sello de la ")),
		Result.FinalState.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::SealReleased; }).Num()
		- CountLines(TEXT(" Warden libera la ")));

	// S-006: vínculos y fin de carrera, una línea por evento.
	const FMatchState& Final = Result.FinalState;
	TestEqual(TEXT("Cada vínculo tiene su línea"), CountLines(TEXT(" se vincula al Elegido ")), CountEvents(Final, EMatchEventType::Bonded));
	TestEqual(TEXT("El Despertar tiene su línea"), CountLines(TEXT(" despierta el tablero")), CountEvents(Final, EMatchEventType::BoardAwakening));
	TestEqual(TEXT("La carrera termina y tiene su línea"), CountLines(TEXT(" Fin de la carrera ")), 1);
	TestEqual(TEXT("Cada hogar asignado tiene su línea"), CountLines(TEXT(" queda Eco ")), CountEvents(Final, EMatchEventType::HomeAssigned));
	TestEqual(TEXT("Cada vuelta al hogar tiene su línea"), CountLines(TEXT(" vuelve a su hogar")), CountEvents(Final, EMatchEventType::ReturnedHome));
	TestEqual(TEXT("La victoria del Warden tiene su línea"), CountLines(TEXT(" Victoria del Warden")), CountEvents(Final, EMatchEventType::WardenVictory));

	if (FPlatformMisc::GetEnvironmentVariable(TEXT("SOULBOUND_CRONICA")) == TEXT("1"))
	{
		for (const FString& Line : Result.Log)
		{
			UE_LOG(LogSoulboundTest, Display, TEXT("%s"), *Line);
		}
	}
	else
	{
		UE_LOG(LogSoulboundTest, Display, TEXT("crónica: %d líneas; ejecuta test.ps1 -Cronica para verla"), Result.Log.Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationSealsTest,
	"Soulbound.Core.Simulation.SellosDentroDeLasReglas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationSealsTest::RunTest(const FString& Parameters)
{
	// Criterio "Cerebro del spike" (S-004, R-TAB-22) en partidas enteras: se rehace el tablero evento a evento desde
	// el registro y, después de cada Sello, nunca hay más de M, ninguno en casilla prohibida y toda casilla no sellada
	// tiene camino al Altar (nadie queda encerrado por Sellos). Ninguna intervención de sellar se pierde: cada línea
	// "Warden sella" de la crónica tiene su SealPlaced. Diez semillas, empezando por la de la crónica.
	int32 CapReleases = 0;
	for (int32 Seed = 42; Seed < 52; ++Seed)
	{
		FSimulationParams Params;
		if (!MakeParams(*this, Seed, Params))
		{
			return false;
		}
		const FSimulationResult Result = FMatchSimulation(Params, FourDummies()).Run();

		FBoardState Board = Result.FinalState.Board;
		Board.SealedTiles.Reset();
		for (FTileState& Tile : Board.Tiles)
		{
			Tile.bSealed = false;
		}
		int32 Placed = 0, Released = 0;
		bool bBroken = false;
		for (const FMatchEvent& Event : Result.FinalState.EventLog)
		{
			const bool bSealEvent = Event.Type == EMatchEventType::SealPlaced || Event.Type == EMatchEventType::SealReleased;
			if (bSealEvent && !Board.IsValidTile(Event.Value))
			{
				AddError(FString::Printf(TEXT("Semilla %d: evento de Sello con una casilla inexistente, la %d"), Seed, Event.Value));
				bBroken = true;
				break;
			}
			if (Event.Type == EMatchEventType::SealReleased)
			{
				++Released;
				Board.Tiles[Event.Value].bSealed = false;
				Board.SealedTiles.Remove(Event.Value);
				continue;
			}
			if (Event.Type != EMatchEventType::SealPlaced)
			{
				continue;
			}
			++Placed;
			const FTileState& Tile = Board.Tiles[Event.Value];
			Board.Tiles[Event.Value].bSealed = true;
			Board.SealedTiles.Add(Event.Value);
			const TArray<int32> CutOff = Soulbound::Tests::TilesCutOffFromAltar(Board);
			if (Board.SealedTiles.Num() > Board.Params.MaxSealedTiles || Tile.Type == ETileType::Altar || Tile.Type == ETileType::Niche
				|| Tile.Type == ETileType::PowerSource || !CutOff.IsEmpty())
			{
				AddError(FString::Printf(TEXT("Semilla %d, [%.0f s]: tras sellar la %d hay %d Sellos y %d casillas sin camino al Altar"),
					Seed, Event.ClockSeconds, Event.Value, Board.SealedTiles.Num(), CutOff.Num()));
				bBroken = true;
				break;
			}
		}
		if (bBroken)
		{
			break;
		}
		TestTrue(FString::Printf(TEXT("Semilla %d: los Sellos rehechos desde el registro son los del estado final"), Seed), Board.SealedTiles == Result.FinalState.Board.SealedTiles);

		auto CountLines = [&Result](const TCHAR* Fragment)
		{
			return Result.Log.FilterByPredicate([Fragment](const FString& Line) { return Line.Contains(Fragment, ESearchCase::CaseSensitive); }).Num();
		};
		TestEqual(FString::Printf(TEXT("Semilla %d: cada intervención de sellar sella"), Seed), Placed, CountLines(TEXT("] Warden sella la ")));
		CapReleases += Released - CountLines(TEXT("] Warden libera la "));
	}
	TestTrue(TEXT("En alguna partida el tope entra en juego"), CapReleases > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationBondRaceTest,
	"Soulbound.Core.Simulation.CarreraConDummiesDeCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationBondRaceTest::RunTest(const FString& Parameters)
{
	// Criterio "Simulación" (S-006): dada la simulación con cuatro dummies de carrera sobre el tablero de 35 casillas,
	// la partida entera termina, es determinista por semilla y baja del segundo. Semilla fija 42; otra, la 43.
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 42, Params))
	{
		return false;
	}
	const FSimulationResult A = FMatchSimulation(Params, FourRaceDummies()).Run();
	const FSimulationResult B = FMatchSimulation(Params, FourRaceDummies()).Run();

	TestTrue(TEXT("Se juegan turnos"), A.Turns > 0);
	TestEqual(TEXT("La carrera termina una vez"), CountEvents(A.FinalState, EMatchEventType::BondRaceEnded), 1);
	TestTrue(TEXT("La partida termina: o se acaba o llega a su duración"),
		A.FinalState.IsMatchOver() || A.FinalState.ClockSeconds >= Params.DurationSeconds - Params.EvaluationPeriodSeconds);
	TestTrue(TEXT("Misma semilla, mismo registro"), A.Log == B.Log);
	TestTrue(TEXT("Misma semilla, mismo estado final"), Soulbound::Tests::SameState(A.FinalState, B.FinalState));
	TestTrue(FString::Printf(TEXT("Una partida tarda menos de un segundo (%.4f s)"), A.WallSeconds), A.WallSeconds < 1.0);

	FSimulationParams Other = Params;
	Other.Seed = 43;
	TestTrue(TEXT("Otra semilla, otro registro"), FMatchSimulation(Other, FourRaceDummies()).Run().Log != A.Log);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationBondRaceReportTest,
	"Soulbound.Core.Simulation.InformeDeCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationBondRaceReportTest::RunTest(const FString& Parameters)
{
	// Herramienta de lectura, no un criterio sobre cifras (criterio "Simulación" de S-006): por cada tablero de
	// Content/Data/Boards, 50 partidas con cuatro dummies de carrera (semillas fijas 1 a 50). Imprime un bloque por
	// tablero: % de partidas sin ningún vínculo, Elegidos por partida de media, % de asientos que quedan Eco y segundo
	// medio de fin de carrera. Objetivo del GDD 03, para el autor: nadie vinculado < 5 %, Eco por jugador entre 15 y
	// 35 %. Solo comprueba que cada partida cierra su carrera. Imprime con SOULBOUND_INFORME=1 (Scripts/test.ps1
	// -Informe); sin ella, una línea de resumen.
	constexpr int32 Games = 50;
	const TArray<FString> Boards = Soulbound::Tests::BoardFileNames();
	if (!TestTrue(TEXT("Hay archivos de tablero"), Boards.Num() > 0))
	{
		return false;
	}

	TArray<FString> Lines;
	for (const FString& BoardFile : Boards)
	{
		FSimulationParams Params;
		if (!MakeBoardParams(*this, BoardFile, 1, Params))
		{
			continue;
		}
		int32 WardenVictories = 0;
		int32 Elects = 0;
		int32 EchoSeats = 0;
		int32 Seats = 0;
		double RaceEndSum = 0.0;
		double WallSum = 0.0;
		for (int32 Seed = 1; Seed <= Games; ++Seed)
		{
			Params.Seed = Seed;
			const FSimulationResult Result = FMatchSimulation(Params, FourRaceDummies()).Run();
			const FMatchState& State = Result.FinalState;
			const TArray<FMatchEvent> Ended = Soulbound::Tests::EventsOfType(State, EMatchEventType::BondRaceEnded);
			if (!TestEqual(FString::Printf(TEXT("%s, semilla %d: la carrera termina una vez"), *BoardFile, Seed), Ended.Num(), 1))
			{
				continue;
			}
			RaceEndSum += Ended[0].ClockSeconds;
			WardenVictories += State.Outcome == EMatchOutcome::WardenVictory ? 1 : 0;
			for (const FSeatState& Seat : State.Seats)
			{
				Elects += Seat.Role == ESeatRole::Elect ? 1 : 0;
				EchoSeats += Seat.Role == ESeatRole::LostEcho ? 1 : 0;
				++Seats;
			}
			WallSum += Result.WallSeconds;
		}

		Lines.Add(FString::Printf(TEXT("Informe de carrera | %s | %d casillas | %d partidas | 4 dummies de carrera | carrera de %.0f s"),
			*BoardFile, Params.Board.Tiles.Num(), Games, Params.MatchParams.RaceDurationSeconds));
		Lines.Add(FString::Printf(TEXT("    partidas sin ningún vínculo (victoria del Warden): %.1f %% (objetivo < 5 %%)"), 100.0 * WardenVictories / Games));
		Lines.Add(FString::Printf(TEXT("    Elegidos por partida, de media: %.2f de 4"), static_cast<double>(Elects) / Games));
		Lines.Add(FString::Printf(TEXT("    asientos que quedan Eco: %.1f %% (objetivo por jugador entre 15 y 35 %%)"), Seats > 0 ? 100.0 * EchoSeats / Seats : 0.0));
		Lines.Add(FString::Printf(TEXT("    fin de carrera, segundo medio: %.1f"), RaceEndSum / Games));
		Lines.Add(FString::Printf(TEXT("    tiempo real: %.3f s en total"), WallSum));
	}

	if (FPlatformMisc::GetEnvironmentVariable(TEXT("SOULBOUND_INFORME")) == TEXT("1"))
	{
		for (const FString& Line : Lines)
		{
			UE_LOG(LogSoulboundTest, Display, TEXT("%s"), *Line);
		}
	}
	else
	{
		UE_LOG(LogSoulboundTest, Display, TEXT("informe de carrera: %d tableros; ejecuta test.ps1 -Informe para verlo"), Boards.Num());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
