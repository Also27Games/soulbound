# Tablero y movimiento

> **Estado en la entrega del TFM (noviembre de 2026)**: Las reglas R-TAB-1 a R-TAB-28 están implementadas sobre el tablero de 86 casillas (S-001, S-002, S-004, S-006, S-008, S-013, S-014, S-016, S-017, S-020). Tres cosas se hicieron de otra manera: una casilla solo se resuelve si el asiento se ha movido, el Nodo de Duelo actúa cuando ha terminado la carrera, y el motor guarda el camino del movimiento (S-020). Valores que cambiaron: los Nichos quedan a entre 3 y 7 pasos del Altar, hay de 13 a 15 casillas por Dominio y seis Nodos, y la mesa mide 120 cm de radio. La sección de presentación es intención: existe el tablero 3D generado desde los datos (S-019, S-029, S-037, S-039 a S-041, S-047); los sonidos, la cámara y la niebla no. Las reglas R-TAB-29 y los criterios CA-TAB-30 a 32 nacieron después, en S-039, y están en su story.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (16/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-16
> **Fase**: 1
> **Pilares que sirve**: 1, 2, 3
> **Fuente**: GDD v0.1 secciones 3.1, 3.3, 3.4.1, 3.4.3, 3.6.1, 5.3, 5.5; decisiones del autor del 15 y 16/09/2026

## Resumen

El tablero es un Altar central del que salen seis Dominios con caminos internos y enlaces entre vecinos. Todos empiezan en el Altar, paran siempre al entrar en él y vuelven a él para ganar. El movimiento se resuelve eligiendo destino entre las casillas iluminadas que la tirada permite alcanzar. Es el sistema sobre el que se apoyan todos los demás.

## Fantasía del jugador

Estás en la plaza del Warden y todo camino sale de ella y vuelve a ella. Cada tirada es una elección corta y visible: adónde puedo llegar, y qué me arriesgo a encontrar allí. El tablero no te castiga: te castiga él, y cuando lo hace, todos lo ven. Los Dominios son suyos hasta que un Elegido los reclama, y los que nadie reclama se pudren.

## Reglas

### Estructura y estados (tanda 1, aprobada el 15/09/2026)

- **R-TAB-1.** El tablero es un grafo de casillas y conexiones. Cada casilla tiene un tipo y una lista de conexiones. Su geometría concreta se carga de un archivo de datos, no está en el código.
- **R-TAB-2.** Existe una única casilla de tipo Altar. Todos los jugadores empiezan en ella.
- **R-TAB-3.** Hay exactamente seis Dominios. Cada Dominio es un conjunto de casillas con al menos una conexión al Altar y al menos una conexión a cada uno de sus dos Dominios vecinos (los seis forman un anillo alrededor del Altar). Los Dominios tienen formas distintas y un número de casillas parecido, definido en datos.
- **R-TAB-4.** Cada Dominio contiene exactamente un Nicho y un número de Fuentes de Poder definido en datos, valor inicial uno, en posiciones fijas.
- **R-TAB-4b.** Cada Fuente de Poder guarda una reserva de runas del tipo de su Dominio, con cantidad inicial definida en datos. Una runa que un jugador pierde vuelve a la reserva de su Fuente. Cómo se obtienen y se pierden lo define el GDD de runas.
- **R-TAB-4c.** El Nicho de cada Dominio es una casilla con una sola conexión, al final de un ramal. Se entra y se sale por la misma casilla.
- **R-TAB-5.** El tablero contiene un número de casillas de tipo Nodo de Poder definido en datos. Al iniciar la partida se asigna a cada Nodo, al azar, uno de tres efectos, Duelo, Oráculo o Planar, respetando las cantidades de cada efecto definidas en datos. Las posiciones de los Nodos son fijas; solo varía el efecto.
- **R-TAB-6.** Al iniciar la partida con N jugadores, el sistema elige al azar 6 menos N Dominios y los marca como Olvidados.
- **R-TAB-7.** En un Dominio Olvidado el Nicho está cerrado, y para obtener runa de su Fuente un Elegido debe superar el minijuego de habilidad en su dificultad más baja. Si falla, no obtiene runa en ese turno. Un Eco Perdido la obtiene sin minijuego (R-ECO-3).
- **R-TAB-8.** El tablero está en estado Primordial desde el inicio hasta el Despertar. En Primordial, los Nodos de Poder y las Fuentes de Poder se comportan como casillas normales: los Nodos no revelan su efecto y nadie obtiene runas.
- **R-TAB-9.** El Despertar ocurre cuando el primer jugador reclama un Nicho. En ese momento los Nodos se revelan con su efecto, todos a la vez, las Fuentes empiezan a dar runas y el tablero pasa a Despierto. No hay transición inversa.
- **R-TAB-10.** Al terminar la carrera por el vínculo, todo Dominio cuyo Nicho no haya sido reclamado pasa a Olvidado, con los efectos de R-TAB-7. Los Dominios Olvidados son, por tanto, siempre 6 menos el número de Elegidos.
- **R-TAB-11.** Si al terminar la carrera nadie ha reclamado un Nicho, la partida termina con victoria del Warden.
- **R-TAB-12.** Al terminar la carrera, cada Eco Perdido recibe un Dominio Olvidado asignado al azar, distinto para cada Eco. Es su hogar durante el resto de la partida.
- **R-TAB-13.** El hogar de un jugador es: el Altar durante la carrera; su Nicho reclamado si es Elegido; el Nicho cerrado de su Dominio asignado si es Eco.

### Movimiento, Sellos y tránsito (tanda 2, aprobada el 15/09/2026; 26 a 28 el 16/09/2026)

- **R-TAB-14.** Un movimiento consiste en elegir una casilla de destino entre las alcanzables y desplazar la ficha hasta ella. Una casilla es alcanzable si existe un camino desde la casilla actual, sin repetir casillas, cuya longitud es exactamente la tirada, y cuyas casillas son todas transitables para ese jugador.
- **R-TAB-15.** Las casillas de paso no tienen efecto. Solo la casilla de destino resuelve su efecto, si lo tiene.
- **R-TAB-16.** El Altar detiene el movimiento: si un camino entra en el Altar, termina ahí aunque la tirada no se haya agotado. No se puede cruzar el Altar dentro de un mismo movimiento. El Altar es alcanzable si hay un camino transitable hasta él de longitud menor o igual a la tirada.
- **R-TAB-17.** Un Sello es un estado de casilla. Una casilla sellada no es transitable ni como paso ni como destino, salvo para un jugador con Paso Libre. Si un camino topa con una casilla sellada, la casilla anterior es alcanzable aunque la tirada no se haya agotado.
- **R-TAB-18.** Si un jugador no tiene ninguna casilla alcanzable, no se mueve y su turno continúa con la resolución de su casilla actual.
- **R-TAB-19.** Las fichas de otros jugadores nunca bloquean: se puede pasar por una casilla ocupada y acabar en ella. Qué ocurre al compartir casilla lo definen los GDD de runas y castigos.
- **R-TAB-20.** Durante la carrera, todos los jugadores pueden transitar por cualquier Dominio. Una vez terminada, un Eco Perdido solo puede transitar por el Altar y por los Dominios Olvidados: las casillas de un Dominio con dueño no son transitables para él, salvo permiso temporal de un decreto del Warden.
- **R-TAB-21.** El sistema mantiene un único criterio de transitabilidad por jugador y casilla, que reúne Sellos, confinamiento de Ecos y permisos temporales. Es el criterio que usa el cálculo de casillas alcanzables y el que la interfaz ilumina.
- **R-TAB-22.** Puede haber varias casillas selladas a la vez, hasta un máximo definido en datos; al sellar por encima del máximo, se libera la más antigua. No pueden sellarse el Altar, los Nichos, las Fuentes de Poder ni una casilla ocupada. No puede sellarse una casilla si con ello alguna casilla quedara sin camino transitable hasta el Altar para un jugador sin Paso Libre.
- **R-TAB-23.** Una casilla sellada solo se libera al caer por exceder el máximo. Los jugadores no mueven ni retiran Sellos. Si el Warden retira Sellos, es un evento suyo definido en su GDD.
- **R-TAB-24.** Cuando un jugador es enviado al Vacío, su ficha sale del tablero y no ocupa ninguna casilla. Al volver, reaparece en su hogar según R-TAB-13. Lo mismo ocurre con cualquier castigo de "vuelta al principio".
- **R-TAB-25.** La Ley de Sombras desplaza inmediatamente a cada Eco Perdido a su hogar (su Nicho), esté donde esté.
- **R-TAB-26.** Toda interrupción del Warden que corta (un golpe, un evento programado o una escena marcada como cortante; R-TUR-11) corta el turno en curso. Si el jugador no había confirmado destino, la tirada se pierde. Si la ficha estaba en movimiento, se detiene en la casilla de su camino en la que se encuentra en ese instante (la última en la que ha entrado del todo) y esa casilla no resuelve efecto. La presentación comunica al motor esa casilla; el motor no conoce la animación. Los cambios del tablero (Sellos, corrupciones, transiciones) se aplican después de la interrupción, nunca durante un movimiento. El fin de la carrera es una interrupción del Warden. El detalle del turno lo define el GDD de turnos.
- **R-TAB-27.** La carrera por el vínculo termina cuando se agota su tiempo o cuando todos los Nichos disponibles han sido reclamados, lo que ocurra antes.
- **R-TAB-28.** Existe un evento propio del Warden, Alto, cuyo único efecto es la parada descrita en R-TAB-26, para usarla a propósito. Cuándo y contra quién, lo decide el GDD del Warden.

## Estados y transiciones

Aprobado el 16/09/2026.

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Tablero | Primordial | Inicio de partida | Se reclama el primer Nicho (R-TAB-9) | Los Nodos y las Fuentes actúan como casillas normales; nadie obtiene runas |
| Tablero | Despierto | Primer Nicho reclamado | Nunca | Los Nodos revelan y aplican su efecto; las Fuentes dan runas |
| Dominio | Con dueño | Inicio, si no sale en el sorteo | Fin de carrera sin Nicho reclamado (R-TAB-10) | Solo transitable por Elegidos tras la carrera |
| Dominio | Olvidado | Sorteo inicial (R-TAB-6) o fin de carrera sin dueño (R-TAB-10) | Nunca en Fase 1 | Nicho cerrado; runa con minijuego (R-TAB-7); territorio de Eco |
| Nicho | Libre | Inicio, en Dominio con dueño | Un Eco acaba en él en la carrera, o su Dominio se corrompe | Reclamable |
| Nicho | Reclamado | Un Eco acaba en él en la carrera | Nunca | Hogar de su Elegido; casilla normal para los demás |
| Nicho | Cerrado | Su Dominio pasa a Olvidado | Nunca | Hogar del Eco asignado; casilla normal para los demás |
| Casilla | Normal | Inicio, o al liberarse un Sello | Evento del Warden que sella (R-TAB-22) | Transitable según R-TAB-21 |
| Casilla | Sellada | Evento del Warden | Cae por exceder el máximo (R-TAB-23) | No transitable salvo Paso Libre |
| Nodo de Poder | Latente / Revelado | Sigue al estado del tablero | Sigue al estado del tablero | No es una máquina propia |
| Ficha | (dato) | | | Posición: una casilla o "ninguna" mientras está en el Vacío. Última casilla conocida solo para cálculos. |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Turnos y dados | Turnos depende de este | Recibe la tirada; devuelve las casillas alcanzables y ejecuta el movimiento. Recibe las interrupciones (R-TAB-26) |
| Carrera y Elegidos | Mutua | Este expone Nichos y su estado y emite "primer Nicho reclamado" y "fin de carrera"; Carrera dice quién es Eco o Elegido y dispara R-TAB-10 a 12 |
| Runas y poderes | Runas depende de este | Este expone Fuentes, reservas y el criterio de tránsito; Runas define obtención, pérdida, Paso Libre, Expulsión, Nodos Planares y qué pasa al compartir casilla |
| Castigos y Vacío | Castigos depende de este | Este expone el hogar de cada jugador y "sacar ficha del tablero" y "devolverla" |
| Warden | Warden depende de este | Este expone: sellar casilla (con validación R-TAB-22), corromper Dominio, mover ficha a hogar, detener ficha (R-TAB-26 y 28), Ley de Sombras; y consultas de distancia y ocupación |
| Interfaz en plano | Interfaz depende de este | Casillas alcanzables por jugador, tipo y estado de cada casilla, reservas de Fuentes |
| Bots | Bots depende de este | Casillas alcanzables y distancias a objetivos |

## Fórmulas y probabilidades

No hay fórmulas propias. Una probabilidad a vigilar:

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| P(nadie se vincula) | probabilidad | objetivo < 5 % | simulación con dummies y bots | Probabilidad de victoria del Warden por R-TAB-11. Depende de la distancia del Altar a los Nichos, del tiempo de carrera y de un dado |

## Casos límite

Aprobado el 16/09/2026.

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| Nadie reclama Nicho al acabar la carrera | Victoria del Warden | R-TAB-11 |
| Todos los Nichos disponibles reclamados antes de tiempo | La carrera termina en ese momento | R-TAB-27 |
| Dos jugadores quieren el mismo Nicho | El primero que acaba en él lo reclama; para los demás pasa a ser casilla normal | R-TAB-9; un Nicho por Elegido |
| El tiempo de la carrera se agota a mitad de un turno | Interrupción del Warden: si no había destino confirmado, la tirada se pierde; si la ficha se movía, se queda en la casilla en la que está; después, las transiciones de fin de carrera | R-TAB-26 |
| El Warden aparece con una ficha a medio camino del Nicho o del Altar | La ficha se queda donde está; esa casilla no actúa; el jugador pierde el resto del movimiento y nada más | R-TAB-26 y 28. Tensión buscada: culpable con cara, pérdida acotada, público |
| Un jugador está en el Vacío cuando acaba la carrera | Recibe Dominio asignado igual; al volver reaparece en su hogar | R-TAB-12 y 13 |
| Ley de Sombras sobre un Eco que está en su Dominio pero no en su Nicho | Se desplaza a su Nicho | R-TAB-25; el hogar es el Nicho |
| Ley de Sombras sobre un Eco que ya está en su Nicho | Sin efecto sobre él | R-TAB-25 |
| Tirada mayor que cualquier camino simple posible | Sin casillas alcanzables: no se mueve y resuelve su casilla actual | R-TAB-18. Un callejón no absorbe la tirada, para que el Nicho exija acierto exacto |
| La casilla sellada es vecina directa del origen (paso 1) | La casilla de origen nunca es alcanzable: un movimiento lleva siempre a otra casilla. Si no queda ninguna, R-TAB-18 | R-TAB-17, 18. Aclarado el 24/09 al implementar la story S-001 |
| El Warden interrumpe mientras un jugador elige destino | La tirada se pierde; las posiciones no cambian; los Sellos u otros cambios se aplican después | R-TAB-26 |
| La ficha se detiene por interrupción en una casilla que no sería destino legal para ese jugador | No puede ocurrir: solo recorre casillas transitables para él; y si es un Nodo, no actúa | R-TAB-14, 21 y 26 |
| Un castigo mueve a un jugador a una casilla sellada | No puede ocurrir: los destinos forzados son hogares y Nichos, que no se sellan | R-TAB-22 |
| Sellar dejaría a alguien sin camino al Altar | El Sello no se coloca; el Warden elige otra casilla | R-TAB-22 |
| Un Eco acaba en la Fuente de un Olvidado | Obtiene la runa sin minijuego (R-ECO-3); un Elegido allí juega el minijuego en fácil (R-TAB-7) | Resuelto el 16/09/2026 |
| Caer en una casilla con dos o más jugadores | Quien llega elige a uno de los que ya están para robar, retar o expulsar; los detalles del robo, del duelo y de la Expulsión, en los GDD de runas y castigos | Decisión del autor, 16/09/2026 |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Casillas por Dominio | pendiente de dataset | | Viajes más largos, Altar menos frecuente | Altar constante, partida más corta |
| Distancia del Altar al Nicho | 2-4 | 2-5 | Más fallos en la carrera, más P(nadie se vincula) | Carrera trivial |
| Fuentes por Dominio | 1 | 1-2 | Caza más fácil, más duplicados | |
| Runas en reserva por Fuente | 4 (GDD 04, 17/09) | 3-5 | Más duplicados | Escasez, más robos |
| Nodos de Poder (total y por efecto) | pendiente | | Más caos, menos legibilidad | Tablero plano |
| Máximo de casillas selladas a la vez | 3 (provisional, 25/09, S-002) | 2-4 | Más caos, recorridos más largos | Los Sellos pierden peso |
| Dificultad del minijuego en Olvidado | fácil (GDD 09) | | Olvidados más peligrosos | |
| Tiempo de la carrera | 5 min | 4-6 | Menos P(nadie se vincula), Acto I más largo | Lo contrario |

## Presentación en pantalla plana

Aprobado el 16/09/2026. Intenciones; el arte se concreta con los assets.

| Evento o información | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Casillas alcanzables al tirar | Tablero | Se iluminan los destinos legales, el resto se atenúa; al pasar el ratón por un destino se marca el camino. Lo ven todos los jugadores | Pulso suave | Nada |
| Nicho, estado | Casilla al final de un ramal | Asset propio: apagado si Libre, con el emblema del Elegido si Reclamado, roto o velado si Cerrado | Al reclamarse, campanada grave | Frase de vínculo al que lo reclama |
| Fuente de Poder | Casilla | Runa flotante del color del Dominio; contador de reserva al pasar el ratón | Tintineo al obtener | Nada, salvo en Olvidado |
| Nodo de Poder, Latente | Casilla | Idéntica a una casilla normal | Nada | Nada |
| Despertar del tablero | Todo el tablero | Los Nodos se transforman a la vez con sus iconos: Duelo, Oráculo, Planar. Cámara que se aleja un instante | Trueno, coro corto | Anuncia el Despertar y nombra al primer Elegido |
| Dominio Olvidado | Todo el Dominio | Se oscurece, niebla, color desaturado; su Nicho pasa a Cerrado | Zumbido grave al corromperse | Al corromper al inicio y al fin de carrera, con frase |
| Sello | Casilla | La casilla se agrieta o se vuelve translúcida; icono de cadena. Al liberarse, se recompone | Golpe seco al sellar; crujido al liberar | Frase al sellar |
| El Altar | Casilla central | Grande, con el trono del Warden vacío; se ilumina cuando un jugador con seis runas llega | Latido lento cuando alguien espera el desafío | Frase al detener a alguien |
| Interrupción a medio camino | Ficha | La animación se congela donde está; foco sobre la ficha; el Warden se manifiesta | Corte de sonido y trueno | La frase del evento, dirigida al jugador |
| Vuelta al hogar | Ficha | La ficha se desvanece y reaparece en su Nicho o en el Altar | Cadenas | Frase del castigo correspondiente |
| Ley de Sombras | Todos los Ecos | Las fichas de los Ecos se arrastran a sus Nichos, a la vez | Viento, susurros | Decreto en voz alta |

Todas las frases del Warden se muestran como texto en pantalla para toda la mesa.

**Decisión del 30/09/2026 (tras las pruebas de arte): lo especial es la estructura, no la losa.** Todas las casillas usan la misma losa. Lo que distingue a una casilla especial es una estructura junto a ella, tocando su borde por el lado contrario al camino, y un filo del mismo color en la losa, para que no haya duda de a cuál pertenece. La ficha se posa en la losa sin tapar la estructura. Concreta la tabla de arriba, no la sustituye:
- **Nicho**: el **trono del Elegido** de ese Dominio, al fondo del ramal. Libre: vacío y apagado. Reclamado: con el emblema del Elegido y luz. Cerrado (Dominio Olvidado): roto o velado. Es la pieza más alta del Dominio.
- **Fuente de Poder**: un pilón junto a la losa con la runa flotando encima, del color del Dominio; sin runa cuando la reserva se agota.
- **Nodo de Poder**: latente, nada (idéntica a una casilla normal, como ya decía la tabla). En el Despertar emerge del suelo junto a la losa la estructura de su efecto: **Planar**, un arco o anillo de piedra con un velo de luz (todos los Planares brillan a la vez cuando alguien puede saltar); **Duelo**, dos armas clavadas y cruzadas; **Oráculo**, un pedestal con un orbe.
- Las estructuras cuentan para la regla de visibilidad (R-CAM-4, CA-CAM-3): ninguna casilla queda oculta del todo desde ningún Trono.
- Reparto de producción: las losas, los senderos y las estructuras los coloca el código desde los datos del tablero (el lado al que mira cada estructura lo calcula el editor de tablero y se puede corregir); el decorado (árboles, rocas, charcos) lo compone el autor a mano en Unreal con las piezas de un kit generado en Blender.

**Decisión del 15/10/2026 (tras la prueba de conjunto en Unreal): el kit pasa a producción.** Low-poly facetado, color por vértice, sin texturas, importado en centímetros reales (1 m de Blender = 100 cm). Cambios respecto a la ronda anterior: el Nicho con su trono va al **extremo exterior** del Dominio, de cara al asiento del jugador (en la ronda anterior estaba junto al Altar y los seis tronos juntos hacían de Altar); Fuente y Nodo hacia dentro; el **Altar es una pieza propia**, más alta que los tronos; seis Dominios con tema y paleta propios; más losas por Dominio según el informe de tamaño. La luz principal y la ambiental las da la Cripta; el tablero lleva luces propias (braseros, velas, antorchas) y emisivos contenidos. "Vida" en el tablero (pájaros, río, entes volando) es deseable, siempre con animación barata (material, vertex animation, instancias) por los 60 FPS en Steam Deck; se decide tras la primera build. Punto de desacuerdo registrado: para el autor, un árbol que tapa parcialmente una losa desde el asiento se salva con rotación, zoom y contorno de las fichas; se comprueba en la prueba de usabilidad (R-CAM-4).

**Decisión del 21/10/2026 (tras estudiar tableros físicos de referencia): el Nicho va por la parte central del Dominio, no al extremo exterior.** Cambia la decisión del 15/10. Motivos: con la variante "el Nicho es el trono" (losa con respaldo bajo mirando al jugador) ya no hay un trono alto que tapar; el hogar en el centro, rodeado de camino, hace que el anillo salga solo, que el Nicho sea el corazón del Dominio y que el tráfico (duelos, robos) pase junto a casa. No tiene que estar justo en el centro en todos los Dominios: la diversidad entre cuñas es parte de la vida del tablero. El trono alto queda para el Warden en el Altar. Reglas del tablero definitivo (editor web, autor): 12-15 losas por Dominio (por tamaño físico: losas de 6,3 cm en una mesa de 148 cm); columna recta desde el Altar sin especiales; al menos un anillo; Nicho en un ramal corto del anillo, por la parte central; Fuente en el anillo; Nodos en la columna o la fila exterior; un acceso al Altar y una o dos conexiones por lado con cada vecino (así el Warden puede sellar la entrada sin dejar casillas sin camino); casillas especiales con una o dos conexiones; distancia mínima entre centros un décimo del radio; 2 Planares, 1-2 Duelo, 1 Oráculo.

## Contribución a los pilares y a las métricas

- **Pilar 1**: el tablero no castiga por sí mismo; corromper, sellar, detener y desterrar son actos del Warden. Los Nodos son voluntarios.
- **Pilar 2**: la iluminación de destinos y las interrupciones son públicas; el Altar como plaza concentra encuentros. Mueve M1 hacia abajo.
- **Pilar 3**: pocos tipos de casilla, un solo criterio de tránsito, estados con icono. Mueve M2 (jugador nuevo explica en una frase).
- **Pilar 4**: no aplica directamente; las paradas y vueltas al hogar se presentan con foco y frase.

## Criterios de aceptación

Formato Dado / Cuando / Entonces. Cada criterio cita la regla que comprueba. Los marcados "(datos)" se comprueban cargando un tablero de prueba.

- [ ] **CA-TAB-1** (R-TAB-1). Dado un archivo de datos de tablero válido, cuando se carga, entonces el grafo tiene las casillas y conexiones descritas y ningún valor de geometría procede del código.
- [ ] **CA-TAB-2** (R-TAB-2). Dado un tablero cargado, cuando empieza una partida, entonces hay exactamente una casilla Altar y todas las fichas están en ella.
- [ ] **CA-TAB-3** (R-TAB-3). Dado un tablero cargado, cuando se valida, entonces hay seis Dominios, cada uno con al menos una conexión al Altar y a cada uno de sus dos vecinos.
- [ ] **CA-TAB-4** (R-TAB-4, 4c). Dado un tablero cargado, cuando se valida, entonces cada Dominio tiene exactamente un Nicho con una sola conexión y el número de Fuentes definido en datos.
- [ ] **CA-TAB-5** (R-TAB-4b). Dada una Fuente con reserva R, cuando un jugador pierde una runa de ese tipo, entonces la reserva pasa a R más uno.
- [ ] **CA-TAB-6** (R-TAB-5). Dado un tablero con K Nodos y cantidades por efecto en datos, cuando empieza la partida, entonces cada Nodo tiene un efecto y las cantidades coinciden con los datos; la misma semilla produce la misma asignación; y sobre muchas semillas (50) aparecen al menos dos asignaciones distintas. (Redacción corregida el 25/09 en S-002: con pocos Nodos, dos semillas concretas pueden coincidir por azar.)
- [ ] **CA-TAB-7** (R-TAB-6). Dada una partida con N jugadores, cuando empieza, entonces exactamente 6 menos N Dominios son Olvidados y sus Nichos están Cerrados.
- [ ] **CA-TAB-8** (R-TAB-7). Dado un Elegido que acaba en la Fuente de un Olvidado, cuando falla el minijuego, entonces no obtiene runa; cuando lo supera, entonces la obtiene.
- [ ] **CA-TAB-9** (R-TAB-8). Dado el tablero en Primordial, cuando un jugador acaba en un Nodo o en una Fuente, entonces no ocurre ningún efecto y nadie obtiene runa.
- [ ] **CA-TAB-10** (R-TAB-9). Dado el tablero en Primordial, cuando un jugador reclama un Nicho, entonces el tablero pasa a Despierto y todos los Nodos quedan Revelados en el mismo instante.
- [ ] **CA-TAB-11** (R-TAB-10). Dada una carrera que termina con E Elegidos, cuando se aplican las transiciones, entonces hay exactamente 6 menos E Dominios Olvidados y ningún Nicho en estado Libre.
- [ ] **CA-TAB-12** (R-TAB-11). Dada una carrera que termina sin ningún Nicho reclamado, cuando se aplican las transiciones, entonces la partida termina con victoria del Warden.
- [ ] **CA-TAB-13** (R-TAB-12). Dada una carrera que termina con ecos E1..Ek, cuando se asignan hogares, entonces cada Eco tiene un Dominio Olvidado distinto.
- [ ] **CA-TAB-14** (R-TAB-13). Dado cualquier jugador y fase, cuando se consulta su hogar, entonces es el Altar en la carrera, su Nicho reclamado si es Elegido, o el Nicho cerrado de su Dominio asignado si es Eco.
- [ ] **CA-TAB-15** (R-TAB-14). Dada una posición y una tirada T, cuando se calculan las alcanzables, entonces toda casilla devuelta tiene un camino simple transitable de longitud exactamente T, y ninguna casilla con tal camino queda fuera.
- [ ] **CA-TAB-16** (R-TAB-15). Dado un movimiento que pasa por un Nodo Revelado y acaba en otra casilla, cuando se ejecuta, entonces el Nodo de paso no produce efecto.
- [ ] **CA-TAB-17** (R-TAB-16). Dada una posición desde la que el Altar está a distancia D menor o igual que T, cuando se calculan las alcanzables, entonces el Altar está entre ellas y ninguna casilla al otro lado del Altar lo está.
- [ ] **CA-TAB-18** (R-TAB-17, R-POD-2). Dado un camino que topa con una casilla sellada en el paso P menor o igual que T (también en el último paso; aclarado el 24/09 en S-001), cuando se calculan las alcanzables para un jugador sin Paso Libre, entonces la casilla P menos uno está entre ellas y la sellada no; para un jugador con Paso Libre, la sellada y las posteriores sí.
- [ ] **CA-TAB-19** (R-TAB-18). Dada una posición sin ninguna casilla alcanzable, cuando el jugador intenta moverse, entonces su posición no cambia y el turno continúa.
- [ ] **CA-TAB-20** (R-TAB-19). Dada una casilla ocupada por otro jugador, cuando se calculan las alcanzables, entonces la ocupada y las que están más allá cuentan como transitables.
- [ ] **CA-TAB-21** (R-TAB-20, R-ECO-2). Dado un Eco tras la carrera, cuando se calculan las alcanzables, entonces ninguna casilla de un Dominio con dueño aparece; dado el mismo Eco con permiso temporal, entonces sí aparecen.
- [ ] **CA-TAB-22** (R-TAB-21). Dado cualquier estado, cuando la interfaz pide casillas alcanzables y el motor ejecuta un movimiento, entonces ambos usan el mismo criterio de transitabilidad (mismo resultado para los mismos datos).
- [ ] **CA-TAB-23** (R-TAB-22). Dado el máximo M de Sellos ya alcanzado, cuando el Warden sella otra casilla, entonces la más antigua se libera y hay exactamente M selladas; dado un intento de sellar Altar, Nicho, Fuente o casilla ocupada, entonces se rechaza; dado un intento que dejaría alguna casilla sin camino al Altar para un jugador sin Paso Libre, entonces se rechaza.
- [ ] **CA-TAB-24** (R-TAB-23). Dada una casilla sellada, cuando cualquier jugador intenta retirarla o moverla, entonces no existe tal acción en el motor.
- [ ] **CA-TAB-25** (R-TAB-24). Dado un jugador enviado al Vacío, cuando se consulta su posición, entonces es "ninguna"; cuando vuelve, entonces está en su hogar.
- [ ] **CA-TAB-26** (R-TAB-25). Dados Ecos en cualquier posición, cuando se aplica la Ley de Sombras, entonces cada uno está en su Nicho y los Elegidos no se han movido.
- [ ] **CA-TAB-27** (R-TAB-26). Dado un jugador con tirada pendiente sin destino confirmado, cuando el Warden interrumpe, entonces la tirada se descarta y la posición no cambia; dado un movimiento en curso con la ficha en la casilla P del camino, cuando el Warden interrumpe indicando P, entonces la posición es P y P no resuelve efecto; dado un Sello o corrupción emitido durante un movimiento, entonces se aplica tras la interrupción.
- [ ] **CA-TAB-28** (R-TAB-27). Dada una carrera en curso, cuando se reclama el último Nicho disponible, entonces la carrera termina antes de agotar el tiempo.
- [ ] **CA-TAB-29** (R-TAB-28). Dado el evento Alto sobre un jugador en movimiento, cuando se aplica, entonces produce el mismo resultado que CA-TAB-27 para una ficha en movimiento.
- [ ] Sin valores fijos en el código: todos los parámetros de la tabla de ajuste salen de datos.
- [ ] Registro: cada transición de estado y cada movimiento emiten un evento con marca de tiempo y jugador afectado (para M1).

## Fuera de alcance de esta fase

- Verticalidad y diorama manipulable (v0.1 3.3.1): Fase 3.
- Mini-puzle ambiental en Olvidados (v0.1 3.3.2): sustituido por el minijuego de habilidad.
- Casillas "Rapiña" y "Oráculo" como tipos fijos (v0.1 3.3.3): Rapiña fuera; Oráculo entra como Nodo.
- Casilla del Vacío: eliminada; al Vacío solo se va por el Warden.
- Un Dominio Olvidado que vuelve a tener dueño; un tablero que vuelve a dormirse: Fase 2.
- Tableros adicionales: Fase 2.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| Caer en una casilla con dos o más jugadores (quién roba o somete a quién) | Autor | GDD de runas y castigos | Resuelta el 16/09/2026: quien llega elige a uno |
| Probabilidad de "nadie se vincula en 5 minutos" | Playtest y simulación | Antes del sprint del Warden | Ajustar si supera el 5 % |
| Dataset del primer tablero (geometría concreta de los seis Dominios) | Autor | Antes del spike | Redibujar la rueda con Dominios de formas distintas |
| Identidad de cada Dominio (tema, nombre) | Autor | Tras el rediseño de los Elegidos | |
