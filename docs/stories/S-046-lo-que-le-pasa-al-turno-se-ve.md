# S-046. Lo que le pasa al turno se ve: el turno perdido y el turno extra se anuncian, y la ficha cortada no salta

**Sistema**: turnos (GDD 02; R-TUR-15, el registro) e interfaz en plano (GDD 07; pilar 2, el turno de los demás también es tu turno, y pilar 3, caos legible): panel de decisión en su modo de desenlace (R-DEC-10, S-033) y fichas (S-020). **Módulos**: `SoulboundCore` (un evento nuevo en el registro) y `Soulbound` (presentación: `DecisionText`, `EventText`, `SeatTokenActor`, `BoardViewActor`). **Rama base**: `main`, con S-043 fusionada. **Tamaño**: pequeño. **Sin assets**.

## Contexto

En la prueba del 15/11/2026 con la build de probadores (P4) y en las pruebas del autor del 16/11/2026 se vieron tres cosas del turno que pasan sin que la pantalla las cuente:

1. **Un turno perdido no se anuncia.** Cuando un jugador pierde el turno, el turno simplemente no ocurre. Solo queda una línea en la crónica.
2. **Un turno extra tampoco.** El mismo jugador vuelve a jugar y nada dice por qué.
3. **La ficha cortada salta.** Si el Warden corta un movimiento cuando la ficha va entre dos casillas, la ficha aparece de golpe en la casilla anterior. P4 lo contó como "me devolvió a mi casilla anterior". La regla es correcta (la ficha se queda en la última casilla en la que entró, R-TAB-26); lo que confunde es el salto.

Decidido por el autor el 16/11/2026 (ninguna regla cambia):

- El turno perdido y el turno extra se anuncian en el panel de abajo, como los demás desenlaces.
- La ficha cortada vuelve a su casilla deslizándose, no de golpe.

## Reglas a implementar

### `SoulboundCore`

- **R-TUR-17 (nueva, precisión de R-TUR-15). El turno extra deja su evento.** Cuando un asiento conserva el turno porque tiene un turno extra (hoy `LeaveEnd` descuenta `FSeatState::ExtraTurns` sin dejar rastro), el registro lleva un evento nuevo (nombre a proponer; por ejemplo `ExtraTurnTaken`), con el asiento y, en `Value`, los turnos extra que le quedan después. Se registra en el instante en que se decide, antes del `TurnStarted` de ese turno extra. No cambia cuándo se concede ni cuándo se consume un turno extra, ni cómo se cancela con un turno perdido.
  - El turno perdido ya deja `TurnSkipped`: no cambia.

### `Soulbound` (`Presentation/`)

- **R-DEC-19 (nueva, ampliación de R-DEC-10). El turno perdido y el turno extra son desenlaces.** El panel de decisión, en su modo de desenlace (`OutcomeView`, el que enseña unos segundos "Gana…", "falla…"), enseña también:
  - `TurnSkipped`: "El asiento N pierde este turno"; si es el humano, "Pierdes este turno".
  - el evento del turno extra: "Turno extra del asiento N"; si es el humano, "Tienes un turno extra".
  - Duran lo que los demás desenlaces (`OutcomeSeconds`) y no detienen la mesa.
  - A diferencia de otros desenlaces, estos dos se enseñan también cuando son del humano: es justo cuando más hace falta saberlo.
  - La crónica y el acta escriben el evento nuevo: "el asiento N juega un turno extra".
- **R-VIS-32 (nueva, precisión de S-020). La ficha cortada se desliza.** Cuando el estado coloca la ficha en una casilla distinta de aquella hacia la que caminaba (un corte a medio camino, o que el motor vaya por delante), la ficha va a esa casilla deslizándose, con el deslizamiento que ya usan los movimientos que no son un camino (`TokenSlideSeconds`), en vez de aparecer en ella de golpe. Si la ficha ya está en la casilla, no se mueve.
  - No cambia la casilla en la que queda (R-TAB-26) ni cuándo se entrega cada avance del camino.
