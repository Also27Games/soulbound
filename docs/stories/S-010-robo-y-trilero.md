# S-010. Robo y trilero

**Sistema**: runas y poderes (GDD 04), carrera y Elegidos (GDD 03). **Módulos**: `SoulboundCore` y `Soulbound` (vista). **Rama base**: `main`. **Tamaño**: media.

## Contexto

Las runas ya se obtienen, pero no cambian de manos: en la simulación de S-008 las 24 runas se reparten y ahí se quedan. Esta story trae la primera forma de quitárselas a otro, el **robo del Eco**, y el mecanismo con que se resuelve todo robo del juego, el **trilero**. Con ella el Eco deja de pasear y tiene un arma. Introduce además algo que el motor aún no sabe hacer: **pedir una elección a un asiento que no tiene el turno** (la víctima que aparta una runa; en S-011, el retado que gana un duelo y roba).

Decidido por el autor el 05/10/2026: (1) lo que queda del GDD 04 va en dos stories, esta y S-011 (duelo), y las apuestas se aplazan hasta después del 25/10; (2) el Eco solo roba tras la carrera; (3) si Koschei no elige a tiempo, no aparta ninguna runa; (4) en la vista de depuración el trilero no se anima: esferas numeradas y elección a ciegas, hasta la interfaz real.

## Reglas a implementar

- **R-ECO-4, R-INT-2.** Un Eco, tras la carrera, que termina su movimiento en una casilla ocupada por otro jugador (Elegido o Eco) puede robarle una runa; si hay varios, elige a uno. Es opcional. Roba directamente, sin duelo.
- **R-INT-1.** Orden al llegar: primero el efecto de la casilla (un Eco en la Fuente de un Olvidado coge su runa), después como mucho una acción contra un jugador.
- **R-ROB-1.** El trilero se juega cuando una regla da a un asiento (ladrón) el derecho de robar a otro (víctima). En esta story, solo el robo del Eco.
- **R-ROB-2.** Secuencia: todas las runas de la víctima, duplicados incluidos, salen a la vista; se oculta cada una en una esfera; las esferas se mezclan; el ladrón tiene un tiempo (datos, valor inicial 5 s) para elegir una esfera. Duración de la muestra y de la mezcla y número de intercambios, en datos.
- **R-ROB-3.** La runa elegida pasa al Brazal del ladrón, aunque ya tenga de ese tipo; el resto vuelve a la víctima. Si no elige a tiempo, no roba nada y el registro lo deja.
- **R-ROB-4.** Víctima sin runas: no hay trilero y el registro lo deja.
- **R-ROB-5, R-ELE-2 (Koschei).** Koschei como víctima aparta antes una runa, que no entra en la mezcla. Con una sola runa, el robo se anula.
- **R-ROB-6.** La mezcla se genera de una semilla: misma semilla, mismos intercambios y misma posición final de cada esfera.
- **R-ELE-5.** Si la víctima pierde su última runa de un tipo, pierde ese poder en el acto (ya lo calcula `IsPowerActive`; se comprueba).
- **R-INT-4.** El robo no cambia turnos extra ni perdidos.
- **Registro**: inicio del trilero, runa apartada, runa robada, ladrón que no elige, robo a quien no tiene runas y robo anulado, con marca de tiempo y asientos.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-ECO-3** (R-ECO-4). Dado un Eco tras la carrera que termina en una casilla con uno o más jugadores, se le ofrece robar a uno o a ninguno; si elige a uno, empieza el trilero. Durante la carrera, o a un Elegido, no se le ofrece el robo.
- **CA-INT-1** (R-INT-1). Dado un Eco que termina en la Fuente de un Olvidado ocupada, primero obtiene la runa y después se le ofrece el robo.
- **CA-ROB-1** (R-ROB-2, 3). Dada una víctima con R runas (con duplicados), cuando se resuelve el trilero, hay R esferas, el ladrón tiene una runa más, la víctima una menos, y el total de runas en juego y las reservas no cambian. Si el ladrón ya tenía ese tipo, lo tiene duplicado.
- **CA-ROB-2** (R-ROB-3). Dado un ladrón que no elige en su tiempo, ninguna runa cambia de mano y el registro lleva el evento.
- **CA-ROB-3** (R-ROB-6). Dada una semilla y un número de esferas, la secuencia de intercambios y la posición final de cada esfera son idénticas en dos generaciones; con otra semilla, distintas. La runa que recibe el ladrón es la de la esfera que eligió según esa posición final.
- **CA-ROB-4** (R-ROB-4). Dado un robo a una víctima sin runas, no se abre trilero, nada cambia de mano y el registro lleva el evento.
- **CA-ELE-7** (R-ROB-5). Dado Koschei víctima con R runas, se le pide apartar una (elección de un asiento sin turno); el trilero se juega con R menos una y la apartada sigue en su Brazal. Con R igual a uno, el robo se anula. Si no elige a tiempo, se juega con las R.
- **Poder perdido** (R-ELE-5). Dada una víctima que pierde por robo su última runa de un tipo, el poder deja de estar activo en ese instante; si era Celeridad, su siguiente turno es de un dado sin elección.
- **CA-INT-3** (R-INT-4). Tras un robo, los turnos extra y perdidos de la víctima no cambian.
- **El sorteo de los dados no depende de los robos.** Dadas dos partidas con la misma semilla, una con robos y otra sin ellos, los valores del sorteo que consume cada turno son los mismos (el consumo fijo de S-009 se mantiene).
- **Registro.** Los eventos nuevos, con asientos, dato y marca de tiempo.
- **Simulación.** Los dummies roban y eligen esfera al azar con su generador. Determinista por semilla y bajo el segundo. El informe añade robos por partida y runas que cambian de mano.
- Los 101 tests actuales siguen en verde.

