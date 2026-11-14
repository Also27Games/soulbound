# S-038. Las Ayudas empiezan en "siempre": un perfil nuevo nace con las Ayudas en siempre, no en automático

**Sistema**: interfaz (GDD 07, ajuste "ayudas"; S-021, A3; S-022). **Módulos**: `Soulbound` (`Profile/`). `SoulboundCore` no cambia. **Rama base**: `main`, con S-037 fusionada. **Tamaño**: mínimo. **Sin assets**.

## Contexto

El 12/11/2026 el autor decidió que la build de los probadores lleve las Ayudas en "siempre". El 14/11/2026, al ir a ponerlo, vio que el desplegable de Opciones sale en "Automático": el valor con que nace un perfil no está en la plantilla, está en el código (`UPlayerProfile::Hints = EHintsMode::Automatic`, `Profile/PlayerProfile.h`).

Qué cambia para quien abre el juego por primera vez:

- En su primera partida, nada: en automático y en siempre el Warden enseña cada Ayuda.
- Desde la segunda, en automático las Ayudas ya no se repiten y, cuando el perfil tiene las diez instrucciones, tampoco salen las líneas de petición ("tu turno: clic en una casilla blanca"). En siempre, se repiten en cada partida.

Los probadores del 18 y 19/11 jugarán más de una partida, y una de ellos no es jugadora. Decidido por el autor el 14/11/2026: **un perfil nuevo nace en siempre**.

El autor ya ha cambiado en el editor el botón de restablecer de Opciones para que ponga "siempre" (asset de la plantilla, no versionado): esta story no lo toca.

## Reglas a implementar (`Soulbound`, `Profile/`)

- **R-AYU-1 (nueva). El valor inicial de las Ayudas es siempre.** Un perfil nuevo nace con `Hints` en `EHintsMode::Always`: el recién creado, el de una ranura que no existe y el de una ranura o unos datos que no se pueden leer.
- **El valor inicial vive en un solo sitio.** Hoy `UPlayerProfileSubsystem::GetHints` devuelve `Automatic` por su cuenta cuando no hay perfil cargado; debe devolver el mismo valor inicial del perfil, sin repetirlo.
- **Un perfil guardado conserva su ajuste.** Quien eligió automático o nunca y lo guardó, lo sigue teniendo al cargar.
- **Lo que no cambia.** El significado de los tres ajustes, el orden del combo (automático 0, siempre 1, nunca 2), que un índice fuera de rango dé automático (CA-AYU-1 de S-022), y el comando `SbHints`.
- **Glosario.** La entrada "Ayudas" dice "automático (valor inicial)": pasa a decirlo de siempre, con la fecha y el motivo. No hay nombres nuevos.

## Criterios de aceptación

Automáticos, en `Soulbound`:

- **CA-AYU-2** (valor inicial, R-AYU-1). Un perfil recién creado, el de una ranura que no existe y el de unos datos ilegibles tienen las Ayudas en siempre y la lista de instrucciones vacía. Sin perfil cargado, el subsistema da el mismo valor.
- **CA-AYU-3** (se conserva, R-AYU-1). Un perfil puesto en automático, guardado y vuelto a cargar, en memoria y por disco, sigue en automático; y lo mismo con nunca.

Manuales, por el autor, en PIE:

- Sin perfil guardado (borrando el archivo de la ranura `perfil` en `Saved/SaveGames`), Opciones enseña "Siempre" en las Ayudas, y en una segunda partida seguida el Warden vuelve a dar las Ayudas.
- Cambiando a "Nunca", saliendo y volviendo a entrar, sigue en "Nunca".

## Notas para quien lo implemente

- Tests primero, en rojo. `PlayerProfileTest.cpp` fija hoy el valor inicial en automático en dos sitios ("Ajuste inicial automático" y "Y automático"): se cambian antes que el código y se ven fallar.
- Unreal guarda de un `USaveGame` solo lo que difiere de los valores por defecto de la clase. Consecuencia probable: un perfil guardado **antes** de esta story en automático no lleva el ajuste escrito y, tras el cambio, se leerá como siempre. Es aceptable y es lo que el autor quiere en su máquina; no se escribe una migración. Si se comprueba, se anota en la bitácora; si no se puede comprobar sin PIE, se dice que no se ha comprobado.
- El valor inicial sigue en el código, como desde S-021. Llevarlo a Config es deuda: se deja como `// TODO(R-AYU-1)` con el motivo, no se hace aquí.
- Los comentarios que dicen "Valor inicial Automatic" (`PlayerProfile.h`, `HintsMode.h`) se corrigen.

## Fuera de esta story

- Llevar el valor inicial a Config.
- Los assets de Opciones de la plantilla.
- Migrar perfiles ya guardados.
- Cambiar qué hace cada ajuste.
