#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoardViewActor.generated.h"

struct FMatchState;
class ABoardTileActor;
class ACameraActor;
class APostProcessVolume;
class ASeatTokenActor;
class SChroniclePanel;
class SWidget;
class UMaterialInterface;
class UMatchHostSubsystem;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Vista de depuración de la partida (S-005; nombre propuesto): genera desde los datos, al iniciar la partida, las
 * conexiones del grafo, una casilla por casilla del tablero, una ficha por asiento y la cámara cenital, y los
 * redibuja cuando el anfitrión de partida avisa de un cambio. Lee el estado en solo lectura; no contiene reglas
 * (las casillas alcanzables son las que guarda el estado, calculadas por el motor). También pinta el texto de
 * depuración del turno, que refresca a intervalo corto porque el reloj cambia sin que cambie el estado. Desde S-006
 * enseña la carrera por el vínculo: Nicho Libre, Cerrado o con el color de quien lo reclamó, ficha cónica del
 * Elegido, tiempo de carrera, rol y hogar de cada asiento, fin de carrera y victoria del Warden.
 * Desde S-007 es legible sin leer la crónica de consola: un volumen de posproceso de exposición fija para que los
 * colores salgan como se escriben, paleta separada por Dominio, el tipo de casilla por su forma, etiqueta con el
 * identificador y el Dominio en cada casilla (TileLabel), leyenda (Legend), asiento activo elevado y con aro
 * (SeatRing), y el panel de crónica (ChroniclePanel) con todos los sucesos.
 * Sin arte: formas básicas del motor, texto del motor y Slate. Nada se coloca a mano en el mapa.
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

	// ---- Tamaños ----

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

	/** El Altar mide esto por el diámetro de una casilla. Valor inicial 1,8. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float AltarScale = 1.8f;

	/** Grosor de la losa del Altar, en cm (las demás miden 10). Valor inicial 22. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float AltarThickness = 22.f;

	/** Diámetro de la marca del tipo (cubo del Nicho, anillo de la Fuente, esfera del Nodo) por el de la casilla. Valor inicial 0,6. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float InlayScale = 0.6f;

	/** Diámetro del hueco del anillo de la Fuente por el de la casilla. Valor inicial 0,34. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float RingHoleScale = 0.34f;

	// ---- Etiquetas ----

	/** Alto de la letra de la etiqueta de casilla (TileLabel), en cm. Valor inicial 24. */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float LabelSize = 24.f;

	/** Distancia de la etiqueta al centro, hacia abajo de la pantalla, por el diámetro de la casilla. Valor inicial 0,39. */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float LabelOffsetFraction = 0.39f;

	/** Alto de la letra del número de la ficha, en cm. Valor inicial 26. */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float TokenLabelSize = 26.f;

	// ---- Asiento activo ----

	/** Cuánto sube la ficha del asiento activo, en cm. Valor inicial 45. */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float ActiveLift = 45.f;

	/** Grados por segundo que gira la ficha activa. Valor inicial 90 (una vuelta cada cuatro segundos). */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float ActiveSpinDegreesPerSecond = 90.f;

	/** Diámetro del aro de color (SeatRing) bajo la ficha activa, en cm. Valor inicial 150. */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float RingDiameter = 150.f;

	/** Ancho del aro, en cm. Valor inicial 10. */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float RingWidth = 10.f;

	// ---- Paneles ----

	/** Ancho de la columna de la derecha (leyenda y crónica), en píxeles. La cámara deja ese hueco al tablero. Valor inicial 440. */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	float PanelWidthPixels = 440.f;

	/** Tamaño de la letra de los paneles. Valor inicial 11 (se lee a 1280x800). */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	int32 PanelFontSize = 11;

	/** Líneas de crónica que se conservan; al pasarse se recortan las más viejas. Valor inicial 300. */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	int32 ChronicleMaxLines = 300;

	// ---- Exposición ----

	/** Exposición manual fija del volumen de posproceso, como EV100: sube la imagen al bajar. Valor inicial 9, a ajustar en PIE. */
	UPROPERTY(EditAnywhere, Category = "Vista|Exposicion")
	float ExposureEV100 = 9.f;

	// ---- Colores. Seis Dominios con tonos separados; tipos y estados contrastados con ellos. ----

	/** Color de cada Dominio (0 a 5): azul, rojo, verde, magenta, cian, amarillo. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	TArray<FLinearColor> DomainColors;

	/** Color de cada asiento (0 a 3), claros para distinguirse de los Dominios. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	TArray<FLinearColor> SeatColors;

	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor AltarColor = FLinearColor(0.9f, 0.4f, 0.f);

	/** Marca del Nicho Libre. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor NicheFreeColor = FLinearColor(0.55f, 0.15f, 1.f);

	/** Marca del Nicho Cerrado (Dominio Olvidado). */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor NicheClosedColor = FLinearColor(0.12f, 0.12f, 0.12f);

	/** Anillo de la Fuente de Poder. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor SourceColor = FLinearColor(1.f, 0.5f, 0.05f);

	/** Esfera del Nodo de Poder. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor NodeColor = FLinearColor(0.7f, 1.f, 0.7f);

	/** Casilla alcanzable por la tirada. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor ReachableColor = FLinearColor(1.f, 1.f, 1.f);

	/** Casilla sellada. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor SealedColor = FLinearColor(0.01f, 0.01f, 0.01f);

	/** Cuánto se apaga la casilla de un Dominio Olvidado (multiplica el color). Valor inicial 0,3. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	float ForgottenDim = 0.3f;

	/** Línea de crónica de las intervenciones del Warden. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor WardenLineColor = FLinearColor(1.f, 0.45f, 0.05f);

	/** Línea de crónica de la carrera por el vínculo. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor RaceLineColor = FLinearColor(0.f, 0.85f, 1.f);

private:
	/** Posición en el mundo de cada casilla: la del archivo escalada, o un anillo por Dominio si falta alguna. */
	TArray<FVector> ComputeTilePositions() const;
	void BuildEdges();
	void BuildTiles();
	void BuildTokens();
	void BuildCamera();
	void BuildPostProcess();
	void BuildOverlay();

	/** Coloca la cámara cenital para que el tablero quepa en el hueco que dejan los paneles, con el tamaño actual del viewport. */
	void UpdateCamera();

	/** Repinta casillas y recoloca fichas según el estado. */
	void Refresh();
	/** Reconstruye el panel de crónica si el registro de eventos ha crecido. */
	void RefreshChronicle(const FMatchState& State);
	/** Texto de depuración del turno; y pone la cámara como vista del jugador local si aún no lo es. */
	void TickHud();
	void ShowText(int32 Line, const FColor& Color, const FString& Text) const;

	FLinearColor DomainColor(int32 DomainId) const;
	FLinearColor SeatColor(int32 Seat) const;

	TWeakObjectPtr<UMatchHostSubsystem> Host;
	FDelegateHandle ChangedHandle;
	FTimerHandle HudTimer;
	TArray<FVector> TilePositions;
	/** Altura de la cara superior de cada casilla. */
	TArray<float> TileTops;
	FVector2D BoardCenter = FVector2D::ZeroVector;
	FVector2D BoardHalf = FVector2D::ZeroVector;
	FVector2D LastViewportSize = FVector2D::ZeroVector;
	int32 ChronicleLineCount = -1;

	TSharedPtr<SChroniclePanel> ChroniclePanel;
	TSharedPtr<SWidget> Overlay;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> EdgeStrips;

	/** Marca del tipo sobre cada casilla (nula si no lleva): cubo, anillo o esfera. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Inlays;

	/** Hueco del anillo de la Fuente (nulo en las demás). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> InlayHoles;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> Labels;

	UPROPERTY()
	TArray<TObjectPtr<ABoardTileActor>> TileActors;

	UPROPERTY()
	TArray<TObjectPtr<ASeatTokenActor>> TokenActors;

	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> PostProcess;
};
