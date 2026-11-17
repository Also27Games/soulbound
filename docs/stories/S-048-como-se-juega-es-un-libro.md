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

## Fuera de esta story

- Escribir o corregir el texto del libro y hacer sus capturas (lo hace el autor).
- Montar el libro en el widget del menú (asset del autor).
- Ayuda que salta sola según el momento, o tutorial jugable.
- Animación de pasar página, sonido o cualquier adorno del libro.
- Traducciones.
