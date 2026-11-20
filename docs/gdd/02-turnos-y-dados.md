# Turnos y dados

> **Estado en la entrega del TFM (noviembre de 2026)**: Las reglas R-TUR-1 a 12, 14 y 15 están implementadas, con los relojes y los topes en datos (S-003, S-009, S-013, S-016, S-017, S-020, S-021). Se hicieron de otra manera: el turno tiene once fases y no cinco; hay un tope de dos turnos perdidos y dos extra; mientras el Warden habla, la mesa espera y los plazos se desplazan (S-023); el fin de la carrera espera a que acabe un duelo, un trilero, una prueba o una consulta, y el fin de la partida lo corta todo (S-017, S-018). Valores que cambiaron: reloj corto de 6 s, voto de 8 s y un reloj de elección de 10 s. No existen las apuestas, las desconexiones ni el bot, y la tabla de presentación es intención. Las reglas R-TUR-16 y 17 nacieron después (S-039, S-046).
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (16/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-16
> **Fase**: 1
> **Pilares que sirve**: 1, 2, 3
> **Fuente**: GDD v0.1 secciones 3.1, 3.4.1; decisiones del autor del 16/09/2026

## Resumen

Define quién juega cuándo, qué es un turno, cómo se tira y cuánto tiempo hay para hacerlo. Es corto a propósito: un turno son dos clics, y todo lo demás lo hace el Warden interrumpiendo.

## Fantasía del jugador

Te toca, y tienes medio minuto antes de que él pierda la paciencia. Decides rápido, porque si aparece mientras dudas, te quedas donde estás y encima se ríe de ti.

## Reglas

Aprobadas el 16/09/2026.

- **R-TUR-1.** La partida se juega por turnos, uno por jugador, en un orden fijo. El orden se sortea una vez al inicio y el Warden lo anuncia con una frase dirigida a cada jugador, no como un sorteo mudo.
- **R-TUR-2.** Un turno tiene estas fases, en este orden: inicio; decisiones previas a la tirada; tirada y elección de destino; movimiento y resolución de la casilla de destino; fin de turno.
- **R-TUR-3.** Inicio: si el jugador está en el Vacío, resuelve su salida según el GDD de castigos (R-VAC-2): un Eco sale sin jugar; quien usa Desvío sale y juega el turno; el resto juega el minijuego de escape. Salvo con Desvío, el turno termina aquí tanto si sale como si no (R-VAC-3).
- **R-TUR-4.** Decisiones previas: un jugador con Celeridad elige si tira uno o dos dados. Cualquier otro poder que altere la tirada antes de lanzar se decide aquí.
- **R-TUR-5.** Tirada: todo jugador tira un dado por defecto, durante la carrera y después, sea Eco o Elegido. Solo quien tiene el poder Celeridad elige uno o dos (R-TUR-4). Los dados son de seis caras normales y el resultado es la suma.
- **R-TUR-6.** Tras ver la tirada y antes de marcar destino, un jugador cuyo Elegido tenga la afinidad de repetir puede repetir la tirada una vez y debe aceptar el segundo resultado. Una vez marcado el destino no se puede repetir.
- **R-TUR-7.** Elección de destino: el jugador elige una de las casillas alcanzables según el GDD de tablero (R-TAB-14). Tirada y elección forman un solo paso.
- **R-TUR-8.** Reloj de turno: el jugador dispone de un tiempo definido en datos, valor inicial treinta segundos, desde que empieza su turno hasta que confirma destino. Las decisiones previas a la tirada cuentan dentro. Si se agota, el turno termina sin mover y el jugador sube un nivel de insolencia, con el castigo que corresponda según el GDD del Warden.
- **R-TUR-9.** Turnos extra: se juegan inmediatamente y seguidos, tras el turno en curso. Un jugador con K turnos extra juega K más uno turnos consecutivos. Cada uno es un turno completo con todas sus fases y su propio reloj.
- **R-TUR-10.** Turnos perdidos: cuando le toca a un jugador con turnos perdidos pendientes, se salta su turno, se descuenta uno, y el Warden lo anuncia. Los turnos perdidos se acumulan.
- **R-TUR-11.** El Warden actúa por tres canales (GDD del Warden): un **golpe** llega en cualquier instante del turno y lo corta con los efectos del tablero R-TAB-26 (la tirada pendiente se pierde y una ficha en movimiento se queda donde está); una **escena** entra entre turno y turno y no corta nada, salvo que en datos esté marcada como cortante; un **comentario** es solo voz y no interrumpe. Los eventos programados (fin de carrera, fin de partida, Furia, aspirante) cortan siempre. Tras resolver un golpe, el turno pasa al siguiente jugador, salvo que el evento diga otra cosa (por ejemplo, turnos extra para el interrumpido). El reloj de partida no se detiene nunca.
- **R-TUR-12.** La frecuencia con la que el Warden interviene se define **por acto** y es creciente: intervalo de evaluación, probabilidad de intervenir, pesos de canal y objetivo de turnos cortados son parámetros por acto en el GDD del Warden (valores iniciales de cortes: 15 %, 25 % y 35 %), con el Warden más tranquilo en la carrera, más presente en la caza y desatado en el clímax. Restricciones: el porcentaje de turnos terminados por golpe se mide por acto en simulación; y en cualquier acto ocurre al menos un golpe o una escena cada dos minutos (pilar 1).
- **R-TUR-13.** Los jugadores que no tienen el turno actúan solo cuando un evento del Warden se lo pide: apostar, votar o elegir. Esas acciones tienen su propio reloj corto, definido en datos, y no alteran el turno activo.
- **R-TUR-14.** Un bot juega su turno con las mismas fases y el mismo reloj. Un jugador desconectado o ausente se gestiona según el GDD de multijugador.
- **R-TUR-15.** El motor emite un evento con marca de tiempo al inicio y al fin de cada turno, en cada tirada, en cada agotamiento de reloj y en cada interrupción, con el jugador afectado.

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Turno | Inicio | Le toca al jugador (o turno extra) | Escape del Vacío resuelto, o no aplica | Intento de escape si procede |
| Turno | Decisiones previas | Inicio superado | Jugador confirma, o no tiene decisiones | Celeridad y poderes previos; reloj corriendo |
| Turno | Tirada y destino | Decisiones cerradas | Destino confirmado, reloj agotado o interrupción | Tirada, repetición opcional, elección; reloj corriendo |
| Turno | Movimiento y resolución | Destino confirmado | Movimiento completado o interrupción | Según GDD de tablero |
| Turno | Fin | Cualquier fase terminada o cortada | Pasa al siguiente | Turnos extra encadenan aquí |
| Jugador | Turnos extra pendientes | Evento del Warden | Se juegan | Contador; se consumen seguidos |
| Jugador | Turnos perdidos pendientes | Evento del Warden o insolencia | Se saltan al tocarle | Contador; el Warden anuncia cada salto |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Tablero | Este depende de Tablero | Casillas alcanzables y ejecución del movimiento; R-TAB-26 para interrupciones |
| Carrera y Elegidos | Este depende de Carrera | Fin de carrera como interrupción |
| Runas y poderes | Este depende de Runas | Celeridad; afinidad de repetir tirada |
| Castigos y Vacío | Mutua | Este cede el inicio del turno al intento de escape; Castigos entrega turnos perdidos |
| Warden | Warden depende de este | Interrumpir en cualquier instante; conceder turnos extra y perdidos; recibir el agotamiento del reloj como insolencia; anunciar el orden inicial |
| Multijugador y bots | Este depende de Multijugador | Quién ocupa cada asiento; ausencias |
| Interfaz | Interfaz depende de este | Fase actual, jugador activo, reloj, orden de turno |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| Tirada 1d6 | entero | 1-6 | dado | Tirada por defecto de todo jugador |
| Tirada 2d6 | entero | 2-12 | dados | Solo con Celeridad, si se elige; distribución triangular con moda 7 |
| Turnos cortados por golpe | porcentaje | objetivo por acto: 15 / 25 / 35 % | simulación | R-TUR-12 |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| El reloj se agota durante las decisiones previas | Turno terminado sin mover, insolencia | R-TUR-8: el reloj empieza con el turno |
| El reloj se agota justo al confirmar destino | Lo que llegue primero al motor manda; el motor registra un orden total de eventos | Sin empates: un solo hilo de reglas |
| Interrupción durante el intento de escape del Vacío | No ocurre: el Warden no interrumpe minijuegos (R-WAR-7); un evento programado espera a que termine | R-WAR-4d y 7 |
| Interrupción en el primer segundo de un turno | El turno termina sin jugar; se mide en R-TUR-12 | R-TUR-11. Es intencionado y raro |
| Un evento concede turnos extra al jugador interrumpido | Termina el turno cortado y empiezan los extra | R-TUR-9 y 11 |
| Turnos extra y turnos perdidos a la vez | Primero se descuentan los perdidos contra los extra, uno a uno; el resto se aplica | Evita contadores contradictorios |
| Un Eco obtiene Celeridad por Runa de Liberación | Puede elegir dos dados aunque sea Eco | R-TUR-5 se aplica salvo poder que diga otra cosa |
| El jugador activo se desconecta | GDD de multijugador; mientras tanto, el reloj sigue y aplica R-TUR-8 | Que la partida no se congele |
| Bot con reloj | El bot decide en menos tiempo que el reloj; si no, se aplica R-TUR-8 igual | Ningún trato especial |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Reloj de turno | 30 s | 20-45 | Turnos más largos, menos presión, M1 sube | Más agotamientos, más insolencia |
| Relojes de acciones de espectador | por acción: apuesta 5 s (GDD 04), voto 8 s (GDD 06), señalar 5 s y dilema 8 s más 3 por opción (GDD 05) | | Más participación, más lento | Menos participación |
| Objetivo de turnos cortados, por acto | I: bajo, II: medio, III: alto (cifras en GDD del Warden) | creciente | Más tensión, más frustración | El Warden parece educado |
| Dados | 1 siempre; 2 solo con Celeridad si se elige | fijo | | |

## Presentación en pantalla plana

| Evento o información | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Orden de turno inicial | Escena | El Warden señala a cada jugador por orden; los paneles se ordenan | Trueno corto | Una frase por jugador, con motivo burlón |
| Jugador activo | Panel y tablero | Panel resaltado; ficha con halo; los demás atenuados | Nada | Nada |
| Reloj de turno | Retrato del activo y su ficha, visibles para todos | La luz del retrato y el aura de la ficha se consumen; sin anillo ni número (R-SEL-3) | Tic en los últimos 5 s | Al agotarse, burla y castigo |
| Tirada | Bandeja de dados delante del Trono del activo (R-DIE-3) | Dados que ruedan con resultado predeterminado por el motor; resultado grande y legible para todos | Dados de hueso | Comentario ocasional a tiradas muy buenas o muy malas |
| Repetir tirada | Junto a la bandeja | Opción del componente de selección visible solo si procede; su luz se apaga con el tiempo | | |
| Turno extra | Panel | Contador "x2, x3" y anuncio | Campanilla | "Otra vez tú" |
| Turno perdido | Panel | Panel tachado; se salta con animación breve | Cadena | Anuncio con nombre |
| Interrupción | Toda la pantalla | Corte de sonido, foco, el Warden se manifiesta; la ficha en movimiento se congela | Trueno | La frase del evento |
| Acción de espectador | Sobre los avatares o cartas en el mundo (GDD 07) | Componente de selección con la luz que se apaga | Redoble | Pregunta a la mesa |

## Contribución a los pilares y a las métricas

- **Pilar 1**: el Warden anuncia el orden, castiga la lentitud y corta turnos. Sin él, el turno es dos clics.
- **Pilar 2**: reloj corto y visible para todos; acciones de espectador con reloj propio. M1 baja.
- **Pilar 3**: fases fijas, un solo paso de tirada y destino, contadores anunciados. M2.
- **Pilar 4**: interrupción con foco y frase al afectado.

## Criterios de aceptación

- [ ] **CA-TUR-1** (R-TUR-1). Dada una partida nueva, cuando se sortea el orden, entonces es una permutación de los jugadores, se mantiene toda la partida, y dos partidas con semillas distintas dan órdenes distintos.
- [ ] **CA-TUR-2** (R-TUR-2). Dado un turno, cuando se ejecuta sin interrupciones, entonces pasa por las cinco fases en orden y ninguna se repite.
- [ ] **CA-TUR-3** (R-TUR-3). Dado un jugador en el Vacío, cuando empieza su turno, entonces resuelve su salida antes de nada y, salvo con Desvío, el turno termina sin tirada tanto si sale como si no; con Desvío, sale y el turno sigue con la tirada.
- [ ] **CA-TUR-4** (R-TUR-4, R-POD-1). Dado un jugador con Celeridad, cuando empieza su turno, entonces puede elegir uno o dos dados antes de tirar; sin Celeridad, no hay elección.
- [ ] **CA-TUR-5** (R-TUR-5). Dado un jugador sin Celeridad, Eco o Elegido, en la carrera o después, cuando tira, entonces es 1d6. Dado un jugador con Celeridad, entonces tira 1d6 o 2d6 según haya elegido en R-TUR-4. Dado un jugador que pierde su última runa de Celeridad, cuando llega su siguiente turno, entonces vuelve a 1d6 sin elección.
- [ ] **CA-TUR-6** (R-TUR-6). Dado un jugador con afinidad de repetir, cuando repite antes de marcar destino, entonces la segunda tirada sustituye a la primera y no puede repetir de nuevo; cuando ya ha marcado destino, entonces no puede repetir.
- [ ] **CA-TUR-7** (R-TUR-7). Dada una tirada, cuando el jugador confirma un destino, entonces es una de las casillas alcanzables devueltas por el tablero.
- [ ] **CA-TUR-8** (R-TUR-8). Dado un turno cuyo reloj se agota antes de confirmar destino, entonces la posición no cambia, el turno termina y el nivel de insolencia del jugador aumenta en uno.
- [ ] **CA-TUR-9** (R-TUR-9). Dado un jugador con K turnos extra, cuando termina su turno, entonces juega K turnos más seguidos antes de que juegue otro.
- [ ] **CA-TUR-10** (R-TUR-10). Dado un jugador con P turnos perdidos, cuando le toca, entonces se salta, P baja en uno y se emite el anuncio.
- [ ] **CA-TUR-11** (R-TUR-11). Dada una interrupción en cualquier fase, cuando se aplica, entonces el turno termina, la tirada pendiente se descarta y, si había movimiento, la posición es la casilla indicada por R-TAB-26.
- [ ] **CA-TUR-12** (R-TUR-12). Dada una simulación de N partidas con la configuración del Warden, cuando se mide por acto, entonces el porcentaje de turnos terminados por interrupción no supera el objetivo de datos de ese acto, los objetivos son crecientes de Acto I a III, y en ningún tramo de dos minutos falta una intervención.
- [ ] **CA-TUR-13** (R-TUR-13). Dado un evento que pide acción a los espectadores, cuando se abre, entonces solo los jugadores no activos pueden responder, dentro de su reloj, y el turno activo no cambia de fase.
- [ ] **CA-TUR-14** (R-TUR-14). Dado un bot, cuando le toca, entonces su turno pasa por las mismas fases y está sujeto al mismo reloj.
- [ ] **CA-TUR-15** (R-TUR-15). Dado cualquier turno, cuando termina, entonces el registro contiene inicio, tirada, fin, y en su caso agotamiento o interrupción, con jugador y marca de tiempo.
- [ ] Sin valores fijos en el código: relojes, dados y objetivo de cortes salen de datos.

## Fuera de alcance de esta fase

- Penalización progresiva por turnos lentos distinta de la insolencia: cubierta por insolencia.
- Pausa total en partida individual (v0.1 menú de pausa): GDD de multijugador e interfaz.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| Valor del reloj de acciones de espectador | Playtest | Primera build en red | |
| Objetivo de turnos cortados | Simulación | Antes del sprint del Warden | |
