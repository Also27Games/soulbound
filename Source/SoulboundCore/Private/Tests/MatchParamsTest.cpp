// Tests de los parámetros de partida (S-003, criterio "Parámetros"): el reloj de turno (R-TUR-8) y los topes de
// turnos extra y perdidos (R-TUR-9, R-TUR-10) salen de Content/Data/Match/partida.json; un archivo mal formado se
// rechaza con motivo, igual que el tablero. Filtro: "Soulbound.Core.Match.Params".

#include "Match/MatchParams.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchParamsLoadTest,
	"Soulbound.Core.Match.Params.SalenDelArchivo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchParamsLoadTest::RunTest(const FString& Parameters)
{
	// Criterio "Parámetros": el archivo de datos del proyecto carga y es válido.
	FMatchParams FromFile;
	if (!Soulbound::Tests::LoadTestMatchParams(*this, FromFile))
	{
		return false;
	}
	TestTrue(TEXT("El reloj de turno del archivo es positivo"), FromFile.TurnClockSeconds > 0.0);

	// Los valores salen del texto, no del código: con otros valores en el JSON, otros parámetros.
	const FMatchParamsLoadResult Other = Soulbound::Rules::LoadMatchParamsFromJsonString(
		TEXT("{ \"turnClockSeconds\": 12.5, \"maxExtraTurns\": 3, \"maxLostTurns\": 0 }"));
	if (TestTrue(FString::Printf(TEXT("Otros valores cargan (%s)"), *Other.Message), Other.IsSuccess()))
	{
		TestEqual(TEXT("Reloj de turno leído del texto"), Other.Params.TurnClockSeconds, 12.5);
		TestEqual(TEXT("Tope de extra leído del texto"), Other.Params.MaxExtraTurns, 3);
		TestEqual(TEXT("Tope de perdidos leído del texto"), Other.Params.MaxLostTurns, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchParamsRejectTest,
	"Soulbound.Core.Match.Params.RechazaMalFormado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchParamsRejectTest::RunTest(const FString& Parameters)
{
	// Criterio "Parámetros": un archivo mal formado se rechaza entero, con MalformedData y un motivo legible.
	const TPair<const TCHAR*, const TCHAR*> Cases[] = {
		{TEXT("no es JSON"), TEXT("reloj: 30")},
		{TEXT("falta el reloj"), TEXT("{ \"maxExtraTurns\": 2, \"maxLostTurns\": 2 }")},
		{TEXT("falta el tope de extra"), TEXT("{ \"turnClockSeconds\": 30, \"maxLostTurns\": 2 }")},
		{TEXT("falta el tope de perdidos"), TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2 }")},
		{TEXT("reloj como texto"), TEXT("{ \"turnClockSeconds\": \"30\", \"maxExtraTurns\": 2, \"maxLostTurns\": 2 }")},
		{TEXT("reloj a cero"), TEXT("{ \"turnClockSeconds\": 0, \"maxExtraTurns\": 2, \"maxLostTurns\": 2 }")},
		{TEXT("reloj negativo"), TEXT("{ \"turnClockSeconds\": -5, \"maxExtraTurns\": 2, \"maxLostTurns\": 2 }")},
		{TEXT("tope de extra negativo"), TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": -1, \"maxLostTurns\": 2 }")},
		{TEXT("tope de perdidos con decimales"), TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 1.5 }")},
	};
	for (const TPair<const TCHAR*, const TCHAR*>& Case : Cases)
	{
		const FMatchParamsLoadResult Result = Soulbound::Rules::LoadMatchParamsFromJsonString(Case.Value);
		TestEqual(FString::Printf(TEXT("%s: se rechaza por datos mal formados"), Case.Key), static_cast<int32>(Result.Error), static_cast<int32>(EMatchParamsError::MalformedData));
		TestFalse(FString::Printf(TEXT("%s: con un motivo legible"), Case.Key), Result.Message.IsEmpty());
	}

	const FMatchParamsLoadResult Missing = Soulbound::Rules::LoadMatchParamsFromFile(
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Match/no_existe.json")));
	TestEqual(TEXT("Archivo inexistente: FileNotFound"), static_cast<int32>(Missing.Error), static_cast<int32>(EMatchParamsError::FileNotFound));
	TestFalse(TEXT("Archivo inexistente: con un motivo legible"), Missing.Message.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
