# S-048. "Cómo se juega" es un libro: una página por tema, con su imagen y el comentario del Warden

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible, y pilar 4, el Warden te habla a ti): "Cómo se juega" (S-022) y Grimorio por pestañas (S-044). **Módulos**: `Soulbound` (presentación: `HowToPlay`, `HowToPlayLibrary`, `GrimoirePanel`, `GrimoireBook`). `SoulboundCore` no cambia. Datos: `Content/Data/Textos/como-se-juega.md` (lo escribe el autor). **Rama base**: `main`, con S-045 fusionada. **Tamaño**: pequeño. **Con assets del autor**: una imagen por página, capturas del propio juego, que importa el autor en commit aparte; sin ellas, las páginas salen sin imagen.

## Contexto

En la prueba del 15/11/2026 con la build de probadores (P4), el "Cómo se juega" no se entendió: estaba desordenado, era un bloque de texto largo y solo se podía leer en el menú, antes de jugar. El autor apuntó entonces lo que haría falta: texto a un lado, imagen al otro, y cada imagen relacionada con su texto. Desde S-044 ese texto se lee también dentro de la partida, en una pestaña del Grimorio, pero sigue siendo un bloque que se desplaza.

Decidido por el autor el 17/11/2026:

- "Cómo se juega" pasa a ser un libro: una página por tema, que se pasa.
- Cada página lleva un título, dos o tres frases con la regla dicha en claro, una imagen y, debajo, un comentario del Warden con su color. La regla se entiende sola; el Warden pone el tono.
- El mismo libro se lee en el menú y en el Grimorio: se escribe una vez.
- Las imágenes son capturas del propio juego, del autor.

## El formato del archivo (lo escribe el autor)

`como-se-juega.md` sigue siendo Markdown y pasa a leerse así:

- La portada: el título de primer nivel del archivo (`# El Grimorio de los condenados`) es el título del libro, y una línea `>` antes de la primera página es la frase de la portada, del Warden. Las dos son opcionales. Texto del autor (17/11/2026): "El Grimorio de los condenados" y "Aquí está todo escrito… salvo cuánto tardaréis en decepcionarme."
- Cada título de segundo nivel (`## Tu turno`) empieza una página y es su título.
- Los párrafos que siguen son el texto de la regla.
- Una línea que empieza por `>` es el comentario del Warden de esa página. Como mucho uno por página; puede no haber.
- Una línea `![](NombreDeLaImagen)` dice qué imagen lleva la página; puede no haber.

Ejemplo de una página:

```markdown
## Tu turno

El dado se tira solo. Las casillas a las que puedes ir laten: haz clic en una.

Al llegar, la casilla hace lo suyo: una Fuente te da su runa, un Nodo te deja retar, consultar o saltar.

![](T_Ayuda_Turno)

> Tira. Luego ya veremos si te sirve de algo.
```

Ya no hay una lista fija de títulos exigidos: el autor decide cuántas páginas hay, cómo se llaman y en qué orden.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-HUD-17 (nueva, sustituye la carga de S-022). El texto se lee como páginas.** El cargador devuelve una lista de páginas, cada una con su título, su texto, su comentario del Warden y el nombre de su imagen (los dos últimos pueden faltar). Es una función pura, sin Slate, con test.
  - El archivo vale si tiene al menos una página y cada página tiene título y texto. Si falta el archivo, no hay ninguna página o una página no tiene texto, el cargador lo dice con un mensaje que nombra el archivo y la página, como hoy (`FHowToPlayLoadResult`); nunca lanza.
  - El cargador devuelve también el título del libro y la frase de la portada, vacíos si el archivo no los trae.
  - Desaparece la lista de títulos exigidos (`HowToPlayHeadings`).
  - El nombre de la imagen se resuelve en una carpeta fija de contenido (nombre a proponer; por ejemplo `/Game/Art/Ayuda/`). Si la imagen no existe, la página sale sin imagen y se dice una vez en el registro: no es un error.
- **R-HUD-18 (nueva). La pestaña "Cómo se juega" del Grimorio es un libro.** En lugar del texto que se desplaza (S-044), la pestaña enseña una página cada vez:
  - el título; el texto a la izquierda y la imagen a la derecha (si no caben lado a lado, la imagen debajo del texto); y abajo, el comentario del Warden con el color de su línea;
  - al pie, "Anterior", "N de M" y "Siguiente". "Anterior" no hace nada en la primera página ni "Siguiente" en la última (no es cíclico: un libro tiene principio y final);
  - se pasa de página con esos dos botones y con el teclado (teclas a proponer en el plan, distintas de las que cambian de pestaña);
  - al volver a la pestaña durante la misma partida se ve la página en la que se dejó; en una partida nueva, la primera.
  - **La última página es "Teclas" y la pone el código**, con las teclas configuradas, para que nunca se contradiga con la configuración. El autor no la escribe.
  - Si el archivo no carga, la pestaña enseña el mensaje del cargador.
