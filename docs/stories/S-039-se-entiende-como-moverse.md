# S-039. Se entiende cómo moverse: las casillas alcanzables laten, la tirada se ve en dados y un clic imposible dice por qué

**Sistema**: turnos y dados (GDD 02: R-TUR-5) e interfaz en plano (GDD 07; pilar 3, caos legible): halos (R-VIS), panel de decisión (R-DEC), panel de estado (R-HUD) y acta (R-ACT). **Módulos**: `SoulboundCore` (el estado guarda el valor de cada dado; el motivo de una casilla inalcanzable) y `Soulbound` (presentación: `BoardViewActor`, `DecisionText`, `DecisionPanel`, `StatusText`, `MatchRecord`). **Rama base**: `main`. **Tamaño**: mediano. **Sin assets en esta story**: la imagen del dado es opcional y la importa el autor más adelante, en commit aparte; sin ella, el dado se dibuja por código.

## Contexto

En la prueba del 15/11/2026 con la build de probadores, una persona que no juega a videojuegos (P4) no llegó a entender cómo se mueve la ficha:

1. **No relacionó el halo blanco con "puedes ir aquí".** Hacía clic en el sitio al que quería llegar (una Fuente, un Nicho), no en las casillas con halo. Más de la mitad de sus clics de destino se rechazaron, todos sobre casillas especiales, y el juego no le dijo nada: el clic rechazado no deja rastro en pantalla.
2. **Los motivos del rechazo eran tres y distintos**: el dado no llegaba (la mayoría), era Eco y la casilla estaba en el Dominio de un Elegido, o la casilla estaba sellada. Desde fuera los tres se ven igual: no pasa nada.
3. **Leyó "Tirada 1" como "tirada número 1"**, el contador de turno, no como el resultado del dado.

Decidido por el autor el 15/11/2026, con el criterio "que se entienda mejor, no que sea más fácil" (ninguna regla de movimiento cambia):

- Las casillas alcanzables laten: el halo sube y baja de brillo.
- Un clic en una casilla a la que no se puede ir enseña un aviso con el motivo, en la voz del Warden.
- La tirada se enseña con la cara del dado, no con un número suelto; con dos dados, las dos caras. Para eso el motor guarda el valor de cada dado.
- La instrucción "Elige una casilla que late." va fija en el panel de decisión, no en el aviso.

## Reglas a implementar

### `SoulboundCore`

- **R-TUR-16 (nueva, precisión de R-TUR-5). El estado guarda el valor de cada dado de la tirada.** Junto a `FMatchState::Roll`, el estado guarda los valores de los dados tirados en el turno (nombre a proponer; por ejemplo `DiceValues`): tantos como `DiceCount`, en el orden del sorteo, y su suma es `Roll`. Se escriben donde hoy se escribe `Roll` (la tirada y la repetición de Apep) y se vacían donde `Roll` vuelve a 0.
  - El consumo fijo del sorteo no cambia (glosario "Tirada"): se siguen sacando siempre `Dice::MaxCount` valores y solo se guardan los que se tiran. Una partida con la misma semilla da el mismo registro de eventos que antes de esta story.
  - Con una tirada fijada por depuración (`ForcedRoll`, S-011) no hay valores de dados: quedan vacíos y `Roll` es la suma fijada. La presentación lo trata aparte (R-DEC-15).
- **R-TAB-29 (nueva). El motivo de una casilla inalcanzable.** Una función pública del motor (nombre a proponer; por ejemplo `UnreachableReason(State, Seat, TileId)`) dice por qué el asiento no puede ir a una casilla que no está entre sus alcanzables. Devuelve un enumerado (nombre a proponer; por ejemplo `EUnreachableReason`), mirando solo la casilla pulsada y los rasgos del asiento, **sin calcular caminos**, y en este orden:
  1. `Sealed`: la casilla está sellada y el asiento no tiene Paso Libre (R-TAB-17, R-POD-2).
  2. `Confined`: el asiento está confinado a los Dominios Olvidados (R-TAB-20: es Eco, la carrera ha terminado y no es la Noche de los Ecos) y la casilla está en un Dominio que no es Olvidado. Usa la misma comprobación que el movimiento (`MakeMoverTraits` y el confinamiento de `MovementRules`), no una copia.
  3. `Roll`: cualquier otro caso.
  - No valida nada ni cambia el estado: `SubmitDestinationIntent` sigue devolviendo `Unreachable` como hasta ahora.

