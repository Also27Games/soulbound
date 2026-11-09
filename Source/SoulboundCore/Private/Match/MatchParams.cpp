#include "Match/MatchParams.h"

#include "Data/JsonFields.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using Soulbound::JsonFields::ReadIntegerField;
using Soulbound::JsonFields::ReadNumberField;

namespace
{
	FMatchParamsLoadResult Reject(EMatchParamsError Error, const FString& Message)
	{
		FMatchParamsLoadResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}

	/** Nombres de las dificultades en el JSON, en el orden de ESkillDifficulty (S-013). */
	const TCHAR* const DifficultyNames[] = {TEXT("easy"), TEXT("normal"), TEXT("hard")};
	constexpr int32 DifficultyCount = UE_ARRAY_COUNT(DifficultyNames);

	/** Si Difficulty es uno de los valores de ESkillDifficulty. */
	bool IsKnownDifficulty(ESkillDifficulty Difficulty)
	{
		return static_cast<int32>(Difficulty) < DifficultyCount;
	}

	/** Lee el nombre de una dificultad ("easy", "normal", "hard"). Devuelve false si no es ninguna. */
	bool ParseDifficulty(const FString& Name, ESkillDifficulty& OutDifficulty)
	{
		for (int32 Candidate = 0; Candidate < DifficultyCount; ++Candidate)
		{
			if (Name == DifficultyNames[Candidate])
			{
				OutDifficulty = static_cast<ESkillDifficulty>(Candidate);
				return true;
			}
		}
		return false;
	}

