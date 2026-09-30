#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoardTileActor.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Casilla en pantalla (glosario "Tile"; actor propuesto en S-005): una forma básica del motor con un color. Es un
 * adaptador de presentación: no sabe de reglas, solo de qué casilla es y de qué color se pinta. El clic sobre ella
 * lo recoge el controlador del jugador y envía la intención de destino.
 */
UCLASS(NotBlueprintable)
class SOULBOUND_API ABoardTileActor : public AActor
{
	GENERATED_BODY()

public:
	ABoardTileActor();

	/** Identificador de la casilla del tablero que representa. */
	void SetTileId(int32 InTileId) { TileId = InTileId; }
	int32 GetTileId() const { return TileId; }

	/** Pinta la casilla. */
	void SetColor(const FLinearColor& Color);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	int32 TileId = INDEX_NONE;
};
