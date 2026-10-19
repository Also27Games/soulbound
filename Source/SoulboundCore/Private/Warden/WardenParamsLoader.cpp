#include "Warden/WardenParams.h"

#include "Misc/FileHelper.h"

namespace
{
	FWardenParamsLoadResult Reject(EWardenParamsError Error, const FString& Message)
	{
		FWardenParamsLoadResult Result;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}
}

namespace Soulbound::Rules
{
	FWardenParamsLoadResult ValidateWardenParams(const FWardenParams& Params)
	{
		// TODO(R-SAC-2): firma vacía del ciclo rojo de S-016.
		return FWardenParamsLoadResult();
	}

	FWardenParamsLoadResult LoadWardenParamsFromJsonString(const FString& Json)
	{
		// TODO(R-SAC-2): firma vacía del ciclo rojo de S-016.
		return Reject(EWardenParamsError::MalformedData, TEXT("Sin implementar."));
	}

	FWardenParamsLoadResult LoadWardenParamsFromFile(const FString& FilePath)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FilePath))
		{
			return Reject(EWardenParamsError::FileNotFound, FString::Printf(TEXT("No se pudo leer el archivo del Warden: %s"), *FilePath));
		}
		return LoadWardenParamsFromJsonString(Json);
	}
}
