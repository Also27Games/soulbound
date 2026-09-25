# Aplicación del máster en Soulbound Core

Este documento relaciona cada decisión del proyecto con la práctica del Máster en Desarrollo con IA (BIG School) que la respalda, y dice qué prácticas del temario no se adoptan y por qué. Sirve para que quien evalúe el TFM vea de un vistazo dónde está aplicado cada bloque. Se cita el bloque y la lección por su título; el material del campus es privado y no forma parte de este repositorio.

Última revisión: 25/09/2026, tras leer el temario completo.

## 1. Decisiones respaldadas por el temario

| Decisión del proyecto | Bloque y lección del máster | Dónde está |
|---|---|---|
| Motor de reglas sin mundo (`SoulboundCore`), dependencias hacia dentro; actores y subsistemas como adaptadores de entrada, replicación y presentación | Arquitectura: Hexagonal (Ports & Adapters), Clean Architecture; Seguridad: arquitectura de un proyecto seguro (separación de responsabilidades, defensa en profundidad) | ADR-001, `Source/SoulboundCore`, `CLAUDE.md` |
| Glosario oficial obligatorio; los nombres nuevos se proponen en el PR, no se inventan en el código | Arquitectura: masterclass de Domain-Driven Design (lenguaje ubicuo) | `.claude/skills/soulbound/references/glosario.md` |
| Cada regla del GDD → criterio Dado/Cuando/Entonces → test que cita criterio y regla | Ingeniería de software: análisis de requisitos, validación y verificación; DDD: criterios Given/When/Then; Calidad: TDD con IA | Plantilla de GDD, `Source/*/Private/Tests` |
| Test escrito antes de implementar y visto fallar (rojo, verde, refactor) | Calidad: TDD con IA; Desarrollo moderno: SDD implementación; Herramientas: tests potenciados por IA | `CLAUDE.md`, tabla siempre/preguntar/nunca |
| Decisiones de arquitectura como ADR con contexto, decisión, alternativas y consecuencias; un ADR no se edita, se sustituye | Calidad: ADR, documentar el porqué; Ingeniería: directo (el ADR como registro, no documentación viva) | `docs/adr` (en publicación), ADR-001 a ADR-008 |
| Especificación que se reajusta con cada story (Spec-Anchored), no una spec nueva por cambio | Ingeniería: Spec-Driven Development, niveles | ADR-008, GDD por sistema |
| Monolito modular: un proceso, un motor, sin backend propio ni servicios distribuidos | Arquitectura: monolitos modulares, mitos de los microservicios, ley de Conway; Infraestructura: directo (vendor locking) | ADR-003, ADR-005, ADR-007 |
| Online por la interfaz genérica de sesiones de Unreal, con el proveedor (Redpoint EOS) detrás de un plugin propio; login delegado en Steam y EOS | Ingeniería: SOLID, Dependency Inversion; Seguridad: identificación y autenticación (no reinventar la autenticación) | ADR-003, `Plugins/A27Online` |
| Secretos fuera del repositorio: filtro de Git que vacía las claves y hook que rechaza commits con ellas | Seguridad: gestión de credenciales, Shift Left; Calidad: variables de entorno y secretos; Infraestructura: secretos en CI | ADR-004, `Scripts/eos-secrets`, `Scripts/git-hooks/pre-commit` |
| Valores de juego como parámetros con valor inicial documentado, nunca números mágicos | Ingeniería: por qué son necesarias las buenas prácticas; Calidad: code smells | `CLAUDE.md`, sección "Parámetros de ajuste" de cada GDD |
| Guía de agentes corta que remite a una skill y a referencias, en vez de un archivo gigante | Flujo de desarrollo con IA: AGENTS.md y skills; Desarrollo moderno: evolución del contexto | `CLAUDE.md`, `.claude/skills/soulbound` |
| Tabla siempre / preguntar / nunca con el motivo de cada regla | Desarrollo moderno: masterclass GitHub y agentes (elementos de un archivo de agente eficaz) | `CLAUDE.md` |
| Stack con versiones exactas en la guía del agente | Desarrollo moderno: masterclass GitHub y agentes | `CLAUDE.md` |
| Una story, una conversación nueva, un agente, con el contexto mínimo (guía, skill y story) | Desarrollo moderno: context window, evolución del contexto; Flujo: directo (reiniciar contexto por feature) | ADR-008 |
| Plan aprobado por el autor antes de que el agente toque código | Flujo de desarrollo con IA: modo Plan y modo Build | `CLAUDE.md`, "Antes de tocar nada" |
| Verificación con severidades (crítico, aviso, sugerencia) antes de cerrar la story | Desarrollo moderno: SDD implementación (fase verify) | `CLAUDE.md`, "Al cerrar la story" |
| Las reglas que el agente añade sin que la story las pida se marcan y se preguntan | Herramientas: JetBrains AI Assistant (una regla inventada en la demo); Flujo: filosofía de uso | `CLAUDE.md`, tabla |
| Revisión humana con checklist antes de fusionar; ningún agente fusiona a `main`; nunca auto-merge | Herramientas: conclusiones de IAs desde CLI y de Codex; Desarrollo moderno: permisos por rama | ADR-008, `references/checklist-revision.md`, "Cómo leer este PR" en cada PR |
| MCP del editor de Unreal solo para consulta y tests; toda escritura de asset con confirmación, commit aparte y verificación en el editor | Flujo: qué son los MCP; Calidad: directo (mínimo privilegio en agentes) | `CLAUDE.md`, "Unreal MCP" |
| Refactors nativos del IDE antes que la IA para renombrar, extraer y cambiar firmas | Herramientas: JetBrains, refactors nativos | `CLAUDE.md` |
| Checklist de revisión de código: una razón de cambio por clase, sustitución de implementaciones de una interfaz, sin `switch` que crece, sin duplicación, sin código muerto | Ingeniería: SOLID (SRP como razón de cambio, Liskov), antipatrones; Calidad: code smells y refactor seguro | `references/checklist-revision.md`, parte B |
| Deuda como `TODO` con la regla que la motiva; comentario de cabecera en las funciones públicas del motor | Calidad: deuda técnica práctica, docs as code | `CLAUDE.md` |
| Validación que devuelve un resultado, sin excepciones | Arquitectura: value objects (adaptado: Unreal compila sin excepciones de C++) | `CLAUDE.md` |
| Cuando la story toca red: el servidor valida todo lo que llega del cliente y el PR responde qué pasa si un cliente miente | Seguridad: Injection, Insecure Design, Broken Access Control; Seguridad por diseño | ADR-002, `references/checklist-revision.md` |
| Toda pantalla define estados normal, vacío, error y carga con texto real; contraste mínimo 4.5:1 en HUD y subtítulos en Steam Deck; una sola forma de entrada para ratón y mando | Herramientas: introducción al diseño de webs y apps; Calidad: heurísticas de usabilidad y accesibilidad, microcopy | `references/plantilla-gdd-sistema.md`, GDD 07 |
| Presupuesto de renderizado y 60 FPS medidos en Deck en cada hito | Calidad: performance de usuario (presupuestos de rendimiento) | ADR-006, skill (restricciones fijas) |
| Build, tests y empaquetado reproducibles en local con dos scripts, los mismos que correría la integración continua | Infraestructura: qué es CI/CD (reproducible en local), automatizar lo que se repite | `Scripts/build.ps1`, `Scripts/test.ps1` |
| Alcance como MVP con el núcleo jugable y una lista de recortes ordenada | Herramientas: qué es un MVP, validar antes de invertir; Proyecto final: MVP con funcionalidades core | Skill (alcance del TFM), decisión de TFM |
| Bucle de autoría del GDD: contexto, preguntas, opciones, decisión del autor, borrador, aprobación | Ingeniería: RFC y design doc; Herramientas: directo (entrevistar a la IA antes de generar); Flujo: zona roja y zona verde | Skill, "Bucle de autoría" |
| Bitácora por story con cuatro datos de proceso fijos (tiempo, preguntas, correcciones, verde a la primera) | Desarrollo moderno: productividad (medir experiencia, no volumen); Proyecto final: el log del proceso suma | `docs/bitacora.md` |
| Lectura del temario con agentes en paralelo que devuelven un resumen, y consolidado a mano | Desarrollo moderno: orquestador y subagentes | `docs/bitacora.md`, D-001 |

