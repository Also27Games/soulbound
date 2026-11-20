# S-042. Las pruebas avisan y empiezan cuando estás listo: recordatorio de la Prueba del Warden, lo que te juegas en el título y botón "Empezar"

**Sistema**: minijuego del anillo (GDD 09; S-026, S-031) e interfaz en plano (GDD 07; pilar 3, caos legible). **Módulos**: `Soulbound` (anfitrión y presentación: `MatchHostSubsystem`, `RingPanelModel`, `RingPanel`, `BoardViewActor`, `StatusText`). `SoulboundCore` no cambia salvo que el plan demuestre que hace falta, y entonces se pregunta antes. **Rama base**: `main`, con S-041 fusionada. **Tamaño**: mediano. **Sin assets**.

## Contexto

En la prueba del 15/11/2026 con la build de probadores, una persona que no juega a videojuegos (P4) recibió tres veces el evento que impone la Prueba del Warden:

1. **No supo que le habían puesto la Prueba.** El Warden lo dijo en una frase y, un turno después, se le abrió un anillo sin saber por qué.
2. **Perdió siempre el primer pulso entendiendo qué pasaba.** El anillo se anuncia 2,5 segundos y arranca solo. En las dos primeras pruebas el primer pulso venció sin pulsar; en la tercera no pulsó ninguno.
3. **No sabía qué se jugaba.** El título dice "PRUEBA DEL WARDEN" y nada más. Su reacción al fallar fue "pues vale".

La S-041 ya explica el momento del golpe ("Tu turno se corta. En tu próximo turno, la Prueba del Warden"). Esta story cubre lo que pasa después.

Decidido por el autor el 15/11/2026, con el criterio "que se entienda mejor, no que sea más fácil" (la dificultad del anillo no se toca: ni pulsos, ni cierre, ni zona):

- Mientras espera su turno con la Prueba pendiente, un recordatorio fijo: "Te espera la Prueba del Warden".
- El título del anillo dice qué se juega: "Prueba del Warden: si fallas, pierdes el turno".
- Idea del autor: un botón "Empezar" en todos los anillos. Si no se pulsa en unos segundos, la prueba empieza sola, con la cuenta a la vista y sin castigo. Descartado: dar la prueba por fallida si no se pulsa.

## Reglas a implementar (`Soulbound`)

- **R-MIN-19 (nueva). El anillo espera a que el jugador esté listo.** Al abrirse un anillo del humano (Fuente de un Dominio Olvidado, Prueba del Warden, escape del Vacío, duelo y desafío final), los pulsos no empiezan todavía. El panel enseña el título, la instrucción de siempre, un botón **"Empezar"** con su tecla y una cuenta atrás.
  - La prueba empieza cuando el jugador pulsa "Empezar" (con el ratón o con la tecla) o cuando la cuenta llega a cero, lo que pase antes. La espera máxima es un parámetro de juego en datos, con el resto de parámetros del anillo (nombre a proponer; por ejemplo `startWaitSeconds`; valor inicial 8 s). Sustituye al anuncio fijo de hoy (`RingAnnounceSeconds`, 2,5 s), que desaparece.
  - Desde que empieza hasta que el primer pulso comienza a cerrarse pasa un respiro corto (Config, nombre a proponer; valor inicial 0,5 s), para que la pulsación de "Empezar" no se encadene con la del primer pulso.
  - **La pulsación de "Empezar" no cuenta como pulsación del anillo**, ni a favor ni en contra. Una pulsación del anillo hecha durante la espera, fuera del botón y de su tecla, tampoco cuenta ni penaliza.
  - Empezar sola no tiene castigo: no sube la insolencia ni cambia el resultado. A partir de ahí la prueba es la de siempre.
  - La cuenta atrás se ve (un número o una barra, a proponer en el plan) y dice que la prueba empezará sola: "Empieza sola en N s".
  - **Corrección del autor del 16/11/2026 tras PIE: dónde va el botón.** El botón "Empezar", "o pulsa Intro" y la cuenta tapaban la runa y la franja verde justo mientras se lee "haz clic cuando el aro toque la runa". Van debajo del aro, entre el círculo y la barra de progreso, y el centro queda libre: la runa y la franja verde se ven durante la espera. Tiene que seguir cabiendo a 1024x768 con un título de dos líneas.
  - **Corrección del autor del 16/11/2026 tras PIE: el acta.** En una prueba salió "Anillo: empieza por el botón tras 8.0 s de espera", con la espera en el máximo. Si la pulsación de "Empezar" llega cuando la cuenta ya ha llegado a cero y aún no se ha notado, la prueba empezó sola y el acta dice "sola".
  - En el duelo de hoy solo juega el anillo el humano (los dummies resuelven al azar), así que vale la misma regla. Que en un duelo entre dos humanos empiece cuando los dos hayan pulsado es de la Fase 2 (partida en línea) y se deja anotado, no implementado.
  - El acta deja constancia: una línea al empezar que dice si empezó por el botón o por la cuenta, y los segundos que esperó.
