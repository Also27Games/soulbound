// Tests de la máquina de turnos (S-003, GDD 02) sobre el tablero de pruebas y los parámetros de partida del archivo:
// CA-TUR-1, 2, 5, 7, 8, 9, 10, 14 y 15, y el caso límite de turnos extra y perdidos a la vez.
// Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Match.Turn".

#include "Decider/Decider.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Rules::AdvanceTurn;
	using Soulbound::Rules::SubmitDestinationIntent;

	/** Inicia la partida de pruebas y comprueba que tiene orden de turno, para que ningún test indexe uno vacío. */
	bool StartTestMatch(FAutomationTestBase& Test, FRandomStream& Random, FMatchState& OutState)
	{
		return Soulbound::Tests::StartTestMatch(Test, Random, OutState)
			&& Test.TestEqual(TEXT("La partida tiene orden de turno"), OutState.TurnOrder.Num(), Soulbound::Match::SeatCount);
	}

	/** Llamadas de más que se toleran antes de dar un turno por atascado: un turno completo son seis. */
	constexpr int32 MaxStepsPerTurn = 8;

	/** Avanza el turno en NowSeconds hasta que espera destino. Devuelve false si no llega a esperarlo. */
	bool AdvanceToChoice(FMatchState& State, FRandomStream& Random, double NowSeconds)
	{
		for (int32 Step = 0; Step < MaxStepsPerTurn && State.TurnPhase != ETurnPhase::RollAndChoose; ++Step)
		{
			if (!AdvanceTurn(State, NowSeconds, Random))
			{
				return false;
			}
		}
		return State.TurnPhase == ETurnPhase::RollAndChoose;
	}

	/** Avanza el turno en NowSeconds hasta que termina. Devuelve false si no llega a terminar. */
	bool AdvanceToBetweenTurns(FMatchState& State, FRandomStream& Random, double NowSeconds)
	{
		for (int32 Step = 0; Step < MaxStepsPerTurn && State.TurnPhase != ETurnPhase::BetweenTurns; ++Step)
		{
			if (!AdvanceTurn(State, NowSeconds, Random))
			{
				return false;
			}
		}
		return State.TurnPhase == ETurnPhase::BetweenTurns;
	}

	/** Un turno jugado: quién lo jugó y el límite de su reloj. */
	struct FPlayedTurn
	{
		int32 Seat = INDEX_NONE;
		double DeadlineSeconds = 0.0;
	};

	/**
	 * Juega un turno completo: empieza en StartAt y el asiento activo confirma en ConfirmAt la primera casilla
	 * alcanzable. Devuelve Seat INDEX_NONE si el turno no llega a esperar destino o no termina.
	 */
	FPlayedTurn PlayTurn(FMatchState& State, FRandomStream& Random, double StartAt, double ConfirmAt)
	{
		FPlayedTurn Played;
		if (!AdvanceToChoice(State, Random, StartAt) || State.ReachableOptions.IsEmpty())
		{
			return Played;
		}
		const int32 Seat = State.ActiveSeat;
		const double Deadline = State.TurnDeadlineSeconds;
		if (SubmitDestinationIntent(State, Seat, State.ReachableOptions[0], ConfirmAt) != EIntentResult::Accepted
			|| !AdvanceToBetweenTurns(State, Random, ConfirmAt))
		{
			return Played;
		}
		Played.Seat = Seat;
		Played.DeadlineSeconds = Deadline;
		return Played;
	}

	/** Asiento que sigue a Seat en el orden de turno. */
	int32 NextInOrder(const FMatchState& State, int32 Seat)
	{
		const int32 Index = State.TurnOrder.Find(Seat);
		return Index == INDEX_NONE ? INDEX_NONE : State.TurnOrder[(Index + 1) % State.TurnOrder.Num()];
	}

	int32 CountEvents(const FMatchState& State, EMatchEventType Type, int32 Seat)
	{
		return State.EventLog.FilterByPredicate([Type, Seat](const FMatchEvent& Event) { return Event.Type == Type && Event.Seat == Seat; }).Num();
	}

	FString Describe(const TArray<int32>& Values)
	{
		TArray<FString> Parts;
		for (const int32 Value : Values)
		{
			Parts.Add(FString::FromInt(Value));
		}
		return FString::Join(Parts, TEXT(","));
	}

	/** Comprueba un evento del registro: tipo, marca de tiempo y asiento. */
	void ExpectEvent(FAutomationTestBase& Test, const FString& What, const FMatchState& State, int32 Index, EMatchEventType Type, double ClockSeconds, int32 Seat)
	{
		if (!Test.TestTrue(What + TEXT(": el evento existe"), State.EventLog.IsValidIndex(Index)))
		{
			return;
		}
		const FMatchEvent& Event = State.EventLog[Index];
		Test.TestEqual(What + TEXT(": tipo"), static_cast<int32>(Event.Type), static_cast<int32>(Type));
		Test.TestEqual(What + TEXT(": marca de tiempo"), Event.ClockSeconds, ClockSeconds);
		Test.TestEqual(What + TEXT(": asiento"), Event.Seat, Seat);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnOrderTest,
	"Soulbound.Core.Match.Turn.OrdenSorteadoEsPermutacion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnOrderTest::RunTest(const FString& Parameters)
{
	// CA-TUR-1 (R-TUR-1): el orden es una permutación de los cuatro asientos, la misma semilla da el mismo orden,
	// en 50 semillas salen al menos dos órdenes distintos y el orden se mantiene toda la partida.
	constexpr int32 SeedCount = 50;
	TArray<int32> AllSeats;
	for (int32 Seat = 0; Seat < Soulbound::Match::SeatCount; ++Seat)
	{
		AllSeats.Add(Seat);
	}

	TSet<FString> Distinct;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		FRandomStream Random(Seed);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		TArray<int32> Sorted = State.TurnOrder;
		Sorted.Sort();
		TestEqual(FString::Printf(TEXT("Semilla %d: el orden es una permutación de los cuatro asientos"), Seed), Describe(Sorted), Describe(AllSeats));
		TestTrue(FString::Printf(TEXT("Semilla %d: empieza el primero del orden"), Seed), State.TurnOrder.Num() > 0 && State.ActiveSeat == State.TurnOrder[0]);
		Distinct.Add(Describe(State.TurnOrder));
	}
	TestTrue(FString::Printf(TEXT("Al menos dos órdenes distintos en %d semillas (salen %d)"), SeedCount, Distinct.Num()), Distinct.Num() >= 2);

	FRandomStream RandomA(1234), RandomB(1234);
	FMatchState A, B;
	if (!StartTestMatch(*this, RandomA, A) || !StartTestMatch(*this, RandomB, B))
	{
		return false;
	}
	TestEqual(TEXT("Misma semilla, mismo orden"), Describe(B.TurnOrder), Describe(A.TurnOrder));

	// Se mantiene toda la partida: dos rondas en el orden sorteado, y el orden no cambia aunque haya turnos
	// extra y perdidos de por medio.
	const TArray<int32> Order = A.TurnOrder;
	double Now = 0.0;
	for (int32 Turn = 0; Turn < 2 * Soulbound::Match::SeatCount; ++Turn, Now += 10.0)
	{
		const FPlayedTurn Played = PlayTurn(A, RandomA, Now, Now + 5.0);
		TestEqual(FString::Printf(TEXT("Turno %d: juega el asiento del orden"), Turn), Played.Seat, Order[Turn % Order.Num()]);
	}
	Soulbound::Rules::GrantExtraTurns(A, Order[1], 1);
	Soulbound::Rules::GrantLostTurns(A, Order[2], 1);
	for (int32 Turn = 0; Turn < 2 * Soulbound::Match::SeatCount; ++Turn, Now += 10.0)
	{
		PlayTurn(A, RandomA, Now, Now + 5.0);
	}
	TestEqual(TEXT("El orden no cambia en toda la partida"), Describe(A.TurnOrder), Describe(Order));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnPhasesTest,
	"Soulbound.Core.Match.Turn.CincoFasesEnOrden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnPhasesTest::RunTest(const FString& Parameters)
{
	// CA-TUR-2 (R-TUR-2): un turno sin interrupciones pasa por las cinco fases en orden y ninguna se repite.
	FRandomStream Random(3);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	TestEqual(TEXT("Antes del primer turno no juega nadie"), static_cast<int32>(State.TurnPhase), static_cast<int32>(ETurnPhase::BetweenTurns));

	TArray<int32> Phases;
	const double Now = 0.0;
	for (int32 Step = 0; Step < 3; ++Step)
	{
		AdvanceTurn(State, Now, Random);
		Phases.Add(static_cast<int32>(State.TurnPhase));
	}

	// Esperando destino, avanzar no cambia de fase.
	TestFalse(TEXT("Con la tirada hecha, el turno espera la intención"), AdvanceTurn(State, Now + 1.0, Random));
	TestEqual(TEXT("Y sigue en tirada y elección"), static_cast<int32>(State.TurnPhase), static_cast<int32>(ETurnPhase::RollAndChoose));

	if (TestFalse(TEXT("Desde el Altar hay casillas alcanzables"), State.ReachableOptions.IsEmpty()))
	{
		SubmitDestinationIntent(State, State.ActiveSeat, State.ReachableOptions[0], Now + 2.0);
	}
	Phases.Add(static_cast<int32>(State.TurnPhase));
	AdvanceTurn(State, Now + 2.0, Random);
	Phases.Add(static_cast<int32>(State.TurnPhase));

	const TArray<int32> Expected = {
		static_cast<int32>(ETurnPhase::Start),
		static_cast<int32>(ETurnPhase::PreRoll),
		static_cast<int32>(ETurnPhase::RollAndChoose),
		static_cast<int32>(ETurnPhase::MoveAndResolve),
		static_cast<int32>(ETurnPhase::End),
	};
	TestEqual(TEXT("Las cinco fases, en orden y sin repetir"), Describe(Phases), Describe(Expected));

	AdvanceTurn(State, Now + 2.0, Random);
	TestEqual(TEXT("Tras el fin, entre turnos"), static_cast<int32>(State.TurnPhase), static_cast<int32>(ETurnPhase::BetweenTurns));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnRollTest,
	"Soulbound.Core.Match.Turn.TiradaDeUnDadoDeSeis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnRollTest::RunTest(const FString& Parameters)
{
	// CA-TUR-5 (R-TUR-5), parte sin Celeridad: la tirada es un entero de 1 a 6 sacado del FRandomStream de la
	// partida; misma semilla, misma tirada. Semilla fija 77.
	FRandomStream DiceA(77), DiceB(77);
	TSet<int32> Seen;
	for (int32 Throw = 0; Throw < 600; ++Throw)
	{
		const int32 A = Soulbound::Rules::RollDice(DiceA, Soulbound::Dice::DefaultCount);
		const int32 B = Soulbound::Rules::RollDice(DiceB, Soulbound::Dice::DefaultCount);
		if (A < 1 || A > Soulbound::Dice::Faces || A != B)
		{
			AddError(FString::Printf(TEXT("Tirada %d: %d y %d con la misma semilla; debe ser la misma y de 1 a %d"), Throw, A, B, Soulbound::Dice::Faces));
			break;
		}
		Seen.Add(A);
	}
	TestEqual(TEXT("En 600 tiradas salen las seis caras"), Seen.Num(), Soulbound::Dice::Faces);

	FRandomStream Untouched(5), Probe(5);
	TestEqual(TEXT("Sin dados, la tirada es 0"), Soulbound::Rules::RollDice(Probe, 0), 0);
	TestEqual(TEXT("Sin dados, no se consume el sorteo"), Probe.GetCurrentSeed(), Untouched.GetCurrentSeed());

	// En la partida: la tirada del turno sale del sorteo de la partida, justo donde lo dejó el inicio.
	FRandomStream Random(9);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	FRandomStream Replay = Random;
	const int32 Expected = Soulbound::Rules::RollDice(Replay, Soulbound::Dice::DefaultCount);
	if (TestTrue(TEXT("El turno llega a la tirada"), AdvanceToChoice(State, Random, 0.0)))
	{
		TestEqual(TEXT("La tirada del turno es la del sorteo de la partida"), State.Roll, Expected);
		const FMatchEvent* Rolled = State.EventLog.FindByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::Rolled; });
		TestTrue(TEXT("El registro guarda la tirada"), Rolled != nullptr && Rolled->Value == Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnIntentTest,
	"Soulbound.Core.Match.Turn.IntencionNoAlcanzableSeRechaza",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnIntentTest::RunTest(const FString& Parameters)
{
	// CA-TUR-7 (R-TUR-7): el destino confirmado es una casilla alcanzable; una intención no válida (casilla no
	// alcanzable, otro asiento, otra fase, fuera de tiempo) se rechaza sin cambiar el estado ni la fase.
	FRandomStream Random(13);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const double Now = 0.0;

	// Otra fase: antes de tirar no se acepta destino.
	AdvanceTurn(State, Now, Random);
	AdvanceTurn(State, Now, Random);
	TestEqual(TEXT("Decisiones previas a la tirada"), static_cast<int32>(State.TurnPhase), static_cast<int32>(ETurnPhase::PreRoll));
	FMatchState Before = State;
	TestEqual(TEXT("Antes de tirar: fase equivocada"), static_cast<int32>(SubmitDestinationIntent(State, State.ActiveSeat, 1, Now)), static_cast<int32>(EIntentResult::WrongPhase));
	TestTrue(TEXT("Antes de tirar: el estado no cambia"), Soulbound::Tests::SameState(Before, State));

	if (!TestTrue(TEXT("El turno llega a la tirada"), AdvanceToChoice(State, Random, Now)) || !TestFalse(TEXT("Hay casillas alcanzables"), State.ReachableOptions.IsEmpty()))
	{
		return false;
	}
	const int32 Seat = State.ActiveSeat;
	const int32 Valid = State.ReachableOptions[0];
	int32 Unreachable = INDEX_NONE;
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (!State.ReachableOptions.Contains(Tile.TileId) && Tile.TileId != State.Seats[Seat].TileId)
		{
			Unreachable = Tile.TileId;
			break;
		}
	}
	Before = State;

	struct FBadIntent
	{
		const TCHAR* What;
		int32 Seat;
		int32 TileId;
		double At;
		EIntentResult Expected;
	};
	const FBadIntent BadIntents[] = {
		{TEXT("Casilla no alcanzable"), Seat, Unreachable, Now, EIntentResult::Unreachable},
		{TEXT("Casilla inexistente"), Seat, State.Board.Tiles.Num(), Now, EIntentResult::Unreachable},
		{TEXT("Sin casilla"), Seat, INDEX_NONE, Now, EIntentResult::Unreachable},
		{TEXT("Casilla de origen"), Seat, State.Seats[Seat].TileId, Now, EIntentResult::Unreachable},
		{TEXT("Otro asiento"), NextInOrder(State, Seat), Valid, Now, EIntentResult::WrongSeat},
		{TEXT("En el límite del reloj"), Seat, Valid, State.TurnDeadlineSeconds, EIntentResult::ClockExpired},
	};
	for (const FBadIntent& Bad : BadIntents)
	{
		TestEqual(FString::Printf(TEXT("%s: se rechaza por su motivo"), Bad.What), static_cast<int32>(SubmitDestinationIntent(State, Bad.Seat, Bad.TileId, Bad.At)), static_cast<int32>(Bad.Expected));
		TestTrue(FString::Printf(TEXT("%s: el estado y la fase no cambian"), Bad.What), Soulbound::Tests::SameState(Before, State));
	}

	TestEqual(TEXT("Casilla alcanzable: se acepta"), static_cast<int32>(SubmitDestinationIntent(State, Seat, Valid, Now + 1.0)), static_cast<int32>(EIntentResult::Accepted));
	TestEqual(TEXT("La ficha está en el destino elegido"), State.Seats[Seat].TileId, Valid);
	TestEqual(TEXT("El turno pasa a movimiento y resolución"), static_cast<int32>(State.TurnPhase), static_cast<int32>(ETurnPhase::MoveAndResolve));
	TestEqual(TEXT("Una segunda intención ya no vale"), static_cast<int32>(SubmitDestinationIntent(State, Seat, Valid, Now + 1.0)), static_cast<int32>(EIntentResult::WrongPhase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnClockTest,
	"Soulbound.Core.Match.Turn.RelojAgotadoSubeInsolencia",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnClockTest::RunTest(const FString& Parameters)
{
	// CA-TUR-8 (R-TUR-8): si el motor recibe un tiempo igual o posterior al límite antes de confirmar destino, la
	// posición no cambia, el turno termina y la insolencia sube uno. Confirmado antes del límite, no cambia.
	const double TurnStart = 100.0;
	{
		FRandomStream Random(21);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State) || !TestTrue(TEXT("Agotado: el turno llega a la tirada"), AdvanceToChoice(State, Random, TurnStart)))
		{
			return false;
		}
		const int32 Seat = State.ActiveSeat;
		const int32 Tile = State.Seats[Seat].TileId;
		const int32 Insolence = State.Seats[Seat].Insolence;
		const double Deadline = State.TurnDeadlineSeconds;
		TestEqual(TEXT("El límite es el inicio más el reloj de turno de los datos"), Deadline, TurnStart + State.Params.TurnClockSeconds);

		TestFalse(TEXT("Justo antes del límite, el turno sigue esperando"), AdvanceTurn(State, Deadline - 0.01, Random));
		TestEqual(TEXT("Justo antes del límite, sigue en tirada y elección"), static_cast<int32>(State.TurnPhase), static_cast<int32>(ETurnPhase::RollAndChoose));

		TestTrue(TEXT("En el límite, el turno avanza"), AdvanceTurn(State, Deadline, Random));
		TestEqual(TEXT("Agotado: el turno termina"), static_cast<int32>(State.TurnPhase), static_cast<int32>(ETurnPhase::End));
		TestEqual(TEXT("Agotado: la posición no cambia"), State.Seats[Seat].TileId, Tile);
		TestEqual(TEXT("Agotado: la insolencia sube uno"), State.Seats[Seat].Insolence, Insolence + 1);
		TestEqual(TEXT("Agotado: queda en el registro"), CountEvents(State, EMatchEventType::TurnClockExpired, Seat), 1);
	}
	{
		FRandomStream Random(21);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State) || !TestTrue(TEXT("Confirmado: el turno llega a la tirada"), AdvanceToChoice(State, Random, TurnStart)))
		{
			return false;
		}
		const int32 Seat = State.ActiveSeat;
		const int32 Insolence = State.Seats[Seat].Insolence;
		const double Deadline = State.TurnDeadlineSeconds;
		TestEqual(TEXT("Confirmado antes del límite: se acepta"), static_cast<int32>(SubmitDestinationIntent(State, Seat, State.ReachableOptions.IsEmpty() ? INDEX_NONE : State.ReachableOptions[0], Deadline - 0.01)), static_cast<int32>(EIntentResult::Accepted));
		// El reloj cubre hasta confirmar destino: lo que tarde después la resolución no agota el turno.
		TestTrue(TEXT("Confirmado: el turno termina aunque ya sea después del límite"), AdvanceToBetweenTurns(State, Random, Deadline + 5.0));
		TestEqual(TEXT("Confirmado: la insolencia no cambia"), State.Seats[Seat].Insolence, Insolence);
		TestEqual(TEXT("Confirmado: no hay agotamiento en el registro"), CountEvents(State, EMatchEventType::TurnClockExpired, Seat), 0);
	}
	{
		// La insolencia es un contador de 0 a 5 (glosario): no pasa del tope.
		FRandomStream Random(22);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		for (int32 Raise = 0; Raise < Soulbound::Match::MaxInsolence + 2; ++Raise)
		{
			Soulbound::Rules::RaiseInsolence(State, 0, 1.0);
		}
		TestEqual(TEXT("La insolencia se queda en su tope"), State.Seats[0].Insolence, Soulbound::Match::MaxInsolence);
		TestFalse(TEXT("Un asiento inexistente no tiene insolencia"), Soulbound::Rules::RaiseInsolence(State, Soulbound::Match::SeatCount, 1.0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnExtraTurnsTest,
	"Soulbound.Core.Match.Turn.TurnosExtraSeguidos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnExtraTurnsTest::RunTest(const FString& Parameters)
{
	// CA-TUR-9 (R-TUR-9): con K turnos extra, al terminar su turno el jugador juega K más seguidos antes que nadie,
	// cada uno con su reloj. K = 1 concedido antes de su turno; K = 2 concedido durante su turno (como hará el Warden).
	for (const int32 K : {1, 2})
	{
		FRandomStream Random(31);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const int32 First = State.ActiveSeat;
		if (K == 1)
		{
			TestTrue(TEXT("K = 1: se concede"), Soulbound::Rules::GrantExtraTurns(State, First, K));
		}
		else
		{
			if (!TestTrue(TEXT("K = 2: el turno llega a la tirada"), AdvanceToChoice(State, Random, 0.0)))
			{
				return false;
			}
			TestTrue(TEXT("K = 2: se concede durante el turno"), Soulbound::Rules::GrantExtraTurns(State, First, K));
		}

		double Now = 0.0;
		for (int32 Turn = 0; Turn <= K; ++Turn, Now += 10.0)
		{
			const FPlayedTurn Played = PlayTurn(State, Random, Now, Now + 5.0);
			TestEqual(FString::Printf(TEXT("K = %d, turno %d: juega el mismo asiento"), K, Turn), Played.Seat, First);
			// Cada turno arranca su reloj; el primero con K = 2 empezó en 0, antes de concederlos.
			TestEqual(FString::Printf(TEXT("K = %d, turno %d: con su propio reloj"), K, Turn), Played.DeadlineSeconds, Now + State.Params.TurnClockSeconds);
		}
		TestEqual(FString::Printf(TEXT("K = %d: sin turnos extra pendientes"), K), State.Seats[First].ExtraTurns, 0);
		TestEqual(FString::Printf(TEXT("K = %d: después juega el siguiente del orden"), K), PlayTurn(State, Random, Now, Now + 5.0).Seat, NextInOrder(State, First));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnLostTurnsTest,
	"Soulbound.Core.Match.Turn.TurnoPerdidoSeSalta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnLostTurnsTest::RunTest(const FString& Parameters)
{
	// CA-TUR-10 (R-TUR-10): con P turnos perdidos, cuando le toca se salta, P baja uno, el registro contiene el
	// evento y juega el siguiente del orden. Se acumulan.
	{
		FRandomStream Random(41);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const TArray<int32> Order = State.TurnOrder;
		const int32 Victim = Order[1];
		TestTrue(TEXT("P = 1: se impone"), Soulbound::Rules::GrantLostTurns(State, Victim, 1));
		TestEqual(TEXT("P = 1: juega el primero"), PlayTurn(State, Random, 0.0, 5.0).Seat, Order[0]);
		TestEqual(TEXT("P = 1: se salta al que tiene el turno perdido y juega el siguiente"), PlayTurn(State, Random, 10.0, 15.0).Seat, Order[2]);
		TestEqual(TEXT("P = 1: P baja a cero"), State.Seats[Victim].LostTurns, 0);
		const FMatchEvent* Skipped = State.EventLog.FindByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::TurnSkipped; });
		TestTrue(TEXT("P = 1: el registro contiene el salto, con su asiento y su marca de tiempo"), Skipped != nullptr && Skipped->Seat == Victim && Skipped->ClockSeconds == 10.0);
	}
	{
		FRandomStream Random(42);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const TArray<int32> Order = State.TurnOrder;
		const int32 Victim = Order[1];
		TestTrue(TEXT("P = 2: se impone"), Soulbound::Rules::GrantLostTurns(State, Victim, 2));
		const TArray<int32> Expected = {Order[0], Order[2], Order[3], Order[0], Order[2], Order[3], Order[0], Order[1]};
		TArray<int32> Played;
		for (int32 Turn = 0; Turn < Expected.Num(); ++Turn)
		{
			Played.Add(PlayTurn(State, Random, Turn * 10.0, Turn * 10.0 + 5.0).Seat);
		}
		TestEqual(TEXT("P = 2: se salta dos veces y a la tercera juega"), Describe(Played), Describe(Expected));
		TestEqual(TEXT("P = 2: dos saltos en el registro"), CountEvents(State, EMatchEventType::TurnSkipped, Victim), 2);
		TestEqual(TEXT("P = 2: los saltos no cuentan como turnos jugados"), State.TurnCount, Expected.Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnExtraAndLostTest,
	"Soulbound.Core.Match.Turn.ExtrasYPerdidosSeCompensan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnExtraAndLostTest::RunTest(const FString& Parameters)
{
	// Caso límite del GDD 02: con extra y perdidos a la vez, los perdidos se descuentan contra los extra uno a uno
	// y el resto se aplica. Con 2 extra y 1 perdido juega 2 turnos seguidos y los contadores quedan a cero.
	// Tope de extra y de perdidos: parámetro de partida (2 en partida.json, GDD 05).
	FRandomStream Random(51);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const TArray<int32> Order = State.TurnOrder;
	const int32 Player = Order[1];
	Soulbound::Rules::GrantExtraTurns(State, Player, 2);
	Soulbound::Rules::GrantLostTurns(State, Player, 1);
	TestEqual(TEXT("Antes de su turno conserva los dos contadores"), State.Seats[Player].ExtraTurns + State.Seats[Player].LostTurns, 3);

	const TArray<int32> Expected = {Order[0], Order[1], Order[1], Order[2]};
	TArray<int32> Played;
	for (int32 Turn = 0; Turn < Expected.Num(); ++Turn)
	{
		Played.Add(PlayTurn(State, Random, Turn * 10.0, Turn * 10.0 + 5.0).Seat);
	}
	TestEqual(TEXT("2 extra y 1 perdido: dos turnos seguidos (uno normal y uno extra)"), Describe(Played), Describe(Expected));
	TestEqual(TEXT("Turnos extra a cero"), State.Seats[Player].ExtraTurns, 0);
	TestEqual(TEXT("Turnos perdidos a cero"), State.Seats[Player].LostTurns, 0);
	TestEqual(TEXT("No se salta ningún turno"), CountEvents(State, EMatchEventType::TurnSkipped, Player), 0);

	// Topes.
	const int32 Other = Order[3];
	Soulbound::Rules::GrantExtraTurns(State, Other, State.Params.MaxExtraTurns);
	Soulbound::Rules::GrantExtraTurns(State, Other, 1);
	TestEqual(TEXT("Al tope de extra, conceder otro lo deja en el tope"), State.Seats[Other].ExtraTurns, State.Params.MaxExtraTurns);
	Soulbound::Rules::GrantLostTurns(State, Other, State.Params.MaxLostTurns + 3);
	TestEqual(TEXT("Los perdidos tampoco pasan de su tope"), State.Seats[Other].LostTurns, State.Params.MaxLostTurns);

	// Peticiones sin sentido: no cambian nada.
	const FMatchState Before = State;
	TestFalse(TEXT("Cero turnos extra: no se concede"), Soulbound::Rules::GrantExtraTurns(State, Player, 0));
	TestFalse(TEXT("Turnos perdidos negativos: no se imponen"), Soulbound::Rules::GrantLostTurns(State, Player, -1));
	TestFalse(TEXT("Asiento inexistente: no se concede"), Soulbound::Rules::GrantExtraTurns(State, Soulbound::Match::SeatCount, 1));
	TestTrue(TEXT("Las peticiones rechazadas no cambian el estado"), Soulbound::Tests::SameState(Before, State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnDummyTest,
	"Soulbound.Core.Match.Turn.DummyUsaLasMismasFunciones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnDummyTest::RunTest(const FString& Parameters)
{
	// CA-TUR-14 (R-TUR-14): el turno de un asiento con decisor dummy pasa por las mismas funciones y fases que el de
	// cualquier otro. El motor no recibe decisores: en las dos partidas, el destino entra por SubmitDestinationIntent
	// y el turno avanza con AdvanceTurn; una lo elige un FDummyDecider y la otra se escribe a mano, como un humano.
	auto Trace = [this](bool bDummy, TArray<FString>& OutTrace)
	{
		FRandomStream Random(61);
		FRandomStream DeciderRandom(62);
		FDummyDecider Dummy;
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return;
		}
		for (int32 Turn = 0; Turn < Soulbound::Match::SeatCount; ++Turn)
		{
			const double Now = Turn * 10.0;
			const int32 LogStart = State.EventLog.Num();
			TArray<int32> Phases;
			while (State.TurnPhase != ETurnPhase::RollAndChoose && Phases.Num() < MaxStepsPerTurn && AdvanceTurn(State, Now, Random))
			{
				Phases.Add(static_cast<int32>(State.TurnPhase));
			}
			const int32 Destination = State.ReachableOptions.IsEmpty() ? INDEX_NONE
				: bDummy ? Dummy.ChooseDestination(State, State.ActiveSeat, State.ReachableOptions, DeciderRandom)
				: State.ReachableOptions.Last();
			SubmitDestinationIntent(State, State.ActiveSeat, Destination, Now + 5.0);
			Phases.Add(static_cast<int32>(State.TurnPhase));
			while (State.TurnPhase != ETurnPhase::BetweenTurns && Phases.Num() < 2 * MaxStepsPerTurn && AdvanceTurn(State, Now + 5.0, Random))
			{
				Phases.Add(static_cast<int32>(State.TurnPhase));
			}
			TArray<int32> Events;
			for (int32 Index = LogStart; Index < State.EventLog.Num(); ++Index)
			{
				// Solo los eventos de la máquina de turnos: los de resolver la casilla (vínculo y Despertar, S-006)
				// dependen del destino elegido, no de quién lo elige.
				const EMatchEventType Type = State.EventLog[Index].Type;
				if (Type == EMatchEventType::TurnStarted || Type == EMatchEventType::Rolled || Type == EMatchEventType::TurnClockExpired
					|| Type == EMatchEventType::TurnSkipped || Type == EMatchEventType::TurnEnded)
				{
					Events.Add(static_cast<int32>(Type));
				}
			}
			OutTrace.Add(FString::Printf(TEXT("fases %s eventos %s"), *Describe(Phases), *Describe(Events)));
		}
	};

	TArray<FString> WithDummy, ByHand;
	Trace(true, WithDummy);
	Trace(false, ByHand);
	TestEqual(TEXT("Una ronda completa"), WithDummy.Num(), Soulbound::Match::SeatCount);
	TestEqual(TEXT("Mismas fases y mismos eventos con dummy y a mano"), FString::Join(WithDummy, TEXT(" | ")), FString::Join(ByHand, TEXT(" | ")));

	// Y esas fases y eventos son los de un turno completo, no dos trazas vacías iguales.
	const FString FullTurn = FString::Printf(TEXT("fases %s eventos %s"),
		*Describe({
			static_cast<int32>(ETurnPhase::Start), static_cast<int32>(ETurnPhase::PreRoll), static_cast<int32>(ETurnPhase::RollAndChoose),
			static_cast<int32>(ETurnPhase::MoveAndResolve), static_cast<int32>(ETurnPhase::End), static_cast<int32>(ETurnPhase::BetweenTurns)}),
		*Describe({
			static_cast<int32>(EMatchEventType::TurnStarted), static_cast<int32>(EMatchEventType::Rolled), static_cast<int32>(EMatchEventType::TurnEnded)}));
	for (int32 Turn = 0; Turn < WithDummy.Num(); ++Turn)
	{
		TestEqual(FString::Printf(TEXT("Turno %d del dummy: un turno completo"), Turn), WithDummy[Turn], FullTurn);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnEventLogTest,
	"Soulbound.Core.Match.Turn.RegistroDeEventos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnEventLogTest::RunTest(const FString& Parameters)
{
	// CA-TUR-15 (R-TUR-15): al terminar cualquier turno, el registro contiene inicio, tirada y fin con el asiento y la
	// marca de tiempo del reloj de partida; y agotamiento si lo hubo. Interrupción: story del Warden.
	FRandomStream Random(71);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}

	// Turno confirmado: empieza en 12 y confirma en 17.
	const int32 First = State.ActiveSeat;
	TestEqual(TEXT("Turno confirmado: se juega"), PlayTurn(State, Random, 12.0, 17.0).Seat, First);
	TestEqual(TEXT("Turno confirmado: cuatro eventos (con el movimiento, S-007)"), State.EventLog.Num(), 4);
	ExpectEvent(*this, TEXT("Confirmado, inicio"), State, 0, EMatchEventType::TurnStarted, 12.0, First);
	ExpectEvent(*this, TEXT("Confirmado, tirada"), State, 1, EMatchEventType::Rolled, 12.0, First);
	ExpectEvent(*this, TEXT("Confirmado, movimiento"), State, 2, EMatchEventType::Moved, 17.0, First);
	ExpectEvent(*this, TEXT("Confirmado, fin"), State, 3, EMatchEventType::TurnEnded, 17.0, First);
	TestTrue(TEXT("La tirada queda en el registro"), State.EventLog.IsValidIndex(1) && State.EventLog[1].Value >= 1 && State.EventLog[1].Value <= Soulbound::Dice::Faces);

	// Turno agotado: empieza en 20 y nadie confirma.
	const int32 Second = State.ActiveSeat;
	if (!TestTrue(TEXT("Turno agotado: llega a la tirada"), AdvanceToChoice(State, Random, 20.0)))
	{
		return false;
	}
	const double Deadline = State.TurnDeadlineSeconds;
	TestTrue(TEXT("Turno agotado: termina"), AdvanceToBetweenTurns(State, Random, Deadline));
	TestEqual(TEXT("Turno agotado: cuatro eventos más, sin movimiento"), State.EventLog.Num(), 8);
	ExpectEvent(*this, TEXT("Agotado, inicio"), State, 4, EMatchEventType::TurnStarted, 20.0, Second);
	ExpectEvent(*this, TEXT("Agotado, tirada"), State, 5, EMatchEventType::Rolled, 20.0, Second);
	ExpectEvent(*this, TEXT("Agotado, agotamiento"), State, 6, EMatchEventType::TurnClockExpired, Deadline, Second);
	ExpectEvent(*this, TEXT("Agotado, fin"), State, 7, EMatchEventType::TurnEnded, Deadline, Second);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnMovedEventTest,
	"Soulbound.Core.Match.Turn.MovimientoEnElRegistro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTurnMovedEventTest::RunTest(const FString& Parameters)
{
	// S-007, ampliación del autor (R-TUR-15): al aceptar la intención de destino, el registro lleva un evento Moved con
	// el asiento, la casilla de destino y la marca de tiempo de la confirmación, antes del fin del turno. Las
	// intenciones rechazadas no dejan evento.
	FRandomStream Random(81);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State) || !TestTrue(TEXT("El turno llega a la tirada"), AdvanceToChoice(State, Random, 10.0))
		|| !TestFalse(TEXT("Hay casillas alcanzables"), State.ReachableOptions.IsEmpty()))
	{
		return false;
	}
	const int32 Seat = State.ActiveSeat;
	const int32 Origin = State.Seats[Seat].TileId;
	const int32 Destination = State.ReachableOptions.Last();
	const int32 OtherSeat = NextInOrder(State, Seat);

	auto CountMoved = [&State]()
	{
		return State.EventLog.FilterByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::Moved; }).Num();
	};

	TestEqual(TEXT("Otro asiento: se rechaza"), SubmitDestinationIntent(State, OtherSeat, Destination, 12.0), EIntentResult::WrongSeat);
	TestEqual(TEXT("Casilla no alcanzable (el origen): se rechaza"), SubmitDestinationIntent(State, Seat, Origin, 12.0), EIntentResult::Unreachable);
	TestEqual(TEXT("Las intenciones rechazadas no dejan evento de movimiento"), CountMoved(), 0);

	TestEqual(TEXT("Destino alcanzable: se acepta"), SubmitDestinationIntent(State, Seat, Destination, 14.0), EIntentResult::Accepted);
	if (TestEqual(TEXT("La intención aceptada deja un evento de movimiento"), CountMoved(), 1))
	{
		const FMatchEvent& Moved = State.EventLog.Last();
		TestEqual(TEXT("Es el último evento"), static_cast<int32>(Moved.Type), static_cast<int32>(EMatchEventType::Moved));
		TestEqual(TEXT("Asiento que se mueve"), Moved.Seat, Seat);
		TestEqual(TEXT("Casilla de destino"), Moved.Value, Destination);
		TestEqual(TEXT("Marca de tiempo de la confirmación"), Moved.ClockSeconds, 14.0);
	}
	TestTrue(TEXT("El turno termina"), AdvanceToBetweenTurns(State, Random, 14.0));
	TestEqual(TEXT("Terminar el turno no añade otro movimiento"), CountMoved(), 1);
	TestEqual(TEXT("El asiento está en el destino"), State.Seats[Seat].TileId, Destination);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