### `Soulbound` (`Presentation/`)

- **R-VIS-26 (nueva). Las casillas alcanzables laten.** Mientras se ven los halos de casilla alcanzable, su brillo sube y baja con la misma función del pulso del aro del turno (`RingPulse`, `Presentation/TokenLook.h`), entre un mínimo y 1. Periodo y mínimo en Config, con valor inicial documentado (nombres a proponer; por ejemplo `ReachablePulseSeconds`, valor inicial 1,2 s, y `ReachablePulseMin`, valor inicial 0,12; corrección del autor del 15/11/2026 tras las pruebas en PIE: con 0,35 el punto apagado se veía demasiado). El color sigue siendo `ReachableColor`. El halo no cambia de tamaño ni de sitio. Mientras dura un destello de Fuente sobre un halo (R-VIS-12), manda el destello. La tecla de etiquetas sigue sin ocultar los halos.
- **R-DEC-15 (nueva). La tirada se ve en dados.**
  - En el turno de tirada del humano, el panel de decisión dice "Tu turno" de título, y debajo "Has sacado" seguido de la cara de cada dado tirado (una o dos), en el orden del estado.
  - En la espera del turno de tirada de otro asiento, bajo "Turno de …", "Ha sacado" seguido de las caras. Sustituye a "Tirada N: elige casilla".
  - Con la tirada fijada por depuración (sin valores de dados), no se dibuja ninguna cara: "Has sacado un 9" o "Ha sacado un 9".
  - La instrucción pasa de "Elige una casilla iluminada" a **"Elige una casilla que late."**, y con la repetición de Apep, "Elige una casilla que late o repite la tirada."
  - **La cara del dado.** Si existe la imagen de caras (ruta en Config, `FSoftObjectPath`, nombre a proponer; valor inicial `/Game/Art/Dados/T_DadoCaras.T_DadoCaras`), se pinta el trozo que toca: la imagen lleva las seis caras en una fila, del 1 al 6 de izquierda a derecha, y cada cara ocupa un sexto del ancho. Si la ruta está vacía o la imagen no existe, el panel dibuja la cara por código (un cuadrado de esquinas redondeadas con sus puntos; corrección del autor del 15/11/2026 tras las pruebas en PIE: los puntos, redondos, más pequeños y más separados, con proporciones respecto al lado de la cara, no números sueltos) y lo dice una vez en el registro, como la runa del anillo (S-031, `RingRuneTexture`). Tamaño de la cara y colores del dado dibujado, en Config con valor inicial documentado.
