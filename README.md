# Soulbound Core

Trabajo de Fin de Máster del Máster en Desarrollo con IA (BIG School, 3ª edición). Autor: Lluis Alonso.

Este README sigue el guion de entrega del TFM: descripción general, stack tecnológico, instalación y ejecución, estructura del proyecto, funcionalidades principales y acceso de prueba. Los enlaces de entrega (despliegue, slides, vídeo) están al final.

## 1. Descripción general

**Soulbound** es un party game de terror social: cuatro jugadores atrapados en una cripta viviente compiten en un juego de mesa mortal dirigido por un anfitrión sádico, el Warden, que cambia las reglas a su antojo. Para escapar hay que reunir seis runas y sobrevivir a sus caprichos antes de que el tiempo se agote.

**Soulbound Core** es la primera fase del juego y el objeto de este TFM: el tablero multijugador en pantalla plana (Windows, Steam Deck), el motor de reglas y la IA directora del Warden. Construido en Unreal Engine 5.8 con C++ aplicando la metodología del máster: diseño por sistemas con reglas y criterios de aceptación, motor de reglas sin mundo probado por consola, y stories implementadas con agentes y revisadas por el autor. El proceso está contado en `docs/bitacora.md`.

## 2. Stack tecnológico

- **Motor**: Unreal Engine 5.8.3, C++ (módulos `SoulboundCore` y `Soulbound`), Blueprints solo como adaptadores de presentación.
- **Online**: Epic Online Services a través de Redpoint EOS Online Framework (Free Edition) y un plugin propio de sesiones por código (`A27Online`); login por Steam en la build empaquetada.
- **Menús, opciones y carga**: Ultra Game Template y Ultra Loading Screen (Fab), usados como cajas negras tras un adaptador propio.
- **Herramientas**: Rider, Visual Studio 2022 (toolchain), Git con LFS, GitHub CLI, PowerShell (`Scripts/`), Python (utilidades de arte).
- **IA en el desarrollo**: Claude Code con la skill del proyecto (`.claude/skills/soulbound/`), guía para agentes en `CLAUDE.md`, y el plugin Unreal MCP del editor para consulta y tests.
- **Arte**: placeholders generados con IA (declarados como tales), a sustituir por arte encargado si el juego funciona.

## 3. Instalación y ejecución

### Jugar la build

Requisitos: Windows 10 u 11 (o Steam Deck con Proton), cliente de Steam abierto con sesión iniciada y la clave de la app (ver "Acceso de prueba"). Descargar la build del enlace de despliegue, descomprimir y ejecutar `Soulbound\Binaries\Win64\Soulbound.exe`. Para jugar en línea: un jugador crea partida y comparte el código de cuatro letras; los demás lo introducen.

### Desarrollo en Windows

Lo que no está en el repositorio y hay que instalar a mano, en este orden.

