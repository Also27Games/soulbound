# S-043. Se ve cuándo es la Noche de los Ecos: el tablero en penumbra, una línea que lo explica y un final con voz

**Sistema**: efectos del Warden (R-EFE-18, la Noche de los Ecos), la voz del Warden (R-VOZ) e interfaz en plano (GDD 07; pilar 3, caos legible). **Módulos**: `SoulboundCore` (el Warden dice una frase cuando la Noche termina por el tope de rondas) y `Soulbound` (presentación: `WardenLineQueue`, `BoardViewActor`, `StatusText`); datos (`Content/Data/Warden/frases.json`). **Rama base**: `main`, con S-042 fusionada. **Tamaño**: pequeño. **Sin assets**.

## Contexto

Visto por el autor el 16/11/2026, probando las stories anteriores: no queda claro cuándo está activa la Noche de los Ecos y cuándo no. Es el único momento en que un Eco puede entrar en el Dominio de un Elegido, así que cambia lo que puede hacer media mesa, y hoy casi no se enseña:

- Al empezar, el Warden dice su frase ("Que llegue la oscuridad…"), que no dice qué significa.
- Mientras dura, solo hay una línea pequeña en el panel de estado, entre las demás.
- El tablero no cambia.
- Al terminar por el tope de rondas, que es el final normal, no lo anuncia nadie: queda una línea en la crónica y los Ecos vuelven a su hogar sin aviso.

Decidido por el autor el 16/11/2026 (ninguna regla de la Noche cambia: ni cuándo empieza, ni cuánto dura, ni qué permite):

- Mientras dura, el tablero está en penumbra.
- Una línea en la caja del objetivo dice qué significa.
- Empieza y termina con un aviso claro, y el final por el tope de rondas tiene una frase del Warden.

## Reglas a implementar

### `SoulboundCore`

- **R-VOZ-9 (nueva). El Warden dice que la Noche ha terminado.** Cuando la Noche de los Ecos termina por el tope de rondas (`EEchoNightEnd::RoundLimit`), el motor dice la frase de clave `programado:fin-noche-ecos` (nombre a proponer), en el mismo instante que el evento `EchoNightEnded` y justo después de él, con la función con que ya dice las frases programadas (`SpeakWardenLine`). Cuando termina por la Ley de Sombras (`EEchoNightEnd::ShadowLaw`) no dice esta frase: ese evento ya trae la suya.
  - La clave pasa a ser una de las exigidas (`RequiredWardenLineKeys`) y detiene la mesa como las demás frases programadas (no se añade a las excepciones de `LineHoldsTheTable`).
  - Sin tabla de frases (partida sin voz), no se dice nada y nada más cambia.
- **Datos.** En `frases.json`, la entrada nueva con estas tres variantes, del autor (16/11/2026):
  1. "Se acabó la noche. Ecos, a vuestras ruinas… el paseo ha terminado."
  2. "Amanece. Qué lástima… Volved a vuestros agujeros, Ecos."
  3. "La oscuridad se retira. Ecos: a casa. No me hagáis repetirlo."

### `Soulbound` (`Presentation/`)

- **R-VOZ-10 (nueva, sobre la línea pequeña de R-VOZ-8). La Noche empieza y termina con su línea pequeña.** Con el mismo mecanismo y el mismo aspecto que la línea pequeña del corte (S-041):
  - bajo la frase del evento que la empieza (`EchoNightBegan`): "Empieza la Noche de los Ecos: los Ecos caminan por cualquier Dominio.";
  - bajo la frase con que termina (`EchoNightEnded`), sea la nueva de R-VOZ-9 o la de la Ley de Sombras: "Termina la Noche de los Ecos: los Ecos vuelven a su hogar.".
  - No llevan palabra de corte ni los pulsos del oscurecido: no son un corte de turno. Si el evento que empieza o termina la Noche además corta un turno, valen las dos cosas: la línea pequeña dice primero lo del corte y después lo de la Noche (se propone en el plan cómo queda el texto).
  - La duración de la frase cuenta también los caracteres de esta línea pequeña, como en un corte.
- **R-VIS-31 (nueva). El tablero en penumbra.** Mientras la Noche de los Ecos está activa (`FMatchState::IsEchoNight`), una capa de color oscuro y azulado cubre el tablero, por debajo de todos los paneles y sin recibir clics. Aparece y se va con una transición corta. Color, opacidad y duración de la transición en Config, con valor inicial documentado (nombres a proponer; valores iniciales: un azul noche, opacidad 0,3 y 0,8 s).
  - Convive con los pulsos del oscurecido del corte (S-041): durante un pulso se ve el más oscuro de los dos, no la suma.
  - Sin posproceso (restricción de rendimiento de la skill). Con la partida terminada no hay penumbra.
  - Los halos que laten, las fichas y los números tienen que seguir leyéndose con la penumbra: se comprueba en la prueba manual y la opacidad se ajusta ahí.
- **R-HUD-10 (nueva). La línea de la Noche en la caja del objetivo.** Mientras la Noche está activa, la caja del objetivo (S-040) lleva una línea más, con el color de la línea del Warden: "Noche de los Ecos: los Ecos entran en cualquier Dominio". Va debajo del objetivo y, si lo hay, del recordatorio de la Prueba del Warden (S-042). Sigue las condiciones de la caja: no se ve en el prólogo ni mientras hay una frase del Warden en pantalla, y ninguna tecla la oculta.
  - La línea que hoy lleva el panel de estado ("NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio (como mucho hasta la ronda N)") se queda: es donde consta hasta qué ronda dura.
