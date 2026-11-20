// Tests del libro de "Cómo se juega" como pieza propia y en el menú (S-049, R-HUD-21 y R-HUD-22): lo que enseña una vista, los ajustes que lee el menú
// de la vista, las páginas del widget del menú, su pie y el archivo roto; desde la ronda de correcciones, las carpetas que se cocinan y las capas. Sin
// mundo, sin editor y sin FSlateApplication: el widget de UMG se crea en el paquete temporal y su pieza de Slate se recorre sin pintarla. Filtros:
// "Soulbound.Game.Presentation.Grimorio" y "Soulbound.Game.Presentation.ComoSeJuega".

#include "../../Presentation/BoardViewActor.h"
#include "../../Presentation/GrimoireBook.h"
#include "../../Presentation/HowToPlay.h"
#include "../../Presentation/HowToPlayBookPanel.h"
#include "../../Presentation/HowToPlayBookWidget.h"
#include "../../Presentation/HowToPlayLibrary.h"
#include "../../Presentation/StatusText.h"
#include "../../Presentation/ViewLayers.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags HbTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Los textos de STextBlock y SRichTextBlock de un widget y de sus hijos, en su orden, sin pintarlo; lo que no se ve no cuenta. */
	void HbCollect(const TSharedRef<SWidget>& Widget, TArray<FString>& Out)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		if (Widget->GetType() == TEXT("STextBlock"))
		{
			Out.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
		}
		else if (Widget->GetType() == TEXT("SRichTextBlock"))
		{
			Out.Add(StaticCastSharedRef<SRichTextBlock>(Widget)->GetText().ToString());
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
		{
			HbCollect(Children->GetChildAt(Index), Out);
		}
	}

	TArray<FString> HbTexts(const TSharedRef<SWidget>& Widget)
	{
		TArray<FString> Out;
		HbCollect(Widget, Out);
		return Out;
	}

	/** Unas teclas de prueba, distintas de las de la configuración. */
	Soulbound::Presentation::FGrimoireKeyNames HbKeys()
	{
		Soulbound::Presentation::FGrimoireKeyNames Keys;
		Keys.Grimoire = TEXT("G");
		Keys.PreviousTab = TEXT("Q");
		Keys.NextTab = TEXT("E");
		Keys.PreviousPage = TEXT("Up");
		Keys.NextPage = TEXT("Down");
		return Keys;
	}

	/** Un widget del menú en el paquete temporal, sin mundo. */
	UHowToPlayBookWidget* HbNewWidget()
	{
		return NewObject<UHowToPlayBookWidget>(GetTransientPackage());
	}

	FString HbRepositoryFile()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::HowToPlayRelativePath);
	}
}