- **R-HUD-1 (precisión). El panel de estado.** La línea "Tirada: N (dos dados) | reloj de turno: …" pasa a "Dado: N | reloj de turno: …" con un dado y "Dados: N | reloj de turno: …" con dos (N es la suma). La Ayuda de esa línea pasa de "clic en una casilla blanca" a "clic en una casilla que late". **Corrección del autor del 15/11/2026 tras las pruebas en PIE**: en el turno de tirada del humano, con el panel de decisión a la vista (`FStatusContext::bDecisionPanelShown`), el panel de estado **no repite** la línea de la tirada (regla de S-028, R-DEC-3: con el panel a la vista, el de estado no repite la petición); la Ayuda "clic en una casilla que late" solo sigue saliendo cuando el panel de decisión no está a la vista. En el turno de otro asiento, la línea "Dado: N" o "Dados: N" sigue, sin Ayuda.
- **R-DEC-16 (nueva). El aviso de casilla inalcanzable.** Cuando el humano, en su turno de tirada, hace clic en una casilla y el motor contesta `Unreachable`, se enseña un aviso con el motivo (R-TAB-29), con el color de la línea del Warden (`WardenLineColor`):
  - `Sealed`: "Esa casilla la he sellado yo. Busca otro camino."
  - `Confined`: "Los Ecos no pisan el Dominio de un Elegido. Por ahora."
  - `Roll`: "Con un 3 no llegas ahí. Qué lástima." (con la tirada del turno, la suma).
  - **Dónde sale (corrección del autor del 15/11/2026 tras las pruebas en PIE)**: no en el panel de decisión, sino **junto al punto del clic**: los ojos están en la casilla pulsada, no en el panel. Sale en la posición de pantalla del clic rechazado y se queda quieto ahí. Se recoloca para verse entero: dentro del viewport y sin quedar debajo de la columna de paneles ni del panel de estado; la colocación sale de una función pura con test. No recibe clics (no puede tapar una casilla al ratón) y lleva un fondo oscuro que lo hace legible sobre el tablero, como los paneles. El punto se captura en el momento del clic (el anfitrión lo lee del cursor en la misma llamada que entrega la intención), no en el siguiente refresco del HUD. El panel de decisión no lleva ninguna línea reservada para el aviso.
  - El aviso dura unos segundos (Config, nombre a proponer; valor inicial 4 s) y se quita antes si el humano hace otro clic de destino (aceptado, o rechazado: sale el aviso nuevo, en el punto nuevo) o si termina su turno de tirada.
  - Es presentación: **no** es una frase del Warden (no deja `WardenSpoke`, no pasa por la línea del Warden ni la sustituye, no detiene la mesa) y no cuenta como insolencia ni como nada del motor. Los tres textos son del autor (15/11/2026) y van tal cual.
  - Solo sale con `Unreachable`. Un clic fuera de turno, fuera de fase, con el reloj agotado o mientras la mesa espera al Warden no enseña este aviso.
- **R-ACT (precisión de R-ACT-8). El acta dice el motivo.** La línea "humano" de un destino rechazado por inalcanzable añade el motivo: "destino: casilla 80 (…) -> Unreachable (dado)", "(Eco)" o "(sello)". Sirve para leer las pruebas con probadores.
- **Lo que no cambia.** Las reglas de movimiento, las casillas alcanzables, el color y el tamaño del halo, el reloj de turno, el evento `Rolled` y la línea de la tirada en la crónica.

## Criterios de aceptación

Automáticos, sin mundo.

En `SoulboundCore`:

- **CA-TUR-16** (R-TUR-16). Tras la tirada con un dado, el estado guarda un valor, entre 1 y 6, igual a `Roll`. Con dos dados (Celeridad), guarda dos valores, cada uno entre 1 y 6, y su suma es `Roll`.
- **CA-TUR-17** (R-TUR-16). Tras repetir la tirada (Apep), los valores guardados son los de la tirada nueva y suman el `Roll` nuevo. Al terminar el turno, y en un turno cortado antes de tirar, no queda ningún valor.
- **CA-TUR-18** (R-TUR-16). Con `ForcedRoll` fijado, `Roll` es el valor fijado y no se guarda ningún valor de dado. Y el sorteo se consume igual: una partida simulada con una semilla dada deja el mismo registro de eventos que antes de la story (el test de determinismo que ya existe sigue en verde sin tocarlo).
- **CA-TAB-30** (R-TAB-29, sello). Una casilla sellada que el asiento no alcanza da `Sealed`. La misma casilla, para un asiento con Paso Libre que no la alcanza con su tirada, da `Roll`.
- **CA-TAB-31** (R-TAB-29, Eco). Para un Eco, con la carrera terminada, una casilla de un Dominio con Elegido da `Confined`; una casilla de un Dominio Olvidado fuera de su tirada da `Roll`. Durante la carrera, y durante la Noche de los Ecos, la casilla del Dominio con Elegido da `Roll`. Para un Elegido, siempre `Roll`.
- **CA-TAB-32** (R-TAB-29, orden). Una casilla sellada en un Dominio con Elegido, para un Eco confinado sin Paso Libre, da `Sealed`.

