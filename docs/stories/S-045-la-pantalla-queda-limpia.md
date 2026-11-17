# S-045. La pantalla queda limpia: arriba, la partida; abajo, tú; lo demás, en el Grimorio

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible): panel de estado (R-HUD-1), columna de leyenda y crónica, teclas de la vista (R-CAM-1). **Módulos**: `Soulbound` (presentación: `BoardViewActor`, `StatusText`, `StatusPanel`, `BoardViewState`, `UnreachableNotice`). `SoulboundCore` no cambia. **Rama base**: `main`, con S-044 fusionada. **Tamaño**: mediano. **Sin assets**.

## Contexto

El panel de arriba a la izquierda nació para sustituir a los mensajes de depuración (S-027) y la columna de la derecha enseña siempre la leyenda o la crónica. En las últimas stories la información que el jugador necesita ha ido encontrando su sitio: la decisión en su panel, el objetivo abajo a la izquierda, el resultado en el centro, y todo lo de consulta en el Grimorio (S-044). Lo que queda arriba a la izquierda y en la columna ya no hace falta a la vista, y a una persona que juega por primera vez le sobra.

Decidido por el autor el 16/11/2026:

- Se quita casi todo lo de arriba a la izquierda y toda la columna de la leyenda. En concreto, fuera: "Dado: 0 | reloj de turno" (también en el prólogo), "Prólogo: el Warden abre la partida…" y "Sellos activos".
- Siempre en pantalla queda solo esto: el contador de tiempo, la caja del objetivo, el panel de decisión, la frase del Warden y el botón del Grimorio.
- El contador va arriba a la izquierda y dice "Carrera 4:11" durante la carrera y "Partida 33:25" después.
- Una regla para toda la pantalla: **arriba a la izquierda, la partida** (el reloj y lo que afecta a toda la mesa); **abajo a la izquierda, tú** (tu objetivo y lo que tienes pendiente).
- Dos avisos de mesa van bajo el reloj: que es la Noche de los Ecos y que un jugador está a un turno de ganar.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-HUD-14 (nueva). La caja de la partida.** Arriba a la izquierda, en lugar del panel de estado, una caja pequeña con el aspecto de la caja del objetivo:
  - **El reloj**, siempre que la partida cuenta: "Carrera M:SS" con lo que queda de la carrera por el vínculo y, terminada la carrera, "Partida M:SS" con lo que queda de partida. En minutos y segundos, sin más texto.
  - **Los avisos de mesa**, debajo del reloj, con el color de la línea del Warden, mientras duran:
    - con la Noche de los Ecos activa: "Noche de los Ecos: los Ecos entran en cualquier Dominio";
    - con un aspirante que no es el humano (`IsAspirant`): "El asiento N está a un turno de ganar". Con varios, una línea por cada uno. Si el aspirante es el humano no sale: su objetivo ya lo dice (S-040).
  - No se ve en el prólogo ni con la partida terminada. Ninguna tecla la oculta. Mientras hay una frase del Warden en pantalla sigue a la vista (no comparte sitio con la frase, a diferencia de la caja del objetivo).
- **R-HUD-15 (nueva, sustituye a R-HUD-1 como HUD del jugador). El panel de estado pasa a ser de depuración.** El panel de estado de hoy, con todas sus líneas, deja de verse por defecto. Sigue existiendo, oculto, y su tecla (`DebugTextToggleKey`, T) lo enseña y lo vuelve a ocultar: sirve para depurar y para leer lo que pasa en una prueba. Su contenido no cambia en esta story, salvo que ya no hace falta que esquive nada ni que reserve sitio.
  - Con él oculto, nada de lo que solo él decía se pierde para el jugador. Cada cosa tiene ya su sitio: de quién es el turno y su tirada, en el panel de decisión o de espera (S-033, S-039); las peticiones, en el panel de decisión; el resultado, en el centro (S-041); el objetivo y la Prueba pendiente, abajo a la izquierda (S-040, S-042); el reloj, la Noche de los Ecos y el aspirante, en la caja de la partida; los Sellos, sobre el tablero (S-037); y lo demás, en el Grimorio (S-044). El plan repasa las líneas del panel una por una y dice dónde queda cada una; si alguna no tiene sitio, se pregunta antes de implementar.
  - **Precisión del autor del 17/11/2026, al aprobar el plan: el trilero en la espera.** Dos líneas del trilero no tenían sitio fuera del panel de estado: la muestra de las runas y la mezcla (S-010), en las que nadie elige. La espera (`WaitView`, R-DEC-5) gana un caso: "Trilero: X roba a Y", con "Runas a la vista: D1 D3" durante la muestra (y " (a salvo: D2)" detrás cuando Koschei apartó una) y, después, "Mezclando N esferas" durante la mezcla. Los datos son los mismos que enseñan hoy esas dos líneas del panel de estado; la etapa de apartar la runa y la de elegir esfera siguen siendo una elección (panel de decisión o espera de elección, como hoy).
