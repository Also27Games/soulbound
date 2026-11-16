# S-043. Se ve cuándo es la Noche de los Ecos: el tablero en penumbra y un principio y un final con aviso y con voz

**Sistema**: efectos del Warden (R-EFE-18, la Noche de los Ecos), la voz del Warden (R-VOZ) e interfaz en plano (GDD 07; pilar 3, caos legible). **Módulos**: `SoulboundCore` (el Warden dice una frase cuando la Noche termina por el tope de rondas) y `Soulbound` (presentación: `WardenLineQueue`, `BoardViewActor`); datos (`Content/Data/Warden/frases.json`). **Rama base**: `main`, con S-042 fusionada. **Tamaño**: pequeño. **Sin assets**.

## Contexto

Visto por el autor el 16/11/2026, probando las stories anteriores: no queda claro cuándo está activa la Noche de los Ecos y cuándo no. Es el único momento en que un Eco puede entrar en el Dominio de un Elegido, así que cambia lo que puede hacer media mesa, y hoy casi no se enseña:

- Al empezar, el Warden dice su frase ("Que llegue la oscuridad…"), que no dice qué significa.
- Mientras dura, solo hay una línea pequeña en el panel de estado, entre las demás.
- El tablero no cambia.
- Al terminar por el tope de rondas, que es el final normal, no lo anuncia nadie: queda una línea en la crónica y los Ecos vuelven a su hogar sin aviso.

Decidido por el autor el 16/11/2026 (ninguna regla de la Noche cambia: ni cuándo empieza, ni cuánto dura, ni qué permite):

- Mientras dura, el tablero está en penumbra.
- Una línea fija dice qué significa mientras dura. Esa línea va arriba a la izquierda, con lo que afecta a toda la mesa, y no en la caja del objetivo, que es lo del jugador (decisión del autor del 16/11/2026): la pone la story de la pantalla limpia (S-045), no esta.
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
- **La línea fija de la Noche no es de esta story.** La que hoy lleva el panel de estado ("NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio (como mucho hasta la ronda N)") se queda como está; su sitio nuevo, bajo el reloj, lo pone S-045. **Corrección del autor del 17/11/2026, tras PIE**: quitar "(como mucho hasta la ronda N)" de lo que ve el jugador, porque no le interesa. La línea de la Noche del panel de estado y la de la crónica pasan a "NOCHE DE LOS ECOS: los Ecos caminan por cualquier Dominio"; el acta la conserva con la ronda, porque sirve para leer partidas. Tests: `Estado.NocheSinRonda` y `Cronica.NocheSinRonda` (vistos en rojo) y `Acta.NocheConRonda` (de regresión, pasa desde el principio).
- **R-VOZ-11 (nueva, añadida por el autor el 16/11/2026; no es de la Noche, pero toca las mismas líneas). El prefijo de la frase, como en un guion.** A quién habla el Warden va entre paréntesis y sin coma: "Warden (a ti): …", "Warden (al asiento 2): …" y, cuando habla a toda la mesa, "Warden: …". Sustituye a "Warden, a ti: …" y "Warden, al asiento 2: …", que se leían rígidos. Vale para todas las frases que llevan prefijo (`WardenLinePrefix`): la línea del Warden, la crónica y el acta. El orden con la palabra de corte no cambia: "Warden (a ti): ¡Quietos! …".
- **Lo que no cambia.** Cuándo lanza el Warden la Noche, cuánto dura, qué permite a los Ecos, que no obtengan runa en las Fuentes de los Dominios con Elegido y que vuelvan a su hogar al terminar.

### Decisiones del autor sobre el plan (17/11/2026)

- **Corte y Noche en la misma frase**: dos líneas pequeñas, primero la del corte como hoy y debajo la de la Noche.
- **Noche sin frase** (un evento que no la tiene, como `noche` de S-014, o una partida sin voz): la línea pequeña sale sola, sin prefijo, sin detener la mesa y sin protección frente a las burlas.
- **`FWardenLineItem::CutNote`** conserva el nombre y lleva también la línea de la Noche.
- **Con la partida terminada y la Noche activa**, la penumbra se va con su transición.
- Una sola capa para la penumbra y los pulsos del corte; la línea pequeña cuelga del principio y del final reales de la Noche, sea cual sea el evento que la abra o la cierre.
- Nombres: la clave es `programado:fin-noche-ecos` (`Soulbound::Warden::LineEchoNightEnd`); la Config de la penumbra, `EchoNightDimColor`, `EchoNightDimMaxOpacity` y `EchoNightDimFadeSeconds`.

## Criterios de aceptación

Automáticos, sin mundo.

En `SoulboundCore`:

- **CA-VOZ-11** (R-VOZ-9). Al terminar la Noche por el tope de rondas, el registro lleva `EchoNightEnded` y, justo después y en el mismo instante, un `WardenSpoke` de la clave del final de la Noche. Al terminar por la Ley de Sombras, no lleva ese `WardenSpoke`. Sin tabla de frases, no hay `WardenSpoke` y el resto del registro es el de antes.
- **CA-VOZ-12** (R-VOZ-9, datos). `frases.json` trae la clave nueva con sus tres variantes, y el cargador la exige: sin ella, la carga falla con un mensaje que la nombra. La frase detiene la mesa.

