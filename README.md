# Soulbound Core

Trabajo de Fin de Máster del Máster en Desarrollo con IA (BIG School, 3ª edición). Autor: Lluis Alonso.

## 1. Descripción general

**Soulbound** es un party game de terror social: cuatro jugadores atrapados en una cripta viviente compiten en un juego de mesa mortal dirigido por un anfitrión sádico, el Warden, que cambia las reglas a su antojo. Para escapar hay que reunir seis runas y sobrevivir a sus caprichos antes de que el tiempo se agote.

**Soulbound Core** es la primera fase del juego y el objeto de este TFM: el tablero multijugador en pantalla plana (Windows, Steam Deck), el motor de reglas y la IA directora del Warden. Construido en Unreal Engine 5.8 con C++ aplicando la metodología del máster: diseño por sistemas con reglas y criterios de aceptación, motor de reglas sin mundo probado por consola, y stories implementadas con agentes y revisadas por el autor. El proceso está contado en `docs/bitacora.md`.

## 2. Stack tecnológico

- **Motor**: Unreal Engine 5.8.3, C++ (módulos `SoulboundCore` y `Soulbound`), Blueprints solo como adaptadores de presentación.
- **Online** (infraestructura, sin partida en línea en esta entrega): Epic Online Services a través de Redpoint EOS Online Framework (Free Edition) y un plugin propio de sesiones por código (`A27Online`, escrito antes del máster y reutilizado aquí tal cual, como dependencia externa); login por Steam en la build empaquetada cuando hay cliente de Steam, y sin él el juego arranca igual.
- **Menús, opciones y carga**: Ultra Game Template y Ultra Loading Screen (Fab), usados como cajas negras tras un adaptador propio.
- **Herramientas**: Rider, Visual Studio 2022 (toolchain), Git con LFS, GitHub CLI, PowerShell (`Scripts/`), Python (utilidades de arte).
- **IA en el desarrollo**: Claude Code con la skill del proyecto (`.claude/skills/soulbound/`), guía para agentes en `CLAUDE.md`, y el plugin Unreal MCP del editor para consulta y tests.
- **Arte**: placeholders generados con IA (declarados como tales), a sustituir por arte encargado si el juego funciona.

## 3. Instalación y ejecución

### Jugar la build

Requisitos: Windows 10 u 11 de 64 bits, tarjeta gráfica con DirectX 12, 8 GB de memoria, ratón y teclado; 1 GB libre en disco. No hace falta Steam, cuenta ni conexión. Descargar la build del enlace de despliegue (al final), descomprimir en una carpeta propia (no en `Archivos de programa`) y ejecutar `Soulbound.exe`. Windows avisa de que el programa no está firmado ("Windows protegió su PC"): *Más información* › *Ejecutar de todas formas*. Si pide instalar un componente de Microsoft Visual C++, aceptar. En el menú, *Jugar* abre la pantalla "Cómo se juega" y de ahí a la partida contra tres jugadores automáticos. Teclas en partida: clic en una casilla que late para moverse; para cada decisión, los botones del panel de abajo o su número (también para votar y para el Oráculo), o clic en la ficha para votar; Esc para la pausa, C vista, L etiquetas, G Grimorio (tu Elegido, su afinidad y lo que da cada runa), H paneles, D panel de depuración, Intro para volver al menú al acabar (lleva al menú principal). El log del motor queda en `Soulbound\Saved\Logs\` y el acta de cada partida (todo lo que pasó en la mesa, turno a turno, y cómo acabó), en `Soulbound\Saved\Partidas\`: es el archivo que se pide a los probadores.

### Desarrollo en Windows

Lo que no está en el repositorio y hay que instalar a mano, en este orden.

1. **Herramientas**: Unreal Engine **5.8.3** (Epic Games Launcher), Visual Studio 2022 con "Desarrollo de juegos con C++" (o solo Build Tools), Rider (opcional; abre el `.uproject`, no los `.slnx`), Git for Windows (incluye LFS) y GitHub CLI. En PowerShell, una vez: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`.
2. **SDK de .NET 10 global cuyo runtime coincida con el del motor** (hoy, 10.0.7: SDK 10.0.2xx). El plugin de Redpoint recompila UnrealBuildTool con el SDK global; uno más nuevo que el runtime del motor falla. Comprobar con `dotnet --list-sdks` y `dotnet --list-runtimes`.
3. **Clonar** y activar el filtro de secretos y los hooks (en Git Bash): `sh Scripts/eos-secrets/install.sh`. Rellenar `Config/Local/EOS.ini` con los identificadores del producto en el portal de Epic y la clave de licencia de Redpoint (nunca se versionan), y regenerar el archivo de trabajo: `Remove-Item Config\DefaultEngine.ini; git checkout -- Config/DefaultEngine.ini`.
4. **Redpoint EOS Online Framework, Free Edition** para 5.8, en `Plugins/OnlineSubsystemRedpointEOS/`, y el **SDK de EOS** en `Plugins/EOS-SDK-<version>/` (la versión la dice Redpoint al compilar). Los dos están ignorados por Git.
5. **Ultra Loading Screen** (Fab): instalar en el motor; el plugin ya está activado en el proyecto. Va antes que UGT porque UGT se reparenta a su GameInstance.
6. **Ultra Game Template** (Fab, Launcher > Add to Project): queda en `Content/UltraGameTemplate/`, ignorado. Sus ajustes no viajan con el repositorio: la integración con Ultra Loading Screen y el telón, los menús, las opciones y la pantalla de carga se rehacen a mano siguiendo `docs/configuracion-ugt.md`. Sin ellos el juego arranca y la partida funciona, pero con los menús de ejemplo de UGT.
7. **Steam**: el App Id del producto está en `Config/Windows/WindowsEngine.ini`. Solo interviene en la sesión en línea del spike: la build empaquetada intenta el login por Steam si el cliente está abierto, y si no, arranca igual y la partida local no lo nota. En el editor se usa la Developer Authentication Tool de Epic, que Redpoint lanza solo.

