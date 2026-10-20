// Tests del Vacío (S-013, GDD 05, con Desvío del GDD 04 y el inicio de turno del GDD 02): entrar (R-VAC-1, R-TAB-24) y la
// inmunidad dentro (R-INT-3, R-DIR-2), la salida al empezar cada turno propio en el orden de R-VAC-2 (Eco, Desvío, escalera
// de escape de datos, Mictlantecuhtli), la vuelta al hogar al salir (R-VAC-3), la cuenta desde cero (R-VAC-4), Liberación
// (R-VAC-5, R-EFE-17), el destierro en la carrera (R-CAR-7), la Vuelta al hogar (R-EFE-4), el registro y el sorteo de la
// partida, que no se toca. Más las decisiones del autor del 08/10/2026: quién está en juego, Desvío en el turno de salida y
// los turnos perdidos. Cada test cita el criterio de aceptación (CA-XXX-n) y la regla (R-XXX-n) que cubre. Se lanzan sin
// editor con Scripts/test.ps1. Filtro: "Soulbound.Core.Void".
//
// Montaje: el de las elecciones (ChoiceFixture.h), tablero de 35 casillas. El asiento 0 se vincula a Fenrir y el 1 a
// Mictlantecuhtli (afinidad VoidEscape) y la carrera termina: los asientos 2 y 3 quedan Ecos con hogar. Las runas, las
// posiciones y el estado de un duelo o un trilero se fijan a mano: es lo que el test necesita, no lo que comprueba. Los
// turnos se juegan con la máquina de turnos (AdvanceTurn y las intenciones).

