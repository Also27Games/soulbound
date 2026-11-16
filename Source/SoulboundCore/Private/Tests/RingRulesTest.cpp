// Tests de las reglas del anillo (S-026, GDD 09): precisión de un pulso (R-MIN-2), secuencia con semilla (R-MIN-3), parámetros de
// cada uso desde datos (R-MIN-6, R-MIN-8), desafío final sin fallos (R-MIN-12), resultado por media y rendimiento del duelo (R-MIN-4)
// y el bloque "ring" de partida.json, que se rechaza con motivo si está mal. Se conserva la numeración de criterios del GDD 09.
// Sin mundo ni editor; se lanzan con Scripts/test.ps1. Filtro: "Soulbound.Core.Ring".

#include "Match/MatchParams.h"
#include "Match/RingRules.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "MatchTestSetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Un bloque "ring" válido con un valor distinto en cada campo, para que un parámetro leído del campo equivocado se note (CA-MIN-3).
	 * No son los valores iniciales: esos están en partida.json.
	 */
	const TCHAR* const DistinctRing = TEXT(R"({
		"pulseGapMinSeconds": 0.5, "pulseGapMaxSeconds": 1.5,
		"easy":   { "closeSeconds": 1.7, "sweetZone": 0.31, "passThreshold": 0.41 },
		"normal": { "closeSeconds": 1.3, "sweetZone": 0.19, "passThreshold": 0.57 },
		"hard":   { "closeSeconds": 0.8, "sweetZone": 0.11, "passThreshold": 0.67 },
		"forgottenSourcePulses": 2, "wardenTrialPulses": 4, "voidEscapePulses": 5,
		"duelPulses": 3, "duelDifficulty": "hard",
		"finalChallengePulses": 7, "finalChallengeSweetZone": 0.23,
		"startWaitSeconds": 6.5
	})");

	TSharedPtr<FJsonObject> ParseObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
		return Object;
	}

	/**
	 * Carga los parámetros de partida.json con su bloque "ring" sustituido por Ring; sin Ring, sin el bloque. Así cada test prueba solo
	 * el anillo y el resto de parámetros siguen siendo los de verdad.
	 */
	FMatchParamsLoadResult LoadWithRing(const TSharedPtr<FJsonObject>& Ring)
	{
		FString Json;
		FFileHelper::LoadFileToString(Json, *Soulbound::Tests::MatchParamsPath());
		const TSharedPtr<FJsonObject> Root = ParseObject(Json);
		if (!Root.IsValid())
		{
			FMatchParamsLoadResult Failed;
			Failed.Error = EMatchParamsError::FileNotFound;
			Failed.Message = TEXT("No se pudo leer partida.json para el test.");
			return Failed;
		}
		Root->RemoveField(TEXT("ring"));
		if (Ring.IsValid())
		{
			Root->SetObjectField(TEXT("ring"), Ring);
		}
		FString Out;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Out));
		return Soulbound::Rules::LoadMatchParamsFromJsonString(Out);
	}

	/** Los parámetros con el bloque DistinctRing. */
	FMatchParamsLoadResult LoadDistinct()
	{
		return LoadWithRing(ParseObject(DistinctRing));
	}

	/** Lista de precisiones, para escribir los casos en una línea. */
	TArray<double> Precisions(std::initializer_list<double> Values)
	{
		return TArray<double>(Values);
	}
}

