# S-029. Se ve lo que pasa: feedback visual mínimo en el tablero

**Sistema**: interfaz en plano (GDD 07: fichas, foco y consecuencias visibles; pilar 3, caos legible; pilar 4, el Warden te habla a ti). **Módulos**: `Soulbound` (presentación). `SoulboundCore` no cambia. **Rama base**: `main`, con S-031 fusionada. **Tamaño**: pequeño. Es la última de la semana de arreglos y la primera que se cae si no hay tiempo.

## Contexto

El tablero en 3D enseña dónde está cada ficha, pero no qué le pasa: un Eco y un Elegido se distinguen por un aro tenue que nadie nota; ganar o perder una runa solo se lee en la crónica; al vincularse a un Elegido, el jugador no ve qué ha conseguido. Un probador, tras diez partidas, no sabía "para qué sirve el Elegido". El Grimorio de S-027 lo explica si se abre; esta story hace que lo importante se vea sin abrir nada.

Decidido por el autor el 9/11/2026, cuatro cosas y nada más:

1. **La ficha toma el color de su Dominio al vincularse**; como Eco es gris. El número del asiento sigue encima.
2. **La ficha activa se resalta**: un aro luminoso bajo la ficha del asiento que tiene el turno, del color del asiento; desaparece al acabar su turno.
3. **Destello al ganar o perder una runa**: la ficha del asiento y la Fuente implicada parpadean un instante, en claro al ganar y en rojo al perder (robo, devolución, Desvío, desafío fallado).
4. **La carta del Elegido al vincularse**: un panel centrado, unos segundos, con el nombre del Elegido, su Dominio, su afinidad en una frase y su mapa de runa a poder. Reutiliza los textos y el formato del Grimorio (S-027).

Sin sonido, sin partículas, sin arte nuevo: colores sobre los materiales que ya existen y un panel Slate.

Añadido por el autor el 12/11/2026:

5. **El Grimorio no vibra.** En algunas partidas el panel del Grimorio (S-027) tiembla mientras está abierto. Las Ayudas mandan al jugador a pulsar G, y un panel que tiembla parece roto. No cambia lo que enseña.

Añadido por el autor el 13/11/2026, tras probar el PR en PIE:

