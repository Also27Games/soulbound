// Tests del robo del Eco y del trilero (S-010, GDD 04 y GDD 03): el robo como acción contra un jugador (R-ECO-4, R-INT-1,
// R-INT-2), el trilero (R-ROB-1 a R-ROB-6), la afinidad de Koschei (R-ELE-2) como elección de un asiento sin turno, el
// poder perdido por robo (R-ELE-5), los turnos que no cambian (R-INT-4), el sorteo de los dados que no depende de los
// robos y el registro. Cada test cita el criterio de aceptación (CA-XXX-n) y la regla (R-XXX-n) que cubre. Se lanzan sin
// editor con Scripts/test.ps1. Filtro: "Soulbound.Core.ShellGame".
//
// Montaje: el de las elecciones (ChoiceFixture.h), tablero de 35 casillas. Los asientos 0 y 1 se vinculan y la carrera
// termina: los asientos 2 y 3 quedan Ecos con hogar. El Eco que roba es el 2. Las runas de los Brazales y las posiciones
// de las fichas se fijan a mano: es lo que el test necesita, no lo que comprueba. Los turnos se juegan con la máquina de
// turnos (AdvanceTurn y las intenciones).

#include "ChoiceFixture.h"
#include "Match/BondRaceRules.h"
#include "Match/RuneRules.h"
#include "Match/ShellGameRules.h"
#include "Match/TurnRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::EventsOfType;
	using Soulbound::Tests::FChoiceFixture;

	/** Dominio de Koschei en Content/Data/Elects/elegidos.json: afinidad safeRune (R-ELE-2). */
	constexpr int32 KoscheiDomain = 5;

	/** El Eco que roba en todos los tests. */
	constexpr int32 Thief = 2;

	int32 AsInt(EIntentResult Result) { return static_cast<int32>(Result); }
	int32 AsInt(EChoiceKind Kind) { return static_cast<int32>(Kind); }
	int32 AsInt(ETurnPhase Phase) { return static_cast<int32>(Phase); }
	int32 AsInt(EShellGameStage Stage) { return static_cast<int32>(Stage); }

	/**
	 * Partida tras la carrera: el asiento 0 se vincula (a Koschei si bKoscheiVictim, si no al primer Dominio libre), el 1 al
	 * primero libre que quede, y la carrera termina. El 2 y el 3 quedan Ecos con hogar.
	 */
	bool StartAfterRace(FAutomationTestBase& Test, FChoiceFixture& F, bool bKoscheiVictim = false)
	{
		if (!F.Start(Test))
		{
			return false;
		}
		const bool bBonded = bKoscheiVictim ? F.Bond(Test, 0, KoscheiDomain) : F.BondAny(Test, 0) != INDEX_NONE;
		if (!bBonded || F.BondAny(Test, 1) == INDEX_NONE || !Test.TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now, F.Random)))
		{
			return false;
		}
		const FSeatState& Echo = F.State.Seats[Thief];
		return Test.TestTrue(TEXT("El asiento 2 queda Eco con hogar"), Echo.Role == ESeatRole::LostEcho && Echo.DomainId != INDEX_NONE);
	}

	/** Una casilla normal del Dominio hogar del Eco ladrón: donde puede terminar su movimiento tras la carrera (R-TAB-20). */
	int32 EchoTile(const FChoiceFixture& F, int32 Index = 0)
	{
		const TArray<int32> Tiles = F.NormalTiles(F.State.Seats[Thief].DomainId);
		return Tiles.IsValidIndex(Index) ? Tiles[Index] : INDEX_NONE;
	}

	/** Pone en el Brazal de Seat las runas Counts (una entrada por tipo, de 0 a 5). */
	void SetRunes(FChoiceFixture& F, int32 Seat, const TArray<int32>& Counts)
	{
		F.State.Seats[Seat].Runes = Counts;
	}

	/** Runas en juego: las de todos los Brazales más las reservas de todas las Fuentes. */
	int32 RunesInPlay(const FMatchState& State)
	{
		int32 Total = 0;
		for (const FSeatState& Seat : State.Seats)
		{
			for (const int32 Count : Seat.Runes)
			{
				Total += Count;
			}
		}
		for (const FTileState& Tile : State.Board.Tiles)
		{
			Total += Tile.RuneReserve;
		}
		return Total;
	}

	/** Reservas de las Fuentes, casilla a casilla. */
	TArray<int32> Reserves(const FMatchState& State)
	{
		TArray<int32> Result;
		for (const FTileState& Tile : State.Board.Tiles)
		{
			Result.Add(Tile.RuneReserve);
		}
		return Result;
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

	/** El Eco ladrón termina en la casilla de Victim y elige robarle. Devuelve lo que dijo el motor. */
	EIntentResult LandAndSteal(FAutomationTestBase& Test, FChoiceFixture& F, int32 Victim, int32 TileIndex = 0)
	{
		const int32 Tile = EchoTile(F, TileIndex);
		F.State.Seats[Victim].TileId = Tile;
		if (!Test.TestTrue(FString::Printf(TEXT("El Eco termina donde está el asiento %d"), Victim), F.Land(Thief, Tile)))
		{
			return EIntentResult::WrongPhase;
		}
		F.Now += 1.0;
		return F.Act(Thief, EPlayerAction::Steal, Victim);
	}

	/** Deja pasar la muestra y la mezcla (R-ROB-2) hasta la elección de esfera. Devuelve si se llegó a ella. */
	bool PlayToPick(FChoiceFixture& F)
	{
		for (int32 Step = 0; Step < 2; ++Step)
		{
			F.Now = FMath::Max(F.Now, F.State.ShellGame.StageDeadlineSeconds);
			F.Advance();
		}
		return F.State.ShellGame.Stage == EShellGameStage::Pick && F.State.PendingChoice.Kind == EChoiceKind::ShellPick;
	}

	/** Lista "a, b, c" de enteros, para los mensajes. */
	FString Join(const TArray<int32>& Values)
	{
		return FString::JoinBy(Values, TEXT(", "), [](int32 Value) { return FString::FromInt(Value); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameEchoStealTest,
	"Soulbound.Core.ShellGame.RoboDelEcoTrasLaCarrera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameEchoStealTest::RunTest(const FString& Parameters)
{
	// CA-ECO-3 (R-ECO-4, R-INT-2): dado un Eco tras la carrera que termina en una casilla con uno o más jugadores, se le
	// ofrece robar a uno o a ninguno; si elige a uno, empieza el trilero. Un Elegido (el 0) y un Eco (el 3) en la casilla.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	SetRunes(F, 0, {1, 0, 0, 0, 0, 0});
	SetRunes(F, 3, {0, 0, 1, 0, 0, 0});
	const int32 Tile = EchoTile(F);
	F.State.Seats[0].TileId = Tile;
	F.State.Seats[3].TileId = Tile;
	TestTrue(TEXT("El Eco termina con dos jugadores"), F.Land(Thief, Tile));
	const FPendingChoice& Choice = F.State.PendingChoice;
	TestEqual(TEXT("Se le ofrece la acción contra un jugador"), AsInt(Choice.Kind), AsInt(EChoiceKind::PlayerAction));
	TestEqual(TEXT("... a él"), Choice.Seat, Thief);
	TestTrue(TEXT("... robar al Elegido o al otro Eco"), Choice.ActionOptions == TArray<FPlayerActionOption>{{EPlayerAction::Steal, 0}, {EPlayerAction::Steal, 3}});
	TestTrue(TEXT("... o a ninguno"), Choice.CanDecline());
	TestEqual(TEXT("... con el reloj de elección"), Choice.DeadlineSeconds, F.Now + F.State.Params.ChoiceClockSeconds);
	const FChoiceFixture Waiting = F;

	// A ninguno: el turno termina y nada cambia de mano.
	TestEqual(TEXT("Ninguno: aceptado"), AsInt(F.Choose(Thief, EChoiceKind::PlayerAction, INDEX_NONE)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Ninguno: el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Ninguno: no hay trilero"), EventsOfType(F.State, EMatchEventType::ShellGameStarted).Num(), 0);
	TestTrue(TEXT("Ninguno: los Brazales no cambian"), Bracers(F.State) == Bracers(Waiting.State));

	// A uno: empieza el trilero contra ese.
	F = Waiting;
	F.Now += 1.0;
	TestEqual(TEXT("Robar al Elegido: aceptado"), AsInt(F.Act(Thief, EPlayerAction::Steal, 0)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("La máquina pasa al trilero"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame));
	TestEqual(TEXT("... en la muestra"), AsInt(F.State.ShellGame.Stage), AsInt(EShellGameStage::Reveal));
	TestTrue(TEXT("... con el Eco de ladrón y el Elegido de víctima"), F.State.ShellGame.Thief == Thief && F.State.ShellGame.Victim == 0);
	const TArray<FMatchEvent> Started = EventsOfType(F.State, EMatchEventType::ShellGameStarted);
	TestTrue(TEXT("El registro lleva el inicio del trilero"), Started.Num() == 1 && Started[0].Seat == Thief && Started[0].OtherSeat == 0);
	TestFalse(TEXT("La máquina espera mientras dura"), F.Advance());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameNoStealOfferTest,
	"Soulbound.Core.ShellGame.SinRoboEnCarreraNiParaElegidos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameNoStealOfferTest::RunTest(const FString& Parameters)
{
	// CA-ECO-3 (R-ECO-4): durante la carrera, o a un Elegido, no se le ofrece el robo. Y solo al terminar su movimiento
	// (decisión del autor del 05/10/2026, como Expulsión, salto y Fuente): sin moverse, no hay robo.
	FChoiceFixture F;
	if (!F.Start(*this) || F.BondAny(*this, 0) == INDEX_NONE)
	{
		return false;
	}
	SetRunes(F, 0, {1, 0, 0, 0, 0, 0});
	SetRunes(F, 3, {0, 1, 0, 0, 0, 0});
	const TArray<int32> Normal = F.NormalTiles();

	// Durante la carrera: el Eco 2 termina con el Elegido 0 y el Eco 3, y no se le ofrece nada.
	F.State.Seats[0].TileId = Normal[0];
	F.State.Seats[3].TileId = Normal[0];
	TestTrue(TEXT("En carrera, el Eco termina con dos jugadores"), F.Land(Thief, Normal[0]));
	TestFalse(TEXT("En carrera, no hay elección"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("En carrera, el turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
	TestTrue(TEXT("En carrera, StealTargets está vacío"), Soulbound::Rules::StealTargets(F.State, Thief).IsEmpty());

	if (F.BondAny(*this, 1) == INDEX_NONE || !TestTrue(TEXT("La carrera termina"), Soulbound::Rules::EndBondRace(F.State, F.Now, F.Random)))
	{
		return false;
	}

	// Un Elegido sin poderes que termina con un Eco: nada. Con Expulsión: solo expulsar, nunca robar.
	SetRunes(F, 0, {0, 0, 0, 0, 0, 0});
	const int32 Tile = EchoTile(F);
	F.State.Seats[3].TileId = Tile;
	TestTrue(TEXT("El Elegido termina con un Eco"), F.Land(0, Tile));
	TestFalse(TEXT("Al Elegido no se le ofrece robar"), F.State.PendingChoice.IsOpen());
	F.FinishTurn();
	F.GivePower(0, EPower::Expel);
	F.State.Seats[0].TileId = F.State.Board.FindAltar();
	TestTrue(TEXT("El Elegido con Expulsión termina con el Eco"), F.Land(0, Tile));
	const bool bOnlyExpel = F.State.PendingChoice.ActionOptions.Num() > 0
		&& !F.State.PendingChoice.ActionOptions.ContainsByPredicate([](const FPlayerActionOption& Option) { return Option.Action == EPlayerAction::Steal; });
	TestTrue(TEXT("... y solo se le ofrece expulsar"), bOnlyExpel);
	F.FinishTurn();

	// Un Eco tras la carrera que no se mueve (sin casillas alcanzables, R-TAB-18) en una casilla ocupada: no hay robo.
	F.State.Seats[Thief].TileId = Tile;
	F.State.Seats[3].TileId = Tile;
	F.Now += 1.0;
	F.State.ActiveSeat = Thief;
	F.State.TurnPhase = ETurnPhase::RollAndChoose;
	F.State.TurnDeadlineSeconds = F.Now + F.State.Params.TurnClockSeconds;
	F.State.Roll = 1;
	F.State.ReachableOptions.Reset();
	F.State.bActiveSeatMoved = false;
	for (int32 Step = 0; Step < 4 && F.State.TurnPhase != ETurnPhase::BetweenTurns && !F.State.PendingChoice.IsOpen(); ++Step)
	{
		F.Advance();
	}
	TestFalse(TEXT("Sin moverse, no se le ofrece robar"), F.State.PendingChoice.IsOpen());

	// Control: el mismo Eco, moviéndose a la casilla ocupada, sí recibe la oferta. Sin él, los "no se ofrece" de arriba
	// pasarían con un motor que nunca ofrece nada.
	F.FinishTurn();
	F.State.Seats[0].TileId = F.State.Board.FindAltar();
	F.State.Seats[Thief].TileId = EchoTile(F, 1);
	TestTrue(TEXT("Control: el Eco termina con el otro Eco"), F.Land(Thief, Tile));
	TestTrue(TEXT("Control: se le ofrece robarle"), F.State.PendingChoice.ActionOptions == TArray<FPlayerActionOption>{{EPlayerAction::Steal, 3}});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameSourceThenStealTest,
	"Soulbound.Core.ShellGame.FuenteAntesQueRobo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameSourceThenStealTest::RunTest(const FString& Parameters)
{
	// CA-INT-1 (R-INT-1): dado un Eco que termina en la Fuente de un Olvidado ocupada, primero obtiene la runa y después
	// se le ofrece el robo.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	const int32 Home = F.State.Seats[Thief].DomainId;
	const int32 Source = F.SourceOf(Home);
	if (!TestTrue(TEXT("El hogar del Eco es un Olvidado con Fuente y reserva"), Source != INDEX_NONE
		&& F.State.Board.Domains[Home].State == EDomainState::Forgotten && F.State.Board.Tiles[Source].RuneReserve > 0))
	{
		return false;
	}
	F.State.Seats[0].TileId = Source;
	const int32 Before = F.State.Seats[Thief].RuneCountOf(Home);
	TestTrue(TEXT("El Eco termina en la Fuente ocupada"), F.Land(Thief, Source));
	TestEqual(TEXT("Ya ha obtenido la runa"), F.State.Seats[Thief].RuneCountOf(Home), Before + 1);
	TestTrue(TEXT("... y después se le ofrece robar"), F.State.PendingChoice.Kind == EChoiceKind::PlayerAction
		&& F.State.PendingChoice.ActionOptions == TArray<FPlayerActionOption>{{EPlayerAction::Steal, 0}});
	const TArray<FMatchEvent>& Log = F.State.EventLog;
	TestTrue(TEXT("El último evento es la runa obtenida"), Log.Num() > 0 && Log.Last().Type == EMatchEventType::RuneObtained);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameOneRuneChangesHandsTest,
	"Soulbound.Core.ShellGame.TrileroPasaUnaRuna",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameOneRuneChangesHandsTest::RunTest(const FString& Parameters)
{
	// CA-ROB-1 (R-ROB-2, R-ROB-3): dada una víctima con R runas (con duplicados), cuando se resuelve el trilero, hay R
	// esferas, el ladrón tiene una runa más, la víctima una menos, y el total de runas en juego y las reservas no cambian.
	// Si el ladrón ya tenía ese tipo, lo tiene duplicado: tiene de todos los tipos de la víctima, salga la que salga.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	SetRunes(F, 0, {0, 2, 0, 1, 0, 1});
	SetRunes(F, Thief, {0, 1, 0, 1, 0, 1});
	const int32 TotalBefore = RunesInPlay(F.State);
	const TArray<int32> ReservesBefore = Reserves(F.State);

	TestEqual(TEXT("Robar a la víctima: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("R esferas: cuatro, duplicados incluidos"), F.State.ShellGame.Orbs.Num(), 4);
	TestTrue(FString::Printf(TEXT("... con las runas de la víctima en orden de tipo (%s)"), *Join(F.State.ShellGame.Orbs)), F.State.ShellGame.Orbs == TArray<int32>{1, 1, 3, 5});
	TestEqual(TEXT("La muestra dura lo que dicen los datos"), F.State.ShellGame.StageDeadlineSeconds, F.Now + F.State.Params.ShellRevealSeconds);

	// Muestra y mezcla, con sus plazos: antes de cada uno la máquina espera.
	const double RevealEnd = F.State.ShellGame.StageDeadlineSeconds;
	TestFalse(TEXT("Antes del fin de la muestra, espera"), Soulbound::Rules::AdvanceTurn(F.State, RevealEnd - 0.01, F.Random));
	F.Now = RevealEnd;
	TestTrue(TEXT("Al fin de la muestra, avanza"), F.Advance());
	TestEqual(TEXT("... a la mezcla"), AsInt(F.State.ShellGame.Stage), AsInt(EShellGameStage::Shuffle));
	TestEqual(TEXT("La mezcla dura lo que dicen los datos"), F.State.ShellGame.StageDeadlineSeconds, RevealEnd + F.State.Params.ShellShuffleSeconds);
	const double ShuffleEnd = F.State.ShellGame.StageDeadlineSeconds;
	TestFalse(TEXT("Antes del fin de la mezcla, espera"), Soulbound::Rules::AdvanceTurn(F.State, ShuffleEnd - 0.01, F.Random));
	F.Now = ShuffleEnd;
	TestTrue(TEXT("Al fin de la mezcla, avanza"), F.Advance());
	const FPendingChoice& Pick = F.State.PendingChoice;
	TestEqual(TEXT("El ladrón elige esfera"), AsInt(Pick.Kind), AsInt(EChoiceKind::ShellPick));
	TestEqual(TEXT("... él"), Pick.Seat, Thief);
	TestTrue(TEXT("... entre las cuatro posiciones"), Pick.Options == TArray<int32>{0, 1, 2, 3});
	TestFalse(TEXT("... sin \"no\""), Pick.CanDecline());
	TestEqual(TEXT("... con el tiempo de elección del trilero"), Pick.DeadlineSeconds, ShuffleEnd + F.State.Params.ShellPickSeconds);
	TestTrue(TEXT("Mientras dura, las runas siguen en el Brazal de la víctima"), F.State.Seats[0].Runes == TArray<int32>{0, 2, 0, 1, 0, 1});

	const int32 Expected = Soulbound::Rules::ShellRuneAtPosition(F.State, 2);
	TestTrue(TEXT("La esfera de la posición 2 tiene una runa de la víctima"), Expected == 1 || Expected == 3 || Expected == 5);
	F.Now += 1.0;
	TestEqual(TEXT("Elige la esfera 2: aceptado"), AsInt(F.Choose(Thief, EChoiceKind::ShellPick, 2)), AsInt(EIntentResult::Accepted));
	if (Expected >= 0)
	{
		TestEqual(TEXT("El ladrón tiene esa runa duplicada"), F.State.Seats[Thief].RuneCountOf(Expected), 2);
		TestEqual(TEXT("La víctima tiene una menos de ese tipo"), F.State.Seats[0].RuneCountOf(Expected), (Expected == 1 ? 2 : 1) - 1);
	}
	int32 ThiefTotal = 0;
	int32 VictimTotal = 0;
	for (int32 Type = 0; Type < Soulbound::Board::DomainCount; ++Type)
	{
		ThiefTotal += F.State.Seats[Thief].RuneCountOf(Type);
		VictimTotal += F.State.Seats[0].RuneCountOf(Type);
	}
	TestEqual(TEXT("El ladrón, una runa más"), ThiefTotal, 4);
	TestEqual(TEXT("La víctima, una menos"), VictimTotal, 3);
	TestEqual(TEXT("El total de runas en juego no cambia"), RunesInPlay(F.State), TotalBefore);
	TestTrue(TEXT("Las reservas no cambian"), Reserves(F.State) == ReservesBefore);
	TestFalse(TEXT("El trilero termina"), F.State.ShellGame.IsActive());
	TestEqual(TEXT("... y el turno"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameNoPickTest,
	"Soulbound.Core.ShellGame.LadronQueNoElige",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameNoPickTest::RunTest(const FString& Parameters)
{
	// CA-ROB-2 (R-ROB-3): dado un ladrón que no elige en su tiempo, ninguna runa cambia de mano y el registro lleva el evento.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	SetRunes(F, 0, {1, 1, 0, 0, 0, 0});
	TestEqual(TEXT("Robar a la víctima: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	if (!TestTrue(TEXT("Llega a la elección de esfera"), PlayToPick(F)))
	{
		return false;
	}
	const TArray<TArray<int32>> Before = Bracers(F.State);
	const double Deadline = F.State.PendingChoice.DeadlineSeconds;
	TestFalse(TEXT("Antes del plazo, espera"), Soulbound::Rules::AdvanceTurn(F.State, Deadline - 0.01, F.Random));
	TestEqual(TEXT("Una respuesta en el plazo se rechaza"), AsInt(Soulbound::Rules::SubmitChoiceIntent(F.State, Thief, EChoiceKind::ShellPick, 0, Deadline, F.Random)),
		AsInt(EIntentResult::ClockExpired));
	TestTrue(TEXT("En el plazo, la máquina avanza"), Soulbound::Rules::AdvanceTurn(F.State, Deadline, F.Random));
	TestTrue(TEXT("Ninguna runa cambia de mano"), Bracers(F.State) == Before);
	TestEqual(TEXT("Sin runa robada"), EventsOfType(F.State, EMatchEventType::RuneStolen).Num(), 0);
	const TArray<FMatchEvent> Expired = EventsOfType(F.State, EMatchEventType::ShellGameExpired);
	TestTrue(TEXT("El registro lleva al ladrón que no elige, con la víctima y el plazo"), Expired.Num() == 1 && Expired[0].Seat == Thief
		&& Expired[0].OtherSeat == 0 && Expired[0].ClockSeconds == Deadline);
	TestFalse(TEXT("El trilero termina"), F.State.ShellGame.IsActive());
	TestEqual(TEXT("... y el turno"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Sin castigo: la insolencia no cambia"), F.State.Seats[Thief].Insolence, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameShuffleSeedTest,
	"Soulbound.Core.ShellGame.MezclaPorSemilla",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameShuffleSeedTest::RunTest(const FString& Parameters)
{
	// CA-ROB-3 (R-ROB-6): dada una semilla y un número de esferas, la secuencia de intercambios y la posición final de cada
	// esfera son idénticas en dos generaciones; con otra semilla, distintas. La runa que recibe el ladrón es la de la esfera
	// que eligió según esa posición final.
	constexpr int32 Orbs = 6;
	constexpr int32 Swaps = 8;
	const TArray<FIntPoint> A = Soulbound::Rules::ShellGameSwaps(12345, Orbs, Swaps);
	const TArray<FIntPoint> B = Soulbound::Rules::ShellGameSwaps(12345, Orbs, Swaps);
	const TArray<FIntPoint> Other = Soulbound::Rules::ShellGameSwaps(54321, Orbs, Swaps);
	TestEqual(TEXT("Tantos intercambios como dicen los datos"), A.Num(), Swaps);
	TestTrue(TEXT("Misma semilla, mismos intercambios"), A == B);
	TestTrue(TEXT("Otra semilla, otros intercambios"), A != Other);
	for (const FIntPoint& Swap : A)
	{
		TestTrue(FString::Printf(TEXT("Intercambio (%d, %d): dos posiciones distintas y válidas, la menor primero"), Swap.X, Swap.Y),
			Swap.X >= 0 && Swap.X < Swap.Y && Swap.Y < Orbs);
	}

	// Posiciones finales: las mismas en dos generaciones, distintas con otra semilla, y las que salen de aplicar los
	// intercambios uno a uno (oráculo propio del test).
	const TArray<int32> Final = Soulbound::Rules::ShellGameFinalPositions(12345, Orbs, Swaps);
	TestTrue(TEXT("Misma semilla, mismas posiciones finales"), Final == Soulbound::Rules::ShellGameFinalPositions(12345, Orbs, Swaps));
	TestTrue(TEXT("Otra semilla, otras posiciones finales"), Final != Soulbound::Rules::ShellGameFinalPositions(54321, Orbs, Swaps));
	TArray<int32> AtPosition;
	for (int32 Orb = 0; Orb < Orbs; ++Orb)
	{
		AtPosition.Add(Orb);
	}
	for (const FIntPoint& Swap : A)
	{
		AtPosition.Swap(Swap.X, Swap.Y);
	}
	TArray<int32> Expected;
	Expected.SetNum(Orbs);
	for (int32 Position = 0; Position < Orbs; ++Position)
	{
		Expected[AtPosition[Position]] = Position;
	}
	TestTrue(FString::Printf(TEXT("Las posiciones finales son las de los intercambios (%s frente a %s)"), *Join(Final), *Join(Expected)), Final == Expected);
	TestTrue(TEXT("Con una esfera, no hay intercambios"), Soulbound::Rules::ShellGameSwaps(7, 1, Swaps).IsEmpty());
	TestTrue(TEXT("... y la esfera se queda en su sitio"), Soulbound::Rules::ShellGameFinalPositions(7, 1, Swaps) == TArray<int32>{0});

	// En partida: cuatro runas distintas para identificarlas; para cada posición, el ladrón recibe la de la esfera que acaba
	// allí según la semilla del trilero y los intercambios de los datos.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	SetRunes(F, 0, {1, 0, 1, 1, 0, 1});
	SetRunes(F, Thief, {0, 0, 0, 0, 0, 0});
	TestEqual(TEXT("Robar a la víctima: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("La semilla del primer trilero sale del valor base de la partida"), F.State.ShellGame.Seed, Soulbound::Rules::ShellGameSeed(F.State.ShellSeedBase, 0));
	if (!TestTrue(TEXT("Llega a la elección de esfera"), PlayToPick(F)))
	{
		return false;
	}
	const TArray<int32> Runes = F.State.ShellGame.Orbs;
	const TArray<int32> Positions = Soulbound::Rules::ShellGameFinalPositions(F.State.ShellGame.Seed, Runes.Num(), F.State.Params.ShellSwapCount);
	const FChoiceFixture AtPick = F;
	for (int32 Position = 0; Position < Runes.Num(); ++Position)
	{
		F = AtPick;
		const int32 Orb = Positions.IndexOfByKey(Position);
		const int32 Rune = Runes.IsValidIndex(Orb) ? Runes[Orb] : INDEX_NONE;
		F.Now += 1.0;
		F.Choose(Thief, EChoiceKind::ShellPick, Position);
		TestTrue(FString::Printf(TEXT("Posición %d: el ladrón recibe la runa %d, la de la esfera %d"), Position, Rune, Orb),
			Rune != INDEX_NONE && F.State.Seats[Thief].RuneCountOf(Rune) == 1 && F.State.Seats[0].RuneCountOf(Rune) == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameEmptyVictimTest,
	"Soulbound.Core.ShellGame.VictimaSinRunas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameEmptyVictimTest::RunTest(const FString& Parameters)
{
	// CA-ROB-4 (R-ROB-4): dado un robo a una víctima sin runas, no se abre trilero, nada cambia de mano y el registro lleva
	// el evento. La víctima sin runas sí es un objetivo: el ladrón no ve el Brazal ajeno.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	SetRunes(F, 0, {0, 0, 0, 0, 0, 0});
	SetRunes(F, Thief, {0, 0, 1, 0, 0, 0});
	const TArray<TArray<int32>> Before = Bracers(F.State);
	TestEqual(TEXT("Robar a quien no tiene runas: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("No se abre trilero"), F.State.ShellGame.IsActive());
	TestEqual(TEXT("... ni se registra su inicio"), EventsOfType(F.State, EMatchEventType::ShellGameStarted).Num(), 0);
	TestEqual(TEXT("El turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestTrue(TEXT("Nada cambia de mano"), Bracers(F.State) == Before);
	const TArray<FMatchEvent> Empty = EventsOfType(F.State, EMatchEventType::StealNoRunes);
	TestTrue(TEXT("El registro lleva el robo a quien no tiene runas"), Empty.Num() == 1 && Empty[0].Seat == Thief && Empty[0].OtherSeat == 0 && Empty[0].ClockSeconds == F.Now);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameKoscheiTest,
	"Soulbound.Core.ShellGame.KoscheiApartaUnaRuna",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameKoscheiTest::RunTest(const FString& Parameters)
{
	// CA-ELE-7 (R-ROB-5, R-ELE-2): dado Koschei víctima con R runas, se le pide apartar una (elección de un asiento sin
	// turno); el trilero se juega con R menos una y la apartada sigue en su Brazal. Con R igual a uno, el robo se anula. Si
	// no elige a tiempo, se juega con las R.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F, true))
	{
		return false;
	}
	const FChoiceFixture Base = F;

	// R = 3: se le pide a Koschei (asiento 0), no al Eco que tiene el turno.
	SetRunes(F, 0, {0, 2, 0, 0, 1, 0});
	TestEqual(TEXT("Robar a Koschei: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("La máquina pasa al trilero"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame));
	TestEqual(TEXT("... esperando la runa a salvo"), AsInt(F.State.ShellGame.Stage), AsInt(EShellGameStage::SafeRune));
	const FPendingChoice& Safe = F.State.PendingChoice;
	TestEqual(TEXT("Se pide apartar una runa"), AsInt(Safe.Kind), AsInt(EChoiceKind::SafeRune));
	TestEqual(TEXT("... a Koschei, que no tiene el turno"), Safe.Seat, 0);
	TestEqual(TEXT("... mientras el turno sigue siendo del Eco"), F.State.ActiveSeat, Thief);
	TestTrue(TEXT("... entre los tipos de su Brazal"), Safe.Options == TArray<int32>{1, 4});
	TestFalse(TEXT("... sin \"no\""), Safe.CanDecline());
	TestEqual(TEXT("... con el reloj de elección"), Safe.DeadlineSeconds, F.Now + F.State.Params.ChoiceClockSeconds);
	const FChoiceFixture Asked = F;
	F.Now += 1.0;
	TestEqual(TEXT("El asiento activo no puede contestar por Koschei"), AsInt(F.Choose(Thief, EChoiceKind::SafeRune, 1)), AsInt(EIntentResult::WrongSeat));
	TestEqual(TEXT("Koschei no puede no apartar"), AsInt(F.Choose(0, EChoiceKind::SafeRune, INDEX_NONE)), AsInt(EIntentResult::InvalidOption));
	TestEqual(TEXT("... ni apartar un tipo que no tiene"), AsInt(F.Choose(0, EChoiceKind::SafeRune, 3)), AsInt(EIntentResult::InvalidOption));
	TestEqual(TEXT("Koschei aparta la runa 4: aceptado"), AsInt(F.Choose(0, EChoiceKind::SafeRune, 4)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Empieza la muestra"), AsInt(F.State.ShellGame.Stage), AsInt(EShellGameStage::Reveal));
	TestTrue(FString::Printf(TEXT("Se juega con R menos una (%s)"), *Join(F.State.ShellGame.Orbs)), F.State.ShellGame.Orbs == TArray<int32>{1, 1});
	TestEqual(TEXT("La apartada queda anotada"), F.State.ShellGame.SetAsideRune, 4);
	TestEqual(TEXT("... y sigue en su Brazal"), F.State.Seats[0].RuneCountOf(4), 1);
	F.FinishTurn();
	TestEqual(TEXT("Tras el trilero, la apartada sigue en su Brazal"), F.State.Seats[0].RuneCountOf(4), 1);
	TestEqual(TEXT("... y se ha robado una de las otras"), F.State.Seats[0].RuneCountOf(1), 1);

	// Sin elegir a tiempo: se juega con las R.
	F = Asked;
	const double Deadline = F.State.PendingChoice.DeadlineSeconds;
	TestFalse(TEXT("Antes del plazo, espera a Koschei"), Soulbound::Rules::AdvanceTurn(F.State, Deadline - 0.01, F.Random));
	F.Now = Deadline;
	TestTrue(TEXT("En el plazo, la máquina avanza"), F.Advance());
	TestEqual(TEXT("Sin elegir: empieza la muestra"), AsInt(F.State.ShellGame.Stage), AsInt(EShellGameStage::Reveal));
	TestTrue(FString::Printf(TEXT("... con las R (%s)"), *Join(F.State.ShellGame.Orbs)), F.State.ShellGame.Orbs == TArray<int32>{1, 1, 4});
	TestEqual(TEXT("... sin runa apartada"), F.State.ShellGame.SetAsideRune, INDEX_NONE);
	const TArray<FMatchEvent> Expired = EventsOfType(F.State, EMatchEventType::ChoiceExpired);
	TestTrue(TEXT("El registro lleva la elección agotada de Koschei"), Expired.Num() == 1 && Expired[0].Seat == 0 && Expired[0].Value == AsInt(EChoiceKind::SafeRune));

	// R = 1: el robo se anula.
	F = Base;
	SetRunes(F, 0, {0, 0, 0, 1, 0, 0});
	const TArray<TArray<int32>> Before = Bracers(F.State);
	TestEqual(TEXT("Robar a Koschei con una runa: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("No hay trilero"), F.State.ShellGame.IsActive());
	TestFalse(TEXT("... ni se le pide apartar"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("El turno termina"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestTrue(TEXT("Nada cambia de mano"), Bracers(F.State) == Before);
	const TArray<FMatchEvent> Cancelled = EventsOfType(F.State, EMatchEventType::StealCancelled);
	TestTrue(TEXT("El registro lleva el robo anulado"), Cancelled.Num() == 1 && Cancelled[0].Seat == Thief && Cancelled[0].OtherSeat == 0 && Cancelled[0].Value == 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGamePowerLostTest,
	"Soulbound.Core.ShellGame.PoderPerdidoAlRobar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGamePowerLostTest::RunTest(const FString& Parameters)
{
	// Criterio "Poder perdido" (R-ELE-5): dada una víctima que pierde por robo su última runa de un tipo, el poder deja de
	// estar activo en ese instante; si era Celeridad, su siguiente turno es de un dado sin elección. Una sola runa: una
	// sola esfera, así que se roba la de Celeridad.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	SetRunes(F, 0, {0, 0, 0, 0, 0, 0});
	F.GivePower(0, EPower::Haste);
	TestTrue(TEXT("La víctima tiene Celeridad"), Soulbound::Rules::IsPowerActive(F.State, 0, EPower::Haste));
	TestEqual(TEXT("Robar a la víctima: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	if (!TestTrue(TEXT("Llega a la elección de esfera"), PlayToPick(F)))
	{
		return false;
	}
	TestTrue(TEXT("Hasta la elección, la víctima conserva Celeridad"), Soulbound::Rules::IsPowerActive(F.State, 0, EPower::Haste));
	F.Now += 1.0;
	F.Choose(Thief, EChoiceKind::ShellPick, 0);
	TestFalse(TEXT("En el instante del robo, pierde Celeridad"), Soulbound::Rules::IsPowerActive(F.State, 0, EPower::Haste));
	TestTrue(TEXT("... y no le queda ningún poder"), Soulbound::Rules::GetActivePowers(F.State, 0).IsEmpty());
	F.FinishTurn();
	F.BeginTurn(0);
	TestEqual(TEXT("En su siguiente turno, la máquina no espera elección de dados"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::PreRoll));
	TestFalse(TEXT("... no hay elección"), F.State.PendingChoice.IsOpen());
	TestEqual(TEXT("... y tira un dado"), F.State.DiceCount, Soulbound::Dice::DefaultCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameTurnsUnchangedTest,
	"Soulbound.Core.ShellGame.RoboSinTocarTurnos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameTurnsUnchangedTest::RunTest(const FString& Parameters)
{
	// CA-INT-3 (R-INT-4): tras un robo, los turnos extra y perdidos de la víctima no cambian (ni los del ladrón).
	FChoiceFixture F;
	if (!StartAfterRace(*this, F))
	{
		return false;
	}
	SetRunes(F, 0, {1, 0, 1, 0, 0, 0});
	F.State.Seats[0].ExtraTurns = 1;
	F.State.Seats[0].LostTurns = 2;
	TestEqual(TEXT("Robar a la víctima: aceptado"), AsInt(LandAndSteal(*this, F, 0)), AsInt(EIntentResult::Accepted));
	if (!TestTrue(TEXT("Llega a la elección de esfera"), PlayToPick(F)))
	{
		return false;
	}
	F.Now += 1.0;
	F.Choose(Thief, EChoiceKind::ShellPick, 1);
	TestEqual(TEXT("Hay robo"), EventsOfType(F.State, EMatchEventType::RuneStolen).Num(), 1);
	TestEqual(TEXT("La víctima conserva su turno extra"), F.State.Seats[0].ExtraTurns, 1);
	TestEqual(TEXT("... y sus turnos perdidos"), F.State.Seats[0].LostTurns, 2);
	TestEqual(TEXT("El ladrón no gana ni pierde turnos"), F.State.Seats[Thief].ExtraTurns + F.State.Seats[Thief].LostTurns, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameFixedDrawsTest,
	"Soulbound.Core.ShellGame.SorteoNoDependeDeLosRobos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameFixedDrawsTest::RunTest(const FString& Parameters)
{
	// Criterio "El sorteo de los dados no depende de los robos" (R-ROB-6, consumo fijo de S-009): dadas dos partidas con la
	// misma semilla, una con robos y otra sin ellos, los valores del sorteo que consume cada turno son los mismos. Las dos
	// copias juegan los mismos turnos con las mismas fichas (antes de que el Eco confirme destino, el asiento 0 se pone en
	// esa casilla y con runas); solo cambia que una roba siempre que puede y la otra nunca. Tras cada turno, el sorteo está
	// en el mismo punto. El trilero dura tiempo de partida: en la copia que roba el reloj va por delante, y no importa.
	FChoiceFixture Base;
	if (!StartAfterRace(*this, Base))
	{
		return false;
	}
	Base.State.TurnPhase = ETurnPhase::BetweenTurns;
	constexpr int32 Turns = 16;
	TArray<TArray<int32>> Seeds;
	TArray<FChoiceFixture> Played;
	for (const bool bSteal : {true, false})
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
					const int32 Steal = Choice.ActionOptions.IndexOfByPredicate([](const FPlayerActionOption& Option) { return Option.Action == EPlayerAction::Steal; });
					F.Choose(Choice.Seat, Choice.Kind, bSteal ? Steal : INDEX_NONE);
				}
				else if (Choice.IsOpen())
				{
					F.Choose(Choice.Seat, Choice.Kind, Choice.CanDecline() ? INDEX_NONE : Choice.AnswerAt(0));
				}
				else if (F.State.TurnPhase == ETurnPhase::RollAndChoose && !F.State.ReachableOptions.IsEmpty())
				{
					const int32 Destination = F.State.ReachableOptions[0];
					if (F.State.ActiveSeat == Thief)
					{
						F.State.Seats[0].TileId = Destination;
						F.State.Seats[0].Runes[0] = FMath::Max(F.State.Seats[0].Runes[0], 2);
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

	TestTrue(TEXT("La copia que roba robó alguna runa"), EventsOfType(Played[0].State, EMatchEventType::RuneStolen).Num() > 0);
	TestEqual(TEXT("La otra no robó ninguna"), EventsOfType(Played[1].State, EMatchEventType::RuneStolen).Num(), 0);
	TestEqual(TEXT("Las dos jugaron los mismos turnos"), Played[0].State.TurnCount, Played[1].State.TurnCount);
	for (int32 Turn = 0; Turn < Turns; ++Turn)
	{
		if (Seeds[0][Turn] != Seeds[1][Turn])
		{
			AddError(FString::Printf(TEXT("Tras el turno %d el sorteo está en otro punto con robos que sin ellos"), Turn + 1));
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShellGameEventLogTest,
	"Soulbound.Core.ShellGame.RegistroDelTrilero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShellGameEventLogTest::RunTest(const FString& Parameters)
{
	// Criterio "Registro" (R-TUR-15): los eventos nuevos, con asientos, dato y marca de tiempo: inicio del trilero, runa
	// apartada, runa robada, ladrón que no elige, robo a quien no tiene runas y robo anulado. Seat es quien actúa, OtherSeat
	// el afectado y Value el dato; nunca un asiento en Value.
	FChoiceFixture F;
	if (!StartAfterRace(*this, F, true))
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
	const int32 Koschei = 0;
	const int32 Other = 1;

	// Robo a quien no tiene runas.
	SetRunes(F, Other, {0, 0, 0, 0, 0, 0});
	LandAndSteal(*this, F, Other);
	Expect(EMatchEventType::StealNoRunes, 0, TEXT("StealNoRunes"), Thief, Other, INDEX_NONE, F.Now);
	F.FinishTurn();

	// Robo anulado: Koschei con una sola runa, la 2.
	SetRunes(F, Koschei, {0, 0, 1, 0, 0, 0});
	LandAndSteal(*this, F, Koschei, 1);
	Expect(EMatchEventType::StealCancelled, 0, TEXT("StealCancelled"), Thief, Koschei, 2, F.Now);
	F.FinishTurn();

	// Runa apartada e inicio del trilero, en el instante en que Koschei elige; y el ladrón que no elige, en su plazo.
	SetRunes(F, Koschei, {1, 0, 1, 0, 0, 0});
	LandAndSteal(*this, F, Koschei);
	F.Now += 1.0;
	F.Choose(Koschei, EChoiceKind::SafeRune, 2);
	Expect(EMatchEventType::SafeRuneSetAside, 0, TEXT("SafeRuneSetAside"), Koschei, Thief, 2, F.Now);
	Expect(EMatchEventType::ShellGameStarted, 0, TEXT("ShellGameStarted (Koschei)"), Thief, Koschei, 1, F.Now);
	PlayToPick(F);
	const double Deadline = F.State.PendingChoice.DeadlineSeconds;
	F.Now = Deadline;
	F.Advance();
	Expect(EMatchEventType::ShellGameExpired, 0, TEXT("ShellGameExpired"), Thief, Koschei, INDEX_NONE, Deadline);
	F.FinishTurn();

	// Inicio de un trilero sin Koschei y runa robada.
	SetRunes(F, Other, {0, 0, 0, 3, 0, 0});
	LandAndSteal(*this, F, Other, 1);
	Expect(EMatchEventType::ShellGameStarted, 1, TEXT("ShellGameStarted"), Thief, Other, 3, F.Now);
	PlayToPick(F);
	F.Now += 1.0;
	F.Choose(Thief, EChoiceKind::ShellPick, 0);
	Expect(EMatchEventType::RuneStolen, 0, TEXT("RuneStolen"), Thief, Other, 3, F.Now);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
