// Tests de los parámetros de partida (S-003, criterio "Parámetros"): el reloj de turno (R-TUR-8), los topes de
// turnos extra y perdidos (R-TUR-9, R-TUR-10), desde S-006 la duración de la carrera (R-CAR-1), desde S-009 el reloj de
// elección, desde S-010 los del trilero (R-ROB-2) y desde S-012 el margen de Shuten-dōji en el duelo (R-ELE-2) salen de
// Content/Data/Match/partida.json; un archivo mal formado se rechaza con motivo, igual que el tablero.
// Filtro: "Soulbound.Core.Match.Params".

#include "Match/MatchParams.h"
#include "Misc/AutomationTest.h"
#include "MatchTestSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Los cuatro parámetros del trilero (S-010) con valores válidos, para añadir a un JSON de parámetros. */
	const TCHAR* const ShellFields = TEXT("\"shellRevealSeconds\": 2, \"shellShuffleSeconds\": 4, \"shellSwapCount\": 8, \"shellPickSeconds\": 5");

	/** El margen de Shuten-dōji (S-012) con un valor válido, para añadir a un JSON de parámetros. */
	const TCHAR* const DuelField = TEXT("\"duelAdvantageMargin\": 0.15");

	/** El JSON Json con Fields añadidos antes de la última llave. Un texto sin llave de cierre se devuelve tal cual. */
	FString Append(const FString& Json, const TCHAR* Fields)
	{
		const int32 Close = Json.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Close == INDEX_NONE)
		{
			return Json;
		}
		return Json.Left(Close).TrimEnd() + TEXT(", ") + Fields + TEXT(" }");
	}

	/**
	 * El JSON Json con los parámetros del trilero (S-010) y el margen del duelo (S-012) válidos añadidos antes de la última
	 * llave: así los casos de S-003 a S-009 siguen probando solo lo que probaban.
	 */
	FString WithShell(const TCHAR* Json)
	{
		return Append(Append(FString(Json), ShellFields), DuelField);
	}

	/** Parámetros válidos de S-009, con el margen del duelo y sin los del trilero, para construir los casos de S-010. */
	FString BaseWith(const TCHAR* ShellPart)
	{
		return FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s%s%s }"),
			DuelField, *ShellPart ? TEXT(", ") : TEXT(""), ShellPart);
	}

	/** Parámetros válidos hasta S-010 sin el margen del duelo, y DuelPart en su lugar, para los casos de S-012. */
	FString DuelCase(const TCHAR* DuelPart)
	{
		const FString Base = FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s }"), ShellFields);
		return *DuelPart ? Append(Base, DuelPart) : Base;
	}
}

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
	TestTrue(TEXT("La duración de la carrera del archivo es positiva (R-CAR-1)"), FromFile.RaceDurationSeconds > 0.0);
	TestTrue(TEXT("El reloj de elección del archivo es positivo (S-009)"), FromFile.ChoiceClockSeconds > 0.0);
	// S-010 (R-ROB-2): los del trilero, con sus valores iniciales de la story.
	TestEqual(TEXT("Muestra del trilero: 2 s"), FromFile.ShellRevealSeconds, 2.0);
	TestEqual(TEXT("Mezcla del trilero: 4 s"), FromFile.ShellShuffleSeconds, 4.0);
	TestEqual(TEXT("Intercambios de la mezcla: 8"), FromFile.ShellSwapCount, 8);
	TestEqual(TEXT("Tiempo de elección de esfera: 5 s"), FromFile.ShellPickSeconds, 5.0);
	// S-012 (R-ELE-2): el margen de Shuten-dōji, con su valor inicial de la story.
	TestEqual(TEXT("Margen de Shuten-dōji en el duelo: 0,15"), FromFile.DuelAdvantageMargin, 0.15);

	// Los valores salen del texto, no del código: con otros valores en el JSON, otros parámetros.
	const FMatchParamsLoadResult Other = Soulbound::Rules::LoadMatchParamsFromJsonString(
		TEXT("{ \"turnClockSeconds\": 12.5, \"maxExtraTurns\": 3, \"maxLostTurns\": 0, \"raceDurationSeconds\": 45, \"choiceClockSeconds\": 7.5, ")
		TEXT("\"shellRevealSeconds\": 1.5, \"shellShuffleSeconds\": 3, \"shellSwapCount\": 11, \"shellPickSeconds\": 6.5, \"duelAdvantageMargin\": 0.4 }"));
	if (TestTrue(FString::Printf(TEXT("Otros valores cargan (%s)"), *Other.Message), Other.IsSuccess()))
	{
		TestEqual(TEXT("Reloj de turno leído del texto"), Other.Params.TurnClockSeconds, 12.5);
		TestEqual(TEXT("Tope de extra leído del texto"), Other.Params.MaxExtraTurns, 3);
		TestEqual(TEXT("Tope de perdidos leído del texto"), Other.Params.MaxLostTurns, 0);
		TestEqual(TEXT("Duración de la carrera leída del texto"), Other.Params.RaceDurationSeconds, 45.0);
		TestEqual(TEXT("Reloj de elección leído del texto"), Other.Params.ChoiceClockSeconds, 7.5);
		TestEqual(TEXT("Muestra leída del texto"), Other.Params.ShellRevealSeconds, 1.5);
		TestEqual(TEXT("Mezcla leída del texto"), Other.Params.ShellShuffleSeconds, 3.0);
		TestEqual(TEXT("Intercambios leídos del texto"), Other.Params.ShellSwapCount, 11);
		TestEqual(TEXT("Tiempo de elección de esfera leído del texto"), Other.Params.ShellPickSeconds, 6.5);
		TestEqual(TEXT("Margen del duelo leído del texto"), Other.Params.DuelAdvantageMargin, 0.4);
	}

	// S-012: el margen admite de 0 a 1, el 1 excluido (decisión del autor del 06/10/2026).
	for (const TCHAR* Margin : {TEXT("\"duelAdvantageMargin\": 0"), TEXT("\"duelAdvantageMargin\": 0.99")})
	{
		const FMatchParamsLoadResult Edge = Soulbound::Rules::LoadMatchParamsFromJsonString(DuelCase(Margin));
		TestTrue(FString::Printf(TEXT("%s carga (%s)"), Margin, *Edge.Message), Edge.IsSuccess());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatchParamsRejectTest,
	"Soulbound.Core.Match.Params.RechazaMalFormado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchParamsRejectTest::RunTest(const FString& Parameters)
{
	// Criterio "Parámetros": un archivo mal formado se rechaza entero, con MalformedData y un motivo legible. Desde S-009
	// todos los casos llevan un reloj de elección válido, salvo los tres que lo prueban (choiceClockSeconds, con su validación).
	// Desde S-010 los casos anteriores llevan los parámetros del trilero válidos (WithShell), y los nuevos prueban cada uno.
	TArray<TPair<FString, FString>> Cases = {
		{TEXT("no es JSON"), TEXT("reloj: 30")},
		{TEXT("falta el reloj"), WithShell(TEXT("{ \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("falta el tope de extra"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("falta el tope de perdidos"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("reloj como texto"), WithShell(TEXT("{ \"turnClockSeconds\": \"30\", \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("reloj a cero"), WithShell(TEXT("{ \"turnClockSeconds\": 0, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("reloj negativo"), WithShell(TEXT("{ \"turnClockSeconds\": -5, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("tope de extra negativo"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": -1, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("tope de perdidos con decimales"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 1.5, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10 }"))},
		{TEXT("falta la duración de la carrera"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"choiceClockSeconds\": 10 }"))},
		{TEXT("duración de la carrera a cero"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 0, \"choiceClockSeconds\": 10 }"))},
		{TEXT("duración de la carrera como texto"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": \"300\", \"choiceClockSeconds\": 10 }"))},
		{TEXT("falta el reloj de elección"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300 }"))},
		{TEXT("reloj de elección a cero"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 0 }"))},
		{TEXT("reloj de elección como texto"), WithShell(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": \"10\" }"))},
		// S-010 (R-ROB-2): los cuatro del trilero, obligatorios; tiempos mayores que cero e intercambios enteros desde uno.
		{TEXT("faltan los del trilero"), BaseWith(TEXT(""))},
		{TEXT("falta la muestra"), BaseWith(TEXT("\"shellShuffleSeconds\": 4, \"shellSwapCount\": 8, \"shellPickSeconds\": 5"))},
		{TEXT("falta la mezcla"), BaseWith(TEXT("\"shellRevealSeconds\": 2, \"shellSwapCount\": 8, \"shellPickSeconds\": 5"))},
		{TEXT("faltan los intercambios"), BaseWith(TEXT("\"shellRevealSeconds\": 2, \"shellShuffleSeconds\": 4, \"shellPickSeconds\": 5"))},
		{TEXT("falta el tiempo de elección de esfera"), BaseWith(TEXT("\"shellRevealSeconds\": 2, \"shellShuffleSeconds\": 4, \"shellSwapCount\": 8"))},
		{TEXT("muestra a cero"), BaseWith(TEXT("\"shellRevealSeconds\": 0, \"shellShuffleSeconds\": 4, \"shellSwapCount\": 8, \"shellPickSeconds\": 5"))},
		{TEXT("mezcla negativa"), BaseWith(TEXT("\"shellRevealSeconds\": 2, \"shellShuffleSeconds\": -4, \"shellSwapCount\": 8, \"shellPickSeconds\": 5"))},
		{TEXT("intercambios a cero"), BaseWith(TEXT("\"shellRevealSeconds\": 2, \"shellShuffleSeconds\": 4, \"shellSwapCount\": 0, \"shellPickSeconds\": 5"))},
		{TEXT("intercambios con decimales"), BaseWith(TEXT("\"shellRevealSeconds\": 2, \"shellShuffleSeconds\": 4, \"shellSwapCount\": 8.5, \"shellPickSeconds\": 5"))},
		{TEXT("tiempo de elección de esfera a cero"), BaseWith(TEXT("\"shellRevealSeconds\": 2, \"shellShuffleSeconds\": 4, \"shellSwapCount\": 8, \"shellPickSeconds\": 0"))},
		{TEXT("muestra como texto"), BaseWith(TEXT("\"shellRevealSeconds\": \"2\", \"shellShuffleSeconds\": 4, \"shellSwapCount\": 8, \"shellPickSeconds\": 5"))},
		// S-012 (R-ELE-2): el margen de Shuten-dōji, obligatorio, de 0 a 1 con el 1 excluido.
		{TEXT("falta el margen del duelo"), DuelCase(TEXT(""))},
		{TEXT("margen del duelo negativo"), DuelCase(TEXT("\"duelAdvantageMargin\": -0.1"))},
		{TEXT("margen del duelo de 1"), DuelCase(TEXT("\"duelAdvantageMargin\": 1"))},
		{TEXT("margen del duelo mayor que 1"), DuelCase(TEXT("\"duelAdvantageMargin\": 1.5"))},
		{TEXT("margen del duelo como texto"), DuelCase(TEXT("\"duelAdvantageMargin\": \"0.15\""))},
	};
	for (const TPair<FString, FString>& Case : Cases)
	{
		const FMatchParamsLoadResult Result = Soulbound::Rules::LoadMatchParamsFromJsonString(Case.Value);
		TestEqual(FString::Printf(TEXT("%s: se rechaza por datos mal formados"), *Case.Key), static_cast<int32>(Result.Error), static_cast<int32>(EMatchParamsError::MalformedData));
		TestFalse(FString::Printf(TEXT("%s: con un motivo legible"), *Case.Key), Result.Message.IsEmpty());
	}
	// Control: la base con los del trilero válidos carga; así cada rechazo de arriba es por el campo que cambia.
	const FMatchParamsLoadResult Valid = Soulbound::Rules::LoadMatchParamsFromJsonString(BaseWith(ShellFields));
	TestTrue(FString::Printf(TEXT("La base con los del trilero carga (%s)"), *Valid.Message), Valid.IsSuccess());
	const FMatchParamsLoadResult ValidDuel = Soulbound::Rules::LoadMatchParamsFromJsonString(DuelCase(DuelField));
	TestTrue(FString::Printf(TEXT("La base con el margen del duelo carga (%s)"), *ValidDuel.Message), ValidDuel.IsSuccess());

	const FMatchParamsLoadResult Missing = Soulbound::Rules::LoadMatchParamsFromFile(
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Match/no_existe.json")));
	TestEqual(TEXT("Archivo inexistente: FileNotFound"), static_cast<int32>(Missing.Error), static_cast<int32>(EMatchParamsError::FileNotFound));
	TestFalse(TEXT("Archivo inexistente: con un motivo legible"), Missing.Message.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
