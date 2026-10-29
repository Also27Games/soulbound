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
   - Data Asset de configuración: `Disable Game Pause on Enter Pause Menu` activado (la pausa nunca detiene la partida); "nueva partida" apuntando al mapa del juego.

   Arranque mínimo (S-022), cinco pasos más, en este orden: el vídeo primero, porque quita los avisos rojos de cada arranque; las imágenes al final, porque dependen del arte. Ninguno toca código; cada asset que cambie va en un commit aparte y se verifica en el editor.
   - **Vídeo por defecto, igual que el proyecto.** UGT aplica sus propios ajustes de vídeo al arrancar y pisa los de `Config/DefaultEngine.ini`. Sus valores por defecto tienen que dejar apagados Lumen (`r.DynamicGlobalIlluminationMethod=0`, `r.ReflectionMethod=0`), las sombras virtuales (`r.Shadow.Virtual.Enable=0`) y el trazado de rayos (`r.RayTracing=False`). Antialiasing: `DefaultEngine.ini` dice `r.AntiAliasingMethod=0` (ninguno) con el renderizador diferido (`r.ForwardShading=False`), así que su `r.MSAACount=4` no se aplica; el método lo decide el autor en este paso. Comprobar en el exe empaquetado que no salen los avisos de Lumen ni de trazado de rayos.
   - **Menús.** Se quedan Nueva partida, Opciones, Cómo se juega (nuevo) y Salir; los demás que trae UGT se quitan. Para volver al menú al acabar la partida no hace falta nada en UGT: el juego pone un botón "Volver al menú" (Slate, en código) y la tecla Intro (`ReturnToMenuKey`). Escape sigue siendo la pausa de UGT; no se usa para esto porque en PIE lo captura el editor.
   - **Página "Cómo se juega".** Un bloque de texto con scroll, enlazado a `UHowToPlayLibrary::HowToPlayText` (nodo *How To Play Text*, `BlueprintPure`). Devuelve el texto de `Content/Data/Textos/como-se-juega.md` con los títulos sin `## `; si el archivo falla, un texto vacío y el motivo en el log. El texto lo escribe el autor, en el mismo commit de datos que `frases.json`.
   - **Opciones › Juego › Ayudas.** Un combo con tres opciones en este orden: Automático, Siempre, Nunca (los `DisplayName` de `EHintsMode`). Al abrir la página, *Get Game Instance Subsystem* (`UPlayerProfileSubsystem`) › `GetHintsIndex` y se selecciona ese índice; al cambiar la selección, `SetHintsIndex` con el índice, que guarda el perfil. La correspondencia índice ↔ ajuste está solo en C++ (`HintsModeToIndex`, `HintsModeFromIndex`); también existen `GetHints` y `SetHints` con el enum. Comprobar en partida con `SbHints` sin argumentos.
   - **Imágenes del menú.** Las texturas del autor sustituyen a las de ejemplo de UGT; están fuera del repositorio hasta que el autor las haga.
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
