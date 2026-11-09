#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "BoardKit.h"
#include "BoardLayout.h"
#include "InstructionTracker.h"
#include "WardenLineQueue.h"
#include "BoardViewActor.generated.h"

struct FBoardViewState;
struct FMatchState;
class ABoardTileActor;
class ACameraActor;
class APostProcessVolume;
class ASeatTokenActor;
class IInputProcessor;
class SChroniclePanel;
class SWidget;
struct FRingOutcome;
struct FRingPanelModel;
enum class ESkillTrial : uint8;
enum class ESkillDifficulty : uint8;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMatchHostSubsystem;
class USkyLightComponent;
class USpotLightComponent;
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
 * Desde S-009 enseña la elección pendiente: una línea de petición con las opciones numeradas y el tiempo que queda, el
 * halo en los Nodos a los que se puede saltar, y el efecto de cada Nodo en su etiqueta tras el Despertar.
 * Desde S-019 coloca el kit de arte desde los datos sobre una mesa de radio de datos (BoardLayout): una losa por casilla,
 * senderos por conexión, la cuña de cada Dominio, el Altar, las estructuras de las casillas especiales, las cadenas de los
 * Sellos y la carta de cada ficha, con la lámpara de la mesa; lo que cambia con la partida sale del estado de vista
 * (BoardViewState). Cada pieza es una referencia blanda en Config (DefaultGame.ini, sección
 * [/Script/Soulbound.BoardViewActor]): vacía, no hay pieza; puesta pero inexistente, la forma básica de hoy como respaldo y
 * un aviso en el registro. Formas básicas, texto del motor y Slate siguen encima. Nada se coloca a mano en el mapa: el
 * decorado del autor vive en el nivel, fuera de esta vista.
 */
UCLASS(NotBlueprintable, Config = Game)
class SOULBOUND_API ABoardViewActor : public AActor
{
	GENERATED_BODY()

public:
	ABoardViewActor();

	/** Genera la vista de la partida de Host y se suscribe a sus cambios. Se llama una vez, con la partida iniciada. */
	void Initialize(UMatchHostSubsystem* Host);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Si Component es el orbe de la consulta del Oráculo (S-017): la vista lo enseña mientras alguien consulta, siempre sobre su
	 * ficha (decisión del autor del 26/10/2026). Un clic en él consulta; el clic derecho, el rechazo de Tifón.
	 */
	bool IsOracleOrb(const UPrimitiveComponent* Component) const;

	// ---- Mesa (S-019) ----

	/**
	 * Medidas de la mesa y del kit para colocar el tablero (R-VIS-1 a 5): radio de mesa, losa, Altar, tramo de sendero, losas
	 * partidas y distancias de las estructuras. Valores iniciales en FBoardLayoutParams. Las formas de respaldo miden lo
	 * mismo que las piezas: la losa básica, 2 por SlabHalfSize; el Altar básico, 2 por AltarApothem. En Config
	 * (DefaultGame.ini, sección [/Script/Soulbound.BoardViewActor]) para ajustarlo sin compilar.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Mesa")
	FBoardLayoutParams LayoutParams;

	// ---- Tamaños de las formas de respaldo y de las marcas. Desde S-019, los de S-005 a S-007 por 6,3/175: la mesa mide
	// lo que mide el kit. ----

	/** Grosor de la tira de una conexión (respaldo sin sendero), en cm. Valor inicial 0,8 (22 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float EdgeWidth = 0.8f;

	/** Diámetro de una ficha de formas básicas (respaldo sin carta), en cm. Valor inicial 2,2 (62 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float TokenDiameter = 2.2f;

	/**
	 * Segundos que tarda la ficha en deslizarse en línea recta a su casilla cuando la lleva algo que no es un movimiento (Vacío,
	 * hogar, Expulsión, salto planar, depuración). 0 la coloca sin deslizar. Valor inicial 0,5. Desde S-020, un movimiento
	 * recorre su camino (TokenSpeed).
	 */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float TokenSlideSeconds = 0.5f;

