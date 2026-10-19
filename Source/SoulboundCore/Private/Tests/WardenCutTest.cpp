// Tests del golpe que corta el turno (S-016, R-WAR-4, R-TUR-11, R-TAB-26) y del Alto (R-TAB-28, R-EFE-1). Un golpe corta el
// turno en cualquier fase en que se puede cortar: TurnInterrupted con la fase, la tirada perdida con el consumo fijo del
// sorteo respetado, la ficha donde estaba, las elecciones cerradas y el evento aplicado entre turnos con el cortado como
// activo; el turno pasa al siguiente. Una escena cortante corta; una no cortante espera al hueco. Cada test cita el criterio
// (CA-XXX-n) y la regla (R-XXX-n) que cubre. Filtro: "Soulbound.Core.Warden.Cut".
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos).
// Las decisiones se aplican hechas a mano (ApplyWardenDecision): el test mira el corte, no el sorteo del cerebro.

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
