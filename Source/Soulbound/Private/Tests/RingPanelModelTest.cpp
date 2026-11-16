// Tests del modelo del panel del anillo (S-031): radio del aro, barra de progreso y Dominio de la runa. Funciones puras, sin Slate, sin mundo ni
// editor. Cada test cita el criterio (CA-VIS-n) y la regla (R-VIS-n) que cubre. Filtro: "Soulbound.Game.Presentation.Anillo".

#include "../../Presentation/RingPanelModel.h"
#include "../../Match/RingTrialRun.h"
#include "Match/MatchState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RingModelTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

// CA-VIS-15 (barra), R-VIS-16: el relleno es la suma de precisiones entre el número de pulsos; nunca baja al añadir uno.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingProgressFillTest, "Soulbound.Game.Presentation.Anillo.BarraRelleno", RingModelTestFlags)
bool FRingProgressFillTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::RingProgress;
	const ERingCriterion Mean = ERingCriterion::MeanAtLeastThreshold;
	TestEqual(TEXT("0,45 y 0,28 de tres pulsos: 0,73 / 3"), RingProgress({0.45, 0.28}, 3, Mean, 0.55).Fill, 0.73 / 3.0, 1e-9);
	TestEqual(TEXT("Sin ninguna resuelta, 0"), RingProgress({}, 3, Mean, 0.55).Fill, 0.0, 1e-9);
	TestEqual(TEXT("Tres de 1, llena"), RingProgress({1.0, 1.0, 1.0}, 3, Mean, 0.55).Fill, 1.0, 1e-9);

	const TArray<double> Sequence = {0.0, 0.7, 0.0, 0.3, 1.0};
	double Previous = 0.0;
	for (int32 Count = 0; Count <= Sequence.Num(); ++Count)
	{
		const double Fill = RingProgress(TConstArrayView<double>(Sequence).Left(Count), Sequence.Num(), Mean, 0.55).Fill;
		TestTrue(FString::Printf(TEXT("Con %d pulsos resueltos el relleno no baja"), Count), Fill >= Previous - 1e-12);
		Previous = Fill;
	}
	return true;
}

// CA-VIS-16 (marca), R-VIS-16: con media y umbral, la marca está en el umbral; en el duelo y el desafío final no hay marca.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingProgressMarkTest, "Soulbound.Game.Presentation.Anillo.BarraMarca", RingModelTestFlags)
bool FRingProgressMarkTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::RingProgress;
	const FRingProgress Mean = RingProgress({0.5}, 3, ERingCriterion::MeanAtLeastThreshold, 0.55);
	if (TestTrue(TEXT("Con media y umbral hay marca"), Mean.Mark.IsSet()))
	{
		TestEqual(TEXT("Está en 0,55"), Mean.Mark.GetValue(), 0.55, 1e-9);
	}
	TestFalse(TEXT("En el duelo no hay marca"), RingProgress({0.5}, 3, ERingCriterion::Performance, 0.55).Mark.IsSet());
	TestFalse(TEXT("En el desafío final no hay marca"), RingProgress({0.5}, 3, ERingCriterion::NoMiss, 0.55).Mark.IsSet());
	return true;
}

// CA-VIS-17 (misma regla), R-VIS-16: "el relleno final llega a la marca" coincide con RingTrialPassed, con las precisiones de CA-MIN-12.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingProgressMatchesEngineTest, "Soulbound.Game.Presentation.Anillo.BarraCoincideConMotor", RingModelTestFlags)
bool FRingProgressMatchesEngineTest::RunTest(const FString& Parameters)
{
	FRingTrialParams Params;
	Params.Criterion = ERingCriterion::MeanAtLeastThreshold;
	Params.PassThreshold = 0.55;
	Params.Pulses = 3;
	const TArray<TArray<double>> Cases = {{0.6, 0.5, 0.55}, {0.6, 0.5, 0.5}};
	for (const TArray<double>& Precisions : Cases)
	{
		const FRingProgress Progress = Soulbound::Presentation::RingProgress(Precisions, Params.Pulses, Params.Criterion, Params.PassThreshold);
		if (TestTrue(TEXT("Hay marca"), Progress.Mark.IsSet()))
		{
			// Misma tolerancia aritmética que el motor (Soulbound::Ring::MeanTolerance): la barra no dice nada distinto.
			const bool bReaches = Progress.Fill >= Progress.Mark.GetValue() - Soulbound::Ring::MeanTolerance;
			TestEqual(FString::Printf(TEXT("Precisiones %.2f / %.2f / %.2f: la barra y el motor coinciden"), Precisions[0], Precisions[1], Precisions[2]),
				bReaches, Soulbound::Rules::RingTrialPassed(Params, Precisions));
		}
	}
	return true;
}

