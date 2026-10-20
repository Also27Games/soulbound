# S-020. Movimiento por el camino y corte a medio camino

**Sistema**: tablero y movimiento (GDD 01), turnos (GDD 02), el Warden (GDD 06). **Módulos**: `SoulboundCore` (máquina de turnos y reglas de movimiento) y `Soulbound` (anfitrión y vista). **Rama base**: `main`. **Tamaño**: medio.

## Contexto

Hoy el motor mueve la ficha de golpe: al confirmar destino ya está en la casilla, la casilla resuelve y la vista dibuja un deslizamiento recto de medio segundo. Por eso un golpe del Warden nunca cae "a medio camino": no existe el camino en el motor. R-TAB-26 dice otra cosa: si la ficha está en movimiento cuando el Warden corta, se detiene en la casilla de su camino en la que se encuentra en ese instante (la última en la que ha entrado del todo) y esa casilla no resuelve efecto. La presentación comunica al motor esa casilla; el motor no conoce la animación. Esta story lo hace real: el motor conoce el **camino** del movimiento y lleva una fase de **movimiento en curso**; la presentación recorre el camino casilla a casilla por los senderos y va diciendo al motor en qué casilla ha entrado; un golpe en medio deja la ficha donde está. Propuesta del autor del 20/10/2026, aceptada: va después de S-019 y antes del Warden II.

## Reglas a implementar

- **R-TAB-14, R-TAB-16, R-TAB-17, R-TAB-20. El camino.** Al confirmar destino, el motor calcula **un** camino válido hasta él (sin repetir casillas, todas transitables para el jugador, de longitud exactamente la tirada, o menor si acaba en el Altar o en la casilla anterior a un Sello), elegido de forma determinista (el primero en orden ascendente de vecinos). Es una regla de movimiento, hermana de `ReachableTiles`, y lleva su test.
- **R-TUR-7, nueva fase. Movimiento en curso.** Entre confirmar destino y resolver la casilla hay una fase de movimiento: el estado guarda el camino y cuántas casillas ha entrado la ficha. La presentación entrega una **intención de avance** por cada casilla en la que entra la ficha, en orden; el motor la valida (asiento, fase, la siguiente casilla del camino) y actualiza la posición del asiento y el registro (`TileEntered`). Al entrar en la última, la casilla resuelve como hoy (Nicho, Fuente, salto, acción) y se registra `Moved`. Una intención fuera de orden o de otra casilla se rechaza sin castigo. El reloj de turno ya se ha parado al confirmar, como hoy.
- **Salvaguarda.** El motor no depende de la presentación: si no llega el avance de una casilla en el tiempo de datos (`moveStepSeconds`, valor inicial 0,6 s; el doble del paso de la animación), la máquina avanza esa casilla por su cuenta. Así una presentación lenta o ausente (simulación, comandlet, cliente colgado) no para la partida. La simulación y los dummies entregan el camino entero en el acto.
- **R-TAB-26, R-TUR-11, R-TAB-28. Corte a medio camino.** Un golpe (o una escena cortante, o el Alto) durante el movimiento en curso **corta ahí**: la ficha se queda en la última casilla en la que entró, el resto del camino se descarta, esa casilla **no resuelve** (ni Nicho, ni Fuente, ni salto, ni acción), se registra `TurnInterrupted` con la fase y el turno termina como cualquier corte. El evento del golpe se aplica entre turnos con el cortado como activo, como en S-016. Cortado antes de entrar en ninguna casilla (en la primera), la posición no cambia.
- **R-WAR-7.** El movimiento en curso **sí** se corta (no es un duelo ni un trilero): el cerebro no aplaza en esta fase.
- **El salto planar** (R-POD-5) sigue siendo instantáneo: es un salto, no un camino.
- **Vista.** La ficha recorre el camino casilla a casilla, en línea recta entre centros (por donde están los senderos), con un paso de datos (`tokenStepSeconds`, valor inicial 0,3 s por casilla, parámetro del actor) y entrega el avance al entrar en cada casilla. Si el motor avanza por su cuenta (salvaguarda) o corta, la ficha se coloca donde dice el estado. Entre turnos, el Vacío y la Vuelta al hogar siguen colocando sin recorrer camino.
- **Comando.** `SbStrike <id>`: lanza el evento como golpe **ahora**, cortando el turno en curso (si no hay turno, como `SbEvent`). Patrón de S-011. Sirve para probar el corte a medio camino sin esperar al cerebro.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-MOV-1** (R-TAB-14, 16, 17, 20). `MovementPath` devuelve un camino de longitud exactamente la tirada hasta cada alcanzable de `ReachableTiles` (y menor hasta el Altar o la casilla anterior a un Sello), con todas sus casillas transitables y sin repetir; vacío hacia una casilla no alcanzable; el mismo camino en dos llamadas iguales; para un Eco confinado, sin casillas de Dominios con dueño.
- **CA-MOV-2** (fase de movimiento). Al confirmar destino, la máquina pasa a movimiento en curso con el camino; cada intención de avance en orden actualiza la posición y deja `TileEntered`; una fuera de orden, de otra casilla o de otro asiento se rechaza sin cambiar nada; al entrar en la última, la casilla resuelve (Nicho se reclama, Fuente da runa o abre minijuego) y se registra `Moved`, y lo que sigue (salto, acción) es como hoy.
- **CA-MOV-3** (salvaguarda). Sin intenciones de avance, la máquina recorre el camino por su cuenta a razón de una casilla por `moveStepSeconds` y llega igual; el parámetro se carga y valida (positivo).
- **CA-TAB-27, CA-TAB-29** (R-TAB-26, R-TAB-28). Golpe con la ficha en la casilla P del camino (dos de cuatro, por ejemplo): la posición es P, P no resuelve (sin `Bonded`, sin `RuneObtained`, sin minijuego), el resto del camino se descarta, `TurnInterrupted` con la fase de movimiento, el turno termina y el evento se aplica al cortado entre turnos; cortado antes de la primera casilla, la posición es la de origen; con Alto, lo mismo. Un Sello que cae en ese evento se aplica después del corte.
- **CA-MOV-4** (R-WAR-7). Una evaluación del Warden durante el movimiento en curso no se aplaza: puede cortar.
- **Simulación y determinismo.** La simulación entrega el camino entero en el acto; la crónica gana como mucho una línea por movimiento (el camino) o ninguna, a decidir en el plan; los 269 tests siguen en verde, salvo los que miren la fase tras confirmar destino, que se adaptan y se listan; el test de determinismo de S-010 sigue con sus tiradas.
- **Registro.** `TileEntered` (asiento, casilla) y `Moved` como hoy; `TurnInterrupted` con la fase nueva.

