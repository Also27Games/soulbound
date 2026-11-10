# S-028. Decidir a la vista: cada decisión del jugador, en un panel con botones

**Sistema**: interfaz en plano (GDD 07: la petición al jugador, el foco de evento, votaciones y Oráculo en pantalla plana). **Módulos**: `Soulbound` (vista y controlador). `SoulboundCore` no cambia: el anfitrión ya expone todo lo que el panel necesita. **Rama base**: `main`, con S-032 fusionada. **Tamaño**: mediano.

## Contexto

Hoy, cuando el jugador tiene que decidir algo (cuántos dados, repetir tirada, saltar, retar, expulsar, robar, apartar la runa de Koschei, elegir esfera del trilero, usar Desvío, un dilema del Warden, señalar a un rival, qué runa regala o pierde, votar, consultar o rechazar el Oráculo), la petición es una línea de texto en el HUD con las opciones numeradas, y responde con las teclas 1 a 9, 0 para "no", un clic en una ficha para votar y un clic en el orbe para consultar. Dos probadores, sin conocerse, dijeron lo mismo: "no sé qué tengo que hacer". Uno, tras diez partidas: "cuando te dicen que votes, no sé votar; dice pulsa y no hay cómo hacerlo".

El anfitrión ya tiene las entradas: `SubmitHumanChoice(índice)`, `SubmitHumanIntent`, `SubmitHumanVote(asiento)`, `SubmitHumanOracleConsult` y `SubmitHumanOracleRefuse`. El controlador las llama desde teclas y clics. Esta story pone un panel delante de esas mismas entradas; las teclas y los clics de hoy siguen funcionando.

Decidido por el autor el 9/11/2026:

- **Un solo panel de decisión** (`SDecisionPanel`, nombre propuesto), centrado abajo, que aparece cuando hay una decisión abierta para el asiento humano y desaparece al resolverse. Título con qué se decide, una línea de contexto, un botón por opción con su número delante, y el reloj de la elección como barra que se vacía.
- **Votar es clic en una ficha o clic en un botón con el nombre del asiento**: las dos cosas. El panel lo dice con esas palabras.
- **El Oráculo**: dos botones, consultar o rechazar (el segundo solo si el asiento puede, Tifón). El clic en el orbe sigue valiendo.
- **Elegir casilla no lleva botones**: el panel solo dice "Elige una casilla iluminada" con el reloj. Las casillas ya se iluminan.
- Mientras el anillo (S-026) está abierto, el panel de decisión no se muestra: el anillo es la decisión.
- Sin sonido, sin animación y sin arte: Slate con el estilo de los paneles de S-027.

Añadido por el autor el 10/11/2026:

- **Mientras el Warden habla, el panel se queda a la vista pero bloqueado**: botones desactivados y el texto "El Warden habla". El anfitrión ya rechaza cualquier entrada del humano en esa espera (S-023) y el acta la anota como "la mesa espera al Warden"; sin esto, el panel enseñaría botones que parecen activos y no hacen nada.
- **La elección de uno o dos dados (Celeridad) va en el panel como las demás**, con dos botones, y se contesta también con las teclas 1 y 2. Todas las decisiones con botones se contestan igual con su número.

## Reglas a implementar

### Vista (`Soulbound`, `Presentation/`)

- **R-DEC-1 (precisión). Qué se enseña.** Una función pura en `Presentation/DecisionText.h` (nombre propuesto, junto a `EventText.h`) recibe el estado de la partida y el asiento humano y devuelve, si hay decisión abierta para él, un `FDecisionView` (nombre propuesto): título, línea de contexto, lista de opciones (texto y número de tecla), tipo de entrada (botones, ficha, orbe, casilla), el plazo y si está bloqueada. "Bloqueada" es un dato que recibe la función (si la mesa espera al Warden, `UMatchHostSubsystem::IsWardenSpeaking`), no algo que calcule. Reutiliza los textos de `ChoicePrompt` y `EventText`; no inventa reglas: las opciones son exactamente las que el motor tiene abiertas (`FPendingChoice`, votación, Oráculo, destino).
- **R-DEC-2 (precisión). El panel.** `SDecisionPanel` pinta el `FDecisionView`. Un botón por opción, con su número de tecla delante; al pulsarlo, el controlador llama a la misma entrada que la tecla correspondiente, y la tecla sigue valiendo. En la elección de dados (`DiceCount`, solo con Celeridad) son dos botones, "1  Un dado" y "2  Dos dados", y las teclas 1 y 2. Con tipo "ficha", los botones llevan el número y el nombre del asiento votable y la frase "o haz clic en su ficha". Con tipo "orbe", "Consultar" y, si procede, "Rechazar". Con tipo "casilla", solo el texto y el reloj. El reloj es una barra que se vacía con el plazo de la elección (`ChoiceDeadline` del estado), sin números.
- **R-DEC-3 (precisión). Cuándo.** El panel se muestra cuando `FDecisionView` existe y el anillo del humano no está abierto, y se oculta al resolverse la decisión o al vencer su plazo. `H` lo oculta como a los demás paneles; con el telón bajado no aparece. Mientras la mesa espera al Warden, el panel sigue visible con la vista bloqueada: los botones salen desactivados y, en el sitio del reloj, el texto "El Warden habla"; al acabar la espera vuelven los botones y el reloj. No es una regla nueva: enseña el rechazo que el anfitrión ya aplica. Las líneas del panel de estado de S-027 que hoy repiten la petición (`ChoicePrompt`) dejan de hacerlo cuando el panel de decisión está visible, para no decirlo dos veces.
- **R-DEC-4 (precisión). Escala y posición.** Mismo cálculo de fuente que los paneles de S-027 (`PanelFontSize` a 1080 de alto, mínimo `PanelMinFontSize`), centrado abajo, por encima de la frase del Warden y sin tapar el tablero en 1024x768.

