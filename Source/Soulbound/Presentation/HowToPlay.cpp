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
		/** S-048 (ampliación del autor del 17/11/2026): un elemento de lista. */
		const FString ListItemMark = TEXT("- ");

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

		/**
		 * Lo que se va leyendo de una página (S-048, ampliación del autor del 17/11/2026): sus bloques y el que está abierto, un párrafo o una frase
		 * del Warden que puede seguir en la línea de abajo.
		 */
		struct FPageInProgress
		{
			FHowToPlayPage Page;
			TOptional<FHowToPlayBlock> Open;
			bool bHasImage = false;

			/** Cierra el bloque abierto y lo añade, si tiene texto. */
			void Close()
			{
				if (Open.IsSet() && !Open->Text.IsEmpty())
				{
					Page.Blocks.Add(Open.GetValue());
				}
				Open.Reset();
			}

			/** Sigue el bloque abierto si es de esa clase; si no, lo cierra y abre otro. */
			void Continue(EHowToPlayBlockKind Kind, const FString& Text)
			{
				if (Open.IsSet() && Open->Kind != Kind)
				{
					Close();
				}
				if (!Open.IsSet())
				{
					Open.Emplace();
					Open->Kind = Kind;
				}
				AppendWithSpace(Open->Text, Text);
			}
		};

		/** Los bloques unidos como en HowToPlayBodyText, cada uno con el texto que da Text. */
		FString JoinBlocks(const TArray<FHowToPlayBlock>& Blocks, TFunctionRef<FString(const FHowToPlayBlock&)> Text)
		{
			FString Joined;
			for (int32 Index = 0; Index < Blocks.Num(); ++Index)
			{
				const FHowToPlayBlock& Block = Blocks[Index];
				if (Index > 0)
				{
					// Dos elementos de lista seguidos, uno por línea; lo demás, con una línea en blanco entre medias.
					const bool bListRun = Block.Kind == EHowToPlayBlockKind::ListItem && Blocks[Index - 1].Kind == EHowToPlayBlockKind::ListItem;
					Joined += bListRun ? TEXT("\n") : TEXT("\n\n");
				}
				if (Block.Kind == EHowToPlayBlockKind::ListItem)
				{
					Joined += FString(HowToPlayListMark) + TEXT(" ");
				}
				Joined += Text(Block);
			}
			return Joined;
		}

		/** Escapa los caracteres que el texto enriquecido lee como marcas: & < > ". El & primero, para no escapar lo ya escapado. */
		FString EscapeRichText(const FString& Text)
		{
			return Text.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;")).Replace(TEXT("\""), TEXT("&quot;"));
		}
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

		// Cierra la página en curso: su texto sin marcas y, sin ningún bloque, el error que nombra el archivo y la página.
		const auto FinishPage = [&Result, &Current, &SourceName]() -> bool
		{
			if (!Current.IsSet())
			{
				return true;
			}
			Current->Close();
			Current->Page.Body = HowToPlayBodyText(Current->Page.Blocks);
			if (Current->Page.Blocks.IsEmpty())
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
				// Las líneas ">" seguidas son una frase del Warden; tras una línea en blanco, otra (ampliación del autor del 17/11/2026).
				Page.Continue(EHowToPlayBlockKind::WardenLine, Line.RightChop(CommentMark.Len()).TrimStartAndEnd());
				continue;
			}
			if (Line.StartsWith(ListItemMark, ESearchCase::CaseSensitive))
			{
				// Cada línea "- " es un elemento de lista, sin el guion, y no se une a nada (ampliación del autor del 17/11/2026).
				Page.Close();
				FHowToPlayBlock Item;
				Item.Kind = EHowToPlayBlockKind::ListItem;
				Item.Text = Line.RightChop(ListItemMark.Len()).TrimStartAndEnd();
				Page.Page.Blocks.Add(Item);
				continue;
			}

			FString ImageName;
			if (ParseImageLine(Line, ImageName))
			{
				Page.Close();
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
				Page.Close();
				continue;
			}
			// Las líneas de un mismo párrafo se unen con un espacio (precisión del autor del 17/11/2026); tras una frase o una lista, otro párrafo.
			Page.Continue(EHowToPlayBlockKind::Paragraph, Line);
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

	FString HowToPlayBodyText(const TArray<FHowToPlayBlock>& Blocks)
	{
		return JoinBlocks(Blocks, [](const FHowToPlayBlock& Block) { return Block.Text; });
	}

	FString HowToPlayBlockRichText(const FHowToPlayBlock& Block)
	{
		const FString Escaped = EscapeRichText(Block.Text);
		return Block.Kind == EHowToPlayBlockKind::WardenLine ? TEXT("<Warden>") + Escaped + TEXT("</>") : Escaped;
	}

	FString HowToPlayRichText(const FHowToPlayPage& Page)
	{
		return JoinBlocks(Page.Blocks, [](const FHowToPlayBlock& Block) { return HowToPlayBlockRichText(Block); });
	}
}