// CA-VIS-18 (contacto), R-VIS-19: el borde interior del aro toca la runa exactamente en la coincidencia y no baja de ahí.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingInnerRadiusTest, "Soulbound.Game.Presentation.Anillo.RadioInterior", RingModelTestFlags)
bool FRingInnerRadiusTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::RingInnerRadius;
	constexpr double Rune = 34.0;
	constexpr double Max = 150.0;
	TestEqual(TEXT("En la coincidencia (cierre 1) es el radio de la runa"), RingInnerRadius(1.0, Rune, Max), Rune, 1e-9);
	TestEqual(TEXT("Con el cierre en 0 es el radio máximo"), RingInnerRadius(0.0, Rune, Max), Max, 1e-9);
	double Previous = RingInnerRadius(0.0, Rune, Max);
	for (int32 Step = 1; Step <= 100; ++Step)
	{
		const double Radius = RingInnerRadius(Step / 100.0, Rune, Max);
		TestTrue(FString::Printf(TEXT("Decrece sin saltos en el paso %d"), Step), Radius <= Previous && Previous - Radius <= (Max - Rune) / 100.0 + 1e-9);
		Previous = Radius;
	}
	TestEqual(TEXT("Pasada la coincidencia no baja del radio de la runa"), RingInnerRadius(1.3, Rune, Max), Rune, 1e-9);
	return true;
}

// R-VIS-18: la runa se tiñe con el Dominio de la casilla del humano solo en una Fuente.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingRuneDomainTest, "Soulbound.Game.Presentation.Anillo.DominioDeLaRuna", RingModelTestFlags)
bool FRingRuneDomainTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::RingRuneDomain;
	FMatchState State;
	State.Board.Tiles.SetNum(2);
	State.Board.Tiles[0].TileId = 0;
	State.Board.Tiles[1].TileId = 1;
	State.Board.Tiles[1].DomainId = 4;
	State.Seats.SetNum(2);
	State.Seats[0].TileId = 1;
	State.Seats[1].TileId = INDEX_NONE; // en el Vacío

	TestEqual(TEXT("Fuente: el Dominio de la casilla del asiento"), RingRuneDomain(State, 0, ESkillTrial::ForgottenSource), 4);
	TestEqual(TEXT("Prueba del Warden: ninguno"), RingRuneDomain(State, 0, ESkillTrial::WardenTrial), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Escape del Vacío: ninguno"), RingRuneDomain(State, 0, ESkillTrial::VoidEscape), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Duelo (None): ninguno"), RingRuneDomain(State, 0, ESkillTrial::None), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Asiento en el Vacío: ninguno"), RingRuneDomain(State, 1, ESkillTrial::ForgottenSource), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Asiento que no existe: ninguno"), RingRuneDomain(State, 7, ESkillTrial::ForgottenSource), static_cast<int32>(INDEX_NONE));
	return true;
}

