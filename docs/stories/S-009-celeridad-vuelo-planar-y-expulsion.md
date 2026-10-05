# S-009. Celeridad, Vuelo Planar y Expulsión

**Sistema**: runas y poderes (GDD 04), turnos y dados (GDD 02). **Módulos**: `SoulboundCore` y `Soulbound` (vista). **Rama base**: `main`. **Tamaño**: media-grande.

## Contexto

S-008 calcula qué poderes tiene activos cada asiento, pero solo Paso Libre hace algo. Esta story da efecto a tres más y a una afinidad, y con ellos introduce las **elecciones del jugador más allá del destino**: cuántos dados tirar, repetir la tirada, saltar entre Nodos Planares y a quién expulsar. El motor ya tiene el patrón (fase de espera + intención) para el destino y el minijuego; aquí se generaliza a una "elección pendiente" con opciones y reloj, que es la mitad del motor del componente de selección del GDD 07.

Decidido por el autor el 04/10/2026: (1) Desvío pasa a la story del Vacío; entra la repetición de tirada de Apep; (2) las elecciones tras mover tienen su propio reloj, `choiceClockSeconds` (10 s), y agotarlo equivale a "no", sin castigo; (3) un solo salto planar por movimiento.

## Reglas a implementar

- **R-POD-1, R-TUR-4, R-TUR-5.** Quien tiene Celeridad activo elige, antes de tirar, uno o dos dados; el resultado es la suma. Sin Celeridad, un dado y sin elección. Vale igual para el Eco con Runa de Liberación (R-ECO-7).
- **R-TUR-8.** La elección de dados cuenta dentro del reloj de turno: si se agota antes de confirmar destino, el turno termina sin mover y sube la insolencia, también si se agota durante la elección de dados.
- **R-TUR-6, R-ELE-2 (Apep).** Tras ver la tirada y antes de confirmar destino, un Elegido con la afinidad de repetir puede repetir la tirada una vez por turno; la nueva sustituye a la anterior, con el mismo número de dados, y las alcanzables se recalculan. El reloj de turno sigue corriendo.
- **R-POD-5, R-TAB-15.** Con el tablero Despierto, quien termina su movimiento en un Nodo Planar y tiene Vuelo Planar puede saltar a cualquier otro Nodo Planar. Opcional. Sin el poder, o pasando por el Nodo sin acabar en él, es una casilla normal. Un salto por movimiento.
- **R-INT-1.** Al terminar un movimiento (o un salto) se resuelve primero el efecto de la casilla (Nicho, Fuente, Nodo Planar) y después, como mucho, una acción contra un jugador.
- **R-INT-2 (parte).** Acción contra jugadores: en esta story, solo Expulsión. Opcional; si hay varios ocupantes, se elige uno.
- **R-POD-4.** Expulsión: quien tiene el poder y termina su movimiento (o su salto) en una casilla ocupada puede mandar a su hogar (R-TAB-13) a uno de los que están en ella. Sin minijuego ni defensa. Quien ya está en su hogar no es objetivo válido.
- **R-INT-4.** Ninguna acción entre jugadores cambia turnos extra ni perdidos.
- **Registro**: dados elegidos, repetición, salto planar, expulsión y elección agotada emiten eventos con marca de tiempo y asientos.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-TUR-4 y CA-TUR-5** (R-TUR-4, 5, R-POD-1). Dado un asiento con Celeridad, cuando empieza su turno, la máquina espera su elección de dados y tira uno o dos según elija; sin Celeridad no hay espera y tira uno. Dado un asiento que pierde su última runa de Celeridad, en su siguiente turno vuelve a un dado sin elección.
- **CA-ECO-6 (resto)** (R-ECO-7). Dado un Eco tras la carrera con su Runa de Liberación, elige uno o dos dados igual que un Elegido.
- **Reloj en la elección de dados** (R-TUR-8). Dado un asiento con Celeridad que no elige dados, cuando se agota el reloj de turno, el turno termina sin tirar ni mover, con agotamiento registrado e insolencia subida.
- **CA-ELE-4** (R-TUR-6). Dado Apep con tirada vista y sin destino confirmado, cuando repite, la tirada nueva sustituye a la anterior, las alcanzables son las de la nueva y un segundo intento de repetir ese turno se rechaza; en el turno siguiente puede otra vez. Otro Elegido no puede repetir. Repetir tras confirmar destino se rechaza.
- **CA-POD-3 y CA-TAB-16** (R-POD-5, R-TAB-15). Dado un asiento con Vuelo Planar que termina en un Nodo Planar con el tablero Despierto, se le ofrecen los demás Nodos Planares y "no saltar"; si salta, está en el destino y no se le ofrece otro salto. Sin el poder, en Primordial, o pasando por el Nodo sin acabar en él, no se ofrece nada.
- **CA-POD-2** (R-POD-4). Dado un asiento con Expulsión que termina en una casilla con otros, cuando expulsa a un Elegido, este está en su Nicho; a un Eco tras la carrera, en el Nicho Cerrado de su Dominio asignado; a un Eco en carrera, en el Altar. Un ocupante que ya está en su hogar no aparece entre las opciones. Sin el poder, no hay elección.
- **CA-POD-1 (parte)** (R-INT-2). Dado un asiento con Expulsión en una casilla con dos ocupantes, puede expulsar a uno solo o a ninguno.
- **CA-INT-1** (R-INT-1). Dado un asiento con Expulsión que termina en una Fuente ocupada, primero obtiene la runa (o juega su minijuego) y después se le ofrece la acción. Tras un salto planar a un Nodo ocupado, se le ofrece la acción allí.
- **CA-INT-3** (R-INT-4). Tras cualquier expulsión, los turnos extra y perdidos del expulsado no cambian, y su turno le llega en su orden.
- **Reloj de elección.** Dada una elección de salto o de expulsión, cuando pasan `choiceClockSeconds` sin respuesta, se resuelve como "no", se registra, y el turno sigue; la insolencia no cambia.
- **Registro.** Los cinco eventos nuevos, con asiento, dato y marca de tiempo.
- **Simulación.** Los dummies eligen al azar entre las opciones de cada elección (incluida "no"), con su generador. Determinista por semilla y bajo el segundo. El informe añade por partida: % de turnos con dos dados, saltos planares y expulsiones.
- Los 86 tests actuales siguen en verde.

