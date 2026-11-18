#include "HowToPlay.h"

#include "Misc/FileHelper.h"

namespace Soulbound::Presentation
{
	namespace
	{
		/** Las marcas del Markdown sencillo del libro (R-HUD-17). */
		const FString BookTitleMark = TEXT("# ");
		const FString PageTitleMark = TEXT("## ");
		/** S-048 (segunda ampliación del autor del 17/11/2026): una página sin título, "##" sola, y un subtítulo. */
		const FString BarePageTitleMark = TEXT("##");
		const FString SubtitleMark = TEXT("### ");
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

		/** El texto de unos tramos, sin marcas. */
		FString SpansText(const TArray<FHowToPlaySpan>& Spans)
		{
			FString Text;
			for (const FHowToPlaySpan& Span : Spans)
			{
				Text += Span.Text;
			}
			return Text;
		}

		/** Añade un tramo; si es del mismo tipo que el anterior, lo une a él. Los vacíos no cuentan. */
		void AddSpan(TArray<FHowToPlaySpan>& Spans, const FString& Text, bool bBold)
		{
			if (Text.IsEmpty())
			{
				return;
			}
			if (!Spans.IsEmpty() && Spans.Last().bBold == bBold)
			{
				Spans.Last().Text += Text;
				return;
			}
			FHowToPlaySpan Span;
			Span.Text = Text;
			Span.bBold = bBold;
			Spans.Add(MoveTemp(Span));
		}

		/**
		 * Lo que se va leyendo de una página (S-048, ampliación del autor del 17/11/2026): sus bloques y el que está abierto, un párrafo o una frase
		 * del Warden que puede seguir en la línea de abajo.
		 */
		struct FPageInProgress
		{
			FHowToPlayPage Page;
			TOptional<FHowToPlayBlock> Open;

			/** Cierra el bloque abierto y lo añade, con sus tramos, si tiene texto. */
			void Close()
			{
				if (Open.IsSet())
				{
					Add(Open.GetValue());
				}
				Open.Reset();
			}

