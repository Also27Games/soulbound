# S-001. El Altar y los Sellos detienen el movimiento

**Sistema**: tablero y movimiento (GDD 01). **Módulo**: `SoulboundCore`. **Rama base**: `spike`. **Tamaño**: pequeña.

## Contexto

`Soulbound::Rules::ReachableTiles` (`Source/SoulboundCore/Public/Board/MovementRules.h`) calcula las casillas alcanzables según R-TAB-14: caminos sin repetir casilla, de longitud exactamente la tirada, por casillas transitables. Hoy trata un Sello como una casilla que simplemente no existe, y no conoce el Altar. Dos reglas aprobadas del GDD 01 dicen otra cosa.

## Reglas a implementar

- **R-TAB-16.** El Altar detiene el movimiento: si un camino entra en el Altar, termina ahí aunque la tirada no se haya agotado. No se puede cruzar el Altar dentro de un mismo movimiento. El Altar es alcanzable si hay un camino transitable hasta él de longitud menor o igual a la tirada.
- **R-TAB-17.** Un Sello es un estado de casilla. Una casilla sellada no es transitable ni como paso ni como destino, salvo para un jugador con Paso Libre. Si un camino topa con una casilla sellada, la casilla anterior es alcanzable aunque la tirada no se haya agotado.

Se mantienen R-TAB-14 (longitud exacta para el resto de casillas) y R-TAB-18 (sin alcanzables, el jugador no se mueve; eso no cambia en esta story).

## Criterios de aceptación

- **CA-TAB-17** (R-TAB-16). Dada una posición desde la que el Altar está a distancia D menor o igual que T, cuando se calculan las alcanzables, entonces el Altar está entre ellas y ninguna casilla al otro lado del Altar lo está.
- **CA-TAB-18** (R-TAB-17, R-POD-2). Dado un camino que topa con una casilla sellada en el paso P menor que T, cuando se calculan las alcanzables para un jugador sin Paso Libre, entonces la casilla P menos uno está entre ellas y la sellada no; para un jugador con Paso Libre, la sellada y las posteriores sí.
- **CA-TAB-15** (R-TAB-14) sigue cumpliéndose para las casillas que no son el Altar ni topan con un Sello.

## Notas para quien lo implemente

- Hace falta que una casilla pueda ser el Altar. En el glosario, Altar es `Altar` y una casilla es `Tile`; decide la forma (un tipo de casilla o una marca) y justifícala en el PR. El tablero de prueba `FMatchState::MakeSpikeMatch` y el de los tests usan la casilla 0 como Altar.
- La casilla de origen no cuenta como "entrar en el Altar": un jugador que está en el Altar sale de él con normalidad.
- **La casilla de origen nunca es alcanzable.** Si la casilla sellada es vecina directa del origen (paso 1), "la casilla anterior" sería el origen y no cuenta: un movimiento siempre lleva a otra casilla. Si con ello no queda ninguna alcanzable, se aplica R-TAB-18 (el jugador no se mueve). Aclarado el 24/09; va al GDD 01 como caso límite.
- El glosario decía que el Altar era "casilla normal a efectos de tránsito"; era texto desfasado y se ha corregido el 24/09: manda R-TAB-16.
- Los tests actuales de `Source/SoulboundCore/Private/Tests/MovementRulesTest.cpp` codifican el comportamiento antiguo del Sello; hay que corregirlos según R-TAB-17, no borrarlos.
- Nada de `Engine`, `UWorld` ni `UObject` en `SoulboundCore`.

## Fuera de esta story

Ocupación por otras fichas (R-TAB-19), confinamiento de Ecos (R-TAB-20), criterio único de transitabilidad (R-TAB-21), topes de Sellos (R-TAB-22).