// CA-HUD-70 (R-HUD-21; S-049), la parte pura: lo que enseña la pieza en una vista lo da HowToPlayView, apoyada en HowToPlayViewStart,
// HowToPlayPageFooter y HowToPlayPageButtons. Con el libro y cinco páginas, las vistas son (1, 2), (3, 4) y (5, nada); sin él, de una en una. En la
// primera vista no se ve "Anterior" y en la última no se ve "Siguiente". Una página fuera del libro va a la más cercana; sin páginas, nada. Que la
// pestaña construida con la pieza enseñe lo mismo que antes lo comprueba PestanasConstruidasMismosTextos.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayViewTest, "Soulbound.Game.Presentation.Grimorio.VistaDelLibro", HbTestFlags)
bool FHowToPlayViewTest::RunTest(const FString& Parameters)
{
	using namespace Soulbound::Presentation;
	const auto Check = [this](const FString& Case, const FHowToPlayView& View, int32 Left, int32 Right, const FString& Footer, bool bPrevious, bool bNext)
	{
		TestEqual(Case + TEXT(": hoja izquierda"), View.LeftPage, Left);
		TestEqual(Case + TEXT(": hoja derecha"), View.RightPage, Right);
		TestEqual(Case + TEXT(": N de M"), View.Footer, Footer);
		TestEqual(Case + TEXT(": Anterior"), View.Buttons.bPrevious, bPrevious);
		TestEqual(Case + TEXT(": Siguiente"), View.Buttons.bNext, bNext);
	};
	Check(TEXT("Con el libro, página 1"), HowToPlayView(0, 5, true), 0, 1, TEXT("1-2 de 5"), false, true);
	Check(TEXT("Con el libro, página 2"), HowToPlayView(1, 5, true), 0, 1, TEXT("1-2 de 5"), false, true);
	Check(TEXT("Con el libro, página 4"), HowToPlayView(3, 5, true), 2, 3, TEXT("3-4 de 5"), true, true);
	Check(TEXT("Con el libro, página 5"), HowToPlayView(4, 5, true), 4, INDEX_NONE, TEXT("5 de 5"), true, false);
	Check(TEXT("Con el libro, más allá del final"), HowToPlayView(99, 5, true), 4, INDEX_NONE, TEXT("5 de 5"), true, false);
	Check(TEXT("Con el libro, antes del principio"), HowToPlayView(-3, 5, true), 0, 1, TEXT("1-2 de 5"), false, true);
	Check(TEXT("Sin el libro, página 1"), HowToPlayView(0, 5, false), 0, INDEX_NONE, TEXT("1 de 5"), false, true);
	Check(TEXT("Sin el libro, página 3"), HowToPlayView(2, 5, false), 2, INDEX_NONE, TEXT("3 de 5"), true, true);
	Check(TEXT("Sin el libro, página 5"), HowToPlayView(4, 5, false), 4, INDEX_NONE, TEXT("5 de 5"), true, false);
	Check(TEXT("Sin páginas"), HowToPlayView(0, 0, true), INDEX_NONE, INDEX_NONE, FString(), false, false);
	return true;
}

