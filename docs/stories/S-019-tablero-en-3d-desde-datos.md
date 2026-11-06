# S-019. Tablero en 3D desde datos

**Sistema**: tablero y Cripta (GDD 01), presentación en pantalla plana (GDD 07). **Módulo**: `Soulbound` (presentación). **Rama base**: `main`. **Tamaño**: grande. **Assets de Unreal**: sí, pero los importa el autor en un commit aparte; el agente no toca `Content/`.

## Contexto

La vista actual dibuja el tablero con formas básicas del motor. Esta story la hace colocar las piezas del kit de arte (prueba C, ronda 6: low-poly facetado con color por vértice) **desde los datos del tablero**: una losa por casilla, senderos por conexión, una estructura junto a cada casilla especial, el Altar en el centro, la cuña de cada Dominio, luces y Sellos. Todo lo que la vista de depuración sabe hacer (etiquetas, leyenda, crónica, halos de alcanzables, aro del asiento activo, líneas de petición, paso a paso, comandos) sigue funcionando encima, y las formas básicas quedan como **respaldo** cuando las piezas no están. El decorado (árboles, rocas, charcos, braseros) no lo coloca el código: lo compone el autor a mano en el nivel.

Decidido por el autor: 15/10/2026 (el kit pasa a producción; Nicho al extremo exterior; Altar como pieza propia; luces en el tablero; decorado a mano); 19/10 (las estructuras y todo lo que cambia con la partida lo coloca el código; el decorado, a mano; el tablero definitivo lo diseña el autor en el editor web y la vista debe valer para cualquier trazado radial); 20/10: (1) las fichas usan la carta de prueba del kit con el color del asiento; (2) las losas partidas se reparten al azar con una semilla fija, no una de cada N; (3) los assets se importan después de decidir el trono con el tablero definitivo delante, así que el código se desarrolla y se prueba **sin assets** gracias al respaldo. El movimiento de la ficha por el camino casilla a casilla y el corte a mitad de camino (R-TAB-26) son de S-020: aquí la ficha sigue deslizándose en línea recta.

## Reglas a implementar

