# Glosario oficial de Soulbound

Usa estos nombres y solo estos. La columna "Código" es el identificador en inglés para clases, structs y enumeraciones. Si un concepto no está aquí, propón el nombre al autor antes de usarlo. Los documentos anteriores al 15/09/2026 pueden usar nombres antiguos; el glosario manda.

## Actores

| Español | Código | Definición |
|---|---|---|
| El Warden | `Warden` | Anfitrión y director de la partida. Entidad ancestral que se alimenta de la desesperación. Interviene, castiga, ofrece dilemas. Es la IA directora. |
| Eco Perdido | `LostEcho` | Estado inicial de todo jugador: un espectro sin poder. Quien no reclama un Nicho en la carrera se queda así toda la partida. |
| Elegido (plural: Elegidos) | `Elect` | Los seis seres oscuros a los que un Eco puede vincularse. Cada uno gobierna un Dominio, da un poder pasivo y un mapa de runa a poder. Tema: los seis terrores de seis mundos, ecos encadenados de figuras mitológicas coleccionados por el Warden. |
| Los seis Elegidos | `ElectId` | Shuten-dōji (oni, Japón), Fenrir (lobo encadenado, nórdico), Apep (serpiente del caos, Egipto), Mictlantecuhtli (señor de los muertos, azteca), Tifón (tormenta, Grecia), Koschei (el inmortal, eslavo). Nombres en el juego, afinidades y Dominios: GDD de carrera y Elegidos. |
| Jugador | `Player` | Persona o bot que controla un Eco o un Elegido. |
| Dummy | `DummyPlayer` | Jugador automático sin criterio (tira y mueve al azar). Solo para pruebas automáticas y simulación. |
| Bot | `BotPlayer` | Jugador automático con criterio, que implementa la interfaz de decisión del jugador. En Fase 1, el bot básico: va a por el objetivo más cercano y elige en los dilemas lo que le beneficia. Otros bots y niveles de dificultad se añaden como nuevas implementaciones. Rellena los huecos hasta cuatro y permite jugar solo. |

## Tablero

| Español | Código | Definición |
|---|---|---|
| La Cripta | `Crypt` | La sala donde se juega. En VR rodea al jugador; en PC es el escenario alrededor del tablero. |
| Tablero | `Board` | Grafo de casillas: el Altar en el centro y seis Dominios conectados con él y entre vecinos. |
| Casilla | `Tile` | Nodo del grafo. Tiene un tipo y conexiones. |
| Conexión | `Edge` | Enlace entre dos casillas. |
| El Altar | `Altar` | Casilla central grande. Todos los jugadores empiezan en ella. De ella parten los caminos a los seis Dominios. Con seis runas distintas, el jugador vuelve al Altar, las engarza y arranca el desafío final. Es casilla normal a efectos de tránsito y castigos. Si se exige tirada exacta para volver lo decide el GDD de tablero. |
| Trono | `Throne` | Asiento de cada jugador en la Cripta, alrededor de la mesa. No es una casilla del tablero. En el Vacío, el avatar del desterrado sigue en él, espectral. |
| Dominio | `Domain` | Una de las seis zonas del tablero, gobernada por un Elegido. Conectada con el Altar y con sus dos Dominios vecinos. Cada Dominio tiene un Nicho y una Fuente de Poder. |
| Dominio Olvidado | `ForgottenDomain` | Dominio sin dueño: corrompido por el Warden al inicio (6 menos N) o no reclamado al acabar la carrera. Nicho cerrado; en su Fuente, un Elegido debe superar el minijuego de habilidad para obtener runa y un Eco la obtiene sin él; territorio de los Ecos. Al acabar la carrera, cada Eco recibe uno propio, nunca compartido, que es su hogar. |
| Nicho | `Niche` | Casilla fija de cada Dominio donde se reclama al Elegido. Punto de reaparición y de castigo medio. |
| Fuente de Poder | `PowerSource` | Casilla fija de cada Dominio donde se obtiene su runa. |
| Nodo de Poder | `PowerNode` | Casilla especial cuyo efecto se asigna al azar al inicio: Duelo, Oráculo o Planar. Todos de elección voluntaria: solo actúan si el jugador acaba su movimiento en ellos. |
| Nodo de Duelo | `DuelNode` | Casilla desde la que quien acaba en ella puede retar a duelo a cualquier jugador del tablero, esté donde esté, sin necesitar el poder Duelo. |
| Nodo del Oráculo | `OracleNode` | Casilla en la que acabar obliga a consultar el Oráculo del Caos, con sus siete segundos. Riesgo que se elige: el Oráculo también da regalos. |
| Nodo Planar | `PlanarNode` | Casilla desde la que quien tiene Vuelo Planar puede saltar a otro Nodo Planar; la llegada se resuelve como un final de movimiento normal. Sin el poder, casilla normal. |
| Sello | `Seal` | Estado de casilla impuesto por el Warden: una casilla sellada no se pisa ni se atraviesa salvo con Paso Libre. Solo el Warden pone y quita Sellos. No se sellan el Altar, los Nichos, las Fuentes ni casillas ocupadas. |
| Despertar del tablero | `BoardAwakening` | Momento en que el primer jugador se convierte en Elegido y se activan los Nodos de Poder y las Fuentes. |