// CA-MIN-1, R-MIN-2: con zona buena Z = 0,2 s, la precisión es 1 − d / (Z / 2), con mínimo 0; en el borde exacto, 0; fuera, 0; sin
// pulsación, 0. Simétrica: igual antes que después de la coincidencia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingPrecisionTest, "Soulbound.Core.Ring.PrecisionDeUnPulso",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingPrecisionTest::RunTest(const FString& Parameters)
{
	using Soulbound::Rules::PulsePrecision;
	const double Coincidence = 10.0;
	const double Zone = 0.2;
	TestEqual(TEXT("En la coincidencia exacta, 1"), PulsePrecision(Coincidence, 10.0, Zone), 1.0, 1e-9);
	TestEqual(TEXT("A 0,05 s después (un cuarto de la zona), 0,5"), PulsePrecision(Coincidence, 10.05, Zone), 0.5, 1e-9);
	TestEqual(TEXT("A 0,05 s antes, también 0,5"), PulsePrecision(Coincidence, 9.95, Zone), 0.5, 1e-9);
	TestEqual(TEXT("A 0,02 s, 0,8"), PulsePrecision(Coincidence, 10.02, Zone), 0.8, 1e-9);
	TestEqual(TEXT("En el borde exacto (0,1 s), 0"), PulsePrecision(Coincidence, 10.1, Zone), 0.0, 1e-9);
	TestEqual(TEXT("En el borde de antes, 0"), PulsePrecision(Coincidence, 9.9, Zone), 0.0, 1e-9);
	TestEqual(TEXT("Fuera de la zona, 0"), PulsePrecision(Coincidence, 10.15, Zone), 0.0, 1e-9);
	TestEqual(TEXT("Muy lejos, 0"), PulsePrecision(Coincidence, 3.0, Zone), 0.0, 1e-9);
	TestEqual(TEXT("Sin pulsación, 0"), PulsePrecision(Coincidence, TOptional<double>(), Zone), 0.0, 1e-9);
	TestEqual(TEXT("Con zona no positiva, 0 aunque coincida"), PulsePrecision(Coincidence, 10.0, 0.0), 0.0, 1e-9);
	return true;
}

// CA-MIN-2, R-MIN-3: la misma semilla, dificultad y número de pulsos dan dos veces la misma secuencia; 50 semillas distintas dan al
// menos dos secuencias distintas; todos los huecos (del final de la zona buena de un pulso al inicio del siguiente, decisión del autor
// del 06/11/2026) están dentro del rango de datos.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingSequenceTest, "Soulbound.Core.Ring.SecuenciaConSemilla",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingSequenceTest::RunTest(const FString& Parameters)
{
	const FMatchParamsLoadResult Loaded = LoadDistinct();
	if (!TestTrue(FString::Printf(TEXT("Los parámetros de prueba cargan (%s)"), *Loaded.Message), Loaded.IsSuccess()))
	{
		return false;
	}
	const FRingParams& Ring = Loaded.Params.Ring;
	const TOptional<FRingTrialParams> Trial = Soulbound::Rules::RingParamsForTrial(Ring, ESkillTrial::VoidEscape, ESkillDifficulty::Normal);
	if (!TestTrue(TEXT("El escape tiene parámetros"), Trial.IsSet()))
	{
		return false;
	}

	const FRingSequence First = Soulbound::Rules::MakeRingSequence(Ring, *Trial, 1234);
	const FRingSequence Again = Soulbound::Rules::MakeRingSequence(Ring, *Trial, 1234);
	TestEqual(TEXT("Tantos pulsos como dice el uso"), First.Num(), Trial->Pulses);
	TestTrue(TEXT("La misma semilla da la misma secuencia"), First.PulseStarts == Again.PulseStarts);
	TestEqual(TEXT("El cierre es el del uso"), First.CloseSeconds, Trial->CloseSeconds, 1e-9);
	TestEqual(TEXT("La zona en segundos es la del uso"), First.SweetZoneSeconds, Trial->SweetZoneSeconds(), 1e-9);

	TSet<FString> Distinct;
	bool bAllGapsInRange = true;
	bool bAllStartAtZero = true;
	for (int32 Seed = 0; Seed < 50; ++Seed)
	{
		const FRingSequence Sequence = Soulbound::Rules::MakeRingSequence(Ring, *Trial, Seed);
		FString Key;
		for (int32 Pulse = 0; Pulse < Sequence.Num(); ++Pulse)
		{
			Key += FString::Printf(TEXT("%.6f;"), Sequence.PulseStarts[Pulse]);
			if (Pulse > 0)
			{
				const double Gap = Sequence.PulseStarts[Pulse] - Sequence.ExpiryAt(Pulse - 1);
				bAllGapsInRange &= Gap >= Ring.PulseGapMinSeconds - 1e-9 && Gap <= Ring.PulseGapMaxSeconds + 1e-9;
			}
		}
		bAllStartAtZero &= Sequence.Num() > 0 && Sequence.PulseStarts[0] == 0.0;
		Distinct.Add(Key);
	}
	TestTrue(TEXT("En 50 semillas hay al menos dos secuencias distintas"), Distinct.Num() >= 2);
	TestTrue(TEXT("Todos los huecos están entre el mínimo y el máximo de datos"), bAllGapsInRange);
	TestTrue(TEXT("Todas empiezan con un pulso en 0"), bAllStartAtZero);
	return true;
}