- **Lo que no cambia.** Cuándo lanza el Warden la Noche, cuánto dura, qué permite a los Ecos, que no obtengan runa en las Fuentes de los Dominios con Elegido y que vuelvan a su hogar al terminar.

## Criterios de aceptación

Automáticos, sin mundo.

En `SoulboundCore`:

- **CA-VOZ-11** (R-VOZ-9). Al terminar la Noche por el tope de rondas, el registro lleva `EchoNightEnded` y, justo después y en el mismo instante, un `WardenSpoke` de la clave del final de la Noche. Al terminar por la Ley de Sombras, no lleva ese `WardenSpoke`. Sin tabla de frases, no hay `WardenSpoke` y el resto del registro es el de antes.
- **CA-VOZ-12** (R-VOZ-9, datos). `frases.json` trae la clave nueva con sus tres variantes, y el cargador la exige: sin ella, la carga falla con un mensaje que la nombra. La frase detiene la mesa.

En `Soulbound`:

- **CA-VOZ-13** (R-VOZ-10). La función que empareja frases y líneas pequeñas devuelve, letra por letra: la línea de "Empieza…" bajo la frase del evento que abre la Noche; la de "Termina…" bajo la frase nueva al acabar por el tope de rondas; y la de "Termina…" bajo la frase de la Ley de Sombras. Ninguna lleva palabra de corte. Su duración cuenta los caracteres de la línea pequeña.
- **CA-VIS-32** (R-VIS-31). La función que da la opacidad de la penumbra en el tiempo vale 0 sin Noche, sube hasta la opacidad configurada al cumplirse la transición desde que empieza y baja hasta 0 al cumplirse desde que termina. La función que combina penumbra y pulso del corte devuelve el mayor de los dos.
- **CA-HUD-18** (R-HUD-10). Con la Noche activa, las líneas de la caja del objetivo incluyen "Noche de los Ecos: los Ecos entran en cualquier Dominio" con el color del Warden, debajo del objetivo y del recordatorio de la Prueba si lo hay; sin Noche, no. En el prólogo, nada.

Manuales, por el autor, en PIE, en la vista sentada y en la cenital, a 1920x1080 y a 1024x768, con captura:

- **Empieza** (`SbEvent noche-de-los-ecos`, entre turnos): la frase del Warden con "Empieza la Noche de los Ecos: los Ecos caminan por cualquier Dominio." debajo; el tablero pasa a penumbra; la caja del objetivo gana su línea.
- **Mientras dura**: se nota de un vistazo que es de noche, y los halos que laten, las fichas, los números de las casillas y el suelo agrietado de los Olvidados se siguen leyendo. Como Eco, se puede entrar en el Dominio de un Elegido.
- **Termina por el tope de rondas** (dejando pasar las rondas): el Warden dice una de las tres frases nuevas, con "Termina la Noche de los Ecos: los Ecos vuelven a su hogar." debajo; vuelve la luz; la línea de la caja desaparece; los Ecos están en su hogar.
- **Termina por la Ley de Sombras** (`SbEvent la-ley-de-sombras` con la Noche activa): la frase de ese evento con la misma línea pequeña debajo; vuelve la luz.
- **Un golpe durante la Noche** (`SbStrike tropiezo`): los pulsos del corte se ven sobre la penumbra.

## Notas para quien lo implemente

- **El motor.** La Noche empieza en `EffectRules.cpp` (`EchoNightBegan`, con la ronda tope en `Value`) y termina en el mismo archivo (`EchoNightEnded`, con el motivo `EEchoNightEnd` en `Detail`). Las frases programadas se dicen con `SpeakWardenLine` (por ejemplo, la del fin de la carrera en `BondRaceRules.cpp`) y sus claves están en `Warden/WardenLines.h` junto a `RequiredWardenLineKeys`.
- **Los tests que cuentan las claves** de `frases.json` (`WardenLinesTest.cpp`) cambian con la clave nueva: se actualizan en el mismo commit que el dato.
- **La línea pequeña.** `WardenLineItems` (`Presentation/WardenLineQueue.cpp`, S-041) ya empareja cada corte con su frase y le pone su línea pequeña; la Noche es otro caso del mismo emparejado. El caso de un evento que además corta un turno se resuelve ahí.
- **La penumbra.** La capa oscura del corte (S-041) es Slate, por debajo de los paneles; la penumbra puede ser la misma capa con otro cálculo de color y opacidad, o una hermana. Se propone en el plan.
- **La caja del objetivo.** `GoalPanelLines` (`Presentation/StatusText.cpp`, S-040 y S-042).
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Colores y tiempos, en Config con su valor inicial documentado. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- Cambiar cuándo empieza la Noche, cuánto dura o qué permite.
- Que un Eco obtenga runa en la Fuente de un Dominio con Elegido durante la Noche.
- Marcar sobre el tablero adónde puede ir un Eco (descartado por el autor en S-040: es llevar al jugador de la mano).
- Cambiar la frase con que el Warden empieza la Noche.
- Sonido o música de la Noche (Fase 2).
