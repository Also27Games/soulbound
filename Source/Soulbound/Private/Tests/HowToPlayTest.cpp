// Tests del texto de "cómo se juega" (S-022; desde S-048, R-HUD-17 y R-HUD-19, un libro): el cargador del archivo de Content/Data/Textos, que lo
// lee como páginas, y la librería que da ese libro al menú. El texto es del autor: los tests del archivo del repositorio no miran su contenido,
// solo que es un libro válido. Sin mundo. Filtro: "Soulbound.Game.Presentation.ComoSeJuega".

#include "../../Presentation/BoardViewActor.h"
#include "../../Presentation/GrimoireBook.h"
#include "../../Presentation/HowToPlay.h"
#include "../../Presentation/HowToPlayLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags HowToPlayTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** El archivo del repositorio, con su ruta absoluta. */
	FString HowToPlayRepositoryFile()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), Soulbound::Presentation::HowToPlayRelativePath);
	}

	int32 HowToPlayErrorOf(const FHowToPlayLoadResult& Result)
	{
		return static_cast<int32>(Result.Error);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayRepositoryFileTest, "Soulbound.Game.Presentation.ComoSeJuega.ArchivoDelRepositorio", HowToPlayTestFlags)

bool FHowToPlayRepositoryFileTest::RunTest(const FString& Parameters)
{
	// CA-CSJ-1 (S-022; desde S-048, R-HUD-17): el cargador acepta el archivo del repositorio y lo lee como un libro: al menos una página, y todas
	// con título y texto. Desaparece la lista de títulos exigidos: el autor decide cuántas páginas hay y cómo se llaman.
	const FHowToPlayLoadResult Result = Soulbound::Presentation::LoadHowToPlayText(HowToPlayRepositoryFile());
	if (!TestTrue(FString::Printf(TEXT("El archivo del repositorio se carga (%s)"), *Result.Message), Result.IsSuccess()))
	{
		return false;
	}
	TestTrue(TEXT("Al menos una página"), Result.Pages.Num() >= 1);
	for (int32 Index = 0; Index < Result.Pages.Num(); ++Index)
	{
		TestFalse(FString::Printf(TEXT("La página %d tiene título"), Index), Result.Pages[Index].Title.TrimStartAndEnd().IsEmpty());
		TestFalse(FString::Printf(TEXT("La página %d tiene texto"), Index), Result.Pages[Index].Body.TrimStartAndEnd().IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayPagesTest, "Soulbound.Game.Presentation.ComoSeJuega.Paginas", HowToPlayTestFlags)

bool FHowToPlayPagesTest::RunTest(const FString& Parameters)
{
	// CA-HUD-30 (R-HUD-17): tres títulos de segundo nivel dan tres páginas, en orden, cada una con su título y su texto; los párrafos de una página se
	// conservan como párrafos. Precisiones del autor del 17/11/2026: las líneas de un mismo párrafo se unen con un espacio, y el texto suelto antes de
	// la primera "##" que no sea título ni portada se ignora.
	const FString Markdown = TEXT(
		"Texto suelto que se ignora.\n"
		"\n"
		"## Uno\n"
		"\n"
		"Primer párrafo\n"
		"que sigue aquí.\n"
		"\n"
		"Segundo párrafo.\n"
		"\n"
		"## Dos\n"
		"Solo uno.\n"
		"\n"
		"## Tres\n"
		"\n"
		"\n"
		"Último.   \n");
	const FHowToPlayLoadResult Result = Soulbound::Presentation::ParseHowToPlayText(Markdown, TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Tres páginas"), Result.Pages.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("La primera, su título"), Result.Pages[0].Title, FString(TEXT("Uno")));
	TestEqual(TEXT("La primera: dos párrafos, el primero con sus líneas unidas por un espacio"), Result.Pages[0].Body,
		FString(TEXT("Primer párrafo que sigue aquí.\n\nSegundo párrafo.")));
	TestEqual(TEXT("La segunda, su título"), Result.Pages[1].Title, FString(TEXT("Dos")));
	TestEqual(TEXT("La segunda, su texto"), Result.Pages[1].Body, FString(TEXT("Solo uno.")));
	TestEqual(TEXT("La tercera, su título"), Result.Pages[2].Title, FString(TEXT("Tres")));
	TestEqual(TEXT("La tercera, su texto sin blancos de más"), Result.Pages[2].Body, FString(TEXT("Último.")));
	for (const FHowToPlayPage& Page : Result.Pages)
	{
		TestFalse(FString::Printf(TEXT("%s: el texto suelto de antes no aparece"), *Page.Title), Page.Body.Contains(TEXT("suelto")));
		TestTrue(FString::Printf(TEXT("%s: sin comentario"), *Page.Title), Page.WardenComment.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: sin imagen"), *Page.Title), Page.ImageName.IsEmpty());
	}
	TestTrue(TEXT("Sin título de libro"), Result.BookTitle.IsEmpty());
	TestTrue(TEXT("Sin portada"), Result.CoverLine.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayCommentAndImageTest, "Soulbound.Game.Presentation.ComoSeJuega.ComentarioEImagen", HowToPlayTestFlags)

bool FHowToPlayCommentAndImageTest::RunTest(const FString& Parameters)
{
	// CA-HUD-31 (R-HUD-17): una línea ">" es el comentario del Warden, sin el signo, y no está en el texto de la regla; "![](T_Ayuda_Turno)" da ese
	// nombre de imagen, y la línea no está en el texto. Sin esas líneas, comentario e imagen vacíos. Precisiones del autor del 17/11/2026: varias
	// líneas ">" seguidas son un solo comentario, y "![texto](Nombre)" vale (el texto alternativo se ignora). La imagen se busca en la carpeta fija.
	const FString Markdown = TEXT(
		"## Tu turno\n"
		"\n"
		"El dado se tira solo.\n"
		"\n"
		"![](T_Ayuda_Turno)\n"
		"\n"
		"> Tira. Luego ya veremos si te sirve de algo.\n"
		"\n"
		"## Dos líneas\n"
		"Una regla.\n"
		"> Primera línea\n"
		">segunda línea.\n"
		"![Una captura del tablero](T_Ayuda_Tablero)\n"
		"\n"
		"## Sin nada\n"
		"Solo la regla.\n");
	const FHowToPlayLoadResult Result = Soulbound::Presentation::ParseHowToPlayText(Markdown, TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Tres páginas"), Result.Pages.Num(), 3))
	{
		return false;
	}
	const FHowToPlayPage& Turn = Result.Pages[0];
	TestEqual(TEXT("El comentario, sin el signo"), Turn.WardenComment, FString(TEXT("Tira. Luego ya veremos si te sirve de algo.")));
	TestEqual(TEXT("El nombre de la imagen"), Turn.ImageName, FString(TEXT("T_Ayuda_Turno")));
	TestEqual(TEXT("El texto, sin el comentario ni la imagen"), Turn.Body, FString(TEXT("El dado se tira solo.")));

	const FHowToPlayPage& TwoLines = Result.Pages[1];
	TestEqual(TEXT("Dos líneas \">\" seguidas: un comentario, unidas por un espacio"), TwoLines.WardenComment, FString(TEXT("Primera línea segunda línea.")));
	TestEqual(TEXT("Con texto alternativo: el nombre, sin el texto"), TwoLines.ImageName, FString(TEXT("T_Ayuda_Tablero")));
	TestEqual(TEXT("El texto de esa página, solo la regla"), TwoLines.Body, FString(TEXT("Una regla.")));

	const FHowToPlayPage& Plain = Result.Pages[2];
	TestTrue(TEXT("Sin línea \">\": sin comentario"), Plain.WardenComment.IsEmpty());
	TestTrue(TEXT("Sin línea de imagen: sin imagen"), Plain.ImageName.IsEmpty());

	// R-HUD-17: el nombre se resuelve en la carpeta fija de la ayuda.
	using Soulbound::Presentation::HowToPlayImagePath;
	TestEqual(TEXT("La ruta de la imagen"), HowToPlayImagePath(TEXT("/Game/Art/Ayuda/"), TEXT("T_Ayuda_Turno")), FString(TEXT("/Game/Art/Ayuda/T_Ayuda_Turno.T_Ayuda_Turno")));
	TestEqual(TEXT("La carpeta sin barra final, la misma ruta"), HowToPlayImagePath(TEXT("/Game/Art/Ayuda"), TEXT("T_Ayuda_Turno")),
		FString(TEXT("/Game/Art/Ayuda/T_Ayuda_Turno.T_Ayuda_Turno")));
	TestTrue(TEXT("Sin nombre, sin ruta"), HowToPlayImagePath(TEXT("/Game/Art/Ayuda/"), FString()).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayCoverTest, "Soulbound.Game.Presentation.ComoSeJuega.Portada", HowToPlayTestFlags)

bool FHowToPlayCoverTest::RunTest(const FString& Parameters)
{
	// CA-HUD-37 (R-HUD-17): un archivo que empieza por un título de primer nivel y una línea ">" da ese título y esa frase como portada, y ninguno de
	// los dos aparece en las páginas; sin ellos, la portada queda vacía y las páginas son las mismas.
	const FString Pages = TEXT("## Uno\nTexto uno.\n\n> Comentario uno.\n\n## Dos\nTexto dos.\n");
	const FString WithCover = TEXT("# El Grimorio de los condenados\n\n> Aquí está todo escrito… salvo cuánto tardaréis en decepcionarme.\n\n") + Pages;
	const FHowToPlayLoadResult Covered = Soulbound::Presentation::ParseHowToPlayText(WithCover, TEXT("prueba.md"));
	const FHowToPlayLoadResult Bare = Soulbound::Presentation::ParseHowToPlayText(Pages, TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Con portada, se carga (%s)"), *Covered.Message), Covered.IsSuccess())
		|| !TestTrue(FString::Printf(TEXT("Sin portada, se carga (%s)"), *Bare.Message), Bare.IsSuccess()))
	{
		return false;
	}
	TestEqual(TEXT("El título del libro"), Covered.BookTitle, FString(TEXT("El Grimorio de los condenados")));
	TestEqual(TEXT("La frase de la portada, sin el signo"), Covered.CoverLine, FString(TEXT("Aquí está todo escrito… salvo cuánto tardaréis en decepcionarme.")));
	TestTrue(TEXT("Con portada, las páginas son las mismas que sin ella"), Covered.Pages == Bare.Pages);
	TestEqual(TEXT("Dos páginas"), Covered.Pages.Num(), 2);
	for (const FHowToPlayPage& Page : Covered.Pages)
	{
		TestFalse(FString::Printf(TEXT("%s: el título del libro no está en la página"), *Page.Title), Page.Title.Contains(TEXT("condenados")) || Page.Body.Contains(TEXT("condenados")));
		TestFalse(FString::Printf(TEXT("%s: la frase de la portada no es su comentario"), *Page.Title), Page.WardenComment.Contains(TEXT("decepcionarme")));
	}
	TestTrue(TEXT("Sin portada: sin título"), Bare.BookTitle.IsEmpty());
	TestTrue(TEXT("Sin portada: sin frase"), Bare.CoverLine.IsEmpty());
	TestEqual(TEXT("El comentario de la primera página sigue siendo suyo"), Covered.Pages.Num() > 0 ? Covered.Pages[0].WardenComment : FString(), FString(TEXT("Comentario uno.")));

	// Solo uno de los dos: cada uno por su lado.
	const FHowToPlayLoadResult OnlyTitle = Soulbound::Presentation::ParseHowToPlayText(TEXT("# Solo el título\n\n") + Pages, TEXT("prueba.md"));
	TestTrue(TEXT("Solo el título: el título, sin frase"), OnlyTitle.BookTitle == TEXT("Solo el título") && OnlyTitle.CoverLine.IsEmpty() && OnlyTitle.Pages == Bare.Pages);
	const FHowToPlayLoadResult OnlyLine = Soulbound::Presentation::ParseHowToPlayText(TEXT("> Solo la frase\n> en dos líneas.\n\n") + Pages, TEXT("prueba.md"));
	TestTrue(TEXT("Solo la frase: la frase entera, sin título"), OnlyLine.CoverLine == TEXT("Solo la frase en dos líneas.") && OnlyLine.BookTitle.IsEmpty() && OnlyLine.Pages == Bare.Pages);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayErrorsTest, "Soulbound.Game.Presentation.ComoSeJuega.Errores", HowToPlayTestFlags)

bool FHowToPlayErrorsTest::RunTest(const FString& Parameters)
{
	// CA-HUD-32 (R-HUD-17): sin archivo, el error de archivo no encontrado. Un archivo sin ningún título, o con una página sin texto, da un error cuyo
	// mensaje nombra el archivo y, en el segundo caso, el título de la página. Precisión del autor del 17/11/2026: un segundo comentario o una segunda
	// imagen separados en la misma página, también, con el archivo y la página. Ningún caso lanza.
	const FString Missing = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Textos/no-existe.md"));
	const FHowToPlayLoadResult NoFile = Soulbound::Presentation::LoadHowToPlayText(Missing);
	TestEqual(TEXT("Sin archivo: el error de archivo"), HowToPlayErrorOf(NoFile), static_cast<int32>(EHowToPlayError::FileNotFound));
	TestTrue(TEXT("Sin archivo: el motivo nombra el archivo"), NoFile.Message.Contains(TEXT("no-existe.md")));

	const FHowToPlayLoadResult NoHeadings = Soulbound::Presentation::ParseHowToPlayText(TEXT("Un texto sin ningún título.\n\nOtro párrafo.\n"), TEXT("prueba.md"));
	TestEqual(TEXT("Sin títulos: sin páginas"), HowToPlayErrorOf(NoHeadings), static_cast<int32>(EHowToPlayError::NoPages));
	TestTrue(TEXT("Sin títulos: el motivo nombra el archivo"), NoHeadings.Message.Contains(TEXT("prueba.md")));
	TestEqual(TEXT("Vacío: sin páginas"), HowToPlayErrorOf(Soulbound::Presentation::ParseHowToPlayText(FString(), TEXT("prueba.md"))), static_cast<int32>(EHowToPlayError::NoPages));
	TestEqual(TEXT("Solo portada: sin páginas"), HowToPlayErrorOf(Soulbound::Presentation::ParseHowToPlayText(TEXT("# Un libro\n> Su frase.\n"), TEXT("prueba.md"))),
		static_cast<int32>(EHowToPlayError::NoPages));
	TestEqual(TEXT("Un título de tercer nivel no es una página"), HowToPlayErrorOf(Soulbound::Presentation::ParseHowToPlayText(TEXT("### Pequeño\nTexto.\n"), TEXT("prueba.md"))),
		static_cast<int32>(EHowToPlayError::NoPages));

	const FHowToPlayLoadResult EmptyPage = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Uno\nTexto.\n\n## La vacía\n\n> Solo un comentario.\n![](T_Algo)\n\n## Tres\nMás.\n"), TEXT("prueba.md"));
	TestEqual(TEXT("Una página sin texto: su error"), HowToPlayErrorOf(EmptyPage), static_cast<int32>(EHowToPlayError::EmptyPage));
	TestTrue(TEXT("Una página sin texto: el motivo nombra el archivo"), EmptyPage.Message.Contains(TEXT("prueba.md")));
	TestTrue(TEXT("Una página sin texto: el motivo nombra la página"), EmptyPage.Message.Contains(TEXT("La vacía")));
	TestEqual(TEXT("La última página sin texto: también"), HowToPlayErrorOf(Soulbound::Presentation::ParseHowToPlayText(TEXT("## Uno\nTexto.\n\n## Final\n"), TEXT("prueba.md"))),
		static_cast<int32>(EHowToPlayError::EmptyPage));

	const FHowToPlayLoadResult TwoComments = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Doble voz\nTexto.\n> Primero.\n\nMás texto.\n> Segundo.\n"), TEXT("prueba.md"));
	TestEqual(TEXT("Dos comentarios separados: su error"), HowToPlayErrorOf(TwoComments), static_cast<int32>(EHowToPlayError::DuplicateComment));
	TestTrue(TEXT("Dos comentarios: el motivo nombra el archivo y la página"), TwoComments.Message.Contains(TEXT("prueba.md")) && TwoComments.Message.Contains(TEXT("Doble voz")));
	TestEqual(TEXT("Dos comentarios separados por una línea en blanco: también"),
		HowToPlayErrorOf(Soulbound::Presentation::ParseHowToPlayText(TEXT("## Doble voz\nTexto.\n> Primero.\n\n> Segundo.\n"), TEXT("prueba.md"))),
		static_cast<int32>(EHowToPlayError::DuplicateComment));

	const FHowToPlayLoadResult TwoImages = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Dos fotos\nTexto.\n![](T_Una)\n\n![](T_Otra)\n"), TEXT("prueba.md"));
	TestEqual(TEXT("Dos imágenes: su error"), HowToPlayErrorOf(TwoImages), static_cast<int32>(EHowToPlayError::DuplicateImage));
	TestTrue(TEXT("Dos imágenes: el motivo nombra el archivo y la página"), TwoImages.Message.Contains(TEXT("prueba.md")) && TwoImages.Message.Contains(TEXT("Dos fotos")));

	// Un comentario y una imagen en páginas distintas no son dobles.
	TestTrue(TEXT("Un comentario y una imagen por página, en dos páginas: vale"), Soulbound::Presentation::ParseHowToPlayText(
		TEXT("## Uno\nTexto.\n> Uno.\n![](T_Uno)\n\n## Dos\nTexto.\n> Dos.\n![](T_Dos)\n"), TEXT("prueba.md")).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayLibraryTest, "Soulbound.Game.Presentation.ComoSeJuega.MismasPaginasEnMenuYGrimorio", HowToPlayTestFlags)

bool FHowToPlayLibraryTest::RunTest(const FString& Parameters)
{
	// CA-HUD-35 (R-HUD-19): la librería devuelve el mismo número de páginas y los mismos textos que ve el Grimorio, con la de teclas al final; con el
	// archivo roto, cero páginas y el mensaje del cargador. Las teclas, las de la vista (su objeto por defecto, con la Config de la partida).
	using namespace Soulbound::Presentation;
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	if (!TestNotNull(TEXT("El objeto por defecto de la vista"), View))
	{
		return false;
	}
	const FGrimoireKeyNames Keys = View->GrimoireKeyNames();
	const FGrimoireKeyNames LibraryKeys = UHowToPlayLibrary::ConfiguredKeyNames();
	TestTrue(TEXT("La librería nombra las mismas teclas que la vista"), LibraryKeys.Grimoire == Keys.Grimoire && LibraryKeys.PreviousTab == Keys.PreviousTab
		&& LibraryKeys.NextTab == Keys.NextTab && LibraryKeys.View == Keys.View && LibraryKeys.Overlay == Keys.Overlay && LibraryKeys.RingStart == Keys.RingStart
		&& LibraryKeys.ReturnToMenu == Keys.ReturnToMenu && LibraryKeys.PreviousPage == Keys.PreviousPage && LibraryKeys.NextPage == Keys.NextPage);
	TestFalse(TEXT("Y las de página tienen nombre"), Keys.PreviousPage.IsEmpty() || Keys.NextPage.IsEmpty());

	// Lo que ve el Grimorio: el mismo cargador, el mismo archivo y las mismas teclas.
	const FHowToPlayBook Grimoire = HowToPlayBook(LoadHowToPlayText(HowToPlayRepositoryFile()), Keys);
	if (!TestTrue(FString::Printf(TEXT("El Grimorio carga el libro (%s)"), *Grimoire.ErrorMessage), Grimoire.IsLoaded()))
	{
		return false;
	}
	const int32 Count = UHowToPlayLibrary::HowToPlayPageCount();
	TestEqual(TEXT("El mismo número de páginas"), Count, Grimoire.Pages.Num());
	TestTrue(TEXT("Más de una: las del autor y la de teclas"), Count > 1);
	for (int32 Index = 0; Index < FMath::Min(Count, Grimoire.Pages.Num()); ++Index)
	{
		TestTrue(FString::Printf(TEXT("La página %d, la misma"), Index), UHowToPlayLibrary::HowToPlayPage(Index) == Grimoire.Pages[Index]);
	}
	TestEqual(TEXT("La última, Teclas"), UHowToPlayLibrary::HowToPlayPage(Count - 1).Title, FString(HowToPlayKeysTitle));
	TestTrue(TEXT("Fuera de rango, una página vacía"), UHowToPlayLibrary::HowToPlayPage(Count) == FHowToPlayPage() && UHowToPlayLibrary::HowToPlayPage(-1) == FHowToPlayPage());
	TestEqual(TEXT("El mismo título de libro"), UHowToPlayLibrary::HowToPlayBookTitle().ToString(), Grimoire.Title);
	TestEqual(TEXT("La misma frase de portada"), UHowToPlayLibrary::HowToPlayCoverLine().ToString(), Grimoire.CoverLine);
	TestTrue(TEXT("Sin error"), UHowToPlayLibrary::HowToPlayError().IsEmpty());
	TestTrue(TEXT("La página de teclas no lleva imagen"), UHowToPlayLibrary::HowToPlayPageImage(Count - 1) == nullptr);
	// Precisión del autor del 17/11/2026: el texto de hoy del menú es el libro aplanado.
	TestEqual(TEXT("El bloque de texto del menú de hoy, el libro aplanado"), UHowToPlayLibrary::HowToPlayText().ToString(), HowToPlayFlatText(Grimoire));
	TestFalse(TEXT("Y no vacío"), UHowToPlayLibrary::HowToPlayText().IsEmpty());

	// Con el archivo roto (sin ningún título) o sin archivo: cero páginas y el mensaje del cargador.
	const FString Broken = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests"), TEXT("como-se-juega-roto.md"));
	if (TestTrue(TEXT("Se escribe el archivo roto"), FFileHelper::SaveStringToFile(FString(TEXT("Un texto sin ningún título.\n")), *Broken, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))
	{
		const FHowToPlayBook BrokenBook = UHowToPlayLibrary::BookFromFile(Broken);
		TestEqual(TEXT("Archivo roto: cero páginas"), BrokenBook.Pages.Num(), 0);
		TestEqual(TEXT("Archivo roto: el mensaje del cargador"), BrokenBook.ErrorMessage, LoadHowToPlayText(Broken).Message);
		TestFalse(TEXT("Archivo roto: el mensaje no está vacío"), BrokenBook.ErrorMessage.IsEmpty());
		IFileManager::Get().Delete(*Broken);
	}
	const FString Missing = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Textos/no-existe.md"));
	const FHowToPlayBook MissingBook = UHowToPlayLibrary::BookFromFile(Missing);
	TestTrue(TEXT("Sin archivo: cero páginas y el mensaje del cargador"), MissingBook.Pages.Num() == 0 && !MissingBook.ErrorMessage.IsEmpty()
		&& MissingBook.ErrorMessage == LoadHowToPlayText(Missing).Message);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