// CA-MIN-3, R-MIN-6, R-MIN-8: para cada uso, cierre, zona buena, pulsos y umbral salen de datos; el tipo de criterio es regla
// (precisión de la story del autor del 06/11/2026). Cambiar un valor en el JSON de prueba cambia el resultado.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingParamsFromDataTest, "Soulbound.Core.Ring.ParametrosDeDatos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingParamsFromDataTest::RunTest(const FString& Parameters)
{
	const FMatchParamsLoadResult Loaded = LoadDistinct();
	if (!TestTrue(FString::Printf(TEXT("Los parámetros de prueba cargan (%s)"), *Loaded.Message), Loaded.IsSuccess()))
	{
		return false;
	}
	const FRingParams& Ring = Loaded.Params.Ring;

	struct FCase
	{
		const TCHAR* Name;
		ESkillTrial Trial;
		ESkillDifficulty Difficulty;
		double Close;
		double Zone;
		int32 Pulses;
		double Threshold;
		ERingCriterion Criterion;
	};
	const FCase Cases[] = {
		{TEXT("Fuente de un Olvidado en fácil"), ESkillTrial::ForgottenSource, ESkillDifficulty::Easy, 1.7, 0.31, 2, 0.41, ERingCriterion::MeanAtLeastThreshold},
		{TEXT("Prueba del Warden en normal"), ESkillTrial::WardenTrial, ESkillDifficulty::Normal, 1.3, 0.19, 4, 0.57, ERingCriterion::MeanAtLeastThreshold},
		{TEXT("Prueba del Warden en difícil"), ESkillTrial::WardenTrial, ESkillDifficulty::Hard, 0.8, 0.11, 4, 0.67, ERingCriterion::MeanAtLeastThreshold},
		{TEXT("Escape del Vacío en difícil"), ESkillTrial::VoidEscape, ESkillDifficulty::Hard, 0.8, 0.11, 5, 0.67, ERingCriterion::MeanAtLeastThreshold},
		{TEXT("Escape del Vacío en fácil"), ESkillTrial::VoidEscape, ESkillDifficulty::Easy, 1.7, 0.31, 5, 0.41, ERingCriterion::MeanAtLeastThreshold},
		{TEXT("Desafío final en normal: cierre de normal, zona propia"), ESkillTrial::FinalChallenge, ESkillDifficulty::Normal, 1.3, 0.23, 7, 0.57, ERingCriterion::NoMiss},
	};
	for (const FCase& Case : Cases)
	{
		const TOptional<FRingTrialParams> Trial = Soulbound::Rules::RingParamsForTrial(Ring, Case.Trial, Case.Difficulty);
		if (!TestTrue(FString::Printf(TEXT("%s: hay parámetros"), Case.Name), Trial.IsSet()))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s: dificultad"), Case.Name), Trial->Difficulty == Case.Difficulty);
		TestEqual(FString::Printf(TEXT("%s: cierre"), Case.Name), Trial->CloseSeconds, Case.Close, 1e-9);
		TestEqual(FString::Printf(TEXT("%s: zona buena"), Case.Name), Trial->SweetZone, Case.Zone, 1e-9);
		TestEqual(FString::Printf(TEXT("%s: pulsos"), Case.Name), Trial->Pulses, Case.Pulses);
		TestEqual(FString::Printf(TEXT("%s: umbral"), Case.Name), Trial->PassThreshold, Case.Threshold, 1e-9);
		TestTrue(FString::Printf(TEXT("%s: criterio"), Case.Name), Trial->Criterion == Case.Criterion);
	}
	TestFalse(TEXT("Sin prueba pendiente no hay parámetros"), Soulbound::Rules::RingParamsForTrial(Ring, ESkillTrial::None, ESkillDifficulty::Normal).IsSet());

	const FRingTrialParams Duel = Soulbound::Rules::RingParamsForDuel(Ring);
	TestTrue(TEXT("Duelo: la dificultad de datos (difícil)"), Duel.Difficulty == ESkillDifficulty::Hard);
	TestEqual(TEXT("Duelo: cierre de su dificultad"), Duel.CloseSeconds, 0.8, 1e-9);
	TestEqual(TEXT("Duelo: zona de su dificultad"), Duel.SweetZone, 0.11, 1e-9);
	TestEqual(TEXT("Duelo: pulsos"), Duel.Pulses, 3);
	TestTrue(TEXT("Duelo: el criterio es el rendimiento"), Duel.Criterion == ERingCriterion::Performance);
	// S-042 (R-MIN-19): la espera para empezar sale del JSON.
	TestEqual(TEXT("Espera para empezar: la del JSON (6,5 s)"), Ring.StartWaitSeconds, 6.5, 1e-9);

	// Cambiar un valor en el JSON cambia el resultado: 0,6 / 0,6 / 0,6 supera el umbral de normal (0,57) y no uno de 0,7.
	const TArray<double> Steady = Precisions({0.6, 0.6, 0.6});
	const TOptional<FRingTrialParams> Before = Soulbound::Rules::RingParamsForTrial(Ring, ESkillTrial::WardenTrial, ESkillDifficulty::Normal);
	const TSharedPtr<FJsonObject> Changed = ParseObject(DistinctRing);
	Changed->GetObjectField(TEXT("normal"))->SetNumberField(TEXT("passThreshold"), 0.7);
	const FMatchParamsLoadResult Reloaded = LoadWithRing(Changed);
	if (!TestTrue(FString::Printf(TEXT("El JSON cambiado carga (%s)"), *Reloaded.Message), Reloaded.IsSuccess()) || !Before.IsSet())
	{
		return false;
	}
	const TOptional<FRingTrialParams> After = Soulbound::Rules::RingParamsForTrial(Reloaded.Params.Ring, ESkillTrial::WardenTrial, ESkillDifficulty::Normal);
	TestTrue(TEXT("Con el umbral 0,57, 0,6 / 0,6 / 0,6 supera"), Soulbound::Rules::RingTrialPassed(*Before, Steady));
	TestTrue(TEXT("Con el umbral cambiado a 0,7 en el JSON, falla"), After.IsSet() && !Soulbound::Rules::RingTrialPassed(*After, Steady));
	return true;
}

