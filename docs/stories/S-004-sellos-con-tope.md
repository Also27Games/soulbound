# S-004. Sellos con tope

**Sistema**: tablero y movimiento (GDD 01). **Módulo**: `SoulboundCore`. **Rama base**: `main`. **Tamaño**: pequeña.

## Contexto

Hoy el cerebro del Warden del spike (`Source/SoulboundCore/Private/Warden/WardenBrain.cpp`) sella y libera casillas escribiendo `bSealed` a mano en `Apply`, sin tope ni prohibiciones: la crónica de S-003 muestra jugadores encerrados durante siete turnos. El tope ya está en datos (`FBoardParams::MaxSealedTiles`, valor 3) pero nadie lo usa. Sellar es una regla del tablero, no del Warden: esta story la lleva al motor y el cerebro del spike pasa a llamarla. El cerebro en sí sigue siendo el del spike; lo sustituye la story del Warden.

## Reglas a implementar

- **R-TAB-22.** Puede haber varias casillas selladas a la vez, hasta un máximo definido en datos; al sellar por encima del máximo, se libera la más antigua. No pueden sellarse el Altar, los Nichos, las Fuentes de Poder ni una casilla ocupada. No puede sellarse una casilla si con ello alguna casilla quedara sin camino transitable hasta el Altar para un jugador sin Paso Libre.
- **R-TAB-23.** Una casilla sellada solo se libera al caer por exceder el máximo. Los jugadores no mueven ni retiran Sellos. Si el Warden retira Sellos, es un evento suyo definido en su GDD.
- **R-TAB-17** (ya implementada) sigue igual: la transitabilidad no cambia.
- **Registro** (GDD 01, criterio final): cada transición de estado de una casilla emite un evento con marca de tiempo.

## Criterios de aceptación

- **CA-TAB-23** (R-TAB-22). Dado el máximo M de Sellos ya alcanzado, cuando el Warden sella otra casilla, entonces la más antigua se libera y hay exactamente M selladas; dado un intento de sellar Altar, Nicho, Fuente o casilla ocupada, entonces se rechaza sin cambiar el estado; dado un intento que dejaría alguna casilla sin camino al Altar para un jugador sin Paso Libre, entonces se rechaza.
- **CA-TAB-24** (R-TAB-23). Dada una casilla sellada, cuando cualquier jugador intenta retirarla o moverla, entonces no existe tal acción en el motor: la única forma de liberar un Sello es caer por el tope o la función reservada al Warden.
- **Registro.** Dado un Sello que se pone o se libera (por tope o por el Warden), entonces el registro de eventos contiene el evento con la casilla y la marca de tiempo.
- **Cerebro del spike.** Dada la simulación de una partida, cuando el Warden interviene, entonces nunca hay más de M casillas selladas ni ninguna prohibida, y ninguna intervención de sellar se pierde por elegir una casilla inválida (el cerebro elige entre las válidas). `CronicaDeUnaPartida` sigue en verde y ya no muestra jugadores encerrados por Sellos.
- Los 40 tests actuales siguen en verde.

## Decisiones tomadas por el autor (28/09/2026)

1. **Dos funciones, no una**: `CanSealTile` (o el nombre que se proponga) dice si una casilla se puede sellar y por qué no, y `SealTile` sella aplicando el tope. Así el cerebro del spike, y el Warden de verdad después, eligen entre casillas válidas en vez de desperdiciar intervenciones.
2. **La liberación por el Warden existe como función del motor** (`ReleaseSeal` o similar), reservada a él, porque el cerebro del spike ya la usa y el GDD la prevé como evento suyo; los jugadores no tienen ninguna vía.

## Notas para quien lo implemente

- Nombres del glosario: Sello `Seal`, Paso Libre `PowerFreePassage` (rasgo `FMoverTraits::bFreePassage`), Altar, Nicho, Fuente. Los nombres de las funciones, del motivo de rechazo (enum) y de los eventos nuevos del registro se proponen en el plan y en el PR.
- "La más antigua": el estado tiene que recordar el orden en que se pusieron los Sellos. Propón cómo (lista ordenada de casillas selladas en `FBoardState`, o un número de secuencia por casilla) y mantén `bSealed` coherente con ello, o sustitúyelo si la lista lo hace redundante. `IsPassable` y `ReachableTiles` no cambian de contrato.
- "Sin camino hasta el Altar": se comprueba para todas las casillas no selladas, con la transitabilidad de un jugador sin Paso Libre, sobre el tablero tal como quedaría con el Sello nuevo puesto y el más antiguo liberado si toca. Una casilla ocupada se comprueba con `FMatchState::IsTileOccupied`, así que la regla vive a nivel de partida (`FMatchState`), no solo de tablero.
- El registro: dos tipos nuevos de `EMatchEventType` (Sello puesto, Sello liberado), con la casilla en `Value` y sin asiento; el liberado por tope se emite antes que el puesto que lo provoca.
- El cerebro del spike: `Evaluate` elige la casilla entre las que `CanSealTile` acepta; `Apply` llama a `SealTile` y `ReleaseSeal`, nunca a `bSealed`. Los tests actuales del cerebro (`NuncaSellaElAltar`, `CerebroSinMundo`) siguen valiendo; el primero queda cubierto por la regla.
- Sin números mágicos: el tope sale de `FBoardParams`; ninguna constante nueva.
- Nada de `Engine`, `UWorld` ni `UObject`.

## Fuera de esta story

- Sello contra rival, decretos y cualquier lógica de cuándo o dónde sella el Warden: story del Warden.
- Recalcular las casillas alcanzables si un Sello llega entre tirada y confirmación (R-TUR-11): story del Warden.
- Casillas ocupadas como criterio de transitabilidad (R-TAB-19) y confinamiento de Ecos (R-TAB-20): stories de carrera.
- Presentación del Sello (grieta, cadena, sonido, frase): story de tablero visible.
