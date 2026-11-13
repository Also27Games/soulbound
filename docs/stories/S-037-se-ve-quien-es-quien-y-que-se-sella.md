# S-037. Se ve quién es quién y qué se sella: el número de la ficha se lee desde cualquier vista, y el Sello se marca al ponerse, mientras dura y al liberarse

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible): fichas (R-VIS-6, R-VIS-10) y destellos (R-VIS-12). **Módulos**: `Soulbound` (presentación: `SeatTokenActor`, `Presentation/FlashCues`, `BoardViewActor`). `SoulboundCore` no cambia. **Rama base**: `main`, con S-036 fusionada. **Tamaño**: pequeño. **Sin assets nuevos**: todo con las formas básicas y el material básico que ya usan el aro del turno y los halos.

## Contexto

Probando el 13/11/2026, antes de la build de los probadores, el autor vio dos cosas que no se leen:

1. **El número del jugador no se ve en la ficha.** Todo el juego nombra a los jugadores por su número: "Le toca al asiento 2", los botones del voto y del duelo, la leyenda ("1: Elegido, Tifón"), la crónica. La ficha lleva el número (`ASeatTokenActor::Label`), pero tumbado sobre la carta, mirando hacia arriba y girando con ella: se pensó para la vista cenital, y desde la vista sentada, que es en la que arranca la partida, queda de canto y no se lee. El jugador no puede saber a quién vota ni a quién reta.
2. **El Sello no se encuentra.** Cuando el Warden sella una casilla solo aparecen las cadenas del kit, que desde la cámara casi no se ven. El jugador no sabe cuál ha sido, ni la encuentra turnos después, cuando decide por dónde ir. Cuando el Sello se libera, tampoco se entera.

Decidido por el autor el 13/11/2026:

- El número de cada ficha se lee desde las dos vistas.
- Al sellarse una casilla hay un destello sobre ella; mientras está sellada lleva una marca que se queda; y al liberarse hay otro destello, el inverso del primero.
- El color del Sello es el rojo con que el panel de estado ya escribe "Sellos activos", no el morado: el morado significa perder una runa (S-029).

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-VIS-23 (nueva). El número de la ficha mira a la cámara.** El número del asiento va sobre la ficha, deja de girar con ella y se orienta en cada pintado hacia la cámara activa, sea la sentada o la cenital. La orientación sale de una función pura (nombre a proponer: de la posición del número y la de la cámara, la rotación con que el texto queda de frente y derecho). Su color es el del asiento, el mismo de su aro del turno y de su ficha en la leyenda. Su tamaño y su altura sobre la ficha están en Config, con valor inicial documentado, y se eligen para que se lea desde la vista sentada a 1024x768 sin tapar la ficha de al lado. La tecla que oculta las etiquetas (`OverlayToggleKey`) lo sigue ocultando. En el Vacío, con la ficha oculta, no se ve.
- **R-VIS-24 (nueva). La marca del Sello.** Mientras una casilla está sellada (`FBoardState::SealedTiles`) lleva un aro del color del Sello (`SealMarkColor`, Config; valor inicial un rojo vivo, a proponer) alrededor de la losa, por encima del suelo del kit, además de las cadenas. Es un elemento propio, distinto del halo blanco de casilla alcanzable: una casilla sellada y a la vez alcanzable (Paso Libre, R-POD-2) enseña las dos cosas. Aparece al sellarse y desaparece al liberarse. Su tamaño sale de la losa colocada o de Config, nunca de un número fijo en el código.
- **R-VIS-25 (precisión de R-VIS-12). El destello del Sello.** `FlashCuesFromEvents` gana dos casos y un objetivo nuevo, la casilla (`EFlashTarget::Tile`, con el identificador de la casilla en `Id`; nombre propuesto):
  - `SealPlaced`: destello **hacia dentro** sobre la casilla sellada, del color del Sello (`EFlashColorSource::Seal`, nombre propuesto).
  - `SealReleased`: destello **hacia fuera** sobre la casilla liberada, del mismo color.
  - Duración, tamaño y crecimiento, los de los demás destellos (`FlashSeconds`, `FlashRingScale`, `FlashGrowth`). Los de una misma casilla se encadenan como los de un mismo asiento. `FlashColorOf` devuelve el color del Sello para ellos.