## Objetivo y poderes

| Español | Código | Definición |
|---|---|---|
| Runa de Poder | `Rune` | Uno de los seis objetos de colección. Cada Elegido tiene su propio mapa de runa a poder. |
| Brazal Arcano | `Bracer` | Inventario de runas del jugador. Estado oculto o revelado. En PC, un panel de interfaz. |
| Poder | `Power` | Capacidad que otorga una runa según el Elegido: Duelo, Celeridad, Paso Libre, Expulsión, Vuelo Planar, Desvío. |
| Duelo (poder) | `PowerDuel` | Duelo portátil: retar a un jugador con quien se comparte casilla al acabar el movimiento. El ganador del minijuego roba una runa al perdedor con el trilero. |
| Celeridad | `PowerHaste` | Elegir tirar uno o dos dados; sin él, todo jugador tira uno. Lo da la runa del propio Dominio. |
| Paso Libre | `PowerFreePassage` | Atravesar y pisar casillas selladas. |
| Expulsión | `PowerExpel` | Mandar a su hogar a un jugador con quien se comparte casilla al acabar el movimiento. Sin minijuego. |
| Vuelo Planar | `PowerPlanarFlight` | Usar los Nodos Planares. |
| Desvío | `PowerDeflection` | Al inicio del turno en el Vacío, sacrificar la runa (vuelve a su Fuente) para salir sin minijuego, reaparecer en el hogar y jugar el turno. |
| Afinidad | `Affinity` | Poder pasivo único de cada Elegido. |
| Runa de Liberación | `LiberationRune` | Para un Eco, la runa de su Dominio Olvidado asignado: mientras la tiene le da Celeridad. Misma regla que para los Elegidos: la runa de tu Dominio te da Celeridad. |
| Amuleto de Desvío | `DeflectionAmulet` | Fase 2, con otro nombre (choca con el poder Desvío). Objeto de un solo uso que anula un castigo y pasa al jugador de la izquierda. |
| Grimorio | `Grimoire` | Consulta de reglas y estado del jugador. En PC, un panel. |

## Partida

| Español | Código | Definición |
|---|---|---|
| Partida | `Match` | Una sesión completa de 40 minutos. |
| Carrera por el vínculo | `BondRace` | Primera fase, unos 5 minutos: los Ecos salen del Altar y compiten por los Nichos con un dado. |
| Caza de runas | `RuneHunt` | Fase central. |
| Carrera hacia la victoria | `VictoryRun` | Con seis runas distintas, volver al Altar, engarzarlas y superar el desafío final. |
| Acto I, II, III | `ActOne`, `ActTwo`, `ActThree` | Tramos de la partida que cambian el comportamiento del Warden: 0-5, 5-27, 27-40 minutos. |
| Turno | `Turn` | Tirar, mover, resolver casilla. Reloj de turno de 30 segundos. |
| Tirada | `Roll` | Resultado de uno o dos dados de seis caras. |
| Desafío final | `FinalChallenge` | Prueba para ganar tras engarzar las seis runas en el Altar. Presentación del minijuego de habilidad en su dificultad máxima. |
| Victoria del Warden | `WardenVictory` | Derrota colectiva al agotarse el tiempo. |

## Castigos

