#include "HowToPlayLibrary.h"

#include "HowToPlay.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundHowToPlay, Log, All);

FText UHowToPlayLibrary::HowToPlayText()
{
	const FHowToPlayLoadResult Result = Soulbound::Presentation::LoadHowToPlayText(FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::HowToPlayRelativePath));
	if (!Result.IsSuccess())
	{
		UE_LOG(LogSoulboundHowToPlay, Error, TEXT("%s"), *Result.Message);
		return FText::GetEmpty();
	}
	return FText::FromString(Soulbound::Presentation::HowToPlayDisplayText(Result.Text));
}

// TODO(R-HUD-19): por implementar (S-048); los tests están en rojo.
FText UHowToPlayLibrary::HowToPlayBookTitle()
{
	return FText::GetEmpty();
}

FText UHowToPlayLibrary::HowToPlayCoverLine()
{
	return FText::GetEmpty();
}

int32 UHowToPlayLibrary::HowToPlayPageCount()
{
	return 0;
}

FHowToPlayPage UHowToPlayLibrary::HowToPlayPage(int32 Index)
{
	return FHowToPlayPage();
}

UTexture2D* UHowToPlayLibrary::HowToPlayPageImage(int32 Index)
{
	return nullptr;
}

FText UHowToPlayLibrary::HowToPlayError()
{
	return FText::GetEmpty();
}

Soulbound::Presentation::FHowToPlayBook UHowToPlayLibrary::BookFromFile(const FString& FilePath)
{
	return Soulbound::Presentation::FHowToPlayBook();
}

Soulbound::Presentation::FGrimoireKeyNames UHowToPlayLibrary::ConfiguredKeyNames()
{
	return Soulbound::Presentation::FGrimoireKeyNames();
}
