# S-026. El anillo: el jugador humano juega sus pruebas de habilidad

**Sistema**: minijuego de habilidad (GDD 09: R-MIN-1 a R-MIN-4, R-MIN-6, R-MIN-8, R-MIN-12 en parte). **Módulos**: `SoulboundCore` (reglas puras del anillo y sus datos; la máquina de turnos no cambia) y `Soulbound` (anfitrión y vista). **Rama base**: `main`. **Tamaño**: mediano. El número S-025 está reservado para los menús propios.

## Contexto

Las cinco pruebas de habilidad del juego (runa en la Fuente de un Dominio Olvidado, Prueba del Warden, escape del Vacío, duelo y desafío final) tienen sus reglas hechas y probadas desde S-008, S-012, S-013, S-015 y S-018, pero el minijuego no existe: por el jugador humano responde un dummy, al azar, y la pantalla lo dice ("resuelto al azar"). La victoria final la decide el azar, con probabilidad 0,35.

El motor ya está preparado: en la fase `SkillTrial` espera sin plazo a `SubmitSkillTrialResult(bPassed)`, y en la fase `Duel` a `SubmitDuelPerformance(0..1)`. El anfitrión tiene un único punto donde hoy contesta el dummy por el humano (`HumanTrialDecider`). El Warden ya aplaza sus evaluaciones durante una prueba (R-WAR-7).

Esta story pone el anillo entre el motor y el jugador, y nada más. Es la versión mínima del GDD 09: lo que se construye es el núcleo del minijuego definitivo, y lo recortado se añade encima en la Fase 2 sin rehacerlo.

Decidido por el autor el 6/11/2026. Cada punto es un recorte del GDD 09 para esta entrega:

- **Solo el anillo.** Sin desenfunde (R-MIN-5, R-MIN-9): el propio GDD lo marca como primer candidato a recorte.
- **Solo el jugador humano.** Los dummies siguen resolviendo como hoy, con su decisor (R-MIN-15 no cambia).
- **Desafío final sin trampas.** Seis pulsos, zona propia, sin fallos (R-MIN-12), pero sin las trampas del Warden.
- **Duelo de un solo lado.** El humano juega tres pulsos y su rendimiento es la media de sus precisiones; el rival dummy rinde con su decisor, como hoy, y el Warden, en su duelo, su valor fijo de datos. La ventaja de Shuten-dōji sigue siendo el margen sumado de S-012 (`duelAdvantageMargin`), no la zona más ancha de R-MIN-7.
- **Con interruptor.** `bHumanPlaysSkillTrials` en la configuración del anfitrión; apagado, el juego hace exactamente lo de hoy.
- **Entrada**: clic izquierdo, sin tecla alternativa ni mando.
- **Dificultad**: los valores iniciales del GDD 09, sin ajustar. Están en datos y el autor los calibra probando en el editor.

Fecha de corte: si el viernes 13/11/2026 la story no está fusionada y probada en el editor, no entra en la build de entrega y queda como primera de la Fase 2.

## Reglas a implementar

### Reglas sin mundo (`SoulboundCore`, `Match/RingRules.h`, nombre propuesto)

Funciones puras, sin estado de partida, con cabecera de comentario cada una. Nombres del glosario: anillo (`RingPulse`), prueba (`SkillTrial`), pulso (`Pulse`), precisión (`Precision`), zona buena (`SweetZone`).

- **R-MIN-2. Precisión de un pulso.** Dado el instante de coincidencia, el instante de la pulsación y la anchura de la zona buena en segundos: 1 en la coincidencia exacta, decrece linealmente hasta 0 en el borde de la zona buena; fuera, 0. Sin pulsación, 0.
- **R-MIN-3. Secuencia de una prueba.** Dadas una semilla, una dificultad y un número de pulsos, devuelve los instantes de inicio de cada pulso (ritmo irregular dentro del rango de datos, valor inicial 0,8 a 2 s) y la duración del cierre. La misma semilla da la misma secuencia. Sin sentido inverso del anillo en esta story: todos se cierran.
- **R-MIN-6, R-MIN-8. Parámetros de cada uso, desde datos.** Para cada `ESkillTrial` y su `ESkillDifficulty`: duración del cierre, zona buena (fracción del cierre), número de pulsos y criterio. Valores iniciales:

| Dificultad | Cierre del anillo | Zona buena | Umbral de media |
|---|---|---|---|
| Fácil | 1,6 s | 0,25 | 0,4 |
| Normal | 1,2 s | 0,18 | 0,55 |
| Difícil | 0,9 s | 0,12 | 0,65 |
| Desafío final | 1,2 s (el de normal) | 0,20 (propia) | sin fallos: toda precisión mayor que 0 |

| Uso | Dificultad | Pulsos | Criterio |
|---|---|---|---|
| Fuente de un Dominio Olvidado | fácil | 2 | media mayor o igual al umbral |
| Prueba del Warden | la de datos (`wardenTrialDifficulty`) | 3 | media mayor o igual al umbral |
| Escape del Vacío | la del peldaño (`PendingDifficulty`) | 3 | media mayor o igual al umbral |
| Duelo | normal | 3 | el rendimiento es la media |
| Desafío final | la de datos (`finalChallengeDifficulty`), con su zona propia | 6 | ninguna precisión 0 |

