# Castigos, Nicho y Vacío

> **Estado en la entrega del TFM (noviembre de 2026)**: Están implementados los dieciocho efectos, las direcciones, los decretos y los dilemas, el hogar y el Vacío con su escalera (S-013 a S-017). Se hicieron de otra manera: desatender un Duelo forzado o un Robo decretado lleva a un Duelo contra el Warden (S-015); el Vacío de la Furia tiene su propia escalera (S-016); hay una novena dirección, la del más votado (S-017); la Noche de los Ecos termina por su tope, con frase y penumbra (S-043); el reloj para señalar es de 6 s. No están implementados: R-COM-5 (la opción imposible de un dilema no se ofrece, story S-034, fuera de esta entrega), apostar desde el Vacío, la presentación del Vacío y los bots que eligen con criterio. Casi todas las cuestiones abiertas del final están ya decididas; su resolución está en la bitácora.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (18/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-18
> **Fase**: 1
> **Pilares que sirve**: 1, 2, 3, 4
> **Fuente**: GDD v0.1 secciones 4.4, 5.3, 5.4, 5.6; mecánicas del género (solo mecánicas, ningún texto); decisiones del autor del 17 y 18/09/2026

## Resumen

Define la lista cerrada de **efectos** que el Warden puede imponer o regalar, **a quién** puede ir cada uno, y cómo se combinan en eventos. Define también el Vacío (entrar, estar, salir) y el papel del Nicho como hogar y castigo medio. Qué evento usa el Warden en cada momento, con qué frecuencia y con qué dureza (actos, sacos, disparadores, insolencia, Furia, Oráculo) lo decide el GDD del Warden; este documento le da las piezas.

## Fantasía del jugador

El Warden no tiene un solo látigo: tiene dieciocho, y elige cuál usar y sobre quién. A veces te lo pone en la mano a ti: "dos turnos para ti, o mandas a casa a quien señales". El peor de todos es el Vacío: desapareces de la mesa y solo vuelves si demuestras que sabes jugar.

## Reglas

### Efectos

- **R-EFE-0.** Un efecto es una consecuencia atómica con nombre, gravedad, direcciones permitidas y parámetros. La lista de efectos es cerrada: el Warden solo puede hacer lo que está aquí. Todo efecto se aplica igual a Elegidos y a Ecos, salvo donde una regla anterior diga otra cosa (el Eco sale del Vacío al instante, R-ECO-5).

| # | Efecto | Código | Gravedad | Direcciones permitidas |
|---|---|---|---|---|
| 1 | Alto | `Halt` | Leve | Activo |
| 2 | Revelar Brazal | `RevealBracer` | Leve | Activo, rival señalado, líder, todos |
| 3 | Consulta forzada al Oráculo | `ForcedOracle` | Leve | Activo, rival señalado, siguiente |
| 4 | Vuelta al hogar | `SendHome` | Medio | Activo, rival señalado, líder, siguiente, todos, al azar |
| 5 | Turno perdido | `LoseTurn` | Medio | Activo, rival señalado, líder, siguiente, al azar |
| 6 | Sello | `PlaceSeal` | Medio | Tablero (el Warden elige) |
| 7 | Sello contra rival | `SealAgainstRival` | Medio | Rival señalado, líder |
| 8 | Marcado | `Mark` | Medio | Rival señalado, líder, al azar |
| 9 | Ley de Sombras | `ShadowLaw` | Medio | Ecos |
| 10 | Prueba del Warden | `WardenTrial` | Medio | Activo, rival señalado, siguiente |
| 11 | Perder una runa | `LoseRune` | Grave | Activo, rival señalado, líder, todos, al azar |
| 12 | Robo decretado | `DecreedTheft` | Grave | Activo roba a rival señalado, a todos o al azar |
| 13 | Duelo forzado | `ForcedDuel` | Grave | Activo contra rival señalado; dos rivales señalados; dos al azar |
| 14 | El Vacío | `Banish` | Grave | Activo, rival señalado, líder, al azar |
| 15 | Turno extra | `ExtraTurn` | Regalo | Activo, siguiente |
| 16 | Runa regalada | `GiftRune` | Regalo | Activo, siguiente, al azar |
| 17 | Liberación | `ReleaseVoid` | Regalo | Todos los del Vacío |
| 18 | Noche de los Ecos | `EchoNight` | Regalo | Ecos |

- **R-EFE-1. Alto.** Corta el turno en curso con los efectos de R-TAB-26 y R-TAB-28. Sin más consecuencia.
- **R-EFE-2. Revelar Brazal.** Los tipos de runa del objetivo quedan visibles para todos durante un número de rondas definido en datos, valor inicial una ronda completa. Si ya estaba revelado, se prolonga.
- **R-EFE-3. Consulta forzada al Oráculo.** El objetivo consulta el Oráculo como si hubiera acabado en su Nodo; lo que el Oráculo reparte es otro efecto de esta lista (GDD del Warden). Si el objetivo es el jugador activo, corta su turno (R-TUR-11).
- **R-EFE-4. Vuelta al hogar.** La ficha del objetivo va a su hogar (R-TAB-13, R-TAB-24). Un jugador que ya está en su hogar no es objetivo válido. Es el castigo medio: el hogar no es una cárcel, el siguiente turno se juega con normalidad desde allí.
- **R-EFE-5. Turno perdido.** El objetivo acumula uno o dos turnos perdidos (cantidad en datos). Un jugador no acumula más del tope definido en datos, valor inicial dos; lo que exceda se convierte en Vuelta al hogar. Se descuentan según R-TUR-10 y se cancelan uno a uno con los turnos extra (R-TUR-9).
- **R-EFE-6. Sello.** El Warden sella una casilla válida según R-TAB-22. Cuál, lo decide su GDD (por ejemplo, en el camino del aspirante).
- **R-EFE-7. Sello contra rival.** El jugador activo señala a un rival (o la dirección es líder) y el Warden sella la casilla válida según R-TAB-22 que más le estorba: la primera casilla del camino más corto desde su posición hasta su objetivo más probable (la Fuente que le falta más cercana, o el Altar si tiene seis tipos), excluyendo las no sellables. Si no hay casilla válida en ese camino, cualquier válida a distancia mínima de él. El jugador nunca elige casillas (decisión del 21/09: iluminar todo el tablero es ilegible y con mando impracticable). El Sello sigue siendo del Warden (R-TAB-23). Sustituye al Sello dirigido.
- **R-EFE-8. Marcado.** El objetivo queda Marcado y toda la mesa lo ve. Quien le perjudique gana un turno extra: robarle una runa, expulsarle, ganarle un duelo o mandarle al hogar o al Vacío por un efecto que el perjudicador haya dirigido. La marca termina al cobrarse una vez o al pasar un número de rondas definido en datos, valor inicial tres. Puede haber varios Marcados a la vez (por ejemplo, varios líderes empatados); cada marca es independiente y marcar de nuevo a un Marcado reinicia su duración. Nadie se marca a sí mismo.
- **R-EFE-9. Ley de Sombras.** Todos los Ecos van a su hogar (R-TAB-25). Termina la Noche de los Ecos si estaba activa.
- **R-EFE-10. Prueba del Warden.** Al inicio del siguiente turno del objetivo, antes de tirar, juega el minijuego de habilidad en dificultad normal. Si lo supera, el turno sigue con normalidad; si falla, el turno termina sin tirada. Es como mucho un turno y no acumula. Una Prueba pendiente se anula si el jugador es enviado al Vacío. Si el jugador tiene turnos perdidos, estos se descuentan primero y la Prueba queda pendiente para su siguiente turno jugado.
- **R-EFE-11. Perder una runa.** El objetivo pierde una runa, que vuelve a su Fuente (R-RUN-6). Variante en datos: al azar o a elección del objetivo (reloj corto; si no elige, al azar). Sin runas, no hay efecto y el Warden se burla. La afinidad de Koschei no se aplica: no es un robo.
- **R-EFE-12. Robo decretado.** El ladrón roba a la víctima con el trilero (R-ROB). Con dirección "a todos", roba una runa a cada rival, uno tras otro, saltando a los que no tienen runas. La afinidad de Koschei sí se aplica.
- **R-EFE-13. Duelo forzado.** Dos jugadores se baten según R-DUE-8, con las reglas de R-DUE-1 y 3 a 7. En juego, según datos: una runa (trilero) o todas las runas del perdedor (pasan al ganador sin trilero). Con "activo contra rival señalado", el activo es el retador; con "dos rivales señalados", el activo elige a los dos y hace de retador el primero que señala; con "dos al azar", el retador se sortea. Si el retado no tiene runas, R-DUE-4.
- **R-EFE-14. El Vacío.** El objetivo es desterrado (R-VAC). Nadie va al Vacío durante la carrera (R-CAR-7).
- **R-EFE-15. Turno extra.** El objetivo recibe uno o dos turnos extra (cantidad en datos, tope definido en datos, valor inicial dos), que se juegan según R-TUR-9.
- **R-EFE-16. Runa regalada.** El objetivo elige, dentro de un reloj corto, un tipo de runa entre las Fuentes con reserva y la recibe; la reserva baja en uno. Si no elige a tiempo, no recibe nada y el Warden le machaca por lento y desagradecido (R-DIR-4). Sin Fuentes con reserva, no hay efecto. Un Eco puede recibir así runas de cualquier Dominio.
- **R-EFE-17. Liberación.** Todos los jugadores que están en el Vacío salen a la vez y reaparecen en su hogar. Si no hay nadie en el Vacío, el efecto no es válido.
- **R-EFE-18. Noche de los Ecos.** Desde que se decreta, los Ecos pueden transitar por cualquier Dominio (permiso temporal de R-TAB-21). Pueden robar allí a quien encuentren (R-ECO-4), pero siguen sin obtener runas de Fuentes de Dominios con dueño (R-ECO-3). No tiene duración fija: termina cuando el Warden lo decide, y su forma de terminarla es decretar la Ley de Sombras (R-EFE-9), que manda a los Ecos a casa. Cuándo la termina lo decide su GDD. Como salvaguarda contra errores, existe un tope de rondas en datos, alto (valor inicial ocho), que la termina sin Ley de Sombras y que no debe alcanzarse en juego normal.

### Dirección: a quién le pasa

- **R-DIR-1.** Toda aplicación de un efecto lleva una dirección, elegida entre las permitidas para ese efecto:

| Dirección | Código | Quién |
|---|---|---|
| Activo | `ActivePlayer` | El jugador cuyo turno es, o el último que ha tenido turno si el evento llega entre turnos |
| Rival señalado | `ChosenRival` | Uno que el activo elige entre los válidos, con reloj corto |
| Dos rivales señalados | `TwoChosenRivals` | Dos que el activo elige |
| Líder | `Leader` | Quien tiene más tipos de runa distintos; con empate, quien más runas en total; si el empate persiste, todos los empatados son líderes y el efecto se aplica a todos ellos |
| Siguiente | `NextPlayer` | El jugador que sigue al activo en el orden de turno |
| Todos | `Everyone` | Todos los jugadores válidos |
| Al azar | `RandomPlayer` | Uno al azar entre los válidos |
| Ecos | `Echoes` | Todos los Ecos Perdidos |

- **R-DIR-2.** Un jugador en el Vacío no es objetivo válido de ningún efecto salvo Liberación. Un jugador en su hogar no es válido para Vuelta al hogar. Un jugador sin runas no es válido para Perder una runa, Revelar Brazal ni como víctima de robo. El motor filtra los objetivos válidos antes de ofrecer la elección.
- **R-DIR-3.** Si tras filtrar no queda ningún objetivo válido, el efecto no se aplica y el Warden lo convierte en una burla. El GDD del Warden evita elegir eventos sin objetivos válidos.
- **R-DIR-4.** Desatender al Warden nunca sale gratis y nunca sale a cuenta. Cuando el activo debe elegir algo dentro de un reloj y no lo hace: si lo que tenía que elegir era un regalo (Runa regalada), lo pierde y el Warden se burla de él por lento y desagradecido; si tenía que señalar a la víctima de un efecto dañino, ese efecto se aplica a él mismo (con "dos rivales señalados", él se bate contra un rival al azar); si tenía que elegir entre variantes de un castigo propio (qué runa perder), se decide al azar. En todos los casos cuenta como insolencia (GDD del Warden).

### Composición: cómo se montan los eventos

- **R-COM-1.** Un evento del Warden se compone de un texto, una forma de entrega (Warden directo u Oráculo) y de una a cuatro opciones, número en datos. Cada opción es una lista ordenada de aplicaciones (efecto, dirección, cantidad).
- **R-COM-2.** Con una opción es un **decreto**: se aplica sin elegir. Con dos o más es un **dilema**: el jugador activo elige una dentro de un reloj que crece con el número de opciones (datos: base más un incremento por opción). Si no elige, el Warden resuelve el dilema en su contra, según R-DIR-4: toma una opción al azar, aplica al propio jugador todos sus efectos dañinos, aunque fueran para otros, y descarta los regalos; si la opción no tiene nada dañino, no se aplica nada y solo queda la burla. En todos los casos cuenta como insolencia. La **votación** (todos eligen) se define en el GDD del Warden con las mismas piezas.
- **R-COM-3.** Las aplicaciones de una opción se resuelven en orden, y cada una se valida por separado (R-DIR-2 y 3). Ejemplos que deben poder expresarse sin código nuevo:
  - "Pierdes un turno": `LoseTurn(Activo, 1)`.
  - "Dos turnos extra para ti, o mandas a casa a quien señales": opción A `ExtraTurn(Activo, 2)`; opción B `SendHome(RivalSeñalado)`.
  - "Elige a dos y que se maten por todo": `ForcedDuel(DosRivalesSeñalados, todas)`.
  - "Ha llegado la noche": `EchoNight(Ecos)`.
  - "Los del Vacío salen, pero todos volvéis a casa": `ReleaseVoid` y después `SendHome(Todos)`.
  - "Que el líder enseñe lo que lleva y pierda una": `RevealBracer(Líder)` y después `LoseRune(Líder, azar)`.
- **R-COM-4.** Los bots eligen en los dilemas la opción que más les beneficia según su interfaz de decisión (GDD de bots).
- **R-COM-5.** (Decisión del autor del 12/11/2026, S-034.) Un dilema solo ofrece las opciones que tienen ahora algún objetivo válido para quien lo dirige (R-DIR-2 y 3). Con dos o más ofrecibles, se elige entre ellas; con una sola, se aplica como un decreto, sin insolencia; con ninguna, queda la burla. Provisional: tras la entrega, la opción imposible se ofrecerá, elegirla contará como insolencia y se aplicará otra opción.

### El Nicho como hogar

- **R-NIC-1.** El hogar de cada jugador es el de R-TAB-13. Vuelta al hogar es el castigo medio del juego: cuesta posición, nunca turnos.
- **R-NIC-2.** Un jugador en su hogar juega su siguiente turno con normalidad. El Nicho no encierra a nadie.

### El Vacío

- **R-VAC-1.** Entrar: la ficha sale del tablero (R-TAB-24). El jugador no puede ser retado, expulsado ni robado (R-INT-3), ni es objetivo de ningún efecto salvo Liberación (R-DIR-2). Una Prueba del Warden pendiente se anula. Para el desterrado, la Cripta se desvanece a su alrededor y queda flotando en el Vacío; la mesa no desaparece, queda como una isla de luz lejana y atenuada en la que sigue viendo el tablero, las fichas y los focos de evento, con los demás jugadores como siluetas. Oye a los demás y puede hablar (en Fase 2, con la voz distorsionada). Para la mesa, su avatar en el Trono se vuelve espectral. Sigue pudiendo apostar en los duelos (R-DUE-7) y votar (R-VOT-1): es lo único que puede hacer, y es lo que le mantiene en la mesa mientras dura el destierro.
- **R-VAC-2.** Salir, en cada turno propio, antes de nada, y en este orden de prioridad: si es Eco, sale sin jugar (R-ECO-5); si tiene Desvío y decide usarlo, sale y juega el turno (R-POD-6); si no, juega el minijuego de escape. La escalera de dificultad va bajando: en su primer turno en el Vacío juega en difícil; en el segundo, en normal; en el tercero, en fácil; en el cuarto sale sin jugar. Mictlantecuhtli empieza en fácil y sale sin jugar en su segundo turno (R-ELE-2). No hay dados. Las dificultades son las del GDD de minijuego.
- **R-VAC-3.** Al salir, por el motivo que sea salvo Desvío, el jugador reaparece en su hogar y su turno termina.
- **R-VAC-4.** Un jugador enviado de nuevo al Vacío empieza la cuenta de turnos desde cero.
- **R-VAC-5.** Liberación saca a todos a la vez, en el momento del evento, sin esperar a sus turnos.
- **R-VAC-6.** El Vacío existe desde el Acto II (R-CAR-7). Cuántas veces y a quién manda el Warden al Vacío lo limita su GDD; este documento solo fija que nadie está dentro más de cuatro turnos propios (Mictlantecuhtli dos, un Eco uno).

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Jugador | Brazal revelado | R-EFE-2 | Pasan las rondas | Tipos visibles para todos |
| Jugador | Turnos perdidos pendientes (0-2) | R-EFE-5 | Se descuentan o se cancelan con extras | R-TUR-10 |
| Jugador | Marcado | R-EFE-8 | Alguien lo cobra o pasan las rondas | Perjudicarle da turno extra |
| Jugador | Prueba pendiente | R-EFE-10 | Se juega, o Vacío | Minijuego antes de tirar |
| Jugador | En el Vacío, turno 1 a 4 | R-EFE-14 | Escape, Desvío, Liberación, cuarto turno, o Eco | R-VAC |
| Partida | Noche de los Ecos | R-EFE-18 | Ley de Sombras (decisión del Warden), o tope de salvaguarda | Ecos transitan por todo |
| Evento | Presentado, Eligiendo, Resuelto | R-COM-2 | | Reloj de dilema |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Tablero | Castigos depende de este | Hogares, Sellos válidos, permiso temporal de tránsito, Alto |
| Turnos y dados | Mutua | Turnos extra y perdidos; interrupción; inicio de turno cedido al escape y a la Prueba |
| Carrera y Elegidos | Castigos depende de este | Eco sale del Vacío al instante; Mictlantecuhtli; Koschei; Ecos y Noche |
| Runas y poderes | Mutua | Trilero, duelo forzado, Desvío, reservas de Fuentes, líder por runas |
| Warden | Warden depende de este | Este define las piezas; el Warden decide eventos, frecuencia, dureza, insolencia, Furia y Oráculo |
| Minijuego | Castigos depende de este | Escape del Vacío (difícil, normal, fácil) y Prueba del Warden (normal) |
| Interfaz | Interfaz depende de este | Elección de objetivo y de opción con el componente de selección; marcas visibles; presentación del Vacío |
| Bots | Bots depende de este | Elegir objetivo, casilla, opción y tipo de runa regalada |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| Turnos propios en el Vacío | entero | 1-4 (Mictlantecuhtli 1-2, Eco 1) | regla | Minutos fuera: turnos por duración de ronda, entre 1 y 2 min con cuatro jugadores. Riesgo para el pilar 2 si se llega al cuarto turno a menudo: medir |
| Rondas con Marcado por partida | entero | a medir | simulación | Si nadie cobra la marca, subir duración o bajar rondas |
| Frecuencia de cada efecto | distribución | la fija el Warden | GDD del Warden | |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| Vuelta al hogar "a todos" con alguien ya en casa | Los que están en casa no se mueven | R-DIR-2 |
| Turno perdido a quien ya tiene dos pendientes | Se convierte en Vuelta al hogar | R-EFE-5 |
| Turno extra a quien tiene turnos perdidos | Se cancelan uno a uno | R-TUR-9 y 10 |
| Marcar a quien ya está Marcado | Se reinicia la duración | R-EFE-8 |
| Marcar a otro habiendo un Marcado | Los dos quedan Marcados, cada uno con su duración | R-EFE-8 |
| El Marcado va al Vacío | Sigue Marcado; nadie puede perjudicarle dentro; la duración sigue corriendo | R-EFE-8, R-VAC-1 |
| Perjudicar al Marcado con un efecto dirigido por mí | Cobro la marca | R-EFE-8 |
| Prueba pendiente y enviado al Vacío | La Prueba se anula | R-EFE-10 |
| Prueba pendiente y turno perdido | Primero se descuenta el turno perdido; la Prueba sigue pendiente | R-EFE-10 |
| Perder una runa sin runas | No hay efecto; burla | R-EFE-11 |
| Robo decretado "a todos" con rivales sin runas | Se saltan | R-EFE-12 |
| Duelo forzado con todo en juego y perdedor sin runas | R-DUE-6: a su hogar | R-EFE-13 |
| Liberación sin nadie en el Vacío | No válido; el Warden no lo elige | R-EFE-17, R-DIR-3 |
| Noche de los Ecos sin Ecos en la partida | No válido | R-DIR-3 |
| Ley de Sombras durante la Noche de los Ecos | La Noche termina y los Ecos van a casa | R-EFE-9 |
| Eco en un Dominio con dueño al acabar la Noche por el tope de salvaguarda | Va a su hogar, igual que con la Ley de Sombras | R-EFE-18 |
| Consulta forzada al activo en mitad de su movimiento | Corta el turno donde esté la ficha | R-TAB-26 |
| Líder con empate total | Todos los empatados reciben el efecto | R-DIR-1 |
| El activo debe señalar y no hay rivales válidos | El efecto no se aplica; burla | R-DIR-3 |
| Dilema "dos turnos extra o mandas a casa a quien señales" ignorado | El propio jugador va a su hogar; sin turnos extra; insolencia | R-COM-2, R-DIR-4 |
| Dilema con solo regalos ignorado | Nada; burla; insolencia | R-COM-2 |
| Enviado al Vacío en su tercer turno de una estancia anterior | Cuenta desde cero | R-VAC-4 |
| Desvío y minijuego de escape en el mismo turno | Si usa Desvío no juega el escape; si no, lo juega | R-VAC-2 |
| Interrupción del Warden durante el escape o la Prueba | No la hay: el Warden no interrumpe minijuegos (GDD del Warden) | Pilar 3 |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Tope de turnos perdidos acumulados | 2 | 1-2 | Jugadores fuera más tiempo | Castigo casi simbólico |
| Tope de turnos extra por evento | 2 | 1-2 | Los demás esperan más | Regalo pequeño |
| Rondas de Brazal revelado | 1 | 1-2 | | |
| Rondas de Marcado | 3 | 2-4 | Más caza | Marca sin cobrar |
| Tope de salvaguarda de la Noche de los Ecos | 8 rondas | 6-10 | Solo importa si el Warden falla | Podría cortar una Noche legítima |
| Turnos máximos en el Vacío | 4 (Mictlantecuhtli 2) | 3-4 | Más "muermo" | Vacío sin miedo |
| Escalera de dificultad del escape | difícil, normal, fácil | datos | | |
| Reloj para señalar o elegir casilla | 5 s | 4-8 | | Elección al azar frecuente |
| Reloj de dilema | 8 s más 3 s por opción | | | |

## Presentación en pantalla plana

| Evento o información | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Todo efecto | Foco de evento o aviso | Nombre del efecto, causa y víctima siempre en pantalla (glosario: Castigo) | Distintivo por gravedad | Frase con el nombre del jugador |
| Señalar rival u opción | En el mundo (GDD 07) | Avatares válidos o cartas iluminados; el tiempo se ve en la luz que se apaga | Tic-tac | Mete prisa |
| Dilema | Foco sobre el activo | Opciones como cartas de piedra flotando sobre la mesa; su borde se quema con el tiempo; la mesa las ve | | Lo presenta |
| Brazal revelado | Ficha y panel | Tipos visibles sobre la ficha, con borde encendido | | |
| Marcado | Ficha | Marca flotante visible para todos; contador de rondas | Latido | Anuncia la caza |
| Turno perdido | HUD del jugador | Contador de turnos perdidos junto al retrato | | |
| Prueba pendiente | HUD del jugador | Icono de prueba junto al retrato | | Recuerda la prueba al llegar el turno |
| Vacío, para el desterrado | Su vista, transformada | Las paredes y el suelo de la Cripta se desvanecen; cielo negro; la cámara se aleja despacio y la mesa queda como una isla de luz lejana y atenuada, con fichas y focos de evento visibles y los jugadores como siluetas; el minijuego de escape al llegar su turno; puede apostar. Al salir, la Cripta se recompone a su alrededor. Técnica: ocultar la malla de la Cripta para ese cliente, cielo negro, desplazar la cámara y bajar la exposición de la mesa; sin sala nueva ni cambio de escena | Zumbido sordo de fondo | Solo le habla a él |
| Vacío, para la mesa | Trono del jugador | Su avatar se vuelve espectral y translúcido; su ficha desaparece del tablero | | Se burla |
| Salida del Vacío | Tablero | La ficha reaparece en su hogar | Estallido | Comentario |
| Noche de los Ecos | Tablero | Dominios con dueño abiertos para los Ecos, con luz distinta, mientras dure | Viento | La decreta y la termina con la Ley de Sombras |
| Ley de Sombras | Tablero | Los Ecos arrastrados a sus Nichos | Cadenas | Decreto |

## Contribución a los pilares y a las métricas

- **Pilar 1**: dieciocho efectos y ocho direcciones dan al Warden un repertorio grande con un motor pequeño.
- **Pilar 2**: dilemas que afectan a otros, Marcado que pone a la mesa a cazar, Noche de los Ecos, apuestas incluso desde el Vacío. Nadie está fuera más de cuatro turnos.
- **Pilar 3**: lista cerrada, cada efecto con nombre, causa y víctima en pantalla; topes en turnos.
- **Pilar 4**: casi todo se dirige a alguien por su nombre.
- Métricas: M1 (minutos fuera por el Vacío y turnos perdidos), M3 (burlas de objetivos inválidos), rondas con Marcado.

## Criterios de aceptación

- [ ] **CA-EFE-1** (R-EFE-0). Dado el catálogo de efectos, cuando se valida, entonces contiene exactamente los dieciocho de la tabla, cada uno con gravedad y direcciones permitidas, y un evento con un efecto o una dirección fuera de la lista se rechaza al cargar los datos.
- [ ] **CA-EFE-2** (R-EFE-2). Dado un jugador con Brazal oculto, cuando se aplica Revelar Brazal, entonces sus tipos son visibles para todos, y dejan de serlo al pasar las rondas de datos.
- [ ] **CA-EFE-3** (R-EFE-4). Dado un jugador fuera de su hogar, cuando se aplica Vuelta al hogar, entonces está en su hogar; dado un jugador en su hogar, entonces no es objetivo válido.
- [ ] **CA-EFE-4** (R-EFE-5). Dado un jugador con dos turnos perdidos pendientes, cuando se le impone otro, entonces sigue con dos y está en su hogar.
- [ ] **CA-EFE-5** (R-EFE-7). Dado un Sello contra rival señalado, cuando se aplica, entonces queda sellada una casilla válida del camino más corto entre ese rival y su objetivo más probable; cuando no elige a tiempo a quién, entonces se aplica a él mismo (R-DIR-4).
- [ ] **CA-EFE-6** (R-EFE-8). Dado un jugador Marcado, cuando otro le roba una runa, le expulsa, le gana un duelo o le manda al hogar o al Vacío con un efecto dirigido por él, entonces ese otro tiene un turno extra y la marca desaparece; cuando pasan las rondas de datos sin cobrarse, entonces desaparece; cuando se marca a un segundo jugador, entonces ambos están Marcados con duraciones independientes.
- [ ] **CA-EFE-7** (R-EFE-10). Dado un jugador con Prueba pendiente, cuando empieza su turno y falla el minijuego, entonces el turno termina sin tirada; cuando lo supera, entonces tira con normalidad; cuando es enviado al Vacío antes, entonces la Prueba ya no está pendiente.
- [ ] **CA-EFE-8** (R-EFE-11). Dado un jugador con runas, cuando pierde una por este efecto, entonces la reserva de esa Fuente sube en uno; dado Koschei, entonces no aparta ninguna runa.
- [ ] **CA-EFE-9** (R-EFE-12). Dado un Robo decretado a todos con tres rivales, uno sin runas, entonces se juegan dos trileros y el ladrón tiene dos runas más.
- [ ] **CA-EFE-10** (R-EFE-13). Dado un Duelo forzado con todo en juego, cuando termina, entonces todas las runas del perdedor están en el Brazal del ganador sin trilero.
- [ ] **CA-EFE-11** (R-EFE-15). Dado un turno extra a un jugador con un turno perdido, entonces le quedan cero de cada.
- [ ] **CA-EFE-12** (R-EFE-16). Dada una Runa regalada, cuando el jugador elige un tipo con reserva, entonces la tiene y la reserva baja en uno; sin Fuentes con reserva, entonces el efecto no es válido; cuando no elige a tiempo, entonces no recibe runa, se emite la burla y se registra insolencia.
- [ ] **CA-EFE-13** (R-EFE-18). Dada la Noche de los Ecos activa, cuando un Eco calcula sus casillas alcanzables, entonces incluyen las de Dominios con dueño; cuando termina en una Fuente de un Dominio con dueño, entonces no obtiene runa; cuando se decreta la Ley de Sombras, entonces está en su hogar y vuelve a estar confinado; cuando se alcanza el tope de salvaguarda, entonces está en su hogar y vuelve a estar confinado.
- [ ] **CA-DIR-1** (R-DIR-1). Dada una partida con jugadores de distintos tipos de runa, cuando se resuelve Líder, entonces es quien más tipos distintos tiene; con empate, quien más runas; con empate total, todos los empatados.
- [ ] **CA-DIR-2** (R-DIR-2, 3). Dado un efecto cuya dirección no deja objetivos válidos, entonces no se aplica y se emite el evento de burla.
- [ ] **CA-DIR-3** (R-DIR-4). Dado un activo que debe señalar a la víctima de Vuelta al hogar, cuando el reloj se agota, entonces el propio activo está en su hogar y se registra insolencia. Dado un activo que debe elegir un regalo, cuando el reloj se agota, entonces no recibe nada, se emite la burla y se registra insolencia.
- [ ] **CA-COM-1** (R-COM-1 a 3). Dados los seis ejemplos de R-COM-3 escritos en datos, cuando se cargan y se resuelven en una partida de prueba, entonces producen exactamente el resultado descrito, sin código específico para ninguno.
- [ ] **CA-COM-2** (R-COM-2). Dado un dilema de cuatro opciones, entonces su reloj es el base más tres incrementos; cuando se agota, entonces se toma una opción al azar, sus efectos dañinos se aplican al propio jugador, sus regalos se descartan y se registra insolencia.
- [ ] **CA-VAC-1** (R-VAC-2, 3). Dado un Elegido sin Desvío en el Vacío, cuando llega su primer turno, entonces juega el escape en difícil; si falla, en el segundo juega en normal; si falla, en el tercero juega en fácil; si falla, en el cuarto está en su hogar sin jugar y el turno termina. Cuando supera el escape en cualquier turno, entonces está en su hogar y el turno termina.
- [ ] **CA-VAC-2** (R-VAC-2). Dado Mictlantecuhtli en el Vacío, entonces su primer escape es en fácil y en su segundo turno sale sin jugar. Dado un Eco, entonces sale en su primer turno sin jugar.
- [ ] **CA-VAC-3** (R-VAC-1). Dado un jugador en el Vacío, entonces no es objetivo válido de ningún efecto salvo Liberación, y puede apostar en un duelo y votar.
- [ ] **CA-VAC-4** (R-VAC-4, 5, R-EFE-17). Dado un jugador desterrado por segunda vez, entonces su cuenta de turnos es uno. Dada una Liberación con dos jugadores dentro, entonces ambos están en su hogar en ese instante.
- [ ] **CA-EFE-14** (R-EFE-3). Dada una Consulta forzada al activo en mitad de su turno, entonces el turno se corta con los efectos de R-TAB-26 y se abre el Oráculo para él; dada una Consulta forzada a un rival, entonces el turno del activo no se corta y el Oráculo se abre para el rival.
- [ ] Sin valores fijos en el código: topes, rondas, relojes, dificultades y variantes salen de datos.
- [ ] Registro: cada aplicación de efecto emite un evento con marca de tiempo, efecto, dirección, objetivos y causa; las entradas y salidas del Vacío y las marcas también.

## Nombres nuevos para el glosario

Se añaden al glosario oficial al aprobar este documento: Efecto (`Effect`), Dirección (`EffectTarget`), Sello contra rival (`SealAgainstRival`), Prueba del Warden (`WardenTrial`), Noche de los Ecos (`EchoNight`), Runa regalada (`GiftRune`), Liberación (`ReleaseVoid`), Consulta forzada (`ForcedOracle`), Robo decretado (`DecreedTheft`), Duelo forzado (`ForcedDuel`). Amuleto de Desvío pasa a Fase 2 y se renombrará.

## Fuera de alcance de esta fase

- Amuleto de Desvío (v0.1 5.6): Fase 2, con otro nombre, porque choca con el poder Desvío.
- Trampa del Warden (casilla oculta que manda a casa a quien acaba en ella, anunciada solo por Dominio): Fase 2. Idea del autor del 18/09 a partir de un Sello-portal, descartado para mantener el Sello como barrera.
- Trato del Warden (salir del Vacío pagando): idea para el GDD del Warden; se pisa con Desvío.
- Descartados a propósito: perder tantos turnos como marque un dado y quedarse sin jugar hasta sacar una tirada concreta. Rompen el pilar 2. Su sustituto es la Prueba del Warden.
- Insolencia, Furia, actos, sacos, disparadores, Oráculo y votaciones: GDD del Warden.
- Voz distorsionada en el Vacío: Fase 2, con la voz.
- Efectos nuevos (intercambios de posición, pasar runas al de la izquierda, etc.): solo si se pueden componer con estas piezas; si no, son ampliación del catálogo en una fase posterior.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| **Turnos perdidos dentro del Vacío** (S-013, decidido por el autor el 08/10/2026): un turno saltado por turno perdido no cuenta como turno propio en el Vacío, así que la escalera no avanza y la estancia puede pasar de cuatro turnos reales. Alternativa: que el salto cuente como peldaño | Simulación y playtest | Primera build | Si ocurre a menudo, cambiar a que cuente |
| **Desvío en el turno de salida automática** (S-013, 08/10/2026): se ofrece igual; usarlo es salir jugando el turno en vez de salir sin jugar | Autor | Decidido | |
| **Desterrar o mandar a casa a quien está en juego** (S-013, 08/10/2026): hasta las interrupciones de S-015, las funciones lo rechazan (asiento activo con turno empezado, o participante de duelo o trilero) | Autor | Decidido; S-015 lo convierte en interrupción | |
| **Turno extra a quien está en el Vacío** (detectado en S-013, 08/10/2026): avanzaría su escalera de escape, porque sería un turno propio dentro. Propuesta: no es objetivo válido de Turno extra (R-DIR-2 al pie de la letra) | Autor | Al escribir S-014 | |
| **Líder cuando nadie tiene runas** (S-014, 12/10/2026): al pie de la letra empatan todos y un efecto "al líder" alcanza a los cuatro. Alternativa: "sin runas no hay líder" (burla). Hoy, al pie de la letra; el cerebro del Warden no elige "al líder" sin runas en la mesa | Autor | GDD del Warden | |
| **Principio (autor, 14/10/2026): el Warden nunca parece equivocarse.** El motor rechaza lo que no vale (R-DIR-3, burla) y el cerebro del Warden no elige lo que el motor rechazaría; la burla es la red de seguridad, no un comportamiento buscado. Consecuencias decididas en S-014: una Noche con otra activa no vale; la Noche no vale en carrera; la Ley de Sombras sin Noche y con los Ecos en casa no vale; "al azar" incluye a quien dirige el evento (salvo Marcado); un regalo descartado en un dilema en contra lo comenta el Warden con su voz (S-016), no el motor | Autor | Decidido; vale para S-016 | |
| **Decisiones de S-015 (autor, 15/10/2026)**: (1) Duelo forzado: el motor filtra a los rivales sin runas (señalar a uno anularía el duelo y sería la forma de esquivarlo). (2) Robo decretado: cuando señala el jugador se le ofrecen todos los rivales del tablero y robar a uno sin runas es burla para él (como R-ROB-4); cuando elige el Warden ("a todos", "al azar") se salta a los sin runas. **Cambia R-DIR-2 para el robo**: el filtro vale cuando elige el Warden, no cuando señala el jugador. (3) **Duelo contra el Warden** (`WardenDuel`, nombre nuevo): desatender un Duelo forzado es batirse contra el Warden, que reta y rinde un valor fijo de datos (0,75); si gana el Warden, lo que estaba en juego vuelve a la Fuente; si gana el jugador, no pierde nada; insolencia siempre. Desatender un Robo decretado es que el Warden te robe (una runa al azar a su Fuente). **Sustituyen a "contra un rival al azar" de R-DIR-4**, que premiaba a un rival que no había hecho nada. (4) Una segunda Prueba del Warden sobre quien ya la tiene no es válida | Autor | Decidido | |
| **La Furia sobre un Eco dura un turno** (visto en PIE el 20/10/2026, PR B de S-016): un Eco sale del Vacío en su primer turno sin jugar (R-ECO-5) y el Vacío de la Furia solo anula Desvío y Liberación, así que en la carrera (todos son Ecos) cinco insolencias se pagan con un turno perdido. **Decidido el 20/10/2026: la Furia castiga al jugador, no al personaje, así que iguala a todos.** En el Vacío de la Furia no se aplica la regla del Eco (R-ECO-5) ni la escalera de Mictlantecuhtli (su afinidad): todos juegan una escalera propia de datos, más dura (`furyEscapeLadder`, valor inicial difícil, difícil, normal; cuarto turno fuera, tope del pilar 2), sin Desvío ni Liberación. El Vacío de la Furia es otro sitio, con nombre y aspecto propios (autor, con la interfaz). Cambia R-INS-3 del GDD 06 ("Mictlantecuhtli conserva su afinidad"). Va en el PR B de S-016 | Autor | Decidido | PR B de S-016 |
| Duración de Marcado y Brazal revelado en rondas | Simulación y playtest | Primera build | |
| Cuándo termina el Warden la Noche de los Ecos (disparador en su GDD) | Autor | GDD del Warden | Propuesta: al primer robo de un Eco a un Elegido, o pasadas N rondas sin robo |
| Qué cuenta como "perjudicar" al Marcado: ¿también apostar contra él? | Autor | Antes del sprint de castigos | Propuesta: no; solo acciones directas |
| Reloj de dilema base e incremento | Playtest | Primera build | |
| Prueba del Warden: ¿dificultad normal o la del duelo? | Autor | GDD de minijuego | Provisional: normal |
| Cuarto turno en el Vacío: ¿se alcanza a menudo? Si sí, acortar la escalera a tres | Simulación y playtest | Primera build | |