Compilar, probar y empaquetar sin abrir el editor:

```powershell
.\Scripts\build.ps1 -Que editor
.\Scripts\test.ps1
.\Scripts\build.ps1 -Que package
```

## 4. Estructura del proyecto

- `Source/SoulboundCore/`: motor de reglas sin mundo (solo `Core` y `CoreUObject`). Estado en `USTRUCT`, reglas como funciones libres, tests de automatización por criterio de aceptación.
- `Source/Soulbound/`: el juego (subsistemas, GameMode, controladores, presentación).
- `Plugins/A27Online/`: plugin propio de sesiones por código sobre Epic Online Services, anterior al máster; no se modifica desde las stories.
- `Scripts/`: `build.ps1` (compilar y empaquetar sin editor), `test.ps1` (tests por consola), `eos-secrets/` (secretos fuera del repositorio), `git-hooks/`, `art/` (utilidades de arte), `board-editor/` (editor de tableros: una página HTML que genera tableros aleatorios a partir de parámetros, permite editarlos a mano, valida las mismas reglas que el motor y descarga el JSON de `Content/Data/Boards/`).
- `docs/`: `stories/` (una story por archivo), `bitacora.md` (historia legible del proyecto, una entrada por story), `decisiones.md` (las decisiones que definen el proyecto, dónde verlas y qué se dejó fuera a sabiendas), `configuracion-ugt.md` (los ajustes manuales de Ultra Game Template), `test-usabilidad/` (guion e informe), documentación de diseño y arquitectura pública (en construcción).
- `CLAUDE.md`: guía para los agentes. La skill del proyecto (glosario, pilares, plantillas, checklist de revisión) está en `.claude/skills/soulbound/`.

Ramas: `main` contiene el proyecto estable; cada story se hace en una rama `story/S-NNN-nombre` y entra por pull request. La prueba técnica de septiembre de 2026 (rama `spike`, ya fusionada y borrada) sobrevive como `Source/Soulbound/Spike/`: el controlador con los comandos de consola `Sb*` y el mapa de partida.

## 5. Funcionalidades principales

Lo que hay en `main` a fecha del test de usabilidad (noviembre de 2026), tras 24 stories (`docs/stories/`), con 384 tests de automatización por consola:

**Una partida completa, de uno contra tres bots.**

