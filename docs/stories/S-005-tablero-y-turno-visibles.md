# S-005. Tablero y turno visibles (vista de depuración)

**Sistema**: tablero y movimiento (GDD 01), turnos y dados (GDD 02), presentación mínima. **Módulo**: `Soulbound` (juego), con un cambio pequeño en `SoulboundCore`. **Rama base**: `main`. **Tamaño**: media.

## Contexto

El motor ya carga un tablero, inicia una partida, juega turnos con reloj y sella con tope, pero nada de eso se ve: el nivel del spike (`Content/Spike/Maps/L_SpikeOnline`) es un cielo vacío con mensajes de texto, y el anfitrión del Warden (`Source/Soulbound/Warden/WardenHostSubsystem`) evalúa sobre un `MakeSpikeMatch` propio. Esta story pone la partida real en el nivel: casillas y fichas generadas desde los datos, clic para mover, texto con el estado del turno. Es una vista de depuración, con cámara cenital fija y formas básicas del motor: no es la interfaz del GDD 07 ni tiene arte. Sirve para ver cada regla el día que se fusiona.

Decidido por el autor el 28/09/2026: cámara cenital fija; el asiento 0 lo juega el humano con el ratón y los otros tres, decisores dummy que responden a los 5 segundos; la disposición de las casillas sale del bloque `editor.positions` del JSON del tablero (una sola fuente de verdad, la del editor).

Decidido el 30/09/2026: la vista usa un tablero de **15 casillas por Dominio**, `Content/Data/Boards/tablero_15.json` (91 casillas, 112 conexiones, seis Nodos: dos por efecto), generado con el editor de tablero (`Scripts/board-editor`, semilla 28) y ya incluido en esta rama. Nichos a 3 o 4 casillas del Altar y Fuentes entre 4 y 6. Los tests existentes siguen usando `tablero_pruebas.json`. El tamaño definitivo del tablero se decidirá con simulación cuando existan las runas; este archivo es para ver y jugar.

## Qué se construye

1. **Anfitrión de partida** (nombre a proponer; sugerencia `MatchHost`). Subsistema nuevo, distinto del anfitrión del Warden: es el dueño de la partida. El anfitrión del Warden deja de tener partida propia y actúa sobre la de este. Es un `UWorldSubsystem` que solo existe en mapas de juego y solo en el servidor (igual que el anfitrión del Warden, no arranca en los menús). Al empezar el mundo carga `Content/Data/Boards/tablero_15.json` y `Content/Data/Match/partida.json` (las dos rutas, como parámetros con valor inicial documentado, no repartidas por el código), llama a `StartMatch` con semilla y guarda el `FMatchState`. Avanza la máquina de turnos con el reloj real (`AdvanceTurn` con el tiempo de partida, a intervalos cortos), consulta a los decisores dummy de los asientos 1 a 3 (respuesta a los 5 s, con su propio generador, como en la simulación) y recibe la intención de destino del humano. Expone el estado en solo lectura y un delegado "el estado ha cambiado".
2. **El anfitrión del Warden** evalúa y aplica sobre el estado del anfitrión de partida. Mismo cerebro del spike.
3. **Presentación generada desde datos**: al iniciar la partida, las conexiones del grafo dibujadas entre casillas (una línea o tira por conexión: sin ellas no se sabe por dónde se mueve una ficha), un actor por casilla (forma básica del motor, color por tipo: Altar, normal, Nicho, Fuente, Nodo; Dominios Olvidados atenuados) en la posición del bloque `editor.positions` escalada; una ficha por asiento (forma básica con color del asiento) sobre su casilla, y desplazadas entre sí si comparten casilla; las casillas alcanzables del turno en curso resaltadas; las selladas con color propio. Nada se coloca a mano en el mapa: todo lo genera código al empezar la partida, así el mapa no cambia. Materiales: el material básico del motor con parámetro de color; ningún asset nuevo.
4. **Entrada**: clic del ratón sobre una casilla resaltada envía la intención de destino del asiento 0; clic fuera o sobre una no resaltada no hace nada. Cursor visible.
5. **Texto en pantalla** (mensajes de depuración del motor, sin widgets): turno, asiento activo, fase, tirada, segundos que quedan del reloj de turno, Sellos activos, y la última línea del Warden.
6. **Cámara**: una cámara cenital sobre el centro del tablero, generada por código, como vista del jugador.
7. **Datos en la build**: los JSON de `Content/Data` entran en el empaquetado (`DirectoriesToAlwaysStageAsUFS` en `Config/DefaultGame.ini`) y la build de Windows carga la partida igual que el editor.
8. **`SoulboundCore`**: el cargador del tablero lee el bloque opcional `editor.positions` a un campo de posición 2D por casilla (nombre a proponer); si falta, queda sin posición y la presentación reparte las casillas en anillo por Dominio. Con test.