En `Soulbound`:

- **CA-VOZ-14** (R-VOZ-11). El prefijo es "Warden (a ti):" para el asiento humano, "Warden (al asiento 2):" para el asiento 2 y "Warden:" sin asiento. Los tests que hoy esperan "Warden, a ti:" o "Warden, al asiento N:" se actualizan en el mismo commit.
- **CA-VOZ-13** (R-VOZ-10). La función que empareja frases y líneas pequeñas devuelve, letra por letra: la línea de "Empieza…" bajo la frase del evento que abre la Noche; la de "Termina…" bajo la frase nueva al acabar por el tope de rondas; y la de "Termina…" bajo la frase de la Ley de Sombras. Ninguna lleva palabra de corte. Su duración cuenta los caracteres de la línea pequeña.
- **CA-VIS-32** (R-VIS-31). La función que da la opacidad de la penumbra en el tiempo vale 0 sin Noche, sube hasta la opacidad configurada al cumplirse la transición desde que empieza y baja hasta 0 al cumplirse desde que termina. La función que combina penumbra y pulso del corte devuelve el mayor de los dos.

Manuales, por el autor, en PIE, en la vista sentada y en la cenital, a 1920x1080 y a 1024x768, con captura:

- **Empieza** (`SbEvent noche-de-los-ecos`, entre turnos): la frase del Warden con "Empieza la Noche de los Ecos: los Ecos caminan por cualquier Dominio." debajo; el tablero pasa a penumbra.
- **Mientras dura**: se nota de un vistazo que es de noche, y los halos que laten, las fichas, los números de las casillas y el suelo agrietado de los Olvidados se siguen leyendo. Como Eco, se puede entrar en el Dominio de un Elegido.
- **Termina por el tope de rondas** (dejando pasar las rondas): el Warden dice una de las tres frases nuevas, con "Termina la Noche de los Ecos: los Ecos vuelven a su hogar." debajo; vuelve la luz; los Ecos están en su hogar.
- **Termina por la Ley de Sombras** (`SbEvent la-ley-de-sombras` con la Noche activa): la frase de ese evento con la misma línea pequeña debajo; vuelve la luz.
- **Un golpe durante la Noche** (`SbStrike tropiezo`): los pulsos del corte se ven sobre la penumbra.
- **El prefijo**: las frases dirigidas a ti dicen "Warden (a ti): …", las de otro jugador "Warden (al asiento N): …" y las de la mesa "Warden: …", en la línea del Warden y en la crónica.

**Resultado de la prueba manual del autor (17/11/2026).** Superadas todas: empieza la Noche (la frase con "Empieza la Noche de los Ecos…" debajo y el tablero en penumbra, con halos, fichas, números y suelo agrietado legibles); como Eco se entra en el Dominio de un Elegido; un golpe durante la Noche (los pulsos sobre la penumbra y vuelta a la penumbra); termina por la Ley de Sombras (su frase con "Termina la Noche de los Ecos…" debajo y vuelve la luz); termina por el tope de rondas con la frase nueva; y el prefijo "Warden (a ti):" y "Warden (al asiento N):" en la línea del Warden y en la crónica. La opacidad de 0,3 se queda. Dio una corrección: la ronda tope fuera del panel de estado y de la crónica (anotada arriba, en "La línea fija de la Noche").

## Notas para quien lo implemente

- **El motor.** La Noche empieza en `EffectRules.cpp` (`EchoNightBegan`, con la ronda tope en `Value`) y termina en el mismo archivo (`EchoNightEnded`, con el motivo `EEchoNightEnd` en `Detail`). Las frases programadas se dicen con `SpeakWardenLine` (por ejemplo, la del fin de la carrera en `BondRaceRules.cpp`) y sus claves están en `Warden/WardenLines.h` junto a `RequiredWardenLineKeys`.
- **Los tests que cuentan las claves** de `frases.json` (`WardenLinesTest.cpp`) cambian con la clave nueva: se actualizan en el mismo commit que el dato.
- **La línea pequeña.** `WardenLineItems` (`Presentation/WardenLineQueue.cpp`, S-041) ya empareja cada corte con su frase y le pone su línea pequeña; la Noche es otro caso del mismo emparejado. El caso de un evento que además corta un turno se resuelve ahí.
- **La penumbra.** La capa oscura del corte (S-041) es Slate, por debajo de los paneles; la penumbra puede ser la misma capa con otro cálculo de color y opacidad, o una hermana. Se propone en el plan.
- Lo que no se pueda ver sin PIE se anota para la prueba manual y no se rehace a ciegas. Colores y tiempos, en Config con su valor inicial documentado. Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- Cambiar cuándo empieza la Noche, cuánto dura o qué permite.
- Que un Eco obtenga runa en la Fuente de un Dominio con Elegido durante la Noche.
- Marcar sobre el tablero adónde puede ir un Eco (descartado por el autor en S-040: es llevar al jugador de la mano).
- Cambiar la frase con que el Warden empieza la Noche.
- Sonido o música de la Noche (Fase 2).
