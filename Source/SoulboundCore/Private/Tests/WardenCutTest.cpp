// Tests del golpe que corta el turno (S-016, R-WAR-4, R-TUR-11, R-TAB-26) y del Alto (R-TAB-28, R-EFE-1). Un golpe corta el
// turno en cualquier fase en que se puede cortar: TurnInterrupted con la fase, la tirada perdida con el consumo fijo del
// sorteo respetado, la ficha donde estaba, las elecciones cerradas y el evento aplicado entre turnos con el cortado como
// activo; el turno pasa al siguiente. Una escena cortante corta; una no cortante espera al hueco. Cada test cita el criterio
// (CA-XXX-n) y la regla (R-XXX-n) que cubre. Filtro: "Soulbound.Core.Warden.Cut". Desde S-020, el corte a medio camino
// (fase Moving) y el golpe de depuración SbStrike ("Soulbound.Core.Debug.GolpeAhora"), que usa este mismo montaje.
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos).
// Las decisiones se aplican hechas a mano (ApplyWardenDecision): el test mira el corte, no el sorteo del cerebro.

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/DebugEdits.h"
#include "Match/EffectRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::App;
	using Soulbound::Tests::FWardenFixture;

	constexpr int32 Fenrir = FWardenFixture::Fenrir;
	constexpr int32 Koschei = FWardenFixture::Koschei;
	constexpr int32 EchoA = FWardenFixture::EchoA;
	constexpr int32 KoscheiDomain = FWardenFixture::KoscheiDomain;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** El golpe de los tests: un turno perdido para el activo, que deja huella en quien lo recibe. */
	const TCHAR* const StrikeId = TEXT("golpe-de-prueba");

	bool StartForCut(FAutomationTestBase& Test, FWardenFixture& F)
	{
		if (!F.StartWithWarden(Test))
		{
			return false;
		}
		F.AddBagEvent(StrikeId, EAct::ActTwo, EWardenChannel::Strike, {{App(EEffect::LoseTurn, EEffectTarget::ActivePlayer, 1)}}, true);
		return true;
	}

	/** El sorteo de la partida tras sacar Count dados más: lo que debe quedar si el corte respeta el consumo fijo. */
	FRandomStream AfterDice(const FRandomStream& Random, int32 Count)
	{
		FRandomStream Expected = Random;
		for (int32 Die = 0; Die < Count; ++Die)
		{
			Expected.RandRange(1, Soulbound::Dice::Faces);
		}
		return Expected;
	}

	/**
	 * Corta con el golpe de los tests el turno en curso de Seat, que está en Phase, y comprueba lo común a todas las fases:
	 * TurnInterrupted con la fase y antes del TurnEnded del turno, consumo fijo (MissingDice dados más), ficha en su sitio,
	 * elecciones cerradas, evento dirigido por el cortado tras el fin de su turno, y el turno al siguiente del orden.
	 */
	void CutAndCheck(FAutomationTestBase& Test, FWardenFixture& F, int32 Seat, ETurnPhase Phase, int32 MissingDice, const TCHAR* Context)
	{
		if (!Test.TestEqual(FString::Printf(TEXT("%s: el turno está en la fase"), Context), AsInt(F.State.TurnPhase), AsInt(Phase)))
		{
			return;
		}
		const int32 Tile = F.State.Seats[Seat].TileId;
		const FRandomStream Expected = AfterDice(F.Random, MissingDice);
		const int32 Before = F.State.EventLog.Num();
		F.Apply(F.MakeDecision(EWardenDecisionKind::Strike, StrikeId));

		const TArray<FMatchEvent> Cut = F.Since(Before, EMatchEventType::TurnInterrupted);
		if (Test.TestEqual(FString::Printf(TEXT("%s: un TurnInterrupted"), Context), Cut.Num(), 1))
		{
			Test.TestEqual(FString::Printf(TEXT("%s: del asiento cortado"), Context), Cut[0].Seat, Seat);
			Test.TestEqual(FString::Printf(TEXT("%s: con la fase"), Context), Cut[0].Value, AsInt(Phase));
		}
		Test.TestEqual(FString::Printf(TEXT("%s: consumo fijo respetado"), Context), F.Random.GetCurrentSeed(), Expected.GetCurrentSeed());
		Test.TestEqual(FString::Printf(TEXT("%s: la ficha se queda donde estaba"), Context), F.State.Seats[Seat].TileId, Tile);
		Test.TestEqual(FString::Printf(TEXT("%s: sin moverse"), Context), F.Since(Before, EMatchEventType::Moved).Num(), 0);
		Test.TestFalse(FString::Printf(TEXT("%s: elecciones cerradas"), Context), F.State.PendingChoice.IsOpen());
		Test.TestEqual(FString::Printf(TEXT("%s: entre turnos"), Context), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
		Test.TestEqual(FString::Printf(TEXT("%s: la tirada se pierde"), Context), F.State.Roll, 0);

		// El evento, entre turnos y con el cortado como activo, después del fin de su turno.
		int32 EndedAt = INDEX_NONE;
		int32 StartedAt = INDEX_NONE;
		for (int32 Index = Before; Index < F.State.EventLog.Num(); ++Index)
		{
			const FMatchEvent& Event = F.State.EventLog[Index];
			EndedAt = Event.Type == EMatchEventType::TurnEnded && Event.Seat == Seat && EndedAt == INDEX_NONE ? Index : EndedAt;
			StartedAt = Event.Type == EMatchEventType::WardenEventStarted && StartedAt == INDEX_NONE ? Index : StartedAt;
		}
		Test.TestTrue(FString::Printf(TEXT("%s: el turno termina y después llega el evento"), Context), EndedAt != INDEX_NONE && StartedAt > EndedAt);
		if (StartedAt != INDEX_NONE)
		{
			Test.TestEqual(FString::Printf(TEXT("%s: el evento lo dirige el cortado"), Context), F.State.EventLog[StartedAt].Seat, Seat);
		}
		Test.TestEqual(FString::Printf(TEXT("%s: el cortado es el último que jugó"), Context), F.State.LastTurnSeat, Seat);
		Test.TestEqual(FString::Printf(TEXT("%s: el turno pasa al siguiente"), Context), F.State.ActiveSeat, F.NextOf(Seat));
	}

	/** Un movimiento preparado (S-020): desde dónde, adónde y por qué camino. */
	struct FPlannedMove
	{
		int32 Origin = INDEX_NONE;
		int32 Destination = INDEX_NONE;
		TArray<int32> Path;
	};

	/**
	 * Un movimiento de Seat con la tirada Roll, desde una casilla normal, cuyo camino mide Roll y pasa en la posición SourceAt por
	 * una Fuente de un Dominio con dueño con reserva: si resolviera, Seat (Elegido) obtendría su runa. Lo busca con las reglas del
	 * camino; lo que el test comprueba es lo que pasa al cortarlo.
	 */
	FPlannedMove FindMoveThroughSource(const FWardenFixture& F, int32 Seat, int32 Roll, int32 SourceAt)
	{
		const FBoardState& Board = F.State.Board;
		const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(F.State, Seat);
		for (const FTileState& Tile : Board.Tiles)
		{
			if (Tile.Type != ETileType::Normal || Tile.bSealed)
			{
				continue;
			}
			for (const int32 To : Soulbound::Rules::ReachableTiles(Board, Tile.TileId, Roll, Mover))
			{
				const TArray<int32> Path = Soulbound::Rules::MovementPath(Board, Tile.TileId, Roll, Mover, To);
				const FTileState* Source = Path.Num() == Roll && Path.IsValidIndex(SourceAt) ? &Board.Tiles[Path[SourceAt]] : nullptr;
				if (Source && Source->Type == ETileType::PowerSource && Source->RuneReserve > 0 && Board.Domains[Source->DomainId].State == EDomainState::Owned)
				{
					return {Tile.TileId, To, Path};
				}
			}
		}
		return FPlannedMove();
	}

	/**
	 * Turno de Seat por el movimiento Move: empieza en su origen con la tirada fijada, confirma el destino y entra en las Entered
	 * primeras casillas del camino con sus intenciones de avance. Devuelve si quedó en movimiento en curso.
	 */
	bool MoveInto(FWardenFixture& F, int32 Seat, const FPlannedMove& Move, int32 Entered)
	{
		F.State.Seats[Seat].TileId = Move.Origin;
		F.BeginTurn(Seat);
		F.State.ForcedRoll = Move.Path.Num();
		F.Advance();
		if (Soulbound::Rules::SubmitDestinationIntent(F.State, Seat, Move.Destination, F.Now) != EIntentResult::Accepted)
		{
			return false;
		}
		for (int32 Index = 0; Index < Entered; ++Index)
		{
			F.Now += 0.3;
			Soulbound::Rules::SubmitMoveProgress(F.State, Seat, Move.Path[Index], F.Now);
		}
		return F.State.TurnPhase == ETurnPhase::Moving && F.State.Movement.EnteredCount == Entered;
	}

	/** Si la casilla en que se cortó no resolvió nada: ni runa, ni minijuego, ni vínculo, ni llegada; y el camino se descartó. */
	void CheckUnresolved(FAutomationTestBase& Test, const FWardenFixture& F, int32 From, const TCHAR* Context)
	{
		Test.TestEqual(FString::Printf(TEXT("%s: sin runa"), Context), F.Since(From, EMatchEventType::RuneObtained).Num(), 0);
		Test.TestEqual(FString::Printf(TEXT("%s: sin minijuego"), Context),
			F.Since(From, EMatchEventType::SkillMinigamePassed).Num() + F.Since(From, EMatchEventType::SkillMinigameFailed).Num(), 0);
		Test.TestEqual(FString::Printf(TEXT("%s: sin prueba pendiente"), Context), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::None));
		Test.TestEqual(FString::Printf(TEXT("%s: sin vínculo"), Context), F.Since(From, EMatchEventType::Bonded).Num(), 0);
		Test.TestEqual(FString::Printf(TEXT("%s: sin Moved"), Context), F.Since(From, EMatchEventType::Moved).Num(), 0);
		Test.TestFalse(FString::Printf(TEXT("%s: el resto del camino se descarta"), Context), F.State.Movement.IsActive());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenCutStartTest, "Soulbound.Core.Warden.Cut.EnStartConDesvioAbierto",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenCutStartTest::RunTest(const FString& Parameters)
{
	// CA-WAR-3b (R-WAR-4, R-TUR-11): un golpe corta el turno en Start, con la elección de Desvío abierta. Sin llegar a PreRoll,
	// no saca dados. El cortado sigue en el Vacío: el turno perdido no vale sobre él y es burla.
	FWardenFixture F;
	if (!StartForCut(*this, F))
	{
		return false;
	}
	F.GivePower(Fenrir, EPower::Deflection);
	Soulbound::Rules::Banish(F.State, Fenrir, false, F.Now);
	F.BeginTurn(Fenrir);
	TestEqual(TEXT("Desvío abierto"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Deflection));
	const int32 Before = F.State.EventLog.Num();
	CutAndCheck(*this, F, Fenrir, ETurnPhase::Start, 0, TEXT("Start"));
	TestTrue(TEXT("Start: sigue en el Vacío"), F.State.Seats[Fenrir].IsInVoid());
	TestEqual(TEXT("Start: la runa de Desvío no se gasta"), F.RuneTotal(Fenrir), 1);
	TestEqual(TEXT("Start: el turno perdido, sobre quien está en el Vacío, es burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenCutPreRollTest, "Soulbound.Core.Warden.Cut.EnPreRollConDadosAbiertos",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenCutPreRollTest::RunTest(const FString& Parameters)
{
	// CA-WAR-3b (R-TUR-11, consumo fijo de S-009): en PreRoll con la elección de dados abierta, el turno llegó a PreRoll y aún
	// no tiró: se sacan los cuatro valores. El evento se aplica sobre el cortado.
	FWardenFixture F;
	if (!StartForCut(*this, F))
	{
		return false;
	}
	F.GivePower(Koschei, EPower::Haste);
	F.BeginTurn(Koschei);
	TestEqual(TEXT("Dados abiertos"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::DiceCount));
	CutAndCheck(*this, F, Koschei, ETurnPhase::PreRoll, 4, TEXT("PreRoll"));
	TestEqual(TEXT("PreRoll: el turno perdido cae sobre el cortado"), F.State.Seats[Koschei].LostTurns, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenCutRollTest, "Soulbound.Core.Warden.Cut.EnRollAndChoose",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenCutRollTest::RunTest(const FString& Parameters)
{
	// CA-WAR-3b (R-TUR-11): en RollAndChoose, sin oferta de repetir (los cuatro valores ya salieron al tirar) y con ella (faltan
	// los dos de la repetición). La tirada se pierde y la ficha no se mueve.
	{
		FWardenFixture F;
		if (!StartForCut(*this, F))
		{
			return false;
		}
		F.BeginTurn(Koschei);
		F.Advance();
		TestFalse(TEXT("Sin oferta de repetir"), F.State.PendingChoice.IsOpen());
		CutAndCheck(*this, F, Koschei, ETurnPhase::RollAndChoose, 0, TEXT("RollAndChoose sin repetir"));
		TestEqual(TEXT("RollAndChoose: el turno perdido cae sobre el cortado"), F.State.Seats[Koschei].LostTurns, 1);
	}
	{
		FWardenFixture F;
		if (!StartForCut(*this, F))
		{
			return false;
		}
		// Koschei con la afinidad de Apep (R-TUR-6): lo que el test necesita es la oferta de repetir abierta.
		F.State.Elects.Elects[KoscheiDomain].Affinity = EAffinity::RerollOnce;
		F.BeginTurn(Koschei);
		F.Advance();
		TestEqual(TEXT("Oferta de repetir abierta"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Reroll));
		CutAndCheck(*this, F, Koschei, ETurnPhase::RollAndChoose, 2, TEXT("RollAndChoose con repetir"));
		TestFalse(TEXT("Con repetir: no queda nada por sacar"), F.State.bRerollValuesPending);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenCutMoveTest, "Soulbound.Core.Warden.Cut.EnMoveAndResolve",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenCutMoveTest::RunTest(const FString& Parameters)
{
	// CA-WAR-3b (R-TUR-11): en MoveAndResolve con el salto planar abierto y con la acción contra un jugador abierta. La ficha ya
	// llegó y su casilla ya resolvió: se queda en ella y el salto o la acción se pierden.
	{
		FWardenFixture F;
		if (!StartForCut(*this, F))
		{
			return false;
		}
		F.MakeAllNodesPlanar();
		F.GivePower(Koschei, EPower::PlanarFlight);
		const TArray<int32> Nodes = F.PlanarNodes();
		if (!TestTrue(TEXT("Llega a un Nodo Planar"), Nodes.Num() > 1 && F.Land(Koschei, Nodes[0])))
		{
			return false;
		}
		TestEqual(TEXT("Salto abierto"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlanarJump));
		CutAndCheck(*this, F, Koschei, ETurnPhase::MoveAndResolve, 0, TEXT("MoveAndResolve con salto"));
		TestEqual(TEXT("Con salto: sin saltar"), F.State.Seats[Koschei].TileId, Nodes[0]);
	}
	{
		FWardenFixture F;
		if (!StartForCut(*this, F))
		{
			return false;
		}
		F.GivePower(Koschei, EPower::Expel);
		const TArray<int32> Tiles = F.NormalTiles(KoscheiDomain);
		if (!TestTrue(TEXT("Hay casillas normales"), !Tiles.IsEmpty()))
		{
			return false;
		}
		F.State.Seats[EchoA].TileId = Tiles[0];
		if (!TestTrue(TEXT("Llega a la casilla del Eco"), F.Land(Koschei, Tiles[0])))
		{
			return false;
		}
		TestEqual(TEXT("Acción abierta"), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::PlayerAction));
		const int32 Before = F.State.EventLog.Num();
		CutAndCheck(*this, F, Koschei, ETurnPhase::MoveAndResolve, 0, TEXT("MoveAndResolve con acción"));
		TestEqual(TEXT("Con acción: nadie es expulsado"), F.Since(Before, EMatchEventType::Expelled).Num(), 0);
		TestEqual(TEXT("Con acción: el Eco sigue en la casilla"), F.State.Seats[EchoA].TileId, Tiles[0]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenCutMidPathTest, "Soulbound.Core.Warden.Cut.AMedioCamino",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenCutMidPathTest::RunTest(const FString& Parameters)
{
	// CA-TAB-27 (R-TAB-26, R-TUR-11, S-020): un golpe con la ficha en la casilla P del camino (la segunda de cuatro, una Fuente
	// con reserva) la deja en P sin resolverla: sin runa ni minijuego, el resto del camino descartado, TurnInterrupted con la fase
	// Moving, el turno termina y el evento se aplica al cortado entre turnos. Cortado antes de la primera, la ficha sigue en el
	// origen.
	{
		FWardenFixture F;
		if (!StartForCut(*this, F))
		{
			return false;
		}
		const FPlannedMove Move = FindMoveThroughSource(F, Koschei, 4, 1);
		if (!TestTrue(TEXT("Hay un camino de cuatro con una Fuente en la segunda"), !Move.Path.IsEmpty())
			|| !TestTrue(TEXT("En la segunda casilla"), MoveInto(F, Koschei, Move, 2)))
		{
			return false;
		}
		const int32 From = F.State.EventLog.Num();
		CutAndCheck(*this, F, Koschei, ETurnPhase::Moving, 0, TEXT("A medio camino"));
		TestEqual(TEXT("A medio camino: la ficha en P, la Fuente"), F.State.Seats[Koschei].TileId, Move.Path[1]);
		CheckUnresolved(*this, F, From, TEXT("A medio camino"));
		TestEqual(TEXT("A medio camino: el turno perdido cae sobre el cortado"), F.State.Seats[Koschei].LostTurns, 1);
	}
	{
		FWardenFixture F;
		if (!StartForCut(*this, F))
		{
			return false;
		}
		const FPlannedMove Move = FindMoveThroughSource(F, Koschei, 4, 1);
		if (!TestTrue(TEXT("Antes de la primera: confirmado sin entrar"), !Move.Path.IsEmpty() && MoveInto(F, Koschei, Move, 0)))
		{
			return false;
		}
		const int32 From = F.State.EventLog.Num();
		CutAndCheck(*this, F, Koschei, ETurnPhase::Moving, 0, TEXT("Antes de la primera"));
		TestEqual(TEXT("Antes de la primera: la ficha en el origen"), F.State.Seats[Koschei].TileId, Move.Origin);
		TestEqual(TEXT("Antes de la primera: sin casillas entradas"), F.Since(From, EMatchEventType::TileEntered).Num(), 0);
		CheckUnresolved(*this, F, From, TEXT("Antes de la primera"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenCutMidPathHaltTest, "Soulbound.Core.Warden.Cut.AMedioCaminoConAltoYSello",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenCutMidPathHaltTest::RunTest(const FString& Parameters)
{
	// CA-TAB-29 (R-TAB-28, R-TAB-26): con el Alto a medio camino, lo mismo: la ficha en P, sin resolverla, y el Alto aplicado al
	// cortado. CA-TAB-27: un Sello que cae en el evento del golpe se aplica después del corte, nunca en la casilla del cortado.
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		const FPlannedMove Move = FindMoveThroughSource(F, Koschei, 4, 1);
		if (!TestTrue(TEXT("Alto: en la segunda casilla"), !Move.Path.IsEmpty() && MoveInto(F, Koschei, Move, 2)))
		{
			return false;
		}
		const int32 From = F.State.EventLog.Num();
		F.Apply(F.MakeDecision(EWardenDecisionKind::Strike, TEXT("tropiezo")));
		const TArray<FMatchEvent> Cut = F.Since(From, EMatchEventType::TurnInterrupted);
		TestTrue(TEXT("Alto: corta con la fase Moving"), Cut.Num() == 1 && Cut[0].Value == AsInt(ETurnPhase::Moving));
		const TArray<FMatchEvent> Applied = F.Since(From, EMatchEventType::EffectApplied);
		TestTrue(TEXT("Alto: aplicado al cortado"), Applied.Num() == 1 && Applied[0].Value == AsInt(EEffect::Halt) && Applied[0].Seat == Koschei);
		TestEqual(TEXT("Alto: la ficha en P"), F.State.Seats[Koschei].TileId, Move.Path[1]);
		CheckUnresolved(*this, F, From, TEXT("Alto"));
	}
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.AddBagEvent(TEXT("golpe-sello"), EAct::ActTwo, EWardenChannel::Strike, {{App(EEffect::PlaceSeal, EEffectTarget::None)}}, true);
		const FPlannedMove Move = FindMoveThroughSource(F, Koschei, 4, 1);
		if (!TestTrue(TEXT("Sello: en la segunda casilla"), !Move.Path.IsEmpty() && MoveInto(F, Koschei, Move, 2)))
		{
			return false;
		}
		const int32 From = F.State.EventLog.Num();
		F.Apply(F.MakeDecision(EWardenDecisionKind::Strike, TEXT("golpe-sello")));
		int32 CutAt = INDEX_NONE;
		int32 SealAt = INDEX_NONE;
		for (int32 Index = From; Index < F.State.EventLog.Num(); ++Index)
		{
			const EMatchEventType Type = F.State.EventLog[Index].Type;
			CutAt = Type == EMatchEventType::TurnInterrupted && CutAt == INDEX_NONE ? Index : CutAt;
			SealAt = Type == EMatchEventType::SealPlaced && SealAt == INDEX_NONE ? Index : SealAt;
		}
		TestTrue(TEXT("Sello: cae después del corte"), CutAt != INDEX_NONE && SealAt > CutAt);
		if (SealAt != INDEX_NONE)
		{
			TestNotEqual(TEXT("Sello: no en la casilla del cortado"), F.State.EventLog[SealAt].Value, Move.Path[1]);
		}
		TestEqual(TEXT("Sello: la ficha en P"), F.State.Seats[Koschei].TileId, Move.Path[1]);
		CheckUnresolved(*this, F, From, TEXT("Sello"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenMovingNotDeferredTest, "Soulbound.Core.Warden.Cut.MovimientoNoSeAplaza",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenMovingNotDeferredTest::RunTest(const FString& Parameters)
{
	// CA-MOV-4 (R-WAR-7): durante el movimiento en curso el cerebro no aplaza (no es un duelo, un trilero ni un minijuego): su
	// golpe es candidato y corta. El que se mueve está en juego (IsSeatInPlay), como en cualquier fase de su turno.
	FWardenFixture F;
	if (!StartForCut(*this, F))
	{
		return false;
	}
	F.AlwaysIntervene(EWardenChannel::Strike);
	F.KeepOnly({StrikeId});
	const FPlannedMove Move = FindMoveThroughSource(F, Koschei, 4, 1);
	if (!TestTrue(TEXT("En movimiento en curso"), !Move.Path.IsEmpty() && MoveInto(F, Koschei, Move, 1)))
	{
		return false;
	}
	TestFalse(TEXT("No hay que aplazar"), Soulbound::Rules::IsWardenDeferred(F.State));
	TestTrue(TEXT("Se puede cortar"), Soulbound::Rules::CanInterruptTurn(F.State));
	TestTrue(TEXT("El que se mueve está en juego"), Soulbound::Rules::IsSeatInPlay(F.State, Koschei));
	TestTrue(TEXT("El golpe es candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(F.IndexOf(StrikeId)));
	const FWardenDecision Decision = F.Evaluate(600.0);
	TestEqual(TEXT("El cerebro golpea"), AsInt(Decision.Kind), AsInt(EWardenDecisionKind::Strike));
	TestEqual(TEXT("Con el golpe"), Decision.EventIndex, F.IndexOf(StrikeId));
	const int32 From = F.State.EventLog.Num();
	Soulbound::Rules::EvaluateWarden(F.State, 600.0, F.Random);
	const TArray<FMatchEvent> Cut = F.Since(From, EMatchEventType::TurnInterrupted);
	TestTrue(TEXT("Y corta a medio camino"), Cut.Num() == 1 && Cut[0].Value == AsInt(ETurnPhase::Moving));
	TestEqual(TEXT("La ficha en la casilla en que estaba"), F.State.Seats[Koschei].TileId, Move.Path[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugStrikeTest, "Soulbound.Core.Debug.GolpeAhora",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugStrikeTest::RunTest(const FString& Parameters)
{
	// SbStrike (S-020, patrón de S-011): lanza el evento como golpe ahora. A medio camino corta el turno (la edición antes del
	// corte) y la ficha se queda en P; entre turnos, como SbEvent, sin corte; en un duelo no corta y se rechaza; un evento que no
	// existe se rechaza antes de cortar nada.
	FWardenFixture F;
	if (!StartForCut(*this, F))
	{
		return false;
	}
	const FPlannedMove Move = FindMoveThroughSource(F, Koschei, 4, 1);
	if (!TestTrue(TEXT("En la segunda casilla"), !Move.Path.IsEmpty() && MoveInto(F, Koschei, Move, 2)))
	{
		return false;
	}
	FMatchState Before = F.State;
	TestEqual(TEXT("Evento inexistente: se rechaza"), AsInt(Soulbound::Debug::StrikeEvent(F.State, TEXT("no-existe"), F.Now, F.Random, F.WardenRandom)), AsInt(EWardenEventStart::UnknownEvent));
	TestTrue(TEXT("... sin cortar nada"), Soulbound::Tests::SameState(Before, F.State));

	int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("A medio camino: empieza"), AsInt(Soulbound::Debug::StrikeEvent(F.State, StrikeId, F.Now, F.Random, F.WardenRandom)), AsInt(EWardenEventStart::Started));
	const TArray<FMatchEvent> Cut = F.Since(From, EMatchEventType::TurnInterrupted);
	TestTrue(TEXT("A medio camino: corta con la fase Moving"), Cut.Num() == 1 && Cut[0].Value == AsInt(ETurnPhase::Moving));
	TestTrue(TEXT("A medio camino: la edición va primero"), F.State.EventLog.IsValidIndex(From) && F.State.EventLog[From].Type == EMatchEventType::DebugEdit
		&& F.State.EventLog[From].Value == AsInt(EDebugEditKind::StrikeEvent));
	TestEqual(TEXT("A medio camino: la ficha en P"), F.State.Seats[Koschei].TileId, Move.Path[1]);
	TestEqual(TEXT("A medio camino: el evento cae sobre el cortado"), F.State.Seats[Koschei].LostTurns, 1);
	CheckUnresolved(*this, F, From, TEXT("A medio camino"));

	// Entre turnos: como SbEvent, sin corte.
	TestEqual(TEXT("Entre turnos"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	From = F.State.EventLog.Num();
	TestEqual(TEXT("Entre turnos: empieza"), AsInt(Soulbound::Debug::StrikeEvent(F.State, StrikeId, F.Now, F.Random, F.WardenRandom)), AsInt(EWardenEventStart::Started));
	TestEqual(TEXT("Entre turnos: sin corte"), F.Since(From, EMatchEventType::TurnInterrupted).Num(), 0);
	TestEqual(TEXT("Entre turnos: el evento se aplica"), F.Since(From, EMatchEventType::WardenEventStarted).Num(), 1);

	// En un duelo no se corta: se rechaza sin tocar nada.
	F.State.TurnPhase = ETurnPhase::Duel;
	F.State.ActiveSeat = Fenrir;
	Before = F.State;
	TestEqual(TEXT("En un duelo: se rechaza"), AsInt(Soulbound::Debug::StrikeEvent(F.State, StrikeId, F.Now, F.Random, F.WardenRandom)), AsInt(EWardenEventStart::NotBetweenTurns));
	TestTrue(TEXT("... sin tocar nada"), Soulbound::Tests::SameState(Before, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenCutSceneTest, "Soulbound.Core.Warden.Cut.EscenaCortanteYNoCortante",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenCutSceneTest::RunTest(const FString& Parameters)
{
	// CA-WAR-3b (R-WAR-4, glosario `CutsTurn`): una escena cortante corta como un golpe; una no cortante no corta: espera al hueco
	// entre turnos y entra entonces, dirigida por el último que jugó.
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.AddBagEvent(TEXT("escena-cortante"), EAct::ActTwo, EWardenChannel::Scene, {{App(EEffect::LoseTurn, EEffectTarget::ActivePlayer, 1)}}, true);
		F.BeginTurn(Koschei);
		F.Advance();
		const int32 Before = F.State.EventLog.Num();
		F.Apply(F.MakeDecision(EWardenDecisionKind::Scene, TEXT("escena-cortante")));
		TestEqual(TEXT("La escena cortante corta"), F.Since(Before, EMatchEventType::TurnInterrupted).Num(), 1);
		TestEqual(TEXT("Y se aplica ya"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 1);
	}
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		const int32 Scene = F.AddBagEvent(TEXT("escena-que-espera"), EAct::ActTwo, EWardenChannel::Scene, {{App(EEffect::LoseTurn, EEffectTarget::ActivePlayer, 1)}}, false);
		F.BeginTurn(Koschei);
		F.Advance();
		const int32 Before = F.State.EventLog.Num();
		F.Apply(F.MakeDecision(EWardenDecisionKind::Scene, TEXT("escena-que-espera")));
		TestEqual(TEXT("La no cortante no corta"), F.Since(Before, EMatchEventType::TurnInterrupted).Num(), 0);
		TestEqual(TEXT("Ni se aplica aún"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 0);
		TestEqual(TEXT("Espera al hueco"), F.State.Warden.PendingScene, Scene);
		TestEqual(TEXT("El turno sigue"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::RollAndChoose));

		F.FinishTurn();
		TestEqual(TEXT("Acabado el turno, aún no ha entrado"), F.Since(Before, EMatchEventType::WardenEventStarted).Num(), 0);
		F.Advance();
		const TArray<FMatchEvent> Started = F.Since(Before, EMatchEventType::WardenEventStarted);
		if (TestEqual(TEXT("En el hueco, entra"), Started.Num(), 1))
		{
			TestEqual(TEXT("Dirigida por el último que jugó"), Started[0].Seat, Koschei);
		}
		TestEqual(TEXT("Ya no espera"), F.State.Warden.PendingScene, INDEX_NONE);
		TestEqual(TEXT("Y el turno perdido cae sobre él"), F.State.Seats[Koschei].LostTurns, 1);
		TestEqual(TEXT("El turno siguiente aún no ha empezado"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenStrikeBetweenTurnsTest, "Soulbound.Core.Warden.Cut.GolpeEntreTurnosNoCorta",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenStrikeBetweenTurnsTest::RunTest(const FString& Parameters)
{
	// R-WAR-4 (decisión no solicitada aceptada por el autor el 19/10/2026): un golpe sin turno en curso se aplica sin cortar
	// nada, con el último que jugó como activo.
	FWardenFixture F;
	if (!StartForCut(*this, F))
	{
		return false;
	}
	F.Director(Koschei);
	const int32 Before = F.State.EventLog.Num();
	F.Apply(F.MakeDecision(EWardenDecisionKind::Strike, StrikeId));
	TestEqual(TEXT("No corta nada"), F.Since(Before, EMatchEventType::TurnInterrupted).Num(), 0);
	TestEqual(TEXT("No termina ningún turno"), F.Since(Before, EMatchEventType::TurnEnded).Num(), 0);
	TestEqual(TEXT("Se aplica"), F.SeatsSince(Before, EMatchEventType::WardenEventStarted), TArray<int32>{Koschei});
	TestEqual(TEXT("Sobre el último que jugó"), F.State.Seats[Koschei].LostTurns, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenHaltCutsTest, "Soulbound.Core.Warden.Cut.AltoSoloCorta",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenHaltCutsTest::RunTest(const FString& Parameters)
{
	// CA-TAB-28 (R-TAB-28, R-EFE-1): un golpe con Alto (Tropiezo) corta el turno y no aplica nada más: su efecto es el corte.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.BeginTurn(Koschei);
	F.Advance();
	const FSeatState Seat = F.State.Seats[Koschei];
	const int32 Before = F.State.EventLog.Num();
	F.Apply(F.MakeDecision(EWardenDecisionKind::Strike, TEXT("tropiezo")));
	TestEqual(TEXT("Corta el turno"), F.Since(Before, EMatchEventType::TurnInterrupted).Num(), 1);
	const TArray<FMatchEvent> Applied = F.Since(Before, EMatchEventType::EffectApplied);
	if (TestEqual(TEXT("Un efecto aplicado"), Applied.Num(), 1))
	{
		TestEqual(TEXT("El Alto"), Applied[0].Value, AsInt(EEffect::Halt));
		TestEqual(TEXT("Sobre el cortado"), Applied[0].Seat, Koschei);
	}
	TestEqual(TEXT("Sin burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 0);
	const FSeatState& After = F.State.Seats[Koschei];
	TestTrue(TEXT("Y nada más: ficha, runas y turnos como estaban"), After.TileId == Seat.TileId && After.Runes == Seat.Runes
		&& After.LostTurns == Seat.LostTurns && After.ExtraTurns == Seat.ExtraTurns && After.Insolence == Seat.Insolence);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardenHaltBetweenTurnsTest, "Soulbound.Core.Warden.Cut.AltoEntreTurnosEsBurla",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWardenHaltBetweenTurnsTest::RunTest(const FString& Parameters)
{
	// CA-TAB-28 (R-TAB-28): fuera de un golpe que corta, sin turno que cortar, el Alto no es válido: burla. Ni por SbEvent ni
	// como golpe entre turnos; y el cerebro no lo elige sin turno en curso.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	F.Director(Koschei);
	TestEqual(TEXT("Sin corte, el Alto no vale"), AsInt(Soulbound::Rules::CanApplyEffect(F.State, EEffect::Halt, Koschei, Koschei)), AsInt(EEffectResult::NoTurnToCut));

	int32 Before = F.State.EventLog.Num();
	TestEqual(TEXT("Lanzado entre turnos (SbEvent), empieza"), AsInt(F.Launch(TEXT("tropiezo"))), AsInt(EWardenEventStart::Started));
	TestEqual(TEXT("Y es burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin efecto aplicado"), F.Since(Before, EMatchEventType::EffectApplied).Num(), 0);

	Before = F.State.EventLog.Num();
	F.Apply(F.MakeDecision(EWardenDecisionKind::Strike, TEXT("tropiezo")));
	TestEqual(TEXT("Como golpe entre turnos, también burla"), F.Since(Before, EMatchEventType::WardenTaunt).Num(), 1);
	TestEqual(TEXT("Sin corte"), F.Since(Before, EMatchEventType::TurnInterrupted).Num(), 0);

	F.State.Warden.bGapUsed = false;
	const int32 Halt = F.IndexOf(TEXT("tropiezo"));
	TestFalse(TEXT("Entre turnos, el cerebro no lo tiene de candidato"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(Halt));
	F.BeginTurn(Koschei);
	F.Advance();
	TestTrue(TEXT("Con un turno en curso, sí"), Soulbound::Rules::EventCandidates(F.State, EWardenChannel::Strike, 600.0).Contains(Halt));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