// CA-HUD-71 (R-HUD-22; S-049): la función que reúne los ajustes del libro para el menú devuelve los de la Config de la vista (su objeto por defecto):
// la fuente y sus cuatro tipos, la imagen del libro y sus márgenes, el margen de la página, el hueco del lomo, la marca de agua con su opacidad y su
// tamaño, el alto máximo de una lámina, la carpeta de las imágenes, el color del Warden, los nombres y las teclas de página y el criterio de la letra.
// El color del Warden y la letra son de Config desde S-049 (decisión del autor del 18/11/2026). El widget del menú se pinta con esos mismos ajustes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayBookSettingsTest, "Soulbound.Game.Presentation.ComoSeJuega.LibroDelMenuAjustesDeLaVista", HbTestFlags)
bool FHowToPlayBookSettingsTest::RunTest(const FString& Parameters)
{
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	if (!TestNotNull(TEXT("La vista tiene objeto por defecto"), View))
	{
		return false;
	}
	const Soulbound::Presentation::FHowToPlayBookSettings Settings = UHowToPlayLibrary::ConfiguredBookSettings();
	TestEqual(TEXT("La fuente"), Settings.Font, View->GrimoireFont);
	TestEqual(TEXT("El tipo normal"), Settings.RegularTypeface, View->GrimoireFontRegularTypeface);
	TestEqual(TEXT("El tipo negrita"), Settings.BoldTypeface, View->GrimoireFontBoldTypeface);
	TestEqual(TEXT("El tipo cursiva"), Settings.ItalicTypeface, View->GrimoireFontItalicTypeface);
	TestEqual(TEXT("El tipo negrita cursiva"), Settings.BoldItalicTypeface, View->GrimoireFontBoldItalicTypeface);
	TestEqual(TEXT("La imagen del libro"), Settings.BookTexture, View->GrimoireBackgroundTexture);
	TestTrue(TEXT("Los márgenes del libro"), Settings.BookMargin == View->GrimoireBackgroundMargin);
	TestEqual(TEXT("El margen de la página"), Settings.PageInset, View->GrimoirePageInset);
	TestEqual(TEXT("El hueco del lomo"), Settings.SpineGap, View->GrimoireSpineGap);
	TestEqual(TEXT("La marca de agua"), Settings.WatermarkTexture, View->HowToPlayWatermarkTexture);
	TestEqual(TEXT("Su opacidad"), Settings.WatermarkOpacity, View->HowToPlayWatermarkOpacity);
	TestEqual(TEXT("Su tamaño"), Settings.WatermarkSize, View->HowToPlayWatermarkSize);
	TestEqual(TEXT("El alto máximo de una lámina"), Settings.ImageMaxHeight, View->HowToPlayImageMaxHeight);
	TestEqual(TEXT("La carpeta de las imágenes"), Settings.ImageFolder, View->HowToPlayImageFolder);
	TestEqual(TEXT("El color del Warden"), Settings.WardenColor, View->WardenLineColor);
	const Soulbound::Presentation::FGrimoireKeyNames Keys = View->GrimoireKeyNames();
	TestTrue(TEXT("Los nombres de las teclas"), Settings.Keys.Grimoire == Keys.Grimoire && Settings.Keys.PreviousTab == Keys.PreviousTab
		&& Settings.Keys.NextTab == Keys.NextTab && Settings.Keys.View == Keys.View && Settings.Keys.Overlay == Keys.Overlay
		&& Settings.Keys.RingStart == Keys.RingStart && Settings.Keys.ReturnToMenu == Keys.ReturnToMenu && Settings.Keys.PreviousPage == Keys.PreviousPage
		&& Settings.Keys.NextPage == Keys.NextPage);
	TestEqual(TEXT("La tecla de la página anterior"), Settings.PreviousPageKey, View->GrimoirePreviousPageKey);
	TestEqual(TEXT("La de la siguiente"), Settings.NextPageKey, View->GrimoireNextPageKey);
	TestEqual(TEXT("La letra a 1080"), Settings.PanelFontSize, View->PanelFontSize);
	TestEqual(TEXT("La letra mínima"), Settings.PanelMinFontSize, View->PanelMinFontSize);

	// Decisión del autor del 18/11/2026: el color del Warden y la letra pasan a Config, para que el menú los lea del mismo sitio que la partida.
	for (const TCHAR* Name : {TEXT("WardenLineColor"), TEXT("PanelFontSize"), TEXT("PanelMinFontSize")})
	{
		const FProperty* Property = FindFProperty<FProperty>(ABoardViewActor::StaticClass(), Name);
		TestTrue(FString::Printf(TEXT("%s es de Config"), Name), Property && Property->HasAnyPropertyFlags(CPF_Config));
	}

	// El widget del menú se pinta con esos ajustes.
	UHowToPlayBookWidget* Widget = HbNewWidget();
	Widget->LoadBook(HbRepositoryFile());
	TestEqual(TEXT("El widget: la fuente de la vista"), Widget->GetSettings().Font, Settings.Font);
	TestEqual(TEXT("El widget: la imagen del libro de la vista"), Widget->GetSettings().BookTexture, Settings.BookTexture);
	TestEqual(TEXT("El widget: la marca de agua de la vista"), Widget->GetSettings().WatermarkTexture, Settings.WatermarkTexture);
	TestEqual(TEXT("El widget: el color del Warden de la vista"), Widget->GetSettings().WardenColor, Settings.WardenColor);
	return true;
}

