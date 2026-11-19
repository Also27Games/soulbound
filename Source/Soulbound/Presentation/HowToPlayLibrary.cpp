#include "HowToPlayLibrary.h"

#include "BoardViewActor.h"
#include "Engine/Texture2D.h"
#include "HowToPlay.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundHowToPlay, Log, All);

namespace
{
	/** El libro del archivo del repositorio, con las teclas de la vista. Se lee cada vez: es un archivo pequeño y el menú lo pide al montarse. */
	Soulbound::Presentation::FHowToPlayBook RepositoryBook()
	{
		const Soulbound::Presentation::FHowToPlayBook Book =
			UHowToPlayLibrary::BookFromFile(FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::HowToPlayRelativePath));
		if (!Book.IsLoaded())
		{
			UE_LOG(LogSoulboundHowToPlay, Error, TEXT("%s"), *Book.ErrorMessage);
		}
		return Book;
	}
}

Soulbound::Presentation::FHowToPlayBook UHowToPlayLibrary::BookFromFile(const FString& FilePath)
{
	// Sin registro: lo dice quien lo usa (RepositoryBook), para que un test con un archivo roto no ensucie el registro con un error.
	return Soulbound::Presentation::HowToPlayBook(Soulbound::Presentation::LoadHowToPlayText(FilePath), ConfiguredKeyNames());
}

Soulbound::Presentation::FGrimoireKeyNames UHowToPlayLibrary::ConfiguredKeyNames()
{
	// R-HUD-19: las teclas de la partida, del objeto por defecto de la vista (lee la misma sección de DefaultGame.ini).
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	return View ? View->GrimoireKeyNames() : Soulbound::Presentation::FGrimoireKeyNames();
}

Soulbound::Presentation::FHowToPlayBookSettings UHowToPlayLibrary::ConfiguredBookSettings()
{
	// R-HUD-22: todo del objeto por defecto de la vista, que lee la misma sección de DefaultGame.ini que la partida.
	Soulbound::Presentation::FHowToPlayBookSettings Settings;
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	if (!View)
	{
		return Settings;
	}
	Settings.Font = View->GrimoireFont;
	Settings.RegularTypeface = View->GrimoireFontRegularTypeface;
	Settings.BoldTypeface = View->GrimoireFontBoldTypeface;
	Settings.ItalicTypeface = View->GrimoireFontItalicTypeface;
	Settings.BoldItalicTypeface = View->GrimoireFontBoldItalicTypeface;
	Settings.BookTexture = View->GrimoireBackgroundTexture;
	Settings.BookMargin = View->GrimoireBackgroundMargin;
	Settings.PageInset = View->GrimoirePageInset;
	Settings.SpineGap = View->GrimoireSpineGap;
	Settings.WatermarkTexture = View->HowToPlayWatermarkTexture;
	Settings.WatermarkOpacity = View->HowToPlayWatermarkOpacity;
	Settings.WatermarkSize = View->HowToPlayWatermarkSize;
	Settings.ImageMaxHeight = View->HowToPlayImageMaxHeight;
	Settings.ImageFolder = View->HowToPlayImageFolder;
	Settings.WardenColor = View->WardenLineColor;
	Settings.Keys = View->GrimoireKeyNames();
	Settings.PreviousPageKey = View->GrimoirePreviousPageKey;
	Settings.NextPageKey = View->GrimoireNextPageKey;
	Settings.PanelFontSize = View->PanelFontSize;
	Settings.PanelMinFontSize = View->PanelMinFontSize;
	return Settings;
}

FText UHowToPlayLibrary::HowToPlayText()
{
	// Precisión del autor del 17/11/2026 (R-HUD-19): el widget de hoy del menú lee un solo bloque de texto; le llega el libro aplanado.
	return FText::FromString(Soulbound::Presentation::HowToPlayFlatText(RepositoryBook()));
}

FText UHowToPlayLibrary::HowToPlayBookTitle()
{
	return FText::FromString(RepositoryBook().Title);
}

FText UHowToPlayLibrary::HowToPlayCoverLine()
{
	return FText::FromString(RepositoryBook().CoverLine);
}

int32 UHowToPlayLibrary::HowToPlayPageCount()
{
	return RepositoryBook().Pages.Num();
}

FHowToPlayPage UHowToPlayLibrary::HowToPlayPage(int32 Index)
{
	const Soulbound::Presentation::FHowToPlayBook Book = RepositoryBook();
	return Book.Pages.IsValidIndex(Index) ? Book.Pages[Index] : FHowToPlayPage();
}

FText UHowToPlayLibrary::HowToPlayPageRichText(int32 Index)
{
	const Soulbound::Presentation::FHowToPlayBook Book = RepositoryBook();
	return Book.Pages.IsValidIndex(Index) ? FText::FromString(Soulbound::Presentation::HowToPlayRichText(Book.Pages[Index])) : FText();
}

FText UHowToPlayLibrary::HowToPlayBlockRichText(const FHowToPlayBlock& Block)
{
	return FText::FromString(Soulbound::Presentation::HowToPlayBlockRichText(Block));
}

UTexture2D* UHowToPlayLibrary::HowToPlayImage(const FString& ImageName)
{
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	const FString Path = Soulbound::Presentation::HowToPlayImagePath(View ? View->HowToPlayImageFolder : FString(TEXT("/Game/Art/Ayuda/")), ImageName);
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
	if (!Texture)
	{
		// R-HUD-17: no es un error; el bloque no se pinta y se dice una vez por nombre.
		static TSet<FString> Warned;
		if (!Warned.Contains(Path))
		{
			Warned.Add(Path);
			UE_LOG(LogSoulboundHowToPlay, Warning, TEXT("Cómo se juega: no existe la imagen %s; su bloque no se pinta."), *Path);
		}
	}
	return Texture;
}

FText UHowToPlayLibrary::HowToPlayError()
{
	return FText::FromString(RepositoryBook().ErrorMessage);
}
