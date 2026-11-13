# S-036. La dificultad no se lee: el anillo y el panel de estado no dicen en qué nivel se juega, y el panel de depuración sí

**Sistema**: interfaz en plano (GDD 07: el panel del anillo, R-VIS-14 a R-VIS-19; el panel de estado, R-HUD-1). **Módulos**: `Soulbound` (presentación: el título del anillo y `Presentation/StatusText`). `SoulboundCore` no cambia. **Rama base**: `main`, con S-035 fusionada. **Tamaño**: muy pequeño.

## Contexto

El 13/11/2026, al elegir la frase de Ayuda del Vacío, el autor quitó de ella "cada vez será más fácil" y decidió lo mismo para la pantalla: el nivel de dificultad de una prueba no es algo que el jugador deba leer. Lo tiene que notar en la franja y en la velocidad del aro. Es la misma decisión que en S-031 sacó del anillo las cifras y la palabra "rendimiento".

Hoy el nivel sale en dos sitios que ve el jugador:

- El título del anillo: "PRUEBA DEL WARDEN · normal", "ESCAPE DEL VACÍO · difícil" (`ABoardViewActor::RingTitle`).
- El panel de estado, en la línea de la prueba: "Escape del Vacío: el asiento 0 juega en difícil…", y lo mismo en la Prueba del Warden y en el desafío final (`StatusLines`, `Presentation/StatusText.cpp`).

Decidido por el autor el 13/11/2026: **se quita de los dos**. La crónica y el acta de partida no cambian.

Corrección del 13/11/2026, al revisar el plan: el panel de depuración (tecla D) **no dice hoy el nivel** (`DebugLines` no usa la dificultad), así que al quitarlo de esos dos sitios el autor dejaría de verlo mientras se juega una prueba. Decidido por el autor: **el panel de depuración gana una línea con el nivel de la prueba pendiente**.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-VIS-22 (precisión de R-VIS-14). El título del anillo dice solo el uso.** "DUELO", "FUENTE DE UN DOMINIO OLVIDADO", "PRUEBA DEL WARDEN", "ESCAPE DEL VACÍO", "DESAFÍO FINAL" o "PRUEBA", sin el separador ni el nivel. La dificultad sigue decidiendo los parámetros del anillo (`RingParamsForTrial`), que no se tocan.
- **R-HUD-1 (precisión). La línea de la prueba en el panel de estado no dice el nivel.** Las tres líneas de `StatusLines` (desafío final, escape del Vacío y Prueba del Warden) pierden "en <nivel>" y quedan con el resto igual: quién juega, cuándo y cómo se resuelve.
- **R-HUD-1 (precisión). El panel de depuración dice el nivel.** Mientras hay una prueba de habilidad pendiente (`ETurnPhase::SkillTrial`), `DebugLines` lleva una línea "Prueba pendiente: <uso>, nivel <nivel>", con el uso tal como lo dice el título del anillo y el nivel de `DifficultyText(State.PendingDifficulty)`. Sin prueba pendiente, la línea no está. El duelo queda fuera: su nivel no está en `PendingDifficulty`.
- **Lo que no cambia.** La crónica (`EventText`) y el acta siguen diciendo el nivel. `DifficultyText` se queda, porque lo usan ellas y la línea nueva.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo:

- **CA-VIS-20** (título, R-VIS-22). Para cada uso (duelo, Fuente de un Dominio Olvidado, Prueba del Warden, escape del Vacío, desafío final) y cada dificultad, el título es el nombre del uso y no contiene ni "·" ni "fácil", "normal" o "difícil". Dos dificultades distintas del mismo uso dan el mismo título.
- **CA-HUD-11** (panel de estado, R-HUD-1). Con una prueba pendiente de cada tipo (desafío final, escape del Vacío, Prueba del Warden) en cada dificultad, ninguna línea de `StatusLines` contiene "fácil", "normal" ni "difícil", y la línea de la prueba sigue nombrando al asiento que juega.
- **CA-HUD-12** (lo que no cambia). La línea de la crónica de una Prueba del Warden superada sigue diciendo su nivel.
- **CA-HUD-13** (depuración, R-HUD-1). Con un escape del Vacío pendiente en difícil, `DebugLines` tiene una línea con "ESCAPE DEL VACÍO" y "difícil"; con una Prueba del Warden en normal, con "PRUEBA DEL WARDEN" y "normal"; sin prueba pendiente, ninguna línea de `DebugLines` empieza por "Prueba pendiente".

Manuales, por el autor, en PIE, con captura:

- Una Prueba del Warden (`SbEvent` con un evento que la mande, o `SbTrial`) y un escape del Vacío (`SbBanish`): el título del anillo dice solo el uso, y el panel de estado no dice el nivel.
- Con la prueba abierta, el panel de depuración (`D`) dice "Prueba pendiente" con su uso y su nivel, y la línea desaparece al acabar. La pestaña Crónica sigue diciendo el nivel cuando la prueba termina.

## Notas para quien lo implemente

- `RingTitle` es hoy un método estático de `ABoardViewActor` sin test. Para probarlo sin mundo conviene que el texto salga de una función pura junto al modelo del panel (`Presentation/RingPanelModel.h`); el nombre se propone en el plan. Si deja de usar la dificultad, el parámetro sobra: se quita, y con él lo que solo existía para pasárselo.
- En `StatusLines` la variable `Difficulty` deja de usarse en esas tres líneas; que no quede código muerto.
- Tests primero, en rojo. Si algún test de hoy fija el texto con el nivel, se cambia antes que el código.
- Nada que no se pueda ver sin PIE se rehace a ciegas: se anota para la prueba manual.

## Fuera de esta story

- Quitar el nivel de la crónica: comparte texto con el acta, que es del autor y lo necesita; separarlas es otra story, y la crónica no estará en la interfaz definitiva.
- La mención a `SbTrial` en la línea de la prueba del panel de estado ("con el anillo: clic izquierdo, o con SbTrial"): es un comando de depuración a la vista del jugador. Se anota para que el autor decida; aquí no se toca.
- Cambiar los niveles o sus parámetros (`anillo.json`).