	/**
	 * Lee la escalera Field del Vacío: una lista de textos "easy", "normal" o "hard" (S-013). Devuelve false si falta, no es
	 * una lista o tiene un elemento que no es una de esas dificultades; la longitud la comprueba ValidateMatchParams.
	 */
	bool ReadLadder(const FJsonObject& Root, const TCHAR* Field, TArray<ESkillDifficulty>& OutLadder)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root.TryGetArrayField(Field, Values) || !Values)
		{
			return false;
		}
		OutLadder.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Name;
			ESkillDifficulty Difficulty = ESkillDifficulty::Easy;
			if (!Value.IsValid() || !Value->TryGetString(Name) || !ParseDifficulty(Name, Difficulty))
			{
				return false;
			}
			OutLadder.Add(Difficulty);
		}
		return true;
	}

	/** Lee los tres números de una dificultad del anillo (S-026) del objeto Field de Ring. Devuelve el motivo si falla, o vacío. */
	FString ReadRingDifficulty(const FJsonObject& Ring, const TCHAR* Field, FRingDifficultyParams& Out)
	{
		const TSharedPtr<FJsonObject>* Tier = nullptr;
		if (!Ring.TryGetObjectField(Field, Tier) || !Tier || !Tier->IsValid())
		{
			return FString::Printf(TEXT("El parámetro \"ring.%s\" falta o no es un objeto."), Field);
		}
		const TPair<const TCHAR*, double*> Numbers[] = {
			{TEXT("closeSeconds"), &Out.CloseSeconds},
			{TEXT("sweetZone"), &Out.SweetZone},
			{TEXT("passThreshold"), &Out.PassThreshold},
		};
		for (const TPair<const TCHAR*, double*>& Number : Numbers)
		{
			if (!ReadNumberField(**Tier, Number.Key, *Number.Value))
			{
				return FString::Printf(TEXT("El parámetro \"ring.%s.%s\" falta o no es un número."), Field, Number.Key);
			}
		}
		return FString();
	}

	/** Lee el bloque "ring" del anillo (S-026) de Root. Devuelve el motivo si falta o tiene un campo mal, o vacío si se leyó entero. */
	FString ReadRing(const FJsonObject& Root, FRingParams& Out)
	{
		const TSharedPtr<FJsonObject>* RingObject = nullptr;
		if (!Root.TryGetObjectField(TEXT("ring"), RingObject) || !RingObject || !RingObject->IsValid())
		{
			return TEXT("S-026: el bloque del anillo \"ring\" falta o no es un objeto.");
		}
		const FJsonObject& Ring = **RingObject;
		const TPair<const TCHAR*, double*> Numbers[] = {
			{TEXT("pulseGapMinSeconds"), &Out.PulseGapMinSeconds},
			{TEXT("pulseGapMaxSeconds"), &Out.PulseGapMaxSeconds},
			{TEXT("finalChallengeSweetZone"), &Out.FinalChallengeSweetZone},
		};
		for (const TPair<const TCHAR*, double*>& Number : Numbers)
		{
			if (!ReadNumberField(Ring, Number.Key, *Number.Value))
			{
				return FString::Printf(TEXT("El parámetro \"ring.%s\" falta o no es un número."), Number.Key);
			}
		}
		const TPair<const TCHAR*, int32*> Counts[] = {
			{TEXT("forgottenSourcePulses"), &Out.ForgottenSourcePulses},
			{TEXT("wardenTrialPulses"), &Out.WardenTrialPulses},
			{TEXT("voidEscapePulses"), &Out.VoidEscapePulses},
			{TEXT("duelPulses"), &Out.DuelPulses},
			{TEXT("finalChallengePulses"), &Out.FinalChallengePulses},
		};
		for (const TPair<const TCHAR*, int32*>& Count : Counts)
		{
			if (!ReadIntegerField(Ring, Count.Key, *Count.Value))
			{
				return FString::Printf(TEXT("El parámetro \"ring.%s\" falta o no es un entero."), Count.Key);
			}
		}
		const TPair<const TCHAR*, FRingDifficultyParams*> Tiers[] = {
			{TEXT("easy"), &Out.Easy},
			{TEXT("normal"), &Out.Normal},
			{TEXT("hard"), &Out.Hard},
		};
		for (const TPair<const TCHAR*, FRingDifficultyParams*>& Tier : Tiers)
		{
			const FString Reason = ReadRingDifficulty(Ring, Tier.Key, *Tier.Value);
			if (!Reason.IsEmpty())
			{
				return Reason;
			}
		}
		FString DuelDifficulty;
		if (!Ring.TryGetStringField(TEXT("duelDifficulty"), DuelDifficulty) || !ParseDifficulty(DuelDifficulty, Out.DuelDifficulty))
		{
			return TEXT("El parámetro \"ring.duelDifficulty\" falta o no es \"easy\", \"normal\" o \"hard\".");
		}
		return FString();
	}

	/** Comprueba el bloque del anillo (S-026). Devuelve el motivo del rechazo, o vacío si es válido. */
	FString ValidateRing(const FRingParams& Ring)
	{
		// R-MIN-3 (decisión del autor del 06/11/2026): el hueco mínimo mayor que cero y el máximo no menor que el mínimo.
		if (!(Ring.PulseGapMinSeconds > 0.0))
		{
			return FString::Printf(TEXT("R-MIN-3: el hueco mínimo entre pulsos del anillo debe ser mayor que cero (es %g)."), Ring.PulseGapMinSeconds);
		}
		if (!(Ring.PulseGapMaxSeconds >= Ring.PulseGapMinSeconds))
		{
			return FString::Printf(TEXT("R-MIN-3: el hueco máximo entre pulsos del anillo (%g) no puede ser menor que el mínimo (%g)."),
				Ring.PulseGapMaxSeconds, Ring.PulseGapMinSeconds);
		}
		// R-MIN-6: por dificultad, cierre positivo, zona buena en (0, 1] y umbral en [0, 1].
		const TPair<const TCHAR*, const FRingDifficultyParams*> Tiers[] = {
			{TEXT("fácil"), &Ring.Easy},
			{TEXT("normal"), &Ring.Normal},
			{TEXT("difícil"), &Ring.Hard},
		};
		for (const TPair<const TCHAR*, const FRingDifficultyParams*>& Tier : Tiers)
		{
			if (!(Tier.Value->CloseSeconds > 0.0))
			{
				return FString::Printf(TEXT("R-MIN-6: el cierre del anillo en %s debe ser mayor que cero (es %g)."), Tier.Key, Tier.Value->CloseSeconds);
			}
			if (!(Tier.Value->SweetZone > 0.0 && Tier.Value->SweetZone <= 1.0))
			{
				return FString::Printf(TEXT("R-MIN-6: la zona buena del anillo en %s debe ser mayor que cero y no mayor que 1 (es %g)."), Tier.Key, Tier.Value->SweetZone);
			}
			if (!(Tier.Value->PassThreshold >= 0.0 && Tier.Value->PassThreshold <= 1.0))
			{
				return FString::Printf(TEXT("R-MIN-4: el umbral del anillo en %s debe estar entre 0 y 1 (es %g)."), Tier.Key, Tier.Value->PassThreshold);
			}
		}
		if (!(Ring.FinalChallengeSweetZone > 0.0 && Ring.FinalChallengeSweetZone <= 1.0))
		{
			return FString::Printf(TEXT("R-MIN-12: la zona buena propia del desafío final debe ser mayor que cero y no mayor que 1 (es %g)."), Ring.FinalChallengeSweetZone);
		}
		// R-MIN-8: cada uso, al menos un pulso.
		const TPair<const TCHAR*, int32> Uses[] = {
			{TEXT("la Fuente de un Dominio Olvidado"), Ring.ForgottenSourcePulses},
			{TEXT("la Prueba del Warden"), Ring.WardenTrialPulses},
			{TEXT("el escape del Vacío"), Ring.VoidEscapePulses},
			{TEXT("el duelo"), Ring.DuelPulses},
			{TEXT("el desafío final"), Ring.FinalChallengePulses},
		};
		for (const TPair<const TCHAR*, int32>& Use : Uses)
		{
			if (Use.Value < 1)
			{
				return FString::Printf(TEXT("R-MIN-8: el anillo en %s necesita al menos un pulso (son %d)."), Use.Key, Use.Value);
			}
		}
		if (!IsKnownDifficulty(Ring.DuelDifficulty))
		{
			return TEXT("R-MIN-8: la dificultad del anillo en el duelo debe ser fácil, normal o difícil.");
		}
		return FString();
	}
}