- **La leyenda.** En el bloque "Tablero", la línea del Sello pasa a "Aro rojo y cadenas: sellada, no se pasa" (sin kit: "Aro rojo y SELLO: sellada, no se pasa"), y su muestra de color es la del Sello, no el negro de hoy, que no se ve.
- **Lo que no cambia.** Las cadenas del kit, la regla del Sello, la línea "Sellos activos" del panel de estado y los destellos de runas.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo:

- **CA-VIS-22** (orientación, R-VIS-23). Con la cámara justo encima del número, la rotación lo deja mirando hacia arriba; con la cámara delante y a su misma altura, lo deja vertical y de frente a ella; con la cámara al otro lado, de frente a ese lado. En los tres casos el texto queda derecho, no boca abajo ni en espejo. Con la cámara en el mismo punto que el número, devuelve una rotación válida y no falla.
- **CA-VIS-23** (destello al sellar, R-VIS-25). Un `SealPlaced` de la casilla 12 da un destello con objetivo casilla, `Id` 12, hacia dentro y color del Sello, con la duración pedida. `FlashColorOf` de ese destello es el color del Sello que se le pasa.
- **CA-VIS-24** (destello al liberar, R-VIS-25). Un `SealReleased` de la casilla 12 da un destello con objetivo casilla, `Id` 12, hacia fuera y color del Sello. Un `SealPlaced` y un `SealReleased` de la misma casilla en la misma lectura salen encadenados: el segundo espera una duración.
- **CA-VIS-25** (los demás no cambian, R-VIS-25). Los destellos de `RuneObtained`, `RuneReturned` y `RuneStolen` son los mismos que antes de esta story.
- **CA-VIS-26** (marca, R-VIS-24). La función que dice qué casillas llevan la marca (nombre a proponer, del estado) devuelve exactamente las de `SealedTiles`: ninguna sin Sellos, y deja de incluir una casilla cuando su Sello se libera.

Manuales, por el autor, en PIE, en la vista sentada y en la cenital (tecla C), a 1920x1080 y a 1024x768, con captura:

- El número de cada ficha se lee en las dos vistas, del color de su asiento, y no gira. Con dos fichas en la misma casilla se leen los dos. La tecla de ocultar etiquetas lo oculta.
- Al sellar una casilla (un evento del Warden que selle, lanzado con `SbEvent` entre turnos y en paso a paso): destello rojo hacia dentro sobre ella y, después, el aro rojo se queda. Se encuentra de un vistazo desde la vista sentada.
- Al liberarse el Sello: destello rojo hacia fuera y el aro desaparece.
- La leyenda dice "Aro rojo y cadenas" con una muestra roja.

## Notas para quien lo implemente

- El número: `ASeatTokenActor::Label` es un `UTextRenderComponent` enganchado a `Pivot` (gira con la ficha) y con rotación fija `FRotator(90, 180, 0)`. Hay que desengancharlo del giro y orientarlo con la cámara del jugador local; la vista ya sabe cuál es la activa (`SeatedViewKey`, S-019 y S-022). La orientación se calcula en la función pura y el actor solo la aplica.
- El destello de casilla: hoy hay destellos de ficha y de Fuente (`FSourceFlash`, `SourceFlashes`, `RefreshFlashes` en `BoardViewActor`). El de casilla puede reutilizar ese mecanismo con otra pieza; se propone en el plan.
- Lo aprendido en S-029 y S-033, para no repetirlo:
  - Lo que queda por debajo del suelo del kit (`KitFloorHeight`, 3,1; el relieve llega a 3,9) no se ve. La marca y el destello van por encima.
  - Para teñir, el material básico (`BaseMaterial`), que tiene el parámetro "Color". El emisivo del kit no lo tiene.
  - El tamaño y la cota salen de la pieza colocada o de Config, no de un número a ojo.
  - Lo que no se pueda ver sin PIE no se rehace a ciegas: se anota para la prueba manual. Lo que ya se ve bien no se repinta.
- Los colores nuevos y los tamaños, en Config con su valor inicial documentado. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.

## Fuera de esta story

- Imágenes en las cartas de las fichas (un Eco y los seis Elegidos): decidido por el autor para después de las pruebas; pide un material nuevo y assets.
- Cambiar las cadenas del kit o la regla del Sello.
- Un sonido al sellar o liberar.
- Que la vista reintente el avance de una ficha rechazado mientras el Warden habla (bloqueo visto solo con comandos de depuración).
