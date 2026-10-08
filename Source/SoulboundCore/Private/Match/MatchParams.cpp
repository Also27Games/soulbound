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
			if (!Value.IsValid() || !Value->TryGetString(Name))
			{
				return false;
			}
			int32 Index = INDEX_NONE;
			for (int32 Candidate = 0; Candidate < DifficultyCount; ++Candidate)
			{
				Index = Name == DifficultyNames[Candidate] ? Candidate : Index;
			}
			if (Index == INDEX_NONE)
			{
				return false;
			}
			OutLadder.Add(static_cast<ESkillDifficulty>(Index));
		}
		return true;
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
		const bool bKnown = Params.VoidEscapeLadder.FindByPredicate([](ESkillDifficulty Difficulty) { return !IsKnownDifficulty(Difficulty); }) == nullptr
			&& Params.VoidEscapeAffinityLadder.FindByPredicate([](ESkillDifficulty Difficulty) { return !IsKnownDifficulty(Difficulty); }) == nullptr;
		if (!bKnown)
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("R-VAC-2: las escaleras de escape del Vacío solo admiten las dificultades fácil, normal y difícil."));
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
			{TEXT("shellRevealSeconds"), &Params.ShellRevealSeconds},
			{TEXT("shellShuffleSeconds"), &Params.ShellShuffleSeconds},
			{TEXT("shellPickSeconds"), &Params.ShellPickSeconds},
			{TEXT("duelAdvantageMargin"), &Params.DuelAdvantageMargin},
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
		};
		for (const TPair<const TCHAR*, TArray<ESkillDifficulty>*>& Ladder : Ladders)
		{
			if (!ReadLadder(*Root, Ladder.Key, *Ladder.Value))
			{
				return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("El parámetro \"%s\" falta o no es una lista de dificultades (\"easy\", \"normal\", \"hard\")."), Ladder.Key));
			}
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
