#include "HowToPlay.h"

#include "Misc/FileHelper.h"

namespace Soulbound::Presentation
{
	namespace
	{
		/** Las marcas del Markdown sencillo del libro (R-HUD-17). */
		const FString BookTitleMark = TEXT("# ");
		const FString PageTitleMark = TEXT("## ");
		const FString CommentMark = TEXT(">");

		FHowToPlayLoadResult Reject(EHowToPlayError Error, const FString& Message)
		{
			FHowToPlayLoadResult Result;
			Result.Error = Error;
			Result.Message = Message;
			return Result;
		}

		/** Une un trozo a un texto con un espacio, sin espacios de más si alguno está vacío. */
		void AppendWithSpace(FString& Text, const FString& Piece)
		{
			if (Piece.IsEmpty())
			{
				return;
			}
			Text += Text.IsEmpty() ? Piece : TEXT(" ") + Piece;
		}

		/** Si la línea (ya recortada) es una imagen, "![texto](Nombre)", y su nombre; el texto alternativo se ignora. */
		bool ParseImageLine(const FString& Line, FString& OutName)
		{
			if (!Line.StartsWith(TEXT("![")) || !Line.EndsWith(TEXT(")")))
			{
				return false;
			}
			const int32 Open = Line.Find(TEXT("]("), ESearchCase::CaseSensitive);
			if (Open == INDEX_NONE)
			{
				return false;
			}
			const int32 NameStart = Open + 2;
			OutName = Line.Mid(NameStart, Line.Len() - 1 - NameStart).TrimStartAndEnd();
			return true;
		}

		/** Lo que se va leyendo de una página: el párrafo en curso y lo que ya tiene. */
		struct FPageInProgress
		{
			FHowToPlayPage Page;
			TArray<FString> Paragraphs;
			FString Paragraph;
			bool bInComment = false;
			bool bHasComment = false;
			bool bHasImage = false;

			void EndParagraph()
			{
				if (!Paragraph.IsEmpty())
				{
					Paragraphs.Add(Paragraph);
					Paragraph.Reset();
				}
			}
		};
	}

	FHowToPlayLoadResult ParseHowToPlayText(const FString& Markdown, const FString& SourceName)
	{
		TArray<FString> Lines;
		Markdown.ParseIntoArrayLines(Lines, false);

		FHowToPlayLoadResult Result;
		TOptional<FPageInProgress> Current;
		// La portada: el primer "# " y el primer bloque de líneas ">" antes de la primera página (R-HUD-17); lo demás de antes se ignora.
		bool bCoverOpen = false;
		bool bCoverDone = false;

		// Cierra la página en curso: sus párrafos al texto y, sin texto, el error que nombra el archivo y la página.
		const auto FinishPage = [&Result, &Current, &SourceName]() -> bool
		{
			if (!Current.IsSet())
			{
				return true;
			}
			Current->EndParagraph();
			Current->Page.Body = FString::Join(Current->Paragraphs, TEXT("\n\n"));
			if (Current->Page.Body.IsEmpty())
			{
				Result = Reject(EHowToPlayError::EmptyPage,
					FString::Printf(TEXT("En el texto de cómo se juega (%s), la página \"%s\" no tiene texto"), *SourceName, *Current->Page.Title));
				return false;
			}
			Result.Pages.Add(Current->Page);
			Current.Reset();
			return true;
		};

		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.StartsWith(PageTitleMark, ESearchCase::CaseSensitive))
			{
				if (!FinishPage())
				{
					return Result;
				}
				Current.Emplace();
				Current->Page.Title = Line.RightChop(PageTitleMark.Len()).TrimStartAndEnd();
				continue;
			}

			if (!Current.IsSet())
			{
				// Antes de la primera página: el título del libro, la frase de la portada o texto suelto, que se ignora.
				if (Line.StartsWith(CommentMark, ESearchCase::CaseSensitive))
				{
					if (!bCoverDone)
					{
						bCoverOpen = true;
						AppendWithSpace(Result.CoverLine, Line.RightChop(CommentMark.Len()).TrimStartAndEnd());
					}
					continue;
				}
				bCoverDone = bCoverDone || bCoverOpen;
				if (Line.StartsWith(BookTitleMark, ESearchCase::CaseSensitive) && Result.BookTitle.IsEmpty())
				{
					Result.BookTitle = Line.RightChop(BookTitleMark.Len()).TrimStartAndEnd();
				}
				continue;
			}

			FPageInProgress& Page = *Current;
			if (Line.StartsWith(CommentMark, ESearchCase::CaseSensitive))
			{
				// Varias líneas ">" seguidas son un comentario; otro bloque separado es un error (precisión del autor del 17/11/2026).
				Page.EndParagraph();
				if (!Page.bInComment && Page.bHasComment)
				{
					return Reject(EHowToPlayError::DuplicateComment, FString::Printf(
						TEXT("En el texto de cómo se juega (%s), la página \"%s\" tiene dos comentarios del Warden separados; como mucho uno por página"),
						*SourceName, *Page.Page.Title));
				}
				Page.bInComment = true;
				Page.bHasComment = true;
				AppendWithSpace(Page.Page.WardenComment, Line.RightChop(CommentMark.Len()).TrimStartAndEnd());
				continue;
			}
			Page.bInComment = false;

			FString ImageName;
			if (ParseImageLine(Line, ImageName))
			{
				Page.EndParagraph();
				if (Page.bHasImage)
				{
					return Reject(EHowToPlayError::DuplicateImage, FString::Printf(
						TEXT("En el texto de cómo se juega (%s), la página \"%s\" tiene dos imágenes; como mucho una por página"), *SourceName, *Page.Page.Title));
				}
				Page.bHasImage = true;
				Page.Page.ImageName = ImageName;
				continue;
			}

			if (Line.IsEmpty())
			{
				Page.EndParagraph();
				continue;
			}
			// Las líneas de un mismo párrafo se unen con un espacio (precisión del autor del 17/11/2026).
			AppendWithSpace(Page.Paragraph, Line);
		}

		if (!FinishPage())
		{
			return Result;
		}
		if (Result.Pages.IsEmpty())
		{
			return Reject(EHowToPlayError::NoPages, FString::Printf(
				TEXT("El texto de cómo se juega (%s) no tiene ninguna página: cada página empieza con un título \"## \""), *SourceName));
		}
		return Result;
	}

	FHowToPlayLoadResult LoadHowToPlayText(const FString& FilePath)
	{
		FString Markdown;
		if (!FFileHelper::LoadFileToString(Markdown, *FilePath))
		{
			return Reject(EHowToPlayError::FileNotFound, FString::Printf(TEXT("No se pudo leer el texto de cómo se juega: %s"), *FilePath));
		}
		return ParseHowToPlayText(Markdown, FilePath);
	}

	FString HowToPlayImagePath(const FString& Folder, const FString& ImageName)
	{
		const FString Name = ImageName.TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			return FString();
		}
		FString Path = Folder.TrimStartAndEnd();
		if (!Path.EndsWith(TEXT("/")))
		{
			Path += TEXT("/");
		}
		return Path + Name + TEXT(".") + Name;
	}
}
