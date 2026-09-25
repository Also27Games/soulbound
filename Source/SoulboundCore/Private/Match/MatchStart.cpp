#include "Match/MatchStart.h"

#include "Math/RandomStream.h"

namespace Soulbound::Rules
{
	FMatchStartResult StartMatch(const FBoardState& Board, int32 PlayerCount, FRandomStream& Random)
	{
		FMatchStartResult Result;
		Result.Error = EMatchStartError::InvalidBoard;
		Result.Message = TEXT("Sin implementar");
		return Result;
	}
}