- **R-VIS-30 (nueva, precisión de R-VIS-22). El título dice qué te juegas.** Bajo el nombre de la prueba, o en la misma línea si cabe, una frase corta con lo que pasa si fallas. La de la Prueba del Warden es del autor: **"Prueba del Warden: si fallas, pierdes el turno"**. Las de las otras cuatro (Fuente de un Dominio Olvidado, escape del Vacío, duelo y desafío final) las propone el agente en el plan, sacadas de lo que hacen las reglas al fallar o perder, con la regla que lo dice; el autor las aprueba o las cambia antes de implementarlas. Ninguna inventa una consecuencia que el motor no aplique.
- **R-HUD-9 (nueva). El recordatorio de la Prueba del Warden.** Mientras el humano tiene la Prueba del Warden pendiente (`FSeatState::bWardenTrialPending`), su caja del objetivo (S-040, abajo a la izquierda) lleva una segunda línea, con el color de la línea del Warden: **"Te espera la Prueba del Warden"**. Sale desde que se le impone hasta que la juega. Sigue las condiciones de la caja: no se ve en el prólogo ni mientras hay una frase del Warden en pantalla, y ninguna tecla la oculta.
  - Que vaya en la caja del objetivo es una concreción de la story (el autor pidió "un recordatorio fijo en su panel"): el autor lo confirma o lo cambia al leer la story.
