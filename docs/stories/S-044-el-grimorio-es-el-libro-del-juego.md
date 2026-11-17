# S-044. El Grimorio es el libro del juego: cinco pestañas, un botón a la vista y el Warden que lo nombra

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible): Grimorio (R-HUD-4, S-027), leyenda, crónica y "Cómo se juega" (S-022). **Módulos**: `Soulbound` (presentación: `GrimoirePanel`, `StatusText`, `LegendPanel`, `ChroniclePanel`, `HowToPlay`, `InstructionTracker`, `BoardViewActor`) y datos (`Content/Data/Warden/frases.json`). `SoulboundCore` solo cambia en un punto: el momento de instrucción nuevo, `EInstructionMoment::Grimoire` (`Warden/WardenLines.h`), porque el enum y las claves exigidas por el cargador viven allí (corrección del autor del 17/11/2026, en el plan); ninguna regla cambia. **Rama base**: `main`, con S-043 fusionada. **Tamaño**: mediano. **Sin assets**.

## Contexto

En la prueba del 15/11/2026 con la build de probadores, una persona que no juega a videojuegos (P4) no supo qué era el Grimorio, ni cómo usarlo, ni cuándo consultarlo; tuvo la leyenda delante toda la partida y no supo cuáles eran las casillas especiales; y leyó el "Cómo se juega" en el menú, antes de jugar, y cuando le hizo falta ya no lo tenía.

El autor quiere además una pantalla limpia: lo que es de consulta no debe estar siempre a la vista. Decidido por el autor el 16/11/2026:

- El Grimorio pasa a ser el único sitio de consulta durante la partida, con pestañas: tú, la mesa, el tablero, cómo se juega y la crónica.
- Se abre siempre en la pestaña "Tú".
- Hay un botón "Grimorio (G)" a la vista, abajo a la derecha.

Esta story construye el Grimorio nuevo **sin quitar todavía nada de la pantalla**: la columna de la leyenda y el panel de estado siguen como hoy. Quitarlos es la story siguiente (S-045). Así cada una se puede probar y fusionar por separado.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-HUD-10 (nueva, sustituye el contenido de R-HUD-4). El Grimorio tiene cinco pestañas.** En este orden:
  1. **Tú.** Lo que enseña hoy el Grimorio (tu rol, tu Elegido y su afinidad, tus runas y el poder que da cada una, tus poderes activos) y además tu insolencia ("Insolencia: N de 5"). Si eres Eco tras la carrera, dice lo único que te da una runa: "La runa de tu hogar (DN, nombre del Dominio) te da Celeridad: puedes tirar dos dados" (R-ECO-7; corrección del autor del 17/11/2026 tras PIE: decía "tiras dos dados", y Celeridad deja elegir).
  2. **La mesa.** Los cuatro jugadores, cada uno con su color: rol y Elegido u hogar, runas que tiene de cada Dominio, poderes activos, y si tiene pendiente la Prueba del Warden o es aspirante. Y los seis Dominios: de quién es cada uno o si está Olvidado. Es lo que hoy reparten los bloques "Los seis Dominios y su Elegido", "Jugadores" y "Runas de cada jugador" de la leyenda. **De cada asiento enseña lo mismo que hoy enseña la leyenda, ni más ni menos** (corrección del autor del 17/11/2026, en el plan): se mantienen los filtros de R-HUD-6 y R-RUN-7, así que la insolencia solo va en la entrada del humano, y las runas de otro asiento salen como "N runas" salvo con su Brazal revelado (`CanSeeBracer`). La marca de aspirante es lo único nuevo.
  - Las tarjetas de los otros tres asientos que enseñaba el Grimorio de S-027 se sustituyen por esta pestaña, y las seis líneas de poderes van al pie de "Tú" (aceptado por el autor el 17/11/2026).
  3. **El tablero.** La leyenda de casillas y marcas: los bloques "Casillas", "Nichos", "Tablero" y la explicación de los destellos, con sus muestras de color.
  4. **Cómo se juega.** El mismo texto del menú (`Content/Data/Textos/como-se-juega.md`, con el mismo cargador), y al final las teclas.
  5. **Crónica.** Lo que ha pasado, lo último arriba, como la crónica de hoy.
  - **Se abre siempre en "Tú"** (decisión del autor): no recuerda la última pestaña.
  - Se cambia de pestaña con el ratón, haciendo clic en su nombre, y con el teclado: flecha derecha a la siguiente y flecha izquierda a la anterior, de forma cíclica (de "Crónica" a "Tú" y al revés), solo con el Grimorio abierto (aceptado por el autor el 17/11/2026).
  - Los textos nombran las teclas en español donde lo haya, como ya se hacía con Enter (Intro): el título dice "G o H para cerrar; flechas izquierda y derecha, pestañas", y con otras teclas configuradas, su nombre en español (corrección del autor del 17/11/2026 tras PIE: decía "Left y Right"). Lo mismo el botón y el bloque de teclas de "Cómo se juega".
  - El contenido de cada pestaña sale de las funciones que ya lo calculan hoy (las del Grimorio, las filas de la leyenda, las líneas de la crónica, el cargador de "Cómo se juega"): se reutilizan, no se reescriben. Lo que haya que partir o reordenar para repartirlo en pestañas se hace en funciones puras, con test.
  - El texto que no quepa se desplaza con la rueda dentro de la pestaña. El panel cabe a 1024x768.
