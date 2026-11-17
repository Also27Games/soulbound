#include "HowToPlay.h"

#include "Misc/FileHelper.h"

namespace Soulbound::Presentation
{
	namespace
	{
		/** La marca de un título de apartado en el Markdown sencillo del texto. */
		const FString HeadingMark = TEXT("## ");

		FHowToPlayLoadResult Reject(EHowToPlayError Error, const FString& Message)
		{
			FHowToPlayLoadResult Result;
			Result.Error = Error;
			Result.Message = Message;
			return Result;
		}
	}

	const TArray<FString>& HowToPlayHeadings()
	{
		static const TArray<FString> Headings = {HeadingMark + TEXT("Qué es esto"), HeadingMark + TEXT("Cómo se gana"), HeadingMark + TEXT("El Warden")};
		return Headings;
	}

	FHowToPlayLoadResult ParseHowToPlayText(const FString& Markdown, const FString& SourceName)
	{
		TArray<FString> Lines;
		Markdown.ParseIntoArrayLines(Lines, false);
		// Cada título exigido, en su propia línea y después del anterior.
		int32 Next = 0;
		for (const FString& Heading : HowToPlayHeadings())
		{
			while (Next < Lines.Num() && Lines[Next].TrimEnd() != Heading)
			{
				++Next;
			}
			if (Next == Lines.Num())
			{
				return Reject(EHowToPlayError::MissingHeading, FString::Printf(TEXT("Al texto de cómo se juega le falta el título \"%s\", o no está en su orden"), *Heading));
			}
			++Next;
		}
		FHowToPlayLoadResult Result;
		Result.Text = Markdown;
		return Result;
	}

	FHowToPlayLoadResult LoadHowToPlayText(const FString& FilePath)
	{
		FString Markdown;
		if (!FFileHelper::LoadFileToString(Markdown, *FilePath))
		{
			return Reject(EHowToPlayError::FileNotFound, FString::Printf(TEXT("No se pudo leer el texto de cómo se juega: %s"), *FilePath));
		}
		return ParseHowToPlayText(Markdown);
	}

	FString HowToPlayDisplayText(const FString& Markdown)
	{
		// Línea a línea, conservando los saltos tal cual: solo cambia el principio de las líneas de título.
		FString Display;
		Display.Reserve(Markdown.Len());
		int32 LineStart = 0;
		while (LineStart < Markdown.Len())
		{
			int32 LineEnd = Markdown.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LineStart);
			LineEnd = LineEnd == INDEX_NONE ? Markdown.Len() : LineEnd + 1;
			const FString Line = Markdown.Mid(LineStart, LineEnd - LineStart);
			Display += Line.StartsWith(HeadingMark, ESearchCase::CaseSensitive) ? Line.RightChop(HeadingMark.Len()) : Line;
			LineStart = LineEnd;
		}
		return Display;
	}

	FString HowToPlayImagePath(const FString& Folder, const FString& ImageName)
	{
		// TODO(R-HUD-17): por implementar (S-048); el test está en rojo.
		return FString();
	}
}
