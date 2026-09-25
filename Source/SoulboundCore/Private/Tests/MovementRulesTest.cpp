// Tests de casillas alcanzables: CA-TAB-15 (R-TAB-14), CA-TAB-17 (R-TAB-16, Altar) y CA-TAB-18 (R-TAB-17, Sello).
// Se lanzan sin editor con Scripts/test.ps1 (comandlet con -nullrhi). Filtro: "Soulbound.Core".

#include "Board/BoardState.h"
#include "Board/MovementRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Grafo de prueba de seis casillas, dos caminos entre 1 y 3. La 0 es el Altar, como en MakeSpikeMatch:
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
		Board.Tiles[0].Type = ETileType::Altar;
		Board.Connect(0, 1);
		Board.Connect(1, 2);
		Board.Connect(2, 3);
		Board.Connect(1, 4);
		Board.Connect(4, 5);
		Board.Connect(5, 3);
		return Board;
	}

	/**
	 * Grafo de prueba con el Altar (0) en medio de un camino, para ver qué hay "al otro lado":
	 *           5 - 6
	 *           |
	 *   1 - 2 - 0 - 3 - 4
	 * La rama 5-6 sale de la 2, no del Altar: desde la 1 hay caminos que no pasan por él.
	 */
	FBoardState MakeAltarBoard()
	{
		FBoardState Board;
		for (int32 i = 0; i < 7; ++i)
		{
			Board.AddTile();
		}
		Board.Tiles[0].Type = ETileType::Altar;
		Board.Connect(1, 2);
		Board.Connect(2, 0);
		Board.Connect(0, 3);
		Board.Connect(3, 4);
		Board.Connect(2, 5);
		Board.Connect(5, 6);
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

	// CA-TAB-18 (R-TAB-17): el Sello de la 2 corta el camino de arriba en el paso 2 y la 1 pasa a ser alcanzable;
	// el camino de abajo sigue cumpliendo CA-TAB-15. Antes de R-TAB-17 la 1 no aparecía.
	FMoverTraits SinPasoLibre;
	TestEqual(TEXT("Con la 2 sellada, desde 0 con tirada 3: la 1 (topa con el Sello) y la 5"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 3, SinPasoLibre)), TEXT("1,5"));
	TestEqual(TEXT("Con la 2 sellada, desde 0 con tirada 2: la 1 (topa con el Sello) y la 4"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 2, SinPasoLibre)), TEXT("1,4"));

	// CA-TAB-18 (R-POD-2): Paso Libre atraviesa y pisa la casilla sellada (glosario "Paso Libre"); no se detiene en la 1.
	FMoverTraits ConPasoLibre;
	ConPasoLibre.bFreePassage = true;
	TestEqual(TEXT("Con Paso Libre, la 2 sellada no estorba (tirada 3)"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 3, ConPasoLibre)), TEXT("3,5"));
	TestEqual(TEXT("Con Paso Libre se puede acabar en la 2 sellada (tirada 2)"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 2, ConPasoLibre)), TEXT("2,4"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReachableTilesSealStopsTest,
	"Soulbound.Core.Board.ReachableTiles.SelloDetieneAntes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FReachableTilesSealStopsTest::RunTest(const FString& Parameters)
{
	FBoardState Board = MakeSixTileBoard();
	Board.Tiles[2].bSealed = true;
	const FMoverTraits SinPasoLibre;
	FMoverTraits ConPasoLibre;
	ConPasoLibre.bFreePassage = true;

	// CA-TAB-18 (R-TAB-17): el camino 0-1-2 topa con el Sello en el paso P=2 con tirada T=4 (P < T).
	// Sin Paso Libre: la casilla P-1 (la 1) está y la sellada no. La 3 llega por abajo con longitud exacta (CA-TAB-15).
	const TArray<int32> Sin = Soulbound::Rules::ReachableTiles(Board, 0, 4, SinPasoLibre);
	TestEqual(TEXT("Sin Paso Libre, tirada 4: la 1 (anterior al Sello) y la 3 por abajo"), Join(Sin), TEXT("1,3"));
	TestFalse(TEXT("Sin Paso Libre, la sellada nunca es alcanzable"), Sin.Contains(2));

	// Con Paso Libre: la sellada es un paso más y las posteriores llegan con longitud exacta (0-1-2-3-5 y 0-1-4-5-3).
	TestEqual(TEXT("Con Paso Libre, tirada 4: la 3 y la 5, sin parar en la 1"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 4, ConPasoLibre)), TEXT("3,5"));
	TestEqual(TEXT("Con Paso Libre, tirada 3: la 3, posterior a la sellada, y la 5"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 3, ConPasoLibre)), TEXT("3,5"));

	// Caso límite aclarado en la story: Sello en el paso 1. "La casilla anterior" sería el origen, que nunca es
	// alcanzable; sin otra salida no queda ninguna y se aplica R-TAB-18 (el jugador no se mueve).
	FBoardState Encerrado = MakeSixTileBoard();
	Encerrado.Tiles[1].bSealed = true;
	TestEqual(TEXT("Sello en el paso 1 y sin otra salida: ninguna, el origen no cuenta"), Join(Soulbound::Rules::ReachableTiles(Encerrado, 0, 3, SinPasoLibre)), TEXT(""));
	TestEqual(TEXT("Sello en el paso 1 con Paso Libre: se cruza con normalidad"), Join(Soulbound::Rules::ReachableTiles(Encerrado, 0, 3, ConPasoLibre)), TEXT("3,5"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReachableTilesAltarStopsTest,
	"Soulbound.Core.Board.ReachableTiles.AltarDetieneElMovimiento",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FReachableTilesAltarStopsTest::RunTest(const FString& Parameters)
{
	const FBoardState Board = MakeAltarBoard();
	const FMoverTraits Mover;

	// CA-TAB-17 (R-TAB-16): desde la 1 el Altar está a distancia D=2. Para toda tirada T >= D el Altar está y
	// ninguna casilla al otro lado (3 y 4) lo está. La rama 5-6 no pasa por el Altar y sigue CA-TAB-15.
	TestEqual(TEXT("D=2, T=2: el Altar a distancia exacta y la 5"), Join(Soulbound::Rules::ReachableTiles(Board, 1, 2, Mover)), TEXT("0,5"));
	TestEqual(TEXT("D=2, T=3: el Altar aunque sobre tirada y la 6; la 3 no"), Join(Soulbound::Rules::ReachableTiles(Board, 1, 3, Mover)), TEXT("0,6"));
	TestEqual(TEXT("D=2, T=4: solo el Altar; la 4 no"), Join(Soulbound::Rules::ReachableTiles(Board, 1, 4, Mover)), TEXT("0"));
	TestEqual(TEXT("D=2, T=1: el Altar queda lejos, solo la 2"), Join(Soulbound::Rules::ReachableTiles(Board, 1, 1, Mover)), TEXT("2"));

	// El origen no cuenta como entrar en el Altar: quien está en él sale con normalidad y longitud exacta.
	TestEqual(TEXT("Desde el Altar con tirada 2: la 1, la 4 y la 5"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 2, Mover)), TEXT("1,4,5"));
	TestEqual(TEXT("Desde el Altar con tirada 3: la 6"), Join(Soulbound::Rules::ReachableTiles(Board, 0, 3, Mover)), TEXT("6"));

	// En el tablero de seis casillas el Altar (0) es un extremo: desde la 3 está a D=3 por arriba y D=4 por abajo.
	const FBoardState Six = MakeSixTileBoard();
	// Con T=4: 3-2-1-0 para en el Altar con tirada de sobra, 3-5-4-1-0 llega exacto, y 3-5-4-1-2 y 3-2-1-4-5 son exactas.
	TestEqual(TEXT("Tablero de seis, desde 3 con T=4: el Altar por los dos caminos, la 2 y la 5"), Join(Soulbound::Rules::ReachableTiles(Six, 3, 4, Mover)), TEXT("0,2,5"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
