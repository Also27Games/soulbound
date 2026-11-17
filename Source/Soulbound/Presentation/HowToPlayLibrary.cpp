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

UTexture2D* UHowToPlayLibrary::HowToPlayPageImage(int32 Index)
{
	const FHowToPlayPage Page = HowToPlayPage(Index);
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	const FString Path = Soulbound::Presentation::HowToPlayImagePath(View ? View->HowToPlayImageFolder : FString(TEXT("/Game/Art/Ayuda/")), Page.ImageName);
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
	if (!Texture)
	{
		// R-HUD-17: no es un error; la página sale sin imagen y se dice una vez por nombre.
		static TSet<FString> Warned;
		if (!Warned.Contains(Path))
		{
			Warned.Add(Path);
			UE_LOG(LogSoulboundHowToPlay, Warning, TEXT("Cómo se juega: no existe la imagen %s de la página \"%s\"; sale sin imagen."), *Path, *Page.Title);
		}
	}
	return Texture;
}

FText UHowToPlayLibrary::HowToPlayError()
{
	return FText::FromString(RepositoryBook().ErrorMessage);
}
