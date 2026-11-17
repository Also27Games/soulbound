#pragma once

#include "CoreMinimal.h"
#include "HowToPlay.generated.h"

/** Qué falló al cargar el texto de "cómo se juega" (S-022; desde S-048, R-HUD-17, por páginas). Nombre propuesto. */
enum class EHowToPlayError : uint8
{
	None,
	/** El archivo no existe o no se puede leer. */
	FileNotFound,
	/** Falta un título exigido, o no está en su orden. */
	MissingHeading,
	/** S-048 (R-HUD-17): el texto no tiene ningún título de segundo nivel ("## "), así que no tiene ninguna página. */
	NoPages,
	/** S-048 (R-HUD-17): una página tiene título pero no texto de la regla. */
	EmptyPage,
	/** S-048 (precisión del autor de R-HUD-17): una página con un segundo comentario del Warden separado del primero. */
	DuplicateComment,
	/** S-048 (precisión del autor de R-HUD-17): una página con una segunda imagen. */
	DuplicateImage,
};

/**
 * Una página de "Cómo se juega" (S-048, R-HUD-17, nombre propuesto): su título, el texto de la regla, el comentario del Warden y el nombre de su
 * imagen; los dos últimos pueden estar vacíos. Es un USTRUCT para que el widget del menú, que monta el autor, la lea (R-HUD-19).
 */
USTRUCT(BlueprintType)
struct SOULBOUND_API FHowToPlayPage
{
	GENERATED_BODY()

	/** El título de la página: lo que sigue a "## ". */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString Title;

	/** El texto de la regla: los párrafos separados por una línea en blanco ("\n\n"); las líneas de un mismo párrafo, unidas con un espacio. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString Body;

	/** El comentario del Warden, sin el signo ">"; vacío si la página no lo trae. Varias líneas ">" seguidas, unidas con un espacio. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString WardenComment;

	/** El nombre de la imagen (lo que va entre paréntesis en "![](Nombre)"); vacío si la página no lleva. */
	UPROPERTY(BlueprintReadOnly, Category = "Cómo se juega")
	FString ImageName;

	bool operator==(const FHowToPlayPage& Other) const
	{
		return Title == Other.Title && Body == Other.Body && WardenComment == Other.WardenComment && ImageName == Other.ImageName;
	}
};

/** Resultado de cargar el texto de "cómo se juega" (S-022, nombre aprobado): o el libro, o el error con su motivo. */
struct FHowToPlayLoadResult
{
	EHowToPlayError Error = EHowToPlayError::None;

	/** Motivo legible del rechazo, con el archivo y, si es de una página, su título. Vacío si hay éxito. */
	FString Message;

	/** El texto tal como está en el archivo, en Markdown. Solo tiene sentido si IsSuccess(). */
	FString Text;

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
 * título de segundo nivel, con su texto, su comentario del Warden (">") y su imagen ("![](Nombre)"). Funciones puras, sin Slate. Nunca lanza.
 */
namespace Soulbound::Presentation
{
	/** Ruta del texto, relativa a Content/. */
	inline const FString HowToPlayRelativePath = TEXT("Data/Textos/como-se-juega.md");

	/** Los títulos que el texto tiene que tener, en este orden, cada uno en su propia línea. La única lista de títulos. */
	const TArray<FString>& HowToPlayHeadings();

	/**
	 * Lee un texto de "cómo se juega" ya cargado como libro (S-048, R-HUD-17). Vale si tiene al menos una página y cada página tiene título y
	 * texto; con las precisiones del autor del 17/11/2026: varias líneas ">" seguidas son un comentario; un segundo comentario o una segunda imagen
	 * separados en la misma página son un error; el texto suelto antes de la primera "##" que no sea título ni portada se ignora; las líneas de un
	 * párrafo se unen con un espacio; "![texto](Nombre)" vale y el texto alternativo se ignora.
	 * @param Markdown El contenido del archivo.
	 * @param SourceName El nombre del archivo, para los mensajes de error.
	 * @return El libro, o NoPages, EmptyPage, DuplicateComment o DuplicateImage con un mensaje que nombra el archivo y, si es de una página, su título.
	 */
	FHowToPlayLoadResult ParseHowToPlayText(const FString& Markdown, const FString& SourceName = HowToPlayRelativePath);

	/**
	 * Lee el texto de "cómo se juega" de un archivo (ruta absoluta) y lo comprueba con ParseHowToPlayText.
	 * @param FilePath Ruta del archivo.
	 * @return El libro, FileNotFound si no se puede leer o lo que diga ParseHowToPlayText.
	 */
	FHowToPlayLoadResult LoadHowToPlayText(const FString& FilePath);

	/**
	 * El texto preparado para un bloque de texto, que no entiende Markdown: los títulos sin las almohadillas ("## Qué es esto" pasa a "Qué es
	 * esto"); el resto, igual.
	 * @param Markdown El texto cargado.
	 * @return El texto para enseñar.
	 */
	FString HowToPlayDisplayText(const FString& Markdown);

	/**
	 * La ruta de la textura de una página (S-048, R-HUD-17): el nombre de la imagen dentro de la carpeta fija de la ayuda. "T_Ayuda_Turno" en
	 * "/Game/Art/Ayuda/" es "/Game/Art/Ayuda/T_Ayuda_Turno.T_Ayuda_Turno"; da igual si la carpeta acaba en "/" o no.
	 * @param Folder La carpeta (HowToPlayImageFolder de la vista).
	 * @param ImageName El nombre de la imagen de la página.
	 * @return La ruta del asset; vacía si el nombre está vacío.
	 */
	FString HowToPlayImagePath(const FString& Folder, const FString& ImageName);
}
