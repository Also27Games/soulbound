#include "PlayerProfileSubsystem.h"

#include "../Presentation/InstructionTracker.h"

void UPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Profile = UPlayerProfile::LoadFromSlot(UPlayerProfile::DefaultSlot);
}

EHintsMode UPlayerProfileSubsystem::GetHints() const
{
	return Profile ? Profile->Hints : EHintsMode::Automatic;
}

void UPlayerProfileSubsystem::SetHints(EHintsMode Mode)
{
	if (Profile)
	{
		Profile->Hints = Mode;
		Save();
	}
}

bool UPlayerProfileSubsystem::TeachInstruction(EInstructionMoment Moment)
{
	if (!Profile)
	{
		return false;
	}
	const int32 Before = Profile->InstructionsSeen.Num();
	const bool bTeach = Soulbound::Presentation::TeachAndRecord(Profile->Hints, Profile->InstructionsSeen, Moment);
	if (Profile->InstructionsSeen.Num() != Before)
	{
		Save();
	}
	return bTeach;
}

bool UPlayerProfileSubsystem::ShowsRequestHints() const
{
	return !Profile || Soulbound::Presentation::ShowsRequestHints(Profile->Hints, Profile->InstructionsSeen);
}

void UPlayerProfileSubsystem::ForgetInstructions()
{
	if (Profile)
	{
		Profile->ForgetInstructions();
		Save();
	}
}

const TArray<FString>& UPlayerProfileSubsystem::GetInstructionsSeen() const
{
	static const TArray<FString> None;
	return Profile ? Profile->InstructionsSeen : None;
}

void UPlayerProfileSubsystem::Save() const
{
	if (Profile && !Profile->SaveToSlot(UPlayerProfile::DefaultSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Perfil] No se pudo guardar el perfil en la ranura '%s'."), *UPlayerProfile::DefaultSlot);
	}
}
