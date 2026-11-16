// Tests de la prueba del anillo en curso (S-026, R-MIN-2): primera pulsación, pulsaciones entre pulsos, pulsos que vencen sin pulsar y
// la fracción de cierre para pintar. Sin mundo ni editor, como CurtainTest.cpp. Filtro: "Soulbound.Game.Match.Anillo".
//
// Montaje: una secuencia hecha a mano, sin sorteo: cierre de 1 s, zona buena de 0,2 s y pulsos que empiezan en 0, 2 y 4 s; la prueba
// arranca en el 10. El pulso 0 va del 10 al 11,1 y coincide en el 11; el 1, del 12 al 13,1 y coincide en el 13; el 2, del 14 al 15,1.

#include "../../Match/RingTrialRun.h"
#include "Match/MatchParams.h"
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

	/**
	 * S-042: una secuencia sorteada como la sortea el anfitrión (MakeRingSequence), con la semilla Seed: huecos de 0,8 a 2 s y tres pulsos con el
	 * cierre y la zona de normal (1,2 s y 0,18). Montaje de la espera: se abre en el 10, espera máxima 8 s (cuenta a cero en el 18), respiro 0,5 s.
	 */
	constexpr double WaitOpen = 10.0;
	constexpr double WaitMax = 8.0;
	constexpr double WaitBreather = 0.5;

	FRingSequence MakeDrawnSequence(int32 Seed)
	{
		FRingParams Ring;
		Ring.PulseGapMinSeconds = 0.8;
		Ring.PulseGapMaxSeconds = 2.0;
		FRingTrialParams Trial;
		Trial.CloseSeconds = 1.2;
		Trial.SweetZone = 0.18;
		Trial.Pulses = 3;
		return Soulbound::Rules::MakeRingSequence(Ring, Trial, Seed);
	}

	FRingTrialRun MakeWaitingRun(int32 Seed = 1234)
	{
		FRingTrialRun Run;
		Run.Open(MakeDrawnSequence(Seed), WaitOpen, WaitMax, WaitBreather);
		return Run;
	}

	/** Si en la espera el anillo está quieto en Now: sin pulsos resueltos, sin pulso en curso, sin cierre y ninguna pulsación cuenta. */
	bool IsIdleWhileWaiting(const FRingTrialRun& Run, double Now)
	{
		return Run.Precisions().IsEmpty() && Run.CurrentPulse(Now) == INDEX_NONE && Run.CloseFraction(Now) == 0.0 && !Run.IsInSweetZone(Now)
			&& Run.QueryPress(Now).Kind == ERingPressKind::Waiting;
	}
}

// CA-MIN-14 (empezar por el botón), R-MIN-19: con el anillo abierto y sin empezar, ningún pulso se cierra ni vence aunque pase el tiempo, hasta la
// espera máxima. Al pulsar "Empezar" en el segundo 2 (el 12), el primer pulso comienza a cerrarse en el 12 más el respiro, y la secuencia es la
// sorteada con la misma semilla, sin tocar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunStartByButtonTest, "Soulbound.Game.Match.Anillo.EsperaYEmpiezaPorElBoton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunStartByButtonTest::RunTest(const FString& Parameters)
{
	FRingTrialRun Run = MakeWaitingRun();
	TestTrue(TEXT("Abierta, espera"), Run.IsWaiting());
	bool bIdle = true;
	for (const double Now : {10.0, 11.0, 11.2, 12.5, 14.0, 16.0, 17.99})
	{
		Run.Advance(Now);
		bIdle &= IsIdleWhileWaiting(Run, Now);
	}
	TestTrue(TEXT("Hasta la espera máxima, ningún pulso se cierra ni vence"), bIdle);
	TestFalse(TEXT("Y no ha terminado"), Run.IsFinished());

	// En la prueba, el tiempo de antes ya pasó: se vuelve a una copia abierta, sin avances, para pulsar en el 12.
	FRingTrialRun Pressed = MakeWaitingRun();
	Pressed.Advance(11.9);
	TestTrue(TEXT("\"Empezar\" en el segundo 2 empieza la prueba"), Pressed.Start(WaitOpen + 2.0));
	TestFalse(TEXT("Empezada, ya no espera"), Pressed.IsWaiting());
	TestTrue(TEXT("Por el botón"), Pressed.StartCause == ERingStartCause::Button);
	TestEqual(TEXT("Esperó 2 s"), Pressed.WaitedSeconds(), 2.0, 1e-9);
	const double FirstPulse = WaitOpen + 2.0 + WaitBreather;
	TestEqual(TEXT("El primer pulso empieza en el 12 más el respiro"), Pressed.StartSeconds, FirstPulse, 1e-9);
	TestEqual(TEXT("En el respiro no hay pulso en curso"), Pressed.CurrentPulse(FirstPulse - 0.01), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("En el 12,5 el pulso 0 está en curso"), Pressed.CurrentPulse(FirstPulse), 0);
	TestEqual(TEXT("Y empieza a cerrarse desde 0"), Pressed.CloseFraction(FirstPulse), 0.0, 1e-9);
	TestTrue(TEXT("A mitad del cierre (0,6 s), se está cerrando"), Pressed.CloseFraction(FirstPulse + 0.6) > 0.4 && Pressed.CloseFraction(FirstPulse + 0.6) < 0.6);
	TestFalse(TEXT("Un segundo \"Empezar\" no hace nada"), Pressed.Start(FirstPulse + 0.1));
	TestEqual(TEXT("Ni mueve el inicio"), Pressed.StartSeconds, FirstPulse, 1e-9);

	// Misma semilla, mismos pulsos: la espera no vuelve a sortear ni desplaza los pulsos entre sí.
	const FRingSequence Drawn = MakeDrawnSequence(1234);
	TestEqual(TEXT("La secuencia es la sorteada con la misma semilla"), Pressed.Sequence.PulseStarts, Drawn.PulseStarts);
	TestEqual(TEXT("Con el mismo cierre"), Pressed.Sequence.CloseSeconds, Drawn.CloseSeconds, 1e-12);
	TestEqual(TEXT("Y la misma zona"), Pressed.Sequence.SweetZoneSeconds, Drawn.SweetZoneSeconds, 1e-12);
	FRingTrialRun NoWait;
	NoWait.Begin(Drawn, FirstPulse);
	TestTrue(TEXT("Cada pulso coincide donde coincidiría sin espera, empezando en el mismo instante"),
		Pressed.QueryPress(FirstPulse + Drawn.CoincidenceAt(1)).Pulse == NoWait.QueryPress(FirstPulse + Drawn.CoincidenceAt(1)).Pulse
		&& FMath::IsNearlyZero(Pressed.QueryPress(FirstPulse + Drawn.CoincidenceAt(2)).OffsetSeconds, 1e-9));
	return true;
}

