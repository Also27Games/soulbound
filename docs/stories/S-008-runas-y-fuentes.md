# S-008. Runas y Fuentes

**Sistema**: runas y poderes (GDD 04), con las reglas de Fuente del GDD 01 y del GDD 03. **Módulos**: `SoulboundCore` (reglas) y `Soulbound` (vista). **Rama base**: `main`. **Tamaño**: media-grande.

## Contexto

La carrera ya reparte Elegidos y despierta el tablero, pero las Fuentes siguen siendo casillas normales y nadie tiene nada que coleccionar. Esta story pone el segundo bucle del juego: obtener runas en las Fuentes, con reserva, y que cada runa dé un poder según el mapa del Elegido. De los seis poderes solo entra aquí el que ya tiene hueco en el motor, Paso Libre (`FMoverTraits::bFreePassage`); los demás vienen en S-009 y S-010. El minijuego de habilidad no existe aún: el motor lo trata como una pregunta al decisor del asiento (superado o fallado), que el dummy responde con una probabilidad fija.

Decidido por el autor el 02/10/2026: (1) el GDD 04 se reparte en cuatro stories (S-008 runas y Fuentes; S-009 Celeridad, Vuelo Planar, Expulsión y Desvío; S-010 duelo, trilero y apuestas; victoria y desafío final tras la story del Warden); (2) el minijuego es una decisión del decisor (`IDecider`), y el dummy responde con probabilidad 0,5; (3) los Elegidos (nombre, afinidad, mapa de runa a poder) viven en `Content/Data/Elects/elegidos.json`, y el archivo del tablero guarda solo el nombre del Dominio: cada dato en un sitio.

## Reglas a implementar

