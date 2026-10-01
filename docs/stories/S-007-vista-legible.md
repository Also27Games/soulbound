# S-007. Vista legible

**Sistema**: presentación de depuración (sin GDD propio; sirve a los criterios manuales de GDD 01, 02 y 03). **Módulo**: `Soulbound`. **Rama base**: `main`. **Tamaño**: media. **Se trabaja por rondas** (ver "Cómo se cierra").

## Contexto

La vista de S-005 y S-006 pinta la partida, pero el autor no puede seguirla en PIE: los colores salen pastel por la exposición automática del nivel, no se sabe qué casilla es cuál ni de qué Dominio, no se distingue quién juega y el único suceso en pantalla desaparece antes de leerlo. Resultado: los criterios manuales de S-005 y S-006 se dieron por buenos con la crónica, no con la vista. Esta story convierte la vista en una herramienta que se entiende sin leer la crónica. Sigue siendo vista de depuración, sin arte y sin assets.

Decidido por el autor el 02/10/2026: etiquetas con identificador y Dominio en todas las casillas; tecla del paso a paso, la barra espaciadora (y el botón A del mando si no complica); el modo paso a paso se cambia en caliente (Blueprint y consola), no solo por configuración; la crónica entera en un panel lateral con scroll, no seis líneas; la story se cierra por rondas.

## Qué se construye

1. **Exposición fija y paleta legible.** Un volumen de posproceso generado por código al iniciar la partida (sin assets, ilimitado) con exposición manual fija, para que los colores salgan como se escriben. Paleta pensada para distinguirse a un metro de distancia: seis Dominios con tonos bien separados; tipos especiales (Altar, Nicho, Fuente, Nodo) con color y, además, **forma distinta** (el Altar más grande; Nichos y Fuentes con un anillo o un cubo encima, generado por código), para que no dependan solo del color; sellada, alcanzable y Olvidado como hasta ahora pero contrastados.
2. **Etiquetas.** Sobre cada casilla, texto con su identificador y su Dominio ("D3·41"; el Altar "ALTAR"; los Nichos "N", las Fuentes "F", los Nodos "X" delante), con el componente de texto del motor, plano sobre la casilla y orientado a la cámara cenital. Tamaño en parámetro.
3. **Leyenda.** Texto fijo en pantalla con el significado de cada color y forma: tipos, Dominios con su número, alcanzable, sellada, Olvidado, Nicho Libre/Reclamado/Cerrado, ficha de Eco y de Elegido.
4. **Asiento activo.** La ficha del asiento activo se eleva y gira despacio mientras dura su turno; la línea del turno lleva el color del asiento; un aro del mismo color bajo su ficha.
5. **Crónica en pantalla.** Panel lateral a la derecha con todos los sucesos de la partida (el registro de eventos del motor, con el mismo texto que la crónica de la simulación): el último arriba, con scroll, cada línea con la marca de tiempo y el color del asiento afectado (los del Warden en naranja, los de carrera en cian). Hecho en C++ con Slate (`SScrollBox` añadido al viewport), sin widget ni asset. Si pasa de un tope de líneas en parámetro (valor inicial 300), se recortan las más viejas. Sustituye a la línea "último suceso".
6. **Modo paso a paso.** Propiedad del anfitrión de partida (`bStepMode`, valor inicial `false`): de configuración (`DefaultGame.ini`), legible y escribible desde Blueprint, y cambiable en caliente con dos comandos de consola en el controlador del spike, `SbStepMode 1|0` y `SbStep`, como los `SbStatus`/`SbLogin` que ya existen. Activo, la partida se detiene al terminar cada turno (al entrar en `BetweenTurns`) y el reloj de partida se congela hasta que el jugador pulsa la barra espaciadora (o el botón A del mando) o ejecuta `SbStep`; entonces se juega el siguiente turno entero (los dummies responden a sus 5 s; el humano, con su clic y su reloj de 30 s) y vuelve a detenerse. El anfitrión del Warden no evalúa mientras la partida está detenida. El texto dice "PASO A PASO: espacio para el siguiente turno" cuando espera. Congelar el reloj es desplazar el origen (`OriginWorldSeconds`), como ya hace el anfitrión.
7. **Capturas pendientes.** Con la vista nueva se repiten y documentan con captura los criterios manuales de S-005 (CA-TAB-22, Partida viva, Reloj, Sellos) y S-006 (Vínculo visible, Fin de carrera, Victoria del Warden).

## Criterios de aceptación

Automáticos: la story no toca las reglas de `SoulboundCore`; la única excepción es el evento `Moved` (ver "Añadidos del autor"), con su test. Los 65 anteriores siguen en verde. Compila y empaqueta (`build.ps1 -Que package`).

Manuales, en PIE, con captura cada uno:

