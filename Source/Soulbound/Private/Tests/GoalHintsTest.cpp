// Tests de S-040 (el tablero dice adónde ir): el apagado del suelo de un Dominio Olvidado, las casillas con la luz de la meta y la línea de
// objetivo del panel de estado. Funciones puras, sin mundo, sin actor y sin assets: lo que se ve en pantalla (la textura del suelo, la luz)
// solo tiene criterio manual. Filtros: "Soulbound.Game.Presentation.Tablero3D" y "Soulbound.Game.Presentation.Estado".

#include "../../Presentation/BoardViewState.h"
#include "../../Presentation/StatusText.h"
#include "Board/BoardLoader.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GoalHintsTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	using Soulbound::Presentation::FPanelLine;
	using Soulbound::Presentation::FStatusContext;

	constexpr int32 GhHuman = 0;
	constexpr int32 GhElect = 1;
	/** Dominio del Elegido (el asiento 1) y Dominios Olvidados del estado armado a mano. */
	constexpr int32 GhElectDomain = 2;
	constexpr int32 GhForgottenA = 4;
	constexpr int32 GhForgottenB = 5;
	/** Un color de luz que no se confunde con ningún otro de las líneas. */
	const FLinearColor GhGoalColor(1.f, 0.82f, 0.35f);

	/**
	 * Partida armada a mano sobre tablero_35.json, en la carrera y con el tablero Despierto: el asiento 1 es Elegido del Dominio 2 (su Nicho
	 * reclamado), los Dominios 4 y 5 Olvidados (Nichos cerrados), dos runas de reserva en cada Fuente, y los asientos 0, 2 y 3 Ecos sin
	 * vincular. La vista solo lee el estado, así que no hace falta StartMatch.
	 */
	bool GhMakeState(FAutomationTestBase& Test, FMatchState& OutState)
	{
		const FBoardLoadResult Result = Soulbound::Rules::LoadBoardFromFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Boards/tablero_35.json")));
		if (!Test.TestTrue(FString::Printf(TEXT("tablero_35.json se carga (%s)"), *Result.Message), Result.IsSuccess()))
		{
			return false;
		}
		OutState.Board = Result.Board;
		OutState.Board.Phase = EBoardPhase::Awakened;
		OutState.MatchPhase = EMatchPhase::BondRace;
		for (FTileState& Tile : OutState.Board.Tiles)
		{
			if (Tile.Type == ETileType::PowerSource)
			{
				Tile.RuneReserve = 2;
			}
		}
		for (const int32 Domain : {GhForgottenA, GhForgottenB})
		{
			OutState.Board.Domains[Domain].State = EDomainState::Forgotten;
			OutState.Board.Domains[Domain].NicheState = ENicheState::Closed;
		}
		OutState.Board.Domains[GhElectDomain].NicheState = ENicheState::Claimed;
		OutState.Board.Domains[GhElectDomain].OwnerSeat = GhElect;
		OutState.Seats.SetNum(Soulbound::Match::SeatCount);
		for (FSeatState& Seat : OutState.Seats)
		{
			Seat.TileId = OutState.Board.FindAltar();
		}
		OutState.Seats[GhElect].Role = ESeatRole::Elect;
		OutState.Seats[GhElect].DomainId = GhElectDomain;
		OutState.Seats[GhElect].TileId = OutState.Board.FindNiche(GhElectDomain);
		return true;
	}

	/** Las casillas del tipo Type, en orden ascendente; con Domains no vacío, solo las de esos Dominios. */
	TArray<int32> GhTilesOfType(const FMatchState& State, ETileType Type, const TArray<int32>& Domains = {})
	{
		TArray<int32> Tiles;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			if (Tile.Type == Type && (Domains.IsEmpty() || Domains.Contains(Tile.DomainId)))
			{
				Tiles.Add(Tile.TileId);
			}
		}
		return Tiles;
	}

	FString GhTilesText(const TArray<int32>& Tiles)
	{
		return FString::JoinBy(Tiles, TEXT(", "), [](int32 Tile) { return FString::FromInt(Tile); });
	}

	/** El humano (asiento 0) en su turno de tirada, como en StatusTextTest: tirada 4, quedan 15 s de reloj de turno. */
	void GhHumanRolling(FMatchState& State)
	{
		State.ActiveSeat = GhHuman;
		State.TurnPhase = ETurnPhase::RollAndChoose;
		State.Roll = 4;
		State.ClockSeconds = 25.0;
		State.TurnDeadlineSeconds = 40.0;
		State.ReachableOptions = {1, 2};
	}

	FStatusContext GhContext()
	{
		FStatusContext Context;
		Context.HumanSeat = GhHuman;
		Context.ReturnKey = TEXT("Intro");
		Context.SeatColors = {FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};
		Context.GoalColor = GhGoalColor;
		return Context;
	}

	/** La posición de la línea que empieza por Text, o INDEX_NONE. */
	int32 GhIndexOf(const TArray<FPanelLine>& Lines, const FString& Text)
	{
		return Lines.IndexOfByPredicate([&Text](const FPanelLine& Line) { return Line.Text.StartsWith(Text); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoalFloorDimTest, "Soulbound.Game.Presentation.Tablero3D.ApagadoDelSueloOlvidado", GoalHintsTestFlags)

bool FGoalFloorDimTest::RunTest(const FString& Parameters)
{
	// CA-VIS-28 (R-VIS-27): la función que dice con qué factor se apaga el suelo de una cuña devuelve 1 para un Dominio con dueño; ForgottenFloorDim
	// para un Olvidado con el material propio disponible; y ForgottenDim para un Olvidado sin él. El tinte del Dominio (WedgeFloorTint) se aplica
	// igual en los tres casos.
	using Soulbound::Presentation::WedgeFloorDim;
	using Soulbound::Presentation::WedgeFloorTint;
	constexpr float FloorDim = 0.6f;
	constexpr float TileDim = 0.3f;
	TestEqual(TEXT("Con dueño y con el material: 1"), WedgeFloorDim(false, true, FloorDim, TileDim), 1.f);
	TestEqual(TEXT("Con dueño y sin el material: 1"), WedgeFloorDim(false, false, FloorDim, TileDim), 1.f);
	TestEqual(TEXT("Olvidado con el material: ForgottenFloorDim"), WedgeFloorDim(true, true, FloorDim, TileDim), FloorDim);
	TestEqual(TEXT("Olvidado sin el material: ForgottenDim"), WedgeFloorDim(true, false, FloorDim, TileDim), TileDim);

	const FLinearColor Domain(0.8f, 0.2f, 0.4f);
	constexpr float Strength = 0.35f;
	const FLinearColor Mixed = FLinearColor::White + (Domain - FLinearColor::White) * Strength;
	auto Scaled = [&Mixed](float Factor) { return FLinearColor(Mixed.R * Factor, Mixed.G * Factor, Mixed.B * Factor, Mixed.A); };
	TestTrue(TEXT("Con dueño: el tinte del Dominio, sin apagar"),
		WedgeFloorTint(Domain, false, Strength, WedgeFloorDim(false, true, FloorDim, TileDim)).Equals(Mixed, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Olvidado con el material: el mismo tinte por ForgottenFloorDim"),
		WedgeFloorTint(Domain, true, Strength, WedgeFloorDim(true, true, FloorDim, TileDim)).Equals(Scaled(FloorDim), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Olvidado sin el material: el mismo tinte por ForgottenDim, como antes de S-040"),
		WedgeFloorTint(Domain, true, Strength, WedgeFloorDim(true, false, FloorDim, TileDim)).Equals(Scaled(TileDim), KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoalLightTilesTest, "Soulbound.Game.Presentation.Tablero3D.LuzSoloEnLaMetaDelHumano", GoalHintsTestFlags)

bool FGoalLightTilesTest::RunTest(const FString& Parameters)
{
	// CA-VIS-29 (R-VIS-28): la función que dice qué casillas llevan luz (del estado y del asiento humano) devuelve las de la meta del humano y
	// ninguna de otro asiento; ninguna con la partida terminada; ninguna en el prólogo.
	using Soulbound::Presentation::GoalLightTiles;
	FMatchState State;
	if (!GhMakeState(*this, State))
	{
		return false;
	}
	const TArray<int32> FreeNiches = GhTilesOfType(State, ETileType::Niche, {0, 1, 3});
	const TArray<int32> Sources = GhTilesOfType(State, ETileType::PowerSource);
	if (!TestTrue(TEXT("Hay Nichos Libres y Fuentes"), !FreeNiches.IsEmpty() && !Sources.IsEmpty()))
	{
		return false;
	}

	// El humano es un Eco en la carrera: luz en los Nichos Libres, y en ninguna de las Fuentes que busca el Elegido.
	const TArray<int32> HumanLights = GoalLightTiles(State, GhHuman, false);
	TestEqual(TEXT("Humano Eco en la carrera: luz en los Nichos Libres"), GhTilesText(HumanLights), GhTilesText(FreeNiches));
	for (const int32 Source : Sources)
	{
		TestFalse(FString::Printf(TEXT("Ninguna luz en la Fuente %d, meta de otro asiento"), Source), HumanLights.Contains(Source));
	}

	// Si el humano es el asiento 1, el Elegido: luz en las Fuentes, no en los Nichos.
	TestEqual(TEXT("Humano Elegido: luz en las Fuentes con reserva"), GhTilesText(GoalLightTiles(State, GhElect, false)), GhTilesText(Sources));

	TestTrue(TEXT("En el prólogo, ninguna"), GoalLightTiles(State, GhHuman, true).IsEmpty());
	TestTrue(TEXT("Sin asiento humano, ninguna"), GoalLightTiles(State, INDEX_NONE, false).IsEmpty());
	State.MatchPhase = EMatchPhase::MatchOver;
	TestTrue(TEXT("Con la partida terminada, ninguna"), GoalLightTiles(State, GhHuman, false).IsEmpty());
	TestTrue(TEXT("... tampoco para el Elegido"), GoalLightTiles(State, GhElect, false).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoalLineTest, "Soulbound.Game.Presentation.Estado.LineaDeObjetivo", GoalHintsTestFlags)

bool FGoalLineTest::RunTest(const FString& Parameters)
{
	// CA-HUD-15 (R-HUD-7, con la precisión del 16/11/2026 sobre el prólogo): la línea de objetivo dice, letra por letra, los cuatro textos de la
	// regla en sus cuatro casos, con N igual a las runas distintas del humano; con la meta None, no hay línea; en el prólogo, tampoco.
	using Soulbound::Presentation::GoalLineText;
	using Soulbound::Presentation::StatusLines;
	FMatchState State;
	if (!GhMakeState(*this, State))
	{
		return false;
	}

	// Niche: el humano, Eco sin vincular en la carrera.
	TestEqual(TEXT("Niche"), GoalLineText(State, GhHuman), FString(TEXT("Llega a un Nicho libre")));

	// Runes, Elegido: dos tipos distintos (uno repetido).
	State.Seats[GhElect].Runes[0] = 2;
	State.Seats[GhElect].Runes[3] = 1;
	TestEqual(TEXT("Runes, Elegido"), GoalLineText(State, GhElect), FString(TEXT("Consigue runas en las Fuentes (2/6)")));

	// Runes, Eco: tras la carrera, con un tipo; y sin ninguno.
	State.MatchPhase = EMatchPhase::RuneHunt;
	State.Seats[GhHuman].DomainId = GhForgottenA;
	TestEqual(TEXT("Runes, Eco sin runas"), GoalLineText(State, GhHuman), FString(TEXT("Consigue runas en las Fuentes de los Olvidados, o róbalas (0/6)")));
	State.Seats[GhHuman].Runes[GhForgottenB] = 1;
	TestEqual(TEXT("Runes, Eco con una"), GoalLineText(State, GhHuman), FString(TEXT("Consigue runas en las Fuentes de los Olvidados, o róbalas (1/6)")));

	// Altar: los seis tipos.
	for (int32 RuneType = 0; RuneType < Soulbound::Board::DomainCount; ++RuneType)
	{
		State.Seats[GhElect].Runes[RuneType] = 1;
	}
	TestEqual(TEXT("Altar"), GoalLineText(State, GhElect), FString(TEXT("Vuelve al Altar")));

	// En el panel de estado: tras la petición, con el color de la luz y la letra normal.
	GhHumanRolling(State);
	const FString EchoLine = TEXT("Consigue runas en las Fuentes de los Olvidados, o róbalas (1/6)");
	{
		const TArray<FPanelLine> Lines = StatusLines(State, GhContext());
		const int32 Goal = GhIndexOf(Lines, EchoLine);
		if (TestTrue(TEXT("La línea de objetivo está en el panel de estado"), Goal != INDEX_NONE))
		{
			TestEqual(TEXT("Justo tras la petición, que va la primera"), Goal, 1);
			TestTrue(TEXT("La petición sigue la primera"), Lines[0].Text.Contains(TEXT("tu turno")));
			TestEqual(TEXT("Con el color de la luz"), Lines[Goal].Color, GhGoalColor);
			TestFalse(TEXT("Con la letra normal"), Lines[Goal].bLarge);
		}
	}
	{
		// Con el panel de decisión a la vista, la petición no está aquí: la línea de objetivo va la primera.
		FStatusContext Context = GhContext();
		Context.bDecisionPanelShown = true;
		TestEqual(TEXT("Sin la petición, la primera"), GhIndexOf(StatusLines(State, Context), EchoLine), 0);
	}
	{
		FStatusContext Context = GhContext();
		Context.bPrologue = true;
		TestEqual(TEXT("En el prólogo no sale"), GhIndexOf(StatusLines(State, Context), TEXT("Consigue runas")), INDEX_NONE);
	}

	// None: la partida terminada.
	State.MatchPhase = EMatchPhase::MatchOver;
	TestTrue(TEXT("None: sin texto"), GoalLineText(State, GhHuman).IsEmpty());
	const TArray<FPanelLine> Over = StatusLines(State, GhContext());
	for (const TCHAR* Text : {TEXT("Llega a"), TEXT("Consigue runas"), TEXT("Vuelve al Altar")})
	{
		TestEqual(FString::Printf(TEXT("None: ninguna línea empieza por \"%s\""), Text), GhIndexOf(Over, Text), INDEX_NONE);
	}
	return true;
}

#endif
