// Tests de los parámetros de partida (S-003, criterio "Parámetros"): el reloj de turno (R-TUR-8), los topes de
// turnos extra y perdidos (R-TUR-9, R-TUR-10), desde S-006 la duración de la carrera (R-CAR-1), desde S-009 el reloj de
// elección, desde S-010 los del trilero (R-ROB-2), desde S-012 el margen de Shuten-dōji en el duelo (R-ELE-2) y desde S-013
// las escaleras de escape del Vacío (R-VAC-2, R-VAC-6) y desde S-014 los de los efectos y eventos del Warden (R-EFE-2, R-EFE-8,
// R-EFE-18, R-COM-2, reloj corto) y desde S-015 la dificultad de la Prueba del Warden (R-EFE-10) y el rendimiento del Warden
// en su duelo, y desde S-020 la salvaguarda del movimiento, salen de Content/Data/Match/partida.json; un archivo mal formado se rechaza con motivo,
// igual que el tablero.
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

	/** Las dos escaleras del Vacío (S-013) con valores válidos, para añadir a un JSON de parámetros. */
	const TCHAR* const VoidFields = TEXT("\"voidEscapeLadder\": [\"hard\", \"normal\", \"easy\"], \"voidEscapeAffinityLadder\": [\"easy\"]");

	/** La escalera del Vacío de la Furia (S-016, decisión del autor del 20/10/2026) con un valor válido, para añadir a un JSON de parámetros. */
	const TCHAR* const FuryField = TEXT("\"furyEscapeLadder\": [\"hard\", \"hard\", \"normal\"]");

	/** Los seis de los efectos y eventos del Warden (S-014) con valores válidos, para añadir a un JSON de parámetros. */
	const TCHAR* const EventFields = TEXT("\"revealBracerRounds\": 1, \"markRounds\": 3, \"echoNightMaxRounds\": 8, \"dilemmaClockSeconds\": 8, ")
		TEXT("\"dilemmaClockPerOptionSeconds\": 3, \"shortChoiceClockSeconds\": 6");

	/** La salvaguarda del movimiento (S-020) con un valor válido, para añadir a un JSON de parámetros. */
	const TCHAR* const MoveField = TEXT("\"moveStepSeconds\": 2.5");

	/** Los dos de la Prueba del Warden y el duelo contra el Warden (S-015) con valores válidos, para añadir a un JSON de parámetros. */
	const TCHAR* const WardenFields = TEXT("\"wardenTrialDifficulty\": \"normal\", \"wardenDuelPerformance\": 0.75");

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
	 * El JSON Json con los parámetros del trilero (S-010), el margen del duelo (S-012), las escaleras del Vacío (S-013) y los de
	 * los eventos (S-014) válidos añadidos antes de la última llave: así los casos de S-003 a S-009 siguen probando solo lo que
	 * probaban.
	 */
	FString WithShell(const TCHAR* Json)
	{
		return Append(Append(Append(Append(Append(Append(Append(FString(Json), ShellFields), DuelField), VoidFields), EventFields), WardenFields), FuryField), MoveField);
	}

	/** Parámetros válidos de S-009, con el margen del duelo, las escaleras y los de los eventos y sin los del trilero, para los casos de S-010. */
	FString BaseWith(const TCHAR* ShellPart)
	{
		return Append(Append(Append(FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s, %s, %s%s%s }"),
			DuelField, VoidFields, EventFields, *ShellPart ? TEXT(", ") : TEXT(""), ShellPart), WardenFields), FuryField), MoveField);
	}

	/** Parámetros válidos hasta S-010, con las escaleras y los de los eventos, sin el margen del duelo y DuelPart en su lugar, para los casos de S-012. */
	FString DuelCase(const TCHAR* DuelPart)
	{
		const FString Base = Append(Append(Append(FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s, %s, %s }"),
			ShellFields, VoidFields, EventFields), WardenFields), FuryField), MoveField);
		return *DuelPart ? Append(Base, DuelPart) : Base;
	}

	/** Parámetros válidos hasta S-012 y los de los eventos, sin las escaleras del Vacío y VoidPart en su lugar, para los casos de S-013. */
	FString VoidCase(const TCHAR* VoidPart)
	{
		const FString Base = Append(Append(Append(FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s, %s, %s }"),
			ShellFields, DuelField, EventFields), WardenFields), FuryField), MoveField);
		return *VoidPart ? Append(Base, VoidPart) : Base;
	}

	/** Parámetros válidos hasta S-013 sin los de los eventos, y EventPart en su lugar, para los casos de S-014. */
	FString EventCase(const TCHAR* EventPart)
	{
		const FString Base = Append(Append(Append(FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s, %s, %s }"),
			ShellFields, DuelField, VoidFields), WardenFields), FuryField), MoveField);
		return *EventPart ? Append(Base, EventPart) : Base;
	}

	/** Parámetros válidos hasta S-014 y WardenPart en lugar de los de la Prueba y el duelo del Warden, para los casos de S-015. */
	FString WardenCase(const TCHAR* WardenPart)
	{
		const FString Base = Append(FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s, %s, %s, %s, %s }"),
			ShellFields, DuelField, VoidFields, EventFields, FuryField), MoveField);
		return *WardenPart ? Append(Base, WardenPart) : Base;
	}

	/** Parámetros válidos hasta S-015 y FuryPart en lugar de la escalera del Vacío de la Furia, para los casos de S-016. */
	FString FuryCase(const TCHAR* FuryPart)
	{
		const FString Base = Append(Append(FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s, %s, %s, %s }"),
			ShellFields, DuelField, VoidFields, EventFields), WardenFields), MoveField);
		return *FuryPart ? Append(Base, FuryPart) : Base;
	}

	/** Parámetros válidos hasta S-016 y MovePart en lugar de la salvaguarda del movimiento, para los casos de S-020. */
	FString MoveCase(const TCHAR* MovePart)
	{
		const FString Base = Append(Append(FString::Printf(TEXT("{ \"turnClockSeconds\": 30, \"maxExtraTurns\": 2, \"maxLostTurns\": 2, \"raceDurationSeconds\": 300, \"choiceClockSeconds\": 10, %s, %s, %s, %s }"),
			ShellFields, DuelField, VoidFields, EventFields), WardenFields), FuryField);
		return *MovePart ? Append(Base, MovePart) : Base;
	}

	/** Los seis de los eventos (S-014) válidos con Name cambiado a Value: para los casos que prueban uno de ellos. */
	FString EventFieldsWith(const TCHAR* Name, const TCHAR* Value)
	{
		const TPair<const TCHAR*, const TCHAR*> Fields[] = {
			{TEXT("revealBracerRounds"), TEXT("1")}, {TEXT("markRounds"), TEXT("3")}, {TEXT("echoNightMaxRounds"), TEXT("8")},
			{TEXT("dilemmaClockSeconds"), TEXT("8")}, {TEXT("dilemmaClockPerOptionSeconds"), TEXT("3")}, {TEXT("shortChoiceClockSeconds"), TEXT("6")},
		};
		TArray<FString> Parts;
		for (const TPair<const TCHAR*, const TCHAR*>& Field : Fields)
		{
			if (FCString::Strcmp(Field.Key, Name) == 0 && Value == nullptr)
			{
				continue; // el campo falta
			}
			Parts.Add(FString::Printf(TEXT("\"%s\": %s"), Field.Key, FCString::Strcmp(Field.Key, Name) == 0 ? Value : Field.Value));
		}
		return FString::Join(Parts, TEXT(", "));
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
	// S-013 (R-VAC-2, R-ELE-2): las escaleras del Vacío, con sus valores iniciales de la story.
	TestTrue(TEXT("Escalera de escape: difícil, normal, fácil"), FromFile.VoidEscapeLadder == TArray<ESkillDifficulty>{ESkillDifficulty::Hard, ESkillDifficulty::Normal, ESkillDifficulty::Easy});
	TestTrue(TEXT("Escalera de Mictlantecuhtli: fácil"), FromFile.VoidEscapeAffinityLadder == TArray<ESkillDifficulty>{ESkillDifficulty::Easy});
	// S-016 (decisión del autor del 20/10/2026): la escalera del Vacío de la Furia, con su valor inicial.
	TestTrue(TEXT("Escalera del Vacío de la Furia: difícil, difícil, normal"), FromFile.FuryEscapeLadder == TArray<ESkillDifficulty>{ESkillDifficulty::Hard, ESkillDifficulty::Hard, ESkillDifficulty::Normal});
	// S-014 (R-EFE-2, R-EFE-8, R-EFE-18, R-COM-2): los de los eventos del Warden, con sus valores iniciales de la story.
	TestEqual(TEXT("Brazal revelado: 1 ronda"), FromFile.RevealBracerRounds, 1);
	TestEqual(TEXT("Marcado: 3 rondas"), FromFile.MarkRounds, 3);
	TestEqual(TEXT("Tope de la Noche: 8 rondas"), FromFile.EchoNightMaxRounds, 8);
	TestEqual(TEXT("Reloj del dilema: 8 s"), FromFile.DilemmaClockSeconds, 8.0);
	TestEqual(TEXT("Incremento por opción: 3 s"), FromFile.DilemmaClockPerOptionSeconds, 3.0);
	TestEqual(TEXT("Reloj corto: 6 s"), FromFile.ShortChoiceClockSeconds, 6.0);
	// S-015 (R-EFE-10, Duelo contra el Warden): con sus valores iniciales de la story.
	TestEqual(TEXT("Prueba del Warden: normal"), static_cast<int32>(FromFile.WardenTrialDifficulty), static_cast<int32>(ESkillDifficulty::Normal));
	TestEqual(TEXT("Rendimiento del Warden en su duelo: 0,75"), FromFile.WardenDuelPerformance, 0.75);
	// S-020 (CA-MOV-3): la salvaguarda del movimiento, con su valor inicial (2,5 s desde la corrección del autor del 21/10/2026).
	TestEqual(TEXT("Salvaguarda del movimiento: 6 s"), FromFile.MoveStepSeconds, 6.0);

	// Los valores salen del texto, no del código: con otros valores en el JSON, otros parámetros.
	const FMatchParamsLoadResult Other = Soulbound::Rules::LoadMatchParamsFromJsonString(
		TEXT("{ \"turnClockSeconds\": 12.5, \"maxExtraTurns\": 3, \"maxLostTurns\": 0, \"raceDurationSeconds\": 45, \"choiceClockSeconds\": 7.5, ")
		TEXT("\"shellRevealSeconds\": 1.5, \"shellShuffleSeconds\": 3, \"shellSwapCount\": 11, \"shellPickSeconds\": 6.5, \"duelAdvantageMargin\": 0.4, ")
		TEXT("\"voidEscapeLadder\": [\"normal\", \"easy\"], \"voidEscapeAffinityLadder\": [\"hard\", \"normal\"], ")
		TEXT("\"revealBracerRounds\": 2, \"markRounds\": 4, \"echoNightMaxRounds\": 5, \"dilemmaClockSeconds\": 9.5, \"dilemmaClockPerOptionSeconds\": 0, \"shortChoiceClockSeconds\": 4.5, ")
		TEXT("\"wardenTrialDifficulty\": \"hard\", \"wardenDuelPerformance\": 0.4, \"furyEscapeLadder\": [\"normal\"], \"moveStepSeconds\": 1.25 }"));
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
		TestTrue(TEXT("Escalera de escape leída del texto"), Other.Params.VoidEscapeLadder == TArray<ESkillDifficulty>{ESkillDifficulty::Normal, ESkillDifficulty::Easy});
		TestTrue(TEXT("Escalera de la afinidad leída del texto"), Other.Params.VoidEscapeAffinityLadder == TArray<ESkillDifficulty>{ESkillDifficulty::Hard, ESkillDifficulty::Normal});
		TestEqual(TEXT("Rondas de Brazal revelado leídas del texto"), Other.Params.RevealBracerRounds, 2);
		TestEqual(TEXT("Rondas de marca leídas del texto"), Other.Params.MarkRounds, 4);
		TestEqual(TEXT("Tope de la Noche leído del texto"), Other.Params.EchoNightMaxRounds, 5);
		TestEqual(TEXT("Reloj del dilema leído del texto"), Other.Params.DilemmaClockSeconds, 9.5);
		TestEqual(TEXT("Incremento por opción leído del texto (0 vale)"), Other.Params.DilemmaClockPerOptionSeconds, 0.0);
		TestEqual(TEXT("Reloj corto leído del texto"), Other.Params.ShortChoiceClockSeconds, 4.5);
		TestEqual(TEXT("Dificultad de la Prueba leída del texto"), static_cast<int32>(Other.Params.WardenTrialDifficulty), static_cast<int32>(ESkillDifficulty::Hard));
		TestEqual(TEXT("Rendimiento del Warden leído del texto"), Other.Params.WardenDuelPerformance, 0.4);
		TestTrue(TEXT("Escalera de la Furia leída del texto"), Other.Params.FuryEscapeLadder == TArray<ESkillDifficulty>{ESkillDifficulty::Normal});
		TestEqual(TEXT("Salvaguarda del movimiento leída del texto"), Other.Params.MoveStepSeconds, 1.25);
	}

	// S-013 (decisión del autor del 08/10/2026): la principal de 1 a 3 peldaños; la de la afinidad, de 1 a los de la principal.
	for (const TCHAR* Ladders : {TEXT("\"voidEscapeLadder\": [\"easy\"], \"voidEscapeAffinityLadder\": [\"easy\"]"), TEXT("\"voidEscapeLadder\": [\"hard\", \"hard\", \"hard\"], \"voidEscapeAffinityLadder\": [\"normal\", \"easy\", \"easy\"]")})
	{
		const FMatchParamsLoadResult Edge = Soulbound::Rules::LoadMatchParamsFromJsonString(VoidCase(Ladders));
		TestTrue(FString::Printf(TEXT("%s carga (%s)"), Ladders, *Edge.Message), Edge.IsSuccess());
	}

	// S-016: la escalera de la Furia, de 1 a 3 peldaños, como la principal.
	for (const TCHAR* Fury : {TEXT("\"furyEscapeLadder\": [\"easy\"]"), TEXT("\"furyEscapeLadder\": [\"hard\", \"hard\", \"hard\"]")})
	{
		const FMatchParamsLoadResult Edge = Soulbound::Rules::LoadMatchParamsFromJsonString(FuryCase(Fury));
		TestTrue(FString::Printf(TEXT("%s carga (%s)"), Fury, *Edge.Message), Edge.IsSuccess());
	}

	// S-015: el rendimiento del Warden admite de 0 a 1, los dos incluidos; las tres dificultades.
	for (const TCHAR* Warden : {TEXT("\"wardenTrialDifficulty\": \"easy\", \"wardenDuelPerformance\": 0"),
		TEXT("\"wardenTrialDifficulty\": \"hard\", \"wardenDuelPerformance\": 1")})
	{
		const FMatchParamsLoadResult Edge = Soulbound::Rules::LoadMatchParamsFromJsonString(WardenCase(Warden));
		TestTrue(FString::Printf(TEXT("%s carga (%s)"), Warden, *Edge.Message), Edge.IsSuccess());
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
		// S-013 (R-VAC-2, R-VAC-6): las dos escaleras, obligatorias, listas de dificultades conocidas; la principal de 1 a 3
		// peldaños y la de la afinidad de 1 a los de la principal (decisión del autor del 08/10/2026).
		{TEXT("faltan las escaleras"), VoidCase(TEXT(""))},
		{TEXT("falta la escalera principal"), VoidCase(TEXT("\"voidEscapeAffinityLadder\": [\"easy\"]"))},
		{TEXT("falta la escalera de la afinidad"), VoidCase(TEXT("\"voidEscapeLadder\": [\"hard\", \"normal\", \"easy\"]"))},
		{TEXT("escalera principal vacía"), VoidCase(TEXT("\"voidEscapeLadder\": [], \"voidEscapeAffinityLadder\": [\"easy\"]"))},
		{TEXT("escalera principal de cuatro peldaños"), VoidCase(TEXT("\"voidEscapeLadder\": [\"hard\", \"hard\", \"normal\", \"easy\"], \"voidEscapeAffinityLadder\": [\"easy\"]"))},
		{TEXT("dificultad desconocida"), VoidCase(TEXT("\"voidEscapeLadder\": [\"hard\", \"imposible\", \"easy\"], \"voidEscapeAffinityLadder\": [\"easy\"]"))},
		{TEXT("dificultad que no es texto"), VoidCase(TEXT("\"voidEscapeLadder\": [\"hard\", 2, \"easy\"], \"voidEscapeAffinityLadder\": [\"easy\"]"))},
		{TEXT("escalera que no es lista"), VoidCase(TEXT("\"voidEscapeLadder\": \"hard\", \"voidEscapeAffinityLadder\": [\"easy\"]"))},
		{TEXT("escalera de la afinidad vacía"), VoidCase(TEXT("\"voidEscapeLadder\": [\"hard\", \"normal\", \"easy\"], \"voidEscapeAffinityLadder\": []"))},
		{TEXT("escalera de la afinidad más larga que la principal"), VoidCase(TEXT("\"voidEscapeLadder\": [\"hard\"], \"voidEscapeAffinityLadder\": [\"easy\", \"easy\"]"))},
		// S-014: los seis de los eventos, obligatorios; rondas enteras desde 1, reloj del dilema y reloj corto mayores que cero,
		// incremento por opción de cero en adelante.
		{TEXT("faltan los de los eventos"), EventCase(TEXT(""))},
		{TEXT("falta el Brazal revelado"), EventCase(*EventFieldsWith(TEXT("revealBracerRounds"), nullptr))},
		{TEXT("Brazal revelado a cero"), EventCase(*EventFieldsWith(TEXT("revealBracerRounds"), TEXT("0")))},
		{TEXT("falta la marca"), EventCase(*EventFieldsWith(TEXT("markRounds"), nullptr))},
		{TEXT("marca a cero"), EventCase(*EventFieldsWith(TEXT("markRounds"), TEXT("0")))},
		{TEXT("marca con decimales"), EventCase(*EventFieldsWith(TEXT("markRounds"), TEXT("2.5")))},
		{TEXT("falta el tope de la Noche"), EventCase(*EventFieldsWith(TEXT("echoNightMaxRounds"), nullptr))},
		{TEXT("tope de la Noche negativo"), EventCase(*EventFieldsWith(TEXT("echoNightMaxRounds"), TEXT("-1")))},
		{TEXT("falta el reloj del dilema"), EventCase(*EventFieldsWith(TEXT("dilemmaClockSeconds"), nullptr))},
		{TEXT("reloj del dilema a cero"), EventCase(*EventFieldsWith(TEXT("dilemmaClockSeconds"), TEXT("0")))},
		{TEXT("falta el incremento por opción"), EventCase(*EventFieldsWith(TEXT("dilemmaClockPerOptionSeconds"), nullptr))},
		{TEXT("incremento por opción negativo"), EventCase(*EventFieldsWith(TEXT("dilemmaClockPerOptionSeconds"), TEXT("-3")))},
		{TEXT("falta el reloj corto"), EventCase(*EventFieldsWith(TEXT("shortChoiceClockSeconds"), nullptr))},
		{TEXT("reloj corto a cero"), EventCase(*EventFieldsWith(TEXT("shortChoiceClockSeconds"), TEXT("0")))},
		{TEXT("reloj corto como texto"), EventCase(*EventFieldsWith(TEXT("shortChoiceClockSeconds"), TEXT("\"6\"")))},
		// S-015: la dificultad de la Prueba del Warden, una de las tres, y el rendimiento del Warden, de 0 a 1; los dos obligatorios.
		{TEXT("faltan los de la Prueba y el duelo del Warden"), WardenCase(TEXT(""))},
		{TEXT("falta la dificultad de la Prueba"), WardenCase(TEXT("\"wardenDuelPerformance\": 0.75"))},
		{TEXT("dificultad de la Prueba desconocida"), WardenCase(TEXT("\"wardenTrialDifficulty\": \"imposible\", \"wardenDuelPerformance\": 0.75"))},
		{TEXT("dificultad de la Prueba como número"), WardenCase(TEXT("\"wardenTrialDifficulty\": 1, \"wardenDuelPerformance\": 0.75"))},
		{TEXT("falta el rendimiento del Warden"), WardenCase(TEXT("\"wardenTrialDifficulty\": \"normal\""))},
		{TEXT("rendimiento del Warden negativo"), WardenCase(TEXT("\"wardenTrialDifficulty\": \"normal\", \"wardenDuelPerformance\": -0.1"))},
		{TEXT("rendimiento del Warden mayor que 1"), WardenCase(TEXT("\"wardenTrialDifficulty\": \"normal\", \"wardenDuelPerformance\": 1.5"))},
		{TEXT("rendimiento del Warden como texto"), WardenCase(TEXT("\"wardenTrialDifficulty\": \"normal\", \"wardenDuelPerformance\": \"0.75\""))},
		// S-016 (decisión del autor del 20/10/2026): la escalera del Vacío de la Furia, obligatoria y validada como la principal.
		{TEXT("falta la escalera de la Furia"), FuryCase(TEXT(""))},
		{TEXT("escalera de la Furia vacía"), FuryCase(TEXT("\"furyEscapeLadder\": []"))},
		{TEXT("escalera de la Furia de cuatro peldaños"), FuryCase(TEXT("\"furyEscapeLadder\": [\"hard\", \"hard\", \"normal\", \"easy\"]"))},
		{TEXT("escalera de la Furia con una dificultad desconocida"), FuryCase(TEXT("\"furyEscapeLadder\": [\"hard\", \"imposible\"]"))},
		{TEXT("escalera de la Furia que no es lista"), FuryCase(TEXT("\"furyEscapeLadder\": \"hard\""))},
		// S-020 (CA-MOV-3): la salvaguarda del movimiento, obligatoria y mayor que cero.
		{TEXT("falta la salvaguarda del movimiento"), MoveCase(TEXT(""))},
		{TEXT("salvaguarda del movimiento a cero"), MoveCase(TEXT("\"moveStepSeconds\": 0"))},
		{TEXT("salvaguarda del movimiento negativa"), MoveCase(TEXT("\"moveStepSeconds\": -2.5"))},
		{TEXT("salvaguarda del movimiento como texto"), MoveCase(TEXT("\"moveStepSeconds\": \"2.5\""))},
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
	const FMatchParamsLoadResult ValidVoid = Soulbound::Rules::LoadMatchParamsFromJsonString(VoidCase(VoidFields));
	TestTrue(FString::Printf(TEXT("La base con las escaleras del Vacío carga (%s)"), *ValidVoid.Message), ValidVoid.IsSuccess());
	const FMatchParamsLoadResult ValidEvents = Soulbound::Rules::LoadMatchParamsFromJsonString(EventCase(EventFields));
	TestTrue(FString::Printf(TEXT("La base con los de los eventos carga (%s)"), *ValidEvents.Message), ValidEvents.IsSuccess());
	const FMatchParamsLoadResult ValidWarden = Soulbound::Rules::LoadMatchParamsFromJsonString(WardenCase(WardenFields));
	TestTrue(FString::Printf(TEXT("La base con los de la Prueba y el duelo del Warden carga (%s)"), *ValidWarden.Message), ValidWarden.IsSuccess());
	const FMatchParamsLoadResult ValidFury = Soulbound::Rules::LoadMatchParamsFromJsonString(FuryCase(FuryField));
	TestTrue(FString::Printf(TEXT("La base con la escalera de la Furia carga (%s)"), *ValidFury.Message), ValidFury.IsSuccess());
	const FMatchParamsLoadResult ValidMove = Soulbound::Rules::LoadMatchParamsFromJsonString(MoveCase(MoveField));
	TestTrue(FString::Printf(TEXT("La base con la salvaguarda del movimiento carga (%s)"), *ValidMove.Message), ValidMove.IsSuccess());

	const FMatchParamsLoadResult Missing = Soulbound::Rules::LoadMatchParamsFromFile(
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Match/no_existe.json")));
	TestEqual(TEXT("Archivo inexistente: FileNotFound"), static_cast<int32>(Missing.Error), static_cast<int32>(EMatchParamsError::FileNotFound));
	TestFalse(TEXT("Archivo inexistente: con un motivo legible"), Missing.Message.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
