# El minijuego de habilidad

> **Estado en la entrega del TFM (noviembre de 2026)**: Está implementada la versión mínima del anillo para el jugador humano en sus cinco usos (S-026), con su lectura visual (S-031, S-036) y la espera con el botón "Empezar" (S-042, regla R-MIN-19, que nació en su story). Se hicieron de otra manera: el duelo se juega de un solo lado y la ventaja de Shuten-dōji es un margen de 0,15 sumado al rendimiento (S-012); la zona buena del desafío final es 0,14, por debajo del rango que este documento da por seguro (valor del autor del 14/11/2026, a revisar con el equilibrio); el panel enseña una franja y una barra, no cifras. Pasan a la Fase 2: el desenfunde, las trampas, el duelo simultáneo, la evaluación en red, la habilidad de los bots, la vista de espectadores y el mando.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (22/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-21
> **Fase**: 1
> **Pilares que sirve**: 2 (los espectadores ven venir el momento), 3 (una mecánica, aprendida una vez), 4 (el Warden juega contra ti en el final)
> **Fuente**: GDD v0.1 secciones 5.1, 5.4.3 y 3.6.2 (todas "pendiente de definir"); decisiones del autor del 21/09/2026

## Resumen

Define la única mecánica de habilidad del juego, el **anillo**, su variante **desenfunde**, la escalera de dificultad, cuántos pulsos y qué umbral tiene cada uso (Fuente de Olvidado, Prueba del Warden, escape del Vacío, duelo, desafío final), cómo se resuelve un duelo, cómo se evalúa en red, cómo lo resuelven los bots, y qué hace especial el desafío final. Se construye una vez y se aprende una vez; lo que cambia es la dificultad, el número de pulsos, el significado del resultado y el vestido.

## Fantasía del jugador

Una runa delante de ti y un anillo que se cierra. Toda la mesa ve venir el momento. Pulsas. Si es un duelo, el otro ha pulsado también y los dos anillos se comparan en el aire. Si es el Altar, son seis, y el Warden mete la mano.

## Reglas

### La mecánica: el anillo

- **R-MIN-1.** Una **prueba** es una secuencia de N **pulsos**. En cada pulso, un anillo aparece grande alrededor de una runa y se cierra hacia su contorno a velocidad constante; el jugador debe pulsar el botón en el instante en que el anillo coincide con el contorno. Un solo botón o clic (R-ENT-3).
- **R-MIN-2.** Cada pulso da una **precisión** de 0 a 1: 1 en la coincidencia exacta, decreciendo linealmente hasta 0 en el borde de la **zona buena** (anchura en datos por dificultad). Pulsar fuera de la zona buena, o no pulsar, es 0. Solo cuenta la primera pulsación de cada pulso.
- **R-MIN-3.** La secuencia de una prueba (instantes de inicio de cada pulso, con ritmo irregular dentro de un rango en datos; sentido de cada anillo, cerrándose o abriéndose) la genera el servidor con semilla. Los dos jugadores de un duelo reciben la misma secuencia.
- **R-MIN-4.** El **resultado** de una prueba es la lista de precisiones. Cada uso define qué hace con ella: un umbral (superada o fallada), una comparación (duelo) o "sin fallos" (desafío final).
- **R-MIN-5.** Variante **desenfunde**: el anillo es invisible; la runa se enciende en un instante al azar dentro de un rango en datos (valor inicial 1 a 4 s) y el jugador pulsa lo antes posible. Precisión: 1 si reacciona dentro del tiempo mínimo de la dificultad, decreciendo hasta 0 en el máximo (ventana en datos); pulsar antes de la señal es 0. Es el mismo componente con otros parámetros. Quién usa cuál lo dice R-MIN-9.

### Dificultades y umbrales

- **R-MIN-6.** Hay tres dificultades en datos: fácil, normal y difícil. Cada una fija la duración del cierre del anillo, la anchura de la zona buena y, para el desenfunde, la ventana de reacción. El desafío final usa la normal con una anchura de zona buena propia en datos (valor inicial 0,20), que se calibra para que se supere en torno a la mitad de los intentos. Valores iniciales:

| Dificultad | Cierre del anillo | Zona buena (fracción del cierre) | Ventana de desenfunde | Umbral de media para "superar" |
|---|---|---|---|---|
| Fácil | 1,6 s | 0,25 | 0,25 a 0,6 s | 0,4 |
| Normal | 1,2 s | 0,18 | 0,2 a 0,45 s | 0,55 |
| Difícil | 0,9 s | 0,12 | 0,18 a 0,35 s | 0,65 |
| Desafío final | normal (1,2 s) | 0,20 (propia) | no se usa | sin fallos: toda precisión mayor que 0 |

- **R-MIN-7.** La ventaja de Shuten-dōji en duelos (R-ELE-2) es una zona buena más ancha: multiplicador en datos, valor inicial 1,4.

### Usos

- **R-MIN-8.** Cada uso define dificultad, número de pulsos, criterio y vestido:

| Uso | Dificultad | Pulsos | Criterio | Vestido |
|---|---|---|---|---|
| Fuente de un Dominio Olvidado (solo Elegidos, R-TAB-7) | fácil | 2 | media mayor o igual al umbral | La runa se arranca de la Fuente corrupta |
| Prueba del Warden (R-EFE-10) | normal | 3 | media mayor o igual al umbral | El Warden examina, foco corto sobre el jugador |
| Escape del Vacío (R-VAC-2) | difícil, normal, fácil por turno; Mictlantecuhtli fácil | 3 | media mayor o igual al umbral | Una grieta en el negro que se cierra; la mesa la ve en su Trono |
| Duelo (R-DUE) | normal | 3 por jugador, misma secuencia | R-MIN-10 | Dos runas que chocan; dos anillos lado a lado para la mesa |
| Desafío final (R-VIC-1) | normal, zona propia | 6, una por runa | R-MIN-12: ninguna precisión 0 | El Altar, las seis runas encendiéndose una a una |

- **R-MIN-9.** Variante: las Fuentes, el escape y el desafío final usan siempre el anillo. En un duelo y en la Prueba del Warden, el Warden elige al azar anillo o desenfunde (peso en datos, valor inicial 70 / 30) y lo anuncia. El desenfunde nunca se usa en el desafío final.

### El duelo

- **R-MIN-10.** Los dos jugadores juegan la misma secuencia **a la vez**, cada uno en su cliente, con inicio sincronizado por el servidor (R-MIN-13). Gana la suma de precisiones más alta. Con desenfunde, gana la suma de reacciones, es decir, quien acumule menos tiempo, con las anticipaciones contando como el máximo.
- **R-MIN-11.** Empate exacto: gana el **retado**. Quien reta tiene que ser mejor; así el motor devuelve siempre un ganador sin sorteo (R-DUE-1). En un duelo forzado por el Warden, el retador es el que él designe (R-EFE-13).

### El desafío final

- **R-MIN-12.** Seis pulsos en dificultad normal con la zona buena propia del final, uno por runa del Altar, en el orden en que el jugador las engarzó. **Sin fallos**: basta una precisión 0 para fallar (R-VIC-3). El Warden a punto de perder no regala nada: la dificultad la ponen la perfección exigida, los seis pulsos y las trampas, no la velocidad del anillo. En una o dos de las seis, elegidas al azar (número en datos), el Warden mete **una trampa** de una lista en datos: el anillo cambia de velocidad a mitad del cierre; el anillo invierte el sentido; la mano del Warden tapa la runa durante los últimos décimas antes del momento (el anillo sigue visible). Nunca en la primera runa. El Warden habla entre runa y runa. No hay apuestas. Los otros tres miran.

### Evaluación en red

- **R-MIN-13.** Es la única excepción a "el cliente nunca evalúa reglas" (R-RED-2), justificada por la latencia: el servidor envía la secuencia (semilla, dificultad, variante, trampas) y un instante de inicio en su reloj; cada cliente la ejecuta en local desde ese instante corregido por latencia y **mide su propia precisión**; envía la lista de precisiones como intención (R-RED-3). El servidor la acepta si es plausible: tantos valores como pulsos, cada uno entre 0 y 1, recibida antes de que termine la duración de la secuencia más una tolerancia en datos (valor inicial 1 s). Si no llega a tiempo o no es plausible, se trata como todo 0.
- **R-MIN-14.** Un jugador cuyo decisor pase a bot durante la prueba (caída) se resuelve por habilidad (R-BOT-6) para los pulsos que falten.
- **R-MIN-15.** En una simulación, el decisor bot resuelve la prueba entera con su habilidad: superada o fallada según probabilidad por dificultad, y en duelo se generan precisiones sintéticas coherentes con esa probabilidad, para que las sumas y los empates existan igual (R-BOT-6).

### Presentación para los espectadores y accesibilidad

- **R-MIN-16.** En duelo y desafío final, los espectadores ven los anillos en grande en el centro (R-FOC-2), sin los controles, con la precisión de cada pulso apareciendo como un número o un destello al resolverse. En el escape del Vacío, la mesa ve la grieta en el Trono del desterrado reaccionar a cada pulso.
- **R-MIN-17.** Cada pulso lleva sonido: un tono creciente durante el cierre y un golpe en el momento exacto, para que el timing no dependa solo de la vista. Con desenfunde, la señal es visual y sonora a la vez.
- **R-MIN-18.** Todo se juega a 60 FPS en la Deck (restricción de la skill); la lectura del instante de pulsación usa la marca de tiempo del evento de entrada, no el fotograma, para que la precisión no dependa de la tasa de refresco.

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Prueba | Anunciada, En curso (pulso k de N), Enviada, Resuelta | Uso que la invoca | Resultado aplicado | R-MIN-1 a 4 |
| Pulso | Esperando, Cerrando, Resuelto | Secuencia | Pulsación o fin | Primera pulsación cuenta |
| Duelo | Ambos en curso, Comparando, Ganador | R-MIN-10 | R-DUE-5 | |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Runas y poderes | Minijuego depende de este | Duelo, desafío final, Fuentes de Olvidado, apuestas sobre el resultado |
| Castigos | Minijuego depende de este | Prueba del Warden, escape del Vacío con su escalera |
| Carrera y Elegidos | Minijuego depende de este | Ventaja de Shuten-dōji; Mictlantecuhtli en fácil |
| Warden | Mutua | Elige variante y trampas; no interrumpe; comenta |
| Interfaz | Interfaz depende de este | Anillo y runa, foco, espectadores, sonido |
| Multijugador y bots | Mutua | Evaluación en cliente, plausibilidad, habilidad del bot, simulación |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| Precisión de un pulso | 0 a 1 | | regla | 1 menos distancia al momento dividida por media zona buena, con mínimo 0 |
| P(humano supera) por dificultad | probabilidad | objetivo fácil 85 %, normal 60 %, difícil 40 %; desafío final 50 % por intento | playtest | Si se aleja, ajustar cierre y zona buena; el final, solo su zona propia |
| P(bot supera) por dificultad | probabilidad | 75 / 50 / 30 %; desafío final 25 % | datos (R-BOT-6) | Un poco por debajo del humano medio, a propósito |
| Duración de una prueba de 3 pulsos | segundos | 5-8 | regla | Ritmo irregular entre pulsos, 0,8 a 2 s |
| Duración del desafío final | segundos | 10-14 | regla | Seis pulsos más frases del Warden |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| Dos pulsaciones en un mismo pulso | Solo cuenta la primera | R-MIN-2 |
| Pulsación entre dos pulsos | No cuenta para ninguno | R-MIN-2 |
| Cliente que envía más precisiones de las debidas | No plausible: todo 0 | R-MIN-13 |
| Resultado que llega tras la tolerancia | Todo 0; si era un duelo, gana el otro | R-MIN-13 |
| Los dos duelistas envían todo 0 | Empate: gana el retado | R-MIN-11 |
| Caída en el pulso 2 de 3 | Los pulsos 2 y 3 los resuelve el bot por habilidad | R-MIN-14 |
| Shuten-dōji contra Shuten-dōji | No ocurre: cada Elegido es de un solo jugador | |
| Trampa en la primera runa del final | No ocurre: nunca en la primera | R-MIN-12 |
| Desenfunde con pulsación anticipada | Precisión 0 en ese pulso; la prueba sigue | R-MIN-5 |
| Warden evalúa durante una prueba | Aplaza (R-WAR-7) | |
| Espectador abre el Grimorio durante el duelo | Lo ve encima; el duelo sigue | R-HUD-4 |
| Tasa de fotogramas baja en un cliente | La precisión se mide por marca de tiempo de entrada, no por fotograma | R-MIN-18 |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Cierre del anillo por dificultad | 1,6 / 1,2 / 0,9 s | | Más fácil | Más difícil |
| Zona buena por dificultad | 0,25 / 0,18 / 0,12 | | Más fácil | Más difícil |
| Zona buena del desafío final | 0,20 | 0,15-0,25 | Final más fácil | Final imposible |
| Umbral de media | 0,4 / 0,55 / 0,65 | | Más difícil | Más fácil |
| Pulsos por uso | 2 / 3 / 3 / 3 / 6 | 1-6 | Pruebas más largas | Más azar |
| Ritmo entre pulsos | 0,8 a 2 s | | | |
| Multiplicador de Shuten-dōji | 1,4 | 1,2-1,6 | Afinidad decisiva | Irrelevante |
| Peso anillo / desenfunde en duelos y Prueba | 70 / 30 | | | |
| Trampas en el final | 1 o 2 de 6 | 0-3 | Final cruel | Final neutro |
| Tolerancia de recepción | 1 s | 0,5-2 | | |

## Presentación en pantalla plana

| Uso | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Anuncio de prueba | Delante del jugador | La runa aparece y el Warden dice qué es (anillo o desenfunde) | Acorde | Anuncia |
| Pulso | Delante del jugador; en grande para la mesa si es duelo o final | Anillo cerrándose; destello en la coincidencia; número o color según precisión | Tono creciente y golpe | Comentario en el final |
| Duelo | Foco de evento | Dos anillos lado a lado; al final, suma de cada uno y ganador | Choque | Narra, burla al perdedor |
| Escape | Vista del Vacío para el desterrado; grieta en su Trono para la mesa | Grieta que se abre con cada acierto | Susurros | Comentario al salir o al fallar |
| Fuente de Olvidado | Fuente | La runa se arranca o se hunde | Tintineo o chasquido | Nada, o burla al fallar |
| Prueba del Warden | Foco corto | El Warden delante, tres anillos | | Examina |
| Desafío final | Zoom al Altar | Seis runas, trampas, la mano del Warden | Música propia | Habla entre runas; se lo cobra o estalla en furia |

## Contribución a los pilares y a las métricas

- **Pilar 2**: los espectadores ven venir el momento en cada duelo; apuestas.
- **Pilar 3**: una mecánica; la precisión es visible al instante.
- **Pilar 4**: el desafío final es el Warden jugando contra ti, con trampas y voz.
- Métricas: P(supera) por dificultad; duración media de las pruebas; abandono del jugador por frustración (playtest).

## Criterios de aceptación

- [ ] **CA-MIN-1** (R-MIN-2). Dado un pulso con zona buena Z y una pulsación a distancia d del momento, entonces la precisión es 1 menos d dividido por Z medio, con mínimo 0; fuera de la zona, 0; sin pulsación, 0; una segunda pulsación no cambia nada.
- [ ] **CA-MIN-2** (R-MIN-3). Dada una semilla, entonces la secuencia generada dos veces es idéntica, y en un duelo ambos jugadores reciben la misma.
- [ ] **CA-MIN-3** (R-MIN-6, 8). Dado cada uso, entonces la prueba se crea con la dificultad, el número de pulsos y el criterio de la tabla, leídos de datos.
- [ ] **CA-MIN-4** (R-MIN-7). Dado Shuten-dōji en un duelo, entonces su zona buena es la base por el multiplicador; en cualquier otro uso, la base.
- [ ] **CA-MIN-5** (R-MIN-10, 11). Dado un duelo con sumas distintas, entonces gana la mayor; con sumas iguales, entonces gana el retado; con desenfunde, entonces gana la menor suma de tiempos y las anticipaciones cuentan como el máximo.
- [ ] **CA-MIN-6** (R-MIN-12). Dado un desafío final, entonces tiene seis pulsos con el cierre de normal y la zona buena propia del final, entre una y dos trampas nunca en el primero, y una sola precisión 0 lo falla.
- [ ] **CA-MIN-7** (R-MIN-13). Dado un resultado con un número incorrecto de valores, un valor fuera de 0 a 1, o recibido tras la tolerancia, entonces se trata como todo 0.
- [ ] **CA-MIN-8** (R-MIN-14, 15). Dado un decisor bot, entonces la prueba se resuelve según su habilidad por dificultad, y en N pruebas la frecuencia de superadas está dentro de más o menos 5 puntos de la habilidad.
- [ ] **CA-MIN-9** (R-MIN-18) (manual). Dado un cliente limitado a 30 FPS, cuando pulsa en el mismo instante real que otro a 60, entonces la precisión medida es la misma dentro de un margen en datos.
- [ ] **CA-MIN-10** (R-MIN-5). Dado un desenfunde, cuando el jugador pulsa antes de la señal, entonces la precisión es 0; cuando reacciona dentro del tiempo mínimo, entonces 1; entre el mínimo y el máximo, entonces decrece linealmente; tras el máximo, 0.
- [ ] **CA-MIN-11** (R-MIN-9). Dadas N pruebas de Fuente, escape y desafío final, entonces ninguna es desenfunde; dadas N pruebas de duelo y de Prueba del Warden, entonces la proporción de desenfundes está dentro de más o menos 5 puntos del peso de datos.
- [ ] Sin valores fijos en el código: todo lo de la tabla de parámetros sale de datos.

## Nombres nuevos para el glosario

Anillo (`RingPulse`), Desenfunde (`QuickDraw`), Prueba (`SkillTrial`), Pulso (`Pulse`), Precisión (`Precision`), Zona buena (`SweetZone`), Trampa del final (`FinalTrick`). Corrección de la entrada "Minijuego de habilidad": cinco usos, no tres.

## Fuera de alcance de esta fase

- Mecánicas distintas del anillo y su variante: Fase 2, como nuevas implementaciones si el playtest muestra monotonía.
- Minijuegos temáticos por Elegido (v0.1 5.1): Fase 2 o nunca.
- Mesa de práctica en el Umbral: opcional del GDD 07.
- Anti-trampa sobre el resultado enviado por el cliente: fuera de todas las fases previstas.
- En VR, el gesto en vez del botón: Fase 3, mismo componente.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| Valores de cierre, zona buena y umbrales | Playtest | Primera build | Los iniciales son punto de partida; objetivo P(supera) de la tabla |
| **Reloj de turno y Prueba del Warden** (S-015, 15/10/2026): la Prueba se juega al empezar el turno, antes de tirar, y hoy el reloj de turno (30 s) corre mientras dura (sin tiempo del minijuego aún da igual). Cuando el minijuego dure, decidir si el reloj de turno arranca después de la Prueba o se pausa mientras dura. Propuesta: arranca después (la Prueba tiene su propio tiempo) | Autor | Story del minijuego | |
| Zona buena del desafío final: ¿se supera en torno al 50 % de los intentos? | Playtest | Primera build | Decidido el 22/09: normal sin fallos, nunca un fallo permitido |
| ¿El desenfunde entra en el TFM o solo el anillo? | Autor | Sprint del minijuego | Propuesta: entra, es el mismo componente; primera candidata a recorte si hay prisa |
| Lista de trampas del final y su lectura para el jugador | Autor y arte | Sprint del minijuego | Deben ser injustas pero legibles |
| Sincronización de inicio en duelo con latencias muy distintas | Spike de red | Spike | Corrección por latencia de Unreal; medir |
