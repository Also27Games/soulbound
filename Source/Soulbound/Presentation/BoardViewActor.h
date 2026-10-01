#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoardViewActor.generated.h"

class ABoardTileActor;
class ACameraActor;
class ASeatTokenActor;
class UMaterialInterface;
class UMatchHostSubsystem;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Vista de depuración de la partida (S-005; nombre propuesto): genera desde los datos, al iniciar la partida, las
 * conexiones del grafo, una casilla por casilla del tablero, una ficha por asiento y la cámara cenital, y los
 * redibuja cuando el anfitrión de partida avisa de un cambio. Lee el estado en solo lectura; no contiene reglas
 * (las casillas alcanzables son las que guarda el estado, calculadas por el motor). También pinta el texto de
 * depuración del turno, que refresca a intervalo corto porque el reloj cambia sin que cambie el estado. Desde S-006
 * enseña la carrera por el vínculo: Nicho Libre, Cerrado o con el color de quien lo reclamó, ficha cónica del
 * Elegido, tiempo de carrera, rol y hogar de cada asiento, fin de carrera y victoria del Warden.
 * Sin arte: formas básicas del motor y el material básico con parámetro de color. Nada se coloca a mano en el mapa.
 */
UCLASS(NotBlueprintable)
class SOULBOUND_API ABoardViewActor : public AActor
{
	GENERATED_BODY()

public:
	ABoardViewActor();

	/** Genera la vista de la partida de Host y se suscribe a sus cambios. Se llama una vez, con la partida iniciada. */
	void Initialize(UMatchHostSubsystem* Host);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Centímetros del mundo por unidad de posición del archivo del tablero. Valor inicial 3. */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float PositionScale = 3.f;

	/** Diámetro de una casilla, en cm. Valor inicial 110. */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float TileDiameter = 110.f;

	/** Grosor de la tira de una conexión, en cm. Valor inicial 14. */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float EdgeWidth = 14.f;

	/** Diámetro de una ficha, en cm. Valor inicial 40. */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float TokenDiameter = 40.f;

	/** Segundos entre refrescos del texto y de la cámara. Valor inicial 0,25 (4 Hz). */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float HudIntervalSeconds = 0.25f;

private:
	/** Posición en el mundo de cada casilla: la del archivo escalada, o un anillo por Dominio si falta alguna. */
	TArray<FVector> ComputeTilePositions() const;
	void BuildEdges();
	void BuildTiles();
	void BuildTokens();
	void BuildCamera();

	/** Repinta casillas y recoloca fichas según el estado. */
	void Refresh();
	/** Texto de depuración del turno; y pone la cámara como vista del jugador local si aún no lo es. */
	void TickHud();
	void ShowText(int32 Line, const FColor& Color, const FString& Text) const;

	TWeakObjectPtr<UMatchHostSubsystem> Host;
	FDelegateHandle ChangedHandle;
	FTimerHandle HudTimer;
	TArray<FVector> TilePositions;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> EdgeStrips;

	UPROPERTY()
	TArray<TObjectPtr<ABoardTileActor>> TileActors;

	UPROPERTY()
	TArray<TObjectPtr<ASeatTokenActor>> TokenActors;

	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;
};
