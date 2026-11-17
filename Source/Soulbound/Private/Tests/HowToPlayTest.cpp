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

	/** Un bloque de imagen, para comparar (S-048, segunda ampliación del autor del 17/11/2026). */
	FHowToPlayBlock HowToPlayImageOf(const FString& ImageName)
	{
		FHowToPlayBlock Block;
		Block.Kind = EHowToPlayBlockKind::Image;
		Block.ImageName = ImageName;
		return Block;
	}

	/** Un bloque, para comparar (S-048, ampliación del autor del 17/11/2026). */
	FHowToPlayBlock HowToPlayBlockOf(EHowToPlayBlockKind Kind, const FString& Text)
	{
		FHowToPlayBlock Block;
		Block.Kind = Kind;
		Block.Text = Text;
		return Block;
	}

	/** Los bloques de una página, en una línea legible para los mensajes: "P:texto | W:texto | L:texto". */
	FString HowToPlayBlocksText(const TArray<FHowToPlayBlock>& Blocks)
	{
		TArray<FString> Parts;
		for (const FHowToPlayBlock& Block : Blocks)
		{
			const TCHAR* Kind = Block.Kind == EHowToPlayBlockKind::Paragraph ? TEXT("P") : Block.Kind == EHowToPlayBlockKind::WardenLine ? TEXT("W")
				: Block.Kind == EHowToPlayBlockKind::ListItem ? TEXT("L") : Block.Kind == EHowToPlayBlockKind::Subtitle ? TEXT("S") : TEXT("I");
			// S-048 (segunda ampliación): una imagen se nombra por su imagen.
			Parts.Add(FString::Printf(TEXT("%s:%s"), Kind, Block.Kind == EHowToPlayBlockKind::Image ? *Block.ImageName : *Block.Text));
		}
		return FString::Join(Parts, TEXT(" | "));
	}

	/** Comprueba que una página tiene exactamente esos bloques, en ese orden: su clase y su texto (los tramos de la negrita, en su propio test). */
	void HowToPlayExpectBlocks(FAutomationTestBase& Test, const FString& What, const FHowToPlayPage& Page, const TArray<FHowToPlayBlock>& Expected)
	{
		Test.TestEqual(What, HowToPlayBlocksText(Page.Blocks), HowToPlayBlocksText(Expected));
	}

	/** Un tramo, para comparar (S-048, la negrita). */
	FHowToPlaySpan HowToPlaySpanOf(const FString& Text, bool bBold)
	{
		FHowToPlaySpan Span;
		Span.Text = Text;
		Span.bBold = bBold;
		return Span;
	}

	/** Los tramos en una línea legible para los mensajes: "[uno ][*dos*][ tres]", con la negrita entre asteriscos. */
	FString HowToPlaySpansText(const TArray<FHowToPlaySpan>& Spans)
	{
		FString Text;
		for (const FHowToPlaySpan& Span : Spans)
		{
			Text += Span.bBold ? FString::Printf(TEXT("[*%s*]"), *Span.Text) : FString::Printf(TEXT("[%s]"), *Span.Text);
		}
		return Text;
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
	// Desde la ampliación del autor del 17/11/2026 (R-HUD-17): un bloque por párrafo.
	HowToPlayExpectBlocks(*this, TEXT("La primera: dos párrafos, dos bloques"), Result.Pages[0],
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Primer párrafo que sigue aquí.")), HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Segundo párrafo."))});
	for (const FHowToPlayPage& Page : Result.Pages)
	{
		TestFalse(FString::Printf(TEXT("%s: el texto suelto de antes no aparece"), *Page.Title), Page.Body.Contains(TEXT("suelto")));
		TestFalse(FString::Printf(TEXT("%s: sin frases del Warden ni listas"), *Page.Title),
			Page.Blocks.ContainsByPredicate([](const FHowToPlayBlock& Block) { return Block.Kind != EHowToPlayBlockKind::Paragraph; }));
	}
	TestTrue(TEXT("Sin título de libro"), Result.BookTitle.IsEmpty());
	TestTrue(TEXT("Sin portada"), Result.CoverLine.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayCommentAndImageTest, "Soulbound.Game.Presentation.ComoSeJuega.ComentarioEImagen", HowToPlayTestFlags)

