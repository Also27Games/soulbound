#include "Board/BoardLoader.h"

namespace Soulbound::Rules
{
	FBoardLoadResult ValidateBoard(const FBoardState& Board)
	{
		FBoardLoadResult Result;
		Result.Error = EBoardError::MalformedData;
		Result.Message = TEXT("Sin implementar");
		return Result;
	}

	FBoardLoadResult LoadBoardFromJsonString(const FString& Json)
	{
		FBoardLoadResult Result;
		Result.Error = EBoardError::MalformedData;
		Result.Message = TEXT("Sin implementar");
		return Result;
	}

	FBoardLoadResult LoadBoardFromFile(const FString& FilePath)
	{
		FBoardLoadResult Result;
		Result.Error = EBoardError::FileNotFound;
		Result.Message = TEXT("Sin implementar");
		return Result;
	}
}
