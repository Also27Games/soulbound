# S-022. Arranque mínimo: el exe se puede poner delante de alguien que no conoce el juego

**Sistema**: interfaz (GDD 07: R-CAM-1, R-HUD-4, decisión del autor del 22/10/2026 sobre las ayudas), el Warden (GDD 06: R-VOZ-3b, la crónica con la voz). **Módulos**: `Soulbound` solo; `SoulboundCore` no se toca. **Rama base**: `main`. **Tamaño**: medio. Es la última story antes del test de usabilidad (9-15/11/2026): lo que no entre aquí, lo verán dos personas ajenas al proyecto.

## Contexto

El motor está completo y el exe empaquetado arranca una partida desde el menú de Ultra Game Template (UGT), pero arranca como una herramienta de depuración: cámara cenital, menú con las imágenes de ejemplo de UGT y menús que no aplican, sin ninguna explicación de qué hay que hacer, la crónica repite cada golpe en dos líneas (la descriptiva y la frase del Warden) y al acabar la partida no hay forma de volver al menú. Esta story hace que una persona que no ha leído nada pueda abrir el exe, leer en dos pantallas de qué va, empezar una partida, jugarla con el Warden hablándole y volver al menú.

Dos clases de trabajo, separadas a propósito porque `Content/UltraGameTemplate` **no se versiona** (licencia de UGT):

- **Código y datos**, del agente, en esta story: lo que UGT necesita leer o llamar lo expone C++ como funciones de Blueprint; los textos van en `Content/Data`, que se empaqueta como archivos sueltos (`DirectoriesToAlwaysStageAsUFS`); la vista arranca sentada; la crónica deja de duplicar; y hay una forma de volver al menú al acabar.
- **Pasos manuales en el editor**, del autor, después de fusionar: imágenes del menú, qué menús quedan, el ajuste de ayudas en opciones, la página "cómo se juega" y los ajustes de vídeo por defecto de UGT. El agente deja escritos esos pasos en el README, en la lista que ya existe para UGT (apartado 3, punto 6), y el autor los corrige al hacerlos. Ningún asset de Unreal entra en esta story.

Decidido por el autor (27-28/10/2026, ESTADO): la vista sentada por defecto; los ajustes de vídeo de UGT iguales a los del proyecto (sin Lumen, sin sombras virtuales, sin trazado de rayos: hoy UGT los pisa y salen avisos rojos); configurar UGT con imágenes propias, quitar los menús que no aplican y añadir el ajuste de ayudas automático/siempre/nunca que lee el perfil de S-021; una página "cómo se juega"; y que en la crónica sobre la línea descriptiva de cada golpe ahora que el Warden habla.

## Reglas a implementar

### Código (agente)

