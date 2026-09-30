// Tests de los Sellos con tope (S-004): CA-TAB-23 (R-TAB-22), CA-TAB-24 (R-TAB-23) y el registro de eventos de los
// Sellos (GDD 01, criterio final). Se lanzan sin editor con Scripts/test.ps1.

#include "Board/SealRules.h"
#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "MatchTestSetup.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Partida de prueba de los Sellos, con los cuatro asientos en el Altar (0):
	 *
	 *            6 (Fuente)
	 *           / \
	 *  (Nicho) 5 - 4 - 0 - 1 - 7
	 *                 /|\     /
	 *                3 8 2 ---
	 *
	 * Conexiones: 0-1, 0-2, 0-3, 0-4, 0-6, 0-8, 1-6, 1-7, 2-7, 4-5. Sellar la 4 deja el Nicho sin camino al Altar;
	 * sellar la 1 y la 2 a la vez deja sin camino a la 7. Las demás casillas normales se sellan sin aislar nada.
	 * MaxSeals es un dato del test, no un valor de juego.
	 */
	FMatchState MakeSealMatch(int32 MaxSeals)
	{
		FMatchState State;
		FBoardState& Board = State.Board;
		for (int32 i = 0; i < 9; ++i)
		{
			Board.AddTile();
		}
		Board.Tiles[0].Type = ETileType::Altar;
		Board.Tiles[5].Type = ETileType::Niche;
		Board.Tiles[6].Type = ETileType::PowerSource;
		for (const int32 Tile : {1, 2, 3, 4, 6, 8})
		{
			Board.Connect(0, Tile);
		}
		Board.Connect(1, 6);
		Board.Connect(1, 7);
		Board.Connect(2, 7);
		Board.Connect(4, 5);
		Board.Params.MaxSealedTiles = MaxSeals;

		State.Seats.SetNum(Soulbound::Match::SeatCount);
		for (FSeatState& Seat : State.Seats)
		{
			Seat.TileId = 0;
		}
		return State;
	}

	FString ResultName(ESealResult Result)
	{
		switch (Result)
		{
		case ESealResult::Accepted: return TEXT("Accepted");
		case ESealResult::InvalidTile: return TEXT("InvalidTile");
		case ESealResult::NoSealsAllowed: return TEXT("NoSealsAllowed");
		case ESealResult::AlreadySealed: return TEXT("AlreadySealed");
		case ESealResult::Altar: return TEXT("Altar");
		case ESealResult::Niche: return TEXT("Niche");
		case ESealResult::PowerSource: return TEXT("PowerSource");
		case ESealResult::Occupied: return TEXT("Occupied");
		case ESealResult::CutsOffAltar: return TEXT("CutsOffAltar");
		}
		return TEXT("?");
	}

	/** Comprueba que CanSealTile y SealTile dan Expected y que un rechazo no cambia el estado ni el registro. */
	void ExpectSeal(FAutomationTestBase& Test, FMatchState& State, int32 TileId, ESealResult Expected, const TCHAR* What)
	{
		const FMatchState Before = State;
		Test.TestEqual(FString::Printf(TEXT("%s: CanSealTile(%d)"), What, TileId), ResultName(Soulbound::Rules::CanSealTile(State, TileId)), ResultName(Expected));
		Test.TestEqual(FString::Printf(TEXT("%s: SealTile(%d)"), What, TileId), ResultName(Soulbound::Rules::SealTile(State, TileId, 1.0)), ResultName(Expected));
		if (Expected != ESealResult::Accepted)
		{
			Test.TestTrue(FString::Printf(TEXT("%s: el rechazo no cambia el estado"), What), Soulbound::Tests::SameState(Before, State));
		}
	}

	FString Join(const TArray<int32>& Tiles)
	{
		return FString::JoinBy(Tiles, TEXT(", "), [](int32 Id) { return FString::FromInt(Id); });
	}

	int32 CountSealEvents(const FMatchState& State)
	{
		return State.EventLog.FilterByPredicate([](const FMatchEvent& Event)
		{
			return Event.Type == EMatchEventType::SealPlaced || Event.Type == EMatchEventType::SealReleased;
		}).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSealCapReleasesOldestTest,
	"Soulbound.Core.Seals.TopeLiberaElMasAntiguo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSealCapReleasesOldestTest::RunTest(const FString& Parameters)
{
	// CA-TAB-23 (R-TAB-22): dado el máximo M ya alcanzado, al sellar otra casilla se libera la más antigua y quedan
	// exactamente M selladas. M = 2; se sella dos veces por encima del máximo para ver que cae siempre la más antigua.
	const int32 MaxSeals = 2;
	FMatchState State = MakeSealMatch(MaxSeals);

	ExpectSeal(*this, State, 1, ESealResult::Accepted, TEXT("Primer Sello"));
	ExpectSeal(*this, State, 3, ESealResult::Accepted, TEXT("Segundo Sello, máximo alcanzado"));
	TestEqual(TEXT("Con el máximo alcanzado, las dos en orden"), Join(State.Board.SealedTiles), FString(TEXT("1, 3")));

	ExpectSeal(*this, State, 2, ESealResult::Accepted, TEXT("Tercer Sello, por encima del máximo"));
	TestEqual(TEXT("Cae la más antigua (1)"), Join(State.Board.SealedTiles), FString(TEXT("3, 2")));
	TestFalse(TEXT("La 1 queda liberada"), State.Board.Tiles[1].bSealed);
	TestEqual(TEXT("Exactamente M selladas"), State.Board.SealedTiles.Num(), MaxSeals);

	ExpectSeal(*this, State, 8, ESealResult::Accepted, TEXT("Cuarto Sello"));
	TestEqual(TEXT("Cae la siguiente más antigua (3)"), Join(State.Board.SealedTiles), FString(TEXT("2, 8")));
	TestFalse(TEXT("La 3 queda liberada"), State.Board.Tiles[3].bSealed);
	TestEqual(TEXT("Sigue habiendo exactamente M"), State.Board.SealedTiles.Num(), MaxSeals);

	Soulbound::Tests::CheckSealInvariants(*this, State, TEXT("Tras sellar por encima del máximo"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSealForbiddenTilesTest,
	"Soulbound.Core.Seals.CasillasProhibidas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSealForbiddenTilesTest::RunTest(const FString& Parameters)
{
	// CA-TAB-23 (R-TAB-22): Altar, Nicho, Fuente y casilla ocupada se rechazan, cada una por su motivo, sin cambiar el
	// estado. Además, los rechazos que no nombra el criterio: casilla inexistente, ya sellada y máximo 0.
	FMatchState State = MakeSealMatch(2);
	State.Seats[1].TileId = 3;

	ExpectSeal(*this, State, 0, ESealResult::Altar, TEXT("El Altar"));
	ExpectSeal(*this, State, 5, ESealResult::Niche, TEXT("Un Nicho"));
	ExpectSeal(*this, State, 6, ESealResult::PowerSource, TEXT("Una Fuente"));
	ExpectSeal(*this, State, 3, ESealResult::Occupied, TEXT("Una casilla ocupada"));
	ExpectSeal(*this, State, 99, ESealResult::InvalidTile, TEXT("Una casilla que no existe"));
	ExpectSeal(*this, State, INDEX_NONE, ESealResult::InvalidTile, TEXT("Ninguna casilla"));
	TestTrue(TEXT("Ningún rechazo deja eventos"), State.EventLog.IsEmpty());

	ExpectSeal(*this, State, 1, ESealResult::Accepted, TEXT("Una casilla normal libre"));
	ExpectSeal(*this, State, 1, ESealResult::AlreadySealed, TEXT("La misma casilla otra vez: un Sello no se renueva"));

	FMatchState NoSeals = MakeSealMatch(0);
	ExpectSeal(*this, NoSeals, 8, ESealResult::NoSealsAllowed, TEXT("Con máximo 0"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSealKeepsPathToAltarTest,
	"Soulbound.Core.Seals.NoDejaCasillasSinCaminoAlAltar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSealKeepsPathToAltarTest::RunTest(const FString& Parameters)
{
	// CA-TAB-23 (R-TAB-22): se rechaza un Sello que dejaría alguna casilla no sellada sin camino al Altar para un
	// jugador sin Paso Libre. Se mira el tablero tal como quedaría: si el Sello que cae por el tope reabre el camino,
	// el Sello nuevo vale.
	FMatchState State = MakeSealMatch(2);
	ExpectSeal(*this, State, 4, ESealResult::CutsOffAltar, TEXT("La 4 aislaría el Nicho 5"));
	ExpectSeal(*this, State, 1, ESealResult::Accepted, TEXT("La 1 sola no aísla nada"));
	ExpectSeal(*this, State, 2, ESealResult::CutsOffAltar, TEXT("La 2 con la 1 sellada aislaría la 7"));
	TestEqual(TEXT("Solo sigue sellada la 1"), Join(State.Board.SealedTiles), FString(TEXT("1")));

	FMatchState OneSeal = MakeSealMatch(1);
	ExpectSeal(*this, OneSeal, 1, ESealResult::Accepted, TEXT("Con máximo 1, la 1"));
	ExpectSeal(*this, OneSeal, 2, ESealResult::Accepted, TEXT("Con máximo 1, la 2 vale porque la 1 cae por el tope"));
	TestEqual(TEXT("Queda sellada solo la 2"), Join(OneSeal.Board.SealedTiles), FString(TEXT("2")));
	Soulbound::Tests::CheckSealInvariants(*this, OneSeal, TEXT("Con máximo 1"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSealOnlyWardenReleasesTest,
	"Soulbound.Core.Seals.SoloElWardenLibera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSealOnlyWardenReleasesTest::RunTest(const FString& Parameters)
{
	// CA-TAB-24 (R-TAB-23): los jugadores no tienen ninguna acción que mueva o retire un Sello. Todo lo que un
	// jugador puede hacer en el motor es jugar su turno (AdvanceTurn y SubmitDestinationIntent): decenas de turnos
	// no cambian los Sellos. La única otra forma de liberar uno es ReleaseSeal, reservada al Warden.
	FRandomStream Random(3);
	FMatchState State;
	if (!Soulbound::Tests::StartTestMatch(*this, Random, State))
	{
		return false;
	}
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (State.Board.SealedTiles.Num() < State.Board.Params.MaxSealedTiles)
		{
			Soulbound::Rules::SealTile(State, Tile.TileId, 0.0);
		}
	}
	const TArray<int32> Sealed = State.Board.SealedTiles;
	const int32 SealEvents = CountSealEvents(State);
	if (!TestEqual(TEXT("Se llega al máximo de Sellos del tablero de pruebas"), Sealed.Num(), State.Board.Params.MaxSealedTiles))
	{
		return false;
	}

	int32 Moves = 0;
	for (int32 Step = 0; Step < 400; ++Step)
	{
		const double Now = Step;
		if (State.TurnPhase == ETurnPhase::RollAndChoose && !State.ReachableOptions.IsEmpty())
		{
			Moves += Soulbound::Rules::SubmitDestinationIntent(State, State.ActiveSeat, State.ReachableOptions[0], Now) == EIntentResult::Accepted ? 1 : 0;
		}
		else
		{
			Soulbound::Rules::AdvanceTurn(State, Now, Random);
		}
	}
	TestTrue(TEXT("Los jugadores se han movido"), Moves > 10);
	TestEqual(TEXT("Tras los turnos, los mismos Sellos en el mismo orden"), Join(State.Board.SealedTiles), Join(Sealed));
	TestEqual(TEXT("Los turnos no dejan eventos de Sello"), CountSealEvents(State), SealEvents);
	Soulbound::Tests::CheckSealInvariants(*this, State, TEXT("Tras los turnos"));

	// La función del Warden: libera, saca la casilla del orden y no hace nada con una casilla sin Sello.
	const int32 Released = Sealed[1];
	TestTrue(TEXT("ReleaseSeal libera una casilla sellada"), Soulbound::Rules::ReleaseSeal(State, Released, 500.0));
	TestFalse(TEXT("La casilla queda sin Sello"), State.Board.Tiles[Released].bSealed);
	TestEqual(TEXT("Y sale del orden sin mover a las demás"), Join(State.Board.SealedTiles), FString::Printf(TEXT("%d, %d"), Sealed[0], Sealed[2]));

	const FMatchState Before = State;
	TestFalse(TEXT("ReleaseSeal de una casilla sin Sello devuelve false"), Soulbound::Rules::ReleaseSeal(State, Released, 501.0));
	TestFalse(TEXT("ReleaseSeal de una casilla inexistente devuelve false"), Soulbound::Rules::ReleaseSeal(State, 999, 501.0));
	TestTrue(TEXT("Y no cambia el estado"), Soulbound::Tests::SameState(Before, State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSealEventLogTest,
	"Soulbound.Core.Seals.RegistroDeSellos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSealEventLogTest::RunTest(const FString& Parameters)
{
	// Criterio de registro (GDD 01): cada Sello puesto o liberado, por tope o por el Warden, deja un evento con la
	// casilla y la marca de tiempo. El liberado por tope va antes que el puesto que lo provoca.
	FMatchState State = MakeSealMatch(1);
	auto ExpectEvent = [this, &State](int32 Index, EMatchEventType Type, double Clock, int32 Tile, const TCHAR* What)
	{
		if (!TestTrue(FString::Printf(TEXT("%s: hay evento %d"), What, Index), State.EventLog.IsValidIndex(Index)))
		{
			return;
		}
		const FMatchEvent& Event = State.EventLog[Index];
		TestEqual(FString::Printf(TEXT("%s: tipo"), What), static_cast<int32>(Event.Type), static_cast<int32>(Type));
		TestEqual(FString::Printf(TEXT("%s: marca de tiempo"), What), Event.ClockSeconds, Clock);
		TestEqual(FString::Printf(TEXT("%s: casilla"), What), Event.Value, Tile);
		TestEqual(FString::Printf(TEXT("%s: sin asiento"), What), Event.Seat, static_cast<int32>(INDEX_NONE));
	};

	Soulbound::Rules::SealTile(State, 1, 10.0);
	TestEqual(TEXT("Un Sello, un evento"), State.EventLog.Num(), 1);
	ExpectEvent(0, EMatchEventType::SealPlaced, 10.0, 1, TEXT("Sello puesto"));

	Soulbound::Rules::SealTile(State, 2, 20.0);
	TestEqual(TEXT("Sello por encima del máximo, dos eventos más"), State.EventLog.Num(), 3);
	ExpectEvent(1, EMatchEventType::SealReleased, 20.0, 1, TEXT("Liberado por tope, primero"));
	ExpectEvent(2, EMatchEventType::SealPlaced, 20.0, 2, TEXT("Sello puesto, después"));

	Soulbound::Rules::ReleaseSeal(State, 2, 30.0);
	TestEqual(TEXT("Liberado por el Warden, un evento más"), State.EventLog.Num(), 4);
	ExpectEvent(3, EMatchEventType::SealReleased, 30.0, 2, TEXT("Liberado por el Warden"));

	Soulbound::Rules::SealTile(State, 0, 40.0);
	Soulbound::Rules::ReleaseSeal(State, 2, 40.0);
	TestEqual(TEXT("Los intentos rechazados no dejan evento"), State.EventLog.Num(), 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