- **R-HUD-19 (nueva). El menú lee las mismas páginas.** La librería que ya sirve el texto al menú (`UHowToPlayLibrary`) ofrece también las páginas: cuántas hay y, de cada una, su título, su texto, su comentario y su imagen, incluida la de teclas. También el título del libro y la frase de la portada. Con eso el autor monta en el menú la pantalla que ha decidido (17/11/2026): el libro cerrado arriba, con su portada, y el botón "Jugar" debajo; el libro se abre al pulsarlo. Ese widget es un asset suyo y no es de esta story. Dentro de la partida el Grimorio se abre directamente, sin portada.
- **R-HUD-20 (nueva, añadida por el autor el 17/11/2026). El fondo del libro.** El Grimorio, en todas sus pestañas, se pinta sobre una imagen de libro abierto en vez de sobre el fondo negro liso. La imagen es una textura opcional (ruta en Config, nombre a proponer; por ejemplo `GrimoireBackgroundTexture`, valor inicial `/Game/Art/Grimorio/T_GrimorioFondo.T_GrimorioFondo`), que se estira al tamaño del panel conservando sus bordes (los márgenes del borde en Config, como fracción de la imagen), de modo que el lomo y las esquinas no se deformen. Las páginas de la imagen son oscuras, así que los colores de los textos de hoy no cambian. El contenido de las pestañas se coloca dentro de las páginas, con un margen que no pisa el borde del libro. Ningún texto cruza el lomo: el contenido se reparte en dos columnas, una por página (en "Cómo se juega", el texto en la izquierda y la imagen en la derecha). Si la ruta está vacía o la textura no existe, el Grimorio se pinta con el fondo de hoy y se dice una vez en el registro.
- **Lo que no cambia.** El archivo sigue en `Content/Data/Textos/` y se empaqueta como archivo suelto. Las demás pestañas del Grimorio. Ninguna regla.

## Assets (los crea el autor en el editor; commit aparte)

- `Content/Art/Grimorio/T_GrimorioPortada`: la portada del libro cerrado, sin texto (el título lo escribe el juego), generada con IA, placeholder. La usa el widget del menú, no el código de esta story.
- `Content/Art/Grimorio/T_GrimorioFondo`: la imagen del libro abierto, con las páginas oscuras y vacías, generada con IA, placeholder.
- Una textura por página que lleve imagen, en la carpeta de la ayuda, con el nombre que diga el archivo (por ejemplo `T_Ayuda_Turno`): capturas del propio juego, recortadas a lo que explica la página.
- El agente no crea ni toca estos assets. El PR los describe en texto y el autor los verifica en el editor antes de fusionar.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-HUD-30** (R-HUD-17, páginas). Un texto con tres títulos de segundo nivel da tres páginas, en orden, cada una con su título y su texto; los párrafos de una página se conservan como párrafos.
- **CA-HUD-31** (R-HUD-17, comentario e imagen). Una página con una línea `>` lleva ese texto como comentario del Warden, sin el signo, y no en el texto de la regla. Una página con `![](T_Ayuda_Turno)` lleva ese nombre de imagen, y la línea no aparece en el texto. Sin esas líneas, comentario e imagen vacíos.
- **CA-HUD-37** (R-HUD-17, portada). Un archivo que empieza por un título de primer nivel y una línea `>` da ese título y esa frase como portada, y ninguno de los dos aparece en las páginas; sin ellos, la portada queda vacía y las páginas son las mismas.
- **CA-HUD-32** (R-HUD-17, errores). Sin archivo, el error es el de archivo no encontrado. Un archivo sin ningún título, o con una página sin texto, da un error cuyo mensaje nombra el archivo y, en el segundo caso, el título de la página. Ningún caso lanza.
- **CA-HUD-33** (R-HUD-18, pasar página). Con cuatro páginas del autor hay cinco en el libro, la última "Teclas". Se empieza en la primera; "Siguiente" avanza una y se queda en la última; "Anterior" retrocede una y se queda en la primera; el pie dice "N de M" con la página y el total.
- **CA-HUD-34** (R-HUD-18, teclas y recuerdo). La página "Teclas" lleva las teclas de la vista con el nombre de las configuradas. Al cerrar y abrir el Grimorio y volver a la pestaña, la página es la que se dejó.
- **CA-HUD-36** (R-HUD-20). La función que da la zona de contenido dentro del libro devuelve, para un panel dado y unos márgenes de borde, un rectángulo que queda dentro del panel y fuera de los bordes; con la textura ausente, la zona es la de hoy.
- **CA-HUD-35** (R-HUD-19). La librería devuelve el mismo número de páginas y los mismos textos que ve el Grimorio, con la de teclas al final; con el archivo roto, cero páginas y el mensaje del cargador.

Manuales, por el autor, en PIE, a 1920x1080 y a 1024x768, con captura:

- La pestaña "Cómo se juega" enseña una página, con su título, su texto, su imagen y el comentario del Warden en naranja; se pasa con los botones y con el teclado; el pie dice por cuál va.
- A 1024x768 la página cabe; si texto e imagen no entran lado a lado, la imagen va debajo.
- Una página sin imagen se ve bien, sin hueco raro. Una página con un nombre de imagen que no existe sale sin imagen y el registro lo dice una vez.
- La última página lista las teclas reales.
- Con la imagen del libro importada, el Grimorio se ve como un libro abierto en las cinco pestañas, los textos se leen sobre las páginas y nada pisa el borde; a 1024x768 el borde no se deforma. Sin la imagen (ruta vacía en Config), se ve como antes.
- Si se rompe el archivo a propósito (se le quita un título), la pestaña enseña el mensaje y el juego no falla.