- **R-CAM-1. Sentado por defecto.** `ABoardViewActor` arranca en la vista sentada del asiento humano. Una propiedad `bStartSeated` (Config, `DefaultGame.ini`, sección `[/Script/Soulbound.BoardViewActor]`, valor inicial `true`; nombre propuesto) lo decide, para poder volver a la cenital sin compilar. La tecla C sigue alternando; `DescribeViewKeys` dice la vista actual. En el editor, la vista previa (`ABoardPreviewActor`) no cambia.
- **Crónica sin duplicar (R-HUD-4, R-VOZ-3b).** En la crónica del juego (`DescribeChronicleEvent`), cuando un evento va seguido inmediatamente de su `WardenSpoke`, los dos se enseñan en **una sola línea** con una etiqueta corta y la frase: `golpe "Impuesto": <frase>`, `escena "Cosecha": <frase>`, `castigo leve: <frase>`, `Furia: <frase>`, `comentario: <frase>`, `aviso: <frase>`; el resto de la información que hoy lleva la línea descriptiva (a quién dirige, cuántas opciones, qué corta) no se pierde: ya está en las líneas de `EffectApplied`, `TurnInterrupted` y las elecciones. Igual que se hizo con la sentencia del Oráculo en S-021. Sin tabla de frases (tests de reglas, `SbWardenMode off`), las líneas de siempre. La crónica de la simulación (`MatchSimulation.cpp`) no cambia.
- **El ajuste de ayudas para UGT.** `UPlayerProfileSubsystem` expone a Blueprint `GetHints` y `SetHints` (`UFUNCTION(BlueprintCallable)`), y `EHintsMode` pasa a `BlueprintType` con `DisplayName` en español ("Automático", "Siempre", "Nunca"). Como los menús de opciones de UGT trabajan con un índice de combo, dos funciones puras con test en `Presentation/InstructionTracker.h`: `HintsModeToIndex` y `HintsModeFromIndex` (nombres propuestos), con el orden automático 0, siempre 1, nunca 2, y fuera de rango automático. El perfil se guarda al cambiar, como con `SbHints`.
- **"Cómo se juega" desde datos.** El texto vive en `Content/Data/Textos/como-se-juega.md` (nombre propuesto): Markdown sencillo, tres apartados con título (`## Qué es esto`, `## Cómo se gana`, `## El Warden`), UTF-8. Un cargador en `Soulbound` (`LoadHowToPlayText` → resultado con el texto o el error, nunca lanza; nombre propuesto) y una función de Blueprint (`UHowToPlayLibrary::HowToPlayText`, `BlueprintPure`, devuelve `FText`; nombre propuesto) que UGT enlaza a un bloque de texto. El agente escribe el archivo con los tres títulos y `(pendiente)` bajo cada uno: **el texto es del autor**, como las frases, y entra con su commit de datos. Es contenido de juego: nada de inventar reglas en él.
- **Volver al menú.** Con la partida terminada (`IsMatchOver`), una tecla (valor inicial `Escape`, Config como las demás teclas de la vista; nombre propuesto `ReturnToMenuKey`) vuelve al mapa del menú (`GameDefaultMap` de `DefaultEngine.ini`, leído, no escrito a mano) tras cerrar la sesión en línea si la hay. La pantalla del resultado de S-018 lo dice: "Escape: volver al menú". Antes del final, Escape sigue siendo lo que sea hoy (el menú de pausa de UGT, que no detiene nada, R-FLU-5).
- **README.** En el apartado "Desarrollo en Windows", punto 6 (UGT), el agente añade los pasos manuales que el autor hará en el editor, en el orden en que conviene hacerlos y con el nombre exacto de la función o propiedad de C++ que cada uno enlaza: imágenes del menú (texturas del autor, fuera del repo hasta que las haga), menús que quedan (Nueva partida, Opciones, Cómo se juega, Salir) y los que se quitan, ajuste de ayudas en Opciones › Juego enlazado a `GetHints`/`SetHints` con el índice, página "cómo se juega" enlazada a `HowToPlayText`, vídeo por defecto igual al proyecto (Lumen, sombras virtuales y trazado de rayos apagados, antialiasing MSAA 4 como `DefaultEngine.ini`). El autor corrige la lista al hacerla.

### Pasos manuales (autor, después de fusionar, con captura en el PR de datos)

Imágenes del menú, menús, ajuste de ayudas, página "cómo se juega", vídeo por defecto. Con los pasos del README. Assets solo del autor, en commit aparte, verificados en el editor. El texto de "cómo se juega" lo escribe el autor con el tutor y entra en `Content/Data/Textos/como-se-juega.md` en el mismo commit de datos que `frases.json`.

## Criterios de aceptación

Automáticos, en `Soulbound`:

