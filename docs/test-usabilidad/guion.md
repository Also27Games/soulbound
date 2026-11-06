# Test de usabilidad de Soulbound (Fase 1): guion

**Qué se prueba**: la build empaquetada del TFM (Development, `.\Scripts\build.ps1 -Que package`, un solo zip con fecha para todos), con un humano y tres jugadores automáticos (dummies: corren al Nicho libre más cercano, luego hacia el Altar, y eligen al azar lo demás; el bot básico del diseño es Fase 2). **Cuándo**: del 9 al 15 de noviembre de 2026. **Quién**: cuatro probadores, P1 a P4, que en este repositorio nunca llevan nombre. P1 a P3 juegan en su propio PC, lejos del autor, en una videollamada con pantalla compartida (sesión remota). P4 juega al lado del autor (sesión presencial). P1 a P3 tienen costumbre de videojuegos; P4 juega a juegos de mesa y no a videojuegos.

**Qué queremos saber** (en este orden; lo que se aprende, no lo que se mide):

1. Si alguien que no ha visto nunca el juego entiende qué tiene que hacer en su turno **solo con lo que el Warden dice** y la pantalla "Cómo se juega" (decisión de diseño del 22/10/2026: el Warden enseña a jugar, no hay tutorial).
2. Si el ritmo aguanta: prólogo, espera del Warden entre frases, turnos de los bots, cuarenta minutos de tope.
3. Dónde se atasca: qué tecla, qué pantalla, qué frase.
4. Si el Warden hace gracia o molesta, y si el jugador entiende que las burlas van con el personaje.

**Qué no se prueba**: el arte (placeholder declarado), el sonido (no hay), el online (fuera de la Fase 1), el equilibrio de la victoria (se ajusta después con lo que salga).

Tres partes: la **hoja del probador** (se la mandas tal cual), el **protocolo de sesión** (lo sigues tú) y el **cuestionario con la plantilla del informe**.

---

## 1. Hoja del probador

> Se envía como mensaje o documento aparte, con el enlace de descarga. No se le mandan las tareas ni las preguntas: se las haces tú en la sesión.

### Gracias por probar Soulbound

Soulbound es un juego de mesa digital para cuatro, con un anfitrión que no está de tu lado: el Warden. Vas a jugar tú contra tres jugadores automáticos. Es una versión en desarrollo de un trabajo de fin de máster: **el arte es provisional, no hay sonido y verás textos de depuración en pantalla**. Nada de eso es lo que se prueba. Lo que se prueba es si el juego se entiende y si engancha.

No hay respuestas buenas ni malas. Si algo no se entiende, el fallo es del juego, no tuyo, y es justo lo que buscamos.

**Lo que necesitas**

- Windows 10 u 11 de 64 bits, tarjeta gráfica con DirectX 12, 8 GB de memoria. Ratón y teclado. No hace falta Steam ni ninguna cuenta.
- El zip pesa unos 411 MB; ten 1 GB libre en disco.
- Una hora, sin interrupciones si puede ser.
- Para la sesión: una videollamada en la que compartas pantalla (la que prefieras). Si no puede ser, graba la partida con la barra de juegos de Windows: **Win+Alt+R** empieza y para la grabación, y el vídeo queda en `Vídeos\Capturas`.

**Antes de la sesión**

1. Descarga el zip del enlace de Google Drive. Drive avisará de que no puede analizar un archivo tan grande en busca de virus: pulsa **Descargar de todos modos**. Descomprímelo en una carpeta tuya, por ejemplo `C:\Juegos\Soulbound`. No lo descomprimas dentro de `Archivos de programa`.
2. Abre `Soulbound.exe`. Windows puede avisar con "Windows protegió su PC": pulsa **Más información** y luego **Ejecutar de todas formas**. El aviso sale porque el programa no está firmado, es normal en una versión de desarrollo.
3. Si el juego pide instalar algo de Microsoft Visual C++, acepta: es un componente de Windows que el juego necesita.
4. Si llega al menú principal, cierra el juego con **Salir**. Ya está. **No empieces ninguna partida hasta la sesión**: queremos ver tu primera vez.

**Si algo falla al abrir**

- Pantalla negra más de un minuto, o el juego se cierra solo: anótalo y dímelo. No hace falta que lo arregles.
- Si se ve muy oscuro o borroso en tu pantalla, puedes tocar **Configuración › Vídeo** durante la sesión; dime qué cambiaste.

**Al acabar la sesión**

Mándame dos cosas:

1. La carpeta `Soulbound\Saved\Logs` que está dentro de la carpeta del juego (contiene el registro de la partida, sin ningún dato tuyo).
2. La grabación, si has grabado tú.

Y el cuestionario, que te paso al terminar. Cinco minutos.

---

## 2. Protocolo de sesión

> Lo sigue el autor. Sesión remota: videollamada con pantalla compartida, grabada con permiso del probador. Sesión presencial (P4): el autor al lado, sin tocar el ratón, con esta misma hoja impresa. En las dos, el autor **no ayuda** salvo en los puntos marcados; si el probador pregunta, se le devuelve la pregunta: "¿Qué crees tú que hay que hacer?". Se anota la pregunta.

