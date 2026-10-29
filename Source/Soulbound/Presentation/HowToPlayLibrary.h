#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HowToPlayLibrary.generated.h"

/**
 * "Cómo se juega" para Blueprint (S-022; nombre aprobado): la página de UGT enlaza HowToPlayText a un bloque de texto. El texto vive en
 * Content/Data/Textos/como-se-juega.md y lo escribe el autor; la carga y la comprobación son de Presentation/HowToPlay.h.
 */
UCLASS()
class SOULBOUND_API UHowToPlayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * El texto de "cómo se juega", listo para un bloque de texto: los títulos sin "## " y el resto tal cual (HowToPlayDisplayText). Si el
	 * archivo no se puede leer o le falta un apartado, devuelve un texto vacío y deja el motivo en el log.
	 */
	UFUNCTION(BlueprintPure, Category = "Cómo se juega")
	static FText HowToPlayText();
};
