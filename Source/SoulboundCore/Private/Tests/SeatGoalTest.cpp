// Tests de la meta del asiento (S-040, R-VIC-5): CA-VIC-6 a CA-VIC-10 y CA-RUN-6. Cada test cita el criterio y la regla que cubre.
// Se lanzan sin editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Meta".
//
// Montaje: tablero de 35 casillas, cuatro asientos, como RuneRulesTest: el asiento 0 se vincula al primer Dominio con Nicho Libre (el
// tablero despierta de verdad, por ResolveNicheLanding) y los demás siguen Ecos. Las runas y las reservas se ponen a mano: la meta solo
// las lee.

#include "Match/BondRaceRules.h"
#include "Match/MatchStart.h"
#include "Match/RuneRules.h"
#include "Match/SeatGoalRules.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Rules::SeatGoal;

	constexpr EAutomationTestFlags GoalTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	constexpr int32 GoalElect = 0;
	constexpr int32 GoalEcho = 1;

	/** Partida de cuatro sobre el tablero de 35 casillas, con los Elegidos del archivo de datos. */
	struct FGoalFixture
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

		/** El asiento 0 reclama el Nicho del primer Dominio con dueño: el tablero despierta y las Fuentes se llenan. */
		bool Awaken(FAutomationTestBase& Test)
		{
			if (!Start(Test))
			{
				return false;
			}
			ElectDomain = DomainOf(EDomainState::Owned);
			return Test.TestTrue(TEXT("El asiento 0 reclama el Nicho"), Land(GoalElect, State.Board.FindNiche(ElectDomain)))
				&& Test.TestTrue(TEXT("El tablero despierta"), State.Board.Phase == EBoardPhase::Awakened);
		}

		/** Termina el movimiento de Seat en TileId, un segundo después del anterior. */
		bool Land(int32 Seat, int32 TileId)
		{
			Now += 1.0;
			return Soulbound::Tests::LandOn(State, Random, Seat, TileId, Now);
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

		/** Todos los Dominios en el estado indicado. */
		TArray<int32> DomainsOf(EDomainState DomainState) const
		{
			TArray<int32> Domains;
			for (const FDomainState& Domain : State.Board.Domains)
			{
				if (Domain.State == DomainState)
				{
					Domains.Add(Domain.DomainId);
				}
			}
			return Domains;
		}

		/** Las Fuentes de Poder del Dominio, en orden ascendente. */
		TArray<int32> SourcesOf(int32 DomainId) const
		{
			TArray<int32> Sources;
			for (const FTileState& Tile : State.Board.Tiles)
			{
				if (Tile.Type == ETileType::PowerSource && Tile.DomainId == DomainId)
				{
					Sources.Add(Tile.TileId);
				}
			}
			return Sources;
		}

		/** Deja sin reserva todas las Fuentes del Dominio. */
		void Deplete(int32 DomainId)
		{
			for (const int32 Source : SourcesOf(DomainId))
			{
				State.Board.Tiles[Source].RuneReserve = 0;
			}
		}

		/** El Brazal de Seat con una runa de cada tipo de Types y ninguna más. */
		void SetRunes(int32 Seat, const TArray<int32>& Types)
		{
			for (int32 RuneType = 0; RuneType < Soulbound::Board::DomainCount; ++RuneType)
			{
				State.Seats[Seat].Runes[RuneType] = Types.Contains(RuneType) ? 1 : 0;
			}
		}
	};

	/** Las Fuentes de los Dominios Domains, juntas y en orden ascendente. */
	TArray<int32> SourcesOfAll(const FGoalFixture& F, const TArray<int32>& Domains)
	{
		TArray<int32> Sources;
		for (const int32 Domain : Domains)
		{
			Sources.Append(F.SourcesOf(Domain));
		}
		Sources.Sort();
		return Sources;
	}

	/** Lista de casillas para los mensajes: "3, 17, 22". */
	FString TilesText(const TArray<int32>& Tiles)
	{
		return FString::JoinBy(Tiles, TEXT(", "), [](int32 Tile) { return FString::FromInt(Tile); });
	}

	/**
	 * El caso de CA-VIC-7: el Elegido (asiento 0) con el tablero Despierto y dos runas distintas, la de un Dominio Olvidado y la de otro con
	 * dueño, y sin reserva la Fuente de uno de los tipos que le faltan (el de otro Dominio con dueño). Devuelve los tipos que tiene y el
	 * Dominio agotado en Have y Depleted.
	 */
	bool MakeElectRunesCase(FAutomationTestBase& Test, FGoalFixture& F, TArray<int32>& Have, int32& Depleted)
	{
		if (!F.Awaken(Test))
		{
			return false;
		}
		const int32 Forgotten = F.DomainOf(EDomainState::Forgotten);
		const int32 OtherOwned = F.DomainOf(EDomainState::Owned, F.ElectDomain);
		Have = {Forgotten, F.ElectDomain};
		F.SetRunes(GoalElect, Have);
		Depleted = OtherOwned;
		F.Deplete(Depleted);
		return Test.TestTrue(TEXT("Hay un Dominio Olvidado y otro con dueño"), Forgotten != INDEX_NONE && OtherOwned != INDEX_NONE);
	}

	/**
	 * El caso de CA-VIC-8: tras la carrera (todos los Dominios sin dueño pasan a Olvidados, R-TAB-10), el Eco (asiento 1) con la runa de un
	 * Dominio Olvidado, y sin reserva la Fuente de otro Olvidado. Devuelve el tipo que tiene y el Dominio agotado en Have y Depleted.
	 */
	bool MakeEchoRunesCase(FAutomationTestBase& Test, FGoalFixture& F, int32& Have, int32& Depleted)
	{
		if (!F.Awaken(Test) || !Test.TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now)))
		{
			return false;
		}
		const TArray<int32> Forgotten = F.DomainsOf(EDomainState::Forgotten);
		if (!Test.TestTrue(TEXT("Hay al menos tres Dominios Olvidados"), Forgotten.Num() >= 3))
		{
			return false;
		}
		Have = Forgotten[0];
		Depleted = Forgotten[1];
		F.SetRunes(GoalEcho, {Have});
		F.Deplete(Depleted);
		return Test.TestEqual(TEXT("El asiento 1 es Eco"), static_cast<int32>(F.State.Seats[GoalEcho].Role), static_cast<int32>(ESeatRole::LostEcho));
	}

	/**
	 * CA-RUN-6: para cada Fuente del tablero, la casilla está en la meta Runes de Seat si y solo si llevar al asiento a ella y resolver la llegada
	 * (ResolvePowerSourceLanding, sobre una copia del estado) da Obtained o TrialPending y el asiento no tiene runas de su tipo.
	 */
	void CheckSameConditionAsLanding(FAutomationTestBase& Test, const FMatchState& State, int32 Seat, const FString& Case)
	{
		const FSeatGoal Goal = SeatGoal(State, Seat);
		Test.TestEqual(FString::Printf(TEXT("%s: la meta es Runes"), *Case), Goal.Kind, ESeatGoal::Runes);
		for (const FTileState& Tile : State.Board.Tiles)
		{
			if (Tile.Type != ETileType::PowerSource)
			{
				continue;
			}
			FMatchState Landed = State;
			Landed.ActiveSeat = Seat;
			Landed.Seats[Seat].TileId = Tile.TileId;
			Landed.bActiveSeatMoved = true;
			const ERuneLanding Landing = Soulbound::Rules::ResolvePowerSourceLanding(Landed, Seat, State.ClockSeconds + 1.0);
			const bool bWouldGet = (Landing == ERuneLanding::Obtained || Landing == ERuneLanding::TrialPending) && State.Seats[Seat].RuneCountOf(Tile.DomainId) == 0;
			Test.TestEqual(FString::Printf(TEXT("%s: la Fuente %d (Dominio %d) está en la meta si y solo si la llegada da runa que le falta"), *Case, Tile.TileId, Tile.DomainId),
				Goal.Tiles.Contains(Tile.TileId), bWouldGet);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeatGoalRaceTest, "Soulbound.Core.Meta.CarreraNichosLibres", GoalTestFlags)

bool FSeatGoalRaceTest::RunTest(const FString& Parameters)
{
	// CA-VIC-6 (R-VIC-5, carrera): en la carrera, para un asiento sin vincular, la meta es Niche y sus casillas son exactamente los Nichos
	// Libres: no incluye el Nicho que otro asiento acaba de reclamar ni los cerrados de los Dominios Olvidados.
	FGoalFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	auto FreeNiches = [&F]()
	{
		TArray<int32> Niches;
		for (const FDomainState& Domain : F.State.Board.Domains)
		{
			if (Domain.NicheState == ENicheState::Free)
			{
				Niches.Add(F.State.Board.FindNiche(Domain.DomainId));
			}
		}
		Niches.Sort();
		return Niches;
	};
	TArray<int32> ClosedNiches;
	for (const int32 Domain : F.DomainsOf(EDomainState::Forgotten))
	{
		ClosedNiches.Add(F.State.Board.FindNiche(Domain));
	}
	if (!TestTrue(TEXT("Al empezar hay Nichos Libres y Nichos Cerrados"), FreeNiches().Num() > 1 && !ClosedNiches.IsEmpty()))
	{
		return false;
	}

	const FSeatGoal AtStart = SeatGoal(F.State, GoalElect);
	TestEqual(TEXT("Al empezar, la meta es Niche"), AtStart.Kind, ESeatGoal::Niche);
	TestEqual(TEXT("Al empezar, las casillas son los Nichos Libres"), TilesText(AtStart.Tiles), TilesText(FreeNiches()));

	// El asiento 1 reclama un Nicho: para el asiento 0 deja de ser meta.
	const int32 Claimed = F.State.Board.FindNiche(F.DomainOf(EDomainState::Owned));
	if (!TestTrue(TEXT("El asiento 1 reclama un Nicho"), F.Land(GoalEcho, Claimed)))
	{
		return false;
	}
	const FSeatGoal AfterClaim = SeatGoal(F.State, GoalElect);
	TestEqual(TEXT("Tras el vínculo de otro, la meta sigue siendo Niche"), AfterClaim.Kind, ESeatGoal::Niche);
	TestEqual(TEXT("Tras el vínculo de otro, las casillas son los Nichos Libres"), TilesText(AfterClaim.Tiles), TilesText(FreeNiches()));
	TestFalse(TEXT("El Nicho reclamado no está"), AfterClaim.Tiles.Contains(Claimed));
	for (const int32 Closed : ClosedNiches)
	{
		TestFalse(FString::Printf(TEXT("El Nicho cerrado %d no está"), Closed), AfterClaim.Tiles.Contains(Closed));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeatGoalElectRunesTest, "Soulbound.Core.Meta.RunasQueFaltanAlElegido", GoalTestFlags)

bool FSeatGoalElectRunesTest::RunTest(const FString& Parameters)
{
	// CA-VIC-7 (R-VIC-5, runas del Elegido): con el tablero Despierto, para un Elegido con dos runas distintas, la meta es Runes y sus casillas
	// son las Fuentes con reserva de las cuatro runas que le faltan, sean de Dominios con dueño u Olvidados. Una Fuente sin reserva no está.
	// Una Fuente de una runa que ya tiene no está.
	FGoalFixture F;
	TArray<int32> Have;
	int32 Depleted = INDEX_NONE;
	if (!MakeElectRunesCase(*this, F, Have, Depleted))
	{
		return false;
	}
	TArray<int32> Missing;
	for (int32 RuneType = 0; RuneType < Soulbound::Board::DomainCount; ++RuneType)
	{
		if (!Have.Contains(RuneType) && RuneType != Depleted)
		{
			Missing.Add(RuneType);
		}
	}
	const TArray<int32> Expected = SourcesOfAll(F, Missing);
	const FSeatGoal Goal = SeatGoal(F.State, GoalElect);
	TestEqual(TEXT("La meta es Runes"), Goal.Kind, ESeatGoal::Runes);
	TestEqual(TEXT("Las casillas son las Fuentes con reserva de las runas que le faltan"), TilesText(Goal.Tiles), TilesText(Expected));
	for (const int32 Source : F.SourcesOf(Depleted))
	{
		TestFalse(FString::Printf(TEXT("La Fuente sin reserva %d no está"), Source), Goal.Tiles.Contains(Source));
	}
	for (const int32 Source : SourcesOfAll(F, Have))
	{
		TestFalse(FString::Printf(TEXT("La Fuente %d, de una runa que ya tiene, no está"), Source), Goal.Tiles.Contains(Source));
	}
	const bool bAnyForgotten = Goal.Tiles.ContainsByPredicate([&F](int32 Tile) { return F.State.Board.Domains[F.State.Board.Tiles[Tile].DomainId].State == EDomainState::Forgotten; });
	const bool bAnyOwned = Goal.Tiles.ContainsByPredicate([&F](int32 Tile) { return F.State.Board.Domains[F.State.Board.Tiles[Tile].DomainId].State == EDomainState::Owned; });
	TestTrue(TEXT("Hay Fuentes de Dominios Olvidados"), bAnyForgotten);
	TestTrue(TEXT("Y de Dominios con dueño"), bAnyOwned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeatGoalEchoRunesTest, "Soulbound.Core.Meta.RunasDelEcoSoloOlvidados", GoalTestFlags)

bool FSeatGoalEchoRunesTest::RunTest(const FString& Parameters)
{
	// CA-VIC-8 (R-VIC-5, runas del Eco): para un Eco, tras la carrera, las casillas son solo las Fuentes con reserva de los Dominios Olvidados
	// cuya runa le falta. Si no hay ninguna, la meta sigue siendo Runes, con la lista vacía. Durante la Noche de los Ecos la lista no cambia.
	FGoalFixture F;
	int32 Have = INDEX_NONE;
	int32 Depleted = INDEX_NONE;
	if (!MakeEchoRunesCase(*this, F, Have, Depleted))
	{
		return false;
	}
	TArray<int32> Wanted = F.DomainsOf(EDomainState::Forgotten);
	Wanted.Remove(Have);
	Wanted.Remove(Depleted);
	const TArray<int32> Expected = SourcesOfAll(F, Wanted);

	const FSeatGoal Goal = SeatGoal(F.State, GoalEcho);
	TestEqual(TEXT("La meta es Runes"), Goal.Kind, ESeatGoal::Runes);
	TestEqual(TEXT("Las casillas son las Fuentes con reserva de los Olvidados cuya runa le falta"), TilesText(Goal.Tiles), TilesText(Expected));
	for (const int32 Source : F.SourcesOf(F.ElectDomain))
	{
		TestFalse(FString::Printf(TEXT("La Fuente %d, de un Dominio con dueño, no está"), Source), Goal.Tiles.Contains(Source));
	}

	// La Noche de los Ecos deja al Eco entrar en los Dominios con dueño, pero no obtener en su Fuente: la lista no cambia.
	F.State.EchoNightUntilRound = F.State.Round + 2;
	if (TestTrue(TEXT("Es la Noche de los Ecos"), F.State.IsEchoNight()))
	{
		const FSeatGoal AtNight = SeatGoal(F.State, GoalEcho);
		TestEqual(TEXT("En la Noche, la meta sigue siendo Runes"), AtNight.Kind, ESeatGoal::Runes);
		TestEqual(TEXT("En la Noche, la lista no cambia"), TilesText(AtNight.Tiles), TilesText(Expected));
	}
	F.State.EchoNightUntilRound = 0;

	// Sin ninguna Fuente de Olvidado con reserva: Runes, sin casillas.
	for (const int32 Domain : F.DomainsOf(EDomainState::Forgotten))
	{
		F.Deplete(Domain);
	}
	const FSeatGoal Empty = SeatGoal(F.State, GoalEcho);
	TestEqual(TEXT("Sin Fuentes que le sirvan, la meta sigue siendo Runes"), Empty.Kind, ESeatGoal::Runes);
	TestTrue(TEXT("... con la lista vacía"), Empty.Tiles.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeatGoalBondedInRaceTest, "Soulbound.Core.Meta.VinculadoEnCarreraBuscaRunas", GoalTestFlags)

bool FSeatGoalBondedInRaceTest::RunTest(const FString& Parameters)
{
	// CA-VIC-9 (R-VIC-5, vinculado en la carrera): durante la carrera, un asiento ya vinculado tiene la meta Runes, no Niche. Con el tablero en
	// Primordial, la lista de Runes está vacía.
	{
		FGoalFixture F;
		if (!F.Awaken(*this))
		{
			return false;
		}
		TestEqual(TEXT("La carrera sigue"), static_cast<int32>(F.State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));
		TestTrue(TEXT("Quedan Nichos Libres"), Soulbound::Rules::HasFreeNiche(F.State.Board));
		const FSeatGoal Goal = SeatGoal(F.State, GoalElect);
		TestEqual(TEXT("El vinculado busca runas"), Goal.Kind, ESeatGoal::Runes);
		TestFalse(TEXT("... en alguna Fuente"), Goal.Tiles.IsEmpty());
		TestEqual(TEXT("Un Eco sin vincular sigue con Niche"), SeatGoal(F.State, GoalEcho).Kind, ESeatGoal::Niche);
	}
	{
		// Un Elegido con el tablero en Primordial (armado a mano): Runes, sin casillas, porque en Primordial no hay runas (R-TAB-8).
		FGoalFixture F;
		if (!F.Start(*this))
		{
			return false;
		}
		F.State.Seats[GoalElect].Role = ESeatRole::Elect;
		F.State.Seats[GoalElect].DomainId = F.DomainOf(EDomainState::Owned);
		TestEqual(TEXT("El tablero está en Primordial"), static_cast<int32>(F.State.Board.Phase), static_cast<int32>(EBoardPhase::Primordial));
		const FSeatGoal Goal = SeatGoal(F.State, GoalElect);
		TestEqual(TEXT("Primordial: la meta es Runes"), Goal.Kind, ESeatGoal::Runes);
		TestTrue(TEXT("Primordial: la lista está vacía"), Goal.Tiles.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeatGoalAltarAndEndTest, "Soulbound.Core.Meta.AltarConSeisYNadaAlFinal", GoalTestFlags)

bool FSeatGoalAltarAndEndTest::RunTest(const FString& Parameters)
{
	// CA-VIC-10 (R-VIC-5, Altar y final): con las seis runas distintas, la meta es Altar y su única casilla es el Altar. Con la partida
	// terminada, None y ninguna casilla.
	FGoalFixture F;
	if (!F.Awaken(*this))
	{
		return false;
	}
	const int32 Altar = F.State.Board.FindAltar();
	F.SetRunes(GoalElect, {0, 1, 2, 3, 4, 5});
	TestFalse(TEXT("El Elegido no está en el Altar"), F.State.Board.IsAltar(F.State.Seats[GoalElect].TileId));
	const FSeatGoal Away = SeatGoal(F.State, GoalElect);
	TestEqual(TEXT("Con seis, fuera del Altar: la meta es Altar"), Away.Kind, ESeatGoal::Altar);
	TestEqual(TEXT("... y su única casilla es el Altar"), TilesText(Away.Tiles), FString::FromInt(Altar));

	F.State.Seats[GoalElect].TileId = Altar;
	const FSeatGoal There = SeatGoal(F.State, GoalElect);
	TestEqual(TEXT("Con seis, en el Altar: la meta sigue siendo Altar"), There.Kind, ESeatGoal::Altar);
	TestEqual(TEXT("... con el Altar"), TilesText(There.Tiles), FString::FromInt(Altar));

	// Con cinco tipos (y duplicados), aún no.
	F.SetRunes(GoalElect, {0, 1, 2, 3, 4});
	F.State.Seats[GoalElect].Runes[0] = 3;
	TestEqual(TEXT("Con cinco tipos, Runes"), SeatGoal(F.State, GoalElect).Kind, ESeatGoal::Runes);

	F.State.MatchPhase = EMatchPhase::MatchOver;
	for (int32 Seat = 0; Seat < F.State.Seats.Num(); ++Seat)
	{
		const FSeatGoal Over = SeatGoal(F.State, Seat);
		TestEqual(FString::Printf(TEXT("Partida terminada, asiento %d: None"), Seat), Over.Kind, ESeatGoal::None);
		TestTrue(FString::Printf(TEXT("Partida terminada, asiento %d: sin casillas"), Seat), Over.Tiles.IsEmpty());
	}
	F.State.MatchPhase = EMatchPhase::RuneHunt;
	TestEqual(TEXT("Un asiento que no existe: None"), SeatGoal(F.State, Soulbound::Match::SeatCount).Kind, ESeatGoal::None);
	TestTrue(TEXT("... sin casillas"), SeatGoal(F.State, INDEX_NONE).Tiles.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeatGoalSameConditionTest, "Soulbound.Core.Meta.MismaCondicionQueLaFuente", GoalTestFlags)

bool FSeatGoalSameConditionTest::RunTest(const FString& Parameters)
{
	// CA-RUN-6 (R-VIC-5, una sola condición; texto corregido en el plan, 16/11/2026): para cada caso de CA-VIC-7 y CA-VIC-8, una Fuente está en
	// la meta si y solo si llevar al asiento a ella y resolver la llegada (ResolvePowerSourceLanding) da Obtained o TrialPending y la runa le falta.
	{
		FGoalFixture F;
		TArray<int32> Have;
		int32 Depleted = INDEX_NONE;
		if (MakeElectRunesCase(*this, F, Have, Depleted))
		{
			CheckSameConditionAsLanding(*this, F.State, GoalElect, TEXT("Elegido"));
		}
	}
	{
		FGoalFixture F;
		int32 Have = INDEX_NONE;
		int32 Depleted = INDEX_NONE;
		if (MakeEchoRunesCase(*this, F, Have, Depleted))
		{
			CheckSameConditionAsLanding(*this, F.State, GoalEcho, TEXT("Eco tras la carrera"));
			F.State.EchoNightUntilRound = F.State.Round + 2;
			CheckSameConditionAsLanding(*this, F.State, GoalEcho, TEXT("Eco en la Noche de los Ecos"));
		}
	}
	return true;
}

#endif