// CA-MIN-6 en parte, R-MIN-12: con los datos de partida.json, el desafío final tiene seis pulsos, el cierre de normal y su zona propia;
// con las seis precisiones mayores que 0 se supera; con una sola en 0, se falla. Sin trampas (Fase 2).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingFinalChallengeTest, "Soulbound.Core.Ring.DesafioFinalSinFallos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingFinalChallengeTest::RunTest(const FString& Parameters)
{
	FMatchParams Params;
	if (!Soulbound::Tests::LoadTestMatchParams(*this, Params))
	{
		return false;
	}
	const TOptional<FRingTrialParams> Final = Soulbound::Rules::RingParamsForTrial(Params.Ring, ESkillTrial::FinalChallenge, Params.FinalChallengeDifficulty);
	if (!TestTrue(TEXT("El desafío final tiene parámetros"), Final.IsSet()))
	{
		return false;
	}
	TestEqual(TEXT("Seis pulsos"), Final->Pulses, 6);
	TestTrue(TEXT("La dificultad de datos del desafío final es normal"), Params.FinalChallengeDifficulty == ESkillDifficulty::Normal);
	TestEqual(TEXT("El cierre es el de normal"), Final->CloseSeconds, Params.Ring.Normal.CloseSeconds, 1e-9);
	TestEqual(TEXT("La zona buena es la propia del desafío"), Final->SweetZone, Params.Ring.FinalChallengeSweetZone, 1e-9);
	TestTrue(TEXT("Su zona propia no es la de normal"), !FMath::IsNearlyEqual(Final->SweetZone, Params.Ring.Normal.SweetZone));
	TestTrue(TEXT("Criterio sin fallos"), Final->Criterion == ERingCriterion::NoMiss);

	TestTrue(TEXT("Seis precisiones mayores que 0, aunque bajas: superado"),
		Soulbound::Rules::RingTrialPassed(*Final, Precisions({0.05, 0.9, 0.1, 1.0, 0.2, 0.01})));
	TestFalse(TEXT("Una sola en 0, con las demás perfectas: fallado"),
		Soulbound::Rules::RingTrialPassed(*Final, Precisions({1.0, 1.0, 1.0, 0.0, 1.0, 1.0})));
	return true;
}

