// Tests de las ediciones de depuración (S-011, Match/DebugEdits.h): runas, colocar, vincular, Sellos, fin de carrera,
// tirada fijada, siguiente asiento y registro. Cada test cita el criterio de aceptación (CA-DEP-n, en el orden de la
// story S-011) y la regla (R-XXX-n) del GDD que reutiliza la edición. Se lanzan sin editor con Scripts/test.ps1.
// Filtro: "Soulbound.Core.Debug". Montaje: tablero de 35 casillas, cuatro asientos, como en los tests de runas.

#include "Board/MovementRules.h"
#include "Board/SealRules.h"
#include "Match/BondRaceRules.h"
#include "Match/DebugEdits.h"
#include "Match/EffectRules.h"
#include "Match/MatchStart.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Match/WardenEventRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::DomainsWithNiche;
	using Soulbound::Tests::EventsOfType;
	using Soulbound::Tests::LandOn;
	using Soulbound::Tests::SameState;

	/** Partida de cuatro sobre el tablero de 35 casillas. Now es el reloj de partida de los tests, siempre creciente. */
	struct FDebugFixture
	{
		FMatchState State;
		FRandomStream Random{7};
		double Now = 1.0;

		bool Start(FAutomationTestBase& Test, int32 Seed = 7)
		{
			Random.Initialize(Seed);
			FBoardState Board;
			FMatchParams Params;
			FElectsTable Elects;
			if (!Soulbound::Tests::LoadBoardFile(Test, Soulbound::Tests::RaceBoardFileName(), Board) || !Soulbound::Tests::LoadTestMatchParams(Test, Params)
				|| !Soulbound::Tests::LoadTestElects(Test, Elects))
			{
				return false;
			}
			FMatchStartResult Result = Soulbound::Rules::StartMatch(Board, Params, Elects, Soulbound::Match::MaxPlayers, Random);
			State = MoveTemp(Result.State);
			return Test.TestTrue(FString::Printf(TEXT("La partida empieza (%s)"), *Result.Message), Result.IsSuccess());
		}

		/** El asiento 0 reclama con la regla el primer Nicho Libre: el tablero despierta. Devuelve su Dominio. */
		int32 Awaken(FAutomationTestBase& Test)
		{
			if (!Start(Test))
			{
				return INDEX_NONE;
			}
			const int32 Domain = DomainsWithNiche(State, ENicheState::Free)[0];
			Now += 1.0;
			Test.TestTrue(TEXT("El asiento 0 reclama el Nicho"), LandOn(State, Random, 0, State.Board.FindNiche(Domain), Now));
			Test.TestTrue(TEXT("El tablero despierta"), State.Board.Phase == EBoardPhase::Awakened);
			return Domain;
		}

		int32 SourceOf(int32 DomainId) const
		{
			for (const FTileState& Tile : State.Board.Tiles)
			{
				if (Tile.Type == ETileType::PowerSource && Tile.DomainId == DomainId)
				{
					return Tile.TileId;
				}
			}
			return INDEX_NONE;
		}

		/** Runas de un tipo en todos los Brazales y en las reservas de sus Fuentes: lo que ninguna edición debe cambiar. */
		int32 TotalOfType(int32 RuneType) const
		{
			int32 Total = 0;
			for (const FSeatState& Seat : State.Seats)
			{
				Total += Seat.RuneCountOf(RuneType);
			}
			for (const FTileState& Tile : State.Board.Tiles)
			{
				if (Tile.Type == ETileType::PowerSource && Tile.DomainId == RuneType)
				{
					Total += Tile.RuneReserve;
				}
			}
			return Total;
		}

		/** Avanza un paso de la máquina de turnos, un segundo después del anterior. */
		bool Step()
		{
			Now += 1.0;
			return Soulbound::Rules::AdvanceTurn(State, Now, Random);
		}

		/**
		 * Avanza hasta el final del turno en curso sin que nadie conteste: cada paso llega cuando el reloj de turno ya se
		 * agotó. Devuelve si llegó a BetweenTurns.
		 */
		bool FinishTurn()
		{
			for (int32 Count = 0; Count < 14 && State.TurnPhase != ETurnPhase::BetweenTurns; ++Count)
			{
				Now = FMath::Max(Now, State.TurnDeadlineSeconds + 1.0);
				Soulbound::Rules::AdvanceTurn(State, Now, Random);
			}
			return State.TurnPhase == ETurnPhase::BetweenTurns;
		}
	};

	/** Número de eventos de depuración del registro. */
	int32 DebugEventCount(const FMatchState& State)
	{
		return EventsOfType(State, EMatchEventType::DebugEdit).Num();
	}

	/** Copia del estado sin los eventos de depuración: para compararla con la que dejan las reglas solas. */
	FMatchState WithoutDebugEvents(FMatchState State)
	{
		State.EventLog.RemoveAll([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::DebugEdit; });
		return State;
	}

	/** Copia del estado sin registro y con la ficha del asiento en la casilla 0: para comparar sin la posición ni el registro. */
	FMatchState WithoutLogAndPosition(FMatchState State, int32 Seat)
	{
		State.EventLog.Reset();
		State.Seats[Seat].TileId = 0;
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugRunesTest,
	"Soulbound.Core.Debug.Runas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugRunesTest::RunTest(const FString& Parameters)
{
	// CA-DEP-1: dar n runas sube el Brazal y baja la reserva en n; quitarlas, al revés; la suma no cambia; se rechaza
	// con n mayor que la reserva, con el tablero Primordial o quitando más de las que hay. R-RUN-1, R-RUN-6.
	FDebugFixture F;
	const int32 Domain = F.Awaken(*this);
	if (Domain == INDEX_NONE)
	{
		return false;
	}
	const int32 Source = F.SourceOf(Domain);
	const int32 R = F.State.Board.Tiles[Source].RuneReserve;
	const int32 Total = F.TotalOfType(Domain);

	TestEqual(TEXT("Dar una runa se aplica"), Soulbound::Debug::GiveRune(F.State, 1, Domain, 1, F.Now), EDebugEditResult::Applied);
	TestEqual(TEXT("Dar otra: el Brazal ya tenía una"), Soulbound::Debug::GiveRune(F.State, 1, Domain, 1, F.Now), EDebugEditResult::Applied);
	TestEqual(TEXT("El Brazal sube n"), F.State.Seats[1].RuneCountOf(Domain), 2);
	TestEqual(TEXT("La reserva baja n"), F.State.Board.Tiles[Source].RuneReserve, R - 2);
	TestEqual(TEXT("La suma de Brazales y reserva no cambia al dar"), F.TotalOfType(Domain), Total);

	TestEqual(TEXT("Quitar 2 se aplica"), Soulbound::Debug::TakeRune(F.State, 1, Domain, 2, F.Now), EDebugEditResult::Applied);
	TestEqual(TEXT("El Brazal vuelve a 0"), F.State.Seats[1].RuneCountOf(Domain), 0);
	TestEqual(TEXT("La reserva vuelve a R"), F.State.Board.Tiles[Source].RuneReserve, R);
	TestEqual(TEXT("La suma no cambia al quitar"), F.TotalOfType(Domain), Total);
	TestEqual(TEXT("Quitar deja un RuneReturned por runa (regla ReturnRuneToSource)"), EventsOfType(F.State, EMatchEventType::RuneReturned).Num(), 2);

	// Rechazos: nada cambia, nada se registra.
	const FMatchState Before = F.State;
	TestEqual(TEXT("Más que la reserva"), Soulbound::Debug::GiveRune(F.State, 1, Domain, R + 1, F.Now), EDebugEditResult::NotEnoughReserve);
	TestEqual(TEXT("Quitar más de las que tiene"), Soulbound::Debug::TakeRune(F.State, 1, Domain, 1, F.Now), EDebugEditResult::NotEnoughRunes);
	TestEqual(TEXT("Cantidad cero"), Soulbound::Debug::GiveRune(F.State, 1, Domain, 0, F.Now), EDebugEditResult::InvalidAmount);
	TestEqual(TEXT("Cantidad negativa al quitar"), Soulbound::Debug::TakeRune(F.State, 1, Domain, -1, F.Now), EDebugEditResult::InvalidAmount);
	TestEqual(TEXT("Tipo de runa inexistente"), Soulbound::Debug::GiveRune(F.State, 1, Soulbound::Board::DomainCount, 1, F.Now), EDebugEditResult::InvalidDomain);
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::GiveRune(F.State, 4, Domain, 1, F.Now), EDebugEditResult::InvalidSeat);
	TestTrue(TEXT("Un rechazo no cambia el estado"), SameState(Before, F.State));

	FDebugFixture Primordial;
	if (Primordial.Start(*this))
	{
		const FMatchState PrimordialBefore = Primordial.State;
		TestEqual(TEXT("Con el tablero Primordial se rechaza"), Soulbound::Debug::GiveRune(Primordial.State, 1, Domain, 1, Primordial.Now), EDebugEditResult::BoardPrimordial);
		TestTrue(TEXT("Y no cambia nada"), SameState(PrimordialBefore, Primordial.State));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPlaceTokenTest,
	"Soulbound.Core.Debug.Colocar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugPlaceTokenTest::RunTest(const FString& Parameters)
{
	// CA-DEP-2: colocar una ficha en una casilla válida la deja en ella y nada más cambia (ni Nicho, ni runas, ni
	// bActiveSeatMoved, ni el turno); una casilla inexistente se rechaza. R-INT-1 (sin resolver la casilla).
	FDebugFixture F;
	const int32 Domain = F.Awaken(*this);
	if (Domain == INDEX_NONE)
	{
		return false;
	}
	const FMatchState Before = F.State;
	const int32 Niche = F.State.Board.FindNiche(DomainsWithNiche(F.State, ENicheState::Free)[0]);
	const int32 Source = F.SourceOf(Domain);

	TestEqual(TEXT("Colocar en un Nicho Libre"), Soulbound::Debug::PlaceToken(F.State, 2, Niche, F.Now), EDebugEditResult::Applied);
	TestEqual(TEXT("La ficha está en la casilla"), F.State.Seats[2].TileId, Niche);
	FMatchState Expected = Before;
	Expected.Seats[2].TileId = Niche;
	TestTrue(TEXT("Nada más cambia, salvo el registro"), SameState(Expected, WithoutDebugEvents(F.State)));
	TestEqual(TEXT("El Nicho sigue Libre: no se resolvió"), F.State.Board.Domains[F.State.Board.Tiles[Niche].DomainId].NicheState, ENicheState::Free);

	TestEqual(TEXT("Colocar en una Fuente"), Soulbound::Debug::PlaceToken(F.State, 2, Source, F.Now), EDebugEditResult::Applied);
	TestEqual(TEXT("Sin runa: no se resolvió la Fuente"), F.State.Seats[2].RuneCountOf(Domain), 0);
	TestEqual(TEXT("No cuenta como movimiento del turno"), F.State.bActiveSeatMoved, Before.bActiveSeatMoved);

	const FMatchState Mid = F.State;
	TestEqual(TEXT("Casilla inexistente"), Soulbound::Debug::PlaceToken(F.State, 2, F.State.Board.Tiles.Num(), F.Now), EDebugEditResult::InvalidTile);
	TestEqual(TEXT("Casilla negativa"), Soulbound::Debug::PlaceToken(F.State, 2, -1, F.Now), EDebugEditResult::InvalidTile);
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::PlaceToken(F.State, 9, Niche, F.Now), EDebugEditResult::InvalidSeat);
	TestTrue(TEXT("Un rechazo no cambia el estado"), SameState(Mid, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugBondTest,
	"Soulbound.Core.Debug.Vincular",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugBondTest::RunTest(const FString& Parameters)
{
	// CA-DEP-3: vincular con la edición deja el mismo estado que reclamar el Nicho con ResolveNicheLanding (salvo la
	// posición de la ficha y el registro), incluidos el Despertar del primer vínculo y el fin de carrera del último
	// Nicho Libre; fuera de carrera, con un Elegido o con el Nicho no Libre, se rechaza. R-CAR-4, R-CAR-5, R-TAB-9, R-CAR-1.
	FDebugFixture ByRule;
	FDebugFixture ByEdit;
	if (!ByRule.Start(*this) || !ByEdit.Start(*this))
	{
		return false;
	}
	const TArray<int32> Free = DomainsWithNiche(ByRule.State, ENicheState::Free);
	if (!TestEqual(TEXT("Con cuatro jugadores hay cuatro Nichos Libres"), Free.Num(), 4))
	{
		return false;
	}
	for (int32 Seat = 0; Seat < 4; ++Seat)
	{
		const int32 Domain = Free[Seat];
		ByRule.State.Seats[Seat].TileId = ByRule.State.Board.FindNiche(Domain);
		ByRule.Now += 1.0;
		TestTrue(TEXT("La regla vincula"), Soulbound::Rules::ResolveNicheLanding(ByRule.State, Seat, ByRule.Now, ByRule.Random));
		// La ficha vuelve al Altar: la edición no la mueve, y los dos caminos deben acabar con las fichas en el mismo sitio.
		ByRule.State.Seats[Seat].TileId = ByEdit.State.Seats[Seat].TileId;
		ByEdit.Now += 1.0;
		TestEqual(TEXT("La edición vincula"), Soulbound::Debug::BondSeat(ByEdit.State, Seat, Domain, ByEdit.Now, ByEdit.Random), EDebugEditResult::Applied);
		TestTrue(FString::Printf(TEXT("Vínculo %d: mismo estado salvo ficha y registro"), Seat + 1),
			SameState(WithoutLogAndPosition(ByRule.State, Seat), WithoutLogAndPosition(ByEdit.State, Seat)));
		if (Seat == 0)
		{
			TestTrue(TEXT("El primer vínculo despierta el tablero"), ByEdit.State.Board.Phase == EBoardPhase::Awakened);
			TestEqual(TEXT("Y deja BoardAwakening"), EventsOfType(ByEdit.State, EMatchEventType::BoardAwakening).Num(), 1);
		}
	}
	// Desde S-017 (R-PRO-0) el último vínculo no termina la carrera por sí solo: la termina la máquina de turnos, que antes corta el
	// turno en curso. Sin turno en curso, en su siguiente avance.
	TestTrue(TEXT("El último Nicho Libre deja la carrera por terminar"), Soulbound::Rules::IsBondRaceDue(ByEdit.State, ByEdit.Now) && Soulbound::Rules::IsBondRaceDue(ByRule.State, ByRule.Now));
	Soulbound::Rules::AdvanceTurn(ByEdit.State, ByEdit.Now, ByEdit.Random);
	Soulbound::Rules::AdvanceTurn(ByRule.State, ByRule.Now, ByRule.Random);
	TestTrue(TEXT("Y la máquina la termina, tras la edición"), ByEdit.State.MatchPhase == EMatchPhase::RuneHunt);
	TestTrue(TEXT("Y tras la regla"), ByRule.State.MatchPhase == EMatchPhase::RuneHunt);
	TestEqual(TEXT("Cada vínculo deja su evento de depuración"), DebugEventCount(ByEdit.State), 4);

	// Rechazos.
	FDebugFixture G;
	if (!G.Start(*this))
	{
		return false;
	}
	const TArray<int32> GFree = DomainsWithNiche(G.State, ENicheState::Free);
	const TArray<int32> Closed = DomainsWithNiche(G.State, ENicheState::Closed);
	TestEqual(TEXT("Nicho Cerrado"), Soulbound::Debug::BondSeat(G.State, 0, Closed[0], G.Now, G.Random), EDebugEditResult::NicheNotFree);
	TestEqual(TEXT("Dominio inexistente"), Soulbound::Debug::BondSeat(G.State, 0, 6, G.Now, G.Random), EDebugEditResult::InvalidDomain);
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::BondSeat(G.State, 4, GFree[0], G.Now, G.Random), EDebugEditResult::InvalidSeat);
	TestEqual(TEXT("Un Eco vincula"), Soulbound::Debug::BondSeat(G.State, 0, GFree[0], G.Now, G.Random), EDebugEditResult::Applied);
	const FMatchState Before = G.State;
	TestEqual(TEXT("Nicho Reclamado"), Soulbound::Debug::BondSeat(G.State, 1, GFree[0], G.Now, G.Random), EDebugEditResult::NicheNotFree);
	TestEqual(TEXT("Un Elegido no vuelve a vincularse"), Soulbound::Debug::BondSeat(G.State, 0, GFree[1], G.Now, G.Random), EDebugEditResult::NotLostEcho);
	TestTrue(TEXT("Los rechazos no cambian el estado"), SameState(Before, G.State));
	TestEqual(TEXT("Se termina la carrera"), Soulbound::Debug::EndRace(G.State, G.Now, G.Random), EDebugEditResult::Applied);
	const FMatchState AfterRace = G.State;
	TestEqual(TEXT("Con la carrera terminada se rechaza"), Soulbound::Debug::BondSeat(G.State, 1, GFree[1], G.Now, G.Random), EDebugEditResult::NotInBondRace);
	TestTrue(TEXT("Sin cambios"), SameState(AfterRace, G.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugSealsTest,
	"Soulbound.Core.Debug.Sellos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugSealsTest::RunTest(const FString& Parameters)
{
	// CA-DEP-4: las ediciones de Sello aceptan y rechazan exactamente lo que CanSealTile y ReleaseSeal, con el mismo
	// motivo. Se recorren todas las casillas varias veces, con el tope en 3, para pasar por los motivos. R-TAB-22, R-TAB-23.
	FDebugFixture Mirror;
	if (!Mirror.Start(*this))
	{
		return false;
	}
	Mirror.State.Board.Params.MaxSealedTiles = 3;
	FMatchState Edited = Mirror.State;
	TSet<ESealResult> Seen;
	const int32 Tiles = Mirror.State.Board.Tiles.Num();
	for (int32 Round = 0; Round < 3; ++Round)
	{
		for (int32 Index = 0; Index <= Tiles + 1; ++Index)
		{
			// Las rondas pares suben y las impares bajan: así se pasa por casillas ya selladas.
			const int32 Tile = Round % 2 == 0 ? Index - 1 : Tiles - Index;
			Mirror.Now += 1.0;
			const ESealResult Expected = Soulbound::Rules::CanSealTile(Mirror.State, Tile);
			TestEqual(FString::Printf(TEXT("Sellar %d: mismo motivo"), Tile), Soulbound::Debug::SealTileEdit(Edited, Tile, Mirror.Now), Expected);
			Seen.Add(Expected);
			Soulbound::Rules::SealTile(Mirror.State, Tile, Mirror.Now);
			TestTrue(FString::Printf(TEXT("Sellar %d: mismo estado salvo registro de depuración"), Tile), SameState(Mirror.State, WithoutDebugEvents(Edited)));
		}
	}
	TestTrue(TEXT("Se probó Accepted"), Seen.Contains(ESealResult::Accepted));
	TestTrue(TEXT("Se probó InvalidTile"), Seen.Contains(ESealResult::InvalidTile));
	TestTrue(TEXT("Se probó AlreadySealed"), Seen.Contains(ESealResult::AlreadySealed));
	TestTrue(TEXT("Se probó Altar, Niche o PowerSource"), Seen.Contains(ESealResult::Altar) && Seen.Contains(ESealResult::Niche) && Seen.Contains(ESealResult::PowerSource));

	for (int32 Tile = -1; Tile <= Tiles; ++Tile)
	{
		Mirror.Now += 1.0;
		const bool bValid = Mirror.State.Board.IsValidTile(Tile);
		const bool bWasSealed = bValid && Mirror.State.Board.Tiles[Tile].bSealed;
		const EDebugEditResult Expected = !bValid ? EDebugEditResult::InvalidTile : (bWasSealed ? EDebugEditResult::Applied : EDebugEditResult::NotSealed);
		TestEqual(FString::Printf(TEXT("Liberar %d"), Tile), Soulbound::Debug::UnsealTile(Edited, Tile, Mirror.Now), Expected);
		TestEqual(FString::Printf(TEXT("Liberar %d: la regla acepta lo mismo"), Tile), Soulbound::Rules::ReleaseSeal(Mirror.State, Tile, Mirror.Now), Expected == EDebugEditResult::Applied);
		TestTrue(FString::Printf(TEXT("Liberar %d: mismo estado salvo registro de depuración"), Tile), SameState(Mirror.State, WithoutDebugEvents(Edited)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugEndRaceTest,
	"Soulbound.Core.Debug.FinDeCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugEndRaceTest::RunTest(const FString& Parameters)
{
	// CA-DEP-5: con la carrera en curso, terminarla con la edición deja el estado que deja EndBondRace; fuera de carrera
	// se rechaza. Con y sin Elegidos (victoria del Warden). R-CAR-8, R-TAB-10 a R-TAB-12.
	for (const bool bWithElect : {false, true})
	{
		FDebugFixture ByRule;
		FDebugFixture ByEdit;
		if (!ByRule.Start(*this) || !ByEdit.Start(*this))
		{
			return false;
		}
		if (bWithElect)
		{
			const int32 Domain = DomainsWithNiche(ByRule.State, ENicheState::Free)[0];
			ByRule.State.Seats[0].TileId = ByRule.State.Board.FindNiche(Domain);
			Soulbound::Rules::ResolveNicheLanding(ByRule.State, 0, 1.0, ByRule.Random);
			ByEdit.State = ByRule.State;
			ByEdit.Random = ByRule.Random;
		}
		TestTrue(TEXT("La regla termina la carrera"), Soulbound::Rules::EndBondRace(ByRule.State, 50.0));
		TestEqual(TEXT("La edición termina la carrera"), Soulbound::Debug::EndRace(ByEdit.State, 50.0, ByEdit.Random), EDebugEditResult::Applied);
		TestTrue(bWithElect ? TEXT("Con un Elegido: mismo estado") : TEXT("Sin Elegidos: mismo estado"), SameState(ByRule.State, WithoutDebugEvents(ByEdit.State)));
		TestEqual(TEXT("Deja un evento de depuración"), DebugEventCount(ByEdit.State), 1);

		const FMatchState After = ByEdit.State;
		TestEqual(TEXT("Fuera de carrera se rechaza"), Soulbound::Debug::EndRace(ByEdit.State, 60.0, ByEdit.Random), EDebugEditResult::NotInBondRace);
		TestTrue(TEXT("Y no cambia nada"), SameState(After, ByEdit.State));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugFixedRollTest,
	"Soulbound.Core.Debug.TiradaFijada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugFixedRollTest::RunTest(const FString& Parameters)
{
	// CA-DEP-6: con un valor fijado, la siguiente tirada es ese valor y las alcanzables son las de ese valor; la de
	// después vuelve al sorteo; el sorteo queda en el mismo punto con y sin fijar; un valor fuera de 1 al máximo de dos
	// dados se rechaza. R-TUR-5, R-TUR-7, consumo fijo de S-009.
	FDebugFixture Fixed;
	FDebugFixture Free;
	if (!Fixed.Start(*this, 11) || !Free.Start(*this, 11))
	{
		return false;
	}
	const int32 Value = 5;
	TestEqual(TEXT("Fijar 5"), Soulbound::Debug::FixNextRoll(Fixed.State, Value, 1.0), EDebugEditResult::Applied);

	// Un turno hasta la tirada: BetweenTurns, Start, PreRoll, RollAndChoose.
	for (FDebugFixture* F : {&Fixed, &Free})
	{
		for (int32 Count = 0; Count < 3; ++Count)
		{
			Soulbound::Rules::AdvanceTurn(F->State, 2.0, F->Random);
		}
		TestEqual(TEXT("El turno espera destino"), F->State.TurnPhase, ETurnPhase::RollAndChoose);
	}
	TestEqual(TEXT("La tirada es la fijada"), Fixed.State.Roll, Value);
	const int32 Seat = Fixed.State.ActiveSeat;
	const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(Fixed.State, Seat);
	TestTrue(TEXT("Las alcanzables son las de ese valor"),
		Fixed.State.ReachableOptions == Soulbound::Rules::ReachableTiles(Fixed.State.Board, Fixed.State.Seats[Seat].TileId, Value, Mover));
	TestEqual(TEXT("El Rolled registra la tirada fijada"), EventsOfType(Fixed.State, EMatchEventType::Rolled)[0].Value, Value);
	TestEqual(TEXT("La tirada fijada se consume"), Fixed.State.ForcedRoll, 0);

	// Se acaba el turno de los dos (nadie contesta): el sorteo está en el mismo punto.
	TestTrue(TEXT("El turno fijado termina"), Fixed.FinishTurn());
	TestTrue(TEXT("El turno libre termina"), Free.FinishTurn());
	TestEqual(TEXT("El sorteo de la partida queda en el mismo punto"), Fixed.Random.GetCurrentSeed(), Free.Random.GetCurrentSeed());

	// La tirada siguiente vuelve al sorteo: la del turno sin fijar y la del fijado coinciden.
	Fixed.Step();
	Free.Step();
	TestTrue(TEXT("El segundo turno fijado termina"), Fixed.FinishTurn());
	TestTrue(TEXT("El segundo turno libre termina"), Free.FinishTurn());
	const TArray<FMatchEvent> FixedRolls = EventsOfType(Fixed.State, EMatchEventType::Rolled);
	const TArray<FMatchEvent> FreeRolls = EventsOfType(Free.State, EMatchEventType::Rolled);
	if (TestEqual(TEXT("Hay dos tiradas"), FixedRolls.Num(), 2) && TestEqual(TEXT("En los dos"), FreeRolls.Num(), 2))
	{
		TestEqual(TEXT("La segunda tirada es la del sorteo"), FixedRolls[1].Value, FreeRolls[1].Value);
	}

	// Rechazos y valores límite.
	FDebugFixture G;
	if (!G.Start(*this))
	{
		return false;
	}
	const int32 Max = Soulbound::Debug::MaxFixedRoll();
	TestEqual(TEXT("El máximo es el de dos dados"), Max, Soulbound::Dice::MaxCount * Soulbound::Dice::Faces);
	const FMatchState Before = G.State;
	TestEqual(TEXT("0 se rechaza"), Soulbound::Debug::FixNextRoll(G.State, 0, 1.0), EDebugEditResult::InvalidRoll);
	TestEqual(TEXT("Negativo se rechaza"), Soulbound::Debug::FixNextRoll(G.State, -3, 1.0), EDebugEditResult::InvalidRoll);
	TestEqual(TEXT("Más del máximo se rechaza"), Soulbound::Debug::FixNextRoll(G.State, Max + 1, 1.0), EDebugEditResult::InvalidRoll);
	TestTrue(TEXT("Sin cambios"), SameState(Before, G.State));
	TestEqual(TEXT("1 vale"), Soulbound::Debug::FixNextRoll(G.State, 1, 1.0), EDebugEditResult::Applied);
	TestEqual(TEXT("El máximo vale"), Soulbound::Debug::FixNextRoll(G.State, Max, 1.0), EDebugEditResult::Applied);
	TestEqual(TEXT("Una nueva sustituye a la anterior"), G.State.ForcedRoll, Max);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugNextSeatTest,
	"Soulbound.Core.Debug.SiguienteAsiento",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugNextSeatTest::RunTest(const FString& Parameters)
{
	// CA-DEP-7: el siguiente turno que empieza es del asiento pedido y el orden continúa desde él; un turno extra
	// pendiente del asiento actual se pierde (decisión del autor del 05/10/2026); asiento inexistente, se rechaza. R-TUR-1, R-TUR-9.
	FDebugFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const int32 Target = (F.State.TurnOrder[0] + 2) % 4;
	TestEqual(TEXT("Fijar el siguiente asiento"), Soulbound::Debug::FixNextSeat(F.State, Target, 1.0), EDebugEditResult::Applied);
	F.Step();
	TestEqual(TEXT("El turno que empieza es del asiento pedido"), F.State.ActiveSeat, Target);
	TestEqual(TEXT("Y la petición se consume"), F.State.ForcedNextSeat, INDEX_NONE);
	TestTrue(TEXT("El turno termina"), F.FinishTurn());
	const int32 Index = F.State.TurnOrder.Find(Target);
	F.Step();
	TestEqual(TEXT("El orden continúa desde él"), F.State.ActiveSeat, F.State.TurnOrder[(Index + 1) % 4]);

	// Con un turno extra pendiente del que juega, el pedido gana.
	FDebugFixture E;
	if (!E.Start(*this))
	{
		return false;
	}
	E.Step();
	const int32 Active = E.State.ActiveSeat;
	Soulbound::Rules::GrantExtraTurns(E.State, Active, 1);
	const int32 Other = (Active + 1) % 4;
	TestEqual(TEXT("Fijar con un extra pendiente"), Soulbound::Debug::FixNextSeat(E.State, Other, E.Now), EDebugEditResult::Applied);
	TestTrue(TEXT("El turno termina"), E.FinishTurn());
	E.Step();
	TestEqual(TEXT("El siguiente turno es del pedido, no el extra"), E.State.ActiveSeat, Other);
	TestEqual(TEXT("El extra se perdió"), E.State.Seats[Active].ExtraTurns, 0);

	const FMatchState Before = E.State;
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::FixNextSeat(E.State, 4, E.Now), EDebugEditResult::InvalidSeat);
	TestEqual(TEXT("Asiento negativo"), Soulbound::Debug::FixNextSeat(E.State, -1, E.Now), EDebugEditResult::InvalidSeat);
	TestTrue(TEXT("Sin cambios"), SameState(Before, E.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugEventLogTest,
	"Soulbound.Core.Debug.Registro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugEventLogTest::RunTest(const FString& Parameters)
{
	// CA-DEP-8: toda edición aplicada deja su evento de depuración, con tipo, asiento y marca de tiempo; una rechazada
	// no deja nada. R-TUR-15.
	FDebugFixture F;
	const int32 Domain = F.Awaken(*this);
	if (Domain == INDEX_NONE)
	{
		return false;
	}
	int32 Expected = 0;
	auto Check = [&](const TCHAR* Name, EDebugEditKind Kind, int32 Seat, double Time)
	{
		++Expected;
		const TArray<FMatchEvent> Events = EventsOfType(F.State, EMatchEventType::DebugEdit);
		if (TestEqual(FString::Printf(TEXT("%s: un evento más"), Name), Events.Num(), Expected))
		{
			const FMatchEvent& Event = Events.Last();
			TestEqual(FString::Printf(TEXT("%s: tipo"), Name), Event.Value, static_cast<int32>(Kind));
			TestEqual(FString::Printf(TEXT("%s: asiento"), Name), Event.Seat, Seat);
			TestEqual(FString::Printf(TEXT("%s: marca de tiempo"), Name), Event.ClockSeconds, Time);
		}
	};
	const int32 Seat = 2;
	TestEqual(TEXT("Dar"), Soulbound::Debug::GiveRune(F.State, Seat, Domain, 1, 11.0), EDebugEditResult::Applied);
	Check(TEXT("Dar"), EDebugEditKind::GiveRune, Seat, 11.0);
	TestEqual(TEXT("Quitar"), Soulbound::Debug::TakeRune(F.State, Seat, Domain, 1, 12.0), EDebugEditResult::Applied);
	Check(TEXT("Quitar"), EDebugEditKind::TakeRune, Seat, 12.0);
	TestEqual(TEXT("Colocar"), Soulbound::Debug::PlaceToken(F.State, Seat, 0, 13.0), EDebugEditResult::Applied);
	Check(TEXT("Colocar"), EDebugEditKind::Move, Seat, 13.0);
	TestEqual(TEXT("Vincular"), Soulbound::Debug::BondSeat(F.State, Seat, DomainsWithNiche(F.State, ENicheState::Free)[0], 14.0, F.Random), EDebugEditResult::Applied);
	Check(TEXT("Vincular"), EDebugEditKind::Bond, Seat, 14.0);

	F.State.Board.Params.MaxSealedTiles = 1;
	int32 Sealable = INDEX_NONE;
	for (const FTileState& Tile : F.State.Board.Tiles)
	{
		if (Soulbound::Rules::CanSealTile(F.State, Tile.TileId) == ESealResult::Accepted)
		{
			Sealable = Tile.TileId;
			break;
		}
	}
	TestEqual(TEXT("Sellar"), Soulbound::Debug::SealTileEdit(F.State, Sealable, 15.0), ESealResult::Accepted);
	Check(TEXT("Sellar"), EDebugEditKind::Seal, INDEX_NONE, 15.0);
	TestEqual(TEXT("Liberar"), Soulbound::Debug::UnsealTile(F.State, Sealable, 16.0), EDebugEditResult::Applied);
	Check(TEXT("Liberar"), EDebugEditKind::Unseal, INDEX_NONE, 16.0);
	TestEqual(TEXT("Fijar tirada"), Soulbound::Debug::FixNextRoll(F.State, 3, 17.0), EDebugEditResult::Applied);
	Check(TEXT("Fijar tirada"), EDebugEditKind::FixRoll, F.State.ActiveSeat, 17.0);
	TestEqual(TEXT("Fijar asiento"), Soulbound::Debug::FixNextSeat(F.State, 3, 18.0), EDebugEditResult::Applied);
	Check(TEXT("Fijar asiento"), EDebugEditKind::FixNextSeat, 3, 18.0);
	TestEqual(TEXT("Terminar carrera"), Soulbound::Debug::EndRace(F.State, 19.0, F.Random), EDebugEditResult::Applied);
	Check(TEXT("Terminar carrera"), EDebugEditKind::EndRace, INDEX_NONE, 19.0);

	// Rechazadas: ni un evento más.
	Soulbound::Debug::GiveRune(F.State, Seat, Domain, 1000, 20.0);
	Soulbound::Debug::PlaceToken(F.State, Seat, -1, 20.0);
	Soulbound::Debug::SealTileEdit(F.State, -1, 20.0);
	Soulbound::Debug::UnsealTile(F.State, -1, 20.0);
	Soulbound::Debug::FixNextRoll(F.State, 0, 20.0);
	Soulbound::Debug::FixNextSeat(F.State, 9, 20.0);
	Soulbound::Debug::EndRace(F.State, 20.0, F.Random);
	TestEqual(TEXT("Las rechazadas no dejan evento"), DebugEventCount(F.State), Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugFixTrialTest,
	"Soulbound.Core.Debug.FijarMinijuego",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugFixTrialTest::RunTest(const FString& Parameters)
{
	// CA-DEP-8 (registro) para SbTrial: fijar el resultado del minijuego deja su evento de depuración, con tipo, asiento
	// y marca de tiempo, y no cambia nada más del estado; un asiento inexistente se rechaza sin dejar nada. R-TUR-15.
	FDebugFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const FMatchState Before = F.State;
	TestEqual(TEXT("Fijar el minijuego se aplica"), Soulbound::Debug::FixTrial(F.State, 0, 21.0), EDebugEditResult::Applied);
	const TArray<FMatchEvent> Events = EventsOfType(F.State, EMatchEventType::DebugEdit);
	if (TestEqual(TEXT("Deja un evento de depuración"), Events.Num(), 1))
	{
		TestEqual(TEXT("Tipo"), Events[0].Value, static_cast<int32>(EDebugEditKind::FixTrial));
		TestEqual(TEXT("Asiento"), Events[0].Seat, 0);
		TestEqual(TEXT("Marca de tiempo"), Events[0].ClockSeconds, 21.0);
	}
	TestTrue(TEXT("Nada más cambia"), SameState(Before, WithoutDebugEvents(F.State)));

	const FMatchState Mid = F.State;
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::FixTrial(F.State, 4, 22.0), EDebugEditResult::InvalidSeat);
	TestTrue(TEXT("Un rechazo no deja nada"), SameState(Mid, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugFixDuelTest,
	"Soulbound.Core.Debug.FijarDuelo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugFixDuelTest::RunTest(const FString& Parameters)
{
	// S-012, herramienta SbDuel (con el patrón de SbTrial, CA-DEP-8): fijar el resultado del próximo duelo del humano deja
	// su evento de depuración, con tipo, asiento y marca de tiempo, y no cambia nada más del estado; el resultado vive en el
	// anfitrión. Un asiento inexistente se rechaza sin dejar nada. R-TUR-15.
	FDebugFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const FMatchState Before = F.State;
	TestEqual(TEXT("Fijar el duelo se aplica"), Soulbound::Debug::FixDuel(F.State, 0, 23.0), EDebugEditResult::Applied);
	const TArray<FMatchEvent> Events = EventsOfType(F.State, EMatchEventType::DebugEdit);
	if (TestEqual(TEXT("Deja un evento de depuración"), Events.Num(), 1))
	{
		TestEqual(TEXT("Tipo"), Events[0].Value, static_cast<int32>(EDebugEditKind::FixDuel));
		TestEqual(TEXT("Asiento"), Events[0].Seat, 0);
		TestEqual(TEXT("Marca de tiempo"), Events[0].ClockSeconds, 23.0);
	}
	TestTrue(TEXT("Nada más cambia"), SameState(Before, WithoutDebugEvents(F.State)));

	const FMatchState Mid = F.State;
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::FixDuel(F.State, 4, 24.0), EDebugEditResult::InvalidSeat);
	TestTrue(TEXT("Un rechazo no deja nada"), SameState(Mid, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugVoidEffectsTest,
	"Soulbound.Core.Debug.DesterrarLiberarMandarACasa",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugVoidEffectsTest::RunTest(const FString& Parameters)
{
	// S-013, herramientas SbBanish, SbRelease y SbSendHome (patrón de S-011): cada edición reutiliza la regla del efecto
	// (Banish sin la Furia, ReleaseVoid, SendHome) y deja el mismo estado que ella, con su evento de depuración justo antes de
	// los de la regla; un rechazo devuelve el motivo de la regla y no deja nada. R-EFE-14, R-EFE-17, R-EFE-4, R-CAR-7.
	FDebugFixture F;
	if (F.Awaken(*this) == INDEX_NONE)
	{
		return false;
	}
	// Durante la carrera, el destierro sin la Furia se rechaza con el motivo de la regla.
	const FMatchState InRace = F.State;
	TestEqual(TEXT("En la carrera: el motivo de la regla"), Soulbound::Debug::BanishSeat(F.State, 1, 30.0), EEffectResult::BondRace);
	TestTrue(TEXT("... y no deja nada"), SameState(InRace, F.State));
	TestEqual(TEXT("Termina la carrera"), Soulbound::Debug::EndRace(F.State, 31.0, F.Random), EDebugEditResult::Applied);

	// Desterrar: lo mismo que la regla, con el evento de depuración delante.
	FMatchState Mirror = F.State;
	TestEqual(TEXT("La regla destierra en el espejo"), Soulbound::Rules::Banish(Mirror, 1, false, 32.0), EEffectResult::Applied);
	const int32 Before = F.State.EventLog.Num();
	TestEqual(TEXT("Desterrar se aplica"), Soulbound::Debug::BanishSeat(F.State, 1, 32.0), EEffectResult::Applied);
	TestTrue(TEXT("Desterrar deja el estado de la regla"), SameState(WithoutDebugEvents(Mirror), WithoutDebugEvents(F.State)));
	if (TestTrue(TEXT("Deja dos eventos"), F.State.EventLog.Num() == Before + 2))
	{
		const FMatchEvent& Edit = F.State.EventLog[Before];
		TestTrue(TEXT("Primero el de depuración: tipo, asiento y marca"), Edit.Type == EMatchEventType::DebugEdit && Edit.Value == static_cast<int32>(EDebugEditKind::Banish)
			&& Edit.Seat == 1 && Edit.ClockSeconds == 32.0);
		TestTrue(TEXT("Después el destierro"), F.State.EventLog[Before + 1].Type == EMatchEventType::Banished);
	}

	// Rechazos con el motivo de la regla, sin dejar nada.
	const FMatchState Mid = F.State;
	TestEqual(TEXT("Desterrar a quien ya está dentro: InVoid"), Soulbound::Debug::BanishSeat(F.State, 1, 33.0), EEffectResult::InVoid);
	TestEqual(TEXT("Mandar a casa a quien está dentro: InVoid"), Soulbound::Debug::SendHomeEdit(F.State, 1, 33.0), EEffectResult::InVoid);
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::BanishSeat(F.State, 4, 33.0), EEffectResult::InvalidSeat);
	TestTrue(TEXT("Los rechazos no dejan nada"), SameState(Mid, F.State));

	// Liberación: el evento de depuración, sin asiento, y después la salida.
	const int32 BeforeRelease = F.State.EventLog.Num();
	TestEqual(TEXT("Liberar se aplica"), Soulbound::Debug::ReleaseVoidEdit(F.State, 34.0), EEffectResult::Applied);
	TestEqual(TEXT("El asiento 1 está en su hogar"), F.State.Seats[1].TileId, Soulbound::Rules::HomeTile(F.State, 1));
	if (TestTrue(TEXT("Liberar deja dos eventos"), F.State.EventLog.Num() == BeforeRelease + 2))
	{
		const FMatchEvent& Edit = F.State.EventLog[BeforeRelease];
		TestTrue(TEXT("Primero el de depuración, sin asiento"), Edit.Type == EMatchEventType::DebugEdit && Edit.Value == static_cast<int32>(EDebugEditKind::ReleaseVoid)
			&& Edit.Seat == INDEX_NONE);
		TestTrue(TEXT("Después la salida"), F.State.EventLog[BeforeRelease + 1].Type == EMatchEventType::LeftVoid);
	}
	const FMatchState Empty = F.State;
	TestEqual(TEXT("Liberar sin nadie dentro: NobodyInVoid"), Soulbound::Debug::ReleaseVoidEdit(F.State, 35.0), EEffectResult::NobodyInVoid);
	TestTrue(TEXT("... y no deja nada"), SameState(Empty, F.State));

	// Vuelta al hogar: lo mismo que la regla.
	const int32 Away = F.State.Board.Tiles.IndexOfByPredicate([](const FTileState& Tile) { return Tile.Type == ETileType::Normal; });
	F.State.Seats[2].TileId = Away;
	FMatchState HomeMirror = F.State;
	TestEqual(TEXT("La regla manda a casa en el espejo"), Soulbound::Rules::SendHome(HomeMirror, 2, 36.0), EEffectResult::Applied);
	TestEqual(TEXT("Mandar a casa se aplica"), Soulbound::Debug::SendHomeEdit(F.State, 2, 36.0), EEffectResult::Applied);
	TestTrue(TEXT("Mandar a casa deja el estado de la regla"), SameState(WithoutDebugEvents(HomeMirror), WithoutDebugEvents(F.State)));
	TestEqual(TEXT("El último de depuración es la Vuelta al hogar"), EventsOfType(F.State, EMatchEventType::DebugEdit).Last().Value, static_cast<int32>(EDebugEditKind::SendHome));
	TestEqual(TEXT("Mandar a casa a quien ya está en ella: AlreadyHome"), Soulbound::Debug::SendHomeEdit(F.State, 2, 37.0), EEffectResult::AlreadyHome);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugLaunchEventTest,
	"Soulbound.Core.Debug.LanzarEvento",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugLaunchEventTest::RunTest(const FString& Parameters)
{
	// S-014, herramienta SbEvent (patrón de S-011): la edición lanza el evento con la regla (ApplyEvent) y deja el mismo estado
	// que ella, con su evento de depuración, sin asiento, justo antes de los del evento; un rechazo devuelve el motivo de la
	// regla y no deja nada. El sorteo es el del Warden, nunca el de la partida.
	FDebugFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	FWardenEventDef& Event = F.State.WardenEvents.Events.AddDefaulted_GetRef();
	Event.Id = TEXT("extra");
	Event.Text = TEXT("Un turno extra");
	FEffectApplication Extra;
	Extra.Effect = EEffect::ExtraTurn;
	Extra.Target = EEffectTarget::ActivePlayer;
	Event.Options.AddDefaulted_GetRef().Applications.Add(Extra);

	const FMatchState Before = F.State;
	FRandomStream Warden(11);
	TestEqual(TEXT("Un id que no existe: el motivo de la regla"), Soulbound::Debug::LaunchEvent(F.State, TEXT("no-existe"), 10.0, Warden), EWardenEventStart::UnknownEvent);
	TestTrue(TEXT("... y no deja nada"), SameState(Before, F.State));

	FMatchState Mirror = F.State;
	FRandomStream MirrorWarden(11);
	TestEqual(TEXT("La regla lo lanza en el espejo"), Soulbound::Rules::ApplyEvent(Mirror, TEXT("extra"), 11.0, MirrorWarden), EWardenEventStart::Started);
	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("Lanzar se aplica"), Soulbound::Debug::LaunchEvent(F.State, TEXT("extra"), 11.0, Warden), EWardenEventStart::Started);
	TestTrue(TEXT("Deja el estado de la regla"), SameState(WithoutDebugEvents(Mirror), WithoutDebugEvents(F.State)));
	if (TestTrue(TEXT("Deja eventos"), F.State.EventLog.Num() > From + 1))
	{
		const FMatchEvent& Edit = F.State.EventLog[From];
		TestTrue(TEXT("Primero el de depuración, sin asiento"), Edit.Type == EMatchEventType::DebugEdit && Edit.Value == static_cast<int32>(EDebugEditKind::LaunchEvent)
			&& Edit.Seat == INDEX_NONE);
		TestTrue(TEXT("Después el del evento"), F.State.EventLog[From + 1].Type == EMatchEventType::WardenEventStarted);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugInsolenceTest,
	"Soulbound.Core.Debug.FijarInsolencia",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugInsolenceTest::RunTest(const FString& Parameters)
{
	// S-016 (SbInsolence, patrón de S-011): fija la insolencia de un asiento, de 0 a 4, sin castigo, y deja su DebugEdit; fuera
	// de rango o con un asiento que no existe, se rechaza sin tocar nada.
	FDebugFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const int32 Before = F.State.EventLog.Num();
	TestEqual(TEXT("Fijar 4 se aplica"), Soulbound::Debug::SetInsolence(F.State, 2, 4, F.Now), EDebugEditResult::Applied);
	TestEqual(TEXT("Insolencia 4"), F.State.Seats[2].Insolence, 4);
	if (TestEqual(TEXT("Un solo evento"), F.State.EventLog.Num(), Before + 1))
	{
		const FMatchEvent& Edit = F.State.EventLog.Last();
		TestTrue(TEXT("El de la edición, con el asiento"), Edit.Type == EMatchEventType::DebugEdit && Edit.Seat == 2
			&& Edit.Value == static_cast<int32>(EDebugEditKind::SetInsolence));
	}
	const FMatchState Fixed = F.State;
	TestEqual(TEXT("5 está fuera de rango: es la Furia, no un nivel"), Soulbound::Debug::SetInsolence(F.State, 2, Soulbound::Match::MaxInsolence, F.Now), EDebugEditResult::InvalidAmount);
	TestEqual(TEXT("Negativo, fuera de rango"), Soulbound::Debug::SetInsolence(F.State, 2, -1, F.Now), EDebugEditResult::InvalidAmount);
	TestEqual(TEXT("Asiento inexistente"), Soulbound::Debug::SetInsolence(F.State, 4, 1, F.Now), EDebugEditResult::InvalidSeat);
	TestTrue(TEXT("Un rechazo no cambia el estado"), SameState(Fixed, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugClockTest,
	"Soulbound.Core.Debug.FijarReloj",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDebugClockTest::RunTest(const FString& Parameters)
{
	// S-018 (SbClock, patrón de S-011): adelanta el reloj de partida y deja su DebugEdit, sin asiento, en el instante de la edición;
	// hacia atrás (decisión del autor del 27/10/2026), se rechaza con InvalidAmount sin tocar nada.
	FDebugFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.State.ClockSeconds = F.Now;
	const int32 Before = F.State.EventLog.Num();
	TestEqual(TEXT("Adelantar a 2390 s se aplica"), Soulbound::Debug::SetClock(F.State, 2390.0, F.Now), EDebugEditResult::Applied);
	TestEqual(TEXT("El reloj marca 2390 s"), F.State.ClockSeconds, 2390.0);
	if (TestEqual(TEXT("Un solo evento"), F.State.EventLog.Num(), Before + 1))
	{
		const FMatchEvent& Edit = F.State.EventLog.Last();
		TestTrue(TEXT("El de la edición, sin asiento, en el instante de la edición"), Edit.Type == EMatchEventType::DebugEdit && Edit.Seat == INDEX_NONE
			&& Edit.Value == static_cast<int32>(EDebugEditKind::SetClock) && Edit.ClockSeconds == F.Now);
	}
	const FMatchState Moved = F.State;
	TestEqual(TEXT("Hacia atrás, se rechaza"), Soulbound::Debug::SetClock(F.State, 100.0, 2390.0), EDebugEditResult::InvalidAmount);
	TestEqual(TEXT("Negativo, se rechaza"), Soulbound::Debug::SetClock(F.State, -1.0, 2390.0), EDebugEditResult::InvalidAmount);
	TestTrue(TEXT("Un rechazo no cambia el estado"), SameState(Moved, F.State));
	TestEqual(TEXT("Al mismo instante, se aplica (no retrocede)"), Soulbound::Debug::SetClock(F.State, 2390.0, 2390.0), EDebugEditResult::Applied);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