	/**
	 * Velocidad de la ficha por su camino, en cm por segundo (S-020): recorre el camino tramo a tramo, en línea recta entre
	 * centros y a velocidad constante, y al entrar en cada casilla entrega la intención de avance de su asiento, humano o dummy. Valor inicial 13
	 * (decisión del autor del 21/10/2026, probando en PIE): unos 0,6 s entre losas vecinas y, en el Altar, el rodeo del trono más la salida, unos 6 s: la ficha se mueve siempre igual. La salvaguarda
	 * del motor (moveStepSeconds, 8 en partida.json) tiene que superar siempre el tramo más largo de esta animación; si se
	 * sube TokenSpeed, puede bajar, y si se baja, hay que subirla. Sustituye al tiempo fijo por tramo, con el que el tramo del
	 * Altar salía disparado.
	 */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float TokenSpeed = 13.f;

	/** Segundos entre refrescos del texto y de la cámara. Valor inicial 0,25 (4 Hz). */
	UPROPERTY(EditAnywhere, Category = "Vista")
	float HudIntervalSeconds = 0.25f;

	/** Grosor del Altar de respaldo, en cm (las losas, LayoutParams.SlabTop). Valor inicial 0,8 (22 antes de S-019). Con la pieza, su alto. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float AltarThickness = 0.8f;

	/** Diámetro de la marca del tipo (cubo del Nicho, cruz de la Fuente, esfera del Nodo) por el de la casilla. Valor inicial 0,6. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float InlayScale = 0.6f;

	/** Grosor de los brazos de la cruz de la Fuente por su largo. Valor inicial 0,3. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float CrossArmThickness = 0.3f;

	/** Diámetro del halo bajo una casilla alcanzable, por el de la casilla. Valor inicial 1,35. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float HaloScale = 1.35f;

	/**
	 * Diámetro del halo del aspirante bajo el Altar (S-018), por el del halo de alcanzable: más ancho, para que se vea alrededor. Con
	 * dos o más aspirantes, cada halo es AspirantHaloStep más ancho que el anterior. Valores iniciales 1,25 y 0,15.
	 */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float AspirantHaloScale = 1.25f;

	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float AspirantHaloStep = 0.15f;

	/** Diámetro del aro de las casillas de un Dominio Olvidado, por el de la casilla. Valor inicial 1,2. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float ForgottenRingScale = 1.2f;

	/** Cuánto crece el contorno negro de las marcas de tipo (cubo, cruz, esfera) por la propia marca. Valor inicial 1,18. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float OutlineScale = 1.18f;

	// ---- Etiquetas ----

	/**
	 * Alto de la letra de la etiqueta de casilla (TileLabel), en cm. Valor inicial 1,8 (40 antes de S-019): "N D3·41" mide
	 * unos 7 y las losas más juntas del tablero de 35 distan 9,2; a 1,4 no se leía a 1280x800.
	 */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float LabelSize = 1.8f;

	/** Distancia de la etiqueta al centro, hacia abajo de la pantalla, por el diámetro de la casilla. Valor inicial 0,4. */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float LabelOffsetFraction = 0.4f;

	/** Alto de la letra del número de la ficha, en cm. Valor inicial 1,4 (40 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float TokenLabelSize = 1.4f;

	/** Radio del aro del asiento en un Nicho reclamado, por el diámetro de la casilla. Valor inicial 0,7. */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float NicheRingRadiusScale = 0.7f;

	/** Ancho del aro del Nicho reclamado, en cm. Valor inicial 0,45 (12 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista|Formas")
	float NicheRingWidth = 0.45f;

	/** Alto de la letra del número de asiento sobre el cubo del Nicho reclamado, en cm. Valor inicial 2,2 (60 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float NicheNumberSize = 2.2f;

	/** Color del contador de reserva de las Fuentes (S-008): azul claro, para que no se confunda con la etiqueta blanca. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FColor SourceReserveColor = FColor(70, 170, 255);

	/** Alto de la letra del nombre del Elegido en la etiqueta del Nicho, en cm. Valor inicial 1,1 (30 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float NicheNameSize = 1.1f;

	/** Distancia del nombre al centro del Nicho, hacia arriba de la pantalla, por el diámetro de la casilla. Valor inicial 0,55. */
	UPROPERTY(EditAnywhere, Category = "Vista|Etiquetas")
	float NicheNameOffsetFraction = 0.55f;