// CA-VIS-20, R-VIS-22: el nombre del uso es solo el uso, sin separador ni nivel. La función no recibe la dificultad, así que dos niveles del
// mismo uso dan por fuerza el mismo título; el test fija el texto exacto de cada uso y que no se cuela ninguna palabra de nivel. Desde S-042 el
// panel del anillo lleva RingStakeTitle (CA-VIS-31, abajo); este nombre lo sigue usando la línea "Prueba pendiente" del panel de estado.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingTitleTextTest, "Soulbound.Game.Presentation.Anillo.TituloSoloElUso", RingModelTestFlags)
bool FRingTitleTextTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::RingTitleText;
	struct FCase
	{
		const TCHAR* Name;
		ESkillTrial Trial;
		bool bDuel;
		const TCHAR* Expected;
	};
	const FCase Cases[] = {
		{TEXT("duelo"), ESkillTrial::None, true, TEXT("DUELO")},
		{TEXT("duelo gana a la prueba"), ESkillTrial::WardenTrial, true, TEXT("DUELO")},
		{TEXT("Fuente de un Dominio Olvidado"), ESkillTrial::ForgottenSource, false, TEXT("FUENTE DE UN DOMINIO OLVIDADO")},
		{TEXT("Prueba del Warden"), ESkillTrial::WardenTrial, false, TEXT("PRUEBA DEL WARDEN")},
		{TEXT("escape del Vacío"), ESkillTrial::VoidEscape, false, TEXT("ESCAPE DEL VACÍO")},
		{TEXT("desafío final"), ESkillTrial::FinalChallenge, false, TEXT("DESAFÍO FINAL")},
		{TEXT("sin uso conocido"), ESkillTrial::None, false, TEXT("PRUEBA")},
	};
	for (const FCase& Case : Cases)
	{
		const FString Title = RingTitleText(Case.Trial, Case.bDuel);
		TestEqual(FString::Printf(TEXT("Título de %s"), Case.Name), Title, FString(Case.Expected));
		for (const TCHAR* Forbidden : {TEXT("·"), TEXT("fácil"), TEXT("normal"), TEXT("difícil")})
		{
			TestFalse(FString::Printf(TEXT("El título de %s no lleva «%s»"), Case.Name, Forbidden), Title.Contains(Forbidden));
		}
	}
	return true;
}

// CA-VIS-21, R-VIS-17: la instrucción son dos líneas, ninguna vacía y ninguna habla de "pulso"; la función no recibe el estado del anillo, así que
// son las mismas en el anuncio y con un pulso resuelto (dos llamadas dan lo mismo).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingInstructionLinesTest, "Soulbound.Game.Presentation.Anillo.InstruccionEnDosLineas", RingModelTestFlags)
bool FRingInstructionLinesTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::RingInstructionLines;
	const TArray<FString> Lines = RingInstructionLines();
	if (!TestEqual(TEXT("Dos líneas"), Lines.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Primera línea"), Lines[0], FString(TEXT("Haz clic cuando el aro toque la runa.")));
	TestEqual(TEXT("Segunda línea"), Lines[1], FString(TEXT("En la franja verde cuenta; cuanto más cerca, mejor.")));
	for (const FString& Line : Lines)
	{
		TestFalse(TEXT("Ninguna vacía"), Line.IsEmpty());
		TestFalse(FString::Printf(TEXT("«%s» no habla de pulso"), *Line), Line.Contains(TEXT("pulso"), ESearchCase::IgnoreCase));
	}
	TestEqual(TEXT("Las mismas en el anuncio y con un pulso resuelto"), RingInstructionLines(), Lines);
	return true;
}

// CA-VIS-31, R-VIS-30: el título dice qué te juegas. La Prueba del Warden, letra por letra la del autor; las otras cuatro, las aprobadas por el autor
// el 16/11/2026 en el plan; el duelo gana a la prueba; ninguna de las cinco queda sin frase.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingStakeTitleTest, "Soulbound.Game.Presentation.Anillo.TituloConLoQueTeJuegas", RingModelTestFlags)
bool FRingStakeTitleTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::RingStakeTitle;
	struct FCase
	{
		const TCHAR* Name;
		ESkillTrial Trial;
		bool bDuel;
		const TCHAR* Expected;
	};
	const FCase Cases[] = {
		{TEXT("Prueba del Warden"), ESkillTrial::WardenTrial, false, TEXT("Prueba del Warden: si fallas, pierdes el turno")},
		{TEXT("Fuente de un Dominio Olvidado"), ESkillTrial::ForgottenSource, false, TEXT("Fuente de un Dominio Olvidado: si fallas, no consigues la runa")},
		{TEXT("escape del Vacío"), ESkillTrial::VoidEscape, false, TEXT("Escape del Vacío: si fallas, sigues en el Vacío")},
		{TEXT("desafío final"), ESkillTrial::FinalChallenge, false, TEXT("Desafío final: si fallas, pierdes una runa y vuelves a tu hogar")},
		{TEXT("duelo"), ESkillTrial::None, true, TEXT("Duelo: si pierdes, te roban runas")},
		{TEXT("duelo gana a la prueba"), ESkillTrial::WardenTrial, true, TEXT("Duelo: si pierdes, te roban runas")},
	};
	for (const FCase& Case : Cases)
	{
		const FString Title = RingStakeTitle(Case.Trial, Case.bDuel);
		TestEqual(FString::Printf(TEXT("Título de %s, letra por letra"), Case.Name), Title, FString(Case.Expected));
		TestTrue(FString::Printf(TEXT("%s lleva su frase de lo que se juega"), Case.Name), Title.Contains(TEXT(": si ")));
	}
	return true;
}