6. **El color del rol va en una peana de color, no en un aro fino.** El plan tiñó el aro de la base (0,25 cm de grosor) porque la carta del kit no se tiñe, y desde la cámara un Eco y un Elegido se ven iguales. Bajo la ficha va un disco plano relleno, más ancho que la peana de la carta: gris de Eco o color del Dominio. Es una forma básica teñida, sin asset nuevo; su diámetro y su cota van en configuración. Sustituye al aro de rol; el aro del turno (color del asiento, con pulso) se queda. El destello de la ficha enciende el disco. Tiene que verse en una casilla normal y sobre el Altar.
7. **La instrucción del anillo decía dónde fallar** (corrección a S-031, R-VIS-17). "Haz clic cuando el aro entre en la franja verde" manda a pulsar en el borde de la zona buena, donde la precisión vale casi cero: la precisión es 1 cuando el aro toca la runa y baja en línea recta hasta 0 en el borde de la franja. En las actas del 13/11 el autor, siguiéndola, pulsó entre 0,15 y 0,27 s antes y falló todos los anillos. La instrucción pasa a "Haz clic cuando el aro toque la runa. En la franja verde cuenta; cuanto más cerca, mejor." La frase del Warden y "Cómo se juega" ya lo dicen así en `main` (commit c294966). No cambian ni la franja ni las reglas.
8. **El destello dice qué runa es y hacia dónde va** (13/11/2026, corregido el mismo día). Hacia dentro es recibir y hacia fuera es perder, igual para la ficha que para la Fuente. Al coger una runa, el aro de la ficha se cierra hacia dentro con el color del Dominio de esa runa y el halo de la Fuente se abre hacia fuera con ese color. Al perderla, el aro de la ficha se abre hacia fuera con el color de perder (`FlashLossColor`, en configuración) y el halo de la Fuente se cierra hacia dentro con el color de su runa. En un robo, el ladrón hacia dentro con el color de la runa y la víctima hacia fuera con el de perder. Un destello sin tipo de runa conocido usa el claro de siempre. El color de perder es fijo y no el de la runa porque el color se lee antes que el sentido; choca con el de algún Dominio sea rojo o morado, y lo desempata el sentido. Decisión del autor del 13/11/2026: el color de perder pasa de rojo a un morado vivo (valor inicial de `FlashLossColor`: R 0,55, G 0, B 1); donde esta story dice "rojo" al perder, léase el color de perder.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-VIS-10 (precisión). Color de la ficha.** `ASeatTokenActor` recibe del estado el rol del asiento y su Dominio: Eco, color gris de datos (`EchoTokenColor`, Config); Elegido, el color del Dominio que ya usa la leyenda (`DomainColor`). Cambia en el momento del vínculo (`Bonded`) y vuelve a gris si el asiento deja de ser Elegido. El aro de rol actual se mantiene o se retira, a elección de quien implemente, sin que el número deje de verse.
- **R-VIS-11 (precisión). Ficha activa.** Un aro plano bajo la ficha del asiento activo (`State.ActiveSeat`), con el color del asiento y un pulso lento de intensidad (periodo en Config, `ActiveRingPulseSeconds`, valor inicial 1,5). Solo durante el turno, incluidas sus fases de elección; entre turnos, ninguno. En un duelo o un trilero entre turnos (S-015), lo llevan los dos participantes.
- **R-VIS-12 (precisión). Destellos.** Al leer en el registro `RuneObtained`, `RuneStolen` (ganador y víctima), `RuneReturned`, `RunesSeized`, `FinalChallengeFailed`: la ficha del asiento afectado parpadea `FlashSeconds` (Config, valor inicial 0,6), claro si gana y rojo si pierde; la Fuente del tipo de runa implicado, igual. Varios eventos seguidos no se pisan: se encadenan. Una función pura (`Presentation/FlashCues.h`, nombre propuesto) convierte los eventos nuevos del registro en la lista de destellos (asiento o casilla, color, duración); la vista los ejecuta.
- **R-VIS-13 (precisión). Carta del Elegido.** Al `Bonded` del asiento humano, un panel centrado (`SElectCardPanel`, nombre propuesto, reutilizando las filas del Grimorio de S-027) durante `ElectCardSeconds` (Config, valor inicial 6), o hasta que el jugador haga clic. Si el jugador abre el Grimorio con `G`, la carta se cierra. No se muestra para los dummies.
- **R-VIS-21 (precisión). El Grimorio, quieto.** Hoy `TickHud` llama a `SGrimoirePanel::SetContent` en cada refresco con el panel abierto, y `SetContent` borra y vuelve a crear todos sus textos, que además usan `AutoWrapText` (parte al ancho de lo pintado antes). Hipótesis del autor: cada reconstrucción estrena textos sin ancho conocido y el panel cambia de tamaño de un fotograma a otro; depende de lo largos que sean los textos, y por eso solo pasa en algunas partidas. Desde esta story: el panel solo se reconstruye cuando su contenido cambia (las filas y las líneas de poderes se comparan con las anteriores, como `SDecisionPanel::SameLook`), y sus textos se parten con `WrapTextAt` a un ancho que no depende de lo pintado. Si la hipótesis no explica el temblor, se dice en el PR antes de probar otra cosa.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/FlashCuesTest.cpp`, nombres `Soulbound.Game.Presentation.Destellos.<QueSeComprueba>`; y los de color y aro en `Private/Tests/TokenLookTest.cpp`, `Soulbound.Game.Presentation.Ficha.<QueSeComprueba>`):

- **CA-VIS-10** (color). Dado un asiento Eco, el color de ficha es `EchoTokenColor`; dado el mismo asiento vinculado al Dominio 3, es el color del Dominio 3; si vuelve a Eco, gris otra vez.
- **CA-VIS-11** (aro). Con el asiento 2 activo en fase de tirada, solo el 2 lleva aro; entre turnos, ninguno; en un duelo entre turnos entre el 1 y el 3, los dos.
- **CA-VIS-12** (destellos). Un `RuneObtained` del asiento 0 con la runa del Dominio 4 produce dos destellos claros, ficha 0 y Fuente del Dominio 4; un `RuneStolen` del 1 al 0 produce claro en el 1 y rojo en el 0; dos eventos en el mismo instante producen destellos encadenados, no simultáneos sobre la misma ficha.
- **CA-VIS-13** (carta). La regla de visibilidad de la carta devuelve visible desde el `Bonded` del humano hasta `ElectCardSeconds` después, o hasta un clic, o hasta abrir el Grimorio; no la enseña para un `Bonded` de un dummy.
- **CA-VIS-19** (Grimorio, R-VIS-21). Las filas del Grimorio de un mismo estado, calculadas dos veces, son iguales; si un asiento gana una runa, se vincula o activa un poder, dejan de serlo. La comparación es una función pura o el operador de igualdad de `FGrimoireRow`, sin Slate.

Manuales, por el autor, en PIE, con captura:

- Al vincularse (`SbBond 0 2`), la ficha pasa de gris al color del Dominio 2 y sale la carta del Elegido con su afinidad y su mapa; un clic la cierra.
- Durante tu turno, tu ficha lleva el aro luminoso; durante el de un dummy, la suya.
- Al coger una runa, tu ficha y la Fuente parpadean en claro; al perder una (`SbTakeRune 0 2 1` no vale: es edición; usar un robo o `SbEvent` con pérdida de runa), en rojo.
- En 1024x768 la carta no tapa la frase del Warden.
- Con el Grimorio abierto (`G`) durante una ronda entera de los dummies, el panel no tiembla ni cambia de tamaño, a 1024x768 y a 1920x1080, en una partida con los cuatro asientos vinculados; cuando alguien gana una runa, su fila se actualiza.

## Notas para quien lo implemente

- Los colores de Dominio y de asiento ya están en la vista (`DomainColor`, `SeatColor`); los materiales de la ficha ya reciben color por `SetComponentColor`. No hay materiales nuevos.
- Los destellos salen del registro de eventos (`EventLog`), como la crónica y las Ayudas: la misma lectura incremental, sin escuchar al motor.
- La carta reutiliza `GrimoireRows` de S-027 para un solo asiento.
- Tests primero, en rojo; los actores solo tienen criterio manual.
- Lo aprendido en S-033 y S-031 (12/11/2026), para no repetirlo:
  - Lo que va por debajo del suelo del kit no se ve: las casillas están a la cota `KitFloorHeight` y el Altar a 0, y el relieve de la tierra tapa lo que quede más abajo (así estaba el halo del Altar). El aro de la ficha activa tiene que verse con el kit, en una casilla normal y sobre el Altar; su cota sale de la pieza real ya colocada o va en configuración, no de una cuenta a ciegas.
  - Una capa centrada con márgenes desiguales (16 a la izquierda, la columna de paneles a la derecha) queda media columna a la izquierda: `AlignChild` no centra dentro del hueco. La carta del Elegido va como la capa de la frase y la del anillo: la capa ocupa el hueco (`HAlign_Fill` con sus márgenes) y el contenido se centra dentro.
  - Los textos que se parten usan `WrapTextAt` con el ancho del hueco, no `AutoWrapText`, que parte al ancho de lo pintado antes.
  - No se cambia la forma de pintar algo que ya se ve bien sin poder verlo: lo que no tenga test se anota para la prueba manual del autor.
- Al cerrar: la leyenda del HUD (S-027) explica el aro, los colores de ficha y los destellos en una línea cada uno.

## Fuera de esta story

- Partículas, sonido, animaciones de cámara y el foco de evento del diseño (GDD 07).
- Cualquier cambio en lo que el jugador puede hacer: eso fue S-028.
