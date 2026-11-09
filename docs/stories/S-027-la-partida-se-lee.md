# S-027. La partida se lee: el HUD en paneles que caben en cualquier pantalla, el Grimorio, cuatro Ayudas más y la vuelta al menú

**Sistema**: interfaz en plano (GDD 07: el HUD de la Fase 1 y el Grimorio, R-HUD-4) y las Ayudas del Warden (GDD 06, S-021 A3). **Módulos**: `Soulbound` (vista y presentación) y, solo para los tres momentos de instrucción nuevos, `SoulboundCore` (`Warden/WardenLines.h`). Ninguna regla de juego cambia. **Rama base**: `main`, con S-026 fusionada. **Tamaño**: mediano.

## Contexto

El test de usabilidad (noviembre de 2026) ha dado tres cosas que esta story arregla:

1. **En una pantalla a 1024x768 el HUD no cabe.** Las líneas de estado se dibujan con `AddOnScreenDebugMessage`, con tamaño fijo en píxeles: en el PC de un probador se montan unas sobre otras y sobre la frase del Warden, y Unreal añade su propio aviso ("'DisableAllScreenMessages' to suppress"). Los paneles Slate de la derecha (leyenda y crónica), con ancho propio, se ven bien.
2. **El jugador no sabe para qué sirve su Elegido.** Afinidad, mapa de runa a poder y poderes activos no se leen en ningún sitio salvo una línea críptica de la leyenda ("Brazal por asiento: runas | poderes"). Un probador, tras diez partidas: "el Elegido no sé para qué sirve".
3. **Hay acciones que el Warden nunca explica**: ser Eco al acabar la carrera, robar, el Vacío y, desde S-026, el anillo. El probador: "robar es la acción que aún no sé cómo se activa".
4. **"Volver al menú" al acabar la partida lleva a los logos de arranque**, no al menú principal: `TravelToMenu` (S-022) abre `GameDefaultMap`, que es `UGT_LauncherMap`, el lanzador con la intro. El menú de pausa de UGT abre `UGT_MainMenuMap`.

Decidido por el autor el 9/11/2026:

- El HUD de estado pasa a **paneles Slate** que escalan con la altura de la pantalla, como la leyenda y la crónica. Lo que es para el jugador se queda a la vista; lo que es de depuración (semilla, tiempo, ronda, evaluación del Warden, insolencias, paso a paso) va a un panel aparte, **oculto por defecto**, que se abre con **F1**.
- **El Grimorio**: la tecla **G** abre un panel con los cuatro asientos: Elegido, afinidad, runas y el poder que da cada runa, y poderes activos. Los textos de afinidades y poderes salen de datos.
- **Cuatro Ayudas nuevas** del Warden: al quedar como Eco al acabar la carrera, la primera vez que se puede robar, al entrar en el Vacío, y la primera vez que se abre el anillo. Los textos los escribe el autor; la story deja las claves con "(pendiente)" como en S-021.
- **"Volver al menú" abre el mapa del menú principal**, el mismo que el menú de pausa de UGT, leído de un parámetro de configuración.
- La ficha con el color del Dominio y el resto del feedback visual **no** van aquí: son S-029.
- La resolución por defecto y el ancho del texto de "Cómo se juega" son configuración de UGT y los hace el autor a mano (`docs/configuracion-ugt.md`), no esta story.

## Reglas a implementar

### Vista (`Soulbound`, `Presentation/`)

