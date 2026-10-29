// Tests del desplazamiento de plazos (S-023, glosario propuesto "ShiftOpenDeadlines"): la espera del Warden detiene la mesa y, al
// acabar, el anfitrión suma lo que ha durado a los plazos que esperan a alguien, sin tocar el reloj de partida ni los del Warden
// (CA-ESP-1). Sin tablero ni archivos: el estado se monta a mano. Filtro: "Soulbound.Core.Match.Turn.DesplazarPlazos".

#include "Match/MatchState.h"
#include "Match/TurnRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Un estado con los seis plazos abiertos, cada uno con su valor, y los relojes que no se tocan con el suyo. */
	FMatchState WithEveryDeadlineOpen()
	{
		FMatchState State;
		State.Seats.SetNum(Soulbound::Match::SeatCount);
		State.ClockSeconds = 100.0;
		State.TurnPhase = ETurnPhase::Moving;
		State.TurnDeadlineSeconds = 130.0;
		State.PendingChoice.Kind = EChoiceKind::DiceCount;
		State.PendingChoice.DeadlineSeconds = 131.0;
		State.ShellGame.Stage = EShellGameStage::Shuffle;
		State.ShellGame.StageDeadlineSeconds = 132.0;
		State.Movement.Path = {1, 2, 3};
		State.Movement.StepDeadlineSeconds = 133.0;
		State.Vote.bOpen = true;
		State.Vote.DeadlineSeconds = 134.0;
		State.Oracle.Stage = EOracleStage::Waiting;
		State.Oracle.DeadlineSeconds = 135.0;
		State.Warden.NextEvaluationSeconds = 140.0;
		State.Warden.LastEffectSeconds = 90.0;
		State.LastProgressSeconds = 80.0;
		State.Seats[1].LastTargetedSeconds = 70.0;
		return State;
	}

	/** Comprueba que los relojes que la espera no toca siguen como en WithEveryDeadlineOpen. */
	void TestUntouchedClocks(FAutomationTestBase& Test, const FMatchState& State, const TCHAR* Case)
	{
		Test.TestEqual(FString::Printf(TEXT("%s: el reloj de partida no cambia"), Case), State.ClockSeconds, 100.0);
		Test.TestEqual(FString::Printf(TEXT("%s: la evaluación siguiente no cambia"), Case), State.Warden.NextEvaluationSeconds, 140.0);
		Test.TestEqual(FString::Printf(TEXT("%s: la garantía no cambia"), Case), State.Warden.LastEffectSeconds, 90.0);
		Test.TestEqual(FString::Printf(TEXT("%s: la espera no es progreso"), Case), State.LastProgressSeconds, 80.0);
		Test.TestEqual(FString::Printf(TEXT("%s: ni ser objetivo"), Case), State.Seats[1].LastTargetedSeconds, 70.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShiftOpenDeadlinesTest, "Soulbound.Core.Match.Turn.DesplazarPlazos.Abiertos",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShiftOpenDeadlinesTest::RunTest(const FString& Parameters)
{
	// CA-ESP-1 (S-023, espera del Warden): con un turno en curso, una elección abierta, un trilero en etapa de reloj, un camino con
	// salvaguarda, una votación y una consulta, ShiftOpenDeadlines(State, 6) suma 6 a los seis plazos y devuelve 6; no cambia el reloj
	// de partida, NextEvaluationSeconds, LastEffectSeconds, LastProgressSeconds ni LastTargetedSeconds.
	FMatchState State = WithEveryDeadlineOpen();
	TestEqual(TEXT("Mueve los seis plazos"), Soulbound::Rules::ShiftOpenDeadlines(State, 6.0), 6);
	TestEqual(TEXT("El reloj de turno"), State.TurnDeadlineSeconds, 136.0);
	TestEqual(TEXT("La elección"), State.PendingChoice.DeadlineSeconds, 137.0);
	TestEqual(TEXT("El trilero"), State.ShellGame.StageDeadlineSeconds, 138.0);
	TestEqual(TEXT("La salvaguarda del movimiento"), State.Movement.StepDeadlineSeconds, 139.0);
	TestEqual(TEXT("La votación"), State.Vote.DeadlineSeconds, 140.0);
	TestEqual(TEXT("La consulta"), State.Oracle.DeadlineSeconds, 141.0);
	TestUntouchedClocks(*this, State, TEXT("Con plazos abiertos"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShiftClosedDeadlinesTest, "Soulbound.Core.Match.Turn.DesplazarPlazos.CerradosONoPositivo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShiftClosedDeadlinesTest::RunTest(const FString& Parameters)
{
	// CA-ESP-1 (S-023): con los plazos cerrados no toca nada y devuelve 0, aunque los campos guarden un valor de antes; con 0 o
	// negativo, nada, ni con los plazos abiertos. El trilero en una etapa sin reloj (la esfera, que va por la elección) tampoco cuenta.
	FMatchState Closed = WithEveryDeadlineOpen();
	Closed.TurnPhase = ETurnPhase::BetweenTurns;
	Closed.PendingChoice.Kind = EChoiceKind::None;
	Closed.ShellGame.Stage = EShellGameStage::Pick;
	Closed.Movement.Path.Reset();
	Closed.Vote.bOpen = false;
	Closed.Oracle.Stage = EOracleStage::Applying;
	TestEqual(TEXT("Cerrados: no mueve ninguno"), Soulbound::Rules::ShiftOpenDeadlines(Closed, 6.0), 0);
	TestEqual(TEXT("Cerrados: el reloj de turno no cambia"), Closed.TurnDeadlineSeconds, 130.0);
	TestEqual(TEXT("Cerrados: la elección no cambia"), Closed.PendingChoice.DeadlineSeconds, 131.0);
	TestEqual(TEXT("Cerrados: el trilero no cambia"), Closed.ShellGame.StageDeadlineSeconds, 132.0);
	TestEqual(TEXT("Cerrados: la salvaguarda no cambia"), Closed.Movement.StepDeadlineSeconds, 133.0);
	TestEqual(TEXT("Cerrados: la votación no cambia"), Closed.Vote.DeadlineSeconds, 134.0);
	TestEqual(TEXT("Cerrados: la consulta no cambia"), Closed.Oracle.DeadlineSeconds, 135.0);
	TestUntouchedClocks(*this, Closed, TEXT("Cerrados"));

	for (const double Seconds : {0.0, -6.0})
	{
		FMatchState Open = WithEveryDeadlineOpen();
		const FString Case = FString::Printf(TEXT("Con %.0f s"), Seconds);
		TestEqual(*(Case + TEXT(": no mueve ninguno")), Soulbound::Rules::ShiftOpenDeadlines(Open, Seconds), 0);
		TestEqual(*(Case + TEXT(": el reloj de turno no cambia")), Open.TurnDeadlineSeconds, 130.0);
		TestEqual(*(Case + TEXT(": la consulta no cambia")), Open.Oracle.DeadlineSeconds, 135.0);
		TestUntouchedClocks(*this, Open, *Case);
	}
	return true;
}

#endif