1. **Herramientas**: Unreal Engine **5.8.3** (Epic Games Launcher), Visual Studio 2022 con "Desarrollo de juegos con C++" (o solo Build Tools), Rider (opcional; abre el `.uproject`, no los `.slnx`), Git for Windows (incluye LFS) y GitHub CLI. En PowerShell, una vez: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`.
2. **SDK de .NET 10 global cuyo runtime coincida con el del motor** (hoy, 10.0.7: SDK 10.0.2xx). El plugin de Redpoint recompila UnrealBuildTool con el SDK global; uno más nuevo que el runtime del motor falla. Comprobar con `dotnet --list-sdks` y `dotnet --list-runtimes`.
3. **Clonar** y activar el filtro de secretos y los hooks (en Git Bash): `sh Scripts/eos-secrets/install.sh`. Rellenar `Config/Local/EOS.ini` con los identificadores del producto en el portal de Epic y la clave de licencia de Redpoint (nunca se versionan), y regenerar el archivo de trabajo: `Remove-Item Config\DefaultEngine.ini; git checkout -- Config/DefaultEngine.ini`.
4. **Redpoint EOS Online Framework, Free Edition** para 5.8, en `Plugins/OnlineSubsystemRedpointEOS/`, y el **SDK de EOS** en `Plugins/EOS-SDK-<version>/` (la versión la dice Redpoint al compilar). Los dos están ignorados por Git.
5. **Ultra Loading Screen** (Fab): instalar en el motor; el plugin ya está activado en el proyecto. Va antes que UGT porque UGT se reparenta a su GameInstance.
6. **Ultra Game Template** (Fab, Launcher > Add to Project): queda en `Content/UltraGameTemplate/`, ignorado. Tras instalarlo, tres ajustes manuales dentro de su contenido:
   - `UGT_GameInstance`: Class Settings > Parent Class = `UltraLoadingScreen_GameInstance` (integración con Ultra Loading Screen; tutorial de cinco minutos del autor de ambos).
   - `TransitionOrLoadingScreenWidget`: añadir los nodos `Show Ultra Loading Screen` y `Hide Ultra Loading Screen` donde indica el mismo tutorial.
   - `UGT_GameInstance` (S-024, el telón): en el evento `FadeLoadingScreen` de `UltraLoadingScreen_GameInstance` (la clase padre), que avisa de que la pantalla de carga empieza a irse, *Get World Subsystem* (`MatchHostSubsystem`) › *Is Valid* › `RaiseCurtain`. En los menús el subsistema existe pero está inactivo y la llamada no hace nada; sin esta conexión el juego arranca igual, porque el telón se sube solo a los `CurtainTimeoutSeconds` (6 s, `DefaultGame.ini`). Comprobar en el log: "Telón subido (presentación)" después de "Login OK".
   - Data Asset de configuración (`DefaultGameDataAsset`): `Disable Game Pause on Enter Pause Menu` activado (la pausa nunca detiene la partida); "nueva partida" apuntando al mapa del juego.

   Una regla antes de seguir: **UGT aplica sus ajustes guardados al arrancar y al abrir cada menú, por encima de los ini del proyecto.** Lo que puede sorprender no son sus menús sino lo que aplican; por eso se han quitado todas las filas sin función en Soulbound, y cualquier opción que se añada hay que comprobar qué comando ejecuta. Los pasos siguientes son los que dejan UGT como está en el test de usabilidad (noviembre de 2026); ninguno toca código. Los assets propios que usan (logos y fondos) sí están en el repositorio, en `Content/Art/Logos/` y `Content/Art/Carga/`, y son arte generado por IA declarado como placeholder.
   - **Arranque limpio** (`DefaultGameDataAsset`): sin diapositiva del publisher, sin diapositiva de agradecimientos, sin mensaje de bienvenida al abrir el juego, sin enlaces a redes y sin la pantalla "Configuración inicial" del primer arranque (lo útil de ella ya está en Opciones › Vídeo). El texto de la esquina del menú: "Soulbound v0.1 · en desarrollo". El logo del estudio de la intro es `Content/Art/Logos/`.
   - **Idioma**: castellano por defecto en `UGT_GameSettingsMenu`. Comprobar con la carpeta `Saved` borrada: el menú tiene que arrancar en castellano a la primera.
   - **Menú principal**: Jugar, Configuración y Salir; el menú de créditos se quita hasta la entrega (volverá con los créditos reales). Fondo: la imagen de `Content/Art/Carga/` sin el Warden, en vez del escenario 3D de ejemplo; logo del juego de `Content/Art/Logos/` arriba a la izquierda. Para volver al menú al acabar la partida no hace falta nada en UGT: el juego pone un botón "Volver al menú" (Slate, en código) y la tecla Intro (`ReturnToMenuKey`).
   - **"Cómo se juega"**: es la pantalla de nueva partida de UGT, la que sale cada vez que se pulsa Jugar (la de arranque queda desactivada). Título "Cómo se juega"; el bloque de texto, con ajuste de línea automático, se rellena en su Construct con el nodo *How To Play Text* (`UHowToPlayLibrary::HowToPlayText`, `BlueprintPure`), que devuelve `Content/Data/Textos/como-se-juega.md` con los títulos sin `## `; si el archivo falla, texto vacío y el motivo en el log. El botón "¡Saber más!" se oculta; el de Jugar se queda. Fondo negro.
   - **Opciones › Juego**: se quitan las filas de UGT (idiomas, subtítulos, tamaño de texto, dificultad, daño, temblores, balanceo, vibración, modo realista) y se añade **Ayudas**, un selector con Automático, Siempre y Nunca en este orden (los `DisplayName` de `EHintsMode`). En el Construct del menú, *Get Game Instance Subsystem* (`PlayerProfileSubsystem`) › `GetHintsIndex` selecciona el índice; al cambiar la selección, `SetHintsIndex` guarda el perfil; "Restablecer" pone 0. Comprobar que se recuerda al cerrar y abrir el juego, y en partida con `SbHints` sin argumentos.
   - **Opciones › Audio**: el botón se oculta, no se borra (el juego no emite sonido; volverá con la story de audio).
   - **Opciones › Vídeo**, en `UGT_VideoSettingsMenu`: se quitan el bloque de escaladores con IA (DLSS y FSR no tienen plugin; TSR y escalado simple sobran), campo de visión, HDR, Stat Unit, desenfoque de movimiento, y de la configuración avanzada las filas Lumen, Nanite, Distance Field Shadows, Lumen Hardware Raytracing y densidad de vegetación; también el botón de Steam Deck. Se quedan resolución, modo, aplicar, límite de FPS, gamma, anti-aliasing, V-Sync, preajuste global y las filas avanzadas restantes. En la función `ConfigGlobalPresets`, las seis ramas del preajuste (Ultra Low a Cinematic) escriben todas las variables de `UGT_VideoSave`: poner **Lumen, Nanite, Hardware Ray Tracing y Distance Field Shadows a Desactivado en las seis**; si no, el preajuste "Alto" vuelve a encender Lumen aunque el ini diga lo contrario. Comprobar en PIE que al abrir Opciones › Vídeo en la pausa no sale el aviso rojo de Lumen, y en el log del arranque del mapa `r.DynamicGlobalIlluminationMethod = "0"` y `r.Nanite = "0"`. El resto de valores por defecto de UGT (gamma, escalado, pantalla completa) se dejan hasta el test; la resolución se adapta sola a cada pantalla.
   - **Menú de pausa**: Reanudar, Configuración y Salir al menú principal, sin logo. El HUD de depuración se dibuja por encima de la pausa; es cosa del juego, no de UGT.
   - **Pantalla de carga** (Ultra Loading Screen): la imagen de `Content/Art/Carga/` con el Warden (textura con *Compression* UserInterface2D y *Never Stream*), texto "Cargando", y el círculo, la barra y el porcentaje recolocados en la franja inferior de la imagen, que está vacía para eso. Al irse la imagen, el juego espera `CurtainGraceSeconds` (1,5 s) antes de que el Warden hable.
