# S-041. Se entiende cuándo corta el Warden: señal de corte, frases que dan tiempo a leerse y el orbe del Oráculo a la vista

**Sistema**: la voz del Warden (R-VOZ-1, sin audio: la frase es un subtítulo), interfaz en plano (GDD 07; pilar 3, caos legible, y pilar 4, el Warden te habla a ti) y Oráculo (presentación del orbe). **Módulos**: `Soulbound` (presentación y anfitrión: `WardenLineQueue`, `BoardViewActor`, `MatchHostSubsystem`, `DecisionPanel`) y datos (`Content/Data/Warden/frases.json`). `SoulboundCore` no cambia. **Rama base**: `main`, con S-040 fusionada. **Tamaño**: mediano. **Sin assets**.

## Contexto

En la prueba del 15/11/2026 con la build de probadores, una persona que no juega a videojuegos (P4):

1. **No entendió que cuando el Warden actúa se corta el turno.** No sabía por qué unas veces jugaba y otras no. En 22 turnos, cinco se los cortó un golpe del Warden. En pantalla, un golpe es una frase más del Warden y la ficha que se para.
2. **No le dio tiempo a leer al Warden.** Cada frase dura 6 segundos, tenga 40 letras o 160. Además una frase puede sustituir a la anterior antes de que se lea: en su partida, la burla por ir última tapó en el mismo instante la frase que le daba un turno extra.
3. **No vio el orbe del Oráculo la primera vez.** Le llegó como golpe del Warden, en el instante del corte y sin frase previa, mientras intentaba mover. Estuvo abierto 7 segundos, no lo consultó y le subió la insolencia. Las tres veces siguientes, que llegaron entre turnos y con el Warden hablando antes, lo consultó en 2 o 3 segundos.

Decidido por el autor el 15/11/2026, con el criterio "que se entienda mejor, no que sea más fácil" (ninguna regla del Warden cambia: ni cuántos golpes, ni cuándo, ni el plazo del Oráculo):

- El corte de turno tiene una señal propia: el tablero se oscurece un instante, el Warden dice "¡Quietos!" antes de su frase (palabra provisional) y, debajo de la frase, una línea pequeña y neutra dice qué ha pasado. Sin cartel grande en el centro. Sin sonido: el audio es de la Fase 2.
- Cada frase dura según su longitud, con los 6 segundos de hoy como mínimo.
- Una burla nunca tapa una frase que cambia la partida. Las frases no hacen cola (dicha tarde, una frase ya no tiene sentido) y las burlas no detienen la mesa (son ambiente).
- El orbe del Oráculo, más grande y latiendo, sobre la ficha como hasta ahora. Sin texto junto al orbe.

## Reglas a implementar (`Soulbound`)

- **R-VOZ-4 (nueva). Cada frase dura según su longitud.** La duración de una frase en pantalla es el mayor de dos valores: `WardenLineSeconds` (6 s, el mínimo) y su número de caracteres, tal como se enseña (con su prefijo "Warden, a ti:"), por un tiempo por carácter (Config, nombre a proponer; por ejemplo `WardenLineSecondsPerChar`, valor inicial 0,065 s: unas quince letras por segundo). Sale de una función pura (nombre a proponer) que usan los tres sitios que hoy multiplican por `WardenLineSeconds`:
  - la cola de la línea del Warden (`FWardenLineQueue`), para saber cuándo quitar o pasar cada frase;
  - la espera de la mesa (`WardenWaitSeconds`): la mesa espera la suma de las duraciones de las frases que la detienen, no un número fijo por frase;
  - el prólogo (`OpeningSeconds`): dura la suma de las duraciones de las frases de la apertura.
  - Qué frases detienen la mesa no cambia (`LineHoldsTheTable`).
