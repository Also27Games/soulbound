// Tests del modelo del panel del anillo (S-031): radio del aro, barra de progreso y Dominio de la runa. Funciones puras, sin Slate, sin mundo ni
// editor. Cada test cita el criterio (CA-VIS-n) y la regla (R-VIS-n) que cubre. Filtro: "Soulbound.Game.Presentation.Anillo".

#include "../../Presentation/RingPanelModel.h"
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

#endif
