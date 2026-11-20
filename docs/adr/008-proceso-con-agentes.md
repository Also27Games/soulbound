# ADR-008. Proceso con agentes: story, PR con guía de lectura, bitácora y MCP de consulta

**Estado**: aceptado el 24/09/2026 (spike, pasos 8 y 8b). **Relacionado**: ADR-001.

## Contexto

El máster evalúa el proceso de programar con agentes tanto como el resultado. El autor quiere entender cada cambio ("si no, acabaré a ciegas"). La primera story implementada por un agente (S-001) salió bien en siete minutos, con dos preguntas legítimas causadas por documentación desfasada, y con una carencia clara: nada explicaba qué se había hecho y por qué en un formato que el autor pudiera leer.

## Decisión

- **Una story, una conversación nueva, un agente**: parte solo de `CLAUDE.md`, la skill (glosario, pilares, plantillas, checklist) y el archivo de la story en `docs/stories/`. Modelo Opus 5 mientras la skill madura; Sonnet 5 en stories pequeñas cuando se haya calibrado.
- **Rama por story y PR** contra la rama base que diga la story, fusionado con merge commit (la historia de la story queda visible). Commits en español, sin líneas de atribución de herramientas.
- **La descripción del PR lleva una sección "Cómo leer este PR"**: archivo por archivo, en orden de lectura, qué cambia y por qué, citando la regla del GDD. Más criterios cubiertos con su test, decisiones con alternativa descartada, dudas y pendiente.
- **Bitácora** (`docs/bitacora.md`): una entrada de diez líneas por story, escrita en el mismo PR, revisada por el autor. Historia legible del proyecto y materia prima de la memoria.
- **Una story cada vez**: no se abre la siguiente hasta que la anterior está fusionada y leída. Si el ritmo del agente supera al autor, se baja el ritmo.
- **El agente para y pregunta** cuando la story contradice un GDD o al glosario; la respuesta corrige el documento, no solo al agente.
- **Unreal MCP** con el editor abierto: consulta (nivel, actores, logs, configuración) y tests (décimas de segundo). Assets solo si la story lo pide, con confirmación antes de cada escritura, en commit aparte y verificados en el editor por una persona. Condición legal de la EULA de Unreal: la cuenta del modelo con el uso de datos para entrenamiento desactivado.
- **Verificación sin editor** como base: `Scripts/build.ps1` y `Scripts/test.ps1`; el MCP es complemento.

## Alternativas descartadas

- Conversación larga y continua con el agente: arrastra conocimiento que no está en el repo y esconde lo que falta en la documentación.
- Squash al fusionar: historia más corta pero se pierde cómo se llegó.
- Prohibir assets a los agentes: demasiado, habrá stories de nivel o material; se regula en vez de prohibir.

## Consecuencias

- La documentación tiene que estar al día o el agente tropieza con ella (dos de dos preguntas de S-001 lo fueron): mantener glosario y GDD es parte de cada story.
- El coste por story se mide (tiempo, intervenciones, carencias) y va a la bitácora y a la memoria.

## Nota del 20/11/2026, al cierre del TFM

El proceso se mantuvo hasta el final: cincuenta stories, cada una con su rama, su PR con guía de lectura y su entrada en la bitácora. El formato de las stories y de la memoria quedó como aquí se describe.
