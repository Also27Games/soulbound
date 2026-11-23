# Test de usabilidad de Soulbound (Fase 1): informe

Noviembre de 2026. El guion con el que se preparó está en [`guion.md`](guion.md). Los probadores aparecen como P1, P2 y P4, sin nombres; sus datos no están en este repositorio.

## 1. Qué se probó

Tres personas que no conocían el juego lo probaron en dos rondas. Siempre un jugador contra tres jugadores automáticos.

| Quién | A qué juega | Primera ronda (8 a 15/11) | Segunda ronda (20 a 22/11) |
|---|---|---|---|
| P1 | Juegos de mesa y de PC | Diez partidas por su cuenta; cuestionario | Tres partidas por su cuenta |
| P2 | Videojuegos | No jugó | Dos partidas por su cuenta; cuestionario |
| P4 | Juegos de mesa; no videojuegos | Una partida, con el autor al lado | Una partida, con el autor al lado |

La segunda ronda se jugó con la build `v0.3`, que llevaba todo lo arreglado tras la primera. La build que se entrega es la `v0.3.1`: la `v0.3` más un botón, añadido por lo que salió en la segunda ronda. Esa última no la ha probado nadie.

El guion preveía cuatro probadores, P1 a P4, y cuatro sesiones con el autor delante. P3 no llegó a jugar, y solo las dos sesiones de P4 fueron con el autor delante. Los demás jugaron por su cuenta, y los dos cuestionarios se respondieron después de varias partidas, no tras la primera. Las cifras de este informe salen del acta que el juego escribe de cada partida.

## 2. Primera ronda (8 a 15 de noviembre): qué se entendió y qué no

Jugaron P1 y P4, con la primera versión del juego.

**Lo que funcionó**: el Warden y las ganas de repetir. P1 puso un 5 de 5 a "el Warden me ha hecho gracia" y a "jugaría otra partida", y jugó diez.

**Lo que no funcionó**: aprender a jugar. Los dos dijeron, cada uno con sus palabras, que no sabían qué hacer ni qué estaba pasando. En concreto:

- **Las reglas no se podían leer bien.** A P2 el texto le salía cortado en su pantalla, y por eso no llegó a jugar. P4 lo leyó y lo encontró desordenado.
- **No se sabía cómo moverse.** P1 necesitó tres o cuatro partidas para entender lo básico. P4 pulsaba la casilla a la que quería ir, no una de las iluminadas: de 27 clics, 11 fueron a casillas a las que no podía llegar, y en 10 de sus 22 turnos no movió la ficha.
- **No se sabía cómo responder al Warden.** P1 no supo votar. P4 no vio, la primera vez, el orbe que había que pulsar.
- **No se entendía qué hacía el Warden.** P4 no se dio cuenta de que le había cortado el turno ni de que le había impuesto una prueba. P1 y P4 dijeron que habla demasiado deprisa.

El guion del test fijaba de antemano qué contaba como problema: si dos o más probadores se atascan en lo mismo, se arregla antes de entregar. Era el caso.

## 3. Lo que se cambió entre las dos rondas

No era un problema de reglas, sino de presentación. No se cambió ninguna regla ni se bajó la dificultad.

| Lo que no se entendía | Qué se hizo | Story |
|---|---|---|
| Qué hacer en el turno | Las casillas alcanzables laten, la tirada se enseña con las caras del dado y un clic imposible dice por qué | S-039 |
| Adónde ir | Una línea dice la meta de cada momento; los Dominios Olvidados se reconocen por el suelo; cada Dominio lleva su nombre | S-040, S-047 |
| Cómo responder al Warden | Cada decisión, en un panel con botones; se ve a quién espera la mesa | S-028, S-033 |
| Qué ha pasado cuando el Warden corta | Señal de corte con sonido, frases que duran lo que se tarda en leerlas, el orbe a la vista, el turno perdido y el turno extra anunciados | S-041, S-046, S-050 |
| Las pruebas de habilidad | El jugador las juega con el anillo; cada una avisa de lo que te juegas y espera a "Empezar" | S-026, S-031, S-042 |
| Las reglas | Un libro, una página por tema, con su lámina; el mismo en el menú y en la partida | S-048, S-049 |
| Demasiado en pantalla | Pantalla limpia; lo demás, en el Grimorio | S-044, S-045 |
| Textos cortados a baja resolución | Paneles que caben a 1024x768 | S-027, S-035 |

## 4. Segunda ronda (20 a 22 de noviembre): si los cambios funcionaron

Jugaron P4 y P1, que repetían, y P2, que jugaba por primera vez. Todos con la build `v0.3`, que lleva los cambios de la tabla anterior.

**P4** dijo que la mejora es muy significativa, que el juego se entiende mucho mejor y que va a buena velocidad. Las actas de sus dos partidas dicen lo mismo:

| P4 | Primera ronda (15/11) | Segunda ronda (20/11) |
|---|---|---|
| Duración | 20:52, cortada | 40:00, entera |
| Clics para moverse a una casilla imposible | 11 de 27 | 1 de 25 |
| Turnos en que movió la ficha | 12 de 22 | 21 de 30 |
| Carrera por el vínculo | No se vinculó | Vinculada en 1:49 |
| Runas al acabar | 0 | 6 |
| Oráculo consultado a tiempo | 3 de 4 | 3 de 3 |