bool FHowToPlayCommentAndImageTest::RunTest(const FString& Parameters)
{
	// CA-HUD-31 (R-HUD-17): "![](T_Ayuda_Turno)" da ese nombre de imagen, y la línea no está en el texto; sin esa línea, sin imagen. Ajustado por la
	// segunda ampliación del autor del 17/11/2026 (CA-HUD-55): la imagen es un bloque, en su sitio, con su nombre. Precisión del
	// autor del 17/11/2026: "![texto](Nombre)" vale (el texto alternativo se ignora). La imagen se busca en la carpeta fija. Ajustado por la ampliación
	// del autor del 17/11/2026 (CA-HUD-43): ya no hay comentario de la página; una línea ">" es una frase del Warden, sin el signo, en su bloque y en su
	// sitio, y varias líneas ">" seguidas son una sola frase.
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
	HowToPlayExpectBlocks(*this, TEXT("La regla, la imagen con su nombre y la frase del Warden, sin el signo, en su orden"), Turn,
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("El dado se tira solo.")), HowToPlayImageOf(TEXT("T_Ayuda_Turno")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Tira. Luego ya veremos si te sirve de algo."))});
	TestEqual(TEXT("El texto sin marcas: los bloques, sin la imagen"), Turn.Body, FString(TEXT("El dado se tira solo.\n\nTira. Luego ya veremos si te sirve de algo.")));

	const FHowToPlayPage& TwoLines = Result.Pages[1];
	HowToPlayExpectBlocks(*this, TEXT("Dos líneas \">\" seguidas: una frase; con texto alternativo, la imagen con su nombre, sin el texto"), TwoLines,
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Una regla.")), HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Primera línea segunda línea.")),
		 HowToPlayImageOf(TEXT("T_Ayuda_Tablero"))});

	const FHowToPlayPage& Plain = Result.Pages[2];
	HowToPlayExpectBlocks(*this, TEXT("Sin línea \">\": solo el párrafo"), Plain, {HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Solo la regla."))});
	TestFalse(TEXT("Sin línea de imagen: sin imagen"), Plain.Blocks.ContainsByPredicate([](const FHowToPlayBlock& Block) { return Block.Kind == EHowToPlayBlockKind::Image; }));

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
		TestFalse(FString::Printf(TEXT("%s: la frase de la portada no está en sus bloques"), *Page.Title), HowToPlayBlocksText(Page.Blocks).Contains(TEXT("decepcionarme")));
	}
	TestTrue(TEXT("Sin portada: sin título"), Bare.BookTitle.IsEmpty());
	TestTrue(TEXT("Sin portada: sin frase"), Bare.CoverLine.IsEmpty());
	if (Covered.Pages.Num() > 0)
	{
		HowToPlayExpectBlocks(*this, TEXT("La frase del Warden de la primera página sigue siendo suya"), Covered.Pages[0],
			{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Texto uno.")), HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Comentario uno."))});
	}

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
	// Ajustado por la segunda ampliación del autor del 17/11/2026 (CA-HUD-55): varias imágenes en una página ya no son un error, y una página con solo
	// una imagen vale; una página sin ningún bloque sigue siéndolo.
	// CA-HUD-32 (R-HUD-17): sin archivo, el error de archivo no encontrado. Un archivo sin ningún título, o con una página sin texto, da un error cuyo
	// mensaje nombra el archivo y, en el segundo caso, el título de la página. Precisión del autor del 17/11/2026: una segunda imagen en la misma página,
	// también, con el archivo y la página. Ningún caso lanza. Ajustado por la ampliación del autor del 17/11/2026 (CA-HUD-43): una página sin texto es
	// una página sin ningún bloque; varias frases del Warden ya no son un error, y una página con solo frases del Warden vale.
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

	const FHowToPlayLoadResult EmptyPage = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Uno\nTexto.\n\n## La vacía\n\n\n## Tres\nMás.\n"), TEXT("prueba.md"));
	TestEqual(TEXT("Una página sin texto: su error"), HowToPlayErrorOf(EmptyPage), static_cast<int32>(EHowToPlayError::EmptyPage));
	TestTrue(TEXT("Una página sin texto: el motivo nombra el archivo"), EmptyPage.Message.Contains(TEXT("prueba.md")));
	TestTrue(TEXT("Una página sin texto: el motivo nombra la página"), EmptyPage.Message.Contains(TEXT("La vacía")));
	TestEqual(TEXT("La última página sin texto: también"), HowToPlayErrorOf(Soulbound::Presentation::ParseHowToPlayText(TEXT("## Uno\nTexto.\n\n## Final\n"), TEXT("prueba.md"))),
		static_cast<int32>(EHowToPlayError::EmptyPage));

	const FHowToPlayLoadResult TwoLines = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Doble voz\nTexto.\n> Primero.\n\nMás texto.\n> Segundo.\n"), TEXT("prueba.md"));
	TestTrue(FString::Printf(TEXT("Dos frases del Warden separadas: ya no es error (%s)"), *TwoLines.Message), TwoLines.IsSuccess());
	const FHowToPlayLoadResult OnlyWarden = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Solo él\n> Primero.\n\n> Segundo.\n"), TEXT("prueba.md"));
	TestTrue(FString::Printf(TEXT("Una página con solo frases del Warden: vale (%s)"), *OnlyWarden.Message), OnlyWarden.IsSuccess());

	const FHowToPlayLoadResult TwoImages = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Dos fotos\nTexto.\n![](T_Una)\n\n![](T_Otra)\n"), TEXT("prueba.md"));
	TestTrue(FString::Printf(TEXT("Dos imágenes: ya no es error (%s)"), *TwoImages.Message), TwoImages.IsSuccess());
	const FHowToPlayLoadResult OnlyImage = Soulbound::Presentation::ParseHowToPlayText(TEXT("## Lámina\n![](T_Lamina)\n"), TEXT("prueba.md"));
	TestTrue(FString::Printf(TEXT("Solo una imagen: vale (%s)"), *OnlyImage.Message), OnlyImage.IsSuccess());

	// Una frase y una imagen en páginas distintas no son dobles.
	TestTrue(TEXT("Una frase y una imagen por página, en dos páginas: vale"), Soulbound::Presentation::ParseHowToPlayText(
		TEXT("## Uno\nTexto.\n> Uno.\n![](T_Uno)\n\n## Dos\nTexto.\n> Dos.\n![](T_Dos)\n"), TEXT("prueba.md")).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayBlocksTest, "Soulbound.Game.Presentation.ComoSeJuega.BloquesEnSuOrden", HowToPlayTestFlags)

