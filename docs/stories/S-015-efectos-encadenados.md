# S-015. Efectos encadenados: Prueba del Warden, Robo decretado y Duelo forzado

**Sistema**: castigos, Nicho y Vacío (GDD 05), sobre el duelo y el trilero (GDD 04). **Módulos**: `SoulboundCore` y `Soulbound` (vista). **Rama base**: `main`. **Tamaño**: grande.

## Contexto

S-014 dejó el catálogo con tres efectos que devuelven "pendiente de su story" porque no se resuelven en el acto: encadenan con el minijuego de habilidad, el trilero o el duelo. Esta story los implementa y, de paso, enseña a la máquina de turnos a jugar un duelo o un trilero **entre turnos**, como parte de un evento del Warden, sin que termine el turno de nadie. Entra también la dirección "dos rivales señalados", que solo usa el Duelo forzado, y un concepto nuevo: el **Duelo contra el Warden**, que es lo que le pasa a quien desatiende un Duelo forzado.

Decidido por el autor el 16/10/2026: (1) en el Duelo forzado el motor filtra a los rivales sin runas (si no, señalar a uno sin runas anularía el duelo y sería la forma de esquivarlo); (2) en el Robo decretado, cuando señala el jugador se le ofrecen todos los rivales del tablero y robar a uno sin runas es burla para él (como el robo del Eco, R-ROB-4), y cuando elige el Warden ("a todos", "al azar") se salta a los que no tienen runas: cambia R-DIR-2 del GDD 05 para el robo; (3) desatender un Duelo forzado es batirse contra el Warden, y desatender un Robo decretado es que el Warden te robe a ti: sustituyen a "contra un rival al azar" de R-DIR-4; (4) una Prueba del Warden sobre quien ya la tiene no es válida (principio "el Warden nunca parece equivocarse", 14/10/2026).

## Reglas a implementar