			/** Añade un bloque ya entero: parte su texto en tramos (la negrita) y le quita los asteriscos; sin texto, nada. */
			void Add(FHowToPlayBlock Block)
			{
				Block.Spans = HowToPlaySpans(Block.Text);
				Block.Text = SpansText(Block.Spans);
				if (Block.Kind == EHowToPlayBlockKind::Subtitle)
				{
					// Segunda ampliación del autor: el subtítulo no lleva negrita propia; sin asteriscos, todo con su estilo.
					Block.Spans.Reset();
					AddSpan(Block.Spans, Block.Text, false);
				}
				if (!Block.Text.IsEmpty())
				{
					Page.Blocks.Add(MoveTemp(Block));
				}
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
			// Segunda ampliación del autor: las imágenes no van en el texto; dos elementos de lista a ambos lados de una imagen siguen seguidos.
			TArray<const FHowToPlayBlock*> Shown;
			for (const FHowToPlayBlock& Block : Blocks)
			{
				if (Block.Kind != EHowToPlayBlockKind::Image)
				{
					Shown.Add(&Block);
				}
			}
			FString Joined;
			for (int32 Index = 0; Index < Shown.Num(); ++Index)
			{
				const FHowToPlayBlock& Block = *Shown[Index];
				if (Index > 0)
				{
					// Dos elementos de lista seguidos, uno por línea; lo demás, con una línea en blanco entre medias.
					const bool bListRun = Block.Kind == EHowToPlayBlockKind::ListItem && Shown[Index - 1]->Kind == EHowToPlayBlockKind::ListItem;
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
			// "## Título" o "##" sola, sin título (segunda ampliación del autor del 17/11/2026).
			if (Line.StartsWith(PageTitleMark, ESearchCase::CaseSensitive) || Line.Equals(BarePageTitleMark, ESearchCase::CaseSensitive))
			{
				if (!FinishPage())
				{
					return Result;
				}
				Current.Emplace();
				Current->Page.Title = Line.RightChop(BarePageTitleMark.Len()).TrimStartAndEnd();
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
				Page.Add(Item);
				continue;
			}

			if (Line.StartsWith(SubtitleMark, ESearchCase::CaseSensitive))
			{
				// Segunda ampliación del autor: un subtítulo, una línea, sin las almohadillas.
				Page.Close();
				FHowToPlayBlock Subtitle;
				Subtitle.Kind = EHowToPlayBlockKind::Subtitle;
				Subtitle.Text = Line.RightChop(SubtitleMark.Len()).TrimStartAndEnd();
				Page.Add(Subtitle);
				continue;
			}

			FString ImageName;
			if (ParseImageLine(Line, ImageName))
			{
				// Segunda ampliación del autor: cada imagen es un bloque en su sitio, las que haya.
				Page.Close();
				if (!ImageName.IsEmpty())
				{
					FHowToPlayBlock Image;
					Image.Kind = EHowToPlayBlockKind::Image;
					Image.ImageName = ImageName;
					Page.Page.Blocks.Add(MoveTemp(Image));
				}
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

	TArray<FHowToPlaySpan> HowToPlaySpans(const FString& Text)
	{
		// Solo "**texto**" es negrita (precisión del autor del 17/11/2026): se busca un "**" que abra y otro que cierre; si no cierra, lo que queda es
		// texto, con sus asteriscos. Un asterisco suelto nunca abre nada.
		static const FString BoldMark = TEXT("**");
		TArray<FHowToPlaySpan> Spans;
		int32 From = 0;
		while (From < Text.Len())
		{
			const int32 Open = Text.Find(BoldMark, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			const int32 Close = Open == INDEX_NONE ? INDEX_NONE : Text.Find(BoldMark, ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + BoldMark.Len());
			if (Close == INDEX_NONE)
			{
				AddSpan(Spans, Text.Mid(From), false);
				break;
			}
			AddSpan(Spans, Text.Mid(From, Open - From), false);
			AddSpan(Spans, Text.Mid(Open + BoldMark.Len(), Close - Open - BoldMark.Len()), true);
			From = Close + BoldMark.Len();
		}
		return Spans;
	}

	FString HowToPlayBodyText(const TArray<FHowToPlayBlock>& Blocks)
	{
		return JoinBlocks(Blocks, [](const FHowToPlayBlock& Block) { return Block.Text; });
	}

	FString HowToPlayBlockRichText(const FHowToPlayBlock& Block)
	{
		// Segunda ampliación del autor: la imagen no va en el texto; el subtítulo, entero con su marca.
		if (Block.Kind == EHowToPlayBlockKind::Image)
		{
			return FString();
		}
		if (Block.Kind == EHowToPlayBlockKind::Subtitle)
		{
			return TEXT("<Subtitle>") + EscapeRichText(Block.Text) + TEXT("</>");
		}
		// Un bloque hecho a mano sin tramos (la página "Teclas", un test) es un solo tramo normal.
		TArray<FHowToPlaySpan> Spans = Block.Spans;
		if (Spans.IsEmpty())
		{
			AddSpan(Spans, Block.Text, false);
		}
		// El texto enriquecido de UMG no anida marcas: dentro de la frase del Warden, su negrita cierra <Warden> y abre <WardenBold>.
		const bool bWarden = Block.Kind == EHowToPlayBlockKind::WardenLine;
		FString Rich;
		for (const FHowToPlaySpan& Span : Spans)
		{
			const FString Escaped = EscapeRichText(Span.Text);
			const TCHAR* Tag = bWarden ? (Span.bBold ? TEXT("WardenBold") : TEXT("Warden")) : (Span.bBold ? TEXT("Bold") : nullptr);
			Rich += Tag ? FString::Printf(TEXT("<%s>%s</>"), Tag, *Escaped) : Escaped;
		}
		return Rich;
	}

	FString HowToPlayRichText(const FHowToPlayPage& Page)
	{
		return JoinBlocks(Page.Blocks, [](const FHowToPlayBlock& Block) { return HowToPlayBlockRichText(Block); });
	}
}
