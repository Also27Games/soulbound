# S-049. El libro del menú es el mismo libro: "Cómo se juega" se ve en el menú como en el Grimorio

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible, y pilar 4, el Warden te habla a ti): "Cómo se juega" (S-022, S-048) y Grimorio (S-044, S-048). **Módulos**: `Soulbound` (presentación: `GrimoirePanel`, `GrimoireBook`, `HowToPlayLibrary` y una pieza nueva para UMG). `SoulboundCore` no cambia. **Rama base**: `main`, con S-048 fusionada. **Tamaño**: pequeño. **Con un asset del autor**: la pantalla de "Cómo se juega" del menú, donde coloca la pieza; no es de esta story.

## Contexto

Desde S-048, "Cómo se juega" es un libro: 29 páginas del autor más la de teclas, con frases del Warden en su sitio, listas, negrita, una imagen por página y el fondo de libro abierto. Dentro de la partida se lee en una pestaña del Grimorio, ya maquetada.

En el menú no. La pantalla de "Cómo se juega" del menú sigue enseñando `HowToPlayText()`: desde S-048, el libro entero aplanado en un solo bloque de texto, unas 2.600 palabras sin formato. Es lo que falló en la prueba del 15/11/2026 con la build de probadores (P4), y ahora más largo. Hay que cambiarlo antes de la build del 21/11/2026.

S-048 dejó preparado un camino: la librería da cada página en texto enriquecido para que el autor monte el libro a mano en UMG. Eso obliga a maquetar por segunda vez lo que el Grimorio ya maqueta, y las dos versiones nunca quedarían iguales.

Decidido por el autor el 17/11/2026:

- El libro del menú es la misma pieza que pinta el Grimorio, no una copia hecha a mano. Lo que se ajuste en una se ve en las dos.
- El autor solo la coloca en la pantalla del menú, con la portada (el libro cerrado arriba) y el botón "Jugar" debajo, que son suyos.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-HUD-21 (nueva). El libro de "Cómo se juega" es una pieza propia.** Lo que hoy pinta la pestaña "Cómo se juega" dentro de `SGrimoirePanel` (el título de la página, sus bloques, la imagen, "Anterior", "N de M" y "Siguiente", sobre la imagen del libro si la hay) pasa a ser un widget de Slate propio (nombre a proponer), que el Grimorio usa para esa pestaña.
  - El Grimorio se ve y se comporta como hoy: mismos anchos, márgenes, fuente, estilos, teclas y recuerdo de la página. Es un cambio de sitio del código, no de aspecto.
  - Lo que enseña la pieza para una página (título, bloques, nombre de la imagen, "N de M" y si cada botón está encendido) lo da una función pura, la que ya haya o una nueva a proponer, con test.
- **R-HUD-22 (nueva). La pieza se ofrece para el menú.** Un widget de UMG escrito en C++ (nombre a proponer; por ejemplo `UHowToPlayBookWidget`) que el autor arrastra a su pantalla del menú y que pinta la pieza de R-HUD-21:
  - el libro abierto: sobre la imagen del libro si existe, el texto en la página izquierda, la imagen en la derecha y, al pie, "Anterior", "N de M" y "Siguiente". Sin la imagen del libro, como el Grimorio sin ella;
  - sin pestañas, sin el título "Grimorio" y sin la línea de teclas de cerrar y de pestañas, que en el menú no existen;
  - **los mismos ajustes que la partida, leídos del mismo sitio**: el archivo del texto, la carpeta de las imágenes, la imagen del libro con sus márgenes, el margen de la página y el hueco del lomo, la fuente con sus cuatro tipos, el color del Warden y los nombres de las teclas salen de la Config de la vista, como ya hace la librería con las teclas. No hay ajustes duplicados para el menú;
  - el tamaño de letra sigue el criterio del Grimorio (a proponer en el plan cómo se obtiene en el menú, donde no hay capa de la vista);
  - empieza en la primera página cada vez que se crea, y ofrece a Blueprint una función para volver a la primera (para cuando el autor abra la portada). No comparte página con el Grimorio de una partida;
  - las imágenes, la fuente y la imagen del libro que cargue las mantiene vivas, y las que falten se dicen una vez en el registro, como en la vista;
  - si el archivo no carga, enseña el mensaje del cargador, como la pestaña;
  - **las teclas de página**: las flechas configuradas pasan página si el widget tiene el foco del teclado. Si eso choca con la navegación del menú de la plantilla, se queda solo con los botones y se dice en el PR; no se pelea con la plantilla.
