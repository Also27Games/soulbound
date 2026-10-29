#pragma once

#include "CoreMinimal.h"

/** Qué falló al cargar el texto de "cómo se juega" (S-022). Nombre propuesto. */
enum class EHowToPlayError : uint8
{
	None,
	/** El archivo no existe o no se puede leer. */
	FileNotFound,
	/** Falta un título exigido, o no está en su orden. */
	MissingHeading,
};

/** Resultado de cargar el texto de "cómo se juega" (S-022, nombre aprobado): o el texto, o el error con su motivo. */
struct FHowToPlayLoadResult
{
	EHowToPlayError Error = EHowToPlayError::None;

	/** Motivo legible del rechazo, con el archivo o el título afectado. Vacío si hay éxito. */
	FString Message;

	/** El texto tal como está en el archivo, en Markdown. Solo tiene sentido si IsSuccess(). */
	FString Text;

	bool IsSuccess() const { return Error == EHowToPlayError::None; }
};

/**
 * El texto de "cómo se juega" (S-022): vive en Content/Data/Textos/como-se-juega.md, que se empaqueta como archivo suelto, y lo escribe el
 * autor. Aquí solo se lee, se comprueba que tiene sus apartados y se prepara para un bloque de texto de UGT (UHowToPlayLibrary). Nunca lanza.
 */
namespace Soulbound::Presentation
{
	/** Ruta del texto, relativa a Content/. */
	inline const FString HowToPlayRelativePath = TEXT("Data/Textos/como-se-juega.md");

	/** Los títulos que el texto tiene que tener, en este orden, cada uno en su propia línea. La única lista de títulos. */
	const TArray<FString>& HowToPlayHeadings();

	/**
	 * Comprueba un texto de "cómo se juega" ya leído: no vacío y con los títulos de HowToPlayHeadings en orden.
	 * @param Markdown El contenido del archivo.
	 * @return El texto o MissingHeading con el título que falta.
	 */
	FHowToPlayLoadResult ParseHowToPlayText(const FString& Markdown);

	/**
	 * Lee el texto de "cómo se juega" de un archivo (ruta absoluta) y lo comprueba con ParseHowToPlayText.
	 * @param FilePath Ruta del archivo.
	 * @return El texto, FileNotFound si no se puede leer o lo que diga ParseHowToPlayText.
	 */
	FHowToPlayLoadResult LoadHowToPlayText(const FString& FilePath);

	/**
	 * El texto preparado para un bloque de texto, que no entiende Markdown: los títulos sin las almohadillas ("## Qué es esto" pasa a "Qué es
	 * esto"); el resto, igual.
	 * @param Markdown El texto cargado.
	 * @return El texto para enseñar.
	 */
	FString HowToPlayDisplayText(const FString& Markdown);
}
