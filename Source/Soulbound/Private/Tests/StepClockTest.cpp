// Tests del reloj del modo paso a paso (S-007, glosario "StepMode"): cuándo se detiene la partida, qué marca el reloj
// mientras espera y qué suelta un paso. Sin mundo ni editor. Cada test cita el criterio manual de la story que respalda
// ("Paso a paso"). Filtro: "Soulbound.Game.Match.PasoAPaso".

#include "../../Match/StepClock.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Criterio "Paso a paso: desactivado, nada cambia respecto a hoy": el reloj es el del mundo menos el origen y nunca detiene.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepClockOffTest, "Soulbound.Game.Match.PasoAPaso.DesactivadoNoCambiaNada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FStepClockOffTest::RunTest(const FString& Parameters)
{
	FStepClock Clock;
	Clock.OriginWorldSeconds = 10.0;
	TestEqual(TEXT("Con origen 10, el segundo 25 del mundo es el 15 de partida"), Clock.MatchSeconds(25.0), 15.0);
	TestFalse(TEXT("Con el modo desactivado nunca se detiene, entre turnos"), Clock.ShouldHold(false, ETurnPhase::BetweenTurns, EMatchPhase::BondRace));
	TestFalse(TEXT("Con el modo desactivado, un paso no hace nada"), Clock.Step(30.0));
	TestEqual(TEXT("El reloj sigue siendo el del mundo"), Clock.MatchSeconds(40.0), 30.0);
	return true;
}

// Criterio "la partida se detiene al acabar cada turno": solo entre turnos, y no si ya espera ni con la partida terminada.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepClockHoldTest, "Soulbound.Game.Match.PasoAPaso.SeDetieneEntreTurnos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FStepClockHoldTest::RunTest(const FString& Parameters)
{
	FStepClock Clock;
	TestTrue(TEXT("Activo y entre turnos: se detiene"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::BondRace));
	for (const ETurnPhase Phase : {ETurnPhase::Start, ETurnPhase::PreRoll, ETurnPhase::RollAndChoose, ETurnPhase::MoveAndResolve, ETurnPhase::End})
	{
		TestFalse(FString::Printf(TEXT("En mitad del turno (fase %d) no se detiene"), static_cast<int32>(Phase)), Clock.ShouldHold(true, Phase, EMatchPhase::BondRace));
	}
	TestFalse(TEXT("Con la partida terminada no se detiene"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::MatchOver));
	TestTrue(TEXT("Tras la carrera, en la caza de runas, también"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::RuneHunt));

	Clock.Hold(12.0);
	TestFalse(TEXT("Ya detenida: no vuelve a detenerse"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::BondRace));
	return true;
}

// Criterio "el reloj de carrera no avanza mientras espera": congelado en el segundo en que se detuvo, y sin salto al reanudar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepClockFreezeTest, "Soulbound.Game.Match.PasoAPaso.ElRelojNoAvanzaEsperando",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FStepClockFreezeTest::RunTest(const FString& Parameters)
{
	FStepClock Clock;
	Clock.OriginWorldSeconds = 5.0;
	Clock.Hold(20.0);
	TestTrue(TEXT("Espera"), Clock.bHeld);
	TestEqual(TEXT("Congelado en el segundo 15 de partida (20 - 5)"), Clock.MatchSeconds(20.0), 15.0);
	TestEqual(TEXT("Cien segundos después sigue en el 15"), Clock.MatchSeconds(120.0), 15.0);

	TestTrue(TEXT("El paso suelta"), Clock.Step(120.0));
	TestFalse(TEXT("Ya no espera"), Clock.bHeld);
	TestEqual(TEXT("Reanuda en el 15, no salta a 115"), Clock.MatchSeconds(120.0), 15.0);
	TestEqual(TEXT("Y corre desde ahí: 7 s después, el 22"), Clock.MatchSeconds(127.0), 22.0);
	return true;
}

