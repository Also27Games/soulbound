# Soulbound: guía para agentes

Soulbound es un party game de terror social con un anfitrión IA, el Warden. Unreal Engine 5.8, C++ primero. Este repositorio es el Trabajo de Fin de Máster de Lluis Alonso y, a la vez, la Fase 1 del producto. Se evalúan el proceso y la calidad del código tanto como el resultado.

**Stack, con versiones**: Unreal Engine 5.8.3; C++ con el toolchain de Visual Studio 2022 (MSVC 14.4x); .NET SDK 10.0.2xx (runtime 10.0.7, el del motor); Redpoint EOS Online Framework Free Edition para 5.8 y EOS SDK 1.19.2.1; Ultra Game Template 1.3 y Ultra Loading Screen 1.2; Rider 2026.1; Git con LFS. Cuando propongas una API o un ajuste, que sea de estas versiones, no de otras.

## Antes de tocar nada

1. Carga la skill `soulbound` (`.claude/skills/soulbound/SKILL.md`): glosario oficial, pilares, restricciones fijas, plantilla de GDD, checklist de revisión. Los nombres de clases, structs, funciones y variables salen del glosario (`references/glosario.md`); si un concepto no está, se propone en el PR, no se inventa en el código.
2. Lee la story que te han asignado entera, incluidos los criterios de aceptación. Cada criterio se convierte en al menos un test. Si la story contradice un GDD o al glosario, para y pregunta en el PR; no decidas por tu cuenta.
3. **Plan antes de código**: presenta al autor la lista de archivos que vas a tocar, qué criterio cubre cada test y qué dudas tienes, y espera su visto bueno. Hasta entonces no escribes ni un archivo.

| Documento | Para qué |
|---|---|
| `.claude/skills/soulbound/SKILL.md` | Qué es el juego, pilares, restricciones, alcance, bucle de autoría |
| `references/glosario.md` | Nombres oficiales de conceptos y su identificador en código |
| `references/plantilla-gdd-sistema.md` | Estructura de un GDD de sistema |
| `references/checklist-revision.md` | Revisión de un GDD y revisión de un PR de código |
| `docs/decisiones.md` | Las decisiones que definen el proyecto, dónde verlas y qué se dejó fuera a sabiendas |

## Arquitectura en dos líneas

- `Source/SoulboundCore/`: el motor de reglas **sin mundo**. Solo depende de `Core`, `CoreUObject` y `Json` (este último solo para cargar datos). Estado en `USTRUCT`, reglas como funciones libres o clases sin `UWorld`, `UObject` ni `Engine`. Todo lo que sea regla de juego vive aquí y se prueba aquí.
- `Source/Soulbound/`: el juego. Subsistemas (`UWorldSubsystem`, `UGameInstanceSubsystem`), GameMode, controladores, presentación. Llama al motor; nunca duplica reglas. Los actores son adaptadores: entrada, replicación y presentación; nunca contienen reglas.
- `Plugins/A27Online/`: plugin propio de sesiones por código sobre Redpoint EOS. Dependencia externa: no se modifica desde una story de juego.
- Lo de terceros (`Plugins/OnlineSubsystemRedpointEOS`, `Plugins/EOS-SDK-*`, `Content/UltraGameTemplate`) no está en el repositorio y **no se toca ni se versiona**.

## Siempre, preguntar, nunca

Cada "nunca" tiene un motivo real detrás; no es burocracia.

| | Regla | Motivo |
|---|---|---|
| **Siempre** | Un test por criterio de aceptación, escrito **antes** de implementar y visto fallar | Un test que nace en verde no comprueba nada |
| **Siempre** | Compilar y pasar los tests antes de abrir el PR, y otra vez tras escribir la documentación | Un PR no se abre con tests en rojo |
| **Siempre** | Nombres del glosario; parámetros de juego con valor inicial documentado, nunca fijos en el código | Los valores salen de datos y se equilibran en playtest |
| **Preguntar** | Un concepto que no está en el glosario | Se propone en el PR; el autor lo incorpora o lo rechaza |
| **Preguntar** | Una regla de juego que no pide la story, aunque parezca obvia | Se marca "decisión no solicitada" en el PR y se pregunta. Un asistente de IA inventó una regla de negocio en una demo del máster; aquí no |
| **Preguntar** | Cualquier escritura de un asset por el MCP de Unreal: qué asset, qué cambio | El plugin es experimental y tiene defectos documentados que borran propiedades |
| **Nunca** | Fusionar a `main` ni a la rama base de la story | El agente hace commits en su rama y abre el PR; leer y fusionar es del autor |
| **Nunca** | Líneas de coautoría o atribución de herramientas en commits o PR | Norma del TFM: el trabajo lo firma el alumno |
| **Nunca** | Valores de `ProductId`, `ClientSecret`, claves de licencia o tokens en ningún archivo | Un hook los rechaza; si te lo encuentras, no lo esquives |
| **Nunca** | Tocar o versionar plugins y contenido de terceros | Licencias de Redpoint, EOS SDK, UGT y ULS |
| **Nunca** | Assets de Unreal como efecto lateral de una story de código | Solo si la story lo pide, por MCP, con confirmación, en commit aparte, verificados en el editor |
| **Nunca** | Salir del directorio del proyecto | `Config/Local/EOS.ini` está en disco con secretos, aunque no en Git |

