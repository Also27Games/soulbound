// Tests de S-040 (se sabe adónde ir): el apagado del suelo de un Dominio Olvidado y la línea de objetivo del panel de estado. Funciones puras,
// sin mundo, sin actor y sin assets: lo que se ve en pantalla (la textura del suelo) solo tiene criterio manual. La luz de la meta (R-VIS-28,
// CA-VIS-29) se retiró tras las pruebas en PIE del 16/11/2026, con su test. Filtros: "Soulbound.Game.Presentation.Tablero3D" y "Soulbound.Game.Presentation.Estado".

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
	/** Un color de línea de objetivo que no se confunde con ningún otro de las líneas. */
	const FLinearColor GhGoalLineColor(1.f, 0.82f, 0.35f);
	/** El color de la línea del Warden (S-042), distinto del de la línea de objetivo. */
	const FLinearColor GhWardenLineColor(1.f, 0.45f, 0.05f);

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
		Context.SeatColors = {FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};
		Context.GoalLineColor = GhGoalLineColor;
		Context.WardenLineColor = GhWardenLineColor;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoalLineTest, "Soulbound.Game.Presentation.Estado.LineaDeObjetivo", GoalHintsTestFlags)

bool FGoalLineTest::RunTest(const FString& Parameters)
{
	// CA-HUD-15 (R-HUD-7, con la precisión del 16/11/2026 sobre el prólogo y la corrección del mismo día tras PIE): la línea de objetivo dice, letra
	// por letra, sus textos en sus casos, con N igual a las runas distintas del humano, y el del aspirante en el Altar; va en su caja, con la letra
	// grande y su color, y no en el panel de estado; con la meta None no hay línea; en el prólogo, tampoco.
	using Soulbound::Presentation::GoalLineText;
	using Soulbound::Presentation::GoalPanelLines;
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

	// Altar: los seis tipos, fuera del Altar (el Elegido está en su Nicho).
	for (int32 RuneType = 0; RuneType < Soulbound::Board::DomainCount; ++RuneType)
	{
		State.Seats[GhElect].Runes[RuneType] = 1;
	}
	TestEqual(TEXT("Altar, fuera de él"), GoalLineText(State, GhElect), FString(TEXT("Vuelve al Altar")));

	// Aspirante (corrección del 16/11/2026): en el Altar con los seis tipos (IsAspirant). SeatGoal no cambia: sigue siendo Altar.
	{
		FMatchState Aspirant = State;
		Aspirant.Seats[GhElect].TileId = Aspirant.Board.FindAltar();
		TestEqual(TEXT("Aspirante: en el Altar con los seis"), GoalLineText(Aspirant, GhElect), FString(TEXT("Supera el desafío final en tu próximo turno")));
	}

	// La caja de la línea de objetivo (corrección del 16/11/2026): una sola línea, grande y con su color; el panel de estado ya no la lleva.
	GhHumanRolling(State);
	const FString EchoLine = TEXT("Consigue runas en las Fuentes de los Olvidados, o róbalas (1/6)");
	{
		const TArray<FPanelLine> Box = GoalPanelLines(State, GhContext());
		if (TestEqual(TEXT("La caja lleva una línea"), Box.Num(), 1))
		{
			TestEqual(TEXT("El texto de GoalLineText"), Box[0].Text, EchoLine);
			TestEqual(TEXT("Con el color de la línea de objetivo"), Box[0].Color, GhGoalLineColor);
			TestTrue(TEXT("Con la letra grande"), Box[0].bLarge);
		}
		const TArray<FPanelLine> Lines = StatusLines(State, GhContext());
		TestEqual(TEXT("El panel de estado no la lleva"), GhIndexOf(Lines, TEXT("Consigue runas")), INDEX_NONE);
		TestTrue(TEXT("La petición sigue la primera del panel de estado"), Lines.Num() > 0 && Lines[0].Text.Contains(TEXT("tu turno")));
	}
	{
		FStatusContext Context = GhContext();
		Context.bPrologue = true;
		TestTrue(TEXT("En el prólogo, la caja no tiene línea"), GoalPanelLines(State, Context).IsEmpty());
	}

	// None: la partida terminada.
	State.MatchPhase = EMatchPhase::MatchOver;
	TestTrue(TEXT("None: sin texto"), GoalLineText(State, GhHuman).IsEmpty());
	TestTrue(TEXT("None: la caja no tiene línea"), GoalPanelLines(State, GhContext()).IsEmpty());
	const TArray<FPanelLine> Over = StatusLines(State, GhContext());
	for (const TCHAR* Text : {TEXT("Llega a"), TEXT("Consigue runas"), TEXT("Vuelve al Altar"), TEXT("Supera el desafío")})
	{
		TestEqual(FString::Printf(TEXT("None: ninguna línea empieza por \"%s\""), Text), GhIndexOf(Over, Text), INDEX_NONE);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenTrialReminderTest, "Soulbound.Game.Presentation.Estado.RecordatorioDeLaPrueba", GoalHintsTestFlags)

bool FWardenTrialReminderTest::RunTest(const FString& Parameters)
{
	// CA-HUD-17 (R-HUD-9): con la Prueba del Warden pendiente para el humano, la caja del objetivo lleva la línea del objetivo y, debajo, "Te espera la
	// Prueba del Warden" con el color del Warden (y la letra grande, decisión del autor del 16/11/2026); sin Prueba pendiente, solo la del objetivo.
	// Con la Prueba pendiente para otro asiento, la caja del humano no cambia. Se quita al abrirse el anillo de la Prueba (decisión del 16/11/2026):
	// el turno del humano en la fase de la prueba con la Prueba del Warden. En el prólogo, la caja no tiene línea, como siempre.
	using Soulbound::Presentation::GoalPanelLines;
	FMatchState State;
	if (!GhMakeState(*this, State))
	{
		return false;
	}
	State.ActiveSeat = GhElect; // espera su turno: juega otro
	State.TurnPhase = ETurnPhase::RollAndChoose;
	const TArray<FPanelLine> Without = GoalPanelLines(State, GhContext());
	TestEqual(TEXT("Sin Prueba pendiente, solo la línea del objetivo"), Without.Num(), 1);

	State.Seats[GhHuman].bWardenTrialPending = true;
	const TArray<FPanelLine> With = GoalPanelLines(State, GhContext());
	if (TestEqual(TEXT("Con la Prueba pendiente, dos líneas"), With.Num(), 2))
	{
		TestEqual(TEXT("La primera, la del objetivo"), With[0].Text, FString(TEXT("Llega a un Nicho libre")));
		TestEqual(TEXT("La primera, con su color"), With[0].Color, GhGoalLineColor);
		TestEqual(TEXT("Debajo, el recordatorio"), With[1].Text, FString(TEXT("Te espera la Prueba del Warden")));
		TestEqual(TEXT("Con el color del Warden"), With[1].Color, GhWardenLineColor);
		TestTrue(TEXT("Con la letra grande"), With[1].bLarge);
	}

	// Se quita al abrirse el anillo de la Prueba: su turno, en la fase de la prueba, con la Prueba del Warden.
	{
		FMatchState Playing = State;
		Playing.ActiveSeat = GhHuman;
		Playing.TurnPhase = ETurnPhase::SkillTrial;
		Playing.PendingTrial = ESkillTrial::WardenTrial;
		TestEqual(TEXT("Jugándola, solo la línea del objetivo"), GoalPanelLines(Playing, GhContext()), Without);
	}

	// Pendiente para otro asiento: la caja del humano no cambia.
	{
		FMatchState Other = State;
		Other.Seats[GhHuman].bWardenTrialPending = false;
		Other.Seats[GhElect].bWardenTrialPending = true;
		TestEqual(TEXT("Pendiente para otro asiento, la caja del humano no cambia"), GoalPanelLines(Other, GhContext()), Without);
	}

	FStatusContext Prologue = GhContext();
	Prologue.bPrologue = true;
	TestTrue(TEXT("En el prólogo, ninguna línea"), GoalPanelLines(State, Prologue).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoalPanelShownTest, "Soulbound.Game.Presentation.Estado.CajaDeObjetivoSeVe", GoalHintsTestFlags)

bool FGoalPanelShownTest::RunTest(const FString& Parameters)
{
	// CA-HUD-15 (R-HUD-7, corrección del autor del 16/11/2026 tras PIE): la caja de la línea de objetivo, abajo a la izquierda en la franja de la frase
	// del Warden, se ve con cualquier meta salvo None; deja de verse en el prólogo, con la meta None y mientras hay una frase del Warden en pantalla.
	// Ninguna tecla interviene: la función no recibe ninguna.
	using Soulbound::Presentation::IsGoalPanelShown;
	for (const ESeatGoal Goal : {ESeatGoal::Niche, ESeatGoal::Runes, ESeatGoal::Altar})
	{
		const FString Name = StaticEnum<ESeatGoal>()->GetNameStringByValue(static_cast<int64>(Goal));
		TestTrue(FString::Printf(TEXT("%s, en partida y sin frase: se ve"), *Name), IsGoalPanelShown(false, Goal, false));
		TestFalse(FString::Printf(TEXT("%s, con una frase del Warden en pantalla: no se ve"), *Name), IsGoalPanelShown(false, Goal, true));
		TestFalse(FString::Printf(TEXT("%s, en el prólogo: no se ve"), *Name), IsGoalPanelShown(true, Goal, false));
	}
	TestFalse(TEXT("None: no se ve"), IsGoalPanelShown(false, ESeatGoal::None, false));
	TestFalse(TEXT("None con frase: no se ve"), IsGoalPanelShown(false, ESeatGoal::None, true));
	return true;
}

#endif
