# Carrera por el vínculo y Elegidos

> **Estado en la entrega del TFM (noviembre de 2026)**: Implementado casi entero: la carrera (S-006, con su corte al acabar en S-017), el mapa de runas y poderes (S-008, S-009), las seis afinidades (S-008, S-009, S-010, S-012, S-013, S-017), los Ecos (S-010, S-013) y el Grimorio (S-027, S-044). Se hicieron de otra manera: el fin de la carrera corta el turno en curso; la Noche de los Ecos, que aquí se deja para la Fase 2, está implementada (S-014, S-043); Tifón rechaza dentro del plazo de la consulta, sin plazo propio; en el Vacío de la Furia todos juegan la misma escalera (S-016). No existen las cinemáticas, los retratos, el sonido ni el bot.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (17/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-16
> **Fase**: 1
> **Pilares que sirve**: 1, 2, 3, 4
> **Fuente**: GDD v0.1 secciones 2.3, 3.4, 3.5.2, 3.5.3, 6; decisiones del autor del 16/09/2026

## Resumen

Define los cinco primeros minutos de partida, en los que los Ecos compiten por vincularse a uno de los seis Elegidos, y qué es cada Elegido: su afinidad y su mapa de runa a poder. Define también qué puede y qué no puede hacer un Eco que no lo consigue. Los Elegidos son los seis terrores de seis mundos, ecos encadenados que el Warden colecciona.

## Fantasía del jugador

Sales del Altar siendo nadie, un espectro con un dado, y tienes cinco minutos para llegar a un Nicho antes que los demás y convertirte en algo terrible. Si lo consigues, el tablero despierta contigo. Si no, serás la plaga que acecha a los que sí.

## Reglas

### La carrera

- **R-CAR-1.** La carrera por el vínculo empieza con la partida y dura un tiempo definido en datos, valor inicial cinco minutos, o menos si todos los Nichos disponibles se reclaman antes (R-TAB-27).
- **R-CAR-2.** Todos los jugadores empiezan como Ecos Perdidos en el Altar, con un dado por turno (R-TUR-5), y pueden transitar por cualquier Dominio (R-TAB-20).
- **R-CAR-3.** Con N jugadores, el Warden corrompe 6 menos N Dominios al inicio (R-TAB-6). Los Nichos de los N Dominios no corrompidos empiezan en estado Libre, que significa que se pueden reclamar, y son visibles y reconocibles desde el inicio. Los de los Dominios corrompidos empiezan Cerrados. No hay nada que descubrir.
- **R-CAR-4.** Un Eco reclama un Nicho al terminar su movimiento en él (R-TAB-14). Solo puede reclamarse un Nicho en estado Libre; el primero que lo reclama se lo queda.
- **R-CAR-5.** Al reclamar un Nicho, el Eco se vincula al Elegido de ese Dominio: pasa a estado Elegido, obtiene su afinidad y su mapa de runa a poder. Sigue tirando un dado: los dos dados solo los da el poder Celeridad (R-TUR-5). El primer vínculo de la partida provoca el Despertar del tablero (R-TAB-9).
- **R-CAR-6.** Un jugador no puede cambiar de Elegido ni volver a ser Eco durante la partida.
- **R-CAR-7.** Durante la carrera el Warden actúa según su Acto I, el más tranquilo (R-TUR-12), cuyo saco de eventos no incluye el destierro al Vacío, y sus castigos por insolencia tampoco. Nadie va al Vacío durante la carrera, con una sola excepción: la Furia del Warden (R-INS-3).
- **R-CAR-7b.** Hasta el Despertar nadie obtiene runas: las Fuentes despiertan con el tablero (R-TAB-8 y 9). Desde el Despertar, también durante la carrera, se aplica la regla general: un Elegido obtiene runas en cualquier Fuente, con minijuego en las de Olvidados; un Eco, solo en las de Olvidados y sin minijuego (R-ECO-3). Hueco aceptado y a medir: un Eco que coge una runa en un Olvidado durante la carrera y luego se vincula.
- **R-CAR-8.** Al terminar la carrera se aplican, en este orden: si nadie se ha vinculado, victoria del Warden (R-TAB-11); todo Dominio sin dueño pasa a Olvidado (R-TAB-10); cada Eco recibe un Dominio Olvidado propio (R-TAB-12); se anuncian los resultados.

### Los Elegidos

- **R-ELE-1.** Hay seis Elegidos, uno por Dominio. Cada uno tiene una afinidad, un poder pasivo único, y un mapa de runa a poder, fijo y distinto de los demás.
- **R-ELE-2.** Las afinidades son:

| Elegido | Dominio (provisional) | Afinidad |
|---|---|---|
| Fenrir | El Fiordo Encadenado | Atraviesa y pisa casillas selladas sin necesidad de Paso Libre |
| Shuten-dōji | El Banquete | Ventaja en el minijuego de duelo: margen de acierto mayor, definido en datos |
| Apep | El Río de la Noche | Una vez por turno, tras ver su tirada y antes de marcar destino, puede repetirla; acepta el segundo resultado (R-TUR-6) |
| Mictlantecuhtli | El Descenso | En el Vacío, su primer intento de escape ya es en dificultad fácil y sale como máximo en su segundo turno (los demás: difícil, normal, fácil, salida automática al cuarto; GDD de castigos) |
| Tifón | La Ruina Tormentosa | Tras revelarse un destino del Oráculo, tiene un tiempo corto definido en datos para rechazarlo; si lo rechaza, consulta de nuevo y el segundo resultado es obligatorio |
| Koschei | El Bosque Negro | Al ser víctima de un robo, antes del trilero elige una runa que queda a salvo |

- **R-ELE-3.** Hay seis tipos de runa, uno por Dominio, nombrados por él: Banquete, Fiordo, Río, Descenso, Ruina y Bosque. Cada Elegido asigna cada tipo de runa a uno de los seis poderes, Duelo, Celeridad, Paso Libre, Expulsión, Vuelo Planar y Desvío, de modo que ningún Elegido repite poder y ninguna runa da el mismo poder a dos Elegidos. Qué hace cada poder lo define el GDD de runas.

| Runa → | Banquete | Fiordo | Río | Descenso | Ruina | Bosque |
|---|---|---|---|---|---|---|
| **Shuten-dōji** | **Celeridad** | Paso Libre | Expulsión | Desvío | Vuelo Planar | Duelo |
| **Fenrir** | Duelo | **Celeridad** | Paso Libre | Expulsión | Desvío | Vuelo Planar |
| **Apep** | Vuelo Planar | Duelo | **Celeridad** | Paso Libre | Expulsión | Desvío |
| **Mictlantecuhtli** | Desvío | Vuelo Planar | Duelo | **Celeridad** | Paso Libre | Expulsión |
| **Tifón** | Expulsión | Desvío | Vuelo Planar | Duelo | **Celeridad** | Paso Libre |
| **Koschei** | Paso Libre | Expulsión | Desvío | Vuelo Planar | Duelo | **Celeridad** |

La tabla es un cuadrado latino: cada fila y cada columna contiene los seis poderes una vez. Regla universal: **la runa de tu propio Dominio te da Celeridad** (la diagonal), de modo que ningún Elegido parte con ventaja de velocidad y todo Elegido nuevo tiene un primer objetivo claro, la Fuente de su casa. El resto de la tabla es cíclico: si los Dominios se colocan alrededor del Altar en el orden de las columnas, todos los Elegidos tienen cada poder a la misma distancia de su casa. El orden de los cinco poderes restantes es ajustable en datos. Para Fenrir, la runa que da Paso Libre no añade nada a su afinidad; es el precio de tenerla gratis desde el principio.

- **R-ELE-4.** El mapa de runa a poder de cada Elegido es visible para todos en el Grimorio.
- **R-ELE-5.** La afinidad y el mapa de runa a poder de un Elegido no se pierden ni se transfieren. Un poder está activo mientras el Elegido tiene al menos una runa del tipo que se lo da; si pierde la última de ese tipo, pierde el poder hasta que recupere una. Solo las runas cambian de mano.

### Los Ecos Perdidos tras la carrera

- **R-ECO-1.** Un Eco Perdido mantiene un dado por turno (R-TUR-5), no tiene afinidad y sus runas no le dan poderes, con la única excepción de la Runa de Liberación (R-ECO-7): son colección para la condición de victoria.
- **R-ECO-2.** Solo transita por el Altar y los Dominios Olvidados (R-TAB-20). Su hogar es el Nicho cerrado de su Dominio asignado (R-TAB-13).
- **R-ECO-3.** Obtiene la runa al terminar su movimiento en la Fuente de un Dominio Olvidado, sin minijuego. Un Elegido en esa misma Fuente debe superar el minijuego (R-TAB-7).
- **R-ECO-4.** Roba una runa al terminar su movimiento en una casilla ocupada por otro jugador, Elegido o Eco, eligiendo a uno si hay varios. El robo se resuelve según el GDD de runas (trilero).
- **R-ECO-5.** Si es enviado al Vacío, sale de él al inicio de su siguiente turno sin minijuego, y reaparece en su hogar.
- **R-ECO-6.** Puede ganar la partida con las mismas condiciones que un Elegido: seis runas distintas, volver al Altar y superar el desafío final.
- **R-ECO-7.** Runa de Liberación: la runa del Dominio Olvidado asignado a un Eco es su Runa de Liberación. Mientras tiene al menos una, el Eco tiene el poder Celeridad (R-TUR-4 y 5); si pierde la última, lo pierde hasta recuperar una. Es la misma regla que la de los Elegidos: la runa de tu Dominio te da Celeridad. No da inmunidad a la Ley de Sombras ni ningún otro poder, y las demás runas de Olvidados no dan nada al Eco.

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Jugador | Eco en carrera | Inicio de partida | Reclama un Nicho, o termina la carrera | Un dado; transita por todo |
| Jugador | Elegido | Reclama un Nicho | Nunca | Un dado, o dos si tiene Celeridad y lo elige; afinidad; mapa de poderes; transita por todo |
| Jugador | Eco tras la carrera | Termina la carrera sin Nicho | Nunca | Un dado, o dos si tiene su Runa de Liberación y lo elige; sin más poderes; confinado a Altar y Olvidados; hogar asignado |
| Partida | Carrera | Inicio | Tiempo agotado o todos los Nichos reclamados | Acto I del Warden |
| Partida | Caza | Fin de carrera con al menos un Elegido | Fin de partida | Actos II y III |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Tablero | Mutua | Nichos y su estado; Despertar; transiciones de fin de carrera; hogares |
| Turnos y dados | Mutua | Número de dados por estado; repetición de tirada de Apep; fin de carrera como interrupción |
| Runas y poderes | Runas depende de este | Mapa de runa a poder por Elegido; quién puede coger runas dónde; robo del Eco |
| Castigos y Vacío | Castigos depende de este | Afinidad de Mictlantecuhtli; salida inmediata del Eco; afinidad de Koschei en robos |
| Warden | Mutua | Acto I durante la carrera; anuncio de vínculos y de fin de carrera; afinidad de Tifón ante el Oráculo |
| Minijuego | Minijuego depende de este | Ventaja de Shuten-dōji en duelos; exención del Eco en Olvidados |
| Interfaz | Interfaz depende de este | Reloj de carrera; Nichos disponibles; Grimorio con afinidad y mapa |
| Bots | Bots depende de este | Objetivo de carrera: Nicho libre más cercano |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| P(nadie se vincula) | probabilidad | objetivo < 5 % | simulación | Ver GDD de tablero |
| P(un jugador concreto queda Eco) | probabilidad | a medir | simulación | Con cuatro jugadores y Nichos a 2-4 casillas del Altar, debería estar entre el 15 % y el 35 % por jugador; fuera de eso, ajustar distancia o tiempo |
| Victorias de Eco | porcentaje | a medir | playtest | Si supera a las de Elegido, quitar Fuentes a los Ecos; si es cero sostenido y los Ecos se apagan, darles algo más |
| Margen de Shuten-dōji en duelos | parámetro | datos | GDD de minijuego | |
| Tiempo de rechazo de Tifón | segundos | 3-5 | datos | |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| Dos jugadores llegan al mismo Nicho en turnos consecutivos | El primero se vincula; el segundo ve el Nicho como casilla normal y sigue como Eco | R-CAR-4 |
| Un jugador reclama el último Nicho disponible | La carrera termina en ese instante; el resto son Ecos | R-CAR-1, R-TAB-27 |
| Un jugador va al Vacío durante la carrera | Solo por la Furia (R-INS-3); ni el saco del Acto I ni los castigos por insolencia contienen destierro. Como todos son Ecos, saldría al turno siguiente (R-ECO-5), salvo que sea la Furia, que es Vacío de la Furia | R-CAR-7, R-INS-2 y 3 |
| Nadie reclama Nicho | Victoria del Warden | R-CAR-8 |
| Un Eco acaba en la Fuente de un Dominio con dueño | No puede ocurrir: no es transitable para él | R-ECO-2 |
| Un Elegido y un Eco en la misma Fuente de Olvidado en turnos distintos | El Eco coge sin minijuego; el Elegido lo juega | R-ECO-3, R-TAB-7 |
| Un Eco roba a otro Eco | Permitido; trilero igual | R-ECO-4 |
| A un Eco le roban su única Runa de Liberación | Pierde Celeridad hasta coger otra en su Fuente, que tiene en casa y sin minijuego | R-ECO-7 |
| Un Eco reúne seis runas distintas | Puede ir al Altar y jugar el desafío final | R-ECO-6 |
| Apep repite tirada y el reloj de turno se agota | Se aplica R-TUR-8; la repetición cuenta dentro del reloj | R-TUR-6 y 8 |
| Tifón rechaza el Oráculo y el segundo resultado es peor | Obligatorio | R-ELE-2 |
| Koschei con una sola runa es robado | Esa runa queda a salvo; el robo se anula | R-ELE-2; ver GDD de runas para el mensaje |
| Fenrir acaba su movimiento en una casilla sellada | Permitido: para él es transitable como paso y como destino | R-ELE-2, R-TAB-17 |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Duración de la carrera | 5 min | 4-6 | Menos Ecos, menos P(nadie) | Más Ecos, más tensión |
| Distancia Altar-Nicho | 2-4 | 2-5 | Más Ecos | Carrera trivial |
| Margen de Shuten-dōji en duelos | datos | | Duelos desequilibrados | Afinidad irrelevante |
| Tiempo de rechazo de Tifón | 3-5 s | | Menos presión | Afinidad inútil |
| Turnos máximos de Mictlantecuhtli en el Vacío | 2 | 1-2 | Afinidad irrelevante | Vacío trivial para él |

## Presentación en pantalla plana

| Evento o información | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Inicio de la carrera | Escena | El reloj de la Cripta marca el tramo de la carrera (R-DIE-1); los Nichos disponibles se encienden; fichas de Eco en el Altar (aspecto según la dirección de arte, pendiente) | Trueno de inicio | Presenta a los Ecos y corrompe los Dominios sobrantes, con frase por cada uno |
| Nicho disponible | Casilla | Asset del Nicho encendido, con el emblema del Elegido | Latido suave | Nada |
| Vínculo | Ficha y panel | Cinemática corta: la ficha de Eco arde con el color del Elegido y se sustituye por la del Elegido (carta con peana u otra forma según la dirección de arte, pendiente); el retrato cambia; el Brazal aparece | Campanada grave; motivo sonoro del Elegido | Frase de vínculo con el nombre del Elegido (fija) y el del jugador en el subtítulo |
| Despertar del tablero | Tablero | Ver GDD de tablero | | |
| Fin de la carrera | Escena | Reloj a cero o último Nicho; los Dominios sin dueño se corrompen a la vez; a cada Eco se le asigna su Dominio con una línea desde el Altar | Zumbido de corrupción | Anuncia el fin, se burla de los Ecos y les asigna hogar por su nombre |
| Nadie se vincula | Escena | Todo el tablero se corrompe; cinemática de derrota | Carcajada larga | Victoria del Warden con la burla más larga del juego |
| Grimorio de Elegido | Panel | Retrato, afinidad en una frase, tabla de runa a poder con iconos | | |
| Eco tras la carrera | Panel y ficha | Ficha de espectro; panel con su Dominio asignado; los Dominios con dueño se muestran atenuados para él | | |

## Contribución a los pilares y a las métricas

- **Pilar 1**: el Warden abre y cierra la carrera, corrompe, asigna hogares y nombra a cada vinculado. La carrera es su primer acto.
- **Pilar 2**: varios Ecos rondando un Nicho es una escena para toda la mesa; el fin de carrera afecta a todos a la vez.
- **Pilar 3**: seis Elegidos con silueta y color propios; afinidad en una frase; mapa visible en el Grimorio.
- **Pilar 4**: la transformación es una cinemática personal con nombre.
- Métricas: M1 durante la carrera; nuevas para este sistema: Ecos activos en el minuto 30 y victorias de Eco.

## Criterios de aceptación

- [ ] **CA-CAR-1** (R-CAR-1). Dada una carrera, cuando pasa el tiempo de datos o se reclama el último Nicho disponible, entonces termina, lo que ocurra antes.
- [ ] **CA-CAR-2** (R-CAR-2). Dado el inicio, cuando se consulta cualquier jugador, entonces es Eco, está en el Altar, tira un dado y puede transitar por cualquier Dominio.
- [ ] **CA-CAR-3** (R-CAR-3). Dado el inicio, cuando la interfaz consulta los Nichos, entonces hay N Nichos en estado Libre, los de los Dominios no corrompidos, mostrados como reclamables, y 6 menos N en estado Cerrado, los de los Dominios corrompidos.
- [ ] **CA-CAR-4** (R-CAR-4). Dado un Nicho Libre, cuando un Eco termina su movimiento en él, entonces pasa a Reclamado por ese jugador; dado un Nicho Reclamado o Cerrado, cuando otro termina en él, entonces nada cambia.
- [ ] **CA-CAR-5** (R-CAR-5). Dado un Eco que reclama un Nicho, cuando se aplica, entonces su estado es Elegido, tiene la afinidad y el mapa de ese Elegido, y sigue tirando un dado mientras no tenga Celeridad; si era el primero, el tablero pasa a Despierto.
- [ ] **CA-CAR-6** (R-CAR-6). Dado un Elegido, cuando termina en otro Nicho Libre, entonces nada cambia.
- [ ] **CA-CAR-7** (R-CAR-7). Dada la carrera, cuando el Warden evalúa, entonces usa los parámetros del Acto I y ningún evento elegido envía a nadie al Vacío.
- [ ] **CA-CAR-7b** (R-CAR-7b). Dado el tablero en Primordial, cuando cualquier jugador acaba en una Fuente, entonces no obtiene runa. Dado el tablero Despierto durante la carrera, cuando un Eco acaba en la Fuente de un Dominio corrompido con reserva, entonces obtiene la runa sin minijuego; cuando acaba en la Fuente de un Dominio no corrompido, entonces no obtiene nada.
- [ ] **CA-CAR-8** (R-CAR-8). Dada una carrera terminada con E Elegidos y K Ecos, cuando se aplica el fin, entonces si E es cero la partida termina con victoria del Warden; si no, hay 6 menos E Olvidados y cada Eco tiene un Olvidado distinto asignado.
- [ ] **CA-ELE-1** (R-ELE-1, 3). Dada la tabla de mapas, cuando se valida, entonces cada fila y cada columna contienen los seis poderes exactamente una vez.
- [ ] **CA-ELE-2** (R-ELE-2). Dado Fenrir, cuando se calculan sus casillas alcanzables, entonces las selladas cuentan como transitables.
- [ ] **CA-ELE-3** (R-ELE-2). Dado Shuten-dōji en un duelo, cuando se evalúa el minijuego, entonces su margen es el de datos, mayor que el base.
- [ ] **CA-ELE-4** (R-ELE-2). Dado Apep con tirada vista y sin destino marcado, cuando repite, entonces la nueva tirada sustituye a la anterior y no puede repetir de nuevo ese turno.
- [ ] **CA-ELE-5** (R-ELE-2). Dado Mictlantecuhtli en el Vacío, cuando llega su primer turno, entonces su minijuego de escape es el de dificultad fácil; cuando llega su segundo turno sin haber salido, entonces sale sin jugar.
- [ ] **CA-ELE-6** (R-ELE-2). Dado Tifón con un destino del Oráculo revelado, cuando lo rechaza dentro del tiempo, entonces se consulta de nuevo y el segundo resultado se aplica sin opción de rechazo.
- [ ] **CA-ELE-7** (R-ELE-2, R-ROB-5). Dado Koschei víctima de un robo con R runas, cuando elige una a salvo, entonces el trilero se juega con R menos una y la elegida permanece; con R igual a uno, el robo se anula.
- [ ] **CA-ELE-8** (R-ELE-4). Dado cualquier jugador, cuando abre el Grimorio de un Elegido, entonces ve su afinidad y su mapa completo.
- [ ] **CA-ELE-9** (R-ELE-5). Dado un Elegido con una sola runa de un tipo, cuando la pierde, entonces el poder que le daba deja de estar activo; cuando obtiene otra de ese tipo, vuelve a estarlo. Con dos del mismo tipo, perder una no cambia nada.
- [ ] **CA-ECO-1** (R-ECO-1). Dado un Eco tras la carrera, cuando obtiene una runa que no es la de su Dominio asignado, entonces no obtiene ningún poder y sigue tirando un dado.
- [ ] **CA-ECO-2** (R-ECO-3). Dado un Eco que termina en la Fuente de un Olvidado con reserva, entonces obtiene la runa sin minijuego; dado un Elegido en la misma situación, entonces se lanza el minijuego.
- [ ] **CA-ECO-3** (R-ECO-4). Dado un Eco que termina en una casilla con uno o más jugadores, entonces elige a uno y se inicia el robo.
- [ ] **CA-ECO-4** (R-ECO-5). Dado un Eco en el Vacío, cuando empieza su turno, entonces está en su hogar sin minijuego.
- [ ] **CA-ECO-5** (R-ECO-6). Dado un Eco con seis runas distintas en el Altar, entonces puede iniciar el desafío final igual que un Elegido.
- [ ] **CA-ECO-6** (R-ECO-7). Dado un Eco con al menos una runa de su Dominio asignado, cuando empieza su turno, entonces puede elegir uno o dos dados; cuando pierde la última, entonces vuelve a un dado sin elección; cuando se decreta la Ley de Sombras, entonces le afecta igual que a cualquier Eco.
- [ ] **CA-ELE-10** (R-ELE-3). Dada la tabla de mapas, cuando se valida, entonces para cada Elegido la runa de su propio Dominio da Celeridad.
- [ ] Sin valores fijos en el código: duración, distancias, márgenes y umbrales salen de datos.
- [ ] Registro: vínculos, fin de carrera y asignación de hogares emiten eventos con marca de tiempo y jugador.

## Fuera de alcance de esta fase

- De las Runas de Liberación del v0.1 se descartan la inmunidad a la Ley de Sombras y las runas potenciadas al azar. Un segundo poder para Ecos (por ejemplo Duelo con una segunda runa de Olvidado): Fase 2, solo si los playtests muestran Ecos apagados.
- Permiso temporal de invasión para Ecos (v0.1 3.4.3): Fase 2, junto con su decreto contrario.
- Lore extenso de cada Elegido y de cada Dominio (v0.1 sección 6): documento de arte y narrativa, fuera del TFM.
- Avatares de cuerpo completo y VR: Fase 3.

## Decisiones de implementación (S-006, 01/10/2026)

- **Fin de carrera por tiempo**: se aplica al terminar el turno en curso (como mucho 30 s de retraso; nadie pierde una tirada). La interrupción del turno que describe la tabla de interacción con turnos queda para la story del Warden (R-TAB-26); `TODO` en el motor.
- **Eco que acaba la carrera dentro de un Dominio con dueño**: vuelve a su hogar (el Nicho Cerrado de su Dominio asignado), porque el confinamiento de R-TAB-20 lo dejaría encerrado. Los demás Ecos no se mueven. Evento `ReturnedHome`. Aclaración de R-CAR-8.
- **Hogar del Elegido durante la carrera**: su Nicho desde el instante del vínculo (R-TAB-13 se lee en ese orden: Elegido antes que "Altar durante la carrera").
- **Nadie se vincula (E = 0)**: se registra la victoria del Warden y el fin de carrera, y nada más: ni Olvidados nuevos ni hogares; el Warden deja de actuar y la máquina de turnos no avanza.
- **Turno extra pendiente al agotarse el tiempo**: la carrera termina al acabar el turno en curso y el turno extra se juega después, ya en la caza y con sus condiciones (confinamiento si es Eco). Alternativa descartada: que el extra retrase el fin de carrera.
- **Vínculo = Dominio**: en S-006 ser Elegido es estar vinculado a un Dominio; afinidades y mapa de runa a poder llegan con la story de runas.
- **Dummy de carrera**: doble de prueba que va al Nicho Libre más cercano (pasos del grafo, sin mirar Sellos ni Altar); no es el bot del GDD 08. Sus cifras (0 % sin vínculo, 1,5 % de Ecos en el tablero de 35) no sirven para equilibrar.
- **Reparto de hogares con semilla propia** (corregido en S-016, 19/10/2026): `EndBondRace` sacaba valores del sorteo de los dados para repartir los Olvidados entre los Ecos, en un momento que depende de lo que deciden los asientos, así que "misma semilla, mismas tiradas" dejaba de cumplirse desde el fin de carrera (lo destapó el test de determinismo con el Warden real). Ahora baraja con una semilla propia en el estado (la de la partida + 3; el Warden tiene + 2). R-TAB-12 no cambia; a cada Eco le toca otro Olvidado en cada semilla y la crónica y el informe de carrera cambian por eso.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| **La carrera casi nunca deja Ecos en simulación** (informe del 23/10/2026, 50 partidas por tablero con dummies de carrera): Elegidos 3,96 de 4, Ecos 1 % (objetivo 15-35 %), carrera cerrada a los 135 s de 300, en todos los tableros. Causas medidas: los Nichos del 86 están a 3-7 pasos del Altar y con un dado se llega en uno o dos turnos; el Warden del Acto I interviene 1,2 veces por partida (3 % de cortes, objetivo 15 %). Experimentos: Nichos a 7 pasos (extremo exterior) → 3 % y 170 s; con el Warden del Acto I al 0,5 → 4,5 % y 197 s. Decisión del autor (23/10): no mover los Nichos ni el Warden hasta medir con dummies a ritmo humano (15-20 s por turno, cuatro o cinco turnos en la carrera), porque los de la simulación juegan nueve; esa medida va con la story de victoria, que da tiempo de respuesta a los dummies. Palancas apuntadas para entonces: Nichos colgados del fondo del anillo sin salir de la parte central (siete pasos dando la vuelta, como el del Dominio 0), opción `--nichos lejos` del generador, probabilidad del Acto I, duración de la carrera. El objetivo 15-35 % se revisa con ese informe, no antes | Autor | Story de victoria | Pendiente |
| Nombres definitivos en el juego (mitológicos tal cual o nombre propio más título) | Autor | Antes del arte final | Provisional: mitológicos |
| Identidad y nombre de cada Dominio | Autor | Tras estudiar assets | Provisional en R-ELE-2 |
| Rango de P(un jugador queda Eco) | Simulación | Antes del sprint de carrera | |
| Métricas de Eco (activos en minuto 30, victorias) | Playtest | Primera build en red | |
| Frecuencia del hueco de R-CAR-7b (Eco que coge runa en carrera y luego se vincula) | Simulación y playtest | Primera build | Si es frecuente o decisivo, el Eco no coge runas hasta el fin de la carrera |