// CA-MIN-15 (empezar sola), R-MIN-19: sin pulsar, al cumplirse la espera máxima la prueba empieza; el primer pulso comienza a cerrarse a la espera
// máxima más el respiro, aunque el avance llegue tarde. Sin castigo: no se resuelve ningún pulso, y las mismas pulsaciones dan el mismo resultado
// que empezando por el botón. La insolencia no la toca el anillo (no tiene estado de partida); el anfitrión, al empezar, solo escribe el acta.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunStartsAloneTest, "Soulbound.Game.Match.Anillo.EmpiezaSola",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunStartsAloneTest::RunTest(const FString& Parameters)
{
	FRingTrialRun Run = MakeWaitingRun();
	TestEqual(TEXT("La cuenta vence en el 18"), Run.StartDueSeconds(), WaitOpen + WaitMax, 1e-9);
	TestFalse(TEXT("En el 17,9 aún no empieza"), Run.StartIfDue(17.9));
	TestTrue(TEXT("Y sigue esperando"), Run.IsWaiting());
	// El tick que lo nota llega tarde, en el 18,3: empieza en el 18, no en el 18,3.
	TestTrue(TEXT("En el 18,3, empieza sola"), Run.StartIfDue(18.3));
	TestTrue(TEXT("Por la cuenta"), Run.StartCause == ERingStartCause::Countdown);
	TestEqual(TEXT("Esperó la espera máxima"), Run.WaitedSeconds(), WaitMax, 1e-9);
	const double FirstPulse = WaitOpen + WaitMax + WaitBreather;
	TestEqual(TEXT("El primer pulso empieza a la espera máxima más el respiro"), Run.StartSeconds, FirstPulse, 1e-9);
	TestFalse(TEXT("Empezada, no vuelve a empezar"), Run.StartIfDue(30.0));
	TestTrue(TEXT("Sin castigo: ningún pulso resuelto"), Run.Precisions().IsEmpty());

	// "Empezar" pulsado cuando la cuenta ya venció no empieza por el botón: gana la cuenta, en su instante.
	FRingTrialRun Late = MakeWaitingRun();
	TestFalse(TEXT("\"Empezar\" en el 18,2, con la cuenta vencida: no es por el botón"), Late.Start(18.2));
	TestTrue(TEXT("Pero ha empezado sola"), !Late.IsWaiting() && Late.StartCause == ERingStartCause::Countdown);
	TestEqual(TEXT("En el instante de la cuenta"), Late.StartSeconds, FirstPulse, 1e-9);

	// Las mismas pulsaciones, relativas al primer pulso, dan las mismas precisiones que empezando por el botón: empezar sola no cambia nada.
	FRingTrialRun Button = MakeWaitingRun();
	Button.Start(WaitOpen + 3.0);
	const double ButtonFirst = WaitOpen + 3.0 + WaitBreather;
	for (const double Offset : {0.05, -0.02, 0.0})
	{
		const int32 Pulse = Run.Precisions().Num();
		if (!TestTrue(TEXT("Queda un pulso por jugar"), Pulse < Run.NumPulses() && Pulse < Button.NumPulses()))
		{
			break;
		}
		Run.Press(FirstPulse + Run.Sequence.CoincidenceAt(Pulse) + Offset);
		Button.Press(ButtonFirst + Button.Sequence.CoincidenceAt(Pulse) + Offset);
	}
	TestEqual(TEXT("Mismas precisiones que por el botón"), Run.Precisions(), Button.Precisions());
	TestTrue(TEXT("Y la prueba terminada"), Run.IsFinished());
	return true;
}