- **R-RUN-1.** Seis tipos de runa, uno por Dominio (R-ELE-3). Las runas de un asiento están en su Brazal, sin límite, contadas por tipo.
- **R-RUN-2, con R-TAB-7, R-ECO-3 y R-CAR-7b.** Con el tablero Despierto, quien termina su movimiento en una Fuente con reserva obtiene una runa de su tipo: un Elegido en cualquier Fuente, con minijuego en las de Olvidados (si lo falla, no obtiene runa ese turno); un Eco solo en las de Olvidados, sin minijuego. En Primordial nadie obtiene nada (R-TAB-8).
- **R-RUN-3.** Una runa por llegada: quedarse en la Fuente por no tener casillas alcanzables no cuenta.
- **R-RUN-4.** Fuente sin reserva: no da runa y el registro lo deja (el Warden lo señalará en su story).
- **R-RUN-5.** Se pueden tener varias del mismo tipo; los duplicados no dan nada extra.
- **R-RUN-6, con R-TAB-4b.** Toda runa que un asiento pierde sin que otro la gane vuelve a la reserva de su Fuente. Función del motor; en esta story nadie la llama salvo los tests (la usarán Desvío, el desafío final y el Warden).
- **R-RUN-7.** El estado guarda los tipos; qué se enseña a quién es de la interfaz (fuera de esta story).
- **R-ELE-1, R-ELE-3, R-ELE-5.** Seis Elegidos, uno por Dominio, con nombre, afinidad y un mapa fijo de runa a poder que es un cuadrado latino con Celeridad en la diagonal (la runa del propio Dominio). Un poder está activo mientras el Elegido tiene al menos una runa del tipo que se lo da; se pierde con la última y vuelve con otra.
- **R-ECO-1 y R-ECO-7.** A un Eco tras la carrera, sus runas no le dan poderes salvo la Runa de Liberación (la de su Dominio asignado), que le da Celeridad mientras tiene al menos una. En esta story Celeridad solo se calcula como poder activo; su efecto sobre los dados es de S-009.
- **R-POD-2 y R-ELE-2 (Fenrir).** Paso Libre: las casillas selladas son transitables como paso y como destino. Fenrir lo tiene siempre por afinidad; las demás afinidades solo se cargan como dato (sus efectos, en sus stories).
- **Reserva inicial**: `runeReservePerSource` del archivo del tablero (ya existe, valor 4). Cada Fuente arranca con ella al Despertar.
- **Registro**: runa obtenida, minijuego superado o fallado, Fuente agotada y runa devuelta a su Fuente emiten eventos con marca de tiempo y asiento.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-RUN-1** (R-RUN-2, 3). Dado el tablero Despierto y una Fuente con reserva R, cuando un Elegido termina su movimiento en ella, tiene una runa más de ese tipo y la reserva es R menos 1; cuando empieza su siguiente turno sin haberse movido (sin alcanzables), no obtiene otra; cuando sale y vuelve, sí.
- **CA-RUN-2** (R-RUN-4). Dada una Fuente con reserva cero, cuando un asiento termina en ella, no obtiene runa y el registro lleva el evento de Fuente agotada.
- **CA-RUN-3 y CA-TAB-5** (R-RUN-6, R-TAB-4b). Dado un asiento con una runa de un tipo, cuando la devuelve con la función del motor, tiene una menos y la reserva de esa Fuente sube en uno; sin runas de ese tipo, la función rechaza y nada cambia.
- **CA-RUN-5** (R-RUN-5). Dado un asiento con dos runas de un tipo, sus poderes activos son los mismos que con una y sus alcanzables no cambian.
- **CA-TAB-8** (R-TAB-7). Dado un Elegido que termina en la Fuente de un Olvidado, cuando el decisor falla el minijuego, no obtiene runa y la reserva no baja; cuando lo supera, la obtiene. En una Fuente con dueño no se pregunta al decisor.
- **CA-TAB-9 y CA-CAR-7b** (R-TAB-8, R-CAR-7b). Dado el tablero en Primordial, nadie obtiene runa en una Fuente. Dado el tablero Despierto durante la carrera, un Eco en la Fuente de un Olvidado con reserva obtiene la runa sin minijuego; en la de un Dominio con dueño, nada.
- **CA-ECO-1 y CA-ECO-2** (R-ECO-1, R-ECO-3). Dado un Eco tras la carrera con runas de otros tipos, no tiene ningún poder activo; en la Fuente de un Olvidado obtiene sin minijuego mientras un Elegido en la misma lo juega.
- **CA-ECO-6 (parte)** (R-ECO-7). Dado un Eco con una runa de su Dominio asignado, Celeridad está activo; con cero, no; con dos, perder una no lo quita.
- **CA-ELE-1 y CA-ELE-10** (R-ELE-3). Dado `elegidos.json`, cuando se carga y valida, cada fila y cada columna del mapa contienen los seis poderes una vez y la diagonal es Celeridad; un archivo que lo incumpla se rechaza con motivo.
- **CA-ELE-9** (R-ELE-5). Dado un Elegido con una sola runa de un tipo, cuando la pierde, el poder deja de estar activo; cuando obtiene otra, vuelve.
- **CA-ELE-2 y CA-TAB-18** (R-ELE-2, R-POD-2). Dado Fenrir sin runas, sus alcanzables cuentan las selladas como transitables; dado otro Elegido con la runa que le da Paso Libre, igual; sin ella, no. La máquina de turnos usa el mismo cálculo (`MakeMoverTraits`).
- **Registro.** Runa obtenida, minijuego superado o fallado, Fuente agotada y runa devuelta aparecen con asiento, tipo y marca de tiempo.
- **Simulación.** Los dummies, acabada la carrera, van a la Fuente con reserva más cercana donde puedan obtener (Elegido: cualquiera; Eco: Olvidados). La crónica cuenta runas y minijuegos; el informe de 50 semillas añade runas por asiento al final y % de Fuentes agotadas. Determinista por semilla y bajo el segundo.
- Los 71 tests actuales siguen en verde.

Manuales, en PIE, con captura:

- **Fuentes vivas.** Cada Fuente enseña su reserva en la etiqueta; al obtener una runa baja; agotada se ve distinta.
- **Brazal visible.** La leyenda o el panel enseña, por asiento, sus runas por tipo y sus poderes activos; al obtener la runa de su Dominio, el Elegido ve Celeridad activo.
- **Paso Libre visible.** Un asiento con Paso Libre (o Fenrir) ve blancas casillas selladas en su turno; otro, no.

## Notas para quien lo implemente