- **CA-CRO-1** (crónica sin duplicar). Sobre un registro armado a mano con la tabla de frases: `WardenEventStarted` seguido de su `WardenSpoke` da una sola línea con la etiqueta del canal, el nombre del evento y la frase, y la línea del `WardenSpoke` queda vacía; lo mismo para `InsolencePunished` (con su gravedad), `FuryUnleashed`, `WardenRemark` y `MatchEndWarning`; un `WardenEventStarted` sin `WardenSpoke` detrás conserva la línea descriptiva de hoy; con la tabla vacía, todo como hoy; la sentencia del Oráculo sigue siendo una línea.
- **CA-AYU-1** (índice de ayudas). `HintsModeToIndex` y `HintsModeFromIndex` son inversas para los tres valores; un índice fuera de rango da automático.
- **CA-CSJ-1** (cómo se juega). El cargador acepta el archivo del repositorio, devuelve texto no vacío con los tres títulos en orden, y falla con el error de archivo si no existe. Sin lista de títulos escrita en dos sitios: los títulos exigidos son una constante.
- **Texto.** `DescribeViewKeys` nombra la vista actual (sentada o cenital) y la tecla que la alterna; la pantalla del resultado incluye "Escape: volver al menú".

Manuales, por el autor, en PIE y en el exe empaquetado con captura:

- PIE arranca sentado en el asiento humano; C pasa a cenital y vuelve. Con `bStartSeated=false` en el ini, arranca cenital.
- Una partida con `SbStrike`: la crónica enseña una sola línea por golpe, con el nombre del evento y la frase; `SbEvent` por Oráculo sigue dando una sola línea de sentencia.
- Al terminar la partida (`SbClock 2395` y esperar, o victoria), Escape vuelve al menú de UGT; antes del final, Escape abre la pausa de UGT como hoy.
- Exe empaquetado (`build.ps1 -Que package`): arranca sin los avisos rojos de Lumen y trazado de rayos tras el paso manual de vídeo; el menú tiene Nueva partida, Opciones (con Ayudas en Juego, que cambia el perfil: comprobar con `SbHints` sin argumentos), Cómo se juega (con el texto del archivo) y Salir.

## Notas para quien lo implemente

- Nombres del glosario: vista sentada, cenital, crónica (`Chronicle`), golpe, escena, comentario, castigo de insolencia, Furia, aviso del final, perfil del jugador, ayudas (`EHintsMode`). Nombres nuevos que propone esta story y confirma el plan: `bStartSeated`, `ReturnToMenuKey`, `HintsModeToIndex`/`HintsModeFromIndex`, `LoadHowToPlayText`, `UHowToPlayLibrary::HowToPlayText`, `Content/Data/Textos/como-se-juega.md`. Van al glosario con el PR.
- **Reutilizar**: la fusión de líneas sigue el patrón de la sentencia del Oráculo en `DescribeChronicleEvent` (S-021); el cargador de texto, el de `frases.json` (`FFileHelper`, resultado con error, nunca lanza); las teclas Config, las de la vista (`SeatedViewKey`); el perfil ya tiene `GetHints`/`SetHints` en C++, solo se exponen.
- **Nada de `Content/UltraGameTemplate`** ni de ningún `.uasset`: lo que UGT tenga que hacer se describe en el README y lo hace el autor en el editor. Si el agente ve que algo no se puede enlazar desde Blueprint sin un asset, para y lo dice en el PR.
- **El texto de "cómo se juega" no lo escribe el agente**: deja los títulos y `(pendiente)`. Si el test necesita contenido bajo los títulos, con `(pendiente)` basta.
- **Volver al menú** en el spike: hoy `GameDefaultMap` apunta al `UGT_LauncherMap`. Leerlo de `UGameMapsSettings`, no copiar la ruta.
- Tests en `Source/Soulbound/Private/Tests/`, patrón de `WardenVoiceTextTest.cpp` e `InstructionTrackerTest.cpp` (funciones puras, sin mundo). La vista y la tecla solo tienen criterio manual.

## Fuera de esta story

- Las imágenes del menú y la máscara del Warden: arte del autor, assets en commit aparte.
- El texto de "cómo se juega" y `frases.json`: datos del autor, commit propio.
- Audio del Warden (corte B de S-021), la interfaz en plano de verdad (GDD 07: tablero que gira, Grimorio, foco de evento, retratos), la pantalla de postpartida: después del test de usabilidad.
- Un instalador o la página de tienda: despliegue, 16-20/11.
