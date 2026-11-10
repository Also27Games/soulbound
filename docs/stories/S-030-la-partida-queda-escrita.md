# S-030. La partida queda escrita: el acta de cada partida en un archivo

**Sistema**: registro de partida (glosario "Registro de eventos", R-TUR-15; "Postpartida": "métricas y registro exportados a archivo"). **Módulos**: `Soulbound` (anfitrión de partida y anfitrión del Warden; una función pura de texto en `Presentation/`). `SoulboundCore` no cambia: el registro de eventos ya es la verdad de la partida. **Rama base**: `main`, con S-027 fusionada. **Tamaño**: pequeño. Independiente de S-028 y S-029; conviene que esté en la build de probadores del 17/11/2026.

## Contexto

Un probador jugó cinco partidas el 8/11/2026 y mandó los logs del juego (`Saved/Logs`). Con ellos se supo cuánto duró cada partida, en qué acto la dejó, cuántas veces pulsó mientras el Warden hablaba y que "Volver al menú" lo echó al launcher. No se supo nada de la mesa: ni a qué Elegido se vinculó, ni qué runas ganó o perdió, ni quién ganó. El log del motor solo recoge lo que el anfitrión decide contar (prólogo, esperas del Warden, entradas rechazadas, anillo) y el cerebro del Warden (una línea por evaluación). La crónica que el jugador ve en pantalla se calcula al vuelo a partir del registro de eventos y se pierde al cerrar.

Decidido por el autor el 9/11/2026: **cada partida deja un acta**, un archivo de texto propio, escrito mientras se juega, que permita reconstruir la partida entera leyéndolo: cada turno, cada tirada, cada casilla pisada, cada vínculo, cada runa, cada minijuego, cada duelo, cada decisión y cada frase del Warden, cada entrada del humano con su resultado, y al final quién ganó y cómo quedó cada asiento. Sin pantalla nueva, sin subir nada a ningún sitio, sin cambiar lo que ve el jugador.

## Reglas a implementar

### Texto del acta (`Soulbound`, `Presentation/MatchRecord.h`, nombre propuesto; glosario propuesto "Acta de partida", `MatchRecord`)

Funciones puras, sin actor, sin disco y sin mundo, probadas sin editor:

- **R-ACT-1 (precisión). Una línea por evento, ninguno se salta.** `MatchRecordLine(State, Index, RingSeat)` devuelve la línea del evento `Index` del registro: marca de tiempo del reloj de partida como `[mm:ss.d]`, el índice del evento, el nombre del tipo (`EMatchEventType`, tal cual en inglés para poder filtrar), los cuatro campos del evento (`asiento`, `otro`, `valor`, `detalle`, con `-` en los `INDEX_NONE`), y tras `|` la frase en español de `DescribeEvent`. A diferencia del panel de crónica, aquí van **todos** los eventos, también `TileEntered` y `WardenTriggered`, y nunca se funden dos en una línea. Ejemplo: `[01:23.4] #57 Rolled asiento=2 otro=- valor=7 detalle=- | El asiento 2 saca un 7.`
- **R-ACT-2 (precisión). La frase del Warden, con su texto.** En un `WardenSpoke` la frase va completa tras `|`: el prefijo de `WardenLinePrefix` y el texto de `SpokenText` sin las acotaciones de audio. Si la entrada no existe en la tabla, la línea lo dice (`frase desconocida`), no queda vacía.
- **R-ACT-3 (precisión). Cabecera.** `MatchRecordHeader(State, Seed, HumanSeat, BuildText, StartedAt)` devuelve las líneas del principio: fecha y hora local de inicio, texto de la build (lo que le pase el anfitrión: `FApp::GetBuildVersion()` y la configuración, Development o Shipping), semilla, número de casillas y ruta del tablero, asiento del humano y número de asientos, duración prevista (`matchEndSeconds` de los parámetros del Warden), y una línea por Dominio con el nombre de su Elegido.
- **R-ACT-4 (precisión). Cierre.** `MatchRecordSummary(State)` devuelve las líneas del final: el resultado (`un jugador gana: asiento N`, `el Warden gana: nadie se vinculó` o `el Warden gana: tiempo`, según `Outcome`, `WinnerSeat` y `EndReason`), el reloj de partida y la ronda en que acaba, y una línea por asiento con su rol, su Elegido o su Dominio Olvidado, las runas por tipo y en total, su insolencia y si está en el Vacío. Si la partida **no** ha terminado (`IsMatchOver()` falso), la primera línea dice `partida abandonada` con el reloj, la ronda, la fase del turno y el asiento activo, y el resto es igual.
- **R-ACT-5 (precisión). Nombre del archivo.** `MatchRecordFileName(StartedAt, Seed)` devuelve `partida-AAAAMMDD-HHMMSS-semilla-N.log`, para que dos partidas no se pisen y la semilla se lea en el nombre.