## Notas para quien lo implemente

- **Lo que hay.** `HowToPlay.h/.cpp` carga el texto y comprueba los títulos (`HowToPlayHeadings`, que desaparece); `UHowToPlayLibrary` lo prepara para el bloque de texto del menú. La pestaña del Grimorio es de S-044 (`GrimoireBook`, `GrimoireHowToPlayText`).
- **El texto actual del archivo** tiene cuatro títulos y ni comentarios ni imágenes: con el formato nuevo da cuatro páginas válidas. El autor lo reescribe aparte; esta story no toca el contenido del archivo.
- **Las imágenes** se cargan como las demás texturas opcionales de la vista (la runa del anillo, el dado): ruta, textura mantenida viva, aviso una vez si falta.
- **El menú.** El widget de "Cómo se juega" del menú es de la plantilla y lo monta el autor en el editor: no se toca desde código. La librería solo le da los datos.
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Tamaños, en Config con su valor inicial documentado. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Precisiones del autor al aprobar el plan (17/11/2026)

- **R-HUD-17, el formato** (con sus casos en los tests de CA-HUD-31 y CA-HUD-32):
  - Varias líneas `>` seguidas son un solo comentario del Warden.
  - Un segundo comentario o una segunda imagen separados en la misma página son un error cuyo mensaje nombra el archivo y la página.
  - El texto suelto antes de la primera `##` que no sea el título ni la frase de la portada se ignora.
  - Las líneas de un mismo párrafo se unen con un espacio; los párrafos se separan con una línea en blanco.
  - `![texto](Nombre)` también vale; el texto alternativo se ignora.
  - La imagen se busca en `/Game/Art/Ayuda/` (`HowToPlayImageFolder`, Config): `T_Ayuda_Turno` es `/Game/Art/Ayuda/T_Ayuda_Turno.T_Ayuda_Turno`.
- **R-HUD-18, las teclas**: flecha arriba, página anterior; flecha abajo, página siguiente (`GrimoirePreviousPageKey`, `GrimoireNextPageKey`, Config). Solo actúan con el Grimorio abierto en "Cómo se juega". Las lista la página "Teclas".
- **R-HUD-20, nada cruza el lomo**:
  - La cabecera se reparte: el título y "Tú · La mesa · El tablero" en la página izquierda; "Cómo se juega · Crónica" en la derecha.
  - Las dos columnas de cada pestaña: "Tú", la tarjeta a la izquierda y la insolencia, el hogar y los poderes a la derecha; "La mesa", Jugadores a la izquierda y Dominios a la derecha; "El tablero", Casillas y Nichos a la izquierda y Tablero y destellos a la derecha; "Crónica", entera en la izquierda.
  - En "Cómo se juega", el comentario del Warden va al pie de la página izquierda, bajo la regla; "Anterior" abajo en la izquierda y "N de M · Siguiente" abajo en la derecha. Sin la imagen del libro, el comentario va a todo el ancho y el pie, centrado.
- **R-HUD-19, el menú de hoy**: `UHowToPlayLibrary::HowToPlayText()` se mantiene para el widget actual del menú, con el libro aplanado (título y texto de cada página), y un `TODO(R-HUD-19)` para quitarlo cuando el widget nuevo lea las páginas.
- **El texto del archivo** no se toca: su párrafo de teclas (nombra H y P, que ya no existen, y repite la página "Teclas") queda como pendiente en el PR.

## Correcciones del autor tras PIE (17/11/2026)

Probado en PIE con `T_GrimorioFondo` importado. Superado: sin imagen se ve como antes; el libro por páginas con sus botones, las flechas arriba y abajo, "N de M", la página de teclas y el recuerdo de la página; con la imagen, el libro abierto en las cinco pestañas, las pestañas repartidas, nada cruza el lomo ni pisa el borde. Los márgenes iniciales valen. Pendiente de probar: las páginas con imagen y con comentario del Warden, cuando el autor tenga el texto y las capturas.

- **Corrección 1 (R-HUD-20), el ancho del texto.** En "Tú", el texto de la página derecha (los poderes) se cortaba por el borde derecho: se partía a un ancho mayor que el de la columna (no restaba su sangría ni la barra de desplazamiento). Todo el texto de las dos páginas se parte al ancho de su columna, en las cinco pestañas: el de su propia columna, menos lo que reserva la barra de desplazamiento y menos la sangría de la línea. **CA-HUD-39**: para cada columna del libro (y la única sin él) y cada sangría, el ancho de partir más la sangría y la reserva de la barra no pasa del ancho de la columna; cada página usa el suyo.
- **Corrección 2 (R-HUD-20), el título.** Ocupaba tres líneas. Dice solo "Grimorio"; las teclas (cerrar, pestañas, páginas) van en una línea pequeña al pie del libro, sin cruzar el lomo: cerrar y pestañas al pie de la página izquierda; páginas al pie de la derecha. Sin el fondo del libro, la misma línea entera al pie del panel. **CA-HUD-40**: el título es "Grimorio"; la línea del pie dice, a la izquierda, "G para cerrar; flechas izquierda y derecha, pestañas" y, a la derecha, "flechas arriba y abajo, páginas", con los nombres de las teclas configuradas. *Ajustado en la segunda prueba en PIE (corrección 5, abajo): la parte de páginas, solo en "Cómo se juega".*
- **Corrección 3 (R-HUD-20), sin cajas oscuras sobre el libro.** Con el fondo del libro, fuera la caja negra de "Crónica" y el fondo de la tarjeta de "Tú": el texto va directamente sobre la página. Sin el fondo del libro, como hoy. Se comprueba en PIE.
- **Corrección 4 (R-HUD-20), la fuente del libro.** Opcional, con el patrón de las imágenes: la ruta de un asset de fuente en Config (`GrimoireFont`, valor inicial `/Game/Art/Fuentes/F_Grimorio.F_Grimorio`) con sus tipos normal y negrita (los nombres de los tipos dentro del asset, también en Config, propuestos por el agente: `GrimoireFontRegularTypeface` y `GrimoireFontBoldTypeface`, valores iniciales `Regular` y `Bold`); si la ruta está vacía o el asset no existe, la fuente de hoy y un aviso una vez en el registro. Se usa en todo el texto del Grimorio. El asset lo importa el autor. Los tamaños no se tocan: se ajustan con el texto final. **CA-HUD-41**: sin fuente, la letra es la de hoy, con el tipo y el tamaño pedidos; con fuente, esa fuente, con su tipo normal o negrita y el tamaño pedido.

