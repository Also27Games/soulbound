# S-049. El libro del menú es el mismo libro: "Cómo se juega" se ve en el menú como en el Grimorio

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible, y pilar 4, el Warden te habla a ti): "Cómo se juega" (S-022, S-048) y Grimorio (S-044, S-048). **Módulos**: `Soulbound` (presentación: `GrimoirePanel`, `GrimoireBook`, `HowToPlayLibrary` y una pieza nueva para UMG). `SoulboundCore` no cambia. **Rama base**: `main`, con S-048 fusionada (PR 76, 18/11/2026). **Tamaño**: pequeño. **Con un asset del autor**: la pantalla de "Cómo se juega" del menú, donde coloca la pieza; no es de esta story.

## Contexto

Desde S-048, "Cómo se juega" es un libro a doble página: 29 hojas del autor más la de teclas, dos por vista (15 vistas), con frases del Warden en su sitio, listas, negrita, subtítulos, láminas (dentro del texto o a hoja entera), el sello como marca de agua en las dos hojas y el fondo de libro abierto. Dentro de la partida se lee en una pestaña del Grimorio, ya maquetada y probada en PIE a 1920x1080 y a 1024x768.

En el menú no. La pantalla de "Cómo se juega" del menú sigue enseñando `HowToPlayText()`: desde S-048, el libro entero aplanado en un solo bloque de texto, unas 2.600 palabras sin formato. Es lo que falló en la prueba del 15/11/2026 con la build de probadores (P4), y ahora más largo. Hay que cambiarlo antes de la build del 21/11/2026.

S-048 dejó preparado un camino: la librería da cada página en texto enriquecido para que el autor monte el libro a mano en UMG. Eso obliga a maquetar por segunda vez lo que el Grimorio ya maqueta, y las dos versiones nunca quedarían iguales.

Decidido por el autor el 17/11/2026:

- El libro del menú es la misma pieza que pinta el Grimorio, no una copia hecha a mano. Lo que se ajuste en una se ve en las dos.
- El autor solo la coloca en la pantalla del menú, con la portada (el libro cerrado arriba) y el botón "Jugar" debajo, que son suyos.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-HUD-21 (nueva). El libro de "Cómo se juega" es una pieza propia.** Lo que hoy pinta la pestaña "Cómo se juega" dentro de `SGrimoirePanel` (las dos hojas de la vista, cada una con su título, sus bloques y sus láminas, la marca de agua, "Anterior", "N de M" y "Siguiente", sobre la imagen del libro si la hay) pasa a ser un widget de Slate propio (nombre a proponer), que el Grimorio usa para esa pestaña.
  - El Grimorio se ve y se comporta como hoy: mismos anchos, márgenes, fuente, estilos, teclas, botones de página ocultos en la primera y en la última vista, clics rápidos que cuentan (`GrimoireOnPress`) y recuerdo de la página. Es un cambio de sitio del código, no de aspecto.
  - Lo que enseña la pieza para una vista (qué hojas, sus bloques, sus láminas, "N de M" y qué botones se ven) lo dan funciones puras con test: las que ya hay (`HowToPlayViewStart`, `HowToPlayPageFooter`, `HowToPlayPageButtons`, `HowToPlayFullSheetImage`, `GrimoireBookLayout`, `GrimoireWatermarks`) y, si hace falta, una nueva a proponer.
- **R-HUD-22 (nueva). La pieza se ofrece para el menú.** Un widget de UMG escrito en C++ (nombre a proponer; por ejemplo `UHowToPlayBookWidget`) que el autor arrastra a su pantalla del menú y que pinta la pieza de R-HUD-21:
  - el libro abierto a doble página, como la pestaña: sobre la imagen del libro si existe, dos hojas por vista con la marca de agua y, al pie, "Anterior", "N de M" y "Siguiente" (ocultos en la primera y en la última vista, guardando su sitio). Sin la imagen del libro, como el Grimorio sin ella;
  - sin pestañas, sin el título "Grimorio" y sin la línea de teclas de cerrar y de pestañas, que en el menú no existen;
  - **los mismos ajustes que la partida, leídos del mismo sitio**: el archivo del texto, la carpeta de las imágenes, la imagen del libro con sus márgenes, el margen de la página y el hueco del lomo, la marca de agua con su opacidad y su tamaño, el alto máximo de una lámina dentro del texto, la fuente con sus cuatro tipos, el color del Warden y los nombres de las teclas salen de la Config de la vista, como ya hace la librería con las teclas. No hay ajustes duplicados para el menú;
  - el tamaño de letra sigue el criterio del Grimorio (a proponer en el plan cómo se obtiene en el menú, donde no hay capa de la vista);
  - empieza en la primera página cada vez que se crea, y ofrece a Blueprint una función para volver a la primera (para cuando el autor abra la portada). No comparte página con el Grimorio de una partida;
  - las imágenes, la fuente y la imagen del libro que cargue las mantiene vivas, y las que falten se dicen una vez en el registro, como en la vista;
  - **el tamaño de una textura se lee con `GrimoireTextureSize`**, nunca con `GetSizeX` ni `GetSizeY`: en el editor, una textura que se está compilando da el tamaño de la de relleno y las láminas salen deformadas (corrección de S-048 del 18/11/2026);
  - si el archivo no carga, enseña el mensaje del cargador, como la pestaña;
  - **las teclas de página**: las teclas de página configuradas (valores iniciales, flecha arriba y flecha abajo) pasan de vista si el widget tiene el foco del teclado. Si eso choca con la navegación del menú de la plantilla, se queda solo con los botones y se dice en el PR; no se pelea con la plantilla.
