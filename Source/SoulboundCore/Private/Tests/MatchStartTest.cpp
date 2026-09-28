// Tests del inicio de partida sobre el tablero de pruebas (S-002): CA-TAB-2 (R-TAB-2), CA-TAB-6 (R-TAB-5)
// y CA-TAB-7 (R-TAB-6). Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Match".
// El sorteo del orden de turno que StartMatch gana en S-003 se prueba en TurnRulesTest (CA-TUR-1).

#include "Match/MatchStart.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::LoadTestBoard;

	/** Parámetros de partida del archivo de datos; si no cargan, StartMatch los rechaza y el test falla con el motivo. */
	FMatchParams TestMatchParams()
	{
		return Soulbound::Rules::LoadMatchParamsFromFile(Soulbound::Tests::MatchParamsPath()).Params;
	}

	FMatchStartResult Start(const FBoardState& Board, int32 Players, int32 Seed)
	{
		FRandomStream Random(Seed);
		return Soulbound::Rules::StartMatch(Board, TestMatchParams(), Players, Random);
	}

	/** Efectos de los Nodos en orden de casilla, como texto "casilla:efecto": "5:1,13:3,24:2". */
	FString NodeAssignment(const FBoardState& Board)
	{
		TArray<FString> Parts;
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Tile.Type == ETileType::PowerNode)
			{
				Parts.Add(FString::Printf(TEXT("%d:%d"), Tile.TileId, static_cast<int32>(Tile.PowerNodeEffect)));
			}
		}
		return FString::Join(Parts, TEXT(","));
	}

	int32 CountEffect(const FBoardState& Board, EPowerNodeEffect Effect)
	{
		return Board.Tiles.FilterByPredicate([Effect](const FTileState& T) { return T.PowerNodeEffect == Effect; }).Num();
	}

	/** Comprueba que cada Nodo tiene efecto, que ninguna otra casilla lo tiene y que las cantidades son las de los datos. */
	void ExpectEffectsMatchData(FAutomationTestBase& Test, const FString& What, const FBoardState& Board)
	{
		for (const FTileState& Tile : Board.Tiles)
		{
			const bool bIsNode = Tile.Type == ETileType::PowerNode;
			if (bIsNode == (Tile.PowerNodeEffect == EPowerNodeEffect::None))
			{
				Test.AddError(FString::Printf(TEXT("%s: la casilla %d %s"), *What, Tile.TileId,
					bIsNode ? TEXT("es un Nodo sin efecto") : TEXT("tiene efecto sin ser Nodo")));
			}
		}
		Test.TestEqual(What + TEXT(": Nodos de Duelo"), CountEffect(Board, EPowerNodeEffect::Duel), Board.Params.DuelNodeCount);
		Test.TestEqual(What + TEXT(": Nodos del Oráculo"), CountEffect(Board, EPowerNodeEffect::Oracle), Board.Params.OracleNodeCount);
		Test.TestEqual(What + TEXT(": Nodos Planares"), CountEffect(Board, EPowerNodeEffect::Planar), Board.Params.PlanarNodeCount);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchStartSeatsOnAltarTest,
	"Soulbound.Core.Match.Start.TodosEmpiezanEnElAltar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchStartSeatsOnAltarTest::RunTest(const FString& Parameters)
{
	// CA-TAB-2 (R-TAB-2): hay exactamente una casilla Altar y todas las fichas están en ella.
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	for (int32 Players = Soulbound::Match::MinPlayers; Players <= Soulbound::Match::MaxPlayers; ++Players)
	{
		const FMatchStartResult Result = Start(Board, Players, 7);
		if (!TestTrue(FString::Printf(TEXT("%d jugadores: la partida empieza (%s)"), Players, *Result.Message), Result.IsSuccess()))
		{
			continue;
		}
		const FBoardState& Started = Result.State.Board;
		const int32 Altars = Started.Tiles.FilterByPredicate([](const FTileState& T) { return T.Type == ETileType::Altar; }).Num();
		TestEqual(FString::Printf(TEXT("%d jugadores: un solo Altar"), Players), Altars, 1);
		TestEqual(FString::Printf(TEXT("%d jugadores: cuatro asientos"), Players), Result.State.Seats.Num(), Soulbound::Match::SeatCount);
		for (int32 Seat = 0; Seat < Result.State.Seats.Num(); ++Seat)
		{
			TestTrue(FString::Printf(TEXT("%d jugadores: el asiento %d está en el Altar"), Players, Seat), Started.IsAltar(Result.State.Seats[Seat].TileId));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchStartPowerNodeEffectsTest,
	"Soulbound.Core.Match.Start.EfectosDeNodosPorSemilla",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchStartPowerNodeEffectsTest::RunTest(const FString& Parameters)
{
	// CA-TAB-6 (R-TAB-5), con el texto reescrito por el autor el 25/09/2026:
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	constexpr int32 SeedCount = 50;

	// (1) Determinismo: la misma semilla produce la misma asignación dos veces.
	const FString First = NodeAssignment(Start(Board, 4, 1234).State.Board);
	TestEqual(TEXT("Misma semilla, misma asignación"), NodeAssignment(Start(Board, 4, 1234).State.Board), First);

	// (2) El sorteo depende de la semilla: en 50 semillas consecutivas salen al menos dos asignaciones distintas.
	// (3) En las 50, cada Nodo tiene efecto y las cantidades por efecto coinciden con los datos.
	TSet<FString> Distinct;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FMatchStartResult Result = Start(Board, 4, Seed);
		if (TestTrue(FString::Printf(TEXT("Semilla %d: la partida empieza"), Seed), Result.IsSuccess()))
		{
			Distinct.Add(NodeAssignment(Result.State.Board));
			ExpectEffectsMatchData(*this, FString::Printf(TEXT("Semilla %d"), Seed), Result.State.Board);
		}
	}
	TestTrue(FString::Printf(TEXT("Al menos dos asignaciones distintas en %d semillas (salen %d)"), SeedCount, Distinct.Num()), Distinct.Num() >= 2);

	// Las cantidades salen de los datos, no del código: con tres de Duelo y ninguno de los otros, todos son de Duelo.
	FBoardState AllDuel = Board;
	AllDuel.Params.DuelNodeCount = 3;
	AllDuel.Params.OracleNodeCount = 0;
	AllDuel.Params.PlanarNodeCount = 0;
	const FMatchStartResult Duel = Start(AllDuel, 4, 99);
	TestTrue(TEXT("Con otras cantidades en datos, la partida empieza"), Duel.IsSuccess());
	ExpectEffectsMatchData(*this, TEXT("Tres de Duelo en datos"), Duel.State.Board);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchStartForgottenDomainsTest,
	"Soulbound.Core.Match.Start.DominiosOlvidados",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchStartForgottenDomainsTest::RunTest(const FString& Parameters)
{
	// CA-TAB-7 (R-TAB-6): con N jugadores, exactamente 6 menos N Dominios son Olvidados y sus Nichos están Cerrados.
	// N son los asientos ocupados por humanos o bots (autor, 25/09/2026): en Fase 1 siempre 4. Se prueban de 1 a 4.
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	for (int32 Players = Soulbound::Match::MinPlayers; Players <= Soulbound::Match::MaxPlayers; ++Players)
	{
		TSet<int32> EverForgotten;
		for (int32 Seed = 0; Seed < 20; ++Seed)
		{
			const FMatchStartResult Result = Start(Board, Players, Seed);
			if (!TestTrue(FString::Printf(TEXT("%d jugadores, semilla %d: la partida empieza"), Players, Seed), Result.IsSuccess()))
			{
				continue;
			}
			int32 Forgotten = 0;
			for (const FDomainState& Domain : Result.State.Board.Domains)
			{
				const bool bForgotten = Domain.State == EDomainState::Forgotten;
				Forgotten += bForgotten ? 1 : 0;
				if (bForgotten)
				{
					EverForgotten.Add(Domain.DomainId);
				}
				TestEqual(FString::Printf(TEXT("%d jugadores, semilla %d: el Nicho del Dominio %d está %s"), Players, Seed, Domain.DomainId, bForgotten ? TEXT("Cerrado") : TEXT("Libre")),
					static_cast<int32>(Domain.NicheState), static_cast<int32>(bForgotten ? ENicheState::Closed : ENicheState::Free));
			}
			TestEqual(FString::Printf(TEXT("%d jugadores, semilla %d: 6 menos N Olvidados"), Players, Seed), Forgotten, Soulbound::Board::DomainCount - Players);
		}
		// El sorteo es al azar: en 20 semillas los Olvidados no caen siempre en los mismos Dominios.
		TestTrue(FString::Printf(TEXT("%d jugadores: los Olvidados varían con la semilla"), Players), EverForgotten.Num() > Soulbound::Board::DomainCount - Players);
	}

	// Determinismo: misma semilla, mismos Olvidados.
	auto ForgottenIds = [](const FMatchStartResult& Result)
	{
		TArray<int32> Ids;
		for (const FDomainState& Domain : Result.State.Board.Domains)
		{
			if (Domain.State == EDomainState::Forgotten)
			{
				Ids.Add(Domain.DomainId);
			}
		}
		return Ids;
	};
	TestEqual(TEXT("Misma semilla, mismos Olvidados"), ForgottenIds(Start(Board, 2, 42)), ForgottenIds(Start(Board, 2, 42)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchStartRejectsInvalidInputTest,
	"Soulbound.Core.Match.Start.RechazaJugadoresFueraDeRangoYTableroInvalido",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchStartRejectsInvalidInputTest::RunTest(const FString& Parameters)
{
	// Nota de la story: con menos de uno o más de cuatro jugadores, resultado de error.
	FBoardState Board;
	if (!LoadTestBoard(*this, Board))
	{
		return false;
	}
	for (const int32 Players : {-1, 0, 5})
	{
		const FMatchStartResult Result = Start(Board, Players, 1);
		TestEqual(FString::Printf(TEXT("%d jugadores: error de número de jugadores"), Players), static_cast<int32>(Result.Error), static_cast<int32>(EMatchStartError::InvalidPlayerCount));
		TestFalse(FString::Printf(TEXT("%d jugadores: con un motivo legible"), Players), Result.Message.IsEmpty());
	}

	FBoardState Invalid = Board;
	Invalid.Domains.RemoveAt(5);
	const FMatchStartResult Result = Start(Invalid, 4, 1);
	TestEqual(TEXT("Tablero inválido: error de tablero"), static_cast<int32>(Result.Error), static_cast<int32>(EMatchStartError::InvalidBoard));
	TestFalse(TEXT("Tablero inválido: con un motivo legible"), Result.Message.IsEmpty());

	// S-003: los parámetros de partida también se validan; un reloj de turno a cero no es una partida jugable.
	FMatchParams Zero = TestMatchParams();
	Zero.TurnClockSeconds = 0.0;
	FRandomStream Random(1);
	const FMatchStartResult NoClock = Soulbound::Rules::StartMatch(Board, Zero, 4, Random);
	TestEqual(TEXT("Parámetros inválidos: error de parámetros"), static_cast<int32>(NoClock.Error), static_cast<int32>(EMatchStartError::InvalidMatchParams));
	TestFalse(TEXT("Parámetros inválidos: con un motivo legible"), NoClock.Message.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
