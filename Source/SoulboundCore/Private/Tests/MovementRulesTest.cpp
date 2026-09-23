// Tests de CA-TAB-15 (R-TAB-14): casillas alcanzables.
// Se lanzan sin editor con Scripts/test.ps1 (comandlet con -nullrhi). Filtro: "Soulbound.Core".

#include "Board/BoardState.h"
#include "Board/MovementRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Grafo de prueba de seis casillas, dos caminos entre 1 y 3:
	 *   0 - 1 - 2 - 3
	 *       |       |
	 *       4 ----- 5
	 */
	FBoardState MakeSixTileBoard()
	{
		FBoardState Board;
		for (int32 i = 0; i < 6; ++i)
		{
			Board.AddTile();
		}
		Board.Connect(0, 1);
		Board.Connect(1, 2);
		Board.Connect(2, 3);
		Board.Connect(1, 4);
		Board.Connect(4, 5);
		Board.Connect(5, 3);
		return Board;
	}

	FString Join(const TArray<int32>& Tiles)
	{
		return FString::JoinBy(Tiles, TEXT(","), [](int32 T) { return FString::FromInt(T); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReachableTilesExactLengthTest,
	"Soulbound.Core.Board.ReachableTiles.CaminoDeLongitudExacta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FReachableTilesExactLengthTest::RunTest(const FString& Parameters)
{
	const FBoardState Board = MakeSixTileBoard();
	const FMoverTraits Mover;

	// CA-TAB-15: toda casilla devuelta tiene un camino simple de longitud exactamente T, y ninguna con tal camino queda fuera.
	TestEqual(TEXT("Desde 0 con tirada 1: solo la 1"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 1, Mover)), TEXT("1"));
	TestEqual(TEXT("Desde 0 con tirada 2: la 2 y la 4"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 2, Mover)), TEXT("2,4"));
	TestEqual(TEXT("Desde 0 con tirada 3: la 3 y la 5"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 3, Mover)), TEXT("3,5"));
	// Con tirada 4 los dos caminos cruzan al otro lado del ciclo sin repetir casilla: 0-1-2-3-5 y 0-1-4-5-3.
	TestEqual(TEXT("Desde 0 con tirada 4: la 3 y la 5 por el otro lado"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 4, Mover)), TEXT("3,5"));
	// Tirada 5 exigiría repetir casilla (solo hay seis y el origen no cuenta): 0-1-2-3-5-4 es válido; 0-1-4-5-3-2 también.
	TestEqual(TEXT("Desde 0 con tirada 5: la 2 y la 4 dando la vuelta"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 5, Mover)), TEXT("2,4"));
	TestEqual(TEXT("Desde 0 con tirada 6: ninguna (habría que repetir casilla)"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 6, Mover)), TEXT(""));

	// Casos límite.
	TestEqual(TEXT("Tirada 0: ninguna"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 0, Mover)), TEXT(""));
	TestEqual(TEXT("Casilla de origen inválida: ninguna"), Join(Soulbound::Rules::ReachableTiles(Board, 99, 2, Mover)), TEXT(""));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReachableTilesSealedTest,
	"Soulbound.Core.Board.ReachableTiles.SelloYPasoLibre",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FReachableTilesSealedTest::RunTest(const FString& Parameters)
{
	FBoardState Board = MakeSixTileBoard();
	Board.Tiles[2].bSealed = true;

	// CA-TAB-15 con casillas no transitables: un Sello cierra el camino de arriba, pero no el de abajo.
	FMoverTraits SinPasoLibre;
	TestEqual(TEXT("Con la 2 sellada, desde 0 con tirada 3 solo queda la 5"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 3, SinPasoLibre)), TEXT("5"));
	TestEqual(TEXT("Con la 2 sellada, desde 0 con tirada 2 solo queda la 4"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 2, SinPasoLibre)), TEXT("4"));

	// Paso Libre atraviesa y pisa la casilla sellada (glosario "Paso Libre").
	FMoverTraits ConPasoLibre;
	ConPasoLibre.bFreePassage = true;
	TestEqual(TEXT("Con Paso Libre, la 2 sellada no estorba (tirada 3)"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 3, ConPasoLibre)), TEXT("3,5"));
	TestEqual(TEXT("Con Paso Libre se puede acabar en la 2 sellada (tirada 2)"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 2, ConPasoLibre)), TEXT("2,4"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
