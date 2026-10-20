// Tests de la carrera por el vínculo (S-006, GDD 03, con las transiciones del tablero del GDD 01): Ecos al inicio,
// Nichos Libres y Cerrados, vínculo y Despertar, fin de carrera por último Nicho y por tiempo, Dominios Olvidados y
// hogares, confinamiento de los Ecos, registro y dummy de carrera. Sobre el tablero de pruebas con los parámetros de
// partida del archivo. Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.BondRace".

#include "Board/MovementRules.h"
#include "Decider/Decider.h"
#include "Match/BondRaceRules.h"
#include "Match/TurnRules.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"
#include "Warden/WardenBrain.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::DomainsWithNiche;
	using Soulbound::Tests::EventsOfType;
	using Soulbound::Tests::LandOn;
	using Soulbound::Tests::StartTestMatch;

	/** Avanza la máquina en NowSeconds hasta que el turno espera destino. Devuelve false si no llega. */
	bool AdvanceToChoice(FMatchState& State, FRandomStream& Random, double NowSeconds)
	{
		for (int32 Step = 0; Step < 8 && State.TurnPhase != ETurnPhase::RollAndChoose; ++Step)
		{
			if (!Soulbound::Rules::AdvanceTurn(State, NowSeconds, Random))
			{
				return false;
			}
		}
		return State.TurnPhase == ETurnPhase::RollAndChoose;
	}

	/** Dominios de las casillas alcanzables desde FromTile con cualquier tirada de 1 a MaxRoll, con los rasgos de Seat. */
	TSet<int32> ReachableDomains(const FMatchState& State, int32 Seat, int32 FromTile, int32 MaxRoll)
	{
		TSet<int32> Domains;
		const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(State, Seat);
		for (int32 Roll = 1; Roll <= MaxRoll; ++Roll)
		{
			for (const int32 Tile : Soulbound::Rules::ReachableTiles(State.Board, FromTile, Roll, Mover))
			{
				Domains.Add(State.Board.Tiles[Tile].DomainId);
			}
		}
		return Domains;
	}

	int32 CountDomains(const FMatchState& State, EDomainState DomainState)
	{
		return State.Board.Domains.FilterByPredicate([DomainState](const FDomainState& Domain) { return Domain.State == DomainState; }).Num();
	}

	/**
	 * Partida con semilla Seed en la que los Elects primeros asientos del orden de turno reclaman, uno cada diez
	 * segundos, los primeros Nichos Libres. Random queda como lo dejan las reglas: es el sorteo de la partida.
	 */
	bool StartWithElects(FAutomationTestBase& Test, int32 Seed, int32 Elects, FRandomStream& Random, FMatchState& State)
	{
		Random.Initialize(Seed);
		if (!StartTestMatch(Test, Random, State))
		{
			return false;
		}
		const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
		for (int32 i = 0; i < Elects; ++i)
		{
			if (!Test.TestTrue(TEXT("El asiento llega al Nicho Libre"), LandOn(State, Random, State.TurnOrder[i], State.Board.FindNiche(Free[i]), 10.0 * (i + 1))))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceAllStartAsEchoesTest,
	"Soulbound.Core.BondRace.TodosEmpiezanComoEcos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceAllStartAsEchoesTest::RunTest(const FString& Parameters)
{
	// CA-CAR-2 (R-CAR-2), con R-TAB-8: dado el inicio, todos los asientos son Eco, están en el Altar, tiran un dado y
	// sus alcanzables incluyen casillas de cualquier Dominio. El tablero empieza Primordial. Semillas fijas 1 a 20.
	TArray<int32> FirstRolls;
	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		FRandomStream Random(Seed);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const int32 Altar = State.Board.FindAltar();
		TestEqual(TEXT("La partida empieza en la carrera"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));
		TestEqual(TEXT("El tablero empieza Primordial (R-TAB-8)"), static_cast<int32>(State.Board.Phase), static_cast<int32>(EBoardPhase::Primordial));
		TestFalse(TEXT("Los Nodos no están revelados"), State.Board.AreNodesRevealed());
		for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
		{
			const FSeatState& SeatState = State.Seats[Seat];
			TestEqual(FString::Printf(TEXT("Semilla %d, asiento %d: Eco Perdido"), Seed, Seat), static_cast<int32>(SeatState.Role), static_cast<int32>(ESeatRole::LostEcho));
			TestEqual(FString::Printf(TEXT("Semilla %d, asiento %d: sin Dominio"), Seed, Seat), SeatState.DomainId, static_cast<int32>(INDEX_NONE));
			TestEqual(FString::Printf(TEXT("Semilla %d, asiento %d: en el Altar"), Seed, Seat), SeatState.TileId, Altar);
			// Transitan por cualquier Dominio, Olvidados incluidos: con tiradas de 1 a 6 desde el Altar se llega a los seis.
			TestEqual(FString::Printf(TEXT("Semilla %d, asiento %d: alcanza casillas de los seis Dominios"), Seed, Seat),
				ReachableDomains(State, Seat, Altar, Soulbound::Dice::Faces).Num(), Soulbound::Board::DomainCount);
		}

		if (!TestTrue(TEXT("El primer turno llega a la tirada"), AdvanceToChoice(State, Random, 0.0)))
		{
			return false;
		}
		FirstRolls.Add(State.Roll);
		TestTrue(TEXT("Las opciones del turno son las de los rasgos del asiento"),
			State.ReachableOptions == Soulbound::Rules::ReachableTiles(State.Board, Altar, State.Roll, Soulbound::Rules::MakeMoverTraits(State, State.ActiveSeat)));
	}
	// Un dado: ninguna tirada pasa de 6, y sale algún 1 (con dos dados el mínimo sería 2).
	TestTrue(TEXT("Ninguna primera tirada pasa de un dado"), FirstRolls.FindByPredicate([](int32 Roll) { return Roll < 1 || Roll > Soulbound::Dice::Faces; }) == nullptr);
	TestTrue(TEXT("Alguna primera tirada es un 1"), FirstRolls.Contains(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceNicheStatesAtStartTest,
	"Soulbound.Core.BondRace.NichosLibresYCerrados",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceNicheStatesAtStartTest::RunTest(const FString& Parameters)
{
	// CA-CAR-3 (R-CAR-3): dado el inicio con N jugadores, hay N Nichos Libres (los de los Dominios no Olvidados) y
	// 6 menos N Cerrados, sin dueño. Lo hace StartMatch desde S-002; aquí se fija. Semillas fijas 1 a 10.
	FBoardState Board;
	FMatchParams Params;
	FElectsTable Elects;
	if (!Soulbound::Tests::LoadTestBoard(*this, Board) || !Soulbound::Tests::LoadTestMatchParams(*this, Params) || !Soulbound::Tests::LoadTestElects(*this, Elects))
	{
		return false;
	}
	for (int32 Players = Soulbound::Match::MinPlayers; Players <= Soulbound::Match::MaxPlayers; ++Players)
	{
		for (int32 Seed = 1; Seed <= 10; ++Seed)
		{
			FRandomStream Random(Seed);
			const FMatchStartResult Start = Soulbound::Rules::StartMatch(Board, Params, Elects, Players, Random);
			if (!TestTrue(TEXT("La partida empieza"), Start.IsSuccess()))
			{
				return false;
			}
			const FString Context = FString::Printf(TEXT("N = %d, semilla %d"), Players, Seed);
			TestEqual(Context + TEXT(": N Nichos Libres"), DomainsWithNiche(Start.State, ENicheState::Free).Num(), Players);
			TestEqual(Context + TEXT(": 6 menos N Cerrados"), DomainsWithNiche(Start.State, ENicheState::Closed).Num(), Soulbound::Board::DomainCount - Players);
			for (const FDomainState& Domain : Start.State.Board.Domains)
			{
				TestEqual(Context + TEXT(": Libre si y solo si no es Olvidado"), Domain.NicheState == ENicheState::Free, Domain.State == EDomainState::Owned);
				TestEqual(Context + TEXT(": sin dueño al empezar"), Domain.OwnerSeat, static_cast<int32>(INDEX_NONE));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceClaimNicheTest,
	"Soulbound.Core.BondRace.ReclamarNicho",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceClaimNicheTest::RunTest(const FString& Parameters)
{
	// CA-CAR-4 y CA-CAR-6 (R-CAR-4, R-CAR-6): dado un Nicho Libre, cuando un Eco termina en él, pasa a Reclamado por
	// ese asiento; cuando otro jugador termina después en él, nada cambia; cuando un Elegido termina en otro Nicho
	// Libre, nada cambia. Terminar en un Nicho Cerrado tampoco cambia nada. Semilla fija 3.
	FRandomStream Random(3);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
	const TArray<int32> Closed = DomainsWithNiche(State, ENicheState::Closed);
	const int32 A = State.TurnOrder[0];
	const int32 B = State.TurnOrder[1];
	const int32 Niche = State.Board.FindNiche(Free[0]);

	if (!TestTrue(TEXT("A termina en el Nicho Libre"), LandOn(State, Random, A, Niche, 10.0)))
	{
		return false;
	}
	TestEqual(TEXT("El Nicho pasa a Reclamado"), static_cast<int32>(State.Board.Domains[Free[0]].NicheState), static_cast<int32>(ENicheState::Claimed));
	TestEqual(TEXT("Reclamado por A: el Dominio tiene dueño"), State.Board.Domains[Free[0]].OwnerSeat, A);

	TestTrue(TEXT("B termina después en el mismo Nicho"), LandOn(State, Random, B, Niche, 20.0));
	TestEqual(TEXT("Sigue Reclamado"), static_cast<int32>(State.Board.Domains[Free[0]].NicheState), static_cast<int32>(ENicheState::Claimed));
	TestEqual(TEXT("Sigue siendo de A"), State.Board.Domains[Free[0]].OwnerSeat, A);
	TestEqual(TEXT("B sigue siendo Eco"), static_cast<int32>(State.Seats[B].Role), static_cast<int32>(ESeatRole::LostEcho));
	TestEqual(TEXT("B sigue sin Dominio"), State.Seats[B].DomainId, static_cast<int32>(INDEX_NONE));

	TestTrue(TEXT("A, ya Elegido, termina en otro Nicho Libre"), LandOn(State, Random, A, State.Board.FindNiche(Free[1]), 30.0));
	TestEqual(TEXT("El otro Nicho sigue Libre"), static_cast<int32>(State.Board.Domains[Free[1]].NicheState), static_cast<int32>(ENicheState::Free));
	TestEqual(TEXT("El otro Dominio sigue sin dueño"), State.Board.Domains[Free[1]].OwnerSeat, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A no cambia de Dominio (R-CAR-6)"), State.Seats[A].DomainId, Free[0]);

	TestTrue(TEXT("B termina en un Nicho Cerrado"), LandOn(State, Random, B, State.Board.FindNiche(Closed[0]), 40.0));
	TestEqual(TEXT("El Nicho Cerrado sigue Cerrado"), static_cast<int32>(State.Board.Domains[Closed[0]].NicheState), static_cast<int32>(ENicheState::Closed));
	TestEqual(TEXT("B sigue siendo Eco"), static_cast<int32>(State.Seats[B].Role), static_cast<int32>(ESeatRole::LostEcho));

	TestEqual(TEXT("Un solo vínculo en el registro"), EventsOfType(State, EMatchEventType::Bonded).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceBondAndAwakeningTest,
	"Soulbound.Core.BondRace.VinculoYDespertar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceBondAndAwakeningTest::RunTest(const FString& Parameters)
{
	// CA-CAR-5 y CA-TAB-10 (R-CAR-5, R-TAB-9): dado un Eco que reclama, su rol es Elegido del Dominio del Nicho,
	// sigue con un dado, y si era el primero el tablero pasa de Primordial a Despierto con todos los Nodos revelados
	// en ese instante; un segundo vínculo no cambia el estado del tablero. Semilla fija 4.
	FRandomStream Random(4);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
	const int32 A = State.TurnOrder[0];
	const int32 B = State.TurnOrder[1];
	TestFalse(TEXT("Antes del vínculo, los Nodos no están revelados"), State.Board.AreNodesRevealed());

	if (!TestTrue(TEXT("A reclama el Nicho"), LandOn(State, Random, A, State.Board.FindNiche(Free[0]), 12.0)))
	{
		return false;
	}
	TestEqual(TEXT("A es Elegido"), static_cast<int32>(State.Seats[A].Role), static_cast<int32>(ESeatRole::Elect));
	TestEqual(TEXT("Del Dominio del Nicho"), State.Seats[A].DomainId, Free[0]);
	TestEqual(TEXT("El tablero pasa a Despierto"), static_cast<int32>(State.Board.Phase), static_cast<int32>(EBoardPhase::Awakened));
	TestTrue(TEXT("Con todos los Nodos revelados"), State.Board.AreNodesRevealed());
	const TArray<FMatchEvent> Awakenings = EventsOfType(State, EMatchEventType::BoardAwakening);
	if (TestEqual(TEXT("Un Despertar en el registro"), Awakenings.Num(), 1))
	{
		TestEqual(TEXT("En el instante del vínculo"), Awakenings[0].ClockSeconds, 12.0);
		TestEqual(TEXT("Con el asiento que despierta el tablero"), Awakenings[0].Seat, A);
	}

	// Sigue con un dado: cuarenta tiradas de A como Elegido, ninguna de más de 6 y algún 1.
	TArray<int32> Rolls;
	for (int32 Turn = 0; Turn < 40; ++Turn)
	{
		State.ActiveSeat = A;
		State.TurnPhase = ETurnPhase::BetweenTurns;
		if (!TestTrue(TEXT("El turno de A llega a la tirada"), AdvanceToChoice(State, Random, 13.0 + Turn)))
		{
			return false;
		}
		Rolls.Add(State.Roll);
	}
	TestTrue(TEXT("El Elegido tira un dado: ninguna tirada pasa de 6"), Rolls.FindByPredicate([](int32 Roll) { return Roll < 1 || Roll > Soulbound::Dice::Faces; }) == nullptr);
	TestTrue(TEXT("El Elegido tira un dado: sale algún 1"), Rolls.Contains(1));
	State.TurnPhase = ETurnPhase::BetweenTurns;

	TestTrue(TEXT("B reclama otro Nicho"), LandOn(State, Random, B, State.Board.FindNiche(Free[1]), 60.0));
	TestEqual(TEXT("B es Elegido"), static_cast<int32>(State.Seats[B].Role), static_cast<int32>(ESeatRole::Elect));
	TestEqual(TEXT("El tablero sigue Despierto"), static_cast<int32>(State.Board.Phase), static_cast<int32>(EBoardPhase::Awakened));
	TestEqual(TEXT("El segundo vínculo no despierta otra vez"), EventsOfType(State, EMatchEventType::BoardAwakening).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceEndTriggersTest,
	"Soulbound.Core.BondRace.FinPorUltimoNichoYPorTiempo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceEndTriggersTest::RunTest(const FString& Parameters)
{
	// CA-CAR-1 y CA-TAB-28 (R-CAR-1, R-TAB-27): dada la carrera, termina al reclamarse el último Nicho Libre aunque
	// quede tiempo; y dado el tiempo agotado, termina al acabar el turno en curso, no antes. Semillas fijas 5 y 6.
	{
		FRandomStream Random(5);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
		for (int32 i = 0; i < Free.Num(); ++i)
		{
			const double Now = 10.0 * (i + 1);
			TestTrue(TEXT("Queda tiempo de carrera"), Now < State.Params.RaceDurationSeconds);
			TestEqual(FString::Printf(TEXT("Antes del vínculo %d, la carrera sigue"), i + 1), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));
			if (!TestTrue(TEXT("El asiento reclama su Nicho"), LandOn(State, Random, State.TurnOrder[i], State.Board.FindNiche(Free[i]), Now)))
			{
				return false;
			}
		}
		TestEqual(TEXT("Último Nicho Libre reclamado: la carrera termina"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::RuneHunt));
		const TArray<FMatchEvent> Ended = EventsOfType(State, EMatchEventType::BondRaceEnded);
		if (TestEqual(TEXT("Un fin de carrera"), Ended.Num(), 1))
		{
			TestEqual(TEXT("En el instante del último vínculo"), Ended[0].ClockSeconds, 10.0 * Free.Num());
			TestEqual(TEXT("Con tantos Elegidos como Nichos"), Ended[0].Value, Free.Num());
		}
	}
	{
		FRandomStream Random(6);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const double RaceEnd = State.Params.RaceDurationSeconds;
		const int32 A = State.TurnOrder[0];
		const int32 B = State.TurnOrder[1];
		// Un Elegido, para que el fin no sea la victoria del Warden; su turno, lejos del límite, no termina la carrera.
		if (!TestTrue(TEXT("A reclama un Nicho"), LandOn(State, Random, A, State.Board.FindNiche(DomainsWithNiche(State, ENicheState::Free)[0]), 10.0)))
		{
			return false;
		}
		TestEqual(TEXT("Un turno que acaba antes del límite no termina la carrera"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));

		// El turno de B empieza antes del límite y confirma después: el límite no lo interrumpe (TODO(R-TAB-26)).
		State.ActiveSeat = B;
		if (!TestTrue(TEXT("El turno de B llega a la tirada"), AdvanceToChoice(State, Random, RaceEnd - 5.0)) || !TestFalse(TEXT("B tiene opciones"), State.ReachableOptions.IsEmpty()))
		{
			return false;
		}
		TestEqual(TEXT("Intención después del límite: se acepta"), static_cast<int32>(Soulbound::Rules::SubmitDestinationIntent(State, B, State.ReachableOptions[0], RaceEnd + 1.0)), static_cast<int32>(EIntentResult::Accepted));
		TestEqual(TEXT("Con el tiempo agotado, la carrera sigue mientras dura el turno"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));
		while (State.TurnPhase != ETurnPhase::End && Soulbound::Rules::AdvanceTurn(State, RaceEnd + 1.0, Random))
		{
		}
		TestEqual(TEXT("En el fin de turno, la carrera sigue"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));
		Soulbound::Rules::AdvanceTurn(State, RaceEnd + 1.0, Random);
		TestEqual(TEXT("Al acabar el turno en curso, la carrera termina"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::RuneHunt));
		const TArray<FMatchEvent> Ended = EventsOfType(State, EMatchEventType::BondRaceEnded);
		if (TestEqual(TEXT("Un fin de carrera"), Ended.Num(), 1))
		{
			TestEqual(TEXT("En el instante en que acaba el turno"), Ended[0].ClockSeconds, RaceEnd + 1.0);
		}
	}
	{
		// Entre turnos: si el tiempo se agota sin turno en curso, la carrera termina antes de empezar el siguiente.
		FRandomStream Random(6);
		FMatchState State;
		if (!StartWithElects(*this, 6, 1, Random, State))
		{
			return false;
		}
		const int32 TurnsStarted = EventsOfType(State, EMatchEventType::TurnStarted).Num();
		Soulbound::Rules::AdvanceTurn(State, State.Params.RaceDurationSeconds + 3.0, Random);
		TestEqual(TEXT("Tiempo agotado entre turnos: la carrera termina"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::RuneHunt));
		TestEqual(TEXT("Antes de empezar otro turno"), EventsOfType(State, EMatchEventType::TurnStarted).Num(), TurnsStarted);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceExtraTurnBeforeLimitTest,
	"Soulbound.Core.BondRace.TurnoExtraAntesDelLimite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceExtraTurnBeforeLimitTest::RunTest(const FString& Parameters)
{
	// R-TUR-9 antes que R-CAR-1 (revisión de S-006): un turno extra concedido y empezado antes del límite de la carrera
	// se juega entero, y la carrera termina al acabarlo. Semilla fija 13.
	FRandomStream Random;
	FMatchState State;
	if (!StartWithElects(*this, 13, 1, Random, State))
	{
		return false;
	}
	const double RaceEnd = State.Params.RaceDurationSeconds;
	const int32 Seat = State.TurnOrder[1];
	State.ActiveSeat = Seat;
	Soulbound::Rules::GrantExtraTurns(State, Seat, 1);

	// Primer turno: acaba antes del límite y deja el extra pendiente.
	if (!TestTrue(TEXT("El turno llega a la tirada"), AdvanceToChoice(State, Random, RaceEnd - 10.0)) || !TestFalse(TEXT("Hay opciones"), State.ReachableOptions.IsEmpty()))
	{
		return false;
	}
	Soulbound::Rules::SubmitDestinationIntent(State, Seat, State.ReachableOptions[0], RaceEnd - 8.0);
	while (State.TurnPhase != ETurnPhase::BetweenTurns && Soulbound::Rules::AdvanceTurn(State, RaceEnd - 8.0, Random))
	{
	}
	TestEqual(TEXT("Juega otra vez el mismo asiento"), State.ActiveSeat, Seat);
	TestEqual(TEXT("La carrera sigue"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));

	// Turno extra: empieza antes del límite y confirma después; se juega entero.
	const int32 StartedBefore = EventsOfType(State, EMatchEventType::TurnStarted).Num();
	if (!TestTrue(TEXT("El turno extra llega a la tirada"), AdvanceToChoice(State, Random, RaceEnd - 2.0)) || !TestFalse(TEXT("Hay opciones"), State.ReachableOptions.IsEmpty()))
	{
		return false;
	}
	TestEqual(TEXT("El turno extra empieza antes del límite"), EventsOfType(State, EMatchEventType::TurnStarted).Num(), StartedBefore + 1);
	TestEqual(TEXT("Intención del turno extra después del límite: se acepta"),
		static_cast<int32>(Soulbound::Rules::SubmitDestinationIntent(State, Seat, State.ReachableOptions[0], RaceEnd + 3.0)), static_cast<int32>(EIntentResult::Accepted));
	while (State.TurnPhase != ETurnPhase::End && Soulbound::Rules::AdvanceTurn(State, RaceEnd + 3.0, Random))
	{
	}
	TestEqual(TEXT("Mientras dura el turno extra, la carrera sigue"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));
	Soulbound::Rules::AdvanceTurn(State, RaceEnd + 3.0, Random);
	TestEqual(TEXT("Al acabar el turno extra, la carrera termina"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::RuneHunt));
	const TArray<FMatchEvent> Ended = EventsOfType(State, EMatchEventType::BondRaceEnded);
	if (TestEqual(TEXT("Un fin de carrera"), Ended.Num(), 1))
	{
		TestEqual(TEXT("En el instante en que acaba el turno extra"), Ended[0].ClockSeconds, RaceEnd + 3.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceExtraTurnAfterLimitTest,
	"Soulbound.Core.BondRace.TurnoExtraPendienteTrasElLimite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceExtraTurnAfterLimitTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 01/10/2026 (revisión de S-006): si el turno que acaba ya pasó el límite y deja un turno
	// extra pendiente, la carrera termina al acabar ese turno y el extra se juega después, con las condiciones de
	// fuera de la carrera (el Eco, confinado). Semilla fija 14.
	FRandomStream Random;
	FMatchState State;
	if (!StartWithElects(*this, 14, 1, Random, State))
	{
		return false;
	}
	const double RaceEnd = State.Params.RaceDurationSeconds;
	const int32 Seat = State.TurnOrder[1];
	State.ActiveSeat = Seat;
	Soulbound::Rules::GrantExtraTurns(State, Seat, 1);

	if (!TestTrue(TEXT("El turno llega a la tirada"), AdvanceToChoice(State, Random, RaceEnd - 5.0)))
	{
		return false;
	}
	// Un destino que no sea un Nicho Libre, para que el asiento siga siendo Eco.
	const int32* Destination = State.ReachableOptions.FindByPredicate([&State](int32 Tile)
	{
		const FTileState& TileState = State.Board.Tiles[Tile];
		return TileState.Type != ETileType::Niche || State.Board.Domains[TileState.DomainId].NicheState != ENicheState::Free;
	});
	if (!TestNotNull(TEXT("Hay un destino que no es un Nicho Libre"), Destination))
	{
		return false;
	}
	Soulbound::Rules::SubmitDestinationIntent(State, Seat, *Destination, RaceEnd + 1.0);
	while (State.TurnPhase != ETurnPhase::BetweenTurns && Soulbound::Rules::AdvanceTurn(State, RaceEnd + 1.0, Random))
	{
	}
	TestEqual(TEXT("La carrera termina al acabar el turno, aunque quede el extra"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::RuneHunt));
	TestEqual(TEXT("El extra lo juega el mismo asiento"), State.ActiveSeat, Seat);

	if (!TestTrue(TEXT("El turno extra llega a la tirada"), AdvanceToChoice(State, Random, RaceEnd + 2.0)))
	{
		return false;
	}
	const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(State, Seat);
	TestTrue(TEXT("En el turno extra, el Eco ya está confinado"), Mover.bConfinedToForgotten);
	TestTrue(TEXT("Y sus opciones son las del confinamiento"),
		State.ReachableOptions == Soulbound::Rules::ReachableTiles(State.Board, State.Seats[Seat].TileId, State.Roll, Mover));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceEndOfRaceTest,
	"Soulbound.Core.BondRace.FinDeCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceEndOfRaceTest::RunTest(const FString& Parameters)
{
	// CA-CAR-8, CA-TAB-11, 12 y 13 (R-CAR-8, R-TAB-10 a 12): dada una carrera terminada con E Elegidos: si E es 0, la
	// partida termina con victoria del Warden y la máquina de turnos no avanza más (y nada más cambia, decisión del
	// autor del 01/10/2026); si no, hay exactamente 6 menos E Olvidados, ningún Nicho Libre, y cada Eco tiene un
	// Olvidado distinto asignado. Misma semilla, misma asignación. Semillas fijas 1 a 10.
	{
		FRandomStream Random;
		FMatchState State;
		if (!StartWithElects(*this, 1, 0, Random, State))
		{
			return false;
		}
		const int32 ForgottenAtStart = CountDomains(State, EDomainState::Forgotten);
		const double RaceEnd = State.Params.RaceDurationSeconds;
		TestTrue(TEXT("E = 0: la carrera termina"), Soulbound::Rules::EndBondRace(State, RaceEnd));
		TestTrue(TEXT("E = 0: la partida termina"), State.IsMatchOver());
		TestEqual(TEXT("E = 0: con victoria del Warden"), static_cast<int32>(State.Outcome), static_cast<int32>(EMatchOutcome::WardenVictory));
		TestEqual(TEXT("E = 0: no se olvida ningún Dominio más"), CountDomains(State, EDomainState::Forgotten), ForgottenAtStart);
		TestEqual(TEXT("E = 0: no se asignan hogares"), EventsOfType(State, EMatchEventType::HomeAssigned).Num(), 0);

		const FMatchState Before = State;
		TestFalse(TEXT("E = 0: la máquina de turnos no avanza"), Soulbound::Rules::AdvanceTurn(State, RaceEnd + 100.0, Random));
		TestNotEqual(TEXT("E = 0: ninguna intención se acepta"),
			static_cast<int32>(Soulbound::Rules::SubmitDestinationIntent(State, State.ActiveSeat, State.Board.FindAltar(), RaceEnd + 100.0)), static_cast<int32>(EIntentResult::Accepted));
		TestFalse(TEXT("E = 0: ni la carrera termina otra vez"), Soulbound::Rules::EndBondRace(State, RaceEnd + 100.0));
		TestTrue(TEXT("E = 0: el estado no cambia"), Soulbound::Tests::SameState(Before, State));

		// Y el Warden no actúa sobre una partida terminada (decisión del autor del 01/10/2026). Generador con semilla 1. Desde
		// S-016, con el Warden de los datos y siempre dispuesto a intervenir: aun así, nada.
		FWardenParams WardenParams;
		if (!Soulbound::Tests::LoadTestWardenParams(*this, WardenParams))
		{
			return false;
		}
		for (FWardenActParams& Act : WardenParams.Acts)
		{
			Act.InterveneProbability = 1.0;
		}
		State.WardenParams = WardenParams;
		FRandomStream WardenRandom(1);
		const FWardenBrain Brain;
		for (int32 Evaluation = 0; Evaluation < 50; ++Evaluation)
		{
			if (Brain.Evaluate(State, RaceEnd + 15.0 * Evaluation, WardenRandom).Kind != EWardenDecisionKind::None)
			{
				AddError(TEXT("E = 0: el Warden interviene en una partida terminada"));
				break;
			}
		}
	}

	for (int32 Elects = 1; Elects < Soulbound::Match::SeatCount; ++Elects)
	{
		for (int32 Seed = 1; Seed <= 10; ++Seed)
		{
			const FString Context = FString::Printf(TEXT("E = %d, semilla %d"), Elects, Seed);
			FRandomStream Random;
			FMatchState State;
			if (!StartWithElects(*this, Seed, Elects, Random, State))
			{
				return false;
			}
			TestTrue(Context + TEXT(": la carrera termina"), Soulbound::Rules::EndBondRace(State, State.Params.RaceDurationSeconds));
			TestEqual(Context + TEXT(": pasa a la caza de runas"), static_cast<int32>(State.MatchPhase), static_cast<int32>(EMatchPhase::RuneHunt));
			TestEqual(Context + TEXT(": sin resultado aún"), static_cast<int32>(State.Outcome), static_cast<int32>(EMatchOutcome::Undecided));
			TestEqual(Context + TEXT(": 6 menos E Olvidados"), CountDomains(State, EDomainState::Forgotten), Soulbound::Board::DomainCount - Elects);
			TestEqual(Context + TEXT(": ningún Nicho Libre"), DomainsWithNiche(State, ENicheState::Free).Num(), 0);
			for (const FDomainState& Domain : State.Board.Domains)
			{
				const bool bForgotten = Domain.State == EDomainState::Forgotten;
				TestEqual(Context + TEXT(": Olvidado si y solo si su Nicho está Cerrado"), Domain.NicheState == ENicheState::Closed, bForgotten);
				TestEqual(Context + TEXT(": con dueño si y solo si no es Olvidado"), State.Seats.IsValidIndex(Domain.OwnerSeat), !bForgotten);
			}

			TSet<int32> EchoHomes;
			int32 Echoes = 0;
			for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
			{
				const FSeatState& SeatState = State.Seats[Seat];
				if (SeatState.Role == ESeatRole::Elect)
				{
					TestEqual(Context + TEXT(": el Elegido es dueño de su Dominio"), State.Board.Domains[SeatState.DomainId].OwnerSeat, Seat);
					continue;
				}
				++Echoes;
				if (TestTrue(Context + TEXT(": el Eco tiene Dominio asignado"), State.Board.Domains.IsValidIndex(SeatState.DomainId)))
				{
					TestEqual(Context + TEXT(": su Dominio es Olvidado"), static_cast<int32>(State.Board.Domains[SeatState.DomainId].State), static_cast<int32>(EDomainState::Forgotten));
					EchoHomes.Add(SeatState.DomainId);
				}
			}
			TestEqual(Context + TEXT(": quedan 4 menos E Ecos"), Echoes, Soulbound::Match::SeatCount - Elects);
			TestEqual(Context + TEXT(": cada Eco con un Olvidado distinto"), EchoHomes.Num(), Echoes);
			TestTrue(Context + TEXT(": la máquina de turnos sigue"), Soulbound::Rules::AdvanceTurn(State, State.Params.RaceDurationSeconds + 1.0, Random));

			// Misma semilla, misma asignación.
			FRandomStream RandomAgain;
			FMatchState Again;
			StartWithElects(*this, Seed, Elects, RandomAgain, Again);
			Soulbound::Rules::EndBondRace(Again, Again.Params.RaceDurationSeconds);
			for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
			{
				TestEqual(Context + TEXT(": misma semilla, mismo Dominio asignado"), Again.Seats[Seat].DomainId, State.Seats[Seat].DomainId);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceEchoReturnsHomeTest,
	"Soulbound.Core.BondRace.EcoEnDominioConDuenoVuelveAlHogar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceEchoReturnsHomeTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 01/10/2026 (plan de S-006), por R-TAB-20: un Eco que acaba la carrera dentro de un
	// Dominio con dueño vuelve a su hogar; si no, el confinamiento lo dejaría encerrado. El que está en el Altar o en
	// un Olvidado no se mueve. Semilla fija 7.
	FRandomStream Random(7);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
	const TArray<int32> Closed = DomainsWithNiche(State, ENicheState::Closed);
	const int32 A = State.TurnOrder[0];
	const int32 B = State.TurnOrder[1];
	const int32 C = State.TurnOrder[2];
	const int32 D = State.TurnOrder[3];
	const int32 ClaimedNiche = State.Board.FindNiche(Free[0]);
	const int32 ClosedNiche = State.Board.FindNiche(Closed[0]);
	if (!LandOn(State, Random, A, ClaimedNiche, 10.0) || !LandOn(State, Random, B, ClaimedNiche, 20.0) || !LandOn(State, Random, D, ClosedNiche, 30.0))
	{
		AddError(TEXT("El montaje no llega a sus casillas"));
		return false;
	}
	const int32 Altar = State.Board.FindAltar();
	TestEqual(TEXT("C sigue en el Altar"), State.Seats[C].TileId, Altar);

	Soulbound::Rules::EndBondRace(State, State.Params.RaceDurationSeconds);
	TestEqual(TEXT("B, Eco en un Dominio con dueño, vuelve a su hogar"), State.Seats[B].TileId, Soulbound::Rules::HomeTile(State, B));
	TestEqual(TEXT("C, en el Altar, no se mueve"), State.Seats[C].TileId, Altar);
	TestEqual(TEXT("D, en un Olvidado, no se mueve"), State.Seats[D].TileId, ClosedNiche);
	TestEqual(TEXT("A, Elegido, no se mueve"), State.Seats[A].TileId, ClaimedNiche);
	const TArray<FMatchEvent> Returned = EventsOfType(State, EMatchEventType::ReturnedHome);
	if (TestEqual(TEXT("Una vuelta al hogar en el registro"), Returned.Num(), 1))
	{
		TestEqual(TEXT("La de B"), Returned[0].Seat, B);
		TestEqual(TEXT("Con su casilla hogar"), Returned[0].Value, State.Seats[B].TileId);
		TestEqual(TEXT("Al acabar la carrera"), Returned[0].ClockSeconds, State.Params.RaceDurationSeconds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceHomeTileTest,
	"Soulbound.Core.BondRace.Hogar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceHomeTileTest::RunTest(const FString& Parameters)
{
	// CA-TAB-14 (R-TAB-13): dado cualquier asiento, su hogar es el Altar en carrera, su Nicho si es Elegido (desde el
	// vínculo, decisión del autor del 01/10/2026), o el Nicho de su Dominio asignado si es Eco tras la carrera.
	// Semilla fija 8.
	FRandomStream Random(8);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const int32 Altar = State.Board.FindAltar();
	for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
	{
		TestEqual(FString::Printf(TEXT("Al empezar, el hogar del asiento %d es el Altar"), Seat), Soulbound::Rules::HomeTile(State, Seat), Altar);
	}
	const int32 A = State.TurnOrder[0];
	const int32 B = State.TurnOrder[1];
	const int32 Domain = DomainsWithNiche(State, ENicheState::Free)[0];
	if (!LandOn(State, Random, A, State.Board.FindNiche(Domain), 10.0))
	{
		AddError(TEXT("A no llega al Nicho"));
		return false;
	}
	TestEqual(TEXT("Elegido durante la carrera: su Nicho"), Soulbound::Rules::HomeTile(State, A), State.Board.FindNiche(Domain));
	TestEqual(TEXT("Eco durante la carrera: el Altar"), Soulbound::Rules::HomeTile(State, B), Altar);

	Soulbound::Rules::EndBondRace(State, State.Params.RaceDurationSeconds);
	TestEqual(TEXT("Elegido tras la carrera: su Nicho"), Soulbound::Rules::HomeTile(State, A), State.Board.FindNiche(Domain));
	const int32 Assigned = State.Seats[B].DomainId;
	if (TestTrue(TEXT("Eco tras la carrera: tiene Dominio asignado"), State.Board.Domains.IsValidIndex(Assigned)))
	{
		TestEqual(TEXT("Eco tras la carrera: el Nicho de su Dominio asignado"), Soulbound::Rules::HomeTile(State, B), State.Board.FindNiche(Assigned));
		TestEqual(TEXT("Que está Cerrado"), static_cast<int32>(State.Board.Domains[Assigned].NicheState), static_cast<int32>(ENicheState::Closed));
	}
	TestEqual(TEXT("Asiento inexistente: ninguna casilla"), Soulbound::Rules::HomeTile(State, Soulbound::Match::SeatCount), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Asiento negativo: ninguna casilla"), Soulbound::Rules::HomeTile(State, INDEX_NONE), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceEchoConfinementTest,
	"Soulbound.Core.BondRace.ConfinamientoDeEcos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceEchoConfinementTest::RunTest(const FString& Parameters)
{
	// CA-TAB-21 y CA-TAB-22 (R-TAB-20, R-ECO-2, R-TAB-21): dado un Eco tras la carrera, ninguna casilla de un Dominio
	// con dueño está entre sus alcanzables ni se acepta como destino; el Altar y los Olvidados sí. Durante la carrera,
	// y para un Elegido siempre, no hay restricción. La interfaz y el motor usan la misma función. Semilla fija 9.
	FRandomStream Random(9);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const int32 Altar = State.Board.FindAltar();
	const int32 A = State.TurnOrder[0];
	const int32 B = State.TurnOrder[1];
	const int32 C = State.TurnOrder[2];
	TestFalse(TEXT("Eco en carrera: sin confinamiento"), Soulbound::Rules::MakeMoverTraits(State, C).bConfinedToForgotten);
	TestEqual(TEXT("Eco en carrera: alcanza los seis Dominios"), ReachableDomains(State, C, Altar, 12).Num(), Soulbound::Board::DomainCount);

	const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
	if (!LandOn(State, Random, A, State.Board.FindNiche(Free[0]), 10.0) || !LandOn(State, Random, B, State.Board.FindNiche(Free[1]), 20.0))
	{
		AddError(TEXT("A y B no llegan a sus Nichos"));
		return false;
	}
	TestFalse(TEXT("Elegido en carrera: sin confinamiento"), Soulbound::Rules::MakeMoverTraits(State, A).bConfinedToForgotten);
	const double RaceEnd = State.Params.RaceDurationSeconds;
	Soulbound::Rules::EndBondRace(State, RaceEnd);

	const FMoverTraits Echo = Soulbound::Rules::MakeMoverTraits(State, C);
	TestTrue(TEXT("Eco tras la carrera: confinado"), Echo.bConfinedToForgotten);
	TestFalse(TEXT("Elegido tras la carrera: sin confinamiento"), Soulbound::Rules::MakeMoverTraits(State, A).bConfinedToForgotten);

	TArray<int32> OwnedTiles;
	TArray<int32> Starts = {Altar, Soulbound::Rules::HomeTile(State, C)};
	for (const FTileState& Tile : State.Board.Tiles)
	{
		if (State.Board.Domains.IsValidIndex(Tile.DomainId))
		{
			(State.Board.Domains[Tile.DomainId].State == EDomainState::Owned ? OwnedTiles : Starts).Add(Tile.TileId);
		}
	}
	for (const int32 Owned : OwnedTiles)
	{
		TestFalse(FString::Printf(TEXT("Eco: la casilla %d de un Dominio con dueño no es transitable"), Owned), Soulbound::Rules::IsPassable(State.Board, Owned, Echo));
	}
	TestTrue(TEXT("Eco: el Altar es transitable"), Soulbound::Rules::IsPassable(State.Board, Altar, Echo));
	TSet<int32> EchoReach;
	for (const int32 From : Starts)
	{
		for (int32 Roll = 1; Roll <= 12; ++Roll)
		{
			for (const int32 Tile : Soulbound::Rules::ReachableTiles(State.Board, From, Roll, Echo))
			{
				EchoReach.Add(Tile);
				if (OwnedTiles.Contains(Tile))
				{
					AddError(FString::Printf(TEXT("Eco: desde la %d con %d alcanza la %d, de un Dominio con dueño"), From, Roll, Tile));
				}
			}
		}
	}
	TestTrue(TEXT("Eco: alcanza el Altar"), EchoReach.Contains(Altar));
	TestTrue(TEXT("Eco: alcanza casillas de los Olvidados"), EchoReach.Num() > 1);
	TestTrue(TEXT("Elegido tras la carrera: alcanza el Dominio del otro Elegido"), ReachableDomains(State, A, Altar, 12).Contains(State.Seats[B].DomainId));

	// La máquina de turnos usa la misma función: las opciones del Eco son las de sus rasgos y el destino se valida contra ellas.
	State.ActiveSeat = C;
	State.TurnPhase = ETurnPhase::BetweenTurns;
	if (!TestTrue(TEXT("El turno del Eco llega a la tirada"), AdvanceToChoice(State, Random, RaceEnd + 10.0)))
	{
		return false;
	}
	TestTrue(TEXT("Opciones del turno = ReachableTiles con MakeMoverTraits"),
		State.ReachableOptions == Soulbound::Rules::ReachableTiles(State.Board, State.Seats[C].TileId, State.Roll, Echo));
	for (const int32 Owned : OwnedTiles)
	{
		if (Soulbound::Rules::SubmitDestinationIntent(State, C, Owned, RaceEnd + 11.0) == EIntentResult::Accepted)
		{
			AddError(FString::Printf(TEXT("Eco: se acepta como destino la %d, de un Dominio con dueño"), Owned));
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceEventLogTest,
	"Soulbound.Core.BondRace.RegistroDeLaCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceEventLogTest::RunTest(const FString& Parameters)
{
	// Criterio "Registro" (GDD 03): vínculo, Despertar, fin de carrera, hogar asignado y victoria del Warden aparecen
	// en el registro con asiento y marca de tiempo. Fin de carrera y victoria del Warden afectan a todos: sin asiento.
	// Semillas fijas 10 y 11.
	{
		FRandomStream Random(10);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
		const int32 A = State.TurnOrder[0];
		const int32 B = State.TurnOrder[1];
		LandOn(State, Random, A, State.Board.FindNiche(Free[0]), 15.0);
		LandOn(State, Random, B, State.Board.FindNiche(Free[1]), 25.0);
		const int32 FirstBond = State.EventLog.IndexOfByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::Bonded; });
		if (TestTrue(TEXT("Hay vínculo y le sigue el Despertar"), FirstBond != INDEX_NONE && State.EventLog.IsValidIndex(FirstBond + 1)))
		{
			const FMatchEvent& Bond = State.EventLog[FirstBond];
			TestEqual(TEXT("Vínculo: asiento"), Bond.Seat, A);
			TestEqual(TEXT("Vínculo: Dominio"), Bond.Value, Free[0]);
			TestEqual(TEXT("Vínculo: marca de tiempo"), Bond.ClockSeconds, 15.0);
			const FMatchEvent& Awakening = State.EventLog[FirstBond + 1];
			TestEqual(TEXT("Justo después, el Despertar"), static_cast<int32>(Awakening.Type), static_cast<int32>(EMatchEventType::BoardAwakening));
			TestEqual(TEXT("Despertar: asiento"), Awakening.Seat, A);
			TestEqual(TEXT("Despertar: marca de tiempo"), Awakening.ClockSeconds, 15.0);
		}
		const TArray<FMatchEvent> Bonds = EventsOfType(State, EMatchEventType::Bonded);
		if (TestEqual(TEXT("Dos vínculos"), Bonds.Num(), 2))
		{
			TestEqual(TEXT("Segundo vínculo: asiento"), Bonds[1].Seat, B);
			TestEqual(TEXT("Segundo vínculo: marca de tiempo"), Bonds[1].ClockSeconds, 25.0);
		}

		const double RaceEnd = State.Params.RaceDurationSeconds;
		Soulbound::Rules::EndBondRace(State, RaceEnd);
		const TArray<FMatchEvent> Homes = EventsOfType(State, EMatchEventType::HomeAssigned);
		TestEqual(TEXT("Un hogar asignado por Eco"), Homes.Num(), 2);
		for (const FMatchEvent& Home : Homes)
		{
			if (TestTrue(TEXT("Hogar: asiento válido"), State.Seats.IsValidIndex(Home.Seat)))
			{
				TestEqual(TEXT("Hogar: asiento Eco"), static_cast<int32>(State.Seats[Home.Seat].Role), static_cast<int32>(ESeatRole::LostEcho));
				TestEqual(TEXT("Hogar: el Dominio asignado"), Home.Value, State.Seats[Home.Seat].DomainId);
			}
			TestEqual(TEXT("Hogar: marca de tiempo"), Home.ClockSeconds, RaceEnd);
		}
		const FMatchEvent& Last = State.EventLog.Last();
		TestEqual(TEXT("El fin de carrera es lo último"), static_cast<int32>(Last.Type), static_cast<int32>(EMatchEventType::BondRaceEnded));
		TestEqual(TEXT("Fin de carrera: marca de tiempo"), Last.ClockSeconds, RaceEnd);
		TestEqual(TEXT("Fin de carrera: sin asiento"), Last.Seat, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("Fin de carrera: número de Elegidos"), Last.Value, 2);
	}
	{
		FRandomStream Random(11);
		FMatchState State;
		if (!StartTestMatch(*this, Random, State))
		{
			return false;
		}
		const double RaceEnd = State.Params.RaceDurationSeconds;
		const int32 Before = State.EventLog.Num();
		Soulbound::Rules::EndBondRace(State, RaceEnd);
		if (TestEqual(TEXT("Sin vínculos: dos eventos"), State.EventLog.Num(), Before + 2))
		{
			const FMatchEvent& Victory = State.EventLog[Before];
			TestEqual(TEXT("Primero, la victoria del Warden"), static_cast<int32>(Victory.Type), static_cast<int32>(EMatchEventType::WardenVictory));
			TestEqual(TEXT("Victoria del Warden: marca de tiempo"), Victory.ClockSeconds, RaceEnd);
			TestEqual(TEXT("Victoria del Warden: sin asiento"), Victory.Seat, static_cast<int32>(INDEX_NONE));
			const FMatchEvent& Ended = State.EventLog[Before + 1];
			TestEqual(TEXT("Después, el fin de carrera"), static_cast<int32>(Ended.Type), static_cast<int32>(EMatchEventType::BondRaceEnded));
			TestEqual(TEXT("Fin de carrera: cero Elegidos"), Ended.Value, 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBondRaceDummyDeciderTest,
	"Soulbound.Core.BondRace.DummyDeCarreraBuscaElNicho",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBondRaceDummyDeciderTest::RunTest(const FString& Parameters)
{
	// Doble de prueba de S-006 (nota "Dummy de carrera"): un Eco en carrera elige la opción que deja más cerca el
	// Nicho Libre más cercano; un Elegido elige al azar. Semilla fija 12; generador del decisor con semilla 1.
	FRandomStream Random(12);
	FMatchState State;
	if (!StartTestMatch(*this, Random, State))
	{
		return false;
	}
	const TArray<int32> Free = DomainsWithNiche(State, ENicheState::Free);
	const int32 Seat = State.TurnOrder[0];
	const int32 Altar = State.Board.FindAltar();
	const int32 Niche = State.Board.FindNiche(Free[0]);
	const int32 BeforeNiche = State.Board.Tiles[Niche].Neighbours[0];
	TestNotEqual(TEXT("El Nicho no está pegado al Altar (montaje)"), BeforeNiche, Altar);

	FRaceDummyDecider Dummy;
	FRandomStream DeciderRandom(1);
	TestEqual(TEXT("Sin opciones: ninguna"), Dummy.ChooseDestination(State, Seat, {}, DeciderRandom), static_cast<int32>(INDEX_NONE));
	for (int32 Draw = 0; Draw < 20; ++Draw)
	{
		TestEqual(TEXT("Eco en carrera: el Nicho Libre antes que el Altar"), Dummy.ChooseDestination(State, Seat, {Altar, Niche}, DeciderRandom), Niche);
		TestEqual(TEXT("Eco en carrera: la casilla pegada al Nicho antes que el Altar"), Dummy.ChooseDestination(State, Seat, {BeforeNiche, Altar}, DeciderRandom), BeforeNiche);
	}

	if (!LandOn(State, Random, Seat, Niche, 10.0))
	{
		AddError(TEXT("El asiento no llega al Nicho"));
		return false;
	}
	const int32 OtherNiche = State.Board.FindNiche(Free[1]);
	TSet<int32> Chosen;
	for (int32 Draw = 0; Draw < 50; ++Draw)
	{
		Chosen.Add(Dummy.ChooseDestination(State, Seat, {Altar, OtherNiche}, DeciderRandom));
	}
	TestEqual(TEXT("Elegido: elige al azar, las dos opciones salen"), Chosen.Num(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