## Correcciones del autor tras la segunda prueba en PIE (17/11/2026)

Superadas en PIE las cuatro correcciones anteriores, y la fuente: `F_Grimorio` importado con los tipos `Regular` y `Bold`, el texto sale en la fuente del libro y la negrita se distingue. Decisiones sobre lo que quedó abierto: "La partida sigue" (lo decía el título antiguo) se queda fuera; el título y el pie nuevos también sin el libro, bien. Los tamaños de letra siguen sin tocarse.

- **Corrección 5 (R-HUD-20), las teclas de páginas solo donde hay páginas.** La parte de páginas del pie ("flechas arriba y abajo, páginas") solo se enseña en la pestaña "Cómo se juega", la única con páginas; en las demás, el pie de la página derecha queda vacío. Sin el fondo del libro, la línea única lleva la parte de páginas solo en esa pestaña. **CA-HUD-40, ajustado**: el título es "Grimorio"; al pie de la izquierda, "G para cerrar; flechas izquierda y derecha, pestañas" en las cinco pestañas; al pie de la derecha, "flechas arriba y abajo, páginas" en "Cómo se juega" y nada en las otras cuatro; la línea única sin el libro es "G para cerrar; flechas izquierda y derecha, pestañas; flechas arriba y abajo, páginas" en "Cómo se juega" y solo la parte izquierda en las otras; con los nombres de las teclas configuradas.
- **Corrección 6 (R-HUD-20), las tildes.** El encabezado de la Crónica decía "CRONICA (la ultima arriba; rueda para ir al inicio)"; dice "CRÓNICA (la última arriba; rueda para ir al inicio)". Revisados los demás textos fijos del Grimorio (pestañas, título, pie, "Tú", leyendas, tarjetas, página de teclas, errores de "Cómo se juega") y sus datos (`grimorio.json`, `elegidos.json`, `como-se-juega.md`): no hay más sin tildes. **CA-HUD-42**: el encabezado de la Crónica es "CRÓNICA (la última arriba; rueda para ir al inicio)".
- **Corrección 7 (R-HUD-20), aire sobre el pie.** En "Crónica", la última línea visible quedaba pegada a la línea de teclas del pie. Entre el final de la zona de contenido y el pie hay un pequeño margen, el mismo en las cinco pestañas, con y sin el fondo del libro: una constante documentada (`GrimoireFootGap`, nombre propuesto, valor inicial 12 unidades de Slate; antes, 6), como las de la barra de desplazamiento. Se comprueba en PIE.

## Ampliación del autor: el formato del texto real (17/11/2026, noche)

Superadas en PIE las tres correcciones de la segunda prueba. El autor ha escrito el texto del libro y no cabe en el formato: son 27 páginas, casi todo en voz del Warden, con varias frases suyas por página, listas y negritas. Se amplía el formato. El plan se aprobó el mismo día, con las recomendaciones del agente en sus siete dudas y las precisiones que siguen.

