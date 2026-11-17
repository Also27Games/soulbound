#pragma once

#include "CoreMinimal.h"
#include "HowToPlay.generated.h"

/** Qué falló al cargar el texto de "cómo se juega" (S-022; desde S-048, R-HUD-17, por páginas). Nombre propuesto. */
enum class EHowToPlayError : uint8
{
	None,
	/** El archivo no existe o no se puede leer. */
	FileNotFound,
	/** S-048 (R-HUD-17): el texto no tiene ningún título de segundo nivel ("## "), así que no tiene ninguna página. */
	NoPages,
	/** S-048 (R-HUD-17): una página tiene título pero ningún bloque (desde la ampliación del autor del 17/11/2026, ni párrafo, ni frase, ni lista). */
	EmptyPage,
	/** S-048 (precisión del autor de R-HUD-17): una página con una segunda imagen. */
	DuplicateImage,
};

/** La clase de un bloque de una página de "Cómo se juega" (S-048, ampliación del autor del 17/11/2026, R-HUD-17, nombre propuesto). */
UENUM(BlueprintType)
enum class EHowToPlayBlockKind : uint8
{
	/** Un párrafo: líneas seguidas sin marca, unidas con un espacio. */
	Paragraph,
	/** Una frase del Warden: una o varias líneas ">" seguidas, sin línea en blanco entre ellas, unidas con un espacio. */
	WardenLine,
	/** Un elemento de lista: una línea que empieza por "- ", sin el guion. */
	ListItem,
	/** S-048 (segunda ampliación del autor del 17/11/2026): un subtítulo, una línea que empieza por "### ", sin las almohadillas. */
	Subtitle,
	/** S-048 (segunda ampliación del autor del 17/11/2026): una imagen, una línea "![](Nombre)", con su nombre en ImageName y sin texto. */
	Image,
};

/**
 * Un tramo del texto de un bloque (S-048, ampliación del autor del 17/11/2026, R-HUD-17, nombre propuesto): un trozo seguido, en negrita o no. La
 * negrita se escribe "**texto**" y el tramo no lleva los asteriscos.
 */
USTRUCT(BlueprintType)
struct SOULBOUND_API FHowToPlaySpan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	bool bBold = false;

	bool operator==(const FHowToPlaySpan& Other) const
	{
		return Text == Other.Text && bBold == Other.bBold;
	}
};

/**
 * Un bloque de una página de "Cómo se juega" (S-048, ampliación del autor del 17/11/2026, R-HUD-17, nombre propuesto): su clase, su texto sin
 * marcas y sus tramos, normales o en negrita. Es un USTRUCT para que el widget del menú lo lea.
 */
USTRUCT(BlueprintType)
struct SOULBOUND_API FHowToPlayBlock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	EHowToPlayBlockKind Kind = EHowToPlayBlockKind::Paragraph;

	/** El texto del bloque, sin marcas: sin ">", sin el guion de la lista, sin los asteriscos de la negrita. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString Text;

	/** Los tramos del texto, en su orden (HowToPlaySpans): juntos, dan Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	TArray<FHowToPlaySpan> Spans;

	/** S-048 (segunda ampliación del autor del 17/11/2026): en un bloque de imagen, su nombre (HowToPlayImagePath); en los demás, vacío. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString ImageName;

	bool operator==(const FHowToPlayBlock& Other) const
	{
		return Kind == Other.Kind && Text == Other.Text && Spans == Other.Spans && ImageName == Other.ImageName;
	}
};

/**
 * Una página de "Cómo se juega" (S-048, R-HUD-17, nombre propuesto): su título, su texto sin marcas, sus bloques y el nombre de su imagen, que puede
 * estar vacío. Es un USTRUCT para que el widget del menú, que monta el autor, la lea (R-HUD-19). Desde la ampliación del autor del 17/11/2026, sin
 * comentario del Warden: las frases van en sus bloques, en su sitio.
 */
USTRUCT(BlueprintType)
struct SOULBOUND_API FHowToPlayPage
{
	GENERATED_BODY()

	/** El título de la página: lo que sigue a "## ". */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString Title;

	/**
	 * El texto sin marcas de la página (HowToPlayBodyText): sus bloques en su orden, separados por una línea en blanco ("\n\n"), salvo los
	 * elementos de lista seguidos, uno por línea y con su marca; las líneas de un mismo párrafo o de una misma frase, unidas con un espacio.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString Body;

	/** S-048 (ampliación del autor del 17/11/2026): los bloques de la página, en su orden: párrafos, frases del Warden y elementos de lista. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	TArray<FHowToPlayBlock> Blocks;

	/** El nombre de la imagen (lo que va entre paréntesis en "![](Nombre)"); vacío si la página no lleva. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString ImageName;

	bool operator==(const FHowToPlayPage& Other) const
	{
		return Title == Other.Title && Body == Other.Body && Blocks == Other.Blocks && ImageName == Other.ImageName;
	}
};

/** Resultado de cargar el texto de "cómo se juega" (S-022, nombre aprobado): o el libro, o el error con su motivo. */
struct FHowToPlayLoadResult
{
	EHowToPlayError Error = EHowToPlayError::None;

	/** Motivo legible del rechazo, con el archivo y, si es de una página, su título. Vacío si hay éxito. */
	FString Message;

	/** S-048 (R-HUD-17): el título del libro, el de primer nivel ("# "); vacío si el archivo no lo trae. */
	FString BookTitle;

	/** S-048 (R-HUD-17): la frase de la portada, del Warden (las líneas ">" antes de la primera página); vacía si el archivo no la trae. */
	FString CoverLine;