Duración prevista: 50 a 60 minutos. Antes de empezar, comprueba la grabación y ten a mano la hoja de incidencias (al final de esta sección).

### 2.1 Briefing (5 min)

Lo que se dice, con estas palabras o parecidas:

- "Vas a jugar una partida contra tres jugadores automáticos. Yo no te voy a explicar nada: el juego tiene que explicarse solo. Si no entiendes algo, dilo en voz alta y sigue probando; eso es lo que más nos sirve."
- "Piensa en voz alta todo el rato: qué crees que pasa, qué vas a hacer, qué esperabas que pasara."
- "El arte es provisional. La pantalla de carga tiene un personaje que luego no aparece en el tablero: es intencionado por ahora."
- "Verás texto pequeño por la pantalla. Es de depuración. Úsalo si te sirve, ignóralo si no."
- "Las pruebas de habilidad no están hechas todavía. Cuando haya un duelo, una prueba o el desafío final, el juego lo resuelve solo, al azar, y lo dice en pantalla: no tienes que pulsar nada. El trilero sí es tuyo: ahí eliges tú."
- "Puedes parar cuando quieras. Si a los veinte minutos de partida quieres dejarlo, lo dejamos."

No se dice: qué teclas hay, qué es el Warden, cómo se gana. Eso lo tiene que contar el juego.

### 2.2 Tareas

Cada tarea tiene un objetivo, lo que debería pasar, qué se observa y cuándo se interviene. Se anota el tiempo de cada una.

**T1. Abrir el juego y leer "Cómo se juega"** (objetivo: 3 min)

- Qué se pide: "Abre el juego y empieza una partida nueva."
- Debería pasar: menú principal, Jugar, pantalla "Cómo se juega" con el texto y la leyenda del tablero, botón Jugar.
- Se observa: si lee el texto entero, por encima o nada; si mira la leyenda; cuánto tarda; si dice algo. Pregunta al pulsar Jugar, antes de que cargue: "¿Me dices con tus palabras cómo se gana?" (anotar literal).
- Intervención: ninguna.

**T2. El prólogo** (objetivo: 1 min)

- Debería pasar: pantalla de carga, medio segundo de tablero, y las frases del Warden una tras otra; nadie se mueve hasta que acaba.
- Se observa: si entiende que tiene que esperar o intenta hacer clic; si lee las frases; si pregunta quién habla. Si intenta hacer clic, no pasa nada: anotarlo, es un dato.
- Intervención: ninguna.

**T3. Dos turnos propios** (objetivo: 6 min)

- Debería pasar: cuando le toca, el Warden se lo dice; tira el dado (el juego lo tira solo), las casillas alcanzables se iluminan, hace clic en una, la ficha anda. En algún turno le pedirá una elección por número.
- Se observa: cuánto tarda en descubrir que hay que hacer clic en una casilla iluminada; si se le agota el reloj de turno (30 s) y entiende el castigo; si entiende a qué va (Nicho, Fuente); si distingue su ficha; si usa C, L o H por su cuenta.
- Intervención: solo si se le agotan **dos** relojes de turno seguidos sin haberse movido: "Fíjate en lo que el Warden te ha pedido." Nada más. Anotarlo como intervención.

**T4. Responder al Warden** (objetivo: cuando ocurra; antes de los 15 min de partida)

- Debería pasar: una petición del Warden (un dilema con opciones numeradas, una votación, el Oráculo). Las opciones se responden con las teclas numéricas (0 es "no"); en una votación, con un clic en la ficha a la que se vota; el Oráculo, con un clic en el orbe que aparece sobre la ficha. Si no responde a tiempo, sube la insolencia; a la quinta, el Vacío.
- Se observa: si entiende que le están pidiendo algo y a quién; cuánto tarda en encontrar cómo responder; si lee las opciones; su reacción a la burla.
- Intervención: ninguna. Si sube la insolencia, se anota qué hizo o dejó de hacer.

**T5. Pausa y salida** (objetivo: 1 min, en cualquier momento después de T3)

- Qué se pide: "Pon el juego en pausa y vuelve a la partida." Y al final de la sesión: "Sal de la partida como te parezca."
- Debería pasar: Esc abre el menú de pausa (la partida **no se detiene**: decisión de diseño); Reanudar vuelve. Con la partida terminada, un botón "Volver al menú" o Intro.
- Se observa: si encuentra Esc; si entiende que la partida sigue mientras está en pausa; si al salir sabe dónde está.
- Intervención: si no encuentra la pausa en un minuto, decírselo y anotarlo.

**Juego libre** (hasta 20 min o hasta que lo deje). Después de T5 se le deja jugar. Se anota: cuándo deja de pensar en voz alta (señal de que está dentro), cuándo mira el reloj, cuándo se aburre, qué dice del Warden. Si gana alguien o se acaba el tiempo, se observa la pantalla de fin y la vuelta al menú (T5).

### 2.3 Cierre (10 min)