En `Soulbound`:

- **CA-VIS-27** (R-VIS-26). La función que da el brillo del halo devuelve el mínimo al empezar el periodo, 1 a mitad de periodo y otra vez el mínimo al acabarlo; con periodo cero o negativo, 1 fijo.
- **CA-DEC-27** (R-DEC-15, humano). En el turno de tirada del humano con un dado que ha sacado 4, la vista de decisión lleva el título "Tu turno", el contexto "Has sacado", un dado de valor 4 y la instrucción "Elige una casilla que late.". Con dos dados que han sacado 3 y 5, lleva los dos, en ese orden. Con la repetición ofrecida, la instrucción es "Elige una casilla que late o repite la tirada.".
- **CA-DEC-28** (R-DEC-15, espera). En el turno de tirada de otro asiento, la vista de espera lleva "Ha sacado" y los dados de ese asiento.
- **CA-DEC-29** (R-DEC-15, depuración). Con la tirada fijada en 9 y sin valores de dados, la vista no lleva dados y el contexto es "Has sacado un 9".
- **CA-DEC-30** (R-DEC-15, la cara). La función que coloca los puntos de una cara devuelve tantos puntos como el valor, del 1 al 6, todos dentro de la cara y sin repetir posición; para un valor fuera de 1 a 6, ninguno. La función que da el trozo de la imagen devuelve, para la cara N, el sexto N del ancho (la cara 1 empieza en 0; la 6 acaba en 1) y todo el alto.
- **CA-DEC-31** (R-DEC-16, textos). La función del texto del aviso devuelve, para `Sealed`, `Confined` y `Roll` con tirada 3, los tres textos de la regla, letra por letra; con tirada 7, "Con un 7 no llegas ahí. Qué lástima.".
- **CA-DEC-32** (R-DEC-16, vida del aviso; corrección del autor del 15/11/2026 tras las pruebas en PIE). El aviso se ve desde el clic rechazado, en el punto del clic, hasta que pasan sus segundos; un segundo clic rechazado lo sustituye, lo lleva al punto nuevo y vuelve a contar (también con el mismo texto); un clic aceptado o el final del turno de tirada lo quitan. Se comprueba sobre una función o un objeto puro del tiempo, como `FWardenLineQueue`. La colocación (R-DEC-16) también es una función pura con test: el aviso queda dentro del viewport y sin tapar la columna de paneles ni el panel de estado, para cualquier punto de clic.
- **CA-HUD-14** (precisión de R-HUD-1; corrección del autor del 15/11/2026 tras las pruebas en PIE). La línea de la tirada del panel de estado dice "Dado: 4" con un dado y "Dados: 8" con dos, y la Ayuda dice "clic en una casilla que late"; pero en el turno de tirada del humano, con el panel de decisión a la vista, esa línea no sale. En el turno de otro asiento, "Dado: N" o "Dados: N" sigue, sin Ayuda.
- **CA-ACT-8** (R-ACT). El texto del motivo para la línea "humano" es "dado", "Eco" y "sello" para `Roll`, `Confined` y `Sealed`.

Manuales, por el autor, en PIE, en la vista sentada y en la cenital, a 1920x1080 y a 1024x768, con captura:

