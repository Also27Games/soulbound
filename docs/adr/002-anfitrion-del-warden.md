# ADR-002. El anfitrión del Warden pertenece a la partida y solo actúa con autoridad

**Estado**: aceptado el 24/09/2026 (spike, paso 4 y hallazgo del 24/09). **Relacionado**: ADR-001.

## Contexto

El GDD 06 separa la IA directora (`WardenBrain`, decide) del anfitrión (`WardenHost`, es dueño del reloj de evaluación, aplica y replica). El GDD 08 fija que solo el servidor tiene autoridad. En el spike, un `UWorldSubsystem` sin más condición se creó también en los mapas de menú y siguió evaluando fuera de la partida.

## Decisión

- El cerebro es una clase sin mundo en `SoulboundCore` (`FWardenBrain::Evaluate(estado, generador) -> decisión`; `Apply(estado, decisión)` estático). La simulación y el juego usan exactamente la misma clase.
- El anfitrión en juego es un `UWorldSubsystem` del módulo `Soulbound` que: se crea solo en mundos de juego o PIE; no hace nada si el proceso es cliente; y **solo arma su reloj cuando el mundo es una partida** (en el spike, cuando el GameMode es el de partida; en el motor real, lo arranca y lo para el GameMode de la Cripta con el inicio y el fin de la partida). Nunca evalúa en el Umbral, la barca o la postpartida.
- El reloj de evaluación es del anfitrión, nunca del cerebro; el cerebro recibe el reloj de partida como dato del estado.

## Alternativas descartadas

- Cerebro como función pura sin estado propio dentro del subsystem: mezcla decisión y reloj y no se puede simular.
- Un actor `AWarden` con Tick: depende del mundo, no se prueba sin él, y en el cliente habría que impedir que actuara.

## Consecuencias

- Cualquier prueba del Warden que necesite el editor es sospechosa: la lógica se prueba en simulación.
- La presentación del Warden (voz, máscara, golpes en pantalla) escucha las decisiones aplicadas y replicadas; no consulta al cerebro.
- En PIE con varios jugadores en un proceso, los mensajes en pantalla son globales al proceso: para observar el anfitrión se lee el log con el prefijo del mapa (`UEDPIE_n`).

## Nota del 20/11/2026, al cierre del TFM

El Umbral, la barca y la postpartida no existen en esta entrega (Fase 2). En el código el anfitrión son dos subsistemas de mundo, `UWardenHostSubsystem` y `UMatchHostSubsystem`. Como la partida en línea no se entrega, la condición de no actuar en un cliente solo se ha ejercitado en la prueba técnica.