| Español | Código | Definición |
|---|---|---|
| Castigo | `Punishment` | Cualquier consecuencia impuesta por el Warden. Siempre con nombre, causa y víctima. |
| Efecto | `Effect` | Consecuencia atómica que el Warden puede imponer o regalar. Lista cerrada de dieciocho, con gravedad leve, media, grave o regalo. GDD de castigos. |
| Dirección | `EffectTarget` | A quién le pasa un efecto: activo, rival señalado, dos rivales señalados, líder, siguiente, todos, al azar, Ecos. Desatender al Warden nunca beneficia: un regalo ignorado se pierde, un ataque ignorado recae en quien lo ignora. |
| Líder | `Leader` | Quien tiene más tipos de runa distintos; con empate, más runas; con empate total, todos los empatados. |
| Vuelta al hogar | `SendHome` | Castigo medio: la ficha va a su hogar. El hogar no encierra: el siguiente turno se juega con normalidad. |
| Sello contra rival | `SealAgainstRival` | El jugador activo señala a un rival y el Warden sella la casilla válida que más le estorba en su camino. El jugador nunca elige casillas. |
| Prueba del Warden | `WardenTrial` | En tu siguiente turno debes superar el minijuego para poder tirar; si fallas, ese turno termina. Máximo un turno. |
| Consulta forzada | `ForcedOracle` | El Warden te obliga a consultar el Oráculo. |
| Robo decretado | `DecreedTheft` | El Warden concede a un jugador robar a otro, o a todos, con el trilero. |
| Duelo forzado | `ForcedDuel` | El Warden obliga a dos jugadores a batirse; en juego una runa o todas. |
| Runa regalada | `GiftRune` | El jugador elige un tipo entre las Fuentes con reserva y la recibe. |
| Liberación | `ReleaseVoid` | Todos los del Vacío salen a la vez a su hogar. |
| Noche de los Ecos | `EchoNight` | Decreto por el que los Ecos transitan por cualquier Dominio hasta que el Warden la termina con la Ley de Sombras. Siguen sin coger runas en Fuentes de Dominios con dueño. |
| El Vacío | `TheVoid` | Castigo máximo: destierro. Solo se llega por el Warden (decreto, Oráculo o Furia); no hay casilla del Vacío en el tablero. Se sale superando el escape, sin dados; nunca más de cuatro turnos. Para el desterrado la Cripta se desvanece y flota en el negro con la mesa como una isla lejana y atenuada que sigue viendo; puede hablar y apostar. Para la mesa, su avatar se vuelve espectral. Se reaparece en el hogar. |
| Escape del Vacío | `VoidEscape` | Prueba para volver: el minijuego de habilidad en cada turno, en difícil el primero, normal el segundo y fácil el tercero; al cuarto turno se sale sin jugar. Mictlantecuhtli empieza en fácil y sale como máximo al segundo. |
| Ley de Sombras | `ShadowLaw` | Decreto del Warden que manda a cada Eco al Nicho cerrado de su Dominio asignado. Termina la Noche de los Ecos. |
| Hogar | `HomeTile` | Casilla a la que un jugador vuelve tras el Vacío o un castigo de "vuelta al principio": el Nicho reclamado para un Elegido, el Nicho cerrado de su Dominio asignado para un Eco, el Altar para todos durante la carrera. |
| Marcado | `Marked` | Estado del jugador perseguido: quien le robe, le expulse, le gane un duelo o le mande a casa o al Vacío gana un turno extra. Dura unas rondas o hasta que alguien lo cobra. Puede haber varios Marcados. |
| Insolencia | `Insolence` | Contador oculto de 0 a 5 por jugador. Sube uno cada vez que no hace algo que le tocaba: agotar el reloj de turno, no atender al Oráculo, no señalar, no elegir en un dilema, desaprovechar un regalo, no votar. Cada subida trae un castigo inmediato sorteado por gravedad según el nivel; al quinto, la Furia. Los bots nunca son insolentes. Fase 1. |
| Furia del Warden | `WardenFury` | Cinemática al llegar a insolencia 5. Castigo fijo: el Vacío de la Furia. Después el contador vuelve a 0. |
| Vacío de la Furia | `FuryVoid` | El Vacío normal, pero dentro no funcionan ni Desvío ni Liberación: el jugador puede intentarlos y el Warden se niega con frase (la runa no se consume). |
| Minijuego de habilidad | `SkillMinigame` | Una única mecánica corta de timing o reacción con tres presentaciones: duelo, escape del Vacío y desafío final. |
| Trilero | `ShellGame` | Interacción de robo: las runas de la víctima se ocultan en esferas que se mezclan; el ladrón elige una. La mezcla es continua y se puede seguir con la vista; velocidad y duración salen de datos. |
| Foco de evento | `EventFocus` | Escenificación de un evento: luces fuera, foco sobre los implicados, el resto mira. |

