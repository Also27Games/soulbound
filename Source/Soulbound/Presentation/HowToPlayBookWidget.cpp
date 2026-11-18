#include "HowToPlayBookWidget.h"

#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "GrimoireWidgets.h"
#include "HowToPlay.h"
#include "HowToPlayBookPanel.h"
#include "Misc/Paths.h"
#include "StatusText.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoulboundHowToPlayBook, Log, All);

void UHowToPlayBookWidget::GoToFirstPage()
{
	// R-HUD-22: la primera vista, para cuando se abre la portada; la pestaña de FGrimoireTabState no se usa.
	Pages.Page = 0;
	Refresh();
}

void UHowToPlayBookWidget::NextPage()
{
	EnsureLoaded();
	Pages.NextPage(Book.Pages.Num(), GetPagesPerView());
	Refresh();
}

void UHowToPlayBookWidget::PreviousPage()
{
	EnsureLoaded();
	Pages.PreviousPage(Book.Pages.Num(), GetPagesPerView());
	Refresh();
}

int32 UHowToPlayBookWidget::GetPage() const
{
	return Pages.Page;
}

int32 UHowToPlayBookWidget::GetPagesPerView() const
{
	// Como la vista: dos hojas por vista si la imagen del libro existe.
	return Soulbound::Presentation::HowToPlayPagesPerView(BookBrush.IsSet());
}

const Soulbound::Presentation::FHowToPlayBook& UHowToPlayBookWidget::GetBook() const
{
	return Book;
}

const Soulbound::Presentation::FHowToPlayBookSettings& UHowToPlayBookWidget::GetSettings() const
{
	return Settings;
}

FString UHowToPlayBookWidget::FootHint() const
{
	// R-HUD-22: solo las teclas de página, y solo si se atienden.
	return bPageKeys ? Soulbound::Presentation::HowToPlayMenuFootHint(Settings.Keys) : FString();
}

void UHowToPlayBookWidget::LoadBook(const FString& FilePath)
{
	// Declaraciones una a una, no "using namespace": en una compilación unity, otro archivo puede tener en su espacio anónimo un nombre igual.
	using Soulbound::Presentation::FGrimoireFont;
	using Soulbound::Presentation::FGrimoireTabState;
	using Soulbound::Presentation::GrimoireTextureBrush;
	using Soulbound::Presentation::HowToPlayBook;
	using Soulbound::Presentation::HowToPlayImagePath;
	using Soulbound::Presentation::LoadHowToPlayText;
	bLoaded = true;
	// R-HUD-22: los ajustes de la vista, sin ajustes propios; el libro, el mismo que lee el Grimorio (HowToPlayBook), con la página de teclas.
	Settings = UHowToPlayLibrary::ConfiguredBookSettings();
	Book = HowToPlayBook(LoadHowToPlayText(FilePath), Settings.Keys);
	// Empieza en la primera vista; no comparte página con nadie.
	Pages = FGrimoireTabState();

	// Como la vista (ABoardViewActor), con su patrón: lo que falta no es un error; se pinta sin ello y se dice una vez en el registro.
	Font = FGrimoireFont();
	Font.RegularTypeface = Settings.RegularTypeface;
	Font.BoldTypeface = Settings.BoldTypeface;
	Font.ItalicTypeface = Settings.ItalicTypeface;
	Font.BoldItalicTypeface = Settings.BoldItalicTypeface;
	FontAsset = nullptr;
	if (Settings.Font.IsNull())
	{
		UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): GrimoireFont está vacío; el texto sale con la fuente de siempre."));
	}
	else
	{
		FontAsset = Cast<UFont>(Settings.Font.TryLoad());
		if (FontAsset)
		{
			Font.Font = FontAsset;
		}
		else
		{
			UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): no existe la fuente %s; el texto sale con la fuente de siempre."), *Settings.Font.ToString());
		}
	}

	BookTexture = nullptr;
	BookBrush.Reset();
	if (Settings.BookTexture.IsNull())
	{
		UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): GrimoireBackgroundTexture está vacío; se pinta con el fondo negro de siempre."));
	}
	else
	{
		BookTexture = Cast<UTexture2D>(Settings.BookTexture.TryLoad());
		if (BookTexture)
		{
			BookBrush = GrimoireTextureBrush(BookTexture, ESlateBrushDrawType::Box, Settings.BookMargin);
		}
		else
		{
			UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): no existe la imagen del libro %s; se pinta con el fondo negro de siempre."),
				*Settings.BookTexture.ToString());
		}
	}

	WatermarkTexture = nullptr;
	WatermarkBrush.Reset();
	if (Settings.WatermarkTexture.IsNull())
	{
		UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): HowToPlayWatermarkTexture está vacío; las hojas van sin marca de agua."));
	}
	else
	{
		WatermarkTexture = Cast<UTexture2D>(Settings.WatermarkTexture.TryLoad());
		if (WatermarkTexture)
		{
			WatermarkBrush = GrimoireTextureBrush(WatermarkTexture, ESlateBrushDrawType::Image);
		}
		else
		{
			UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): no existe la marca de agua %s; las hojas van sin ella."), *Settings.WatermarkTexture.ToString());
		}
	}

	// Las láminas: cada nombre se carga una vez y, si no existe, se dice una vez (como ABoardViewActor::LoadHowToPlayImages).
	PageTextures.Reset();
	PageBrushes.Reset();
	TSet<FString> Seen;
	for (const FHowToPlayPage& Page : Book.Pages)
	{
		for (const FHowToPlayBlock& Block : Page.Blocks)
		{
			if (Block.Kind != EHowToPlayBlockKind::Image || Block.ImageName.IsEmpty() || Seen.Contains(Block.ImageName))
			{
				continue;
			}
			Seen.Add(Block.ImageName);
			const FString Path = HowToPlayImagePath(Settings.ImageFolder, Block.ImageName);
			UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
			if (!Texture)
			{
				UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): no existe la imagen %s (página \"%s\"); su bloque no se pinta."), *Path, *Page.Title);
				continue;
			}
			PageTextures.Add(Texture);
			PageBrushes.Add(Block.ImageName, GrimoireTextureBrush(Texture, ESlateBrushDrawType::Image));
		}
	}
}

void UHowToPlayBookWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Piece.Reset();
}

#if WITH_EDITOR
const FText UHowToPlayBookWidget::GetPaletteCategory()
{
	return FText::FromString(TEXT("Soulbound"));
}
#endif

TSharedRef<SWidget> UHowToPlayBookWidget::RebuildWidget()
{
	EnsureLoaded();
	const TWeakObjectPtr<UHowToPlayBookWidget> Weak(this);
	Piece = SNew(SHowToPlayBookPanel)
		.FontSize_Lambda([Weak]() { return Weak.IsValid() ? Weak->MenuFontSize() : 1; })
		.Font(Font)
		.WardenColor(Settings.WardenColor)
		.ImageMaxHeight(Settings.ImageMaxHeight)
		// Los pinceles no cambian después de cargar: la pieza puede apuntarlos.
		.ImageForPage([Weak](const FString& ImageName) -> const FSlateBrush* { return Weak.IsValid() ? Weak->PageBrushes.Find(ImageName) : nullptr; })
		.OnPreviousPage_UObject(this, &UHowToPlayBookWidget::PreviousPage)
		.OnNextPage_UObject(this, &UHowToPlayBookWidget::NextPage)
		.Framed(true)
		.BookBrush(BookBrush)
		.PageInset(Settings.PageInset)
		.SpineGap(Settings.SpineGap)
		.Watermark(WatermarkBrush)
		.WatermarkOpacity(Settings.WatermarkOpacity)
		.WatermarkSize(Settings.WatermarkSize)
		.FootHint(FootHint())
		.PageKeys(bPageKeys)
		.PreviousPageKey(Settings.PreviousPageKey)
		.NextPageKey(Settings.NextPageKey);
	Piece->SetHowToPlay(Book, Pages.Page);
	return Piece.ToSharedRef();
}

void UHowToPlayBookWidget::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	LoadBook(FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::HowToPlayRelativePath));
	// Como la vista: si el archivo no carga, la pieza enseña el mensaje del cargador y se dice en el registro.
	if (!Book.IsLoaded())
	{
		UE_LOG(LogSoulboundHowToPlayBook, Warning, TEXT("Cómo se juega (menú): %s"), *Book.ErrorMessage);
	}
}

void UHowToPlayBookWidget::Refresh()
{
	if (Piece.IsValid())
	{
		Piece->SetHowToPlay(Book, Pages.Page);
	}
}

int32 UHowToPlayBookWidget::MenuFontSize() const
{
	// R-HUD-22: el criterio de la vista (ABoardViewActor::UpdatePanelScale), con la pantalla del juego; en el diseñador, que no tiene, a 1080.
	float Height = 1080.f;
	float Dpi = 1.f;
	const UWorld* World = GetWorld();
	UGameViewportClient* Client = World ? World->GetGameViewport() : nullptr;
	if (!Client && GEngine)
	{
		Client = GEngine->GameViewport;
	}
	if (Client)
	{
		FVector2D Size = FVector2D::ZeroVector;
		Client->GetViewportSize(Size);
		if (Size.Y > 0.0)
		{
			Height = static_cast<float>(Size.Y);
			const UUserInterfaceSettings* UiSettings = GetDefault<UUserInterfaceSettings>();
			Dpi = UiSettings ? UiSettings->GetDPIScaleBasedOnSize(FIntPoint(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y))) : 1.f;
		}
	}
	return Soulbound::Presentation::ScaledPanelSlateFontSize(Height, Dpi, Settings.PanelFontSize, Settings.PanelMinFontSize);
}
