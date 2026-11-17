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
  - Desaparece la lista de títulos exigidos (`HowToPlayHeadings`).
  - El nombre de la imagen se resuelve en una carpeta fija de contenido (nombre a proponer; por ejemplo `/Game/Art/Ayuda/`). Si la imagen no existe, la página sale sin imagen y se dice una vez en el registro: no es un error.
- **R-HUD-18 (nueva). La pestaña "Cómo se juega" del Grimorio es un libro.** En lugar del texto que se desplaza (S-044), la pestaña enseña una página cada vez:
  - el título; el texto a la izquierda y la imagen a la derecha (si no caben lado a lado, la imagen debajo del texto); y abajo, el comentario del Warden con el color de su línea;
  - al pie, "Anterior", "N de M" y "Siguiente". "Anterior" no hace nada en la primera página ni "Siguiente" en la última (no es cíclico: un libro tiene principio y final);
  - se pasa de página con esos dos botones y con el teclado (teclas a proponer en el plan, distintas de las que cambian de pestaña);
  - al volver a la pestaña durante la misma partida se ve la página en la que se dejó; en una partida nueva, la primera.
  - **La última página es "Teclas" y la pone el código**, con las teclas configuradas, para que nunca se contradiga con la configuración. El autor no la escribe.
  - Si el archivo no carga, la pestaña enseña el mensaje del cargador.
- **R-HUD-19 (nueva). El menú lee las mismas páginas.** La librería que ya sirve el texto al menú (`UHowToPlayLibrary`) ofrece también las páginas: cuántas hay y, de cada una, su título, su texto, su comentario y su imagen, incluida la de teclas. Con eso el autor monta el libro en el widget del menú; ese widget es un asset suyo y no es de esta story.
- **R-HUD-20 (nueva, añadida por el autor el 17/11/2026). El fondo del libro.** El Grimorio, en todas sus pestañas, se pinta sobre una imagen de libro abierto en vez de sobre el fondo negro liso. La imagen es una textura opcional (ruta en Config, nombre a proponer; por ejemplo `GrimoireBackgroundTexture`, valor inicial `/Game/Art/Grimorio/T_GrimorioFondo.T_GrimorioFondo`), que se estira al tamaño del panel conservando sus bordes (los márgenes del borde en Config, como fracción de la imagen), de modo que el lomo y las esquinas no se deformen. Las páginas de la imagen son oscuras, así que los colores de los textos de hoy no cambian. El contenido de las pestañas se coloca dentro de las páginas, con un margen que no pisa el borde del libro. Si la ruta está vacía o la textura no existe, el Grimorio se pinta con el fondo de hoy y se dice una vez en el registro.
- **Lo que no cambia.** El archivo sigue en `Content/Data/Textos/` y se empaqueta como archivo suelto. Las demás pestañas del Grimorio. Ninguna regla.

## Assets (los crea el autor en el editor; commit aparte)

- `Content/Art/Grimorio/T_GrimorioFondo`: la imagen del libro abierto, con las páginas oscuras y vacías, generada con IA, placeholder.
- Una textura por página que lleve imagen, en la carpeta de la ayuda, con el nombre que diga el archivo (por ejemplo `T_Ayuda_Turno`): capturas del propio juego, recortadas a lo que explica la página.
- El agente no crea ni toca estos assets. El PR los describe en texto y el autor los verifica en el editor antes de fusionar.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-HUD-30** (R-HUD-17, páginas). Un texto con tres títulos de segundo nivel da tres páginas, en orden, cada una con su título y su texto; los párrafos de una página se conservan como párrafos.
- **CA-HUD-31** (R-HUD-17, comentario e imagen). Una página con una línea `>` lleva ese texto como comentario del Warden, sin el signo, y no en el texto de la regla. Una página con `![](T_Ayuda_Turno)` lleva ese nombre de imagen, y la línea no aparece en el texto. Sin esas líneas, comentario e imagen vacíos.
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

## Fuera de esta story

- Escribir o corregir el texto del libro y hacer sus capturas (lo hace el autor).
- Montar el libro en el widget del menú (asset del autor).
- Ayuda que salta sola según el momento, o tutorial jugable.
- Animación de pasar página, sonido o cualquier adorno del libro.
- Traducciones.
