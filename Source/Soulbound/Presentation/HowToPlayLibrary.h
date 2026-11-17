#pragma once

#include "CoreMinimal.h"
#include "GrimoireBook.h"
#include "HowToPlay.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HowToPlayLibrary.generated.h"

class UTexture2D;

/**
 * "Cómo se juega" para Blueprint (S-022; nombre aprobado). El texto vive en Content/Data/Textos/como-se-juega.md y lo escribe el autor; la carga
 * y la comprobación son de Presentation/HowToPlay.h. Desde S-048 (R-HUD-19) da también el libro, el mismo que lee el Grimorio (HowToPlayBook): sus
 * páginas, la de teclas al final, el título del libro y la frase de la portada, para el widget del menú que monta el autor. Las teclas y la carpeta
 * de las imágenes son las de la vista (ABoardViewActor, en Config), para que el menú y la partida digan lo mismo.
 */
UCLASS()
class SOULBOUND_API UHowToPlayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * El texto de "cómo se juega", listo para un bloque de texto: desde S-048, el libro aplanado (HowToPlayFlatText). Si el archivo no se puede
	 * leer o no vale, devuelve un texto vacío y deja el motivo en el log.
	 * TODO(R-HUD-19): quitarlo cuando el widget nuevo del menú lea las páginas (precisión del autor del 17/11/2026: lo usa el widget de hoy).
	 */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static FText HowToPlayText();

	/** S-048 (R-HUD-19): el título del libro, para la portada; vacío si el archivo no lo trae o no carga. */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static FText HowToPlayBookTitle();

	/** S-048 (R-HUD-19): la frase de la portada, del Warden; vacía si el archivo no la trae o no carga. */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static FText HowToPlayCoverLine();

	/** S-048 (R-HUD-19): cuántas páginas tiene el libro, la de teclas incluida; cero si el archivo no carga. */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static int32 HowToPlayPageCount();

	/**
	 * S-048 (R-HUD-19): una página, desde 0: título, texto sin marcas, bloques y nombre de su imagen (desde la ampliación del autor del 17/11/2026,
	 * sin el comentario del Warden: las frases van en sus bloques). Fuera de rango, una página vacía.
	 */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static FHowToPlayPage HowToPlayPage(int32 Index);

	/**
	 * S-048 (ampliación del autor del 17/11/2026, R-HUD-19): una página, desde 0, en texto enriquecido para un bloque de texto enriquecido de UMG
	 * (HowToPlayRichText), con las marcas <Bold>, <Warden> y <WardenBold>, sin el título. Fuera de rango, vacío.
	 */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static FText HowToPlayPageRichText(int32 Index);

	/**
	 * S-048 (R-HUD-19): la imagen de una página, de la carpeta de la ayuda (HowToPlayImageFolder). Nula si la página no lleva o si la textura no
	 * existe; esto último se dice una vez en el registro por cada nombre.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cómo se juega")
	static UTexture2D* HowToPlayPageImage(int32 Index);

	/** S-048 (R-HUD-19): el mensaje del cargador si el archivo no carga; vacío si carga. */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static FText HowToPlayError();

	/**
	 * El libro de un archivo (S-048, R-HUD-19), con las teclas configuradas en la vista: lo que leen todas las funciones de arriba con el archivo del
	 * repositorio. Público para los tests (un archivo roto).
	 * @param FilePath Ruta absoluta del archivo.
	 */
	static Soulbound::Presentation::FHowToPlayBook BookFromFile(const FString& FilePath);

	/** S-048 (R-HUD-19): las teclas que nombra la página "Teclas", las de la vista (el objeto por defecto de ABoardViewActor, con su Config). */
	static Soulbound::Presentation::FGrimoireKeyNames ConfiguredKeyNames();
};