- **R-HUD-17, precisión: una página es su título y una lista ordenada de bloques.** Tres clases de bloque (`EHowToPlayBlockKind`, nombre propuesto): **párrafo**, como hoy; **frase del Warden**, una o varias líneas `>` seguidas, sin línea en blanco entre ellas (dos bloques `>` separados por una línea en blanco son dos frases distintas, cada una en su bloque), los que se quiera, en cualquier punto de la página, en su orden; **elemento de lista**, cada línea que empieza por `- `, una sola línea, sin el guion. Una línea sin `- ` justo después de un elemento de lista empieza un párrafo nuevo. Dentro de cualquier bloque, `**texto**` es negrita; solo con dos asteriscos (uno suelto es texto) y unos asteriscos sin cerrar se dejan como texto; ni en el título de la página ni en la portada. No hay cursiva dentro del texto, ni tablas, ni subtítulos. Una página vale si tiene título y al menos un bloque de cualquier clase: una página con solo frases del Warden es válida. Desaparece el error de "dos comentarios". La imagen, como hoy: una como mucho. La portada no cambia. El campo del comentario de la página (`WardenComment`) desaparece: las frases van en sus bloques. El texto sin marcas de la página (`Body`) sigue: todos los bloques en su orden, sin asteriscos, las frases del Warden incluidas y los elementos de lista con su marca.
- **R-HUD-18 y R-HUD-20, precisión: cómo se ve.** En la página izquierda, los bloques en su orden, dentro de la zona que se desplaza: el párrafo, con el color y el tipo normal de hoy; la frase del Warden, con el color de su línea y en cursiva; los elementos de lista, cada uno en su línea con una marca delante y seguidos sin el hueco de párrafo; la negrita, en negrita, y dentro de una frase del Warden, negrita cursiva. Desaparece el comentario fijo al pie de la página. Valores iniciales propuestos por el agente, que se ajustan en PIE: la marca "•" con una sangría de 12, 2 de separación entre elementos de lista y 8 entre los demás bloques. La página "Teclas" pasa a ser una lista, una tecla por elemento.
- **Corrección 4, precisión: la fuente.** Dos tipos más en Config, con el patrón de los que hay: cursiva y negrita cursiva (`GrimoireFontItalicTypeface` y `GrimoireFontBoldItalicTypeface`, nombres propuestos, valores iniciales `Italic` y `BoldItalic`). Sin la fuente del libro, los tipos `Italic` y `BoldItalic` de la fuente de siempre. El autor añade esos dos tipos a `F_Grimorio`.
- **R-HUD-19, precisión: el menú.** La librería ofrece, además de lo que ya da, cada página como texto enriquecido para un bloque de texto enriquecido de UMG (`HowToPlayPageRichText`, nombre propuesto): una sola cadena con las marcas `<Bold>`, `<Warden>` y `<WardenBold>` (el texto enriquecido de UMG no anida marcas; son los nombres de las filas de la tabla de estilos del autor), los párrafos separados por una línea en blanco, los elementos de lista uno por línea con "• " delante, y `& < > "` escapados como `&amp; &lt; &gt; &quot;`. Lo que ya ofrece la librería y `HowToPlayText` se mantienen, con el texto sin marcas; el comentario de la página desaparece con su campo. Se comprueba en el menú.
- **Orden**: primero bloques, frases del Warden y listas, en sus commits; después la negrita, en commits aparte. Si la negrita no sale limpia, se para ahí, con los asteriscos quitados del texto.

Criterios, automáticos y sin mundo:

- **CA-HUD-43** (R-HUD-17, bloques). Una página con párrafo, frase del Warden, párrafo y otra frase del Warden da cuatro bloques en ese orden, cada uno con su clase; ya no es error. Dos bloques `>` separados por una línea en blanco son dos frases; dos líneas `>` seguidas, una. Una página con solo frases del Warden es válida. Ajusta CA-HUD-31 (ya no hay comentario de la página) y CA-HUD-32 (ya no hay error de dos comentarios; una página sin ningún bloque sigue siendo un error).
- **CA-HUD-44** (R-HUD-17, listas). Tres líneas `- ` seguidas dan tres bloques de lista, sin el guion en el texto; una línea `- ` entre dos párrafos no se une a ninguno.
- **CA-HUD-45** (R-HUD-17, negrita). "uno **dos** tres" da tres tramos, el segundo en negrita y sin asteriscos; unos asteriscos sin cerrar se dejan como texto. Vale en las tres clases de bloque.
- **CA-HUD-46** (R-HUD-19, texto enriquecido). La librería da para esa página el texto enriquecido con sus marcas, y el texto sin marcas donde ya lo daba.
- **CA-HUD-47** (corrección 4, cursiva). La fuente del Grimorio da el tipo cursiva para la frase del Warden y negrita cursiva para su negrita; sin fuente del libro, los de la fuente de siempre.

## Segunda ampliación del autor: el libro a doble página (17/11/2026, noche)

Superado en PIE el formato ampliado: la fuente con sus cuatro tipos, las frases del Warden en cursiva, las listas, la negrita y las imágenes con transparencia. Al ver el texto real, el autor decide que el libro se lea a doble página: la hoja derecha solo llevaba la imagen y muchas páginas quedaban medio vacías. Es un cambio de maquetación, no de reglas. El plan se aprobó el mismo día, con las recomendaciones del agente en sus siete dudas y la corrección del autor sobre la marca de agua (en las dos hojas).