bool FHowToPlayBlocksTest::RunTest(const FString& Parameters)
{
	// CA-HUD-43 (R-HUD-17, ampliación del autor del 17/11/2026): una página es su título y una lista ordenada de bloques. Párrafo, frase del Warden,
	// párrafo y otra frase del Warden dan cuatro bloques en ese orden, cada uno con su clase; ya no es error. Precisión del autor al aprobar el plan:
	// dos bloques ">" separados por una línea en blanco son dos frases; solo las líneas ">" seguidas forman una. Una página con solo frases del Warden
	// vale. Una línea ">" justo después de un párrafo lo cierra, y una línea sin marca justo después de una frase empieza un párrafo.
	using namespace Soulbound::Presentation;
	const FString Markdown = TEXT(
		"## Cuatro\n"
		"Un párrafo\n"
		"en dos líneas.\n"
		"> Primera frase\n"
		"> en dos líneas.\n"
		"Otro párrafo.\n"
		"\n"
		"> Segunda frase.\n"
		"\n"
		"## Separadas\n"
		"> Una.\n"
		"\n"
		"> Dos.\n"
		">Sigue dos.\n"
		"\n"
		"\n"
		"> Tres.\n"
		"\n"
		"## Solo voz\n"
		"> Solo hablo yo.\n");
	const FHowToPlayLoadResult Result = ParseHowToPlayText(Markdown, TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Tres páginas"), Result.Pages.Num(), 3))
	{
		return false;
	}
	HowToPlayExpectBlocks(*this, TEXT("Párrafo, frase, párrafo y frase: cuatro bloques en su orden"), Result.Pages[0],
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Un párrafo en dos líneas.")), HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Primera frase en dos líneas.")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Otro párrafo.")), HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Segunda frase."))});
	TestEqual(TEXT("El texto sin marcas: los cuatro, separados por una línea en blanco"), Result.Pages[0].Body,
		FString(TEXT("Un párrafo en dos líneas.\n\nPrimera frase en dos líneas.\n\nOtro párrafo.\n\nSegunda frase.")));
	HowToPlayExpectBlocks(*this, TEXT("Separadas por una línea en blanco: frases distintas; seguidas, una"), Result.Pages[1],
		{HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Una.")), HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Dos. Sigue dos.")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Tres."))});
	HowToPlayExpectBlocks(*this, TEXT("Solo frases del Warden: vale"), Result.Pages[2], {HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("Solo hablo yo."))});
	TestEqual(TEXT("Y su texto sin marcas, la frase"), Result.Pages[2].Body, FString(TEXT("Solo hablo yo.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayListTest, "Soulbound.Game.Presentation.ComoSeJuega.ElementosDeLista", HowToPlayTestFlags)

bool FHowToPlayListTest::RunTest(const FString& Parameters)
{
	// CA-HUD-44 (R-HUD-17, ampliación del autor del 17/11/2026): tres líneas "- " seguidas dan tres bloques de lista, sin el guion en el texto; una
	// línea "- " entre dos párrafos no se une a ninguno. Precisión del autor al aprobar el plan: una línea sin "- " justo después de un elemento de
	// lista empieza un párrafo nuevo. En el texto sin marcas, cada elemento con su marca, y los seguidos, uno por línea.
	using namespace Soulbound::Presentation;
	const FString Markdown = TEXT(
		"## Lista\n"
		"Antes.\n"
		"\n"
		"- Uno\n"
		"- Dos\n"
		"-   Tres  \n"
		"\n"
		"Después.\n"
		"\n"
		"## Suelto\n"
		"Primer párrafo\n"
		"- Suelto\n"
		"Segundo párrafo\n"
		"que sigue.\n");
	const FHowToPlayLoadResult Result = ParseHowToPlayText(Markdown, TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Dos páginas"), Result.Pages.Num(), 2))
	{
		return false;
	}
	HowToPlayExpectBlocks(*this, TEXT("Tres líneas \"- \" seguidas: tres elementos, sin el guion"), Result.Pages[0],
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Antes.")), HowToPlayBlockOf(EHowToPlayBlockKind::ListItem, TEXT("Uno")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::ListItem, TEXT("Dos")), HowToPlayBlockOf(EHowToPlayBlockKind::ListItem, TEXT("Tres")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Después."))});
	TestEqual(TEXT("El texto sin marcas: los elementos con su marca, uno por línea"), Result.Pages[0].Body, FString(TEXT("Antes.\n\n• Uno\n• Dos\n• Tres\n\nDespués.")));
	HowToPlayExpectBlocks(*this, TEXT("Una línea \"- \" entre dos párrafos no se une a ninguno"), Result.Pages[1],
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Primer párrafo")), HowToPlayBlockOf(EHowToPlayBlockKind::ListItem, TEXT("Suelto")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Segundo párrafo que sigue."))});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayRichTextTest, "Soulbound.Game.Presentation.ComoSeJuega.TextoEnriquecidoParaElMenu", HowToPlayTestFlags)

