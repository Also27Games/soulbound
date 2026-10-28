#include "PlayerProfile.h"

#include "Kismet/GameplayStatics.h"

const FString UPlayerProfile::DefaultSlot = TEXT("perfil");

namespace
{
	/** Índice de usuario de la ranura: un solo jugador local por máquina. */
	constexpr int32 ProfileUserIndex = 0;
}

bool UPlayerProfile::HasSeen(EInstructionMoment Moment) const
{
	return InstructionsSeen.Contains(Soulbound::Rules::InstructionKey(Moment));
}

void UPlayerProfile::ForgetInstructions()
{
	InstructionsSeen.Empty();
}

bool UPlayerProfile::SaveToSlot(const FString& Slot) const
{
	return UGameplayStatics::SaveGameToSlot(const_cast<UPlayerProfile*>(this), Slot, ProfileUserIndex);
}

UPlayerProfile* UPlayerProfile::LoadFromSlot(const FString& Slot)
{
	if (UGameplayStatics::DoesSaveGameExist(Slot, ProfileUserIndex))
	{
		if (UPlayerProfile* Loaded = Cast<UPlayerProfile>(UGameplayStatics::LoadGameFromSlot(Slot, ProfileUserIndex)))
		{
			return Loaded;
		}
	}
	return NewObject<UPlayerProfile>(GetTransientPackage());
}

TArray<uint8> UPlayerProfile::ToBytes() const
{
	TArray<uint8> Bytes;
	UGameplayStatics::SaveGameToMemory(const_cast<UPlayerProfile*>(this), Bytes);
	return Bytes;
}

UPlayerProfile* UPlayerProfile::FromBytes(const TArray<uint8>& Bytes)
{
	if (UPlayerProfile* Loaded = Cast<UPlayerProfile>(UGameplayStatics::LoadGameFromMemory(Bytes)))
	{
		return Loaded;
	}
	return NewObject<UPlayerProfile>(GetTransientPackage());
}
