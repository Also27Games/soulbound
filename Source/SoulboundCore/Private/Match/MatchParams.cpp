#include "Match/MatchParams.h"

namespace Soulbound::Rules
{
	FMatchParamsLoadResult ValidateMatchParams(const FMatchParams& Params)
	{
		FMatchParamsLoadResult Result;
		Result.Error = EMatchParamsError::MalformedData;
		Result.Message = TEXT("Sin implementar.");
		return Result;
	}

	FMatchParamsLoadResult LoadMatchParamsFromJsonString(const FString& Json)
	{
		return ValidateMatchParams(FMatchParams());
	}

	FMatchParamsLoadResult LoadMatchParamsFromFile(const FString& FilePath)
	{
		return ValidateMatchParams(FMatchParams());
	}
}