- En su turno, las casillas alcanzables laten y se distinguen de un vistazo del resto del tablero, también en la vista sentada. El latido no molesta ni tapa la marca del Sello ni el destello de una Fuente.
- El panel de decisión enseña "Has sacado" y la cara del dado dibujada por código (sin imagen importada), legible a 1024x768. Con Celeridad y dos dados (con `SbGiveRune`, la runa que da Celeridad), las dos caras.
- En el turno de un dummy, "Ha sacado" y su dado.
- Clic en una casilla lejana: sale "Con un N no llegas ahí. Qué lástima." con su tirada, y se quita solo. Clic en una casilla sellada (`SbSeal`): la frase del Sello. Como Eco tras la carrera, clic en un Dominio con Elegido: la frase del Eco.
- El aviso no tapa ni sustituye una frase del Warden que esté en pantalla.
- Con `SbRoll` (tirada fijada): "Has sacado un N", sin dado, sin errores en el registro.
- El acta de la partida lleva el motivo en las líneas de destino rechazado.

## Notas para quien lo implemente

- **Los dados en el motor.** `DrawDice` ya saca `Dice::MaxCount` valores y `SumDice` suma los primeros `DiceCount` (`TurnRules.cpp`). Guardar es copiar esos primeros valores al estado en `EnterRollAndChoose` y en la repetición (`EChoiceKind::Reroll`), y vaciarlos donde hoy se hace `State.Roll = 0`. El glosario dice que ningún valor **futuro** del sorteo se guarda en el estado: estos son los de la tirada ya hecha, y los de la repetición no se guardan hasta que se repite.
- **El motivo.** `IsOutsideConfinement` es hoy privada de `MovementRules.cpp`. Para que el motivo use la misma comprobación hay que hacerla accesible o poner la función nueva junto a ella; se propone en el plan. La presentación no repite la regla: pregunta al motor.
- **El latido.** Los halos son `ABoardViewActor::Halos`, una pieza por casilla, pintados con `BaseMaterial` y `ReachableColor`. `RefreshFlashes` ya cambia el color de un halo mientras dura un destello de Fuente y lo devuelve al acabar: el latido tiene que convivir con eso sin pelearse (el destello manda). El brillo se cambia solo en los halos visibles.
- **El panel.** `FDecisionView` necesita llevar los valores de los dados (campo nuevo, nombre a proponer) y `SDecisionPanel` pintarlos tras el contexto. La carga de la imagen y su respaldo siguen el patrón de `RingRuneTexture` / `RuneTexture` / `RuneBrush` en `BoardViewActor` (S-031): un `UPROPERTY` mantiene viva la textura y el pincel la apunta. Los puntos de la cara y el trozo de la imagen salen de funciones puras con test.
- **El aviso.** `UMatchHostSubsystem::SubmitHumanIntent` ya devuelve el `EIntentResult` y escribe la línea "humano" del acta (`RecordHuman`). El aviso lo enseña la vista al recibir `Unreachable`; su vida (cuándo se ve) va en algo puro y con test, no en el actor.
- **Textos que ya nombran la casilla "iluminada" o "blanca"** (leyenda, Ayudas, `como-se-juega.md` si lo hace): se listan en el plan; los de código se alinean en esta story, los de datos del autor se le señalan y no se tocan.
- **Tests que hoy esperan "Tu turno: tirada N" o "Tirada: N"**: se actualizan en el mismo commit que el cambio de texto, no se borran.
- Lo aprendido en S-029, S-033 y S-037: para teñir, `BaseMaterial` (parámetro "Color"); tamaños y tiempos en Config con valor inicial documentado; lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas.
- Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE, no se compila ni se lanza nada.

## Fuera de esta story

- La imagen del dado: la importa el autor en el editor cuando la tenga, en commit aparte. La story deja la ruta y el respaldo.
- Cambiar qué casillas son alcanzables, la regla del Eco o la del Sello.
- La luz de los Nichos y de las Fuentes, y el suelo de los Dominios Olvidados (story siguiente, con assets del autor).
- La señal de corte del Warden, el botón "Empezar" de las pruebas y el orbe del Oráculo (story aparte).
- Un sonido al hacer clic en una casilla inalcanzable.
- Enseñar el valor de cada dado en la crónica o en el evento `Rolled`.