- **R-HUD-18 y R-HUD-20, precisión: la doble página.** Con la imagen del libro se ven dos páginas a la vez: la hoja izquierda enseña una página y la derecha, la siguiente; la primera vista son las páginas 1 y 2. Cada hoja pinta lo suyo, su título (si lo tiene) y sus bloques en orden, y se desplaza por su cuenta si no cabe. "Siguiente" y "Anterior", y las flechas arriba y abajo, pasan dos páginas, sin ciclo. El pie dice "1-2 de 30"; si el total es impar, la última vista lleva la última página a la izquierda, la derecha vacía, y el pie dice "29 de 29". La página que se recuerda durante la partida es la vista en que se dejó (su primera página). La página "Teclas" sigue siendo la última, caiga en la hoja que caiga. Sin la imagen del libro, una página cada vez, como hasta ahora (`HowToPlayPagesPerView`, nombre propuesto: 2 con el libro y 1 sin él).
- **R-HUD-17 y R-HUD-18, precisión: la imagen es un bloque.** Una línea `![](Nombre)` es un bloque de imagen (`Image`, con su nombre en el campo `ImageName` del bloque) y va donde esté escrita; puede haber varias por página. Desaparecen el error de "dos imágenes" (`DuplicateImage`) y el campo de la imagen de la página. Con el libro y sin él, la imagen va en su sitio entre los bloques (desaparece la regla de "al lado del texto si cabe", con `HowToPlayImageBeside` y `HowToPlayMinTextWidth`). Se pinta centrada, sin deformarse, al mayor tamaño que quepa en el ancho de la hoja y en un alto máximo, agrandándose si es pequeña: el alto máximo es una fracción del alto de la hoja (`HowToPlayImageMaxHeight`, Config, valor inicial 0,6); una página cuyo único bloque es una imagen la pinta a hoja entera, sin ese límite. Si la textura no existe, el bloque no se pinta y se dice una vez en el registro. El texto sin marcas y el texto enriquecido no llevan las imágenes; el menú lee la página como lista de bloques (`HowToPlayPage(Index).Blocks`), con el texto enriquecido de cada bloque (`HowToPlayBlockRichText`) y la textura de cada imagen por su nombre (`HowToPlayImage`, que sustituye a `HowToPlayPageImage`).
- **R-HUD-20, nueva: la marca de agua.** En "Cómo se juega", con la imagen del libro, cada hoja, la izquierda y la derecha, lleva su marca de agua detrás del texto y de los bloques: centrada en su hoja, apagada, siempre, lleve la página imágenes o no, también en la hoja derecha vacía de la última vista impar. La misma textura, opacidad y tamaño en las dos. Es una textura opcional, con el patrón de las demás: ruta en Config (`HowToPlayWatermarkTexture`, valor inicial `/Game/Art/Ayuda/T_Ayuda_Sello.T_Ayuda_Sello`), su opacidad (`HowToPlayWatermarkOpacity`, valor inicial 0,25) y su tamaño como fracción de la hoja (`HowToPlayWatermarkSize`, valor inicial 0,7: la textura cabe, sin deformarse, en esa fracción del ancho y del alto de la hoja). Vacía o inexistente, no se pinta y se dice una vez en el registro. No estorba al clic ni al desplazamiento.
- **R-HUD-17, precisión: el formato.** Una clase de bloque más, el subtítulo (`Subtitle`): una línea que empieza por `### `. Se pinta como un título pequeño dentro de la página: en negrita, con el dorado del título, al tamaño del texto y con 4 más de hueco encima (valores iniciales propuestos por el agente). Los `**` dentro de un subtítulo se quitan y todo va con su estilo. En el texto enriquecido del menú lleva su marca, `<Subtitle>`; en el texto sin marcas, sale como una línea. Una página vale si tiene al menos un bloque de cualquier clase, imagen incluida: una página con solo una imagen es válida (una lámina a hoja entera). Una línea `##` sin texto empieza una página sin título, que no pinta título. Sigue siendo error la página sin ningún bloque.
- **Lo que no cambia**: las otras cuatro pestañas, la portada y los tamaños de letra. `como-se-juega.md` no se toca: el autor lo reparte de nuevo para la doble página y quita las líneas `![](T_Ayuda_Sello)`, porque el sello pasa a ser la marca de agua.

Criterios, automáticos y sin mundo:

- **CA-HUD-53** (R-HUD-17, subtítulo). `### Texto` da un bloque de subtítulo con ese texto, sin las almohadillas, en su orden; lleva su marca en el texto enriquecido y sale como una línea en el texto sin marcas.
- **CA-HUD-54** (R-HUD-18, doble página). Con cinco páginas y el libro, las vistas son (1, 2), (3, 4) y (5, nada). "Siguiente" desde la primera lleva a 3 y 4 y se queda en la última; "Anterior", al revés, se queda en la primera. El pie dice "1-2 de 5", "3-4 de 5" y "5 de 5". Sin el libro, de una en una. Ajusta CA-HUD-33 y CA-HUD-34.
- **CA-HUD-55** (R-HUD-17, imágenes). Una página con párrafo, imagen, párrafo e imagen da cuatro bloques en ese orden, dos de imagen con su nombre; ya no es error. Una página con solo una imagen es válida; `##` sola da una página con el título vacío; una página sin bloques sigue dando el error que nombra el archivo. Ajusta CA-HUD-31 y CA-HUD-32.
- **CA-HUD-56** (R-HUD-18, tamaño de la imagen). La función que da el tamaño de una imagen dentro de una hoja devuelve, para el ancho de la hoja, el alto máximo y las proporciones de la imagen, un tamaño que cabe en los dos límites y conserva las proporciones.
- **CA-HUD-57** (R-HUD-20, marca de agua; corrección del autor: en las dos hojas). La función que da el sitio de la marca de agua devuelve un rectángulo por hoja, cada uno centrado en la suya y dentro de ella, con la fracción de tamaño dada; sin textura o sin el libro, ninguna marca.

## Ronda de maquetación del Grimorio (18/11/2026)

