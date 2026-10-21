// Tests del camino del movimiento (S-020, CA-MOV-1; R-TAB-14, R-TAB-16, R-TAB-17, R-TAB-20): MovementPath da, para cada
// casilla alcanzable de ReachableTiles, un camino válido hasta ella, y ninguno hacia una que no lo es. El oráculo de validez es
// propio de los tests (conexiones, sin repetir, sin el origen, longitud) y solo comparte con las reglas el criterio único de
// transitabilidad (IsPassable, R-TAB-21).
// Filtro: "Soulbound.Core.Board.MovementPath".

#include "Board/BoardState.h"
#include "Board/MovementRules.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString Join(const TArray<int32>& Tiles)
	{
		return FString::JoinBy(Tiles, TEXT(","), [](int32 T) { return FString::FromInt(T); });
	}

	/** Si la casilla Tile tiene un vecino que el jugador no puede pisar (sellado sin Paso Libre): ahí un camino puede parar antes (R-TAB-17). */
	bool TouchesSeal(const FBoardState& Board, int32 Tile, const FMoverTraits& Mover)
	{
		return Board.Tiles[Tile].Neighbours.ContainsByPredicate([&](int32 Next)
		{
			return Board.IsValidTile(Next) && Board.Tiles[Next].bSealed && !Mover.bFreePassage;
		});
	}

	/**
	 * Por qué Path no es un camino válido de From a To con la tirada Roll, o vacío si lo es: empieza junto al origen, cada casilla
	 * conecta con la anterior, no repite ni pasa por el origen, todas transitables, ninguna antes de la última es el Altar
	 * (R-TAB-16), acaba en To y mide Roll, o menos si acaba en el Altar o junto a un Sello (R-TAB-17).
	 */
	FString WhyInvalid(const FBoardState& Board, int32 From, int32 Roll, const FMoverTraits& Mover, int32 To, const TArray<int32>& Path)
	{
		if (Path.IsEmpty() || Path.Last() != To)
		{
			return TEXT("no acaba en el destino");
		}
		TSet<int32> Seen{From};
		int32 Previous = From;
		for (int32 Index = 0; Index < Path.Num(); ++Index)
		{
			const int32 Tile = Path[Index];
			if (!Board.IsValidTile(Tile) || !Board.Tiles[Previous].Neighbours.Contains(Tile))
			{
				return FString::Printf(TEXT("la %d no conecta con la %d"), Tile, Previous);
			}
			if (Seen.Contains(Tile))
			{
				return FString::Printf(TEXT("repite la %d"), Tile);
			}
			if (!Soulbound::Rules::IsPassable(Board, Tile, Mover))
			{
				return FString::Printf(TEXT("la %d no es transitable"), Tile);
			}
			if (Index < Path.Num() - 1 && Board.IsAltar(Tile))
			{
				return TEXT("cruza el Altar");
			}
			Seen.Add(Tile);
			Previous = Tile;
		}
		const bool bShortAllowed = Board.IsAltar(To) || TouchesSeal(Board, To, Mover);
		if (Path.Num() > Roll || (Path.Num() < Roll && !bShortAllowed))
		{
			return FString::Printf(TEXT("mide %d con tirada %d"), Path.Num(), Roll);
		}
		return FString();
	}

	/**
	 * Comprueba MovementPath desde cada casilla de Board con cada tirada de 1 a MaxRoll: un camino válido hacia cada alcanzable, el
	 * mismo en dos llamadas, vacío hacia el origen y unas cuantas no alcanzables; y ninguna casilla de Forbidden en ningún camino.
	 * Devuelve cuántos caminos ha comprobado.
	 */
	int32 CheckAllPaths(FAutomationTestBase& Test, const FBoardState& Board, const FMoverTraits& Mover, int32 MaxRoll, const TSet<int32>& Forbidden, const TCHAR* Context)
	{
		int32 Checked = 0;
		for (int32 From = 0; From < Board.Tiles.Num(); ++From)
		{
			if (!Soulbound::Rules::IsPassable(Board, From, Mover))
			{
				continue;
			}
			for (int32 Roll = 1; Roll <= MaxRoll; ++Roll)
			{
				const TArray<int32> Reachable = Soulbound::Rules::ReachableTiles(Board, From, Roll, Mover);
				// Todas las alcanzables y, de las que no lo son, el origen y las cuatro primeras: comprobar todas haría lento el test.
				TArray<int32> Targets = Reachable;
				Targets.Add(From);
				for (int32 Tile = 0, Unreachable = 0; Tile < Board.Tiles.Num() && Unreachable < 4; ++Tile)
				{
					if (!Reachable.Contains(Tile) && Tile != From)
					{
						Targets.Add(Tile);
						++Unreachable;
					}
				}
				for (const int32 To : Targets)
				{
					const TArray<int32> Path = Soulbound::Rules::MovementPath(Board, From, Roll, Mover, To);
					if (!Reachable.Contains(To))
					{
						if (!Path.IsEmpty())
						{
							Test.AddError(FString::Printf(TEXT("%s: de %d a %d con %d, no alcanzable, da camino %s"), Context, From, To, Roll, *Join(Path)));
							return Checked;
						}
						continue;
					}
					const FString Why = WhyInvalid(Board, From, Roll, Mover, To, Path);
					if (!Why.IsEmpty())
					{
						Test.AddError(FString::Printf(TEXT("%s: de %d a %d con %d, camino [%s] %s"), Context, From, To, Roll, *Join(Path), *Why));
						return Checked;
					}
					if (Soulbound::Rules::MovementPath(Board, From, Roll, Mover, To) != Path)
					{
						Test.AddError(FString::Printf(TEXT("%s: de %d a %d con %d, dos llamadas iguales dan caminos distintos"), Context, From, To, Roll));
						return Checked;
					}
					if (const int32* Bad = Path.FindByPredicate([&Forbidden](int32 Tile) { return Forbidden.Contains(Tile); }))
					{
						Test.AddError(FString::Printf(TEXT("%s: de %d a %d con %d, el camino pisa la %d, de un Dominio con dueño"), Context, From, To, Roll, *Bad));
						return Checked;
					}
					++Checked;
				}
			}
		}
		return Checked;
	}

	/** Sella a mano las dos primeras casillas normales del tablero (el test no comprueba las reglas de Sello). */
	void SealTwoNormalTiles(FBoardState& Board)
	{
		int32 Sealed = 0;
		for (FTileState& Tile : Board.Tiles)
		{
			if (Tile.Type == ETileType::Normal && Sealed < 2)
			{
				Tile.bSealed = true;
				Board.SealedTiles.Add(Tile.TileId);
				++Sealed;
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementPathToEveryReachableTest,
	"Soulbound.Core.Board.MovementPath.CaminoHastaCadaAlcanzable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMovementPathToEveryReachableTest::RunTest(const FString& Parameters)
{
	// CA-MOV-1 (R-TAB-14, R-TAB-16, R-TAB-17): en los tableros del proyecto, desde cada casilla y con cada tirada de uno y dos
	// dados, cada alcanzable tiene un camino de longitud exactamente la tirada (menor hasta el Altar o junto a un Sello), con
	// todas sus casillas transitables y sin repetir; las no alcanzables, ninguno; dos llamadas iguales, el mismo camino. Con dos
	// Sellos puestos a mano, para que R-TAB-17 aparezca; con Paso Libre, los Sellos no paran.
	for (const FString& FileName : Soulbound::Tests::BoardFileNames())
	{
		FBoardState Board;
		if (!Soulbound::Tests::LoadBoardFile(*this, FileName, Board))
		{
			return false;
		}
		SealTwoNormalTiles(Board);
		const int32 Plain = CheckAllPaths(*this, Board, FMoverTraits(), Soulbound::Dice::Faces * Soulbound::Dice::MaxCount, {}, *FileName);
		FMoverTraits FreePassage;
		FreePassage.bFreePassage = true;
		const int32 Free = CheckAllPaths(*this, Board, FreePassage, Soulbound::Dice::Faces, {}, *FString::Printf(TEXT("%s con Paso Libre"), *FileName));
		TestTrue(FString::Printf(TEXT("%s: se comprueban caminos (%d y %d)"), *FileName, Plain, Free), Plain > 0 && Free > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementPathConfinedTest,
	"Soulbound.Core.Board.MovementPath.EcoConfinadoSinDominiosConDueno",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMovementPathConfinedTest::RunTest(const FString& Parameters)
{
	// CA-MOV-1 (R-TAB-20): para un Eco confinado a los Olvidados, ningún camino pisa una casilla de un Dominio con dueño. Dos
	// Dominios Olvidados puestos a mano; el resto con dueño.
	FBoardState Board;
	if (!Soulbound::Tests::LoadBoardFile(*this, Soulbound::Tests::RaceBoardFileName(), Board) || !TestEqual(TEXT("Seis Dominios"), Board.Domains.Num(), Soulbound::Board::DomainCount))
	{
		return false;
	}
	Board.Domains[1].State = EDomainState::Forgotten;
	Board.Domains[4].State = EDomainState::Forgotten;
	TSet<int32> Owned;
	for (const FTileState& Tile : Board.Tiles)
	{
		if (Board.Domains.IsValidIndex(Tile.DomainId) && Board.Domains[Tile.DomainId].State == EDomainState::Owned)
		{
			Owned.Add(Tile.TileId);
		}
	}
	FMoverTraits Confined;
	Confined.bConfinedToForgotten = true;
	const int32 Checked = CheckAllPaths(*this, Board, Confined, Soulbound::Dice::Faces * Soulbound::Dice::MaxCount, Owned, TEXT("Eco confinado"));
	TestTrue(FString::Printf(TEXT("Se comprueban caminos (%d)"), Checked), Checked > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementPathAscendingTest,
	"Soulbound.Core.Board.MovementPath.PrimeroEnOrdenAscendente",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMovementPathAscendingTest::RunTest(const FString& Parameters)
{
	// CA-MOV-1 (R-TAB-14, decisión de la story): entre dos caminos de la misma longitud, el primero en orden ascendente de
	// vecinos, aunque el archivo los conecte en otro orden. Rombo 0-1-3 y 0-2-3, con la 2 conectada antes que la 1:
	//   0 - 2 - 3
	//    \     /
	//      1 -
	FBoardState Board;
	for (int32 i = 0; i < 4; ++i)
	{
		Board.AddTile();
	}
	Board.Connect(0, 2);
	Board.Connect(0, 1);
	Board.Connect(2, 3);
	Board.Connect(1, 3);
	const FMoverTraits Mover;
	TestEqual(TEXT("De 0 a 3 con 2: por la 1"), Join(Soulbound::Rules::MovementPath(Board, 0, 2, Mover, 3)), TEXT("1,3"));
	TestEqual(TEXT("De 3 a 0 con 2: por la 1"), Join(Soulbound::Rules::MovementPath(Board, 3, 2, Mover, 0)), TEXT("1,0"));
	// Con la 1 sellada, por la 2; hacia la 0 con tirada 3, el camino da la vuelta sin repetir: 0-2-3-1 no existe (la 1 sellada).
	Board.Tiles[1].bSealed = true;
	TestEqual(TEXT("Con la 1 sellada: por la 2"), Join(Soulbound::Rules::MovementPath(Board, 0, 2, Mover, 3)), TEXT("2,3"));
	TestEqual(TEXT("Hacia el origen: ninguno"), Join(Soulbound::Rules::MovementPath(Board, 0, 2, Mover, 0)), TEXT(""));
	TestEqual(TEXT("Casilla inexistente: ninguno"), Join(Soulbound::Rules::MovementPath(Board, 0, 2, Mover, 9)), TEXT(""));
	TestEqual(TEXT("Tirada 0: ninguno"), Join(Soulbound::Rules::MovementPath(Board, 0, 0, Mover, 3)), TEXT(""));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