Manuales, en PIE con paso a paso, con captura:

- **Robar siendo Eco.** Quedándote Eco tras la carrera y cayendo donde hay otro jugador, la vista pide a quién robar; se ven las runas de la víctima, luego las esferas numeradas y el tiempo; eliges con tecla y la runa aparece en tu Brazal de la leyenda.
- **Ser robado.** Un dummy Eco te roba: la crónica cuenta el trilero y tu Brazal pierde la runa.
- **No elegir.** Dejas pasar el tiempo como ladrón: no robas y la crónica lo dice.

## Notas para quien lo implemente

- Nombres del glosario: Trilero `ShellGame`, Brazal `Bracer` (`FSeatState::Runes`), elección pendiente `FPendingChoice` con `SubmitChoiceIntent`, afinidad `EAffinity::SafeRune`, Eco `ESeatRole::LostEcho`. Nombres nuevos (estado del trilero, parámetros, tipos de elección, eventos) se proponen en el plan y en el PR.
- **Elección de un asiento sin turno.** `FPendingChoice` necesita decir a qué asiento se le pide; `SubmitChoiceIntent` valida ese asiento, no el activo. El anfitrión y la simulación preguntan al decisor de ese asiento. Propón el cambio sin romper las elecciones de S-009.
- **Acción contra un jugador.** Hoy la elección es `Expel` con asientos como opciones. Aquí entra el robo, y en S-011 un Elegido tendrá Duelo y Expulsión a la vez. Propón en el plan una forma que sirva para elegir acción y objetivo sin rehacerla en S-011.
- **El trilero es estado del motor**: ladrón, víctima, runas en las esferas, semilla de la mezcla, y los tiempos como plazos del reloj de partida (muestra, mezcla, elección). La secuencia de intercambios es una función pura de la semilla y del número de esferas, en el motor, para que la interfaz real la anime después. La máquina de turnos espera mientras dura, como espera cualquier elección.
- **La semilla de la mezcla no sale del sorteo de los dados** en un momento que dependa de decisiones: rompería el consumo fijo de S-009. Propón de dónde sale (por ejemplo, un valor base sacado una vez en `StartMatch` y combinado con un contador de trileros). La semilla del sorteo de la partida no va al estado.
- Parámetros en `partida.json`, con validación y valores iniciales: muestra 2 s, mezcla 4 s, intercambios 8, tiempo de elección 5 s. Koschei aparta bajo el reloj de elección (`choiceClockSeconds`).
- Dummies: roban al azar entre los objetivos y "no"; eligen esfera al azar; Koschei aparta al azar. Dobles de prueba.
- Vista: sin animación. Línea de petición con las opciones numeradas, como en S-009, también cuando el humano no tiene el turno (víctima Koschei). Durante la muestra se listan las runas de la víctima; después, solo esferas numeradas. Textos en la crónica.
- Nada de `Engine` en `SoulboundCore`.

## Fuera de esta story

- Duelo portátil, Nodo de Duelo, afinidad de Shuten-dōji: S-011.
- Apuestas y su premio: después del 25/10.
- Robos por evento del Warden, duelo forzado, burlas: Warden.
- Inmunidad de quien está en el Vacío (R-INT-3): story del Vacío.
- Brazal oculto o revelado y animación de la mezcla: interfaz.
