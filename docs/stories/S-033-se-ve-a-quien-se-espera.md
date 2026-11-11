# S-033. Se ve a quién se espera: el panel también dice qué deciden los demás

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible). Continúa S-028. **Módulos**: `Soulbound` (presentación: `Presentation/DecisionText`, `Presentation/DecisionPanel` y la vista). `SoulboundCore` no cambia. **Rama base**: `main`, con S-028 fusionada. **Tamaño**: pequeño.

## Contexto

S-028 puso las decisiones del jugador en un panel con botones. Probándolo el 10/11/2026, el autor vio que sus decisiones quedan claras y que lo que sigue sin verse es lo que hacen los demás: si un dummy está eligiendo, si la mesa sigue votando, si alguien está ante el Oráculo. Hoy eso es una línea gris en el panel de estado, a un lado, que nadie mira. El jugador ve la partida parada y no sabe a quién espera.

Decidido por el autor el 10/11/2026:

- **El mismo panel, en el mismo sitio, en modo espera**: cuando no hay ninguna decisión abierta para el humano y sí para otro asiento, el panel dice quién decide y qué, con la barra del plazo y **sin botones**.
- **Se distingue a simple vista de "te toca a ti"**: el título no va en verde y no hay nada que pulsar.
- **Lo propio manda**: si el humano tiene algo que decidir, el panel enseña eso, como en S-028, y nada de los demás.
- Sin reglas nuevas, sin sonido, sin animación.

Añadido por el autor el 11/11/2026, al cerrar la prueba de S-028:

- **El resultado de la votación se ve**: al cerrarse, el panel enseña unos segundos quién ha sido el más votado y qué votó cada uno. Hoy solo lo cuenta la crónica y no queda claro quién se lo lleva.
- **El panel no se mueve**: en S-028 va apilado sobre la frase del Warden y sube y baja cuando la frase aparece, crece o se va. Queda fijo abajo, con el hueco de la frase siempre reservado. En el centro de la pantalla no: taparía el Altar y las fichas justo cuando hay que elegir casilla o votar con un clic.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-DEC-5 (precisión). Qué se enseña mientras esperas.** Una función pura junto a `DecisionView` (`WaitView`, nombre propuesto) recibe el estado y el asiento humano y devuelve, si la mesa espera a otro asiento, un `FDecisionView` con un tipo de entrada nuevo (`EDecisionInput::Wait`, nombre propuesto), sin opciones. No inventa nada: quién decide y qué lo dice el estado, y los textos salen de los que ya existen (`ChoiceWhat`, los nombres de asiento de S-028, el nombre y el texto del evento del Warden). Casos, en el mismo orden en que el panel de estado los enseña:
  1. **Elección de otro asiento** (`FPendingChoice` abierta para él): quién elige y qué. En las de un evento del Warden, el título es el nombre del evento y el contexto su texto. Plazo: el de la elección.
  2. **Destino de otro asiento** (su turno, fase de tirada): de quién es el turno y su tirada. Plazo: el reloj de turno.
  3. **Consulta del Oráculo de otro asiento**: quién está ante el Oráculo. Plazo: el de la consulta.
  4. **Votación abierta en la que el humano ya votó o no puede votar**: el nombre del evento y qué asientos faltan por votar (los que pueden votar y aún no lo han hecho). Plazo: el de la votación.
  5. **Prueba de habilidad o duelo de otros asientos** (Fuente, Prueba del Warden, escape del Vacío, desafío final, duelo): quién juega y qué; en el duelo, los dos asientos. Sin plazo: no lleva barra.