	/** S-048 (R-HUD-17): las páginas, en el orden del archivo. Solo tiene sentido si IsSuccess(). */
	TArray<FHowToPlayPage> Pages;

	bool IsSuccess() const { return Error == EHowToPlayError::None; }
};

/**
 * El texto de "cómo se juega" (S-022): vive en Content/Data/Textos/como-se-juega.md, que se empaqueta como archivo suelto, y lo escribe el
 * autor. Desde S-048 (R-HUD-17) se lee como un libro: un título de primer nivel y una línea ">" de portada, opcionales, y una página por cada
 * título de segundo nivel, con sus bloques (párrafos, frases del Warden ">" y elementos de lista "- ", desde la ampliación del autor del
 * 17/11/2026) y su imagen ("![](Nombre)"). Funciones puras, sin Slate. Nunca lanza.
 */
namespace Soulbound::Presentation
{
	/** Ruta del texto, relativa a Content/. */
	inline const FString HowToPlayRelativePath = TEXT("Data/Textos/como-se-juega.md");

	/**
	 * Lee un texto de "cómo se juega" ya cargado como libro (S-048, R-HUD-17). Vale si tiene al menos una página y cada página tiene título y
	 * al menos un bloque. Con las precisiones del autor del 17/11/2026: una segunda imagen en la misma página es un error; el texto suelto antes de
	 * la primera "##" que no sea título ni portada se ignora; las líneas de un párrafo se unen con un espacio; "![texto](Nombre)" vale y el texto
	 * alternativo se ignora. Con la ampliación del mismo día, los bloques: las líneas ">" seguidas, sin línea en blanco entre ellas, son una frase
	 * del Warden (las que se quiera, en su sitio); cada línea "- " es un elemento de lista, sin el guion; una línea de otra clase cierra el bloque
	 * abierto, y una línea sin marca tras un elemento de lista empieza un párrafo.
	 * @param Markdown El contenido del archivo.
	 * @param SourceName El nombre del archivo, para los mensajes de error.
	 * @return El libro, o NoPages, EmptyPage o DuplicateImage con un mensaje que nombra el archivo y, si es de una página, su título.
	 */
	FHowToPlayLoadResult ParseHowToPlayText(const FString& Markdown, const FString& SourceName = HowToPlayRelativePath);

	/**
	 * Lee el texto de "cómo se juega" de un archivo (ruta absoluta) y lo comprueba con ParseHowToPlayText.
	 * @param FilePath Ruta del archivo.
	 * @return El libro, FileNotFound si no se puede leer o lo que diga ParseHowToPlayText.
	 */
	FHowToPlayLoadResult LoadHowToPlayText(const FString& FilePath);

	/**
	 * La ruta de la textura de una página (S-048, R-HUD-17): el nombre de la imagen dentro de la carpeta fija de la ayuda. "T_Ayuda_Turno" en
	 * "/Game/Art/Ayuda/" es "/Game/Art/Ayuda/T_Ayuda_Turno.T_Ayuda_Turno"; da igual si la carpeta acaba en "/" o no.
	 * @param Folder La carpeta (HowToPlayImageFolder de la vista).
	 * @param ImageName El nombre de la imagen de la página.
	 * @return La ruta del asset; vacía si el nombre está vacío.
	 */
	FString HowToPlayImagePath(const FString& Folder, const FString& ImageName);

	/**
	 * Parte el texto de un bloque en tramos normales y en negrita (S-048, ampliación del autor del 17/11/2026, R-HUD-17, nombre propuesto). Solo
	 * "**texto**" es negrita: un asterisco suelto es texto, y unos asteriscos sin cerrar se dejan como texto. Sin tramos vacíos; dos seguidos del
	 * mismo tipo, uno.
	 * @param Text El texto del bloque, con sus marcas de negrita.
	 * @return Los tramos, en su orden, sin los asteriscos de la negrita; vacío si el texto está vacío.
	 */
	TArray<FHowToPlaySpan> HowToPlaySpans(const FString& Text);

	/** La marca de un elemento de lista (S-048, ampliación del autor del 17/11/2026, nombre propuesto), en el texto sin marcas y en el enriquecido. */
	inline const TCHAR* const HowToPlayListMark = TEXT("•");

	/**
	 * El texto sin marcas de una página (S-048, ampliación del autor del 17/11/2026, nombre propuesto): sus bloques en su orden, separados por una
	 * línea en blanco, salvo dos elementos de lista seguidos, que van uno por línea; cada elemento, con su marca y un espacio delante.
	 * @param Blocks Los bloques de la página.
	 * @return El texto, que es FHowToPlayPage::Body.
	 */
	FString HowToPlayBodyText(const TArray<FHowToPlayBlock>& Blocks);

	/**
	 * Un bloque en texto enriquecido (S-048, ampliación del autor del 17/11/2026, R-HUD-19, nombre propuesto), sin la marca de lista: la frase del
	 * Warden, entre <Warden> y </>; los caracteres & < > " escapados (&amp; &lt; &gt; &quot;).
	 */
	FString HowToPlayBlockRichText(const FHowToPlayBlock& Block);

	/**
	 * Una página en texto enriquecido, para un bloque de texto enriquecido de UMG (S-048, ampliación del autor del 17/11/2026, R-HUD-19, nombre
	 * propuesto): sus bloques (HowToPlayBlockRichText) separados como en HowToPlayBodyText, y los elementos de lista con su marca delante. Sin el
	 * título, que va aparte. Marcas: <Warden> y, con la negrita, <Bold> y <WardenBold>, los nombres de las filas de la tabla de estilos del menú.
	 */
	FString HowToPlayRichText(const FHowToPlayPage& Page);
}