- **R-HUD-11 (nueva). El Grimorio no tapa una decisión.** Como hoy, con el Grimorio abierto la partida sigue y los clics sobre el tablero no cuentan. Desde esta story, si mientras está abierto se abre una decisión del humano (elección, destino en su turno, Oráculo, voto) o un anillo suyo, el Grimorio se cierra solo. El plan dice qué pasa hoy en ese caso, para que se vea qué cambia. Precisiones del autor del 17/11/2026: con una decisión propia ya abierta, el Grimorio se puede abrir para consultar; se cierra solo al aparecer una decisión propia **nueva** o un anillo, también si la decisión sale bloqueada porque habla el Warden. Durante un anillo del humano ni el botón ni la tecla abren el Grimorio.
- **R-HUD-12 (nueva). El botón del Grimorio.** Un botón "Grimorio (G)", con el nombre de la tecla configurada, abajo a la derecha del hueco del tablero, en la franja de la frase del Warden, en espejo de la caja de la línea de objetivo (corrección del autor del 17/11/2026, en el plan). Abre y cierra el Grimorio, igual que la tecla. Sigue la regla de la caja de objetivo: no se ve en el prólogo, con la partida terminada ni mientras hay una frase del Warden en pantalla, y vuelve al quitarse la frase; la frase no se estrecha por él. Ninguna tecla de ocultar lo oculta. La tecla G funciona siempre (salvo durante un anillo del humano, R-HUD-11). El aviso de casilla inalcanzable (S-039) lo esquiva.
- **R-HUD-13 (nueva). El Warden nombra el Grimorio.** Una instrucción nueva (`instruccion:grimorio`, con el mecanismo de las instrucciones de S-021: una vez por jugador, según su perfil y el ajuste de Ayudas) la primera vez que el humano tiene de verdad algo que consultar: una afinidad o un poder activo. Para un Elegido es al vincularse; para un Eco, al conseguir la runa de su hogar, que le da Celeridad. Se mira en el estado, no en un evento (corrección del autor del 17/11/2026, en el plan: así el texto "Tienes poderes…" es siempre cierto). No detiene la mesa, como las demás instrucciones. La clave la exige el cargador como las demás instrucciones y no cuenta para el texto de petición (`ShowsRequestHints`), como las Ayudas de S-027. Texto, aprobado por el autor el 17/11/2026: "Tienes poderes y no sabes cuáles. Pulsa G y lee tu Grimorio… si es que sabes leer."
- **Datos: una instrucción que se quedó vieja.** `instruccion:elegir-casilla` dice "Las casillas que brillan son tuyas por un momento"; desde S-039 laten. Pasa a "Las casillas que laten son tuyas por un momento. Elige una. Solo una." (aprobado por el autor el 17/11/2026).
- **Lo que no cambia en esta story.** La columna de la leyenda y la crónica, el panel de estado y sus teclas siguen como hoy (los quita S-045). El menú y su "Cómo se juega". Ninguna regla.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-HUD-18** (R-HUD-10, pestañas). La lista de pestañas es, en este orden, Tú, La mesa, El tablero, Cómo se juega y Crónica. Al abrir el Grimorio la activa es Tú, también si antes se cerró en otra. Pasar a la siguiente desde la última vuelve a la primera (o se queda, según lo que se proponga en el plan; el test fija lo decidido).
- **CA-HUD-19** (R-HUD-10, Tú). Para un Elegido, la pestaña lleva lo que llevaba el Grimorio y su insolencia. Para un Eco tras la carrera con hogar en el Dominio 4, lleva la línea de la runa de su hogar con Celeridad, con el número y el nombre de ese Dominio ("…te da Celeridad: puedes tirar dos dados", corrección del autor del 17/11/2026); durante la carrera, sin hogar, no la lleva.
- **CA-HUD-20** (R-HUD-10, La mesa). Lleva una entrada por asiento, con su rol, sus runas por Dominio y sus poderes activos (los de otro asiento, como "N runas" salvo con su Brazal revelado), la insolencia solo en la del humano, y las marcas de Prueba del Warden pendiente y de aspirante cuando tocan; y una entrada por Dominio con su dueño o "Olvidado".
- **CA-HUD-21** (R-HUD-10, El tablero, Cómo se juega y Crónica). El tablero lleva las filas de casillas, Nichos, marcas y destellos, y ninguna de jugadores ni de runas. Cómo se juega lleva el texto del archivo y, si el archivo falta, el mensaje del cargador en vez de quedarse vacía. Crónica lleva las mismas líneas y en el mismo orden que la crónica de hoy.
- **CA-HUD-22** (R-HUD-11). La función que decide si el Grimorio sigue abierto devuelve que se cierra al abrirse una decisión nueva del humano (también bloqueada porque habla el Warden) o un anillo suyo, y que sigue abierto con una decisión de otro asiento, con el Warden hablando sin decisión propia y con la decisión propia que ya estaba abierta al abrirlo. Durante un anillo del humano, el Grimorio no se abre.
- **CA-HUD-23** (R-HUD-12). El texto del botón es "Grimorio (G)" con la tecla por defecto y lleva el nombre de otra tecla si se configura. La función que decide si se ve devuelve que no en el prólogo, con la partida terminada ni con una frase del Warden en pantalla, y que sí en el resto, con cualquier combinación de teclas de ocultar.
- **CA-HUD-24** (R-HUD-13). El seguimiento de instrucciones da el momento del Grimorio al vincularse el humano y, si es un Eco, al tener la runa de su hogar (Celeridad); no por una runa de otro Dominio; una sola vez; no lo da por lo que les pase a otros asientos.