// CA-HUD-72 (R-HUD-22; S-049): el libro del menú, con el archivo del repositorio y la imagen del libro de la configuración, empieza en la primera vista;
// "Siguiente" avanza una vista (dos hojas) y se queda en la última; "Anterior" retrocede una y se queda en la primera; GoToFirstPage la deja en la
// primera desde cualquiera. Un widget nuevo empieza otra vez en la primera: no comparte página con ningún otro.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayMenuPagesTest, "Soulbound.Game.Presentation.ComoSeJuega.LibroDelMenuPaginas", HbTestFlags)
bool FHowToPlayMenuPagesTest::RunTest(const FString& Parameters)
{
	UHowToPlayBookWidget* Widget = HbNewWidget();
	Widget->LoadBook(HbRepositoryFile());
	const int32 Count = Widget->GetBook().Pages.Num();
	if (!TestTrue(TEXT("El libro del repositorio tiene páginas"), Count > 4))
	{
		return false;
	}
	TestEqual(TEXT("Con la imagen del libro, dos hojas por vista"), Widget->GetPagesPerView(), 2);
	const int32 PerView = FMath::Max(Widget->GetPagesPerView(), 1);
	const int32 Last = Soulbound::Presentation::HowToPlayViewStart(Count - 1, PerView);

	TestEqual(TEXT("Empieza en la primera vista"), Widget->GetPage(), 0);
	Widget->NextPage();
	TestEqual(TEXT("Siguiente avanza una vista"), Widget->GetPage(), PerView);
	Widget->NextPage();
	TestEqual(TEXT("Y otra"), Widget->GetPage(), 2 * PerView);
	for (int32 Step = 0; Step < Count + 3; ++Step)
	{
		Widget->NextPage();
	}
	TestEqual(TEXT("Se queda en la última"), Widget->GetPage(), Last);
	Widget->PreviousPage();
	TestEqual(TEXT("Anterior retrocede una vista"), Widget->GetPage(), Last - PerView);
	for (int32 Step = 0; Step < Count + 3; ++Step)
	{
		Widget->PreviousPage();
	}
	TestEqual(TEXT("Se queda en la primera"), Widget->GetPage(), 0);

	Widget->GoToFirstPage();
	TestEqual(TEXT("Volver a la primera desde la primera"), Widget->GetPage(), 0);
	Widget->NextPage();
	Widget->NextPage();
	Widget->NextPage();
	Widget->GoToFirstPage();
	TestEqual(TEXT("Volver a la primera desde una de en medio"), Widget->GetPage(), 0);
	for (int32 Step = 0; Step < Count; ++Step)
	{
		Widget->NextPage();
	}
	Widget->GoToFirstPage();
	TestEqual(TEXT("Volver a la primera desde la última"), Widget->GetPage(), 0);

	Widget->NextPage();
	UHowToPlayBookWidget* Other = HbNewWidget();
	Other->LoadBook(HbRepositoryFile());
	TestEqual(TEXT("Otro widget empieza en la primera"), Other->GetPage(), 0);
	TestEqual(TEXT("Y no mueve al primero"), Widget->GetPage(), PerView);
	return true;
}

// CA-HUD-73 (R-HUD-22; S-049): en el menú, el pie no nombra la tecla de cerrar ni las de pestañas; como mucho, las de página. La línea del menú es la
// parte de páginas del pie del Grimorio en "Cómo se juega"; el widget la da con las teclas encendidas y ninguna con ellas apagadas. El libro del menú
// construido no lleva el título "Grimorio", ni pestañas, ni la parte de cerrar y pestañas del pie.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayMenuFootTest, "Soulbound.Game.Presentation.ComoSeJuega.LibroDelMenuPie", HbTestFlags)
bool FHowToPlayMenuFootTest::RunTest(const FString& Parameters)
{
	using namespace Soulbound::Presentation;
	const FGrimoireKeyNames Keys = HbKeys();
	const FGrimoireFootHints Grimoire = GrimoireFootHints(Keys, EGrimoireTab::HowToPlay);
	const FString Hint = HowToPlayMenuFootHint(Keys);
	TestFalse(TEXT("La línea del menú dice algo"), Hint.IsEmpty());
	TestEqual(TEXT("La línea del menú es la de páginas del Grimorio"), Hint, Grimoire.Right);
	TestFalse(TEXT("No dice la parte de cerrar y pestañas"), Hint.Contains(Grimoire.Left));
	TestFalse(TEXT("Ni la frase de pestañas"), Hint.Contains(TEXT("pestañas")));
	TestFalse(TEXT("Ni la de cerrar"), Hint.Contains(TEXT("cerrar")));

	UHowToPlayBookWidget* Widget = HbNewWidget();
	Widget->LoadBook(HbRepositoryFile());
	TestEqual(TEXT("El widget, con las teclas: la línea de páginas"), Widget->FootHint(), HowToPlayMenuFootHint(Widget->GetSettings().Keys));
	Widget->bPageKeys = false;
	TestTrue(TEXT("El widget, sin las teclas: ninguna"), Widget->FootHint().IsEmpty());

	// El libro del menú construido, con un libro corto y sin imagen del libro.
	FHowToPlayBook Book = HowToPlayBook(ParseHowToPlayText(TEXT("# Libro\n\n## Uno\n\nTexto uno.\n")), Keys);
	const TSharedRef<SHowToPlayBookPanel> Piece = SNew(SHowToPlayBookPanel).Framed(true).FontSize(14).FootHint(Hint);
	Piece->SetHowToPlay(Book, 0);
	const TArray<FString> Texts = HbTexts(Piece);
	TestTrue(TEXT("Construido: dice la línea de páginas"), Texts.Contains(Hint));
	TestTrue(TEXT("Construido: enseña su página"), Texts.Contains(TEXT("Uno")));
	TestFalse(TEXT("Construido: sin el título del Grimorio"), Texts.Contains(GrimoireTitleText));
	for (const EGrimoireTab Tab : GrimoireTabs())
	{
		TestFalse(TEXT("Construido: sin pestañas: ") + GrimoireTabTitle(Tab), Texts.Contains(GrimoireTabTitle(Tab)));
	}
	TestFalse(TEXT("Construido: sin la parte de cerrar y pestañas"), Texts.ContainsByPredicate([&Grimoire](const FString& Text) { return Text.Contains(Grimoire.Left); }));
	return true;
}