- **R-DEC-6 (precisión). Una sola vista para el panel.** Una función pura (`PanelView`, nombre propuesto) decide qué enseña el panel: la decisión del humano (`DecisionView`) si existe; si no, la espera (`WaitView`); si no hay ninguna, nada. Mientras la mesa espera al Warden no se enseña la espera de otros: lo que pasa es la frase del Warden, que ya está en pantalla. La decisión propia sí se sigue enseñando bloqueada, como en S-028.
- **R-DEC-8 (precisión). El resultado de la votación.** Al resolverse una votación (`VoteResolved` en el registro), el panel enseña el resultado durante `VoteResultSeconds` (Config, valor inicial 5, en segundos del reloj de partida): de título, el nombre del evento; después, el más votado con sus votos, y si desempató el Warden; debajo, una línea por voto (quién votó a quién, de los `VoteCast` de esa votación, marcando el que salió al azar por no votar a tiempo). Sin botones ni barra. Una función pura (`VoteResultView`, nombre propuesto) lo saca del registro y del reloj: hay vista si el último `VoteResolved` tiene menos de `VoteResultSeconds`. En `PanelView` va después de la decisión propia y antes de la espera, y a diferencia de la espera se enseña también mientras habla el Warden. No dice qué le pasa al más votado: eso lo dicen el Warden y la crónica.
- **R-DEC-7 (precisión). Cómo se pinta.** `SDecisionPanel` pinta la vista de espera con el título en el gris de lo secundario, sin botones y con la barra en gris; la primera línea nombra al asiento ("Asiento 2 · nombre del Elegido", "Asiento 2 (Eco)", como en los botones de voto de S-028). Las reglas de visibilidad son las de R-DEC-3: no se enseña con el anillo del humano abierto, con el telón bajado ni con los paneles ocultos (`H`). El panel de estado no cambia: sigue llevando sus líneas con el detalle.
- **R-DEC-9 (precisión). Posición fija.** El borde inferior del panel queda a una altura fija sobre el borde de la pantalla: la que ocupa una frase del Warden de `WardenLineReservedLines` líneas (Config, valor inicial 2) con su fondo y sus márgenes. Ese hueco se reserva siempre, haya frase o no, y la frase se pinta dentro de él, pegada abajo como hoy. El panel no cambia de sitio cuando la frase aparece, cambia de largo o desaparece; sí crece hacia arriba si la decisión tiene más botones. Si una frase ocupa más líneas de las reservadas, se sale del hueco por arriba y el panel sigue quieto: se anota en el PR, no se arregla aquí (es el `TODO(R-HUD-3)` de la frase a 1024x768).

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/DecisionTextTest.cpp`, nombres `Soulbound.Game.Presentation.Decision.<QueSeComprueba>`):

- **CA-DEC-9** (elección de otro, R-DEC-5). Con una elección `DiceCount` abierta para el asiento 2 y el humano en el 0: no hay `DecisionView`; `WaitView` existe, es de tipo espera, no tiene opciones, nombra al asiento 2 y lleva el plazo de la elección. Con un dilema del Warden abierto para el asiento 2, el título es el nombre del evento y el contexto su texto.
- **CA-DEC-10** (turno de otro, R-DEC-5). Con el asiento 1 en fase de tirada y una tirada de 5, la vista nombra al asiento 1 y la tirada, y su plazo es el del reloj de turno.
- **CA-DEC-11** (Oráculo de otro, R-DEC-5). Con una consulta del Oráculo abierta para el asiento 2, la vista nombra al asiento 2 y no tiene opciones.
- **CA-DEC-12** (votación, R-DEC-5). Con una votación abierta, el humano ya votado y los asientos 1 y 3 sin votar, la vista dice que faltan el 1 y el 3; cuando vota el 1, solo el 3. Un asiento que no puede votar no aparece entre los que faltan.
- **CA-DEC-13** (prueba y duelo de otros, R-DEC-5). Con el asiento 2 en una prueba de habilidad, la vista lo nombra y dice qué prueba; con un duelo entre el 1 y el 3, nombra a los dos. En los dos casos no hay plazo.
- **CA-DEC-14** (lo propio manda, R-DEC-6). En cada estado de los tests de S-028 en que `DecisionView` existe, `PanelView` devuelve esa misma vista. Con una votación abierta en la que el humano aún puede votar, `PanelView` es su voto, no la espera.
- **CA-DEC-15** (el Warden habla, R-DEC-6). Con una elección abierta para un dummy y la mesa esperando al Warden, `PanelView` no devuelve nada; sin la espera, devuelve la vista de espera.
- **CA-DEC-16** (nada, R-DEC-6). Entre turnos sin nada abierto, con una ficha moviéndose y con la partida terminada, `PanelView` no devuelve nada.

- **CA-DEC-17** (resultado, R-DEC-8). Con un registro en que el 0 y el 1 votan al 2, el 3 vota al 1 al azar y `VoteResolved` da al 2 con 2 votos en el segundo 100: en el segundo 102 hay vista, nombra al asiento 2 con sus 2 votos, lleva tres líneas de voto y marca la del 3 como al azar; pasado `VoteResultSeconds`, no hay vista. Con desempate del Warden, la vista lo dice. Con dos votaciones en el registro, solo cuentan los votos de la última.
- **CA-DEC-18** (orden, R-DEC-6 y R-DEC-8). Con un resultado reciente y una decisión abierta para el humano, `PanelView` es su decisión; con un resultado reciente y una elección abierta para un dummy, es el resultado; con la mesa esperando al Warden, el resultado se sigue enseñando.

Manuales, por el autor, en PIE, a 1024x768 y a 1920x1080, con captura:

- En el turno de cada dummy el panel dice de quién es el turno; si elige algo, qué.
- En una votación (`SbEvent juicio`), tras votar el panel pasa a decir quién falta y se va al cerrarse la votación.
- Al cerrarse la votación sale el resultado, se lee sin prisa y se va solo: quién es el más votado y qué votó cada asiento.
- Con el Oráculo de un dummy (`SbEvent el-orbe-llama` hasta que le toque a uno), el panel dice quién está ante él.
- La espera no se confunde con una decisión propia: sin verde y sin botones.
- Con una decisión abierta, el panel no se mueve cuando la frase del Warden aparece, pasa de una línea a dos o desaparece. A 1024x768 no tapa las casillas iluminadas de abajo más que en S-028; si lo hace, se dice en el PR.
- Si el panel aparece y desaparece tan deprisa que molesta, se dice en el PR: no se arregla en esta story.

## Notas para quien lo implemente

- Todo parte de `DecisionText` (S-028): `WaitView` y `PanelView` van en el mismo archivo y `TickHud` pasa a llamar a `PanelView`. El reloj de la barra (`AdvanceDecisionClock`) sirve tal cual.
- Los asientos que faltan por votar: los que tienen `State.Vote.Votes[asiento] == INDEX_NONE` y `VoteOptions` no vacío. No lo decide el panel.
- El panel ya recibe el tipo de entrada: el modo espera es un caso más de `Rebuild`, sin botones.
- Las teclas no cambian: `OnChoiceKey` sigue mirando `DecisionView`, no `PanelView`, para que una tecla nunca conteste algo que el panel enseña como espera.
- La posición fija se hace en la capa de la vista (`WardenLineOverlay`, `BuildOverlay`): hoy el panel y la frase van en un `SVerticalBox` de alto automático; la frase pasa a ir dentro de una caja de alto fijo. El alto sale del tamaño de letra de la frase (`WardenLineFontSize`) y de sus márgenes, no de un número a mano.
- Tests primero, en rojo. El pintado y la posición solo tienen criterio manual.
- El resultado sale del registro (`FMatchState::EventLog`): `VoteCast` lleva quién vota (`Seat`), a quién (`OtherSeat`) y si fue al azar (`Detail`); `VoteResolved`, el más votado (`Seat`), sus votos (`Value`) y si hubo desempate (`Detail`). La edad se mide con `ClockSeconds` del evento y del estado, sin reloj real.
- Nombres propuestos para el glosario, en el PR: `WaitView`, `PanelView`, `VoteResultView`, `EDecisionInput::Wait`.

## Fuera de esta story

- Un tiempo mínimo en pantalla para que la espera no parpadee (el resultado de la votación sí tiene el suyo, R-DEC-8), y cambiar lo que tardan los dummies en contestar (`DummyResponseSeconds`, datos del autor).
- Resaltar en el tablero la ficha de quien decide: es el aro de S-029.
- Enseñar qué ha elegido cada dummy: eso lo cuenta la crónica.
- Quitar del panel de estado las líneas de los demás.
