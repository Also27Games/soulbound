# S-016. Warden I: actos, sacos, cerebro, interrupciones, insolencia y Furia

**Sistema**: el Warden (GDD 06), sobre castigos (GDD 05) y turnos (GDD 02). **Módulos**: `SoulboundCore` y `Soulbound` (anfitrión del Warden, vista, comandos). **Rama base**: `main`. **Tamaño**: muy grande, la mayor del proyecto. Si el plan sale desproporcionado, el agente propone partirla en dos PR y el autor decide.

## Contexto

Hasta hoy el Warden es el cerebro del spike: cada 15 s decide si sella una casilla, y los eventos se lanzan con un comando. Esta story pone el Warden de verdad: la partida tiene **actos**; los eventos viven en **tres sacos**, uno por acto y acumulativos, con peso, usos y ventana; un **cerebro** sin mundo evalúa a intervalos, decide intervenir o no, elige canal y evento, y respeta la **garantía** del pilar 1; un **golpe corta el turno** en curso; la **insolencia** trae castigo inmediato y, a cinco, la **Furia**. Quedan para S-017: Oráculo y Consulta forzada, disparadores, votaciones, comentarios con texto, eventos programados y voz. La vista 3D es S-019.

Decidido por el autor el 08/10 y el 19/10/2026: el Warden va antes que la vista 3D; comentarios, Oráculo, disparadores y votaciones son de S-017; el cerebro del spike (Sellos sueltos) desaparece y el Sello entra como evento del saco ("Primer muro"); (1) el objetivo de cortes por acto se mide e imprime, no se exige en test; (2) el canal comentario existe desde ya con sus pesos y queda registrado sin texto; (3) el castigo de insolencia por reloj de turno se aplica al terminar ese turno, entre turnos, por el mismo camino que los efectos de un evento.

## Reglas a implementar

