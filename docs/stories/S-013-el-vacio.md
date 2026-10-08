# S-013. El Vacío

**Sistema**: castigos, Nicho y Vacío (GDD 05), con Desvío (GDD 04) y el inicio de turno (GDD 02). **Módulos**: `SoulboundCore` y `Soulbound` (vista y comandos). **Rama base**: `main`. **Tamaño**: media.

## Contexto

El Warden todavía no castiga a nadie, pero el castigo mayor del juego, el Vacío, es un sistema por sí mismo: la ficha sale del tablero, el jugador queda inmune y aislado, y en cada turno propio intenta escapar con el minijuego, cada vez más fácil, hasta salir al cuarto. Esta story lo construye entero con las piezas que ya existen (minijuego como espera, elecciones pendientes, hogar), cierra el poder Desvío que quedó fuera de S-009, y deja hecho el efecto Vuelta al hogar, que comparte mecánica con salir del Vacío y que el catálogo de S-014 reutilizará. Hasta el Warden, al Vacío se entra con un comando.

Decidido por el autor el 08/10/2026: (1) los GDD 05 y 06 se reparten en cinco stories (S-013 el Vacío; S-014 catálogo de efectos y eventos desde datos; S-015 Warden I: interrupciones, insolencia, Furia, actos, sacos; S-016 Warden II: Oráculo, disparadores, votaciones, comentarios, eventos programados; S-017 victoria); (2) el cerebro del spike no destierra: el Vacío se prueba con `SbBanish` y la simulación no cambia; (3) Turno perdido y Turno extra como efectos van a S-014, no aquí.

## Reglas a implementar

- **R-VAC-1, R-TAB-24, R-INT-3, R-DIR-2.** Entrar: la ficha sale del tablero (posición "ninguna"). Dentro, el jugador no puede ser retado, expulsado ni robado, no ocupa casilla y no es objetivo válido de ningún efecto salvo Liberación.
- **R-VAC-2, R-TUR-3, R-ELE-2 (Mictlantecuhtli), R-ECO-5, R-POD-6.** Salir, al inicio de cada turno propio, antes de nada y en este orden: un Eco sale sin jugar; quien tiene Desvío activo elige si lo usa (sacrifica una runa del tipo que se lo da, que vuelve a su Fuente; sale y juega el turno); si no, juega el minijuego de escape con la escalera de dificultad de datos: primer turno difícil, segundo normal, tercero fácil, cuarto sale sin jugar. Mictlantecuhtli: fácil en el primero, sale sin jugar en el segundo. Sin dados.
- **R-VAC-3.** Al salir, por cualquier motivo, el jugador reaparece en su hogar. Si sale por Desvío, además juega ese turno con normalidad; por cualquier otro motivo (escape superado, cuarto turno, Eco, Liberación), su turno termina sin tirar. Si falla el escape, sigue dentro y su turno termina.
- **R-VAC-4.** Un jugador desterrado de nuevo empieza la cuenta desde cero.
- **R-VAC-5, R-EFE-17.** Liberación: todos los del Vacío salen a la vez a su hogar, sin esperar turnos. Sin nadie dentro, no es válida.
- **R-EFE-14, R-CAR-7.** Desterrar: función del motor. Durante la carrera nadie va al Vacío salvo por la Furia (parámetro de la función); hoy nadie la llama salvo el comando.
- **R-EFE-4, R-NIC-1, R-NIC-2.** Vuelta al hogar: la ficha va a su hogar; quien ya está en él, o está en el Vacío, no es objetivo válido. El siguiente turno se juega con normalidad.
- **Registro**: destierro, intento de escape (dificultad y resultado), salida (motivo: escape, Desvío, cuarto turno, Eco, Liberación) y Vuelta al hogar, con marca de tiempo y asientos.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-TAB-25 y CA-VAC-3** (R-TAB-24, R-VAC-1, R-INT-3). Dado un asiento desterrado, su posición es "ninguna", no cuenta como ocupante de ninguna casilla, y no aparece como objetivo de robo, duelo, Expulsión ni Vuelta al hogar; al volver, está en su hogar.
- **CA-VAC-1** (R-VAC-2, 3). Dado un Elegido sin Desvío en el Vacío, en su primer turno se le pide el minijuego en difícil; si falla, en el segundo en normal; si falla, en el tercero en fácil; si falla, en el cuarto está en su hogar sin jugar y el turno termina. Cuando lo supera en cualquier turno, está en su hogar y el turno termina sin tirada. La escalera sale de datos.
- **CA-VAC-2 y CA-ELE-5** (R-VAC-2, R-ELE-2, R-ECO-5). Dado Mictlantecuhtli, su primer escape es en fácil y en el segundo turno sale sin jugar. Dado un Eco, sale en su primer turno sin jugar, en su hogar.
- **CA-POD-4** (R-POD-6). Dado un Elegido en el Vacío con una runa que le da Desvío, al empezar su turno se le ofrece usarlo; si acepta, la runa ha vuelto a su Fuente, está en su hogar y juega el turno completo (tira y mueve); si era su última de ese tipo, ya no tiene Desvío; si rechaza, juega el escape. Sin Desvío activo no se le ofrece nada.
- **CA-VAC-4** (R-VAC-4, 5, R-EFE-17). Dado un asiento desterrado por segunda vez, su cuenta es uno. Dada una Liberación con dos dentro, ambos están en su hogar en ese instante; sin nadie dentro, se rechaza.
- **Carrera** (R-CAR-7). Dado un destierro durante la carrera, se rechaza salvo con el parámetro de Furia.
- **CA-EFE-3** (R-EFE-4). Dado un asiento fuera de su hogar, con Vuelta al hogar está en él; en su hogar o en el Vacío, no es objetivo válido.
- **Nada anterior cambia**: las elecciones, el trilero y el duelo de S-009 a S-012 siguen igual (sus tests en verde).
- **Registro.** Los eventos nuevos.
- **Sorteo**: desterrar, escapar y usar Desvío no consumen el sorteo de la partida.
- Los 139 tests actuales siguen en verde.

