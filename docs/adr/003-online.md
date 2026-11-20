# ADR-003. Online: plugin propio sobre Redpoint EOS, login por Steam en la build

**Estado**: aceptado el 23/09/2026 (spike, pasos 0.3, 0.5, 0.6, 1 y 7).

## Contexto

El GDD 08 fija servidor de escucha (el creador es el servidor), sesión por código de cuatro letras, sin voz ni replicación propia del plugin. El autor tiene un plugin (`A27Online`) que ya lo hacía en el proyecto anterior, cuenta de desarrollador de Steam con la app creada, y el producto dado de alta en el portal de Epic con Steam como proveedor de identidad. El TFM necesita que quien evalúa y los playtesters arranquen una build sin la herramienta de desarrollo.

## Decisión

Tres capas, cada una intercambiable sin tocar la de arriba:

1. **`A27Online`** (propio, versionado): `UGameInstanceSubsystem` con login, crear sesión con el código como atributo, buscar por código, unirse, salir. Solo usa la interfaz genérica de Unreal (`IOnlineSubsystem`, `IOnlineIdentity`, `IOnlineSession`). No se modifica desde stories de juego; sus cambios son stories propias.
2. **Redpoint EOS Online Framework, Free Edition** (de terceros, no versionado): implementa la interfaz sobre Epic Online Services y transporta la partida por P2P de EOS. Condiciones: menos de 30 000 USD al año, y siempre la última versión de Unreal (ver ADR-007).
3. **Identidad**: en la build empaquetada, **Steam** (`[OnlineSubsystemSteam]` con `bEnabled=True` y el App Id del producto; grafo de autenticación `Default`, que con Steam abierto lo elige solo). En el editor, la Developer Authentication Tool de Epic (`EditorAuthenticationGraph=DevAuthTool`), porque Steam solo funciona en builds y dos ventanas de PIE no pueden compartir cuenta. Caída documentada: grafo `Anonymous` de Redpoint (cuentas por dispositivo, sin nombre).

## Alternativas descartadas

- `OnlineSubsystemEOS` del motor en vez de Redpoint: sin dependencia de terceros ni atadura de versión, pero login y sesiones más toscos y un día de incógnita en el spike. **Queda como salida prevista para la Fase 2**: cambiar `FName("RedpointEOS")` por el subsistema por defecto, la sección de configuración y el login; el resto de `A27Online` no cambia.
- Anonymous en la Fase 1: sin fricción para quien evalúa, pero sin nombre de jugador y sin camino a Steam; Steam ya estaba configurado y es el destino del producto.

## Consecuencias

- Quien pruebe la build necesita Steam abierto y una clave de la app (entregable admitido por las reglas del TFM). El nombre del jugador lo da Steam.
- **Story pendiente**: el juego debe tolerar el login fallido (sin Steam o sin conexión) y permitir crear partida local igualmente (R-FLU del GDD 07).
- Comprobado: sesión por código entre dos jugadores en PIE; build empaquetada entra por Steam; reconexión con el mismo código tras una caída (R-CAI-2 deja de ser opcional; lo que queda es que el que vuelve recupere su asiento del bot).
- Riesgo de Fase 3: el plugin de Meta para Quest suele ir por detrás del motor y Redpoint gratuito exige ir en la última versión; la salida es la alternativa 1.

## Nota del 20/11/2026, al cierre del TFM

La consecuencia "quien pruebe la build necesita Steam abierto y una clave" ya no es verdad. La entrega es partida local: la build arranca sin Steam, sin cuenta y sin conexión, y si Steam está abierto intenta el login sin que la partida dependa de él. La story del login fallido se resolvió así. La partida en línea queda para la Fase 2; la sesión por código sigue existiendo como prueba técnica.
