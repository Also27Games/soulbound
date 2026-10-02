// Tests de runas, Fuentes y poderes (S-008, GDD 04 con las reglas de Fuente de los GDD 01 y 03). Cada test cita el
// criterio de aceptación (CA-XXX-n) y la regla (R-XXX-n) que cubre. Se lanzan sin editor con Scripts/test.ps1.
// Filtro: "Soulbound.Core.Runes".
//
// Montaje: tablero de 35 casillas, cuatro asientos, el asiento 0 se vincula al primer Dominio con Nicho Libre (así el
// tablero despierta de verdad, por ResolveNicheLanding) y los demás siguen Ecos. Los asientos terminan su movimiento con
// LandOn, que pasa por la máquina de turnos; el minijuego se resuelve con SubmitSkillTrialResult.

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/MatchStart.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::EventsOfType;
	using Soulbound::Tests::LandOn;

	constexpr int32 Elect = 0;
	constexpr int32 FirstEcho = 1;

	/** Partida de cuatro sobre el tablero de 35 casillas, con los Elegidos del archivo de datos. */
	struct FRuneFixture
	{
		FMatchState State;
		FRandomStream Random{1};
		double Now = 1.0;
		/** Dominio del Elegido (el asiento 0) tras Awaken. */
		int32 ElectDomain = INDEX_NONE;

		bool Start(FAutomationTestBase& Test)
		{
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

		/** El asiento 0 reclama el Nicho del primer Dominio con dueño posible: el tablero despierta y las Fuentes se llenan. */
		bool Awaken(FAutomationTestBase& Test)
		{
			if (!Start(Test))
			{
				return false;
			}
			ElectDomain = DomainOf(EDomainState::Owned);
			return Test.TestTrue(TEXT("El asiento 0 reclama el Nicho"), Land(Elect, State.Board.FindNiche(ElectDomain)))
				&& Test.TestTrue(TEXT("El tablero despierta"), State.Board.Phase == EBoardPhase::Awakened);
		}

		/** Termina el movimiento de Seat en TileId, un segundo después del anterior. Devuelve LandOn. */
		bool Land(int32 Seat, int32 TileId)
		{
			Now += 1.0;
			return LandOn(State, Random, Seat, TileId, Now);
		}

		/** Resuelve el minijuego pendiente de Seat un segundo después. */
		EIntentResult Trial(int32 Seat, bool bPassed)
		{
			Now += 1.0;
			return Soulbound::Rules::SubmitSkillTrialResult(State, Seat, bPassed, Now);
		}

		/** El turno de Seat empieza y termina sin que se mueva (sin casillas alcanzables, R-TAB-18). */
		void StayPut(int32 Seat)
		{
			Now += 1.0;
			State.ActiveSeat = Seat;
			State.TurnPhase = ETurnPhase::BetweenTurns;
			for (int32 Step = 0; Step < 3; ++Step)
			{
				Soulbound::Rules::AdvanceTurn(State, Now, Random);
			}
			State.ReachableOptions.Reset();
			for (int32 Step = 0; Step < 3; ++Step)
			{
				Soulbound::Rules::AdvanceTurn(State, Now, Random);
			}
		}

		/** Primer Dominio en el estado indicado que no sea Except. */
		int32 DomainOf(EDomainState DomainState, int32 Except = INDEX_NONE) const
		{
			for (const FDomainState& Domain : State.Board.Domains)
			{
				if (Domain.State == DomainState && Domain.DomainId != Except)
				{
					return Domain.DomainId;
				}
			}
			return INDEX_NONE;
		}

		/** Primera Fuente de Poder del Dominio. */
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

		int32 Reserve(int32 SourceTile) const { return State.Board.Tiles[SourceTile].RuneReserve; }
		int32 Runes(int32 Seat, int32 RuneType) const { return State.Seats[Seat].RuneCountOf(RuneType); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesObtainOnArrivalTest,
	"Soulbound.Core.Runes.UnaRunaPorLlegada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesObtainOnArrivalTest::RunTest(const FString& Parameters)
{
	// CA-RUN-1 (R-RUN-2, R-RUN-3): dado el tablero Despierto y una Fuente con reserva R, cuando un Elegido termina su
	// movimiento en ella, tiene una runa más de ese tipo y la reserva es R menos 1; cuando empieza su siguiente turno sin
	// haberse movido, no obtiene otra; cuando sale y vuelve, sí. Y la reserva arranca al Despertar con el valor del tablero.
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 Domain = F.ElectDomain;
	const int32 Source = F.SourceOf(Domain);
	const int32 R = F.State.Board.Params.RuneReservePerSource;
	TestTrue(TEXT("El valor de datos de la reserva es mayor que cero"), R > 0);
	for (const FTileState& Tile : F.State.Board.Tiles)
	{
		TestEqual(FString::Printf(TEXT("Casilla %d: reserva al Despertar"), Tile.TileId), Tile.RuneReserve, Tile.Type == ETileType::PowerSource ? R : 0);
	}

	TestTrue(TEXT("El Elegido llega a su Fuente"), F.Land(Elect, Source));
	TestEqual(TEXT("Una runa más de ese tipo"), F.Runes(Elect, Domain), 1);
	TestEqual(TEXT("La reserva baja uno"), F.Reserve(Source), R - 1);

	F.StayPut(Elect);
	TestEqual(TEXT("Sin moverse no obtiene otra"), F.Runes(Elect, Domain), 1);
	TestEqual(TEXT("Sin moverse la reserva no baja"), F.Reserve(Source), R - 1);

	TestTrue(TEXT("Sale"), F.Land(Elect, F.State.Board.Tiles[Source].Neighbours[0]));
	TestTrue(TEXT("Y vuelve"), F.Land(Elect, Source));
	TestEqual(TEXT("Al volver obtiene otra"), F.Runes(Elect, Domain), 2);
	TestEqual(TEXT("Al volver la reserva baja otro"), F.Reserve(Source), R - 2);
	TestEqual(TEXT("Dos eventos de runa obtenida"), EventsOfType(F.State, EMatchEventType::RuneObtained).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesDepletedSourceTest,
	"Soulbound.Core.Runes.FuenteAgotada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesDepletedSourceTest::RunTest(const FString& Parameters)
{
	// CA-RUN-2 (R-RUN-4): dada una Fuente con reserva cero, cuando un asiento termina en ella, no obtiene runa y el
	// registro lleva el evento de Fuente agotada. Un Elegido en una Fuente con dueño y un Eco en la de un Olvidado.
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 Owned = F.ElectDomain;
	const int32 Forgotten = F.DomainOf(EDomainState::Forgotten);
	const int32 OwnedSource = F.SourceOf(Owned);
	const int32 ForgottenSource = F.SourceOf(Forgotten);
	F.State.Board.Tiles[OwnedSource].RuneReserve = 0;
	F.State.Board.Tiles[ForgottenSource].RuneReserve = 0;

	TestTrue(TEXT("El Elegido llega a la Fuente agotada"), F.Land(Elect, OwnedSource));
	TestEqual(TEXT("El Elegido no obtiene runa"), F.Runes(Elect, Owned), 0);
	TestEqual(TEXT("La reserva sigue en cero"), F.Reserve(OwnedSource), 0);
	TArray<FMatchEvent> Depleted = EventsOfType(F.State, EMatchEventType::SourceDepleted);
	if (TestEqual(TEXT("Un evento de Fuente agotada"), Depleted.Num(), 1))
	{
		TestEqual(TEXT("... del asiento del Elegido"), Depleted[0].Seat, Elect);
		TestEqual(TEXT("... con el tipo de runa"), Depleted[0].Value, Owned);
	}

	TestTrue(TEXT("Un Eco llega a la Fuente agotada de un Olvidado"), F.Land(FirstEcho, ForgottenSource));
	TestEqual(TEXT("El Eco no obtiene runa"), F.Runes(FirstEcho, Forgotten), 0);
	Depleted = EventsOfType(F.State, EMatchEventType::SourceDepleted);
	if (TestEqual(TEXT("Dos eventos de Fuente agotada"), Depleted.Num(), 2))
	{
		TestEqual(TEXT("... el segundo, del Eco"), Depleted[1].Seat, FirstEcho);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesReturnToSourceTest,
	"Soulbound.Core.Runes.DevolverUnaRuna",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesReturnToSourceTest::RunTest(const FString& Parameters)
{
	// CA-RUN-3 y CA-TAB-5 (R-RUN-6, R-TAB-4b): dado un asiento con una runa de un tipo, cuando la devuelve con la función
	// del motor, tiene una menos y la reserva de esa Fuente sube en uno; sin runas de ese tipo, la función rechaza y
	// nada cambia.
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 Domain = F.ElectDomain;
	const int32 Source = F.SourceOf(Domain);
	const int32 R = F.State.Board.Params.RuneReservePerSource;
	TestTrue(TEXT("El Elegido obtiene una runa"), F.Land(Elect, Source));

	TestTrue(TEXT("Devolver la runa"), Soulbound::Rules::ReturnRuneToSource(F.State, Elect, Domain, Source, F.Now + 1.0));
	TestEqual(TEXT("Una runa menos"), F.Runes(Elect, Domain), 0);
	TestEqual(TEXT("La reserva sube uno: vuelve a su valor"), F.Reserve(Source), R);
	const TArray<FMatchEvent> Returned = EventsOfType(F.State, EMatchEventType::RuneReturned);
	if (TestEqual(TEXT("Un evento de runa devuelta"), Returned.Num(), 1))
	{
		TestEqual(TEXT("... con el asiento"), Returned[0].Seat, Elect);
		TestEqual(TEXT("... el tipo"), Returned[0].Value, Domain);
		TestEqual(TEXT("... y la marca de tiempo"), Returned[0].ClockSeconds, F.Now + 1.0);
	}

	const FMatchState Before = F.State;
	TestFalse(TEXT("Sin runas de ese tipo, rechaza"), Soulbound::Rules::ReturnRuneToSource(F.State, Elect, Domain, Source, F.Now + 2.0));
	TestTrue(TEXT("... y nada cambia"), Soulbound::Tests::SameState(Before, F.State));

	F.State.Seats[Elect].Runes[Domain] = 1;
	const FMatchState WithRune = F.State;
	TestFalse(TEXT("Una casilla que no es una Fuente, rechaza"), Soulbound::Rules::ReturnRuneToSource(F.State, Elect, Domain, F.State.Board.FindAltar(), F.Now + 3.0));
	TestFalse(TEXT("Una Fuente de otro Dominio, rechaza"), Soulbound::Rules::ReturnRuneToSource(F.State, Elect, Domain, F.SourceOf((Domain + 1) % 6), F.Now + 3.0));
	TestFalse(TEXT("Un tipo de runa que no existe, rechaza"), Soulbound::Rules::ReturnRuneToSource(F.State, Elect, 6, Source, F.Now + 3.0));
	TestTrue(TEXT("... y nada cambia"), Soulbound::Tests::SameState(WithRune, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesDuplicatesGiveNothingTest,
	"Soulbound.Core.Runes.LosDuplicadosNoDanNada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesDuplicatesGiveNothingTest::RunTest(const FString& Parameters)
{
	// CA-RUN-5 (R-RUN-5): dado un asiento con dos runas de un tipo, sus poderes activos son los mismos que con una y sus
	// alcanzables no cambian. Con la runa que da Paso Libre, para que haya algo que comparar.
	FRuneFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	// Shuten-dōji (Dominio 0): su runa 1 da Paso Libre. Un Sello junto al Altar da algo que ver en los alcanzables.
	F.State.Seats[Elect].Role = ESeatRole::Elect;
	F.State.Seats[Elect].DomainId = 0;
	const int32 Altar = F.State.Board.FindAltar();
	const int32 Sealed = F.State.Board.Tiles[Altar].Neighbours[0];
	F.State.Board.Tiles[Sealed].bSealed = true;
	F.State.Board.SealedTiles.Add(Sealed);

	// Con tirada 1, la casilla sellada solo es alcanzable con Paso Libre: así hay algo que comparar.
	auto ReachableNow = [&F, Altar]() { return Soulbound::Rules::ReachableTiles(F.State.Board, Altar, 1, Soulbound::Rules::MakeMoverTraits(F.State, Elect)); };
	const TArray<int32> ReachableWithNone = ReachableNow();
	F.State.Seats[Elect].Runes[1] = 1;
	const TArray<EPower> PowersWithOne = Soulbound::Rules::GetActivePowers(F.State, Elect);
	const TArray<int32> ReachableWithOne = ReachableNow();
	F.State.Seats[Elect].Runes[1] = 2;
	const TArray<EPower> PowersWithTwo = Soulbound::Rules::GetActivePowers(F.State, Elect);
	const TArray<int32> ReachableWithTwo = ReachableNow();

	TestTrue(TEXT("Con una runa, solo Paso Libre"), PowersWithOne == TArray<EPower>{EPower::FreePassage});
	TestTrue(TEXT("Con dos, los mismos poderes"), PowersWithTwo == PowersWithOne);
	TestTrue(TEXT("Con dos, los mismos alcanzables"), ReachableWithTwo == ReachableWithOne);
	TestTrue(TEXT("La primera runa sí cambió los alcanzables: la casilla sellada"), ReachableWithOne.Contains(Sealed) && !ReachableWithNone.Contains(Sealed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesSkillTrialTest,
	"Soulbound.Core.Runes.MinijuegoEnLaFuenteDeUnOlvidado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesSkillTrialTest::RunTest(const FString& Parameters)
{
	// CA-TAB-8 (R-TAB-7): dado un Elegido que termina en la Fuente de un Olvidado, cuando el decisor falla el minijuego,
	// no obtiene runa y la reserva no baja; cuando lo supera, la obtiene. En una Fuente con dueño no se pregunta al
	// decisor. La máquina espera en SkillTrial como espera el destino (decisión del autor, S-008).
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 Forgotten = F.DomainOf(EDomainState::Forgotten);
	const int32 Source = F.SourceOf(Forgotten);
	const int32 R = F.State.Board.Params.RuneReservePerSource;

	TestTrue(TEXT("El Elegido llega a la Fuente de un Olvidado"), F.Land(Elect, Source));
	TestEqual(TEXT("La máquina espera la prueba"), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::SkillTrial));
	TestEqual(TEXT("Prueba pendiente: la de la Fuente de un Olvidado"), static_cast<int32>(F.State.PendingTrial), static_cast<int32>(ESkillTrial::ForgottenSource));
	TestEqual(TEXT("... fácil"), static_cast<int32>(F.State.PendingDifficulty), static_cast<int32>(ESkillDifficulty::Easy));
	TestEqual(TEXT("Antes del resultado, sin runa"), F.Runes(Elect, Forgotten), 0);
	const FMatchState Waiting = F.State;
	TestFalse(TEXT("AdvanceTurn no avanza mientras espera"), Soulbound::Rules::AdvanceTurn(F.State, F.Now + 100.0, F.Random));
	TestTrue(TEXT("... y no cambia nada"), Soulbound::Tests::SameState(Waiting, F.State));
	TestEqual(TEXT("Otro asiento no puede responder"), static_cast<int32>(Soulbound::Rules::SubmitSkillTrialResult(F.State, FirstEcho, true, F.Now + 1.0)), static_cast<int32>(EIntentResult::WrongSeat));
	TestTrue(TEXT("... y no cambia nada"), Soulbound::Tests::SameState(Waiting, F.State));

	FMatchState NoPending = F.State;
	NoPending.PendingTrial = ESkillTrial::None;
	const FMatchState NoPendingBefore = NoPending;
	TestEqual(TEXT("En SkillTrial pero sin prueba pendiente, el resultado se rechaza por fase"), static_cast<int32>(Soulbound::Rules::SubmitSkillTrialResult(NoPending, Elect, true, F.Now + 1.0)), static_cast<int32>(EIntentResult::WrongPhase));
	TestTrue(TEXT("... y no cambia nada"), Soulbound::Tests::SameState(NoPendingBefore, NoPending));

	TestEqual(TEXT("Falla el minijuego: aceptado"), static_cast<int32>(F.Trial(Elect, false)), static_cast<int32>(EIntentResult::Accepted));
	TestEqual(TEXT("Falla: sin runa"), F.Runes(Elect, Forgotten), 0);
	TestEqual(TEXT("Falla: la reserva no baja"), F.Reserve(Source), R);
	TestEqual(TEXT("Falla: ya no hay prueba pendiente"), static_cast<int32>(F.State.PendingTrial), static_cast<int32>(ESkillTrial::None));
	TestEqual(TEXT("Falla: el turno termina"), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::End));
	Soulbound::Rules::AdvanceTurn(F.State, F.Now, F.Random);
	TestEqual(TEXT("... y pasa a entre turnos"), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::BetweenTurns));
	F.State.ActiveSeat = Elect;
	TestEqual(TEXT("Fuera de la prueba, el resultado se rechaza por fase"), static_cast<int32>(F.Trial(Elect, true)), static_cast<int32>(EIntentResult::WrongPhase));

	TestTrue(TEXT("Sale y vuelve a la Fuente"), F.Land(Elect, F.State.Board.Tiles[Source].Neighbours[0]) && F.Land(Elect, Source));
	TestEqual(TEXT("Otra vez espera la prueba"), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::SkillTrial));
	TestEqual(TEXT("Supera el minijuego: aceptado"), static_cast<int32>(F.Trial(Elect, true)), static_cast<int32>(EIntentResult::Accepted));
	TestEqual(TEXT("Supera: obtiene la runa"), F.Runes(Elect, Forgotten), 1);
	TestEqual(TEXT("Supera: la reserva baja uno"), F.Reserve(Source), R - 1);

	// Una Fuente con dueño, propia o ajena, no pregunta al decisor.
	const int32 OtherOwned = F.DomainOf(EDomainState::Owned, F.ElectDomain);
	for (const int32 Domain : {F.ElectDomain, OtherOwned})
	{
		TestTrue(FString::Printf(TEXT("Llega a la Fuente del Dominio %d, con dueño"), Domain), F.Land(Elect, F.SourceOf(Domain)));
		TestEqual(FString::Printf(TEXT("Dominio %d: no hay prueba"), Domain), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::BetweenTurns));
		TestEqual(FString::Printf(TEXT("Dominio %d: obtiene la runa"), Domain), F.Runes(Elect, Domain), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesBoardPhaseAndEchoesTest,
	"Soulbound.Core.Runes.PrimordialYEcosEnCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesBoardPhaseAndEchoesTest::RunTest(const FString& Parameters)
{
	// CA-TAB-9 (R-TAB-8): dado el tablero en Primordial, nadie obtiene runa en una Fuente.
	{
		FRuneFixture F;
		if (!F.Start(*this))
		{
			return false;
		}
		const int32 Source = F.SourceOf(F.DomainOf(EDomainState::Forgotten));
		TestTrue(TEXT("Primordial: un Eco llega a una Fuente"), F.Land(FirstEcho, Source));
		TestEqual(TEXT("Primordial: sin runa"), F.Runes(FirstEcho, F.State.Board.Tiles[Source].DomainId), 0);
		TestEqual(TEXT("Primordial: la reserva no se toca"), F.Reserve(Source), 0);
		TestEqual(TEXT("Primordial: ningún evento de runa"), EventsOfType(F.State, EMatchEventType::RuneObtained).Num() + EventsOfType(F.State, EMatchEventType::SourceDepleted).Num(), 0);
	}

	// CA-CAR-7b (R-CAR-7b, R-ECO-3): dado el tablero Despierto durante la carrera, un Eco en la Fuente de un Olvidado con
	// reserva obtiene la runa sin minijuego; en la de un Dominio con dueño, nada.
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 R = F.State.Board.Params.RuneReservePerSource;
	const int32 Forgotten = F.DomainOf(EDomainState::Forgotten);
	const int32 ForgottenSource = F.SourceOf(Forgotten);
	TestTrue(TEXT("Un Eco llega a la Fuente de un Olvidado"), F.Land(FirstEcho, ForgottenSource));
	TestEqual(TEXT("Sin minijuego"), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::BetweenTurns));
	TestEqual(TEXT("Obtiene la runa"), F.Runes(FirstEcho, Forgotten), 1);
	TestEqual(TEXT("La reserva baja uno"), F.Reserve(ForgottenSource), R - 1);

	const int32 OtherOwned = F.DomainOf(EDomainState::Owned, F.ElectDomain);
	for (const int32 Domain : {F.ElectDomain, OtherOwned})
	{
		const int32 Source = F.SourceOf(Domain);
		TestTrue(FString::Printf(TEXT("Un Eco llega a la Fuente del Dominio %d, con dueño"), Domain), F.Land(FirstEcho, Source));
		TestEqual(FString::Printf(TEXT("Dominio %d: el Eco no obtiene nada"), Domain), F.Runes(FirstEcho, Domain), 0);
		TestEqual(FString::Printf(TEXT("Dominio %d: la reserva no baja"), Domain), F.Reserve(Source), R);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesEchoAfterRaceTest,
	"Soulbound.Core.Runes.EcoTrasLaCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesEchoAfterRaceTest::RunTest(const FString& Parameters)
{
	// CA-ECO-1 y CA-ECO-2 (R-ECO-1, R-ECO-3): dado un Eco tras la carrera con runas de otros tipos, no tiene ningún poder
	// activo; en la Fuente de un Olvidado obtiene sin minijuego mientras un Elegido en la misma lo juega.
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now, F.Random));
	const int32 Home = F.State.Seats[FirstEcho].DomainId;
	if (!TestTrue(TEXT("El Eco tiene un Dominio Olvidado como hogar"), F.State.Board.Domains.IsValidIndex(Home)))
	{
		return false;
	}
	for (int32 RuneType = 0; RuneType < 6; ++RuneType)
	{
		F.State.Seats[FirstEcho].Runes[RuneType] = RuneType == Home ? 0 : 1;
	}
	TestTrue(TEXT("Con runas de otros tipos, ningún poder activo"), Soulbound::Rules::GetActivePowers(F.State, FirstEcho).IsEmpty());

	const int32 Forgotten = F.DomainOf(EDomainState::Forgotten);
	const int32 Source = F.SourceOf(Forgotten);
	const int32 R = F.State.Board.Params.RuneReservePerSource;
	TestTrue(TEXT("El Eco llega a la Fuente de un Olvidado"), F.Land(FirstEcho, Source));
	TestEqual(TEXT("Obtiene sin minijuego"), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::BetweenTurns));
	TestEqual(TEXT("... la runa de esa Fuente"), F.Runes(FirstEcho, Forgotten), Forgotten == Home ? 1 : 2);
	TestEqual(TEXT("... y la reserva baja uno"), F.Reserve(Source), R - 1);

	TestTrue(TEXT("Un Elegido llega a la misma Fuente"), F.Land(Elect, Source));
	TestEqual(TEXT("El Elegido sí juega el minijuego"), static_cast<int32>(F.State.TurnPhase), static_cast<int32>(ETurnPhase::SkillTrial));
	TestEqual(TEXT("... y aún no obtiene"), F.Runes(Elect, Forgotten), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesLiberationRuneTest,
	"Soulbound.Core.Runes.RunaDeLiberacion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesLiberationRuneTest::RunTest(const FString& Parameters)
{
	// CA-ECO-6 (parte) (R-ECO-7): dado un Eco con una runa de su Dominio asignado, Celeridad está activo; con cero, no;
	// con dos, perder una no lo quita. Un Eco durante la carrera no tiene poderes.
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	F.State.Seats[FirstEcho].Runes[2] = 1;
	TestTrue(TEXT("Durante la carrera, un Eco no tiene poderes"), Soulbound::Rules::GetActivePowers(F.State, FirstEcho).IsEmpty());
	F.State.Seats[FirstEcho].Runes[2] = 0;

	TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now, F.Random));
	const int32 Home = F.State.Seats[FirstEcho].DomainId;
	if (!TestTrue(TEXT("El Eco tiene un Dominio Olvidado como hogar"), F.State.Board.Domains.IsValidIndex(Home)))
	{
		return false;
	}
	const int32 Source = F.SourceOf(Home);
	auto HasHaste = [&F]() { return Soulbound::Rules::IsPowerActive(F.State, FirstEcho, EPower::Haste); };

	TestFalse(TEXT("Sin runas: Celeridad no está activo"), HasHaste());
	F.State.Seats[FirstEcho].Runes[Home] = 1;
	TestTrue(TEXT("Con la Runa de Liberación: Celeridad activo"), HasHaste());
	TestTrue(TEXT("... y es el único poder"), Soulbound::Rules::GetActivePowers(F.State, FirstEcho) == TArray<EPower>{EPower::Haste});
	F.State.Seats[FirstEcho].Runes[Home] = 2;
	TestTrue(TEXT("Con dos: Celeridad activo"), HasHaste());
	TestTrue(TEXT("Pierde una"), Soulbound::Rules::ReturnRuneToSource(F.State, FirstEcho, Home, Source, F.Now + 1.0));
	TestTrue(TEXT("Con una menos, sigue activo"), HasHaste());
	TestTrue(TEXT("Pierde la última"), Soulbound::Rules::ReturnRuneToSource(F.State, FirstEcho, Home, Source, F.Now + 2.0));
	TestFalse(TEXT("Sin ninguna: Celeridad no está activo"), HasHaste());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesElectPowerLifecycleTest,
	"Soulbound.Core.Runes.UnPoderDuraLoQueLaUltimaRuna",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesElectPowerLifecycleTest::RunTest(const FString& Parameters)
{
	// CA-ELE-9 (R-ELE-5): dado un Elegido con una sola runa de un tipo, cuando la pierde, el poder deja de estar activo;
	// cuando obtiene otra, vuelve. Y la runa de su propio Dominio le da Celeridad (R-ELE-3).
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 Own = F.ElectDomain;
	const int32 Other = F.DomainOf(EDomainState::Owned, Own);
	const EPower Power = F.State.Elects.Elects[Own].RuneToPower[Other];
	TestFalse(TEXT("Sin runas, el poder no está activo"), Soulbound::Rules::IsPowerActive(F.State, Elect, Power));

	TestTrue(TEXT("Obtiene su primera runa de ese tipo"), F.Land(Elect, F.SourceOf(Other)));
	TestTrue(TEXT("El poder está activo"), Soulbound::Rules::IsPowerActive(F.State, Elect, Power));
	TestTrue(TEXT("Pierde la única"), Soulbound::Rules::ReturnRuneToSource(F.State, Elect, Other, F.SourceOf(Other), F.Now + 1.0));
	TestFalse(TEXT("El poder deja de estar activo"), Soulbound::Rules::IsPowerActive(F.State, Elect, Power));
	TestTrue(TEXT("Sale y obtiene otra"), F.Land(Elect, F.State.Board.Tiles[F.SourceOf(Other)].Neighbours[0]) && F.Land(Elect, F.SourceOf(Other)));
	TestTrue(TEXT("El poder vuelve"), Soulbound::Rules::IsPowerActive(F.State, Elect, Power));

	TestFalse(TEXT("Celeridad aún no"), Soulbound::Rules::IsPowerActive(F.State, Elect, EPower::Haste));
	TestTrue(TEXT("Obtiene la runa de su Dominio"), F.Land(Elect, F.SourceOf(Own)));
	TestTrue(TEXT("Celeridad activo"), Soulbound::Rules::IsPowerActive(F.State, Elect, EPower::Haste));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesFreePassageTest,
	"Soulbound.Core.Runes.PasoLibre",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesFreePassageTest::RunTest(const FString& Parameters)
{
	// CA-ELE-2 y CA-TAB-18 (R-ELE-2, R-POD-2): dado Fenrir sin runas, sus alcanzables cuentan las selladas como
	// transitables; dado otro Elegido con la runa que le da Paso Libre, igual; sin ella, no. La máquina de turnos usa el
	// mismo cálculo (MakeMoverTraits).
	FRuneFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const int32 Altar = F.State.Board.FindAltar();
	const int32 Sealed = F.State.Board.Tiles[Altar].Neighbours[0];
	F.State.Board.Tiles[Sealed].bSealed = true;
	F.State.Board.SealedTiles.Add(Sealed);

	// Asiento 0: Fenrir (Dominio 1). Asiento 1: Shuten-dōji (Dominio 0), su runa 1 da Paso Libre. Asiento 2: Apep
	// (Dominio 2), su runa 3 da Paso Libre.
	const int32 Domains[] = {1, 0, 2};
	for (int32 Seat = 0; Seat < 3; ++Seat)
	{
		F.State.Seats[Seat].Role = ESeatRole::Elect;
		F.State.Seats[Seat].DomainId = Domains[Seat];
	}
	auto Traits = [&F](int32 Seat) { return Soulbound::Rules::MakeMoverTraits(F.State, Seat); };
	auto Reaches = [&F, &Traits, Altar, Sealed](int32 Seat) { return Soulbound::Rules::ReachableTiles(F.State.Board, Altar, 1, Traits(Seat)).Contains(Sealed); };

	TestTrue(TEXT("Fenrir sin runas: Paso Libre por afinidad"), Traits(0).bFreePassage);
	TestTrue(TEXT("Fenrir sin runas: alcanza la casilla sellada"), Reaches(0));
	TestFalse(TEXT("Shuten-dōji sin runas: sin Paso Libre"), Traits(1).bFreePassage);
	TestFalse(TEXT("Shuten-dōji sin runas: no alcanza la sellada"), Reaches(1));
	F.State.Seats[1].Runes[1] = 1;
	TestTrue(TEXT("Shuten-dōji con la runa 1: Paso Libre"), Traits(1).bFreePassage);
	TestTrue(TEXT("Shuten-dōji con la runa 1: alcanza la sellada"), Reaches(1));
	F.State.Seats[1].Runes[1] = 0;
	TestFalse(TEXT("Shuten-dōji sin la runa: otra vez sin Paso Libre"), Traits(1).bFreePassage);
	F.State.Seats[2].Runes[1] = 1;
	TestFalse(TEXT("Apep con una runa que no le da Paso Libre: sin él"), Traits(2).bFreePassage);
	F.State.Seats[2].Runes[3] = 1;
	TestTrue(TEXT("Apep con la runa 3: Paso Libre"), Traits(2).bFreePassage);
	TestFalse(TEXT("Un Eco con runas: sin Paso Libre"), Traits(3).bFreePassage);

	// La máquina de turnos calcula las opciones con la misma función. Semillas fijas 1 a 40.
	bool bFenrirSawTheSeal = false;
	for (const int32 Seat : {0, 1})
	{
		for (int32 Seed = 1; Seed <= 40; ++Seed)
		{
			FMatchState State = F.State;
			FRandomStream Random(Seed);
			State.ActiveSeat = Seat;
			State.TurnPhase = ETurnPhase::BetweenTurns;
			State.Seats[Seat].TileId = Altar;
			for (int32 Step = 0; Step < 3; ++Step)
			{
				Soulbound::Rules::AdvanceTurn(State, 1.0, Random);
			}
			const TArray<int32> Expected = Soulbound::Rules::ReachableTiles(State.Board, Altar, State.Roll, Soulbound::Rules::MakeMoverTraits(State, Seat));
			TestTrue(FString::Printf(TEXT("Asiento %d, semilla %d: las opciones del turno son las de ReachableTiles"), Seat, Seed), State.ReachableOptions == Expected);
			bFenrirSawTheSeal |= Seat == 0 && State.ReachableOptions.Contains(Sealed);
			TestFalse(FString::Printf(TEXT("Asiento %d, semilla %d: sin Paso Libre no se pisa el Sello"), Seat, Seed), Seat == 1 && State.ReachableOptions.Contains(Sealed));
		}
	}
	TestTrue(TEXT("En alguna tirada, Fenrir ve la casilla sellada entre sus opciones"), bFenrirSawTheSeal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunesEventLogTest,
	"Soulbound.Core.Runes.RegistroDeEventos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRunesEventLogTest::RunTest(const FString& Parameters)
{
	// Registro (S-008): runa obtenida, minijuego superado o fallado, Fuente agotada y runa devuelta aparecen con asiento,
	// tipo y marca de tiempo.
	FRuneFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 Forgotten = F.DomainOf(EDomainState::Forgotten);
	const int32 ForgottenSource = F.SourceOf(Forgotten);
	const int32 OwnSource = F.SourceOf(F.ElectDomain);

	F.Now = 10.0;
	F.Land(Elect, ForgottenSource);
	F.Now = 19.0;
	F.Trial(Elect, false);
	F.Now = 20.0;
	F.Land(Elect, F.State.Board.Tiles[ForgottenSource].Neighbours[0]);
	F.Land(Elect, ForgottenSource);
	F.Now = 29.0;
	F.Trial(Elect, true);
	F.State.Board.Tiles[OwnSource].RuneReserve = 0;
	F.Now = 40.0;
	F.Land(Elect, OwnSource);
	F.Now = 50.0;
	Soulbound::Rules::ReturnRuneToSource(F.State, Elect, Forgotten, ForgottenSource, F.Now);

	struct FExpected { EMatchEventType Type; int32 Seat; int32 Value; double Seconds; };
	const FExpected Expected[] = {
		{EMatchEventType::SkillMinigameFailed, Elect, Forgotten, 20.0},
		{EMatchEventType::SkillMinigamePassed, Elect, Forgotten, 30.0},
		{EMatchEventType::RuneObtained, Elect, Forgotten, 30.0},
		{EMatchEventType::SourceDepleted, Elect, F.ElectDomain, 41.0},
		{EMatchEventType::RuneReturned, Elect, Forgotten, 50.0},
	};
	for (const FExpected& Want : Expected)
	{
		const TArray<FMatchEvent> Found = EventsOfType(F.State, Want.Type);
		const FString Name = FString::Printf(TEXT("Evento %d"), static_cast<int32>(Want.Type));
		if (TestEqual(Name + TEXT(": uno"), Found.Num(), 1))
		{
			TestEqual(Name + TEXT(": asiento"), Found[0].Seat, Want.Seat);
			TestEqual(Name + TEXT(": tipo de runa"), Found[0].Value, Want.Value);
			TestEqual(Name + TEXT(": marca de tiempo del reloj de partida"), Found[0].ClockSeconds, Want.Seconds);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
