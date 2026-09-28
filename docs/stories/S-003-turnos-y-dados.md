# S-003. Turnos y dados

**Sistema**: turnos y dados (GDD 02). **Módulo**: `SoulboundCore`. **Rama base**: `main`. **Tamaño**: media-grande.

## Contexto

El turno del spike vive dentro de `FMatchSimulation::Run` (`Source/SoulboundCore/Private/Simulation/MatchSimulation.cpp`): cada `TurnPeriodSeconds` tira 1d6, pide destino al decisor y pasa al siguiente asiento. No hay fases, orden sorteado, reloj de turno, contadores ni registro. Esta story mueve el turno al motor como máquina de estados sobre `FMatchState`, con el tiempo como dato de entrada, y reescribe la simulación para que lo use. `StartMatch` (S-002) ya prepara el estado inicial; aquí gana el sorteo del orden de turno.

Decidido por el autor el 28/09/2026: (1) todo el alcance de abajo en una story; (2) los parámetros de partida viven en un archivo propio `Content/Data/Match/partida.json`; (3) la simulación se reescribe para usar la máquina de turnos.

## Reglas a implementar

- **R-TUR-1.** La partida se juega por turnos, uno por jugador, en un orden fijo. El orden se sortea una vez al inicio (el anuncio del Warden es de su GDD).
- **R-TUR-2.** Un turno tiene estas fases, en este orden: inicio; decisiones previas a la tirada; tirada y elección de destino; movimiento y resolución de la casilla de destino; fin de turno.
- **R-TUR-5.** Todo jugador tira un dado por defecto. Los dados son de seis caras y el resultado es la suma. (Celeridad, R-TUR-4, es del GDD de runas: aquí la fase "decisiones previas" existe y se pasa sin decisión.)
- **R-TUR-7.** El jugador elige una de las casillas alcanzables (`ReachableTiles`). Tirada y elección forman un solo paso.
- **R-TUR-8.** Reloj de turno: un tiempo definido en datos (valor inicial 30 s) desde que empieza el turno hasta que confirma destino. Si se agota, el turno termina sin mover y el jugador sube un nivel de insolencia (el castigo es del GDD del Warden).
- **R-TUR-9.** Turnos extra: se juegan inmediatamente y seguidos. K extra son K más uno turnos consecutivos, cada uno completo y con su propio reloj.
- **R-TUR-10.** Turnos perdidos: al tocarle a un jugador con turnos perdidos, se salta su turno, se descuenta uno y se emite el anuncio. Se acumulan.
- **R-TUR-14.** Un bot juega con las mismas fases y el mismo reloj.
- **R-TUR-15.** El motor emite un evento con marca de tiempo al inicio y fin de cada turno, en cada tirada, en cada agotamiento de reloj y en cada interrupción, con el jugador afectado.

Caso límite del GDD: turnos extra y perdidos a la vez, primero se descuentan los perdidos contra los extra uno a uno, y el resto se aplica. Tope de turnos extra y de perdidos acumulados: parámetro, valor inicial 2 (GDD 05).

## Criterios de aceptación

- **CA-TUR-1** (R-TUR-1). Dada una partida nueva, cuando se sortea el orden, entonces es una permutación de los cuatro asientos, se mantiene toda la partida, la misma semilla da el mismo orden y en 50 semillas aparecen al menos dos órdenes distintos.
- **CA-TUR-2** (R-TUR-2). Dado un turno, cuando se ejecuta sin interrupciones, entonces pasa por las cinco fases en orden y ninguna se repite.
- **CA-TUR-5** (R-TUR-5), parte sin Celeridad. Dado cualquier jugador, cuando tira, entonces es un entero de 1 a 6 sacado del `FRandomStream` de la partida (misma semilla, misma tirada).
- **CA-TUR-7** (R-TUR-7). Dada una tirada, cuando el jugador confirma un destino, entonces es una de las casillas alcanzables; una intención con una casilla no alcanzable se rechaza sin cambiar el estado ni la fase.
- **CA-TUR-8** (R-TUR-8). Dado un turno cuyo reloj se agota antes de confirmar destino, cuando el motor recibe un tiempo igual o posterior al límite, entonces la posición no cambia, el turno termina y la insolencia del jugador sube en uno. Dado un turno confirmado antes del límite, entonces la insolencia no cambia.
- **CA-TUR-9** (R-TUR-9). Dado un jugador con K turnos extra, cuando termina su turno, entonces juega K turnos más seguidos antes de que juegue otro, cada uno con su reloj.
- **CA-TUR-10** (R-TUR-10). Dado un jugador con P turnos perdidos, cuando le toca, entonces se salta, P baja en uno, el registro contiene el evento y juega el siguiente del orden.
- **Caso límite** (GDD 02). Dado un jugador con 2 extra y 1 perdido, cuando le toca, entonces juega 2 turnos seguidos (uno normal más uno extra) y los contadores quedan a cero. Dado un jugador al tope de extra (2), cuando se le concede otro, entonces se queda en 2.
- **CA-TUR-14** (R-TUR-14). Dado un asiento ocupado por un decisor dummy, cuando le toca, entonces su turno pasa por las mismas funciones que el de cualquier otro (no existe un camino aparte para bots en el motor).
- **CA-TUR-15** (R-TUR-15). Dado cualquier turno, cuando termina, entonces el registro contiene inicio, tirada y fin con el asiento y la marca de tiempo del reloj de partida; y agotamiento si lo hubo. (Interrupción: story del Warden.)
- **Simulación** (R-SIM, GDD 08). Dada la simulación con dummies, cuando corre una partida de 40 minutos, entonces usa la máquina de turnos del motor (la simulación no contiene reglas de turno), sigue siendo determinista por semilla, y los tests actuales `MatchSimulation.DeterminismoPorSemilla` y `CostePorPartida` siguen en verde (una partida en menos de un segundo).
- **Parámetros** (datos). Dado `Content/Data/Match/partida.json`, cuando se carga, entonces el reloj de turno y los topes salen de él y ningún valor vive en el código; un archivo mal formado se rechaza con motivo, igual que el tablero.