// CA-HUD-74 (R-HUD-22; S-049): con el archivo roto, el libro del menú tiene cero páginas y el mensaje del cargador; pasar página y volver a la primera
// no fallan, y el widget construido enseña ese mensaje.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayMenuBrokenTest, "Soulbound.Game.Presentation.ComoSeJuega.LibroDelMenuArchivoRoto", HbTestFlags)
bool FHowToPlayMenuBrokenTest::RunTest(const FString& Parameters)
{
	const FString Broken = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests"), TEXT("como-se-juega-menu-roto.md"));
	if (!TestTrue(TEXT("Se escribe el archivo roto"), FFileHelper::SaveStringToFile(FString(TEXT("Un texto sin ningún título.\n")), *Broken,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))
	{
		return false;
	}
	const FString Message = Soulbound::Presentation::LoadHowToPlayText(Broken).Message;
	UHowToPlayBookWidget* Widget = HbNewWidget();
	Widget->LoadBook(Broken);
	IFileManager::Get().Delete(*Broken);

	TestFalse(TEXT("El cargador da un mensaje"), Message.IsEmpty());
	TestEqual(TEXT("Cero páginas"), Widget->GetBook().Pages.Num(), 0);
	TestEqual(TEXT("El mensaje del cargador"), Widget->GetBook().ErrorMessage, Message);
	Widget->NextPage();
	Widget->PreviousPage();
	Widget->GoToFirstPage();
	TestEqual(TEXT("Pasar página no lo mueve de la primera"), Widget->GetPage(), 0);
	const TArray<FString> Texts = HbTexts(Widget->TakeWidget());
	TestTrue(TEXT("Construido: enseña el mensaje"), Texts.Contains(Message));
	return true;
}

// R-HUD-22 (S-049), el criterio de la letra: el libro del menú y el Grimorio de la partida calculan la letra con la misma función, la de la vista
// (PanelFontSize a 1080 de alto, nunca menos de PanelMinFontSize, dividido por la escala de DPI de Unreal); la vista da lo mismo que antes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayMenuFontTest, "Soulbound.Game.Presentation.ComoSeJuega.LetraDelLibroDelMenu", HbTestFlags)
bool FHowToPlayMenuFontTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::ScaledPanelSlateFontSize;
	TestEqual(TEXT("A 1080 y sin escala, la de 1080"), ScaledPanelSlateFontSize(1080.f, 1.f, 13, 9), 13);
	TestEqual(TEXT("A 2160 y escala 2, la misma en Slate"), ScaledPanelSlateFontSize(2160.f, 2.f, 13, 9), 13);
	// A 768 de alto, 13 * 768 / 1080 = 9,24, que redondea a 9 en pantalla; con la escala de 0,711 de 1024x768, 12,66, que redondea a 13 en Slate.
	TestEqual(TEXT("A 1024x768"), ScaledPanelSlateFontSize(768.f, 0.711f, 13, 9), 13);
	// A 400 de alto se queda en el mínimo de 9 en pantalla.
	TestEqual(TEXT("El mínimo"), ScaledPanelSlateFontSize(400.f, 1.f, 13, 9), 9);
	TestEqual(TEXT("Una escala no válida cuenta como 1"), ScaledPanelSlateFontSize(1080.f, 0.f, 13, 9), 13);
	TestEqual(TEXT("Nunca menos de 1"), ScaledPanelSlateFontSize(1080.f, 100.f, 13, 0), 1);
	return true;
}