### Controlador (`Soulbound`, `Spike/SpikeOnlinePlayerController`)

- Las teclas 1 a 9 y 0, el clic en ficha y el clic en el orbe no cambian. Los botones del panel llaman a las mismas funciones (`SubmitHumanChoice`, `SubmitHumanVote`, `SubmitHumanOracleConsult`, `SubmitHumanOracleRefuse`), nunca al motor directamente.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/DecisionTextTest.cpp`, nombres `Soulbound.Game.Presentation.Decision.<QueSeComprueba>`):

- **CA-DEC-1** (opciones). Con una elección `DiceCount` abierta para el humano, `FDecisionView` tiene tipo botones, tantas opciones como el motor ofrece, numeradas desde 1, y el plazo de la elección: dos opciones, "Un dado" con la tecla 1 y "Dos dados" con la tecla 2.
- **CA-DEC-2** (dilema). Con un dilema abierto, el título es el nombre del evento, la línea de contexto su texto, y las opciones son las del dilema en su orden.
- **CA-DEC-3** (voto). Con una votación abierta en la que el humano puede votar, el tipo es ficha, hay una opción por asiento votable con su número, y el texto incluye "haz clic en su ficha". Si el humano no puede votar, no hay vista.
- **CA-DEC-4** (Oráculo). Con una consulta del Oráculo abierta para el humano, el tipo es orbe; "Rechazar" solo aparece si el asiento tiene la afinidad que lo permite.
- **CA-DEC-5** (casilla). Con el humano en fase de elegir destino, el tipo es casilla, sin opciones, con el texto "Elige una casilla iluminada".
- **CA-DEC-6** (nada). Sin decisión abierta para el humano, o con la decisión abierta para un dummy, no hay vista.
- **CA-DEC-7** (anillo). Con un anillo del humano abierto, la regla de visibilidad devuelve oculto aunque haya decisión.
- **CA-DEC-8** (el Warden habla, R-DEC-3). Con una elección abierta para el humano y la mesa esperando al Warden, hay vista, está bloqueada y sus opciones son las mismas que sin la espera; sin la espera, no está bloqueada. La regla de visibilidad devuelve visible en los dos casos.

Manuales, por el autor, en PIE, con captura, a 1024x768 y a 1920x1080:

- Cada tipo de decisión muestra su panel y se resuelve igual con botón y con tecla: dados, acción de jugador (retar, expulsar, robar), esfera del trilero, dilema (`SbEvent` con uno), votación (`SbEvent` con una), Oráculo, Desvío (`SbBanish 0` con una runa de Desvío).
- El reloj se vacía y, al vencer, el panel desaparece y el motor resuelve como "no", como hoy.
- Con el panel visible, la línea de petición del panel de estado no se repite.
- `H` oculta el panel; vuelve con `H`.
- Con una decisión abierta y el Warden hablando (una frase que detiene la mesa), los botones salen desactivados con "El Warden habla", ni el botón ni la tecla hacen nada, y al terminar la frase vuelven. En el acta, la entrada rechazada sale como "la mesa espera al Warden".
- Con Celeridad, la elección de dados sale con sus dos botones y se contesta igual con el botón, con la tecla 1 y con la tecla 2.
- En 1024x768 el panel no tapa el tablero ni la frase del Warden.

## Notas para quien lo implemente

- Partir de `ChoicePrompt` (`EventText.cpp`): ahí está ya el texto de cada tipo de elección; `DecisionText` lo descompone en título, contexto y opciones en vez de una sola línea.
- La lista de asientos votables y si el humano puede votar las dice el motor (estado de la votación, S-017); el panel no decide quién vota.
- Slate puro, como `SLegendPanel`; los botones, `SButton` con texto. Sin UMG ni assets.
- Estilo: el mismo tamaño y colores que los paneles de S-027; la opción con el número en negrita delante del texto.
- Tests primero, en rojo. El panel y el controlador solo tienen criterio manual.
- Al cerrar: actualizar la línea de teclas de `como-se-juega.md` ("responde con los botones o con su número") y el README.

## Fuera de esta story

- Cambiar qué decisiones existen o sus plazos: son reglas del motor.
- El foco de evento del diseño (cámara, objetos diegéticos, GDD 07) y el sonido.
- Mando.
