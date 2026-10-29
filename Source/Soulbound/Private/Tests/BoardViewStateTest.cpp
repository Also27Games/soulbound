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

// R-VIS-2, R-VIS-5 (decisión del autor del 22/10/2026 sobre S-019): el suelo de la cuña se tiñe con el color de su Dominio, mezclando
// blanco hacia él por Strength, y se apaga por ForgottenDim si el Dominio está Olvidado. Con Strength 0, blanco.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewFloorTintTest, "Soulbound.Game.Presentation.Tablero3D.SueloDeLaCunaTenidoPorDominio", ViewStateTestFlags)
bool FBoardViewFloorTintTest::RunTest(const FString& Parameters)
{
	const FLinearColor Domain(0.8f, 0.2f, 0.4f);
	constexpr float Strength = 0.35f;
	constexpr float Dim = 0.3f;
	const FLinearColor Mixed = FLinearColor::White + (Domain - FLinearColor::White) * Strength;

	const FLinearColor Alive = Soulbound::Presentation::WedgeFloorTint(Domain, false, Strength, Dim);
	TestTrue(TEXT("Sin Olvidar, el blanco mezclado al 35 % con el color del Dominio"), Alive.Equals(Mixed, KINDA_SMALL_NUMBER));

	const FLinearColor Forgotten = Soulbound::Presentation::WedgeFloorTint(Domain, true, Strength, Dim);
	TestTrue(TEXT("Olvidado, lo mismo multiplicado por ForgottenDim"), Forgotten.Equals(FLinearColor(Mixed.R * Dim, Mixed.G * Dim, Mixed.B * Dim, Mixed.A), KINDA_SMALL_NUMBER));

	const FLinearColor Plain = Soulbound::Presentation::WedgeFloorTint(Domain, false, 0.f, Dim);
	TestTrue(TEXT("Con Strength 0, blanco"), Plain.Equals(FLinearColor::White, KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewKeysTextTest, "Soulbound.Game.Presentation.Vista.TeclasNombranLaVista", ViewStateTestFlags)

bool FViewKeysTextTest::RunTest(const FString& Parameters)
{
	// Texto de S-022 (R-CAM-1): la línea de teclas nombra la vista actual, sentada o cenital, y la tecla que cambia a la otra.
	TestEqual(TEXT("Sentada"), Soulbound::Presentation::ViewKeysText(true, TEXT("C"), TEXT("L"), TEXT("H"), TEXT("T")),
		FString(TEXT("vista sentada (C: cenital) | L etiquetas | H paneles | T texto de depuración")));
	TestEqual(TEXT("Cenital"), Soulbound::Presentation::ViewKeysText(false, TEXT("C"), TEXT("L"), TEXT("H"), TEXT("T")),
		FString(TEXT("vista cenital (C: sentada) | L etiquetas | H paneles | T texto de depuración")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchResultTextTest, "Soulbound.Game.Presentation.Vista.ResultadoDiceVolverAlMenu", ViewStateTestFlags)

bool FMatchResultTextTest::RunTest(const FString& Parameters)
{
	// Texto de S-022: la pantalla del resultado (S-018, R-PRO-4) dice quién gana y, debajo, "Escape: volver al menú". Sin final, nada.
	FMatchState State;
	TestTrue(TEXT("Partida en curso: sin resultado"), Soulbound::Presentation::MatchResultText(State, TEXT("Escape")).IsEmpty());
	State.MatchPhase = EMatchPhase::MatchOver;
	State.Outcome = EMatchOutcome::PlayerVictory;
	State.WinnerSeat = 2;
	TestEqual(TEXT("Victoria de un jugador"), Soulbound::Presentation::MatchResultText(State, TEXT("Escape")),
		FString(TEXT("GANA EL ASIENTO 2\nEscape: volver al menú")));
	State.Outcome = EMatchOutcome::WardenVictory;
	State.WinnerSeat = INDEX_NONE;
	State.EndReason = EMatchEndReason::TimeOut;
	TestEqual(TEXT("Victoria del Warden por tiempo"), Soulbound::Presentation::MatchResultText(State, TEXT("Escape")),
		FString(TEXT("GANA EL WARDEN: SE ACABÓ EL TIEMPO\nEscape: volver al menú")));
	State.EndReason = EMatchEndReason::NobodyBonded;
	TestEqual(TEXT("Victoria del Warden: nadie se vinculó"), Soulbound::Presentation::MatchResultText(State, TEXT("Escape")),
		FString(TEXT("GANA EL WARDEN: NADIE SE VINCULÓ\nEscape: volver al menú")));
	return true;
}

#endif