- **R-WAR-2. Actos.** Tres actos por tiempo de datos (valores iniciales 0-5, 5-27 y 27-40 minutos). El fin de la carrera adelanta el Acto II. El acto es una función del estado (reloj de partida y fase), no un contador aparte.
- **R-WAR-1, R-WAR-3. Cerebro y evaluación.** `FWardenBrain` se reescribe. Recibe el estado, el reloj de partida y un sorteo propio; devuelve una decisión (nada, golpe con evento, escena con evento, comentario sin texto, o aplazada) **y el instante de la siguiente evaluación** (intervalo de datos, valor inicial 15 s, más o menos su variación, valor inicial 5 s, sorteada), para que el anfitrión y la simulación lo llamen con el mismo contrato y la partida sea reproducible. Probabilidad de intervenir por acto (20, 35 y 50 %) y pesos de canal por acto (golpe, escena, comentario: 40/20/40, 45/25/30, 50/25/25), todo en datos.
- **R-WAR-4, R-TUR-11, R-TAB-26. Canales y corte.** Un **golpe** llega en cualquier instante de un turno y lo **corta**: el turno termina ahí (evento `TurnInterrupted` con la fase en que estaba), la tirada pendiente se pierde (el consumo fijo del sorteo se respeta: si el turno llegó a `PreRoll`, se sacan los valores que falten), las elecciones abiertas se cierran, la ficha se queda donde está (el motor mueve de golpe: no existe "a medio camino"; la casilla donde está ya resolvió su efecto al llegar), y a continuación el evento se aplica **entre turnos** con el interrumpido como "activo" (`LastTurnSeat`), con todo lo de S-014 y S-015. Una **escena** entra solo entre turnos y no corta, salvo que en datos esté marcada como cortante. Un **comentario** no corta ni aplica nada: en esta story es solo una decisión registrada (`WardenRemark`, sin texto; el tipo y la frase son de S-017). Como mucho una intervención con efecto por hueco entre turnos.
- **R-TAB-28, R-EFE-1. Alto.** Su único efecto es el corte. Un golpe con Alto corta el turno y no aplica nada más. Fuera de un golpe (sin turno que cortar) no es válido: burla.
- **R-WAR-7. Aplazamiento.** Si la evaluación cae durante un duelo, un trilero, un minijuego o un evento en curso, la decisión se aplaza: el cerebro devuelve "aplazada" y se vuelve a evaluar en cuanto aquello termina.
- **R-WAR-4d. Margen.** Ninguna intervención ni comentario en los últimos segundos de datos (valor inicial 12) de un acto ni de la partida.
- **R-WAR-5. Garantía.** Si pasan los segundos de datos (valor inicial 120) sin golpe ni escena, la siguiente evaluación interviene con probabilidad 1 y con efecto, nunca con comentario.
- **R-WAR-8 (sin disparadores), R-SAC-1, R-SAC-2. Sacos y selección.** Cada evento de `eventos.json` lleva además: nombre, acto del saco, canal, entrega (`direct`, `oracle`, `any`), peso por acto, usos máximos, ventana de tiempo opcional dentro de la partida y si corta. **Tres sacos, uno por acto, acumulativos**: en el Acto II están disponibles los sacos I y II; en el III, los tres. Candidatos: del canal elegido, en su ventana, con usos restantes, con objetivos válidos (R-DIR-2 y 3); un dilema con menos de dos opciones válidas se descarta; un evento con un efecto no implementado (Consulta forzada) o con entrega solo por Oráculo nunca es candidato hasta S-017; `any` se entrega directo. Sorteo por peso del acto entre candidatos. Sin candidato en el canal elegido, se prueba el otro canal con efecto; sin ninguno, comentario, y la garantía sigue contando. En datos entran **los 35 eventos del GDD 06** (sus 38 menos las tres votaciones, que son de S-017), con sus nombres, composición, entrega, ventanas y usos (tabla al final).
- **R-WAR-10. Registro.** Cada evaluación y cada decisión dejan evento (`WardenEvaluated`; `WardenDecided` con acto, canal, evento y si fue aplazada o forzada por la garantía). Los usos restantes de cada evento, el instante de la última intervención con efecto y el de la siguiente evaluación viven en el estado (`FWardenState` en `FMatchState`), para que una partida se reproduzca con su semilla.
- **R-INS-1, R-INS-2. Insolencia con castigo.** Las fuentes ya existen (reloj de turno, R-TUR-8; las desatenciones de R-DIR-4). Cada subida trae un castigo inmediato al insolente, sorteado por gravedad según el nivel **anterior** a la subida con la tabla de datos (leve / media / grave: niveles 0 y 1, 70/25/5; 2 y 3, 30/50/20; 4, Furia sin sorteo), y dentro de la gravedad un efecto válido sobre él: leve, Revelar Brazal (Consulta forzada es de S-017; Alto no aplica a quien ya no tiene turno); medio, Vuelta al hogar, Turno perdido o Prueba del Warden; grave, Perder una runa o el Vacío (en el Acto I, lo grave es Vuelta al hogar, R-CAR-7). Si el efecto sorteado no es válido sobre él, se sortea otro de la misma gravedad; sin ninguno válido, burla. Nunca un regalo ni un efecto que necesite a otro jugador. Se aplica cuando el asiento deja de estar en juego: para el reloj de turno, al terminar ese turno (entre turnos, por el mismo camino que un efecto de evento); para las desatenciones de un evento, en el acto. A quien está en el Vacío el castigo se le aplaza a su salida (salvo la Furia). Registro `InsolencePunished` (nivel, gravedad, efecto).
- **R-INS-3. Furia.** Al llegar a cinco, sin sorteo: `Banish` con Furia (ya existe), también durante la carrera (única excepción a R-CAR-7); el contador vuelve a cero y la cuenta de turnos del Vacío a cero. Dentro del **Vacío de la Furia** (`FSeatState::bFuryVoid`): Desvío se ofrece pero no funciona (la runa no se consume; registro `FuryRefusal`); una Liberación saca a todos menos a él; Mictlantecuhtli conserva su escalera; escape y cuarto turno, como siempre. Furia sobre quien ya está en el Vacío: su Vacío pasa a ser de la Furia y la cuenta se reinicia. Registro `FuryUnleashed`.
- **R-INS-1, bots.** Los dummies nunca son insolentes porque siempre contestan dentro del plazo; la simulación lo comprueba.
- **Sorteos del Warden.** El cerebro y los castigos de insolencia sortean con una semilla propia del Warden que vive en el estado (como la del evento en curso: una semilla más un contador de sorteos), fijada al iniciar la partida a partir de la semilla de partida (semilla + 2, como el anfitrión y la simulación hoy). Nada toca el sorteo de los dados.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-WAR-1** (R-WAR-1). Mismo estado, mismo reloj y misma semilla → misma decisión y mismo instante siguiente, dos veces, sin mundo.
- **CA-WAR-1b** (R-WAR-4c, 4d). Una intervención no detiene el reloj de partida; una evaluación en el segundo 290 del Acto I no inicia nada.
- **CA-WAR-2** (R-WAR-3, 5; simulación). En 50 partidas con los valores iniciales, ningún tramo de 120 s sin golpe ni escena, y todos los intervalos entre evaluaciones dentro de 15 ± 5 s.
- **CA-WAR-3b** (R-WAR-4, R-TUR-11, R-TAB-26). Un golpe corta el turno en cada fase: `Start` (con la elección de Desvío abierta), `PreRoll` con la elección de dados abierta, `RollAndChoose` (con y sin oferta de repetir), `MoveAndResolve` con salto o acción abiertos. En todos: `TurnInterrupted` con la fase, la tirada perdida, el consumo fijo respetado, la ficha donde estaba, elecciones cerradas, y el evento aplicado con el interrumpido como activo; el turno pasa al siguiente. Una escena cortante corta; una no cortante espera al hueco entre turnos.
- **CA-TAB-28** (R-TAB-28). Un golpe con Alto corta y no aplica nada más; Alto entre turnos es burla.
- **CA-WAR-4** (R-WAR-7). Evaluación durante un duelo, un trilero, un minijuego o un evento en curso → aplazada; al terminar, interviene si procedía.
- **CA-WAR-5, CA-WAR-6, CA-SAC-2** (R-WAR-8, R-SAC-1). Un dilema con una sola opción válida no es candidato; un evento sin usos, tampoco; en el Acto III los candidatos salen de los tres sacos con los pesos del III; un evento con ventana solo es candidato dentro de ella; con usos máximos 1, no repite en la partida.
- **CA-SAC-1** (R-SAC-2). Los 35 eventos cargan y validan contra el catálogo; `warden.json` carga, valida y rechaza con motivo (actos no crecientes, probabilidades fuera de [0, 1], pesos negativos, tabla de insolencia que no suma 100, margen o garantía no positivos).
- **CA-INS-1** (R-INS-1, 2). Insolencia 0 y reloj de turno agotado → 1 y un castigo de la tabla del nivel 0 sobre él, aplicado al terminar el turno, nunca un regalo; un castigo no válido (Vuelta al hogar estando en casa) → otro de la misma gravedad; en el Acto I, lo grave es Vuelta al hogar; en el Vacío, aplazado a la salida.
- **CA-INS-2** (R-INS-3). A 4 y otra insolencia → Furia sin sorteo: Vacío de la Furia e insolencia 0, también en carrera; Desvío dentro no saca y no gasta la runa; una Liberación le deja dentro y saca a los demás; superar el escape le saca; Furia sobre quien ya está dentro reinicia su cuenta.
- **CA-INS-3** (R-INS-1; simulación). 50 partidas con dummies → insolencia 0 en todos los asientos al final.
- **Registro y determinismo.** Dos simulaciones con la misma semilla dan el mismo registro del Warden; el test de determinismo de S-010 sigue en verde con sus mismas tiradas.
- **Informe** (`-Informe`). Por acto: intervenciones con efecto, porcentaje de turnos cortados, hueco máximo sin golpe ni escena, Furias por partida. El objetivo de cortes (15/25/35 %) se imprime al lado, no se exige (decisión 1).
- **Simulación.** La crónica cambia (el Warden real sustituye al del spike) y se documenta; los 214 tests siguen en verde, salvo los que describían al cerebro del spike, que se adaptan y se listan en el PR.

