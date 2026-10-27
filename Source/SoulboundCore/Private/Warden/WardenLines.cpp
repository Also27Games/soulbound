#include "Warden/WardenLines.h"

// Esqueleto del corte A1 de S-021: la API existe y no hace nada, para que los tests compilen y se vean fallar antes de implementar.

namespace Soulbound::Rules
{
	FString WardenEventKey(const FString& EventId) { return FString(); }
	FString WardenRemarkKey(EWardenRemarkKind Kind) { return FString(); }
	FString InstructionKey(EInstructionMoment Moment) { return FString(); }
	FString ScheduledOrderKey(int32 Position) { return FString(); }
	FString IntroductionKey(int32 DomainId) { return FString(); }
	TArray<FString> RequiredWardenLineKeys(const FWardenEventTable& Events, const FElectsTable& Elects) { return TArray<FString>(); }

	FWardenLinesLoadResult ValidateWardenLines(const FWardenLineTable& Table, const TArray<FString>& Required)
	{
		FWardenLinesLoadResult Result;
		Result.Error = EWardenLinesError::MalformedData;
		return Result;
	}

	FWardenLinesLoadResult LoadWardenLinesFromJsonString(const FString& Json, const TArray<FString>& Required)
	{
		FWardenLinesLoadResult Result;
		Result.Error = EWardenLinesError::MalformedData;
		return Result;
	}

	FWardenLinesLoadResult LoadWardenLinesFromFile(const FString& FilePath, const TArray<FString>& Required)
	{
		FWardenLinesLoadResult Result;
		Result.Error = EWardenLinesError::FileNotFound;
		return Result;
	}

	FString StripLineCues(const FString& Line) { return Line; }
}