#include "ChoiceFixture.h"
#include "Match/BondRaceRules.h"
#include "Match/DuelRules.h"
#include "Match/EffectRules.h"
#include "Match/RuneRules.h"
#include "Match/TurnRules.h"
#include "Match/VoidRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::EventsOfType;
	using Soulbound::Tests::FChoiceFixture;
	using Soulbound::Tests::SameState;

	/** Dominios de Content/Data/Elects/elegidos.json: Fenrir (Desvío con la runa 4) y Mictlantecuhtli (afinidad voidEscape). */
	constexpr int32 FenrirDomain = 1;
	constexpr int32 MictlanDomain = 3;

	/** Asientos del montaje. */
	constexpr int32 Fenrir = 0;
	constexpr int32 Mictlan = 1;
	constexpr int32 EchoA = 2;
	constexpr int32 EchoB = 3;

	int32 AsInt(EEffectResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EIntentResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EChoiceKind Kind) { return static_cast<int32>(Kind); }
	int32 AsInt(ETurnPhase Phase) { return static_cast<int32>(Phase); }
	int32 AsInt(ESkillTrial Trial) { return static_cast<int32>(Trial); }
	int32 AsInt(ESkillDifficulty Difficulty) { return static_cast<int32>(Difficulty); }
	int32 AsInt(EVoidExit Exit) { return static_cast<int32>(Exit); }

	/** Partida tras la carrera: el asiento 0 se vincula a Fenrir y el 1 a Mictlantecuhtli; el 2 y el 3 quedan Ecos con hogar. */
	bool StartAfterRace(FAutomationTestBase& Test, FChoiceFixture& F)
	{
		if (!F.Start(Test) || !F.Bond(Test, Fenrir, FenrirDomain) || !F.Bond(Test, Mictlan, MictlanDomain))
		{
			return false;
		}
		if (!Test.TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now)))
		{
			return false;
		}
		// Nadie tiene runas al empezar: cada test da las que necesita.
		for (FSeatState& Seat : F.State.Seats)
		{
			Seat.Runes.Init(0, Soulbound::Board::DomainCount);
		}
		return Test.TestTrue(TEXT("Los asientos 2 y 3 quedan Ecos con hogar"), F.State.Seats[EchoA].Role == ESeatRole::LostEcho && F.State.Seats[EchoA].DomainId != INDEX_NONE
			&& F.State.Seats[EchoB].Role == ESeatRole::LostEcho && F.State.Seats[EchoB].DomainId != INDEX_NONE);
	}

	/** Destierra a Seat entre turnos, un segundo después del anterior, sin la Furia. Devuelve lo que dijo la regla. */
	EEffectResult BanishSeat(FChoiceFixture& F, int32 Seat, bool bFury = false)
	{
		F.Now += 1.0;
		return Soulbound::Rules::Banish(F.State, Seat, bFury, F.Now);
	}

	/** Avanza la máquina hasta BetweenTurns (el turno en curso ya no espera a nadie). Devuelve si llegó. */
	bool EndTurn(FChoiceFixture& F)
	{
		for (int32 Step = 0; Step < 4 && F.State.TurnPhase != ETurnPhase::BetweenTurns; ++Step)
		{
			F.Advance();
		}
		return F.State.TurnPhase == ETurnPhase::BetweenTurns;
	}

	/** Eventos de un tipo desde la posición From del registro. */
	int32 CountSince(const FChoiceFixture& F, int32 From, EMatchEventType Type)
	{
		int32 Count = 0;
		for (int32 Index = From; Index < F.State.EventLog.Num(); ++Index)
		{
			Count += F.State.EventLog[Index].Type == Type ? 1 : 0;
		}
		return Count;
	}

	/** Último evento de un tipo, o uno vacío (TurnStarted sin asiento) si no hay. */
	FMatchEvent LastOf(const FChoiceFixture& F, EMatchEventType Type)
	{
		const TArray<FMatchEvent> Events = EventsOfType(F.State, Type);
		return Events.IsEmpty() ? FMatchEvent() : Events.Last();
	}

	/**
	 * Juega un turno de Seat en el Vacío que acaba en el minijuego de escape: empieza (BeginTurn), comprueba que espera el
	 * escape con la dificultad Expected, entrega el resultado bPassed y termina el turno. Comprueba también que en el turno
	 * no hubo tirada. Devuelve si todo fue como se esperaba.
	 */
	bool PlayEscape(FAutomationTestBase& Test, FChoiceFixture& F, int32 Seat, ESkillDifficulty Expected, bool bPassed, const FString& Context)
	{
		const int32 From = F.State.EventLog.Num();
		F.BeginTurn(Seat);
		bool bOk = Test.TestEqual(FString::Printf(TEXT("%s: se le pide el minijuego"), *Context), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::SkillTrial));
		bOk &= Test.TestEqual(FString::Printf(TEXT("%s: de escape del Vacío"), *Context), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::VoidEscape));
		bOk &= Test.TestEqual(FString::Printf(TEXT("%s: con su dificultad"), *Context), AsInt(F.State.PendingDifficulty), AsInt(Expected));
		bOk &= Test.TestEqual(FString::Printf(TEXT("%s: entrega el resultado"), *Context),
			AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, Seat, bPassed, F.Now)), AsInt(EIntentResult::Accepted));
		bOk &= Test.TestTrue(FString::Printf(TEXT("%s: el turno termina"), *Context), EndTurn(F));
		bOk &= Test.TestEqual(FString::Printf(TEXT("%s: sin tirada"), *Context), CountSince(F, From, EMatchEventType::Rolled), 0);
		return bOk;
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

	/** Casilla normal número Index del tablero. */
	int32 Normal(const FChoiceFixture& F, int32 Index)
	{
		const TArray<int32> Tiles = F.NormalTiles();
		return Tiles.IsValidIndex(Index) ? Tiles[Index] : INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidOffBoardTest,
	"Soulbound.Core.Void.FueraDelTablero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidOffBoardTest::RunTest(const FString& Parameters)
{
	// CA-TAB-25 y CA-VAC-3 (R-TAB-24, R-VAC-1, R-INT-3, R-DIR-2): dado un asiento desterrado, su posición es "ninguna", no
	// cuenta como ocupante de ninguna casilla, y no aparece como objetivo de robo, duelo, Expulsión ni Vuelta al hogar; al
	// volver, está en su hogar.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Alone = Normal(F, 0);
	const int32 Meeting = Normal(F, 1);
	F.State.Seats[Mictlan].TileId = Alone;
	F.State.Seats[Mictlan].Runes[0] = 1;
	F.State.Seats[EchoA].TileId = Meeting;
	F.State.Seats[EchoB].TileId = Meeting;

	TestEqual(TEXT("El destierro se aplica"), AsInt(BanishSeat(F, Mictlan)), AsInt(EEffectResult::Applied));
	TestEqual(TEXT("Su posición es \"ninguna\""), F.State.Seats[Mictlan].TileId, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("Está en el Vacío"), F.State.Seats[Mictlan].IsInVoid());
	TestFalse(TEXT("Su casilla anterior queda libre"), F.State.IsTileOccupied(Alone));
	for (const FTileState& Tile : F.State.Board.Tiles)
	{
		if (Tile.TileId != Meeting && F.State.IsTileOccupied(Tile.TileId))
		{
			const bool bSomeoneThere = F.State.Seats.ContainsByPredicate([&Tile](const FSeatState& Seat) { return Seat.TileId == Tile.TileId; });
			TestTrue(FString::Printf(TEXT("La casilla %d solo cuenta como ocupada si hay una ficha en ella"), Tile.TileId), bSomeoneThere);
		}
	}

	// Duelo desde un Nodo de Duelo: cualquiera, esté donde esté, salvo quien está en el Vacío.
	const int32 Node = MakeDuelNode(F);
	if (!TestTrue(TEXT("El tablero tiene un Nodo de Poder"), Node != INDEX_NONE))
	{
		return false;
	}
	F.GivePower(Fenrir, EPower::Expel);
	TestTrue(TEXT("Fenrir termina en el Nodo de Duelo"), F.Land(Fenrir, Node));
	const TArray<int32> Duels = Soulbound::Rules::DuelTargets(F.State, Fenrir);
	TestTrue(TEXT("Desde el Nodo puede retar a los dos Ecos"), Duels.Contains(EchoA) && Duels.Contains(EchoB));
	TestFalse(TEXT("... pero no al desterrado"), Duels.Contains(Mictlan));
	for (const FPlayerActionOption& Option : F.State.PendingChoice.ActionOptions)
	{
		TestNotEqual(TEXT("Ninguna acción contra un jugador apunta al desterrado"), Option.TargetSeat, Mictlan);
	}
	TestFalse(TEXT("No es objetivo de Expulsión"), Soulbound::Rules::ExpelTargets(F.State, Fenrir).Contains(Mictlan));
	F.FinishTurn();

	// Robo: un Eco que se ha movido no puede robar al desterrado.
	F.State.ActiveSeat = EchoA;
	F.State.bActiveSeatMoved = true;
	TestFalse(TEXT("No es objetivo de robo"), Soulbound::Rules::StealTargets(F.State, EchoA).Contains(Mictlan));
	F.State.bActiveSeatMoved = false;
	F.State.ActiveSeat = Fenrir;

	// Ningún efecto salvo Liberación.
	TestEqual(TEXT("No es objetivo de Vuelta al hogar"), AsInt(Soulbound::Rules::CanSendHome(F.State, Mictlan)), AsInt(EEffectResult::InVoid));
	TestEqual(TEXT("No se le destierra otra vez"), AsInt(Soulbound::Rules::CanBanish(F.State, Mictlan, true)), AsInt(EEffectResult::InVoid));

	// Al volver, está en su hogar.
	F.Now += 1.0;
	TestEqual(TEXT("La Liberación se aplica"), AsInt(Soulbound::Rules::ReleaseVoid(F.State, F.Now)), AsInt(EEffectResult::Applied));
	TestEqual(TEXT("Al volver, está en su hogar"), F.State.Seats[Mictlan].TileId, Soulbound::Rules::HomeTile(F.State, Mictlan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidLadderTest,
	"Soulbound.Core.Void.EscaleraDeEscape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidLadderTest::RunTest(const FString& Parameters)
{
	// CA-VAC-1 (R-VAC-2, R-VAC-3, R-TUR-3): dado un Elegido sin Desvío en el Vacío, en su primer turno se le pide el
	// minijuego en difícil; si falla, en el segundo en normal; si falla, en el tercero en fácil; si falla, en el cuarto está
	// en su hogar sin jugar y el turno termina. La escalera sale de datos.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const TArray<ESkillDifficulty> FromFile = {ESkillDifficulty::Hard, ESkillDifficulty::Normal, ESkillDifficulty::Easy};
	TestTrue(TEXT("La escalera de partida.json es difícil, normal, fácil"), F.State.Params.VoidEscapeLadder == FromFile);
	FChoiceFixture OtherData = F;

	TestEqual(TEXT("Fenrir es desterrado"), AsInt(BanishSeat(F, Fenrir)), AsInt(EEffectResult::Applied));
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Hard, false, TEXT("Primer turno"));
	TestTrue(TEXT("Tras fallar sigue en el Vacío"), F.State.Seats[Fenrir].IsInVoid());
	TestEqual(TEXT("Su cuenta es 1"), F.State.Seats[Fenrir].VoidTurns, 1);
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Normal, false, TEXT("Segundo turno"));
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Easy, false, TEXT("Tercer turno"));
	TestTrue(TEXT("Tras tres fallos sigue en el Vacío"), F.State.Seats[Fenrir].IsInVoid());

	const int32 From = F.State.EventLog.Num();
	F.BeginTurn(Fenrir);
	TestNotEqual(TEXT("Cuarto turno: no se le pide el minijuego"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::SkillTrial));
	TestEqual(TEXT("Cuarto turno: está en su hogar"), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
	TestEqual(TEXT("... por el límite de la escalera"), LastOf(F, EMatchEventType::LeftVoid).Value, AsInt(EVoidExit::TurnLimit));
	TestTrue(TEXT("... y el turno termina"), EndTurn(F));
	TestEqual(TEXT("... sin jugar: sin tirada"), CountSince(F, From, EMatchEventType::Rolled), 0);
	TestEqual(TEXT("... sin minijuego"), CountSince(F, From, EMatchEventType::VoidEscapeFailed) + CountSince(F, From, EMatchEventType::VoidEscapePassed), 0);
	TestEqual(TEXT("Fuera, su cuenta vuelve a 0"), F.State.Seats[Fenrir].VoidTurns, 0);

	// La escalera sale de datos: con otra, otra secuencia y otro turno de salida.
	OtherData.State.Params.VoidEscapeLadder = {ESkillDifficulty::Normal, ESkillDifficulty::Easy};
	BanishSeat(OtherData, Fenrir);
	PlayEscape(*this, OtherData, Fenrir, ESkillDifficulty::Normal, false, TEXT("Otros datos, primer turno"));
	PlayEscape(*this, OtherData, Fenrir, ESkillDifficulty::Easy, false, TEXT("Otros datos, segundo turno"));
	OtherData.BeginTurn(Fenrir);
	TestFalse(TEXT("Otros datos: sale en el tercer turno"), OtherData.State.Seats[Fenrir].IsInVoid());
	TestEqual(TEXT("Otros datos: por el límite"), LastOf(OtherData, EMatchEventType::LeftVoid).Value, AsInt(EVoidExit::TurnLimit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidEscapePassedTest,
	"Soulbound.Core.Void.EscapeSuperado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidEscapePassedTest::RunTest(const FString& Parameters)
{
	// CA-VAC-1 (R-VAC-2, R-VAC-3): cuando supera el minijuego en cualquier turno, está en su hogar y el turno termina sin
	// tirada.
	for (int32 PassOn = 1; PassOn <= 3; ++PassOn)
	{
		FChoiceFixture F;
		if (!StartAfterRace(*this, F))
		{
			return false;
		}
		BanishSeat(F, Fenrir);
		const ESkillDifficulty Ladder[] = {ESkillDifficulty::Hard, ESkillDifficulty::Normal, ESkillDifficulty::Easy};
		for (int32 Turn = 1; Turn <= PassOn; ++Turn)
		{
			PlayEscape(*this, F, Fenrir, Ladder[Turn - 1], Turn == PassOn, FString::Printf(TEXT("Supera en el turno %d, turno %d"), PassOn, Turn));
		}
		TestEqual(FString::Printf(TEXT("Supera en el turno %d: está en su hogar"), PassOn), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
		TestEqual(FString::Printf(TEXT("Supera en el turno %d: sale por el escape"), PassOn), LastOf(F, EMatchEventType::LeftVoid).Value, AsInt(EVoidExit::Escaped));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMictlanAndEchoTest,
	"Soulbound.Core.Void.MictlantecuhtliYEco",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidMictlanAndEchoTest::RunTest(const FString& Parameters)
{
	// CA-VAC-2 y CA-ELE-5 (R-VAC-2, R-ELE-2, R-ECO-5): dado Mictlantecuhtli, su primer escape es en fácil y en el segundo turno
	// sale sin jugar. Dado un Eco, sale en su primer turno sin jugar, en su hogar.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	TestTrue(TEXT("El asiento 1 tiene la afinidad de escape"), Soulbound::Rules::HasAffinity(F.State, Mictlan, EAffinity::VoidEscape));
	BanishSeat(F, Mictlan);
	PlayEscape(*this, F, Mictlan, ESkillDifficulty::Easy, false, TEXT("Mictlantecuhtli, primer turno"));
	TestTrue(TEXT("Tras fallar sigue en el Vacío"), F.State.Seats[Mictlan].IsInVoid());
	int32 From = F.State.EventLog.Num();
	F.BeginTurn(Mictlan);
	TestEqual(TEXT("Segundo turno: está en su hogar"), F.State.Seats[Mictlan].TileId, Soulbound::Rules::HomeTile(F.State, Mictlan));
	TestEqual(TEXT("... por el límite de su escalera"), LastOf(F, EMatchEventType::LeftVoid).Value, AsInt(EVoidExit::TurnLimit));
	TestTrue(TEXT("... y el turno termina"), EndTurn(F));
	TestEqual(TEXT("... sin tirada"), CountSince(F, From, EMatchEventType::Rolled), 0);

	// Eco: sale en su primer turno, sin minijuego ni tirada.
	F.State.Seats[EchoA].TileId = Normal(F, 0);
	BanishSeat(F, EchoA);
	From = F.State.EventLog.Num();
	F.BeginTurn(EchoA);
	TestNotEqual(TEXT("Eco: no se le pide el minijuego"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::SkillTrial));
	TestEqual(TEXT("Eco: en su primer turno está en su hogar"), F.State.Seats[EchoA].TileId, Soulbound::Rules::HomeTile(F.State, EchoA));
	TestEqual(TEXT("Eco: sale por ser Eco"), LastOf(F, EMatchEventType::LeftVoid).Value, AsInt(EVoidExit::Echo));
	TestTrue(TEXT("Eco: el turno termina"), EndTurn(F));
	TestEqual(TEXT("Eco: sin tirada"), CountSince(F, From, EMatchEventType::Rolled), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDeflectionAcceptedTest,
	"Soulbound.Core.Void.DesvioAceptado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidDeflectionAcceptedTest::RunTest(const FString& Parameters)
{
	// CA-POD-4 (R-POD-6, R-VAC-3): dado un Elegido en el Vacío con una runa que le da Desvío, al empezar su turno se le
	// ofrece usarlo; si acepta, la runa ha vuelto a su Fuente, está en su hogar y juega el turno completo (tira y mueve); si
	// era su última de ese tipo, ya no tiene Desvío.
	for (const int32 Count : {1, 2})
	{
		FChoiceFixture F;
		if (!StartAfterRace(*this, F))
		{
			return false;
		}
		const FString Context = FString::Printf(TEXT("Con %d runa(s) de Desvío"), Count);
		const int32 RuneType = F.State.Elects.Elects[FenrirDomain].RuneToPower.IndexOfByKey(EPower::Deflection);
		const int32 Source = F.SourceOf(RuneType);
		F.State.Seats[Fenrir].Runes[RuneType] = Count;
		const int32 Reserve = F.State.Board.Tiles[Source].RuneReserve;
		TestEqual(FString::Printf(TEXT("%s: la runa que da Desvío a Fenrir"), *Context), Soulbound::Rules::DeflectionRuneType(F.State, Fenrir), RuneType);
		BanishSeat(F, Fenrir);

		const int32 From = F.State.EventLog.Num();
		F.BeginTurn(Fenrir);
		const FPendingChoice& Choice = F.State.PendingChoice;
		TestEqual(FString::Printf(TEXT("%s: se le ofrece Desvío"), *Context), AsInt(Choice.Kind), AsInt(EChoiceKind::Deflection));
		TestEqual(FString::Printf(TEXT("%s: a él"), *Context), Choice.Seat, Fenrir);
		TestTrue(FString::Printf(TEXT("%s: con la opción \"usar\" y \"no\""), *Context), Choice.Options == TArray<int32>{1} && Choice.CanDecline());
		TestEqual(FString::Printf(TEXT("%s: bajo el reloj de turno"), *Context), Choice.DeadlineSeconds, F.State.TurnDeadlineSeconds);
		TestEqual(FString::Printf(TEXT("%s: en el inicio del turno"), *Context), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Start));

		TestEqual(FString::Printf(TEXT("%s: acepta"), *Context), AsInt(F.Choose(Fenrir, EChoiceKind::Deflection, 1)), AsInt(EIntentResult::Accepted));
		TestEqual(FString::Printf(TEXT("%s: la runa vuelve a su Fuente"), *Context), F.State.Board.Tiles[Source].RuneReserve, Reserve + 1);
		TestEqual(FString::Printf(TEXT("%s: y sale de su Brazal"), *Context), F.State.Seats[Fenrir].Runes[RuneType], Count - 1);
		TestEqual(FString::Printf(TEXT("%s: está en su hogar"), *Context), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
		TestEqual(FString::Printf(TEXT("%s: sale por Desvío"), *Context), LastOf(F, EMatchEventType::LeftVoid).Value, AsInt(EVoidExit::Deflected));
		TestEqual(FString::Printf(TEXT("%s: Desvío sigue activo solo si le queda runa"), *Context),
			Soulbound::Rules::IsPowerActive(F.State, Fenrir, EPower::Deflection), Count > 1);

		// Juega el turno completo: tira y mueve.
		F.Advance();
		TestEqual(FString::Printf(TEXT("%s: tira"), *Context), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::RollAndChoose));
		TestEqual(FString::Printf(TEXT("%s: un Rolled en su turno"), *Context), CountSince(F, From, EMatchEventType::Rolled), 1);
		if (TestFalse(FString::Printf(TEXT("%s: tiene adónde ir"), *Context), F.State.ReachableOptions.IsEmpty()))
		{
			TestEqual(FString::Printf(TEXT("%s: mueve"), *Context),
				AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, Fenrir, F.State.ReachableOptions[0], F.Now)), AsInt(EIntentResult::Accepted));
		}
		F.FinishTurn();
		TestEqual(FString::Printf(TEXT("%s: un Moved en su turno"), *Context), CountSince(F, From, EMatchEventType::Moved), 1);
		TestEqual(FString::Printf(TEXT("%s: sin minijuego de escape"), *Context), CountSince(F, From, EMatchEventType::VoidEscapeFailed) + CountSince(F, From, EMatchEventType::VoidEscapePassed), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDeflectionDeclinedTest,
	"Soulbound.Core.Void.DesvioRechazadoOAgotado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidDeflectionDeclinedTest::RunTest(const FString& Parameters)
{
	// CA-POD-4 (R-POD-6, R-VAC-2): si rechaza Desvío, juega el escape y la runa no se toca. Agotar la elección es "no", sin
	// castigo.
	for (const bool bExpire : {false, true})
	{
		FChoiceFixture F;
		if (!StartAfterRace(*this, F))
		{
			return false;
		}
		const FString Context = bExpire ? TEXT("Agota la elección") : TEXT("Rechaza");
		F.GivePower(Fenrir, EPower::Deflection);
		const TArray<int32> Runes = F.State.Seats[Fenrir].Runes;
		BanishSeat(F, Fenrir);
		F.BeginTurn(Fenrir);
		TestEqual(FString::Printf(TEXT("%s: se le ofrece Desvío"), *Context), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Deflection));
		if (bExpire)
		{
			F.Now = F.State.PendingChoice.DeadlineSeconds;
			TestEqual(TEXT("Llega tarde: se rechaza"), AsInt(F.Choose(Fenrir, EChoiceKind::Deflection, 1)), AsInt(EIntentResult::ClockExpired));
			F.Advance();
			TestEqual(TEXT("El agotamiento queda registrado"), LastOf(F, EMatchEventType::ChoiceExpired).Value, AsInt(EChoiceKind::Deflection));
			TestEqual(TEXT("Sin castigo: la insolencia no sube"), F.State.Seats[Fenrir].Insolence, 0);
		}
		else
		{
			TestEqual(TEXT("Dice que no"), AsInt(F.Choose(Fenrir, EChoiceKind::Deflection, INDEX_NONE)), AsInt(EIntentResult::Accepted));
		}
		TestEqual(FString::Printf(TEXT("%s: juega el escape"), *Context), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::VoidEscape));
		TestEqual(FString::Printf(TEXT("%s: en difícil"), *Context), AsInt(F.State.PendingDifficulty), AsInt(ESkillDifficulty::Hard));
		TestTrue(FString::Printf(TEXT("%s: la runa no se toca"), *Context), F.State.Seats[Fenrir].Runes == Runes);
		TestTrue(FString::Printf(TEXT("%s: sigue en el Vacío"), *Context), F.State.Seats[Fenrir].IsInVoid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidNoDeflectionTest,
	"Soulbound.Core.Void.SinDesvioNoSeOfrece",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidNoDeflectionTest::RunTest(const FString& Parameters)
{
	// CA-POD-4 (R-POD-6): sin Desvío activo no se le ofrece nada: con runas que dan otros poderes, va directo al escape.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	F.GivePower(Fenrir, EPower::Duel);
	F.GivePower(Fenrir, EPower::Expel);
	TestEqual(TEXT("Sin Desvío activo, no hay runa de Desvío"), Soulbound::Rules::DeflectionRuneType(F.State, Fenrir), static_cast<int32>(INDEX_NONE));
	BanishSeat(F, Fenrir);
	F.BeginTurn(Fenrir);
	TestFalse(TEXT("No se le ofrece ninguna elección"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("Va directo al escape"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::VoidEscape));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDeflectionOnLastTurnTest,
	"Soulbound.Core.Void.DesvioEnElTurnoDeSalida",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidDeflectionOnLastTurnTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 08/10/2026 (R-VAC-2, R-POD-6): Desvío se ofrece también en el turno en que saldría solo (el
	// cuarto; el segundo de Mictlantecuhtli). Si lo usa, sale y juega el turno; si lo rechaza, sale sin jugar.
	for (const bool bUse : {true, false})
	{
		FChoiceFixture F;
		if (!StartAfterRace(*this, F))
		{
			return false;
		}
		const FString Context = bUse ? TEXT("Lo usa") : TEXT("Lo rechaza");
		F.GivePower(Mictlan, EPower::Deflection);
		BanishSeat(F, Mictlan);
		F.BeginTurn(Mictlan);
		F.Choose(Mictlan, EChoiceKind::Deflection, INDEX_NONE);
		Soulbound::Rules::SubmitSkillTrialResult(F.State, Mictlan, false, F.Now);
		EndTurn(F);

		const int32 From = F.State.EventLog.Num();
		F.BeginTurn(Mictlan);
		TestEqual(FString::Printf(TEXT("%s: en el turno de salida se le ofrece Desvío"), *Context), AsInt(F.State.PendingChoice.Kind), AsInt(EChoiceKind::Deflection));
		F.Choose(Mictlan, EChoiceKind::Deflection, bUse ? 1 : INDEX_NONE);
		TestEqual(FString::Printf(TEXT("%s: está en su hogar"), *Context), F.State.Seats[Mictlan].TileId, Soulbound::Rules::HomeTile(F.State, Mictlan));
		TestEqual(FString::Printf(TEXT("%s: por el motivo que toca"), *Context), LastOf(F, EMatchEventType::LeftVoid).Value,
			AsInt(bUse ? EVoidExit::Deflected : EVoidExit::TurnLimit));
		F.FinishTurn();
		TestEqual(FString::Printf(TEXT("%s: tira solo si usó Desvío"), *Context), CountSince(F, From, EMatchEventType::Rolled), bUse ? 1 : 0);
		TestEqual(FString::Printf(TEXT("%s: la runa vuelve a su Fuente solo si usó Desvío"), *Context), CountSince(F, From, EMatchEventType::RuneReturned), bUse ? 1 : 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidCountAndReleaseTest,
	"Soulbound.Core.Void.CuentaYLiberacion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidCountAndReleaseTest::RunTest(const FString& Parameters)
{
	// CA-VAC-4 (R-VAC-4, R-VAC-5, R-EFE-17): dado un asiento desterrado por segunda vez, su cuenta es uno. Dada una Liberación
	// con dos dentro, ambos están en su hogar en ese instante; sin nadie dentro, se rechaza.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	BanishSeat(F, Fenrir);
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Hard, false, TEXT("Primer destierro, turno 1"));
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Normal, false, TEXT("Primer destierro, turno 2"));
	TestEqual(TEXT("Tras dos turnos dentro, su cuenta es 2"), F.State.Seats[Fenrir].VoidTurns, 2);

	// Liberación con dos dentro: los dos a su hogar a la vez.
	F.State.Seats[EchoA].TileId = Normal(F, 0);
	BanishSeat(F, EchoA);
	F.Now += 1.0;
	const int32 From = F.State.EventLog.Num();
	TestEqual(TEXT("Liberación con dos dentro: se aplica"), AsInt(Soulbound::Rules::ReleaseVoid(F.State, F.Now)), AsInt(EEffectResult::Applied));
	TestEqual(TEXT("Fenrir está en su hogar"), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
	TestEqual(TEXT("El Eco está en su hogar"), F.State.Seats[EchoA].TileId, Soulbound::Rules::HomeTile(F.State, EchoA));
	TestEqual(TEXT("Dos salidas por Liberación"), CountSince(F, From, EMatchEventType::LeftVoid), 2);
	for (int32 Index = From; Index < F.State.EventLog.Num(); ++Index)
	{
		const FMatchEvent& Event = F.State.EventLog[Index];
		TestTrue(TEXT("Cada salida es por Liberación y en ese instante"), Event.Type != EMatchEventType::LeftVoid
			|| (Event.Value == AsInt(EVoidExit::Released) && Event.ClockSeconds == F.Now));
	}

	// Sin nadie dentro, se rechaza sin cambiar nada.
	const FMatchState Before = F.State;
	F.Now += 1.0;
	TestEqual(TEXT("Sin nadie dentro: se rechaza"), AsInt(Soulbound::Rules::ReleaseVoid(F.State, F.Now)), AsInt(EEffectResult::NobodyInVoid));
	TestTrue(TEXT("... sin cambiar nada"), SameState(Before, F.State));

	// Segundo destierro: la cuenta empieza de cero y su primer turno es el 1, en difícil.
	BanishSeat(F, Fenrir);
	TestEqual(TEXT("Desterrado de nuevo, su cuenta empieza en 0"), F.State.Seats[Fenrir].VoidTurns, 0);
	F.BeginTurn(Fenrir);
	TestEqual(TEXT("En su primer turno dentro, su cuenta es uno"), F.State.Seats[Fenrir].VoidTurns, 1);
	TestEqual(TEXT("... y la dificultad, la del primer peldaño"), AsInt(F.State.PendingDifficulty), AsInt(ESkillDifficulty::Hard));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidReleaseDuringWaitTest,
	"Soulbound.Core.Void.LiberacionDuranteLaEspera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidReleaseDuringWaitTest::RunTest(const FString& Parameters)
{
	// Decisión aceptada por el autor el 08/10/2026 (R-VAC-3, R-VAC-5): si llega la Liberación mientras el asiento espera en
	// su turno del Vacío (el escape o la elección de Desvío), la máquina cierra la espera y el turno termina sin tirar.
	for (const bool bWithDeflection : {false, true})
	{
		FChoiceFixture F;
		if (!StartAfterRace(*this, F))
		{
			return false;
		}
		const FString Context = bWithDeflection ? TEXT("Esperando Desvío") : TEXT("Esperando el escape");
		if (bWithDeflection)
		{
			F.GivePower(Fenrir, EPower::Deflection);
		}
		const TArray<int32> Runes = F.State.Seats[Fenrir].Runes;
		BanishSeat(F, Fenrir);
		const int32 From = F.State.EventLog.Num();
		F.BeginTurn(Fenrir);
		F.Now += 1.0;
		TestEqual(FString::Printf(TEXT("%s: la Liberación se aplica"), *Context), AsInt(Soulbound::Rules::ReleaseVoid(F.State, F.Now)), AsInt(EEffectResult::Applied));
		TestEqual(FString::Printf(TEXT("%s: está en su hogar"), *Context), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
		const EIntentResult Late = bWithDeflection ? F.Choose(Fenrir, EChoiceKind::Deflection, 1) : Soulbound::Rules::SubmitSkillTrialResult(F.State, Fenrir, true, F.Now);
		TestEqual(FString::Printf(TEXT("%s: la respuesta ya no contesta nada"), *Context), AsInt(Late), AsInt(EIntentResult::WrongPhase));
		TestTrue(FString::Printf(TEXT("%s: el turno termina"), *Context), EndTurn(F));
		TestEqual(FString::Printf(TEXT("%s: sin tirada"), *Context), CountSince(F, From, EMatchEventType::Rolled), 0);
		TestEqual(FString::Printf(TEXT("%s: una sola salida, por Liberación"), *Context), CountSince(F, From, EMatchEventType::LeftVoid), 1);
		TestEqual(FString::Printf(TEXT("%s: ... por Liberación"), *Context), LastOf(F, EMatchEventType::LeftVoid).Value, AsInt(EVoidExit::Released));
		TestTrue(FString::Printf(TEXT("%s: la runa no se toca"), *Context), F.State.Seats[Fenrir].Runes == Runes);
		TestFalse(FString::Printf(TEXT("%s: no queda nada pendiente"), *Context), F.State.PendingChoice.IsOpen() || F.State.PendingTrial != ESkillTrial::None);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBondRaceTest,
	"Soulbound.Core.Void.DestierroEnCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidBondRaceTest::RunTest(const FString& Parameters)
{
	// Criterio "Carrera" (R-CAR-7, R-EFE-14): dado un destierro durante la carrera, se rechaza salvo con el parámetro de Furia.
	FChoiceFixture F;
	if (!F.Start(*this) || !F.Bond(*this, Fenrir, FenrirDomain))
	{
		return false;
	}
	TestEqual(TEXT("La partida sigue en la carrera"), static_cast<int32>(F.State.MatchPhase), static_cast<int32>(EMatchPhase::BondRace));
	const FMatchState Before = F.State;
	TestEqual(TEXT("Sin la Furia: se rechaza"), AsInt(BanishSeat(F, EchoA)), AsInt(EEffectResult::BondRace));
	TestTrue(TEXT("... sin cambiar nada"), SameState(Before, F.State));
	TestEqual(TEXT("Con la Furia: se aplica"), AsInt(BanishSeat(F, EchoA, true)), AsInt(EEffectResult::Applied));
	TestTrue(TEXT("... y está en el Vacío"), F.State.Seats[EchoA].IsInVoid());
	TestEqual(TEXT("... con el destierro de la Furia en el registro"), LastOf(F, EMatchEventType::Banished).Value, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidSendHomeTest,
	"Soulbound.Core.Void.VueltaAlHogar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidSendHomeTest::RunTest(const FString& Parameters)
{
	// CA-EFE-3 (R-EFE-4, R-NIC-1, R-NIC-2): dado un asiento fuera de su hogar, con Vuelta al hogar está en él; en su hogar o
	// en el Vacío, no es objetivo válido. El siguiente turno se juega con normalidad.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	F.State.Seats[Mictlan].TileId = Normal(F, 0);
	TestEqual(TEXT("Fuera de su hogar: es objetivo válido"), AsInt(Soulbound::Rules::CanSendHome(F.State, Mictlan)), AsInt(EEffectResult::Applied));
	F.Now += 1.0;
	TestEqual(TEXT("La Vuelta al hogar se aplica"), AsInt(Soulbound::Rules::SendHome(F.State, Mictlan, F.Now)), AsInt(EEffectResult::Applied));
	const int32 Home = Soulbound::Rules::HomeTile(F.State, Mictlan);
	TestEqual(TEXT("Está en su hogar"), F.State.Seats[Mictlan].TileId, Home);
	TestEqual(TEXT("El registro deja la casilla del hogar"), LastOf(F, EMatchEventType::SentHome).Value, Home);
	TestEqual(TEXT("... con el asiento"), LastOf(F, EMatchEventType::SentHome).Seat, Mictlan);

	const FMatchState Before = F.State;
	TestEqual(TEXT("En su hogar: no es objetivo válido"), AsInt(Soulbound::Rules::SendHome(F.State, Mictlan, F.Now)), AsInt(EEffectResult::AlreadyHome));
	TestTrue(TEXT("... sin cambiar nada"), SameState(Before, F.State));

	F.State.Seats[EchoA].TileId = Normal(F, 0);
	BanishSeat(F, EchoA);
	TestEqual(TEXT("En el Vacío: no es objetivo válido"), AsInt(Soulbound::Rules::CanSendHome(F.State, EchoA)), AsInt(EEffectResult::InVoid));

	// El hogar no encierra: el siguiente turno se juega con normalidad.
	const int32 From = F.State.EventLog.Num();
	F.BeginTurn(Mictlan);
	F.Advance();
	TestEqual(TEXT("El siguiente turno tira"), CountSince(F, From, EMatchEventType::Rolled), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidInPlayTest,
	"Soulbound.Core.Void.EnJuegoNoEsObjetivo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidInPlayTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 08/10/2026: desterrar o mandar a casa a quien está en juego se rechaza con InPlay. En juego es el
	// asiento activo con el turno empezado (cualquier fase salvo BetweenTurns) y cualquier participante de un duelo o de un
	// trilero en curso. Entre turnos siempre vale.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	for (FSeatState& Seat : F.State.Seats)
	{
		Seat.TileId = Normal(F, 0);
	}

	F.BeginTurn(EchoA);
	TestTrue(TEXT("El turno del Eco ha empezado"), F.State.TurnPhase != ETurnPhase::BetweenTurns);
	TestTrue(TEXT("El activo está en juego"), Soulbound::Rules::IsSeatInPlay(F.State, EchoA));
	TestEqual(TEXT("Desterrar al activo: InPlay"), AsInt(Soulbound::Rules::CanBanish(F.State, EchoA, false)), AsInt(EEffectResult::InPlay));
	TestEqual(TEXT("Mandar a casa al activo: InPlay"), AsInt(Soulbound::Rules::CanSendHome(F.State, EchoA)), AsInt(EEffectResult::InPlay));
	TestEqual(TEXT("Otro asiento, durante ese turno: vale"), AsInt(Soulbound::Rules::CanBanish(F.State, EchoB, false)), AsInt(EEffectResult::Applied));

	// Participantes de un duelo o de un trilero en curso, aunque no tengan el turno.
	FChoiceFixture InDuel = F;
	InDuel.State.Duel.Challenger = EchoA;
	InDuel.State.Duel.Challenged = Mictlan;
	TestEqual(TEXT("El retado de un duelo en curso: InPlay"), AsInt(Soulbound::Rules::CanBanish(InDuel.State, Mictlan, false)), AsInt(EEffectResult::InPlay));
	FChoiceFixture InShell = F;
	InShell.State.ShellGame.Stage = EShellGameStage::Reveal;
	InShell.State.ShellGame.Thief = EchoB;
	InShell.State.ShellGame.Victim = Mictlan;
	TestEqual(TEXT("La víctima de un trilero en curso: InPlay"), AsInt(Soulbound::Rules::CanSendHome(InShell.State, Mictlan)), AsInt(EEffectResult::InPlay));
	TestEqual(TEXT("El ladrón de un trilero en curso: InPlay"), AsInt(Soulbound::Rules::CanBanish(InShell.State, EchoB, false)), AsInt(EEffectResult::InPlay));

	// Rechazado, no cambia nada.
	const FMatchState Before = F.State;
	TestEqual(TEXT("Banish sobre el activo se rechaza"), AsInt(BanishSeat(F, EchoA)), AsInt(EEffectResult::InPlay));
	TestTrue(TEXT("... sin cambiar nada"), SameState(Before, F.State));

	// Entre turnos, siempre vale.
	F.FinishTurn();
	TestFalse(TEXT("Entre turnos nadie está en juego"), Soulbound::Rules::IsSeatInPlay(F.State, F.State.ActiveSeat));
	TestEqual(TEXT("Entre turnos, desterrar al activo vale"), AsInt(BanishSeat(F, F.State.ActiveSeat)), AsInt(EEffectResult::Applied));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidLostTurnTest,
	"Soulbound.Core.Void.TurnoPerdidoNoCuenta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidLostTurnTest::RunTest(const FString& Parameters)
{
	// Decisión del autor del 08/10/2026 (R-VAC-2, R-TUR-10): un turno saltado por turno perdido no es un turno propio
	// empezado en el Vacío; la escalera avanza solo cuando el asiento empieza de verdad su turno dentro. Con turnos perdidos
	// la estancia puede pasar de cuatro turnos reales.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	BanishSeat(F, Fenrir);
	TestTrue(TEXT("Fenrir pierde un turno"), Soulbound::Rules::GrantLostTurns(F.State, Fenrir, 1));
	F.Now += 1.0;
	F.State.ActiveSeat = Fenrir;
	F.State.TurnPhase = ETurnPhase::BetweenTurns;
	F.Advance();
	TestEqual(TEXT("Su turno se salta"), LastOf(F, EMatchEventType::TurnSkipped).Seat, Fenrir);
	TestNotEqual(TEXT("Juega el siguiente"), F.State.ActiveSeat, Fenrir);
	TestEqual(TEXT("El turno saltado no cuenta"), F.State.Seats[Fenrir].VoidTurns, 0);
	F.FinishTurn();
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Hard, false, TEXT("Su primer turno de verdad"));
	TestEqual(TEXT("Su primer turno de verdad cuenta como el 1"), F.State.Seats[Fenrir].VoidTurns, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidEventLogTest,
	"Soulbound.Core.Void.Registro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidEventLogTest::RunTest(const FString& Parameters)
{
	// Criterio "Registro" (R-TUR-15): destierro, intento de escape (dificultad y resultado), salida (motivo) y Vuelta al
	// hogar, con marca de tiempo y asientos.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	BanishSeat(F, Fenrir);
	const FMatchEvent Banished = LastOf(F, EMatchEventType::Banished);
	TestTrue(TEXT("Banished: asiento, sin Furia, con marca de tiempo"), Banished.Seat == Fenrir && Banished.Value == 0 && Banished.ClockSeconds == F.Now);

	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Hard, false, TEXT("Escape fallado"));
	const FMatchEvent Failed = LastOf(F, EMatchEventType::VoidEscapeFailed);
	TestTrue(TEXT("VoidEscapeFailed: asiento y dificultad difícil"), Failed.Seat == Fenrir && Failed.Value == AsInt(ESkillDifficulty::Hard) && Failed.ClockSeconds == F.Now);

	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Normal, true, TEXT("Escape superado"));
	const FMatchEvent Passed = LastOf(F, EMatchEventType::VoidEscapePassed);
	TestTrue(TEXT("VoidEscapePassed: asiento y dificultad normal"), Passed.Seat == Fenrir && Passed.Value == AsInt(ESkillDifficulty::Normal) && Passed.ClockSeconds == F.Now);
	const FMatchEvent Left = LastOf(F, EMatchEventType::LeftVoid);
	TestTrue(TEXT("LeftVoid: asiento y motivo escape"), Left.Seat == Fenrir && Left.Value == AsInt(EVoidExit::Escaped) && Left.ClockSeconds == F.Now);
	const int32 PassedIndex = F.State.EventLog.IndexOfByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::VoidEscapePassed; });
	const int32 LeftIndex = F.State.EventLog.IndexOfByPredicate([](const FMatchEvent& Event) { return Event.Type == EMatchEventType::LeftVoid; });
	TestTrue(TEXT("El escape va antes de la salida"), PassedIndex != INDEX_NONE && PassedIndex < LeftIndex);

	F.State.Seats[EchoA].TileId = Normal(F, 0);
	F.Now += 1.0;
	Soulbound::Rules::SendHome(F.State, EchoA, F.Now);
	const FMatchEvent Sent = LastOf(F, EMatchEventType::SentHome);
	TestTrue(TEXT("SentHome: asiento, casilla del hogar y marca"), Sent.Seat == EchoA && Sent.Value == Soulbound::Rules::HomeTile(F.State, EchoA) && Sent.ClockSeconds == F.Now);
	TestEqual(TEXT("Un evento por suceso: un destierro"), EventsOfType(F.State, EMatchEventType::Banished).Num(), 1);
	TestEqual(TEXT("... una salida"), EventsOfType(F.State, EMatchEventType::LeftVoid).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidRandomStreamTest,
	"Soulbound.Core.Void.SorteoIntacto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVoidRandomStreamTest::RunTest(const FString& Parameters)
{
	// Criterio "Sorteo": desterrar, escapar y usar Desvío no consumen el sorteo de la partida. Tampoco Liberación ni Vuelta
	// al hogar. El turno jugado tras Desvío consume lo de cualquier turno, desde su tirada.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Seed = F.Random.GetCurrentSeed();
	BanishSeat(F, Fenrir);
	TestEqual(TEXT("Desterrar no consume el sorteo"), F.Random.GetCurrentSeed(), Seed);
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Hard, false, TEXT("Escape fallado"));
	PlayEscape(*this, F, Fenrir, ESkillDifficulty::Normal, true, TEXT("Escape superado"));
	TestEqual(TEXT("Escapar no consume el sorteo"), F.Random.GetCurrentSeed(), Seed);

	F.State.Seats[EchoA].TileId = Normal(F, 0);
	F.Now += 1.0;
	Soulbound::Rules::SendHome(F.State, EchoA, F.Now);
	BanishSeat(F, EchoA);
	F.Now += 1.0;
	Soulbound::Rules::ReleaseVoid(F.State, F.Now);
	TestEqual(TEXT("Vuelta al hogar y Liberación no consumen el sorteo"), F.Random.GetCurrentSeed(), Seed);

	F.GivePower(Fenrir, EPower::Deflection);
	BanishSeat(F, Fenrir);
	F.BeginTurn(Fenrir);
	F.Choose(Fenrir, EChoiceKind::Deflection, 1);
	TestFalse(TEXT("Desvío usado"), F.State.Seats[Fenrir].IsInVoid());
	TestEqual(TEXT("Usar Desvío no consume el sorteo"), F.Random.GetCurrentSeed(), Seed);
	F.Advance();
	TestNotEqual(TEXT("El turno jugado tras Desvío tira con el sorteo"), F.Random.GetCurrentSeed(), Seed);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