- **R-HUD-16 (nueva). La columna desaparece.** La columna de la derecha con la leyenda y la crónica se quita de la pantalla: su contenido está en las pestañas del Grimorio. El tablero usa siempre todo el ancho, como hoy cuando los paneles se ocultan. Desaparecen las teclas que solo servían para ella: la de ocultar paneles (`PanelsToggleKey`, H) y la de cambiar entre leyenda y crónica (`ColumnTabKey`, P). El Grimorio deja de anunciar H como tecla para cerrarlo.
- **R-CAM (precisión). Las teclas que quedan.** C cambia la vista, L oculta o muestra las etiquetas y los números, G abre el Grimorio, Intro vuelve al menú al acabar, y T y D son de depuración. `SbHelp`, la pestaña "Cómo se juega" del Grimorio (las teclas que lista el código, no el texto del autor) y cualquier texto de código que nombre teclas dicen solo estas.
- **Lo que se recoloca.** El aviso de casilla inalcanzable (S-039) esquiva la caja de la partida, la caja del objetivo, el panel de decisión y el botón del Grimorio, y ya no la columna ni el panel de estado (salvo que esté a la vista). El panel de decisión, la frase del Warden y el resultado se centran en el tablero a todo el ancho.
- **Lo que no cambia.** El panel de decisión, la frase del Warden, la caja del objetivo, el anillo, el Grimorio y su botón, el resultado en el centro, la etiqueta de versión, y el panel de depuración de la tecla D. Ninguna regla.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-HUD-25** (R-HUD-14, reloj). La línea del reloj dice "Carrera 4:11" con 251 s de carrera por delante y "Partida 33:25" con 2005 s de partida por delante y la carrera terminada; los segundos van siempre con dos cifras ("Carrera 0:07"); no hay línea en el prólogo ni con la partida terminada.
- **CA-HUD-26** (R-HUD-14, avisos de mesa). Con la Noche de los Ecos activa, la caja lleva su línea con el color del Warden; sin Noche, no. Con el asiento 3 de aspirante y el humano en el asiento 0, lleva "El asiento 3 está a un turno de ganar"; si el aspirante es el humano, no lleva ninguna línea de aspirante; con dos aspirantes ajenos, dos líneas en orden de asiento. El reloj va siempre primero.
- **CA-HUD-27** (R-HUD-15). La función que decide qué paneles se ven devuelve, al empezar la partida, el panel de estado oculto; con su tecla pulsada una vez, visible; otra vez, oculto. La caja de la partida, la del objetivo y el botón del Grimorio no dependen de esa tecla.
- **CA-HUD-28** (R-HUD-16). El ancho que usa el tablero es siempre el de todo el viewport. La descripción de las teclas de la vista (`DescribeViewKeys` o la función que la sustituya) nombra C, L, G e Intro, y no H ni P.
- **CA-HUD-29** (lo que se recoloca). Las zonas que esquiva el aviso de casilla inalcanzable son las de la regla: con el panel de estado oculto no está entre ellas, y la columna no está nunca.
- **CA-HUD-38** (R-HUD-15, precisión del trilero). Con un trilero en la muestra, del asiento 1 al asiento 2 con las runas D1 y D3, la espera dice "Trilero: X roba a Y" (con los nombres de asiento de la espera, `SeatLabel`) y "Runas a la vista: D1 D3"; con una runa apartada D2, "Runas a la vista: D1 D3 (a salvo: D2)"; en la mezcla de tres esferas, "Mezclando 3 esferas". Es así sea el humano el ladrón, la víctima o ninguno de los dos; en la etapa de elegir esfera manda la elección (panel de decisión del ladrón humano o espera de elección), como hoy.
- Los tests que hoy fijan líneas del panel de estado (`StatusTextTest`) siguen en verde: su contenido no cambia, solo cuándo se ve. Los que dependían de la columna o de sus teclas se actualizan en el mismo commit, no se borran sin más: el PR dice cuáles y por qué.

Manuales, por el autor, en PIE, en la vista sentada y en la cenital, a 1920x1080 y a 1024x768, con captura:

- Al empezar, en pantalla hay solo: el reloj arriba a la izquierda, el objetivo abajo a la izquierda, el botón del Grimorio abajo a la derecha y, cuando tocan, el panel de decisión y la frase del Warden. El tablero ocupa todo el ancho.
- En el prólogo no hay nada arriba a la izquierda.
- El reloj pasa de "Carrera" a "Partida" al terminar la carrera y descuenta bien.
- Con la Noche de los Ecos (`SbEvent noche-de-los-ecos`) sale su línea bajo el reloj y se va al terminar.
- Con un dummy de aspirante (dándole las seis runas y llevándolo al Altar) sale "El asiento N está a un turno de ganar" y se va si pierde una runa.
- Un robo de un dummy a otro dummy y un robo de un dummy al humano: antes de la mezcla, el panel de espera enseña quién roba a quién y las runas a la vista; después, "Mezclando N esferas".
- La tecla T enseña y oculta el panel de estado de siempre, sin descolocar lo demás. H y P no hacen nada.
- Se puede jugar una partida entera sin echar de menos nada de lo que se ha quitado; lo que se quiera consultar está en el Grimorio.
- El aviso de casilla inalcanzable no tapa ninguna de las cajas.