## 2. Prácticas del temario que no se adoptan, y por qué

| Práctica | Por qué no | Qué se hace en su lugar |
|---|---|---|
| Microservicios, brokers de eventos, patrón outbox, contenedores, Kubernetes, infraestructura como código, costes cloud | Soulbound es un ejecutable de escritorio: un proceso, sin backend ni base de datos propios. El online es un servicio gestionado por Epic y la distribución es Steam | Monolito modular; el "despliegue" es la build en Steam |
| Vulnerabilidades web (XSS, CSRF, SQL injection, SSRF), JWT propios, cookies, cabeceras HTTP | No hay HTTP ni HTML propios; la identidad la dan Steam y EOS | El principio sí: nada que venga del cliente se aplica sin validarlo en el servidor |
| Vault de secretos con rotación automática; inventario de dependencias automatizado | Sin backend que lo justifique; los identificadores de EOS que necesita el cliente acaban en el binario igualmente. Los plugins de terceros no están versionados, así que ninguna herramienta los escanea | Filtro y hook de Git; versiones exactas de cada dependencia anotadas a mano en el ADR correspondiente |
| Revisor de PR con IA de pago (CodeRabbit), reportes periódicos, automatizaciones de recordatorio | Proyecto de un autor con PR pequeñas; el propio instructor no lo recomienda para proyectos pequeños | Checklist de revisión y "Cómo leer este PR" en cada PR; la bitácora hace de reporte |
| Dependabot y Code Scanning | Cubren solo el plugin propio y los scripts; el motor y los plugins de terceros no están en el repositorio | Se activan por ser gratuitos, sin contar con ellos como garantía |
| Orquestadores de flujos de IA (n8n), SDK unificadores, agentes de voz en tiempo real | La voz y el arte se generan en tiempo de desarrollo, no en producción; el Warden habla con frases pregeneradas | Script de generación fuera del código del juego, con claves en un `.env` ignorado |
| Feature flags y despliegue progresivo | Sin usuarios en producción durante la Fase 1 | Playtests presenciales con las métricas M1 a M4 |
| Memoria persistente automática del agente entre sesiones | Reintroduce conocimiento fuera del repositorio, invisible para el autor | La memoria entre sesiones son la bitácora, los ADR y el glosario, versionados |
| Varios agentes en paralelo sobre el mismo código; permisos amplios al agente; agente que fusiona el PR | El autor quiere entender cada cambio antes del siguiente; el plugin MCP de Unreal tiene defectos documentados; el trabajo lo firma el alumno | Una story cada vez; confirmación por escritura de asset; el agente abre el PR y el autor lo lee y fusiona. El paralelismo se reserva a lectura y resúmenes |
| Modelos locales para programar con agentes | El propio ponente los descarta por velocidad y calidad en flujos agénticos | Modelo alojado |
| Git Flow con rama `develop`, ramas de release y entorno de staging | Un autor, sin entorno que desplegar | Rama por story contra la base, PR, merge commit |
| Hooks de pre-commit que compilan y testean | Compilar Unreal tarda minutos, no segundos | Build y tests antes de abrir el PR; si se automatiza, en CI o en pre-push |
| Documentación de agente en inglés | El TFM se evalúa en español | Documentación y textos en español; identificadores de código en inglés, con correspondencia en el glosario |
| Explicabilidad de modelos y obligaciones del AI Act | El Warden es una máquina de reglas determinista por semilla, no un modelo entrenado; no es un sistema de alto riesgo | El pilar "Caos legible": todo castigo tiene nombre, causa visible y consecuencia clara. El único dato personal que se usa es el identificador de Steam o EOS para la sesión |