Manuales, en PIE con paso a paso y comandos, con captura:

- **Escalera**: `SbBanish 0` acabada la carrera; con `SbTrial fail` tres veces, el cuarto turno sales solo; con `SbTrial pass`, sales al primero. La ficha desaparece del tablero y el texto dice "EN EL VACÍO, turno n".
- **Desvío**: con la runa que te da Desvío (tabla impresa), desterrado, la vista ofrece "[1] usar Desvío [0] no"; al usarlo, la runa vuelve a la reserva y juegas el turno.
- **Liberación y Vuelta al hogar**: `SbRelease` saca a todos; `SbSendHome 1` manda a un dummy a casa.

## Notas para quien lo implemente

- Nombres del glosario: el Vacío `Void`, Hogar `HomeTile`, Desvío `PowerDeflection` (`EPower::Deflection`), afinidad `EAffinity::VoidEscape`, Liberación `ReleaseVoid`, Vuelta al hogar `SendHome`, minijuego `SkillTrial` con `ESkillDifficulty`. Nombres nuevos (estado del Vacío por asiento, funciones de efecto y su resultado, tipo de elección de Desvío, tipo de prueba de escape, eventos, parámetros) se proponen en el plan y en el PR.
- **Posición "ninguna"**: `FSeatState::TileId` a `INDEX_NONE` mientras está dentro, y un contador de turnos propios en el Vacío. Todo lo que recorre asientos por casilla (ocupantes, objetivos, vista) tiene que tolerarlo.
- **Inicio de turno**: en `Start`, antes de `PreRoll`. Eco: sale y `End`. Desvío: elección pendiente (bajo el reloj de turno; agotarla es "no"). Escape: `SkillTrial` con un tipo de prueba nuevo (`VoidEscape`) y la dificultad de la escalera; el resultado lleva a casa y `End`, o se queda y `End`. Cuarto turno (o segundo de Mictlantecuhtli): sale sin pedir nada.
- **Datos** en `partida.json`: escalera de dificultades (lista) y escalera de la afinidad de escape (lista). Validadas: no vacías, dificultades conocidas.
- **Efectos** en una cabecera nueva (`Match/EffectRules.h` o el nombre que se proponga): `Banish`, `ReleaseVoid`, `SendHome`, cada una con su comprobación de objetivo válido (R-DIR-2) y su resultado con motivo. S-014 añadirá el resto en la misma cabecera.
- **Comandos** (patrón de S-011, con evento de depuración): `SbBanish <asiento>`, `SbRelease`, `SbSendHome <asiento>`.
- **Vista**: la ficha desterrada sale del tablero (se oculta o va a un rincón "Vacío"); texto por asiento "EN EL VACÍO, turno n"; crónica. Solo lectura.
- Nada de `Engine` en `SoulboundCore`.

## Fuera de esta story

- Los otros efectos (Turno perdido, Turno extra y los demás), direcciones, Líder, decretos y dilemas: S-014.
- Quién manda al Vacío y cuándo, insolencia, Furia (salvo el parámetro de la función): S-015.
- Presentación del Vacío para el desterrado (Cripta que se desvanece): interfaz.
- Apostar y votar desde el Vacío: con las apuestas y las votaciones.
