# S-034. La opción imposible no se ofrece: un dilema solo enseña lo que se puede cumplir

**Sistema**: eventos del Warden (GDD 05: composición de eventos, R-COM-1 a R-COM-4; direcciones, R-DIR-2 y R-DIR-3). **Módulos**: `SoulboundCore` (la regla, en `Match/WardenEventRules`) y `Soulbound` (un texto del desenlace, `Presentation/DecisionText`). **Rama base**: `main`, con S-031 fusionada. **Tamaño**: pequeño.

## Contexto

Probando S-033 el 12/11/2026, el autor lanzó con `SbEvent` el dilema "Elige tu veneno" (perder un turno o perder una runa) sin tener runas, eligió perder la runa, el Warden se burló y no pasó nada más. El código hace lo que dice el diseño: cada aplicación de una opción se valida por separado y, sin objetivo válido, no se aplica y queda la burla (R-COM-3, R-DIR-3). El hueco está en el diseño: nadie escribió qué pasa cuando la opción que no se puede cumplir es una de las que se le ofrecen a quien elige. Elegirla es escapar gratis, y lo que ve el jugador es que elige y no ocurre nada.

En partida real el caso es más estrecho que en la prueba: el cerebro del Warden ya descarta un dilema con una sola opción válida (R-WAR-8, CA-WAR-5), y el Oráculo solo entrega decretos. Queda abierto en los dilemas de tres o cuatro opciones (`tres-caminos`, `trueque`), que sí son candidatos con dos válidas y ofrecen también la imposible, y en cualquier dilema lanzado con `SbEvent`.

Decidido por el autor el 12/11/2026:

- **De momento, la opción imposible no se ofrece.** El dilema enseña solo las opciones que tienen algún objetivo válido.
- **Después de la entrega se cambiará**: la opción se podrá elegir, contará como insolencia y quien la elija recibirá el otro castigo. Esta story no lo hace; lo deja anotado.

## Reglas a implementar

### Motor (`SoulboundCore`, `Match/WardenEventRules`)

- **R-COM-5 (nueva). Opciones ofrecibles.** Al lanzarse un dilema, una opción es **ofrecible** si alguna de sus aplicaciones tiene ahora algún objetivo válido para quien lo dirige, con el mismo filtro que la resuelve (`HasValidTargets`, R-DIR-2 y R-DIR-3). El Alto no cuenta como objetivo válido aquí, igual que en el cerebro. Según cuántas lo sean:
  1. **Dos o más**: el dilema se abre solo con ellas. `FPendingChoice::Options` lleva sus índices en el evento, en orden; el reloj es el de ese número de opciones (`DilemmaClockSeconds`); agotado, el Warden sortea entre las ofrecidas (R-COM-2 no cambia).
  2. **Una sola**: no hay elección; esa opción se aplica como un decreto. Se registra `DilemmaChosen` con el asiento que dirige, el índice de la opción y `Detail` 2 ("la única posible"), para que la crónica y el panel puedan decirlo. No es desatender: ni `WardenIgnored` ni insolencia.
  3. **Ninguna**: no hay elección; se resuelve la primera opción como un decreto, que deja su burla por cada aplicación sin objetivo, como hoy (R-DIR-3).
- La opción ofrecida se valida otra vez al aplicarse, como hoy: si entre abrir el dilema y elegir deja de valer, burla. No se añade ninguna regla para ese caso.
- Los dummies eligen entre las ofrecidas (ya eligen entre `Options`).
- Un decreto (una sola opción en datos) no cambia.

### Presentación (`Soulbound`, `Presentation/DecisionText`)

- **R-DEC-14 (precisión de R-DEC-10, caso 2).** El desenlace de un `DilemmaChosen` con `Detail` 2 dice "<asiento> solo podía: <texto de la opción>", con el nombre del evento de título. Se enseña también cuando el asiento es el humano: no ha elegido nada y tiene que saber qué le ha pasado y por qué no le han preguntado.
- Los botones del dilema ya salen de `FPendingChoice::Options`: con R-COM-5 enseñan solo las ofrecidas, numeradas desde 1. No hay que tocarlos; se comprueba con un test.
- La crónica (`EventText`) dice el caso nuevo con sus palabras ("el asiento n solo puede la opción k del dilema").

## Criterios de aceptación

