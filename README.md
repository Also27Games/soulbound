# Soulbound Core

Trabajo de Fin de Máster del Máster en Desarrollo con IA (BIG School, 3ª edición). Autor: Lluis Alonso.

**Soulbound** es un party game de terror social: cuatro jugadores atrapados en una cripta viviente compiten en un juego de mesa mortal dirigido por un anfitrión sádico, el Warden, que cambia las reglas a su antojo. Para escapar hay que reunir seis runas y sobrevivir a sus caprichos antes de que el tiempo se agote.

**Soulbound Core** es la primera fase del juego: el tablero multijugador en pantalla plana, el motor de reglas y la IA directora del Warden. Construido en Unreal Engine 5.8 con C++ aplicando la metodología del máster: diseño por sistemas con reglas y criterios de aceptación, motor de reglas sin mundo probado por consola, y stories implementadas con agentes y revisadas por el autor.

## Estado

Septiembre de 2026: diseño de la Fase 1 cerrado; semana de prueba técnica (spike) en la rama `spike`. `main` contiene el proyecto y la infraestructura; el código desechable del spike vive en `spike` hasta que se decida qué sobrevive.

## Estructura

- `Source/SoulboundCore/`: motor de reglas sin mundo (solo `Core` y `CoreUObject`). Estado en `USTRUCT`, reglas como funciones libres, tests de automatización por criterio de aceptación.
- `Source/Soulbound/`: el juego (subsistemas, GameMode, controladores, presentación).
- `Plugins/A27Online/`: plugin propio de sesiones por código sobre Epic Online Services.
- `Scripts/`: `build.ps1` (compilar y empaquetar sin editor), `test.ps1` (tests por consola), `eos-secrets/` (secretos fuera del repositorio), `git-hooks/`.
- `docs/`: `stories/` (una story por archivo), `bitacora.md` (historia legible del proyecto, una entrada por story), documentación de diseño pública (pendiente de publicar).
- `CLAUDE.md`: guía para los agentes. La skill del proyecto (glosario, pilares, plantillas, checklist de revisión) está en `.claude/skills/soulbound/`.

## Puesta en marcha en Windows

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
7. **Steam**: el App Id del producto está en `Config/Windows/WindowsEngine.ini`. La build empaquetada hace login por Steam (cliente abierto y sesión iniciada); en el editor se usa la Developer Authentication Tool de Epic, que Redpoint lanza solo.

Compilar y probar sin abrir el editor:

```powershell
.\Scripts\build.ps1 -Que editor
.\Scripts\test.ps1
.\Scripts\build.ps1 -Que package
```

## Propiedad intelectual

Soulbound es un diseño original inspirado en el género de los juegos de mesa con anfitrión en vídeo de los años 90. No contiene material de terceros. Los assets y plugins de pago o con licencia propia (Redpoint, EOS SDK, Ultra Game Template, Ultra Loading Screen) no se versionan en este repositorio. El arte generado con IA que aparezca durante el desarrollo es placeholder declarado.