La dificultad de cada prueba ya la fija el motor en `FMatchState::PendingDifficulty` (fácil en la Fuente, la de datos en la Prueba y en el desafío final, el peldaño en el escape); esta story la lee, no la cambia. La del duelo es nueva y va en datos.

- **R-MIN-4. Resultado.** Dada la lista de precisiones y el uso: superada si la media es mayor o igual que el umbral de la dificultad (Fuente, Prueba, escape); superada si ninguna precisión es 0 (desafío final, R-MIN-12); en duelo, el rendimiento es la media, de 0 a 1.
- **Datos.** Un bloque nuevo en `Content/Data/Match/partida.json` con todo lo anterior, cargado y validado por `MatchParams` como el resto. Ningún valor de las tablas en el código.

### Regla sin mundo del anfitrión (`Soulbound`, `Match/RingTrialRun.h`, junto a `Curtain.h`)

- **`FRingTrialRun`** (nombre propuesto): struct sin mundo, con el patrón de `FCurtain` y `FStepClock`. Recibe el tiempo como dato. Se crea con una secuencia; `Press(Now)` anota la pulsación del pulso en curso (solo la primera de cada pulso; entre dos pulsos no cuenta, R-MIN-2); `Advance(Now)` cierra los pulsos vencidos con precisión 0 si no hubo pulsación; `IsFinished()`; `Precisions()`; y lo que la vista necesita para pintar: índice del pulso en curso y fracción del cierre, de 0 a 1.

### Anfitrión y vista (`Soulbound`)

- **Anfitrión** (`UMatchHostSubsystem`): con `bHumanPlaysSkillTrials` encendido, cuando el asiento humano entra en `SkillTrial` o participa en un `Duel` (como retador o como retado, también contra el Warden), en vez de preguntar a `HumanTrialDecider` crea un `FRingTrialRun` con la secuencia del uso (semilla derivada de la de la partida, como los demás sorteos del anfitrión) tras un anuncio corto (`RingAnnounceSeconds`, Config, valor inicial 1,5). Mientras dura, no entrega nada al motor por ese asiento. Al terminar, convierte las precisiones con las reglas de `RingRules` y llama a `SubmitSkillTrialResult` o a `SubmitDuelPerformance`. `SbTrial` y `SbDuel` siguen mandando: un resultado fijado sustituye al del anillo. Expone a la vista, en solo lectura, la prueba en curso; y `PressRing()` para la entrada.
- **Vista** (`ABoardViewActor`, Slate, como el botón "Volver al menú"): mientras hay prueba del humano, un panel centrado con el nombre del uso, la runa como círculo, el anillo cerrándose hacia ella, el contador "pulso k de N" y, al resolverse cada pulso, su precisión como número y color. Al acabar, "superada" o "fallada" durante un momento. Sustituye al texto "resuelto al azar" solo cuando el interruptor está encendido.
- **Entrada**: clic izquierdo, que durante una prueba no tiene otro uso. La pulsación se fecha con el tiempo del evento de entrada, no con el del fotograma (R-MIN-18).
- **Interruptor**: `bHumanPlaysSkillTrials` (`UPROPERTY(Config, EditAnywhere)`, valor inicial `true` en `Config/DefaultGame.ini`, con comentario). En `false`, anfitrión y vista se comportan como antes de esta story, textos incluidos.

## Criterios de aceptación

Automáticos, en `SoulboundCore` (`Private/Tests/RingRulesTest.cpp`, nombres `Soulbound.Core.Ring.<QueSeComprueba>`). Se conserva la numeración del GDD 09:

- **CA-MIN-1** (R-MIN-2). Con zona buena Z y una pulsación a distancia d de la coincidencia, la precisión es 1 menos d dividido por Z medio, con mínimo 0; en el borde exacto, 0; fuera, 0; sin pulsación, 0.
- **CA-MIN-2** (R-MIN-3). La misma semilla, dificultad y número de pulsos dan dos veces la misma secuencia; semillas distintas, en 50 pruebas, dan al menos dos secuencias distintas; todos los intervalos entre pulsos están dentro del rango de datos.
- **CA-MIN-3** (R-MIN-6, R-MIN-8). Para cada uso, los parámetros devueltos son los de datos: cierre, zona buena, pulsos y criterio. Cambiar un valor en el JSON de prueba cambia el resultado.
- **CA-MIN-6, en parte** (R-MIN-12). El desafío final tiene seis pulsos, el cierre de normal y su zona propia; con las seis precisiones mayores que 0 se supera; con una sola en 0, se falla.
- **CA-MIN-12, nuevo** (R-MIN-4). Con umbral 0,55: precisiones 0,6 / 0,5 / 0,55 superan (media 0,55); 0,6 / 0,5 / 0,5 fallan. En duelo, el rendimiento de 1 / 0,5 / 0 es 0,5.
- **CA-MIN-13, nuevo** (datos). El cargador rechaza, con motivo, un bloque del anillo ausente, un cierre o una zona buena que no sean positivos, una zona buena mayor que 1, un umbral fuera de 0 a 1 y un número de pulsos menor que 1.

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/RingTrialRunTest.cpp`, nombres `Soulbound.Game.Match.Anillo.<QueSeComprueba>`, como `CurtainTest.cpp`):

- **CA-ANI-1** (primera pulsación). En un pulso, dos pulsaciones: cuenta la primera; la segunda no cambia la precisión.
- **CA-ANI-2** (entre pulsos). Una pulsación después de resolverse un pulso y antes de empezar el siguiente no cuenta para ninguno.
- **CA-ANI-3** (sin pulsar). Un pulso que vence sin pulsación queda en 0 y la prueba pasa al siguiente; al vencer el último, `IsFinished()` es verdadero y hay tantas precisiones como pulsos.
- **CA-ANI-4** (pintado). La fracción de cierre vale 0 al empezar el pulso, 1 en la coincidencia y está acotada entre 0 y 1 con cualquier tiempo.

Manuales, por el autor, en PIE, con captura. Para llegar a cada caso se usan los comandos de depuración de S-011:

- Fuente de un Dominio Olvidado, siendo Elegido: salen dos pulsos; acertando los dos se obtiene la runa; sin pulsar, no.
- Prueba del Warden (`SbEvent` con un evento que la imponga): tres pulsos antes de tirar; fallada, el turno termina sin tirada.
- Escape del Vacío (`SbBanish`): tres pulsos al empezar el turno; superado, vuelve a su hogar.
- Duelo contra un dummy: tres pulsos; con tres aciertos altos gana casi siempre; sin pulsar, pierde.
- Desafío final (seis runas y Altar): seis pulsos; un solo fallo lo pierde y el Warden arranca una runa; los seis, victoria.
- `SbTrial pass` y `SbDuel win` durante una prueba: mandan sobre el anillo.
- `bHumanPlaysSkillTrials=false` en el ini: todo como antes de la story, con el texto "resuelto al azar".
- Con el menú de pausa abierto durante una prueba, los pulsos siguen y se pierden: la pausa no detiene la partida. Se anota, no se arregla.

## Notas para quien lo implemente

- **No tocar `TurnRules` ni `DuelRules`.** El motor sigue recibiendo un booleano o un número; lo único nuevo en `SoulboundCore` son las reglas puras del anillo y sus datos. Si el plan necesita cambiar la máquina de turnos, es una duda para el autor, no una decisión.
- **Reutilizar** el patrón de `FCurtain` y `FStepClock` para `FRingTrialRun`: tiempo como dato, sin `UWorld`, sin timers de Unreal.
- **Añadir un bloque obligatorio a `partida.json` rompe los JSON hechos a mano de `MatchParamsTest`** (ya pasó en S-016 B con `furyEscapeLadder`): actualizar sus ayudantes en el mismo commit que los tests en rojo.
- El texto de cada uso en el panel sale de la vista, como el resto de `EventText`; no se añaden frases del Warden.
- El reloj de turno corre durante la Prueba del Warden, como hoy (cuestión abierta del GDD 09). Una prueba de tres pulsos dura entre 5 y 8 s y el reloj de turno es de 30: no se toca en esta story y se deja un `TODO(R-MIN)` donde corresponda.
- Los `TODO(R-MIN)` que ya existen en `TurnRules.cpp` sobre el plazo propio del minijuego se quedan: esta story no pone plazo en el motor; el plazo lo da la propia secuencia en el anfitrión.
- Nombres nuevos que propone esta story, para el glosario con el PR: `RingRules`, `FRingTrialRun`, `bHumanPlaysSkillTrials`, `RingAnnounceSeconds`, `PressRing`.
- La simulación sin mundo no cambia: sigue usando los decisores. `Soulbound.Core.Simulation.*` tiene que seguir en verde sin tocar sus cifras.
- Tests primero, en rojo. El anfitrión y la vista solo tienen criterio manual, como en S-022 a S-024.
- Al cerrar: quitar el minijuego de la lista de recortes de la skill (`SKILL.md`, alcance) y actualizar las dos líneas del README que hoy dicen que las pruebas se resuelven al azar. En el mismo PR.

## Fuera de esta story (queda en el GDD 09 para la Fase 2)

- Desenfunde (R-MIN-5, R-MIN-9) y el anuncio de variante por el Warden.
- Trampas del desafío final y frases del Warden entre runas (R-MIN-12, segunda parte).
- Zona buena más ancha de Shuten-dōji (R-MIN-7): sigue el margen de S-012.
- Duelo simultáneo de dos humanos, evaluación en red y plausibilidad (R-MIN-10, R-MIN-13, R-MIN-14).
- Habilidad de los bots por dificultad (R-MIN-15): los dummies no cambian.
- Vista de espectadores, sonido y el vestido de cada uso (R-MIN-16, R-MIN-17, tabla de presentación).
- Mando: solo ratón en esta story.
