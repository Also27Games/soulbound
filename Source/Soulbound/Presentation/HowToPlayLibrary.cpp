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
