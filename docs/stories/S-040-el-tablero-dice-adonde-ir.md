# S-040. Se sabe adónde ir: los Dominios Olvidados se reconocen por el suelo y una línea dice la meta de cada momento

> **Corrección del autor del 16/11/2026, tras las pruebas en PIE**: la luz de la meta (R-VIS-28, CA-VIS-29) se retira. En PIE se veía como palos amarillos, y la decisión de fondo es que marcar sobre el tablero adónde ir es llevar al jugador de la mano; se valorará de nuevo tras la prueba con otro probador. Se quedan el suelo de los Olvidados, la meta del asiento (R-VIC-5) y la línea de objetivo (R-HUD-7). El título era "El tablero dice adónde ir: los Dominios Olvidados se reconocen por el suelo y una luz marca la meta de cada momento"; el nombre del archivo y de la rama no cambian.

**Sistema**: interfaz en plano (GDD 07; pilar 3, caos legible): suelo de las cuñas (R-VIS-2, R-VIS-5), marcas del tablero y panel de estado (R-HUD-1). Se apoya en las reglas de carrera (GDD 03), de runas (GDD 04) y de victoria (R-VIC-1), sin cambiarlas. **Módulos**: `SoulboundCore` (una consulta nueva: la meta de un asiento) y `Soulbound` (presentación: `BoardKit`, `BoardViewActor`, `BoardViewState`, `StatusText`). **Rama base**: `main`, con S-039 fusionada. **Tamaño**: mediano. **Con assets del autor**: una textura y un material para el suelo de los Dominios Olvidados, que crea e importa el autor en el editor, en commit aparte. El código se escribe y se prueba antes de que existan: sin ellos, el suelo se ve como hoy.

## Contexto

En la prueba del 15/11/2026 con la build de probadores, una persona que no juega a videojuegos (P4):

1. **No distinguió los Dominios Olvidados.** Hoy se diferencian solo por estar más apagados. Su primer movimiento fue hacia dentro de un Dominio Olvidado, donde no hay Elegido con quien vincularse, y pasó ahí los cinco minutos de la carrera.
2. **No supo cómo ni dónde se consiguen las runas, ni cuáles son las casillas especiales.** En toda la partida no cayó en una Fuente. Sabía que quería "ir a sitios", pero no a cuáles ni por qué.

Decidido por el autor el 15/11/2026, con el criterio "que se entienda mejor, no que sea más fácil" (ninguna regla cambia):

- El suelo de un Dominio Olvidado lleva otra textura, además del apagado.
- Una luz, visible desde lejos, marca la meta del jugador en cada momento: durante la carrera, los Nichos libres; después, las Fuentes cuya runa le falta; con las seis, el Altar. Cada jugador ve la suya.
- Una línea de objetivo en su panel dice lo mismo con palabras. En el tablero no se añade texto.
- El rótulo "Olvidado" escrito sobre el Dominio queda fuera: se decide más adelante.
- Corrección del 16/11/2026 tras PIE: la luz se retira (ver la nota del principio y R-VIS-28).

## Reglas a implementar

### `SoulboundCore`

- **R-VIC-5 (nueva, consulta). La meta de un asiento.** Una función pública del motor (nombre a proponer; por ejemplo `SeatGoal(State, Seat)`, en `Match/`) dice qué persigue un asiento en este momento y en qué casillas. Devuelve el tipo de meta (enumerado, nombre a proponer; por ejemplo `ESeatGoal`) y la lista de casillas, en orden ascendente. **No cambia el estado ni decide nada**: solo lee, con las mismas comprobaciones que ya usan las reglas, sin copiarlas.
  1. `None`, sin casillas: la partida ha terminado, o el asiento no existe.
  2. `Niche`: la partida está en la carrera por el vínculo y el asiento no se ha vinculado. Las casillas son los Nichos Libres (`ENicheState::Free`). Los Nichos reclamados y los cerrados de los Dominios Olvidados no cuentan.
  3. `Altar`: el asiento tiene los seis tipos de runa (`RuneTypeCount`, `VictoryRules.h`), esté o no ya en el Altar. La casilla es el Altar.
  4. `Runes`: en cualquier otro caso. Las casillas son las Fuentes en las que, si el asiento terminara ahí su movimiento ahora, obtendría una runa **que le falta** (o jugaría la prueba para obtenerla). Es decir, con las condiciones de `ResolvePowerSourceLanding`: el tablero está Despierto (R-TAB-8), la Fuente tiene reserva (R-RUN-4) y, si el asiento es Eco, la Fuente es de un Dominio Olvidado (R-ECO-3). Puede ser una lista vacía: sigue siendo `Runes`.
  - Para que la consulta y la regla no se separen, la condición "este asiento obtendría runa en esta Fuente" sale de una sola función que usan las dos (se propone en el plan cómo: extraerla de `ResolvePowerSourceLanding` o lo que resulte más limpio). El comportamiento de `ResolvePowerSourceLanding` no cambia.
  - La consulta no mira si el asiento puede llegar a la casilla con su tirada, ni el confinamiento del Eco: dice dónde está la meta, no por dónde se va.

