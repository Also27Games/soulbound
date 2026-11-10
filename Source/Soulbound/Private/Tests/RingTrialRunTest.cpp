// Tests de la prueba del anillo en curso (S-026, R-MIN-2): primera pulsación, pulsaciones entre pulsos, pulsos que vencen sin pulsar y
// la fracción de cierre para pintar. Sin mundo ni editor, como CurtainTest.cpp. Filtro: "Soulbound.Game.Match.Anillo".
//
// Montaje: una secuencia hecha a mano, sin sorteo: cierre de 1 s, zona buena de 0,2 s y pulsos que empiezan en 0, 2 y 4 s; la prueba
// arranca en el 10. El pulso 0 va del 10 al 11,1 y coincide en el 11; el 1, del 12 al 13,1 y coincide en el 13; el 2, del 14 al 15,1.

#include "../../Match/RingTrialRun.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FRingTrialRun MakeRun()
	{
		FRingSequence Sequence;
		Sequence.PulseStarts = {0.0, 2.0, 4.0};
		Sequence.CloseSeconds = 1.0;
		Sequence.SweetZoneSeconds = 0.2;
		FRingTrialRun Run;
		Run.Begin(Sequence, 10.0);
		return Run;
	}
}

// CA-ANI-1 (primera pulsación), R-MIN-2: en un pulso, dos pulsaciones; cuenta la primera y la segunda no cambia la precisión.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunFirstPressTest, "Soulbound.Game.Match.Anillo.PrimeraPulsacion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunFirstPressTest::RunTest(const FString& Parameters)
{
	FRingTrialRun Run = MakeRun();
	TestTrue(TEXT("La primera pulsación del pulso 0 (a 0,05 s de la coincidencia) cuenta"), Run.Press(11.05));
	TestFalse(TEXT("La segunda, aunque sea perfecta, no cuenta"), Run.Press(11.0));
	if (TestEqual(TEXT("Un pulso resuelto"), Run.Precisions().Num(), 1))
	{
		TestEqual(TEXT("Su precisión es la de la primera (0,5)"), Run.Precisions()[0], 0.5, 1e-9);
	}

	// Antes de la zona buena también cuenta como primera: gasta el pulso con 0 (decisión del autor del 06/11/2026).
	FRingTrialRun Early = MakeRun();
	TestTrue(TEXT("Una pulsación antes de la zona buena cuenta"), Early.Press(10.3));
	TestFalse(TEXT("Y la de la coincidencia, después, ya no"), Early.Press(11.0));
	if (TestEqual(TEXT("Un pulso resuelto"), Early.Precisions().Num(), 1))
	{
		TestEqual(TEXT("Con precisión 0"), Early.Precisions()[0], 0.0, 1e-9);
	}
	return true;
}

// CA-ANI-2 (entre pulsos), R-MIN-2: una pulsación después de resolverse un pulso y antes de empezar el siguiente no cuenta para ninguno.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunBetweenPulsesTest, "Soulbound.Game.Match.Anillo.EntrePulsos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunBetweenPulsesTest::RunTest(const FString& Parameters)
{
	FRingTrialRun Run = MakeRun();
	TestFalse(TEXT("Durante el anuncio (antes del 10) no cuenta"), Run.Press(9.5));
	TestTrue(TEXT("El pulso 0, en la coincidencia, cuenta"), Run.Press(11.0));
	TestFalse(TEXT("En el 11,5, resuelto el 0 y antes del 1, no cuenta"), Run.Press(11.5));
	TestTrue(TEXT("El pulso 1, a 0,02 s, cuenta"), Run.Press(13.02));
	if (TestEqual(TEXT("Dos pulsos resueltos"), Run.Precisions().Num(), 2))
	{
		TestEqual(TEXT("El 0 conserva su 1"), Run.Precisions()[0], 1.0, 1e-9);
		TestEqual(TEXT("El 1 tiene la suya (0,8), no la del 11,5"), Run.Precisions()[1], 0.8, 1e-9);
	}

	// Con el pulso vencido sin pulsar: la pulsación del hueco tampoco cuenta para él ni para el siguiente.
	FRingTrialRun Late = MakeRun();
	TestFalse(TEXT("En el 11,5, vencido el pulso 0 sin pulsar, no cuenta"), Late.Press(11.5));
	if (TestEqual(TEXT("El pulso 0 queda resuelto"), Late.Precisions().Num(), 1))
	{
		TestEqual(TEXT("Con 0"), Late.Precisions()[0], 0.0, 1e-9);
	}
	TestTrue(TEXT("El pulso 1 sigue libre: su primera pulsación cuenta"), Late.Press(13.0));
	return true;
}