// CA-MIN-12 (nuevo), R-MIN-4: con umbral 0,55, 0,6 / 0,5 / 0,55 superan (media 0,55, con la tolerancia de coma flotante); 0,6 / 0,5 /
// 0,5 fallan. En duelo, el rendimiento de 1 / 0,5 / 0 es 0,5.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingMeanResultTest, "Soulbound.Core.Ring.ResultadoPorMedia",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingMeanResultTest::RunTest(const FString& Parameters)
{
	FRingTrialParams Trial;
	Trial.CloseSeconds = 1.2;
	Trial.SweetZone = 0.18;
	Trial.Pulses = 3;
	Trial.PassThreshold = 0.55;
	Trial.Criterion = ERingCriterion::MeanAtLeastThreshold;
	TestTrue(TEXT("0,6 / 0,5 / 0,55: media 0,55, supera"), Soulbound::Rules::RingTrialPassed(Trial, Precisions({0.6, 0.5, 0.55})));
	TestFalse(TEXT("0,6 / 0,5 / 0,5: media 0,533, falla"), Soulbound::Rules::RingTrialPassed(Trial, Precisions({0.6, 0.5, 0.5})));
	TestFalse(TEXT("Sin precisiones, falla"), Soulbound::Rules::RingTrialPassed(Trial, TArray<double>()));

	TestEqual(TEXT("Duelo: 1 / 0,5 / 0 rinde 0,5"), Soulbound::Rules::RingDuelPerformance(Precisions({1.0, 0.5, 0.0})), 0.5, 1e-9);
	TestEqual(TEXT("Duelo: tres aciertos perfectos rinden 1"), Soulbound::Rules::RingDuelPerformance(Precisions({1.0, 1.0, 1.0})), 1.0, 1e-9);
	TestEqual(TEXT("Duelo: sin precisiones rinde 0"), Soulbound::Rules::RingDuelPerformance(TArray<double>()), 0.0, 1e-9);
	return true;
}