### `Soulbound` (`Presentation/`)

- **R-VIS-27 (nueva, precisión de R-VIS-5). El suelo de un Dominio Olvidado.** El suelo liso de la cuña de un Dominio Olvidado se pinta con un material propio (ruta en Config, nombre a proponer; por ejemplo `ForgottenFloorMaterial`, valor inicial `/Game/Art/Tablero/M_SueloOlvidado.M_SueloOlvidado`) en lugar del material de color por vértice del kit.
  - El material nuevo tiene el mismo parámetro `Color` que `M_ColorVertice`: el tinte del Dominio se le sigue aplicando (`WedgeFloorTint`).
  - Con el material puesto, el suelo se apaga con un factor propio (Config, nombre a proponer; por ejemplo `ForgottenFloorDim`, valor inicial 0,6) en vez de `ForgottenDim` (0,3): la textura ya lo distingue y con 0,3 no se leería. Las casillas siguen apagándose con `ForgottenDim`.
  - Si la ruta está vacía o el material no existe, el suelo se pinta como hoy (material del kit, apagado por `ForgottenDim`) y se dice una vez en el registro. Es el patrón de `RingRuneTexture` y `DiceFacesTexture`.
  - Sigue las mismas condiciones de cuándo se ve un Dominio como Olvidado: en el prólogo, cuando se enseña su frase de corrupción (S-023); al terminar la carrera, cuando un Dominio sin dueño pasa a Olvidado (R-TAB-10). Un Dominio que no es Olvidado conserva el material del kit.
  - Si el plan ve que también debe cambiar la peana de la cuña, lo propone; por defecto, solo el suelo liso.
- **R-VIS-28 (retirada el 16/11/2026, corrección del autor tras PIE: se veía como palos amarillos, y marcar sobre el tablero adónde ir es llevar al jugador de la mano; se valorará de nuevo tras la prueba con otro probador). Se conserva el texto original.** ~~La luz de la meta.~~ Sobre cada casilla de la meta **del asiento humano** (R-VIC-5) hay una luz: un haz vertical fino y alto, visible desde la vista sentada y desde la cenital, por encima de la pieza del kit de esa casilla.
  - Se hace con una forma básica y el material básico (`BaseMaterial`), sin asset nuevo y sin luces dinámicas (restricción de rendimiento de la skill). Un solo color para las tres metas (Config, nombre a proponer; por ejemplo `GoalLightColor`, valor inicial un dorado claro, a proponer), distinto del blanco de las alcanzables, del rojo del Sello y del morado de perder una runa. Grosor y altura en Config, con valor inicial documentado, relativos a la casilla.
  - **No late.** El latido significa "puedes ir aquí ahora" (R-VIS-26); la luz significa "aquí quieres llegar". Una casilla puede llevar las dos cosas.
  - Se enciende y se apaga con el estado: al vincularse alguien en un Nicho, ese Nicho deja de estar libre y su luz se apaga; al coger el humano una runa, se apaga la luz de esa Fuente; al agotarse la reserva de una Fuente, se apaga; con las seis runas, solo queda la del Altar. Con la partida terminada, ninguna.
  - Las luces son del jugador local: los dummies no tienen luces. La tecla de etiquetas no las oculta. En el prólogo no se ven hasta que la partida empieza a contar.