Manuales, en PIE con captura:

- La ficha recorre el camino casilla a casilla por los senderos (con el kit si está importado; con el respaldo, por las tiras).
- `SbStrike pierdes-un-turno` a mitad de un movimiento de tres o más casillas: la ficha se para en la casilla en la que estaba, la crónica dice el corte con la fase de movimiento y la casilla no resuelve (una Fuente a medio camino no da runa).
- Con el Warden en `auto`, un golpe natural a medio camino.

## Notas para quien lo implemente

- Nombres del glosario: casilla alcanzable, camino (R-TAB-14), Corte `CutsTurn`, `TurnInterrupted`. Nombres nuevos que se proponen en el plan: la fase (`ETurnPhase::Moving` o similar), el estado del movimiento en `FMatchState` (camino e índice), la regla `MovementPath` en `Board/MovementRules.h`, la intención de avance (`SubmitMoveProgress` o similar) con su `EIntentResult`, el evento `TileEntered`, los parámetros `moveStepSeconds` (partida) y `tokenStepSeconds` (actor), el comando `SbStrike`.
- **El camino sale de las mismas reglas que las alcanzables**: `MovementPath` reutiliza la búsqueda de `ReachableTiles` (o se extrae una base común); nada se duplica. Determinista: vecinos en orden ascendente, primer camino válido.
- **La máquina**: la fase de movimiento va entre `RollAndChoose` y `MoveAndResolve`; `AdvanceTurn` en ella aplica la salvaguarda; `SubmitDestinationIntent` ya no mueve: guarda el camino y pasa a la fase; al completar el camino se entra en `MoveAndResolve` como hoy. `CanInterruptTurn` e `InterruptTurn` tratan la fase nueva: posición la última entrada, sin resolver, camino descartado. `IsSeatInPlay` la cuenta como en juego. El consumo fijo del sorteo no cambia (ya se sacó al tirar).
- **La vista**: `ASeatTokenActor` recibe el camino (lista de posiciones de losa, de la colocación de S-019) y avanza por tramos; al entrar en cada casilla avisa al anfitrión, que entrega la intención. Si el estado va por delante (salvaguarda o corte), la ficha se recoloca sin animar. El deslizamiento recto de hoy queda para lo que no es un movimiento (Vacío, hogar, Expulsión).
- **Anfitrión**: los dummies entregan el camino entero nada más confirmar; el humano, según la animación. En paso a paso no cambia nada.
- Nada de `Engine` en `SoulboundCore`. Ningún asset.

## Fuera de esta story

- Animación de la ficha más allá del desplazamiento (saltos, giros, avatares): interfaz y arte.
- La congelación y el foco de la interrupción (GDD 07, R-FOC): interfaz.
- Elegir entre varios caminos de la misma longitud (hoy, el primero): si alguna vez importa, GDD 01.