- **R-VOZ-5 (nueva). Una burla no tapa una frase que detiene la mesa.** Hoy una frase dicha más tarde sustituye a las que quedan. Desde esta story, si la frase nueva es un comentario (`comentario:*`) y la que se está enseñando, o alguna de las que esperan, es una frase que detiene la mesa, el comentario se descarta: no se enseña ni entonces ni después. En los demás casos, como hoy: una frase que detiene la mesa sustituye a lo que hubiera; un comentario sustituye a otro comentario; las del mismo instante hacen cola.
- **R-VOZ-6 (nueva). La señal de corte.** Cuando el Warden corta un turno con un golpe (evento `TurnInterrupted` con `Detail` 0), sea el turno del humano o el de otro asiento:
  1. **El tablero se oscurece un instante.** Una capa oscura sobre el tablero y por debajo de los paneles, que aparece de golpe y se desvanece. Opacidad máxima y duración en Config (nombres a proponer; valores iniciales 0,5 y 0,8 s). Sin posproceso (restricción de rendimiento de la skill).
  2. **El Warden dice la palabra de corte antes de su frase.** La frase de ese golpe se enseña precedida de la palabra de corte, en la misma línea: "¡Quietos! " y la frase. Si el golpe no trae frase (el Oráculo como golpe), la línea es la palabra de corte sola. La palabra está en `frases.json` con una clave propia (a proponer; por ejemplo `corte`), no en el código. Texto inicial, del autor: "¡Quietos!" (provisional).
  3. **Debajo de la frase, una línea pequeña y neutra** (letra menor que la de la frase, color neutro, no el del Warden) que dura lo que su frase:
     - turno del humano: "Tu turno se corta." seguido del texto del evento (`text` en `eventos.json`, lo decretado; por ejemplo "Tu turno se corta. En tu próximo turno, la Prueba del Warden");
     - turno de otro asiento: "Se corta el turno del asiento N." seguido del texto del evento;
     - el Oráculo como golpe (`EOracleOrigin::Strike`): "Tu turno se corta. El Oráculo te llama." o "Se corta el turno del asiento N. El Oráculo le llama.".
  4. **El corte por el fin de la carrera** (`Detail` 1) lleva solo la línea pequeña bajo su frase: "Tu turno se corta: la carrera ha terminado." (o "Se corta el turno del asiento N: la carrera ha terminado."). Sin palabra de corte y sin oscurecer: no es un golpe. El corte por el fin de la partida (`Detail` 2) no lleva nada.
  - Los textos de las líneas pequeñas del turno ajeno y del fin de la carrera son una concreción del tutor: el autor los confirma o los cambia al leer la story.
- **R-DEC-17 (nueva, precisión de R-DEC-9). La frase del Warden no tapa el panel de decisión.** Hoy se reservan a la frase `WardenLineReservedLines` líneas (2); una frase de tres líneas tapa la instrucción y la barra del panel (visto en PIE el 15/11/2026). Desde esta story, el panel de decisión queda siempre por encima de lo que ocupa de verdad la franja del Warden: el mayor entre lo reservado y lo que necesitan la frase y su línea pequeña. La caja del objetivo (S-040) sigue ocultándose mientras hay frase.
- **R-VIS-29 (nueva). El orbe del Oráculo se ve.** Mientras hay una consulta esperando (`State.Oracle.IsWaiting()`), el orbe sobre la ficha del consultante:
  - es más grande: su diámetro y su altura dejan de ser constantes del código y pasan a Config (valor inicial del diámetro 5 cm; hoy 3);
  - late de tamaño, entre un mínimo y su tamaño entero, con la función del pulso que ya usa la vista (`RingPulse`); periodo y mínimo en Config (valores iniciales 0,9 s y 0,8).
  - Sigue sobre la ficha y sigue recibiendo el clic. Los 7 segundos del plazo no cambian. No se añade texto junto al orbe.
- **A comprobar, no es regla nueva.** Según el código, en toda consulta del humano salen las dos cosas: el orbe sobre su ficha y el panel de decisión con "Consultar". El autor cree que en el golpe por el Oráculo solo salió el orbe. La prueba manual de abajo lo comprueba; si el panel no sale, se arregla en esta story, con un test que lo reproduzca.
- **Lo que no cambia.** Cuándo y cuánto interviene el Warden, a quién golpea, los golpes durante la carrera, el plazo del Oráculo, qué frases detienen la mesa y las instrucciones (`instruccion:*`).

## Criterios de aceptación

Automáticos, sin mundo. Los números de los criterios de la voz (CA-VOZ) se proponen en el plan, siguiendo a los que ya existan.

- **CA-VOZ (duración, R-VOZ-4).** Con 6 s de mínimo y 0,065 s por carácter: una frase de 40 caracteres dura 6 s; una de 150, 9,75 s. Con el tiempo por carácter a cero, todas duran el mínimo.
- **CA-VOZ (la cola, R-VOZ-4).** Dos frases del mismo instante, una corta y una larga, se enseñan seguidas, cada una su duración; `ShownCount` sigue contando las que han empezado a enseñarse.
- **CA-VOZ (espera y prólogo, R-VOZ-4).** La espera de la mesa por dos frases que la detienen es la suma de sus dos duraciones; un comentario entre ellas no suma. El prólogo dura la suma de las duraciones de las frases del instante cero.
- **CA-VOZ (la burla no tapa, R-VOZ-5).** Con una frase que detiene la mesa enseñándose, un comentario dicho después se descarta y la frase sigue hasta cumplir su tiempo. Dicho cuando la frase ya ha terminado, el comentario se enseña. Un comentario sustituye a otro comentario. Una frase que detiene la mesa sustituye a un comentario.
- **CA-VOZ (textos de la señal, R-VOZ-6).** La función que da la línea y la línea pequeña de un corte devuelve, letra por letra, los textos de la regla en estos casos: golpe al humano con frase y con texto de evento; golpe a otro asiento; el Oráculo como golpe, sin frase (la línea es la palabra de corte sola); fin de la carrera (sin palabra de corte); y sin corte, la frase tal cual y ninguna línea pequeña. La palabra de corte sale de la tabla de frases, no de un literal del código.
- **CA-VOZ (oscurecido, R-VOZ-6).** La función que da la opacidad de la capa en el tiempo vale el máximo en el instante del corte, baja hasta cero al cumplirse la duración y es cero antes y después. Un corte nuevo mientras dura la reinicia.
- **CA-DEC-33** (R-DEC-17). La función que da el alto de la franja del Warden devuelve lo reservado si la frase y su línea pequeña caben en él, y lo que necesitan si no caben; sin frase, lo reservado.
- **CA-VIS-30** (R-VIS-29). La función que da la escala del orbe devuelve el mínimo al empezar el periodo y 1 a mitad de periodo; con periodo cero o negativo, 1 fijo.

