#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoardPreviewActor.generated.h"

/**
 * Vista previa del tablero en el editor (herramienta sin story; nombre propuesto): un actor que el autor coloca en el nivel y
 * que, al pulsar "Construir", enseña el tablero sin partida (suelo de cada cuña teñido por Dominio, losas, senderos, Altar y las
 * estructuras de las casillas especiales) para decorar a mano alrededor de las losas. Coloca las piezas con FBoardKit y la
 * colocación de ComputeBoardLayout, con los LayoutParams, las piezas y el tinte de ABoardViewActor (su CDO: en el editor no hay
 * ninguna instancia), así que es el mismo tablero que verá la partida. Sin fichas, halos, cadenas, runas flotantes, HUD ni
 * etiquetas: solo lo que no cambia con la partida. Las piezas son componentes transitorios: no se guardan en el mapa, y al
 * abrirlo el actor está vacío hasta que se pulsa "Construir". Si falta una pieza del kit, no se coloca y se avisa en el registro.
 * Solo existe para el editor: es un actor de solo editor (no se cocina) y en Shipping sus funciones están vacías.
 */
UCLASS(NotBlueprintable, Config = Game)
class SOULBOUND_API ABoardPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ABoardPreviewActor();

	/**
	 * Tablero que se enseña, ruta relativa a Content/. Vacía, que es el valor inicial, significa "el que carga el anfitrión de
	 * partida": UMatchHostSubsystem::BoardRelativePath de DefaultGame.ini, que se lee al pulsar "Construir". Si se escribe una
	 * ruta, manda esa.
	 */
	UPROPERTY(EditAnywhere, Category = "Vista previa")
	FString BoardRelativePath;

	/** Carga el tablero y coloca sus piezas, tras quitar las que hubiera. Si el tablero no carga, deja el aviso en el registro y nada colocado. */
	UFUNCTION(CallInEditor, Category = "Vista previa", meta = (DisplayName = "Construir"))
	void BuildPreview();

	/** Quita todas las piezas. */
	UFUNCTION(CallInEditor, Category = "Vista previa", meta = (DisplayName = "Limpiar"))
	void ClearPreview();
};