Tres preguntas en voz alta, antes del cuestionario (se anotan literales):

1. "Cuéntame la partida como si se la contaras a un amigo." (Qué recuerda: si cuenta el tablero, al Warden, a los otros jugadores.)
2. "¿Qué es lo que peor has entendido?"
3. "La pantalla de carga tiene un personaje y una atmósfera. ¿Qué esperabas encontrar al empezar y qué te encontraste?" (Expectativas creadas por el arte provisional frente al tablero de depuración.)

Después, el cuestionario (sección 3). Luego, pedirle el log y la grabación y darle las gracias.

### 2.4 Hoja de incidencias

Una línea por incidencia, en orden, durante la sesión. Se rellena en `docs-privado` (datos de la sesión), nunca aquí.

| Minuto | Tarea | Qué pasó | Gravedad | Qué dijo el probador |
|---|---|---|---|---|
| | | | | |

Gravedad: **bloqueo** (no puede seguir sin ayuda), **tropiezo** (tarda o se equivoca, pero sale solo), **comentario** (lo dice, no le frena).

---

## 3. Cuestionario y plantilla de informe

### 3.1 Cuestionario (al acabar la sesión, 5 min)

Se pasa en un formulario en línea o en papel. Las ocho primeras preguntas se responden de 1 (nada de acuerdo) a 5 (totalmente de acuerdo).

1. He entendido qué tenía que hacer en mi turno sin que nadie me lo explicara.
2. He entendido cómo se gana.
3. Sabía cuándo me tocaba a mí.
4. Cuando el Warden me pedía algo, sabía cómo responderle.
5. El ritmo de la partida me ha parecido bueno (ni lento ni atropellado).
6. El Warden me ha hecho gracia.
7. He entendido que las burlas del Warden son del personaje, no del juego contra mí.
8. Jugaría otra partida.

Tres abiertas:

9. ¿Qué cambiarías primero?
10. ¿Hubo algún momento en que no supieras qué hacer? ¿Cuál?
11. ¿Qué te esperabas del juego por la pantalla de carga y el menú, y qué te encontraste?

### 3.2 Qué cuenta como problema

Para que el informe no dependa del humor del día, se fija antes de empezar. Los umbrales están escritos para cuatro probadores; si al cerrar han jugado menos, se leen en proporción y el informe lo dice:

- Un **bloqueo** en una tarea para dos o más probadores: problema que se arregla antes de la entrega.
- Un **tropiezo** en la misma tarea para tres o más: se arregla si cabe en los dos primeros días de la semana de cierre (16 y 17 de noviembre); si no, va al informe como pendiente para la Fase 2.
- Pregunta 1, 3 o 4 con media por debajo de 3: el Warden no enseña a jugar lo bastante; se revisan las frases de instrucción y la pantalla "Cómo se juega".
- Pregunta 7 por debajo de 3 en algún probador: se revisa el registro de voz del Warden con ese probador delante (qué frase).
- Reloj de turno agotado en T3 por más de un probador: la petición en pantalla no se ve; se revisa antes que nada.

### 3.3 Plantilla del informe

El informe va en `docs/test-usabilidad/informe.md`. Se cierra el domingo 15 de noviembre con los probadores que hayan jugado hasta entonces, cabe en una página y no lleva nombres:

1. **Qué se probó**: build (fecha del zip, commit), fechas y cuántos probadores, con una línea de perfil cada uno (costumbre de videojuegos y de juegos de mesa; nada más).
2. **Qué se entendió y qué no**, tarea por tarea, en una frase cada una, con los bloqueos y tropiezos que la sostienen y las medias del cuestionario que vengan al caso.
3. **Lo que se cambia** antes de la entrega, con la story o el commit que lo arregla. **Lo que se deja** para la Fase 2 y por qué.
4. **Lo que el test no pudo decir**: con tan pocos probadores no hay cifras; qué conviene volver a probar en la Fase 2.

---

## Notas para el autor

- **Antes de la primera sesión**: probar el protocolo entero contigo mismo con el zip definitivo, cronómetro en mano, y borrar `Saved` antes de cada sesión tuya. El perfil del jugador (instrucciones vistas) se guarda en la máquina de cada probador: en la suya empieza vacío, como tiene que ser.
- **La primera sesión es la que más enseña** sobre el guion. Si una tarea no funciona, se corrige el guion para las siguientes y se anota en el informe que P1 jugó con la versión anterior.
- **Limitación conocida, no hallazgo**: el minijuego de habilidad no existe en esta build y las cinco pruebas se resuelven al azar (la pantalla lo dice). Lo que comenten de eso se anota aparte y no cuenta para los umbrales; sí cuenta si no entienden qué ha pasado o creen que el juego se ha colgado.
- **Sesión presencial (P4)**: la tentación de ayudar es mayor. Misma regla: devolver la pregunta y anotar.
- **Lo que se guarda y dónde**: logs, grabaciones, incidencias y respuestas, en `docs-privado`, fuera del repositorio público. Aquí solo el guion y el informe anonimizado.