- **Lo que no cambia.** Las reglas de los turnos perdidos y extra, el corte de turno, la señal de corte (S-041) y la velocidad de las fichas al caminar.

## Criterios de aceptación

Automáticos, sin mundo.

En `SoulboundCore`:

- **CA-TUR-19** (R-TUR-17). Un asiento con un turno extra que termina su turno deja el evento del turno extra con su asiento y 0 turnos restantes, y después el `TurnStarted` del mismo asiento. Con dos turnos extra, deja el evento con 1 restante. Sin turnos extra, no lo deja y el turno pasa al siguiente. Un turno extra cancelado por un turno perdido no deja el evento.
- **CA-TUR-20** (R-TUR-17, guarda). Una partida simulada con una semilla dada deja el mismo registro que antes salvo por los eventos nuevos, que aparecen justo donde un asiento repite turno.

En `Soulbound`:

- **CA-DEC-35** (R-DEC-19, textos). El desenlace de un `TurnSkipped` del asiento 2 es "El asiento 2 pierde este turno" y el del humano, "Pierdes este turno". El del turno extra del asiento 2 es "Turno extra del asiento 2" y el del humano, "Tienes un turno extra".
- **CA-DEC-36** (R-DEC-19, cuándo). Se enseñan durante `OutcomeSeconds` desde su instante y después no; los del humano se enseñan igual que los de otros asientos; los desenlaces que ya existían siguen saliendo como antes.
- **CA-ACT-9** (R-DEC-19). El texto del evento nuevo en la crónica y en el acta es "el asiento N juega un turno extra".
- **CA-VIS-33** (R-VIS-32). La función que decide cómo llega la ficha a la casilla que dice el estado devuelve "deslizarse" cuando esa casilla no es la siguiente de su camino y la ficha no está ya en ella; "nada" cuando ya está en ella; y el caminar de siempre cuando es la siguiente de su camino.

Manuales, por el autor, en PIE, con captura:

- **Turno perdido**: tras un evento que quita un turno a un dummy (o `SbEvent` con uno que lo haga), al llegar su turno el panel de abajo dice "El asiento N pierde este turno". Lo mismo con el humano: "Pierdes este turno".
- **Turno extra**: tras el evento "Prisa" (`SbEvent prisa`) sobre un dummy y sobre el humano, al repetir turno el panel lo dice.
- **La ficha cortada**: con `SbStrike tropiezo` mientras su ficha camina entre dos casillas, y lo mismo con la de un dummy, la ficha vuelve deslizándose a la casilla en la que estaba; no aparece de golpe.
- Los desenlaces de siempre (duelo, dilema, trilero, prueba de otro) siguen saliendo.

## Notas para quien lo implemente

- **El turno extra** se consume en `LeaveEnd` (`TurnRules.cpp`), donde también se cancela contra los turnos perdidos (`SettleExtraAgainstLost`). El evento nuevo se añade a `EMatchEventType` al final de la enumeración, para no cambiar el valor de los demás en el registro, y pasa por `LogEvent`.
- **Los desenlaces** están en `OutcomeView` y sus funciones (`Presentation/DecisionText.cpp`, S-033): hoy descartan los sucesos del humano; estos dos no.
- **La ficha.** `ASeatTokenActor::WalkPath` recorre el camino; cuando el motor corta o va por delante, la vista coloca la ficha donde dice el estado (decisión del autor del 21/10/2026). Ahí está el salto. El deslizamiento que ya existe es el de los movimientos que no son camino (Vacío, hogar, Expulsión, salto planar).
- **Los textos** de `EventText.cpp` para la crónica y el acta.
- Si S-043 ya está fusionada, los prefijos de las frases del Warden van en formato de guion; esta story no añade frases del Warden.
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- Cambiar a qué casilla va la ficha cortada, o cuántos turnos extra o perdidos puede acumular un asiento.
- Que el Warden diga una frase al perderse o repetirse un turno.
- Animaciones nuevas de la ficha.
- El evento "Prisa" que se anula solo cuando el turno extra se pierde por otro efecto (observación apuntada, sin story).