namespace
{
	/** Las carpetas de DirectoriesToAlwaysCook de la Config del juego (DefaultGame.ini), sin la barra final. */
	TArray<FString> HbAlwaysCookedDirectories()
	{
		TArray<FString> Entries;
		GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("DirectoriesToAlwaysCook"), Entries, GGameIni);
		TArray<FString> Directories;
		for (const FString& Entry : Entries)
		{
			FString Path;
			if (FParse::Value(*Entry, TEXT("Path="), Path))
			{
				Path.RemoveFromEnd(TEXT("/"));
				Directories.Add(Path);
			}
		}
		return Directories;
	}

	/** Si una ruta de paquete o de carpeta ("/Game/Art/Ayuda/" o "/Game/Art/Ayuda/T_Ayuda_Sello") cae dentro de alguna de esas carpetas. */
	bool HbIsCooked(FString Path, const TArray<FString>& Directories)
	{
		Path.RemoveFromEnd(TEXT("/"));
		return Directories.ContainsByPredicate([&Path](const FString& Directory) { return Path == Directory || Path.StartsWith(Directory + TEXT("/")); });
	}
}

// CA-HUD-75 (R-HUD-22, ronda de correcciones del 19/11/2026): la vista y el libro del menú cargan por ruta, en tiempo de ejecución, lo que dicen sus
// propiedades de Config; el cocinador no sigue esas rutas. Cada una con valor cae dentro de una carpeta de DirectoriesToAlwaysCook. Se recorren por
// reflexión todas las propiedades de Config de la vista que son rutas de asset (FSoftObjectPath, TSoftObjectPtr y las cadenas que empiezan por
// "/Game/", como la carpeta de las láminas), para que una ruta nueva no se escape. Desde S-050, CA-VOZ-18 (R-VOZ-12, cocinado): el sonido del corte
// (CutSound) es una de esas rutas.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayCookedDirectoriesTest, "Soulbound.Game.Presentation.ComoSeJuega.CarpetasQueSeCocinan", HbTestFlags)
bool FHowToPlayCookedDirectoriesTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Directories = HbAlwaysCookedDirectories();
	if (!TestTrue(TEXT("Hay carpetas en DirectoriesToAlwaysCook"), Directories.Num() > 0))
	{
		return false;
	}
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	TSet<FString> Checked;
	// Las rutas de asset de la Config, también las vacías: una ruta vacía no se comprueba hasta que se pone (S-050, CA-VOZ-18).
	TSet<FString> AssetPaths;
	for (TFieldIterator<FProperty> It(ABoardViewActor::StaticClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}
		FString Path;
		if (const FStructProperty* Struct = CastField<FStructProperty>(Property); Struct && Struct->Struct == TBaseStructure<FSoftObjectPath>::Get())
		{
			AssetPaths.Add(Property->GetName());
			Path = Struct->ContainerPtrToValuePtr<FSoftObjectPath>(View)->GetLongPackageName();
		}
		else if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			AssetPaths.Add(Property->GetName());
			Path = Soft->GetPropertyValue_InContainer(View).ToSoftObjectPath().GetLongPackageName();
		}
		else if (const FStrProperty* String = CastField<FStrProperty>(Property))
		{
			const FString& Value = String->GetPropertyValue_InContainer(View);
			Path = Value.StartsWith(TEXT("/Game/")) ? Value : FString();
		}
		if (Path.IsEmpty())
		{
			continue;
		}
		Checked.Add(Property->GetName());
		TestTrue(FString::Printf(TEXT("%s (%s) está en una carpeta de DirectoriesToAlwaysCook"), *Property->GetName(), *Path), HbIsCooked(Path, Directories));
	}
	// Que el recorrido no pase en vacío: las rutas de esta ronda están entre las que mira.
	for (const TCHAR* Name : {TEXT("GrimoireFont"), TEXT("GrimoireBackgroundTexture"), TEXT("HowToPlayWatermarkTexture"), TEXT("HowToPlayImageFolder"), TEXT("DiceFacesTexture")})
	{
		TestTrue(FString::Printf(TEXT("Se mira %s"), Name), Checked.Contains(Name));
	}
	// CA-VOZ-18 (S-050, cocinado; R-VOZ-12): el sonido del corte es una ruta de asset de la Config, así que el recorrido la comprueba en cuanto el autor la
	// pone (vacía de inicio). Su carpeta, /Game/Audio, ya está en DirectoriesToAlwaysCook.
	TestTrue(TEXT("CutSound es una ruta de asset de la Config que se recorre"), AssetPaths.Contains(TEXT("CutSound")));

	// Y las del libro del menú, que salen de esa misma Config.
	const Soulbound::Presentation::FHowToPlayBookSettings Settings = UHowToPlayLibrary::ConfiguredBookSettings();
	TestTrue(TEXT("Menú: la fuente"), HbIsCooked(Settings.Font.GetLongPackageName(), Directories));
	TestTrue(TEXT("Menú: la imagen del libro"), HbIsCooked(Settings.BookTexture.GetLongPackageName(), Directories));
	TestTrue(TEXT("Menú: la marca de agua"), HbIsCooked(Settings.WatermarkTexture.GetLongPackageName(), Directories));
	TestTrue(TEXT("Menú: la carpeta de las láminas"), HbIsCooked(Settings.ImageFolder, Directories));
	return true;
}