	// ---- Asiento activo ----

	/** Cuánto sube la ficha del asiento activo, en cm. Valor inicial 2,5 (70 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float ActiveLift = 2.5f;

	/** Cuánto crece la ficha activa (1 = nada). Se ve desde arriba mejor que la elevación. Valor inicial 1,3. */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float ActiveScale = 1.3f;

	/** Grados por segundo que gira la ficha activa. Valor inicial 90 (una vuelta cada cuatro segundos). */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float ActiveSpinDegreesPerSecond = 90.f;

	/** Diámetro del aro de color (SeatRing) bajo la ficha activa, en cm. Valor inicial 8,5 (235 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float RingDiameter = 8.5f;

	/** Ancho del aro, en cm. Valor inicial 0,5 (14 antes de S-019). */
	UPROPERTY(EditAnywhere, Category = "Vista|Asiento activo")
	float RingWidth = 0.5f;

	// ---- Ficha con la carta del kit (S-019, R-VIS-6) ----

	/** Diámetro del aro del rol en la base de la carta, por la huella de la carta. Valor inicial 1,25. */
	UPROPERTY(EditAnywhere, Category = "Vista|Ficha")
	float TokenRingScale = 1.25f;

	/** Ancho del aro del rol, en cm. Valor inicial 0,25. */
	UPROPERTY(EditAnywhere, Category = "Vista|Ficha")
	float TokenRingWidth = 0.25f;

	/** Brillo del aro del rol de un Eco, por el de un Elegido (decisión del autor del 20/10/2026). Valor inicial 0,5. */
	UPROPERTY(EditAnywhere, Category = "Vista|Ficha", meta = (ClampMin = "0", ClampMax = "1"))
	float EchoRingBrightness = 0.5f;

	// ---- Oráculo (S-017) ----

	/** Diámetro del orbe de la consulta, en cm. Valor inicial 3 (es la vista de depuración, no el orbe del GDD 07). */
	UPROPERTY(EditAnywhere, Category = "Vista|Oráculo")
	float OracleOrbDiameter = 3.f;

	/** Altura del centro del orbe sobre la base de la ficha del consultante, en cm. Valor inicial 7. */
	UPROPERTY(EditAnywhere, Category = "Vista|Oráculo")
	float OracleOrbHeight = 7.f;

	// ---- Paneles ----

	/** Ancho de la columna de la derecha (leyenda y crónica), en píxeles. La cámara deja ese hueco al tablero. Valor inicial 460. */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	float PanelWidthPixels = 460.f;

	/** Tamaño de la letra de los paneles. Valor inicial 13 (se lee a 1280x800). */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	int32 PanelFontSize = 13;

	/** Líneas de crónica que se conservan; al pasarse se recortan las más viejas. Valor inicial 300. */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	int32 ChronicleMaxLines = 300;

	/**
	 * Segundos que se enseña cada frase de la línea del Warden, grande en la parte baja de la pantalla, o hasta la siguiente línea (S-021,
	 * R-VOZ-1 sin audio). Valor inicial 6; en DefaultGame.ini.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	float WardenLineSeconds = 6.f;

	/** Tamaño de la letra de la línea del Warden (S-021). Valor inicial 28. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 WardenLineFontSize = 28;

	/**
	 * Segundos que el panel del anillo enseña el resultado ("superada", "fallada" o el rendimiento del duelo) tras cerrarse la prueba (S-026).
	 * Valor inicial 2 (Config/DefaultGame.ini). Cero: no se enseña.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	float RingResultSeconds = 2.f;

	/** Tamaño de la letra del título del panel del anillo (S-026); el resto se escala con él. Valor inicial 20. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 RingFontSize = 20;

	/** Margen de la cámara alrededor del tablero (1 = justo al borde). Valor inicial 1,04. */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	float CameraMargin = 1.04f;

	// ---- Luz y exposición (S-019, R-VIS-7: los valores de la prueba de conjunto) ----
	// En Config (DefaultGame.ini, sección [/Script/Soulbound.BoardViewActor]) para ajustarlos sin compilar.

	/** Exposición manual fija del volumen de posproceso, como EV100: sube la imagen al bajar. Valor inicial 4 (9 antes de S-019). */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Luz")
	float ExposureEV100 = 4.f;

