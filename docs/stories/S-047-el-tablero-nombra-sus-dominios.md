# S-047. El tablero nombra sus Dominios: fuera los números de casilla, el nombre de cada Dominio en grande

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible): etiquetas del tablero (`TileLabel`) y cuñas de Dominio (R-VIS-2, R-VIS-5, R-VIS-27). **Módulos**: `Soulbound` (presentación: `BoardViewActor`, `BoardViewState`, `BoardKit`). `SoulboundCore` no cambia. **Rama base**: `main`, con S-049 fusionada (PR 77, 19/11/2026). **Tamaño**: pequeño. **Sin assets**.

## Contexto

Cada casilla del tablero lleva hoy una etiqueta con su Dominio y su número ("D4·58"): 86 etiquetas. Nacieron para depurar y para leer las actas. En la prueba del 15/11/2026 con la build de probadores (P4), el autor apuntó que el número de casilla no aporta nada a quien juega, y que lo que haría falta es saber en qué Dominio se está: su nombre, en grande, sobre el propio Dominio.

Decidido por el autor el 16/11/2026:

- Las etiquetas de las casillas normales, ocultas por defecto.
- Las casillas especiales conservan lo que sí sirve para jugar.
- El nombre de cada Dominio, en grande, sobre su suelo; y debajo, "Olvidado" cuando lo esté (el rótulo que quedó pendiente en S-040).

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-VIS-33 (nueva). Las etiquetas de casilla, ocultas por defecto.** Al empezar la partida no se ve la etiqueta de identificación de ninguna casilla (el Dominio y el número, "D4·58", ni la letra de tipo que la acompaña). La tecla de etiquetas (`OverlayToggleKey`, L) las enseña y las vuelve a ocultar: quedan para depurar y para seguir un acta.
  - **Lo que una casilla especial conserva siempre a la vista**, con las etiquetas ocultas: en una Fuente, su reserva ("4/4"); en un Nodo, su tipo tras el Despertar ("DUELO", "ORÁCULO", "PLANAR"); en un Nicho reclamado, el número de su dueño. Son datos de juego, no identificadores.
  - Los números de las fichas (S-037) no son etiquetas de casilla: se ven siempre, como hoy, salvo que el plan demuestre que hoy dependen de la misma tecla; en ese caso se separan.
- **R-VIS-34 (nueva). El nombre del Dominio sobre su suelo.** Cada Dominio lleva su nombre ("El Banquete", "El Fiordo Encadenado"…, el que ya enseña la leyenda) escrito en grande sobre el suelo de su cuña, tumbado, donde no tape casillas ni conexiones, y orientado para leerse desde la vista sentada y desde la cenital.
  - Color: el del Dominio, aclarado lo necesario para leerse sobre su propio suelo; tamaño y sitio dentro de la cuña en Config, con valor inicial documentado, relativos a la cuña (no un número a ojo).
  - Con el Dominio enseñado como Olvidado (las mismas condiciones del suelo, R-VIS-27: en el prólogo, cuando se dice su frase; al terminar la carrera), debajo del nombre va "Olvidado", en menor tamaño. El nombre también se apaga como el suelo.
  - El nombre no depende de la tecla de etiquetas: se ve siempre. No recibe clics.
- **El Grimorio y la ayuda.** Donde el texto de código hable de las etiquetas o de la tecla L, dice que L enseña los números de casilla, que por defecto están ocultos: hoy, la línea de la página "Teclas" del libro de "Cómo se juega" (`HowToPlayKeysPage`, que ven el Grimorio y el libro del menú) y `SbHelp`. La pestaña "El tablero" ya no existe (S-048). El texto del autor (`como-se-juega.md`) y sus láminas no se tocan.
- **Lo que no cambia.** El acta y la crónica siguen nombrando las casillas por su identificador ("D4·66"): con L se pueden localizar. Las marcas de Nicho, el suelo de los Olvidados, los halos y las fichas.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-VIS-34** (R-VIS-33). La función que dice qué texto lleva una casilla devuelve, con las etiquetas ocultas: nada para una casilla normal y para el Altar; solo la reserva para una Fuente; solo el tipo para un Nodo tras el Despertar y nada antes; solo el número del dueño para un Nicho reclamado. Con las etiquetas a la vista, lo mismo que hoy.
- **CA-VIS-35** (R-VIS-33). Al empezar la partida las etiquetas están ocultas; la tecla las enseña y otra pulsación las oculta.
- **CA-VIS-36** (R-VIS-34, texto). El texto del rótulo de un Dominio es su nombre; con el Dominio enseñado como Olvidado, el nombre y, en una segunda línea, "Olvidado". En el prólogo, antes de su frase de corrupción, un Dominio que va a ser Olvidado lleva solo el nombre.
- **CA-VIS-37** (R-VIS-34, sitio). La función que coloca el rótulo en la cuña devuelve un punto dentro de la cuña del Dominio, que no coincide con ninguna de sus casillas ni queda bajo el Altar, y un giro que deja el texto derecho para la vista sentada.

Manuales, por el autor, en PIE, en la vista sentada y en la cenital, a 1920x1080 y a 1024x768, con captura:

- Al empezar no se ve ningún "D4·58"; el tablero se ve más limpio. Con L aparecen y con L se van.
- Las Fuentes enseñan su reserva y los Nodos su tipo, sin la tecla.
- Los seis nombres se leen en las dos vistas, no tapan casillas, conexiones ni fichas, y se distinguen sobre el suelo liso y sobre el agrietado.
- Los Dominios Olvidados llevan "Olvidado" bajo su nombre, y lo ganan al terminar la carrera los que se queden sin dueño.
- Con la Noche de los Ecos (penumbra, S-043) los nombres se siguen leyendo.

## Notas para quien lo implemente

- **Las etiquetas.** `TileLabel` y lo que cuelga de `bOverlayHidden` en `ABoardViewActor`. Hoy la tecla oculta todo lo que cuelga de esa marca: hay que separar lo que es identificador de lo que es dato de juego. Qué texto lleva cada casilla sale de una función pura, con test.
- **Los nombres** de los Dominios son los que ya enseña "La mesa" del Grimorio, en "Los seis Dominios y su Elegido" (`DomainLegendText`, `DomainTitle`).
- **Dónde va el rótulo.** La colocación del tablero (`ComputeBoardLayout`, `FDomainWedgeLayout`) ya sabe el ángulo medio de cada cuña y dónde están sus casillas. El sitio del rótulo sale de ahí, en una función pura.
- **Cómo se escribe.** Como los demás textos sobre el tablero (`UTextRenderComponent`), sin asset nuevo. Lo aprendido en S-029, S-033 y S-037: lo que queda por debajo del suelo del kit no se ve; el tamaño sale de la pieza o de Config.
- **Cuándo un Dominio se enseña como Olvidado**: `ForgottenDomainsShown` (`BoardViewState`), lo mismo que usa el suelo.
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- El nombre del Elegido sobre el tablero, o cualquier otro rótulo.
- Cambiar cómo nombran las casillas la crónica y el acta.
- Una textura propia para los Dominios con Elegido (idea del autor, aparcada hasta la prueba con otro probador).
- Llamar "jugador" al asiento y numerar del 1 al 4 (aparcado).