- **R-HUD-7 (nueva). La línea de objetivo.** Una línea con la meta del humano, siempre a la vista durante la partida, con su color (Config, `GoalLineColor`, valor inicial un dorado claro (1, 0,82, 0,35); era el de la luz hasta que se retiró R-VIS-28):
  - `Niche`: "Llega a un Nicho libre"
  - `Runes`, Elegido: "Consigue runas en las Fuentes (N/6)"
  - `Runes`, Eco: "Consigue runas en las Fuentes de los Olvidados, o róbalas (N/6)"
  - `Altar`: "Vuelve al Altar"; si ya está en el Altar con los seis tipos (aspirante, `IsAspirant`), "Supera el desafío final en tu próximo turno" (corrección del autor del 16/11/2026 tras PIE; `SeatGoal` no cambia, es solo el texto)
  - N es el número de tipos de runa distintos que tiene (`RuneTypeCount`). Con `None`, la línea no sale.
  - **Precisión del 16/11/2026, en el plan**: en el prólogo la línea no sale: aparece cuando la partida empieza a contar.
  - El texto del Eco es una concreción del tutor (un Eco solo obtiene runa en las Fuentes de los Olvidados, R-ECO-3, y robando, R-ECO-4). Confirmado por el autor el 16/11/2026.
  - **Corrección del autor del 16/11/2026, tras PIE: dónde va.** La línea sale del panel de estado (entre sus líneas quedaba escondida, y es otra clase de información) y va en una caja propia arriba a la izquierda, encima del panel de estado, que queda justo debajo. Fondo como los paneles, la letra grande de las líneas grandes del panel de estado y `GoalLineColor`. Mismas condiciones (ni en el prólogo ni con `None`), y se oculta con la misma tecla que el panel de estado. A 1024x768 cabe sin meterse debajo de la columna de paneles: el texto del Eco puede partirse en dos líneas. El aviso de casilla inalcanzable (`PlaceNotice`) también la esquiva.
  - **Segunda corrección del autor del 16/11/2026, tras ver la caja en PIE: abajo y sin teclas.** La caja se muda a la esquina inferior izquierda del hueco del tablero, dentro de la franja que se reserva siempre a la frase del Warden (R-DEC-9, `WardenLineReservedHeight`), por debajo del panel de decisión: no se pisan nunca. Misma letra grande, mismo color y mismo fondo; el panel de estado vuelve a quedar solo arriba a la izquierda. Mientras hay una frase del Warden en pantalla, la caja se oculta, y vuelve cuando la frase se quita: nunca se ven los dos textos encimados. Ninguna tecla la oculta, ni T ni H (es información importante, no un panel de consulta): solo deja de verse en el prólogo, con la meta `None` y mientras el Warden habla. Lo demás, igual: el texto se parte si no cabe, no se mete bajo la columna de paneles y el aviso de casilla inalcanzable la esquiva en su sitio nuevo. Sustituye a lo que la corrección anterior decía del sitio (encima del panel de estado) y de la tecla.
  - **Corrección tras PIE del 16/11/2026 de un defecto de S-039 (R-DEC-16).** El aviso de casilla inalcanzable se montaba sobre el panel de decisión (tapaba "Tu turno" y "Has sacado") cuando el clic era cerca de él: la vista no le pasaba el panel a `PlaceNotice` como zona. Ahora, mientras el panel de decisión está a la vista, es una zona más que el aviso esquiva. `PlaceNotice` ya estaba probada con zonas cualesquiera; no hay nada nuevo que decidir sin mundo.
- **La leyenda.** En el bloque "Tablero", la línea de los Olvidados pasa a "Suelo agrietado y casillas apagadas: Dominio Olvidado". Sin el material cargado sigue diciendo "Casillas apagadas: Dominio Olvidado" (decisión del autor del 16/11/2026, en el plan: la leyenda no describe un suelo que no se ve). La línea "Luz: tu meta ahora" se quitó con R-VIS-28.
- **Lo que no cambia.** Ninguna regla de carrera, de runas ni de victoria; el aro y el apagado de las casillas de un Dominio Olvidado; las marcas de Nicho libre, reclamado y cerrado; los halos y su latido; los destellos.

## Assets (los crea el autor en el editor; commit aparte)

- `Content/Art/Tablero/T_SueloOlvidado`: textura de 1024 x 1024, piedra agrietada en gris neutro, arte generado con IA, placeholder.
- `Content/Art/Tablero/M_SueloOlvidado`: material con el parámetro vectorial `Color`. Base Color = la textura por `Color` (y por el color por vértice, si `M_ColorVertice` lo usa así: lo más seguro es duplicar `M_ColorVertice` y multiplicar su Base Color por la textura). La textura se proyecta por posición en el mundo, no por las coordenadas de la malla, para que el dibujo tenga el mismo tamaño en las seis cuñas.
- El agente no crea ni toca estos assets. El PR los describe en texto y el autor los verifica en el editor antes de fusionar.
- **Creados por el autor el 16/11/2026** y probados en PIE: `T_SueloOlvidado` con el contraste subido; `M_SueloOlvidado` es una copia de `M_ColorVertice` con la textura multiplicada en Base Color, proyectada por posición en el mundo, con el parámetro escalar `TamanoDibujo` (valor 30) y el parámetro `Color` de siempre.

