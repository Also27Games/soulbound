# S-002. Tablero desde datos e inicio de partida

**Sistema**: tablero y movimiento (GDD 01). **Módulo**: `SoulboundCore`. **Rama base**: `main`. **Tamaño**: media.

## Contexto

Hoy el tablero se construye en código: `FMatchState::MakeSpikeMatch` (`Source/SoulboundCore/Public/Match/MatchState.h`) crea seis casillas en ciclo con la 0 como Altar. `FBoardState` (`Public/Board/BoardState.h`) tiene casillas con tipo (`ETileType`: normal, Altar), conexiones y Sello. El GDD 01 dice que la geometría sale de un archivo de datos y describe una estructura que el código no conoce: seis Dominios en anillo, Nicho y Fuente por Dominio, Nodos de Poder con efecto sorteado, Dominios Olvidados sorteados al inicio.

Esta story hace tres cosas: **cargar** un tablero desde un archivo JSON, **validarlo** contra la estructura del GDD, y **preparar el inicio de partida** (sorteo de efectos de Nodos y de Dominios Olvidados, determinista por semilla). No toca el movimiento.

## Reglas a implementar

- **R-TAB-1.** El tablero es un grafo de casillas y conexiones. Cada casilla tiene un tipo y una lista de conexiones. Su geometría concreta se carga de un archivo de datos, no está en el código.
- **R-TAB-2.** Existe una única casilla de tipo Altar. Todos los jugadores empiezan en ella.
- **R-TAB-3.** Hay exactamente seis Dominios. Cada Dominio es un conjunto de casillas con al menos una conexión al Altar y al menos una conexión a cada uno de sus dos Dominios vecinos (los seis forman un anillo alrededor del Altar). Los Dominios tienen formas distintas y un número de casillas parecido, definido en datos.
- **R-TAB-4.** Cada Dominio contiene exactamente un Nicho y un número de Fuentes de Poder definido en datos, valor inicial uno, en posiciones fijas.
- **R-TAB-4c.** El Nicho de cada Dominio es una casilla con una sola conexión, al final de un ramal. Se entra y se sale por la misma casilla.
- **R-TAB-5.** El tablero contiene un número de casillas de tipo Nodo de Poder definido en datos. Al iniciar la partida se asigna a cada Nodo, al azar, uno de tres efectos, Duelo, Oráculo o Planar, respetando las cantidades de cada efecto definidas en datos. Las posiciones de los Nodos son fijas; solo varía el efecto.
- **R-TAB-6.** Al iniciar la partida con N jugadores, el sistema elige al azar 6 menos N Dominios y los marca como Olvidados.

## Criterios de aceptación

- **CA-TAB-1** (R-TAB-1). Dado un archivo de datos de tablero válido, cuando se carga, entonces el grafo tiene las casillas y conexiones descritas y ningún valor de geometría procede del código.
- **CA-TAB-2** (R-TAB-2). Dado un tablero cargado, cuando empieza una partida, entonces hay exactamente una casilla Altar y todas las fichas están en ella.
- **CA-TAB-3** (R-TAB-3). Dado un tablero cargado, cuando se valida, entonces hay seis Dominios, cada uno con al menos una conexión al Altar y a cada uno de sus dos vecinos.
- **CA-TAB-4** (R-TAB-4, 4c). Dado un tablero cargado, cuando se valida, entonces cada Dominio tiene exactamente un Nicho con una sola conexión y el número de Fuentes definido en datos.
- **CA-TAB-6** (R-TAB-5). Dado un tablero con K Nodos y cantidades por efecto en datos, cuando empieza la partida, entonces cada Nodo tiene un efecto, las cantidades coinciden con los datos, y dos partidas con semillas distintas producen asignaciones distintas.
- **CA-TAB-7** (R-TAB-6). Dada una partida con N jugadores, cuando empieza, entonces exactamente 6 menos N Dominios son Olvidados y sus Nichos están Cerrados.
- Además, un test por cada regla de validación que rechaza un tablero mal formado (sin Altar o con dos, cinco Dominios, Nicho con dos conexiones, anillo roto, Fuentes que no coinciden con los datos), con un archivo de prueba inválido por caso o construyendo el tablero inválido en el test a partir del válido.
- Sin valores fijos en el código: casillas, conexiones, Dominios, número de Fuentes, Nodos y cantidades por efecto salen del archivo.
- Los tests actuales (`MovementRulesTest`, `WardenBrainTest`, `MatchSimulationTest`) siguen en verde. Pueden seguir usando `MakeSpikeMatch` o pasar al tablero de pruebas; si cambian, que sea porque el tablero de pruebas los hace más claros, no por obligación.