- **Lo que no cambia.** La dificultad de las pruebas (pulsos, tiempo de cierre, zona buena, marca), cómo se puntúa, qué pasa al superarlas o fallarlas, que los dummies las resuelvan al azar y el resultado que se enseña al acabar.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-MIN-14** (R-MIN-19, empezar por el botón). Con un anillo abierto y sin empezar, ningún pulso se cierra ni vence aunque pase el tiempo, hasta la espera máxima. Al pulsar "Empezar" en el segundo 2, el primer pulso comienza a cerrarse en el segundo 2 más el respiro, y la secuencia de pulsos es la misma que habría tenido sin espera (misma semilla, mismos pulsos).
- **CA-MIN-15** (R-MIN-19, empezar sola). Sin pulsar, al cumplirse la espera máxima la prueba empieza: el primer pulso comienza a cerrarse a la espera máxima más el respiro. No se registra ningún castigo y la insolencia no cambia.
- **CA-MIN-16** (R-MIN-19, la pulsación de empezar no cuenta). La pulsación que empieza la prueba no se registra como pulsación del anillo: tras ella, el número de pulsos resueltos y su precisión son los de una prueba recién empezada. Una pulsación del anillo durante la espera, fuera del botón, tampoco cuenta ni resuelve ningún pulso.
- **CA-MIN-17** (R-MIN-19, el panel). El modelo del panel, con el anillo sin empezar, lleva el botón "Empezar", su tecla y el texto de la cuenta con los segundos que quedan, redondeados hacia arriba; con la prueba empezada, no lleva ninguno de los tres. **Ampliado el 16/11/2026**: la franja del botón, la tecla y la cuenta empieza debajo del aro (no pisa el círculo, la runa ni la franja verde) y acaba encima de la barra de progreso; con la letra de la configuración y un título de una o dos líneas cabe la fila entera, el aro no encoge y todo cabe en el panel, que no pasa del 60 % del alto de la pantalla. **Ampliado otra vez el 16/11/2026, tras la segunda prueba en PIE** (la fila salía por debajo del panel, encima de la frase del Warden): la fila la coloca el propio panel de Slate, con su tamaño de verdad; colocado con el tamaño que pide y con uno más alto, la fila queda dentro del panel, centrada en su franja y a lo ancho.
- **CA-MIN-18** (R-MIN-19, el acta). La línea del acta al empezar dice "por el botón" o "sola" y los segundos esperados. **Ampliado el 16/11/2026**: "Empezar" pulsado con la cuenta ya a cero, sin que se haya notado, deja "sola" con la espera máxima; pulsado justo antes (a los 7,96 s), "por el botón" y nunca la espera máxima.
- **CA-VIS-31** (R-VIS-30). El título de la Prueba del Warden es, letra por letra, "Prueba del Warden: si fallas, pierdes el turno". Cada una de las otras cuatro pruebas lleva su frase aprobada, y ninguna queda sin frase. **Corrección del autor del 16/11/2026**: la del duelo es, letra por letra, "Duelo: si pierdes, te quitan runas o vuelves a tu hogar" (la aprobada en el plan, "te roban runas", no cubría al que pierde sin runas ni las runas que vuelven a su Fuente).
- **CA-HUD-17** (R-HUD-9). Con la Prueba del Warden pendiente para el humano, las líneas de la caja del objetivo son la del objetivo y, debajo, "Te espera la Prueba del Warden" con el color del Warden; sin Prueba pendiente, solo la del objetivo. Con la Prueba pendiente para otro asiento, la caja del humano no cambia.

Manuales, por el autor, en PIE, a 1920x1080 y a 1024x768, con captura:

- **El recordatorio**: tras imponerse la Prueba (`SbStrike la-prueba` en su turno), la caja del objetivo enseña "Te espera la Prueba del Warden" hasta su siguiente turno, y desaparece al jugarla.
- **El anillo espera**: al abrirse, no se cierra ningún aro; se ven el título con lo que se juega, el botón "Empezar" y la cuenta atrás. Se entiende qué hay que hacer antes de que empiece.
- **Empezar por el botón** con el ratón y con la tecla: el primer aro empieza a cerrarse medio segundo después, y esa pulsación no cuenta.
- **Empezar sola**: sin tocar nada, a los 8 segundos empieza y se puede jugar con normalidad, sin castigo.
- **Las cinco pruebas** llevan su frase de lo que se juega: Fuente de un Dominio Olvidado, Prueba del Warden, escape del Vacío (`SbBanish`), duelo (`SbDuel`) y desafío final.
- El acta de la partida lleva la línea de cómo empezó cada prueba.
- La dificultad se siente igual que antes una vez empezada.

**Resultado de la prueba manual del autor (16/11/2026).** Superado: el recordatorio "Te espera la Prueba del Warden" en la caja del objetivo, y que desaparece al abrirse el anillo; el anillo espera quieto con el título, el botón, "o pulsa Intro" y la cuenta; empezar con el ratón, con Intro y sola a los 8 s; los títulos de la Prueba del Warden y del escape del Vacío; las líneas del acta. Sin verificar a mano: los títulos de la Fuente de un Dominio Olvidado, del duelo y del desafío final (los cubre el test `TituloConLoQueTeJuegas`). Dio tres correcciones (el botón tapaba la runa, la frase del duelo, la línea del acta), anotadas en R-MIN-19 y en los criterios. Con la corrección del botón, queda por mirar a mano que la fila va bajo el aro a 1920x1080 y a 1024x768.