	/** Intensidad de la lámpara sobre la mesa (foco cenital), en candelas. Valor inicial 600. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Luz")
	float LampIntensityCandelas = 600.f;

	/** Cono interior de la lámpara, en grados. Valor inicial 15. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Luz")
	float LampInnerConeDegrees = 15.f;

	/** Cono exterior de la lámpara, en grados. Valor inicial 25. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Luz")
	float LampOuterConeDegrees = 25.f;

	/** Altura de la lámpara sobre la mesa, en cm. Valor inicial 200. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Luz")
	float LampHeight = 200.f;

	/** Intensidad del cielo tenue (luz de cielo, sin cielo visible ni sol). Valor inicial 0,5. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Luz")
	float SkyLightIntensity = 0.5f;

	// ---- Cámaras y teclas (S-019, R-VIS-9). Las teclas, en Config (DefaultGame.ini, sección [/Script/Soulbound.BoardViewActor]).
	// Las de ocultar son independientes y se combinan. ----

	/** Tecla que alterna la cenital con la vista sentada del asiento humano. Valor inicial C. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey SeatedViewKey = EKeys::C;

	/**
	 * Si la partida arranca en la vista sentada del asiento humano (S-022, R-CAM-1; nombre aprobado). Valor inicial true; false arranca en la
	 * cenital, sin compilar. La vista previa del editor (ABoardPreviewActor) no lo mira.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	bool bStartSeated = true;

	/**
	 * Tecla que, con la partida terminada, hace lo mismo que el botón "Volver al menú": cierra la sesión en línea si la hay y vuelve al mapa
	 * del menú (GameDefaultMap) (S-022; nombre aprobado). Es la alternativa al botón. Valor inicial Intro: no Escape, que es la pausa de UGT
	 * (R-FLU-5) y en PIE lo captura el editor (decisión del autor del 29/10/2026). Antes del final no hace nada y no se la queda.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey ReturnToMenuKey = EKeys::Enter;

	/** Tamaño de la letra del botón "Volver al menú" (S-022). Valor inicial 28, como la línea del Warden. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 ReturnToMenuFontSize = 28;

	/** Tecla que oculta y muestra etiquetas para las capturas. Valor inicial L. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey OverlayToggleKey = EKeys::L;

	/** Tecla que oculta y muestra los paneles de Slate (leyenda y crónica); ocultos, la cenital usa todo el ancho. Valor inicial H. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey PanelsToggleKey = EKeys::H;

	/** Tecla que oculta y muestra el texto de depuración de arriba a la izquierda (turno, reloj, Warden, semilla, asientos, petición). Valor inicial T. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey DebugTextToggleKey = EKeys::T;

	/** Color de un Dominio (0 a 5); gris oscuro si no existe. Lo usa también el kit para teñir el suelo de las cuñas. */
	FLinearColor DomainColor(int32 DomainId) const;

	/** Las teclas de la vista y lo que hace cada una, en una línea, con la vista actual (para SbHelp y la línea del paso a paso; ViewKeysText). */
	FString DescribeViewKeys() const;

	// La vista sentada, en Config (DefaultGame.ini, sección [/Script/Soulbound.BoardViewActor]) para ajustarla sin compilar;
	// en partida, con SbSeatedView.

	/** Dónde se sienta el humano alrededor de la mesa, en grados desde el +X de la mesa. Valor inicial 180: el borde de abajo de la cenital. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	float SeatedViewYawDegrees = 180.f;

	/** Altura de la vista sentada sobre la mesa, en cm. Valor inicial 60. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	float SeatedViewHeight = 60.f;

	/** Distancia de la vista sentada fuera del borde de la mesa, en cm. Valor inicial 80. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	float SeatedViewDistance = 80.f;

	/** Inclinación de la vista sentada hacia abajo, en grados; el giro, hacia el centro. Valor inicial 30. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	float SeatedViewPitchDegrees = 30.f;

	/**
	 * Cambia la vista sentada al instante (SbSeatedView) y entra en ella si no está activa: altura sobre la mesa y distancia
	 * fuera del borde, en cm, inclinación hacia abajo y, si viene, el ángulo del asiento, en grados (180 es el borde de abajo
	 * de la cenital; menos, hacia la derecha de la pantalla; más, hacia la izquierda). Sin ángulo, conserva el que tiene. No
	 * guarda nada en Config.
	 */
	void SetSeatedView(float Height, float Distance, float PitchDegrees, TOptional<float> YawDegrees = {});