Manuales, en PIE con captura:

- **Golpe**: en tu turno, a mitad de elegir destino, el Warden te corta: la crónica dice la fase, el nombre del evento y el efecto sobre ti; el turno pasa al siguiente.
- **Insolencia visible**: la leyenda de asientos muestra la insolencia de cada uno (decisión del autor del 05/10/2026); dejas agotar el reloj de turno, sube a 1 y la crónica dice el castigo. Con `SbInsolence 0 4` y otra desatención, la Furia: tu ficha al Vacío de la Furia, y `SbRelease` te deja dentro.
- **Warden en pantalla**: línea con acto, segundos hasta la siguiente evaluación y segundos desde la última intervención; `SbWardenMode off|auto` apaga y enciende el cerebro para probar otras cosas.

## Notas para quien lo implemente

- Nombres del glosario: Golpe `Strike`, Escena `Scene`, Comentario `Remark`, Garantía `InterventionGuarantee`, Vacío de la Furia `FuryVoid`, Ventana de evento `EventWindow`, Corte `CutsTurn`, Usos máximos `MaxUses`, Anfitrión del Warden `WardenHost`, cerebro `WardenBrain`, Acto `Act`, Saco de eventos `EventBag`, Insolencia `Insolence`, Furia `Fury`. Nombres nuevos (el estado del Warden en `FMatchState`, los eventos del registro, los campos de datos, la función de corte, el modo del anfitrión) se proponen en el plan y en el PR.
- **El corte en la máquina**: una regla de `TurnRules` (`InterruptTurn(State, Now)`), que cierra las elecciones, respeta el consumo fijo, deja `TurnInterrupted` y pasa a `End`; el golpe es corte más `ApplyEvent` en el `BetweenTurns` resultante, en la misma llamada. Nunca se corta en `Duel`, `ShellGame` ni `SkillTrial`: ahí el cerebro aplaza (R-WAR-7).
- **El anfitrión** (`UWardenHostSubsystem`) deja el temporizador fijo: pregunta al cerebro con el reloj de partida y respeta el instante que este devuelve; con la partida detenida en paso a paso no evalúa (ya existe). Aplica golpes y escenas a través de `MatchHost`. `SbEvent` sigue existiendo para pruebas.
- **La simulación** llama al cerebro en su bucle con el mismo contrato y aplica sus decisiones; el informe de carrera gana las métricas del Warden. El cerebro del spike (`SealTile`, `ReleaseTile`) desaparece con sus tests.
- **Datos**: `Content/Data/Warden/warden.json` nuevo (actos, intervalo y variación, probabilidad de intervenir y pesos de canal por acto, garantía, margen, objetivos de corte, tabla de insolencia), con cargador y validación como los demás, cargado por el anfitrión y por la simulación; `eventos.json` con los 35 eventos del GDD 06 y los campos nuevos. Los ocho eventos de prueba de S-014 y los tres de S-015 se quedan en el archivo **fuera de los sacos** (sin acto), solo para `SbEvent` y los tests.
- **Insolencia**: el castigo se aplica por `ApplyEffect` con una aplicación sintética (efecto, dirección activo) y el mismo registro `EffectApplied`, más `InsolencePunished`. El aplazamiento en el Vacío es un contador en el asiento que se descarga al salir.
- **Furia**: `Banish(bFury)` ya existe y ya deja `Banished` con Value 1; lo que falta es `bFuryVoid` y sus tres excepciones (Desvío, Liberación, cuenta reiniciada).
- **Vista**: solo lectura; insolencia y Furia en la leyenda de asientos; línea del Warden; en la crónica, el nombre del evento, el canal y la fase cortada.
- **Ediciones de depuración**: `SbInsolence <asiento> <n>` (patrón de S-011, con evento `DebugEdit`). Comando `SbWardenMode` en el controlador, cuerpo vacío en Shipping.
- Nada de `Engine` en `SoulboundCore`. Ningún asset de Unreal.

