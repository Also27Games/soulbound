# S-006. Carrera por el vínculo

**Sistema**: carrera y Elegidos (GDD 03), con las transiciones del tablero que dispara (GDD 01). **Módulos**: `SoulboundCore` (reglas) y `Soulbound` (vista). **Rama base**: `main`. **Tamaño**: media-grande.

## Contexto

La partida ya tiene tablero, turnos con reloj, Sellos con tope y una vista de depuración (S-005) donde cuatro fichas se mueven sin propósito: no hay nada que ganar. Esta story pone el primer objetivo del juego: los cinco minutos en que los Ecos compiten por un Nicho. Quien llega, se vincula a su Elegido y despierta el tablero; quien no, queda Eco y confinado a los Dominios Olvidados. El motor gana el estado del tablero (Primordial, Despierto), el rol de cada asiento y la fase de partida; la vista lo enseña; la simulación mide cuántas partidas acaban sin nadie vinculado, que es la primera cifra de diseño que el GDD pide medir.

Decidido por el autor el 01/10/2026: (1) motor y vista en una story, con un decisor dummy que busca el Nicho Libre más cercano (doble de prueba; el bot del GDD 08 es otra story); (2) el fin de carrera por tiempo se aplica al terminar el turno en curso, no como interrupción (`TODO(R-TAB-26)` para la story del Warden); (3) el vínculo es al Dominio: afinidades y mapa de runa a poder, en la story de runas.

## Reglas a implementar

- **R-TAB-8 y R-TAB-9.** El tablero está en Primordial desde el inicio hasta que el primer jugador reclama un Nicho; entonces pasa a Despierto, sin vuelta atrás, y los Nodos quedan revelados todos a la vez. (Nadie obtiene runas en Primordial: las runas no existen aún; la regla se recoge en el estado.)
- **R-CAR-1 y R-TAB-27.** La carrera empieza con la partida y dura un tiempo definido en datos (`partida.json`, valor inicial 300 s), o termina antes si se reclama el último Nicho Libre.
- **R-CAR-2.** Todos empiezan como Ecos Perdidos en el Altar, con un dado, y transitan por cualquier Dominio.
- **R-CAR-3.** Los Nichos de los Dominios no Olvidados empiezan Libres; los de los Olvidados, Cerrados (ya lo hace `StartMatch`; se comprueba).
- **R-CAR-4, 5 y 6.** Un Eco que termina su movimiento en un Nicho Libre lo reclama: el Nicho pasa a Reclamado por ese asiento, el Dominio tiene dueño, el asiento pasa a Elegido de ese Dominio y sigue tirando un dado. Terminar en un Nicho Reclamado o Cerrado no cambia nada; un Elegido que termina en otro Nicho Libre, tampoco. Un jugador no cambia de rol después.
- **R-CAR-8, con R-TAB-10, 11 y 12.** Al terminar la carrera, en este orden: si nadie se ha vinculado, la partida termina con victoria del Warden; todo Dominio sin dueño pasa a Olvidado con su Nicho Cerrado; cada Eco recibe al azar un Dominio Olvidado distinto como hogar; se registra el fin.
- **R-TAB-13.** El hogar de un jugador es el Altar durante la carrera, su Nicho reclamado si es Elegido, o el Nicho Cerrado de su Dominio asignado si es Eco. Función de consulta; nadie la usa aún (la usan el Vacío y los castigos).
- **R-TAB-19.** Las fichas nunca bloquean: ya es así; se fija con test.
- **R-TAB-20 y R-ECO-2.** Terminada la carrera, un Eco solo transita por el Altar y los Dominios Olvidados. Durante la carrera, por todo. Sin permiso temporal (decreto del Warden, su story).
- **Registro** (GDD 03, criterio final): vínculo, Despertar, fin de carrera, asignación de hogar y victoria del Warden emiten eventos con marca de tiempo y asiento.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-CAR-2** (R-CAR-2). Dado el inicio, todos los asientos son Eco, están en el Altar, tiran un dado y sus alcanzables incluyen casillas de cualquier Dominio.
- **CA-CAR-3** (R-CAR-3). Dado el inicio con N jugadores, hay N Nichos Libres (los de Dominios no Olvidados) y 6 menos N Cerrados.
- **CA-CAR-4 y CA-CAR-6** (R-CAR-4, 6). Dado un Nicho Libre, cuando un Eco termina en él, pasa a Reclamado por ese asiento; cuando otro jugador termina después en él, nada cambia; cuando un Elegido termina en otro Nicho Libre, nada cambia.
- **CA-CAR-5 y CA-TAB-10** (R-CAR-5, R-TAB-9). Dado un Eco que reclama, su rol es Elegido del Dominio del Nicho, sigue con un dado, y si era el primero el tablero pasa de Primordial a Despierto con todos los Nodos revelados en ese instante; un segundo vínculo no cambia el estado del tablero.
- **CA-CAR-1 y CA-TAB-28** (R-CAR-1, R-TAB-27). Dada la carrera, termina al reclamarse el último Nicho Libre aunque quede tiempo; y dado el tiempo agotado, termina al acabar el turno en curso.
- **CA-CAR-8, CA-TAB-11, 12 y 13** (R-CAR-8, R-TAB-10 a 12). Dada una carrera terminada con E Elegidos: si E es 0, la partida termina con victoria del Warden y la máquina de turnos no avanza más; si no, hay exactamente 6 menos E Olvidados, ningún Nicho Libre, y cada Eco tiene un Olvidado distinto asignado. Misma semilla, misma asignación.
- **CA-TAB-14** (R-TAB-13). Dado cualquier asiento, su hogar es el Altar en carrera, su Nicho si es Elegido, o el Nicho de su Dominio asignado si es Eco tras la carrera.
- **CA-TAB-20** (R-TAB-19). Dada una casilla ocupada, las alcanzables la incluyen y las de más allá también.
- **CA-TAB-21** (R-TAB-20). Dado un Eco tras la carrera, ninguna casilla de un Dominio con dueño está entre sus alcanzables ni se acepta como destino; el Altar y los Olvidados sí. Durante la carrera, y para un Elegido siempre, no hay restricción. La interfaz y el motor usan la misma función (CA-TAB-22).
- **Registro.** Los eventos de vínculo, Despertar, fin de carrera, hogar asignado y victoria del Warden aparecen en el registro con asiento y marca de tiempo.
- **Cualquier tablero de datos.** Dado cada archivo `Content/Data/Boards/*.json`, cuando se carga, entonces pasa `ValidateBoard` e inicia una partida. Sustituye al test "el tablero de 15 es válido"; el de simulación sobre el de 15 se queda. Si falla, el fallo es del archivo: se avisa al autor.
- **Simulación.** Dada la simulación con cuatro dummies de carrera sobre el tablero configurado, la partida entera termina, es determinista por semilla y baja del segundo; `CronicaDeUnaPartida` muestra vínculos y fin de carrera. Un test de informe (como la crónica, activado por variable de entorno) corre 50 semillas e imprime: % de partidas sin ningún vínculo, % de asientos que quedan Eco, y a qué segundo termina la carrera de media. No afirma nada sobre las cifras: son para el autor (objetivo del GDD: nadie vinculado < 5 %, Eco por jugador entre 15 y 35 %).
- Los 49 tests actuales siguen en verde.