	/** Los cuatro valores actuales de la vista sentada, en una línea, y si está activa. */
	FString DescribeSeatedView() const;

	// ---- Kit de arte (S-019, R-VIS-8): referencias blandas en Config, con valores iniciales en /Game/Art/Tablero/ según
	// kit.json. Vacía, sin pieza; puesta pero inexistente, la forma básica de hoy y un aviso en el registro. ----

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> SlabMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> CrackedSlabMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> NicheSlabMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> SourceSlabMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> NodeSlabMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> PathSegmentMesh;

	/** Peana de la cuña de un Dominio. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> PedestalMesh;

	/** Suelo liso de la cuña de un Dominio. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> PlainFloorMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> AltarMesh;

	/** El trono del Nicho. Vacío si gana la variante "el Nicho es el trono": entonces no hay trono. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> NicheThroneMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> PylonMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> FloatingRuneMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> PlanarArchMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> DuelSwordsMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> OracleOrbMesh;

	/** Las cadenas del Sello, sobre la losa. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> SealChainsMesh;

	/** La carta de prueba de la ficha. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UStaticMesh> TokenCardMesh;

	/** Material con el color por vértice en Base Color: el de todas las ranuras del kit salvo las emisivas y las tenues. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UMaterialInterface> VertexColorMaterial;

	/** Material emisivo (cian), con un parámetro de color "Color": las ranuras emisivas del kit y el aro del rol de la ficha. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UMaterialInterface> EmissiveMaterial;

	/** Material tenue (cian): las ranuras tenues del kit. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit")
	TSoftObjectPtr<UMaterialInterface> DimMaterial;

	/** Una ranura de material del kit cuyo nombre contiene esto lleva el material emisivo. Valor inicial "Emisivo". */
	UPROPERTY(EditAnywhere, Category = "Vista|Kit")
	FString EmissiveSlotTag = TEXT("Emisivo");

	/** Una ranura de material del kit cuyo nombre contiene esto lleva el material tenue. Valor inicial "Tenue". */
	UPROPERTY(EditAnywhere, Category = "Vista|Kit")
	FString DimSlotTag = TEXT("Tenue");

	/** La referencia configurada de una pieza del kit (S-019, R-VIS-8). */
	const TSoftObjectPtr<UStaticMesh>& KitReference(EBoardKitPiece Piece) const;

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

	/** Cruz de la Fuente de Poder. Blanca con contorno negro: se distingue sobre los seis Dominios. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor SourceColor = FLinearColor(1.f, 1.f, 1.f);

	/** Contorno de las marcas de tipo. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor OutlineColor = FLinearColor(0.f, 0.f, 0.f);

	/** Esfera del Nodo de Poder. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor NodeColor = FLinearColor(0.7f, 1.f, 0.7f);

	/** Halo bajo la casilla alcanzable por la tirada: la casilla conserva su color. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor ReachableColor = FLinearColor(1.f, 1.f, 1.f);

	/** Casilla sellada. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor SealedColor = FLinearColor(0.01f, 0.01f, 0.01f);

	/** Aro bajo las casillas de un Dominio Olvidado (además de apagarlas). */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor ForgottenRingColor = FLinearColor(0.12f, 0.12f, 0.14f);

	/** Cuánto se apaga la casilla de un Dominio Olvidado (multiplica el color). Valor inicial 0,3. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	float ForgottenDim = 0.3f;

	/**
	 * Cuánto se tiñe el suelo de cada cuña (`peana` y `suelo_liso`) con el color de su Dominio: 0 lo deja blanco, 1 lo pone del
	 * color entero (WedgeFloorTint). Un Dominio Olvidado además se apaga por ForgottenDim. Valor inicial 0,35 (S-019, R-VIS-2, R-VIS-5).
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Vista|Colores", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FloorTintStrength = 0.35f;

	/** Cuánto se apaga la cruz de una Fuente agotada (multiplica su color). Valor inicial 0,15 (S-008). */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	float SourceDepletedDim = 0.15f;

