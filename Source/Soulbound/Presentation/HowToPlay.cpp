#include "HowToPlay.h"

#include "Misc/FileHelper.h"

namespace Soulbound::Presentation
{
	const TArray<FString>& HowToPlayHeadings()
	{
		static const TArray<FString> Headings;
		return Headings;
	}

	FHowToPlayLoadResult ParseHowToPlayText(const FString& Markdown)
	{
		return FHowToPlayLoadResult();
	}

	FHowToPlayLoadResult LoadHowToPlayText(const FString& FilePath)
	{
		return FHowToPlayLoadResult();
	}

	FString HowToPlayDisplayText(const FString& Markdown)
	{
		return Markdown;
	}
}