**Segunda prueba manual del autor (16/11/2026).** Superado: el prefijo del corte ("Warden, a ti: ¡Quietos! …" en su turno y "Warden, al asiento N: ¡Quietos! …" en el de un dummy) y el centro del anillo libre, con la runa y la franja verde a la vista. Falló: a 1920x1080 y en ventana pequeña, la fila de "Empezar" salía fuera del panel, por debajo, encima de la franja de la frase del Warden (el botón tapaba la frase y los dos textos se montaban). Causa: la vista ponía la fila en un hueco de `SOverlay` con margen y centrado, y Slate centra sumando el margen de arriba menos el de abajo, no la mitad. Arreglado: la fila es hija del panel, que la coloca con sus propias medidas (CA-MIN-17, test `FilaDeEmpezarDentroDelPanel`). Queda por mirar a mano en las dos resoluciones.

**Tercera prueba manual del autor (16/11/2026).** Superado: la fila de "Empezar" queda dentro del panel, entre el círculo y la barra, a 1920x1080 y en ventana pequeña, y el botón responde al clic. Con esto, todas las pruebas manuales de la story están superadas, salvo los títulos de la Fuente de un Dominio Olvidado, del duelo y del desafío final, que siguen sin verificar a mano y cubre el test `TituloConLoQueTeJuegas`.

## Notas para quien lo implemente

- **Dónde arranca hoy el anillo.** `UMatchHostSubsystem::OpenRing` hace `Ring.Run.Begin(secuencia, ahora + RingAnnounceSeconds)`: los pulsos se fechan al abrir. Para esperar al jugador, la secuencia se sigue sorteando al abrir (la misma semilla da los mismos pulsos) y `Begin` se llama al empezar. Si la clase del anillo necesitara un cambio para eso y está en `SoulboundCore`, se dice en el plan antes de tocarla.
- **La entrada.** Durante el anillo, el clic izquierdo lo recoge `FRingInputProcessor` (`BoardViewActor.cpp`) antes que Slate, y `UMatchHostSubsystem::PressRing` lo entrega. El botón "Empezar" tiene que convivir con eso: se propone en el plan si durante la espera el clic va al botón de Slate o si la propia entrada del anillo decide que esa pulsación es "empezar". Lo que no puede pasar es que el mismo clic empiece la prueba y cuente como pulsación.
- **El panel.** `FRingPanelModel` (`Presentation/RingPanelModel.h`) es lo que pinta `SRingPanel`; `RingTitleText` da hoy el título y `RingInstructionLines` la instrucción. El modelo gana lo necesario para el botón, la tecla y la cuenta; sus tests (`RingPanelModelTest.cpp`) se actualizan, no se borran.
- **El recordatorio.** La caja del objetivo es `GoalPanel`, con `GoalPanelLines` (`StatusText.cpp`, S-040) y `IsGoalPanelShown`. La Prueba pendiente está en `FSeatState::bWardenTrialPending`.
- **El parámetro de la espera** va con los del anillo en los datos de la partida (`FMatchParams::Ring`, `Content/Data/Match/partida.json`). `MatchParamsTest` fija los valores del archivo: el valor nuevo entra en ese test en el mismo commit.
- **El acta.** Las líneas del anillo las escribe el anfitrión (`LogRingPulses`, `RecordHuman`, `RingPressLine` en `MatchRecord`).
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- Cambiar la dificultad de cualquier prueba, o dar por fallida una prueba que no se empieza.
- El duelo simultáneo entre dos humanos y su "empieza cuando los dos pulsen" (Fase 2).
- El resto del minijuego del GDD 09 (desenfunde, trampas del desafío final, habilidad de los bots).
- Un tutorial o una prueba de práctica del anillo.
- Sonido.