## Criterios de aceptación

Automáticos (comandlet):
- **Posiciones desde el archivo** (R-TAB-1). Dado el tablero de pruebas, cuando se carga, entonces cada casilla tiene la posición 2D del bloque `editor.positions`; dado un JSON sin ese bloque, entonces carga igual y las posiciones quedan sin definir. Los tests actuales siguen en verde.
- **El tablero de 15 es válido** (R-TAB-1 a 5). Dado `Content/Data/Boards/tablero_15.json`, cuando se carga con `LoadBoardFromFile`, entonces pasa `ValidateBoard`, tiene 91 casillas y `StartMatch` inicia una partida sobre él. Si no pasa, el fallo es del archivo (lo generó el editor, cuyo validador es un espejo en JavaScript del de C++): se para y se avisa al autor, no se retoca el validador.
- **Simulación sobre el tablero de 15.** Dada la simulación con dummies sobre `tablero_15.json`, cuando corre una partida entera, entonces termina, es determinista por semilla y sigue por debajo del segundo.

Manuales, con el editor (PIE) y con la build empaquetada; se documentan en el PR con una captura cada uno:
- **CA-TAB-22** (R-TAB-21). Dado el turno del asiento 0, cuando se resaltan las casillas alcanzables, entonces son exactamente las que devuelve `ReachableTiles` para esa tirada (misma función, ningún cálculo en la presentación), y solo el clic en una de ellas mueve la ficha.
- **Partida viva.** Dado el nivel del spike en PIE, cuando empieza, entonces se ven el Altar, seis Dominios de quince casillas con sus Nichos, Fuentes y Nodos, las conexiones entre casillas dibujadas, dos Dominios atenuados (Olvidados) y cuatro fichas en el Altar; los dummies mueven solos cada pocos segundos y el texto cambia de asiento activo.
- **Reloj.** Dado el turno del asiento 0, cuando no se hace clic en 30 s, entonces el texto muestra el agotamiento, la ficha no se mueve y el turno pasa.
- **Sellos.** Dado el Warden del spike sellando, cuando pasan unos minutos, entonces nunca hay más de 3 casillas selladas visibles y ninguna es Altar, Nicho ni Fuente.
- **Build.** Dada la build empaquetada en Windows, cuando se entra al nivel, entonces la partida arranca con el tablero de los datos (no falla por archivo no encontrado).
- **Menús.** Dado el menú de UGT, cuando se está en él, entonces no hay tablero ni anfitrión de partida.

## Notas para quien lo implemente

- Nombres del glosario: anfitrión del Warden `WardenHost`, partida `Match`, casilla `Tile`, asiento `Seat`, decisor `Decider`, intención `Intent`. Los nombres de los actores de presentación, del anfitrión de partida y del campo de posición se proponen en el plan y en el PR.
- El módulo `Soulbound` no contiene reglas: nada de calcular alcanzables, comprobar relojes ni decidir por los dummies fuera de lo que ya hace el motor. Si algo parece necesitar una regla nueva, es un `TODO(R-XXX-n)` y una pregunta en el PR.
- El anfitrión de partida es el único que escribe en el estado; la presentación lee y redibuja cuando el delegado avisa (o a intervalo corto si es más simple; dilo en el PR).
- Reloj: el tiempo de partida es el tiempo del mundo desde el inicio de la partida; el anfitrión guarda ese origen. Sin pausa en esta story.
- Los dummies existentes (`FDummyDecider`) sirven tal cual; su generador es propio, no el de la partida (como en S-003).
- Se verá una ficha entrar en una casilla sellada cuando el Sello llegue entre su tirada y su confirmación: es el hueco conocido de R-TUR-11 (story del Warden), no un fallo de esta story.
- El mapa `L_SpikeOnline` y sus Blueprints no se tocan. Si algo exigiera un asset, es una pregunta en el PR, no un cambio.
- Tests en el módulo de juego: solo si no necesitan mundo; los criterios manuales se comprueban a mano y se documentan con capturas en el PR. El del cargador va en `SoulboundCore`.
- Empaquetado: comprobar con `Scripts\build.ps1 -Que package` que los JSON están en el pak y la partida arranca en la build. Borrar `Packaged\Windows\Soulbound\Saved` antes de subir a la Deck.

## Fuera de esta story

- Cámara sentada, componente de selección, HUD, objetos diegéticos, foco de evento (GDD 07): stories de interfaz.
- Arte, materiales propios, sonido, frases del Warden con voz.
- Replicación del estado a clientes y partida en red con esta vista: story de multijugador. Esta vista funciona en el servidor de escucha y en PIE con un jugador.
- Resolución de casillas (Fuentes, Nichos, Nodos), carrera, runas: sus stories.