- **Lo que no cambia.** El formato del archivo, el cargador y las demás pestañas del Grimorio. Las funciones de la librería (`HowToPlayPageRichText` y las demás) se quedan. `HowToPlayText()` se queda con su `TODO(R-HUD-19)` hasta que el autor quite el bloque de texto viejo de su pantalla: quitarlo antes rompería ese asset.

## Assets (los crea el autor en el editor; commit aparte)

- La pantalla de "Cómo se juega" del menú: sustituye el bloque de texto por la pieza, con la portada (`T_GrimorioPortada`, que el autor tiene importada como `T_GrimorioPortada_1024x1387` y renombra en el editor; con el título y la frase que da la librería) y el botón "Jugar". El agente no la crea ni la toca. El PR dice, en texto, cómo se coloca la pieza y qué funciones ofrece a Blueprint.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-HUD-70** (R-HUD-21, guarda). Los tests del libro y del Grimorio que ya existen (CA-HUD-30 a CA-HUD-69) pasan sin cambiar lo que comprueban. La pestaña "Cómo se juega" del Grimorio, construida con la pieza, enseña para una vista los mismos textos que antes: los de sus dos hojas en su orden, "N de M" y los botones que tocan (en la primera vista no se ve "Anterior"; en la última, "Siguiente").
- **CA-HUD-71** (R-HUD-22, ajustes). La función que reúne los ajustes del libro para el menú devuelve los de la Config de la vista: la ruta de la fuente y sus cuatro tipos, la ruta de la imagen del libro y sus márgenes, el margen de la página, el hueco del lomo, la marca de agua con su opacidad y su tamaño, el alto máximo de una lámina, la carpeta de las imágenes, el color del Warden y los nombres de las teclas.
- **CA-HUD-72** (R-HUD-22, páginas). El libro del menú empieza en la primera vista; "Siguiente" avanza una vista (dos hojas) y se queda en la última; "Anterior" retrocede una y se queda en la primera; la función de volver a la primera la deja en la primera desde cualquiera.
- **CA-HUD-73** (R-HUD-22, pie). En el menú, el pie no nombra la tecla de cerrar ni las de pestañas; como mucho, las de página.
- **CA-HUD-74** (R-HUD-22, error). Con el archivo roto, el libro del menú tiene cero páginas y enseña el mensaje del cargador; no falla.

Manuales, por el autor, en el editor y en PIE, a 1920x1080 y a 1024x768, con captura:

- La pieza aparece en la paleta del diseñador de widgets y se puede colocar en la pantalla del menú.
- En el menú, el libro se ve igual que la pestaña "Cómo se juega" del Grimorio: doble página, fondo, fuente, frases del Warden en naranja y cursiva, listas, negrita, subtítulos, láminas con sus proporciones y marca de agua.
- Se pasa de vista con los botones, también con clics rápidos; el pie dice por cuál va; la última hoja lista las teclas reales.
- Al salir de "Cómo se juega" y volver a entrar, o al volver al menú después de una partida, el libro está en la primera página.
- Dentro de la partida, el Grimorio se ve y se comporta como antes de esta story, en sus tres pestañas ("La mesa", "Cómo se juega" y "Crónica").
- Con el archivo roto a propósito, el menú enseña el mensaje y no falla.

## Notas para quien lo implemente