- Nombres del glosario: Runa `Rune`, Brazal `Bracer`, Poder `Power` (`PowerDuel`, `PowerHaste`, `PowerFreePassage`, `PowerExpel`, `PowerPlanarFlight`, `PowerDeflection`), Afinidad `Affinity`, Runa de Liberación `LiberationRune`, Fuente `PowerSource`, minijuego `SkillMinigame`. Nombres nuevos (tipo de runa, reserva, estructura de Elegido y su cargador, resultado de obtener, llamada al decisor para el minijuego, eventos) se proponen en el plan y en el PR.
- El tipo de runa es el identificador de su Dominio (0 a 5); el Brazal, seis contadores por asiento. `FSeatState::RuneCount` (S-002) se sustituye o se deriva; no dos verdades.
- La reserva vive en el estado de la casilla Fuente o del Dominio, a elegir en el plan; arranca al Despertar con el valor del tablero.
- Dónde va: en `AdvanceTurn`, `MoveAndResolve`, junto al Nicho, **solo si el asiento se ha movido este turno** (R-RUN-3); el Nicho mantiene lo que hace. Un movimiento termina en una sola casilla, así que Nicho y Fuente nunca coinciden.
- `IDecider` gana una función para el minijuego (nombre a proponer), con el tipo de prueba y la dificultad como datos; `FDummyDecider` y `FRaceDummyDecider` responden con probabilidad fija usando su generador. La simulación y el anfitrión de partida no cambian de contrato: el motor llama al decisor igual que para el destino. Si la llamada tuviera que esperar al humano (más adelante), se dirá en el PR cómo.
- `MakeMoverTraits` pasa a calcular `bFreePassage` desde los poderes activos y la afinidad; `bConfinedToForgotten` no cambia.
- `elegidos.json`: cargador y validación como los de tablero y partida (`LoadElectsFromFile`, resultado con error y motivo). Esquema: por Dominio, nombre del Elegido, afinidad (identificador) y mapa `{runa: poder}` con las seis runas. Los seis Elegidos y la tabla del GDD, tal cual, como valores iniciales:

  | Dominio | Elegido | Afinidad |
  |---|---|---|
  | 0 | Shuten-dōji | ventaja en duelos |
  | 1 | Fenrir | Paso Libre siempre |
  | 2 | Apep | repetir tirada una vez |
  | 3 | Mictlantecuhtli | escape fácil del Vacío |
  | 4 | Tifón | rechazar el Oráculo |
  | 5 | Koschei | una runa a salvo en el trilero |

  Mapa de runa a poder (filas: Elegido por Dominio; columnas: runa por Dominio 0 a 5): Shuten-dōji: Celeridad, Paso Libre, Expulsión, Desvío, Vuelo Planar, Duelo. Fenrir: Duelo, Celeridad, Paso Libre, Expulsión, Desvío, Vuelo Planar. Apep: Vuelo Planar, Duelo, Celeridad, Paso Libre, Expulsión, Desvío. Mictlantecuhtli: Desvío, Vuelo Planar, Duelo, Celeridad, Paso Libre, Expulsión. Tifón: Expulsión, Desvío, Vuelo Planar, Duelo, Celeridad, Paso Libre. Koschei: Paso Libre, Expulsión, Desvío, Vuelo Planar, Duelo, Celeridad.
- **Datos del tablero**: `domains[].name` de `tablero_35.json` y `tablero_15.json` pasa a ser solo el nombre del Dominio, sin paréntesis ("El Banquete", "El Fiordo Encadenado", "El Río de la Noche", "El Descenso", "La Ruina Tormentosa", "El Bosque Negro"); el nombre del Elegido sale de `elegidos.json`. La vista compone "El Banquete · Shuten-dōji". Commit de datos aparte.
- Vista: solo lectura, como siempre. Reserva en la etiqueta de la Fuente; runas y poderes por asiento en la leyenda. Los JSON nuevos entran en el pak igual que los demás (`Data` ya se empaqueta).
- Nada de `Engine` en `SoulboundCore`. Sin assets.

## Fuera de esta story

- Celeridad sobre los dados, Vuelo Planar, Expulsión, Desvío: S-009.
- Duelo portátil, Nodo de Duelo, trilero, apuestas, robo del Eco, afinidades de Koschei y Shuten-dōji: S-010.
- Desafío final y victoria: tras la story del Warden.
- Brazal oculto o revelado, Grimorio: interfaz.
- Burlas del Warden por Fuente agotada: Warden.
- Minijuego real: su story.