- **R-EFE-10. Prueba del Warden.** Queda pendiente en el asiento objetivo. Al empezar su siguiente turno **jugado**, antes de tirar, juega el minijuego de habilidad con la dificultad de datos (valor inicial normal). Superado, el turno sigue con normalidad; fallado, el turno termina sin tirada y **sin consumir el sorteo** (como un turno en el Vacío). Es como mucho una: sobre un asiento que ya la tiene no es válida (burla). Se anula al entrar en el Vacío. Si el asiento tiene turnos perdidos, se descuentan antes y la Prueba espera a su siguiente turno jugado.
- **R-EFE-12. Robo decretado.** El ladrón es quien dirige el evento. Con "rival señalado": se le ofrecen todos los rivales del tablero (nunca quien está en el Vacío); el trilero se juega como siempre (`BeginSteal`): sin runas la víctima, burla al ladrón (R-ROB-4); Koschei aparta (R-ROB-5). Con "a todos": un trilero por cada rival del tablero con runas, uno tras otro, en orden de asiento. Con "al azar": uno entre los del tablero con runas, con el sorteo del evento. Un robo cobra el Marcado como cualquier robo (ya implementado).
- **R-EFE-13. Duelo forzado.** Según R-DUE-8, con R-DUE-1 y R-DUE-3 a 7. Objetivos válidos: rivales en el tablero con al menos una runa. Con "activo contra rival señalado", quien dirige es el retador; con "dos rivales señalados", quien dirige señala dos, uno tras otro, y el primero es el retador; con "dos al azar", dos válidos con el sorteo del evento y el retador se sortea. En juego, por la variante de datos: `OneRune`, el ganador roba una con trilero (R-DUE-5); `AllRunes`, todas las runas del perdedor pasan al ganador sin trilero. Perdedor sin runas, R-DUE-6 (solo puede ser el retador). Ganar cobra el Marcado (ya implementado).
- **Duelo contra el Warden** (nombre nuevo, decisión del autor del 16/10/2026). Cuando quien dirige un Duelo forzado no señala a tiempo (en una señal o en la segunda), o en un dilema resuelto en contra que contiene un Duelo forzado, se bate contra el Warden. El Warden **reta** (el empate es del retado, decisión de S-012). El Warden rinde un valor fijo de datos (`wardenDuelPerformance`, valor inicial 0,75); el jugador rinde como siempre, con su margen de afinidad si es Shuten-dōji. Si gana el Warden, lo que estaba en juego vuelve a su Fuente (R-RUN-6): una runa del jugador al azar con `OneRune`, todas con `AllRunes`; sin trilero, el Warden no tiene Brazal. Si gana el jugador, no pierde nada. En los dos casos, insolencia (R-DIR-4).
- **R-DIR-4 para el Robo decretado.** Si quien dirige no señala a tiempo a quién robar, el Warden le roba a él: una runa suya al azar vuelve a su Fuente, con insolencia. En un dilema resuelto en contra, lo mismo.
- **Entre turnos.** Un duelo o un trilero que nace de un evento se juega en las fases `Duel` y `ShellGame` de la máquina, con sus relojes y elecciones de siempre, pero **no termina ningún turno**: al acabar, la máquina vuelve a `BetweenTurns` y el evento sigue por su siguiente aplicación. Sus participantes están "en juego" mientras dura (R-INT-3). Las elecciones (runa de Koschei, esfera, rendimientos) las contestan los asientos que toque, tengan o no el turno, como en S-010.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-EFE-7** (R-EFE-10). Prueba pendiente: al empezar su turno falla → el turno termina sin tirada y el sorteo de la partida no se ha movido; supera → tira con normalidad; desterrado antes de su turno → ya no está pendiente; con un turno perdido → se salta ese turno y la Prueba sigue pendiente para el siguiente jugado; una segunda Prueba sobre el mismo asiento → no válida, con burla.
- **CA-EFE-9** (R-EFE-12). Robo decretado "a todos" con tres rivales, uno sin runas → dos trileros, uno tras otro, y el ladrón tiene dos runas más; con Koschei entre las víctimas, aparta una. "Rival señalado" a uno sin runas → burla al ladrón, sin trilero, y el evento sigue.
- **CA-EFE-10** (R-EFE-13). Duelo forzado con `AllRunes` → todas las runas del perdedor están en el Brazal del ganador sin trilero, con un registro por runa; con `OneRune` → trilero como en S-012; retador sin runas que pierde → a su hogar.
- **Dos rivales señalados.** Dos elecciones seguidas; el primero señalado reta al segundo; el segundo no se ofrece como opción de sí mismo ni al primero.
- **Duelo contra el Warden.** Reloj agotado en la primera o la segunda señal, y dilema en contra con Duelo forzado → el jugador se bate contra el Warden como retado; con el rendimiento del Warden por encima, pierde lo que estaba en juego y vuelve a la Fuente (reserva una más por runa); por debajo, no pierde nada; insolencia en todos los casos. Shuten-dōji suma su margen.
- **El Warden te roba.** Reloj agotado al señalar a quién robar, y dilema en contra con Robo decretado → una runa del jugador al azar vuelve a su Fuente, con insolencia; sin runas, burla.
- **CA-COM-1 completa.** `duelo-por-todo` se aplica (deja de ser "pendiente de S-015") y produce lo descrito en R-COM-3.
- **Entre turnos.** Un duelo y un trilero de evento no cambian `TurnCount` ni el último que jugó; `IsSeatInPlay` da verdadero para sus participantes mientras duran; el siguiente turno empieza después de que el evento termine; un asiento en el Vacío no es objetivo, ni ladrón, ni retador.
- **Efectos pendientes.** Solo Alto y Consulta forzada devuelven "no implementado".
- **Registro y sorteo.** Los rendimientos de los dummies y las esferas usan sus sorteos de siempre; "dos al azar", "al azar" y la runa que vuelve a la Fuente usan el sorteo del evento; nada toca el sorteo de la partida.
- **Parámetros.** `wardenTrialDifficulty` y `wardenDuelPerformance` se cargan y validan (dificultad entre las tres; rendimiento en [0, 1]); su rechazo tiene test.
- **Simulación.** Sin cambios: el cerebro del spike no lanza eventos. Los 192 tests actuales siguen en verde.