// CA-MIN-13 (nuevo), datos: el cargador rechaza, con motivo, un bloque del anillo ausente, un cierre o una zona buena que no sean
// positivos, una zona buena mayor que 1, un umbral fuera de 0 a 1 y un número de pulsos menor que 1. Además (decisión del autor del
// 06/11/2026), huecos con mínimo no positivo o máximo menor que el mínimo, y una dificultad del duelo desconocida. Los bordes valen.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRingDataRejectedTest, "Soulbound.Core.Ring.DatosRechazados",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRingDataRejectedTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("El bloque de prueba es válido"), LoadDistinct().IsSuccess());

	const FMatchParamsLoadResult Missing = LoadWithRing(nullptr);
	TestFalse(TEXT("Sin bloque del anillo: rechazado"), Missing.IsSuccess());
	TestFalse(TEXT("Sin bloque del anillo: con motivo"), Missing.Message.IsEmpty());

	// Cada caso: sección ("" para el propio bloque), campo y valor JSON; un valor nulo quita el campo.
	struct FCase
	{
		const TCHAR* Name;
		const TCHAR* Section;
		const TCHAR* Field;
		TSharedPtr<FJsonValue> Value;
		bool bAccepted;
	};
	auto Number = [](double Value) { return MakeShared<FJsonValueNumber>(Value); };
	const FCase Cases[] = {
		{TEXT("cierre 0 en fácil"), TEXT("easy"), TEXT("closeSeconds"), Number(0.0), false},
		{TEXT("cierre negativo en normal"), TEXT("normal"), TEXT("closeSeconds"), Number(-1.0), false},
		{TEXT("zona buena 0 en difícil"), TEXT("hard"), TEXT("sweetZone"), Number(0.0), false},
		{TEXT("zona buena negativa en fácil"), TEXT("easy"), TEXT("sweetZone"), Number(-0.1), false},
		{TEXT("zona buena mayor que 1 en normal"), TEXT("normal"), TEXT("sweetZone"), Number(1.2), false},
		{TEXT("zona buena exactamente 1"), TEXT("normal"), TEXT("sweetZone"), Number(1.0), true},
		{TEXT("zona propia del desafío final 0"), TEXT(""), TEXT("finalChallengeSweetZone"), Number(0.0), false},
		{TEXT("zona propia del desafío final mayor que 1"), TEXT(""), TEXT("finalChallengeSweetZone"), Number(1.5), false},
		{TEXT("umbral negativo"), TEXT("normal"), TEXT("passThreshold"), Number(-0.1), false},
		{TEXT("umbral mayor que 1"), TEXT("hard"), TEXT("passThreshold"), Number(1.1), false},
		{TEXT("umbral 0"), TEXT("easy"), TEXT("passThreshold"), Number(0.0), true},
		{TEXT("umbral 1"), TEXT("hard"), TEXT("passThreshold"), Number(1.0), true},
		{TEXT("Fuente con 0 pulsos"), TEXT(""), TEXT("forgottenSourcePulses"), Number(0.0), false},
		{TEXT("Prueba del Warden con 0 pulsos"), TEXT(""), TEXT("wardenTrialPulses"), Number(0.0), false},
		{TEXT("escape con -1 pulsos"), TEXT(""), TEXT("voidEscapePulses"), Number(-1.0), false},
		{TEXT("duelo con 0 pulsos"), TEXT(""), TEXT("duelPulses"), Number(0.0), false},
		{TEXT("desafío final con 0 pulsos"), TEXT(""), TEXT("finalChallengePulses"), Number(0.0), false},
		{TEXT("pulsos con decimales"), TEXT(""), TEXT("duelPulses"), Number(2.5), false},
		{TEXT("un pulso"), TEXT(""), TEXT("forgottenSourcePulses"), Number(1.0), true},
		{TEXT("hueco mínimo 0"), TEXT(""), TEXT("pulseGapMinSeconds"), Number(0.0), false},
		{TEXT("hueco máximo menor que el mínimo"), TEXT(""), TEXT("pulseGapMaxSeconds"), Number(0.4), false},
		{TEXT("hueco máximo igual al mínimo"), TEXT(""), TEXT("pulseGapMaxSeconds"), Number(0.5), true},
		{TEXT("dificultad del duelo desconocida"), TEXT(""), TEXT("duelDifficulty"), MakeShared<FJsonValueString>(TEXT("imposible")), false},
		{TEXT("sin dificultad del duelo"), TEXT(""), TEXT("duelDifficulty"), nullptr, false},
		{TEXT("sin la dificultad difícil"), TEXT(""), TEXT("hard"), nullptr, false},
		{TEXT("cierre que no es número"), TEXT("easy"), TEXT("closeSeconds"), MakeShared<FJsonValueString>(TEXT("1.6")), false},
		// S-042 (R-MIN-19): la espera para empezar, no menor que cero; con 0 empieza al abrirse.
		{TEXT("espera para empezar negativa"), TEXT(""), TEXT("startWaitSeconds"), Number(-1.0), false},
		{TEXT("espera para empezar 0"), TEXT(""), TEXT("startWaitSeconds"), Number(0.0), true},
		{TEXT("sin espera para empezar"), TEXT(""), TEXT("startWaitSeconds"), nullptr, false},
	};
	for (const FCase& Case : Cases)
	{
		const TSharedPtr<FJsonObject> Ring = ParseObject(DistinctRing);
		const TSharedPtr<FJsonObject> Target = *Case.Section ? Ring->GetObjectField(Case.Section) : Ring;
		if (Case.Value.IsValid())
		{
			Target->SetField(Case.Field, Case.Value);
		}
		else
		{
			Target->RemoveField(Case.Field);
		}
		const FMatchParamsLoadResult Result = LoadWithRing(Ring);
		if (Case.bAccepted)
		{
			TestTrue(FString::Printf(TEXT("%s: se acepta (%s)"), Case.Name, *Result.Message), Result.IsSuccess());
		}
		else
		{
			TestFalse(FString::Printf(TEXT("%s: se rechaza"), Case.Name), Result.IsSuccess());
			TestTrue(FString::Printf(TEXT("%s: con motivo"), Case.Name), Result.Error == EMatchParamsError::MalformedData && !Result.Message.IsEmpty());
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