## Cómo se trabaja

- Rama por story: `story/S-NNN-nombre-corto` desde la rama base que indique la story. PR contra esa rama base con el título de la story.
- Compilar sin editor: `.\Scripts\build.ps1 -Que editor`. Tests sin editor: `.\Scripts\test.ps1` (comandlet, unos 12 s; imprime OK por test).
- Tests con `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, nombre `Soulbound.<Modulo>.<Sistema>.<QueSeComprueba>`, dentro de `#if WITH_DEV_AUTOMATION_TESTS`, en `Source/<Modulo>/Private/Tests/`. Cada test cita en un comentario el criterio (`CA-XXX-n`) y la regla (`R-XXX-n`) que cubre. Ciclo: test en rojo, implementación mínima, refactor.
- Convenciones de código: las funciones públicas de `SoulboundCore` llevan un comentario de cabecera (qué hace, parámetros, qué devuelve); la validación devuelve un resultado (bool, enum o tipo del glosario), nunca lanza excepciones (Unreal compila sin ellas); la deuda se deja como `// TODO(R-XXX-n): motivo y qué haría falta`, nunca un TODO mudo.
- Renombrar, extraer o cambiar firmas: con los refactors nativos de Rider, que actualizan todos los usos; la IA se reserva para lo que no es mecánico.
- Commits en español, cortos, en presente o infinitivo, diciendo qué cambia.
- **Assets de Unreal** (`.uasset`, `.umap`): solo si la story lo pide de forma explícita, nunca como efecto lateral de una story de código. Cuando lo pida: siempre a través del MCP de Unreal (nunca editando binarios ni copiando archivos), pidiendo confirmación antes de cada escritura (qué asset, qué cambio), en un commit aparte del código, con una descripción textual de cada asset en el PR; una persona los abre en el editor y los verifica antes de fusionar, porque el diff no los muestra. Preferencia: lo que pueda generarse desde datos, se genera.

## Unreal MCP (editor abierto)

`.mcp.json` conecta con el servidor MCP del editor de Unreal (`127.0.0.1:8000/mcp`). Solo existe si el editor está abierto y el servidor arrancado (arranca solo, o con `ModelContextProtocol.StartServer` en la consola del editor). Las herramientas se descubren con `list_toolsets` y `describe_toolset`, y se invocan con `call_tool` pasando el **nombre corto** de la herramienta (por ejemplo `find_actors`) junto con `toolset_name`.

- **Sí**: leer el nivel y los actores (`SceneTools`), leer logs, consultar configuración, y **lanzar tests** (`AutomationTestToolset`): con el editor abierto tardan décimas de segundo en vez de los 12 s del comandlet.
- **Escribir assets** (materiales, Blueprints, mapas, Data Assets, texturas…): solo con las condiciones del apartado "Cómo se trabaja" (story que lo pide, confirmación antes de cada escritura, commit aparte, verificación en el editor). El plugin es experimental y tiene defectos documentados que borran propiedades o sobrescriben archivos: por eso cada escritura se confirma y se verifica.
- Condición legal: lo que el editor envía al modelo por este canal es tecnología licenciada bajo la EULA de Unreal (sección 6(e)); el proveedor del modelo no debe usarlo como datos de entrenamiento. La cuenta que use este MCP tiene esa opción desactivada.

## Al cerrar la story

El autor del proyecto tiene que poder entender cada cambio sin haber estado delante. Por eso, antes de abrir el PR:

1. **Verificación**: repasa cada criterio de la story contra el código y los tests y clasifica lo que encuentres en **crítico** (un criterio no se cumple o un test no lo comprueba de verdad), **aviso** (cumple, pero con deuda o una duda de diseño) o **sugerencia**. Lo crítico se arregla antes de seguir; lo demás va al PR.
2. **Bitácora**: añade una entrada a `docs/bitacora.md` con el formato de las anteriores (qué se pidió, qué se decidió y por qué, cómo funciona, dudas y resolución, tests, pendiente, proceso). Diez líneas, en el mismo PR. La línea de proceso lleva siempre los mismos cuatro datos: tiempo, preguntas que pararon al agente, correcciones tras fusionar (las rellena el autor), y si build y tests pasaron a la primera.
3. **Descripción del PR** con estas secciones, en este orden:
   - **Story y criterios**: cuáles se cubren y con qué test cada uno.
   - **Cómo leer este PR**: archivo por archivo, en el orden en que conviene leerlos, qué cambia y por qué, citando la regla (`R-XXX-n`) que lo motiva. Cinco minutos de lectura antes de abrir el diff.
   - **Decisiones tomadas** que no estaban en la story, con la alternativa descartada, y las **decisiones no solicitadas** que se descartaron.
   - **Dudas** sobre el GDD o el glosario, y **pendiente** (qué quedó fuera y por qué).
4. Compila y pasa los tests por última vez después de escribir la documentación.

El revisor usa `references/checklist-revision.md` de la skill (sección de PR de código). Una story cada vez: no se abre la siguiente hasta que la anterior está fusionada y leída por el autor.
