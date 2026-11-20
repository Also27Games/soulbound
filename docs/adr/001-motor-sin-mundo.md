# ADR-001. Motor de reglas sin mundo en un módulo aparte

**Estado**: aceptado el 23/09/2026 (spike, pasos 2 y 3). **Sustituye a**: nada.

## Contexto

Los GDD 06 y 08 exigen simular partidas enteras sin mundo ni red (R-SIM-1 a 5: una partida en menos de un segundo, determinista por semilla), probar cada criterio de aceptación con un test que se lance sin abrir el editor, y que los agentes puedan implementar reglas y verificarlas por consola. Unreal empuja a meter la lógica en actores y componentes, que necesitan un `UWorld` y no se pueden ejecutar en un test unitario ni cien veces por segundo.

## Decisión

- Un módulo propio, `SoulboundCore`, que solo depende de `Core` y `CoreUObject`. Nunca de `Engine`. **Excepción añadida el 25/09/2026 (S-002)**: también `Json`, que solo depende de `Core`, para cargar tableros y parámetros desde archivos JSON. `JsonUtilities` no, porque no hace falta.
- El estado de partida es un árbol de `USTRUCT` (`FMatchState`, `FBoardState`, `FTileState`, `FSeatState`): datos puros, serializables y replicables tal cual.
- Las reglas son funciones libres en el espacio de nombres `Soulbound::Rules` (por ejemplo `ReachableTiles`) o clases C++ sin `UObject` (`FWardenBrain`, `IDecider`). Reciben el estado y, si necesitan azar, un `FRandomStream` con semilla; nunca leen relojes, mundos ni entrada.
- El módulo del juego (`Soulbound`) llama al motor y nunca duplica una regla. La presentación solo lee el estado y manda intenciones.
- Los tests viven en el módulo del motor (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) y se lanzan con `Scripts/test.ps1` (comandlet con `-nullrhi`, 12 s de arranque fijo) o por el MCP con el editor abierto (décimas de segundo).

## Alternativas descartadas

- Lógica en actores y componentes con tests funcionales en mapas de prueba: cada test arranca un mundo, no hay simulación a cien partidas por segundo y los agentes no pueden verificar sin editor.
- Un ejecutable de consola aparte para el motor: duplica el sistema de build y los tests de Unreal ya cubren la necesidad.

## Consecuencias

- Comprobado en el spike: regla de casillas alcanzables con tests en 12 s por consola; simulación de 40 minutos virtuales en 0,1 ms con reglas de mentira; una story implementada por un agente y verificada sin editor.
- Cualquier regla que "necesite" el mundo es una señal de diseño equivocado: se replantea como dato en el estado o como intención.
- Coste: dos módulos y disciplina de dependencias; el módulo del juego tiene que transformar estado en presentación, no razonar.

## Nota del 20/11/2026, al cierre del TFM

El motor sigue sin depender de `Engine`. Los tests son 622 y tardan 89 s por consola, arranque incluido.
