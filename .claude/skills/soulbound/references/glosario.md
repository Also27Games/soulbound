# Glosario oficial de Soulbound

Usa estos nombres y solo estos. La columna "Código" es el identificador en inglés para clases, structs y enumeraciones. Si un concepto no está aquí, propón el nombre al autor antes de usarlo.

## Actores

| Español | Código | Definición |
|---|---|---|
| El Warden | `Warden` | Anfitrión y director de la partida. Entidad ancestral que se alimenta de la desesperación. Interviene, castiga, ofrece dilemas. Es la IA directora. |
| Eco Perdido | `LostEcho` | Estado inicial de todo jugador: un espectro sin poder. Quien no reclama un Sepulcro en la carrera se queda así toda la partida. |
| [CLASE] | `[PENDIENTE]` | Los seis seres oscuros a los que un Eco puede vincularse. Cada uno da un poder pasivo y un mapa de runa a poder. Nombre pendiente de decisión del autor. |
| Jugador | `Player` | Persona o bot que controla un Eco o un [CLASE]. |
| Dummy | `DummyPlayer` | Jugador automático sin criterio (tira y mueve al azar). Solo para pruebas automáticas y simulación. |
| Bot | `BotPlayer` | Jugador automático con criterio, que implementa la interfaz de decisión del jugador. En Fase 1, el bot básico: va a por el objetivo más cercano y elige en los dilemas lo que le beneficia. Otros bots y niveles de dificultad se añaden como nuevas implementaciones. Rellena los huecos hasta cuatro y permite jugar solo. |

## Tablero

| Español | Código | Definición |
|---|---|---|
| La Cripta | `Crypt` | La sala donde se juega. En Fase 1, la escena del tablero. |
| Tablero | `Board` | Grafo de casillas con seis reinos y la Senda del Anillo. |
| Casilla | `Tile` | Nodo del grafo. Tiene un tipo y conexiones. |
| Conexión | `Edge` | Enlace entre dos casillas. Puede tener una Puerta. |
| Senda del Anillo | `RingPath` | Camino circular central que conecta los seis reinos. Contiene las casillas de Casa. |
| Reino | `Realm` | Una de las seis zonas temáticas del tablero. Cada reino tiene un Sepulcro y una Fuente de Poder. |
| Reino Olvidado | `ForgottenRealm` | Reino corrompido por el Warden al inicio (6 menos N). Sepulcro cerrado; runa solo con tirada de 4 o más; territorio de los Ecos. |
| Casa | `HomeTile` | Casilla de inicio y de victoria de cada jugador, numerada del 1 al 4, en la Senda. |
| Sepulcro | `Sepulcher` | Casilla fija de cada reino donde se reclama al [CLASE]. Punto de reaparición y de castigo medio. |
| Fuente de Poder | `PowerSource` | Casilla fija de cada reino donde se obtiene su runa. |
| Nodo de Poder | `PowerNode` | Casilla especial cuyo efecto se asigna al azar al inicio: Duelo, Vacío o Nodo Planar. |
| Nodo Planar | `PlanarNode` | Casilla que permite teletransportarse a otro Nodo Planar. |
| Puerta | `Gate` | Barrera en una conexión. No se cruza sin Paso Libre. |
| Despertar del tablero | `BoardAwakening` | Momento en que el primer jugador se convierte en [CLASE] y se activan los Nodos de Poder. |

## Objetivo y poderes

| Español | Código | Definición |
|---|---|---|
| Runa de Poder | `Rune` | Uno de los seis objetos de colección. Cada [CLASE] tiene su propio mapa de runa a poder. |
| Brazal Arcano | `Bracer` | Inventario de runas del jugador. Estado oculto o revelado. En Fase 1, un panel de interfaz. |
| Poder | `Power` | Capacidad que otorga una runa según el [CLASE]: Duelo, Celeridad, Paso Libre, Dominio, Vuelo Planar, Desvío. |
| Duelo (poder) | `PowerDuel` | Permite iniciar duelos en su casilla. |
| Celeridad | `PowerHaste` | Elegir tirar uno o dos dados. |
| Paso Libre | `PowerFreePassage` | Cruzar y mover Puertas. |
| Dominio | `PowerDominion` | Imponer un castigo a quien comparte casilla. |
| Vuelo Planar | `PowerPlanarFlight` | Usar los Nodos Planares. |
| Desvío | `PowerDeflection` | Sacrificar la runa para salir del Vacío. |
| Afinidad | `Affinity` | Poder pasivo único de cada [CLASE]. |
| Runa de Liberación | `LiberationRune` | Runa de un Reino Olvidado que da poderes a los Ecos. |
| Amuleto de Desvío | `DeflectionAmulet` | Objeto de un solo uso que anula un castigo y pasa al jugador de la izquierda. |
| Grimorio | `Grimoire` | Consulta de reglas y estado del jugador. En Fase 1, un panel. |

## Partida

| Español | Código | Definición |
|---|---|---|
| Partida | `Match` | Una sesión completa de 40 minutos. |
| Carrera por el vínculo | `BondRace` | Primera fase, unos 5 minutos: los Ecos compiten por los Sepulcros con un dado. |
| Caza de runas | `RuneHunt` | Fase central. |
| Carrera hacia la victoria | `VictoryRun` | Con seis runas distintas, volver a Casa y superar el desafío final. |
| Acto I, II, III | `ActOne`, `ActTwo`, `ActThree` | Tramos de la partida que cambian el comportamiento del Warden: 0-5, 5-27, 27-40 minutos. |
| Turno | `Turn` | Tirar, mover, resolver casilla. Reloj de turno de 30 segundos. |
| Tirada | `Roll` | Resultado de uno o dos dados de seis caras. |
| Desafío final | `FinalChallenge` | Prueba para ganar al llegar a Casa con seis runas. Presentación del minijuego de habilidad en su dificultad máxima. |
| Victoria del Warden | `WardenVictory` | Derrota colectiva al agotarse el tiempo. |

## Castigos

| Español | Código | Definición |
|---|---|---|
| Castigo | `Punishment` | Cualquier consecuencia impuesta por el Warden. Siempre con nombre, causa y víctima. |
| El Vacío | `TheVoid` | Castigo máximo: destierro. Se sale con un 6 o tras tres fallos, más el desafío de escape. |
| Escape del Vacío | `VoidEscape` | Prueba para volver: tirada de 6 (o tras tres fallos) y después el minijuego de habilidad. |
| Toque de queda | `Curfew` | Decreto que devuelve a los Ecos a los Reinos Olvidados. |
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
| Fase 1 / Soulbound Core | Tablero en plano, motor de reglas, Warden mínimo. Es el TFM. |
| Fase 2 | Mecánicas completas, minijuegos, insolencia, voz, bots con criterio. |
| Fase 3 | VR. |
| GDD v0.1 | Documento original de julio de 2025. Fuente de cada sistema, no referencia normativa. |
| GDD v0.2 | Conjunto de documentos en `docs/gdd/`. Referencia normativa. |

## Nombres que no se usan

- El nombre antiguo de la clase de personajes. Se escribe [CLASE] hasta que haya decisión.
- "El Barón" como cuarto personaje. Pendiente de rediseño.
- Cualquier nombre, término o texto tomado de un juego de mesa existente.