// CA-MIN-16 (la pulsación de empezar no cuenta), R-MIN-19: la pulsación que empieza la prueba no se registra como pulsación del anillo; tras ella,
// cero pulsos resueltos y la prueba entera por jugar. Una pulsación del anillo durante la espera, fuera del botón, tampoco cuenta ni resuelve nada.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunStartPressNotCountedTest, "Soulbound.Game.Match.Anillo.EmpezarNoCuenta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunStartPressNotCountedTest::RunTest(const FString& Parameters)
{
	FRingTrialRun Run = MakeWaitingRun();
	TestTrue(TEXT("Durante la espera, la consulta dice que espera"), Run.QueryPress(11.0).Kind == ERingPressKind::Waiting);
	TestFalse(TEXT("Una pulsación del anillo durante la espera no cuenta"), Run.Press(11.0));
	TestTrue(TEXT("Ni resuelve ningún pulso"), Run.Precisions().IsEmpty());
	TestTrue(TEXT("Ni empieza la prueba"), Run.IsWaiting());

	TestTrue(TEXT("\"Empezar\" en el 12"), Run.Start(12.0));
	TestEqual(TEXT("La pulsación de empezar no resuelve ningún pulso"), Run.Precisions().Num(), 0);
	// Pulsar el anillo enseguida (en el respiro) tampoco cuenta: es el anuncio, el de siempre.
	TestTrue(TEXT("En el respiro, el anuncio"), Run.QueryPress(12.2).Kind == ERingPressKind::Announcement);
	TestFalse(TEXT("Una pulsación en el respiro no cuenta"), Run.Press(12.2));
	TestEqual(TEXT("Cero pulsos resueltos: la prueba recién empezada"), Run.Precisions().Num(), 0);
	TestEqual(TEXT("Con todos sus pulsos por jugar"), Run.NumPulses(), 3);
	TestTrue(TEXT("La primera pulsación en el pulso 0 cuenta para él"), Run.Press(12.5 + Run.Sequence.CoincidenceAt(0)));
	if (TestEqual(TEXT("Un pulso resuelto"), Run.Precisions().Num(), 1))
	{
		TestEqual(TEXT("Con la precisión de esa pulsación (1)"), Run.Precisions()[0], 1.0, 1e-9);
	}
	return true;
}

// CA-VIS-14 (zona), R-VIS-15: con cierre de 1,2 s y zona de 0,18 (0,216 s), dentro entre la coincidencia menos y más media zona (0,108 s) del primer
// pulso sin resolver; fuera en el anuncio, entre pulsos y con la prueba terminada. Montaje propio: pulsos en 0 y 3 s, la prueba arranca en el 10;
// el pulso 0 coincide en el 11,2.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRunSweetZoneTest, "Soulbound.Game.Presentation.Anillo.ZonaBuena",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingRunSweetZoneTest::RunTest(const FString& Parameters)
{
	FRingSequence Sequence;
	Sequence.PulseStarts = {0.0, 3.0};
	Sequence.CloseSeconds = 1.2;
	Sequence.SweetZoneSeconds = 0.18 * 1.2;
	FRingTrialRun Run;
	Run.Begin(Sequence, 10.0);
	const double Coincidence = 11.2;

	TestFalse(TEXT("Anuncio (antes del 10): fuera"), Run.IsInSweetZone(9.0));
	TestFalse(TEXT("0,2 s antes de la coincidencia: fuera"), Run.IsInSweetZone(Coincidence - 0.2));
	TestTrue(TEXT("0,1 s antes: dentro"), Run.IsInSweetZone(Coincidence - 0.1));
	TestTrue(TEXT("En la coincidencia: dentro"), Run.IsInSweetZone(Coincidence));
	TestTrue(TEXT("0,1 s después: dentro"), Run.IsInSweetZone(Coincidence + 0.1));
	TestFalse(TEXT("0,12 s después: fuera"), Run.IsInSweetZone(Coincidence + 0.12));
	TestFalse(TEXT("Entre pulsos (el 0 vencido, el 1 aún no empieza): fuera"), Run.IsInSweetZone(12.0));

	// Resuelto el pulso 0 con una pulsación, deja de contar aunque el instante siga en su zona: el primero sin resolver es el 1.
	FRingTrialRun Pressed = Run;
	TestTrue(TEXT("La pulsación en la coincidencia cuenta"), Pressed.Press(Coincidence));
	TestFalse(TEXT("Resuelto el pulso, 0,05 s después: fuera"), Pressed.IsInSweetZone(Coincidence + 0.05));
	TestTrue(TEXT("La zona del pulso 1 sí cuenta"), Pressed.IsInSweetZone(10.0 + 3.0 + 1.2));

	Run.Advance(100.0);
	TestFalse(TEXT("Prueba terminada: fuera"), Run.IsInSweetZone(100.0));
	return true;
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
