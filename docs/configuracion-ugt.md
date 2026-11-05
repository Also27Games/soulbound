# Configuración de Ultra Game Template en Soulbound

Ultra Game Template (UGT) y Ultra Loading Screen (ULS) son assets de Fab con licencia propia: están en `Content/UltraGameTemplate/` y en el motor, ignorados por Git. Por eso sus ajustes no viajan con el repositorio y hay que rehacerlos a mano tras instalarlos. Esta guía deja UGT como está en el test de usabilidad (noviembre de 2026). Ninguno de los pasos toca código. Los assets propios que usan (logos, fondos y leyenda) sí están en el repositorio, en `Content/Art/Logos/`, `Content/Art/Carga/` y `Content/Art/Ayuda/`, y son arte generado por IA declarado como placeholder.

Orden de instalación: primero ULS en el motor (el plugin ya está activado en el proyecto), después UGT (Launcher > Add to Project), porque UGT se reparenta al GameInstance de ULS.

## Una regla antes de seguir

**UGT aplica sus ajustes guardados al arrancar y al abrir cada menú, por encima de los ini del proyecto.** Lo que puede sorprender no son sus menús sino lo que aplican; por eso se han quitado todas las filas sin función en Soulbound, y cualquier opción que se añada hay que comprobar qué comando ejecuta.

## Integración con Ultra Loading Screen y el telón

- `UGT_GameInstance`: Class Settings > Parent Class = `UltraLoadingScreen_GameInstance` (integración con Ultra Loading Screen; tutorial de cinco minutos del autor de ambos).
- `TransitionOrLoadingScreenWidget`: añadir los nodos `Show Ultra Loading Screen` y `Hide Ultra Loading Screen` donde indica el mismo tutorial.
- `UGT_GameInstance` (S-024, el telón): en el evento `FadeLoadingScreen` de `UltraLoadingScreen_GameInstance` (la clase padre), que avisa de que la pantalla de carga empieza a irse, *Get World Subsystem* (`MatchHostSubsystem`) › *Is Valid* › `RaiseCurtain`. En los menús el subsistema existe pero está inactivo y la llamada no hace nada; sin esta conexión el juego arranca igual, porque el telón se sube solo a los `CurtainTimeoutSeconds` (6 s, `DefaultGame.ini`). Comprobar en el log: "Telón subido (presentación)" después de "Login OK".
- Data Asset de configuración (`DefaultGameDataAsset`): `Disable Game Pause on Enter Pause Menu` activado (la pausa nunca detiene la partida); "nueva partida" apuntando al mapa del juego.

## Arranque y menús

- **Arranque limpio** (`DefaultGameDataAsset`): sin diapositiva del publisher, sin diapositiva de agradecimientos, sin mensaje de bienvenida al abrir el juego, sin enlaces a redes y sin la pantalla "Configuración inicial" del primer arranque (lo útil de ella ya está en Opciones › Vídeo). El texto de la esquina del menú: "Soulbound v0.1 · en desarrollo". El logo del estudio de la intro es `Content/Art/Logos/`.
- **Idioma**: castellano por defecto en `UGT_GameSettingsMenu`. Comprobar con la carpeta `Saved` borrada: el menú tiene que arrancar en castellano a la primera.
- **Menú principal**: Jugar, Configuración y Salir; el menú de créditos se quita hasta la entrega (volverá con los créditos reales). Fondo: la imagen de `Content/Art/Carga/` sin el Warden, en vez del escenario 3D de ejemplo; logo del juego de `Content/Art/Logos/` arriba a la izquierda. Para volver al menú al acabar la partida no hace falta nada en UGT: el juego pone un botón "Volver al menú" (Slate, en código) y la tecla Intro (`ReturnToMenuKey`).
- **"Cómo se juega"**: es la pantalla de nueva partida de UGT, la que sale cada vez que se pulsa Jugar (la de arranque queda desactivada). Título "Cómo se juega"; el bloque de texto, con ajuste de línea automático, se rellena en su Construct con el nodo *How To Play Text* (`UHowToPlayLibrary::HowToPlayText`, `BlueprintPure`), que devuelve `Content/Data/Textos/como-se-juega.md` con los títulos sin `## `; si el archivo falla, texto vacío y el motivo en el log. El texto no se pega nunca en el widget: se edita el archivo. A la izquierda del texto, una *Image* con la leyenda del tablero, `Content/Art/Ayuda/Ayuda`: una columna con nueve rótulos (Altar, Casilla, Nicho, Fuente, Duelo, Oráculo, Planar, Dominio Olvidado, Sello; generados con IA, placeholder como el resto del arte) y la pieza del kit de cada uno. El botón "¡Saber más!" se oculta; el de Jugar se queda, con su borde. Fondo negro.
- **Menú de pausa**: Reanudar, Configuración y Salir al menú principal, sin logo. El HUD de depuración se dibuja por encima de la pausa; es cosa del juego, no de UGT.

## Opciones

- **Opciones › Juego**: se quitan las filas de UGT (idiomas, subtítulos, tamaño de texto, dificultad, daño, temblores, balanceo, vibración, modo realista) y se añade **Ayudas**, un selector con Automático, Siempre y Nunca en este orden (los `DisplayName` de `EHintsMode`). En el Construct del menú, *Get Game Instance Subsystem* (`PlayerProfileSubsystem`) › `GetHintsIndex` selecciona el índice; al cambiar la selección, `SetHintsIndex` guarda el perfil; "Restablecer" pone 0. Comprobar que se recuerda al cerrar y abrir el juego, y en partida con `SbHints` sin argumentos.
- **Opciones › Audio**: el botón se oculta, no se borra (el juego no emite sonido; volverá con la story de audio).
- **Opciones › Vídeo**, en `UGT_VideoSettingsMenu`: se quitan el bloque de escaladores con IA (DLSS y FSR no tienen plugin; TSR y escalado simple sobran), campo de visión, HDR, Stat Unit, desenfoque de movimiento, y de la configuración avanzada las filas Lumen, Nanite, Distance Field Shadows, Lumen Hardware Raytracing y densidad de vegetación; también el botón de Steam Deck. Se quedan resolución, modo, aplicar, límite de FPS, gamma, anti-aliasing, V-Sync, preajuste global y las filas avanzadas restantes. En la función `ConfigGlobalPresets`, las seis ramas del preajuste (Ultra Low a Cinematic) escriben todas las variables de `UGT_VideoSave`: poner **Lumen, Nanite, Hardware Ray Tracing y Distance Field Shadows a Desactivado en las seis**; si no, el preajuste "Alto" vuelve a encender Lumen aunque el ini diga lo contrario. Comprobar en PIE que al abrir Opciones › Vídeo en la pausa no sale el aviso rojo de Lumen, y en el log del arranque del mapa `r.DynamicGlobalIlluminationMethod = "0"` y `r.Nanite = "0"`. El resto de valores por defecto de UGT (gamma, escalado, pantalla completa) se dejan hasta el test; la resolución se adapta sola a cada pantalla.

## Pantalla de carga

- **Pantalla de carga** (Ultra Loading Screen): la imagen de `Content/Art/Carga/` con el Warden (textura con *Compression* UserInterface2D y *Never Stream*), texto "Cargando", y el círculo, la barra y el porcentaje recolocados en la franja inferior de la imagen, que está vacía para eso. Al irse la imagen, el juego espera `CurtainGraceSeconds` (1,5 s) antes de que el Warden hable.