- **Lo que no cambia.** El formato del archivo, el cargador y las demás pestañas del Grimorio. Las funciones de la librería (`HowToPlayPageRichText` y las demás) se quedan. `HowToPlayText()` se queda con su `TODO(R-HUD-19)` hasta que el autor quite el bloque de texto viejo de su pantalla: quitarlo antes rompería ese asset.

## Assets (los crea el autor en el editor; commit aparte)

- La pantalla de "Cómo se juega" del menú: sustituye el bloque de texto por la pieza, con la portada (`T_GrimorioPortada`, con el título y la frase que da la librería) y el botón "Jugar". El agente no la crea ni la toca. El PR dice, en texto, cómo se coloca la pieza y qué funciones ofrece a Blueprint.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-HUD-48** (R-HUD-21, guarda). Los tests del libro que ya existen (CA-HUD-30 a CA-HUD-47) pasan sin cambiar lo que comprueban. Lo que enseña la pieza para una página lo da una función pura: título, bloques en su orden, nombre de la imagen, "N de M" y los botones "Anterior" y "Siguiente" encendidos o apagados; en la primera página "Anterior" está apagado y en la última, "Siguiente".
- **CA-HUD-49** (R-HUD-22, ajustes). La función que reúne los ajustes del libro para el menú devuelve los de la Config de la vista: la ruta de la fuente y sus cuatro tipos, la ruta de la imagen del libro y sus márgenes, el margen de la página, el hueco del lomo, la carpeta de las imágenes, el color del Warden y los nombres de las teclas.
- **CA-HUD-50** (R-HUD-22, páginas). El libro del menú empieza en la primera página; "Siguiente" avanza una y se queda en la última; "Anterior" retrocede una y se queda en la primera; la función de volver a la primera la deja en la primera desde cualquiera.
- **CA-HUD-51** (R-HUD-22, pie). En el menú, el pie no nombra la tecla de cerrar ni las de pestañas; como mucho, las de página.
- **CA-HUD-52** (R-HUD-22, error). Con el archivo roto, el libro del menú tiene cero páginas y enseña el mensaje del cargador; no falla.

Manuales, por el autor, en el editor y en PIE, a 1920x1080 y a 1024x768, con captura:

- La pieza aparece en la paleta del diseñador de widgets y se puede colocar en la pantalla del menú.
- En el menú, el libro se ve igual que la pestaña "Cómo se juega" del Grimorio: fondo, fuente, frases del Warden en naranja y cursiva, listas, negrita e imagen.
- Se pasa de página con los botones; el pie dice por cuál va; la última página lista las teclas reales.
- Al salir de "Cómo se juega" y volver a entrar, o al volver al menú después de una partida, el libro está en la primera página.
- Dentro de la partida, el Grimorio se ve y se comporta como antes de esta story, en sus cinco pestañas.
- Con el archivo roto a propósito, el menú enseña el mensaje y no falla.

## Notas para quien lo implemente

- **Lo que hay.** `SGrimoirePanel::RebuildHowToPlay` arma la página de "Cómo se juega" con `HowToPlayLeft` y `HowToPlayRight`, los estilos del texto enriquecido, los botones de página y el reparto en dos columnas de `GrimoireBookLayout`. La vista (`ABoardViewActor`) carga la fuente, la imagen del libro y las de las páginas, y las mantiene vivas. `UHowToPlayLibrary` ya lee del objeto por defecto de la vista las teclas y la carpeta de imágenes: es el patrón para el resto de los ajustes.
- **No hay ningún widget de UMG en C++ en el proyecto**: este es el primero. El módulo `Soulbound` necesitará depender de `UMG`; es un módulo del motor y se anota en el `Build.cs` con su motivo, como las demás dependencias.
- **El riesgo de esta story es la separación.** Si sacar la pieza obliga a cambiar cómo se pintan las otras pestañas, más allá de darles lo mismo que reciben hoy, se para y se pregunta en el PR.
- **El menú es de la plantilla** (Ultra Game Template) y de un asset del autor: no se toca desde código, ni se lee, ni se versiona. La pieza no depende de nada de la plantilla.
- Lo que no se pueda ver sin el editor se anota para la prueba manual y no se rehace a ciegas. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- La portada, el botón "Jugar" y cualquier otra parte de la pantalla del menú (assets del autor).
- Animación de abrir el libro o de pasar página, y sonido.
- Cambiar el aspecto del libro: tamaños, márgenes y espacios se ajustan en S-048.
- Quitar `HowToPlayText()` y las funciones de la librería que el menú deje de usar (cuando el autor haya cambiado su pantalla).
- Enseñar en el menú las demás pestañas del Grimorio.
