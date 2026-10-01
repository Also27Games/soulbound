#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoardTileActor.generated.h"

class UMaterialInterface;
class UStaticMesh;
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

	/** Forma de la losa: cuadrada (cubo) o redonda (cilindro, la de origen). Conserva el color. S-007: el tipo se ve por la forma. */
	void SetSquareShape(bool bSquare);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> RoundMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SquareMesh;

	int32 TileId = INDEX_NONE;
};