- **Tablero desde datos**: seis Dominios alrededor del Altar, con Nicho, Fuente de Poder y Nodo de Poder en cada uno; tableros en JSON (`Content/Data/Boards/`) validados por el motor y generados con el editor de tableros de `Scripts/board-editor/`. Dominios Olvidados sorteados al empezar.
- **Turnos y dados**, con reloj de turno y de elección; movimiento por el grafo con las reglas del Altar y de los Sellos.
- **Carrera por el vínculo**: cinco minutos para reclamar un Nicho y vincularse a su Elegido; quien no llega sigue como Eco, confinado a los Dominios Olvidados, y puede ganar igual.
- **Runas y poderes**: seis Fuentes, seis runas, seis poderes (Celeridad, Paso Libre, Vuelo Planar, Expulsión, Duelo, Desvío), con un mapa fijo de runa a poder distinto para cada Elegido: la misma runa da un poder diferente a cada uno, y la runa del propio Dominio siempre da Celeridad.
- **Duelo, robo y trilero**; el Vacío de la Furia y la vuelta al hogar.
- **Victoria**: seis runas distintas, vuelta al Altar y desafío final; a los cuarenta minutos gana el Warden.
- **Pruebas de habilidad con el anillo**: la runa en la Fuente de un Dominio Olvidado, el duelo, el escape del Vacío, la Prueba del Warden y el desafío final las juega el jugador con el minijuego del anillo (GDD 09, versión mínima): un anillo se cierra sobre una runa y se hace clic cuando la toca; cuanto más cerca del momento justo, más precisión. Los parámetros de cada prueba están en `Content/Data/Match/partida.json`. Los jugadores automáticos siguen resolviendo sus pruebas al azar, y un ajuste (`bHumanPlaysSkillTrials`) vuelve al comportamiento anterior. El trilero también lo juega el jugador: elige él.
- **El Warden**: un cerebro por actos con sacos de eventos (38 en datos, `Content/Data/Events/`), decretos, dilemas, votaciones, el Oráculo del Caos, Sellos, insolencia y Furia. Sus frases viven en `Content/Data/Warden/frases.json` y las enseña una vez por jugador (perfil guardado en la máquina), con el ajuste Ayudas en Opciones. Sin voz sintética: subtítulos.
- **Jugadores automáticos**: los tres asientos libres los ocupan dummies, decisores de prueba que corren al Nicho libre más cercano, luego hacia el Altar, y eligen al azar lo demás. Sirven para que la mesa esté llena; el bot básico por prioridades del diseño es Fase 2.
- **Simulación sin mundo**: partidas enteras con reloj virtual, deterministas por semilla, como tests; la misma lógica que mueve la partida en el editor.
- **Presentación**: tablero en 3D generado desde los datos sobre un kit de piezas, fichas que recorren su camino, vista cenital y sentada, una línea de objetivo, en su caja abajo a la izquierda, que dice la meta del jugador en cada momento (un Nicho libre, las Fuentes cuya runa le falta o el Altar), suelo propio en los Dominios Olvidados, la señal de corte cuando el Warden corta un turno (el tablero se oscurece un instante, la frase empieza por "¡Quietos!" y una línea pequeña debajo dice qué ha pasado), frases del Warden que duran lo que se tarda en leerlas y que una burla no tapa, el orbe del Oráculo que late sobre la ficha, HUD en paneles que escalan con la pantalla (estado, depuración con D, leyenda y crónica), Grimorio con la tecla G (Elegidos, afinidades y poderes, textos en `Content/Data/Textos/grimorio.json`), pantalla "Cómo se juega" desde datos, menús, opciones, pausa que no detiene la partida y pantalla de carga; la partida espera a que la pantalla de carga se vaya.

**Lo que queda fuera de esta entrega** y pasa a la Fase 2, con lo que ya existe para cada cosa:

- **Partida en línea.** Existe la sesión por código de cuatro letras sobre Epic Online Services (crear, unirse, salir; plugin propio `A27Online`, probado en el editor con dos jugadores en septiembre de 2026), pero la partida no se replica: quien se une llega al mapa y no ve la partida del anfitrión. La arquitectura está pensada para ello (un solo escritor del estado, intenciones validadas, bots que ocupan asientos), y el GDD de multijugador está aprobado; implementarlo es la primera story de la Fase 2.
- **El resto del minijuego de habilidad** (GDD 09): el anillo ya existe en su versión mínima; faltan la variante de desenfunde, las trampas del Warden en el desafío final, el duelo simultáneo de dos humanos, la habilidad de los bots por dificultad, el mando y el sonido.
- **Interfaz del diseño** (GDD 07): cámara y objetos diegéticos, Umbral, barcas y postpartida. Hoy la interfaz es la vista de depuración, legible pero sin arte.
- **Voz sintética del Warden**, sonido y música. Hoy no hay audio.
- **Bot básico** por prioridades y dificultades (GDD 08); hoy, dummies.
- **Grabación y reproducción de partidas** (la simulación ya es determinista por semilla; falta el registro).
- **Steam Deck a 60 FPS** y la publicación en Steam.

## 6. Acceso de prueba

**No hace falta usuario, contraseña ni cuenta.** La build empaquetada arranca sin Steam y sin conexión, y la partida local contra bots no usa ningún servicio externo. En el editor tampoco: la Developer Authentication Tool de Epic solo interviene si se prueba la sesión en línea del spike (comandos `SbCreate` y `SbJoin` de la consola), que no forma parte de la entrega.

## Entregables del TFM

- Repositorio: este.
- Despliegue o publicación: pendiente (build descargable para Windows, sin cuenta ni clave; enlace al cerrar el test de usabilidad).
- Test de usabilidad: guion en `docs/test-usabilidad/guion.md`; informe en `docs/test-usabilidad/informe.md` al acabar (noviembre de 2026).
- Slides: pendiente (enlace o archivo en `docs/`).
- Vídeo con explicación y captura de pantalla: pendiente.

## Propiedad intelectual

Soulbound es un diseño original inspirado en el género de los juegos de mesa con anfitrión en vídeo de los años 90. No contiene material de terceros. Los assets y plugins de pago o con licencia propia (Redpoint, EOS SDK, Ultra Game Template, Ultra Loading Screen) no se versionan en este repositorio. El arte generado con IA que aparezca durante el desarrollo es placeholder declarado.

**Derechos.** Este repositorio está abierto para que pueda evaluarse mi trabajo de fin de máster y para enseñar cómo se ha hecho. Soulbound es un juego que quiero publicar, y su código, su diseño, sus textos y su arte no llevan licencia de uso. Para reutilizar cualquier parte, pregúntame antes.
