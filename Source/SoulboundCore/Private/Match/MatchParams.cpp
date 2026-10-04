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
		if (!ReadNumberField(*Root, TEXT("choiceClockSeconds"), Params.ChoiceClockSeconds))
		{
			return Reject(EMatchParamsError::MalformedData, TEXT("El parámetro \"choiceClockSeconds\" falta o no es un número."));
		}
		const TPair<const TCHAR*, int32*> Caps[] = {
			{TEXT("maxExtraTurns"), &Params.MaxExtraTurns},
			{TEXT("maxLostTurns"), &Params.MaxLostTurns},
		};
		for (const TPair<const TCHAR*, int32*>& Cap : Caps)
		{
			if (!ReadIntegerField(*Root, Cap.Key, *Cap.Value))
			{
				return Reject(EMatchParamsError::MalformedData, FString::Printf(TEXT("El parámetro \"%s\" falta o no es un entero."), Cap.Key));
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