// CA-MIN-17, R-MIN-19: con el anillo sin empezar, el modelo lleva el botón "Empezar", su tecla y la cuenta con los segundos que quedan, redondeados
// hacia arriba; con la prueba empezada, ninguno de los tres. Montaje: se abre en el 10 con espera de 8 s (vence en el 18) y respiro de 0,5 s.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingStartPromptTest, "Soulbound.Game.Presentation.Anillo.BotonEmpezarYCuenta", RingModelTestFlags)
bool FRingStartPromptTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::AddRingStartPrompt;
	FRingSequence Sequence;
	Sequence.PulseStarts = {0.0, 2.0};
	Sequence.CloseSeconds = 1.0;
	Sequence.SweetZoneSeconds = 0.2;
	FRingTrialRun Run;
	Run.Open(Sequence, 10.0, 8.0, 0.5);

	struct FCase
	{
		double Now;
		const TCHAR* Countdown;
	};
	const FCase Cases[] = {
		{10.0, TEXT("Empieza sola en 8 s")},
		{12.3, TEXT("Empieza sola en 6 s")},
		{17.01, TEXT("Empieza sola en 1 s")},
	};
	for (const FCase& Case : Cases)
	{
		FRingPanelModel Model;
		AddRingStartPrompt(Model, Run, Case.Now, TEXT("Enter"));
		TestEqual(FString::Printf(TEXT("En el %.2f, el botón"), Case.Now), Model.StartLabel, FString(TEXT("Empezar")));
		TestEqual(FString::Printf(TEXT("En el %.2f, su tecla"), Case.Now), Model.StartKeyHint, FString(TEXT("o pulsa Intro")));
		TestEqual(FString::Printf(TEXT("En el %.2f, la cuenta"), Case.Now), Model.StartCountdown, FString(Case.Countdown));
	}

	FRingTrialRun Started = Run;
	Started.Start(12.0);
	for (const double Now : {12.1, 12.6, 20.0})
	{
		FRingPanelModel Model;
		AddRingStartPrompt(Model, Started, Now, TEXT("Enter"));
		TestTrue(FString::Printf(TEXT("Empezada, en el %.1f: ni botón, ni tecla, ni cuenta"), Now),
			Model.StartLabel.IsEmpty() && Model.StartKeyHint.IsEmpty() && Model.StartCountdown.IsEmpty());
	}
	FRingTrialRun Alone = Run;
	Alone.StartIfDue(18.0);
	FRingPanelModel AloneModel;
	AddRingStartPrompt(AloneModel, Alone, 18.1, TEXT("Enter"));
	TestTrue(TEXT("Empezada sola: tampoco"), AloneModel.StartLabel.IsEmpty() && AloneModel.StartKeyHint.IsEmpty() && AloneModel.StartCountdown.IsEmpty());

	// El texto de la cuenta, por sí solo: hacia arriba, y nunca negativo.
	TestEqual(TEXT("5,7 s dicen 6"), Soulbound::Presentation::RingStartCountdownText(5.7), FString(TEXT("Empieza sola en 6 s")));
	TestEqual(TEXT("8 s justos dicen 8"), Soulbound::Presentation::RingStartCountdownText(8.0), FString(TEXT("Empieza sola en 8 s")));
	TestEqual(TEXT("Negativo, 0"), Soulbound::Presentation::RingStartCountdownText(-0.2), FString(TEXT("Empieza sola en 0 s")));
	return true;
}

#endif