	/** Línea de crónica de las intervenciones del Warden. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor WardenLineColor = FLinearColor(1.f, 0.45f, 0.05f);

	/** Línea de crónica de la carrera por el vínculo. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor RaceLineColor = FLinearColor(0.f, 0.85f, 1.f);

	/** Línea de crónica de las ediciones de depuración (S-011): un color que no usa nada más. */
	UPROPERTY(EditAnywhere, Category = "Vista|Colores")
	FLinearColor DebugLineColor = FLinearColor(1.f, 0.f, 1.f);

private:
	void BuildEdges();
	void BuildTiles();
	/** Las piezas del kit que haya: cuñas, Altar, losas, senderos, estructuras, runas flotantes y cadenas (S-019). */
	void BuildKit();
	void BuildTokens();
	void BuildCamera();
	/** Volumen de exposición fija, lámpara y cielo tenue (S-019, R-VIS-7). */
	void BuildLighting();
	void BuildOverlay();

	/** Enseña lo que pide el estado de vista: estructuras, runas flotantes y cadenas (S-019, R-VIS-5). */
	void RefreshKit(const FBoardViewState& View);
	/** Alterna la cenital y la vista sentada (S-019, R-VIS-9). */
	void ToggleSeatedView();
	/** Coloca la cámara sentada con los parámetros actuales: fuera del borde, sobre la mesa, inclinada y girada al centro. */
	void PlaceSeatedCamera();
	/** Oculta o enseña etiquetas (S-019, R-VIS-9). */
	void ToggleOverlay();
	/** Oculta o enseña la leyenda y la crónica, y recoloca la cenital en el hueco que queda. */
	void TogglePanels();
	/** Oculta o enseña el texto de depuración de pantalla; al ocultarlo, lo quita ya. */
	void ToggleDebugText();
	/** Pone como vista del jugador local la cámara que toca, y le engancha las teclas de la vista la primera vez. */
	void ApplyViewTarget();
	/** Si la partida ha terminado: entonces existe el botón "Volver al menú" y vale la tecla (S-022). */
	bool IsMatchOverShown() const;
	/**
	 * Vuelve al menú (S-022), por el botón o por la tecla: solo con la partida terminada y una sola vez; cierra la sesión en línea si la hay
	 * y viaja al menú.
	 */
	void ReturnToMenu();
	/** Cerrada (o no) la sesión en línea, viaja al menú (S-022): un cierre fallido se anota y se viaja igual. */
	UFUNCTION()
	void HandleSessionLeftForMenu(bool bWasSuccessful);
	/** Abre el mapa del menú, el GameDefaultMap de DefaultEngine.ini, leído de UGameMapsSettings (S-022). */
	void TravelToMenu();
	/** Lado de la losa de respaldo: el de la pieza. */
	float SlabSize() const { return LayoutParams.SlabHalfSize * 2.f; }

	/** Coloca la cámara cenital para que el tablero quepa en el hueco que dejan los paneles, con el tamaño actual del viewport. */
	void UpdateCamera();

	/** Repinta casillas y recoloca fichas según el estado. */
	void Refresh();
	/**
	 * Fichas (S-020): la del asiento que se mueve, humano o dummy, recorre lo que le queda del camino según el estado; un recorrido
	 * que el estado deja atrás (salvaguarda, corte, otra vía) se corta y la ficha se coloca donde dice el estado; lo que no es un
	 * movimiento, desliza.
	 */
	void RefreshTokens(const FMatchState& State);
	/** El orbe de la consulta del Oráculo (S-017): visible mientras alguien consulta, del color de su asiento; oculto si no. */
	void RefreshOracleOrb(const FMatchState& State);
	/** Los halos del aspirante (S-018): bajo el Altar, uno del color de cada asiento aspirante, de dentro afuera por asiento; ocultos si no. */
	void RefreshAspirantHalos(const FMatchState& State);
	/** La ficha de Seat llegó a TileId en su recorrido (S-020): entrega la intención de avance de su asiento; al acabar, se recoloca. */
	void OnTokenTileEntered(int32 Seat, int32 TileId, bool bLast);
	/** Base de una ficha sobre el centro de la casilla (la cara superior de la losa). */
	FVector TokenBase(int32 TileId) const;
	/** Reconstruye el panel de crónica si el registro de eventos ha crecido. */
	void RefreshChronicle(const FMatchState& State);

