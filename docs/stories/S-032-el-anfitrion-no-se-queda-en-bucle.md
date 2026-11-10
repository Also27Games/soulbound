# S-032. El anfitrión no se queda en bucle: una respuesta rechazada deja pasar el turno

**Sistema**: anfitrión de partida (glosario "MatchHost", S-005) y turnos (S-009: una elección vencida se resuelve como "no", `ChoiceExpired`). **Módulos**: `Soulbound` (`Match/MatchHostSubsystem`, y una pieza sin mundo junto a `StepClock` y `Curtain`). `SoulboundCore` no cambia: la regla ya está en el motor; lo que falla es que el anfitrión no deja que se aplique. **Rama base**: `main`, con S-030 fusionada. **Tamaño**: pequeño. Es una corrección.

## Contexto

El 10/11/2026, probando S-030, el juego se quedó bloqueado en PIE tras `SbClock 2390`. El acta de esa partida acababa en la línea del comando, sin cierre. El agente de S-030 lo reprodujo sin ventana (`SbEvent elige-tu-veneno` y `SbClock 2390`: 3 de 4 semillas se cuelgan) y encontró la causa, presente en `main` desde S-010:

- En `UMatchHostSubsystem::Advance`, la rama que entrega la respuesta de un dummy a una elección hace `continue` tras entregarla, la acepte el motor o no.
- Si el reloj de partida ha pasado el plazo de esa elección, `SubmitChoiceIntent` devuelve `ClockExpired` sin cambiar nada.
- El `continue` vuelve al principio del bucle sin llegar a `AdvanceTurn`, que es quien caduca la elección. El dummy recalcula su hora de respuesta, que con el plazo vencido cae en el pasado, contesta al instante, lo rechazan, y así para siempre: un hilo al 100 % y el juego parado.

Hoy solo se ha visto con el comando que adelanta el reloj, pero las ramas del voto, el Oráculo, las pruebas y el duelo tienen el mismo patrón, y el anfitrión no debe poder colgarse por ningún camino.

## Reglas a implementar

- **R-ANF-1 (precisión; identificador propuesto). Solo se repite la vuelta si el motor aceptó.** En `Advance`, después de entregar al motor la respuesta de un decisor (elección, voto, Oráculo, destino, prueba de habilidad, duelo), el bucle vuelve al principio solo si el motor la aceptó (`EIntentResult::Accepted`). Con cualquier otro resultado, la respuesta pendiente se descarta y la vuelta sigue hasta `AdvanceTurn` en esa misma iteración, para que el motor caduque lo que esté vencido. La decisión es una función sin mundo (`Soulbound::Match::AfterAnswer(EIntentResult)`, nombre propuesto, devuelve repetir o seguir) que usan todas las ramas: una sola regla en un solo sitio.
- **R-ANF-2 (precisión). El bucle tiene tope.** El `while` de `Advance` cuenta sus vueltas con una pieza sin mundo (`FAdvanceGuard`, nombre propuesto). Si en una sola llamada supera el tope (constante del anfitrión, 1000 vueltas: una llamada normal da menos de diez), sale del bucle y deja un `Error` en el log y una línea `host` en el acta con la fase del turno, el asiento activo, si hay elección, voto u Oráculo abiertos y el reloj. La partida sigue en el siguiente tick. El tope no arregla nada: garantiza que un fallo como este se vea en el acta en vez de congelar el juego.
- **R-ANF-3 (precisión). Sin cambiar lo demás.** Con respuestas aceptadas el comportamiento es idéntico al de hoy: mismos retardos de los dummies, mismo orden de eventos, mismas partidas con la misma semilla.

## Criterios de aceptación

Automáticos, sin mundo (`Source/Soulbound/Private/Tests/AdvanceGuardTest.cpp`, nombres `Soulbound.Game.Match.Anfitrion.<QueSeComprueba>`):

- **CA-ANF-1** (R-ANF-1). `AfterAnswer` dice repetir con `Accepted` y seguir con cada uno de los demás valores de `EIntentResult`; el test los recorre todos, para que un valor nuevo del enum no quede sin decidir.
- **CA-ANF-2** (R-ANF-2). `FAdvanceGuard` con tope 1000 permite 1000 vueltas y niega la 1001; reiniciado, vuelve a permitirlas.
- **CA-ANF-3** (contrato del motor del que depende R-ANF-1). Con una elección abierta de un asiento y el reloj más allá de su plazo: `SubmitChoiceIntent` devuelve `ClockExpired` y no cambia el estado; `AdvanceTurn` con ese mismo reloj deja `ChoiceExpired` y cierra la elección. Si ya hay un test del motor que lo comprueba (S-009), se cita en el PR en vez de duplicarlo.

Sin ventana, por quien implemente, con el acta como prueba (no necesitan ratón):

- En `main` antes del arreglo, la reproducción de S-030 (`-ExecCmds` con `SbEvent elige-tu-veneno` y `SbClock 2390`, semillas 1 a 4) se cuelga en 3 de 4. Con el arreglo, las cuatro terminan y sus actas acaban en `# --- cierre ---`, con el `ChoiceExpired` del dummy antes del final.
- Lo mismo con una votación abierta y con una consulta del Oráculo abierta al adelantar el reloj, si hay eventos de datos que las abran.
- Con una semilla fija y sin comandos, el acta de 120 s de partida es idéntica, línea a línea, antes y después del arreglo (R-ANF-3), quitando la cabecera.

Manual, por el autor, en PIE:

- Con un dummy recién llegado a una casilla compartida o con un dilema abierto (`SbEvent elige-tu-veneno`), `SbClock 2390`: el juego no se para y la partida termina a los diez segundos.

## Notas para quien lo implemente

- Los `continue` están tras `SubmitVote`, `SubmitChoiceIntent`, `SubmitOracleConsult` / `SubmitOracleRefuse`, los dos `SubmitSkillTrialResult` y `SubmitDuelPerformance`. Revisar cada rama: al no aceptar, `Pending.bPending` se apaga y no se reprograma en esa vuelta.
- La hora de respuesta de un dummy con el plazo vencido sale en el pasado (`Now + min(retardo, mitad de lo que queda)`, con lo que queda negativo). No hace falta cambiar la fórmula: con R-ANF-1 la elección caduca antes de que vuelva a usarse.
- Los restos de las pruebas sin ventana (actas y logs propios) se borran por nombre exacto al acabar; nada más de `Saved`.
- Tests primero, en rojo.
- Al cerrar: la bitácora enlaza con el hallazgo anotado en la entrada de S-030.

## Fuera de esta story

- Sacar el bucle entero de `Advance` al motor para probarlo sin mundo: es una refactorización grande; queda como deuda con su `TODO`.
- Los 3 fotogramas por segundo de la sesión del 9/11/2026: sin datos para investigarlo.