Los nueve turnos sin mover de la segunda ronda son cortes del Warden, que forman parte del juego. Era su segunda partida, y parte de la mejora es que ya había jugado: con una persona no se puede separar lo uno de lo otro.

**P1** jugó tres partidas y escribió que ahora el juego se entiende desde la primera, "y no porque ya hubiera jugado a la otra, sino porque está todo mucho más claro". En esa primera partida se vinculó a los 27 segundos y solo 1 de sus 25 clics fue a una casilla imposible; en la primera ronda había necesitado tres o cuatro partidas para lo básico. En la tercera ganó: es la primera vez que un jugador vence al Warden. En la primera ronda, las diez partidas las había ganado el Warden.

**P2** no había jugado nunca. No vio que el libro de reglas se abría y jugó la primera partida sin haberlo leído: no se vinculó a ningún Elegido y 9 de sus 37 clics fueron a casillas imposibles, 7 de ellos por intentar entrar como Eco en el Dominio de un Elegido. Leídas las reglas, en la segunda se vinculó en minuto y medio, solo 1 de 26 clics fue imposible y reunió las seis runas distintas; el Warden le cortó antes de llegar al desafío final. Su cuestionario, tras esas dos partidas: entender el turno, 5; cómo se gana, 5; saber cuándo te toca, 4; responder al Warden, 5; ritmo, 3; "el Warden me ha hecho gracia", 5; entender que las burlas son del personaje, 5; "jugaría otra partida", 5.

**Lo que salió de esta ronda y se arregló.** Ni P1 ni P2 vieron que el libro de reglas del menú se abría con un clic en la portada. P1 entró tres veces en partida sin haberlo leído y P2 jugó así su primera partida entera. Según los umbrales fijados antes de empezar (un bloqueo en la misma tarea para dos probadores) había que arreglarlo antes de entregar, y se arregló en la `v0.3.1`: junto a "Jugar" hay ahora un botón "Cómo jugar", que es el que está resaltado al llegar. Ese cambio no se ha vuelto a probar con nadie.

## 5. Lo que se deja

Lo que salió en las pruebas y no entra en la entrega:

- **Más variedad de pruebas**: hoy todas son el anillo, y P4 avisa de que se hará monótono. Es diseño de la Fase 2.
- **La imagen borrosa en algunos equipos**: a P2 el juego le dibujaba al 59 % de la resolución y lo ampliaba, y no podía leer cuántas runas quedan en cada Fuente. Su equipo iba a 60 fotogramas por segundo; no era falta de potencia.
- **El Grimorio nombra los Dominios como D0, D1…** en vez de con su nombre, y la letra del libro es pequeña (P2).
- **La pausa**: Esc no detiene la partida, a propósito, porque el juego está pensado para jugarse en línea. Contra jugadores automáticos debería poder pararse; a P1 le subió la insolencia por levantarse.
- **La crónica**: va deprisa y pierde líneas; P1 quiso usarla para saber qué llevaba cada rival y no pudo.
- **El equilibrio**: no se probaba en este test. Queda apuntado lo que salió: el Nodo del Oráculo no se sabe para qué sirve, la casilla de Vuelo Planar rara vez compensa, y el Warden cortó 12 de los 32 turnos de P1 en una partida.
- **Detalles** vistos en la segunda ronda: al ganar un duelo, si el vencido lleva muchas runas, no da tiempo a elegir una; la prueba de la Fuente de un Dominio Olvidado es demasiado difícil (P4 ganó cuatro duelos de cinco y no superó ninguna de siete); una página del libro necesita desplazamiento y a otra le falta aire.
- **El primer arranque a baja resolución**: en cuatro de los equipos vistos el juego arrancó la primera vez a 1024x768. Se corrige en el menú de vídeo y está avisado en el README y en el `LEEME.txt` de la build.
- **Rendimiento**: en los equipos en que se probó, todos con gráfica dedicada (de una MX550 a una RTX 3060), el juego fue de 46 a 60 fotogramas por segundo. No se ha probado en condiciones en un equipo que solo tenga gráfica integrada.

## 6. Lo que el test no pudo decir

- **No hay estadística.** Son tres personas, dos respondieron el cuestionario en momentos distintos y solo una fue observada. Lo que hay son coincidencias, personas que dijeron lo mismo por separado, y las actas de sus partidas.
- **La primera partida de alguien que no conozca el juego, con las reglas leídas.** P1 y P4 ya habían jugado la versión anterior. P2 llegaba de nuevas, pero jugó la primera sin reglas porque no encontró el libro. Lo que sí se sabe es que, leídas, las entendió.
- **La partida entre personas.** Todo se probó contra jugadores automáticos. Los pilares del diseño hablan de una mesa de cuatro; eso se prueba cuando exista la partida en línea.
- **Si engancha a la larga.** Diez partidas de un probador es buena señal, pero es un solo probador.