Superado en PIE el libro a doble página con el texto real, las siete láminas y la marca de agua. Quedan cinco cambios en el resto del Grimorio, que cambian sus pestañas. El plan se aprobó el mismo día con estas decisiones del autor: la cabecera con el libro lleva "Grimorio" en la hoja izquierda y las tres pestañas en la derecha, a la misma altura; los nombres propuestos por el agente, aceptados; la línea del jugador humano en "Jugadores" es como la de los demás; las muestras de "Jugadores", una debajo de otra, sin columnas; la descripción de cada poder, debajo de su runa, sangrada y en gris, al tamaño de siempre.

- **R-HUD-10, precisión: tres pestañas.** Fuera la pestaña "El tablero": su leyenda la explican el libro y la lámina rotulada del tablero; las funciones que solo la servían se quitan. "Tú" y "La mesa" se juntan en una sola pestaña, "La mesa", que es la primera y la que se abre con G. Quedan tres, en este orden: "La mesa", "Cómo se juega" y "Crónica"; las flechas pasan de una a otra de forma cíclica.
- **R-HUD-10, precisión: "La mesa".** Hoja izquierda, el jugador (la hoja del jugador, `GrimoirePlayerSheet`, nombre propuesto): su asiento y su papel (Eco, o su Elegido y Dominio), su afinidad, su insolencia ("Insolencia: N de 5"), sus seis runas y sus poderes activos. Cada runa lleva su cuadrito de color, cuántas tiene, el poder que le da y, debajo, sangrada y en gris, qué hace ese poder en una frase (la de `grimorio.json`, `PowerText` de la runa). Una runa que no le da poder no lleva descripción; un Eco tras la carrera lleva en la runa de su hogar Celeridad con su descripción (R-ECO-7). Desaparecen el bloque "PODERES" y la línea aparte de la runa del hogar. Hoja derecha, la mesa: "Jugadores", uno por línea, una muestra debajo de otra con el cuadrito y el texto empezando a la misma altura, y debajo los seis Dominios con su Elegido y su estado, como hoy. Sin la imagen del libro, una columna: primero el jugador y debajo la mesa.
- **R-HUD-10, precisión: las líneas de "Jugadores".** El encabezado dice solo "Jugadores". Cada jugador, en una línea: asiento, papel, sus marcas públicas y "N runas" (sin "Runas: "). La del jugador humano es como la de los demás, con "(tú)": sus tipos, sus poderes y su insolencia ya están en la hoja izquierda y no se repiten. Lo que se enseña de los demás no cambia: los filtros de R-HUD-6 y R-RUN-7 siguen igual (con su Brazal revelado, sus tipos).
- **R-HUD-18, precisión: la lámina a hoja entera.** Una página cuyo único bloque es una imagen la pinta centrada en horizontal y en vertical dentro de la hoja, al mayor tamaño que cabe en el ancho y el alto que hay de verdad en la hoja, sin deformarse y sin barra de desplazamiento (`HowToPlayFullSheetImage`, nombre propuesto).
- **Lo que no cambia**: el libro, la Crónica, las reglas y los tamaños de letra. `como-se-juega.md` y los assets no se tocan.

Criterios, automáticos y sin mundo:

- **CA-HUD-58** (R-HUD-10). Las pestañas son tres, en este orden: "La mesa", "Cómo se juega" y "Crónica"; al abrir, "La mesa"; siguiente y anterior son cíclicas entre las tres. Ajusta CA-HUD-18 y CA-HUD-34.
- **CA-HUD-59** (R-HUD-10, R-ECO-7). Las filas del jugador llevan, por cada runa que le da un poder, el nombre del poder y su descripción; una runa sin poder no lleva descripción; un Eco con la runa de su hogar lleva Celeridad con su descripción. La insolencia va en la hoja del jugador. Sustituye a CA-HUD-19.
- **CA-HUD-60** (R-HUD-10, R-HUD-6, R-RUN-7). Las filas de la mesa son una por jugador, con "N runas", bajo el encabezado "Jugadores"; lo que se oculta de los demás es lo mismo que antes. Ajusta CA-HUD-20; CA-HUD-21 pierde la parte del tablero.
- **CA-HUD-61** (R-HUD-18). La función que da el sitio de una imagen a hoja entera devuelve un rectángulo dentro de la hoja, centrado en horizontal y en vertical, con las proporciones de la imagen, para una imagen apaisada y para una vertical.

## Correcciones tras probar la maquetación (18/11/2026)

Superado en PIE: las tres pestañas, "La mesa" con sus dos hojas, la cabecera y la lámina a hoja entera centrada y sin barra. El autor sube antes el capítulo 9 en una sola hoja (`9fcf79a`; el libro queda en 29 páginas suyas más la de teclas). Quedan tres defectos. Plan aprobado el mismo día, con estas decisiones del autor: el marco de un píxel también en las pestañas; el hueco bajo la cabecera, 16.

