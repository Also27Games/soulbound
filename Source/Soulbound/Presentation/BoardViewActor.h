#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "BoardKit.h"
#include "BoardLayout.h"
#include "ColumnTab.h"
#include "DecisionText.h"
#include "ElectCard.h"
#include "FlashCues.h"
#include "InstructionTracker.h"
#include "UnreachableNotice.h"
#include "Match/RingRules.h"
#include "Match/SeatGoalRules.h"
#include "Styling/SlateBrush.h"
#include "StatusText.h"
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
class SDecisionPanel;
class SElectCardPanel;
class SGrimoirePanel;
class SStatusPanel;
class SWidget;
struct FRingOutcome;
struct FRingPanelModel;
enum class ESkillTrial : uint8;
enum class ESkillDifficulty : uint8;
class UInstancedStaticMeshComponent;
class UTexture2D;
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
 * Desde S-027 el texto del turno ya no son mensajes de pantalla del motor: es el panel de estado (SStatusPanel) arriba a la izquierda,
 * con el de depuración debajo (tecla D), y el Grimorio (tecla G); los paneles escalan con la altura de la pantalla (R-HUD-3).
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

	/** Solo corre mientras hay un destello de una Fuente en curso (S-029): la ficha tiene el suyo. */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Si Component es el orbe de la consulta del Oráculo (S-017): la vista lo enseña mientras alguien consulta, siempre sobre su
	 * ficha (decisión del autor del 26/10/2026). Un clic en él consulta; el clic derecho, el rechazo de Tifón.
	 */
	bool IsOracleOrb(const UPrimitiveComponent* Component) const;

	/** Si el Grimorio está abierto (S-027, R-HUD-4): mientras lo está, los clics sobre el tablero no cuentan (el controlador los ignora). */
	bool IsGrimoireOpen() const { return bGrimoireOpen; }

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
	 * Cota del halo del Altar, en cm (S-033, R-DEC-12, nombre propuesto). El Altar va a cota 0 y las demás casillas a KitFloorHeight, así que su halo a
	 * la cota de siempre (0,07) quedaba bajo el relieve de la tierra (hasta 3,9). Valor inicial 3,17: KitFloorHeight (3,1) más 0,07, la cota del halo
	 * de las demás casillas. Se ajusta en PIE sin recompilar (Config/DefaultGame.ini).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Formas")
	float AltarHaloZ = 3.17f;

	/**
	 * Cuánto sobresale el halo del Altar de su pieza, por la mayor de sus medidas X o Y (S-033, R-DEC-12): el Altar es un hexágono y el halo, redondo,
	 * tiene que asomar también por las esquinas. Valor inicial 1,1 (un 10 %); menos de 1 se toma como 1 (Config/DefaultGame.ini).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Formas", meta = (ClampMin = "1"))
	float AltarHaloMargin = 1.1f;

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

	/**
	 * Alto de la letra del número de la ficha, en cm. Valor inicial 4,0 (S-037, R-VIS-23: antes 1,4; 2,0 se quedó corto en la prueba del autor del 13/11/2026). Valor a ciegas, a
	 * ajustar en PIE a 1024x768 sin tapar la ficha de al lado.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Etiquetas")
	float TokenLabelSize = 4.0f;

	/**
	 * Altura del número sobre lo alto de la ficha, en cm (S-037, R-VIS-23); con la ficha activa sube con ella. Valor inicial 0,6, a ciegas: a ajustar en PIE.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Etiquetas")
	float TokenLabelHeight = 0.6f;

	/**
	 * Diámetro del disco de la marca del Sello y de su destello, por el de la casilla (S-037, R-VIS-24): por debajo del halo de alcanzable (HaloScale, 1,35), para que
	 * una casilla sellada y alcanzable enseñe los dos bordes, el rojo por dentro y el blanco por fuera. Valor inicial 1,20 (el del aro de Dominio Olvidado,
	 * ForgottenRingScale): con 1,28 el halo apenas asomaba por fuera de la marca en la prueba del 14/11/2026. A ajustar en PIE.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Formas")
	float SealMarkScale = 1.20f;

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

	/**
	 * Diámetro del disco de rol bajo la ficha, por la huella de la carta (S-029, R-VIS-10, corrección del autor del 13/11/2026: el aro fino de la base
	 * no se distinguía desde la cámara). El diámetro final (RoleDiscDiameter) nunca pasa del borde interior del aro del turno (RingDiameter menos dos
	 * RingWidth y un margen) ni baja de 1,1 veces la huella (siempre más ancho que la peana). Se ajusta en PIE. Valor inicial 1,8.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Ficha", meta = (ClampMin = "1"))
	float RoleDiscScale = 1.8f;

	/**
	 * Cota de la cara inferior del disco de rol sobre la base de la ficha, en cm (S-029). La base es la cara superior de la losa o del Altar ya
	 * colocados (TokenBase), no una cuenta a ciegas: lo que quede bajo el suelo del kit no se ve (lección del halo del Altar, S-033). Cada asiento sube
	 * su disco RoleDiscSeatStep (0,01 cm) más que el anterior para que dos discos solapados en una casilla no parpadeen. Se ajusta en PIE. Valor inicial 0,1.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Ficha")
	float RoleDiscLift = 0.1f;

	/** Brillo del disco de rol: el color del rol por este factor, el mismo para Eco y Elegido. Se ajusta en PIE. Valor inicial 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Ficha", meta = (ClampMin = "0", ClampMax = "1"))
	float RoleDiscBrightness = 1.f;

	/** Color de la ficha de un Eco (S-029, R-VIS-10): el gris de datos. Un Elegido lleva el color de su Dominio (DomainColor). Valor inicial (0,45, 0,45, 0,45), el gris medio. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Ficha")
	FLinearColor EchoTokenColor = FLinearColor(0.45f, 0.45f, 0.45f);

	/** Periodo del pulso lento del aro del turno (S-029, R-VIS-11), en segundos; 0 o menos, sin pulso. Valor inicial 1,5. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Asiento activo")
	float ActiveRingPulseSeconds = 1.5f;

	/** Intensidad mínima del pulso del aro del turno, por la del color del asiento (S-029). Valor inicial 0,35. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Asiento activo", meta = (ClampMin = "0", ClampMax = "1"))
	float ActiveRingPulseMin = 0.35f;

	/**
	 * Periodo del latido de los halos de casilla alcanzable (S-039, R-VIS-26), en segundos; 0 o menos, sin latido (brillo fijo). Es la función del pulso
	 * del aro del turno (RingPulse). Valor inicial 1,2 (Config/DefaultGame.ini).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Halos")
	float ReachablePulseSeconds = 1.2f;

	/** Brillo mínimo del latido de los halos alcanzables, por el de ReachableColor (S-039, R-VIS-26). Valor inicial 0,12 (corrección del autor del 15/11/2026: con 0,35 el punto apagado se veía demasiado). */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Halos", meta = (ClampMin = "0", ClampMax = "1"))
	float ReachablePulseMin = 0.12f;

	/**
	 * Textura de las caras del dado del panel de decisión (S-039, R-DEC-15): las seis caras en una fila, del 1 al 6 de izquierda a derecha, cada una un
	 * sexto del ancho. Vacía, o con un asset que no existe, el panel dibuja la cara por código (un cuadrado con sus puntos) y lo dice una vez en el
	 * registro. Valor inicial /Game/Art/Dados/T_DadoCaras.T_DadoCaras (Config/DefaultGame.ini); el asset lo importa el autor más adelante.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	FSoftObjectPath DiceFacesTexture;

	/** Lado de la cara del dado del panel de decisión, en unidades de Slate (S-039, R-DEC-15). Valor inicial 44; el autor lo ajusta en PIE a 1024x768. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "8"))
	float DiceFaceSize = 44.f;

	/** Color del cuadrado del dado dibujado por código (S-039, R-DEC-15). Valor inicial (0,92, 0,92, 0,88), un marfil. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	FLinearColor DiceFaceColor = FLinearColor(0.92f, 0.92f, 0.88f);

	/** Color de los puntos del dado dibujado por código (S-039, R-DEC-15). Valor inicial (0,08, 0,08, 0,1), casi negro. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	FLinearColor DiceFacePipColor = FLinearColor(0.08f, 0.08f, 0.1f);

	/** Segundos que se enseña el aviso de casilla inalcanzable (S-039, R-DEC-16); se quita antes con otro clic de destino o al acabar el turno de tirada. Valor inicial 4. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0"))
	float UnreachableNoticeSeconds = 4.f;

	// ---- Destellos (S-029, R-VIS-12) ----

	/** Duración de cada destello de una ficha o de una Fuente al ganar o perder una runa, en segundos. Valor inicial 1 (0,6 era demasiado sutil, prueba del 13/11/2026). */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Destellos")
	float FlashSeconds = 1.f;

	/** Diámetro del aro del destello de una ficha, por el del aro del turno (RingDiameter), al empezar. Valor inicial 1,4. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Destellos", meta = (ClampMin = "0.1"))
	float FlashRingScale = 1.4f;

	/** Grosor del aro del destello de una ficha, en cm (el del aro del turno es RingWidth, 0,5). Valor inicial 1,5. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Destellos", meta = (ClampMin = "0.01"))
	float FlashRingWidth = 1.5f;

	/**
	 * Cuánto crece hacia fuera el aro del destello de la ficha y el halo de la Fuente mientras dura (FlashScale): al final miden 1 más este valor veces su
	 * tamaño inicial. 0, sin crecer. Valor inicial 0,8.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Destellos", meta = (ClampMin = "0"))
	float FlashGrowth = 0.8f;

	/** El claro de siempre: el color de un destello cuyo tipo de runa no se conoce (punto 8 de la story). Valor inicial blanco. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Destellos")
	FLinearColor FlashGainColor = FLinearColor(1.f, 1.f, 1.f);

	/**
	 * Color del destello de la ficha que pierde una runa (robo, devolución, Desvío, desafío fallado; punto 8 de la story). Es fijo y no el de la runa: el
	 * color se lee antes que el sentido y el de algún Dominio puede coincidir, y lo desempata el sentido (hacia fuera es perder). Valor inicial un morado
	 * vivo (0,55, 0, 1); decisión del autor del 13/11/2026 (antes rojo).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Destellos")
	FLinearColor FlashLossColor = FLinearColor(0.55f, 0.f, 1.f);

	// ---- La carta del Elegido (S-029, R-VIS-13) ----

	/** Segundos que se enseña la carta del Elegido al vincularse el jugador, o hasta un clic o abrir el Grimorio. Valor inicial 6. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Carta del Elegido")
	float ElectCardSeconds = 6.f;

	/** Ancho máximo de la carta del Elegido, en unidades de Slate; con menos hueco, el del hueco. Valor inicial 560. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Carta del Elegido")
	float ElectCardMaxWidth = 560.f;

	/**
	 * Fracción del alto de la pantalla que se deja libre abajo bajo la carta, para el panel de decisión y la frase del Warden (S-029: a 1024x768 la
	 * carta no los tapa). La carta se centra en lo que queda encima. Valor inicial 0,3.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Carta del Elegido", meta = (ClampMin = "0", ClampMax = "0.6"))
	float ElectCardBottomFraction = 0.3f;

	// ---- Oráculo (S-017) ----

	/**
	 * Diámetro del orbe de la consulta, en cm, a su tamaño entero. Desde S-041 (R-VIS-29) en Config: valor inicial 5 (antes 3, constante; P4 no lo vio
	 * la primera vez en la prueba del 15/11/2026). Es la vista de depuración, no el orbe del GDD 07.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Oráculo", meta = (ClampMin = "0.1"))
	float OracleOrbDiameter = 5.f;

	/** Altura del centro del orbe sobre la base de la ficha del consultante, en cm. Desde S-041 (R-VIS-29) en Config; valor inicial 7, por ajustar en PIE. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Oráculo")
	float OracleOrbHeight = 7.f;

	/** Periodo del latido del orbe mientras la consulta espera (S-041, R-VIS-29; OracleOrbScale), en segundos. Valor inicial 0,9. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Oráculo")
	float OracleOrbPulseSeconds = 0.9f;

	/** Escala mínima del latido del orbe, de 0 a 1 de su diámetro (S-041, R-VIS-29). Valor inicial 0,8. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Oráculo", meta = (ClampMin = "0", ClampMax = "1"))
	float OracleOrbPulseMin = 0.8f;

	// ---- Paneles ----

	/**
	 * Ancho de la columna de la derecha (leyenda y crónica) a 1080 píxeles de alto; con otra altura, en proporción (S-027, R-HUD-3). La cámara
	 * deja ese hueco al tablero, en píxeles de pantalla. Valor inicial 460.
	 */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	float PanelWidthPixels = 460.f;

	/**
	 * Tamaño de la letra de los paneles (estado, depuración, leyenda, crónica y Grimorio) a 1080 píxeles de alto; con otra altura, en
	 * proporción y nunca menos de PanelMinFontSize (S-027, R-HUD-3). Valor inicial 13.
	 */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	int32 PanelFontSize = 13;

	/** Tamaño mínimo de la letra de los paneles, en píxeles de pantalla (S-027, R-HUD-3). Valor inicial 9 (Config/DefaultGame.ini). */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 PanelMinFontSize = 9;

	/**
	 * Cuánto más grande va, en el panel de estado, la petición al jugador y el resultado de la partida (S-027, R-HUD-1: "con el tamaño mayor del
	 * panel"). Valor inicial 1,5 (Config/DefaultGame.ini).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	float StatusLargeFontScale = 1.5f;

	/** Líneas de crónica que se conservan; al pasarse se recortan las más viejas. Valor inicial 300. */
	UPROPERTY(EditAnywhere, Category = "Vista|Paneles")
	int32 ChronicleMaxLines = 300;

	/**
	 * Segundos que se enseña cada frase de la línea del Warden, grande en la parte baja de la pantalla, o hasta la siguiente línea (S-021,
	 * R-VOZ-1 sin audio). Valor inicial 6; en DefaultGame.ini. Desde S-041 (R-VOZ-6) es el mínimo: una frase larga dura más (WardenLineSecondsPerChar).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	float WardenLineSeconds = 6.f;

	/**
	 * Segundos por carácter de una frase del Warden (S-041, R-VOZ-6, nombre propuesto): una frase dura el mayor entre WardenLineSeconds y sus
	 * caracteres, con el prefijo, la palabra de corte y, en un corte, su línea pequeña, por este valor (WardenLineDuration). Valor inicial 0,065 s,
	 * unas quince letras por segundo; cero, todas duran WardenLineSeconds.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0"))
	float WardenLineSecondsPerChar = 0.065f;

	/** Tamaño de la letra de la línea pequeña de un corte, bajo la frase del Warden (S-041, R-VOZ-8). Valor inicial 18, menor que la de la frase (28). */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 CutNoteFontSize = 18;

	/** Color de la línea pequeña de un corte (S-041, R-VOZ-8): neutro, no el del Warden. Valor inicial un gris claro (0,8). */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	FLinearColor CutNoteColor = FLinearColor(0.8f, 0.8f, 0.8f);

	/**
	 * Opacidad de la capa oscura del tablero al empezar cada pulso cuando el Warden corta un turno (S-041, R-VOZ-8; CutDimOpacity): aparece de golpe
	 * y se desvanece en CutDimPulseSeconds, CutDimPulseCount veces seguidas. Sin posproceso: una capa de Slate negra, por debajo de los paneles. Valor
	 * inicial 0,85 (corrección del autor del 17/11/2026, probada en PIE; antes 0,5, demasiado sutil).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0", ClampMax = "1"))
	float CutDimMaxOpacity = 0.85f;

	/**
	 * Cuántos pulsos da la capa oscura del corte (S-041, R-VOZ-8; corrección del autor del 17/11/2026). Valor inicial 2, probado en PIE (antes 3: con
	 * tres de un tercio de segundo seguía parpadeando mientras la ficha se colocaba).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0"))
	int32 CutDimPulseCount = 2;

	/**
	 * Lo que dura cada pulso de la capa oscura del corte, en segundos reales (S-041, R-VOZ-8; corrección del autor del 17/11/2026). Valor inicial
	 * 0,08, probado en PIE (antes un tercio de segundo).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0"))
	float CutDimPulseSeconds = 0.08f;

	/** Tamaño de la letra de la línea del Warden (S-021). Valor inicial 28. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 WardenLineFontSize = 28;

	/**
	 * Cuántas líneas de la frase del Warden se reservan siempre bajo el panel de decisión (S-033, R-DEC-9), haya frase o no, para que el panel no
	 * se mueva cuando la frase aparece, cambia o se va. Valor inicial 2 (Config/DefaultGame.ini).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0"))
	int32 WardenLineReservedLines = 2;

	/**
	 * Segundos del reloj de partida que el panel de decisión enseña el resultado de una votación al cerrarse (S-033, R-DEC-8). Valor inicial 5
	 * (Config/DefaultGame.ini). Cero: no se enseña.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0"))
	float VoteResultSeconds = 5.f;

	/**
	 * Segundos del reloj de partida que el panel de decisión enseña el desenlace de un duelo, un dilema, un trilero o la prueba de otro asiento
	 * (S-033, R-DEC-10). Valor inicial 4 (Config/DefaultGame.ini). Cero: no se enseña.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (ClampMin = "0"))
	float OutcomeSeconds = 4.f;

	/**
	 * Segundos que el panel del anillo enseña el resultado ("superada", "fallada" o el rendimiento del duelo) tras cerrarse la prueba (S-026).
	 * Valor inicial 2 (Config/DefaultGame.ini). Cero: no se enseña.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	float RingResultSeconds = 2.f;

	/** Tamaño de la letra del título del panel del anillo (S-026); el resto se escala con él. Valor inicial 20. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 RingFontSize = 20;

	/**
	 * Textura de la runa del panel del anillo (S-031, R-VIS-18): un símbolo blanco sobre transparente que el panel tiñe. Vacía, o con un asset que no
	 * existe, el panel pinta un disco relleno del mismo color y lo dice una vez en el registro. Valor inicial /Game/Art/Anillo/T_RunaAnillo.T_RunaAnillo
	 * (Config/DefaultGame.ini); el asset lo importa el autor.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	FSoftObjectPath RingRuneTexture;

	/**
	 * Color y opacidad de la franja verde de la zona buena del panel del anillo (S-031, R-VIS-14). La anchura no se configura: sale de la zona buena.
	 * Valor inicial (0,2, 0,9, 0,3) con opacidad 0,55 (Config/DefaultGame.ini); el autor lo ajusta en PIE.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	FLinearColor RingZoneColor = FLinearColor(0.2f, 0.9f, 0.3f, 0.55f);

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
	 * del menú (ReturnToMenuMap desde S-027) (S-022; nombre aprobado). Es la alternativa al botón. Valor inicial Intro: no Escape, que es la
	 * pausa de UGT (R-FLU-5) y en PIE lo captura el editor (decisión del autor del 29/10/2026). Antes del final no hace nada y no se la queda.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey ReturnToMenuKey = EKeys::Enter;

	/**
	 * El mapa al que llevan "Volver al menú" e Intro al acabar la partida (S-027, R-MEN-1; nombre propuesto): el menú principal de UGT, el mismo
	 * que abre su menú de pausa. Valor inicial /Game/UltraGameTemplate/Maps/UGT_MainMenuMap (Config/DefaultGame.ini). Vacío o inexistente, se
	 * vuelve a GameDefaultMap (el lanzador, con los logos) y se dice en el registro.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath ReturnToMenuMap;

	/** Tamaño de la letra del botón "Volver al menú" (S-022). Valor inicial 28, como la línea del Warden. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 ReturnToMenuFontSize = 28;

	/**
	 * Tamaño de la letra del resultado de la partida, encima del botón "Volver al menú" (S-041, R-HUD-8; añadido del autor del 17/11/2026: antes iba
	 * arriba a la izquierda, en el panel de estado). Valor inicial 40, mayor que el botón.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 MatchResultFontSize = 40;

	/** Tamaño de la letra de "o pulsa Intro", debajo del botón "Volver al menú" (S-041, R-HUD-8). Valor inicial 18, como la línea pequeña del corte. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Paneles")
	int32 ReturnToMenuHintFontSize = 18;

	/** Tecla que oculta y muestra etiquetas para las capturas. Valor inicial L. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey OverlayToggleKey = EKeys::L;

	/**
	 * Tecla que oculta y muestra los paneles de Slate (leyenda, crónica y, desde S-027, estado y depuración; también cierra el Grimorio);
	 * ocultos, la cenital usa todo el ancho. Valor inicial H.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey PanelsToggleKey = EKeys::H;

	/**
	 * Tecla que oculta y muestra el panel de estado de arriba a la izquierda (desde S-027; antes, el texto de depuración). Valor inicial T.
	 * TODO(R-HUD-1): el nombre viene de cuando era texto de depuración; renombrarlo con Rider (StatusPanelToggleKey) toca DefaultGame.ini.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey DebugTextToggleKey = EKeys::T;

	/** Tecla que abre y cierra el Grimorio (S-027, R-HUD-4). Valor inicial G. */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey GrimoireKey = EKeys::G;

	/**
	 * Tecla que muestra y oculta el panel de depuración (S-027, R-HUD-2). Valor inicial D (decisión del autor del 9/11/2026: F1 es una tecla de
	 * depuración del propio Unreal).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey DebugPanelKey = EKeys::D;

	/**
	 * Tecla que cambia la pestaña de la columna de la derecha, de Leyenda a Crónica y al revés (S-035, R-HUD-4). Valor inicial P (de pestaña): la
	 * tabuladora la consume Slate para el foco, C ya cambia la vista y el resto de letras de la vista y del controlador están en uso.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Camara")
	FKey ColumnTabKey = EKeys::P;

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

	/**
	 * Material emisivo (cian) de las ranuras emisivas del kit. Sin parámetro de color (S-029: M_Emisivo es color por vértice por un multiplicador, sin
	 * ningún `VectorParameter`; solo M_ColorVertice tiene "Color"), así que no sirve para teñir en código: el disco de rol usa el material básico.
	 */
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

	/**
	 * El color del Sello (S-037, R-VIS-24, R-VIS-25): el de su marca bajo la casilla, el de sus destellos y la muestra de la leyenda. Valor inicial rojo (1, 0, 0),
	 * el rojo con que el panel de estado escribe "Sellos activos" (FColor::Red); no el morado, que es perder una runa (FlashLossColor).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Colores")
	FLinearColor SealMarkColor = FLinearColor(1.f, 0.f, 0.f);

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

	/**
	 * Material del suelo liso de la cuña de un Dominio Olvidado (S-040, R-VIS-27): piedra agrietada con el parámetro vectorial `Color`, como
	 * M_ColorVertice, para que el tinte del Dominio se le siga aplicando (WedgeFloorTint). Vacío, o con un asset que no existe, el suelo se pinta como
	 * antes (material del kit, apagado por ForgottenDim) y se dice una vez en el registro. Valor inicial /Game/Art/Tablero/M_SueloOlvidado.M_SueloOlvidado
	 * (Config/DefaultGame.ini); el asset lo crea el autor. La peana de la cuña no cambia.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Kit", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath ForgottenFloorMaterial;

	/**
	 * Cuánto se apaga el suelo liso de un Dominio Olvidado cuando lleva su material propio (S-040, R-VIS-27): la textura ya lo distingue, y con ForgottenDim
	 * (0,3) no se leería. Sin el material, el suelo usa ForgottenDim, como antes. Valor inicial 0,6 (Config/DefaultGame.ini).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Colores", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ForgottenFloorDim = 0.6f;

	/**
	 * Color de la línea de objetivo del panel de estado (S-040, R-HUD-7): uno solo para las tres metas, distinto del blanco de las alcanzables, del rojo del
	 * Sello y del morado de perder una runa. Valor inicial (1, 0,82, 0,35), un dorado claro (Config/DefaultGame.ini). Hasta la corrección del 16/11/2026 era
	 * también el de la luz de la meta (R-VIS-28, retirada) y se llamaba GoalLightColor.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vista|Colores")
	FLinearColor GoalLineColor = FLinearColor(1.f, 0.82f, 0.35f);

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

	/**
	 * Dimensiona y coloca el halo del Altar con el componente real de su pieza ya colocado (S-033, R-DEC-12): centro, tamaño (HaloDiameter con
	 * AltarHaloMargin) y cota AltarHaloZ. Sin pieza, queda el del tablero básico. Escribe en el registro los números que usa.
	 */
	void FitAltarHalo();
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
	/**
	 * Oculta o enseña el panel de estado (desde S-027; antes, el texto de depuración de pantalla).
	 * TODO(R-HUD-1): el nombre viene de cuando era texto de depuración; renombrarlo con Rider (ToggleStatusPanel).
	 */
	void ToggleDebugText();
	/** Muestra u oculta el panel de depuración (S-027, R-HUD-2). Al empezar, oculto. */
	void ToggleDebugPanel();
	/** Abre o cierra el Grimorio (S-027, R-HUD-4); abierto, se rellena en el acto. */
	void ToggleGrimoire();
	/** Cambia la pestaña de la columna de la derecha (S-035, R-HUD-4): la tecla ColumnTabKey. */
	void ToggleColumnTab();
	/** Pone la pestaña activa de la columna (S-035): el clic en su nombre de la cabecera. */
	void SetColumnTab(Soulbound::Presentation::EColumnTab Tab);
	/**
	 * Recalcula la letra y el ancho de los paneles para un viewport de ViewportSize (S-027, R-HUD-3): el tamaño en píxeles de pantalla
	 * (ScaledPanelFontSize, ScaledPanelWidth) entre la escala de DPI que Unreal ya aplica a la interfaz. Lo escribe en el registro al
	 * arrancar y cada vez que cambia.
	 */
	void UpdatePanelScale(const FVector2D& ViewportSize);
	/** Enseña en la línea del Warden las instrucciones de Cues que el perfil deje (S-021, A3), y las anota para la crónica. */
	void TeachInstructions(const FMatchState& State, const TArray<Soulbound::Presentation::FInstructionCue>& Cues);
	/** Pone como vista del jugador local la cámara que toca, y le engancha las teclas de la vista la primera vez. */
	void ApplyViewTarget();
	/** Si la partida ha terminado: entonces existe el botón "Volver al menú" y vale la tecla (S-022). */
	bool IsMatchOverShown() const;
	/** El resultado de la partida en grande, encima del botón (S-041, R-HUD-8; MatchResultLine); vacío sin partida o si no ha terminado. */
	Soulbound::Presentation::FPanelLine MatchResultLineNow() const;
	/**
	 * Vuelve al menú (S-022), por el botón o por la tecla: solo con la partida terminada y una sola vez; cierra la sesión en línea si la hay
	 * y viaja al menú.
	 */
	void ReturnToMenu();
	/** Cerrada (o no) la sesión en línea, viaja al menú (S-022): un cierre fallido se anota y se viaja igual. */
	UFUNCTION()
	void HandleSessionLeftForMenu(bool bWasSuccessful);
	/**
	 * Abre el mapa del menú (S-022): desde S-027, ReturnToMenuMap; vacío o inexistente, el GameDefaultMap de DefaultEngine.ini, leído de
	 * UGameMapsSettings, y lo dice en el registro (MenuMapPackage).
	 */
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

	/** El tamaño del orbe del Oráculo en el instante Seconds del reloj de la vista: su diámetro por el latido (S-041, R-VIS-29; OracleOrbScale). */
	void ScaleOracleOrb(double Seconds);
	/** Los halos del aspirante (S-018): bajo el Altar, uno del color de cada asiento aspirante, de dentro afuera por asiento; ocultos si no. */
	void RefreshAspirantHalos(const FMatchState& State);

	/** Si la casilla tiene un destello de Sello en curso (S-037): su marca se ve aunque el estado ya no la marque. */
	bool IsSealMarkFlashing(int32 TileId) const;
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

	/**
	 * Destellos (S-029, R-VIS-12): lee los eventos nuevos del registro, como la crónica y las Ayudas, y pone en cola el destello de cada ficha (la
	 * ficha los encadena sola) y de cada Fuente (los encadena la vista). Lo ya ocurrido al arrancar la vista no destella.
	 */
	void RefreshFlashes(const FMatchState& State);

	/** La carta del Elegido (S-029, R-VIS-13): lee el registro y, con la carta a la vista, le pasa la fila del humano. */
	void RefreshElectCard(const FMatchState& State);

	/** Cierra la carta del Elegido (clic). */
	void DismissElectCard();

	/** Si la carta del Elegido se ve ahora: a tiempo, sin telón, sin paneles ocultos y sin el Grimorio abierto. */
	bool IsElectCardShown() const;

	/**
	 * La frase que toca enseñar ahora en la línea del Warden, con su línea pequeña si es un corte (vacía si ninguna): avanza la cola con el reloj
	 * real. Desde S-041 devuelve la entrada entera (FWardenLineItem).
	 */
	Soulbound::Presentation::FWardenLineItem WardenLineNow();

	/** El alto de la franja de la frase bajo el panel de decisión (S-041, R-DEC-17): lo reservado, o lo que ocupa la frase si no cabe. */
	float WardenLineBandHeightNow();

	/** La opacidad de la capa oscura del corte ahora (S-041, R-VOZ-8; CutDimOpacity), con el telón bajado, cero. */
	float CutDimOpacityNow() const;

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

	/**
	 * Si la vista pinta Olvidado el Dominio DomainId (S-023): fuera del prólogo, lo que dice el estado; en el prólogo, solo si su frase de
	 * corrupción ya se ha enseñado (ForgottenDomainsShown). Lo calcula Refresh.
	 */
	bool IsDomainShownForgotten(int32 DomainId) const { return DomainsShownForgotten.IsValidIndex(DomainId) && DomainsShownForgotten[DomainId]; }
	/**
	 * Rellena los paneles de estado y de depuración y, abierto, el Grimorio (S-027); y pone la cámara como vista del jugador local si aún no lo
	 * es. Se llama a intervalo corto porque el reloj cambia sin que cambie el estado.
	 */
	void TickHud();

	/**
	 * El ancho de la capa de la frase y del panel de decisión, en unidades de Slate (S-033, R-DEC-13): el de su geometría real en el último tick; si aún
	 * no la hay, la cuenta con el tamaño de la ventana y la escala de DPI.
	 */
	float LayerSlateWidth() const;

	/** El hueco libre del tablero en horizontal para un tope, a partir de LayerSlateWidth (BoardFreeWidth). */
	float BoardFreeWidthNow(float Cap) const;
	/** Lo que los paneles de estado y de depuración necesitan y no está en el estado (S-027, FStatusContext). */
	Soulbound::Presentation::FStatusContext MakeStatusContext() const;

	/**
	 * Lo que enseña el panel ahora (S-028, DecisionView; S-033, PanelView): la decisión del humano, el resultado de una votación reciente o la
	 * espera a otro asiento, con la espera del Warden del anfitrión; nada sin partida.
	 */
	TOptional<Soulbound::Presentation::FDecisionView> PanelViewNow() const;

	/** El alto del hueco que se reserva siempre a la frase del Warden (S-033, R-DEC-9): WardenLineReservedLines líneas de su letra más sus márgenes. */
	float WardenLineReservedHeight() const;

	/** Si la caja de la línea de objetivo se ve ahora (S-040, R-HUD-7): IsGoalPanelShown con la frase del Warden de este frame, y nunca con el telón. */
	bool IsGoalPanelVisibleNow();

	/** Si la capa del panel de decisión se enseña (S-028, R-DEC-3): la regla de DecisionText, la tecla de los paneles (H) y el telón. */
	bool IsDecisionPanelVisible() const;

	/** Qué parte del plazo de la decisión que se enseña queda ahora, para la barra del panel (S-028), con el reloj de partida del momento. */
	float DecisionTimeFractionNow() const;

	/**
	 * Un botón del panel de decisión (S-028, R-DEC-2): lo entrega el controlador del jugador local, con la misma entrada del anfitrión que la
	 * tecla. La vista no contesta a nadie por su cuenta.
	 */
	void AnswerDecision(const Soulbound::Presentation::FDecisionOption& Option);

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
	/** La pestaña de la columna de la derecha (S-035, R-HUD-4): empieza en Leyenda en cada partida (la vista se crea con ella). */
	Soulbound::Presentation::FColumnTabState ColumnTab;
	/** Si el panel de estado está oculto (tecla T). TODO(R-HUD-1): renombrarlo con Rider (bStatusPanelHidden). */
	bool bDebugTextHidden = false;
	/** Si el panel de depuración se ve (tecla D, S-027). Al empezar, no. */
	bool bDebugPanelShown = false;
	/** Si el Grimorio está abierto (tecla G, S-027). H lo cierra. */
	bool bGrimoireOpen = false;
	/** Cuándo se enseña la carta del Elegido (S-029), la capa y el panel. */
	Soulbound::Presentation::FElectCardTracker ElectCard;
	TSharedPtr<SElectCardPanel> ElectCardPanel;
	TSharedPtr<SWidget> ElectCardOverlay;
	/** Primer evento del registro que los destellos aún no han leído (S-029). */
	int32 FlashScanned = 0;
	/** Un destello de una Fuente, en segundos del mundo (S-029); la ficha lleva los suyos. */
	struct FSourceFlash
	{
		// TODO(R-VIS-25): con bTile, RuneType guarda el identificador de la casilla y no un tipo de runa; un nombre neutro (FlashTargetId) o un campo propio lo aclararía, pero habría que tocar los usos de las Fuentes.
		int32 RuneType = INDEX_NONE;
		double Start = 0.0;
		double End = 0.0;
		/** Hacia dentro (se cierra: la Fuente que recibe de vuelta una runa) o hacia fuera (se abre), y de qué color (punto 8 de la story). */
		bool bInward = false;
		FLinearColor Color = FLinearColor::White;
		/** S-037 (R-VIS-25): si es el destello de una casilla sellada o liberada, y no de una Fuente; RuneType es entonces el identificador de la casilla. */
		bool bTile = false;
	};
	TArray<FSourceFlash> SourceFlashes;
	/** Hasta cuándo está ocupada cada casilla con destello de Sello, para encadenar los suyos (S-037). */
	TMap<int32, double> SealFlashBusy;
	/** La escala de cada disco de marca antes de crecer con un destello, para devolvérsela al acabar (S-037). */
	TMap<int32, FVector> SealMarkBaseScales;
	/** Hasta cuándo está ocupada la Fuente de cada tipo de runa, para encadenar sus destellos. */
	TMap<int32, double> SourceFlashBusy;
	/** La escala de cada halo de casilla antes de crecer con un destello de su Fuente, para devolvérsela al acabar (S-029). */
	TMap<int32, FVector> HaloBaseScales;
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

	/** Los paneles de estado y de depuración (S-027): una capa arriba a la izquierda, que se oculta con los paneles (H) y con el telón. */
	TSharedPtr<SStatusPanel> StatusPanel;
	TSharedPtr<SStatusPanel> DebugPanel;
	/**
	 * La caja de la línea de objetivo (S-040, R-HUD-7; correcciones del autor del 16/11/2026 tras PIE): en la capa de la frase del Warden, abajo a la
	 * izquierda dentro de la franja que se le reserva, con GoalPanelLines. Ninguna tecla la oculta; se ve según IsGoalPanelShown (IsGoalPanelVisibleNow),
	 * con el prólogo y la meta que TickHud guarda abajo.
	 */
	TSharedPtr<SStatusPanel> GoalPanel;
	bool bGoalPanelPrologue = false;
	ESeatGoal GoalPanelGoal = ESeatGoal::None;
	TSharedPtr<SWidget> StatusOverlay;

	/** El Grimorio (S-027): una capa centrada, por encima de las demás. */
	TSharedPtr<SGrimoirePanel> GrimoirePanel;
	TSharedPtr<SWidget> GrimoireOverlay;
	/** Los textos de grimorio.json, leídos al construir los paneles. */
	Soulbound::Presentation::FGrimoireTexts GrimoireTexts;

	/**
	 * La escala de los paneles (S-027, R-HUD-3), calculada por UpdatePanelScale: letra y ancho de la columna de la derecha en unidades de Slate
	 * (lo que se le da a los widgets) y el ancho en píxeles de pantalla (lo que la cámara deja al tablero).
	 */
	int32 PanelSlateFontSize = 13;
	float PanelSlateWidth = 460.f;
	float PanelScreenWidth = 460.f;
	float PanelDpiScale = 1.f;
	/** Si ya se escribió la escala en el registro (al arrancar se escribe siempre; después, solo si cambia). */
	bool bPanelScaleLogged = false;
	/** Los números de la última línea "Paneles: capa de la frase" (S-033, R-DEC-13); para escribir solo si cambian. */
	FString LoggedLayoutKey;
	/** La columna de paneles y el contenido de la capa de la frase: su geometría real va al registro (S-033, R-DEC-13). */
	TSharedPtr<SWidget> PanelColumn;
	TSharedPtr<SWidget> WardenLineContent;
	/** La pieza del Altar ya colocada y la última visibilidad de su halo que se escribió en el registro (-1: ninguna; S-033, R-DEC-12). */
	TWeakObjectPtr<UStaticMeshComponent> AltarPieceComponent;
	int32 LoggedAltarHalo = -1;

	/**
	 * La línea del Warden en pantalla (S-021): una capa aparte, que no se oculta con los paneles (H). Desde S-028 lleva encima, en la misma
	 * columna, el panel de decisión, que sí se oculta con H: así va siempre por encima de la frase, sea cual sea su alto.
	 */
	TSharedPtr<SWidget> WardenLineOverlay;

	/** El fondo de la frase del Warden, con la frase y su línea pequeña: su alto deseado es lo que necesita la franja (S-041, R-DEC-17). */
	TSharedPtr<SWidget> WardenLineBox;

	/** La capa oscura del corte (S-041, R-VOZ-8): sobre el tablero, por debajo de todos los paneles y sin recibir clics. */
	TSharedPtr<SWidget> CutDimOverlay;

	/** El panel de decisión (S-028, R-DEC-2), sobre la línea del Warden. */
	TSharedPtr<SDecisionPanel> DecisionPanel;
	/** El reloj de su barra: qué decisión mide y su total (AdvanceDecisionClock, en cada TickHud). */
	Soulbound::Presentation::FDecisionClock DecisionClock;
	/** Fin del plazo de la decisión que se enseña, para pintar la barra entre dos TickHud. */
	double DecisionDeadlineSeconds = 0.0;
	/** Si la regla de DecisionText enseña el panel (IsDecisionPanelShown), calculado en TickHud. */
	bool bDecisionShown = false;

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
	/** Criterio, umbral y pulsos de la prueba en curso o de la última (S-031, R-VIS-16): los de datos, que la barra enseña sin reinterpretarlos. */
	FRingTrialParams LastRingParams;
	/** Color con que se tiñe la runa de la prueba en curso (R-VIS-18): el del Dominio de la Fuente, o el azul de siempre. */
	FLinearColor LastRingRuneColor = FLinearColor(0.55f, 0.75f, 1.f);

	/**
	 * La textura de la runa del anillo, cargada al montar el panel (S-031, R-VIS-18). UPROPERTY para que el recolector no la libere mientras el pincel
	 * que la apunta (RuneBrush, que no es UObject) esté en uso por el panel.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RuneTexture;
	/** El pincel de la runa: apunta a RuneTexture; vacío si no hay textura (entonces el panel pinta un disco). */
	FSlateBrush RuneBrush;

	/**
	 * La textura de las caras del dado, cargada al montar el panel de decisión (S-039, R-DEC-15). UPROPERTY por lo mismo que RuneTexture: el pincel que
	 * la apunta (DiceBrush) no es UObject.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DiceTexture;
	/** El pincel de las caras del dado: apunta a DiceTexture; vacío si no hay textura (entonces el panel dibuja la cara por código). */
	FSlateBrush DiceBrush;

	/** La vida del aviso de casilla inalcanzable (S-039, R-DEC-16): cuándo se ve y cuándo se quita; no es del motor ni una frase del Warden. */
	Soulbound::Presentation::FUnreachableNotice UnreachableNotice;
	/** La capa del aviso de casilla inalcanzable (S-039, R-DEC-16): llena el viewport, no recibe clics y pone el aviso donde lo coloca PlaceNotice. */
	TSharedPtr<SWidget> NoticeOverlay;
	/** El margen que coloca el aviso en la capa (PlaceNotice con el viewport, la columna de paneles, el panel de estado, la caja de la línea de objetivo y el panel de decisión de este frame, S-040). */
	FMargin NoticeMarginNow(int32 FontSize) const;
	/** Margen del fondo del aviso por cada lado, y separación entre el punto del clic y el aviso, en unidades de Slate. */
	static constexpr float NoticePadding = 8.f;
	static constexpr float NoticeClickGap = 14.f;
	/** El texto del aviso que se ve ahora (vacío si no hay), actualizado en TickHud, y el ancho al que se parte. */
	FString NoticeText;
	float NoticeWrapAt = 400.f;
	/** El último rechazo del anfitrión que ya se ha mirado; INDEX_NONE hasta el primer TickHud, que se pone al día sin enseñar un aviso viejo. */
	int32 SeenRejectionSerial = INDEX_NONE;
	/** Si hay halos de casilla alcanzable a la vista, es decir, si el actor tiene que seguir ticando para que latan (S-039, R-VIS-26). */
	bool bReachableHalosShown = false;
	/** Si el orbe del Oráculo está a la vista, es decir, si el actor tiene que seguir ticando para que lata (S-041, R-VIS-29). */
	bool bOracleOrbShown = false;

	/** El instante real del último corte de turno del Warden, para la capa oscura (S-041, R-VOZ-8); negativo si no ha habido ninguno. */
	double CutDimStartReal = -1.0;

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

	/**
	 * La marca del Sello (S-037, R-VIS-24): un disco plano por casilla, bajo la losa y más ancho, del que solo se ve el borde, con BaseMaterial y SealMarkColor.
	 * Visible mientras la casilla está sellada (SealMarkedTiles) y durante un destello de Sello (R-VIS-25), que lo escala.
	 */
	TArray<TObjectPtr<UStaticMeshComponent>> SealMarks;

	/** El material del suelo de los Dominios Olvidados (ForgottenFloorMaterial) cargado al empezar (S-040, R-VIS-27), o nulo: el actor lo mantiene vivo. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ForgottenFloor;

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