Automáticos, en `SoulboundCore`, sin mundo (`Private/Tests/WardenEventsTest.cpp`, nombres `Soulbound.Core.WardenEvents.<QueSeComprueba>`):

- **CA-COM-5** (dos o más, R-COM-5). Con `tres-caminos` (perder un turno, perder una runa al azar, Prueba del Warden) dirigido a un asiento sin runas: se abre el dilema con dos opciones, la primera y la tercera; su plazo es el de un dilema de dos; contestar la segunda ofrecida aplica la Prueba del Warden y registra `DilemmaChosen` con el índice 2 del evento. Con runas, se ofrecen las tres, como hoy.
- **CA-COM-6** (reloj agotado, R-COM-5 y R-COM-2). En el caso anterior sin runas, agotado el reloj, la opción sorteada es una de las dos ofrecidas, nunca la de perder una runa, con cualquier semilla de una lista de veinte.
- **CA-COM-7** (una sola, R-COM-5). Con `elige-tu-veneno` dirigido a un asiento sin runas: no se abre ninguna elección; el asiento pierde el turno; hay un `DilemmaChosen` con `Detail` 2; no hay `WardenIgnored` y su insolencia no cambia. Con runas, se abre el dilema con las dos.
- **CA-COM-8** (ninguna, R-COM-5). Con un dilema cuyas dos opciones no tienen objetivo válido (perder una runa sin runas; Vuelta al hogar a quien ya está en su hogar): no se abre ninguna elección, queda la burla y el evento termina.
- **CA-COM-9** (opción compuesta, R-COM-5). Una opción con dos aplicaciones de las que solo una tiene objetivo válido es ofrecible.

Automático, en `Soulbound` (`Private/Tests/DecisionTextTest.cpp`):

- **CA-DEC-26** (R-DEC-14). Con un `DilemmaChosen` de `Detail` 2 del asiento 2, la vista de desenlace lleva el nombre del evento y "solo podía" con el texto de la opción; con el asiento humano, también hay vista. Con un dilema abierto con dos de tres opciones ofrecidas, la vista de decisión del humano tiene dos botones, con las teclas 1 y 2 y los textos de esas dos opciones.

Manuales, por el autor, en PIE, con captura:

- Sin runas, `SbEvent elige-tu-veneno` dirigido a ti: no sale ningún botón, pierdes el turno y el panel dice que solo podías eso. Con una runa (`SbGiveRune 0 0`), salen los dos botones.
- Sin runas, `SbEvent tres-caminos` dirigido a ti: salen dos botones, sin el de perder una runa.
- Lo mismo dirigido a un dummy: el panel de espera y el desenlace dicen lo que pasa.

## Notas para quien lo implemente

- El sitio es `ApplyEvent` (`WardenEventRules.cpp`), donde hoy se abre el dilema con todas las opciones. `HasValidTargets` ya existe (S-016) y es el filtro que usa el cerebro.
- `DilemmaChosen` gana un valor de `Detail`: 0 la eligió, 1 la sorteó el Warden en su contra, 2 la única posible. Actualizar su comentario en `MatchState.h` y los sitios que leen `Detail` (`EventText`, `OutcomeView`, el registro de la simulación).
- `WardenEventStarted` lleva en `Detail` el número de opciones del evento y la crónica dice "DILEMA" o "DECRETA" según ese número. Si con una sola ofrecible queda raro que diga "DILEMA", se propone el cambio en el PR; no se decide por cuenta propia.
- El test del cerebro `Soulbound.Core.Warden.Bags.DilemaConUnaSolaOpcionValida` (CA-WAR-5) no cambia: el cerebro sigue sin elegir esos dilemas.
- Las actas de una misma semilla pueden cambiar en las partidas en que salía un dilema con una opción imposible. Se dice en el PR.
- Deuda que se deja escrita en el código: `// TODO(R-COM-5): tras la entrega, la opción imposible se ofrece, elegirla es insolencia y se aplica otra opción (decisión del autor del 12/11/2026)`.
- Tests primero, en rojo.

## Fuera de esta story

- La regla que vendrá después de la entrega (ofrecer la opción imposible, con insolencia y el otro castigo).
- Enseñar la opción imposible desactivada, con el motivo.
- Cambiar qué dilemas elige el cerebro (R-WAR-8) o qué entrega el Oráculo.
- Un texto propio por opción en `eventos.json`.
