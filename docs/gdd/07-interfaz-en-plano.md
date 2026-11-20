# Interfaz en pantalla plana

> **Estado en la entrega del TFM (noviembre de 2026)**: **Este documento describe la interfaz de la Fase 2, no la de esta entrega.** La cámara de cuerpo sentado, la selección en el mundo, los retratos, la bandeja de dados, el foco de evento, el Umbral, las barcas y la postpartida no existen. La interfaz entregada es la vista de depuración hecha legible: tablero 3D con vista sentada y cenital (S-019, S-022), panel de decisión con botones (S-028), caja de la partida y del objetivo (S-040, S-045), Grimorio con el libro de "Cómo se juega" (S-027, S-044, S-048, S-049), rótulos de los Dominios (S-047) y menús de una plantilla de terceros. De lo que aquí se pide sí se cumple: la pausa no detiene la partida, el Brazal ajeno va oculto (S-035), hay Ayudas (S-021, S-038) y la partida queda escrita en un acta (S-030). Las stories reutilizaron los identificadores R-HUD y CA-HUD con otro significado y añadieron los R-VIS; para la interfaz entregada, la referencia son las stories.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (21/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-21
> **Fase**: 1
> **Pilares que sirve**: 2, 3, 4
> **Fuente**: GDD v0.1 secciones 7.1 a 7.8, 8.1; decisiones del autor del 21/09/2026

## Resumen

Define qué ve y qué toca el jugador en PC: la cámara sentada, el tablero que gira en su vista, la entrada con ratón y mando, el HUD, el componente único de selección, el foco de evento, los relojes diegéticos, y las escenas fuera de la partida (el Umbral, la barca, las puertas, la postpartida). Todo lo diseñado aquí debe poder heredarse en VR (Fase 3): lo que hace la cámara es lo que haría un cuerpo sentado, y lo que se elige está en el mundo.

## Fantasía del jugador

Estás sentado a la mesa de una cripta. Puedes girar la cabeza, inclinarte sobre el tablero, ponerte de pie para ver mejor, y hacer girar el diorama con la mano. Cuando el Warden te habla, sus dilemas flotan delante de ti como cartas de piedra que se apagan mientras dudas. Cuando dos se baten, la sala se oscurece y los ves en el centro, y apuestas.

## Reglas

### Cámara y punto de vista

- **R-CAM-1.** Cada jugador ve la partida desde su Trono, en primera persona, y nunca lo abandona. La cámara solo hace lo que haría un cuerpo sentado: girar la cabeza unos grados a los lados y arriba (ratón o stick derecho, límites en datos), inclinarse hacia delante para acercarse a la mesa (rueda o gatillos), y **ponerse de pie**: una tecla o botón que eleva la cámara sobre el propio asiento para ver el tablero desde arriba, siempre desde su lado.
- **R-CAM-2.** Encuadre automático: al empezar el turno propio, la cámara se inclina hacia la ficha propia y los destinos iluminados; en un foco de evento (R-FOC), hacia los implicados; en el desafío final, el zoom al Altar (GDD 04); en el Vacío, la Cripta se desvanece y la cámara se aleja (GDD 05). Cualquier movimiento del jugador interrumpe el encuadre automático; al cambiar el momento, vuelve.
- **R-CAM-3.** El movimiento libre no es de la cámara sino del tablero: el jugador **gira y acerca el diorama en su propia vista** (botón derecho y arrastrar, o stick derecho con modificador; rueda o gatillos para acercar). La rotación es local a cada cliente y nunca se sincroniza: el estado compartido son casillas y fichas, no ángulos. Una tecla o botón devuelve la orientación del asiento; otra trae la ficha propia delante.
- **R-CAM-4.** El tablero se diseña para verse sentado: Dominios bajos, sin muros ni volúmenes que tapen casillas desde ningún Trono a la altura de la cámara sentada, Altar en el centro, fichas grandes con luz de color. La verticalidad del v0.1 (puentes, atalayas) queda como decoración fuera del área jugable. Es una restricción para el GDD de tablero y para el arte.
- **R-CAM-5.** Cuando una ficha queda tapada por el escenario desde la cámara del jugador, se dibuja su silueta con el color del jugador a través de lo que la tapa. Solo mientras está tapada.

### Entrada

- **R-ENT-1.** Ratón y mando desde Fase 1, con una sola interfaz. Regla de diseño de toda interacción: **elegir entre pocas opciones y confirmar**, nunca un cursor libre obligatorio. Con mando, el stick pasa de una opción a otra y un botón confirma; con ratón, hover y clic. Al abrirse una elección, la primera opción ya está resaltada.
- **R-ENT-2.** Acciones y controles por defecto (en datos, reasignables): confirmar; cancelar o cerrar panel; girar cabeza; inclinarse; ponerse de pie; girar tablero; acercar tablero; volver al asiento; traer mi ficha; abrir Grimorio; abrir crónica; mantener para el Oráculo; acción del minijuego; menú de pausa.
- **R-ENT-3.** Mantener pulsado (Oráculo, R-ORA-3) y la acción del minijuego (GDD de minijuego) funcionan igual con botón y con clic.
- **R-ENT-4.** El teclado solo duplica lo que hace el ratón y da atajos; ningún flujo exige teclado.

### El componente de selección

- **R-SEL-1.** Toda elección con reloj de la partida (destino, señalar rival o dos, elegir opción de dilema, esfera del trilero, tipo de runa regalada, apuesta, voto, elegir qué runa perder, la runa a salvo de Koschei, el rechazo de Tifón al Oráculo, Celeridad, repetir tirada, usar Desvío, salto planar, acción contra un ocupante) pasa por un único componente de selección que recibe: la lista de opciones, dónde está cada una en el mundo, el tiempo, y qué pasa si se agota (GDD 05, R-DIR-4).
- **R-SEL-2.** Las opciones están **en el mundo**: casillas iluminadas; esferas del trilero; fichas o avatares de los otros jugadores para señalar, votar o apostar; las opciones de un dilema como orbes o cartas de piedra que flotan sobre la mesa delante del jugador, visibles para toda la mesa; los seis tipos de runa como runas flotantes; uno y dos dados para Celeridad; el Nodo Planar de destino iluminado. No hay listas 2D para elegir.
- **R-SEL-3.** El tiempo no se muestra con un reloj, y **nunca a costa de la legibilidad de las opciones**: las casillas, esferas, cartas y avatares seleccionables se ven igual de claros del primer al último segundo. El tiempo se ve en algo aparte: en el turno, la luz del retrato del activo en el HUD (R-HUD-1) y el aura de su propia ficha, que se consumen; en un dilema, el borde de las cartas de piedra, que se va quemando; en el Oráculo, el pulso del orbe. Y el Warden mete prisa a mitad (comentario).
- **R-SEL-4.** Una elección abierta bloquea toda otra acción salvo mirar y girar el tablero. Las elecciones de espectador (apuesta, voto) no bloquean al activo ni son bloqueadas por él (R-TUR-13).
- **R-SEL-5.** Con ratón, hover resalta y clic confirma; con mando, el stick recorre las opciones en orden espacial (la más cercana en la dirección del stick) y el botón confirma. Casillas: el orden espacial evita recorrer el tablero entero; con seis o menos opciones el recorrido es directo.
- **R-SEL-6.** Los bots usan el mismo componente con su interfaz de decisión y sin interfaz visual (GDD de bots).

### HUD y paneles

- **R-HUD-1.** Siempre en pantalla, en 2D y sobrio, tres cosas: arriba, **una sola fila de retratos en orden de turno** que sirve para todo: el activo resaltado con su reloj de turno como luz que se consume, y en cada retrato el número de runas, los estados visibles (Marcado, en el Vacío, Brazal revelado con sus tipos) y el Elegido o el Dominio de Eco; al pasar el cursor o seleccionar un retrato se despliega su detalle (R-HUD-3). Abajo, el Brazal propio: runas por tipo con contador de duplicados, poderes activos encendidos, y los estados propios (turnos perdidos y extra, Prueba pendiente, premio de apuesta). Abajo al centro, la franja de subtítulo del Warden con el nombre del destinatario. No hay lateral.
- **R-HUD-2.** No se muestra el acto. No se muestra el tiempo de partida en el HUD por defecto: lo da el reloj diegético (R-DIE-1). Un contador numérico pequeño es opcional en opciones, apagado por defecto.
- **R-HUD-3.** Retratos: hover ilumina la ficha y la casilla de ese jugador; clic o confirmar gira el tablero para traer su ficha delante (R-CAM-3). La cámara no se mueve.
- **R-HUD-4.** Paneles bajo demanda, con tecla o botón, que se abren sin pausar nada: el **Grimorio** (afinidad y mapa de runa a poder del Elegido propio o reglas de Eco, mapas de todos los Elegidos, resumen de reglas clave, todo en datos) y la **crónica** (los últimos sucesos de la partida, número en datos, a partir del registro de eventos: quién obtuvo o perdió qué, duelos, castigos, votos). Abrirlos no bloquea el reloj de turno.
- **R-HUD-5.** Sin minimapa y sin marcadores flotantes en 3D salvo el foco de evento y las siluetas. El tablero es el mapa.
- **R-HUD-6.** Toda información del HUD sale del estado replicado; el HUD no calcula reglas. Lo que un jugador no debe saber (tipos de runa ocultos de otros, insolencia) no llega a su cliente.

### Relojes y objetos diegéticos

- **R-DIE-1.** El reloj de partida es un objeto grande colgado del techo de la Cripta sobre el centro de la mesa (reloj de arena de almas, orrery de cadenas o velas que se consumen; lo decide el arte), legible desde los cuatro Tronos y desde la vista de pie, que marca el tiempo restante de forma aproximada, con el tramo de la carrera (los primeros cinco minutos) señalado en él para que no haga falta otro reloj, y se vuelve más evidente al final (la arena se agota a la vista, una campanada al entrar en el último minuto y otra a los diez segundos). El Warden no cuenta: dice una frase al entrar en el último minuto y otra a los diez segundos (GDD 06, R-WAR-4d).
- **R-DIE-2.** El Oráculo no está siempre a la vista: cuando alguien debe consultarlo, el orbe **emerge de la mesa delante de ese jugador**, pulsa durante sus siete segundos, revela la sentencia y se hunde. Toda la mesa ve delante de quién ha salido. En PC se consulta manteniendo pulsado hasta llenar una barra de un segundo dibujada en el propio orbe (R-ORA-3); en VR estará al alcance de la mano sin levantarse, que es el motivo de que salga delante y no en el centro.
- **R-DIE-3.** Los dados se tiran con un gesto corto (confirmar) y ruedan en una **bandeja propia en el borde de la mesa, delante de cada Trono**, nunca sobre el diorama. El resultado lo predetermina el motor; la animación nunca decide. Los demás ven la bandeja de quien tira.
- **R-DIE-4.** La máscara del Warden cambia por acto (GDD 06). No hay ningún otro indicador de acto.

### Foco de evento

- **R-FOC-1.** En duelo, trilero, desafío final y Furia, la Cripta entra en foco: las antorchas se apagan, los implicados quedan iluminados por haces cenitales, y la cámara de todos los jugadores se inclina hacia ellos sin salir del Trono (R-CAM-2). El tablero se atenúa pero no desaparece.
- **R-FOC-2.** Los espectadores ven el minijuego de los participantes **en grande, en el centro de la escena**, sin los controles de los participantes, para poder seguirlo y apostar. La apuesta (R-DUE-7) se abre en su componente de selección sobre los avatares de los dos, durante los primeros segundos.
- **R-FOC-3.** Un golpe del Warden no entra en foco completo: el Warden se materializa, el activo recibe su elección delante y la mesa lo ve; es rápido. Un comentario no cambia nada en la escena.
- **R-FOC-4.** Al resolverse, animación de resolución (el perdedor se tambalea, el ganador brilla; la runa vuela de un Brazal a otro), las luces vuelven de golpe y la partida sigue.
- **R-FOC-5.** El Vacío tiene su presentación propia (GDD 05): para el desterrado la Cripta se desvanece; para la mesa su avatar se vuelve espectral.

### Fuera de la partida

- **R-FLU-1. El Umbral.** Menú principal como escena 3D estática: un muelle en un mar de niebla púrpura, una mesa de cartógrafo con objetos que son las opciones (hover resalta y muestra su función): una campana para crear partida, una brújula rota para unirse con código, un engranaje roto para opciones, un tintero seco para salir. Objeto opcional: una mesa de práctica para jugar el minijuego contra la IA sin partida. No hay objeto de "jugar solo": la campana sirve para todo (R-FLU-2), y crear e iniciar sin nadie rellena con bots.
- **R-FLU-2. Las barcas.** Sala de espera como río de niebla por el que navegan **cuatro barcas funerarias, una por asiento**, cada una con su Barquero remando; cada jugador ve el río desde su propia barca y ve las de los demás cerca. Al unirse alguien, su barca aparece con su Eco a bordo; las barcas de los asientos vacíos no existen todavía. Cada uno marca "listo"; el anfitrión inicia cuando quiere. Entrar no exige conexión: el nivel se carga como servidor local. Al entrar, si hay internet, la sala se publica sola: un **farol** en la barca del anfitrión, visible desde todas, se enciende y muestra el código de la sala; sin internet, el farol queda apagado y el Barquero lo dice, y se puede iniciar igual con bots. Una sala publicada solo la encuentra quien tiene el código, no hay lista pública, así que abrirla no cuesta nada a quien juega solo. El farol es además un interruptor, con texto flotante al pasar por encima, para el caso raro de cerrar la sala a propósito o volver a publicarla si ha vuelto la conexión.
- **R-FLU-3. Las puertas.** Al pulsar "iniciar partida": un momento de gracia en datos (valor inicial 5 s) durante el que aún puede entrar alguien con el código; después, por cada asiento libre aparece una barca con un bot a bordo (GDD 08, R-CAI-6); la Cripta **emerge de la niebla** delante de las barcas, en el mismo nivel, y las cuatro barcas llegan a su muelle; cuatro puertas de piedra idénticas; cada jugador humano elige una con el componente de selección dentro de un reloj en datos (dos no pueden elegir la misma; quien no elige a tiempo recibe una al azar); **los bots eligen solo cuando todos los humanos han elegido**, entre las que quedan; cada puerta tiene asignado en secreto un asiento; las puertas se abren, fundido, y cambio de nivel a la Cripta con cada uno ya sentado en su Trono. Las puertas cerrándose son el inicio de partida (R-PRO-1).
- **R-FLU-4. Postpartida.** Sin cambiar de nivel: la cinemática de victoria o derrota (GDD 04 y 06), después una vista de resultados (ganador, Elegido o Eco de cada uno, runas finales, duelos ganados, veces en el Vacío) con la crónica completa, y "revancha", que devuelve a todos a la barca con la misma sala. Las métricas M1 a M4 y el registro completo se exportan a archivo, no se muestran.
- **R-FLU-5. Pausa.** El menú de pausa en partida no detiene nada (R-WAR-4c): solo da acceso a opciones y a abandonar. Abandonar deja un bot en el asiento (GDD de multijugador).
- **R-FLU-6. Opciones.** Volumen general y del Warden; subtítulos (siempre activos para el Warden, tamaño en opciones); contador numérico de partida; sensibilidad de cabeza y de tablero; invertir ejes; asignación de controles; idioma de texto (español en Fase 1).

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Cámara | Sentado, Inclinado, De pie, Encuadre automático | Entrada del jugador o momento de partida | Entrada o cambio de momento | R-CAM-1 y 2 |
| Tablero (local) | Orientación del asiento, Girado | Entrada del jugador | Tecla de volver | R-CAM-3 |
| Selección | Cerrada, Abierta (resaltada la primera), Confirmada, Agotada | El motor pide una elección | Confirmación o tiempo | R-SEL |
| Cripta | Normal, Foco, Vacío (local del desterrado) | Evento | Fin del evento | R-FOC |
| Aplicación | Umbral, Barca, Puertas, Cripta, Postpartida | Flujo | Flujo | R-FLU |
| Panel | Grimorio abierto, Crónica abierta | Tecla | Tecla o cierre | No pausa |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Tablero | Interfaz depende de este | Casillas alcanzables, siluetas, restricción de diseño para verse sentado |
| Turnos | Interfaz depende de este | Fase, activo, reloj de turno, orden |
| Runas y poderes | Interfaz depende de este | Brazal, poderes activos, trilero, apuestas, aspirante, desafío |
| Castigos | Interfaz depende de este | Estados visibles, elecciones de efecto, Vacío |
| Warden | Interfaz depende de este | Subtítulos, orbe, dilemas como objetos, votaciones, máscara, foco |
| Minijuego | Interfaz depende de este | Presentación en grande para espectadores; acción con botón o clic |
| Multijugador y bots | Mutua | Barca, listo, rellenar con bots, abandono, replicación del estado que ve el HUD |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| Tiempo medio para confirmar destino | segundos | objetivo < 10 | playtest | Si sube, revisar iluminación de destinos y recorrido con mando |
| Elecciones agotadas por partida y jugador | entero | objetivo < 1 | playtest | Si sube, los relojes son cortos o las opciones poco visibles |
| Casillas tapadas desde algún Trono | entero | 0 | validación de datos del tablero | R-CAM-4 |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| El jugador gira el tablero durante su encuadre automático | El encuadre se interrumpe; la cámara se queda como estaba y el tablero gira | R-CAM-2 y 3 |
| Golpe del Warden mientras el jugador está de pie | La cámara vuelve a sentado con el foco del golpe; al terminar, se queda sentado | R-CAM-2 |
| Elección abierta y el jugador abre el Grimorio | El Grimorio se abre encima; la elección y su tiempo siguen | R-HUD-4, R-SEL-4 |
| Dos elecciones a la vez para el mismo jugador (activo y apuesta) | No ocurre: el activo no apuesta en su propio duelo, y una escena no llega en mitad de un turno | R-TUR-13, R-WAR-4 |
| Con mando, ocho casillas alcanzables en direcciones parecidas | El recorrido es por cercanía en la dirección del stick; la primera resaltada es la más cercana al frente | R-SEL-5 |
| Dilema de cuatro opciones | Cuatro cartas de piedra en abanico delante del jugador; la mesa las lee | R-SEL-2 |
| Jugador en el Vacío con votación abierta | Vota desde su vista del Vacío sobre los avatares lejanos | GDD 05, R-SEL-2 |
| Ficha tapada por la ficha de otro jugador | No es escenario: no se dibuja silueta; las fichas se separan en la casilla | R-CAM-5 |
| Contador numérico activado por un jugador | Solo en su cliente | R-HUD-2 |
| Anfitrión abandona en las barcas | GDD de multijugador | |
| Alguien entra durante el momento de gracia | Entra; su barca aparece y ocupa el asiento antes que un bot | R-FLU-3 |
| Se pierde la conexión con la sala publicada y jugadores dentro | GDD de multijugador; el farol se apaga y el Barquero lo dice | R-FLU-2 |
| Alguien intenta entrar con el código mientras el farol está apagado | No entra; el que tiene el código ve "la barca no está abierta" | R-FLU-2 |
| Dos jugadores eligen la misma puerta a la vez | El motor ordena; el segundo ve su elección rechazada y elige otra | R-FLU-3 |
| Un humano no elige puerta a tiempo | Recibe una al azar; después eligen los bots | R-FLU-3 |
| Revancha con un jugador que se ha ido | Su barca no aparece; el asiento queda libre | R-FLU-4 |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Grados de giro de cabeza | 60 a cada lado, 30 arriba | 45-90 | Se ve más Cripta | Sensación de rigidez |
| Altura de "de pie" | 0,6 m sobre sentado | 0,4-0,8 | Vista más cenital | Poca ganancia |
| Velocidad de giro del tablero | datos | | | |
| Número de sucesos en la crónica | 10 | 6-20 | | |
| Reloj para elegir puerta | 15 s | 10-20 | | |
| Momento de gracia al iniciar | 5 s | 3-10 | Espera más larga | Nadie llega a tiempo |
| Tamaño de subtítulo | medio | | | |

## Presentación

Este documento es la presentación. Referencias de estilo: paleta desaturada, luz dura, magia en cian y púrpura (dirección de arte). Iconos de runas y poderes, retratos de los seis Elegidos y del Eco, y la máscara del Warden en tres estados, como lista de arte para el TFM.

## Contribución a los pilares y a las métricas

- **Pilar 2**: foco de evento con minijuego en grande para todos; apuestas y votos sobre los avatares; la crónica para quien vuelve.
- **Pilar 3**: una sola forma de elegir; el tiempo en la luz; nombre, causa y víctima de cada efecto en pantalla (GDD 05); sin acto ni contadores que distraigan.
- **Pilar 4**: subtítulo con destinatario; el golpe delante de ti; el Vacío a tu alrededor.
- Métricas: tiempo de confirmación y elecciones agotadas como señales de interfaz.

## Criterios de aceptación

Los marcados con (manual) se comprueban en una lista de verificación con build; el resto son automatizables.

- [ ] **CA-CAM-1** (R-CAM-1) (manual). Dado un jugador sentado, cuando mueve el ratón o el stick, entonces la cámara gira dentro de los límites de datos y nunca cambia de posición; cuando pulsa "de pie", entonces la cámara sube sobre su asiento y baja al soltar o al pulsar de nuevo.
- [ ] **CA-CAM-2** (R-CAM-3). Dado un cliente que gira el tablero, entonces ningún otro cliente cambia su vista y el estado replicado no cambia.
- [ ] **CA-CAM-3** (R-CAM-4). Dado un tablero en datos, cuando se valida, entonces desde la vista sentada por defecto de cada uno de los cuatro Tronos toda casilla es al menos parcialmente visible: nada del escenario ni del propio tablero, Altar incluido, la oculta por completo.
- [ ] **CA-CAM-4** (R-CAM-5) (manual). Dada una ficha que queda tapada en la vista actual (que puede no ser la de por defecto: inclinado, tablero girado, ángulo bajo tras el Altar o tras la decoración del borde), entonces se ve su silueta con su color; no tapada, no. CA-CAM-3 garantiza las casillas en la vista por defecto; esta silueta es la red para las fichas, más altas, en cualquier vista.
- [ ] **CA-ENT-1** (R-ENT-1) (manual). Dada cualquier elección de la partida, entonces se completa solo con mando y solo con ratón, sin teclado.
- [ ] **CA-SEL-1** (R-SEL-1). Dado el componente de selección con N opciones y un tiempo T, cuando se abre, entonces la primera está resaltada; cuando se confirma una, entonces devuelve su índice; cuando pasa T sin confirmar, entonces devuelve agotado y el motor aplica R-DIR-4.
- [ ] **CA-SEL-2** (R-SEL-2) (manual). Dado un dilema, entonces sus opciones aparecen como objetos sobre la mesa visibles para todos los jugadores; dado un voto, entonces se elige sobre los avatares.
- [ ] **CA-SEL-3** (R-SEL-3) (manual). Dada una elección abierta, entonces las opciones se ven con la misma claridad al principio y al final del tiempo, el tiempo se ve en un elemento aparte (luz del retrato, aura de la ficha, borde de las cartas, pulso del orbe) y no hay ningún reloj numérico ni circular.
- [ ] **CA-SEL-4** (R-SEL-5). Dadas opciones en posiciones conocidas y una dirección de stick, cuando se mueve, entonces el resaltado pasa a la opción más cercana en esa dirección.
- [ ] **CA-HUD-1** (R-HUD-1, 6). Dado el estado replicado, entonces el HUD muestra el número de runas de cada jugador y no los tipos de los demás, salvo Brazal revelado; y el cliente nunca recibe los tipos ocultos ni la insolencia.
- [ ] **CA-HUD-2** (R-HUD-3) (manual). Dado un retrato, cuando se hace hover, entonces su ficha y casilla se iluminan; cuando se confirma, entonces el tablero gira para traerla delante y la cámara no se mueve.
- [ ] **CA-HUD-3** (R-HUD-4). Dado el registro de eventos, cuando se abre la crónica, entonces muestra los últimos N sucesos en orden y el reloj de turno sigue corriendo.
- [ ] **CA-DIE-1** (R-DIE-1) (manual). Dado el reloj colgado, entonces es legible desde los cuatro Tronos y desde la vista de pie.
- [ ] **CA-FOC-1** (R-FOC-1, 2) (manual). Dado un duelo, entonces la Cripta se oscurece, los dos quedan iluminados, los espectadores ven el minijuego en grande sin controles y pueden apostar en sus primeros segundos.
- [ ] **CA-FLU-1** (R-FLU-1 a 3, R-CAI-6) (manual). Dado el Umbral, cuando se crea partida e inicia sin nadie, entonces tras el momento de gracia aparecen tres barcas con bots, la Cripta emerge, las barcas llegan, cada uno elige puerta, y se llega a la Cripta con cuatro asientos ocupados; sin conexión de red, el flujo funciona igual con el farol apagado; con conexión, el farol está encendido y muestra un código con el que otro cliente puede entrar.
- [ ] **CA-FLU-2** (R-FLU-4). Dada una partida terminada, entonces existe el archivo exportado con el registro completo y las métricas M1 a M4, y "revancha" devuelve a la barca con la misma sala.
- [ ] **CA-SEL-5** (R-SEL-4). Dada una elección abierta para el activo, cuando intenta cualquier otra acción de partida, entonces se rechaza; cuando gira el tablero o mueve la cabeza, entonces se permite; dada una apuesta abierta para un espectador, entonces no bloquea ni es bloqueada por la elección del activo.
- [ ] Sin valores fijos en el código: límites de cámara, controles, número de sucesos, tamaños y sensibilidades salen de datos.

## Nombres nuevos para el glosario

Componente de selección (`SelectionPrompt`), De pie (`StandUp`), Giro del tablero (`BoardSpin`, local), Silueta (`OcclusionOutline`), Crónica (`Chronicle`), Reloj de la Cripta (`CryptClock`), Bandeja de dados (`DiceTray`), El Umbral (`Threshold`), Las barcas (`FerryLobby`, una por asiento), El Barquero (`Ferryman`), El farol (`FerryLantern`, sala publicada), Las puertas (`DoorChoice`), Postpartida (`Aftermatch`). Corrección: Brazal Arcano (panel 2D en PC), Grimorio (panel bajo demanda con contenido en datos), Foco de evento (versión PC).

## Fuera de alcance de esta fase

- Biblioteca de reglas como sala (v0.1 7.5): después del TFM. La mesa de práctica del minijuego, opcional en el Umbral.
- Mando en VR, gestos, hápticos: Fase 3.
- Avatares de cuerpo completo y animación de gestos en la mesa: Fase 3. En Fase 1, avatares sentados sencillos con la miniatura del Elegido en el retrato.
- Voz espacial y chat de voz: Fase 2 (GDD de multijugador).
- Idiomas distintos del español: Fase 2.
- Accesibilidad más allá de subtítulos, tamaño y contador: Fase 2.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| Forma del reloj de la Cripta (arena, orrery, velas) | Arte | Antes del sprint de interfaz | |
| Recorrido con mando entre casillas en tableros con ramales | Prototipo | Spike de interfaz | Si el orden por cercanía confunde, orden fijo por Dominio |
| Mesa de práctica del minijuego en el Umbral | Autor | Tras el GDD de minijuego | Opcional |
| Cómo se ve el propio avatar sentado (manos, Brazal en el brazo) | Arte | Sprint de interfaz | En Fase 1, nada del propio cuerpo |
| Ayudas al jugador nuevo (decisión del autor, 22/10/2026, GDD 06): ajuste "ayudas" con automático / siempre / nunca; la línea de "qué se espera de ti" y las instrucciones del Warden siguen el ajuste; lo visto se guarda por jugador en su máquina, nunca por partida | Autor | Story del menú | Decidido |
