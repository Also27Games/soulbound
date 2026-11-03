# S-024. La partida espera al telón: el prólogo no empieza hasta que la pantalla de carga se va

**Sistema**: el Warden (GDD 06: R-PRO-1, precisión), interfaz (GDD 07: pantalla de carga). **Módulos**: solo `Soulbound` (una regla sin mundo nueva junto a `FStepClock`, el anfitrión y la vista). `SoulboundCore` no cambia. **Rama base**: `main`, con S-023 (PR #53) fusionada. **Tamaño**: pequeño.

## Contexto

Con la pantalla de carga de Ultra Loading Screen puesta (3/11/2026), el autor vio que el prólogo del Warden empieza con la barra de carga todavía en pantalla: al quitarse, la primera frase ya está dicha a medias. El anfitrión de partida fija el final del prólogo en `OnWorldBeginPlay`, y la cola de frases de la vista empieza a contar en su primer tick; las dos cosas pasan mientras el jugador aún ve "Cargando".

Decidido por el autor el 3/11/2026:

- **La partida espera al telón.** Mientras el telón está bajado, ni el prólogo ni sus frases empiezan. La presentación lo sube cuando la pantalla de carga se ha ido: Ultra Loading Screen avisa con un evento de Blueprint (nombre exacto: lo confirma el autor en el PR; la conexión es un paso manual del README porque el plugin no se versiona).
- **Tope de espera.** Si nadie sube el telón, se sube solo pasado un tiempo: parámetro de juego `CurtainTimeoutSeconds`, valor inicial propuesto 6 s (duda para el autor: en PIE sin pantalla de carga, ese tope es lo que tarda en empezar cada partida). Así el juego no depende de la conexión en Blueprint para arrancar.
- **Nombre propuesto**: "telón" (`Curtain`). No está en el glosario; el PR lo propone.

Fuera: esperar también sin prólogo (hoy sin frases la partida arranca al instante y se queda así), la aparición del Warden en escena y cualquier cambio en `SoulboundCore`.

## Reglas a implementar

### Regla sin mundo (`Soulbound`, `Match/Curtain.h`, junto a `StepClock.h`)

- **R-PRO-1 (precisión). El telón.** `FCurtain` (nombre propuesto): struct sin mundo, como `FStepClock`. Recibe el tiempo del mundo como dato. `Lower(WorldNow, TimeoutSeconds)` lo baja y anota desde cuándo; `Raise(WorldNow)` lo sube y devuelve si estaba bajado (subirlo dos veces no hace nada la segunda); `IsDown()`; `TimedOut(WorldNow)` dice si el tope ha vencido con el telón bajado. Con `TimeoutSeconds` cero o negativo, el tope vence en el acto.

### Anfitrión y vista (`Soulbound`)

- **El anfitrión** (`UMatchHostSubsystem`): en `OnWorldBeginPlay`, si hay prólogo, baja el telón en vez de fijar `PrologueEndsWorldSeconds`; el reloj de partida queda detenido como hoy (`FStepClock::Hold`). `RaiseCurtain()` (`UFUNCTION(BlueprintCallable, Category = "Partida")`) lo sube y entonces fija el final del prólogo a `OpeningSeconds` desde ese instante, con una línea en el log. `IsCurtainDown()` (`BlueprintPure`). En `Advance()`, con el telón bajado: si el tope ha vencido, se sube con motivo "tope" en el log; si no, nada avanza. `EndPrologue` (lo llaman `SbEvent`, `SbStrike` y `SbClock`) sube el telón antes, para que las herramientas de depuración sigan funcionando. `CurtainTimeoutSeconds`: `UPROPERTY(Config, EditAnywhere)` del anfitrión, valor inicial en `Config/DefaultGame.ini`, como los demás parámetros del anfitrión.
- **La vista** (`ABoardViewActor`): mientras el anfitrión tiene el telón bajado no pide la frase actual a la cola (`WardenLineNow`), así la primera frase empieza a contar cuando el telón sube. La línea del Warden y el HUD no enseñan nada del Warden mientras tanto.
- **Conexión** (paso manual del README, no se versiona): en el evento de fin de carga de Ultra Loading Screen, obtener el subsistema de mundo `MatchHostSubsystem` y, si es válido (en los menús no existe), llamar a `RaiseCurtain`.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Source/Soulbound/Private/Tests/CurtainTest.cpp`, nombres `Soulbound.Game.Match.Telon.<QueSeComprueba>`, como `StepClockTest.cpp`):

- **CA-TEL-1** (espera). Un telón bajado en el segundo 10 con tope 6: `IsDown()` verdadero; `TimedOut(12)` falso; `Raise(12)` devuelve verdadero y deja `IsDown()` falso; `Raise(13)` devuelve falso.
- **CA-TEL-2** (tope). Bajado en el 10 con tope 6: `TimedOut(15.9)` falso, `TimedOut(16)` verdadero; con tope 0 o negativo, `TimedOut(10)` verdadero. Subido, `TimedOut` es falso siempre.
- **CA-TEL-3** (sin bajar). Un telón recién creado no está bajado, `Raise` devuelve falso y `TimedOut` es falso.

Manuales, por el autor, con captura:

- Standalone con `Saved` borrado: la barra de carga se va y **después** aparece la primera frase del Warden entera; el log dice "Telón subido (presentación)" y luego "Prólogo: la partida empieza dentro de N s".
- PIE sin pantalla de carga: el log dice "Telón subido (tope)" a los `CurtainTimeoutSeconds` y el prólogo empieza entonces.
- `SbStrike` con el telón bajado: el telón sube, el prólogo acaba y el golpe se lanza, como hoy.

## Notas para quien lo implemente

- Nombres del glosario: prólogo (`OpeningSeconds`, `IsInPrologue`), reloj de partida, paso a paso (`FStepClock`), línea del Warden. Nombre nuevo que propone esta story: telón (`FCurtain`, `RaiseCurtain`, `IsCurtainDown`, `CurtainTimeoutSeconds`). Va al glosario con el PR.
- **Reutilizar** el patrón de `FStepClock`: la regla recibe el tiempo del mundo y no sabe de `UWorld`; el anfitrión decide cuándo llamarla. Nada de timers de Unreal.
- No tocar `FStepClock` ni `OpeningSeconds`: el prólogo sigue durando lo mismo; solo cambia desde cuándo se cuenta.
- La espera del Warden (S-023, `bWardenSpeaking`) no interviene: con el telón bajado todavía no hay frases enseñadas.
- Tests primero, en rojo; el anfitrión y la vista solo tienen criterio manual, como en S-022 y S-023.

## Fuera de esta story

- La conexión en el Blueprint de Ultra Loading Screen: paso manual del README, lo hace el autor.
- Que el HUD de depuración se dibuje por encima de la pantalla de carga: cosmético, story de interfaz.
- Esperar al telón sin prólogo: no se pide.
