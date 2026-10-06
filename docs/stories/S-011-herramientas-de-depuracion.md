# S-011. Herramientas de depuración

**Sistema**: herramientas de prueba (sin GDD propio; sirven a los criterios manuales de todas las stories). **Módulos**: `SoulboundCore` (ediciones del estado) y `Soulbound` (comandos de consola). **Rama base**: `main`. **Tamaño**: pequeña-media.

## Contexto

Para comprobar a mano un salto planar, una expulsión o un robo, hoy hay que jugar una partida entera hasta que se dé el caso; en S-010 un criterio manual quedó "no visto en PIE" por eso. Con el duelo será peor ("ganar siendo el retado" es cuestión de suerte). Esta story da al autor comandos de consola para llevar la partida al estado que quiere probar: dar o quitar runas, colocar una ficha, vincular un asiento, sellar, terminar la carrera, fijar la próxima tirada o el resultado del próximo minijuego. Son herramientas de desarrollo: no existen en la build de entrega y cada uso queda en el registro, para que una partida manipulada nunca pase por una normal.

Decidido por el autor el 05/10/2026: las herramientas van antes que el duelo (que pasa a S-012); guardar y cargar escenarios desde archivo queda para después.

## Qué se construye

1. **Ediciones del estado, en el motor.** Funciones nuevas en `SoulboundCore` (cabecera propia, nombre a proponer), una por edición, que dejan el estado coherente y devuelven si se aplicó y, si no, por qué. Cada edición aplicada registra un evento de edición de depuración con su tipo, el asiento afectado y la marca de tiempo.
2. **Comandos de consola, en el juego.** En `ASpikeOnlinePlayerController`, como los `Sb…` que ya existen. Solo con autoridad y solo con partida iniciada. Piden la edición al anfitrión de partida (`UMatchHostSubsystem`), que sigue siendo el único que escribe el estado y avisa a la vista. Cada comando contesta en pantalla qué hizo o por qué no.
3. **Fuera de la build de entrega.** En configuración Shipping los comandos no existen o no hacen nada; se dice en el PR cómo y se comprueba.

| Comando | Hace |
|---|---|
| `SbGiveRune <asiento> <tipo> [n]` | Da n runas (1 si no se dice) de ese tipo al asiento, sacándolas de la reserva de su Fuente. Sin reserva suficiente, o con el tablero en Primordial, se rechaza |
| `SbTakeRune <asiento> <tipo> [n]` | Quita runas al asiento y las devuelve a la reserva (`ReturnRuneToSource`) |
| `SbMove <asiento> <casilla>` | Coloca la ficha en la casilla, sin resolverla (ni Nicho, ni Fuente, ni acciones) y sin contar como movimiento del turno |
| `SbBond <asiento> <dominio>` | Vincula al asiento con el Elegido del Dominio, como si hubiera reclamado su Nicho: mismo estado final que la regla (Nicho Reclamado, dueño, rol, Despertar si es el primero, fin de carrera si era el último Nicho Libre). Solo en carrera, con el asiento Eco y el Nicho Libre |
| `SbSeal <casilla>` / `SbUnseal <casilla>` | Sella o libera con las reglas de Sello (`SealTile`, `ReleaseSeal`); lo prohibido se rechaza con su motivo |
| `SbEndRace` | Termina la carrera en este instante, con la regla de fin de carrera (`EndBondRace`) |
| `SbRoll <n>` | Fija el resultado de la próxima tirada del asiento activo. El sorteo de la partida consume lo mismo que sin fijar (consumo fijo de S-009). La repetición de Apep, si la usa, tira de verdad |
| `SbTrial pass` / `SbTrial fail` | Fija el resultado del próximo minijuego de habilidad del humano (hoy lo resuelve un dummy al azar) |
| `SbTurn <asiento>` | El siguiente turno que empiece es de ese asiento; después el orden sigue desde él |
| `SbHelp` | Lista los comandos con su sintaxis |

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **Runas.** Dada una Fuente con reserva R y un asiento con K runas de su tipo, cuando se le dan n, tiene K más n y la reserva es R menos n; cuando se le quitan, al revés. La suma de runas de ese tipo en Brazales y reserva no cambia nunca. Con n mayor que la reserva, en Primordial, o quitando más de las que tiene, se rechaza con motivo y nada cambia.
- **Colocar.** Dado un asiento, cuando se coloca en una casilla válida, está en ella y nada más cambia: ni Nicho, ni runas, ni `bActiveSeatMoved`, ni el turno. Casilla inexistente: se rechaza.
- **Vincular.** Dado un Eco en carrera y un Dominio con el Nicho Libre, cuando se vincula con la edición, el estado es el mismo que si hubiera reclamado el Nicho con la regla (`ResolveNicheLanding`): se comparan los dos estados salvo la posición de la ficha y el registro. Si es el primer vínculo, el tablero despierta; si era el último Nicho Libre, la carrera termina. Fuera de carrera, con un Elegido, o con el Nicho Reclamado o Cerrado, se rechaza con motivo.
- **Sellos.** Las ediciones de Sello aceptan y rechazan exactamente lo que `CanSealTile` y `ReleaseSeal`, con el mismo motivo.
- **Fin de carrera.** Dada la carrera en curso, cuando se termina con la edición, el estado es el que deja `EndBondRace`. Fuera de carrera, se rechaza.
- **Tirada fijada.** Dado un valor fijado, la siguiente tirada del asiento activo es ese valor y las alcanzables son las de ese valor; la de después vuelve al sorteo. Con y sin tirada fijada, el sorteo de la partida queda en el mismo punto al acabar el turno. Un valor menor que 1 o mayor que el máximo de dos dados se rechaza.
- **Siguiente asiento.** Dado un asiento pedido, el siguiente turno que empieza es suyo y el orden continúa desde él. Asiento inexistente: se rechaza.
- **Registro.** Toda edición aplicada deja su evento de depuración, con tipo, asiento y marca de tiempo; una edición rechazada no deja nada.
- **Simulación intacta.** Sin ediciones, las simulaciones y sus cifras no cambian. Los 114 tests actuales siguen en verde.

