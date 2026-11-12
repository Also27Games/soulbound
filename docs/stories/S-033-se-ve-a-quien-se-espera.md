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

Añadido por el autor el 11/11/2026, tras probar el PR en PIE:

- **El desenlace también se ve**: tras un duelo el autor tuvo que mirar el log para saber quién había ganado, y de las decisiones de los dummies se ve que deciden pero no qué. El panel enseña unos segundos cómo acaba lo que no se ve en el tablero: quién gana un duelo, qué opción de un dilema sale, qué roba el trilero y si otro asiento supera su prueba. Lo que el tablero ya enseña (dados, salto, expulsión) no se repite.

Añadido por el autor el 12/11/2026, tras probar el desenlace en PIE:

- **Señalar también es un desenlace**: cuando otro asiento señala a un rival (para enseñar su Brazal, para un duelo), no se veía a quién.
- **Sin "al activo"**: el texto de las opciones de un dilema lleva pegada la dirección del efecto ("Perder una runa (a elección) al activo"), que para el jugador no significa nada.
- **Lo iluminado tiene que verse**: con el Altar como único destino, el panel dice "Elige una casilla iluminada" y no se ve ninguna: el halo del Altar queda tapado por su pieza del kit. El clic en el Altar funciona.
- **A 1024x768 nada se sale por la izquierda**: la frase del Warden y un título largo de la espera ("Asiento 1 · Koschei decide cuántos dados tira") se cortan por el borde izquierdo.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-DEC-5 (precisión). Qué se enseña mientras esperas.** Una función pura junto a `DecisionView` (`WaitView`, nombre propuesto) recibe el estado y el asiento humano y devuelve, si la mesa espera a otro asiento, un `FDecisionView` con un tipo de entrada nuevo (`EDecisionInput::Wait`, nombre propuesto), sin opciones. No inventa nada: quién decide y qué lo dice el estado, y los textos salen de los que ya existen (`ChoiceWhat`, los nombres de asiento de S-028, el nombre y el texto del evento del Warden). Casos, en el mismo orden en que el panel de estado los enseña:
  1. **Elección de otro asiento** (`FPendingChoice` abierta para él): quién elige y qué. En las de un evento del Warden, el título es el nombre del evento y el contexto su texto. Plazo: el de la elección.
  2. **Destino de otro asiento** (su turno, fase de tirada): de quién es el turno y su tirada. Plazo: el reloj de turno.
  3. **Consulta del Oráculo de otro asiento**: quién está ante el Oráculo. Plazo: el de la consulta.
  4. **Votación abierta en la que el humano ya votó o no puede votar**: el nombre del evento y qué asientos faltan por votar (los que pueden votar y aún no lo han hecho). Plazo: el de la votación.
  5. **Prueba de habilidad o duelo de otros asientos** (Fuente, Prueba del Warden, escape del Vacío, desafío final, duelo): quién juega y qué; en el duelo, los dos asientos. Sin plazo: no lleva barra.