// CA-HUD-77 (R-HUD-11, precisión del autor del 19/11/2026): la interfaz de la vista va por debajo de los menús de la plantilla. El orden relativo entre
// las capas es el de antes (9 a 14, con la carta, el aviso y el anillo en el 13) y todas quedan por debajo del orden con que llega al viewport un
// widget de UMG añadido sin orden.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewLayersTest, "Soulbound.Game.Presentation.Grimorio.CapasDeLaVista", HbTestFlags)
bool FViewLayersTest::RunTest(const FString& Parameters)
{
	using Soulbound::Presentation::EViewLayer;
	using Soulbound::Presentation::ViewLayerZOrder;
	struct FBefore
	{
		EViewLayer Layer;
		const TCHAR* Name;
		int32 ZOrder;
	};
	const FBefore Before[] = {
		{EViewLayer::Dim, TEXT("oscurecido"), 9},
		{EViewLayer::Status, TEXT("HUD"), 10},
		{EViewLayer::WardenLine, TEXT("línea del Warden"), 11},
		{EViewLayer::ReturnToMenu, TEXT("Volver al menú"), 12},
		{EViewLayer::ElectCard, TEXT("carta del Elegido"), 13},
		{EViewLayer::Notice, TEXT("aviso"), 13},
		{EViewLayer::Ring, TEXT("anillo"), 13},
		{EViewLayer::Grimoire, TEXT("Grimorio"), 14},
	};
	const int32 Dim = ViewLayerZOrder(EViewLayer::Dim);
	TestEqual(TEXT("El oscurecido está en la base"), Dim, Soulbound::Presentation::ViewLayerBase);
	for (const FBefore& Layer : Before)
	{
		const int32 ZOrder = ViewLayerZOrder(Layer.Layer);
		TestEqual(FString::Printf(TEXT("%s: la misma distancia al oscurecido que antes"), Layer.Name), ZOrder - Dim, Layer.ZOrder - 9);
		TestTrue(FString::Printf(TEXT("%s (%d) por debajo de un widget de UMG sin orden (%d)"), Layer.Name, ZOrder, Soulbound::Presentation::DefaultViewportWidgetZOrder),
			ZOrder < Soulbound::Presentation::DefaultViewportWidgetZOrder);
	}
	return true;
}

#endif
