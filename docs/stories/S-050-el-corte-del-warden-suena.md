# S-050. El corte del Warden suena: un golpe de sonido con la señal de corte

**Sistema**: la voz del Warden (señal de corte, R-VOZ-8) e interfaz en plano (GDD 07; pilar 3, caos legible, y pilar 4, el Warden te habla a ti). **Módulos**: `Soulbound` (presentación: `BoardViewActor`, `WardenLineQueue`). `SoulboundCore` no cambia. **Rama base**: `main`, con S-047 fusionada (PR 78, 19/11/2026). **Tamaño**: pequeño. **Assets**: un sonido, que importa el autor en el editor, en commit aparte.

## Contexto

En la prueba del 15/11/2026 (P4), lo que menos se entendió fue que el Warden corta el turno: de 22 turnos, cinco se los cortó un golpe. El autor decidió ese día que el corte tuviera una señal propia con tres partes: un golpe de sonido, el tablero que se oscurece un instante y la palabra de corte del Warden. S-041 hizo las dos últimas y dejó fuera el sonido, porque no había asset y el audio estaba pasado a la Fase 2.

Decidido por el autor el 19/11/2026: el sonido del corte entra ahora, antes de la prueba con P2, para comprobar con él si la señal basta. Es el único sonido del juego en esta fase.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-VOZ-12 (nueva). El corte suena.** Cuando el Warden corta un turno y el tablero se oscurece (R-VOZ-8, punto 1: un golpe o una escena que corta el turno, sea el turno del humano o el de otro asiento), suena a la vez un golpe de sonido.
  - **Una vez por corte**, al empezar el primer pulso del oscurecido; no una vez por pulso.
  - **Los mismos casos que el oscurecido, ni uno más**: el corte por el fin de la carrera y el del fin de la partida no suenan; ninguna otra frase del Warden suena.
  - **Sin posición en el mundo**: es un sonido de interfaz, se oye igual en la vista sentada y en la cenital.
  - **El asset es opcional**, como la textura de la runa del anillo o la del dado: su ruta va en Config (nombre propuesto `CutSound`), vacía de inicio hasta que el autor importe el sonido; sin asset, o si no se puede cargar, el corte no suena, la señal sale como hoy y se dice una vez en el registro.
  - **Volumen en Config** (nombre propuesto `CutSoundVolume`, valor inicial 1,0; 0, no suena).
- **El sonido se cocina.** La carpeta del sonido entra en `DirectoriesToAlwaysCook` (`Config/DefaultGame.ini`), como las de S-049: un asset cargado por ruta no se empaqueta si su carpeta no está ahí. El test `ComoSeJuega.CarpetasQueSeCocinan` ya recorre todas las rutas de asset de la Config de la vista y tiene que seguir pasando con la ruta nueva puesta.
- **Lo que no cambia.** R-VOZ-1: el Warden sigue sin voz; sus frases son subtítulos. La palabra de corte, la línea pequeña y el oscurecido, como están. Ninguna regla del Warden: ni cuántos golpes, ni cuándo.

## El sonido (del autor)

- Un golpe corto y seco (menos de un segundo), propio del autor o con licencia que permita usarlo y distribuirlo en el juego. El PR dice de dónde sale y con qué licencia; va a los créditos.
- Del juego de mesa que inspira el proyecto se toma la idea de la señal, **nunca su sonido ni su palabra**.
- Lo importa el autor en el editor (carpeta propuesta `Content/Audio/`, nombre propuesto `S_CorteWarden`), pone la ruta en Config y lo sube en un commit aparte. El agente no crea ni toca assets.
- La clase de sonido del asset (para que obedezca al volumen de los ajustes del juego) la elige el autor en el editor; el código no depende de la plantilla.

## Criterios de aceptación

Automáticos, sin mundo.

- **CA-VOZ-15** (R-VOZ-12, cuándo). La función que dice si una frase que se empieza a enseñar hace sonar el corte devuelve sí para un corte del Warden (golpe o escena que corta el turno), del humano o de otro asiento, y no para el corte por el fin de la carrera, para el del fin de la partida y para cualquier otra frase. Es la misma condición que arranca el oscurecido: si ya existe como función, se reutiliza y el test lo comprueba; no se duplica.
- **CA-VOZ-16** (R-VOZ-12, una vez). Un corte con varios pulsos de oscurecido pide el sonido una sola vez; dos cortes seguidos, dos veces.
- **CA-VOZ-17** (R-VOZ-12, opcional y volumen). Sin sonido cargado, o con el volumen a 0, no se pide ningún sonido y nada falla.
- **CA-VOZ-18** (cocinado). Con la ruta del sonido puesta en Config, su carpeta está en `DirectoriesToAlwaysCook` (el test de las carpetas que se cocinan pasa con ella).

Manuales, por el autor, en PIE y en la build empaquetada:

- Con `SbStrike <id>` sobre el turno del humano, el golpe suena a la vez que el tablero se oscurece, una sola vez.
- Suena también cuando el Warden corta el turno de otro asiento.
- No suena al terminar la carrera (`SbEndRace`) ni con las burlas.
- Sin la ruta en Config, la partida va como hoy y el registro lo dice una vez.
- El volumen de los ajustes del juego: se anota si el golpe lo obedece o no. Si no lo obedece, queda como límite conocido.
- En la build empaquetada, el golpe suena.

## Notas para quien lo implemente

- **Dónde arranca la señal.** En `ABoardViewActor`, donde una frase con `bWardenCut` arranca el oscurecido (`CutDimStartReal`); `CutDimOpacity` en `WardenLineQueue`. El sonido sale del mismo sitio.
- **Cómo suena.** Con lo que ya trae el módulo `Engine` (un sonido de interfaz, sin componente en el mundo). No hace falta ninguna dependencia nueva ni tocar la plantilla.
- **El patrón del asset opcional** es el de `RingRuneTexture` y `DiceFacesTexture`: ruta en Config, carga con aviso único, referencia viva mientras dure la vista.
- **Documentación que cambia en el mismo PR**, una línea en cada sitio: donde se dice que el audio queda para la Fase 2 (`SKILL.md`, `docs/decisiones.md` y el README), se precisa que lo único que entra es el golpe del corte. La voz del Warden sigue en la Fase 2.
- Los nombres nuevos se proponen en el plan y van al glosario en un commit aparte.
- Tests primero, en rojo.
- Mientras el autor prueba en PIE o trabaja en el editor, no se compila ni se lanza nada.

## Fuera de esta story

- Cualquier otro sonido: música, pasos, dados, el anillo, la voz del Warden (Fase 2).
- Ajustes de sonido propios: se usan los que haya.
- Cambiar cuándo corta el Warden o cuántas veces.