**Resultado de las pruebas manuales (autor, PIE, 17/11/2026): superadas, salvo lo que se anota sin verificar.**
- En el prólogo no hay nada arriba a la izquierda.
- Al empezar, en pantalla hay solo el reloj, el objetivo, el botón del Grimorio en la esquina y, cuando tocan, el panel de decisión y la frase del Warden; el tablero ocupa todo el ancho.
- El reloj pasa de "Carrera" a "Partida".
- T enseña y oculta el panel de estado. H y P no hacen nada.
- La línea de la Noche de los Ecos sale bajo el reloj y se va al terminar.
- Con un dummy aspirante sale "El asiento N está a un turno de ganar".
- El trilero en la espera, con el humano de víctima: "Trilero: X roba a Y", "Mezclando N esferas" y su desenlace.
- **Sin verificar a mano** (anotado por el autor): la ventana pequeña de 1024x768, y que el aviso de casilla inalcanzable esquive las cajas. Quedan cubiertos solo por sus tests (CA-HUD-28 y CA-HUD-29), que no miran posiciones en pantalla.
- No se nombran en el informe de la prueba: la línea "Runas a la vista" del trilero (comprobada por CA-HUD-38), el robo entre dos dummies, que la línea del aspirante se va al perder una runa, y la partida entera sin echar nada de menos.

## Notas para quien lo implemente

- **Qué ya existe.** La vista ya sabe trabajar sin columna (`bPanelsHidden`, "con los paneles ocultos, la cenital usa todo el ancho") y sin panel de estado (`bDebugTextHidden`). Esta story hace de eso el estado normal y quita lo que sobra, en vez de construir un modo nuevo.
- **La caja de la partida** puede ser otro `SStatusPanel` con sus líneas, como la caja del objetivo (`GoalPanel`, `GoalPanelLines`, `IsGoalPanelShown`, S-040): mismas piezas, otro sitio y otras condiciones.
- **El reloj.** Los datos están donde hoy los lee la línea "Carrera por el vínculo: quedan N s" y la de "fin de la partida en MM:SS" (`StatusText.cpp`).
- **Las teclas** viven en `ABoardViewActor` (`PanelsToggleKey`, `ColumnTabKey`, `DebugTextToggleKey`, `GrimoireKey`…), en Config y en `DescribeViewKeys`; `SbHelp` las lista. `ColumnTab.h` y lo que solo sirva a la columna se quita si nada más lo usa.
- **El texto del autor** (`como-se-juega.md`) nombra H y P en su párrafo de teclas. Es un dato del autor: no se toca; se le señala en el PR.
- **El acta** no depende de los paneles y no cambia.
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Precisiones del autor al aprobar el plan (17/11/2026)

- El trilero en la espera: ver la precisión de R-HUD-15 y CA-HUD-38.
- El número de turno y el nombre de la fase quedan solo en el panel de estado (tecla T). Lo que se juegan dos dummies en un duelo y el rendimiento fijo del Warden ("rinde 0,73") tampoco salen fuera de él.
- El reloj redondea hacia arriba y enseña "Carrera 0:00" mientras acaba el turno con la carrera agotada; sin Warden no hay línea de "Partida".
- La caja de la partida: el reloj en blanco y con la letra grande; los avisos de mesa con el color del Warden y también grandes.
- Las teclas de "Cómo se juega" (las que lista el código): C, L y G, las flechas del Grimorio, 1 a 9 y 0, e Intro (empezar una prueba y volver al menú al acabar). Sin T ni D.
- Con T, el panel de estado sale debajo de la caja de la partida, y el de depuración debajo de los dos.
- El anillo y la carta del Elegido también se centran a todo el ancho.
- Fuera los TODO de renombrar `DebugTextToggleKey` y `bDebugTextHidden`: el panel vuelve a ser de depuración y el nombre vuelve a valer.
- `ColumnTab.h` y su test se borran.
- La entrada "Reloj de la Cripta" del glosario se corrige: desde esta story hay un contador en la caja de la partida (decisión del autor del 16/11/2026).

## Fuera de esta story

- Cambiar el contenido del Grimorio o de sus pestañas (S-044).
- Rediseñar el panel de decisión, la frase del Warden o el anillo.
- Una marca sobre la ficha del aspirante, o cualquier marca nueva sobre el tablero.
- Enseñar las runas de los demás jugadores fuera del Grimorio (decidido por el autor: dentro).
- El nombre del Dominio en grande sobre el tablero y el número de casilla (pendiente de decisión del autor).
- La interfaz del GDD 07 completa (Fase 2).