- **R-VIS-1. Escala de mesa.** Las posiciones del editor se escalan para que el tablero quepa en una mesa de radio de datos (parámetro del actor, valor inicial 74 cm), con el Altar en el centro. Un metro del kit es 100 cm en Unreal; las piezas no se escalan. Las casillas de un Dominio caen dentro de su sector.
- **R-VIS-2. Cuñas y Altar.** Una peana y un suelo liso por Dominio, girados al sector de ese Dominio (el ángulo medio de sus casillas vistas desde el Altar); el Altar en el centro. Si el trazado no es radial (dos Dominios con el mismo sector, o un Dominio repartido en más de 60°), la vista lo avisa una vez en el registro y coloca las cuñas de 60 en 60 grados por identificador de Dominio.
- **R-VIS-3. Losas.** Una losa por casilla según su tipo: normal (una fracción de datos, valor inicial un cuarto, es la variante partida, sorteada con una semilla fija del tablero, de modo que sea la misma en cada partida y no siga un patrón), Nicho (cabecera hacia su estructura), Fuente, Nodo (puntas en la dirección del camino). El Altar es su pieza propia. La cara superior de la losa es la cota de la ficha.
- **R-VIS-4. Senderos.** Por cada conexión, tramos de sendero a lo largo del hueco entre las dos losas, orientados con la conexión, tantos como quepan (longitud menos las dos medias losas, dividido por el largo del tramo, de datos); las conexiones con el Altar y entre Dominios, igual.
- **R-VIS-5. Estructuras.** Junto a cada casilla especial, tocando su borde por el lado contrario al camino (perpendicular a su conexión, hacia fuera del grafo; con dos conexiones, hacia fuera de la bisectriz), a la distancia de datos: el trono en el Nicho (**si está configurado**: puede no existir si gana la variante "el Nicho es el trono"); el pilón en la Fuente, con la runa flotante encima mientras la reserva sea mayor que cero; en el Nodo, el arco Planar, las espadas de Duelo o el orbe del Oráculo según su efecto, **solo desde el Despertar** (antes, nada, R-TAB-8). Sello: las cadenas sobre la losa mientras la casilla esté sellada.
- **R-VIS-6. Fichas.** Una ficha por asiento con la carta de prueba del kit y el color del asiento (material dinámico sobre la ventana de la carta), posada sobre la losa; dos o más en la misma losa se reparten como hoy; deslizamiento recto como hoy. En el Vacío, fuera del tablero, como hoy.
- **R-VIS-7. Luz.** La lámpara sobre la mesa (foco cenital) y la exposición fija, con los valores de la prueba de conjunto como parámetros del actor (600 cd, conos 15° y 25°, 200 cm de altura, EV100 fijo 4, cielo tenue 0,5); sin sol ni cielo. Los braseros y sus luces son decorado del autor: la vista no los pone.
- **R-VIS-8. Respaldo.** Cada pieza y cada material se referencian por referencia blanda configurable en el actor (`EditAnywhere`), con valores iniciales a `/Game/Art/Tablero/SM_<pieza>` según `kit.json` (`SM_losa`, `SM_losa_partida`, `SM_losa_nicho`, `SM_losa_fuente`, `SM_losa_nodo`, `SM_sendero`, `SM_peana`, `SM_suelo_liso`, `SM_altar`, `SM_trono`, `SM_pilon`, `SM_runa_flotante`, `SM_nodo_planar`, `SM_nodo_duelo`, `SM_nodo_oraculo`, `SM_sello`, `SM_ficha_prueba`; materiales `M_ColorVertice`, `M_Emisivo`, `M_Tenue`). Si una pieza falta, la vista usa la forma básica de hoy para ese elemento y avisa una vez en el registro. El código compila y se prueba sin assets; los tests del comandlet no cargan ninguno.
- **R-VIS-9. Cámara y teclas.** La cenital de hoy sigue siendo la principal, ajustada al radio de mesa. Una tecla (valor inicial C) alterna con la **vista sentada** del asiento humano: 60 cm sobre la mesa, 80 cm fuera del borde, 30° hacia abajo, mirando al centro (la cámara de verdad, R-CAM-1 a 5, es de la interfaz). Otra tecla (valor inicial L) oculta y muestra etiquetas y halos, para las capturas.
- **Lo de siempre, encima**: etiquetas, leyenda, crónica, halos de alcanzables, aro del activo, líneas de petición, paso a paso y comandos siguen funcionando igual sobre el kit. Ninguna regla en la vista: el Despertar, la reserva, los Sellos y el Vacío se leen del `FMatchState`.

## Criterios de aceptación

Automáticos, en `Soulbound` (`Source/Soulbound/Private/Tests/`, sin mundo ni assets: la colocación es una función pura del tablero y los parámetros):

- **CA-VIS-1** (R-VIS-1, 2). Con `tablero_35.json`, todas las casillas caen dentro del radio de mesa, el Altar en el centro y cada casilla en el sector de su Dominio; cada cuña gira al ángulo medio de sus casillas (seis ángulos distintos separados unos 60°). Con posiciones no radiales (dos Dominios en el mismo sector), aviso y cuñas de 60 en 60.
- **CA-VIS-2** (R-VIS-5). El lado de la estructura de cada casilla especial es el contrario al camino (producto escalar con la conexión negativo) y a la distancia de datos del borde de la losa; con dos conexiones, fuera de la bisectriz.
- **CA-VIS-3** (R-VIS-4). El número de tramos de sendero por conexión es el que cabe, nunca cero en una conexión válida del tablero de 35; están orientados con la conexión y repartidos a lo largo del hueco.
- **CA-VIS-4** (R-VIS-3). Las losas partidas son la fracción de datos, las mismas en dos cálculos con el mismo tablero, y no son periódicas (no todas las de identificador múltiplo de N).
- **CA-VIS-5** (R-VIS-5). El estado de vista (calculado del `FMatchState`, sin actor) no pide estructuras de Nodo antes del Despertar y sí después; pide la runa flotante solo con reserva; pide el Sello solo con la casilla sellada; pide el trono solo si está configurado.
- **Motor y simulación**: sin cambios; los 258 tests actuales siguen en verde.