## Fuera de esta story

- Oráculo y Consulta forzada, disparadores contextuales (R-DIS), votaciones, comentarios con tipo y frase, eventos programados (inicio, fin de carrera con frases, aspirante), voz, máscara por acto, foco y cinemáticas: S-017 e interfaz.
- Bajada del contador de insolencia (solo si el playtest lo pide). Un "dummy insolente": los tests dejan correr los relojes.
- Calibración de probabilidades y pesos: playtest y simulación, con el informe.

## Los 35 eventos de los sacos (GDD 06, R-SAC-2)

Composición con los códigos del catálogo de S-014. Entrega: `direct`, `oracle` o `any` (`any` se aplica como directa en esta story; `oracle` no es candidato hasta S-017). Peso por acto, usos y ventana: valores iniciales en datos; donde el GDD no dice nada, peso 1 en su acto y en los siguientes, usos sin límite.

| Acto | Canal | Nombre | Composición | Entrega | Notas |
|---|---|---|---|---|---|
| I | Golpe | Tropiezo | `Halt`, activo | direct | |
| I | Golpe | Pausa | `LoseTurn`, activo, 1 | any | Ventana: hasta el segundo 150 |
| I | Golpe | Al Altar | `SendHome`, activo | any | Ventana: hasta el segundo 150 |
| I | Golpe | La Prueba | `WardenTrial`, activo | direct | |
| I | Golpe | Prisa | `ExtraTurn`, activo, 1 | any | Es un regalo, va como golpe |
| I | Escena | Primer muro | `PlaceSeal` | direct | |
| II | Golpe | Elige tu veneno | Dilema: `LoseTurn`, activo, 1 / `LoseRune`, activo, a elección | direct | |
| II | Golpe | Generosidad envenenada | Dilema: `ExtraTurn`, activo, 2 / `SendHome`, rival señalado | direct | |
| II | Golpe | Enséñame | `RevealBracer`, rival señalado | any | |
| II | Golpe | Ojos en ti | `RevealBracer`, activo | any | |
| II | Golpe | El orbe llama | `ForcedOracle`, activo | direct | No candidato hasta S-017 |
| II | Golpe | Voluntad ajena | `ForcedOracle`, rival señalado | direct | No candidato hasta S-017 |
| II | Golpe | Arquitecto | `SealAgainstRival`, rival señalado | direct | |
| II | Golpe | Hazlo tú | Dilema: `SealAgainstRival`, rival señalado / `RevealBracer`, rival señalado | direct | |
| II | Golpe | Duelo por orden | `ForcedDuel`, activo contra rival señalado, `OneRune` | direct | |
| II | Golpe | El precio | Dilema: `DecreedTheft`, rival señalado / `GiftRune`, activo | direct | |
| II | Golpe | Tres caminos | Dilema: `LoseTurn`, activo, 1 / `LoseRune`, activo, azar / `WardenTrial`, activo | direct | |
| II | Golpe | Ojo por ojo | Dilema: `LoseRune`, activo, azar / `WardenTrial`, rival señalado | direct | |
| II | Golpe | Impuesto | `LoseRune`, activo, azar | any | |
| II | Golpe | Cadena | `SendHome`, siguiente | any | |
| II | Golpe | Destierro | `Banish`, activo | any | Peso bajo (0,3); usos 2 |
| II | Escena | Ley de Sombras | `ShadowLaw` | direct | |
| II | Escena | Noche de los Ecos | `EchoNight` | direct | |
| II | Escena | Piedad | `ReleaseVoid` | direct | |
| II | Escena | Amanecer | `ReleaseVoid` y después `ShadowLaw` | direct | |
| II | Escena | Marcha atrás | `SendHome`, todos | direct | Usos 1 |
| II | Escena | Hermanos de sangre | `ForcedDuel`, dos al azar, `OneRune` | direct | |
| III | Escena | El perseguido | `Mark`, líder | direct | |
| III | Escena | Marca ciega | `Mark`, al azar | direct | |
| III | Golpe | Todo o nada | `ForcedDuel`, dos rivales señalados, `AllRunes` | direct | Ventana: desde el segundo 1920 |
| III | Escena | Cosecha | `LoseRune`, todos, azar | direct | Usos 1 |
| III | Golpe | Cosecha del líder | `LoseRune`, líder, azar | any | |
| III | Golpe | Milagro | `DecreedTheft`, a todos | any | Usos 1 |
| III | Golpe | Destierro del líder | `Banish`, líder | any | |
| III | Golpe | Trueque | Dilema de 4: `ExtraTurn`, activo, 2 / `DecreedTheft`, rival señalado / `GiftRune`, activo / `SealAgainstRival`, rival señalado | direct | |

Las tres votaciones (Juicio, Favor, El chivo) son de S-017.
