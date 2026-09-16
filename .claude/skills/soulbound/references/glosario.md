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
| Trono | `Throne` | Asiento de cada jugador en la Cripta, alrededor de la mesa. No es una casilla del tablero. |
| Dominio | `Domain` | Una de las seis zonas del tablero, gobernada por un Elegido. Conectada con el Altar y con sus dos Dominios vecinos. Cada Dominio tiene un Nicho y una Fuente de Poder. |
| Dominio Olvidado | `ForgottenDomain` | Dominio sin dueño: corrompido por el Warden al inicio (6 menos N) o no reclamado al acabar la carrera. Nicho cerrado; runa solo con tirada de 4 o más; territorio de los Ecos. Al acabar la carrera, cada Eco recibe uno propio, nunca compartido, que es su hogar. |
| Nicho | `Niche` | Casilla fija de cada Dominio donde se reclama al Elegido. Punto de reaparición y de castigo medio. |
| Fuente de Poder | `PowerSource` | Casilla fija de cada Dominio donde se obtiene su runa. |
| Nodo de Poder | `PowerNode` | Casilla especial cuyo efecto se asigna al azar al inicio: Duelo, Oráculo o Planar. Todos de elección voluntaria: solo actúan si el jugador acaba su movimiento en ellos. |
| Nodo del Oráculo | `OracleNode` | Casilla en la que acabar obliga a consultar el Oráculo del Caos, con sus siete segundos. Riesgo que se elige: el Oráculo también da regalos. |
| Nodo Planar | `PlanarNode` | Casilla que permite teletransportarse a otro Nodo Planar. |
| Sello | `Seal` | Estado de casilla impuesto por el Warden: una casilla sellada no se pisa ni se atraviesa salvo con Paso Libre. Solo el Warden pone y quita Sellos. No se sellan el Altar, los Nichos, las Fuentes ni casillas ocupadas. |
| Despertar del tablero | `BoardAwakening` | Momento en que el primer jugador se convierte en Elegido y se activan los Nodos de Poder. |

## Objetivo y poderes

| Español | Código | Definición |
|---|---|---|
| Runa de Poder | `Rune` | Uno de los seis objetos de colección. Cada Elegido tiene su propio mapa de runa a poder. |
| Brazal Arcano | `Bracer` | Inventario de runas del jugador. Estado oculto o revelado. En PC, un panel de interfaz. |
| Poder | `Power` | Capacidad que otorga una runa según el Elegido: Duelo, Celeridad, Paso Libre, Yugo, Vuelo Planar, Desvío. |
| Duelo (poder) | `PowerDuel` | Permite iniciar duelos en su casilla. |
| Celeridad | `PowerHaste` | Elegir tirar uno o dos dados. |
| Paso Libre | `PowerFreePassage` | Atravesar y pisar casillas selladas. |
| Yugo | `PowerYoke` | Someter a quien comparte casilla: imponerle un castigo. Nombre provisional. |
| Vuelo Planar | `PowerPlanarFlight` | Usar los Nodos Planares. |
| Desvío | `PowerDeflection` | Sacrificar la runa para salir del Vacío. |
| Afinidad | `Affinity` | Poder pasivo único de cada Elegido. |
| Runa de Liberación | `LiberationRune` | Runa de un Dominio Olvidado que da poderes a los Ecos. |
| Amuleto de Desvío | `DeflectionAmulet` | Objeto de un solo uso que anula un castigo y pasa al jugador de la izquierda. |
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
| El Vacío | `TheVoid` | Castigo máximo: destierro. Solo se llega por el Warden (decreto, Oráculo o Furia); no hay casilla del Vacío en el tablero. Se sale con un 6 o tras tres fallos, más el escape, y se reaparece en el hogar. |
| Escape del Vacío | `VoidEscape` | Prueba para volver: tirada de 6 (o tras tres fallos) y después el minijuego de habilidad. |
| Ley de Sombras | `ShadowLaw` | Decreto del Warden que manda a cada Eco al Nicho cerrado de su Dominio asignado. |
| Hogar | `HomeTile` | Casilla a la que un jugador vuelve tras el Vacío o un castigo de "vuelta al principio": el Nicho reclamado para un Elegido, el Nicho cerrado de su Dominio asignado para un Eco, el Altar para todos durante la carrera. |
| Marcado | `Marked` | Estado del jugador perseguido: quien le perjudique gana un turno extra. |
| Insolencia | `Insolence` | Contador oculto de desobediencias al Warden por jugador. Castigos de probabilidad creciente por nivel; al quinto, la Furia del Warden con castigo máximo fijo y reinicio. Fase 1. |
| Furia del Warden | `WardenFury` | Evento cinemático al alcanzar el nivel máximo de insolencia. Castigo predefinido, sin azar. |
| Minijuego de habilidad | `SkillMinigame` | Una única mecánica corta de timing o reacción con tres presentaciones: duelo, escape del Vacío y desafío final. |
| Trilero | `ShellGame` | Interacción de robo: las runas de la víctima se ocultan en esferas que se mezclan; el ladrón elige una. |
| Foco de evento | `EventFocus` | Escenificación de un evento: luces fuera, foco sobre los implicados, el resto mira. |

## Warden

| Español | Código | Definición |
|---|---|---|
| IA directora | `WardenBrain` | Función de estado de partida a decisión. Sin mundo, simulable. |
| Evento | `WardenEvent` | Acción del Warden: tipo, víctima, causa, texto, consecuencias. |
| Saco de eventos | `EventBag` | Lista de eventos disponibles por acto. Acumulativa. |
| Disparador contextual | `ContextTrigger` | Condición que altera las probabilidades: líder claro, partida estancada, jugador ignorado, victoria inminente. |
| Oráculo del Caos | `Oracle` | Objeto central que revela un destino cuando el Warden lo ordena. Siete segundos para actuar. |
| Dilema | `Dilemma` | Evento con dos opciones que elige el jugador. |
| Decreto global | `GlobalDecree` | Evento que cambia una regla para todos. |
| Humor del Warden | `WardenMood` | Estado legible: normal, furia, regalo, análisis. |

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
- Yugo es provisional.