	/**
	 * Recoge los WardenSpoke nuevos del registro y los pone en la cola de la línea del Warden (S-021), con el prefijo de a quién hablan.
	 * Un registro más corto que lo recorrido es una partida nueva: la cola empieza de cero.
	 */
	void RefreshWardenLine(const FMatchState& State);

	/** La frase que toca enseñar ahora en la línea del Warden (vacía si ninguna): avanza la cola con el reloj real. */
	FString WardenLineNow();

	/**
	 * Se abre una prueba del anillo del humano (S-026): registra la entrada del anillo (RingInput) en Slate. Condición del autor del
	 * 06/11/2026: solo está registrada mientras hay anillo.
	 */
	void HandleRingOpened();

	/** Se cierra (o se descarta) la prueba del anillo (S-026): retira la entrada y guarda el resultado para enseñarlo un momento. */
	void HandleRingClosed(const TOptional<FRingOutcome>& Outcome);

	/** Retira la entrada del anillo de Slate, si está registrada. La llaman el cierre, el descarte y EndPlay. */
	void UnregisterRingInput();

	/** Lo que pinta el panel del anillo ahora, con el reloj real (S-026). */
	FRingPanelModel RingPanelModel() const;

	/** Nombre del uso del anillo para el panel: "PRUEBA DEL WARDEN · normal" (S-026). Los textos son de la vista, como EventText. */
	static FString RingTitle(ESkillTrial Trial, bool bDuel, ESkillDifficulty Difficulty);

	/**
	 * Si la vista pinta Olvidado el Dominio DomainId (S-023): fuera del prólogo, lo que dice el estado; en el prólogo, solo si su frase de
	 * corrupción ya se ha enseñado (ForgottenDomainsShown). Lo calcula Refresh.
	 */
	bool IsDomainShownForgotten(int32 DomainId) const { return DomainsShownForgotten.IsValidIndex(DomainId) && DomainsShownForgotten[DomainId]; }
	/** Texto de depuración del turno; y pone la cámara como vista del jugador local si aún no lo es. */
	void TickHud();
	void ShowText(int32 Line, const FColor& Color, const FString& Text, float Scale = 1.f) const;

	FLinearColor SeatColor(int32 Seat) const;

	TWeakObjectPtr<UMatchHostSubsystem> Host;
	FDelegateHandle ChangedHandle;
	FTimerHandle HudTimer;
	/** Colocación del tablero en la mesa, calculada una vez al iniciar (S-019). */
	FBoardLayout Layout;
	TArray<FVector> TilePositions;
	/** Si la casilla lleva la losa del kit (su forma básica queda oculta, para el clic). */
	TArray<bool> TileHasKitSlab;
	/** El kit cargado (S-019) y lo que de él se colocó sobre la mesa; el kit lo comparte la vista previa del editor. */
	FBoardKit Kit;
	FBoardKitFixedPieces KitFixed;
	FBoardKitStructures KitStructures;
	bool bSeatedView = false;
	bool bOverlayHidden = false;
	bool bPanelsHidden = false;
	bool bDebugTextHidden = false;
	bool bInputBound = false;
	/** Si ya se pidió volver al menú (S-022): un segundo clic o Intro mientras se cierra la sesión no hace nada. */
	bool bReturningToMenu = false;
	/** Altura de la cara superior de cada casilla. */
	TArray<float> TileTops;
	FVector2D BoardCenter = FVector2D::ZeroVector;
	FVector2D BoardHalf = FVector2D::ZeroVector;
	FVector2D LastViewportSize = FVector2D::ZeroVector;
	int32 ChronicleLineCount = -1;

	TSharedPtr<SChroniclePanel> ChroniclePanel;
	TSharedPtr<SWidget> Overlay;

	/** La línea del Warden en pantalla (S-021): una capa aparte, que no se oculta con los paneles (H). */
	TSharedPtr<SWidget> WardenLineOverlay;