- **R-DEC-6 (precisión). Una sola vista para el panel.** Una función pura (`PanelView`, nombre propuesto) decide qué enseña el panel: la decisión del humano (`DecisionView`) si existe; si no, la espera (`WaitView`); si no hay ninguna, nada. Mientras la mesa espera al Warden no se enseña la espera de otros: lo que pasa es la frase del Warden, que ya está en pantalla. La decisión propia sí se sigue enseñando bloqueada, como en S-028.
- **R-DEC-8 (precisión). El resultado de la votación.** Al resolverse una votación (`VoteResolved` en el registro), el panel enseña el resultado durante `VoteResultSeconds` (Config, valor inicial 5, en segundos del reloj de partida): de título, el nombre del evento; después, el más votado con sus votos, y si desempató el Warden; debajo, una línea por voto (quién votó a quién, de los `VoteCast` de esa votación, marcando el que salió al azar por no votar a tiempo). Sin botones ni barra. Una función pura (`VoteResultView`, nombre propuesto) lo saca del registro y del reloj: hay vista si el último `VoteResolved` tiene menos de `VoteResultSeconds`. En `PanelView` va después de la decisión propia y antes de la espera, y a diferencia de la espera se enseña también mientras habla el Warden. No dice qué le pasa al más votado: eso lo dicen el Warden y la crónica.
- **R-DEC-10 (precisión). El desenlace.** Una función pura (`OutcomeView`, nombre propuesto) saca del registro el último suceso de una lista cerrada y, si tiene menos de `OutcomeSeconds` (Config, valor inicial 4, en segundos del reloj de partida), devuelve una vista de resultado (`EDecisionInput::Result`): título y una línea, sin botones ni barra. Los asientos se nombran como en el resto del panel. La lista:
  1. **Duelo** (`DuelWon`): título "Duelo"; línea "Gana <ganador> a <perdedor>". Contra el Warden, título "Duelo contra el Warden": con `WardenDuelWon`, "Gana el Warden: <asiento> pierde lo que estaba en juego"; con `WardenDuelLost`, "Gana <asiento>: no pierde nada". Se enseña también cuando el humano es uno de los dos.
  2. **Dilema** (`DilemmaChosen`): título, el nombre del evento; línea "<asiento> elige: <texto de la opción>" o, si la sorteó el Warden por no contestar a tiempo (`Detail` 1), "El Warden elige por <asiento>: <texto de la opción>". El texto de la opción es el del botón de S-028. No se enseña la opción que el propio humano eligió.
  3. **Trilero** (`RuneStolen`, `ShellGameExpired`): título "Trilero"; línea "<ladrón> roba a <víctima> la runa de <Dominio>" o "<ladrón> no elige a tiempo y no roba nada". La runa se nombra como en los botones de S-028.
  4. **Prueba de otro asiento** (`SkillMinigamePassed`/`Failed`, `WardenTrialPassed`/`Failed`, `VoidEscapePassed`/`Failed`, `FinalChallengeFailed`): título, el nombre de la prueba (el de la espera, R-DEC-5); línea "<asiento> la supera" o "<asiento> falla". Las del humano no: su resultado ya lo enseña el anillo.
  5. **Señalar** (`EffectApplied` con `OtherSeat`, quien señaló, distinto de `Seat`, el señalado): título, el nombre del efecto; línea "<quien señala> señala a <señalado>". No se enseña cuando quien señala es el humano; sí cuando el señalado es él.
  En `PanelView` va donde el resultado de la votación: después de la decisión propia y antes de la espera, también mientras habla el Warden. Entre el resultado de una votación y un desenlace, manda el suceso más reciente. No inventa nada: quién, a quién y qué lo dice el registro.
- **R-DEC-11 (precisión). El texto de una opción, para el jugador.** En los botones de un dilema (S-028) y en su desenlace, la dirección del efecto no se dice cuando recae en quien elige (`EEffectTarget::ActivePlayer`): "Perder una runa (a elección)", no "… al activo". Las demás direcciones se conservan ("al líder", "a todos", "al rival señalado"). Una función pura junto a `DescribeOption`; la crónica y el acta siguen con el texto de hoy.
- **R-DEC-7 (precisión). Cómo se pinta.** `SDecisionPanel` pinta la vista de espera con el título en el gris de lo secundario, sin botones y con la barra en gris; la primera línea nombra al asiento ("Asiento 2 · nombre del Elegido", "Asiento 2 (Eco)", como en los botones de voto de S-028). Las reglas de visibilidad son las de R-DEC-3: no se enseña con el anillo del humano abierto, con el telón bajado ni con los paneles ocultos (`H`). El panel de estado no cambia: sigue llevando sus líneas con el detalle.
- **R-DEC-9 (precisión). Posición fija.** El borde inferior del panel queda a una altura fija sobre el borde de la pantalla: la que ocupa una frase del Warden de `WardenLineReservedLines` líneas (Config, valor inicial 2) con su fondo y sus márgenes. Ese hueco se reserva siempre, haya frase o no, y la frase se pinta dentro de él, pegada abajo como hoy. El panel no cambia de sitio cuando la frase aparece, cambia de largo o desaparece; sí crece hacia arriba si la decisión tiene más botones. Si una frase ocupa más líneas de las reservadas, se sale del hueco por arriba y el panel sigue quieto: se anota en el PR, no se arregla aquí (es el `TODO(R-HUD-3)` de la frase a 1024x768).
- **R-DEC-12 (precisión). Lo iluminado se ve.** Toda casilla alcanzable enseña su halo con las piezas del kit, también el Altar, que hoy lo tapa. Vale agrandar el halo del Altar hasta asomar por su borde o pintarlo por encima; sin assets nuevos. Se comprueba también en Nicho, Fuente y Nodo.
- **R-DEC-13 (precisión). Dentro del hueco, a cualquier tamaño.** Ni la frase del Warden ni el panel se salen del hueco libre del tablero: lo que no cabe en una línea se parte. El ancho en que se parte sale del ancho real del hueco (la geometría de la capa), no de una cuenta con el tamaño de la ventana, que a 1024x768 da de más. Los textos del panel dejan `AutoWrapText` por la misma razón que la frase.

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