- **R-HUD-1 (precisión). Panel de estado.** Un panel Slate arriba a la izquierda (`SStatusPanel`, nombre propuesto; patrón de `SLegendPanel`) sustituye a las líneas 0, 1, 3, 6, 8, 9, 10, 11, 12 y 13 de `ShowText`: fase de la partida y prólogo, tirada y reloj de turno, Sellos activos, carrera, la petición al jugador (`ChoicePrompt`), trilero, duelo, evento del Warden, Noche de los Ecos y Oráculo. Mismo contenido y mismos colores que hoy; solo cambia dónde y cómo se dibuja. La petición al jugador va la primera y con el tamaño mayor del panel.
- **R-HUD-2 (precisión). Panel de depuración.** Un segundo panel (`SDebugPanel`, nombre propuesto) con las líneas 2, 4, 5 y 7: paso a paso, evaluación del Warden, semilla y tiempo, asientos e insolencias. Oculto al empezar; **F1** lo muestra y lo oculta. `ShowText` y `AddOnScreenDebugMessage` dejan de usarse en la vista: el aviso de Unreal desaparece solo. Las respuestas de los comandos `Sb*` del controlador (`Say`) no cambian: solo salen cuando alguien escribe un comando.
- **R-HUD-3 (precisión). Escala.** El tamaño de fuente de todos los paneles (estado, depuración, leyenda, crónica y Grimorio) se calcula a partir de la altura del viewport: `PanelFontSize` pasa a ser el tamaño a 1080 píxeles de alto y se escala en proporción, con un mínimo en Config (`PanelMinFontSize`, valor inicial 9). El ancho de los paneles de la derecha, igual. Se recalcula al cambiar la resolución desde el menú.
- **R-HUD-4. El Grimorio.** La tecla **G** muestra y oculta un panel centrado (`SGrimoirePanel`, nombre del glosario) con una fila por asiento: número y si es humano; Eco o Elegido, con el nombre del Elegido y su Dominio; la afinidad, con su nombre y una frase de qué hace; las seis runas, con la cantidad que tiene de cada una y el poder que le da cada una según su mapa, marcando las activas; y los poderes activos. Mientras está abierto, la partida sigue (no es pausa) y los clics sobre el tablero no cuentan. **H** oculta también el Grimorio. Los textos de afinidades y poderes se leen de `Content/Data/Textos/grimorio.json` (nombre propuesto): una frase por afinidad (`EAffinity`) y una por poder (`EPower`); si falta el archivo o una clave, el panel enseña el nombre del enumerado y lo dice en el log, como hace `HowToPlayLibrary`.
- **R-HUD-5 (precisión). Lo que sale del panel de estado se construye con funciones puras** en `Presentation/StatusText.h` (nombre propuesto, junto a `EventText.h`): dado el estado de la partida y qué asiento es el humano, devuelven las líneas del panel de estado, las del de depuración y las filas del Grimorio, como texto y color. Sin actor, sin mundo, con tests.

### Vuelta al menú (`Soulbound`)

- **R-MEN-1 (precisión).** `ABoardViewActor::TravelToMenu` abre el mapa de `ReturnToMenuMap` (`UPROPERTY(Config)`, `FSoftObjectPath`, nombre propuesto), con valor inicial `/Game/UltraGameTemplate/Maps/UGT_MainMenuMap.UGT_MainMenuMap` en `Config/DefaultGame.ini`, con comentario. Si está vacío o el mapa no existe, abre `GameDefaultMap` como hoy y lo dice en el log, para que el juego nunca se quede sin salida. El valor se lee, no se copia la ruta en el código.

### Ayudas (`SoulboundCore`, `Warden/WardenLines.h`; `Soulbound`, `Presentation/InstructionTracker`)