Manuales, en PIE con paso a paso, con captura:

- **Elegir dados.** Con Celeridad, la vista pide "1 o 2 dados" y el reloj corre; la tirada con dos dados se ve en la crónica.
- **Nodos revelados.** Tras el Despertar, la etiqueta de cada Nodo enseña su efecto (Planar, Duelo, Oráculo); antes, no.
- **Salto y expulsión.** Con los poderes, la vista pide la elección, se puede contestar con tecla o clic, y la ficha salta o el expulsado vuelve a su hogar deslizándose.

## Notas para quien lo implemente

- Nombres del glosario: Celeridad `PowerHaste` (`EPower::Haste`), Vuelo Planar `PowerPlanarFlight`, Expulsión `PowerExpel`, afinidad `EAffinity::RerollOnce`, Hogar `HomeTile`, Intención `Intent`, Componente de selección `SelectionPrompt`. Nombres nuevos (la elección pendiente, su tipo, la intención, los eventos, el parámetro del reloj) se proponen en el plan y en el PR.
- **Una sola forma de elegir.** Las elecciones nuevas usan un mecanismo común en el estado: tipo de elección, lista de opciones, plazo; una intención genérica que elige una opción (o ninguna); y `IDecider` gana una función genérica para contestarla. La máquina espera como espera el destino y el minijuego. Propón en el plan si la elección de dados y la repetición entran en ese mismo mecanismo (preferible) o por qué no. El destino y el minijuego de S-008 no se tocan en esta story.
- Dónde va cada cosa: elección de dados en `PreRoll` (bajo el reloj de turno); repetición durante `RollAndChoose`, como una intención más junto a la de destino; salto y acción contra jugador tras resolver la casilla en `MoveAndResolve`, en ese orden (R-INT-1). El orden completo al llegar: Nicho o Fuente (con su minijuego si toca), salto planar, acción contra jugador.
- La Expulsión mueve al objetivo con `HomeTile`; no es un movimiento suyo: no resuelve Nicho ni Fuente ni dispara nada, y no cuenta como "se ha movido".
- Efecto de los Nodos: ya se sortea en `StartMatch` (`EPowerNodeEffect`). Aquí solo se usa `Planar`; Duelo es de S-010 y Oráculo, del Warden.
- `choiceClockSeconds` en `partida.json` con su validación; ninguna constante nueva.
- Vista: una línea de petición ("ELIGE: [1] un dado [2] dos dados · quedan 12 s") para cualquier elección pendiente del humano, contestable con teclas numéricas, y con clic cuando la opción es una casilla. Tecla para repetir tirada (Apep). `BindKey`, sin assets. Etiqueta del Nodo con su efecto tras el Despertar. Solo lectura del estado, como siempre. El humano no tiene decisor: sus elecciones llegan por el controlador al anfitrión de partida, como el clic de destino.
- Nada de `Engine` en `SoulboundCore`.

## Fuera de esta story

- Desvío: story del Vacío.
- Duelo portátil, Nodo de Duelo, trilero, apuestas, robo del Eco: S-010.
- Oráculo y afinidad de Tifón: Warden.
- Golpes del Warden que cortan una elección (R-TUR-11): Warden.
- Componente de selección real, con opciones en el mundo (GDD 07): interfaz.
