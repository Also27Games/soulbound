# Soulbound: guía para agentes

Soulbound es un party game de terror social con un anfitrión IA, el Warden. Unreal Engine 5.8, C++ primero. Este repositorio es el Trabajo de Fin de Máster de Lluis Alonso y, a la vez, la Fase 1 del producto. Se evalúan el proceso y la calidad del código tanto como el resultado.

## Antes de tocar nada

1. Carga la skill `soulbound` (`.claude/skills/soulbound/SKILL.md`): glosario oficial, pilares, restricciones fijas, plantilla de GDD, checklist de revisión. Los nombres de clases, structs, funciones y variables salen del glosario (`references/glosario.md`); si un concepto no está, se propone en el PR, no se inventa en el código.
2. Lee la story que te han asignado entera, incluidos los criterios de aceptación. Cada criterio se convierte en al menos un test. Si la story contradice un GDD o al glosario, para y pregunta en el PR; no decidas por tu cuenta.

## Arquitectura en dos líneas

- `Source/SoulboundCore/`: el motor de reglas **sin mundo**. Solo depende de `Core` y `CoreUObject`. Estado en `USTRUCT`, reglas como funciones libres o clases sin `UWorld`, `UObject` ni `Engine`. Todo lo que sea regla de juego vive aquí y se prueba aquí.
- `Source/Soulbound/`: el juego. Subsistemas (`UWorldSubsystem`, `UGameInstanceSubsystem`), GameMode, controladores, presentación. Llama al motor; nunca duplica reglas.
- `Plugins/A27Online/`: plugin propio de sesiones por código sobre Redpoint EOS. Dependencia externa: no se modifica desde una story de juego.
- Lo de terceros (`Plugins/OnlineSubsystemRedpointEOS`, `Plugins/EOS-SDK-*`, `Content/UltraGameTemplate`) no está en el repositorio y **no se toca ni se versiona**.

## Cómo se trabaja

- Rama por story: `story/S-NNN-nombre-corto` desde la rama base que indique la story. PR contra esa rama base con el título de la story.
- Compilar sin editor: `.\Scripts\build.ps1 -Que editor`. Tests sin editor: `.\Scripts\test.ps1` (comandlet, unos 12 s; imprime OK por test). Un PR no se abre con tests en rojo.
- Tests con `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, nombre `Soulbound.<Modulo>.<Sistema>.<QueSeComprueba>`, dentro de `#if WITH_DEV_AUTOMATION_TESTS`, en `Source/<Modulo>/Private/Tests/`. Cada test cita en un comentario el criterio (`CA-XXX-n`) y la regla (`R-XXX-n`) que cubre.
- Valores de juego (tiradas, tiempos, topes) nunca fijos en el código: parámetros con valor inicial documentado, pensados para salir de datos.
- Commits en español, cortos, en presente o infinitivo, diciendo qué cambia. **Sin líneas de coautoría ni de atribución de ninguna herramienta.**
- Nunca escribas valores de `ProductId`, `ClientSecret`, claves de licencia ni tokens en ningún archivo del repositorio. Un hook los rechaza; si te lo encuentras, no lo esquives.
- No crees ni edites assets de Unreal (`.uasset`, `.umap`) desde un agente. Si una story los necesita, deja escrito en el PR qué asset y qué contenido, y lo hace una persona.

## Unreal MCP (editor abierto)

`.mcp.json` conecta con el servidor MCP del editor de Unreal (`127.0.0.1:8000/mcp`). Solo existe si el editor está abierto y el servidor arrancado (arranca solo, o con `ModelContextProtocol.StartServer` en la consola del editor). Las herramientas se descubren con `list_toolsets` y `describe_toolset`, y se invocan con `call_tool` pasando el **nombre corto** de la herramienta (por ejemplo `find_actors`) junto con `toolset_name`.

- **Sí**: leer el nivel y los actores (`SceneTools`), leer logs, consultar configuración, y **lanzar tests** (`AutomationTestToolset`): con el editor abierto tardan décimas de segundo en vez de los 12 s del comandlet.
- **Nunca**: crear, modificar o borrar assets a través del MCP (materiales, Blueprints, mapas, Data Assets, texturas…). Es experimental y hay defectos documentados que borran propiedades o sobrescriben archivos. Si una story necesita assets, se deja escrito en el PR y lo hace una persona.
- Condición legal: lo que el editor envía al modelo por este canal es tecnología licenciada bajo la EULA de Unreal (sección 6(e)); el proveedor del modelo no debe usarlo como datos de entrenamiento. La cuenta que use este MCP tiene esa opción desactivada.

## Al cerrar la story

El autor del proyecto tiene que poder entender cada cambio sin haber estado delante. Por eso, antes de abrir el PR:

1. **Bitácora**: añade una entrada a `docs/bitacora.md` con el formato de las anteriores (qué se pidió, qué se decidió y por qué, cómo funciona, dudas y resolución, tests, pendiente, proceso). Diez líneas, en el mismo PR.
2. **Descripción del PR** con estas secciones, en este orden:
   - **Story y criterios**: cuáles se cubren y con qué test cada uno.
   - **Cómo leer este PR**: archivo por archivo, en el orden en que conviene leerlos, qué cambia y por qué, citando la regla (`R-XXX-n`) que lo motiva. Cinco minutos de lectura antes de abrir el diff.
   - **Decisiones tomadas** que no estaban en la story, con la alternativa descartada.
   - **Dudas** sobre el GDD o el glosario, y **pendiente** (qué quedó fuera y por qué).
3. Compila y pasa los tests por última vez después de escribir la documentación.

El revisor usa `references/checklist-revision.md` de la skill. Una story cada vez: no se abre la siguiente hasta que la anterior está fusionada y leída.
