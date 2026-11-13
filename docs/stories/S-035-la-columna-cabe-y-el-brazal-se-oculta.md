# S-035. La columna cabe y el Brazal se oculta: pestañas de Leyenda y Crónica, y de los demás solo lo que se puede saber

**Sistema**: interfaz en plano (GDD 07: R-HUD-4, paneles bajo demanda; R-HUD-6, lo que un jugador no debe saber no se le enseña) y runas (GDD 04: R-RUN-7, los tipos de runa de los demás están ocultos). **Módulos**: `Soulbound` (presentación: `Presentation/StatusText`, `Presentation/GrimoirePanel`, la columna de paneles de la vista). `SoulboundCore` no cambia. **Rama base**: `main`, con S-029 fusionada. **Tamaño**: pequeño.

## Contexto

La columna de la derecha lleva arriba la leyenda y debajo la crónica. Probando el 13/11/2026 en una ventana pequeña, el autor vio que la leyenda ocupa casi toda la altura y la crónica queda fuera de la vista. No es cosa solo de 1024x768: lo que aprieta es el alto, y un portátil a 1366x768, o a 1920x1080 con el escalado de Windows al 125 %, tiene el mismo.

Al mirarlo salió otra cosa. La leyenda y el Grimorio (S-027) enseñan las runas por tipo y los poderes activos de los cuatro asientos, y la leyenda, además, la insolencia de cada uno. El diseño dice lo contrario: de los demás se ve cuántas runas tienen, no de qué tipo, salvo en un trilero o cuando el Warden manda revelar un Brazal (R-RUN-7), y ni esos tipos ni la insolencia ajena llegan al jugador (R-HUD-6). Hoy el efecto Revelar Brazal no significa nada, porque ya se ve todo, y el jugador decide a quién roba o reta con una información que en el juego no tendrá.

Decidido por el autor el 13/11/2026:

1. **La columna tiene dos pestañas, Leyenda y Crónica.** La activa ocupa toda la altura. Se cambia con un clic en la pestaña y con una tecla. Arranca en Leyenda.
2. **De los demás, solo el número de runas.** Los tipos y los poderes activos de otro asiento se enseñan solo mientras su Brazal está revelado. Lo propio se ve entero.
3. **La insolencia de los demás no se enseña**; la propia, sí. Sustituye a la decisión del 5/10/2026 (S-016), que la ponía a la vista de todos.
4. **El panel de depuración (tecla D) y el acta no cambian**: son para el autor y siguen llevándolo todo.

La leyenda se queda para la prueba y para la entrega: es la clave del mapa mientras no haya arte que lo diga solo. Ni ella ni la crónica estarán en la interfaz definitiva.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-HUD-4 (precisión). La crónica, bajo demanda.** La columna enseña una sola de sus dos pestañas (`EColumnTab`, nombre propuesto: `Legend`, `Chronicle`), con la activa a toda la altura y una cabecera con los dos nombres, la activa resaltada. Empieza en Leyenda en cada partida. Cambia con un clic en el nombre de la otra pestaña y con una tecla (`ColumnTabKey`, Config; la tecla se propone en el plan: que no la use ya el juego ni la consuma Slate para el foco). La cabecera se queda el clic, como el panel de decisión, para que no llegue al tablero; el resto de la columna sigue sin recibir clics. La línea de teclas de la leyenda nombra la tecla nueva. La crónica sigue llenándose aunque no se vea, y conserva su rueda para ir al inicio.
- **R-RUN-7 (precisión). El Brazal de los demás.** Una función pura dice si quien mira puede ver los tipos de runa de un asiento (`CanSeeBracer`, nombre propuesto: del estado, el asiento mirado y el asiento que mira): sí si es el suyo o si su Brazal está revelado (`Soulbound::Rules::IsBracerRevealed`); no en el resto. Donde no se puede:
  - `BracerText` (el bloque "Brazal por asiento" de la leyenda) dice solo el total de runas, sin tipos ni poderes activos. Paso Libre por afinidad sí se dice: la afinidad de un Elegido es pública.
  - Las filas del Grimorio (`GrimoireRows`) de ese asiento llevan su Elegido, su afinidad y su mapa de runa a poder, que son públicos, pero no las cantidades por tipo ni la marca de activo; en lugar de "Poderes activos" dicen cuántas runas tiene y que sus tipos están ocultos.
  - Lo que el trilero ya enseña mientras dura (R-ROB-2) no cambia.
