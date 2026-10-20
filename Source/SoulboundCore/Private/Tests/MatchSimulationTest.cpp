// Tests de la simulación con reloj virtual (R-SIM-1 a 5 del GDD 08). Desde S-003 la simulación inicia la partida
// con StartMatch sobre el tablero de pruebas y juega los turnos con la máquina de turnos del motor.
// Determinismo por semilla, coste por partida y uso de la máquina de turnos. Desde S-004, los Sellos de partidas
// enteras cumplen R-TAB-22 y la crónica cuenta los que caen por el tope. Desde S-006, la carrera por el vínculo con
// cuatro dummies de carrera: simulación y crónica sobre el tablero de 35 casillas, e informe de 50 semillas por cada
// tablero de Content/Data/Boards. Desde S-009, las elecciones de los dummies (dados, repetición, salto y expulsión) y
// sus cifras en el informe. Desde S-010, los robos de los dummies (trilero) y sus cifras en el informe. Desde S-012, los
// duelos de los dummies y sus cifras en el informe. Los tests del motor no leen la configuración del juego.
// Se lanzan con Scripts/test.ps1.

#include "Decider/Decider.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"
#include "Simulation/MatchSimulation.h"
#include "WardenFixture.h"

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

		virtual bool PassSkillMinigame(const FMatchState& State, int32 Seat, ESkillTrial Trial, ESkillDifficulty Difficulty, FRandomStream& Rng) override
		{
			return true;
		}

		/** S-009: también gasta tres valores y elige la última opción: dos dados, repetir, el último Nodo, la última acción (S-010), la última esfera. */
		virtual int32 ChooseOption(const FMatchState& State, int32 Seat, const FPendingChoice& Choice, FRandomStream& Rng) override
		{
			for (int32 Draw = 0; Draw < 3; ++Draw)
			{
				Rng.GetFraction();
			}
			return Choice.AnswerAt(Choice.AnswerCount() - 1);
		}

		/** S-012: también gasta tres valores y rinde lo máximo. */
		virtual double DuelPerformance(const FMatchState& State, int32 Seat, FRandomStream& Rng) override
		{
			for (int32 Draw = 0; Draw < 3; ++Draw)
			{
				Rng.GetFraction();
			}
			return 1.0;
		}
	};

	/** Una tirada del registro y con cuántos dados se hizo (S-009: dos si el asiento lo eligió con Celeridad). */
	struct FLoggedRoll
	{
		int32 Roll = 0;
		int32 Dice = Soulbound::Dice::DefaultCount;
	};

	/** Tiradas de la partida en orden, tal como las registró el motor, con sus dados (DiceChosen del mismo turno). */
	TArray<FLoggedRoll> RollSequence(const FSimulationResult& Result)
	{
		TArray<FLoggedRoll> Rolls;
		int32 Dice = Soulbound::Dice::DefaultCount;
		for (const FMatchEvent& Event : Result.FinalState.EventLog)
		{
			if (Event.Type == EMatchEventType::TurnStarted)
			{
				Dice = Soulbound::Dice::DefaultCount;
			}
			else if (Event.Type == EMatchEventType::DiceChosen)
			{
				Dice = Event.Value;
			}
			else if (Event.Type == EMatchEventType::Rolled)
			{
				Rolls.Add({Event.Value, Dice});
			}
		}
		return Rolls;
	}

	/** Los eventos y los parámetros del Warden de sus archivos (S-016): la simulación juega con el Warden real. */
	bool LoadWarden(FAutomationTestBase& Test, FSimulationParams& OutParams)
	{
		return Soulbound::Tests::LoadTestWardenEvents(Test, OutParams.WardenEvents) && Soulbound::Tests::LoadTestWardenParams(Test, OutParams.WardenParams);
	}

	/** Parámetros de simulación con el tablero de pruebas, los parámetros de partida y el Warden de sus archivos de datos. */
	bool MakeParams(FAutomationTestBase& Test, int32 Seed, FSimulationParams& OutParams)
	{
		OutParams.Seed = Seed;
		return Soulbound::Tests::LoadTestBoard(Test, OutParams.Board) && Soulbound::Tests::LoadTestMatchParams(Test, OutParams.MatchParams)
			&& Soulbound::Tests::LoadTestElects(Test, OutParams.Elects) && LoadWarden(Test, OutParams);
	}

	/** Parámetros de simulación con el tablero BoardFileName de Content/Data/Boards, los parámetros de partida y el Warden de sus archivos. */
	bool MakeBoardParams(FAutomationTestBase& Test, const FString& BoardFileName, int32 Seed, FSimulationParams& OutParams)
	{
		OutParams.Seed = Seed;
		return Soulbound::Tests::LoadBoardFile(Test, BoardFileName, OutParams.Board) && Soulbound::Tests::LoadTestMatchParams(Test, OutParams.MatchParams)
			&& Soulbound::Tests::LoadTestElects(Test, OutParams.Elects) && LoadWarden(Test, OutParams);
	}

	/** Lo más que puede tardar el Warden en volver a evaluar (S-016): intervalo más variación. */
	double MaxEvaluationGap(const FSimulationParams& Params)
	{
		return Params.WardenParams.EvaluationIntervalSeconds + Params.WardenParams.EvaluationVariationSeconds;
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
	// Semilla fija 42. Desde S-016, con el Warden apagado (decisión del autor del 19/10/2026): el Warden real corta turnos y
	// decide según el estado, así que con otro decisor sus cortes (y con ellos los dados que consume cada turno) serían otros.
	// Apagado, la partida conserva las tiradas de S-010; el registro del Warden con la misma semilla lo comprueba
	// RegistroDelWardenDeterminista.
	FSimulationParams Params;
	if (!MakeParams(*this, 42, Params))
	{
		return false;
	}
	Params.bWardenEnabled = false;

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
	// no tocan el sorteo de la partida. Se cambia el decisor del asiento 2 por uno que gasta y elige distinto. Desde
	// S-009 una tirada consume siempre los mismos valores del sorteo (decisión del autor del 04/10/2026), pero con
	// Celeridad su resultado depende de cuántos dados se eligieron: se comparan las tiradas hechas con los mismos dados
	// en las dos partidas. Desde S-010 un trilero dura tiempo de partida, así que con otro decisor caben otros turnos en
	// 40 minutos: se comparan las tiradas de los turnos que juegan las dos (las primeras N, con N el menor de los dos
	// recuentos), y deben coincidir todas. El Warden tiene su propio sorteo (Seed + 2, decisión del autor del 05/10/2026):
	// el retraso de un trilero no cambia en qué punto del sorteo cae cada tirada.
	TArray<TSharedPtr<IDecider>> Mixed = FourDummies();
	Mixed[2] = MakeShared<FLastOptionDecider>();
	const FSimulationResult Other = FMatchSimulation(Params, MoveTemp(Mixed)).Run();
	const TArray<FLoggedRoll> RollsA = RollSequence(A);
	const TArray<FLoggedRoll> RollsOther = RollSequence(Other);
	TestTrue(TEXT("Hay tiradas"), RollsA.Num() > 0);
	const int32 Shared = FMath::Min(RollsA.Num(), RollsOther.Num());
	AddInfo(FString::Printf(TEXT("Tiradas: %d con cuatro dummies, %d con otro decisor en el asiento 2; se comparan las %d primeras"), RollsA.Num(), RollsOther.Num(), Shared));
	int32 Compared = 0;
	int32 Mismatches = 0;
	for (int32 i = 0; i < Shared; ++i)
	{
		if (RollsA[i].Dice != RollsOther[i].Dice)
		{
			continue;
		}
		++Compared;
		if (RollsA[i].Roll != RollsOther[i].Roll && Mismatches++ == 0)
		{
			AddError(FString::Printf(TEXT("Con otro decisor en el asiento 2, la tirada %d cambia: %d frente a %d"), i, RollsA[i].Roll, RollsOther[i].Roll));
		}
	}
	TestEqual(TEXT("Todas las tiradas comparables coinciden"), Mismatches, 0);
	TestTrue(TEXT("Hay tiradas con los mismos dados que comparar"), Compared > 0);
	TestFalse(TEXT("Y la partida es otra: el decisor distinto sí cambia los destinos"), Soulbound::Tests::SameState(A.FinalState, Other.FinalState));

	// Los turnos no son una constante: salen de los parámetros. El dummy confirma a los DummyResponseSeconds de
	// empezar su turno, antes del reloj de turno, así que caben duración entre respuesta (margen de uno por redondeo).
	// Hasta S-003 aquí se exigían 80 turnos: el turno de mentira del spike duraba TurnPeriodSeconds (30 s). Desde S-010 se
	// descuenta el tiempo de los trileros: cada uno alarga su turno la muestra y la mezcla (la esfera, el dummy la elige al
	// instante).
	const double ShellSeconds = Soulbound::Tests::EventsOfType(A.FinalState, EMatchEventType::ShellGameStarted).Num()
		* (Params.MatchParams.ShellRevealSeconds + Params.MatchParams.ShellShuffleSeconds);
	const double ExpectedTurns = (Params.DurationSeconds - ShellSeconds) / Params.DummyResponseSeconds;
	TestTrue(TEXT("Se juegan turnos"), A.Turns > 0);
	TestTrue(FString::Printf(TEXT("Turnos derivados de los parametros: %d frente a %.1f"), A.Turns, ExpectedTurns), FMath::Abs(A.Turns - ExpectedTurns) <= 1.0);
	// Hasta S-015 aquí se exigía una evaluación del cerebro del spike cada 15 s y alguna intervención; apagado, ninguna.
	TestEqual(TEXT("Con el Warden apagado, no evalúa"), A.Evaluations, 0);
	TestEqual(TEXT("Ni interviene"), A.Interventions, 0);

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
	// Desde S-016, con el Warden apagado: lo que se comprueba es la máquina de turnos, y un golpe que corta un turno antes de
	// tirar (o una Prueba fallada, o el Vacío) deja inicios sin tirada. El Warden en la simulación lo comprueban sus tests.
	Params.bWardenEnabled = false;
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
	// Desde S-016 hay turnos que no llegan a tirar (cortados antes, en el Vacío, con la Prueba fallada): la línea de turno
	// sale con la tirada, y cada turno que tiró cuenta cómo acabó, también cortado por un golpe (si movió, su movimiento y su
	// corte).
	int32 RolledTurnsEnded = 0;
	int32 CutAfterRolling = 0;
	int32 CutAfterMoving = 0;
	bool bRolled = false;
	for (const FMatchEvent& Event : Result.FinalState.EventLog)
	{
		bRolled = Event.Type == EMatchEventType::TurnStarted ? false : bRolled || Event.Type == EMatchEventType::Rolled;
		RolledTurnsEnded += Event.Type == EMatchEventType::TurnEnded && bRolled ? 1 : 0;
		if (Event.Type == EMatchEventType::TurnInterrupted)
		{
			CutAfterRolling += Event.Value == static_cast<int32>(ETurnPhase::RollAndChoose) || Event.Value == static_cast<int32>(ETurnPhase::Moving)
				|| Event.Value == static_cast<int32>(ETurnPhase::MoveAndResolve) ? 1 : 0;
			CutAfterMoving += Event.Value == static_cast<int32>(ETurnPhase::MoveAndResolve) ? 1 : 0;
		}
	}
	TestTrue(TEXT("Se empiezan turnos"), Started > 0);
	TestEqual(TEXT("Una línea de turno por cada tirada"), CountLines(TEXT("] Turno ")), CountEvents(Result.FinalState, EMatchEventType::Rolled));
	TestEqual(TEXT("Cada movimiento del registro tiene su línea de crónica"), CountLines(TEXT(" va a la casilla ")), CountEvents(Result.FinalState, EMatchEventType::Moved));
	TestEqual(TEXT("Cada turno con tirada cuenta cómo acabó"), CountLines(TEXT(" va a la casilla ")) + CountLines(TEXT(" no puede moverse")) + CountLines(TEXT(" agota el reloj"))
		+ CutAfterRolling - CutAfterMoving, RolledTurnsEnded);
	// S-020 (decisión del autor del 20/10/2026): una línea por movimiento con su camino, y en ella cada casilla en la que entró.
	TestEqual(TEXT("Cada movimiento tiene su línea de camino"), CountLines(TEXT(" camino del asiento ")), CountEvents(Result.FinalState, EMatchEventType::Moved));
	int32 PathTiles = 0;
	for (const FString& Line : Result.Log)
	{
		FString Tiles;
		if (Line.Contains(TEXT(" camino del asiento "), ESearchCase::CaseSensitive) && Line.Split(TEXT(": "), nullptr, &Tiles))
		{
			TArray<FString> Parts;
			PathTiles += Tiles.ParseIntoArray(Parts, TEXT(", "));
		}
	}
	TestEqual(TEXT("Las líneas de camino cuentan cada casilla entrada"), PathTiles, CountEvents(Result.FinalState, EMatchEventType::TileEntered));
	TestTrue(TEXT("Se entra en casillas"), PathTiles > 0);
	// S-010: con esta semilla, un salto rechazado abría la acción en el Nodo y la simulación se quedaba esperándola hasta
	// el final (defecto de S-009). Al acabar, ninguna elección puede seguir abierta con su plazo vencido.
	const FPendingChoice& Left = Result.FinalState.PendingChoice;
	TestFalse(FString::Printf(TEXT("Ninguna elección queda sin contestar más allá de su plazo (tipo %d, plazo %.0f s)"), static_cast<int32>(Left.Kind), Left.DeadlineSeconds),
		Left.IsOpen() && Left.DeadlineSeconds < Result.FinalState.ClockSeconds);
	// S-016: el Warden real. Una línea por intervención con efecto (el evento, con su canal y su nombre), una por comentario y
	// una por cada turno que corta, con la fase.
	TestEqual(TEXT("Una línea por intervención del Warden"), CountLines(TEXT("] Warden lanza ")), Result.Interventions);
	TestTrue(TEXT("El Warden interviene"), Result.Interventions > 0);
	TestEqual(TEXT("Una línea por comentario"), CountLines(TEXT("] Warden comenta")), CountEvents(Result.FinalState, EMatchEventType::WardenRemark));
	TestEqual(TEXT("Una línea por turno cortado"), CountLines(TEXT(" corta el turno ")), CountEvents(Result.FinalState, EMatchEventType::TurnInterrupted));

	// Desde S-016 nadie libera un Sello a mano: los que caen son los del tope.
	TestEqual(TEXT("Cada Sello que cae por el tope tiene su línea"), CountLines(TEXT(" cae el Sello de la ")), CountEvents(Result.FinalState, EMatchEventType::SealReleased));

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
	// Criterio de S-004 (R-TAB-22) en partidas enteras: se rehace el tablero evento a evento desde el registro y, después de
	// cada Sello, nunca hay más de M, ninguno en casilla prohibida y toda casilla no sellada tiene camino al Altar (nadie
	// queda encerrado por Sellos). Desde S-016 los Sellos son los de los eventos del Warden real (Primer muro, Arquitecto...):
	// cada Sello que se aplica (EffectApplied de Sello o de Sello contra rival) sella. Diez semillas, empezando por la de la
	// crónica.
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
		const int32 SealsApplied = Result.FinalState.EventLog.FilterByPredicate([](const FMatchEvent& Event)
		{
			return Event.Type == EMatchEventType::EffectApplied && (Event.Value == static_cast<int32>(EEffect::PlaceSeal) || Event.Value == static_cast<int32>(EEffect::SealAgainstRival));
		}).Num();
		TestEqual(FString::Printf(TEXT("Semilla %d: cada Sello aplicado sella"), Seed), Placed, SealsApplied);
		TestEqual(FString::Printf(TEXT("Semilla %d: cada Sello tiene su línea"), Seed), CountLines(TEXT(" sella la ")), Placed);
		CapReleases += Released;
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
		A.FinalState.IsMatchOver() || A.FinalState.ClockSeconds >= Params.DurationSeconds - MaxEvaluationGap(Params));
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
	// medio de fin de carrera; desde S-009, también el % de tiradas con dos dados y los saltos planares y expulsiones por
	// partida; desde S-010, los robos y las runas que cambian de mano por partida; desde S-012, los duelos por partida
	// (objetivo del GDD 04: de 3 a 8) y cuántos gana el retado. Objetivo del GDD 03, para el autor: nadie vinculado < 5 %, Eco por jugador entre 15 y
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
		TArray<int32> RunesPerSeat;
		RunesPerSeat.Init(0, Soulbound::Match::SeatCount);
		int32 SourcesTotal = 0;
		int32 SourcesDepleted = 0;
		int32 Rolls = 0;
		int32 TwoDiceRolls = 0;
		int32 Jumps = 0;
		int32 Expulsions = 0;
		int32 Thefts = 0;
		int32 StolenRunes = 0;
		int32 Duels = 0;
		int32 DuelsWon = 0;
		int32 DuelsWonByChallenged = 0;
		// S-016: el Warden por acto (intervenciones con efecto, turnos empezados y cortados, hueco más largo).
		TArray<FWardenActReport> WardenActs;
		WardenActs.SetNum(Soulbound::Warden::ActCount);
		int32 Furies = 0;
		for (int32 Seed = 1; Seed <= Games; ++Seed)
		{
			Params.Seed = Seed;
			const FSimulationResult Result = FMatchSimulation(Params, FourRaceDummies()).Run();
			if (TestEqual(FString::Printf(TEXT("%s, semilla %d: cifras del Warden de los tres actos"), *BoardFile, Seed), Result.WardenActs.Num(), Soulbound::Warden::ActCount))
			{
				int32 Interventions = 0;
				for (int32 Act = 0; Act < Soulbound::Warden::ActCount; ++Act)
				{
					const FWardenActReport& Report = Result.WardenActs[Act];
					TestTrue(TEXT("Un acto no corta más turnos de los que empieza"), Report.TurnsCut <= Report.TurnsStarted);
					WardenActs[Act].EffectInterventions += Report.EffectInterventions;
					WardenActs[Act].TurnsStarted += Report.TurnsStarted;
					WardenActs[Act].TurnsCut += Report.TurnsCut;
					WardenActs[Act].MaxGapSeconds = FMath::Max(WardenActs[Act].MaxGapSeconds, Report.MaxGapSeconds);
					Interventions += Report.EffectInterventions;
				}
				TestEqual(TEXT("Las intervenciones por acto suman las de la partida"), Interventions, Result.Interventions);
			}
			// S-016 (B): las Furias, contadas del registro.
			TestEqual(TEXT("Las Furias del resultado son las del registro"), Result.Furies, CountEvents(Result.FinalState, EMatchEventType::FuryUnleashed));
			Furies += Result.Furies;
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
			const TArray<FMatchEvent> Obtained = Soulbound::Tests::EventsOfType(State, EMatchEventType::RuneObtained);
			for (const FMatchEvent& Event : Obtained)
			{
				// Runas obtenidas por asiento, contadas del registro: las devueltas no restan.
				RunesPerSeat[Event.Seat] += 1;
			}
			for (const FTileState& Tile : State.Board.Tiles)
			{
				SourcesTotal += Tile.Type == ETileType::PowerSource ? 1 : 0;
				SourcesDepleted += Tile.Type == ETileType::PowerSource && State.Board.Phase == EBoardPhase::Awakened && Tile.RuneReserve == 0 ? 1 : 0;
			}
			// S-009: turnos con dos dados (sobre las tiradas), saltos planares y expulsiones.
			Rolls += CountEvents(State, EMatchEventType::Rolled);
			TwoDiceRolls += Soulbound::Tests::EventsOfType(State, EMatchEventType::DiceChosen).FilterByPredicate([](const FMatchEvent& Event) { return Event.Value == 2; }).Num();
			Jumps += CountEvents(State, EMatchEventType::PlanarJumped);
			Expulsions += CountEvents(State, EMatchEventType::Expelled);
			// S-010: robos (todo intento: con trilero, a quien no tiene runas o anulado por Koschei) y runas que cambian de mano.
			Thefts += CountEvents(State, EMatchEventType::ShellGameStarted) + CountEvents(State, EMatchEventType::StealNoRunes)
				+ CountEvents(State, EMatchEventType::StealCancelled);
			StolenRunes += CountEvents(State, EMatchEventType::RuneStolen);
			// S-012: duelos (todo reto: con duelo o a quien no tiene runas) y cuántos se resuelven a favor del retado.
			Duels += CountEvents(State, EMatchEventType::DuelChallenged) + CountEvents(State, EMatchEventType::DuelNoRunes);
			const TArray<FMatchEvent> Won = Soulbound::Tests::EventsOfType(State, EMatchEventType::DuelWon);
			DuelsWon += Won.Num();
			DuelsWonByChallenged += Won.FilterByPredicate([](const FMatchEvent& Event) { return Event.Value == 1; }).Num();
		}

		Lines.Add(FString::Printf(TEXT("Informe de carrera | %s | %d casillas | %d partidas | 4 dummies de carrera | carrera de %.0f s"),
			*BoardFile, Params.Board.Tiles.Num(), Games, Params.MatchParams.RaceDurationSeconds));
		Lines.Add(FString::Printf(TEXT("    partidas sin ningún vínculo (victoria del Warden): %.1f %% (objetivo < 5 %%)"), 100.0 * WardenVictories / Games));
		Lines.Add(FString::Printf(TEXT("    Elegidos por partida, de media: %.2f de 4"), static_cast<double>(Elects) / Games));
		Lines.Add(FString::Printf(TEXT("    asientos que quedan Eco: %.1f %% (objetivo por jugador entre 15 y 35 %%)"), Seats > 0 ? 100.0 * EchoSeats / Seats : 0.0));
		Lines.Add(FString::Printf(TEXT("    fin de carrera, segundo medio: %.1f"), RaceEndSum / Games));
		Lines.Add(FString::Printf(TEXT("    runas obtenidas por asiento, de media por partida: %s"),
			*FString::JoinBy(RunesPerSeat, TEXT(" | "), [](int32 Sum) { return FString::Printf(TEXT("%.2f"), static_cast<double>(Sum) / Games); })));
		Lines.Add(FString::Printf(TEXT("    Fuentes agotadas al final: %.1f %%"), SourcesTotal > 0 ? 100.0 * SourcesDepleted / SourcesTotal : 0.0));
		Lines.Add(FString::Printf(TEXT("    turnos con dos dados: %.1f %% de las tiradas"), Rolls > 0 ? 100.0 * TwoDiceRolls / Rolls : 0.0));
		Lines.Add(FString::Printf(TEXT("    saltos planares por partida, de media: %.2f"), static_cast<double>(Jumps) / Games));
		Lines.Add(FString::Printf(TEXT("    expulsiones por partida, de media: %.2f"), static_cast<double>(Expulsions) / Games));
		Lines.Add(FString::Printf(TEXT("    robos por partida, de media: %.2f (desde S-012, también el botín de los duelos)"), static_cast<double>(Thefts) / Games));
		Lines.Add(FString::Printf(TEXT("    runas que cambian de mano por partida, de media: %.2f"), static_cast<double>(StolenRunes) / Games));
		Lines.Add(FString::Printf(TEXT("    duelos por partida, de media: %.2f (objetivo del GDD 04: de 3 a 8)"), static_cast<double>(Duels) / Games));
		Lines.Add(FString::Printf(TEXT("    duelos que gana el retado: %d de %d (%.1f %%)"), DuelsWonByChallenged, DuelsWon,
			DuelsWon > 0 ? 100.0 * DuelsWonByChallenged / DuelsWon : 0.0));
		static const TCHAR* const ActNames[] = {TEXT("I"), TEXT("II"), TEXT("III")};
		for (int32 Act = 0; Act < Soulbound::Warden::ActCount; ++Act)
		{
			const FWardenActReport& Report = WardenActs[Act];
			const double Target = Params.WardenParams.Acts.IsValidIndex(Act) ? Params.WardenParams.Acts[Act].CutTarget : 0.0;
			Lines.Add(FString::Printf(TEXT("    Warden, Acto %s: %.2f intervenciones con efecto por partida | %.1f %% de turnos cortados (objetivo %.0f %%) | hueco máximo sin golpe ni escena: %.0f s"),
				ActNames[Act], static_cast<double>(Report.EffectInterventions) / Games, Report.TurnsStarted > 0 ? 100.0 * Report.TurnsCut / Report.TurnsStarted : 0.0,
				100.0 * Target, Report.MaxGapSeconds));
		}
		Lines.Add(FString::Printf(TEXT("    Furias del Warden por partida, de media: %.2f"), static_cast<double>(Furies) / Games));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationRunesTest,
	"Soulbound.Core.Simulation.RunasConDummiesDeCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationRunesTest::RunTest(const FString& Parameters)
{
	// Criterio "Simulación" (S-008): dados cuatro dummies de carrera sobre el tablero de 35 casillas, acabada la carrera
	// obtienen runas y juegan minijuegos; la crónica los cuenta; es determinista por semilla y baja del segundo; y no
	// se pierde ninguna runa: las de los Brazales más las de las reservas suman las de partida. Semillas fijas 1 a 20.
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 1, Params))
	{
		return false;
	}
	int32 Obtained = 0;
	int32 Trials = 0;
	bool bChronicleCountsRunes = true;
	bool bChronicleCountsTrials = true;
	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		Params.Seed = Seed;
		const FSimulationResult A = FMatchSimulation(Params, FourRaceDummies()).Run();
		const FSimulationResult B = FMatchSimulation(Params, FourRaceDummies()).Run();
		const FString Context = FString::Printf(TEXT("Semilla %d"), Seed);
		TestTrue(Context + TEXT(": mismo registro"), A.Log == B.Log);
		TestTrue(Context + TEXT(": mismo estado final"), Soulbound::Tests::SameState(A.FinalState, B.FinalState));
		TestTrue(FString::Printf(TEXT("%s: baja del segundo (%.4f s)"), *Context, A.WallSeconds), A.WallSeconds < 1.0);

		const FMatchState& State = A.FinalState;
		const int32 Gained = CountEvents(State, EMatchEventType::RuneObtained);
		const int32 Played = CountEvents(State, EMatchEventType::SkillMinigamePassed) + CountEvents(State, EMatchEventType::SkillMinigameFailed);
		Obtained += Gained;
		Trials += Played;
		bChronicleCountsRunes &= Gained == 0 || A.Log.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("runa")); });
		bChronicleCountsTrials &= Played == 0 || A.Log.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("minijuego")); });

		if (State.Board.Phase == EBoardPhase::Awakened)
		{
			int32 InBracers = 0;
			for (const FSeatState& Seat : State.Seats)
			{
				for (const int32 Count : Seat.Runes)
				{
					InBracers += Count;
				}
			}
			int32 InReserves = 0;
			int32 Sources = 0;
			for (const FTileState& Tile : State.Board.Tiles)
			{
				Sources += Tile.Type == ETileType::PowerSource ? 1 : 0;
				InReserves += Tile.RuneReserve;
			}
			TestEqual(Context + TEXT(": las runas de los Brazales más las reservas son las de partida"), InBracers + InReserves, Sources * State.Board.Params.RuneReservePerSource);
		}
	}
	TestTrue(TEXT("En 20 partidas se obtienen runas"), Obtained > 0);
	TestTrue(TEXT("En 20 partidas se juegan minijuegos"), Trials > 0);
	TestTrue(TEXT("La crónica cuenta las runas"), bChronicleCountsRunes);
	TestTrue(TEXT("La crónica cuenta los minijuegos"), bChronicleCountsTrials);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceDummySeeksSourcesTest,
	"Soulbound.Core.Simulation.DummiesBuscanFuentes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRaceDummySeeksSourcesTest::RunTest(const FString& Parameters)
{
	// Criterio "Simulación" (S-008): acabada la carrera, el dummy va hacia la Fuente con reserva más cercana donde pueda
	// obtener (Elegido: cualquiera; Eco: la de un Olvidado). Desde el Altar, con todas las casillas vecinas como opciones,
	// elige una que no deja más lejos de la Fuente válida más cercana que ninguna otra. Semillas fijas 1 a 10.
	FRandomStream Start(1);
	FMatchState State;
	{
		FBoardState Board;
		FMatchParams MatchParams;
		FElectsTable Elects;
		if (!Soulbound::Tests::LoadBoardFile(*this, Soulbound::Tests::RaceBoardFileName(), Board) || !Soulbound::Tests::LoadTestMatchParams(*this, MatchParams)
			|| !Soulbound::Tests::LoadTestElects(*this, Elects))
		{
			return false;
		}
		FMatchStartResult Result = Soulbound::Rules::StartMatch(Board, MatchParams, Elects, Soulbound::Match::MaxPlayers, Start);
		if (!TestTrue(TEXT("La partida empieza"), Result.IsSuccess()))
		{
			return false;
		}
		State = MoveTemp(Result.State);
	}
	State.MatchPhase = EMatchPhase::RuneHunt;
	State.Board.Phase = EBoardPhase::Awakened;
	for (FTileState& Tile : State.Board.Tiles)
	{
		Tile.RuneReserve = Tile.Type == ETileType::PowerSource ? State.Board.Params.RuneReservePerSource : 0;
	}
	State.Seats[0].Role = ESeatRole::Elect;
	State.Seats[0].DomainId = 0;
	// Asiento 1: Eco con hogar en un Dominio Olvidado.
	State.Seats[1].Role = ESeatRole::LostEcho;
	for (const FDomainState& Domain : State.Board.Domains)
	{
		if (Domain.State == EDomainState::Forgotten)
		{
			State.Seats[1].DomainId = Domain.DomainId;
			break;
		}
	}

	const int32 Altar = State.Board.FindAltar();
	for (const int32 Seat : {0, 1})
	{
		// Pasos desde cada casilla hasta la Fuente válida más cercana, por búsqueda en anchura (oráculo del test).
		TArray<int32> Steps;
		Steps.Init(MAX_int32, State.Board.Tiles.Num());
		TArray<int32> Queue;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			const bool bValid = Tile.Type == ETileType::PowerSource && Tile.RuneReserve > 0
				&& (Seat == 0 || State.Board.Domains[Tile.DomainId].State == EDomainState::Forgotten);
			if (bValid)
			{
				Steps[Tile.TileId] = 0;
				Queue.Add(Tile.TileId);
			}
		}
		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			for (const int32 Next : State.Board.Tiles[Queue[i]].Neighbours)
			{
				if (Steps[Next] == MAX_int32)
				{
					Steps[Next] = Steps[Queue[i]] + 1;
					Queue.Add(Next);
				}
			}
		}
		const TArray<int32>& Options = State.Board.Tiles[Altar].Neighbours;
		int32 Best = MAX_int32;
		for (const int32 Option : Options)
		{
			Best = FMath::Min(Best, Steps[Option]);
		}
		for (int32 Seed = 1; Seed <= 10; ++Seed)
		{
			FRandomStream Rng(Seed);
			const int32 Choice = FRaceDummyDecider().ChooseDestination(State, Seat, Options, Rng);
			TestTrue(FString::Printf(TEXT("Asiento %d, semilla %d: elige una de las opciones"), Seat, Seed), Options.Contains(Choice));
			TestEqual(FString::Printf(TEXT("Asiento %d, semilla %d: la que más acerca a la Fuente válida más cercana"), Seat, Seed), Steps.IsValidIndex(Choice) ? Steps[Choice] : MAX_int32, Best);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationChoicesTest,
	"Soulbound.Core.Simulation.EleccionesConDummies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationChoicesTest::RunTest(const FString& Parameters)
{
	// Criterio "Simulación" (S-009): los dummies eligen al azar entre las opciones de cada elección (incluida "no"), con su
	// generador. Determinista por semilla y bajo el segundo. Y la crónica cuenta cada elección: una línea por evento.

	// El dummy, con su generador: todas las opciones y "no" salen; "no" nunca en la elección de dados.
	FPendingChoice Jump;
	Jump.Kind = EChoiceKind::PlanarJump;
	Jump.Options = {5, 7};
	FPendingChoice Dice;
	Dice.Kind = EChoiceKind::DiceCount;
	Dice.Options = {1, 2};
	TSet<int32> JumpAnswers;
	TSet<int32> DiceAnswers;
	FMatchState Empty;
	for (int32 Seed = 1; Seed <= 100; ++Seed)
	{
		FRandomStream Rng(Seed);
		JumpAnswers.Add(FDummyDecider().ChooseOption(Empty, 0, Jump, Rng));
		DiceAnswers.Add(FRaceDummyDecider().ChooseOption(Empty, 0, Dice, Rng));
	}
	TestTrue(TEXT("El dummy elige cada opción y \"no\" en una elección que lo admite"), JumpAnswers.Num() == 3 && JumpAnswers.Contains(5) && JumpAnswers.Contains(7) && JumpAnswers.Contains(INDEX_NONE));
	TestTrue(TEXT("En la elección de dados, uno o dos y nunca \"no\""), DiceAnswers.Num() == 2 && DiceAnswers.Contains(1) && DiceAnswers.Contains(2));

	// Partidas enteras con dummies de carrera sobre el tablero de 35 casillas (dos Nodos Planares desde S-009), semillas 1 a 20.
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 1, Params))
	{
		return false;
	}
	TSet<int32> DiceChosen;
	int32 Rerolls = 0;
	int32 Jumps = 0;
	int32 Expulsions = 0;
	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		Params.Seed = Seed;
		const FSimulationResult A = FMatchSimulation(Params, FourRaceDummies()).Run();
		const FSimulationResult B = FMatchSimulation(Params, FourRaceDummies()).Run();
		const FString Context = FString::Printf(TEXT("Semilla %d"), Seed);
		TestTrue(Context + TEXT(": mismo registro"), A.Log == B.Log);
		TestTrue(Context + TEXT(": mismo estado final"), Soulbound::Tests::SameState(A.FinalState, B.FinalState));
		TestTrue(FString::Printf(TEXT("%s: baja del segundo (%.4f s)"), *Context, A.WallSeconds), A.WallSeconds < 1.0);

		const FMatchState& State = A.FinalState;
		for (const FMatchEvent& Event : Soulbound::Tests::EventsOfType(State, EMatchEventType::DiceChosen))
		{
			DiceChosen.Add(Event.Value);
		}
		Rerolls += CountEvents(State, EMatchEventType::Rerolled);
		Jumps += CountEvents(State, EMatchEventType::PlanarJumped);
		Expulsions += CountEvents(State, EMatchEventType::Expelled);

		auto CountLines = [&A](const TCHAR* Fragment)
		{
			return A.Log.FilterByPredicate([Fragment](const FString& Line) { return Line.Contains(Fragment, ESearchCase::CaseSensitive); }).Num();
		};
		TestEqual(Context + TEXT(": cada elección de dados tiene su línea"), CountLines(TEXT(" dados para tirar")), CountEvents(State, EMatchEventType::DiceChosen));
		TestEqual(Context + TEXT(": cada repetición tiene su línea"), CountLines(TEXT(" repite la tirada")), CountEvents(State, EMatchEventType::Rerolled));
		TestEqual(Context + TEXT(": cada salto tiene su línea"), CountLines(TEXT(" salta a la casilla ")), CountEvents(State, EMatchEventType::PlanarJumped));
		TestEqual(Context + TEXT(": cada expulsión tiene su línea"), CountLines(TEXT(" a su hogar de una expulsión")), CountEvents(State, EMatchEventType::Expelled));
		TestEqual(Context + TEXT(": los dummies contestan al instante, ninguna elección se agota"), CountEvents(State, EMatchEventType::ChoiceExpired), 0);
	}
	TestTrue(TEXT("En 20 partidas se eligen uno y dos dados"), DiceChosen.Contains(1) && DiceChosen.Contains(2));
	AddInfo(FString::Printf(TEXT("20 partidas: %d repeticiones, %d saltos planares, %d expulsiones"), Rerolls, Jumps, Expulsions));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationTheftsTest,
	"Soulbound.Core.Simulation.RobosConDummies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationTheftsTest::RunTest(const FString& Parameters)
{
	// Criterio "Simulación" (S-010): los dummies roban y eligen esfera al azar con su generador (y Koschei aparta al azar).
	// Determinista por semilla y bajo el segundo. Cada robo tiene su línea en la crónica y ninguna runa se crea ni se pierde.

	// El dummy, con su generador: roba a cualquiera de los objetivos o a ninguno; elige cualquier esfera, nunca "no".
	FPendingChoice Action;
	Action.Kind = EChoiceKind::PlayerAction;
	Action.ActionOptions = {{EPlayerAction::Steal, 1}, {EPlayerAction::Steal, 3}};
	FPendingChoice Pick;
	Pick.Kind = EChoiceKind::ShellPick;
	Pick.Options = {0, 1, 2};
	FPendingChoice Safe;
	Safe.Kind = EChoiceKind::SafeRune;
	Safe.Options = {2, 4};
	TSet<int32> ActionAnswers;
	TSet<int32> PickAnswers;
	TSet<int32> SafeAnswers;
	FMatchState Empty;
	for (int32 Seed = 1; Seed <= 100; ++Seed)
	{
		FRandomStream Rng(Seed);
		ActionAnswers.Add(FDummyDecider().ChooseOption(Empty, 0, Action, Rng));
		PickAnswers.Add(FRaceDummyDecider().ChooseOption(Empty, 0, Pick, Rng));
		SafeAnswers.Add(FDummyDecider().ChooseOption(Empty, 0, Safe, Rng));
	}
	TestTrue(TEXT("El dummy roba a cada objetivo (por su índice) o a ninguno"), ActionAnswers.Num() == 3 && ActionAnswers.Contains(0) && ActionAnswers.Contains(1) && ActionAnswers.Contains(INDEX_NONE));
	TestTrue(TEXT("Elige cualquier esfera y nunca \"no\""), PickAnswers.Num() == 3 && !PickAnswers.Contains(INDEX_NONE));
	TestTrue(TEXT("Koschei aparta cualquiera de sus runas y nunca \"no\""), SafeAnswers.Num() == 2 && !SafeAnswers.Contains(INDEX_NONE));

	// Partidas enteras sobre el tablero de 35 casillas: dos dummies de carrera y dos dummies al azar, que casi nunca se
	// vinculan y quedan Ecos (con cuatro dummies de carrera los Ecos son muy pocos, S-008). Semillas 1 a 20.
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 1, Params))
	{
		return false;
	}
	auto MixedDeciders = []()
	{
		TArray<TSharedPtr<IDecider>> Deciders = {MakeShared<FRaceDummyDecider>(), MakeShared<FRaceDummyDecider>(), MakeShared<FDummyDecider>(), MakeShared<FDummyDecider>()};
		return Deciders;
	};
	int32 Thefts = 0;
	int32 Stolen = 0;
	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		Params.Seed = Seed;
		const FSimulationResult A = FMatchSimulation(Params, MixedDeciders()).Run();
		const FSimulationResult B = FMatchSimulation(Params, MixedDeciders()).Run();
		const FString Context = FString::Printf(TEXT("Semilla %d"), Seed);
		TestTrue(Context + TEXT(": mismo registro"), A.Log == B.Log);
		TestTrue(Context + TEXT(": mismo estado final"), Soulbound::Tests::SameState(A.FinalState, B.FinalState));
		TestTrue(FString::Printf(TEXT("%s: baja del segundo (%.4f s)"), *Context, A.WallSeconds), A.WallSeconds < 1.0);

		const FMatchState& State = A.FinalState;
		Thefts += CountEvents(State, EMatchEventType::ShellGameStarted) + CountEvents(State, EMatchEventType::StealNoRunes) + CountEvents(State, EMatchEventType::StealCancelled);
		Stolen += CountEvents(State, EMatchEventType::RuneStolen);

		// Ninguna runa se crea ni se pierde: las de los Brazales más las reservas son las que había al Despertar.
		if (State.Board.Phase == EBoardPhase::Awakened)
		{
			int32 InPlay = 0;
			int32 Sources = 0;
			for (const FSeatState& Seat : State.Seats)
			{
				for (const int32 Count : Seat.Runes)
				{
					InPlay += Count;
				}
			}
			for (const FTileState& Tile : State.Board.Tiles)
			{
				InPlay += Tile.RuneReserve;
				Sources += Tile.Type == ETileType::PowerSource ? 1 : 0;
			}
			TestEqual(Context + TEXT(": las runas en juego no cambian"), InPlay, Sources * State.Board.Params.RuneReservePerSource);
		}

		auto CountLines = [&A](const TCHAR* Fragment)
		{
			return A.Log.FilterByPredicate([Fragment](const FString& Line) { return Line.Contains(Fragment, ESearchCase::CaseSensitive); }).Num();
		};
		TestEqual(Context + TEXT(": cada trilero tiene su línea"), CountLines(TEXT(" abre el trilero contra ")), CountEvents(State, EMatchEventType::ShellGameStarted));
		TestEqual(Context + TEXT(": cada runa apartada tiene su línea"), CountLines(TEXT(" aparta la runa ")), CountEvents(State, EMatchEventType::SafeRuneSetAside));
		TestEqual(Context + TEXT(": cada runa robada tiene su línea"), CountLines(TEXT(" roba la runa ")), CountEvents(State, EMatchEventType::RuneStolen));
		TestEqual(Context + TEXT(": cada robo sin runas tiene su línea"), CountLines(TEXT(", que no tiene runas")), CountEvents(State, EMatchEventType::StealNoRunes));
		TestEqual(Context + TEXT(": cada robo anulado tiene su línea"), CountLines(TEXT("robo anulado")), CountEvents(State, EMatchEventType::StealCancelled));
		TestEqual(Context + TEXT(": los dummies eligen esfera al instante, ninguna se agota"), CountEvents(State, EMatchEventType::ShellGameExpired), 0);
		TestEqual(Context + TEXT(": ninguna elección se agota"), CountEvents(State, EMatchEventType::ChoiceExpired), 0);
	}
	TestTrue(TEXT("En 20 partidas, los dummies roban runas"), Stolen > 0);
	AddInfo(FString::Printf(TEXT("20 partidas: %d robos, %d runas cambian de mano"), Thefts, Stolen));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationDuelsTest,
	"Soulbound.Core.Simulation.DuelosConDummies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationDuelsTest::RunTest(const FString& Parameters)
{
	// Criterio "Simulación" (S-012): los dummies retan al azar entre sus opciones y rinden al azar con su generador.
	// Determinista por semilla y bajo el segundo. Cada reto acaba en duelo resuelto o sin runas, cada evento de duelo tiene
	// su línea en la crónica y ninguna runa se crea ni se pierde.

	// El dummy, con su generador: reta a cualquiera de los objetivos (por índice) o a ninguno, y rinde de 0 a 1, variado.
	FPendingChoice Action;
	Action.Kind = EChoiceKind::PlayerAction;
	Action.ActionOptions = {{EPlayerAction::Duel, 1}, {EPlayerAction::Duel, 2}, {EPlayerAction::Expel, 1}};
	TSet<int32> ActionAnswers;
	TSet<int32> Tenths;
	bool bInRange = true;
	FMatchState Empty;
	for (int32 Seed = 1; Seed <= 100; ++Seed)
	{
		FRandomStream Rng(Seed);
		ActionAnswers.Add(FDummyDecider().ChooseOption(Empty, 0, Action, Rng));
		const double Performance = FRaceDummyDecider().DuelPerformance(Empty, 0, Rng);
		bInRange &= Performance >= 0.0 && Performance <= 1.0;
		Tenths.Add(FMath::FloorToInt32(Performance * 10.0));
	}
	TestTrue(TEXT("El dummy reta a cada objetivo, expulsa o no hace nada"), ActionAnswers.Num() == 4 && ActionAnswers.Contains(0) && ActionAnswers.Contains(1) && ActionAnswers.Contains(INDEX_NONE));
	TestTrue(TEXT("Rinde entre 0 y 1"), bInRange);
	TestTrue(TEXT("... al azar: en 100 sorteos, al menos cinco décimas distintas"), Tenths.Num() >= 5);
	FRandomStream SameA(7);
	FRandomStream SameB(7);
	TestEqual(TEXT("Con el mismo generador, el mismo rendimiento"), FDummyDecider().DuelPerformance(Empty, 0, SameA), FDummyDecider().DuelPerformance(Empty, 0, SameB));

	// Partidas enteras sobre el tablero de 35 casillas: dos dummies de carrera y dos al azar, como en RobosConDummies (S-010).
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 1, Params))
	{
		return false;
	}
	auto MixedDeciders = []()
	{
		TArray<TSharedPtr<IDecider>> Deciders = {MakeShared<FRaceDummyDecider>(), MakeShared<FRaceDummyDecider>(), MakeShared<FDummyDecider>(), MakeShared<FDummyDecider>()};
		return Deciders;
	};
	int32 Duels = 0;
	int32 Won = 0;
	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		Params.Seed = Seed;
		const FSimulationResult A = FMatchSimulation(Params, MixedDeciders()).Run();
		const FSimulationResult B = FMatchSimulation(Params, MixedDeciders()).Run();
		const FString Context = FString::Printf(TEXT("Semilla %d"), Seed);
		TestTrue(Context + TEXT(": mismo registro"), A.Log == B.Log);
		TestTrue(Context + TEXT(": mismo estado final"), Soulbound::Tests::SameState(A.FinalState, B.FinalState));
		TestTrue(FString::Printf(TEXT("%s: baja del segundo (%.4f s)"), *Context, A.WallSeconds), A.WallSeconds < 1.0);

		const FMatchState& State = A.FinalState;
		const int32 Challenged = CountEvents(State, EMatchEventType::DuelChallenged);
		const int32 Resolved = CountEvents(State, EMatchEventType::DuelWon);
		Duels += Challenged + CountEvents(State, EMatchEventType::DuelNoRunes);
		Won += Resolved;
		// Un duelo en curso al acabar el tiempo de la simulación es el único que puede quedar sin resolver.
		TestTrue(Context + TEXT(": cada duelo se resuelve"), Resolved == Challenged || (Resolved == Challenged - 1 && State.Duel.IsActive()));

		if (State.Board.Phase == EBoardPhase::Awakened)
		{
			int32 InPlay = 0;
			int32 Sources = 0;
			for (const FSeatState& Seat : State.Seats)
			{
				for (const int32 Count : Seat.Runes)
				{
					InPlay += Count;
				}
			}
			for (const FTileState& Tile : State.Board.Tiles)
			{
				InPlay += Tile.RuneReserve;
				Sources += Tile.Type == ETileType::PowerSource ? 1 : 0;
			}
			TestEqual(Context + TEXT(": las runas en juego no cambian"), InPlay, Sources * State.Board.Params.RuneReservePerSource);
		}

		auto CountLines = [&A](const TCHAR* Fragment)
		{
			return A.Log.FilterByPredicate([Fragment](const FString& Line) { return Line.Contains(Fragment, ESearchCase::CaseSensitive); }).Num();
		};
		TestEqual(Context + TEXT(": cada reto tiene su línea"), CountLines(TEXT(" a duelo")), Challenged);
		TestEqual(Context + TEXT(": cada reto sin runas tiene su línea"), CountLines(TEXT(" sin runas: no hay duelo")), CountEvents(State, EMatchEventType::DuelNoRunes));
		TestEqual(Context + TEXT(": cada duelo resuelto tiene su línea"), CountLines(TEXT(" gana el duelo ")), Resolved);
		TestEqual(Context + TEXT(": cada perdedor enviado a su hogar tiene su línea"), CountLines(TEXT(" pierde el duelo sin runas")), CountEvents(State, EMatchEventType::DuelLoserSentHome));
		TestEqual(Context + TEXT(": ninguna elección se agota"), CountEvents(State, EMatchEventType::ChoiceExpired), 0);
	}
	TestTrue(TEXT("En 20 partidas, los dummies se baten en duelo"), Won > 0);
	AddInfo(FString::Printf(TEXT("20 partidas: %d retos, %d duelos resueltos"), Duels, Won));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationWardenGuaranteeTest,
	"Soulbound.Core.Simulation.WardenGarantiaEIntervalos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationWardenGuaranteeTest::RunTest(const FString& Parameters)
{
	// CA-WAR-2 (R-WAR-3, R-WAR-5; simulación): en 50 partidas con los valores iniciales (tablero de 35 casillas, dummies de
	// carrera, semillas 1 a 50), ningún tramo de 120 s sin golpe ni escena (desde el inicio y hasta el final, también), y todos
	// los intervalos entre evaluaciones dentro de 15 ± 5 s. Decisiones del autor del 19/10/2026: las re-evaluaciones tras un
	// aplazamiento no cuentan para el intervalo (el par que empieza en una aplazada no se mide), y R-WAR-7 manda sobre R-WAR-5:
	// si la evaluación que cierra el tramo vino de un aplazamiento, el tramo puede pasar de 120 s solo por el tiempo que estuvo
	// aplazada (desde la evaluación aplazada hasta la que intervino).
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 1, Params))
	{
		return false;
	}
	const double Guarantee = Params.WardenParams.GuaranteeSeconds;
	const double MinInterval = Params.WardenParams.EvaluationIntervalSeconds - Params.WardenParams.EvaluationVariationSeconds;
	const double MaxInterval = Params.WardenParams.EvaluationIntervalSeconds + Params.WardenParams.EvaluationVariationSeconds;
	double WorstGap = 0.0;
	int32 Intervals = 0;
	int32 Evaluations = 0;
	for (int32 Seed = 1; Seed <= 50; ++Seed)
	{
		Params.Seed = Seed;
		const FSimulationResult Result = FMatchSimulation(Params, FourRaceDummies()).Run();
		const FMatchState& State = Result.FinalState;
		const double End = State.IsMatchOver() ? State.ClockSeconds : Params.DurationSeconds;

		double LastEffect = 0.0;
		double LastEvaluation = -1.0;
		bool bLastDeferred = false;
		// Tiempo que estuvo aplazada la última evaluación, si vino de un aplazamiento; 0 si no.
		double DeferredFor = 0.0;
		for (const FMatchEvent& Event : State.EventLog)
		{
			if (Event.Type == EMatchEventType::WardenEventStarted)
			{
				WorstGap = FMath::Max(WorstGap, Event.ClockSeconds - LastEffect);
				const double Allowed = Guarantee + (Event.ClockSeconds == LastEvaluation ? DeferredFor : 0.0);
				if (Event.ClockSeconds - LastEffect > Allowed + KINDA_SMALL_NUMBER)
				{
					AddError(FString::Printf(TEXT("Semilla %d: %.1f s sin golpe ni escena hasta el %.1f"), Seed, Event.ClockSeconds - LastEffect, Event.ClockSeconds));
				}
				LastEffect = Event.ClockSeconds;
			}
			else if (Event.Type == EMatchEventType::WardenDecided)
			{
				++Evaluations;
				if (LastEvaluation >= 0.0 && !bLastDeferred)
				{
					++Intervals;
					const double Interval = Event.ClockSeconds - LastEvaluation;
					if (Interval < MinInterval - KINDA_SMALL_NUMBER || Interval > MaxInterval + KINDA_SMALL_NUMBER)
					{
						AddError(FString::Printf(TEXT("Semilla %d: intervalo de %.2f s entre evaluaciones en el %.1f"), Seed, Interval, Event.ClockSeconds));
					}
				}
				DeferredFor = bLastDeferred ? Event.ClockSeconds - LastEvaluation : 0.0;
				LastEvaluation = Event.ClockSeconds;
				bLastDeferred = Event.Detail == static_cast<int32>(EWardenDecisionKind::Deferred);
			}
		}
		WorstGap = FMath::Max(WorstGap, End - LastEffect);
		if (End - LastEffect > Guarantee)
		{
			AddError(FString::Printf(TEXT("Semilla %d: %.1f s sin golpe ni escena hasta el final"), Seed, End - LastEffect));
		}
	}
	TestTrue(TEXT("Se mide algo"), Intervals > 0);
	const FString Summary = FString::Printf(TEXT("50 partidas: %d evaluaciones, %d intervalos medidos, hueco máximo real sin golpe ni escena %.1f s"), Evaluations, Intervals, WorstGap);
	AddInfo(Summary);
	UE_LOG(LogSoulboundTest, Display, TEXT("%s"), *Summary);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationWardenLogTest,
	"Soulbound.Core.Simulation.RegistroDelWardenDeterminista",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationWardenLogTest::RunTest(const FString& Parameters)
{
	// Criterio "Registro y determinismo" (R-WAR-10): dos simulaciones con la misma semilla dan el mismo registro del Warden
	// (evaluaciones, decisiones, cortes y eventos, con sus marcas) y el mismo estado del Warden al final. Con el Warden
	// encendido, que corta turnos y lanza eventos de los sacos. Semilla fija 42, tablero de 35 casillas.
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 42, Params))
	{
		return false;
	}
	const FSimulationResult A = FMatchSimulation(Params, FourRaceDummies()).Run();
	const FSimulationResult B = FMatchSimulation(Params, FourRaceDummies()).Run();
	auto WardenLog = [](const FSimulationResult& Result)
	{
		return Result.FinalState.EventLog.FilterByPredicate([](const FMatchEvent& Event)
		{
			return Event.Type == EMatchEventType::WardenEvaluated || Event.Type == EMatchEventType::WardenDecided || Event.Type == EMatchEventType::WardenRemark
				|| Event.Type == EMatchEventType::TurnInterrupted || Event.Type == EMatchEventType::WardenEventStarted;
		});
	};
	auto SameEvents = [](const TArray<FMatchEvent>& X, const TArray<FMatchEvent>& Y)
	{
		if (X.Num() != Y.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < X.Num(); ++Index)
		{
			if (X[Index].Type != Y[Index].Type || X[Index].ClockSeconds != Y[Index].ClockSeconds || X[Index].Seat != Y[Index].Seat
				|| X[Index].Value != Y[Index].Value || X[Index].Detail != Y[Index].Detail)
			{
				return false;
			}
		}
		return true;
	};
	const TArray<FMatchEvent> LogA = WardenLog(A);
	TestTrue(TEXT("Misma semilla, mismo registro del Warden"), SameEvents(LogA, WardenLog(B)));
	TestTrue(TEXT("Y mismo estado del Warden"), FWardenState::StaticStruct()->CompareScriptStruct(&A.FinalState.Warden, &B.FinalState.Warden, PPF_None));
	TestTrue(TEXT("Mismo registro de texto"), A.Log == B.Log);
	TestTrue(TEXT("El Warden evalúa"), A.Evaluations > 0);
	TestTrue(TEXT("Interviene con efecto"), A.Interventions > 0);
	TestTrue(TEXT("Y corta algún turno"), CountEvents(A.FinalState, EMatchEventType::TurnInterrupted) > 0);

	FSimulationParams Other = Params;
	Other.Seed = 43;
	TestFalse(TEXT("Otra semilla, otro registro del Warden"), SameEvents(LogA, WardenLog(FMatchSimulation(Other, FourRaceDummies()).Run())));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationNoInsolenceTest,
	"Soulbound.Core.Simulation.DummiesNuncaInsolentes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationNoInsolenceTest::RunTest(const FString& Parameters)
{
	// CA-INS-3 (R-INS-1; simulación): en 50 partidas con dummies de carrera y el Warden real (tablero de 35, semillas 1 a 50),
	// insolencia 0 en todos los asientos al final, ni un castigo ni una Furia: los dummies contestan siempre dentro del plazo.
	FSimulationParams Params;
	if (!MakeBoardParams(*this, Soulbound::Tests::RaceBoardFileName(), 1, Params))
	{
		return false;
	}
	for (int32 Seed = 1; Seed <= 50; ++Seed)
	{
		Params.Seed = Seed;
		const FSimulationResult Result = FMatchSimulation(Params, FourRaceDummies()).Run();
		for (int32 Seat = 0; Seat < Result.FinalState.Seats.Num(); ++Seat)
		{
			if (Result.FinalState.Seats[Seat].Insolence != 0)
			{
				AddError(FString::Printf(TEXT("Semilla %d: el asiento %d acaba con insolencia %d"), Seed, Seat, Result.FinalState.Seats[Seat].Insolence));
			}
		}
		TestEqual(FString::Printf(TEXT("Semilla %d: ningún castigo de insolencia"), Seed), CountEvents(Result.FinalState, EMatchEventType::InsolencePunished), 0);
		TestEqual(FString::Printf(TEXT("Semilla %d: ninguna Furia"), Seed), Result.Furies, 0);
		TestTrue(FString::Printf(TEXT("Semilla %d: el Warden juega"), Seed), Result.Evaluations > 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