Manuales, en PIE:

- **Un minuto por caso.** Con los comandos, el autor provoca en menos de un minuto cada uno: un salto planar, una expulsión, un robo de Eco con trilero y un minijuego fallado en la Fuente de un Olvidado. Captura de la crónica de cada uno, con la línea de edición de depuración visible.
- **Mensajes.** Un comando mal escrito o rechazado dice por qué en pantalla; `SbHelp` lista todos.
- **Entrega.** En una build Shipping (o con la comprobación que se describa en el PR), los comandos no hacen nada.

## Notas para quien lo implemente

- Nombres del glosario: anfitrión de partida `MatchHost`, registro de eventos `EventLog`, Brazal `FSeatState::Runes`, reserva `FTileState::RuneReserve`, reglas de Sello `SealTile`/`ReleaseSeal`, reglas de carrera `ResolveNicheLanding`/`EndBondRace`. Nombres nuevos (cabecera de ediciones, resultado de una edición, evento, campos de tirada fijada y de siguiente asiento) se proponen en el plan y en el PR.
- Las ediciones **reutilizan las reglas** siempre que existan (vincular, sellar, devolver una runa, terminar la carrera); solo escriben a mano lo que ninguna regla hace (colocar una ficha, dar una runa desde la reserva).
- Tirada fijada y siguiente asiento son datos en el estado que la máquina de turnos consulta en su punto; propón dónde. El resultado fijado del minijuego del humano vive en el anfitrión (es quien responde hoy por el humano), no en el motor.
- Las ediciones no saltan esperas: si la máquina espera un destino o una elección, la edición se aplica y la espera sigue. No se comprueba el confinamiento de los Ecos al colocar: es depuración, y queda en el registro.
- Vista: el evento de depuración tiene su línea en la crónica, en un color propio.
- Sin assets. Nada de `Engine` en `SoulboundCore`.
- S-012 (duelo) añadirá su comando (`SbDuel win|lose`) con el mismo patrón.

## Fuera de esta story

- Guardar y cargar una partida o un escenario desde archivo: story posterior, junto con la grabación y reproducción del GDD 08.
- Editar el tablero en caliente (añadir casillas, cambiar Olvidados): se hace con el archivo del tablero.
- Herramientas para el Warden (forzar un golpe, cambiar de acto): su story.
