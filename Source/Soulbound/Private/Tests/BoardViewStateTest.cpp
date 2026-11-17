// Tests del estado de vista del tablero (S-019, glosario propuesto "BoardViewState"): qué piezas del kit enseña la vista según
// la partida, calculado del FMatchState sin actor, sin mundo y sin assets. Cada test cita el criterio (CA-VIS-n) y la regla
// (R-VIS-n) que cubre. Filtro: "Soulbound.Game.Presentation.Tablero3D".

#include "../../Presentation/BoardViewState.h"
#include "Board/BoardLoader.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Warden/WardenLines.h"

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

// CA-VIS-26 (R-VIS-24): las casillas con marca son exactamente las de SealedTiles; ninguna sin Sellos, y una deja de estar cuando su Sello se libera.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewSealMarkTest, "Soulbound.Game.Presentation.Tablero3D.MarcaDelSelloSoloEnLasSelladas", ViewStateTestFlags)
bool FBoardViewSealMarkTest::RunTest(const FString& Parameters)
{
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	TestEqual(TEXT("Sin Sellos, ninguna"), Soulbound::Presentation::SealMarkedTiles(State).Num(), 0);
	State.Board.SealedTiles = {12, 7};
	State.Board.Tiles[12].bSealed = true;
	State.Board.Tiles[7].bSealed = true;
	TestEqual(TEXT("Con dos Sellos, esas dos"), Soulbound::Presentation::SealMarkedTiles(State), TArray<int32>({12, 7}));
	State.Board.SealedTiles.Remove(12);
	State.Board.Tiles[12].bSealed = false;
	TestEqual(TEXT("Liberado el de la 12, solo la 7"), Soulbound::Presentation::SealMarkedTiles(State), TArray<int32>({7}));
	return true;
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
	// Texto de S-022 (R-CAM-1): la línea de teclas nombra la vista actual, sentada o cenital, y la tecla que cambia a la otra. Desde S-027,
	// T oculta el panel de estado, G abre el Grimorio y D el panel de depuración (decisión del autor del 9/11/2026: D en vez de F1).
	TestEqual(TEXT("Sentada"), Soulbound::Presentation::ViewKeysText(true, TEXT("C"), TEXT("L"), TEXT("H"), TEXT("T"), TEXT("G"), TEXT("D")),
		FString(TEXT("vista sentada (C: cenital) | L etiquetas | H paneles | T panel de estado | G Grimorio | D depuración")));
	TestEqual(TEXT("Cenital"), Soulbound::Presentation::ViewKeysText(false, TEXT("C"), TEXT("L"), TEXT("H"), TEXT("T"), TEXT("G"), TEXT("D")),
		FString(TEXT("vista cenital (C: sentada) | L etiquetas | H paneles | T panel de estado | G Grimorio | D depuración")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuMapTest, "Soulbound.Game.Presentation.Vista.MapaDelMenu", ViewStateTestFlags)

bool FMenuMapTest::RunTest(const FString& Parameters)
{
	// CA-MEN-1 (R-MEN-1): la función que elige el mapa devuelve ReturnToMenuMap si tiene valor y, si está vacío, GameDefaultMap. Si tiene valor
	// pero el mapa no existe, también GameDefaultMap: el juego nunca se queda sin salida.
	const FString MainMenu = TEXT("/Game/UltraGameTemplate/Maps/UGT_MainMenuMap.UGT_MainMenuMap");
	const FString Launcher = TEXT("/Game/UltraGameTemplate/Maps/UGT_LauncherMap.UGT_LauncherMap");
	TestEqual(TEXT("Con valor: el menú principal"), Soulbound::Presentation::MenuMapPackage(MainMenu, true, Launcher),
		FString(TEXT("/Game/UltraGameTemplate/Maps/UGT_MainMenuMap")));
	TestEqual(TEXT("Vacío: el de arranque"), Soulbound::Presentation::MenuMapPackage(FString(), false, Launcher),
		FString(TEXT("/Game/UltraGameTemplate/Maps/UGT_LauncherMap")));
	TestEqual(TEXT("Con valor que no existe: el de arranque"), Soulbound::Presentation::MenuMapPackage(MainMenu, false, Launcher),
		FString(TEXT("/Game/UltraGameTemplate/Maps/UGT_LauncherMap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchResultTextTest, "Soulbound.Game.Presentation.Vista.ResultadoDiceVolverAlMenu", ViewStateTestFlags)

bool FMatchResultTextTest::RunTest(const FString& Parameters)
{
	// Texto de S-022: la pantalla del resultado (S-018, R-PRO-4) dice quién gana (decisión del autor del 29/10/2026 para la tecla: Escape es la
	// pausa de UGT y en PIE lo captura el editor). CA-HUD-16 (R-HUD-8, añadido del autor del 16/11/2026 en S-041): el resultado va encima del botón,
	// en una línea, y la tecla debajo, en pequeño: "o pulsa Intro". Sin final, nada.
	FMatchState State;
	TestTrue(TEXT("Partida en curso: sin resultado"), Soulbound::Presentation::MatchResultText(State).IsEmpty());
	State.MatchPhase = EMatchPhase::MatchOver;
	State.Outcome = EMatchOutcome::PlayerVictory;
	State.WinnerSeat = 2;
	TestEqual(TEXT("Victoria de un jugador"), Soulbound::Presentation::MatchResultText(State), FString(TEXT("GANA EL ASIENTO 2")));
	State.Outcome = EMatchOutcome::WardenVictory;
	State.WinnerSeat = INDEX_NONE;
	State.EndReason = EMatchEndReason::TimeOut;
	TestEqual(TEXT("Victoria del Warden por tiempo"), Soulbound::Presentation::MatchResultText(State), FString(TEXT("GANA EL WARDEN: SE ACABÓ EL TIEMPO")));
	State.EndReason = EMatchEndReason::NobodyBonded;
	TestEqual(TEXT("Victoria del Warden: nadie se vinculó"), Soulbound::Presentation::MatchResultText(State), FString(TEXT("GANA EL WARDEN: NADIE SE VINCULÓ")));

	// La tecla, con su nombre en español: Unreal llama "Enter" a Intro.
	TestEqual(TEXT("Intro"), Soulbound::Presentation::ReturnToMenuHint(TEXT("Enter")), FString(TEXT("o pulsa Intro")));
	TestEqual(TEXT("Otra tecla, con su nombre"), Soulbound::Presentation::ReturnToMenuHint(TEXT("F5")), FString(TEXT("o pulsa F5")));
	TestTrue(TEXT("Sin tecla, nada"), Soulbound::Presentation::ReturnToMenuHint(FString()).IsEmpty());
	return true;
}

// CA-DOM-1 (S-023, R-PRO-1, R-WAR-10): con el prólogo en marcha y ninguna corrupción enseñada, ningún Dominio se pinta Olvidado; enseñada
// la primera, el Dominio de su DomainForgotten; enseñadas las dos, los dos. Terminado el prólogo, los del estado, pase lo que pase con
// las líneas. Fuera del prólogo un Dominio se apaga cuando el estado lo dice.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewForgottenOpeningTest, "Soulbound.Game.Presentation.Tablero3D.DominiosEnLaApertura", ViewStateTestFlags)
bool FBoardViewForgottenOpeningTest::RunTest(const FString& Parameters)
{
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	// La apertura como la deja StartMatch: inicio, y por cada Dominio Olvidado (el 4 y el 1, en ese orden a propósito) su
	// DomainForgotten y su corrupción; luego el orden y el comienzo. Las frases son las de la tabla; aquí basta la clave.
	auto AddEntry = [&State](const FString& Key)
	{
		FWardenLineEntry& Entry = State.WardenLines.Entries.AddDefaulted_GetRef();
		Entry.Id = Key;
		Entry.Lines = {Key};
		return State.WardenLines.Entries.Num() - 1;
	};
	auto AddEvent = [&State](EMatchEventType Type, int32 Value)
	{
		FMatchEvent& Event = State.EventLog.AddDefaulted_GetRef();
		Event.Type = Type;
		Event.Value = Value;
		Event.Detail = 0;
	};
	const int32 Start = AddEntry(Soulbound::Warden::LineStart);
	const int32 Corruption = AddEntry(Soulbound::Warden::LineCorruption);
	const int32 Begin = AddEntry(Soulbound::Warden::LineBegin);
	const int32 Forgotten[] = {4, 1};
	for (const int32 Domain : Forgotten)
	{
		State.Board.Domains[Domain].State = EDomainState::Forgotten;
	}
	AddEvent(EMatchEventType::WardenSpoke, Start);
	for (const int32 Domain : Forgotten)
	{
		AddEvent(EMatchEventType::DomainForgotten, Domain);
		AddEvent(EMatchEventType::WardenSpoke, Corruption);
	}
	AddEvent(EMatchEventType::WardenSpoke, Begin);

	auto ShownForgotten = [&State](bool bInPrologue, int32 LinesShown)
	{
		const TArray<bool> Shown = Soulbound::Presentation::ForgottenDomainsShown(State, bInPrologue, LinesShown);
		TArray<int32> Domains;
		for (int32 Domain = 0; Domain < Shown.Num(); ++Domain)
		{
			if (Shown[Domain])
			{
				Domains.Add(Domain);
			}
		}
		return Domains;
	};
	TestEqual(TEXT("Una entrada por Dominio"), Soulbound::Presentation::ForgottenDomainsShown(State, true, 0).Num(), State.Board.Domains.Num());
	TestEqual(TEXT("Prólogo, nada enseñado: ninguno"), ShownForgotten(true, 0), TArray<int32>());
	TestEqual(TEXT("Prólogo, enseñado el inicio: ninguno"), ShownForgotten(true, 1), TArray<int32>());
	TestEqual(TEXT("Prólogo, enseñada la primera corrupción: el de su DomainForgotten"), ShownForgotten(true, 2), TArray<int32>({4}));
	TestEqual(TEXT("Prólogo, enseñadas las dos: los dos"), ShownForgotten(true, 3), TArray<int32>({1, 4}));
	TestEqual(TEXT("Prólogo, enseñado el comienzo: los dos"), ShownForgotten(true, 4), TArray<int32>({1, 4}));
	TestEqual(TEXT("Terminado el prólogo: los del estado, aunque no se haya enseñado nada"), ShownForgotten(false, 0), TArray<int32>({1, 4}));

	// Fuera del prólogo manda el estado: un Dominio que se olvida después se apaga sin frase.
	State.Board.Domains[0].State = EDomainState::Forgotten;
	TestEqual(TEXT("Fuera del prólogo, el estado"), ShownForgotten(false, 4), TArray<int32>({0, 1, 4}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardViewEchoNightDimTest, "Soulbound.Game.Presentation.Tablero3D.PenumbraDeLaNoche", ViewStateTestFlags)

bool FBoardViewEchoNightDimTest::RunTest(const FString& Parameters)
{
	// CA-VIS-32 (R-VIS-31, S-043): la opacidad de la penumbra vale 0 sin Noche, sube hasta la configurada al cumplirse la transición desde que
	// empieza y baja hasta 0 al cumplirse desde que termina. La combinación con el pulso del corte (S-041) da el mayor de los dos, no la suma, con
	// el color del que gana. Con la partida terminada no hay penumbra (decisión del autor del 17/11/2026: se va con su transición). Con una
	// transición de 0,5 s (números exactos en binario; el valor inicial es 0,8 s) y una opacidad de 0,25 (el valor inicial es 0,3).
	using Soulbound::Presentation::EchoNightDimOpacity;
	constexpr float Max = 0.25f;
	constexpr double Fade = 0.5;

	TestEqual(TEXT("Sin Noche y sin cambio, 0"), EchoNightDimOpacity(false, 0.f, 100.0, Max, Fade), 0.f, 1e-6f);
	TestEqual(TEXT("Al empezar, aún 0"), EchoNightDimOpacity(true, 0.f, 0.0, Max, Fade), 0.f, 1e-6f);
	TestEqual(TEXT("A mitad de la transición, la mitad"), EchoNightDimOpacity(true, 0.f, 0.25, Max, Fade), 0.125f, 1e-6f);
	TestEqual(TEXT("Al cumplirse, la configurada"), EchoNightDimOpacity(true, 0.f, 0.5, Max, Fade), Max, 1e-6f);
	TestEqual(TEXT("Y se queda"), EchoNightDimOpacity(true, 0.f, 30.0, Max, Fade), Max, 1e-6f);
	TestEqual(TEXT("Al terminar, aún la configurada"), EchoNightDimOpacity(false, Max, 0.0, Max, Fade), Max, 1e-6f);
	TestEqual(TEXT("A mitad, la mitad"), EchoNightDimOpacity(false, Max, 0.25, Max, Fade), 0.125f, 1e-6f);
	TestEqual(TEXT("Al cumplirse, 0"), EchoNightDimOpacity(false, Max, 0.5, Max, Fade), 0.f, 1e-6f);
	// Si la Noche termina mientras aún sube, baja desde donde iba, sin saltar.
	TestEqual(TEXT("Termina a medio subir: baja desde ahí"), EchoNightDimOpacity(false, 0.125f, 0.0, Max, Fade), 0.125f, 1e-6f);
	TestEqual(TEXT("Y llega a 0 en lo que le queda"), EchoNightDimOpacity(false, 0.125f, 0.25, Max, Fade), 0.f, 1e-6f);
	TestEqual(TEXT("Sin transición, de golpe"), EchoNightDimOpacity(true, 0.f, 0.0, Max, 0.0), Max, 1e-6f);

	// El mayor de los dos, con el color del que gana: el negro del pulso o el azul de la Noche.
	using Soulbound::Presentation::BoardDimColor;
	const FLinearColor Night(0.02f, 0.04f, 0.15f, 1.f);
	const FLinearColor Pulse = BoardDimColor(Night, 0.3f, 0.85f);
	TestEqual(TEXT("Durante el pulso, la del pulso, no la suma"), Pulse.A, 0.85f, 1e-6f);
	TestTrue(TEXT("Y negro"), Pulse.R == 0.f && Pulse.G == 0.f && Pulse.B == 0.f);
	const FLinearColor Dusk = BoardDimColor(Night, 0.3f, 0.1f);
	TestEqual(TEXT("Al final del pulso, la de la penumbra"), Dusk.A, 0.3f, 1e-6f);
	TestTrue(TEXT("Y azul noche"), Dusk.R == Night.R && Dusk.G == Night.G && Dusk.B == Night.B);
	TestEqual(TEXT("Sin nada, transparente"), BoardDimColor(Night, 0.f, 0.f).A, 0.f, 1e-6f);

	// Cuándo hay penumbra: con la Noche activa y la partida sin terminar.
	using Soulbound::Presentation::EchoNightDimShown;
	FMatchState State;
	if (!MakeState(*this, State))
	{
		return false;
	}
	State.Round = 3;
	TestFalse(TEXT("Sin Noche, no"), EchoNightDimShown(State));
	State.EchoNightUntilRound = 5;
	TestTrue(TEXT("Con la Noche activa, sí"), EchoNightDimShown(State));
	State.MatchPhase = EMatchPhase::MatchOver;
	TestFalse(TEXT("Con la partida terminada, no"), EchoNightDimShown(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTokenArrivalTest, "Soulbound.Game.Presentation.Vista.LlegadaDeLaFicha", ViewStateTestFlags)

bool FTokenArrivalTest::RunTest(const FString& Parameters)
{
	// CA-VIS-33 (R-VIS-32, S-046): cómo llega la ficha a la casilla que dice el estado. "Nada" si ya está en ella; el caminar de siempre si es
	// la siguiente de su camino; deslizarse si no es la siguiente y no está en ella (el corte a medio camino, o el motor dos casillas por
	// delante). TokenTile es la casilla en la que está la ficha, INDEX_NONE si va entre dos.
	using Soulbound::Presentation::ETokenArrival;
	using Soulbound::Presentation::TokenArrival;
	const auto Name = [](ETokenArrival Arrival) { return FString::FromInt(static_cast<int32>(Arrival)); };

	TestEqual(TEXT("Quieta en su casilla: nada"), Name(TokenArrival(5, 5, {})), Name(ETokenArrival::Stay));
	TestEqual(TEXT("En su casilla, a punto de salir: nada"), Name(TokenArrival(5, 5, {6, 7})), Name(ETokenArrival::Stay));
	TestEqual(TEXT("Camino de la 6 y el estado dice 6: caminar"), Name(TokenArrival(6, INDEX_NONE, {6, 7})), Name(ETokenArrival::Walk));
	TestEqual(TEXT("Cortada entre la 5 y la 6, el estado dice 5: deslizarse"), Name(TokenArrival(5, INDEX_NONE, {6, 7})), Name(ETokenArrival::Slide));
	TestEqual(TEXT("El motor dos por delante (dice 7, va a la 6): deslizarse"), Name(TokenArrival(7, INDEX_NONE, {6, 7, 8})), Name(ETokenArrival::Slide));
	TestEqual(TEXT("Quieta en la 5 y el estado dice 3 (hogar, Expulsión): deslizarse"), Name(TokenArrival(3, 5, {})), Name(ETokenArrival::Slide));
	TestEqual(TEXT("En la 5 camino de la 6 y el estado dice 6: caminar"), Name(TokenArrival(6, 5, {6, 7})), Name(ETokenArrival::Walk));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