	/**
	 * El botón "Volver al menú" (S-022): una capa aparte, centrada en el hueco del tablero, que solo existe con la partida terminada y no se
	 * oculta con los paneles (H) ni con el texto de depuración (T).
	 */
	TSharedPtr<SWidget> ReturnToMenuOverlay;

	/** El panel del anillo (S-026): una capa aparte, centrada en el hueco del tablero, visible mientras hay prueba o su resultado. */
	TSharedPtr<SWidget> RingOverlay;

	/** La entrada del anillo (S-026): preprocesador de Slate, registrado solo mientras hay prueba del humano. */
	TSharedPtr<IInputProcessor> RingInput;
	FDelegateHandle RingOpenedHandle;
	FDelegateHandle RingClosedHandle;

	/** Lo que se enseña del último anillo cerrado (S-026): título, resultado y hasta cuándo (reloj real). */
	FString LastRingTitle;
	FString LastRingResult;
	FLinearColor LastRingColor = FLinearColor::White;
	FString LastRingNote;
	TArray<double> LastRingPrecisions;
	double LastRingShownUntil = 0.0;

	Soulbound::Presentation::FWardenLineQueue WardenLineQueue;
	/** Cuántos eventos del registro se han mirado en busca de frases. */
	int32 WardenLineScanned = 0;

	/** Qué Dominios se pintan Olvidados (S-023, ForgottenDomainsShown), uno por Dominio; lo calcula Refresh. */
	TArray<bool> DomainsShownForgotten;
	/** Con qué datos se calculó DomainsShownForgotten: si el prólogo duraba y cuántas frases de la apertura se habían enseñado. */
	bool bDomainsShownInPrologue = false;
	int32 DomainsShownOpeningLines = -1;

	/** Qué instrucciones se han dado en esta partida (S-021, A3); el perfil decide si se enseñan. */
	Soulbound::Presentation::FInstructionTracker InstructionTracker;

	/**
	 * Una instrucción enseñada (S-021, A3): no es un evento del motor, así que la vista la guarda aparte y la mezcla en la crónica. After es el
	 * índice del último evento del registro cuando se enseñó; la nota va justo después de él.
	 */
	struct FInstructionNote
	{
		int32 After = INDEX_NONE;
		double ClockSeconds = 0.0;
		FString Text;
	};
	TArray<FInstructionNote> InstructionNotes;

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

	/** Marca del tipo sobre cada casilla (nula si no lleva): cubo, cruz o esfera. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Inlays;

	/** Contorno negro de la marca (nulo si no lleva marca). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> InlayOutlines;

	/** Contorno del segundo brazo de la cruz de la Fuente (nulo en las demás). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> InlayOutlinesB;

	/** Nombre del Elegido (lo que va entre paréntesis en domains[].name) sobre cada Nicho (nulo en las demás casillas). */
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> NicheNames;

	/** Número del asiento sobre el cubo de cada Nicho reclamado (nulo en las demás casillas). */
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> NicheNumbers;

	/** Contador de reserva sobre cada Fuente, en su propio color para distinguirlo de la etiqueta (S-008). Nulo en las demás casillas. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> SourceReserves;

	/** Segmentos de los aros de los Nichos, seguidos: NicheRingFirst[casilla] es el primero de los de esa casilla. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> NicheRingSegments;

	TArray<int32> NicheRingFirst;

	/** Aro bajo cada casilla que se enseña si su Dominio es Olvidado. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ForgottenRings;

	/** Disco bajo cada casilla que se enseña si es alcanzable. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Halos;

	/** Segundo brazo de la cruz de la Fuente (nulo en las demás). */
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

	/** Vista sentada del asiento humano (S-019, R-VIS-9). */
	UPROPERTY()
	TObjectPtr<ACameraActor> SeatedCamera;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> PostProcess;

	UPROPERTY()
	TObjectPtr<USpotLightComponent> Lamp;

	UPROPERTY()
	TObjectPtr<USkyLightComponent> SkyLight;

	/** El orbe de la consulta del Oráculo (S-017): una esfera de formas básicas que recibe el clic. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> OracleOrb;

	/** Los halos del aspirante (S-018), uno por asiento: discos de formas básicas bajo el Altar. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AspirantHalos;
};
