# ADR-005. Ultra Game Template y Ultra Loading Screen como cajas negras con excepciones contadas

**Estado**: aceptado el 24/09/2026 (spike, paso 5).

## Contexto

Menús, opciones de vídeo y audio, accesibilidad, remapeo, subtítulos, localización, guardado y presets por plataforma no son el juego y consumirían semanas. El autor tiene comprados Ultra Game Template (UGT, Blueprints, no replicado) y Ultra Loading Screen (ULS, plugin C++), del mismo autor, que se usan juntos. El GDD 07 exige que la pausa nunca detenga la partida y que el Umbral 3D propio sea el menú principal. La Fase 3 (VR) no podrá usar menús 2D.

## Decisión

- UGT y ULS se usan **como cajas negras**: no se modifican sus Blueprints salvo las excepciones listadas abajo, y todo nuestro código habla con ellos a través de **un Blueprint adaptador** hijo de nuestras clases C++ (hoy, el hijo de `SpikeOnlinePlayerController`), que implementa su interfaz y llama a sus funciones. El C++ no sabe que existen. Dos puntos de entrada: "abrir opciones o pausa" y "cambiar de nivel".
- Su contenido no se versiona (`Content/UltraGameTemplate/`, plugin ULS en el motor); el README documenta la instalación y los ajustes manuales.
- **Excepciones a "no tocar UGT"**, todas de configuración, ninguna de lógica: (1) `UGT_GameInstance` reparentado a `UltraLoadingScreen_GameInstance` y los nodos `Show`/`Hide Ultra Loading Screen` en `TransitionOrLoadingScreenWidget` (integración oficial de su autor); (2) el Data Asset de configuración (`Disable Game Pause on Enter Pause Menu`, mapa de nueva partida, opciones que no aplican); (3) los valores de vídeo por defecto y el preset de Deck en `PlatformSpecificVideoConfigs` de `UGT_GameInstance`, porque sus ajustes guardados mandan sobre la configuración del proyecto y su valor por defecto es Lumen.
- Los mapas que UGT carga por nombre y su contenido se declaran en Packaging ("maps to include", "additional asset directories to cook"); sin eso el cook no los incluye.

## Alternativas descartadas

- Menús propios desde cero: semanas que el TFM no tiene; el Umbral 3D sigue siendo propio.
- Solo UGT sin ULS: su loading screen no tiene barra ni carga asíncrona; la barca a Cripta quiere movimiento durante la carga. ULS costó cinco minutos.
- Reescribir UGT para multijugador: su autor avisa de que no fue diseñado para ello; con "no pausar" bastó sin tocar nada.

## Versiones de las cajas negras

Registro manual, porque nada versionado las escanea (25/09/2026): Ultra Game Template 1.3; Ultra Loading Screen 1.2; EOS SDK 1.17.0 (carpeta `EOS-SDK-41373641-v1.17.0`); Redpoint EOS Online Framework Free Edition para 5.8, versión exacta en el `.uplugin` del PC de desarrollo (anotar en la próxima revisión). Próxima revisión de versiones y avisos de seguridad: antes de subir la primera build a Steam.

## Consecuencias

- Convive con nuestro `UGameInstanceSubsystem` de login (comprobado) y con el anfitrión del Warden (no evalúa en sus mapas, ADR-002).
- Un clon nuevo necesita repetir los ajustes manuales: están en el README y son cinco minutos.
- Story pendiente de la Fase 1: "Adaptar UGT y ULS a Soulbound" (quitar opciones ajenas, imágenes, remapeo, iconos de mando y teclado, textos, vídeo por defecto sin Lumen ni raytracing ni Nanite ni sombras virtuales, preset de Deck verificado en la Deck).
- Si UGT se cae o llega la VR, se sustituye el Blueprint adaptador y los dos puntos de entrada; nada más.

## Nota del 20/11/2026, al cierre del TFM

Versiones al cierre: EOS SDK 1.19.2.1. El Umbral 3D como menú principal no existe: el menú es el de la plantilla. Acoplamientos nuevos: un widget propio (el libro de "Cómo se juega", S-049) vive dentro del menú de la plantilla, y las capas de la vista se dibujan por debajo de su interfaz para que su menú de pausa quede encima. Aprendido con el uso: su pausa no detiene el mundo, como se pidió, y la plantilla recoloca el foco del teclado al abrir una pantalla. Los ajustes de audio se quitaron del menú. Tras la entrega se valorará si seguir con la plantilla o sustituirla por menús propios.