## 3. Pendiente de aplicar

| Práctica | Bloque | Cuándo |
|---|---|---|
| Integración continua en GitHub Actions con runner propio (compilar Unreal exige una máquina con el motor instalado) | Infraestructura, Seguridad, Ingeniería, Flujo, Calidad | Story después de S-003 |
| Registro de eventos de dominio en el motor (evento en pasado, marca de tiempo, destinatario) para la métrica M1 y para auditoría en playtests | Arquitectura (event-driven), Seguridad (logging y monitorización) | Story del Warden |
| Voz del Warden pregenerada con un script reproducible, una llamada por frase, el texto como subtítulo, licencia comercial verificada antes | APIs de IA (texto a voz) | Story del Warden |
| Checklist de hardening antes de la build de Shipping (consola, comandos de depuración, logs) y mensajes de error genéricos para el jugador | Seguridad (Security Misconfiguration) | Antes de subir a Steam |
| Validación al arrancar de la configuración de EOS, con mensaje claro si falta una clave | Calidad (validación fail-fast) | Junto a la anterior |
| Límite de intentos al unirse a una partida por código | Seguridad (fuerza bruta) | Antes de abrir el juego a jugadores externos |
| Etiqueta de versión semántica en `main` por hito | Infraestructura | Primer hito |
| Mockups navegables del HUD y los menús antes de tocar los Blueprints; sistema de diseño escrito cuando se decida el arte | Herramientas (diseño visual) | Story de interfaz |
| Priorización MoSCoW de los eventos del Warden y diagramas de secuencia en el GDD | Ingeniería, Arquitectura | GDD del Warden |