- **CA-DEC-19** (duelo, R-DEC-10). Con un `DuelWon` del asiento 2 sobre el 0 en el segundo 100 y el humano en el 0: en el segundo 101 hay vista de resultado que nombra al 2 como ganador y al 0 como perdedor; pasado `OutcomeSeconds`, no hay vista. Con `WardenDuelWon` y con `WardenDuelLost`, cada una con su línea.
- **CA-DEC-20** (dilema, R-DEC-10). Con un `DilemmaChosen` del asiento 2, opción 1: el título es el nombre del evento y la línea lleva el texto de esa opción. Con `Detail` 1, la línea dice que elige el Warden. Si quien eligió es el humano, no hay vista; si al humano se la sorteó el Warden, sí.
- **CA-DEC-21** (trilero, R-DEC-10). Con un `RuneStolen` del asiento 1 al 3, la línea nombra al ladrón, a la víctima y la runa; con `ShellGameExpired`, dice que no roba nada.
- **CA-DEC-22** (prueba de otro, R-DEC-10). Con un `SkillMinigamePassed` del asiento 2, la vista lleva el nombre de la prueba y dice que la supera; con `SkillMinigameFailed`, que falla; con el mismo suceso del asiento humano, no hay vista.
- **CA-DEC-23** (orden, R-DEC-6 y R-DEC-10). Con un desenlace reciente y una decisión abierta para el humano, `PanelView` es su decisión; con una elección abierta para un dummy, es el desenlace; con la mesa esperando al Warden, el desenlace se sigue enseñando. Con un resultado de votación y un desenlace recientes, `PanelView` es el del suceso más nuevo. Un suceso que no está en la lista (`DiceChosen`, `PlanarJumped`, `Expelled`) no da vista.
- **CA-DEC-24** (señalar, R-DEC-10). Con un `EffectApplied` de Revelar Brazal sobre el asiento 3 señalado por el 1, la vista lleva el nombre del efecto y dice que el 1 señala al 3. Señalado por el humano, no hay vista; con el humano de señalado, sí. Un `EffectApplied` sin señalador no da vista.
- **CA-DEC-25** (texto de opción, R-DEC-11). La opción "Perder una runa, a elección, al activo" se lee "Perder una runa (a elección)" en el botón del dilema y en su desenlace; una opción dirigida al líder conserva "al líder"; `DescribeOption` no cambia.

Manuales, por el autor, en PIE, a 1024x768 y a 1920x1080, con captura:

- En el turno de cada dummy el panel dice de quién es el turno; si elige algo, qué.
- En una votación (`SbEvent juicio`), tras votar el panel pasa a decir quién falta y se va al cerrarse la votación.
- Al cerrarse la votación sale el resultado, se lee sin prisa y se va solo: quién es el más votado y qué votó cada asiento.
- Con el Oráculo de un dummy (`SbEvent el-orbe-llama` hasta que le toque a uno), el panel dice quién está ante él.
- La espera no se confunde con una decisión propia: sin verde y sin botones.
- Con una decisión abierta, el panel no se mueve cuando la frase del Warden aparece, pasa de una línea a dos o desaparece. A 1024x768 no tapa las casillas iluminadas de abajo más que en S-028; si lo hace, se dice en el PR.
- Si el panel aparece y desaparece tan deprisa que molesta, se dice en el PR: no se arregla en esta story.
- Tras un duelo contra un dummy, al cerrarse el anillo el panel dice quién gana y a quién.
- Con el dilema de un dummy (`SbEvent elige-tu-veneno` hasta que le toque a uno), el panel dice qué opción sale; con un robo entre dummies, qué roba el trilero.
- Si los desenlaces tapan la espera siguiente o se pisan unos a otros hasta no leerse, se dice en el PR.
- Cuando un dummy señala a un rival (`SbEvent hazlo-tu` u otro que haga señalar), el panel dice quién señala a quién.
- Junto al Altar con `SbRoll 1`, el Altar se ve iluminado, a los dos tamaños; lo mismo un Nicho, una Fuente y un Nodo alcanzables.
- A 1024x768, la frase del Warden y los títulos largos de la espera se leen enteros, partidos en dos líneas si hace falta, sin cortarse por la izquierda.

## Notas para quien lo implemente

- Todo parte de `DecisionText` (S-028): `WaitView` y `PanelView` van en el mismo archivo y `TickHud` pasa a llamar a `PanelView`. El reloj de la barra (`AdvanceDecisionClock`) sirve tal cual.
- Los asientos que faltan por votar: los que tienen `State.Vote.Votes[asiento] == INDEX_NONE` y `VoteOptions` no vacío. No lo decide el panel.
- El panel ya recibe el tipo de entrada: el modo espera es un caso más de `Rebuild`, sin botones.
- Las teclas no cambian: `OnChoiceKey` sigue mirando `DecisionView`, no `PanelView`, para que una tecla nunca conteste algo que el panel enseña como espera.
- La posición fija se hace en la capa de la vista (`WardenLineOverlay`, `BuildOverlay`): hoy el panel y la frase van en un `SVerticalBox` de alto automático; la frase pasa a ir dentro de una caja de alto fijo. El alto sale del tamaño de letra de la frase (`WardenLineFontSize`) y de sus márgenes, no de un número a mano.
- Tests primero, en rojo. El pintado y la posición solo tienen criterio manual.
- El resultado sale del registro (`FMatchState::EventLog`): `VoteCast` lleva quién vota (`Seat`), a quién (`OtherSeat`) y si fue al azar (`Detail`); `VoteResolved`, el más votado (`Seat`), sus votos (`Value`) y si hubo desempate (`Detail`). La edad se mide con `ClockSeconds` del evento y del estado, sin reloj real.
- El desenlace (R-DEC-10) sigue el patrón de `VoteResultView`: el último suceso de la lista en `FMatchState::EventLog` y su edad con `ClockSeconds`. En el dilema, el evento es el del `WardenEventStarted` anterior en el registro. Lo que lleva cada suceso (`Seat`, `OtherSeat`, `Value`, `Detail`) está en su comentario de `MatchState.h`.
- Nombres propuestos para el glosario, en el PR: `WaitView`, `PanelView`, `VoteResultView`, `OutcomeView`, `EDecisionInput::Wait`.

## Fuera de esta story

- Un tiempo mínimo en pantalla para que la espera no parpadee (el resultado de la votación sí tiene el suyo, R-DEC-8), y cambiar lo que tardan los dummies en contestar (`DummyResponseSeconds`, datos del autor).
- Resaltar en el tablero la ficha de quien decide: es el aro de S-029.
- Enseñar los dados que elige un dummy, su salto planar o a quién expulsa: se ve en el tablero y lo cuenta la crónica.
- El rendimiento de cada uno en el duelo, en cifras: el panel dice quién gana, no por cuánto. La palabra "rendimiento" sale del anillo en S-031.
- Quitar del panel de estado las líneas de los demás.