- **R-HUD-6 (precisión). La insolencia ajena.** El texto de estado de un asiento que va a la leyenda (`EffectStatusText`, o la función que lo sustituya) lleva la insolencia y los castigos que esperan solo para el asiento de quien mira. Marcado y "Brazal revelado" se siguen diciendo de todos: son estados visibles.
- **Lo que no cambia.** El panel de depuración (`DebugLines`) y el acta de partida siguen con todo, de los cuatro asientos. La crónica dice lo mismo que hoy.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/StatusTextTest.cpp`, nombres `Soulbound.Game.Presentation.Estado.<QueSeComprueba>`, y `Private/Tests/GrimoireTextTest.cpp`, `Soulbound.Game.Presentation.Grimorio.<QueSeComprueba>`):

- **CA-HUD-6** (pestañas, R-HUD-4). La pestaña inicial es Leyenda; cambiar da Crónica y volver a cambiar, Leyenda; una partida nueva empieza otra vez en Leyenda.
- **CA-HUD-7** (ver o no ver, R-RUN-7). `CanSeeBracer` es verdadero para el asiento propio; falso para otro asiento; verdadero para otro asiento con el Brazal revelado y falso otra vez cuando la revelación caduca.
- **CA-HUD-8** (Brazal en la leyenda, R-RUN-7). Con el asiento 2 con dos runas del Dominio 1 y una del 3 y el humano en el 0: el texto del Brazal del 2 dice que tiene 3 runas y no nombra ni D1, ni D3, ni ningún poder; el del 0 lleva sus tipos y sus poderes; con el Brazal del 2 revelado, el del 2 lleva los suyos. Un Fenrir ajeno conserva "Paso Libre (afinidad)".
- **CA-HUD-9** (insolencia, R-HUD-6). Con el asiento 2 en insolencia 3 y el humano en 1: el estado del 2 no dice su insolencia; el del 0 dice la suya. El 2 Marcado se sigue diciendo.
- **CA-HUD-10** (Grimorio, R-RUN-7). Las filas del Grimorio del asiento 2, sin revelar: llevan su Elegido, su afinidad y su mapa; ninguna runa lleva cantidad ni marca de activo; dicen su total. Las del 0, enteras. Con el Brazal del 2 revelado, enteras también. La comparación de filas de S-029 (R-VIS-21) nota el cambio al revelarse.

Manuales, por el autor, en PIE, a 1024x768 y a 1920x1080, con captura:

- La columna arranca en Leyenda y la leyenda cabe entera; un clic en "Crónica" y la tecla la cambian, y la crónica ocupa toda la altura. El clic en la cabecera no mueve la ficha ni elige casilla.
- Con los cuatro asientos con runas (`SbGiveRune`), la leyenda y el Grimorio enseñan los tipos y los poderes solo del jugador; de los demás, el número.
- Con un Brazal revelado (`SbEvent ensename` u otro que lo revele, hasta que le toque a un dummy), sus tipos y sus poderes aparecen en la leyenda y en el Grimorio, y desaparecen cuando caduca.
- La insolencia de los dummies no sale en la leyenda; la del jugador, sí.
- El panel de depuración (`D`) sigue enseñándolo todo.

## Notas para quien lo implemente

- La columna es el `SVerticalBox` de `PanelColumn` en `BuildOverlay` (`BoardViewActor.cpp`): hoy, la leyenda con alto automático y la crónica con el resto. Un `SWidgetSwitcher` o la visibilidad de cada panel sirven; la cabecera, dos botones de texto que no toman el foco.
- `BracerText` y `GrimoireRows` ya son funciones puras con tests: ganan el asiento que mira (`GrimoireRows` ya recibe `HumanSeat`). `EffectStatusText` la usa también el panel de depuración: que la depuración no pierda nada.
- Lo aprendido estos días, para no repetirlo: textos con `WrapTextAt` y no `AutoWrapText`; nada centrado con márgenes desiguales; y lo que no se pueda ver sin PIE se anota para la prueba manual, no se rehace a ciegas.
- Al cerrar: la línea de teclas de `Content/Data/Textos/como-se-juega.md` es dato del autor; se avisa en el PR de la tecla nueva.
- Tests primero, en rojo. Los nombres nuevos, en el glosario en un commit aparte.

## Fuera de esta story

- Avisar en la pestaña Crónica de que hay líneas nuevas sin leer.
- Revisar qué cuenta la crónica de las runas de los demás: hoy dice los sucesos públicos tal como ocurren.
- Los retratos por asiento del diseño (R-HUD-1) y quitar la leyenda: llegan con la interfaz definitiva.
- Que el estado oculto no llegue al cliente (R-HUD-6 entera): es de la partida en línea, Fase 2. Aquí solo se deja de enseñar.
