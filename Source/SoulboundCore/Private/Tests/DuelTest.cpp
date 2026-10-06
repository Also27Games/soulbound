// Tests del duelo (S-012, GDD 04): el duelo portátil (R-POD-3) y el reto desde un Nodo de Duelo (R-DUE-2) como acción
// contra un jugador (R-INT-1, R-INT-2), el retado que no elige (R-DUE-3), el reto a quien no tiene runas (R-DUE-4), el
// ganador por rendimiento con la ventaja de Shuten-dōji (R-DUE-1, R-ELE-2), el botín con el trilero sea quien sea el ganador
// (R-DUE-5, Koschei incluido), el perdedor sin runas que vuelve a su hogar (R-DUE-6), los turnos que no cambian (R-INT-4), el
// sorteo de los dados que no depende de los duelos y el registro. Cada test cita el criterio de aceptación (CA-XXX-n) y la
// regla (R-XXX-n) que cubre. Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Duel".
//
// Montaje: el de las elecciones (ChoiceFixture.h), tablero de 35 casillas. Los asientos 0 y 1 se vinculan (a Fenrir y a
// Mictlantecuhtli si el test no pide otros) y la carrera termina: los asientos 2 y 3 quedan Ecos con hogar. Las runas, las
// posiciones de las fichas y el efecto de un Nodo se fijan a mano: es lo que el test necesita, no lo que comprueba. Los
// turnos se juegan con la máquina de turnos (AdvanceTurn y las intenciones).