- **R-INS-1 (precisión). Cuatro momentos nuevos** en `EInstructionMoment`: `Echo` (el humano queda como Eco al acabar la carrera: evento `BondRaceEnded` con su asiento sin vínculo), `Steal` (la primera vez que la elección `PlayerAction` abierta al humano incluye robar), `Void` (la primera vez que el humano entra en el Vacío: evento `Banished` sobre su asiento) y `Ring` (la primera vez que el anfitrión abre un anillo para el humano, S-026: lo detecta la vista, no el registro, porque el anillo no es un evento del motor). Claves `instruccion:eco`, `instruccion:robar`, `instruccion:vacio`, `instruccion:anillo` en `Content/Data/Warden/frases.json`, con el texto "(pendiente)" hasta que el autor escriba el suyo. El cargador de frases las exige como las demás instrucciones. `FInstructionTracker::Observe` las detecta una vez por partida, como las otras, y `ShouldTeach` las trata igual.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/StatusTextTest.cpp`, nombres `Soulbound.Game.Presentation.Estado.<QueSeComprueba>`; y `GrimoireTextTest.cpp`, `Soulbound.Game.Presentation.Grimorio.<QueSeComprueba>`; como `BoardLayoutTest.cpp`):

- **CA-HUD-1** (estado). Con una partida montada en el turno del humano y una elección de destino abierta, las líneas del panel de estado contienen la tirada, el reloj y la petición al jugador en primera posición; ninguna contiene la semilla ni la palabra "Semilla".
- **CA-HUD-2** (depuración). Las líneas del panel de depuración contienen la semilla, el tiempo y las insolencias de los cuatro asientos, y ninguna de las del panel de estado.
- **CA-HUD-3** (Grimorio). Con un asiento Elegido de Fenrir con una runa del Dominio 5 y otro asiento Eco, las filas del Grimorio dicen: para el primero, "Fenrir", su afinidad, la runa D5 con cantidad 1 y el poder que le da (Vuelo Planar) marcado como activo, y el resto de runas sin cantidad; para el segundo, "Eco" sin Elegido ni afinidad.
- **CA-HUD-4** (textos de datos). Con un `grimorio.json` de prueba, la frase de una afinidad y la de un poder son las del archivo; sin el archivo, el panel usa el nombre del enumerado y no falla.
- **CA-HUD-5** (escala). La función de escala devuelve `PanelFontSize` a 1080 de alto, la mitad a 540, y nunca menos de `PanelMinFontSize`.
- **CA-INS-1** (Eco). Con un registro donde la carrera acaba y el asiento humano queda sin vínculo, `Observe` devuelve el momento `Echo` para ese asiento, una sola vez; si el humano se vinculó, no lo devuelve.
- **CA-INS-2** (robar). La primera elección pendiente de robo ofrecida al humano da el momento `Steal`; la segunda, no.
- **CA-INS-3** (Vacío). El primer `Banished` sobre el humano da el momento `Void`; el de un dummy, no.
- **CA-INS-4** (anillo). La primera apertura de un anillo para el humano da el momento `Ring`; la segunda, no.
- **CA-INS-5** (datos). El cargador de frases rechaza un archivo sin alguna de las cuatro claves nuevas, y acepta el de `Content/Data/Warden/` con ellas en "(pendiente)".
- **CA-MEN-1** (vuelta al menú, sin mundo). La función pura que elige el mapa devuelve `ReturnToMenuMap` si tiene valor y, si está vacío, `GameDefaultMap`.

Manuales, por el autor, en PIE y en una build a 1024x768 y a 1920x1080, con captura:

- A 1024x768 ninguna línea del panel de estado se monta con otra ni con la frase del Warden, y no aparece el aviso de Unreal de los mensajes en pantalla.
- F1 muestra el panel de depuración con la semilla; otra vez F1 lo oculta. Al empezar, no está.
- G abre el Grimorio con los cuatro asientos y sus textos de afinidad y poder; G o H lo cierran; con él abierto, un clic en el tablero no mueve la ficha.
- Al cambiar la resolución en Opciones › Vídeo, los paneles cambian de tamaño sin reiniciar.
- Al acabar la carrera como Eco, el Warden dice la ayuda `instruccion:eco`; con "Ayudas: Nunca", no.
- Los comandos `Sb*` siguen respondiendo en pantalla como hasta ahora.
- Al acabar una partida, "Volver al menú" e Intro llevan al menú principal sin pasar por los logos; con `ReturnToMenuMap` vacío en el ini, llevan al lanzador como hoy.
- La primera vez que sale el anillo, el Warden dice la ayuda `instruccion:anillo`.

## Notas para quien lo implemente

- **Reutilizar** `SLegendPanel` y `SChroniclePanel` como patrón: Slate puro, sin UMG, añadidos a la misma capa (`Overlay`) y con la misma regla de visibilidad (`bPanelsHidden` y el telón). El Grimorio es una capa aparte, centrada, por encima.
- Las líneas de hoy están en `ABoardViewActor::Tick` (desde `ShowText(0, …)` hasta `ShowText(13, …)`). Mover el texto de cada una a `StatusText.h` tal cual; esta story no reescribe los textos, salvo quitar de las líneas del jugador las menciones a teclas de depuración.
- `ChoicePrompt`, `ShellGameLine` y los textos de evento y duelo ya existen en `EventText`; el panel los usa, no los duplica.
- El Grimorio lee `FMatchState::Elects` (`FElectsTable`) y `FSeatState::Runes`; `IsPowerActive` y `GetActivePowers` (`Match/RuneRules.h`) dicen qué está activo. Nada nuevo en el motor para esto.
- Los tres momentos nuevos tocan `SoulboundCore` solo en el enumerado; su detección vive en `FInstructionTracker` (presentación). Si el cargador de frases o `ShouldTeach` necesitan algo más que la clave, es duda para el autor.
- `G` y `F1` están libres (las teclas usadas: C, L, H, T, R, números, espacio, clics). Van en el controlador del spike como las demás, con su línea en `SbHelp`.
- Tests primero, en rojo. Los paneles solo tienen criterio manual.
- Al cerrar: la leyenda del HUD y el README (sección "Jugar la build", teclas) mencionan G y F1; la línea de teclas del final de `Content/Data/Textos/como-se-juega.md` también.

## Fuera de esta story

- Las decisiones del jugador como panel con botones (S-028).
- La ficha con el color del Dominio, la ficha activa resaltada, destellos (S-029).
- Los textos de las cuatro Ayudas nuevas: los escribe el autor en `frases.json`, en su commit de datos.
- La resolución por defecto y el ancho de "Cómo se juega" (configuración de UGT, autor).
- Un botón "Cómo se juega" en el menú de pausa (configuración de UGT, autor).
- Sonido y la interfaz del diseño (GDD 07).