bool FHowToPlayRichTextTest::RunTest(const FString& Parameters)
{
	// CA-HUD-46 (R-HUD-19, ampliación del autor del 17/11/2026): la página en texto enriquecido para UMG, con sus marcas (la frase del Warden entre
	// <Warden> y </>), los elementos de lista con su marca, uno por línea, y & < > " escapados; el texto sin marcas, donde ya estaba. La librería da lo
	// mismo para cada página del libro que ve el Grimorio.
	using namespace Soulbound::Presentation;
	const FHowToPlayLoadResult Result = ParseHowToPlayText(TEXT(
		"## Rica\n"
		"Un párrafo con <etiqueta> & \"comillas\".\n"
		"> Habla el Warden: a < b.\n"
		"- Uno\n"
		"- Dos & tres\n"
		"\n"
		"Final.\n"), TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Una página"), Result.Pages.Num(), 1))
	{
		return false;
	}
	const FHowToPlayPage& Page = Result.Pages[0];
	TestEqual(TEXT("El texto enriquecido de la página"), HowToPlayRichText(Page), FString(TEXT(
		"Un párrafo con &lt;etiqueta&gt; &amp; &quot;comillas&quot;.\n\n<Warden>Habla el Warden: a &lt; b.</>\n\n• Uno\n• Dos &amp; tres\n\nFinal.")));
	TestEqual(TEXT("El texto sin marcas, sin escapar"), Page.Body, FString(TEXT("Un párrafo con <etiqueta> & \"comillas\".\n\nHabla el Warden: a < b.\n\n• Uno\n• Dos & tres\n\nFinal.")));
	TestEqual(TEXT("Un bloque de lista, sin su marca"), HowToPlayBlockRichText(HowToPlayBlockOf(EHowToPlayBlockKind::ListItem, TEXT("Dos & tres"))), FString(TEXT("Dos &amp; tres")));
	TestEqual(TEXT("Una frase del Warden, entre sus marcas"), HowToPlayBlockRichText(HowToPlayBlockOf(EHowToPlayBlockKind::WardenLine, TEXT("\"Ya.\""))),
		FString(TEXT("<Warden>&quot;Ya.&quot;</>")));

	// Con la negrita (CA-HUD-45): <Bold> en el párrafo y en la lista; dentro de la frase del Warden, <WardenBold>, cerrando y abriendo <Warden>,
	// porque el texto enriquecido de UMG no anida marcas. El texto sin marcas, sin asteriscos.
	const FHowToPlayLoadResult Bold = ParseHowToPlayText(TEXT("## Fuerte\nUn **fuerte** & claro.\n> Te **vigilo**.\n- **Uno**\n- Dos\n"), TEXT("prueba.md"));
	if (TestTrue(FString::Printf(TEXT("Con negrita, se carga (%s)"), *Bold.Message), Bold.IsSuccess() && Bold.Pages.Num() == 1))
	{
		TestEqual(TEXT("Con negrita, el texto enriquecido"), HowToPlayRichText(Bold.Pages[0]),
			FString(TEXT("Un <Bold>fuerte</> &amp; claro.\n\n<Warden>Te </><WardenBold>vigilo</><Warden>.</>\n\n• <Bold>Uno</>\n• Dos")));
		TestEqual(TEXT("Con negrita, el texto sin marcas"), Bold.Pages[0].Body, FString(TEXT("Un fuerte & claro.\n\nTe vigilo.\n\n• Uno\n• Dos")));
	}

	// La librería: para cada página del libro del Grimorio, el mismo texto enriquecido; fuera de rango, vacío. El texto sin marcas, como antes.
	const ABoardViewActor* View = GetDefault<ABoardViewActor>();
	if (!TestNotNull(TEXT("El objeto por defecto de la vista"), View))
	{
		return false;
	}
	const FHowToPlayBook Grimoire = HowToPlayBook(LoadHowToPlayText(HowToPlayRepositoryFile()), View->GrimoireKeyNames());
	if (!TestTrue(FString::Printf(TEXT("El Grimorio carga el libro (%s)"), *Grimoire.ErrorMessage), Grimoire.IsLoaded()))
	{
		return false;
	}
	for (int32 Index = 0; Index < Grimoire.Pages.Num(); ++Index)
	{
		const FString Rich = UHowToPlayLibrary::HowToPlayPageRichText(Index).ToString();
		TestEqual(FString::Printf(TEXT("La página %d, el mismo texto enriquecido"), Index), Rich, HowToPlayRichText(Grimoire.Pages[Index]));
		TestFalse(FString::Printf(TEXT("La página %d, con texto"), Index), Rich.IsEmpty());
		TestEqual(FString::Printf(TEXT("La página %d, su texto sin marcas donde ya estaba"), Index), UHowToPlayLibrary::HowToPlayPage(Index).Body,
			HowToPlayBodyText(Grimoire.Pages[Index].Blocks));
	}
	TestTrue(TEXT("Fuera de rango, vacío"), UHowToPlayLibrary::HowToPlayPageRichText(Grimoire.Pages.Num()).IsEmpty() && UHowToPlayLibrary::HowToPlayPageRichText(-1).IsEmpty());
	TestFalse(TEXT("El bloque de texto del menú de hoy, sin marcas"), UHowToPlayLibrary::HowToPlayText().ToString().Contains(TEXT("</>")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayBoldTest, "Soulbound.Game.Presentation.ComoSeJuega.NegritaEnLosBloques", HowToPlayTestFlags)

bool FHowToPlayBoldTest::RunTest(const FString& Parameters)
{
	// CA-HUD-45 (R-HUD-17, ampliación del autor del 17/11/2026): "uno **dos** tres" da tres tramos, el segundo en negrita y sin asteriscos; unos
	// asteriscos sin cerrar se dejan como texto. Vale en las tres clases de bloque. Precisiones del autor al aprobar el plan: solo con dos asteriscos
	// (uno suelto es texto), y ni en el título de la página ni en la portada.
	using namespace Soulbound::Presentation;
	const TArray<FHowToPlaySpan> Three{HowToPlaySpanOf(TEXT("uno "), false), HowToPlaySpanOf(TEXT("dos"), true), HowToPlaySpanOf(TEXT(" tres"), false)};
	TestEqual(TEXT("Tres tramos, el segundo en negrita y sin asteriscos"), HowToPlaySpansText(HowToPlaySpans(TEXT("uno **dos** tres"))), HowToPlaySpansText(Three));
	TestEqual(TEXT("Sin cerrar: texto"), HowToPlaySpansText(HowToPlaySpans(TEXT("uno **dos tres"))), HowToPlaySpansText({HowToPlaySpanOf(TEXT("uno **dos tres"), false)}));
	TestEqual(TEXT("La segunda sin cerrar: la primera, negrita; el resto, texto"), HowToPlaySpansText(HowToPlaySpans(TEXT("a **b** c **d"))),
		HowToPlaySpansText({HowToPlaySpanOf(TEXT("a "), false), HowToPlaySpanOf(TEXT("b"), true), HowToPlaySpanOf(TEXT(" c **d"), false)}));
	TestEqual(TEXT("Un asterisco suelto es texto"), HowToPlaySpansText(HowToPlaySpans(TEXT("uno *dos* tres"))), HowToPlaySpansText({HowToPlaySpanOf(TEXT("uno *dos* tres"), false)}));
	TestEqual(TEXT("Todo en negrita: un tramo"), HowToPlaySpansText(HowToPlaySpans(TEXT("**todo**"))), HowToPlaySpansText({HowToPlaySpanOf(TEXT("todo"), true)}));
	TestEqual(TEXT("Sin negrita: un tramo"), HowToPlaySpansText(HowToPlaySpans(TEXT("nada"))), HowToPlaySpansText({HowToPlaySpanOf(TEXT("nada"), false)}));
	TestTrue(TEXT("Vacío: sin tramos"), HowToPlaySpans(FString()).IsEmpty());

	// En las tres clases de bloque; en un párrafo, también a través de dos líneas. El título de la página y la portada se quedan como están.
	const FHowToPlayLoadResult Result = ParseHowToPlayText(TEXT(
		"# Libro **x**\n"
		"> Portada **y**\n"
		"\n"
		"## Título **z**\n"
		"uno **dos** tres\n"
		"\n"
		"uno **dos\n"
		"tres** cuatro\n"
		"> uno **dos**\n"
		"> tres\n"
		"- uno **dos** tres\n"
		"- uno **dos\n"), TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Una página"), Result.Pages.Num(), 1))
	{
		return false;
	}
	const FHowToPlayPage& Page = Result.Pages[0];
	if (!TestEqual(TEXT("Cinco bloques"), Page.Blocks.Num(), 5))
	{
		return false;
	}
	const TCHAR* Kinds[] = {TEXT("el párrafo"), TEXT("el párrafo de dos líneas"), TEXT("la frase del Warden"), TEXT("el elemento de lista"), TEXT("el elemento sin cerrar")};
	const TArray<TArray<FHowToPlaySpan>> Expected{
		Three,
		{HowToPlaySpanOf(TEXT("uno "), false), HowToPlaySpanOf(TEXT("dos tres"), true), HowToPlaySpanOf(TEXT(" cuatro"), false)},
		Three,
		Three,
		{HowToPlaySpanOf(TEXT("uno **dos"), false)}};
	const TArray<FString> Texts{TEXT("uno dos tres"), TEXT("uno dos tres cuatro"), TEXT("uno dos tres"), TEXT("uno dos tres"), TEXT("uno **dos")};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Los tramos de %s"), Kinds[Index]), HowToPlaySpansText(Page.Blocks[Index].Spans), HowToPlaySpansText(Expected[Index]));
		TestEqual(FString::Printf(TEXT("El texto sin marcas de %s"), Kinds[Index]), Page.Blocks[Index].Text, Texts[Index]);
	}
	TestFalse(TEXT("El texto sin marcas de la página, sin los asteriscos de la negrita"), Page.Body.Contains(TEXT("**dos**")) || Page.Body.Contains(TEXT("tres**")));
	TestEqual(TEXT("El título de la página, tal cual"), Page.Title, FString(TEXT("Título **z**")));
	TestEqual(TEXT("El título del libro, tal cual"), Result.BookTitle, FString(TEXT("Libro **x**")));
	TestEqual(TEXT("La portada, tal cual"), Result.CoverLine, FString(TEXT("Portada **y**")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlaySubtitleTest, "Soulbound.Game.Presentation.ComoSeJuega.Subtitulos", HowToPlayTestFlags)

bool FHowToPlaySubtitleTest::RunTest(const FString& Parameters)
{
	// CA-HUD-53 (R-HUD-17, segunda ampliación del autor del 17/11/2026): "### Texto" da un bloque de subtítulo con ese texto, sin las almohadillas, en
	// su orden; lleva su marca en el texto enriquecido y sale como una línea en el texto sin marcas. Un subtítulo justo después de un párrafo lo
	// cierra. Precisión del plan aprobado: los "**" dentro de un subtítulo se quitan, y todo va con su marca.
	using namespace Soulbound::Presentation;
	const FHowToPlayLoadResult Result = ParseHowToPlayText(TEXT(
		"## Página\n"
		"Intro.\n"
		"### Primer **apartado**\n"
		"Texto & más.\n"
		"\n"
		"###   Segundo  \n"
		"- Uno\n"), TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Una página"), Result.Pages.Num(), 1))
	{
		return false;
	}
	const FHowToPlayPage& Page = Result.Pages[0];
	HowToPlayExpectBlocks(*this, TEXT("Los subtítulos, sin almohadillas ni asteriscos, en su orden"), Page,
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Intro.")), HowToPlayBlockOf(EHowToPlayBlockKind::Subtitle, TEXT("Primer apartado")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Texto & más.")), HowToPlayBlockOf(EHowToPlayBlockKind::Subtitle, TEXT("Segundo")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::ListItem, TEXT("Uno"))});
	TestEqual(TEXT("El texto sin marcas: cada subtítulo, una línea"), Page.Body, FString(TEXT("Intro.\n\nPrimer apartado\n\nTexto & más.\n\nSegundo\n\n• Uno")));
	TestEqual(TEXT("El texto enriquecido: cada subtítulo, con su marca"), HowToPlayRichText(Page),
		FString(TEXT("Intro.\n\n<Subtitle>Primer apartado</>\n\nTexto &amp; más.\n\n<Subtitle>Segundo</>\n\n• Uno")));
	TestEqual(TEXT("Un subtítulo con caracteres de marca, escapados"), HowToPlayBlockRichText(HowToPlayBlockOf(EHowToPlayBlockKind::Subtitle, TEXT("A < B"))),
		FString(TEXT("<Subtitle>A &lt; B</>")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowToPlayImageBlocksTest, "Soulbound.Game.Presentation.ComoSeJuega.ImagenesComoBloques", HowToPlayTestFlags)

bool FHowToPlayImageBlocksTest::RunTest(const FString& Parameters)
{
	// CA-HUD-55 (R-HUD-17, segunda ampliación del autor del 17/11/2026): una página con párrafo, imagen, párrafo e imagen da cuatro bloques en ese
	// orden, dos de imagen con su nombre; ya no es error. Una página con solo una imagen es válida; "##" sola da una página con el título vacío; una
	// página sin bloques sigue dando el error que nombra el archivo. Las imágenes no van en el texto sin marcas ni en el enriquecido; el menú las
	// recibe en los bloques de la página, con el texto enriquecido de cada bloque.
	using namespace Soulbound::Presentation;
	const FHowToPlayLoadResult Result = ParseHowToPlayText(TEXT(
		"## Láminas\n"
		"Primero.\n"
		"![](T_Uno)\n"
		"Segundo.\n"
		"![Una captura](T_Dos)\n"
		"\n"
		"## Solo lámina\n"
		"![](T_Sola)\n"
		"\n"
		"##\n"
		"Sin título.\n"
		"\n"
		"##   \n"
		"> También sin título.\n"), TEXT("prueba.md"));
	if (!TestTrue(FString::Printf(TEXT("Se carga (%s)"), *Result.Message), Result.IsSuccess()) || !TestEqual(TEXT("Cuatro páginas"), Result.Pages.Num(), 4))
	{
		return false;
	}
	const FHowToPlayPage& Plates = Result.Pages[0];
	HowToPlayExpectBlocks(*this, TEXT("Párrafo, imagen, párrafo e imagen: cuatro bloques en su orden"), Plates,
		{HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Primero.")), HowToPlayImageOf(TEXT("T_Uno")),
		 HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Segundo.")), HowToPlayImageOf(TEXT("T_Dos"))});
	TestTrue(TEXT("Un bloque de imagen no lleva texto"), Plates.Blocks.Num() == 4 && Plates.Blocks[1].Text.IsEmpty() && Plates.Blocks[3].Text.IsEmpty());
	TestEqual(TEXT("El texto sin marcas, sin las imágenes"), Plates.Body, FString(TEXT("Primero.\n\nSegundo.")));
	TestEqual(TEXT("El texto enriquecido, sin las imágenes"), HowToPlayRichText(Plates), FString(TEXT("Primero.\n\nSegundo.")));
	HowToPlayExpectBlocks(*this, TEXT("Solo una imagen: vale"), Result.Pages[1], {HowToPlayImageOf(TEXT("T_Sola"))});
	TestEqual(TEXT("\"##\" sola: el título vacío"), Result.Pages[2].Title, FString());
	HowToPlayExpectBlocks(*this, TEXT("\"##\" sola: sus bloques"), Result.Pages[2], {HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("Sin título."))});
	TestEqual(TEXT("\"##\" con espacios: también sin título"), Result.Pages[3].Title, FString());

	// El menú: el texto enriquecido de cada bloque, el mismo que el del Grimorio; el de una imagen, vacío.
	for (const FHowToPlayBlock& Block : Plates.Blocks)
	{
		TestEqual(TEXT("La librería da el texto enriquecido del bloque"), UHowToPlayLibrary::HowToPlayBlockRichText(Block).ToString(), HowToPlayBlockRichText(Block));
	}
	TestTrue(TEXT("El de una imagen, vacío"), HowToPlayBlockRichText(HowToPlayImageOf(TEXT("T_Uno"))).IsEmpty());
	TestEqual(TEXT("El de un párrafo, en la librería"), UHowToPlayLibrary::HowToPlayBlockRichText(HowToPlayBlockOf(EHowToPlayBlockKind::Paragraph, TEXT("a & b"))).ToString(),
		FString(TEXT("a &amp; b")));

	// Una página sin bloques sigue siendo un error que nombra el archivo y la página.
	const FHowToPlayLoadResult Empty = ParseHowToPlayText(TEXT("## Vacía\n\n## Otra\nTexto.\n"), TEXT("prueba.md"));
	TestEqual(TEXT("Sin bloques: su error"), static_cast<int32>(Empty.Error), static_cast<int32>(EHowToPlayError::EmptyPage));
	TestTrue(TEXT("Sin bloques: el motivo nombra el archivo y la página"), Empty.Message.Contains(TEXT("prueba.md")) && Empty.Message.Contains(TEXT("Vacía")));
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
	TestFalse(TEXT("La página de teclas no lleva imagen"), UHowToPlayLibrary::HowToPlayPage(Count - 1).Blocks.ContainsByPredicate([](const FHowToPlayBlock& Block)
	{
		return Block.Kind == EHowToPlayBlockKind::Image;
	}));
	TestTrue(TEXT("Sin nombre, sin textura"), UHowToPlayLibrary::HowToPlayImage(FString()) == nullptr);
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
