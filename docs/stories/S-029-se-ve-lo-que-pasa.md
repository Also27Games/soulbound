# S-029. Se ve lo que pasa: feedback visual mínimo en el tablero

**Sistema**: interfaz en plano (GDD 07: fichas, foco y consecuencias visibles; pilar 3, caos legible; pilar 4, el Warden te habla a ti). **Módulos**: `Soulbound` (presentación). `SoulboundCore` no cambia. **Rama base**: `main`, con S-028 fusionada. **Tamaño**: pequeño. Es la última de la semana de arreglos y la primera que se cae si no hay tiempo.

## Contexto

El tablero en 3D enseña dónde está cada ficha, pero no qué le pasa: un Eco y un Elegido se distinguen por un aro tenue que nadie nota; ganar o perder una runa solo se lee en la crónica; al vincularse a un Elegido, el jugador no ve qué ha conseguido. Un probador, tras diez partidas, no sabía "para qué sirve el Elegido". El Grimorio de S-027 lo explica si se abre; esta story hace que lo importante se vea sin abrir nada.

Decidido por el autor el 9/11/2026, cuatro cosas y nada más:

1. **La ficha toma el color de su Dominio al vincularse**; como Eco es gris. El número del asiento sigue encima.
2. **La ficha activa se resalta**: un aro luminoso bajo la ficha del asiento que tiene el turno, del color del asiento; desaparece al acabar su turno.
3. **Destello al ganar o perder una runa**: la ficha del asiento y la Fuente implicada parpadean un instante, en claro al ganar y en rojo al perder (robo, devolución, Desvío, desafío fallado).
4. **La carta del Elegido al vincularse**: un panel centrado, unos segundos, con el nombre del Elegido, su Dominio, su afinidad en una frase y su mapa de runa a poder. Reutiliza los textos y el formato del Grimorio (S-027).

Sin sonido, sin partículas, sin arte nuevo: colores sobre los materiales que ya existen y un panel Slate.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-VIS-10 (precisión). Color de la ficha.** `ASeatTokenActor` recibe del estado el rol del asiento y su Dominio: Eco, color gris de datos (`EchoTokenColor`, Config); Elegido, el color del Dominio que ya usa la leyenda (`DomainColor`). Cambia en el momento del vínculo (`Bonded`) y vuelve a gris si el asiento deja de ser Elegido. El aro de rol actual se mantiene o se retira, a elección de quien implemente, sin que el número deje de verse.
- **R-VIS-11 (precisión). Ficha activa.** Un aro plano bajo la ficha del asiento activo (`State.ActiveSeat`), con el color del asiento y un pulso lento de intensidad (periodo en Config, `ActiveRingPulseSeconds`, valor inicial 1,5). Solo durante el turno, incluidas sus fases de elección; entre turnos, ninguno. En un duelo o un trilero entre turnos (S-015), lo llevan los dos participantes.
- **R-VIS-12 (precisión). Destellos.** Al leer en el registro `RuneObtained`, `RuneStolen` (ganador y víctima), `RuneReturned`, `RunesSeized`, `FinalChallengeFailed`: la ficha del asiento afectado parpadea `FlashSeconds` (Config, valor inicial 0,6), claro si gana y rojo si pierde; la Fuente del tipo de runa implicado, igual. Varios eventos seguidos no se pisan: se encadenan. Una función pura (`Presentation/FlashCues.h`, nombre propuesto) convierte los eventos nuevos del registro en la lista de destellos (asiento o casilla, color, duración); la vista los ejecuta.
- **R-VIS-13 (precisión). Carta del Elegido.** Al `Bonded` del asiento humano, un panel centrado (`SElectCardPanel`, nombre propuesto, reutilizando las filas del Grimorio de S-027) durante `ElectCardSeconds` (Config, valor inicial 6), o hasta que el jugador haga clic. Si el jugador abre el Grimorio con `G`, la carta se cierra. No se muestra para los dummies.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/FlashCuesTest.cpp`, nombres `Soulbound.Game.Presentation.Destellos.<QueSeComprueba>`; y los de color y aro en `Private/Tests/TokenLookTest.cpp`, `Soulbound.Game.Presentation.Ficha.<QueSeComprueba>`):

- **CA-VIS-10** (color). Dado un asiento Eco, el color de ficha es `EchoTokenColor`; dado el mismo asiento vinculado al Dominio 3, es el color del Dominio 3; si vuelve a Eco, gris otra vez.
- **CA-VIS-11** (aro). Con el asiento 2 activo en fase de tirada, solo el 2 lleva aro; entre turnos, ninguno; en un duelo entre turnos entre el 1 y el 3, los dos.
- **CA-VIS-12** (destellos). Un `RuneObtained` del asiento 0 con la runa del Dominio 4 produce dos destellos claros, ficha 0 y Fuente del Dominio 4; un `RuneStolen` del 1 al 0 produce claro en el 1 y rojo en el 0; dos eventos en el mismo instante producen destellos encadenados, no simultáneos sobre la misma ficha.
- **CA-VIS-13** (carta). La regla de visibilidad de la carta devuelve visible desde el `Bonded` del humano hasta `ElectCardSeconds` después, o hasta un clic, o hasta abrir el Grimorio; no la enseña para un `Bonded` de un dummy.

Manuales, por el autor, en PIE, con captura:

- Al vincularse (`SbBond 0 2`), la ficha pasa de gris al color del Dominio 2 y sale la carta del Elegido con su afinidad y su mapa; un clic la cierra.
- Durante tu turno, tu ficha lleva el aro luminoso; durante el de un dummy, la suya.
- Al coger una runa, tu ficha y la Fuente parpadean en claro; al perder una (`SbTakeRune 0 2 1` no vale: es edición; usar un robo o `SbEvent` con pérdida de runa), en rojo.
- En 1024x768 la carta no tapa la frase del Warden.

## Notas para quien lo implemente

- Los colores de Dominio y de asiento ya están en la vista (`DomainColor`, `SeatColor`); los materiales de la ficha ya reciben color por `SetComponentColor`. No hay materiales nuevos.
- Los destellos salen del registro de eventos (`EventLog`), como la crónica y las Ayudas: la misma lectura incremental, sin escuchar al motor.
- La carta reutiliza `GrimoireRows` de S-027 para un solo asiento.
- Tests primero, en rojo; los actores solo tienen criterio manual.
- Al cerrar: la leyenda del HUD (S-027) explica el aro, los colores de ficha y los destellos en una línea cada uno.

## Fuera de esta story

- Partículas, sonido, animaciones de cámara y el foco de evento del diseño (GDD 07).
- Cualquier cambio en lo que el jugador puede hacer: eso fue S-028.