// CA-ANI-3 (sin pulsar), R-MIN-2: un pulso que vence sin pulsación queda en 0 y la prueba pasa al siguiente; al vencer el último,
// IsFinished es verdadero y hay tantas precisiones como pulsos.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunNoPressTest, "Soulbound.Game.Match.Anillo.SinPulsar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunNoPressTest::RunTest(const FString& Parameters)
{
	FRingTrialRun Run = MakeRun();
	Run.Advance(11.0);
	TestEqual(TEXT("En la coincidencia del pulso 0, aún sin resolver"), Run.Precisions().Num(), 0);
	Run.Advance(11.2);
	TestEqual(TEXT("Vencido el pulso 0, resuelto"), Run.Precisions().Num(), 1);
	TestEqual(TEXT("En el 12,5 el pulso en curso es el 1"), Run.CurrentPulse(12.5), 1);
	TestFalse(TEXT("Aún no ha terminado"), Run.IsFinished());
	Run.Advance(15.0);
	TestFalse(TEXT("En el 15, el último no ha vencido"), Run.IsFinished());
	Run.Advance(15.2);
	TestTrue(TEXT("Vencido el último, terminada"), Run.IsFinished());
	if (TestEqual(TEXT("Tantas precisiones como pulsos"), Run.Precisions().Num(), 3))
	{
		TestTrue(TEXT("Todas 0"), Run.Precisions()[0] == 0.0 && Run.Precisions()[1] == 0.0 && Run.Precisions()[2] == 0.0);
	}
	TestFalse(TEXT("Terminada, una pulsación no cuenta"), Run.Press(15.3));
	TestEqual(TEXT("Terminada, no hay pulso en curso"), Run.CurrentPulse(15.3), static_cast<int32>(INDEX_NONE));

	// Un avance tardío que salta varios pulsos los cierra todos de una vez, en orden.
	FRingTrialRun Jump = MakeRun();
	Jump.Advance(100.0);
	TestTrue(TEXT("Un avance muy tardío la termina"), Jump.IsFinished());
	TestEqual(TEXT("Con las tres precisiones"), Jump.Precisions().Num(), 3);
	return true;
}

// CA-ANI-4 (pintado): la fracción de cierre vale 0 al empezar el pulso, 1 en la coincidencia y está acotada entre 0 y 1 con cualquier
// tiempo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunCloseFractionTest, "Soulbound.Game.Match.Anillo.FraccionDeCierre",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunCloseFractionTest::RunTest(const FString& Parameters)
{
	FRingTrialRun Run = MakeRun();
	TestEqual(TEXT("Al empezar el pulso 0, 0"), Run.CloseFraction(10.0), 0.0, 1e-9);
	TestEqual(TEXT("A mitad del cierre, 0,5"), Run.CloseFraction(10.5), 0.5, 1e-9);
	TestEqual(TEXT("En la coincidencia, 1"), Run.CloseFraction(11.0), 1.0, 1e-9);
	TestTrue(TEXT("En el anuncio, ni el pulso ni la prueba han empezado"), Run.IsAnnouncing(9.0) && Run.CurrentPulse(9.0) == INDEX_NONE);
	TestEqual(TEXT("En el pulso 0 y en curso"), Run.CurrentPulse(10.5), 0);

	Run.Advance(11.2);
	TestEqual(TEXT("Al empezar el pulso 1, 0"), Run.CloseFraction(12.0), 0.0, 1e-9);
	TestEqual(TEXT("En la coincidencia del pulso 1, 1"), Run.CloseFraction(13.0), 1.0, 1e-9);

	bool bBounded = true;
	for (const double Now : {-1000.0, 0.0, 9.99, 10.0, 11.05, 11.5, 12.7, 13.1, 14.0, 15.1, 20.0, 1e6})
	{
		const double Fraction = Run.CloseFraction(Now);
		bBounded &= Fraction >= 0.0 && Fraction <= 1.0;
	}
	TestTrue(TEXT("Acotada entre 0 y 1 con cualquier tiempo"), bBounded);
	Run.Advance(100.0);
	const double Finished = Run.CloseFraction(100.0);
	TestTrue(TEXT("Terminada, también acotada"), Finished >= 0.0 && Finished <= 1.0);
	return true;
}

// CA-ACT-7 (consulta del anillo), R-ACT-10: qué sería una pulsación en Now, sin mutar: pulso y desfase con signo, o el motivo. Y coincide
// con lo que decide Press.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunQueryPressTest, "Soulbound.Game.Match.Anillo.ConsultaDePulsacion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunQueryPressTest::RunTest(const FString& Parameters)
{
	const FRingTrialRun Run = MakeRun();
	const FRingPressQuery Announcement = Run.QueryPress(9.5);
	TestTrue(TEXT("Antes de empezar, el anuncio"), Announcement.Kind == ERingPressKind::Announcement);

	const FRingPressQuery Early = Run.QueryPress(10.969);
	TestTrue(TEXT("Dentro del pulso 0, cuenta"), Early.Kind == ERingPressKind::Counts && Early.Pulse == 0);
	TestEqual(TEXT("Su desfase es negativo: antes de la coincidencia (11)"), Early.OffsetSeconds, -0.031, 1e-9);

	const FRingPressQuery Late = Run.QueryPress(13.05);
	TestTrue(TEXT("En el 13,05 el pulso 0 ha vencido sin pulsar; la consulta ve ya el 1"), Late.Kind == ERingPressKind::Counts && Late.Pulse == 1);
	TestEqual(TEXT("Su desfase es positivo: después de la coincidencia (13)"), Late.OffsetSeconds, 0.05, 1e-9);
	TestEqual(TEXT("Consultar no resuelve nada"), Run.Precisions().Num(), 0);

	TestTrue(TEXT("En el 11,5, vencido el 0 y antes del 1, entre pulsos"), Run.QueryPress(11.5).Kind == ERingPressKind::BetweenPulses);
	TestTrue(TEXT("Pasado el último, terminada"), Run.QueryPress(16.0).Kind == ERingPressKind::Finished);

	// Lo que dice la consulta es lo que hace Press: cuenta si y solo si la consulta dice Counts.
	const double Times[] = {9.5, 10.5, 11.5, 12.5, 13.5, 15.5, 16.0};
	for (const double Now : Times)
	{
		FRingTrialRun Pressed = MakeRun();
		const bool bQueryCounts = Pressed.QueryPress(Now).Kind == ERingPressKind::Counts;
		TestEqual(*FString::Printf(TEXT("Press en %.1f coincide con la consulta"), Now), Pressed.Press(Now), bQueryCounts);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