Manuales, en PIE con paso a paso y comandos, con captura:

- **Duelo forzado**: `SbEvent duelo-por-todo` siendo el activo: señalas a dos dummies, se baten en pantalla, el ganador se lleva todas las runas del perdedor y la crónica lo cuenta runa a runa. Repite sin señalar: la crónica dice que te bates contra el Warden y el resultado.
- **Robo decretado**: `SbEvent robo-a-todos` siendo el activo: dos o tres trileros seguidos en tu pantalla, uno por rival con runas.
- **Prueba del Warden**: `SbEvent prueba-del-warden` y señalas a un dummy; la leyenda muestra "PRUEBA DEL WARDEN" en su asiento; en su turno, la crónica dice que la juega y si la supera o falla (`SbTrial pass|fail` la decide).

## Notas para quien lo implemente

- Nombres del glosario: Prueba del Warden `WardenTrial`, Robo decretado `DecreedTheft`, Duelo forzado `ForcedDuel`, insolencia `Insolence`. Nombres nuevos que se proponen en el plan y en el PR: `ESkillTrial::WardenTrial`, el estado de Prueba pendiente en `FSeatState`, el Duelo contra el Warden (`WardenDuel`) en el estado del duelo, la elección de dos rivales (dos `ChosenRival` seguidas valen si queda claro cuál es cuál), eventos del registro (`WardenTrialPending`, `WardenTrialPassed`, `WardenTrialFailed`, `WardenTrialCancelled`, `RunesSeized` por runa, `WardenDuelStarted`, `WardenDuelWon`, `WardenDuelLost`, `RuneTakenByWarden`), motivos nuevos de `EEffectResult` (`AlreadyPending`, y los que hagan falta).
- **Dónde vuelve la máquina.** `BeginDuelAction` y `BeginStealAction` existen y hoy terminan el turno al acabar (`EnterEnd`). Hay que separar "dónde vuelve la máquina al acabar": al `End` del turno, o al evento en curso (`BetweenTurns` y seguir el evento). El evento en curso (`FCurrentWardenEvent`) recuerda que espera un duelo o un trilero suyo. Nada se duplica: el duelo y el trilero son los de S-010 y S-012.
- **Prueba del Warden en la máquina.** Entra en `Start`, después de la salida del Vacío (que la anula) y antes de `PreRoll`, como fase `SkillTrial` con `PendingTrial` propio y la dificultad de datos; `SubmitSkillTrialResult` la contesta. Fallada, `EnterEnd` sin pasar por `PreRoll` (sin consumo).
- **Duelo contra el Warden.** Un duelo con un solo asiento: el rendimiento del Warden es el parámetro y se registra al abrirlo; se resuelve cuando llega el del jugador. `DuelWinner` vale igual (el margen de Shuten-dōji se suma al jugador). Lo que vuelve a la Fuente usa `ReturnRuneToSource`.
- **Datos.** `partida.json`: `wardenTrialDifficulty` (`"normal"`) y `wardenDuelPerformance` (0.75), validados. `eventos.json` gana `prueba-del-warden` (WardenTrial al rival señalado), `robo-a-todos` (DecreedTheft a todos) y `duelo-con-quien-senales` (ForcedDuel activo contra rival señalado, `OneRune`).
- **Vista**: "PRUEBA DEL WARDEN" en la leyenda del asiento que la tiene; la cabecera del duelo y del trilero dice cuándo son de un evento ("DUELO FORZADO", "ROBO DECRETADO", "DUELO CONTRA EL WARDEN"); crónica. Solo lectura.
- Dummies y simulación: contestan rendimientos y esferas con `IDecider` sin cambios.
- Nada de `Engine` en `SoulboundCore`. Ningún asset de Unreal.

## Fuera de esta story

- Alto y Consulta forzada: Warden (S-016, S-017).
- Tiempo del minijuego de habilidad y del duelo, y qué pasa si un participante no rinde: story del minijuego.
- La frase del Warden al ganar o perder su duelo, y sus burlas con voz: S-016.
- Bots que eligen a quién señalar con criterio: GDD 08. Apuestas.