7. **Steam**: el App Id del producto está en `Config/Windows/WindowsEngine.ini`. La build empaquetada hace login por Steam (cliente abierto y sesión iniciada); en el editor se usa la Developer Authentication Tool de Epic, que Redpoint lanza solo.

Compilar, probar y empaquetar sin abrir el editor:

```powershell
.\Scripts\build.ps1 -Que editor
.\Scripts\test.ps1
.\Scripts\build.ps1 -Que package
```

## 4. Estructura del proyecto

- `Source/SoulboundCore/`: motor de reglas sin mundo (solo `Core` y `CoreUObject`). Estado en `USTRUCT`, reglas como funciones libres, tests de automatización por criterio de aceptación.
- `Source/Soulbound/`: el juego (subsistemas, GameMode, controladores, presentación).
- `Plugins/A27Online/`: plugin propio de sesiones por código sobre Epic Online Services.
- `Scripts/`: `build.ps1` (compilar y empaquetar sin editor), `test.ps1` (tests por consola), `eos-secrets/` (secretos fuera del repositorio), `git-hooks/`, `art/` (utilidades de arte), `board-editor/` (editor de tableros: una página HTML que genera tableros aleatorios a partir de parámetros, permite editarlos a mano, valida las mismas reglas que el motor y descarga el JSON de `Content/Data/Boards/`).
- `docs/`: `stories/` (una story por archivo), `bitacora.md` (historia legible del proyecto, una entrada por story), `aplicacion-del-master.md` (qué práctica del máster respalda cada decisión, y qué no aplica y por qué), documentación de diseño y arquitectura pública (en construcción).
- `CLAUDE.md`: guía para los agentes. La skill del proyecto (glosario, pilares, plantillas, checklist de revisión) está en `.claude/skills/soulbound/`.

Ramas: `main` contiene el proyecto y la infraestructura; `spike` (septiembre de 2026) contiene la prueba técnica, con código desechable, hasta que se decida qué sobrevive.

## 5. Funcionalidades principales

Alcance de la Fase 1 (en construcción; se marca lo que está hecho):

- [x] Motor de reglas sin mundo con tests por consola: casillas alcanzables con Altar y Sellos (GDD 01).
- [x] Simulación de partidas enteras con reloj virtual, determinista por semilla.
- [x] Anfitrión del Warden en juego que llama al mismo cerebro que la simulación.
- [x] Partida en línea por código de cuatro letras (crear, unirse, salir, reconexión), login por Steam.
- [x] Menús, opciones, pausa sin detener la partida y pantalla de carga entre niveles.
- [ ] Tablero completo: seis Dominios, Altar, Nichos, Fuentes y Nodos; turnos y dados.
- [ ] Carrera por el vínculo, Elegidos y Ecos; runas y poderes; duelos y trilero; victoria.
- [ ] El Warden completo: eventos, decretos, dilemas, votaciones, Oráculo, insolencia y Furia, voz sintética con subtítulos.
- [ ] Interfaz en plano: cámara sentada, selección en el mundo, HUD, objetos diegéticos, Umbral, barcas y postpartida.
- [ ] Bots básicos y decisores; grabación y reproducción de partidas.
- [ ] 60 FPS en Steam Deck.

## 6. Acceso de prueba

El juego no tiene usuario y contraseña propios: la identidad la da Steam. Para probar la build hace falta una **clave de Steam** de la app, que se facilita con la entrega (formulario) o a petición al autor. En el editor no hace falta clave: se usa la Developer Authentication Tool de Epic.

## Entregables del TFM

- Repositorio: este.
- Despliegue o publicación: pendiente (build descargable y clave de Steam).
- Slides: pendiente (enlace o archivo en `docs/`).
- Vídeo con explicación y captura de pantalla: pendiente.

## Propiedad intelectual

Soulbound es un diseño original inspirado en el género de los juegos de mesa con anfitrión en vídeo de los años 90. No contiene material de terceros. Los assets y plugins de pago o con licencia propia (Redpoint, EOS SDK, Ultra Game Template, Ultra Loading Screen) no se versionan en este repositorio. El arte generado con IA que aparezca durante el desarrollo es placeholder declarado.
