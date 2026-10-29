# S-023. La mesa espera al Warden: la partida se detiene mientras habla, salvo en los comentarios

**Sistema**: el Warden (GDD 06: R-PRO-1 a R-PRO-4, R-WAR-10, R-VOZ-3b), interfaz (GDD 07: R-HUD-4). **Módulos**: `SoulboundCore` (un evento nuevo, una frase programada nueva, una función que desplaza plazos) y `Soulbound` (la espera en el anfitrión, los Dominios en la vista). **Rama base**: `main`, con S-022 (PR #52) fusionada; si la rama sale antes, fusionar `origin/main` al empezar. **Tamaño**: medio.

## Contexto

Con las frases reales en la mesa (29/10/2026), el autor vio tres cosas en PIE:

1. Dos Dominios empiezan Olvidados, pero el Warden dice "¡Este Dominio se apaga!" dos veces en la apertura mientras el tablero ya los enseña apagados desde el primer fotograma: suena a que los cierra en ese momento y no pasa nada.
2. Cuando alguien se vincula, el Warden presenta al Elegido, y mientras habla el siguiente jugador ya está tirando.
3. Cuando el Warden golpea, su frase sale mientras el siguiente turno ya corre. El golpe es su número: si aparece para dar su frase pero la partida sigue, el número no vale nada.

Decidido por el autor el 29/10/2026:

- **La mesa espera al Warden.** Cada frase del Warden que no sea un comentario detiene la mesa mientras se enseña: presentación de un Elegido, golpe, escena, castigo de insolencia, Furia, fin de la carrera, aspirante. Los **comentarios** no detienen nada (son el Warden sin materializarse, R-COM-1), y los **avisos del final** tampoco (sería absurdo parar el reloj mientras dice que se acaba el tiempo). Las **instrucciones** no detienen nada: son de cada máquina (perfil) y en red el anfitrión no puede parar la mesa por lo que ve un cliente.
- **El reloj de partida no se para nunca.** Los 40 minutos empiezan cuando el Warden termina el prólogo y corren hasta el final; una espera del Warden detiene **los relojes de turno y de elección**, no el de partida. Lo que hoy hace el prólogo con el reloj entero (S-022) se queda como está: es el único momento en que el reloj de partida aún no ha empezado.
- **"Que empiece el juego."** El prólogo termina con una frase propia, `programado:comienzo`, la última de la apertura, que marca el arranque del reloj de partida.
- **Dominios Olvidados en la apertura.** Todos los Dominios se ven encendidos mientras el Warden da la bienvenida, y cada frase de corrupción apaga el suyo al enseñarse. Para que la vista sepa cuál, el motor deja un evento `DomainForgotten` (nombre propuesto) con el Dominio justo antes de cada frase de corrupción; hoy esa decisión del inicio no deja rastro en el registro, y R-WAR-10 dice que todo cambio lo deja.

Fuera: la aparición del Warden en escena, el foco sobre el jugador, la cámara y las cinemáticas (R-VOZ-4, R-PRO-5): story de interfaz después del test de usabilidad. Aquí la espera es solo tiempo: la mesa se congela y la frase se lee.

## Reglas a implementar

### Motor (SoulboundCore)

- **R-PRO-1. La frase de comienzo.** `StartMatch` dice `programado:comienzo` (`Soulbound::Warden::LineBegin`, nombre propuesto) en el instante cero, después de la última posición del orden. El cargador la exige: `frases.json` pasa a 84 claves, el agente deja `(pendiente) programado:comienzo` y la frase real es del autor.
- **R-WAR-10. Dominio Olvidado.** Evento `DomainForgotten` (Value el Dominio), registrado en `StartMatch` por cada Dominio que empieza Olvidado, justo antes de su `programado:corrupcion`, en el mismo orden en que hoy se dicen las frases. Nada más cambia: el estado de los Dominios ya se decidía ahí.
- **Desplazar los plazos.** `ShiftOpenDeadlines(State, Seconds)` (nombre propuesto, `Match/TurnRules.h`): suma `Seconds` a todos los plazos abiertos del estado, los que están en segundos del reloj de partida y esperan a alguien: `TurnDeadlineSeconds` con un turno en curso, `PendingChoice.DeadlineSeconds` con una elección abierta, `ShellGame.StageDeadlineSeconds` con un trilero en etapa de reloj, la salvaguarda del movimiento (`StepDeadlineSeconds`) con un camino en curso, `Vote.DeadlineSeconds` con una votación abierta y `Oracle.DeadlineSeconds` con una consulta abierta. **No toca** el reloj de partida, el fin de la carrera, `matchEndSeconds`, la evaluación siguiente del Warden ni su garantía (`NextEvaluationSeconds`, `LastEffectSeconds`), ni `LastProgressSeconds` ni `LastTargetedSeconds`: la espera no es progreso ni es ser objetivo. Con `Seconds` cero o negativo no hace nada. Devuelve cuántos plazos movió. Es una función del anfitrión que vive en el motor porque toca el estado; la simulación no la llama y las cifras de S-018 no cambian.

### Anfitrión y vista (Soulbound)

- **La espera del Warden.** `WardenWaitSeconds(State, FromIndex, LineSeconds)` (nombre propuesto, `Presentation/WardenLineQueue.h`, junto a `OpeningSeconds`): los `WardenSpoke` del registro desde `FromIndex` cuya clave detiene la mesa (todas menos `comentario:*`, `programado:ultimo-minuto`, `programado:diez-segundos`, y las del instante cero, que son del prólogo) por `LineSeconds`. El anfitrión, en cada avance, mira los eventos nuevos; si hay espera, **detiene la mesa** ese tiempo real: ni dummies, ni turno, ni intención del humano (la que llegue se rechaza con el motivo en el log, como en el prólogo), ni evaluación del Warden; el reloj de partida sigue (`FStepClock` no se toca para esto). Al vencer, `ShiftOpenDeadlines(State, EsperaReal)` y las respuestas programadas de los dummies se retrasan lo mismo. Si durante la espera llegan más frases que detienen (una presentación detrás de un golpe), la espera se alarga. El HUD dice "El Warden habla" en la línea del turno mientras dura, como el prólogo.
- **Lo que no espera**: un comentario, un aviso del final, una instrucción, la victoria y el fin por tiempo (la partida ya ha terminado: la pantalla del resultado se queda).
- **Dominios en la vista.** Mientras dura el prólogo, los Dominios Olvidados del inicio se pintan como en juego; cuando la línea del Warden enseña una `programado:corrupcion`, la vista apaga el Dominio del `DomainForgotten` que la precede (suelo, aro, leyenda). Al terminar el prólogo, todo como dice el estado, pase lo que pase con las líneas. Fuera del prólogo nada cambia: un Dominio se apaga cuando el estado lo dice.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-VOZ-2** (ampliado). El cargador exige `programado:comienzo`; las programadas pasan a 15.
- **CA-VOZ-3** (ampliado). `StartMatch` deja `programado:comienzo` como última frase del instante cero, a la mesa, después de la última del orden; y un `DomainForgotten` por Dominio que empieza Olvidado, con su Dominio en Value, justo antes de su `programado:corrupcion`; con cuatro jugadores, dos.
- **CA-ESP-1** (plazos). Sobre un estado con un turno en curso, una elección abierta, un trilero en etapa de reloj, un camino con salvaguarda, una votación y una consulta: `ShiftOpenDeadlines(State, 6)` suma 6 a los seis plazos y devuelve 6; no cambia el reloj de partida, `NextEvaluationSeconds`, `LastEffectSeconds`, `LastProgressSeconds` ni `LastTargetedSeconds`; con los plazos cerrados no toca nada y devuelve 0; con 0 o negativo, nada.
- **CA-VOZ-4** (sigue). `LaVozNoCambiaLaPartida` y el informe de S-018 dan las mismas cifras: la simulación no espera.

Automáticos, en `Soulbound`:

- **CA-ESP-2** (qué espera). `WardenWaitSeconds` con 6 s de línea: una presentación, un golpe, una escena, un castigo, la Furia, el fin de carrera y el aspirante dan 6 cada uno; un comentario, un aviso del final, la victoria y el fin por tiempo dan 0; las frases del instante cero dan 0; dos frases que detienen seguidas dan 12; `FromIndex` deja fuera las ya contadas.
- **CA-DOM-1** (vista). Una función pura (`ForgottenDomainsShown(State, ShownLineIndices)` o la que el plan proponga): con el prólogo en marcha y ninguna corrupción enseñada, ningún Dominio se pinta Olvidado; enseñada la primera, el Dominio de su `DomainForgotten`; terminado el prólogo, los del estado.

Manuales, por el autor en PIE con captura:

- Apertura: los seis Dominios encendidos; con cada frase de corrupción se apaga uno; la última frase es la de comienzo y con ella arranca el reloj de partida y el primer turno.
- Un vínculo: la presentación se lee entera y nadie tira hasta que termina; el reloj de turno del siguiente empieza completo.
- `SbStrike` en mitad del turno del humano: el golpe se lee entero con la mesa parada; después sigue la partida; el reloj de los 40 minutos no se ha detenido (comparar `t=` antes y después).
- Un comentario (esperar a uno, o `SbSay comentario:prisa`): la partida no se detiene.
- `SbClock 2335`: el aviso del último minuto no detiene nada.

## Notas para quien lo implemente

- Nombres del glosario: prólogo (`OpeningSeconds`), línea del Warden, comentario, aviso del final, Dominio Olvidado, reloj de turno, reloj de partida, paso a paso (`FStepClock`). Nombres nuevos que propone esta story y confirma el plan: "espera del Warden" (`WardenWaitSeconds`), `ShiftOpenDeadlines`, `DomainForgotten`, `programado:comienzo` (`LineBegin`). Van al glosario con el PR.
- **Reutilizar**: la espera sigue el patrón del prólogo en `UMatchHostSubsystem` (S-022), pero **sin detener `FStepClock`**: el reloj de partida sigue; lo que se detiene es el avance y se compensa con `ShiftOpenDeadlines` al final. Las respuestas programadas de los dummies (`FPendingDummyAnswer`, o como se llame) llevan su hora: se retrasan lo mismo.
- **El paso a paso** sigue igual: si la espera termina entre turnos con el modo activo, la partida se detiene en el hueco como siempre.
- **Qué detiene y qué no** sale de la clave de la frase, no de una lista de tipos de evento: así una frase nueva detiene por defecto y hay que decidir a propósito que no lo haga.
- Si una regla obliga a tocar el sorteo del Warden o la secuencia de eventos de la simulación, para y dilo: CA-VOZ-4 tiene que seguir en verde.
- Tests de `Soulbound` sin mundo, como `WardenVoiceTextTest.cpp` y `BoardViewStateTest.cpp`. El anfitrión solo tiene criterio manual.

## Fuera de esta story

- La frase real de `programado:comienzo`: datos del autor, commit propio.
- La aparición del Warden, el foco de evento, la cámara y las cinemáticas: interfaz, después del test de usabilidad.
- Que las instrucciones detengan la mesa: descartado el 29/10/2026 por la red (cada cliente ve las suyas).
- Un plazo de seguridad al cerrar la sesión en línea para volver al menú (aviso de S-022): story de red.