- **Legible a un metro.** Con el editor a tamaño normal, a un metro de la pantalla, se distinguen los seis Dominios entre sí y los cuatro tipos especiales sin leer etiquetas.
- **Etiquetas.** Toda casilla muestra identificador y Dominio, legibles con la cámara cenital, sin tapar la casilla ni las fichas.
- **Asiento activo.** En cualquier instante se sabe quién juega mirando el tablero, sin leer el texto.
- **Deslizamiento.** Al mover cualquier asiento, su ficha se desliza en línea recta en el tiempo del parámetro y no salta; con el parámetro a 0 se coloca sin deslizar.
- **Crónica.** Tras un vínculo, un Sello y un agotamiento de reloj, los tres se leen en el panel con su asiento y su tiempo; el scroll permite volver al inicio de la partida.
- **Paso a paso.** Con el modo activo (por consola, en mitad de una partida): la partida se detiene al acabar cada turno, el reloj de carrera no avanza mientras espera, el Warden no sella mientras espera, y con la tecla o `SbStep` se juega exactamente un turno. Desactivado, nada cambia respecto a hoy.
- **El autor sigue una carrera entera.** En modo paso a paso, el autor juega una carrera desde el inicio hasta el fin (o victoria del Warden) y en cada turno puede decir quién tira, qué saca, qué casillas puede elegir, a dónde va y qué suceso produce, **sin abrir la crónica de consola**. Es el criterio que decide la story.
- **Capturas de S-005 y S-006**: las siete listadas arriba, en el PR.

## Cómo se cierra

Por rondas. El agente entrega una primera versión completa, la prueba en PIE por el MCP de Unreal si está conectado, y espera. El autor mira en PIE y dice qué no se lee; el agente corrige y vuelve a esperar. El PR se cierra cuando el autor pasa el criterio de la carrera entera. Cada ronda es un commit; la bitácora cuenta cuántas hubo.

## Notas para quien lo implemente

- Nombres del glosario: vista del tablero `BoardView` (`ABoardViewActor`), ficha `Token` (`ASeatTokenActor`), anfitrión de partida `MatchHost`, anfitrión del Warden `WardenHost`, registro de eventos `EventLog`. Nombres nuevos (modo paso a paso, leyenda, etiqueta, aro, panel de crónica) se proponen en el plan y en el PR.
- Todo generado por código al iniciar la partida: volumen de posproceso, etiquetas, aros, panel. Ni el mapa ni los Blueprints se tocan. Sin assets: formas básicas del motor, componente de texto del motor con su fuente por defecto, Slate para el panel (dependencias `Slate` y `SlateCore` en `Soulbound.Build.cs`).
- La vista sigue en solo lectura; el modo paso a paso vive en el anfitrión de partida (es quien lleva el reloj). La tecla la enlaza `ASpikeOnlinePlayerController` como el clic (`BindKey`, sin assets de entrada); los comandos de consola, `UFUNCTION(Exec)` en el mismo controlador. Que no rompa la pausa de UGT ni el cursor (S-005).
- El texto de cada evento sale de una sola función, compartida con la crónica de la simulación si es posible sin que `SoulboundCore` dependa de nada nuevo (si no, la vista tiene la suya y se dice en el PR).
- Colores y tamaños como parámetros con valor inicial documentado en la clase de la vista, no repartidos por el código.
- Las posiciones de las casillas vienen del archivo del tablero: si se encimán, se arregla en el editor web (`Scripts/board-editor`, botón "Separar casillas"), no en la vista. `PositionScale` sigue siendo parámetro por si hace falta aire.
- Steam Deck: etiquetas y panel tienen que leerse a 1280×800; comprobar al menos en una ventana de PIE a ese tamaño.

## Añadidos del autor durante las rondas

Pedidos por el autor al probar la vista en PIE; son parte de la story. Cada uno es un commit de ronda.

- **Deslizamiento de la ficha.** No se teletransporta: se desliza en línea recta de la casilla de origen a la de destino en un tiempo en parámetro (`TokenSlideSeconds`, valor inicial 0,5 s). Solo vista; el estado ya está en el destino desde que el motor acepta la intención. Recorrer el camino casilla a casilla queda para la story del Warden, cuando el motor guarde el camino.
- **Leyenda con los Dominios fuera y el Elegido por asiento.** Cada Dominio dice si está en juego u Olvidado (fuera) y, si lo hay, el asiento que lo reclamó; cada asiento dice si es Eco o Elegido y de qué Dominio, con su número.
- **Aro de Dominio Olvidado.** Las casillas de un Dominio Olvidado, además de apagadas, llevan un aro gris oscuro por debajo.
- **Cruz en la Fuente.** La Fuente de Poder lleva una cruz blanca con contorno negro (antes un anillo, que se confundía con el Nodo en rombo con esfera).
- **Numeración de fichas y de Nichos reclamados.** El número del asiento va escrito en la ficha y en el cubo del Nicho reclamado. El Nicho reclamado conserva el color de su Dominio y lleva un aro del color del asiento.
- **Evento `Moved`.** Excepción a "esta story no toca `SoulboundCore`": `SubmitDestinationIntent` registra el movimiento (asiento y casilla de destino), con test en `TurnRulesTest` y línea en la crónica de la simulación y en el panel ("el asiento 0 se mueve a D0·4").
- **Nombres de los Elegidos desde datos.** La leyenda y la etiqueta del Nicho enseñan `domains[].name` del archivo del tablero. Los nombres de `tablero_35.json` y `tablero_15.json` entran en un commit de datos aparte.

## Fuera de esta story

- Cámara sentada, HUD, componente de selección, Grimorio (GDD 07): stories de interfaz.
- Arte, materiales propios, kit de Blender.
- Efectos de los Nodos revelados (se verán con su story).
- Reproducción de partidas grabadas (GDD 08).