Manuales, en PIE con captura, cuando el autor haya importado los assets:

- Cenital con el kit: los seis Dominios en cuña, el Altar, losas, senderos, estructuras y la luz de la lámpara.
- Vista sentada (tecla C) desde tu asiento; tecla L sin etiquetas.
- `SbSeal` pone las cadenas y al caer el Sello desaparecen; `SbTakeRune` hasta vaciar una Fuente quita la runa flotante; antes del Despertar no hay estructuras de Nodo y con el primer vínculo aparecen.
- Sin assets (referencias vacías): el tablero de formas básicas de hoy, con el aviso en el registro.

## Notas para quien lo implemente

- La colocación (nombre a proponer: `BoardLayout` o similar; posiciones y rotaciones de losas, lado y posición de cada estructura, tramos de sendero, ángulo de cada cuña, losas partidas) es una **función pura** de `FBoardState` y de una struct de parámetros de vista, en el módulo `Soulbound`, con sus tests. Lo que depende de la partida (Despertar, reserva, Sellos, Vacío, trono configurado) es otra función pura del `FMatchState` que dice qué elementos se muestran. `ABoardViewActor` solo las lee y crea o actualiza componentes de malla estática (instanciadas donde se repita la pieza: losas, senderos).
- Parámetros del actor con valor inicial documentado: radio de mesa, fracción de losas partidas y su semilla, largo del tramo de sendero (2,4 cm), distancia de las estructuras al borde de la losa, luz y exposición, teclas. Nada fijo en el código.
- **Assets**: los importa el autor con Interchange (*Vertex Color Import Option: Replace*, sin materiales del archivo; materiales hechos a mano: `M_ColorVertice` con el color por vértice en Base Color, `M_Emisivo` y `M_Tenue` con el cian) en `Content/Art/Tablero/`, con los nombres de arriba; **commit aparte del autor**, verificados en el editor. El orden no importa: el código funciona sin ellos (R-VIS-8). La referencia de importación está en la prueba de conjunto del autor (14-15/10).
- La vista de hoy no se borra: sus formas son el respaldo. El decorado del autor vive en el nivel (`L_SpikeOnline`), fuera de este actor.
- `kit.json` (en `Saved/Art/prueba-c/ronda6/` del otro PC del autor; su contenido relevante está en estas notas) da las cajas de cada pieza: las losas miden unos 6,3 cm, su cara superior está a 0,55 cm; el Altar tiene 15 cm de apotema; el tramo de sendero mide 2,4 cm a lo largo de x; las estructuras tienen el origen en su centro y la base en z = 0; el trono mira hacia -y (su frente); el arco Planar se abre a lo largo de x y su velo mira a ±y; la losa del Nicho lleva la cabecera en +x; la del Nodo, las puntas en ±x.
- Nada de `Engine` en `SoulboundCore`; esta story no toca el motor. Ningún asset lo crea ni lo modifica el agente.

## Fuera de esta story

- Movimiento de la ficha por el camino casilla a casilla y corte a mitad de camino (R-TAB-26): S-020.
- Cámara de la interfaz (R-CAM-1 a 5), foco de evento, cartas de piedra: interfaz (GDD 07).
- Decorado por código; seis Dominios con tema y paleta propios; avatares; animación del velo del arco y del orbe; braseros; el Muro de los Perdidos; la Cripta.
- Variante "el Nicho es el trono": la decide el autor con su tablero; para la vista solo cambia qué asset existe.
