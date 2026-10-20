// Tests del estado de vista del tablero (S-019, glosario propuesto "BoardViewState"): qué piezas del kit enseña la vista según
// la partida, calculado del FMatchState sin actor, sin mundo y sin assets. Cada test cita el criterio (CA-VIS-n) y la regla
// (R-VIS-n) que cubre. Filtro: "Soulbound.Game.Presentation.Tablero3D".

#include "../../Presentation/BoardViewState.h"
#include "Board/BoardLoader.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags ViewStateTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/**
	 * Partida armada a mano sobre tablero_35.json: Primordial, sin reservas ni Sellos, los cuatro asientos en el Altar y un
	 * efecto en cada Nodo (rotando Planar, Duelo, Oráculo). La vista solo lee el estado, así que no hace falta StartMatch.
	 */
	bool MakeState(FAutomationTestBase& Test, FMatchState& OutState)
	{
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/tablero_35.json")));
		if (!Test.TestTrue(FString::Printf(TEXT("tablero_35.json se carga (%s)"), *Result.Message), Result.IsSuccess()))
		{
			return false;
		}
		OutState.Board = Result.Board;
		const EPowerNodeEffect Effects[] = {EPowerNodeEffect::Planar, EPowerNodeEffect::Duel, EPowerNodeEffect::Oracle};
		int32 Next = 0;
		for (FTileState& Tile : OutState.Board.Tiles)
		{
			if (Tile.Type == ETileType::PowerNode)
			{
				Tile.PowerNodeEffect = Effects[Next++ % UE_ARRAY_COUNT(Effects)];
			}
		}
		OutState.Seats.SetNum(Soulbound::Match::SeatCount);
		for (FSeatState& Seat : OutState.Seats)
		{
			Seat.TileId = OutState.Board.FindAltar();
		}
		return true;
	}

	/** Estructura de Nodo que corresponde a un efecto. */
	EBoardStructure NodeStructure(EPowerNodeEffect Effect)
	{
		switch (Effect)
		{
		case EPowerNodeEffect::Planar: return EBoardStructure::PlanarArch;
		case EPowerNodeEffect::Duel: return EBoardStructure::DuelSwords;
		case EPowerNodeEffect::Oracle: return EBoardStructure::OracleOrb;
		case EPowerNodeEffect::None:
		default: return EBoardStructure::None;
		}
	}
}

// CA-VIS-5 (R-VIS-5, R-TAB-8): antes del Despertar no se pide ninguna estructura de Nodo; desde el Despertar, la de su efecto.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewNodesTest, "Soulbound.Game.Presentation.Tablero3D.NodosSoloDesdeElDespertar", ViewStateTestFlags)
bool FBoardViewNodesTest::RunTest(const FString& Parameters)
{
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	const FBoardViewState Primordial = Soulbound::Presentation::ComputeBoardViewState(State, true);
	State.Board.Phase = EBoardPhase::Awakened;
	const FBoardViewState Awakened = Soulbound::Presentation::ComputeBoardViewState(State, true);
	if (!TestEqual(TEXT("Un estado por casilla, Primordial"), Primordial.Tiles.Num(), State.Board.Tiles.Num())
		|| !TestEqual(TEXT("Un estado por casilla, Despierto"), Awakened.Tiles.Num(), State.Board.Tiles.Num()))
	{
		return false;
	}
	int32 Nodes = 0;
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (Tile.Type != ETileType::PowerNode)
		{
			continue;
		}
		++Nodes;
		TestTrue(FString::Printf(TEXT("Primordial: el Nodo %d no enseña estructura"), Tile.TileId), Primordial.Tiles[Tile.TileId].Structure == EBoardStructure::None);
		TestTrue(FString::Printf(TEXT("Despierto: el Nodo %d enseña la estructura de su efecto"), Tile.TileId), Awakened.Tiles[Tile.TileId].Structure == NodeStructure(Tile.PowerNodeEffect));
	}
	TestTrue(TEXT("El tablero de 35 tiene Nodos"), Nodes > 0);
	return true;
}

// CA-VIS-5 (R-VIS-5): el pilón se pide en cada Fuente siempre; la runa flotante, solo mientras su reserva es mayor que cero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewRuneTest, "Soulbound.Game.Presentation.Tablero3D.RunaSoloConReserva", ViewStateTestFlags)
bool FBoardViewRuneTest::RunTest(const FString& Parameters)
{
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	State.Board.Phase = EBoardPhase::Awakened;
	int32 Sources = 0;
	for (FTileState& Tile : State.Board.Tiles)
	{
		if (Tile.Type == ETileType::PowerSource)
		{
			// Una Fuente con reserva y la siguiente agotada.
			Tile.RuneReserve = Sources++ % 2 == 0 ? 2 : 0;
		}
	}
	const FBoardViewState View = Soulbound::Presentation::ComputeBoardViewState(State, true);
	if (!TestEqual(TEXT("Un estado por casilla"), View.Tiles.Num(), State.Board.Tiles.Num()))
	{
		return false;
	}
	for (const FTileState& Tile : State.Board.Tiles)
	{
		const FTileViewState& Shown = View.Tiles[Tile.TileId];
		if (Tile.Type == ETileType::PowerSource)
		{
			TestTrue(FString::Printf(TEXT("La Fuente %d enseña su pilón"), Tile.TileId), Shown.Structure == EBoardStructure::Pylon);
			TestEqual(FString::Printf(TEXT("La Fuente %d enseña la runa flotante solo con reserva (%d)"), Tile.TileId, Tile.RuneReserve), Shown.bFloatingRune, Tile.RuneReserve > 0);
		}
		else
		{
			TestFalse(FString::Printf(TEXT("La casilla %d, que no es Fuente, no enseña runa flotante"), Tile.TileId), Shown.bFloatingRune);
		}
	}
	TestTrue(TEXT("El tablero de 35 tiene Fuentes con y sin reserva"), Sources > 1);
	return true;
}