Manuales, con PIE y captura en el PR:

- **Vínculo visible.** Los Nichos Libres se distinguen de los Cerrados; al vincularse una ficha, cambia de forma o color y su Nicho toma el color del asiento; el texto de pantalla dice el rol de cada asiento y el tiempo de carrera que queda.
- **Fin de carrera visible.** Al acabar, los Dominios sin dueño se atenúan y el texto anuncia el fin y el hogar de cada Eco; un Eco ya no ve blancas las casillas de Dominios con dueño.
- **Victoria del Warden.** Con `raceDurationSeconds` bajo en `partida.json` (solo para la prueba), la partida termina y el texto lo dice; nada se mueve después.

## Notas para quien lo implemente

- Nombres del glosario: Eco Perdido `LostEcho`, Elegido `Elect`, Despertar `BoardAwakening`, carrera `BondRace`, Hogar `HomeTile`, Nicho `Niche` con `ENicheState`, Dominio Olvidado `Forgotten`. Los nombres nuevos (estado del tablero Primordial/Despierto, rol del asiento, fase de partida, resultado de partida, dueño del Dominio, Dominio asignado, eventos, decisor de carrera, rasgos del jugador para el confinamiento) se proponen en el plan y en el PR.
- Dónde va el vínculo: en `AdvanceTurn`, fase `MoveAndResolve`, donde está el `TODO(R-TUR-2)`: la casilla de destino se resuelve ahí. Solo el Nicho en esta story; Fuentes y Nodos, en las suyas.
- Confinamiento: `ReachableTiles` recibe ya `FMoverTraits`; añádele lo que haga falta y una función que construya los rasgos de un asiento a partir del estado (rol, fase de partida). `SubmitDestinationIntent` valida contra `ReachableOptions`, así que la interfaz y el motor comparten criterio sin más.
- Fin de carrera por tiempo: se comprueba al entrar en `End` o `BetweenTurns`; `TODO(R-TAB-26)` para la interrupción. Con la partida terminada (victoria del Warden), `AdvanceTurn` devuelve false y no cambia nada.
- Datos: `raceDurationSeconds` en `partida.json` con su validación; ninguna constante nueva. El sorteo de hogares usa el generador de la partida.
- Dummy de carrera: elige, entre las opciones, la que deje más cerca (en pasos del grafo) el Nicho Libre más cercano; si es Elegido o la carrera acabó, elige al azar como el dummy actual. Es un doble de prueba: que lo diga su comentario.
- Vista (`ABoardViewActor`, `TickHud`): solo lectura del estado, como en S-005. Nada de reglas en la vista: los alcanzables siguen siendo los del estado.
- Nada de `Engine`, `UWorld` ni `UObject` en `SoulboundCore`. Sin assets.

## Fuera de esta story

- Afinidades y mapa de runa a poder de los Elegidos, Celeridad y los dos dados: story de runas.
- Acto I del Warden, Furia, Vacío, castigos por insolencia (R-CAR-7): stories del Warden y de castigos.
- Runas en Fuentes durante la carrera (R-CAR-7b), minijuego, robo del Eco: runas y minijuego.
- Permiso temporal de invasión para Ecos, Ley de Sombras: Warden.
- Fin de carrera como interrupción del turno (R-TAB-26): Warden.
- Grimorio, cinemática de vínculo, frases del Warden: interfaz y arte.