## Criterios de aceptación

Automáticos, sin mundo.

En `SoulboundCore`:

- **CA-VIC-6** (R-VIC-5, carrera). En la carrera, para un asiento sin vincular, la meta es `Niche` y sus casillas son exactamente los Nichos Libres: no incluye el Nicho que otro asiento acaba de reclamar ni los cerrados de los Dominios Olvidados.
- **CA-VIC-7** (R-VIC-5, runas del Elegido). Con el tablero Despierto, para un Elegido con dos runas distintas, la meta es `Runes` y sus casillas son las Fuentes con reserva de las cuatro runas que le faltan, sean de Dominios con dueño u Olvidados. Una Fuente sin reserva no está. Una Fuente de una runa que ya tiene no está.
- **CA-VIC-8** (R-VIC-5, runas del Eco). Para un Eco, tras la carrera, las casillas son solo las Fuentes con reserva de los Dominios Olvidados cuya runa le falta. Si no hay ninguna, la meta sigue siendo `Runes`, con la lista vacía. Durante la Noche de los Ecos la lista no cambia (puede entrar en un Dominio con dueño, pero no obtiene en su Fuente).
- **CA-VIC-9** (R-VIC-5, vinculado en la carrera). Durante la carrera, un asiento ya vinculado tiene la meta `Runes`, no `Niche`. Con el tablero en Primordial, la lista de `Runes` está vacía.
- **CA-VIC-10** (R-VIC-5, Altar y final). Con las seis runas distintas, la meta es `Altar` y su única casilla es el Altar. Con la partida terminada, `None` y ninguna casilla.
- **CA-RUN-6** (R-VIC-5, una sola condición). Para cada caso de CA-VIC-7 y CA-VIC-8, la casilla está en la meta si y solo si llevar al asiento a esa Fuente y resolver la llegada (`ResolvePowerSourceLanding`) da `Obtained` o `TrialPending` **y la runa de esa Fuente le falta**. (Corrección detectada en el plan, 16/11/2026: con la redacción anterior, una Fuente de una runa que ya tiene daría `Obtained`, R-RUN-5, y no está en la meta según R-VIC-5.) Los tests de runas que ya existen siguen en verde sin tocarlos.

En `Soulbound`:

- **CA-VIS-28** (R-VIS-27). La función que dice con qué factor se apaga el suelo de una cuña devuelve 1 para un Dominio con dueño; `ForgottenFloorDim` para un Olvidado con el material propio disponible; y `ForgottenDim` para un Olvidado sin él. El tinte del Dominio (`WedgeFloorTint`) se aplica igual en los tres casos.
- **CA-VIS-29** (R-VIS-28): **retirado el 16/11/2026 con R-VIS-28**, con su test. ~~La función que dice qué casillas llevan luz (del estado y del asiento humano) devuelve las de la meta del humano y ninguna de otro asiento; ninguna con la partida terminada; ninguna en el prólogo.~~
- **CA-HUD-15** (R-HUD-7). La línea de objetivo dice, letra por letra, los textos de la regla en sus casos, con N igual a las runas distintas del humano, y "Supera el desafío final en tu próximo turno" para el aspirante en el Altar; va en su caja, una sola línea con la letra grande y `GoalLineColor`, y el panel de estado no la lleva; con la meta `None`, no hay línea; en el prólogo, tampoco (precisión de R-HUD-7 del 16/11/2026; caja y aspirante, corrección del mismo día tras PIE). Desde la segunda corrección del mismo día, una función pura dice si la caja se ve: sí con cualquier meta salvo `None`; no en el prólogo, con `None` ni mientras hay una frase del Warden en pantalla; ninguna tecla interviene.

Manuales, por el autor, en PIE, en la vista sentada y en la cenital, a 1920x1080 y a 1024x768, con captura:

- Con los assets importados: los Dominios Olvidados se distinguen de un vistazo de los que tienen Elegido, también en la vista sentada. El dibujo del suelo se lee y tiene el mismo tamaño en todas las cuñas. Sin costuras llamativas.
- Sin los assets (ruta vacía en Config): el suelo se ve como antes y el registro lo dice una vez.
- Al empezar, sin vincular: la línea de objetivo dice "Llega a un Nicho libre".
- Tras vincularse (o tras `SbEndRace`): la línea dice "Consigue runas en las Fuentes (N/6)". Al coger una runa (`SbGiveRune` o cayendo en la Fuente), sube N.
- Como Eco tras la carrera: la línea dice el texto del Eco.
- Con seis runas (`SbGiveRune`): la línea dice "Vuelve al Altar".
- Añadidas con la corrección del 16/11/2026 (las de la caja, según la segunda corrección): la caja va abajo a la izquierda del hueco del tablero, por debajo del panel de decisión y sin pisarse con él, con la letra grande; ni T ni H la ocultan; mientras hay una frase del Warden en pantalla no se ve, y vuelve cuando la frase se quita; el panel de estado queda solo arriba a la izquierda; a 1024x768, como Eco, el texto se parte y la caja no se mete bajo la columna de paneles; ya en el Altar con los seis, dice "Supera el desafío final en tu próximo turno"; el aviso de casilla inalcanzable no tapa la caja.
- Corrección del defecto de S-039 (16/11/2026): con el panel de decisión a la vista, un clic en una casilla inalcanzable cerca de él pone el aviso fuera del panel, sin tapar "Tu turno" ni "Has sacado".
- La leyenda tiene la nueva línea de los Olvidados.
- (Retiradas el 16/11/2026 con R-VIS-28: las de la luz sobre los Nichos, las Fuentes y el Altar, que no tape la pieza ni la ficha, y las dos añadidas en el plan sobre la base en el aire y el material no emisivo.)

## Notas para quien lo implemente

- **El suelo.** Hoy lo tiñe `FBoardKit::TintWedgeFloors` (`BoardKit.cpp`) con `SetComponentColor(*Floor, VertexColor.Get(), WedgeFloorTint(...))`, y `ABoardViewActor::Refresh` le pasa qué Dominios se enseñan como Olvidados (`DomainsShownForgotten`). `SetComponentColor` reutiliza la instancia dinámica que ya tenga la pieza: al cambiar de material hay que crear la instancia sobre el material nuevo, y volver al del kit si un Dominio dejara de enseñarse como Olvidado (pasa en el prólogo). `BoardPreviewActor` también llama a `TintWedgeFloors`.
- **La carga del material** sigue el patrón de `RingRuneTexture` y `DiceFacesTexture` en `BoardViewActor` (S-031, S-039): ruta en Config, un `UPROPERTY` que lo mantiene vivo, aviso una vez en el registro si falta.
- **La luz.** Las piezas por casilla ya tienen sitio en la vista (`Halos`, `SealMarks`, `ForgottenRings`): la luz puede ser otra lista igual, una pieza por casilla que pueda ser meta (Nichos, Fuentes y Altar), visible o no según la consulta. No hace falta `Tick`: cambia solo con el estado.
- **La línea de objetivo** va en `StatusLines` (`StatusText.cpp`), con las demás del panel de estado; su sitio (arriba, tras la petición) se propone en el plan.
- **El motor.** `ResolvePowerSourceLanding` (`RuneRules.cpp`) tiene las condiciones de obtener runa en una Fuente; `RuneTypeCount` (`VictoryRules.h`), la de los seis tipos de runa (`IsAspirant` exige además estar en el Altar: no es la que hace falta). La consulta nueva las usa; no las reescribe.
- **"Meta"** es un concepto nuevo: se propone en el plan con su nombre en código y va al glosario en un commit aparte, como los demás nombres nuevos.
- Lo aprendido en S-029, S-033, S-037 y S-039: para teñir formas básicas, `BaseMaterial` (parámetro "Color"); lo que queda por debajo del suelo del kit no se ve; tamaños y colores en Config con valor inicial documentado; lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- El rótulo "Olvidado" y el nombre del Dominio en grande sobre el tablero (pendiente de decisión del autor).
- La luz de la meta (R-VIS-28, retirada el 16/11/2026): se valorará de nuevo tras la prueba con otro probador. Luces para los dummies o para otros jugadores.
- Marcar por dónde se llega a la meta, o qué casillas alcanzables acercan a ella.
- Cambiar las marcas de los Nichos o el aro de las casillas de un Dominio Olvidado.
- La señal de corte del Warden, el botón "Empezar" de las pruebas y el orbe del Oráculo (story aparte).
- Avisar a un Eco de a quién puede robar.
