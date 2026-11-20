# Multijugador y bots

> **Estado en la entrega del TFM (noviembre de 2026)**: Están implementadas la interfaz de decisión (`IDecider`) con dummies, las intenciones validadas por el motor y la simulación sin mundo, determinista por semilla, que se lanza como tests (S-003, S-016 a S-018). Los asientos libres los llevan dummies de carrera, no el bot básico. Pasan a la Fase 2: la replicación de la partida, las caídas y la reconexión, la ventana de gracia, el bot básico, la grabación reproducible y la integración continua. La sesión por código existe como prueba técnica (crear, unirse y salir), sin partida replicada. El acta de S-030 es un texto legible, no una grabación.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (21/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-21
> **Fase**: 1
> **Pilares que sirve**: 2 (cuatro asientos siempre vivos), 3 (una sola verdad: la del servidor)
> **Fuente**: GDD v0.1 secciones 8.1 y 8.3; plugin online propio del autor (EOS, sesión por código, el creador es el servidor); decisiones del autor del 21/09/2026

## Resumen

Define cómo se juega en red y cómo se juega solo con las mismas reglas: un servidor de escucha en el PC del anfitrión con toda la autoridad, clientes que solo envían intenciones, cuatro asientos siempre ocupados por un decisor (humano, bot o dummy), qué pasa cuando alguien se cae o se va, y cómo la misma partida se ejecuta sin interfaz para simular miles de veces. Las salas, el farol y las puertas están en el GDD 07; aquí va lo que hay detrás.

## Fantasía del jugador

Tú no notas nada de esto. Entras con un código, juegas, y si tu amigo se queda sin internet a mitad, su Eco sigue jugando "solo" y el Warden se ríe de él cuando vuelve. Si juegas solo, la mesa está igual de llena.

## Reglas

### Red y autoridad

- **R-RED-1.** Topología: servidor de escucha. Quien crea la partida es el servidor y a la vez el primer jugador. No hay servidores dedicados en ninguna fase prevista. La sesión se crea, publica y une con el plugin online del autor (EOS, código de sesión), que es una dependencia externa: no se reescribe ni se amplía en el TFM.
- **R-RED-2.** Toda la verdad está en el servidor: el estado de partida (structs del motor), el motor de reglas, el cerebro del Warden y su anfitrión (`UWorldSubsystem`, R-WAR-1), los relojes, y todo el azar (dados, trilero, sorteos del Warden, semillas). Un cliente nunca ejecuta reglas ni genera azar que cuente.
- **R-RED-3.** Los clientes envían **intenciones**: el resultado de un componente de selección (índice de la opción elegida) y el resultado del minijuego (las precisiones medidas en el cliente, R-MIN-13); las acciones locales que no afectan a la partida (girar el tablero, abrir paneles) no se envían. El servidor valida cada intención contra el estado actual (¿es su turno?, ¿esa opción está entre las válidas?, ¿la elección sigue abierta?) y la descarta si no es válida, sin castigo: puede ser latencia.
- **R-RED-4.** El estado replicado a cada cliente es exactamente lo que ese cliente puede saber (R-HUD-6): número de runas de todos, tipos solo propios o revelados, estados visibles, posiciones, relojes, registro público. No se replica insolencia ni tipos ocultos ni la semilla ni las decisiones futuras del Warden.
- **R-RED-5.** Relojes: todos los relojes (partida, turno, elección, Oráculo, votación) corren en el servidor. El cliente los muestra con la corrección de latencia de Unreal. Cuando un reloj se agota lo decide el servidor, con una **ventana de gracia** en datos (valor inicial 300 ms) para intenciones que llegan justo tarde por latencia: una intención recibida dentro de la ventana se acepta.
- **R-RED-6.** Audio y presentación se disparan por identificadores replicados (id de frase del Warden, id de evento, id de foco), nunca por datos de audio o de escena: cada cliente tiene los mismos assets y reproduce localmente. Un cliente con un asset ausente muestra el subtítulo igualmente.
- **R-RED-7.** Jugar solo es la misma topología: servidor de escucha sin sala publicada (farol apagado, R-FLU-2) y tres asientos con bot. No hay una ruta de código distinta para solitario.
- **R-RED-8.** Anti-trampa en Fase 1: solo lo que da la autoridad del servidor y R-RED-4. Nada más.

### Asientos y decisores

- **R-DEC-1.** Una partida tiene siempre cuatro asientos, numerados por el Trono. Cada asiento tiene en todo momento un **decisor**: humano, bot o dummy. El motor solo habla con el decisor de cada asiento y no distingue cuál es.
- **R-DEC-2.** La interfaz de decisión tiene una función por tipo de elección del componente de selección: destino; acción contra ocupante y objetivo; señalar uno o dos; opción de dilema; esfera del trilero; tipo de runa; qué runa perder; runa a salvo (Koschei); rechazar el Oráculo (Tifón); apuesta; voto; uno o dos dados; repetir tirada; usar Desvío; salto planar; puerta; y además **resolver minijuego** (presentación y dificultad a superado o fallado). Cada función recibe las opciones válidas y el tiempo, y devuelve una opción o "agotado".
- **R-DEC-3.** Decisor humano: abre el componente de selección en su cliente y devuelve lo confirmado o agotado; en el minijuego, el humano lo juega, el cliente mide sus precisiones y las envía, y el servidor comprueba que son plausibles (R-MIN-13).
- **R-DEC-4.** Decisor bot: calcula en el servidor y devuelve al momento, con un retardo en datos (valor inicial 1 a 2 s al azar) para que la mesa lo vea "pensar". Nunca devuelve agotado: los bots nunca son insolentes (R-INS-1).
- **R-DEC-5.** Decisor dummy: elige al azar entre las opciones válidas, sin retardo. Solo para pruebas y simulación. Variante "dummy insolente" en datos: devuelve agotado con una probabilidad, para probar insolencia y Furia.
- **R-DEC-6.** Cambiar el decisor de un asiento es una operación del servidor que no toca el estado de partida: es lo que ocurre en una caída (R-CAI) y en una reconexión.

### El bot básico

- **R-BOT-1.** Hay un solo bot en Fase 1, el básico, con sus parámetros en datos. Niveles de dificultad y personalidades (v0.1 8.3.2) son implementaciones futuras del decisor, Fase 2.
- **R-BOT-2.** Movimiento, por prioridades, la primera que aplique: (1) si tengo seis tipos y el Altar es alcanzable, al Altar; (2) si me falta un tipo y una Fuente con reserva de ese tipo es alcanzable, a ella; (3) si puedo perjudicar al líder acabando en su casilla con una acción disponible (duelo, Expulsión, robo si soy Eco) o desde un Nodo de Duelo alcanzable, allí; (4) si no, el destino que más acerque a la Fuente más cercana de un tipo que no tengo; como Eco confinado, al jugador con más runas alcanzable o, si no, hacia mi Fuente. Entre destinos equivalentes, al azar. En la carrera: al Nicho libre más cercano; si hay empate, al que menos rivales tengan más cerca.
- **R-BOT-3.** Acciones contra ocupantes: si hay líder en la casilla, contra él; si no, contra quien más runas tenga; con Duelo y Expulsión disponibles, Expulsión si el objetivo es aspirante o está a un turno del Altar, si no, Duelo. Nodo de Duelo: reta al líder si tiene runas.
- **R-BOT-4.** Elecciones del Warden: dilema, la opción con mayor beneficio propio según una tabla de valor en datos por efecto (turno extra, runa regalada positivos; turno perdido, perder runa, Vacío negativos, ponderados por gravedad), y a igualdad la que más perjudique al líder; señalar y votar castigos, al líder (con empate, al azar entre líderes; nunca a sí mismo); votar favores, a quien no sea líder ni él mismo; apostar, al participante con mayor habilidad conocida (los bots conocen la habilidad de los bots y suponen 50 % a los humanos); qué runa perder, un duplicado si lo tiene, si no la que da el poder que menos usa (tabla en datos); tipo de runa regalada, el que le falta más lejano; Sello contra rival, al líder.
- **R-BOT-5.** Otras: Celeridad, dos dados salvo que un dado llegue exacto a un destino de prioridad 1 o 2; repetir tirada (Apep), si la tirada no alcanza ningún destino de prioridad 1 a 3; Desvío, si tiene duplicado de esa runa o han pasado dos turnos en el Vacío; salto planar, si el Nodo de llegada acerca a su objetivo; esfera del trilero, al azar con probabilidad en datos de acertar la que quiere (valor inicial 40 %); puerta, la primera libre tras todos los humanos (R-FLU-3).
- **R-BOT-6.** Minijuego: el bot no lo juega; lo resuelve con una probabilidad de éxito por dificultad en datos (valores iniciales: fácil 75 %, normal 50 %, difícil 30 %; desafío final 25 %) y la ventaja de Shuten-dōji se aplica como bonificación en datos.
- **R-BOT-7.** El Warden trata a los bots como a cualquier jugador: golpes, dilemas, votaciones, comentarios. Solo la insolencia no aplica.

### Caídas, ausencias y reconexión

- **R-CAI-1.** Cuando un cliente que no es el anfitrión pierde la conexión o abandona, su asiento pasa a decisor bot **en el acto**, con todo su estado (Elegido, runas, posición, marcas, Vacío, turnos pendientes) y resolviendo lo que tuviera abierto. La partida no se detiene. El Warden lo comenta. Su retrato lo indica.
- **R-CAI-2.** Si vuelve con el mismo código y la misma identidad EOS mientras dura la partida, recupera su asiento: el decisor vuelve a humano, recibe el estado completo por replicación y la crónica, y **asume todo lo que hizo el bot**. El Warden lo comenta. Esto es **opcional de Fase 1**, condicionado al spike: si el plugin lo permite en menos de un día de trabajo, entra; si no, Fase 2 y el bot se queda hasta el final.
- **R-CAI-3.** Si cae el anfitrión, la partida termina para todos con victoria del Warden y una burla (cinemática corta), y los clientes vuelven al Umbral con la explicación. La migración de anfitrión es Fase 2.
- **R-CAI-4.** Un jugador conectado pero ausente no recibe ningún trato especial: los relojes, R-DIR-4 y la insolencia lo cubren. No hay "expulsar por inactividad" en Fase 1.
- **R-CAI-5.** Abandonar desde el menú de pausa es una caída voluntaria: mismo resultado que R-CAI-1 (o R-CAI-3 si es el anfitrión, con aviso previo de que la partida terminará para todos).
- **R-CAI-6.** En las barcas no hay bots nunca: los asientos vacíos no tienen barca y a ellos puede entrar alguien con el código hasta el último momento, incluido el momento de gracia tras pulsar "iniciar partida" (R-FLU-3). Si se va un invitado, su barca desaparece y su hueco vuelve a estar libre; si se va el anfitrión, la sala se cierra y los demás vuelven al Umbral. Los bots se crean **al terminar el momento de gracia**, uno por hueco libre, cada uno en su propia barca, y eligen puerta después de los humanos.

### Simulación y pruebas

- **R-SIM-1.** El motor de reglas, el cerebro del Warden y los decisores bot y dummy se ejecutan sin mundo, sin red y sin interfaz, en un bucle que avanza el reloj de partida de forma virtual (no en tiempo real). Una partida simulada de 40 minutos de juego debe ejecutarse en menos de un segundo de reloj real en un PC de desarrollo (objetivo, a medir en el spike).
- **R-SIM-2.** Una simulación se define por: semilla, tablero (datos), configuración (datos), cuatro decisores (bot o dummy con parámetros) y, opcionalmente, un guion de decisiones fijas para tests concretos. Con la misma definición, el resultado es idéntico (R-WAR-10).
- **R-SIM-3.** Cada simulación produce el registro completo de eventos y las métricas calculables sin humanos: duración, ganador y tipo, intervenciones por acto, turnos cortados, duelos, Furias, veces en el Vacío, Ecos que ganan, tramos sin golpe ni escena, reparto de objetivos. Las métricas M1 a M4 que necesitan humanos se miden en playtest con el mismo registro.
- **R-SIM-4.** Los criterios de aceptación "en simulación" de todos los GDD se implementan como tests de automatización de Unreal que ejecutan simulaciones y comprueban los rangos, y se lanzan por línea de comandos sin abrir el editor, en local y en la integración continua.
- **R-SIM-5.** Una partida en red de verdad debe poder grabarse (registro más semilla más intenciones con tiempos) y reproducirse en simulación para depurar.

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Sesión | Local, Publicada, Cerrada | R-FLU-2, farol | Farol o fin | Solo Publicada admite entradas por código |
| Asiento | Humano conectado, Bot por caída, Bot de relleno, Dummy | R-DEC, R-CAI | R-CAI-2 | El motor no distingue |
| Cliente | Conectado, Reconectando, Fuera | Red | Red | R-CAI-1 y 2 |
| Partida | En red, Solitaria, Simulada | Cómo se ha creado | Fin | Mismo motor; solo cambian decisores e interfaz |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Todos los sistemas de reglas | Multijugador depende de ellos | Los ejecuta en el servidor y replica su estado |
| Warden | Mutua | El anfitrión del Warden vive en el servidor; comentarios de caída y vuelta; bots como jugadores |
| Interfaz | Interfaz depende de este | Barca, farol, puertas, retratos con estado de conexión, HUD desde el estado replicado |
| Turnos | Multijugador depende de este | Relojes en servidor; ventana de gracia |
| Castigos | Multijugador depende de este | R-DIR-4 para agotados; bots nunca agotados |
| Minijuego | Mutua | Precisiones medidas en el cliente y validadas por plausibilidad en el servidor (R-MIN-13); resolución por habilidad para bots |
| Plugin online (externo) | Este depende del plugin | Crear, publicar, unir y cerrar sesión por código; identidad EOS |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| Tiempo real por partida simulada | segundos | objetivo < 1 | spike | R-SIM-1 |
| Intenciones descartadas por latencia | porcentaje | objetivo < 1 % | red | Si sube, ampliar ventana de gracia |
| Victorias de bot contra humanos | porcentaje | a medir | playtest | Si los bots ganan mucho más o mucho menos que los humanos, ajustar habilidad y prioridades |
| Retardo del bot | segundos | 1-2 | datos | Sensación de mesa |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| Intención que llega 200 ms después de agotarse el reloj | Se acepta (ventana de gracia) | R-RED-5 |
| Intención para una opción que ya no es válida | Se descarta sin castigo; el cliente ve el estado actual | R-RED-3 |
| Dos intenciones del mismo cliente para la misma elección | La primera válida manda; la segunda se descarta | R-RED-3 |
| Caída en mitad del propio minijuego | El bot resuelve el minijuego con su habilidad | R-CAI-1, R-BOT-6 |
| Caída en mitad de una votación o apuesta | El bot vota o apuesta | R-CAI-1 |
| Caída del anfitrión en la cinemática final | La partida ya ha terminado; se muestra el resultado igualmente en cada cliente | R-CAI-3 |
| Reconexión de alguien cuyo asiento el bot llevó al Vacío | Vuelve dentro del Vacío, con la cuenta de turnos que lleve | R-CAI-2 |
| Reconexión con otra identidad EOS | No es él: no entra en partida iniciada | R-CAI-2 |
| Anfitrión sin internet desde el principio | Juega solo con el farol apagado | R-RED-7 |
| Anfitrión pierde internet con invitados dentro | Para los invitados es R-CAI-3; el anfitrión sigue con bots (su servidor no ha caído) | R-CAI-3, R-RED-7 |
| Cliente sin el audio de una frase | Subtítulo igual | R-RED-6 |
| Bot con ninguna opción válida en una elección | No ocurre: el motor solo abre elecciones con opciones válidas (R-DIR-3) | |
| Simulación con cuatro dummies | Válida; sirve para probar reglas, no equilibrio | R-SIM-2 |
| Test que necesita una decisión concreta | Guion de decisiones fijas para ese asiento | R-SIM-2 |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Ventana de gracia | 300 ms | 150-500 | Relojes "blandos" | Agotados injustos |
| Retardo del bot | 1-2 s | 0,5-3 | Mesa más lenta | Bots "robóticos" |
| Habilidad del bot por dificultad | 75 / 50 / 30 %; final 25 % | datos | Bots imbatibles | Bots inútiles |
| Probabilidad de acertar esfera | 40 % | 25-60 | | |
| Tabla de valor de efectos para dilemas | datos | | | |
| Probabilidad de insolencia del dummy insolente | 20 % | 0-100 | | Solo tests |

## Presentación en pantalla plana

| Evento o información | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Jugador caído | Su retrato | Icono de desconexión; su avatar sigue sentado, el bot mueve su ficha | | Comentario de caída |
| Jugador que vuelve | Su retrato | Icono desaparece | | Comentario de vuelta |
| Bot de relleno | Su barca y su retrato | Su barca aparece al terminar el momento de gracia, con un Eco a bordo; marca de bot en el retrato | | Ninguna distinción en partida |
| Anfitrión caído | Cinemática corta | El Warden gana; vuelta al Umbral con texto | Carcajada | Burla final |
| Latencia alta | Nada | Sin indicador en Fase 1 | | |

## Contribución a los pilares y a las métricas

- **Pilar 2**: cuatro asientos siempre vivos; ninguna caída vacía la mesa.
- **Pilar 3**: una sola verdad, la del servidor; lo que ves es lo que hay.
- Métricas: la simulación es lo que hace medibles todos los rangos "en simulación" de los GDD anteriores.

## Criterios de aceptación

- [ ] **CA-RED-1** (R-RED-2, 3). Dado un cliente que envía una intención inválida (fuera de turno, opción no válida, elección cerrada), entonces el servidor la descarta, el estado no cambia y no se registra insolencia.
- [ ] **CA-RED-2** (R-RED-4). Dado un cliente, cuando se inspecciona lo que recibe, entonces no contiene tipos de runa ocultos de otros, insolencia, semillas ni decisiones futuras del Warden.
- [ ] **CA-RED-3** (R-RED-5). Dada una intención recibida 200 ms después de agotarse el reloj, entonces se acepta; 400 ms después, entonces se aplica el agotado.
- [ ] **CA-RED-4** (R-RED-7). Dada una partida en solitario, entonces el mismo código de servidor ejecuta la partida con tres bots, sin sala publicada y sin red.
- [ ] **CA-DEC-1** (R-DEC-1, 2, 6, R-SEL-6). Dado un asiento, cuando se cambia su decisor de humano a bot en mitad de una elección abierta, entonces la elección la resuelve el bot y el estado de partida no cambia por el cambio de decisor.
- [ ] **CA-DEC-2** (R-DEC-4, 5). Dado un bot, entonces nunca devuelve agotado; dado un dummy insolente con probabilidad p, entonces devuelve agotado con frecuencia p en N elecciones.
- [ ] **CA-BOT-1** (R-BOT-2). Dado un bot con seis tipos y el Altar alcanzable, entonces elige el Altar; con un tipo que le falta y su Fuente alcanzable, entonces la Fuente; con el líder en una casilla alcanzable y una acción disponible, entonces esa casilla.
- [ ] **CA-BOT-2** (R-BOT-4). Dado un dilema "dos turnos extra para ti o rival señalado a casa", entonces el bot elige según la tabla de valor; dada una votación de castigo, entonces vota al líder y nunca a sí mismo.
- [ ] **CA-BOT-3** (R-BOT-6). Dado un bot en un duelo de dificultad normal, cuando se resuelve N veces, entonces supera el minijuego con frecuencia dentro de más o menos 5 puntos del 50 %.
- [ ] **CA-CAI-1** (R-CAI-1). Dado un cliente que se desconecta durante su turno, entonces su asiento pasa a bot en el mismo instante, el turno se completa y ningún reloj se ha detenido.
- [ ] **CA-CAI-2** (R-CAI-2) (si entra). Dado un jugador caído que vuelve con su identidad, entonces recupera su asiento con el estado que tenga el bot en ese momento, y el registro muestra las acciones del bot como suyas.
- [ ] **CA-CAI-3** (R-CAI-3). Dado el anfitrión que se desconecta, entonces todos los clientes terminan la partida con victoria del Warden y vuelven al Umbral.
- [ ] **CA-SIM-1** (R-SIM-1, 2). Dada una definición de simulación, cuando se ejecuta dos veces, entonces el registro es idéntico; y una partida completa tarda menos del objetivo de datos en reloj real.
- [ ] **CA-SIM-2** (R-SIM-4). Dado el conjunto de tests, cuando se lanza por línea de comandos sin editor, entonces ejecuta las simulaciones de todos los criterios "en simulación" de los GDD 01 a 06 y falla si algún rango no se cumple.
- [ ] **CA-SIM-3** (R-SIM-5). Dada una partida en red grabada, cuando se reproduce en simulación, entonces el registro coincide.
- [ ] **CA-RED-5** (R-RED-1). Dada una partida creada por un cliente, entonces ese proceso tiene la autoridad (motor, Warden, relojes, azar) y ningún otro cliente puede ejecutar una regla ni alterar el estado replicado.
- [ ] **CA-RED-6** (R-RED-6). Dado un cliente al que le falta el audio de una frase, cuando se reproduce por id, entonces muestra el subtítulo, no falla y el servidor no se entera.
- [ ] **CA-CAI-4** (R-CAI-4). Dado un jugador conectado que no responde durante varios turnos, entonces no es expulsado ni sustituido: sus turnos terminan por reloj con insolencia y sus elecciones se resuelven por R-DIR-4.
- [ ] **CA-BOT-4** (R-BOT-3). Dado un bot que termina en una casilla con el líder y otro jugador, entonces actúa contra el líder; con Duelo y Expulsión disponibles y el líder aspirante, entonces elige Expulsión; sin aspirante, entonces Duelo.
- [ ] **CA-BOT-5** (R-BOT-5). Dado un bot con Celeridad y un destino de prioridad 1 o 2 a distancia exacta de un dado, entonces elige un dado; dado Apep con una tirada que no alcanza ningún destino de prioridad 1 a 3, entonces repite; dado un bot en el Vacío con duplicado de la runa de Desvío, entonces usa Desvío; dado un salto planar que acerca a su objetivo, entonces salta; dada la elección de puerta, entonces elige la primera libre.
- [ ] Sin valores fijos en el código: ventana de gracia, retardos, habilidades, tablas de valor y probabilidades salen de datos.

## Nombres nuevos para el glosario

Decisor (`Decider`, interfaz de decisión), Decisor humano (`HumanDecider`), Bot básico (`BasicBotDecider`, sustituye a `BotPlayer`), Dummy (`DummyDecider`, sustituye a `DummyPlayer`), Dummy insolente (`InsolentDummy`), Intención (`Intent`), Ventana de gracia (`GraceWindow`), Asiento (`Seat`), Simulación (`Simulation`), Grabación de partida (`MatchRecording`), Habilidad de minijuego (`MinigameSkill`).

## Fuera de alcance de esta fase

- Servidores dedicados: en ninguna fase prevista.
- Migración de anfitrión: Fase 2.
- Reconexión: opcional de Fase 1 según el spike; si no, Fase 2.
- Voz (chat de voz, audio espacial, filtros por Elegido y desde el Vacío, v0.1 8.2.2): Fase 2, ampliación del plugin.
- Avatares con IK y sincronización labial (v0.1 8.2.1): Fase 3.
- Niveles de dificultad y personalidades de bot (v0.1 8.3.2): Fase 2, como nuevos decisores.
- Anti-trampa más allá de la autoridad del servidor: fuera de todas las fases previstas.
- Lista pública de partidas y emparejamiento: no previsto; siempre por código.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| Reconexión con el plugin actual: ¿menos de un día? | Autor | Spike | Decide R-CAI-2 |
| Tiempo real por partida simulada | Spike | Spike | Objetivo 1 s |
| Tabla de valor de efectos para los dilemas del bot | Autor | Sprint de bots | Primera versión: gravedad como valor |
| Habilidad supuesta de los humanos por los bots al apostar | Autor | Sprint de bots | 50 % provisional |