// Criterio "con la tecla o SbStep se juega exactamente un turno": un paso concede un turno; al salir de BetweenTurns
// el permiso se retira y el siguiente BetweenTurns vuelve a detener. Pulsar sin esperar no acumula pasos.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepClockOneTurnTest, "Soulbound.Game.Match.PasoAPaso.UnPasoEsUnTurno",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FStepClockOneTurnTest::RunTest(const FString& Parameters)
{
	FStepClock Clock;
	Clock.Hold(3.0);
	TestTrue(TEXT("Primer paso: suelta"), Clock.Step(50.0));
	TestFalse(TEXT("Con el paso concedido y aún entre turnos, no vuelve a detenerse"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::BondRace));
	TestFalse(TEXT("Un segundo paso antes de que el turno arranque no suma otro turno"), Clock.Step(51.0));

	Clock.NoteTurnPhase(ETurnPhase::Start);
	TestFalse(TEXT("Salió de BetweenTurns: el permiso se retira"), Clock.bStepGranted);
	TestFalse(TEXT("Pulsar el paso en mitad del turno no hace nada"), Clock.Step(60.0));
	TestFalse(TEXT("Y en mitad del turno no se detiene"), Clock.ShouldHold(true, ETurnPhase::RollAndChoose, EMatchPhase::BondRace));

	TestTrue(TEXT("El turno terminó: entre turnos vuelve a detenerse"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::BondRace));
	return true;
}

// Criterio "cambiable en caliente": desactivar a mitad de espera reanuda el reloj sin saltos y no deja pasos pendientes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepClockDisableTest, "Soulbound.Game.Match.PasoAPaso.DesactivarReanuda",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FStepClockDisableTest::RunTest(const FString& Parameters)
{
	FStepClock Clock;
	Clock.Hold(30.0);
	Clock.Disable(90.0);
	TestFalse(TEXT("Ya no espera"), Clock.bHeld);
	TestFalse(TEXT("Sin paso concedido"), Clock.bStepGranted);
	TestEqual(TEXT("El reloj sigue en el 30, sin contar la espera"), Clock.MatchSeconds(90.0), 30.0);
	TestEqual(TEXT("Y corre"), Clock.MatchSeconds(100.0), 40.0);

	FStepClock Running;
	Running.OriginWorldSeconds = 2.0;
	Running.Disable(10.0);
	TestEqual(TEXT("Un reloj en marcha no cambia al desactivar"), Running.MatchSeconds(10.0), 8.0);
	return true;
}

// S-014, criterio "Entre turnos" y prueba manual del dilema en el paso a paso: con un evento del Warden en curso la partida
// no se detiene, para que corra el reloj de sus elecciones; lanzado con la partida detenida, el reloj se reanuda sin conceder
// turno, y al acabar el evento la partida vuelve a detenerse antes del siguiente turno.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepClockWardenEventTest, "Soulbound.Game.Match.PasoAPaso.EventoDelWardenEnCurso",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FStepClockWardenEventTest::RunTest(const FString& Parameters)
{
	FStepClock Clock;
	TestFalse(TEXT("Con un evento en curso, entre turnos no se detiene"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::RuneHunt, true));
	Clock.Hold(10.0);
	Clock.Resume(40.0);
	TestFalse(TEXT("Reanudar suelta la espera"), Clock.bHeld);
	TestFalse(TEXT("Sin conceder turno"), Clock.bStepGranted);
	TestEqual(TEXT("El reloj sigue desde donde se detuvo"), Clock.MatchSeconds(40.0), 10.0);
	TestEqual(TEXT("Y corre"), Clock.MatchSeconds(46.0), 16.0);
	TestTrue(TEXT("Acabado el evento, vuelve a detenerse antes del siguiente turno"), Clock.ShouldHold(true, ETurnPhase::BetweenTurns, EMatchPhase::RuneHunt, false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