## Warden

| Español | Código | Definición |
|---|---|---|
| IA directora, el cerebro | `WardenBrain` | Clase de C++ sin `UWorld`, actores ni temporizadores: de estado de partida, reloj y semilla a una decisión. Se prueba sin editor y se simula miles de veces. |
| Anfitrión del Warden | `WardenHost` | `UWorldSubsystem` solo en el servidor: dueño del intervalo de evaluación, llama al cerebro, aplica la decisión y la replica. No decide. |
| Golpe | `Strike` | Canal del Warden: evento con efecto dirigido al jugador activo, en cualquier instante del turno; lo corta. |
| Escena | `Scene` | Canal del Warden: evento sin protagonista, entre turno y turno; no corta salvo que en datos se marque como cortante. |
| Comentario | `Remark` | Canal del Warden: solo voz y subtítulo, sin efecto, sin cortar; el Warden no se materializa. Doce tipos con tres variantes cada uno. |
| Garantía | `InterventionGuarantee` | Si pasan 120 s sin golpe ni escena, la siguiente evaluación interviene seguro. |
| Corte | `CutsTurn` | Campo en datos de cada evento: si corta el turno en curso. Golpes sí, escenas no por defecto, programados siempre. |
| Evento | `WardenEvent` | Entrada de datos: nombre, saco (acto), canal, entrega, usos máximos, ventana de tiempo, corte, y una a cuatro opciones compuestas de efectos con dirección (GDD de castigos). 38 en el TFM. |
| Ventana de evento | `EventWindow` | Tramo de la partida en el que un evento está disponible (por ejemplo, Al Altar solo hasta el minuto 2,5). |
| Usos máximos | `MaxUses` | Veces que un evento puede salir por partida. |
| Saco de eventos | `EventBag` | Lista de eventos por acto, con peso por acto. Acumulativa: en el Acto III están los tres sacos. |
| Disparador contextual | `ContextTrigger` | Condición con umbrales en datos que multiplica pesos de eventos y direcciones. Cinco en el TFM: líder claro (`ClearLeader`), victoria inminente (`ImminentVictory`), jugador ignorado (`IgnoredPlayer`), partida estancada (`StalledMatch`), acaparador (`Hoarder`). |
| Oráculo del Caos | `Oracle` | Forma de entrega de decretos, nunca de dilemas ni votaciones: orbe central que muestra una sentencia. Siete segundos para consultarlo (en PC, mantener pulsado hasta llenar una barra); si no, la sentencia se aplica igual y sube la insolencia. Su sorteo lleva un sesgo en datos (`OracleBias`, 0,5) entre azar puro y el criterio del Warden. |
| Dilema | `Dilemma` | Evento con dos a cuatro opciones que elige el jugador activo, siempre por Warden directo. Cada opción es una lista de efectos con su dirección. Un evento con una sola opción es un decreto. |
| Votación | `Vote` | Escena en la que todos señalan a otro jugador a la vez; el más votado recibe el efecto; votos públicos; no votar es insolencia. Tres en el TFM: El chivo, Juicio, Favor. |
| Decreto global | `GlobalDecree` | Evento de una sola opción con dirección todos o Ecos. |

## Producción

| Español | Definición |
|---|---|
| Fase 1 / Soulbound Core | Tablero en plano, motor de reglas, Warden. Es el TFM. |
| Fase 2 | Voz, bots con dificultad, más contenido. |
| Fase 3 | VR. |
| GDD v0.1 | Documento original de julio de 2025. Fuente de cada sistema, no referencia normativa. |
| GDD v0.2 | Conjunto de documentos en `docs/gdd/`. Referencia normativa. |

## Pendiente de decisión

- Cuarto personaje: en rediseño.