- **R-HUD-18 y R-HUD-20, precisión: las proporciones de una imagen del libro son siempre las de su textura**, también en PIE con el editor recién abierto. En el editor, mientras una textura se compila, `GetSizeX` y `GetSizeY` devuelven el tamaño de la textura de relleno, cuadrada (comprobado en el código de la 5.8, `UTexture2D::GetSizeX` e `IsDefaultTexture`); el pincel se quedaba con ese tamaño y las láminas salían cuadradas y con barra. El tamaño se lee de la fuente de la textura (`GetImportedSize`, también en una build empaquetada) y, si no tiene, del de siempre (`GrimoireTextureSize`, nombre propuesto). Vale para las láminas, la imagen del libro y la marca de agua. La runa del anillo y el dado no usan ese tamaño para pintarse y no cambian.
- **R-HUD-20, precisión: hueco bajo la cabecera.** Entre la fila del título y las pestañas y el contenido de las hojas, 16 (`GrimoireHeaderGap`, nombre propuesto; antes, 8), el mismo en las tres pestañas, con el libro y sin él. Sin el libro, el hueco entre el título y las pestañas no cambia.
- **R-HUD-18 y R-HUD-10, precisión: los marcos se ven enteros.** El marco de los botones "Anterior" y "Siguiente" y el de las pestañas mide al menos un píxel de pantalla, con cualquier escala (`GrimoireFrameWidth`, nombre propuesto): con una escala menor que 1, un marco de una unidad medía menos de un píxel y un lado podía no pintarse. Hipótesis del agente, sin verificar en el editor; se comprueba en PIE.

Criterios:

- **CA-HUD-62** (R-HUD-18, R-HUD-20). El tamaño de una textura del libro es el de su fuente aunque sus datos derivados digan otro (una fuente de 1642 x 717 con datos de 64 x 64 da 1642 x 717); sin fuente, el de sus datos. No cubre el momento real de la compilación asíncrona del editor, que no se puede provocar en un test: comprobación manual en "Notas para la prueba".
- **CA-HUD-63** (R-HUD-18, R-HUD-10). El grosor del marco, en unidades, es 1 con escala 1 o mayor, 1,5 con escala 0,667 y 1 con una escala no válida (0 o negativa).
- El hueco de la cabecera no lleva test: es aspecto. Se comprueba en PIE.

Notas para la prueba (manual, en PIE):

1. Cerrar el editor y abrirlo sin abrir ninguna textura; PIE, G, "Cómo se juega": Carrera y Moverse, apaisadas y sin barra; el Anillo, vertical a hoja entera; la imagen del libro y la marca de agua, sin deformar. Reiniciar PIE y repetir.
2. El hueco bajo la cabecera, en las tres pestañas, con el libro y sin él.
3. Los cuatro bordes de "Anterior", "Siguiente" y las pestañas, con el libro y sin él, en ventana pequeña y a pantalla completa.

## Retoques tras probar las correcciones (18/11/2026)

Superadas en PIE las tres correcciones: las láminas con su proporción con el editor recién abierto, el hueco bajo la cabecera y los bordes de botones y pestañas. Quedan dos retoques. Plan aprobado el mismo día, con estas decisiones del autor: el fondo del Grimorio se queda también los dobles clics; cualquier botón del ratón sigue pulsando, como hasta ahora.

- **R-HUD-18, precisión: los botones de página que no llevan a ningún sitio no se pintan.** En la primera vista de "Cómo se juega" no se ve "Anterior" y en la última no se ve "Siguiente" (antes salían apagados). Conservan su hueco, para que "N de M" y el otro botón no cambien de sitio (`HowToPlayPageButtons`, nombre propuesto). Las flechas no cambian: en la primera y en la última vista, no hacen nada.
- **R-HUD-18 y R-HUD-10, precisión: cada clic cuenta.** Cada clic sobre un botón de página pasa una vista y cada clic sobre una pestaña la elige, también los clics rápidos seguidos. Slate entrega el segundo clic rápido como doble clic, solo a `OnMouseButtonDoubleClick`, y nadie lo atendía: se perdía uno de cada dos (comprobado en el código de la 5.8, `FSlateApplication::ProcessMouseButtonDoubleClickEvent`). La pulsación y el doble clic llevan a la misma acción (`GrimoireOnPress`, nombre propuesto). El fondo del Grimorio, que se queda las pulsaciones para que ninguna llegue al tablero (S-044), se queda también los dobles clics (decisión del autor).

Criterios:

- **CA-HUD-64** (R-HUD-18). Con 30 páginas y el libro, la primera vista no tiene "Anterior" y sí "Siguiente", la última tiene "Anterior" y no "Siguiente" y una del medio tiene los dos; lo mismo sin el libro, de una en una; con una sola vista, ninguno.
- **CA-HUD-65** (R-HUD-18, R-HUD-10). Una caja con `GrimoireOnPress` que recibe pulsación, doble clic, pulsación y doble clic hace su acción cuatro veces, y se queda los cuatro eventos. Que los botones, las pestañas y el fondo la usen se lee en el código y se comprueba a mano.

Notas para la prueba (manual, en PIE): en la primera vista no se ve "Anterior" y en la última no se ve "Siguiente", con el libro y sin él, y "N de M" no se mueve; cuatro clics rápidos en "Siguiente" pasan cuatro vistas, y en "Anterior" vuelven cuatro; clics rápidos en las pestañas pasan una por clic; un doble clic sobre el Grimorio no hace nada en el tablero.

## Fuera de esta story

- Escribir o corregir el texto del libro y hacer sus capturas (lo hace el autor).
- Montar el libro en el widget del menú (asset del autor).
- Ayuda que salta sola según el momento, o tutorial jugable.
- Animación de pasar página, sonido o cualquier adorno del libro.
- Traducciones.