### Anfitrión (`Soulbound`, `Match/MatchHostSubsystem`, `Warden/WardenHostSubsystem`)

- **R-ACT-6 (precisión). Dónde y cuándo se escribe.** Al iniciarse la partida (`Partida iniciada`), el anfitrión crea el archivo en `<Saved>/Partidas/` (`FPaths::ProjectSavedDir()`, para que valga en el editor y en la build empaquetada) con la cabecera. Cada vez que el registro de eventos crece, añade las líneas nuevas en el acto, en orden, con la misma lectura incremental que ya hacen la crónica y la espera del Warden. Cada escritura se vuelca al disco (`FFileHelper::SaveStringToFile` con `FILEWRITE_Append`, o un `FArchive` con `Flush`): si el jugador cierra la ventana a mitad, el acta llega hasta el último evento. Al terminar la partida, o al destruirse el subsistema con la partida a medias, escribe el cierre.
- **R-ACT-7 (precisión). Lo que no está en el registro de eventos también va al acta.** Todo lo que hoy el anfitrión escribe en `LogSoulboundMatch` durante una partida va además al acta, con la marca de tiempo y el prefijo `host`: telón y prólogo, el Warden habla y termina (con lo que esperó la mesa y los plazos desplazados), `Rechazado (...)`, anillo abierto, cada pulso y anillo cerrado, ediciones de depuración, paso a paso. La línea de cada evaluación del cerebro (`LogSoulboundWarden`, `Warden (eval ...)`) también, con el prefijo `warden`. El log del motor sigue igual: el acta es un destino más, no un sustituto.
- **R-ACT-8 (precisión). Las entradas del humano, con su resultado.** Cada entrada del asiento humano que llega al anfitrión deja una línea `humano`: destino (`SubmitHumanIntent`, con la casilla), avance de ficha no (es de la vista, uno por casilla, y ya deja `TileEntered`), elección (`SubmitHumanChoice`, tipo y opción), consulta y rechazo del Oráculo, voto (asiento votado) y pulsación del anillo. Cada línea dice lo que devolvió el motor (`EIntentResult` en texto) o que la mesa esperaba al Warden. Así se distingue "no pulsó" de "pulsó y no valió".
- **R-ACT-10 (precisión). El anillo, medido.** Cada pulsación del anillo deja su línea, cuente o no: si cuenta, el pulso y el desfase en segundos respecto a su coincidencia, con signo (negativo, antes); si no cuenta, por qué (en el anuncio, entre pulsos, prueba terminada). La línea de `anillo cerrado` añade los fotogramas del motor por segundo durante la prueba (fotogramas contados entre abrir y cerrar, entre los segundos reales). Motivo: el 9 y 10/11/2026 una sesión con anillos imposibles solo se pudo leer cuando se midieron estas dos cosas a mano; a 3 fotogramas por segundo no se puede acertar, y eso tiene que verse en el acta de un probador. El cálculo del desfase es una función pura (`Presentation/MatchRecord.h` o `Match/RingTrialRun.h`), con su test.
- **R-ACT-9 (precisión). Se puede apagar.** `bWriteMatchRecord` (Config, `DefaultGame.ini`, valor inicial `true`) y `MatchRecordDirectory` (Config, valor inicial `Partidas`, relativo a `Saved`). Si no se puede crear el archivo, un `Warning` en el log y la partida sigue: el acta nunca para una partida.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/MatchRecordTest.cpp`, nombres `Soulbound.Game.Presentation.Acta.<QueSeComprueba>`):

- **CA-ACT-1** (línea de evento). Un `Rolled` del asiento 2 con valor 7 a los 83,4 s produce una línea con `[01:23.4]`, su índice, `Rolled`, `asiento=2`, `valor=7`, `otro=-`, `detalle=-` y la frase de `DescribeEvent`. Un `TileEntered` y un `WardenTriggered` también producen línea, no vacía.
- **CA-ACT-2** (frase). Un `WardenSpoke` a la mesa produce una línea con `Warden:` y el texto de la variante sin corchetes; uno que apunta a una entrada inexistente dice `frase desconocida`.
- **CA-ACT-3** (cabecera). La cabecera contiene la semilla, el asiento humano, el número de casillas, la duración prevista y una línea por Dominio con el nombre de su Elegido, en orden de Dominio.
- **CA-ACT-4** (cierre). Con `Outcome` PlayerVictory y `WinnerSeat` 1, el cierre empieza por `un jugador gana: asiento 1`; con WardenVictory y `EndReason` TimeOut, por `el Warden gana: tiempo`; con la partida sin terminar, por `partida abandonada` con el reloj. En los tres casos hay una línea por asiento con rol, runas por tipo, total e insolencia.
- **CA-ACT-5** (nombre). Para el 8/11/2026 a las 19:37:06 y la semilla 742711968, el nombre es `partida-20261108-193706-semilla-742711968.log`.
- **CA-ACT-6** (nada se salta). Un registro con doce eventos de tipos distintos produce doce líneas, en orden, con índices consecutivos de 0 a 11.
- **CA-ACT-7** (anillo). Una pulsación 0,031 s antes de la coincidencia del pulso 3 produce una línea con `pulso 3` y `-0.031`; una en el anuncio, una línea que dice `anuncio` y no lleva desfase; una entre dos pulsos, `entre pulsos`. Con 180 fotogramas en 3,0 s, la línea de cierre dice `60.0` fotogramas por segundo.

Manuales, por el autor, en PIE y en la build empaquetada:

- Al empezar una partida aparece el archivo en `Saved/Partidas/` con la cabecera; abierto con un editor de texto mientras se juega, crece turno a turno.
- Cerrar PIE a mitad de partida: la última línea del acta es el último evento que se vio en la crónica, y el cierre dice `partida abandonada`.
- Partida completa (`SbClock 2390` y esperar): el cierre dice quién gana y cómo queda cada asiento.
- Un clic en casilla mientras el Warden habla deja una línea `humano` con "la mesa espera al Warden"; un clic en una casilla no alcanzable deja la línea con el motivo del motor.
- Una prueba con el anillo deja `anillo abierto`, un pulso por línea y `anillo cerrado`, entre el evento que la abre y el que la resuelve.
- En la build empaquetada el archivo está en `Soulbound/Saved/Partidas/` junto al ejecutable; es lo que se pedirá a los probadores que manden.

## Notas para quien lo implemente

- El registro de eventos (`FMatchState::EventLog`, `FMatchEvent`: `Type`, `ClockSeconds`, `Seat`, `OtherSeat`, `Value`, `Detail`) ya es completo; el acta no inventa datos, los escribe. Los textos salen de `Presentation/EventText.h` (`DescribeEvent`, `WardenLinePrefix`, `SpokenText`); los nombres de los Elegidos, de `State.Elects`; la duración prevista, de los parámetros del Warden cargados en el estado.
- La lectura incremental ya existe dos veces en el anfitrión (`WardenLinesScanned`) y en la vista (`ChronicleLineCount`): el acta lleva su propio contador y se escribe desde el anfitrión, no desde la vista, porque la vista no existe en un servidor sin pantalla.
- Las líneas `host` y `warden` salen de los mismos `UE_LOG` que hoy: una función del anfitrión que escribe en el log y en el acta a la vez, para no duplicar textos. El anfitrión del Warden llega al de partida por el mundo (ya lo hace para `EvaluateWarden`).
- El nombre del tipo de evento: `UEnum::GetValueAsString` sobre `EMatchEventType`, sin el prefijo del enum.
- Tests primero, en rojo. La escritura a disco y los subsistemas solo tienen criterio manual.
- Al cerrar: una línea en el README (dónde queda el acta y que es lo que se pide a los probadores), el glosario con "Acta de partida", y la nota de `docs-privado/test/` que pide los archivos de `Saved/Partidas` a P2 y P4.

## Fuera de esta story

- Exportar a JSON o a otro formato para máquinas: el acta es texto para leerla una persona y filtrarla con `grep`; el JSON se hará cuando haya quien lo consuma.
- Métricas de diversión, resúmenes o gráficas a partir de las actas: leerlas es trabajo del autor, no del juego.
- La pantalla de postpartida (glosario "Postpartida") y la crónica completa en pantalla.
- Mandar el acta a ningún servidor.
