// Tests del paso 3 del spike: bucle de simulación con reloj virtual (R-SIM-1 a 5 del GDD 08).
// Determinismo por semilla y coste por partida. Se lanzan con Scripts/test.ps1.

#include "Decider/Decider.h"
#include "Misc/AutomationTest.h"
#include "Simulation/MatchSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundTest, Log, All);

namespace
{
	TArray<TSharedPtr<IDecider>> FourDummies()
	{
		TArray<TSharedPtr<IDecider>> Deciders;
		for (int32 i = 0; i < 4; ++i)
		{
			Deciders.Add(MakeShared<FDummyDecider>());
		}
		return Deciders;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationDeterminismTest,
	"Soulbound.Core.Simulation.DeterminismoPorSemilla",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationDeterminismTest::RunTest(const FString& Parameters)
{
	FSimulationParams Params;
	Params.Seed = 42;

	const FSimulationResult A = FMatchSimulation(Params, FourDummies()).Run();
	const FSimulationResult B = FMatchSimulation(Params, FourDummies()).Run();

	// R-SIM-2: misma semilla, mismo registro, línea a línea.
	TestEqual(TEXT("Dos ejecuciones con la misma semilla tienen el mismo numero de lineas"), A.Log.Num(), B.Log.Num());
	for (int32 i = 0; i < FMath::Min(A.Log.Num(), B.Log.Num()); ++i)
	{
		if (A.Log[i] != B.Log[i])
		{
			AddError(FString::Printf(TEXT("Divergencia en la linea %d: '%s' frente a '%s'"), i, *A.Log[i], *B.Log[i]));
			break;
		}
	}

	// Una partida de 40 minutos con turno cada 30 s tiene 80 turnos y 160 evaluaciones.
	TestEqual(TEXT("Turnos jugados"), A.Turns, 80);
	TestEqual(TEXT("Evaluaciones del Warden"), A.Evaluations, 160);
	TestTrue(TEXT("El Warden intervino alguna vez"), A.Interventions > 0);
	TestTrue(TEXT("El Warden no intervino siempre"), A.Interventions < A.Evaluations);

	// Otra semilla, otro registro.
	Params.Seed = 43;
	const FSimulationResult C = FMatchSimulation(Params, FourDummies()).Run();
	bool bDifferent = C.Log.Num() != A.Log.Num();
	for (int32 i = 0; !bDifferent && i < A.Log.Num(); ++i)
	{
		bDifferent = A.Log[i] != C.Log[i];
	}
	TestTrue(TEXT("Semillas distintas dan registros distintos"), bDifferent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchSimulationPerformanceTest,
	"Soulbound.Core.Simulation.CostePorPartida",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchSimulationPerformanceTest::RunTest(const FString& Parameters)
{
	FSimulationParams Params;
	Params.Seed = 7;

	// R-SIM-1: una partida en menos de un segundo. Con reglas de mentira sobra margen; el dato real llegará con el motor.
	const FSimulationResult One = FMatchSimulation(Params, FourDummies()).Run();
	const FString OneLine = FString::Printf(TEXT("Una partida de 40 min virtuales: %.4f s reales, %d lineas de registro"), One.WallSeconds, One.Log.Num());
	AddInfo(OneLine);
	UE_LOG(LogSoulboundTest, Display, TEXT("%s"), *OneLine);
	TestTrue(TEXT("Una partida tarda menos de un segundo"), One.WallSeconds < 1.0);

	double Total = 0.0;
	for (int32 i = 0; i < 100; ++i)
	{
		Params.Seed = 1000 + i;
		Total += FMatchSimulation(Params, FourDummies()).Run().WallSeconds;
	}
	const FString HundredLine = FString::Printf(TEXT("Cien partidas: %.3f s reales (%.4f s por partida)"), Total, Total / 100.0);
	AddInfo(HundredLine);
	UE_LOG(LogSoulboundTest, Display, TEXT("%s"), *HundredLine);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