## Notas para quien lo implemente

- **Nombres del glosario**: turno `Turn`, tirada `Roll`, intención `Intent`, insolencia `Insolence` (contador 0 a 5 por jugador), asiento `Seat`, decisor `Decider`, partida `Match`. Hacen falta nombres nuevos: la fase del turno (enum), el orden de turno en el estado, los parámetros de partida (struct y su cargador), el evento del registro (struct con tipo, marca de tiempo y asiento) y las funciones de la máquina de turnos. Propón todos en el plan y en el PR; el autor los incorpora al glosario.
- **El tiempo es un dato.** El motor no lee relojes (ADR-001). Diseño esperado: el estado guarda el límite del reloj de turno (en segundos de `ClockSeconds`), y una función de reglas recibe el tiempo actual y decide si el turno se ha agotado. La simulación avanza el reloj virtual y llama; el anfitrión de Unreal hará lo mismo con el reloj real en su story.
- **Intenciones, no acciones.** El destino llega como intención del asiento activo: el motor la valida (asiento correcto, fase correcta, casilla alcanzable) y la descarta sin castigo si no es válida (glosario "Intención"). Ninguna función del motor mueve una ficha sin pasar por esa validación.
- **Un solo punto de mutación por regla.** Cada cambio de `FMatchState` pasa por una función de `Soulbound::Rules` que valida antes de escribir; los tests no tocan campos a mano salvo para montar el estado inicial.
- **Registro de eventos**: en el propio `FMatchState` o junto a él, en orden, con el tiempo del reloj de partida. Es la base de M1 y de la crónica; que sea barato (structs en un array), sin texto formateado.
- **La simulación** conserva su interfaz (`FSimulationParams`, `FSimulationResult`, decisores por asiento) y su bucle por sucesos; sustituye el turno de mentira por llamadas al motor. `TurnPeriodSeconds` deja de existir como concepto: el siguiente suceso de turno es el límite del reloj de turno o la respuesta del decisor, lo que ocurra antes. Si el decisor responde al instante (dummy), el turno dura lo que tarde en confirmar: fija en la simulación un tiempo de respuesta del dummy como parámetro (valor inicial 5 s) para que la partida tenga turnos de duración creíble.
- **Insolencia**: solo el contador y su tope (5). El castigo y el Warden que lo aplica, en su story.
- **Turnos extra y perdidos**: el motor expone funciones para concederlos (con tope), que hoy solo usan los tests; el Warden las llamará después.
- `MakeSpikeMatch` sigue valiendo como tablero mínimo si los tests de turnos lo prefieren al tablero de pruebas; decídelo y dilo en el PR.
- Nada de `Engine`, `UWorld` ni `UObject` en `SoulboundCore`. Sin números mágicos: 6 caras, 30 s, topes y 5 de insolencia con nombre y origen en datos o constante documentada.

## Fuera de esta story

- Salida del Vacío al inicio del turno (R-TUR-3): castigos y Vacío.
- Celeridad y repetir tirada (R-TUR-4, R-TUR-6, parte de CA-TUR-5): runas y poderes.
- Interrupciones, frecuencia del Warden y anuncios (R-TUR-11, 12; CA-TUR-11, 12; el evento de interrupción de R-TUR-15): story del Warden.
- Acciones de espectador (R-TUR-13).
- Reloj real en el anfitrión de Unreal y replicación: story del juego.
- Cualquier asset de Unreal.