#include "ChoiceFixture.h"
#include "Match/BondRaceRules.h"
#include "Match/DuelRules.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::EventsOfType;
	using Soulbound::Tests::FChoiceFixture;
	using Soulbound::Tests::SameState;

	/** Dominios de Content/Data/Elects/elegidos.json: Shuten-dōji (duelAdvantage), Fenrir, Mictlantecuhtli y Koschei (safeRune). */
	constexpr int32 ShutenDomain = 0;
	constexpr int32 FenrirDomain = 1;
	constexpr int32 MictlanDomain = 3;
	constexpr int32 KoscheiDomain = 5;

	int32 AsInt(EIntentResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EChoiceKind Kind) { return static_cast<int32>(Kind); }
	int32 AsInt(ETurnPhase Phase) { return static_cast<int32>(Phase); }
	int32 AsInt(EShellGameStage Stage) { return static_cast<int32>(Stage); }

	/** Partida tras la carrera: el asiento 0 se vincula al Dominio Domain0 y el 1 al Domain1; el 2 y el 3 quedan Ecos con hogar. */
	bool StartAfterRace(FAutomationTestBase& Test, FChoiceFixture& F, int32 Domain0 = FenrirDomain, int32 Domain1 = MictlanDomain)
	{
		if (!F.Start(Test) || !F.Bond(Test, 0, Domain0) || !F.Bond(Test, 1, Domain1))
		{
			return false;
		}
		if (!Test.TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now, F.Random)))
		{
			return false;
		}
		return Test.TestTrue(TEXT("Los asientos 2 y 3 quedan Ecos con hogar"), F.State.Seats[2].Role == ESeatRole::LostEcho && F.State.Seats[2].DomainId != INDEX_NONE
			&& F.State.Seats[3].Role == ESeatRole::LostEcho && F.State.Seats[3].DomainId != INDEX_NONE);
	}

	/** El primer Nodo de Poder del tablero pasa a ser de Duelo. Devuelve la casilla. */
	int32 MakeDuelNode(FChoiceFixture& F)
	{
		for (FTileState& Tile : F.State.Board.Tiles)
		{
			if (Tile.Type == ETileType::PowerNode)
			{
				Tile.PowerNodeEffect = EPowerNodeEffect::Duel;
				return Tile.TileId;
			}
		}
		return INDEX_NONE;
	}

	/** Casilla normal número Index del tablero: donde se encuentran los asientos en los tests de duelo portátil. */
	int32 Meeting(const FChoiceFixture& F, int32 Index = 0)
	{
		const TArray<int32> Tiles = F.NormalTiles();
		return Tiles.IsValidIndex(Index) ? Tiles[Index] : INDEX_NONE;
	}

	void Place(FChoiceFixture& F, int32 Seat, int32 TileId)
	{
		F.State.Seats[Seat].TileId = TileId;
	}

	/** Pone en el Brazal de Seat las runas Counts (una entrada por tipo, de 0 a 5). */
	void SetRunes(FChoiceFixture& F, int32 Seat, const TArray<int32>& Counts)
	{
		F.State.Seats[Seat].Runes = Counts;
	}

	/** Vacía el Brazal de Seat. */
	void ClearRunes(FChoiceFixture& F, int32 Seat)
	{
		F.State.Seats[Seat].Runes.Init(0, Soulbound::Board::DomainCount);
	}

	/** Objetivos de duelo de la acción contra un jugador abierta, en el orden de las opciones. */
	TArray<int32> OfferedDuels(const FChoiceFixture& F)
	{
		TArray<int32> Targets;
		if (F.State.PendingChoice.Kind == EChoiceKind::PlayerAction)
		{
			for (const FPlayerActionOption& Option : F.State.PendingChoice.ActionOptions)
			{
				if (Option.Action == EPlayerAction::Duel)
				{
					Targets.Add(Option.TargetSeat);
				}
			}
		}
		return Targets;
	}

	/** Seat termina en TileId y reta a Target. Devuelve lo que dijo el motor. */
	EIntentResult LandAndChallenge(FAutomationTestBase& Test, FChoiceFixture& F, int32 Seat, int32 TileId, int32 Target)
	{
		if (!Test.TestTrue(FString::Printf(TEXT("El asiento %d termina en la casilla %d"), Seat, TileId), F.Land(Seat, TileId)))
		{
			return EIntentResult::WrongPhase;
		}
		F.Now += 1.0;
		return F.Act(Seat, EPlayerAction::Duel, Target);
	}

	/** Deja pasar la muestra y la mezcla del trilero (R-ROB-2) hasta la elección de esfera. Devuelve si se llegó a ella. */
	bool PlayToPick(FChoiceFixture& F)
	{
		for (int32 Step = 0; Step < 2; ++Step)
		{
			F.Now = FMath::Max(F.Now, F.State.ShellGame.StageDeadlineSeconds);
			F.Advance();
		}
		return F.State.ShellGame.Stage == EShellGameStage::Pick && F.State.PendingChoice.Kind == EChoiceKind::ShellPick;
	}

	/** Los Brazales de todos los asientos. */
	TArray<TArray<int32>> Bracers(const FMatchState& State)
	{
		TArray<TArray<int32>> Result;
		for (const FSeatState& Seat : State.Seats)
		{
			Result.Add(Seat.Runes);
		}
		return Result;
	}

	FString Join(const TArray<int32>& Values)
	{
		return FString::JoinBy(Values, TEXT(", "), [](int32 Value) { return FString::FromInt(Value); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelAndExpelTest,
	"Soulbound.Core.Duel.DueloYExpulsionALaVez",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelAndExpelTest::RunTest(const FString& Parameters)
{
	// CA-POD-1 (R-POD-3, R-POD-4; R-INT-2): dado un Elegido con Duelo y Expulsión que termina en una casilla con dos
	// jugadores, se le ofrecen las cuatro opciones (duelo o expulsión, con cada uno) y "nada"; elige una sola. Sin el poder
	// Duelo y fuera de un Nodo de Duelo, no se ofrece el duelo.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Tile = Meeting(F);
	F.GivePower(0, EPower::Duel);
	F.GivePower(0, EPower::Expel);
	SetRunes(F, 1, {0, 1, 0, 0, 0, 0});
	Place(F, 1, Tile);
	Place(F, 2, Tile);
	FChoiceFixture WithoutDuel = F;

	TestTrue(TEXT("El Elegido termina con dos jugadores"), F.Land(0, Tile));
	const FPendingChoice& Choice = F.State.PendingChoice;
	const TArray<FPlayerActionOption> Expected = {{EPlayerAction::Duel, 1}, {EPlayerAction::Duel, 2}, {EPlayerAction::Expel, 1}, {EPlayerAction::Expel, 2}};
	TestEqual(TEXT("Se le ofrece la acción contra un jugador"), AsInt(Choice.Kind), AsInt(EChoiceKind::PlayerAction));
	TestTrue(TEXT("... con las cuatro opciones: duelo y expulsión con cada uno, primero los duelos"), Choice.ActionOptions == Expected);
	TestTrue(TEXT("... y \"nada\""), Choice.CanDecline());

	F.Now += 1.0;
	TestEqual(TEXT("Elige el duelo con el asiento 1"), AsInt(F.Act(0, EPlayerAction::Duel, 1)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("No queda ninguna elección abierta"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Una sola acción: una segunda se rechaza"), AsInt(F.Choose(0, EChoiceKind::PlayerAction, 2)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("Empieza el duelo con el asiento 1"), F.State.Duel.Challenger == 0 && F.State.Duel.Challenged == 1);
	F.FinishTurn();
	TestEqual(TEXT("Nadie es expulsado"), EventsOfType(F.State, EMatchEventType::Expelled).Num(), 0);
	TestEqual(TEXT("Hubo un duelo"), EventsOfType(F.State, EMatchEventType::DuelWon).Num(), 1);

	// Sin el poder Duelo y en una casilla normal: solo la expulsión.
	ClearRunes(WithoutDuel, 0);
	WithoutDuel.GivePower(0, EPower::Expel);
	TestTrue(TEXT("Sin Duelo, el Elegido termina con dos jugadores"), WithoutDuel.Land(0, Tile));
	const TArray<FPlayerActionOption> OnlyExpel = {{EPlayerAction::Expel, 1}, {EPlayerAction::Expel, 2}};
	TestTrue(TEXT("Sin el poder Duelo ni Nodo, no se ofrece el duelo"), WithoutDuel.State.PendingChoice.ActionOptions == OnlyExpel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelNodeChallengeTest,
	"Soulbound.Core.Duel.RetoDesdeElNodo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelNodeChallengeTest::RunTest(const FString& Parameters)
{
	// CA-DUE-1 (R-DUE-2): dado un asiento sin el poder Duelo, Elegido o Eco, que termina en un Nodo de Duelo acabada la
	// carrera, se le ofrece retar a cada uno de los otros asientos, estén donde estén. Durante la carrera (aunque el tablero
	// esté Despierto), o pasando por el Nodo sin acabar en él, no.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Node = MakeDuelNode(F);
	if (!TestTrue(TEXT("El tablero tiene un Nodo de Poder"), Node != INDEX_NONE))
	{
		return false;
	}
	Place(F, 1, Meeting(F, 0));
	Place(F, 2, Meeting(F, 1));
	Place(F, 3, Meeting(F, 2));
	ClearRunes(F, 0);
	TestFalse(TEXT("El Elegido no tiene el poder Duelo"), Soulbound::Rules::IsPowerActive(F.State, 0, EPower::Duel));

	// Elegido sin el poder.
	TestTrue(TEXT("El Elegido termina en el Nodo de Duelo"), F.Land(0, Node));
	TestEqual(TEXT("El Elegido puede retar a los otros tres, cada uno en su casilla"), Join(OfferedDuels(F)), FString(TEXT("1, 2, 3")));
	TestTrue(TEXT("El reto sale del Nodo"), Soulbound::Rules::ChallengesFromDuelNode(F.State, 0));
	F.Now += 1.0;
	F.Choose(0, EChoiceKind::PlayerAction, INDEX_NONE);
	F.FinishTurn();

	// Eco, sin el poder: también.
	Place(F, 0, Meeting(F, 3));
	TestTrue(TEXT("El Eco termina en el Nodo de Duelo"), F.Land(2, Node));
	TestEqual(TEXT("El Eco puede retar a los otros tres, estén donde estén"), Join(OfferedDuels(F)), FString(TEXT("0, 1, 3")));
	F.FinishTurn();

	// Acabando en otra casilla: una normal al lado del Nodo, sin nadie.
	Place(F, 2, Meeting(F, 1));
	const int32* BesideTile = F.State.Board.Tiles[Node].Neighbours.FindByPredicate([&F](int32 TileId)
	{
		return F.State.Board.Tiles[TileId].Type == ETileType::Normal && !F.State.IsTileOccupied(TileId);
	});
	const int32 Beside = BesideTile ? *BesideTile : INDEX_NONE;
	TestTrue(TEXT("El Elegido termina junto al Nodo"), F.Land(0, Beside));
	TestTrue(TEXT("Sin acabar en el Nodo, no hay reto"), Soulbound::Rules::DuelTargets(F.State, 0).IsEmpty() && OfferedDuels(F).IsEmpty());
	F.FinishTurn();

	// En el Nodo sin haberse movido este turno (R-RUN-3): tampoco.
	F.State.ActiveSeat = 0;
	Place(F, 0, Node);
	F.State.bActiveSeatMoved = false;
	TestTrue(TEXT("Sin moverse, no hay reto"), Soulbound::Rules::DuelTargets(F.State, 0).IsEmpty());

	// Durante la carrera, con el tablero Despierto: el Nodo es una casilla normal (decisión del autor del 05/10/2026).
	FChoiceFixture Race;
	if (!Race.Start(*this) || !Race.Bond(*this, 0, FenrirDomain))
	{
		return false;
	}
	TestTrue(TEXT("Control: tablero Despierto y carrera en curso"), Race.State.Board.Phase == EBoardPhase::Awakened && Race.State.MatchPhase == EMatchPhase::BondRace);
	const int32 RaceNode = MakeDuelNode(Race);
	Place(Race, 1, Meeting(Race, 0));
	Place(Race, 2, Meeting(Race, 1));
	TestTrue(TEXT("En carrera, el Elegido termina en el Nodo"), Race.Land(0, RaceNode));
	TestTrue(TEXT("En carrera, no hay reto desde el Nodo"), Soulbound::Rules::DuelTargets(Race.State, 0).IsEmpty() && OfferedDuels(Race).IsEmpty());
	Race.FinishTurn();
	TestTrue(TEXT("En carrera, el Eco termina en el Nodo"), Race.Land(3, RaceNode));
	TestTrue(TEXT("... y tampoco reta"), Soulbound::Rules::DuelTargets(Race.State, 3).IsEmpty() && OfferedDuels(Race).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelOccupiedNodeTest,
	"Soulbound.Core.Duel.NodoOcupadoConPoder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelOccupiedNodeTest::RunTest(const FString& Parameters)
{
	// Criterio "Nodo de Duelo ocupado con poder Duelo" (R-POD-3, R-DUE-2, R-INT-2): dado un asiento con el poder en un Nodo
	// de Duelo ocupado, cada objetivo aparece una sola vez como duelo, y hace una sola acción.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Node = MakeDuelNode(F);
	F.GivePower(0, EPower::Duel);
	SetRunes(F, 1, {0, 1, 0, 0, 0, 0});
	Place(F, 1, Node);
	Place(F, 2, Meeting(F, 0));
	Place(F, 3, Meeting(F, 1));
	TestTrue(TEXT("El Elegido con Duelo termina en el Nodo, con el asiento 1"), F.Land(0, Node));
	TestEqual(TEXT("Cada objetivo, una sola vez"), Join(OfferedDuels(F)), FString(TEXT("1, 2, 3")));
	F.Now += 1.0;
	TestEqual(TEXT("Reta al asiento 1"), AsInt(F.Act(0, EPlayerAction::Duel, 1)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("Una sola acción: no queda elección abierta"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Un solo reto en el registro"), EventsOfType(F.State, EMatchEventType::DuelChallenged).Num(), 1);
	TestTrue(TEXT("El duelo es contra el asiento 1"), F.State.Duel.Challenger == 0 && F.State.Duel.Challenged == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelChallengedDoesNotChooseTest,
	"Soulbound.Core.Duel.RetadoNoElige",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelChallengedDoesNotChooseTest::RunTest(const FString& Parameters)
{
	// CA-DUE-6 (R-DUE-3): dado un reto válido, el retado no recibe ninguna elección y el duelo empieza.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Tile = Meeting(F);
	F.GivePower(0, EPower::Duel);
	SetRunes(F, 1, {0, 1, 0, 0, 0, 0});
	Place(F, 1, Tile);
	TestEqual(TEXT("El asiento 0 reta al 1"), AsInt(LandAndChallenge(*this, F, 0, Tile, 1)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("El retado no recibe ninguna elección"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("El duelo empieza: fase Duel"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel));
	TestTrue(TEXT("... con retador y retado"), F.State.Duel.Challenger == 0 && F.State.Duel.Challenged == 1);
	TestTrue(TEXT("... sin rendimientos todavía"), !F.State.Duel.HasPerformed(0) && !F.State.Duel.HasPerformed(1));
	const FMatchState Before = F.State;
	TestFalse(TEXT("La máquina espera los rendimientos"), F.Advance());
	TestTrue(TEXT("... sin cambiar nada"), SameState(Before, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelNoRunesTest,
	"Soulbound.Core.Duel.RetadoSinRunas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelNoRunesTest::RunTest(const FString& Parameters)
{
	// CA-DUE-2 (R-DUE-4): dado un reto a un asiento sin runas, no hay duelo, nada cambia de mano, el registro lleva el
	// evento y el turno termina.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Tile = Meeting(F);
	F.GivePower(0, EPower::Duel);
	ClearRunes(F, 1);
	Place(F, 1, Tile);
	TestTrue(TEXT("El asiento 0 termina con el 1"), F.Land(0, Tile));
	const TArray<TArray<int32>> Before = Bracers(F.State);
	F.Now += 1.0;
	TestEqual(TEXT("Retar a quien no tiene runas: aceptado"), AsInt(F.Act(0, EPlayerAction::Duel, 1)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("No hay duelo"), F.State.Duel.IsActive());
	TestEqual(TEXT("... ni rendimientos que esperar"), EventsOfType(F.State, EMatchEventType::DuelChallenged).Num(), 0);
	TestFalse(TEXT("No hay trilero"), F.State.ShellGame.IsActive());
	TestTrue(TEXT("Nada cambia de mano"), Bracers(F.State) == Before);
	const TArray<FMatchEvent> NoRunes = EventsOfType(F.State, EMatchEventType::DuelNoRunes);
	TestTrue(TEXT("El registro lleva el reto sin runas"), NoRunes.Num() == 1 && NoRunes[0].Seat == 0 && NoRunes[0].OtherSeat == 1);
	TestEqual(TEXT("El turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	const TArray<FMatchEvent> Ended = EventsOfType(F.State, EMatchEventType::TurnEnded);
	TestTrue(TEXT("... el del retador"), Ended.Num() > 0 && Ended.Last().Seat == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelWinnerStealsTest,
	"Soulbound.Core.Duel.GanadorRobaConTrilero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelWinnerStealsTest::RunTest(const FString& Parameters)
{
	// CA-DUE-3 (R-DUE-5): dado un duelo con ganador G y perdedor P con runas, empieza un trilero con G de ladrón y P de
	// víctima, sea quien sea el retador; si G es el retado, la elección de esfera se le pide a él, que no tiene el turno. Al
	// acabar el trilero termina el turno del retador. Ninguno es Shuten-dōji: gana el mayor rendimiento.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Tile = Meeting(F);
	F.GivePower(0, EPower::Duel);
	SetRunes(F, 1, {0, 1, 0, 0, 0, 0});
	Place(F, 1, Tile);
	TestEqual(TEXT("El asiento 0 reta al 1"), AsInt(LandAndChallenge(*this, F, 0, Tile, 1)), AsInt(EIntentResult::Accepted));
	FChoiceFixture ChallengedWins = F;

	// Gana el retador.
	TestEqual(TEXT("Rinde el retador"), AsInt(F.Perform(0, 0.9)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Rinde el retado"), AsInt(F.Perform(1, 0.1)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Gana el retador: trilero"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame));
	TestTrue(TEXT("... con el retador de ladrón y el retado de víctima"), F.State.ShellGame.Thief == 0 && F.State.ShellGame.Victim == 1);
	TestFalse(TEXT("El duelo ha terminado"), F.State.Duel.IsActive());

	// Gana el retado, que rinde primero: los rendimientos llegan en cualquier orden.
	FChoiceFixture& G = ChallengedWins;
	TestEqual(TEXT("Rinde primero el retado"), AsInt(G.Perform(1, 0.9)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Rinde después el retador"), AsInt(G.Perform(0, 0.1)), AsInt(EIntentResult::Accepted));
	TestTrue(TEXT("Gana el retado: trilero con él de ladrón y el retador de víctima"), G.State.ShellGame.Thief == 1 && G.State.ShellGame.Victim == 0);
	if (!TestTrue(TEXT("Llega a la elección de esfera"), PlayToPick(G)))
	{
		return false;
	}
	TestEqual(TEXT("La esfera se le pide al retado"), G.State.PendingChoice.Seat, 1);
	TestEqual(TEXT("... que no tiene el turno"), G.State.ActiveSeat, 0);
	G.Now += 1.0;
	TestEqual(TEXT("El retado elige esfera: aceptado"), AsInt(G.Choose(1, EChoiceKind::ShellPick, 0)), AsInt(EIntentResult::Accepted));
	const TArray<FMatchEvent> Stolen = EventsOfType(G.State, EMatchEventType::RuneStolen);
	TestTrue(TEXT("El retado roba una runa al retador"), Stolen.Num() == 1 && Stolen[0].Seat == 1 && Stolen[0].OtherSeat == 0);
	TestEqual(TEXT("Termina el turno"), AsInt(G.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("... del retador"), EventsOfType(G.State, EMatchEventType::TurnEnded).Last().Seat, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelChallengerWithoutRunesTest,
	"Soulbound.Core.Duel.RetadorSinRunasPierde",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelChallengerWithoutRunesTest::RunTest(const FString& Parameters)
{
	// CA-DUE-4 (R-DUE-6): dado un retador sin runas que pierde (solo puede retar así desde un Nodo de Duelo), está en su
	// hogar, no hay trilero y el registro lleva el evento. Si gana, roba con trilero. El retador es un Eco.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Node = MakeDuelNode(F);
	ClearRunes(F, 2);
	SetRunes(F, 0, {0, 2, 0, 0, 0, 0});
	Place(F, 0, Meeting(F));
	TestEqual(TEXT("El Eco sin runas reta al asiento 0 desde el Nodo"), AsInt(LandAndChallenge(*this, F, 2, Node, 0)), AsInt(EIntentResult::Accepted));
	FChoiceFixture Wins = F;

	F.Perform(2, 0.1);
	F.Perform(0, 0.9);
	const int32 Home = Soulbound::Rules::HomeTile(F.State, 2);
	TestEqual(TEXT("El retador pierde y está en su hogar"), F.State.Seats[2].TileId, Home);
	TestFalse(TEXT("No hay trilero"), F.State.ShellGame.IsActive());
	const TArray<FMatchEvent> SentHome = EventsOfType(F.State, EMatchEventType::DuelLoserSentHome);
	TestTrue(TEXT("El registro lo deja"), SentHome.Num() == 1 && SentHome[0].Seat == 0 && SentHome[0].OtherSeat == 2 && SentHome[0].Value == Home);
	TestEqual(TEXT("El turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));

	Wins.Perform(2, 0.9);
	Wins.Perform(0, 0.1);
	TestTrue(TEXT("Si gana, roba con trilero"), Wins.State.ShellGame.IsActive() && Wins.State.ShellGame.Thief == 2 && Wins.State.ShellGame.Victim == 0);
	TestEqual(TEXT("... y sigue en el Nodo"), Wins.State.Seats[2].TileId, Node);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelShutenAdvantageTest,
	"Soulbound.Core.Duel.VentajaDeShutenDoji",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelShutenAdvantageTest::RunTest(const FString& Parameters)
{
	// CA-ELE-3 (R-ELE-2, R-DUE-1): dados dos rendimientos iguales, gana Shuten-dōji, sea retador o retado; con el rival por
	// delante en menos que el margen de datos, también; por delante en más que el margen, pierde. Entre dos asientos sin la
	// afinidad, gana el mayor rendimiento, y el empate es del retado. Empate tras el margen: Shuten-dōji (decisión del autor
	// del 06/10/2026). El asiento 0 es Shuten-dōji; el 1 y el 2, no.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F, ShutenDomain, MictlanDomain))
	{
		return false;
	}
	const int32 Shuten = 0;
	const int32 Rival = 1;
	const double Margin = F.State.Params.DuelAdvantageMargin;
	TestEqual(TEXT("El margen sale de partida.json: 0,15"), Margin, 0.15);
	auto Winner = [&F](int32 Challenger, int32 Challenged, double ChallengerPerformance, double ChallengedPerformance)
	{
		return Soulbound::Rules::DuelWinner(F.State, Challenger, Challenged, ChallengerPerformance, ChallengedPerformance);
	};
	const double Base = 0.4;

	TestEqual(TEXT("Iguales, Shuten-dōji retador: gana él"), Winner(Shuten, Rival, Base, Base), Shuten);
	TestEqual(TEXT("Iguales, Shuten-dōji retado: gana él"), Winner(Rival, Shuten, Base, Base), Shuten);
	TestEqual(TEXT("Rival por delante en menos del margen, Shuten-dōji retador: gana él"), Winner(Shuten, Rival, Base, Base + Margin / 2.0), Shuten);
	TestEqual(TEXT("... Shuten-dōji retado: gana él"), Winner(Rival, Shuten, Base + Margin / 2.0, Base), Shuten);
	TestEqual(TEXT("Rival por delante justo en el margen: empate, gana Shuten-dōji"), Winner(Rival, Shuten, Base + Margin, Base), Shuten);
	TestEqual(TEXT("Rival por delante en más del margen, Shuten-dōji retador: pierde"), Winner(Shuten, Rival, Base, Base + Margin + 0.05), Rival);
	TestEqual(TEXT("... Shuten-dōji retado: pierde"), Winner(Rival, Shuten, Base + Margin + 0.05, Base), Rival);

	// El margen sale de los datos: con otro, otra frontera.
	F.State.Params.DuelAdvantageMargin = 0.3;
	TestEqual(TEXT("Con margen 0,3, el rival por delante en 0,2 pierde"), Winner(Shuten, Rival, Base, Base + 0.2), Shuten);
	F.State.Params.DuelAdvantageMargin = 0.0;
	TestEqual(TEXT("Con margen 0, iguales: gana Shuten-dōji"), Winner(Rival, Shuten, Base, Base), Shuten);
	F.State.Params.DuelAdvantageMargin = Margin;

	// Sin la afinidad: gana el mayor; el empate es del retado.
	const int32 Echo = 2;
	TestEqual(TEXT("Sin afinidad, gana el mayor (retador)"), Winner(Rival, Echo, 0.7, 0.3), Rival);
	TestEqual(TEXT("Sin afinidad, gana el mayor (retado)"), Winner(Rival, Echo, 0.3, 0.7), Echo);
	TestEqual(TEXT("Sin afinidad, el empate es del retado"), Winner(Rival, Echo, 0.5, 0.5), Echo);
	TestEqual(TEXT("... en los dos sentidos"), Winner(Echo, Rival, 0.5, 0.5), Rival);

	// Por la máquina de turnos: el asiento 1, con Duelo, reta a Shuten-dōji y rinden igual. Gana Shuten-dōji, el retado.
	const int32 Tile = Meeting(F);
	F.GivePower(Rival, EPower::Duel);
	SetRunes(F, Shuten, {0, 1, 0, 0, 0, 0});
	Place(F, Shuten, Tile);
	TestEqual(TEXT("El asiento 1 reta a Shuten-dōji"), AsInt(LandAndChallenge(*this, F, Rival, Tile, Shuten)), AsInt(EIntentResult::Accepted));
	F.Perform(Rival, Base);
	F.Perform(Shuten, Base);
	const TArray<FMatchEvent> Won = EventsOfType(F.State, EMatchEventType::DuelWon);
	TestTrue(TEXT("Gana Shuten-dōji siendo el retado"), Won.Num() == 1 && Won[0].Seat == Shuten && Won[0].Value == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelKoscheiLosesTest,
	"Soulbound.Core.Duel.KoscheiPierde",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelKoscheiLosesTest::RunTest(const FString& Parameters)
{
	// Criterio "Koschei perdedor" (R-DUE-5, R-ROB-5, R-ELE-2): dado Koschei que pierde un duelo con varias runas, aparta una
	// antes del trilero; con una sola, el robo se anula. El asiento 0 es Koschei y el 1, Fenrir con Duelo, el retador.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F, KoscheiDomain, FenrirDomain))
	{
		return false;
	}
	const int32 Koschei = 0;
	const int32 Tile = Meeting(F);
	F.GivePower(1, EPower::Duel);
	Place(F, Koschei, Tile);
	FChoiceFixture OneRune = F;

	SetRunes(F, Koschei, {1, 0, 1, 0, 0, 0});
	LandAndChallenge(*this, F, 1, Tile, Koschei);
	F.Perform(1, 0.9);
	F.Perform(Koschei, 0.1);
	TestEqual(TEXT("Koschei pierde con dos runas: aparta una antes del trilero"), AsInt(F.State.ShellGame.Stage), AsInt(EShellGameStage::SafeRune));
	TestTrue(TEXT("... la elección es suya"), F.State.PendingChoice.Kind == EChoiceKind::SafeRune && F.State.PendingChoice.Seat == Koschei);
	TestEqual(TEXT("... y el ladrón es el ganador"), F.State.ShellGame.Thief, 1);

	SetRunes(OneRune, Koschei, {0, 0, 1, 0, 0, 0});
	LandAndChallenge(*this, OneRune, 1, Tile, Koschei);
	OneRune.Perform(1, 0.9);
	OneRune.Perform(Koschei, 0.1);
	const TArray<FMatchEvent> Cancelled = EventsOfType(OneRune.State, EMatchEventType::StealCancelled);
	TestTrue(TEXT("Con una sola runa, el robo se anula"), Cancelled.Num() == 1 && Cancelled[0].Seat == 1 && Cancelled[0].OtherSeat == Koschei);
	TestFalse(TEXT("... sin trilero"), OneRune.State.ShellGame.IsActive());
	TestEqual(TEXT("... Koschei conserva su runa"), OneRune.State.Seats[Koschei].RuneCountOf(2), 1);
	TestEqual(TEXT("... y el turno termina"), AsInt(OneRune.State.TurnPhase), AsInt(ETurnPhase::End));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelAfterSourceTest,
	"Soulbound.Core.Duel.FuenteAntesQueDuelo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelAfterSourceTest::RunTest(const FString& Parameters)
{
	// CA-INT-1 (R-INT-1): dado un Elegido con Duelo que termina en una Fuente ocupada, primero obtiene la runa y después se le
	// ofrece el duelo. Aquí la runa de esa Fuente es la que le da el Duelo: si la oferta se calculara antes de obtenerla, no
	// habría duelo. El asiento 0 es Fenrir, cuyo Duelo da la runa del Dominio 0.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const TArray<EPower>& Map = F.State.Elects.Elects[FenrirDomain].RuneToPower;
	const int32 DuelRune = Map.IndexOfByKey(EPower::Duel);
	const int32 Source = F.SourceOf(DuelRune);
	ClearRunes(F, 0);
	SetRunes(F, 1, {0, 1, 0, 0, 0, 0});
	Place(F, 1, Source);
	TestFalse(TEXT("Antes de llegar no tiene el poder Duelo"), Soulbound::Rules::IsPowerActive(F.State, 0, EPower::Duel));
	TestTrue(TEXT("El Elegido termina en la Fuente ocupada"), F.Land(0, Source));
	if (F.State.TurnPhase == ETurnPhase::SkillTrial)
	{
		// La Fuente es de un Dominio Olvidado: primero el minijuego, superado.
		Soulbound::Rules::SubmitSkillTrialResult(F.State, 0, true, F.Now);
	}
	const TArray<FMatchEvent> Obtained = EventsOfType(F.State, EMatchEventType::RuneObtained);
	TestTrue(TEXT("Primero obtiene la runa"), Obtained.Num() > 0 && Obtained.Last().Seat == 0 && Obtained.Last().Value == DuelRune);
	TestEqual(TEXT("Después se le ofrece el duelo con el asiento 1"), Join(OfferedDuels(F)), FString(TEXT("1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelTurnsUnchangedTest,
	"Soulbound.Core.Duel.DueloSinTocarTurnos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelTurnsUnchangedTest::RunTest(const FString& Parameters)
{
	// CA-INT-3 (R-INT-4): tras un duelo, los turnos extra y perdidos de los dos no cambian. Con el botín jugado entero, y
	// también con el perdedor enviado a su hogar.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Tile = Meeting(F);
	F.GivePower(0, EPower::Duel);
	SetRunes(F, 1, {0, 1, 0, 0, 0, 0});
	Place(F, 1, Tile);
	for (const int32 Seat : {0, 1, 2})
	{
		F.State.Seats[Seat].ExtraTurns = 1;
		F.State.Seats[Seat].LostTurns = 2;
	}
	FChoiceFixture SentHome = F;

	LandAndChallenge(*this, F, 0, Tile, 1);
	F.Perform(0, 0.9);
	F.Perform(1, 0.1);
	if (!TestTrue(TEXT("Llega a la elección de esfera"), PlayToPick(F)))
	{
		return false;
	}
	F.Now += 1.0;
	F.Choose(0, EChoiceKind::ShellPick, 0);
	TestEqual(TEXT("Hay botín"), EventsOfType(F.State, EMatchEventType::RuneStolen).Num(), 1);
	for (const int32 Seat : {0, 1})
	{
		TestEqual(FString::Printf(TEXT("El asiento %d conserva su turno extra"), Seat), F.State.Seats[Seat].ExtraTurns, 1);
		TestEqual(FString::Printf(TEXT("... el asiento %d, sus turnos perdidos"), Seat), F.State.Seats[Seat].LostTurns, 2);
	}

	// Un Eco sin runas reta desde el Nodo y pierde: vuelve a su hogar con sus turnos intactos.
	const int32 Node = MakeDuelNode(SentHome);
	ClearRunes(SentHome, 2);
	LandAndChallenge(*this, SentHome, 2, Node, 1);
	SentHome.Perform(2, 0.0);
	SentHome.Perform(1, 1.0);
	TestEqual(TEXT("El Eco vuelve a su hogar"), EventsOfType(SentHome.State, EMatchEventType::DuelLoserSentHome).Num(), 1);
	for (const int32 Seat : {1, 2})
	{
		TestEqual(FString::Printf(TEXT("Hogar: el asiento %d conserva su turno extra"), Seat), SentHome.State.Seats[Seat].ExtraTurns, 1);
		TestEqual(FString::Printf(TEXT("Hogar: el asiento %d conserva sus turnos perdidos"), Seat), SentHome.State.Seats[Seat].LostTurns, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelFixedDrawsTest,
	"Soulbound.Core.Duel.SorteoNoDependeDeLosDuelos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelFixedDrawsTest::RunTest(const FString& Parameters)
{
	// Criterio "El sorteo de los dados no depende de los duelos" (como en S-010, consumo fijo de S-009): dadas dos partidas con
	// la misma semilla, una con duelos y otra sin ellos, los valores del sorteo que consume cada turno son los mismos. Las dos
	// copias juegan los mismos turnos con las mismas fichas (antes de que el asiento 0, con Duelo, confirme destino, el
	// asiento 1 se pone en esa casilla y con runas); solo cambia que una reta siempre que puede y la otra nunca. Los duelos los
	// gana el retador y su botín dura tiempo de partida: en la copia que reta el reloj va por delante, y no importa.
	FChoiceFixture Base;
	if (!StartAfterRace(*this, Base))
	{
		return false;
	}
	Base.GivePower(0, EPower::Duel);
	Base.State.TurnPhase = ETurnPhase::BetweenTurns;
	constexpr int32 Turns = 16;
	TArray<TArray<int32>> Seeds;
	TArray<FChoiceFixture> Played;
	for (const bool bDuel : {true, false})
	{
		FChoiceFixture F = Base;
		TArray<int32>& After = Seeds.AddDefaulted_GetRef();
		for (int32 Turn = 0; Turn < Turns; ++Turn)
		{
			F.Now += 1.0;
			F.Advance();
			for (int32 Step = 0; Step < 48 && F.State.TurnPhase != ETurnPhase::BetweenTurns; ++Step)
			{
				const FPendingChoice& Choice = F.State.PendingChoice;
				if (Choice.Kind == EChoiceKind::PlayerAction)
				{
					const int32 Duel = Choice.ActionOptions.IndexOfByPredicate([](const FPlayerActionOption& Option) { return Option.Action == EPlayerAction::Duel; });
					F.Choose(Choice.Seat, Choice.Kind, bDuel ? Duel : INDEX_NONE);
				}
				else if (Choice.IsOpen())
				{
					F.Choose(Choice.Seat, Choice.Kind, Choice.CanDecline() ? INDEX_NONE : Choice.AnswerAt(0));
				}
				else if (F.State.TurnPhase == ETurnPhase::Duel)
				{
					F.Perform(F.State.Duel.Challenger, 1.0);
					F.Perform(F.State.Duel.Challenged, 0.0);
				}
				else if (F.State.TurnPhase == ETurnPhase::RollAndChoose && !F.State.ReachableOptions.IsEmpty())
				{
					const int32 Destination = F.State.ReachableOptions[0];
					if (F.State.ActiveSeat == 0)
					{
						F.State.Seats[1].TileId = Destination;
						F.State.Seats[1].Runes[1] = FMath::Max(F.State.Seats[1].Runes[1], 2);
					}
					Soulbound::Rules::SubmitDestinationIntent(F.State, F.State.ActiveSeat, Destination, F.Now);
				}
				else if (F.State.TurnPhase == ETurnPhase::RollAndChoose)
				{
					F.Now = F.State.TurnDeadlineSeconds;
				}
				else if (F.State.TurnPhase == ETurnPhase::SkillTrial)
				{
					Soulbound::Rules::SubmitSkillTrialResult(F.State, F.State.ActiveSeat, false, F.Now);
				}
				else if (F.State.ShellGame.IsActive())
				{
					F.Now = FMath::Max(F.Now, F.State.ShellGame.StageDeadlineSeconds);
				}
				F.Advance();
			}
			After.Add(F.Random.GetCurrentSeed());
		}
		Played.Add(F);
	}

	TestTrue(TEXT("La copia que reta tuvo algún duelo"), EventsOfType(Played[0].State, EMatchEventType::DuelWon).Num() > 0);
	TestEqual(TEXT("La otra no tuvo ninguno"), EventsOfType(Played[1].State, EMatchEventType::DuelChallenged).Num(), 0);
	TestEqual(TEXT("Las dos jugaron los mismos turnos"), Played[0].State.TurnCount, Played[1].State.TurnCount);
	for (int32 Turn = 0; Turn < Turns; ++Turn)
	{
		if (Seeds[0][Turn] != Seeds[1][Turn])
		{
			AddError(FString::Printf(TEXT("Tras el turno %d el sorteo está en otro punto con duelos que sin ellos"), Turn + 1));
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelEventLogTest,
	"Soulbound.Core.Duel.RegistroDelDuelo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelEventLogTest::RunTest(const FString& Parameters)
{
	// Criterio "Registro" (R-TUR-15): los eventos nuevos, con los dos asientos, dato y marca de tiempo: reto (con su origen),
	// reto a quien no tiene runas, resultado del duelo y perdedor enviado a su hogar. Seat es quien actúa (retador, ganador),
	// OtherSeat el afectado y Value el dato; nunca un asiento en Value.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	auto Expect = [this, &F](EMatchEventType Type, int32 Index, const TCHAR* Name, int32 Seat, int32 OtherSeat, int32 Value, double At)
	{
		const TArray<FMatchEvent> Events = EventsOfType(F.State, Type);
		if (TestTrue(FString::Printf(TEXT("%s: está en el registro"), Name), Events.IsValidIndex(Index)))
		{
			TestEqual(FString::Printf(TEXT("%s: asiento"), Name), Events[Index].Seat, Seat);
			TestEqual(FString::Printf(TEXT("%s: segundo asiento"), Name), Events[Index].OtherSeat, OtherSeat);
			TestEqual(FString::Printf(TEXT("%s: dato"), Name), Events[Index].Value, Value);
			TestEqual(FString::Printf(TEXT("%s: marca de tiempo"), Name), Events[Index].ClockSeconds, At);
		}
	};
	const int32 Tile = Meeting(F);
	const int32 Node = MakeDuelNode(F);
	F.GivePower(0, EPower::Duel);
	SetRunes(F, 1, {0, 3, 0, 0, 0, 0});

	// Duelo portátil que gana el retador.
	Place(F, 1, Tile);
	LandAndChallenge(*this, F, 0, Tile, 1);
	Expect(EMatchEventType::DuelChallenged, 0, TEXT("DuelChallenged (poder)"), 0, 1, 0, F.Now);
	F.Now += 1.0;
	F.Perform(0, 0.8);
	F.Now += 1.0;
	F.Perform(1, 0.2);
	Expect(EMatchEventType::DuelWon, 0, TEXT("DuelWon (retador)"), 0, 1, 0, F.Now);
	F.FinishTurn();

	// Reto a quien no tiene runas: el asiento 3, Eco.
	ClearRunes(F, 3);
	Place(F, 3, Meeting(F, 1));
	LandAndChallenge(*this, F, 0, Meeting(F, 1), 3);
	Expect(EMatchEventType::DuelNoRunes, 0, TEXT("DuelNoRunes"), 0, 3, INDEX_NONE, F.Now);
	F.FinishTurn();

	// Desde el Nodo con el poder además: el origen es el Nodo. Lo gana el retado.
	Place(F, 1, Node);
	LandAndChallenge(*this, F, 0, Node, 1);
	Expect(EMatchEventType::DuelChallenged, 1, TEXT("DuelChallenged (Nodo con poder)"), 0, 1, 1, F.Now);
	F.Perform(1, 0.9);
	F.Perform(0, 0.0);
	Expect(EMatchEventType::DuelWon, 1, TEXT("DuelWon (retado)"), 1, 0, 1, F.Now);
	F.FinishTurn();

	// Eco sin runas que reta desde el Nodo y pierde: vuelve a su hogar.
	ClearRunes(F, 2);
	Place(F, 1, Meeting(F, 2));
	Place(F, 0, Meeting(F, 3));
	LandAndChallenge(*this, F, 2, Node, 1);
	Expect(EMatchEventType::DuelChallenged, 2, TEXT("DuelChallenged (Nodo sin poder)"), 2, 1, 1, F.Now);
	F.Now += 1.0;
	F.Perform(2, 0.1);
	F.Perform(1, 0.6);
	Expect(EMatchEventType::DuelWon, 2, TEXT("DuelWon (Eco pierde)"), 1, 2, 1, F.Now);
	Expect(EMatchEventType::DuelLoserSentHome, 0, TEXT("DuelLoserSentHome"), 1, 2, Soulbound::Rules::HomeTile(F.State, 2), F.Now);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelInvalidPerformanceTest,
	"Soulbound.Core.Duel.RendimientoInvalido",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDuelInvalidPerformanceTest::RunTest(const FString& Parameters)
{
	// Glosario "Intención" (R-DUE-1): el rendimiento se valida y, si no vale, se descarta sin tocar el estado: sin duelo
	// (WrongPhase), de quien no participa (WrongSeat), fuera de 0 a 1 (InvalidOption) o repetido (WrongPhase). 0 y 1 valen.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	FMatchState Before = F.State;
	TestEqual(TEXT("Sin duelo: WrongPhase"), AsInt(F.Perform(0, 0.5)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("... sin tocar el estado"), SameState(Before, F.State));

	const int32 Tile = Meeting(F);
	F.GivePower(0, EPower::Duel);
	SetRunes(F, 1, {0, 1, 0, 0, 0, 0});
	Place(F, 1, Tile);
	LandAndChallenge(*this, F, 0, Tile, 1);
	Before = F.State;
	TestEqual(TEXT("Quien no participa: WrongSeat"), AsInt(F.Perform(3, 0.5)), AsInt(EIntentResult::WrongSeat));
	TestEqual(TEXT("Mayor que 1: InvalidOption"), AsInt(F.Perform(0, 1.5)), AsInt(EIntentResult::InvalidOption));
	TestEqual(TEXT("Negativo: InvalidOption"), AsInt(F.Perform(0, -0.1)), AsInt(EIntentResult::InvalidOption));
	TestTrue(TEXT("Los rechazos no tocan el estado"), SameState(Before, F.State));
	TestEqual(TEXT("1 vale"), AsInt(F.Perform(0, 1.0)), AsInt(EIntentResult::Accepted));
	Before = F.State;
	TestEqual(TEXT("Repetido: WrongPhase"), AsInt(F.Perform(0, 0.5)), AsInt(EIntentResult::WrongPhase));
	TestTrue(TEXT("... sin tocar el estado"), SameState(Before, F.State));
	TestEqual(TEXT("0 vale"), AsInt(F.Perform(1, 0.0)), AsInt(EIntentResult::Accepted));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
