#include "PlayerProfile.h"

const FString UPlayerProfile::DefaultSlot = TEXT("perfil");

bool UPlayerProfile::HasSeen(EInstructionMoment Moment) const
{
	return false;
}

void UPlayerProfile::ForgetInstructions()
{
}

bool UPlayerProfile::SaveToSlot(const FString& Slot) const
{
	return false;
}

UPlayerProfile* UPlayerProfile::LoadFromSlot(const FString& Slot)
{
	return NewObject<UPlayerProfile>(GetTransientPackage());
}

TArray<uint8> UPlayerProfile::ToBytes() const
{
	return TArray<uint8>();
}

UPlayerProfile* UPlayerProfile::FromBytes(const TArray<uint8>& Bytes)
{
	return NewObject<UPlayerProfile>(GetTransientPackage());
}