## Decisiones ya tomadas (25/09/2026, autor)

1. **Formato y lector**: archivo **JSON**, leído desde `SoulboundCore` con el módulo `Json` de Unreal, que solo depende de `Core`. Es la única dependencia nueva permitida; `JsonUtilities` no (depende de más cosas y no hace falta). ADR-001 se ha actualizado con esta excepción. Los tests cargan el archivo real por consola.
2. **Primer tablero**: el agente entrega un **tablero de pruebas** que pase la validación: seis Dominios de unas cinco casillas cada uno con formas distintas (no seis copias), Nicho a distancia 2 a 4 del Altar al final de un ramal, una Fuente por Dominio, y unos pocos Nodos de Poder (por ejemplo tres, uno por efecto). El tablero definitivo lo diseñará el autor con el arte y sustituirá el archivo sin tocar código.
3. **Ubicación y parámetros**: `Content/Data/Boards/tablero_pruebas.json`. Los parámetros del GDD que dependen del tablero van dentro del mismo archivo: Fuentes por Dominio, cantidad de Nodos por efecto, máximo de casillas selladas a la vez, runas en reserva por Fuente. Los dos últimos solo se cargan y quedan disponibles; los usan stories posteriores.

## Notas para quien lo implemente

- **Nombres del glosario**: tablero `Board`, casilla `Tile`, Dominio `Domain`, Dominio Olvidado `ForgottenDomain`, Nicho `Niche`, Fuente de Poder `PowerSource`, Nodo de Poder `PowerNode`, Nodo de Duelo `DuelNode`, Nodo del Oráculo `OracleNode`, Nodo Planar `PlanarNode`, partida `Match`. `ETileType` crece con Nicho, Fuente y Nodo. Para el efecto de un Nodo, el estado de un Dominio (con dueño u Olvidado) y el estado de un Nicho (Libre, Reclamado, Cerrado) hacen falta enums o campos que el glosario no nombra en código: propón los nombres en el PR.
- **Estructura del JSON**: propón el esquema en el plan antes de escribir código. Orientación: una lista de casillas (id, tipo, Dominio al que pertenece o ninguna para el Altar), una lista de conexiones (pares de ids, sin dirección), una lista de seis Dominios (id, nombre provisional, orden en el anillo) y un bloque de parámetros. Un tablero se lee entero o se rechaza entero.
- **Validación con resultado, sin excepciones**: la carga devuelve un resultado (éxito con el tablero, o fallo con un motivo legible que diga qué regla se incumple). Unreal compila sin excepciones de C++.
- **Inicio de partida**: una función de reglas que recibe el tablero cargado, el número de jugadores y un `FRandomStream`, y devuelve el estado inicial: efectos de Nodos asignados, Dominios Olvidados marcados con su Nicho Cerrado, cuatro asientos en el Altar. Determinista: misma semilla, mismo resultado. Con menos de uno o más de cuatro jugadores, resultado de error.
- **Dónde está el archivo en los tests**: ruta relativa al directorio de contenido del proyecto (`FPaths::ProjectContentDir()` está en `Core`). El script `Scripts/test.ps1` ya ejecuta con el `.uproject` cargado.
- **Los Dominios necesitan identidad estable** (id 0 a 5, con el orden del anillo) porque los GDD posteriores asignan Elegidos y Ecos por Dominio. Los nombres son provisionales y solo viven en el archivo.
- `MakeSpikeMatch` puede quedarse como tablero mínimo para tests de movimiento, o desaparecer si todos los tests pasan al tablero de pruebas. Decídelo y dilo en el PR.
- Nada de `Engine`, `UWorld` ni `UObject` en `SoulboundCore`. Sin números mágicos: cualquier "seis" viene de una constante con nombre y comentario que cite R-TAB-3.

## Fuera de esta story

- Reserva de runas y su recarga (R-TAB-4b): GDD de runas. El valor se carga; no se usa.
- Estados Primordial y Despierto, Despertar, corrupción al fin de carrera, hogares, Ecos (R-TAB-7 a 13): S-004, carrera por el vínculo.
- Ocupación, confinamiento, criterio único de transitabilidad, topes de Sellos, Vacío, interrupciones (R-TAB-19 a 28): stories posteriores.
- Registro de eventos para M1: story del Warden.
- Cualquier asset de Unreal. El JSON no es un asset: es un archivo de texto versionado.
