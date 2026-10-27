// Tests de la victoria (S-018; GDD 04, la victoria; GDD 06, R-PRO-4 y R-WAR-4b): el aspirante y su anuncio, el desafío final en vez
// del turno, la victoria de un jugador, el fallo (una runa a su Fuente y la Vuelta al hogar), dos aspirantes, y el fin de la partida
// por tiempo, que corta todo lo que haya en marcha. Cada test cita el criterio (CA-XXX-n) y la regla (R-XXX-n) que cubre. Filtro:
// "Soulbound.Core.Victory".
//
// Montaje: WardenFixture.h (asiento 0 Fenrir, 1 Koschei, 2 y 3 Ecos; acabada la carrera, sin runas, en su hogar, entre turnos, con
// los eventos y el Warden de los archivos). Las runas y la casilla del aspirante se fijan a mano: es lo que el test necesita.

#include "Board/MovementRules.h"
#include "Match/BondRaceRules.h"
#include "Match/EffectRules.h"
#include "Match/VictoryRules.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "WardenFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using Soulbound::Tests::App;
	using Soulbound::Tests::FEventFixture;
	using Soulbound::Tests::FWardenFixture;
	using Soulbound::Tests::SameState;

	constexpr int32 Fenrir = FWardenFixture::Fenrir;
	constexpr int32 Koschei = FWardenFixture::Koschei;
	constexpr int32 EchoA = FWardenFixture::EchoA;

	template <typename TEnum>
	int32 AsInt(TEnum Value) { return static_cast<int32>(Value); }

	/** Brazal con una runa de cada tipo: los seis tipos distintos. */
	TArray<int32> AllSix()
	{
		TArray<int32> Runes;
		Runes.Init(1, Soulbound::Board::DomainCount);
		return Runes;
	}

	/** Seat pasa a estar en el Altar con los seis tipos distintos. */
	void MakeAspirant(FWardenFixture& F, int32 Seat)
	{
		F.State.Seats[Seat].Runes = AllSix();
		F.State.Seats[Seat].TileId = F.State.Board.FindAltar();
	}

	/** Lo que el fin por tiempo no debe tocar (R-VIC-4): los Brazales, las reservas de las Fuentes y dónde está cada ficha. */
	struct FHoldings
	{
		TArray<TArray<int32>> Bracers;
		TArray<int32> Reserves;
		TArray<int32> Tiles;

		explicit FHoldings(const FMatchState& State)
		{
			for (const FSeatState& Seat : State.Seats)
			{
				Bracers.Add(Seat.Runes);
				Tiles.Add(Seat.TileId);
			}
			for (const FTileState& Tile : State.Board.Tiles)
			{
				Reserves.Add(Tile.RuneReserve);
			}
		}

		bool operator==(const FHoldings& Other) const { return Bracers == Other.Bracers && Reserves == Other.Reserves && Tiles == Other.Tiles; }
	};

	/** Si después de un final ya no avanza nada: ni la máquina, ni el cerebro, ni una intención; el estado no cambia. */
	void CheckNothingAdvances(FAutomationTestBase& Test, FWardenFixture& F, const TCHAR* Context)
	{
		const FMatchState Over = F.State;
		F.Now += 60.0;
		Test.TestFalse(FString::Printf(TEXT("%s: AdvanceTurn no cambia nada"), Context), F.Advance());
		Test.TestEqual(FString::Printf(TEXT("%s: la intención de destino, WrongPhase"), Context),
			AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, F.State.ActiveSeat, F.State.Board.FindAltar(), F.Now)), AsInt(EIntentResult::WrongPhase));
		Test.TestEqual(FString::Printf(TEXT("%s: el resultado de un minijuego, WrongPhase"), Context),
			AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, F.State.ActiveSeat, true, F.Now)), AsInt(EIntentResult::WrongPhase));
		Test.TestEqual(FString::Printf(TEXT("%s: una elección, WrongPhase"), Context),
			AsInt(F.Choose(F.State.ActiveSeat, EChoiceKind::DiceCount, 1)), AsInt(EIntentResult::WrongPhase));
		Test.TestEqual(FString::Printf(TEXT("%s: un rendimiento de duelo, WrongPhase"), Context), AsInt(F.Perform(F.State.ActiveSeat, 0.5)), AsInt(EIntentResult::WrongPhase));
		Test.TestEqual(FString::Printf(TEXT("%s: un voto, WrongPhase"), Context), AsInt(Soulbound::Rules::SubmitVote(F.State, Koschei, Fenrir, F.Now)), AsInt(EIntentResult::WrongPhase));
		Test.TestEqual(FString::Printf(TEXT("%s: una consulta, WrongPhase"), Context),
			AsInt(Soulbound::Rules::SubmitOracleConsult(F.State, F.State.ActiveSeat, F.Now)), AsInt(EIntentResult::WrongPhase));
		Test.TestEqual(FString::Printf(TEXT("%s: un avance, WrongPhase"), Context),
			AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, F.State.ActiveSeat, F.State.Board.FindAltar(), F.Now)), AsInt(EIntentResult::WrongPhase));
		// El Warden, siempre dispuesto a intervenir: aun así, no evalúa.
		F.AlwaysIntervene(EWardenChannel::Strike);
		const FMatchState Prepared = F.State;
		Soulbound::Rules::EvaluateWarden(F.State, F.Now, F.Random);
		Test.TestTrue(FString::Printf(TEXT("%s: el cerebro no decide"), Context), SameState(Prepared, F.State));
		Test.TestEqual(FString::Printf(TEXT("%s: ningún turno empieza"), Context), F.Since(Over.EventLog.Num(), EMatchEventType::TurnStarted).Num(), 0);
		Test.TestEqual(FString::Printf(TEXT("%s: el registro no crece"), Context), F.State.EventLog.Num(), Over.EventLog.Num());
	}

	/**
	 * Lleva el reloj a matchEndSeconds y avanza la máquina una vez (R-VIC-4): la partida termina con victoria del Warden por tiempo, nada
	 * cambia de manos, nada queda en marcha y nada avanza después. CutSeat: el asiento cuyo turno se corta en CutPhase, o INDEX_NONE si
	 * el fin cae entre turnos.
	 */
	void EndByTimeAndCheck(FAutomationTestBase& Test, FWardenFixture& F, int32 CutSeat, ETurnPhase CutPhase, const TCHAR* Context)
	{
		const double End = F.State.WardenParams.MatchEndSeconds;
		const FHoldings Before(F.State);
		const int32 From = F.State.EventLog.Num();
		F.Now = End;
		Test.TestTrue(FString::Printf(TEXT("%s: AdvanceTurn cambia el estado"), Context), F.Advance());

		Test.TestTrue(FString::Printf(TEXT("%s: la partida termina"), Context), F.State.IsMatchOver());
		Test.TestEqual(FString::Printf(TEXT("%s: con victoria del Warden"), Context), AsInt(F.State.Outcome), AsInt(EMatchOutcome::WardenVictory));
		Test.TestEqual(FString::Printf(TEXT("%s: por tiempo"), Context), AsInt(F.State.EndReason), AsInt(EMatchEndReason::TimeOut));
		Test.TestEqual(FString::Printf(TEXT("%s: sin ganador"), Context), F.State.WinnerSeat, static_cast<int32>(INDEX_NONE));
		const TArray<FMatchEvent> Victory = F.Since(From, EMatchEventType::WardenVictory);
		if (Test.TestEqual(FString::Printf(TEXT("%s: un WardenVictory"), Context), Victory.Num(), 1))
		{
			Test.TestEqual(FString::Printf(TEXT("%s: con el motivo"), Context), Victory[0].Value, AsInt(EMatchEndReason::TimeOut));
			Test.TestEqual(FString::Printf(TEXT("%s: en el instante del fin"), Context), Victory[0].ClockSeconds, End);
			Test.TestEqual(FString::Printf(TEXT("%s: el último del registro"), Context), AsInt(F.State.EventLog.Last().Type), AsInt(EMatchEventType::WardenVictory));
		}

		const TArray<FMatchEvent> Cut = F.Since(From, EMatchEventType::TurnInterrupted);
		if (CutSeat == INDEX_NONE)
		{
			Test.TestEqual(FString::Printf(TEXT("%s: entre turnos, no se corta ningún turno"), Context), Cut.Num(), 0);
		}
		else if (Test.TestEqual(FString::Printf(TEXT("%s: un TurnInterrupted"), Context), Cut.Num(), 1))
		{
			Test.TestEqual(FString::Printf(TEXT("%s: del asiento en juego"), Context), Cut[0].Seat, CutSeat);
			Test.TestEqual(FString::Printf(TEXT("%s: con la fase"), Context), Cut[0].Value, AsInt(CutPhase));
			Test.TestEqual(FString::Printf(TEXT("%s: con el motivo fin de partida"), Context), Cut[0].Detail, Soulbound::Match::MatchEndCut);
			const TArray<FMatchEvent> Ended = F.Since(From, EMatchEventType::TurnEnded);
			Test.TestTrue(FString::Printf(TEXT("%s: y el turno termina"), Context), Ended.Num() == 1 && Ended[0].Seat == CutSeat);
		}

		Test.TestTrue(FString::Printf(TEXT("%s: ninguna runa cambia de manos y ninguna ficha se mueve"), Context), Before == FHoldings(F.State));
		for (const EMatchEventType Type : {EMatchEventType::RuneStolen, EMatchEventType::RuneObtained, EMatchEventType::DuelWon, EMatchEventType::OracleSentence,
			EMatchEventType::VoteResolved, EMatchEventType::MatchWon, EMatchEventType::FinalChallengeFailed, EMatchEventType::InsolencePunished, EMatchEventType::Moved})
		{
			Test.TestEqual(FString::Printf(TEXT("%s: nada de lo que estaba a medias se aplica (evento %d)"), Context, AsInt(Type)), F.Since(From, Type).Num(), 0);
		}
		Test.TestEqual(FString::Printf(TEXT("%s: entre turnos"), Context), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::BetweenTurns));
		Test.TestFalse(FString::Printf(TEXT("%s: sin elección"), Context), F.State.PendingChoice.IsOpen());
		Test.TestEqual(FString::Printf(TEXT("%s: sin prueba"), Context), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::None));
		Test.TestFalse(FString::Printf(TEXT("%s: sin duelo"), Context), F.State.Duel.IsActive());
		Test.TestFalse(FString::Printf(TEXT("%s: sin trilero"), Context), F.State.ShellGame.IsActive());
		Test.TestFalse(FString::Printf(TEXT("%s: sin consulta"), Context), F.State.Oracle.IsActive());
		Test.TestFalse(FString::Printf(TEXT("%s: sin votación"), Context), F.State.Vote.IsOpen());
		Test.TestFalse(FString::Printf(TEXT("%s: sin evento"), Context), F.State.CurrentEvent.IsActive());
		Test.TestFalse(FString::Printf(TEXT("%s: sin movimiento"), Context), F.State.Movement.IsActive());
		CheckNothingAdvances(Test, F, Context);
	}

	/** Partida con Warden para los tests de fin por tiempo, con el reloj veinte segundos antes del fin. */
	bool StartNearEnd(FAutomationTestBase& Test, FWardenFixture& F)
	{
		if (!F.StartWithWarden(Test))
		{
			return false;
		}
		F.Now = F.State.WardenParams.MatchEndSeconds - 20.0;
		return Test.TestTrue(TEXT("El fin de la partida está en los datos"), F.State.WardenParams.MatchEndSeconds > F.Now);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryAspirantChallengesTest, "Soulbound.Core.Victory.AspiranteDesafiaAlEmpezar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVictoryAspirantChallengesTest::RunTest(const FString& Parameters)
{
	// CA-VIC-1 (R-VIC-1, R-ECO-6): un asiento en el Altar con seis tipos al empezar su turno → fase SkillTrial con FinalChallenge, sin
	// tirada; con cinco tipos, con seis fuera del Altar o con seis en el Altar repetidos (cinco tipos y una doble) → turno normal; un Eco
	// en las mismas condiciones → desafío igual que un Elegido.
	struct FCase
	{
		const TCHAR* Name;
		int32 Seat;
		TArray<int32> Runes;
		bool bOnAltar;
		bool bChallenges;
	};
	const TArray<FCase> Cases = {
		{TEXT("Elegido con seis tipos en el Altar"), Fenrir, AllSix(), true, true},
		{TEXT("Eco con seis tipos en el Altar"), EchoA, AllSix(), true, true},
		{TEXT("Cinco tipos en el Altar"), Fenrir, {1, 1, 1, 1, 1, 0}, true, false},
		{TEXT("Seis tipos fuera del Altar"), Fenrir, AllSix(), false, false},
		{TEXT("Seis runas en el Altar, cinco tipos y una doble"), Fenrir, {2, 1, 1, 1, 1, 0}, true, false},
	};
	for (const FCase& Case : Cases)
	{
		FWardenFixture F;
		if (!F.StartWithWarden(*this))
		{
			return false;
		}
		F.State.Seats[Case.Seat].Runes = Case.Runes;
		if (Case.bOnAltar)
		{
			F.State.Seats[Case.Seat].TileId = F.State.Board.FindAltar();
		}
		TestEqual(FString::Printf(TEXT("%s: IsAspirant"), Case.Name), Soulbound::Rules::IsAspirant(F.State, Case.Seat), Case.bChallenges);
		const int32 From = F.State.EventLog.Num();
		const int32 DiceSeed = F.Random.GetCurrentSeed();
		F.BeginTurn(Case.Seat);
		if (Case.bChallenges)
		{
			TestEqual(FString::Printf(TEXT("%s: fase SkillTrial"), Case.Name), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::SkillTrial));
			TestEqual(FString::Printf(TEXT("%s: el desafío final"), Case.Name), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::FinalChallenge));
			TestEqual(FString::Printf(TEXT("%s: con la dificultad de los datos"), Case.Name), AsInt(F.State.PendingDifficulty), AsInt(F.State.Params.FinalChallengeDifficulty));
			const TArray<FMatchEvent> Started = F.Since(From, EMatchEventType::FinalChallengeStarted);
			TestTrue(FString::Printf(TEXT("%s: FinalChallengeStarted del asiento"), Case.Name), Started.Num() == 1 && Started[0].Seat == Case.Seat
				&& Started[0].Value == AsInt(F.State.Params.FinalChallengeDifficulty) && Started[0].ClockSeconds == F.Now);
			TestFalse(FString::Printf(TEXT("%s: espera el resultado"), Case.Name), F.Advance());
			TestEqual(FString::Printf(TEXT("%s: sin tirada"), Case.Name), F.Since(From, EMatchEventType::Rolled).Num(), 0);
			TestEqual(FString::Printf(TEXT("%s: sin tocar el sorteo de los dados"), Case.Name), F.Random.GetCurrentSeed(), DiceSeed);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s: turno normal, en PreRoll"), Case.Name), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::PreRoll));
			TestEqual(FString::Printf(TEXT("%s: sin prueba"), Case.Name), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::None));
			TestEqual(FString::Printf(TEXT("%s: sin desafío"), Case.Name), F.Since(From, EMatchEventType::FinalChallengeStarted).Num(), 0);
		}
	}

	// Decisión del autor del 27/10/2026: con la Prueba del Warden pendiente, primero el desafío; la Prueba sigue pendiente.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	MakeAspirant(F, Fenrir);
	F.State.Seats[Fenrir].bWardenTrialPending = true;
	F.BeginTurn(Fenrir);
	TestEqual(TEXT("Con la Prueba pendiente: el desafío final"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::FinalChallenge));
	TestTrue(TEXT("Con la Prueba pendiente: la Prueba sigue pendiente"), F.State.Seats[Fenrir].bWardenTrialPending);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryPassedTest, "Soulbound.Core.Victory.DesafioSuperadoGana",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVictoryPassedTest::RunTest(const FString& Parameters)
{
	// CA-VIC-2 (R-VIC-2): desafío superado → MatchOver, PlayerVictory con el asiento, MatchWon registrado; después, AdvanceTurn no
	// cambia nada, una intención devuelve WrongPhase, el cerebro no decide y no empieza ningún turno. Tampoco el fin por tiempo.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	MakeAspirant(F, Fenrir);
	F.BeginTurn(Fenrir);
	const int32 From = F.State.EventLog.Num();
	F.Now += 2.0;
	TestEqual(TEXT("El resultado se acepta"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, Fenrir, true, F.Now)), AsInt(EIntentResult::Accepted));
	TestTrue(TEXT("La partida termina"), F.State.IsMatchOver());
	TestEqual(TEXT("Con victoria de un jugador"), AsInt(F.State.Outcome), AsInt(EMatchOutcome::PlayerVictory));
	TestEqual(TEXT("El ganador es el aspirante"), F.State.WinnerSeat, Fenrir);
	const TArray<FMatchEvent> Won = F.Since(From, EMatchEventType::MatchWon);
	TestTrue(TEXT("MatchWon del asiento, en el instante"), Won.Num() == 1 && Won[0].Seat == Fenrir && Won[0].ClockSeconds == F.Now);
	TestEqual(TEXT("MatchWon es el último del registro"), AsInt(F.State.EventLog.Last().Type), AsInt(EMatchEventType::MatchWon));
	TestEqual(TEXT("Sin WardenVictory"), F.Since(From, EMatchEventType::WardenVictory).Num(), 0);
	TestEqual(TEXT("Se queda con sus runas"), F.State.Seats[Fenrir].Runes, AllSix());
	CheckNothingAdvances(*this, F, TEXT("Tras la victoria"));

	// Ni siquiera el reloj del fin: la partida ya terminó.
	const FMatchState Over = F.State;
	F.Now = F.State.WardenParams.MatchEndSeconds + 1.0;
	TestFalse(TEXT("Tras la victoria, el fin por tiempo no llega"), F.Advance());
	TestTrue(TEXT("Tras la victoria, el estado no cambia"), SameState(Over, F.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryFailedTest, "Soulbound.Core.Victory.DesafioFalladoCuestaUnaRuna",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVictoryFailedTest::RunTest(const FString& Parameters)
{
	// CA-VIC-3 (R-VIC-3, R-RUN-6): desafío fallado → una runa menos, elegida con el sorteo del Warden, la reserva de su Fuente sube uno,
	// el asiento está en su hogar, su turno terminó sin tirada y el consumo fijo del sorteo de dados no se tocó; vuelve a ser aspirante
	// solo si recupera las seis en el Altar.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	MakeAspirant(F, Fenrir);
	const int32 DiceSeed = F.Random.GetCurrentSeed();
	F.BeginTurn(Fenrir);
	if (!TestEqual(TEXT("Juega el desafío"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::FinalChallenge)))
	{
		return false;
	}
	// La runa que debe arrancar: la primera del sorteo del Warden, en un espejo del estado.
	FMatchState Mirror = F.State;
	FRandomStream MirrorDraw = Soulbound::Rules::WardenDraw(Mirror);
	const int32 Expected = Soulbound::Rules::RandomRuneType(Mirror, Fenrir, MirrorDraw);
	const int32 Source = F.State.Board.FindPowerSource(Expected);
	const int32 Reserve = F.State.Board.Tiles.IsValidIndex(Source) ? F.State.Board.Tiles[Source].RuneReserve : INDEX_NONE;
	const int32 WardenDraws = F.State.Warden.Draws;

	const int32 From = F.State.EventLog.Num();
	F.Now += 2.0;
	TestEqual(TEXT("El resultado se acepta"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, Fenrir, false, F.Now)), AsInt(EIntentResult::Accepted));
	TestFalse(TEXT("La partida sigue"), F.State.IsMatchOver());
	TestEqual(TEXT("Una runa menos"), F.RuneTotal(Fenrir), Soulbound::Board::DomainCount - 1);
	TestEqual(TEXT("La del sorteo del Warden"), F.State.Seats[Fenrir].RuneCountOf(Expected), 0);
	TestEqual(TEXT("Un sorteo del Warden"), F.State.Warden.Draws, WardenDraws + 1);
	if (TestTrue(TEXT("Su tipo tiene Fuente"), Source != INDEX_NONE))
	{
		TestEqual(TEXT("La reserva de su Fuente sube uno"), F.State.Board.Tiles[Source].RuneReserve, Reserve + 1);
	}
	TestEqual(TEXT("En su hogar"), F.State.Seats[Fenrir].TileId, Soulbound::Rules::HomeTile(F.State, Fenrir));
	TestEqual(TEXT("Sin prueba pendiente"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::None));
	TestEqual(TEXT("Su turno terminó"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::End));
	TestEqual(TEXT("Sin tirada"), F.Since(From, EMatchEventType::Rolled).Num(), 0);
	TestEqual(TEXT("El sorteo de los dados no se tocó"), F.Random.GetCurrentSeed(), DiceSeed);

	// El registro, en orden: FinalChallengeFailed con la runa, RuneReturned, SentHome y TurnEnded.
	TArray<EMatchEventType> Types;
	for (int32 Index = From; Index < F.State.EventLog.Num(); ++Index)
	{
		Types.Add(F.State.EventLog[Index].Type);
	}
	TestTrue(TEXT("Registro: fallo, runa devuelta, a casa y fin de turno"), Types == TArray<EMatchEventType>{EMatchEventType::FinalChallengeFailed,
		EMatchEventType::RuneReturned, EMatchEventType::SentHome, EMatchEventType::TurnEnded});
	const TArray<FMatchEvent> Failed = F.Since(From, EMatchEventType::FinalChallengeFailed);
	TestTrue(TEXT("FinalChallengeFailed con el asiento y la runa"), Failed.Num() == 1 && Failed[0].Seat == Fenrir && Failed[0].Value == Expected && Failed[0].ClockSeconds == F.Now);

	// Para volver a intentarlo tiene que volver a cumplir R-VIC-1.
	TestFalse(TEXT("Ya no es aspirante"), Soulbound::Rules::IsAspirant(F.State, Fenrir));
	F.State.Seats[Fenrir].TileId = F.State.Board.FindAltar();
	TestFalse(TEXT("En el Altar con cinco tipos, tampoco"), Soulbound::Rules::IsAspirant(F.State, Fenrir));
	F.State.Seats[Fenrir].Runes[Expected] = 1;
	TestTrue(TEXT("Con las seis en el Altar, otra vez"), Soulbound::Rules::IsAspirant(F.State, Fenrir));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryTwoAspirantsTest, "Soulbound.Core.Victory.DosAspirantes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVictoryTwoAspirantsTest::RunTest(const FString& Parameters)
{
	// CA-VIC-5 (R-VIC-1, R-VIC-2): dos asientos aspirantes: cada uno desafía en su turno por orden; si el primero falla y el segundo
	// supera, gana el segundo.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	const int32 First = Fenrir;
	const int32 Second = F.NextOf(First);
	MakeAspirant(F, First);
	MakeAspirant(F, Second);
	F.BeginTurn(First);
	TestTrue(TEXT("El primero desafía en su turno"), F.State.ActiveSeat == First && F.State.PendingTrial == ESkillTrial::FinalChallenge);
	F.Now += 2.0;
	TestEqual(TEXT("El primero falla"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, First, false, F.Now)), AsInt(EIntentResult::Accepted));
	for (int32 Step = 0; Step < 4 && F.State.TurnPhase != ETurnPhase::SkillTrial; ++Step)
	{
		F.Now += 1.0;
		F.Advance();
	}
	TestTrue(TEXT("El segundo desafía en el turno siguiente"), F.State.ActiveSeat == Second && F.State.PendingTrial == ESkillTrial::FinalChallenge);
	TestEqual(TEXT("El segundo supera"), AsInt(Soulbound::Rules::SubmitSkillTrialResult(F.State, Second, true, F.Now)), AsInt(EIntentResult::Accepted));
	TestEqual(TEXT("Gana el segundo"), F.State.WinnerSeat, Second);
	TestEqual(TEXT("Con victoria de un jugador"), AsInt(F.State.Outcome), AsInt(EMatchOutcome::PlayerVictory));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryAnnouncementTest, "Soulbound.Core.Victory.AnuncioDelAspirante",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVictoryAnnouncementTest::RunTest(const FString& Parameters)
{
	// CA-PRO-4 (R-PRO-4): al quedar en el Altar con seis tipos se registra AspirantAnnounced una vez; perder una runa y recuperarla lo
	// registra de nuevo; con cinco tipos no se registra. Lo mira la máquina de turnos en cada AdvanceTurn (decisión del autor del
	// 27/10/2026), aquí durante el turno de otro asiento.
	FWardenFixture F;
	if (!F.StartWithWarden(*this))
	{
		return false;
	}
	const int32 From = F.State.EventLog.Num();
	FSeatState& Seat = F.State.Seats[Fenrir];
	Seat.TileId = F.State.Board.FindAltar();
	Seat.Runes = {1, 1, 1, 1, 1, 0};
	// El Eco A espera su Prueba del Warden, sin plazo: así cada AdvanceTurn mira los aspirantes y el turno no avanza.
	F.State.Seats[EchoA].bWardenTrialPending = true;
	F.BeginTurn(EchoA);
	if (!TestEqual(TEXT("El Eco A juega su Prueba"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::WardenTrial)))
	{
		return false;
	}
	F.Advance();
	TestEqual(TEXT("Cinco tipos en el Altar: sin anuncio"), F.Since(From, EMatchEventType::AspirantAnnounced).Num(), 0);
	F.State.Seats[Fenrir].Runes = {2, 1, 1, 1, 1, 0};
	F.Now += 1.0;
	F.Advance();
	TestEqual(TEXT("Cinco tipos y una doble: sin anuncio"), F.Since(From, EMatchEventType::AspirantAnnounced).Num(), 0);

	F.State.Seats[Fenrir].Runes = AllSix();
	F.Now += 1.0;
	F.Advance();
	TArray<FMatchEvent> Announced = F.Since(From, EMatchEventType::AspirantAnnounced);
	TestTrue(TEXT("Seis tipos en el Altar: un anuncio del asiento, en el instante"), Announced.Num() == 1 && Announced[0].Seat == Fenrir && Announced[0].ClockSeconds == F.Now);
	F.Now += 1.0;
	F.Advance();
	F.Now += 1.0;
	F.Advance();
	TestEqual(TEXT("Sigue siéndolo: no se repite"), F.Since(From, EMatchEventType::AspirantAnnounced).Num(), 1);

	F.State.Seats[Fenrir].Runes[3] = 0;
	F.Now += 1.0;
	F.Advance();
	TestEqual(TEXT("Pierde una runa: sin anuncio"), F.Since(From, EMatchEventType::AspirantAnnounced).Num(), 1);
	F.State.Seats[Fenrir].Runes[3] = 1;
	F.Now += 1.0;
	F.Advance();
	TestEqual(TEXT("La recupera: se anuncia de nuevo"), F.Since(From, EMatchEventType::AspirantAnnounced).Num(), 2);

	F.State.Seats[Fenrir].TileId = Soulbound::Rules::HomeTile(F.State, Fenrir);
	F.Now += 1.0;
	F.Advance();
	F.State.Seats[Fenrir].TileId = F.State.Board.FindAltar();
	F.Now += 1.0;
	F.Advance();
	Announced = F.Since(From, EMatchEventType::AspirantAnnounced);
	TestEqual(TEXT("Lo mueven y vuelve: se anuncia de nuevo"), Announced.Num(), 3);
	TestTrue(TEXT("Todos del mismo asiento"), Announced.FilterByPredicate([](const FMatchEvent& Event) { return Event.Seat == Fenrir; }).Num() == Announced.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryTimeOutTest, "Soulbound.Core.Victory.FinPorTiempoCortaTodo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVictoryTimeOutTest::RunTest(const FString& Parameters)
{
	// CA-VIC-4 (R-VIC-4, R-WAR-4b, decisión 4 de la story): al llegar matchEndSeconds, con un turno en RollAndChoose, en Moving a medio
	// camino, en un duelo, en un trilero, en una consulta, en una votación y en un desafío final: la partida termina con WardenVictory
	// por tiempo, el turno queda cortado con su motivo, no cambia de manos ninguna runa ni se aplica nada de lo que estaba a medias, y
	// nada avanza después.
	{
		FWardenFixture F;
		if (!StartNearEnd(*this, F))
		{
			return false;
		}
		F.BeginTurn(Fenrir);
		F.Advance();
		if (TestEqual(TEXT("RollAndChoose: eligiendo destino"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::RollAndChoose)))
		{
			EndByTimeAndCheck(*this, F, Fenrir, ETurnPhase::RollAndChoose, TEXT("RollAndChoose"));
		}
	}
	{
		FWardenFixture F;
		if (!StartNearEnd(*this, F))
		{
			return false;
		}
		F.BeginTurn(Fenrir);
		F.State.ForcedRoll = 3;
		F.Advance();
		// Un destino con al menos dos casillas de camino: se entra en la primera y el fin llega a medio camino.
		const FMoverTraits Mover = Soulbound::Rules::MakeMoverTraits(F.State, Fenrir);
		TArray<int32> Path;
		int32 Destination = INDEX_NONE;
		for (const int32 Option : F.State.ReachableOptions)
		{
			Path = Soulbound::Rules::MovementPath(F.State.Board, F.State.Seats[Fenrir].TileId, F.State.Roll, Mover, Option);
			if (Path.Num() >= 2)
			{
				Destination = Option;
				break;
			}
		}
		if (TestTrue(TEXT("Moving: hay un destino de dos o más casillas"), Destination != INDEX_NONE)
			&& TestEqual(TEXT("Moving: destino aceptado"), AsInt(Soulbound::Rules::SubmitDestinationIntent(F.State, Fenrir, Destination, F.Now)), AsInt(EIntentResult::Accepted))
			&& TestEqual(TEXT("Moving: entra en la primera"), AsInt(Soulbound::Rules::SubmitMoveProgress(F.State, Fenrir, Path[0], F.Now)), AsInt(EIntentResult::Accepted)))
		{
			EndByTimeAndCheck(*this, F, Fenrir, ETurnPhase::Moving, TEXT("Moving"));
			TestEqual(TEXT("Moving: la ficha se queda en la casilla en que entró"), F.State.Seats[Fenrir].TileId, Path[0]);
		}
	}
	{
		FWardenFixture F;
		if (!StartNearEnd(*this, F))
		{
			return false;
		}
		const int32 Tile = F.NormalTiles()[0];
		F.State.Seats[EchoA].TileId = Tile;
		F.State.Seats[EchoA].Runes[0] = 1;
		F.GivePower(Fenrir, EPower::Duel);
		F.Land(Fenrir, Tile);
		if (TestEqual(TEXT("Duelo: Fenrir reta al Eco A"), AsInt(F.Act(Fenrir, EPlayerAction::Duel, EchoA)), AsInt(EIntentResult::Accepted))
			&& TestEqual(TEXT("Duelo: en curso"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::Duel)))
		{
			F.Perform(Fenrir, 1.0);
			EndByTimeAndCheck(*this, F, Fenrir, ETurnPhase::Duel, TEXT("Duelo"));
		}
	}
	{
		FWardenFixture F;
		if (!StartNearEnd(*this, F))
		{
			return false;
		}
		const int32 Tile = F.AwayTile(EchoA);
		F.State.Seats[Fenrir].TileId = Tile;
		F.State.Seats[Fenrir].Runes[0] = 1;
		F.Land(EchoA, Tile);
		if (TestEqual(TEXT("Trilero: el Eco A roba a Fenrir"), AsInt(F.Act(EchoA, EPlayerAction::Steal, Fenrir)), AsInt(EIntentResult::Accepted))
			&& TestEqual(TEXT("Trilero: en curso"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::ShellGame)))
		{
			EndByTimeAndCheck(*this, F, EchoA, ETurnPhase::ShellGame, TEXT("Trilero"));
		}
	}
	{
		FWardenFixture F;
		if (!StartNearEnd(*this, F))
		{
			return false;
		}
		// El tablero de 35 casillas no tiene Nodos del Oráculo en sus datos: el primer Nodo de Poder pasa a serlo.
		FTileState* Node = F.State.Board.Tiles.FindByPredicate([](const FTileState& T) { return T.Type == ETileType::PowerNode; });
		if (!TestNotNull(TEXT("Consulta: el tablero tiene un Nodo de Poder"), Node))
		{
			return false;
		}
		// Con el catálogo entero: en el Acto III hay sentencias posibles.
		Node->PowerNodeEffect = EPowerNodeEffect::Oracle;
		F.Land(Fenrir, Node->TileId);
		if (TestEqual(TEXT("Consulta: abierta en el Nodo"), AsInt(F.State.TurnPhase), AsInt(ETurnPhase::OracleConsult)))
		{
			EndByTimeAndCheck(*this, F, Fenrir, ETurnPhase::OracleConsult, TEXT("Consulta"));
		}
	}
	{
		FWardenFixture F;
		if (!StartNearEnd(*this, F))
		{
			return false;
		}
		const int32 Index = F.AddBagEvent(TEXT("voto-marca"), EAct::ActTwo, EWardenChannel::Scene, {{App(EEffect::Mark, EEffectTarget::MostVoted)}}, false);
		F.State.WardenEvents.Events[Index].bVote = true;
		if (TestEqual(TEXT("Votación: empieza"), AsInt(F.Launch(TEXT("voto-marca"))), AsInt(EWardenEventStart::Started))
			&& TestTrue(TEXT("Votación: abierta"), F.State.Vote.IsOpen()))
		{
			TestEqual(TEXT("Votación: un voto"), AsInt(Soulbound::Rules::SubmitVote(F.State, Fenrir, EchoA, F.Now)), AsInt(EIntentResult::Accepted));
			EndByTimeAndCheck(*this, F, INDEX_NONE, ETurnPhase::BetweenTurns, TEXT("Votación"));
			TestEqual(TEXT("Votación: nadie queda Marcado"), F.Since(0, EMatchEventType::Marked).Num(), 0);
		}
	}
	{
		FWardenFixture F;
		if (!StartNearEnd(*this, F))
		{
			return false;
		}
		MakeAspirant(F, Fenrir);
		F.BeginTurn(Fenrir);
		if (TestEqual(TEXT("Desafío final: en curso"), AsInt(F.State.PendingTrial), AsInt(ESkillTrial::FinalChallenge)))
		{
			EndByTimeAndCheck(*this, F, Fenrir, ETurnPhase::SkillTrial, TEXT("Desafío final"));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryNoWardenNoTimeOutTest, "Soulbound.Core.Victory.SinWardenNoHayFinPorTiempo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVictoryNoWardenNoTimeOutTest::RunTest(const FString& Parameters)
{
	// CA-VIC-4 (R-VIC-4): sin Warden en la partida (sin sus parámetros, como en los tests y en la simulación con el Warden apagado), el
	// reloj no termina nada: pasados los 40 minutos, los turnos siguen.
	FEventFixture F;
	if (!F.StartAfterRace(*this) || !TestFalse(TEXT("La partida no tiene Warden"), F.State.WardenParams.HasWarden()))
	{
		return false;
	}
	F.Now = 2400.0 + 100.0;
	TestFalse(TEXT("No hay fin debido"), Soulbound::Rules::IsMatchEndDue(F.State, F.Now));
	const int32 From = F.State.EventLog.Num();
	F.BeginTurn(Fenrir);
	F.Advance();
	TestFalse(TEXT("La partida sigue"), F.State.IsMatchOver());
	TestEqual(TEXT("Se tira"), F.Since(From, EMatchEventType::Rolled).Num(), 1);
	TestEqual(TEXT("Sin victoria del Warden"), F.Since(From, EMatchEventType::WardenVictory).Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
