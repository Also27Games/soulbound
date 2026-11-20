# Runas, poderes y victoria

> **Estado en la entrega del TFM (noviembre de 2026)**: Están implementadas las reglas de runas y Fuentes, los seis poderes, el duelo, el robo con trilero y la victoria (S-008, S-009, S-010, S-012, S-013, S-018), y las pruebas se juegan con el anillo (S-026). Se hicieron de otra manera: el Nodo del Oráculo no corta el turno (S-017); el fin por tiempo lo corta todo (S-018); el Nodo de Duelo y el robo del Eco solo funcionan acabada la carrera; no hay que esperar una ronda para el desafío final; la zona buena del desafío final es 0,14; el trilero no anima la mezcla. Pasan a la Fase 2: las apuestas y su premio, las trampas del desafío final, el duelo simultáneo entre humanos y la tabla de presentación.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (17/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-17
> **Fase**: 1
> **Pilares que sirve**: 2, 3, 4 (y 1 en la presentación)
> **Fuente**: GDD v0.1 secciones 3.5, 3.6, 5.1, 5.2; decisiones del autor del 17/09/2026

## Resumen

Define cómo se obtienen, se pierden y se roban las runas, qué hace cada uno de los seis poderes, cómo se enfrentan los jugadores entre sí (duelo, Expulsión y robo con trilero) y cómo se gana la partida: seis runas distintas, el Altar y el desafío final. El diseño del minijuego de habilidad en sí tiene su propio documento; aquí solo se dice cuándo se juega y qué se decide con él.

## Fantasía del jugador

Cada runa que engarzas te hace más peligroso, y cada runa que llevas encima es algo que te pueden quitar. Nadie está a salvo: te pueden retar desde la otra punta del tablero, mandarte a casa cuando ya tocabas el Altar, o meterte la mano en el Brazal mientras toda la mesa intenta seguir la esfera con la vista. Y cuando por fin desafías al Warden, si fallas, te lo cobra.

## Reglas

### Obtener y perder runas

- **R-RUN-1.** Hay seis tipos de runa, uno por Dominio (R-ELE-3). Las runas de un jugador están en su Brazal, sin límite de cantidad.
- **R-RUN-2.** Con el tablero Despierto (R-TAB-9, R-CAR-7b), un jugador que termina su movimiento en una Fuente con reserva obtiene una runa de ese tipo, con las condiciones de quién y dónde de R-TAB-7 y R-ECO-3: un Elegido en cualquier Fuente, con minijuego en las de Olvidados; un Eco solo en las de Olvidados, sin minijuego.
- **R-RUN-3.** Se obtiene una runa por llegada. Para obtener otra de la misma Fuente hay que salir y volver a terminar un movimiento en ella. Quedarse en la Fuente por no tener casillas alcanzables (R-TAB-18) no cuenta como llegada.
- **R-RUN-4.** Si la Fuente no tiene reserva, no se obtiene runa y el Warden lo señala.
- **R-RUN-5.** Se pueden tener varias runas del mismo tipo. Los duplicados no dan nada extra: sirven de seguro para no perder un poder (R-ELE-5) y para diluir el trilero.
- **R-RUN-6.** Toda runa que un jugador pierde sin que otro la gane (Desvío, desafío final fallido, eventos del Warden) vuelve a la reserva de su Fuente (R-TAB-4b).
- **R-RUN-7.** El número de runas de cada jugador es siempre visible para todos. Los tipos están ocultos por defecto; se ven durante un trilero (R-ROB-2) y cuando el Warden ordena revelar un Brazal (GDD del Warden).

### Los seis poderes

Un poder está activo mientras el jugador tiene al menos una runa del tipo que se lo da según su mapa (R-ELE-3 y 5). Para un Eco solo existe Celeridad, por su Runa de Liberación (R-ECO-7).

- **R-POD-1. Celeridad.** Antes de tirar, elige uno o dos dados (R-TUR-4 y 5).
- **R-POD-2. Paso Libre.** Las casillas selladas son transitables para él, como paso y como destino (R-TAB-17). No mueve ni retira Sellos.
- **R-POD-3. Duelo.** Al terminar su movimiento en una casilla ocupada, puede retar a duelo a uno de los jugadores que están en ella (R-DUE).
- **R-POD-4. Expulsión.** Al terminar su movimiento en una casilla ocupada, puede mandar a su hogar (R-TAB-13) a uno de los jugadores que están en ella. Sin minijuego y sin defensa. No tiene efecto sobre quien ya está en su hogar.
- **R-POD-5. Vuelo Planar.** Al terminar su movimiento en un Nodo Planar, puede saltar a cualquier otro Nodo Planar. El salto es opcional. La llegada se resuelve como un final de movimiento normal (R-INT-1). Sin el poder, un Nodo Planar es una casilla normal.
- **R-POD-6. Desvío.** Al inicio de su turno en el Vacío, puede sacrificar una runa del tipo que le da Desvío: la runa vuelve a su Fuente, el jugador sale sin minijuego, reaparece en su hogar y juega ese turno con normalidad. Si era su última runa de ese tipo, pierde el poder.

### Llegar a una casilla: qué se resuelve y en qué orden

- **R-INT-1.** Al terminar un movimiento (o un salto planar) se resuelve, en este orden: primero el efecto de la casilla, si lo tiene (Nicho, Fuente, Nodo); después, como mucho **una** acción contra **un** jugador. Si el efecto de la casilla es una consulta al Oráculo, no hay acción contra jugadores: el Oráculo es del Warden y corta el turno (R-TUR-11).
- **R-INT-2.** Las acciones contra jugadores son: duelo portátil (poder Duelo), Expulsión (poder Expulsión), robo (solo Ecos, R-ECO-4) y reto a distancia (Nodo de Duelo, R-DUE-2). Todas son opcionales. Quien tiene varias disponibles elige una. Si hay varios jugadores en la casilla, elige a uno (R-TAB-19).
- **R-INT-3.** No se puede retar, expulsar ni robar a un jugador que está en el Vacío.
- **R-INT-4.** Los jugadores nunca se quitan turnos entre sí. Quitar turnos es exclusivo del Warden.

### El duelo

- **R-DUE-1.** Un duelo enfrenta a un retador y a un retado en el minijuego de habilidad, presentación de duelo. El minijuego devuelve siempre un ganador (GDD de minijuego). La ventaja de Shuten-dōji se aplica sea retador o retado (R-ELE-2).
- **R-DUE-2.** Nodo de Duelo: quien termina su movimiento en él puede retar a cualquier jugador del tablero, esté donde esté, sin necesitar el poder Duelo. Vale para Elegidos y para Ecos. El número de Nodos de Duelo sale de datos y puede ser cero (R-TAB-5).
- **R-DUE-3.** Un duelo no se puede rechazar.
- **R-DUE-4.** Si el retado no tiene runas, no hay duelo: el Warden se burla de los dos y la acción se pierde.
- **R-DUE-5.** El ganador roba una runa al perdedor con el trilero (R-ROB), sea retador o retado.
- **R-DUE-6.** Si el perdedor no tiene runas (solo puede ocurrir con un retador sin runas desde un Nodo de Duelo), vuelve a su hogar y el Warden se burla de él por pardillo.
- **R-DUE-7.** Apuestas: antes de empezar el duelo, cada jugador que no participa tiene un tiempo corto definido en datos para apostar por un ganador. Apostar es opcional. Quien acierta recibe el premio de apuesta; quien falla no pierde nada, salvo la burla del Warden. El premio se define en datos; valor inicial: en su siguiente tirada, tras verla, puede mover el resultado, el resultado menos 1 o el resultado más 1, a su elección, con mínimo 1; es decir, se le iluminan como alcanzables las casillas de las tres distancias. Usarlo es opcional. El premio vale solo para esa siguiente tirada, se use o no, y no se acumula: como mucho uno pendiente por jugador.
- **R-DUE-8.** El Warden puede forzar un duelo entre dos jugadores como evento propio (GDD del Warden). Se aplican R-DUE-1 y 3 a 7; el Warden decide quién hace de retador.

### El robo y el trilero

- **R-ROB-1.** El trilero se juega cuando una regla da a un jugador (ladrón) el derecho a robar una runa a otro (víctima): victoria en duelo (R-DUE-5), robo de Eco (R-ECO-4) o evento del Warden. El Eco roba directamente, sin duelo previo.
- **R-ROB-2.** Secuencia: todas las runas de la víctima, duplicados incluidos, salen a la vista de toda la mesa; se ocultan cada una en una esfera; las esferas se mezclan con un movimiento continuo que se puede seguir con la vista, de velocidad y duración definidas en datos; el ladrón tiene un tiempo definido en datos, valor inicial cinco segundos, para elegir una esfera.
- **R-ROB-3.** La runa elegida pasa al Brazal del ladrón, aunque ya tenga de ese tipo. El resto vuelve a la víctima. Si el ladrón no elige a tiempo, no roba nada: todas las runas vuelven a la víctima y el Warden se burla de él por lento.
- **R-ROB-4.** Si la víctima no tiene runas, no hay trilero y el Warden se burla del ladrón.
- **R-ROB-5.** Koschei como víctima aparta antes una runa, que no entra en la mezcla (R-ELE-2). Solo vale contra robos, no contra pérdidas impuestas por el Warden.
- **R-ROB-6.** La mezcla se genera a partir de una semilla, igual para todos los jugadores, de modo que toda la mesa ve el mismo movimiento y el resultado es reproducible en pruebas.

### La victoria

- **R-VIC-1.** Si al inicio de su turno un jugador, Elegido o Eco, está en el Altar y tiene las seis runas de tipos distintos, en lugar de su turno normal juega el desafío final: seis pulsos del minijuego en dificultad normal, sin un solo fallo, con trampas del Warden (GDD 09, R-MIN-12). Como el Altar detiene siempre el movimiento (R-TAB-16), entre llegar y desafiar pasa al menos una ronda completa: es la ventana de los demás para impedirlo.
- **R-VIC-2.** Si supera el desafío final, gana la partida, que termina en ese instante.
- **R-VIC-3.** Si lo falla, el Warden le arranca una runa al azar de entre todas las que tiene, que vuelve a su Fuente, lo manda a su hogar y su turno termina. Para volver a intentarlo debe cumplir de nuevo R-VIC-1.
- **R-VIC-4.** Si el tiempo de partida se agota sin ganador, gana el Warden (GDD del Warden). Decisión del autor del 27/10/2026: el fin corta todo lo que haya en marcha, también un desafío final a medias (GDD 06, cuestiones abiertas). Dummies: superan el desafío final con una probabilidad en datos, valor inicial 0,35 (S-018).

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Poder de un jugador | Inactivo | No tiene runa del tipo que lo da | Obtiene una | Sin efecto |
| Poder de un jugador | Activo | Tiene al menos una runa de ese tipo | Pierde la última | Aplica R-POD |
| Fuente | Dormida | Inicio de partida | Despertar | Casilla normal |
| Fuente | Con reserva | Despertar, o recupera una runa | Reserva a cero | Da una runa por llegada |
| Fuente | Agotada | Reserva a cero | Recupera una runa | No da runa; burla del Warden |
| Duelo | Apuestas | Reto válido | Fin del tiempo de apuesta | Espectadores eligen ganador |
| Duelo | Minijuego | Fin de apuestas | Hay ganador | Foco de evento sobre los dos |
| Duelo | Botín | Hay ganador | Trilero resuelto, o perdedor a su hogar | R-DUE-5 y 6 |
| Trilero | Muestra, Mezcla, Elección, Resuelto | R-ROB-2 | | Secuencial, con tiempos de datos |
| Premio de apuesta | Pendiente | Acierta una apuesta | Lo usa o lo deja pasar en su siguiente tirada | Permite sumar o restar 1 |
| Jugador | Aspirante al desafío | En el Altar con seis distintas | Pierde una runa, lo mueven, o llega su turno | Al llegar su turno, desafío final |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Tablero | Runas depende de este | Fuentes y reservas, Nodos, hogares, transitabilidad con Paso Libre |
| Turnos y dados | Mutua | Celeridad en decisiones previas; premio de apuesta tras la tirada; Desvío y desafío final al inicio del turno; toda acción contra jugadores ocurre en la fase de resolución |
| Carrera y Elegidos | Runas depende de este | Mapa de runa a poder, afinidades (Shuten-dōji, Koschei), reglas de Eco, Runa de Liberación |
| Castigos y Vacío | Mutua | Desvío; inmunidad de quien está en el Vacío |
| Warden | Mutua | Burlas (mendigo, pardillo, Fuente agotada, acaparador); duelo forzado; revelar Brazales; eventos que dan o quitan runas; narración de duelos y del desafío final |
| Minijuego | Runas depende de este | Duelo, Fuentes de Olvidados y desafío final son presentaciones del mismo minijuego |
| Interfaz | Interfaz depende de este | Brazal propio y contadores ajenos; elección de acción y de objetivo; apuestas; trilero |
| Bots | Bots depende de este | Decidir acción contra jugadores, objetivo, apuesta y esfera |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| Duelos entre jugadores por partida | entero | objetivo 3 a 8 | simulación y playtest | Si queda por debajo, más Nodos de Duelo; si por encima, menos |
| P(acertar la esfera deseada) | probabilidad | objetivo 40-70 % con 3 runas, menos con más | playtest | Se ajusta con velocidad y duración de la mezcla. Al 100 % el hábil se escapa; a 1/n es azar puro |
| Intentos de desafío final por partida | entero | a medir | simulación | Con R-VIC-3, varios asaltos son deseables |
| Partidas que gana el Warden por tiempo | porcentaje | a medir | simulación | Si sube mucho, revisar reserva de Fuentes y dificultad del desafío |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| Llegar a una Fuente donde hay otro jugador | Primero la runa, luego una acción contra él si se tiene | R-INT-1 |
| Llegar a un Nodo de Duelo ocupado teniendo el poder Duelo | Una sola acción: reto a distancia o duelo portátil, a elegir | R-INT-2 |
| Saltar con Vuelo Planar a un Nodo ocupado | Se resuelve la llegada: una acción contra un ocupante | R-POD-5 |
| Expulsar a quien está en el Altar con seis runas | Vuelve a su hogar; ya no cumple R-VIC-1 | R-POD-4 |
| Expulsar a un Eco | Va al Nicho cerrado de su Dominio asignado | R-TAB-13 |
| Expulsar a alguien que ya está en su hogar | No seleccionable | R-POD-4 |
| Retar a quien no tiene runas | No hay duelo; burla a los dos | R-DUE-4 |
| Retador sin runas pierde | A su hogar; burla por pardillo | R-DUE-6 |
| Retador sin runas gana | Roba con trilero | R-DUE-5 |
| El ladrón roba un tipo que ya tiene | Se añade como duplicado | R-ROB-3 |
| Robar a la víctima su última runa de un tipo | La víctima pierde ese poder al instante; si era Celeridad, vuelve a un dado | R-ELE-5 |
| Robar a Koschei con una sola runa | El robo se anula | CA-ELE-7 |
| Todas las runas de un tipo están en manos de un jugador | Permitido; el resto debe robarle. El trilero favorece sacarle ese tipo. El Warden puede señalarlo | R-RUN-5 |
| El aspirante pierde una runa antes de su turno | No hay desafío; juega turno normal | R-VIC-1 |
| Dos aspirantes en el Altar | Cada uno desafía en su turno, por orden; el primero que lo supera gana | R-VIC-1 y 2 |
| El Warden interrumpe durante un duelo o un trilero | No lo hace: duelos, trileros y desafío final no se interrumpen (GDD del Warden) | Legibilidad, pilar 3 |
| Desvío con varias runas de ese tipo | Sacrifica una y conserva el poder | R-POD-6 |
| Un jugador con premio de apuesta tira con dos dados | El más o menos 1 se aplica a la suma | R-DUE-7 |
| Un participante del duelo se desconecta | GDD de multijugador | |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Reserva inicial por Fuente | 4, igual al número de jugadores de la partida, bots incluidos | 3-6 | Más duplicados, menos bloqueo | Más robo obligado, más riesgo de estancamiento |
| Nodos de Duelo | 2 | 0-3 | Más duelos | Duelos solo con poder y por el Warden |
| Nodos Planares | 2 | 2-4 | Más movilidad | Vuelo Planar pierde valor |
| Tiempo de apuesta | 5 s | 3-8 | Ritmo más lento | Apuestas a ciegas |
| Premio de apuesta | más o menos 1 a la siguiente tirada | datos | | |
| Velocidad y duración de la mezcla | datos | | Hacia azar puro | Hacia robo seguro |
| Tiempo para elegir esfera | 5 s | 3-7 | | |
| Zona buena del desafío final | 0,20 (GDD 09) | 0,15-0,25 | Final más fácil | Más asaltos, más victorias del Warden |

## Presentación en pantalla plana

| Evento o información | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Obtener runa | Fuente y Brazal | La runa sube de la Fuente y se engarza en el Brazal; contador de reserva de la Fuente visible | Tintineo grave | Frase solo en la primera de cada jugador |
| Poder activado o perdido | Panel del jugador | Icono del poder se enciende o se apaga, con nombre | Acorde corto | Nada |
| Fuente agotada | Fuente | Fuente apagada | Eco hueco | Burla |
| Elegir acción y objetivo | Casilla | Iconos de las acciones disponibles sobre cada ocupante | | |
| Reto desde Nodo de Duelo | Tablero | Rayo desde el Nodo hasta la ficha del retado | Trueno | Anuncia el duelo con los dos nombres |
| Apuestas | Sobre los avatares de los dos duelistas (GDD 07) | Componente de selección; la luz se apaga con el tiempo | Tic-tac | "Hagan sus apuestas" |
| Duelo | Foco de evento | Luces fuera, foco sobre los dos; minijuego a pantalla para todos | | Narra y se burla del perdedor y de quien apostó mal |
| Expulsión | Tablero | La ficha es arrastrada hasta su hogar | Cadenas | Frase breve |
| Trilero | Foco de evento | Runas a la vista, esferas, mezcla, elección; todos lo ven igual | Zumbido creciente | Burla al resolver; si el ladrón no elige a tiempo, burla por lento |
| Reto a un mendigo, pardillo | Escena | Foco sobre los dos, o sobre el retador | Carcajada | Burla |
| Aspirante en el Altar | Tablero y HUD de todos | El Altar se enciende con el color del aspirante; aviso a toda la mesa de que desafiará en su próximo turno | Latido que acelera | Avisa a los demás: "alguien cree que puede conmigo" |
| Desafío final | Zoom al Altar | Engarce de las seis runas; minijuego a pantalla para todos | | Se enfrenta al aspirante |
| Desafío fallido | Altar | El Warden arranca una runa, que vuela a su Fuente; ficha arrastrada a su hogar | Carcajada | Se lo cobra con nombre |
| Victoria | Escena | Cinemática; el Warden furioso | | |

## Contribución a los pilares y a las métricas

- **Pilar 1**: el Warden narra cada duelo, se burla del mendigo y del pardillo, fuerza duelos y cobra el desafío fallido.
- **Pilar 2**: apuestas en cada duelo; trilero que toda la mesa intenta seguir; el aviso de aspirante pone a todos a cazar al mismo jugador.
- **Pilar 3**: una sola acción contra un jugador por llegada; número de runas siempre visible; cada poder es una frase.
- **Pilar 4**: burlas y cobros con nombre.
- Métricas: M1 (duelos y trileros cuentan como momento para todos), M3 (burlas), duelos por partida, intentos de desafío final.

## Criterios de aceptación

- [ ] **CA-RUN-1** (R-RUN-2, 3). Dado el tablero Despierto y una Fuente con reserva R, cuando un Elegido termina su movimiento en ella, entonces tiene una runa más de ese tipo y la reserva es R menos 1; cuando empieza su siguiente turno sin haberse movido de ella, entonces no obtiene otra.
- [ ] **CA-RUN-2** (R-RUN-4). Dada una Fuente con reserva cero, cuando un jugador termina en ella, entonces no obtiene runa y se emite el evento de burla.
- [ ] **CA-RUN-3** (R-RUN-6). Dado un jugador que pierde una runa por Desvío o por desafío fallido, entonces la reserva de la Fuente de ese tipo sube en uno.
- [ ] **CA-RUN-4** (R-RUN-7). Dado cualquier jugador, cuando otro consulta su Brazal, entonces ve el número de runas y no los tipos, salvo Brazal revelado.
- [ ] **CA-POD-1** (R-POD-3, 4; R-INT-2). Dado un jugador con Duelo y Expulsión que termina en una casilla con dos jugadores, entonces puede ejecutar una sola acción sobre uno solo, o ninguna.
- [ ] **CA-POD-2** (R-POD-4). Dado un jugador con Expulsión, cuando la usa sobre un Elegido, entonces este pasa a su Nicho; sobre un Eco, a su Nicho cerrado asignado; y un jugador que ya está en su hogar no es objetivo válido.
- [ ] **CA-POD-3** (R-POD-5). Dado un jugador con Vuelo Planar en un Nodo Planar, cuando salta a otro, entonces se resuelve la llegada en el destino según R-INT-1; sin el poder, no se ofrece el salto.
- [ ] **CA-POD-4** (R-POD-6). Dado un jugador en el Vacío con una runa que le da Desvío, cuando la sacrifica al inicio de su turno, entonces está en su hogar, la runa ha vuelto a su Fuente y juega el turno completo.
- [ ] **CA-INT-1** (R-INT-1). Dado un jugador que termina en una Fuente ocupada, entonces primero obtiene la runa y después se le ofrece la acción contra el ocupante.
- [ ] **CA-INT-2** (R-INT-3). Dado un jugador en el Vacío, entonces no aparece como objetivo de reto, Expulsión ni robo.
- [ ] **CA-DUE-1** (R-DUE-2). Dado un jugador sin poder Duelo que termina en un Nodo de Duelo, entonces puede retar a cualquier jugador que no esté en el Vacío.
- [ ] **CA-DUE-2** (R-DUE-4). Dado un reto a un jugador sin runas, entonces no se inicia minijuego y se emite la burla a los dos.
- [ ] **CA-DUE-3** (R-DUE-5). Dado un duelo con ganador G y perdedor P con runas, entonces se inicia un trilero con G como ladrón y P como víctima, sea quien sea el retador.
- [ ] **CA-DUE-4** (R-DUE-6). Dado un retador sin runas que pierde, entonces pasa a su hogar y se emite la burla.
- [ ] **CA-DUE-5** (R-DUE-7). Dado un duelo, cuando un espectador apuesta por el ganador, entonces tiene un premio pendiente; en su siguiente tirada puede sumar o restar 1 antes de marcar destino; después el premio desaparece, lo use o no. Quien falla o no apuesta no cambia de estado.
- [ ] **CA-ROB-1** (R-ROB-2, 3). Dada una víctima con R runas, cuando se resuelve el trilero, entonces hay R esferas, el ladrón tiene una runa más, la víctima una menos, y el total de runas en juego no cambia.
- [ ] **CA-ROB-2** (R-ROB-3). Dado un ladrón que no elige en el tiempo, entonces ninguna runa cambia de mano y se emite la burla al ladrón.
- [ ] **CA-ROB-3** (R-ROB-6). Dada una semilla, cuando se genera la mezcla dos veces, entonces las trayectorias y la posición final de cada esfera son idénticas.
- [ ] **CA-VIC-1** (R-VIC-1). Dado un jugador en el Altar con seis tipos distintos al inicio de su turno, entonces se inicia el desafío final y no hay tirada. Con cinco tipos, o fuera del Altar, entonces turno normal.
- [ ] **CA-VIC-2** (R-VIC-2). Dado un desafío superado, entonces la partida termina con ese jugador como ganador, sea Elegido o Eco.
- [ ] **CA-VIC-3** (R-VIC-3). Dado un desafío fallido, entonces el jugador tiene una runa menos, elegida al azar, la reserva de esa Fuente sube en uno, está en su hogar y su turno ha terminado.
- [ ] **CA-RUN-5** (R-RUN-5). Dado un jugador con dos runas de un tipo, entonces sus poderes activos son los mismos que con una, y ningún cálculo del motor (movimiento, duelo, victoria) cambia por el duplicado salvo la cuenta de runas y el trilero.
- [ ] **CA-INT-3** (R-INT-4). Dada cualquier acción de un jugador contra otro (duelo, Expulsión, robo, reto), cuando se resuelve, entonces los contadores de turnos extra y perdidos de la víctima no cambian.
- [ ] **CA-DUE-6** (R-DUE-3). Dado un reto válido, entonces el retado no recibe ninguna elección para rechazarlo y el duelo empieza tras las apuestas.
- [ ] **CA-ROB-4** (R-ROB-4). Dado un derecho de robo sobre una víctima sin runas, entonces no se abre ningún trilero, ninguna runa cambia de mano y se emite la burla al ladrón.
- [ ] Sin valores fijos en el código: reservas, número de Nodos, tiempos, premio de apuesta y parámetros de mezcla salen de datos.
- [ ] Registro: obtención, pérdida y robo de runas, retos, apuestas, resultados de duelo, expulsiones e intentos de desafío emiten eventos con marca de tiempo y jugadores.

## Fuera de alcance de esta fase

- Mover Sellos con Paso Libre (v0.1 5.5): descartado.
- "Mano de glifos" de duelo (v0.1 3.5.1) y duelos temáticos por Elegido: Fase 2, si acaso.
- Duelos con todas las runas en juego y otros eventos de runas del Warden: GDD del Warden.
- Amuleto de Desvío: GDD de castigos.
- Apuestas en el desafío final: Fase 2.
- Penalización por apuesta fallida: no en Fase 1.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| Premio de apuesta: más o menos 1 es provisional; el autor no está convencido | Autor | Primer playtest | Alternativas: nada mecánico y marcador de aciertos al final; repetir tirada una vez |
| Número de Nodos de Duelo (0, 1 o 2) | Simulación y playtest | Primera build | |
| Velocidad de la mezcla del trilero | Playtest | Primera build | |
| **Ping-pong de Expulsión en el Altar** (detectado por el autor el 05/10/2026): un Elegido con seis runas distintas tiene los seis poderes, Expulsión incluida. Con dos aspirantes, el que llega segundo al Altar expulsa al primero, y el primero vuelve desde su Nicho (a 3-4 casillas, con dos dados llega más del 90 % de las veces) y expulsa al segundo. Ninguno empieza turno en el Altar y gana el Warden por tiempo | Simulación cuando exista la story de victoria; decide el autor | Antes de cerrar la story de victoria | Medir: expulsiones en el Altar por intento de desafío y partidas con tres o más seguidas. Arreglos candidatos: (a) usar Expulsión sobre alguien en el Altar cuesta la runa (vuelve a su Fuente): quien frena al aspirante deja de serlo; (b) quien es expulsado del Altar no puede ser expulsado de nuevo hasta jugar un turno; (c) solo datos: Nichos más lejos del Altar; (d) dejarlo: que dos ambiciosos se anulen y gane el Warden es coherente con el tema, si no es frecuente |
| Reserva inicial por Fuente: 4 (nadie excluido) frente a 3 (escasez que obliga a robar; idea del autor) | Simulación con bots | Antes del sprint de runas | Comparar porcentaje de victorias del Warden por tiempo, duración de partida y duelos por partida con ambos valores |