namespace Soulbound::Rules
{
	FMatchParamsLoadResult ValidateMatchParams(const FMatchParams& Params)
	{
		if (!(Params.TurnClockSeconds > 0.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-TUR-8: el reloj de turno debe ser mayor que cero (es %g)."), Params.TurnClockSeconds));
		}
		if (Params.MaxExtraTurns < 0)
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-TUR-9: el tope de turnos extra no puede ser negativo (es %d)."), Params.MaxExtraTurns));
		}
		if (Params.MaxLostTurns < 0)
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-TUR-10: el tope de turnos perdidos no puede ser negativo (es %d)."), Params.MaxLostTurns));
		}
		if (!(Params.RaceDurationSeconds > 0.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-CAR-1: la duración de la carrera debe ser mayor que cero (es %g)."), Params.RaceDurationSeconds));
		}
		if (!(Params.ChoiceClockSeconds > 0.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("S-009: el reloj de elección debe ser mayor que cero (es %g)."), Params.ChoiceClockSeconds));
		}
		if (!(Params.MoveStepSeconds > 0.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("S-020: la salvaguarda del movimiento debe ser mayor que cero (es %g)."), Params.MoveStepSeconds));
		}
		const TPair<const TCHAR*, double> ShellTimes[] = {
			{TEXT("la muestra del trilero"), Params.ShellRevealSeconds},
			{TEXT("la mezcla del trilero"), Params.ShellShuffleSeconds},
			{TEXT("el tiempo de elección de esfera"), Params.ShellPickSeconds},
		};
		for (const TPair<const TCHAR*, double>& Time : ShellTimes)
		{
			if (!(Time.Value > 0.0))
			{
				return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-ROB-2: %s debe durar más de cero segundos (es %g)."), Time.Key, Time.Value));
			}
		}
		if (Params.ShellSwapCount < 1)
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-ROB-2: la mezcla del trilero necesita al menos un intercambio (son %d)."), Params.ShellSwapCount));
		}
		// R-ELE-2 (S-012): un margen de 1 o más sería ganar siempre, otra regla (decisión del autor del 06/10/2026).
		if (!(Params.DuelAdvantageMargin >= 0.0 && Params.DuelAdvantageMargin < 1.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-ELE-2: el margen de Shuten-dōji en el duelo debe estar entre 0 y 1, el 1 excluido (es %g)."), Params.DuelAdvantageMargin));
		}
		// R-VAC-2, R-VAC-6 (S-013, decisión del autor del 08/10/2026): la principal de 1 a 3 peldaños, así nadie pasa del
		// cuarto turno dentro; la de la afinidad, de 1 a los de la principal. Todas con dificultades conocidas.
		const int32 Steps = Params.VoidEscapeLadder.Num();
		if (Steps < 1 || Steps > Soulbound::Match::MaxVoidEscapeSteps)
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-VAC-6: la escalera de escape del Vacío debe tener de 1 a %d peldaños (tiene %d)."),
				Soulbound::Match::MaxVoidEscapeSteps, Steps));
		}
		const int32 AffinitySteps = Params.VoidEscapeAffinityLadder.Num();
		if (AffinitySteps < 1 || AffinitySteps > Steps)
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-ELE-2: la escalera de escape de Mictlantecuhtli debe tener de 1 a %d peldaños, los de la principal (tiene %d)."),
				Steps, AffinitySteps));
		}
		// R-INS-3 (S-016, decisión del autor del 20/10/2026): la del Vacío de la Furia, validada como la principal.
		const int32 FurySteps = Params.FuryEscapeLadder.Num();
		if (FurySteps < 1 || FurySteps > Soulbound::Match::MaxVoidEscapeSteps)
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-INS-3: la escalera de escape del Vacío de la Furia debe tener de 1 a %d peldaños (tiene %d)."),
				Soulbound::Match::MaxVoidEscapeSteps, FurySteps));
		}
		const auto IsUnknown = [](ESkillDifficulty Difficulty) { return !IsKnownDifficulty(Difficulty); };
		const bool bKnown = Params.VoidEscapeLadder.FindByPredicate(IsUnknown) == nullptr && Params.VoidEscapeAffinityLadder.FindByPredicate(IsUnknown) == nullptr
			&& Params.FuryEscapeLadder.FindByPredicate(IsUnknown) == nullptr;
		if (!bKnown)
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("R-VAC-2: las escaleras de escape del Vacío solo admiten las dificultades fácil, normal y difícil."));
		}
		// S-014: las duraciones en rondas, de una en adelante; los relojes del dilema y corto, mayores que cero; el incremento
		// por opción del dilema puede ser cero.
		const TPair<const TCHAR*, int32> Rounds[] = {
			{TEXT("R-EFE-2: el Brazal revelado"), Params.RevealBracerRounds},
			{TEXT("R-EFE-8: la marca"), Params.MarkRounds},
			{TEXT("R-EFE-18: la Noche de los Ecos"), Params.EchoNightMaxRounds},
		};
		for (const TPair<const TCHAR*, int32>& Round : Rounds)
		{
			if (Round.Value < 1)
			{
				return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("%s debe durar al menos una ronda (son %d)."), Round.Key, Round.Value));
			}
		}
		if (!(Params.DilemmaClockSeconds > 0.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-COM-2: el reloj del dilema debe ser mayor que cero (es %g)."), Params.DilemmaClockSeconds));
		}
		if (!(Params.DilemmaClockPerOptionSeconds >= 0.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-COM-2: el incremento por opción del dilema no puede ser negativo (es %g)."), Params.DilemmaClockPerOptionSeconds));
		}
		if (!(Params.ShortChoiceClockSeconds > 0.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-DIR-1: el reloj corto debe ser mayor que cero (es %g)."), Params.ShortChoiceClockSeconds));
		}
		// S-015: la Prueba del Warden con una de las tres dificultades; el rendimiento del Warden en su duelo, de 0 a 1.
		if (!IsKnownDifficulty(Params.WardenTrialDifficulty))
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("R-EFE-10: la dificultad de la Prueba del Warden debe ser fácil, normal o difícil."));
		}
		if (!(Params.WardenDuelPerformance >= 0.0 && Params.WardenDuelPerformance <= 1.0))
		{
			return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("R-DIR-4: el rendimiento del Warden en su duelo debe estar entre 0 y 1 (es %g)."),
				Params.WardenDuelPerformance));
		}
		// S-018: el desafío final con una de las tres dificultades.
		if (!IsKnownDifficulty(Params.FinalChallengeDifficulty))
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("R-VIC-1: la dificultad del desafío final debe ser fácil, normal o difícil."));
		}
		// S-026: el anillo.
		const FString RingReason = ValidateRing(Params.Ring);
		if (!RingReason.IsEmpty())
		{
			return Reject(EMatchParamsError::MalformedData, RingReason);
		}
		return FMatchParamsLoadResult();
	}

	FMatchParamsLoadResult LoadMatchParamsFromJsonString(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("El texto no es un objeto JSON válido."));
		}

		// Se leen enteros o se rechazan enteros: el primer campo que falla devuelve su motivo.
		FMatchParams Params;
		if (!ReadNumberField(*Root, TEXT("turnClockSeconds"), Params.TurnClockSeconds))
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("El parámetro \"turnClockSeconds\" falta o no es un número."));
		}
		if (!ReadNumberField(*Root, TEXT("raceDurationSeconds"), Params.RaceDurationSeconds))
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("El parámetro \"raceDurationSeconds\" falta o no es un número."));
		}
		const TPair<const TCHAR*, double*> Times[] = {
			{TEXT("choiceClockSeconds"), &Params.ChoiceClockSeconds},
			{TEXT("moveStepSeconds"), &Params.MoveStepSeconds},
			{TEXT("shellRevealSeconds"), &Params.ShellRevealSeconds},
			{TEXT("shellShuffleSeconds"), &Params.ShellShuffleSeconds},
			{TEXT("shellPickSeconds"), &Params.ShellPickSeconds},
			{TEXT("duelAdvantageMargin"), &Params.DuelAdvantageMargin},
			{TEXT("dilemmaClockSeconds"), &Params.DilemmaClockSeconds},
			{TEXT("dilemmaClockPerOptionSeconds"), &Params.DilemmaClockPerOptionSeconds},
			{TEXT("shortChoiceClockSeconds"), &Params.ShortChoiceClockSeconds},
			{TEXT("wardenDuelPerformance"), &Params.WardenDuelPerformance},
		};
		for (const TPair<const TCHAR*, double*>& Time : Times)
		{
			if (!ReadNumberField(*Root, Time.Key, *Time.Value))
			{
				return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("El parámetro \"%s\" falta o no es un número."), Time.Key));
			}
		}
		const TPair<const TCHAR*, int32*> Caps[] = {
			{TEXT("maxExtraTurns"), &Params.MaxExtraTurns},
			{TEXT("maxLostTurns"), &Params.MaxLostTurns},
			{TEXT("shellSwapCount"), &Params.ShellSwapCount},
			{TEXT("revealBracerRounds"), &Params.RevealBracerRounds},
			{TEXT("markRounds"), &Params.MarkRounds},
			{TEXT("echoNightMaxRounds"), &Params.EchoNightMaxRounds},
		};
		for (const TPair<const TCHAR*, int32*>& Cap : Caps)
		{
			if (!ReadIntegerField(*Root, Cap.Key, *Cap.Value))
			{
				return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("El parámetro \"%s\" falta o no es un entero."), Cap.Key));
			}
		}

		const TPair<const TCHAR*, TArray<ESkillDifficulty>*> Ladders[] = {
			{TEXT("voidEscapeLadder"), &Params.VoidEscapeLadder},
			{TEXT("voidEscapeAffinityLadder"), &Params.VoidEscapeAffinityLadder},
			{TEXT("furyEscapeLadder"), &Params.FuryEscapeLadder},
		};
		for (const TPair<const TCHAR*, TArray<ESkillDifficulty>*>& Ladder : Ladders)
		{
			if (!ReadLadder(*Root, Ladder.Key, *Ladder.Value))
			{
				return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("El parámetro \"%s\" falta o no es una lista de dificultades (\"easy\", \"normal\", \"hard\")."), Ladder.Key));
			}
		}

		FString TrialDifficulty;
		if (!Root->TryGetStringField(TEXT("wardenTrialDifficulty"), TrialDifficulty) || !ParseDifficulty(TrialDifficulty, Params.WardenTrialDifficulty))
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("El parámetro \"wardenTrialDifficulty\" falta o no es \"easy\", \"normal\" o \"hard\"."));
		}
		FString FinalDifficulty;
		if (!Root->TryGetStringField(TEXT("finalChallengeDifficulty"), FinalDifficulty) || !ParseDifficulty(FinalDifficulty, Params.FinalChallengeDifficulty))
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("El parámetro \"finalChallengeDifficulty\" falta o no es \"easy\", \"normal\" o \"hard\"."));
		}
		const FString RingReason = ReadRing(*Root, Params.Ring);
		if (!RingReason.IsEmpty())
		{
			return Reject(EMatchParamsError::MalformedData, RingReason);
		}

		FMatchParamsLoadResult Result = ValidateMatchParams(Params);
		if (Result.IsSuccess())
		{
			Result.Params = Params;
		}
		return Result;
	}

	FMatchParamsLoadResult LoadMatchParamsFromFile(const FString& FilePath)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			return Reject(EMatchParamsError::FileNotFound, FString::Printf(TEXT("No se pudo leer el archivo de parámetros de partida: %s"), *FilePath));
		}
		return LoadMatchParamsFromJsonString(Json);
	}
}