- **Lo que hay.** `SGrimoirePanel::RebuildHowToPlay` arma la vista de "Cómo se juega": cada hoja con `MakeHowToPlaySheet`, la marca de agua con `MakeWatermark`, los botones con `MakePageButton` y `GrimoireOnPress`, los estilos del texto enriquecido con `UpdateHowToPlayStyles` y el reparto en dos hojas con `GrimoireBookLayout`. La vista (`ABoardViewActor`) carga la fuente, la imagen del libro, el sello y las láminas (`LoadHowToPlayImages`), y los mantiene vivos. `UHowToPlayLibrary` ya lee del objeto por defecto de la vista las teclas y la carpeta de imágenes: es el patrón para el resto de los ajustes.
- **No hay ningún widget de UMG en C++ en el proyecto**: este es el primero. El módulo `Soulbound` necesitará depender de `UMG`; es un módulo del motor y se anota en el `Build.cs` con su motivo, como las demás dependencias.
- **El riesgo de esta story es la separación.** Si sacar la pieza obliga a cambiar cómo se pintan las otras pestañas, más allá de darles lo mismo que reciben hoy, se para y se pregunta en el PR.
- **El menú es de la plantilla** (Ultra Game Template) y de un asset del autor: no se toca desde código, ni se lee, ni se versiona. La pieza no depende de nada de la plantilla.
- Lo que no se pueda ver sin el editor se anota para la prueba manual y no se rehace a ciegas. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Ronda de correcciones (19/11/2026, tras la prueba del autor en PIE y en el menú)

El Grimorio se ve y se comporta como antes en sus tres pestañas; el libro funciona en el menú a 1920x1080 y a 1024x768, y las flechas no chocan con la navegación de la plantilla. Cuatro correcciones, en la misma rama y el mismo PR:

- **Las carpetas que la build necesita.** La vista y el libro del menú cargan por ruta, en tiempo de ejecución, la fuente, la imagen del libro, la marca de agua y las láminas; el cocinador no sigue esas rutas, así que sus carpetas van en `DirectoriesToAlwaysCook` de `DefaultGame.ini`, como ya iban `/Game/Art/Tablero` y `/Game/Art/Anillo`: `/Game/Art/Ayuda`, `/Game/Art/Grimorio` y `/Game/Art/Fuentes`. También `/Game/Art/Dados`, la del dado del panel de decisión (`DiceFacesTexture`), que tenía el mismo defecto.
- **R-HUD-11, precisión del autor del 19/11/2026: la pausa cierra el Grimorio.** Al abrirse la pausa con el Grimorio abierto, el Grimorio se cierra; al reanudar no se reabre. El fallo (el Grimorio se quedaba pintado detrás del menú de pausa y la G no lo cerraba hasta reanudar) viene de S-027, no de esta story. El menú de pausa es de la plantilla: no se toca ni se lee; la vista mira si el mundo está en pausa. Si en PIE no se cierra, queda como fallo conocido en el PR. Impedir que se abra durante la pausa no se decide aquí: lo mira el autor en PIE.
- **Documentación de cómo se coloca la pieza** (en el PR), con lo que comprobó el autor: el foco se pide al enseñar el libro, no en Event Construct; un clic fuera del libro le quita el foco; en un Canvas, anclas al centro con tamaño fijo (1100x738, la proporción de la imagen del libro).
- **La frase de la portada** de `Content/Data/Textos/como-se-juega.md` pasa a ser la del autor, en un commit aparte.

Criterios nuevos, automáticos:

- **CA-HUD-75** (R-HUD-22, build). Cada propiedad de Config de la vista que es una ruta de asset con valor (la fuente, la imagen del libro, la marca de agua, la carpeta de las láminas, el dado y las demás) cae dentro de alguna carpeta de `DirectoriesToAlwaysCook`.
- **CA-HUD-76** (R-HUD-11, pausa). La función que vigila la pausa dice que se cierra el Grimorio al entrar en pausa; no lo vuelve a decir mientras sigue la pausa ni al reanudar; y lo dice otra vez en la pausa siguiente.

Manual, por el autor: en partida, con el Grimorio abierto, abrir la pausa lo cierra; al reanudar sigue cerrado y la G lo abre. Y una build empaquetada con fondo, fuente, sello, láminas y dado.

## Fuera de esta story

- La portada, el botón "Jugar" y cualquier otra parte de la pantalla del menú (assets del autor).
- Animación de abrir el libro o de pasar página, y sonido.
- Cambiar el aspecto del libro: tamaños, márgenes y espacios quedaron ajustados en S-048.
- Quitar `HowToPlayText()` y las funciones de la librería que el menú deje de usar (cuando el autor haya cambiado su pantalla).
- Enseñar en el menú las demás pestañas del Grimorio.