// CA-VIS-5 (R-VIS-5): las cadenas del Sello, solo sobre la casilla sellada.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewSealTest, "Soulbound.Game.Presentation.Tablero3D.SelloSoloSellada", ViewStateTestFlags)
bool FBoardViewSealTest::RunTest(const FString& Parameters)
{
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	const FBoardViewState Unsealed = Soulbound::Presentation::ComputeBoardViewState(State, true);
	// La vista lee bSealed tal cual; quién pone los Sellos es cosa de Board/SealRules.h.
	constexpr int32 SealedTile = 3;
	State.Board.Tiles[SealedTile].bSealed = true;
	State.Board.SealedTiles.Add(SealedTile);
	const FBoardViewState Sealed = Soulbound::Presentation::ComputeBoardViewState(State, true);
	if (!TestEqual(TEXT("Un estado por casilla, sin Sello"), Unsealed.Tiles.Num(), State.Board.Tiles.Num())
		|| !TestEqual(TEXT("Un estado por casilla, con Sello"), Sealed.Tiles.Num(), State.Board.Tiles.Num()))
	{
		return false;
	}
	for (const FTileState& Tile : State.Board.Tiles)
	{
		TestFalse(FString::Printf(TEXT("Sin Sellos, la casilla %d no lleva cadenas"), Tile.TileId), Unsealed.Tiles[Tile.TileId].bSealChains);
		TestEqual(FString::Printf(TEXT("Con la casilla %d sellada, cadenas solo en ella (casilla %d)"), SealedTile, Tile.TileId), Sealed.Tiles[Tile.TileId].bSealChains, Tile.TileId == SealedTile);
	}
	return true;
}

// CA-VIS-5 (R-VIS-5): el trono del Nicho, solo si la vista lo tiene configurado (variante "el Nicho es el trono").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewThroneTest, "Soulbound.Game.Presentation.Tablero3D.TronoSoloConfigurado", ViewStateTestFlags)
bool FBoardViewThroneTest::RunTest(const FString& Parameters)
{
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	const FBoardViewState With = Soulbound::Presentation::ComputeBoardViewState(State, true);
	const FBoardViewState Without = Soulbound::Presentation::ComputeBoardViewState(State, false);
	if (!TestEqual(TEXT("Un estado por casilla, con trono"), With.Tiles.Num(), State.Board.Tiles.Num())
		|| !TestEqual(TEXT("Un estado por casilla, sin trono"), Without.Tiles.Num(), State.Board.Tiles.Num()))
	{
		return false;
	}
	int32 Niches = 0;
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (Tile.Type != ETileType::Niche)
		{
			continue;
		}
		++Niches;
		TestTrue(FString::Printf(TEXT("Con trono configurado, el Nicho %d lo enseña"), Tile.TileId), With.Tiles[Tile.TileId].Structure == EBoardStructure::NicheThrone);
		TestTrue(FString::Printf(TEXT("Sin trono configurado, el Nicho %d no enseña estructura"), Tile.TileId), Without.Tiles[Tile.TileId].Structure == EBoardStructure::None);
	}
	TestEqual(TEXT("Seis Nichos"), Niches, Soulbound::Board::DomainCount);
	return true;
}

// R-VIS-6 (sin criterio propio: "en el Vacío, fuera del tablero, como hoy"): la ficha de un asiento en el Vacío no está en el
// tablero; las demás, sí.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewVoidTest, "Soulbound.Game.Presentation.Tablero3D.FichaFueraEnElVacio", ViewStateTestFlags)
bool FBoardViewVoidTest::RunTest(const FString& Parameters)
{
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	constexpr int32 Banished = 2;
	State.Seats[Banished].TileId = INDEX_NONE;
	const FBoardViewState View = Soulbound::Presentation::ComputeBoardViewState(State, true);
	if (!TestEqual(TEXT("Una ficha por asiento"), View.TokensOnBoard.Num(), State.Seats.Num()))
	{
		return false;
	}
	for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
	{
		TestEqual(FString::Printf(TEXT("La ficha del asiento %d está en el tablero salvo en el Vacío"), Seat), View.TokensOnBoard[Seat], Seat != Banished);
	}
	return true;
}

#endif