Manuales, por el autor, en PIE, a 1920x1080 y a 1024x768, con captura:

- **Golpe en su turno** (con `SbStrike` y un golpe del acto I, en su turno de tirada): el tablero se oscurece un instante, la línea dice "¡Quietos!" y la frase, y debajo "Tu turno se corta." con lo decretado. Se entiende de un vistazo que ha sido el Warden y qué ha hecho.
- **Golpe en el turno de un dummy**: lo mismo, con "Se corta el turno del asiento N.".
- **El Oráculo como golpe** (con `SbStrike` y `el-orbe-llama`, en su turno): la señal de corte con "Tu turno se corta. El Oráculo te llama.", el orbe grande y latiendo sobre su ficha **y** el panel de decisión con "Consultar". Si el panel no sale, es un fallo a arreglar aquí.
- **El orbe** se ve desde la vista sentada y desde la cenital sin tapar la ficha, y el clic en él consulta.
- **Frase larga**: la frase de apertura da tiempo a leerse entera. Una frase de tres líneas a 1024x768 no tapa el panel de decisión.
- **La burla no tapa**: tras lanzar un evento (`SbEvent`), una burla dicha mientras dura su frase (con `SbSay` y la clave de un comentario de `frases.json`) no la sustituye.
- **Fin de la carrera en su turno**: la línea pequeña bajo la frase del fin de la carrera.
- La partida no se hace notablemente más lenta: el prólogo y las esperas duran lo que se tarda en leer.

## Notas para quien lo implemente

- **La cola.** `FWardenLineQueue` (`Presentation/WardenLineQueue.h`) guarda textos y los enseña `LineSeconds` cada uno; `Push` descarta lo pendiente si la frase nueva es de un instante posterior. Hace falta que cada entrada lleve su duración, si detiene la mesa (para R-VOZ-5) y su línea pequeña (para R-VOZ-6). Lo que cambie de su interfaz se propone en el plan; sus tests se actualizan, no se borran.
- **Quién usa la duración.** `WardenWaitSeconds` y `OpeningSeconds` (mismo archivo) y sus llamadas en `MatchHostSubsystem` y en la vista; el acta escribe "El Warden habla: la mesa espera N s", que pasa a decir la suma real.
- **La frase y su corte.** `ABoardViewActor::RefreshWardenLine` recorre el registro y hace `Push` de cada `WardenSpoke`. El corte está en el registro como `TurnInterrupted` (asiento cortado, `Detail` 0, 1 o 2) junto a `WardenDecided` (el evento) y, si es el Oráculo, `OracleOpened` con origen `Strike`. Cómo se empareja cada corte con su frase se propone en el plan; el caso sin frase (el Oráculo como golpe) tiene que enseñar igualmente la palabra de corte y la línea pequeña.
- **La palabra de corte** va en `frases.json` como una frase más, con su clave. Es un texto del Warden: propio y original (R-VOZ-2).
- **La franja.** `WardenLineReservedHeight()` (S-033, R-DEC-9) y la colocación del panel de decisión y de la caja del objetivo (S-040) dependen de ese alto.
- **La capa oscura** es Slate, en la capa del tablero, por debajo de todos los paneles y sin recibir clics.
- **El orbe.** `ABoardViewActor::RefreshOracleOrb`; hoy su tamaño y su altura son constantes (`OracleOrbDiameter` 3, `OracleOrbHeight` 7). El latido necesita que el actor tique mientras hay consulta, como hace con los halos (S-039).
- **El panel del Oráculo.** `DecisionView` (`Presentation/DecisionText.cpp`) devuelve la vista del Oráculo solo si antes no hay una elección pendiente del humano ni está en su turno de tirada: si el golpe por el Oráculo deja el estado en alguna de esas dos situaciones, ahí estaría el fallo que cree haber visto el autor. Se mira antes de la prueba manual y se dice en el plan.
- Tiempos, tamaños y colores nuevos, en Config con su valor inicial documentado. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- El sonido del corte y cualquier audio (Fase 2).
- Cambiar cuántos golpes hay, a quién caen o quitarlos de la carrera (decidido por el autor: se quedan).
- El recordatorio de la Prueba del Warden, el título del anillo y el botón "Empezar" de las pruebas (story siguiente).
- Cambiar el plazo del Oráculo o añadir texto junto al orbe.
- Que las frases hagan cola o que las burlas detengan la mesa (descartado por el autor).
- Ayuda consultable durante la partida y el "Cómo se juega".