Manuales, por el autor, en PIE, a 1920x1080 y a 1024x768, con captura:

- El botón "Grimorio (G)" se ve abajo a la derecha, no tapa casillas importantes ni se pisa con el panel de decisión, se va mientras hay frase del Warden y vuelve al quitarse, y abre y cierra el Grimorio; la tecla G hace lo mismo.
- Las cinco pestañas se leen, se cambia entre ellas con el ratón y con el teclado, y siempre se abre en "Tú".
- "Tú" como Elegido y como Eco (tras `SbEndRace` con alguien vinculado); "La mesa" refleja un robo o una runa nueva sin cerrar el Grimorio; "El tablero" tiene sus muestras de color; "Cómo se juega" se lee entero con la rueda; "Crónica" va creciendo.
- Con el Grimorio abierto, al llegar una decisión propia (por ejemplo `SbStrike ojos-en-ti oraculo` en su turno) se cierra solo y se ve el panel de decisión.
- La primera vez que se vincula, o como Eco consigue la runa de su hogar, el Warden dice la instrucción del Grimorio (con `SbForgetInstructions` para repetirla).

**Resultado de las pruebas manuales (autor, PIE, 17/11/2026): superadas.**
- El botón "Grimorio (G)" abre y cierra, igual que la tecla.
- Las cinco pestañas se leen y se cambian con el clic y con las flechas; la rueda desplaza; siempre se abre en "Tú".
- El Grimorio se cierra solo al llegar el turno del humano y con el Oráculo como golpe, y se puede abrir con una decisión ya abierta.
- "Tú" como Elegido y como Eco, con la línea de la runa del hogar y Celeridad en los poderes activos al tenerla.
- La instrucción del Warden sale al conseguir la runa del hogar.
- "La mesa" queda comprobada solo por su test (CA-HUD-20).
- Dos correcciones de texto del autor tras la prueba, ya en las reglas de arriba: el título con las teclas en español ("flechas izquierda y derecha") y "puedes tirar dos dados" en la línea del Eco.

## Notas para quien lo implemente

- **Lo que ya existe.** `SGrimoirePanel` y sus datos (`GrimoireRows`, `GrimoirePowerLines`, `grimorio.json`); `SLegendPanel` con `FLegendRow` (las filas se montan en `ABoardViewActor::BuildOverlay`); `SChroniclePanel` con `FChronicleLine`; el cargador de `HowToPlay.h`; `FInstructionTracker` y `TeachInstructions` para las instrucciones. La columna usa hoy `SLegendPanel` y `SChroniclePanel`: en esta story se usan también dentro del Grimorio sin quitarlos de la columna.
- **Los títulos de "Cómo se juega".** `HowToPlayHeadings()` exige unos títulos concretos y en orden. El autor está reordenando ese texto: si cambia los títulos, hay que cambiar esa lista. No es de esta story; se anota en el PR para que no sorprenda.
- **El botón** convive con la entrada del tablero: un clic en él no debe llegar a las casillas. Durante un anillo del humano el clic lo recoge antes la entrada del anillo (`FRingInputProcessor`): el botón no hace falta ahí.
- **`frases.json`**: la instrucción nueva la exige el cargador, como todas las instrucciones (`RequiredWardenLineKeys`; la versión anterior de esta nota decía que era opcional, y no lo es ninguna). Si la tabla no la tuviera, el seguimiento ya trata la frase que falta (no se enseña ni se anota).
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Tamaños y colores, en Config con su valor inicial documentado. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- Quitar la columna de la leyenda, el panel de estado y sus teclas, y poner el reloj arriba a la izquierda (S-045).
- Reescribir o reordenar el texto de "Cómo se juega" (lo hace el autor) y darle imágenes o formato de tarjetas.
- Ayuda que salta sola según el momento, más allá de las instrucciones del Warden que ya existen.
- Pausar la partida con el Grimorio abierto.
- Cambiar el Grimorio del menú, si lo hubiera, o cualquier pantalla de UGT.
