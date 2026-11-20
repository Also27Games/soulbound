# El Warden

> **Estado en la entrega del TFM (noviembre de 2026)**: Están implementados el cerebro sin mundo y su anfitrión, los actos, los canales, la garantía, los sacos, los disparadores, la insolencia y la Furia, el Oráculo, las tres votaciones, los eventos programados y las frases como subtítulo (S-016, S-017, S-018, S-021, S-023). Los 38 eventos de la tabla están en los datos, uno por uno, y los valores de `warden.json` coinciden con este documento. Se hicieron de otra manera: en la Furia todos juegan la misma escalera; el fin de la partida lo corta todo; el Oráculo se consulta con un clic o una tecla; hay quince momentos de instrucción con Ayudas, que empiezan en "siempre" (S-021, S-027, S-038), y una señal de corte con un golpe de sonido (S-041, S-050). Pasan a la Fase 2: la voz sintética, la máscara por actos, las cinemáticas, la réplica a clientes y el Muro de los Perdidos.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado (21/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 2026-09-18
> **Fase**: 1
> **Pilares que sirve**: 1 (es el pilar), 2, 3, 4
> **Fuente**: GDD v0.1 sección 4 y 7.8; mecánicas del género (solo mecánicas, ningún texto); decisiones del autor del 17 y 18/09/2026

## Resumen

Define el cerebro del Warden: cuándo interviene, por qué canal, a quién se dirige, qué evento elige y cómo lo entrega. Define los disparadores contextuales, la insolencia y la Furia, el Oráculo, las votaciones, los eventos programados de inicio y fin, la voz, y el contenido inicial: 38 eventos y 12 tipos de comentario. Las piezas con las que se construyen los eventos (efectos, direcciones, composición) son del GDD de castigos; este documento decide cómo y cuándo se usan.

## Fantasía del jugador

Tienes treinta segundos para tu turno y en cualquier momento puede sonar su voz. A veces solo se ríe. A veces te para en seco y te pone un dilema: dos turnos para ti, o mandas a casa a tu amigo. Todos le miran a él, pero él te mira a ti. Y si le ignoras, lo apunta.

## Reglas

### El cerebro: cuándo y cómo interviene

- **R-WAR-1.** El Warden se divide en dos piezas. El **cerebro** (`WardenBrain`) es una clase de C++ corriente, sin `UWorld`, sin actores y sin temporizadores: recibe el estado de partida, el reloj de partida y un generador de azar con semilla, y devuelve una decisión (nada, comentario, escena o golpe, con su evento, dirección y entrega). El **anfitrión** es lo que Unreal espera: un `UWorldSubsystem` que existe solo en el servidor, es dueño del intervalo de evaluación, llama al cerebro en cada evaluación, aplica la decisión sobre la partida y la replica a los clientes. El anfitrión no toma decisiones y el cerebro no toca el mundo. Motivo: el cerebro se prueba sin abrir el editor y se simula miles de partidas en segundos, cosa imposible si la decisión vive dentro del ciclo de tick de un mundo; y el subsystem sigue siendo el sitio correcto de Unreal para todo lo demás.
- **R-WAR-2.** La partida tiene tres actos definidos en datos: Acto I de 0 a 5 minutos (la carrera, R-CAR-7), Acto II de 5 a 27, Acto III de 27 a 40. El fin de la carrera (R-TAB-27) adelanta el Acto II si llega antes de los 5 minutos.
- **R-WAR-3.** El Warden evalúa cada intervalo definido en datos, valor inicial 15 segundos con variación aleatoria de más o menos 5. En cada evaluación decide intervenir con una probabilidad por acto, valores iniciales 20 %, 35 % y 50 %. Si interviene, elige canal con pesos por acto.
- **R-WAR-4.** Hay tres canales:
  - **Golpe**: un evento con efecto dirigido al jugador activo, que es siempre el protagonista de la escena y quien elige en los dilemas. Llega en cualquier instante del turno y lo corta (R-TUR-11, R-TAB-26). Es la presión.
  - **Escena**: un evento sin protagonista (Liberación, Noche de los Ecos, Ley de Sombras, duelo entre dos rivales, votación, marca al azar). Por defecto entra entre turno y turno y no corta nada. Cada evento lleva en datos si corta o no: los golpes cortan por defecto, las escenas no, y una escena concreta puede marcarse como cortante si debe ser inmediata.
  - **Comentario**: solo voz y subtítulo, sin efecto y sin cortar. El Warden no se materializa: se le oye. Es lo que llena los huecos.
- **R-WAR-4b.** Los eventos programados (R-PRO) no pasan por la evaluación ni por los sacos: saltan por su condición (tiempo, fin de carrera, aspirante, Furia, victoria) y **cortan siempre** el turno en curso con los efectos de R-TAB-26. Esperar a que el activo termine sería regalarle tiempo: el fin de la carrera y el fin de la partida llegan cuando llegan.
- **R-WAR-4c.** El reloj de partida no se detiene nunca: ni durante un golpe, una escena, un comentario o un evento programado, ni durante duelos, trileros, votaciones, minijuegos o cinemáticas. La partida dura 40 minutos de reloj, pase lo que pase. El reloj de turno del activo tampoco se detiene por un comentario; un golpe lo corta; una escena entra cuando no hay turno.
- **R-WAR-4d.** Para que el reloj nunca corte al Warden a media frase, el cerebro no inicia ninguna intervención ni comentario en los últimos segundos de un acto ni de la partida: un margen en datos igual a la duración máxima de una frase, valor inicial 12 s. En el último minuto el Warden no cuenta: dice una frase al entrar en el último minuto y otra a los diez segundos (el reloj de la Cripta hace el resto, GDD 07), y en el minuto 40 exacto entra el evento programado de fin. Lo que ya ha empezado (un duelo, un trilero, un desafío final) se termina y solo después se aplica el fin: si el aspirante supera el desafío con el reloj a cero, gana.
- **R-WAR-5.** Garantía del pilar 1: si pasan 120 segundos (datos) sin golpe ni escena, la siguiente evaluación interviene con probabilidad 1 y con un golpe o una escena, nunca un comentario.
- **R-WAR-6.** Objetivo de cortes: la probabilidad de golpe por acto se calibra en simulación para que el porcentaje de turnos cortados por acto quede en los objetivos de datos, valores iniciales 15 %, 25 % y 35 % (CA-TUR-12).
- **R-WAR-7.** El Warden no interrumpe duelos, trileros, minijuegos, votaciones, desafíos finales ni cinemáticas. Si la evaluación cae dentro de uno, la decisión se aplaza a su final.
- **R-WAR-8.** Selección de un golpe o escena, en este orden: (1) se leen los disparadores activos (R-DIS), que aportan multiplicadores de peso y direcciones preferidas; (2) se forma la lista de candidatos con los eventos de los sacos disponibles (R-SAC-1) cuyo canal coincide, que están en su ventana de tiempo, que conservan usos y que tienen objetivos válidos (R-DIR-2 y 3); (3) para un dilema, cada opción sin objetivos válidos se elimina, y si quedan menos de dos el evento se descarta; (4) se sortea por peso: peso del saco por acto, por multiplicadores de disparadores; (5) se entrega por Warden directo o por Oráculo según el evento (R-ORA-1).
- **R-WAR-9.** Un comentario se elige por tipo (R-COM-1) según lo que acaba de pasar y los disparadores, con al menos tres variantes de frase por tipo y sin repetir la última variante usada.
- **R-WAR-10.** Toda decisión del Warden se registra con marca de tiempo, acto, canal, evento, dirección, objetivos, disparadores activos y semilla, para reproducir cualquier partida en pruebas.

### Disparadores contextuales

- **R-DIS-1.** Un disparador es una condición sobre el estado de partida, con umbrales en datos, que mientras está activa altera los pesos de eventos y direcciones, y puede alterar la probabilidad de intervenir. Varios pueden estar activos a la vez; sus multiplicadores se multiplican.

| Disparador | Código | Condición (datos) | Efecto sobre el cerebro |
|---|---|---|---|
| Líder claro | `ClearLeader` | Un jugador tiene 2 o más tipos de runa distintos que el segundo | Sube el peso de los eventos con dirección líder y de los dilemas que permiten al activo perjudicar al líder; si el líder es el activo, suben los golpes contra él; comentarios sobre el líder |
| Victoria inminente | `ImminentVictory` | Un jugador tiene 5 tipos, o está en el Altar con 6 | Probabilidad de intervenir por 1,5; casi todo el peso a golpes y escenas contra ese jugador: Destierro del líder, El perseguido, Cosecha del líder, Primer muro en su camino al Altar; aviso a la mesa |
| Jugador ignorado | `IgnoredPlayer` | Un jugador lleva más de N minutos (inicial 4) sin ser objetivo de ningún efecto ni protagonista de un golpe | Sube su peso como dirección "al azar" y "rival señalado" sugerido, y como blanco de comentarios |
| Partida estancada | `StalledMatch` | Sin runas obtenidas ni duelos en los últimos N minutos (inicial 4) | Suben Noche de los Ecos, Piedad, Amanecer, Hermanos de sangre, Favor y Trueque |
| Acaparador | `Hoarder` | Un jugador tiene 3 o más runas del mismo tipo | Burla; suben Impuesto, El precio y Milagro con él como víctima preferida |

- **R-DIS-2.** Los umbrales, multiplicadores y la lista de eventos afectados por cada disparador son datos. Añadir un disparador es añadir una condición y una tabla de multiplicadores.
- **R-DIS-3.** Sin disparadores activos, la dirección "rival señalado" la elige el activo libremente, y las direcciones "al azar" reparten con peso a quien lleva más tiempo sin ser objetivo.

### Insolencia y Furia

- **R-INS-1.** Cada jugador tiene un contador de insolencia de 0 a 5, oculto. Sube uno cada vez que no hace algo que le tocaba hacer: agotar el reloj de turno (R-TUR-8), no atender al Oráculo en su tiempo (R-ORA-3), no señalar, no elegir en un dilema o desaprovechar un regalo (R-DIR-4), no votar (R-VOT-4). Los bots nunca son insolentes.
- **R-INS-2.** Cada subida trae un castigo inmediato al insolente, sorteado por gravedad según su nivel con una tabla en datos. Valores iniciales de probabilidad leve / media / grave: niveles 0 y 1, 70 / 25 / 5; niveles 2 y 3, 30 / 50 / 20; nivel 4, 5 / 35 / 60. Dentro de la gravedad se sortea un efecto de los aplicables al propio insolente: leves, Revelar Brazal o Consulta forzada (Alto solo si tiene un turno en curso); medios, Vuelta al hogar, Turno perdido o Prueba del Warden; graves, Perder una runa o el Vacío. Nunca un regalo ni un efecto que necesite a otro jugador. Durante el Acto I no hay Vacío (R-CAR-7): un resultado grave se aplica como Vuelta al hogar. El Warden nombra la causa: "por ignorarme".
- **R-INS-3.** Al llegar a 5, en lugar del sorteo, la **Furia del Warden**: cinemática sin azar y castigo fijo, el Vacío de la Furia. Es el Vacío normal (R-VAC) con una diferencia: dentro no funcionan ni Desvío ni Liberación. El jugador puede intentarlos; el botón está, y el Warden se niega con frase propia ("esta vez no; me hiciste esperar a mí, ahora te haré esperar a ti"). La runa de Desvío no se consume. En una Liberación, el Warden libera a todos "menos a ti". Mictlantecuhtli conserva su afinidad. Tras la Furia el contador vuelve a 0. La Furia es la única excepción a "nadie va al Vacío durante la carrera" (R-CAR-7): cinco insolencias en cinco minutos se las ha ganado.
- **R-INS-4.** El contador no baja solo en Fase 1. Si en playtest resulta demasiado duro, se añade en datos una bajada de un punto por acto.
- **R-INS-5.** El Warden deja pistas del nivel sin mostrar el número, solo con comentarios distintos por nivel ("es la tercera vez que me ignoras"). La máscara del Warden es un indicador de la partida (acto), nunca del estado de un jugador.

### El Oráculo

- **R-ORA-1.** El Oráculo es una forma de entrega de eventos, no un evento. Cada evento lleva en datos por dónde puede llegar: Warden directo, Oráculo, o cualquiera. Los dilemas y las votaciones llegan siempre por Warden directo; el Oráculo entrega decretos, que se muestran como una sentencia en el orbe.
- **R-ORA-2.** Se consulta por el Nodo del Oráculo (R-TAB, glosario), por Consulta forzada (R-EFE-3) o cuando un golpe elige entrega por Oráculo. La consulta corta el turno del activo si está en curso (R-TUR-11).
- **R-ORA-3.** El jugador tiene un tiempo en datos, valor inicial 7 segundos, para consultar: en PC, mantener pulsado el orbe hasta llenar una barra de un segundo, que en VR será cogerlo y agitarlo. Si no lo hace a tiempo, la sentencia se revela igual y se aplica, y cuenta como insolencia (R-INS-1).
- **R-ORA-4.** El Oráculo no es azar puro. Su sorteo mezcla pesos uniformes y los pesos del cerebro con disparadores según un sesgo en datos entre 0 y 1, valor inicial 0,5: peso final igual a (1 menos sesgo) por uniforme más sesgo por peso del Warden. Con 0 es destino ciego; con 1 es el Warden con máscara. La mesa lo ve como destino; el Warden no da motivos por el Oráculo.
- **R-ORA-5.** La afinidad de Tifón (R-ELE-2) se aplica a toda sentencia del Oráculo, venga por donde venga: rechaza dentro de su tiempo y la segunda sentencia es obligatoria.

### Votaciones

- **R-VOT-1.** Una votación es una escena en la que todos los jugadores, incluido el activo, señalan a la vez a otro jugador dentro de un reloj en datos, valor inicial 8 segundos. Nadie puede votarse a sí mismo. Los jugadores en el Vacío votan también.
- **R-VOT-2.** El más votado recibe el efecto de la votación. En empate, el Warden desempata al azar entre los empatados y lo dice.
- **R-VOT-3.** Al resolverse, los votos son públicos: la mesa ve quién votó a quién.
- **R-VOT-4.** No votar cuenta como insolencia y ese voto se emite al azar entre los válidos.
- **R-VOT-5.** Los bots votan según su interfaz de decisión (GDD de bots): en castigos, al líder; en favores, a quien menos les amenace.
- **R-VOT-6.** En el TFM hay tres votaciones: El chivo (Acto III, el Vacío), Juicio (Acto II, Marcado) y Favor (Acto II, Runa regalada). Cualquier efecto con dirección de un solo jugador puede convertirse en votación en datos.

### Sacos y eventos

- **R-SAC-1.** Cada evento pertenece a un saco por acto. Los sacos son acumulativos: en el Acto II están disponibles los sacos I y II, en el III los tres. Cada evento tiene un peso por acto (en datos), un canal, una entrega, unos usos máximos por partida y una ventana opcional de tiempo dentro de la partida.
- **R-SAC-2.** Contenido inicial del TFM: los 38 eventos de la tabla. Se pueden añadir, quitar y reponderar en datos sin tocar código. Regla de recorte: si falta tiempo se recortan eventos del saco, nunca el motor ni las votaciones.

| Acto | Canal | Nombre | Composición (GDD 05) | Entrega | Ventana / notas |
|---|---|---|---|---|---|
| I | Golpe | Tropiezo | Alto | Warden | |
| I | Golpe | Pausa | Turno perdido, activo, 1 | Cualquiera | Solo hasta el minuto 2,5 |
| I | Golpe | Al Altar | Vuelta al hogar, activo | Cualquiera | Solo hasta el minuto 2,5 |
| I | Golpe | La Prueba | Prueba del Warden, activo | Warden | Enseña el minijuego |
| I | Regalo | Prisa | Turno extra, activo, 1 | Cualquiera | |
| I | Escena | Primer muro | Sello, el Warden elige | Warden | Nunca en el único camino a un Nicho libre |
| II | Golpe | Elige tu veneno | Dilema: turno perdido, activo, 1 / perder una runa, activo, a elección | Warden | |
| II | Golpe | Generosidad envenenada | Dilema: turno extra, activo, 2 / vuelta al hogar, rival señalado | Warden | |
| II | Golpe | Enséñame | Revelar Brazal, rival señalado | Cualquiera | Con Líder claro, dirección líder |
| II | Golpe | Ojos en ti | Revelar Brazal, activo | Cualquiera | |
| II | Golpe | El orbe llama | Consulta forzada, activo | Warden | |
| II | Golpe | Voluntad ajena | Consulta forzada, rival señalado | Warden | |
| II | Golpe | Arquitecto | Sello contra rival señalado | Warden | |
| II | Golpe | Hazlo tú | Dilema: Sello contra rival señalado / revelar Brazal, rival señalado | Warden | |
| II | Golpe | Duelo por orden | Duelo forzado, activo contra rival señalado, una runa | Warden | |
| II | Golpe | El precio | Dilema: robo decretado, activo a rival señalado / runa regalada, activo | Warden | Con Acaparador, víctima preferida |
| II | Golpe | Tres caminos | Dilema: turno perdido, activo, 1 / perder una runa, activo, azar / Prueba del Warden, activo | Warden | |
| II | Golpe | Ojo por ojo | Dilema: perder una runa, activo, azar / Prueba del Warden, rival señalado | Warden | |
| II | Golpe | Impuesto | Perder una runa, activo, azar | Cualquiera | Con Acaparador, más peso |
| II | Golpe | Cadena | Vuelta al hogar, siguiente | Cualquiera | |
| II | Golpe | Destierro | El Vacío, activo | Cualquiera | Peso bajo; usos máximos 2 |
| II | Escena | Ley de Sombras | Ley de Sombras | Warden | Termina la Noche |
| II | Escena | Noche de los Ecos | Noche de los Ecos | Warden | Solo si hay Ecos |
| II | Escena | Piedad | Liberación | Warden | Solo con alguien en el Vacío |
| II | Escena | Amanecer | Liberación y después Ley de Sombras | Warden | |
| II | Escena | Marcha atrás | Vuelta al hogar, todos | Warden | Usos máximos 1 |
| II | Escena | Hermanos de sangre | Duelo forzado, dos al azar, una runa | Warden | |
| II | Escena | Juicio | Votación: Marcado al más votado | Warden | |
| II | Escena | Favor | Votación: Runa regalada al más votado | Warden | |
| III | Escena | El perseguido | Marcado, líder | Warden | |
| III | Escena | Marca ciega | Marcado, al azar | Warden | |
| III | Golpe | Todo o nada | Duelo forzado, dos rivales señalados, todas las runas | Warden | Solo últimos 8 minutos |
| III | Escena | Cosecha | Perder una runa, todos, azar | Warden | Usos máximos 1 |
| III | Golpe | Cosecha del líder | Perder una runa, líder, azar | Cualquiera | |
| III | Golpe | Milagro | Robo decretado, activo a todos | Cualquiera | Usos máximos 1 |
| III | Golpe | Destierro del líder | El Vacío, líder | Cualquiera | Con Victoria inminente, peso alto |
| III | Golpe | Trueque | Dilema de 4: turno extra, activo, 2 / robo decretado, activo a rival señalado / runa regalada, activo / Sello contra rival señalado | Warden | |
| III | Escena | El chivo | Votación: el Vacío al más votado | Warden | Solo últimos 8 minutos; usos máximos 1 |

- **R-SAC-3.** Los golpes que llegan por Oráculo se muestran como sentencia; los mismos golpes por Warden directo llevan la frase con el motivo si hay disparador ("por líder", "por acaparar", "por ignorarme").

### Comentarios

- **R-COM-1.** Un comentario es una frase con voz, sin efecto, elegida por tipo. Tipos iniciales: burla al último; aviso de cinco runas; prisa al que tarda; acaparador; mendigo (retar a quien no tiene runas); pardillo (retador sin runas que pierde); lento (trilero sin elegir); desagradecido (regalo ignorado); apuesta fallida; presentación de cada Elegido al vincularse; risa al cortar un turno; "esta vez no" de la Furia. Cada tipo tiene al menos tres variantes en datos.
- **R-COM-2.** Algunos comentarios no los elige la evaluación sino el suceso: mendigo, pardillo, lento, desagradecido, apuesta fallida, presentación y "esta vez no" se disparan por el evento que los causa, sin cortar nada.

### Eventos programados

- **R-PRO-0.** Todo evento programado corta el turno en curso (R-WAR-4b), salvo el de inicio, que ocurre antes del primer turno.
- **R-PRO-1.** Inicio: el Warden se presenta, corrompe los 6 menos N Dominios con una frase por cada uno, sortea el orden de turno y lo anuncia con una frase por posición ("el primero en sufrir...", con el foco sobre cada jugador y su nombre en el subtítulo; R-TUR-1), y abre la carrera.
- **R-PRO-2.** Despertar: comentario de presentación del primer Elegido; cada vínculo posterior, su presentación.
- **R-PRO-3.** Fin de la carrera: corta el turno en curso (R-PRO-0), anuncia el fin, corrompe los Dominios sin dueño, asigna a cada Eco su Olvidado y se burla (R-CAR-8). El reloj de partida sigue corriendo durante toda la escena (R-WAR-4c); el Acto II empieza en el minuto 5 y sus pesos se aplican desde la siguiente evaluación, que por R-WAR-4d no llega hasta que el Warden termina de hablar. Si nadie se ha vinculado, cinemática de victoria del Warden.
- **R-PRO-4.** Aspirante: cuando alguien queda en el Altar con seis runas, aviso a la mesa (R-VIC-1). Desafío final: el Warden se enfrenta al aspirante y, si falla, se lo cobra con nombre (R-VIC-3).
- **R-PRO-5.** Furia: cinemática (R-INS-3). Victoria de un jugador: cinemática con el Warden furioso. Tiempo agotado: cinemática de victoria del Warden con la burla más larga del juego (R-VIC-4).

### Voz y personalidad

- **R-VOZ-1.** El Warden habla en Fase 1 con voz sintética pregenerada a partir de las frases en datos, con subtítulo siempre. Las frases se generan una vez y se guardan como audio en el proyecto; ninguna dependencia en línea en partida.
- **R-VOZ-2.** Todas las frases son originales, escritas desde cero. Ninguna procede de material ajeno ni de sus traducciones.
- **R-VOZ-3.** Personalidad: teatral, impaciente, sarcástico, de falsas amabilidades; trata a los jugadores como juguetes. Siempre con humor negro, nunca con insultos fuertes. Nunca se mete con rasgos reales de la persona, solo con lo que hace en la partida.
- **R-VOZ-3b.** En Fase 1 el Warden **no pronuncia nombres**: ni reales ni motes ni huecos rellenados. Las frases se escriben en segunda persona ("tú", "pequeño", "el que tiembla"; ciertas muletillas quedan prohibidas, decisión del autor del 22/10/2026) y son audios completos, sin montaje. Quién es el destinatario lo dicen la escena y la interfaz: el foco de evento sobre el jugador, y el subtítulo, que sí lleva su nombre real ("Warden, a Lluis: ..."). Las únicas frases con nombre propio son las seis presentaciones de los Elegidos, que son fijas. Nombres por voz (mote o nombre real con síntesis en tiempo real) quedan para la Fase 2, con la voz definitiva.
- **R-VOZ-4.** Presentación por actos: máscara intacta en el Acto I, agrietándose en el II, rota con el vacío detrás en el III. Los comentarios no materializan al Warden; los golpes y escenas sí.

## Estados y transiciones

| Entidad | Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|---|
| Warden | Acto I, II, III | Tiempo o fin de carrera | Tiempo | Probabilidades, pesos y máscara del acto |
| Warden | Evaluando | Cada intervalo | Decisión tomada | R-WAR-3 |
| Warden | Aplazado | Evaluación durante duelo, trilero, minijuego, votación, desafío o cinemática | Fin de aquello | Decide al terminar |
| Warden | En escena | Golpe o escena en curso | Evento resuelto | No evalúa |
| Jugador | Insolencia 0-5 | Suma por R-INS-1 | Furia | Tabla de gravedad por nivel |
| Jugador | Vacío de la Furia | Furia | Escape o cuarto turno | Sin Desvío ni Liberación |
| Oráculo | Latente, Llamando (7 s), Revelando, Resuelto | R-ORA-3 | | |
| Votación | Abierta, Cerrada, Revelada | R-VOT-1 a 3 | | |
| Evento | Usos restantes | Datos | Cero | Fuera de candidatos |

## Interacción con otros sistemas

| Sistema | Dirección | Naturaleza |
|---|---|---|
| Castigos | Warden depende de este | Efectos, direcciones, composición; el Warden solo combina |
| Turnos y dados | Mutua | Cadencia, cortes, garantía; reloj de turno como fuente de insolencia; acciones de espectadores |
| Tablero | Warden depende de este | Sello del Warden en el camino del aspirante; ventanas; estado para disparadores |
| Runas y poderes | Warden depende de este | Líder, aspirante, acaparador; duelos forzados; apuestas |
| Carrera | Mutua | Acto I; eventos programados de inicio y fin de carrera |
| Minijuego | Warden depende de este | Prueba del Warden; escape de la Furia |
| Interfaz | Interfaz depende de este | Subtítulos, orbe con barra, pantalla de voto, máscara por acto, foco de evento |
| Bots | Bots depende de este | Elegir en dilemas, señalar, votar, atender el Oráculo |
| Multijugador | Mutua | El cerebro corre solo en el servidor; los clientes reciben decisiones y audio por id |

## Fórmulas y probabilidades

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|
| Intervenciones con efecto por partida | entero | objetivo 25-45 | simulación | Golpes más escenas |
| Turnos cortados por acto | porcentaje | 15 / 25 / 35 | simulación | R-WAR-6, CA-TUR-12 |
| Tramos de 2 minutos sin golpe ni escena | entero | 0 | simulación | R-WAR-5 |
| Furias por partida | media | objetivo menor que 0,5 | simulación con bots que a veces ignoran | Si sube, revisar fuentes de insolencia |
| Reparto de objetivos | desviación | a medir | simulación | Nadie debería recibir más del doble de golpes que otro sin disparador que lo explique |
| P(evento) | fórmula | | regla | peso del acto por producto de multiplicadores de disparadores activos, normalizado entre candidatos válidos |
| P(Oráculo) | fórmula | | regla | (1 menos sesgo) por uniforme más sesgo por P(evento) |

## Casos límite

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|
| Evaluación en mitad de un duelo | Se aplaza al final del duelo | R-WAR-7 |
| Golpe elegido y el activo es un bot | Igual; el bot decide en su reloj | R-WAR-4, R-TUR-14 |
| Golpe elegido y no hay turno activo (entre turnos) | Se trata como escena si el evento lo permite; si no, se descarta y se elige otro | R-WAR-4 |
| Dilema con una sola opción válida | Se descarta el evento | R-WAR-8 |
| Ningún candidato válido en el canal | Se prueba el otro canal con efecto; si tampoco, comentario, y la garantía sigue contando | R-WAR-5 y 8 |
| Garantía vencida durante una cinemática | Interviene al terminar | R-WAR-5 y 7 |
| Insolencia en el Vacío (por ejemplo, no votar) | Sube el contador; el castigo inmediato se aplaza a su salida, salvo que sea Furia, que se aplica como Vacío de la Furia desde ese momento | R-INS-2 y 3, R-DIR-2 |
| Furia sobre alguien ya en el Vacío | Su Vacío pasa a ser de la Furia y la cuenta de turnos se reinicia | R-INS-3 |
| Sentencia del Oráculo sin objetivos válidos | El orbe muestra "nada" y el Warden lo comenta; no cuenta como uso | R-DIR-3 |
| Tifón rechaza y la segunda sentencia no tiene objetivos válidos | Se aplica la regla anterior; no hay tercera | R-ORA-5 |
| Votación con un jugador y tres bots | Válida; los bots votan. Siempre hay cuatro votantes: la partida se rellena con bots hasta cuatro | R-VOT-5 |
| Victoria inminente de dos jugadores a la vez | Ambos objetivos; el Warden reparte | R-DIS-1 |
| Jugador ignorado que está en el Vacío | No cuenta como ignorado mientras esté dentro | R-DIS-1 |
| Primer muro sin casilla válida | Se descarta | R-TAB-22 |
| Dos escenas seguidas entre los mismos turnos | No: como mucho una intervención con efecto por hueco entre turnos | Legibilidad |
| Comentario justo cuando llega un golpe | El golpe tiene prioridad; el comentario se descarta | Pilar 3 |
| Cambio de acto en mitad de un turno | Los pesos cambian en la siguiente evaluación; la máscara cambia al final del turno | Presentación |
| El Warden aún habla cuando llega el minuto 5 o el 40 | No ocurre: no se inician frases en los últimos 12 s de un acto ni de la partida; el fin de carrera y el fin de partida tienen sus propias frases | R-WAR-4d |
| Duelo, trilero o desafío final en curso en el minuto 40 | Se termina y después se aplica el fin; si el aspirante supera el desafío, gana | R-WAR-4d |
| Fin de la carrera o fin de partida en mitad de un turno | Corta el turno donde esté la ficha; no se espera al activo | R-WAR-4b |
| Escena marcada como cortante en datos | Corta como un golpe pero sin protagonista | R-WAR-4 |

## Parámetros de ajuste

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|
| Intervalo de evaluación | 15 s más o menos 5 | 10-20 | Warden más lento | Más carga y más presión |
| Probabilidad de intervenir por acto | 20 / 35 / 50 % | | Más Warden | Menos Warden |
| Pesos de canal por acto (golpe / escena / comentario) | 40/20/40, 45/25/30, 50/25/25 | | | |
| Garantía | 120 s | 90-150 | Huecos más largos | Warden metrónomo |
| Objetivo de cortes por acto | 15 / 25 / 35 % | | Más frustración | Menos presión |
| Multiplicadores de disparadores | 2 a 4 | 1,5-6 | Warden obsesivo | Disparadores invisibles |
| Umbral Líder claro | 2 tipos | 1-3 | | |
| Umbral Jugador ignorado | 4 min | 3-6 | | |
| Umbral Partida estancada | 4 min | 3-6 | | |
| Umbral Acaparador | 3 | 3-4 | | |
| Tabla de insolencia | ver R-INS-2 | | | |
| Tiempo del Oráculo | 7 s | 5-10 | | |
| Sesgo del Oráculo | 0,5 | 0-1 | Oráculo con intención | Oráculo ciego |
| Reloj de votación | 8 s | 6-12 | | |
| Usos máximos por evento | por evento | 1-4 | Repetición | Saco corto |
| Variantes por comentario | 3 | 3-6 | Más grabación | Repetición |

## Presentación en pantalla plana

| Evento o información | Dónde | Visual | Sonido | Warden |
|---|---|---|---|---|
| Comentario | Nada en escena | Subtítulo con su nombre; ligero pulso de luz | Su voz, su risa | No se materializa |
| Golpe | Foco de evento sobre el activo | El Warden se materializa; nombre del evento; efecto, dirección y causa en pantalla | Trueno de aparición | Frase con nombre y motivo |
| Escena | Tablero entero | El Warden sobre el Altar; efecto en todos los afectados | Trueno largo | Decreto |
| Dilema | Cartas de piedra sobre la mesa delante del activo, visibles para la mesa (GDD 07) | Su borde se quema con el tiempo | Tic-tac | Presenta y mete prisa |
| Votación | Sobre los avatares, para todos (GDD 07) | Avatares votables; la luz se apaga con el tiempo; revelación de votos con líneas de quién a quién | Redoble | Presenta, desempata, comenta |
| Oráculo | Orbe que emerge de la mesa delante de quien consulta (R-DIE-2) | Pulso creciente 7 s; barra al mantener; sentencia en el orbe; se hunde al terminar | Zumbido y estallido | Ordena consultar; no da motivos |
| Insolencia | Nada visible | Sin número ni indicador; solo la voz | Chasquido | Comentario por nivel |
| Furia | Cinemática corta para todos | El Warden rompe la máscara; foco sobre el insolente; ficha al Vacío | Rugido | Frase fija de Furia |
| Máscara por acto | Warden | Intacta, agrietada, rota | | |
| Inicio | Cinemática | Presentación, corrupción de Dominios, sorteo de orden | | Frase por Dominio y por jugador |
| Fin de carrera | Cinemática corta | Corrupción de los sin dueño; líneas a los Ecos | | Burla y asignación |
| Victoria y derrota | Cinemática | Ver GDD de runas | | |

## Contribución a los pilares y a las métricas

- **Pilar 1**: garantía de 2 minutos; 25 a 45 intervenciones con efecto por partida; el Warden abre, cierra y narra.
- **Pilar 2**: escenas para todos, votaciones, dilemas que afectan a otros, comentarios sobre quien no juega.
- **Pilar 3**: canales distintos con presentación distinta; motivo en pantalla en los golpes; sentencia en el Oráculo.
- **Pilar 4**: todo golpe lleva nombre; comentarios personales; insolencia con pistas.
- Métricas: M1, M3 (comentarios y burlas), tramos de 2 minutos vacíos (debe ser 0), cortes por acto, Furias por partida.

## Criterios de aceptación

- [ ] **CA-WAR-1** (R-WAR-1). Dado un estado de partida, un reloj y una semilla, cuando el cerebro decide dos veces, entonces la decisión es idéntica; y el test corre sin mundo cargado, con el cerebro instanciado directamente, sin subsystem.
- [ ] **CA-WAR-1b** (R-WAR-4c, 4d). Dada una intervención cualquiera, entonces el reloj de partida no se ha detenido. Dada una evaluación en el segundo 290 del Acto I, entonces no se inicia ninguna intervención ni comentario. Dado un desafío final iniciado en el segundo 2395, entonces se resuelve completo antes de aplicar el fin de partida.
- [ ] **CA-WAR-2** (R-WAR-3, 5). Dada una simulación de N partidas, cuando se mide, entonces ningún tramo de 120 s carece de golpe o escena, y el intervalo entre evaluaciones está dentro de 15 más o menos 5 s.
- [ ] **CA-WAR-3** (R-WAR-6). Dada la configuración inicial, cuando se simulan N partidas, entonces el porcentaje de turnos cortados por acto está dentro de más o menos 5 puntos del objetivo de datos.
- [ ] **CA-WAR-3b** (R-WAR-4, 4b, R-PRO-0). Dado un turno en curso, cuando se agota el tiempo de la carrera o de la partida, entonces el turno se corta en ese instante con los efectos de R-TAB-26 y el evento programado se ejecuta sin esperar. Dada una escena marcada como cortante, entonces corta; dada una escena sin la marca, entonces espera al hueco entre turnos.
- [ ] **CA-WAR-4** (R-WAR-7). Dada una evaluación durante un duelo, trilero, minijuego, votación, desafío o cinemática, entonces no hay intervención hasta que termina, y entonces la hay si procedía.
- [ ] **CA-WAR-5** (R-WAR-8). Dado un dilema cuya segunda opción no tiene objetivos válidos, entonces el evento no es candidato; dado un evento sin usos restantes, entonces tampoco.
- [ ] **CA-WAR-6** (R-WAR-8, R-SAC-1). Dado el Acto III, entonces los candidatos incluyen eventos de los tres sacos con los pesos del Acto III; dado un evento con ventana, entonces solo es candidato dentro de ella.
- [ ] **CA-WAR-7** (R-WAR-10). Dada una partida registrada, cuando se reproduce con la misma semilla y las mismas entradas de jugadores, entonces el registro de decisiones del Warden es idéntico.
- [ ] **CA-DIS-1** (R-DIS-1). Dado un jugador con dos tipos más que el segundo, entonces Líder claro está activo y el peso de los eventos con dirección líder es el base por su multiplicador; cuando deja de cumplirse, entonces vuelve al base.
- [ ] **CA-DIS-2** (R-DIS-1). Dado un jugador con cinco tipos, entonces la probabilidad de intervenir es la del acto por 1,5 y los eventos contra él dominan los candidatos.
- [ ] **CA-DIS-3** (R-DIS-1). Dado un jugador sin ser objetivo durante el umbral, entonces su peso como dirección "al azar" es mayor que el de los demás; dado que está en el Vacío, entonces no cuenta.
- [ ] **CA-DIS-4** (R-DIS-1). Dados dos disparadores activos que afectan al mismo evento, entonces su peso es el base por ambos multiplicadores.
- [ ] **CA-INS-1** (R-INS-1, 2). Dado un jugador con insolencia 0, cuando agota el reloj de turno, entonces su insolencia es 1 y recibe un castigo sorteado con la tabla del nivel 0, nunca un regalo, con causa "por ignorarme".
- [ ] **CA-INS-2** (R-INS-3). Dado un jugador con insolencia 4, cuando comete otra, entonces se lanza la Furia sin sorteo: está en el Vacío de la Furia y su insolencia es 0; cuando dentro intenta Desvío, entonces no sale, no pierde la runa y se emite la frase de negativa; cuando hay Liberación, entonces sigue dentro; cuando supera el escape, entonces sale.
- [ ] **CA-INS-3** (R-INS-1). Dado un bot, cuando pasa lo que sea, entonces su insolencia sigue en 0.
- [ ] **CA-ORA-1** (R-ORA-3). Dada una consulta, cuando el jugador mantiene la barra hasta llenarla dentro de los 7 s, entonces la sentencia se revela y se aplica sin insolencia; cuando no lo hace, entonces se revela y se aplica igual y su insolencia sube.
- [ ] **CA-ORA-2** (R-ORA-4). Dado sesgo 0, cuando se consulta N veces con el mismo estado, entonces la distribución es uniforme entre candidatos válidos; dado sesgo 1, entonces coincide con la del cerebro; dado 0,5, entonces es la media.
- [ ] **CA-ORA-3** (R-ORA-1). Dado un dilema o una votación, entonces nunca se entrega por el Oráculo.
- [ ] **CA-VOT-1** (R-VOT-1 a 4). Dada una votación con cuatro jugadores, cuando todos votan, entonces el más votado recibe el efecto y los votos quedan registrados y visibles; cuando hay empate, entonces se elige al azar entre los empatados; cuando alguien no vota, entonces su voto es al azar y su insolencia sube; dado un jugador, entonces no puede votarse a sí mismo.
- [ ] **CA-SAC-1** (R-SAC-2). Dados los 38 eventos en datos, cuando se cargan, entonces cada uno se valida contra el catálogo del GDD 05 y los seis ejemplos de R-COM-3 del GDD 05 producen su resultado.
- [ ] **CA-SAC-2** (R-SAC-1). Dado un evento con usos máximos 1, cuando se ha usado, entonces no vuelve a salir en esa partida.
- [ ] **CA-COM-1** (R-WAR-9, R-COM-1). Dado un tipo de comentario con tres variantes, cuando se elige tres veces seguidas, entonces no se repite la variante inmediatamente anterior.
- [ ] **CA-PRO-1** (R-PRO-1, 3). Dado el inicio, entonces se emiten en orden: presentación, una frase por Dominio corrompido, una frase por jugador con el orden, apertura; dado el fin de carrera, entonces la asignación de Olvidados va acompañada de una frase por Eco con su nombre.
- [ ] **CA-VOZ-1** (R-VOZ-1). Dada una frase en datos, entonces existe su audio pregenerado y su subtítulo, y la partida no hace ninguna petición de red para reproducirla.
- [ ] **CA-ORA-4** (R-ORA-5). Dado Tifón, cuando recibe una sentencia por el Nodo del Oráculo, por Consulta forzada o por un golpe entregado por el Oráculo, entonces en los tres casos puede rechazarla una vez dentro de su tiempo y la segunda es obligatoria.
- [ ] Sin valores fijos en el código: todo lo de la tabla de parámetros sale de datos.
- [ ] Registro: cada decisión, cada insolencia, cada consulta y cada voto emiten eventos con marca de tiempo.

## Nombres nuevos para el glosario

Anfitrión del Warden (`WardenHost`, el `UWorldSubsystem`), Golpe (`Strike`), Escena (`Scene`), Comentario (`Remark`), Garantía (`InterventionGuarantee`), Vacío de la Furia (`FuryVoid`), Votación (`Vote`), Ventana de evento (`EventWindow`), Corte (`CutsTurn`), Sesgo del Oráculo (`OracleBias`), Usos máximos (`MaxUses`). Disparadores: `ClearLeader`, `ImminentVictory`, `IgnoredPlayer`, `StalledMatch`, `Hoarder`. Corrección de entradas: Insolencia (fuentes), Furia (Vacío de la Furia), Oráculo (sesgo y barra), Saco de eventos (ventanas y usos), Disparador contextual (los cinco), Dilema (entrega solo directa), Humor del Warden (se sustituye por canal y acto).

## Fuera de alcance de esta fase

- Voz real y actor: Fase 2. La sintética es placeholder.
- Bajada del contador de insolencia: solo si el playtest lo pide.
- Más votaciones y más eventos: contenido de datos, cuando haya frases y voz.
- Trato del Warden (salir del Vacío pagando): idea aparcada; se pisa con Desvío.
- Trampa del Warden: Fase 2 (GDD 05).
- Disparadores adicionales (por ejemplo, "Eco apagado", "Elegido sin runas") y un modelo de tensión objetivo: Fase 2.
- Warden que aprende de la mesa entre partidas: fuera de todas las fases previstas.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| **El fin de partida corta todo** (decisión del autor, 27/10/2026, cambia la lectura de R-WAR-4d y R-VIC-4): a los 40 minutos gana el Warden y se corta lo que haya en marcha, también un duelo, un trilero, una consulta, una votación o el desafío final, sin margen de arranque y sin excepción; lo que estuviera en juego no cambia de manos. Es justo porque es visible (el reloj en pantalla, el Warden avisa al minuto y a los diez segundos) e igual para todos; un margen de arranque se descartó porque esconde el final. El informe de S-018 cuenta los desafíos finales que corta el reloj | Autor | Decidido | S-018 |
| **El Warden enseña a jugar; no hay tutorial** (decisión del autor, 22/10/2026): un tipo de frase nuevo, **instrucción**, que el Warden dice la primera vez que a un jugador le toca tirar, elegir casilla, caer en una Fuente, ver un Nicho, recibir un dilema, batirse, consultar el orbe o votar, con el foco sobre él y su personalidad de siempre. Cada instrucción tiene identificador y **el jugador guarda en su máquina cuáles ha visto** (perfil local, no estado de partida): vista una vez, no vuelve nunca; en red, cada cliente trae su lista al unirse y el Warden instruye solo a quien le falta; en una mesa local con un humano, la lista es la del PC. Ajuste "ayudas" en opciones (GDD 07): automático (hasta verlas una vez; inicial), siempre, nunca; la línea de pantalla que dice qué se espera del jugador sigue el mismo ajuste. Página "cómo se juega" en el menú local. Lo que decide si basta es el test de usabilidad con dos personas de fuera, sin que nadie explique nada. Frases con la story "La voz del Warden"; ajuste y perfil con la del menú; nombres al glosario con ellas | Autor | Decidido | |
| Valores de probabilidad, pesos y multiplicadores | Simulación | Antes del sprint del Warden | Los iniciales son punto de partida |
| **Decisiones de S-016 (autor, 19/10/2026)**: (1) **Garantía anticipada** (cambia la lectura de R-WAR-5): el cerebro sortea el instante de la siguiente evaluación y, si esperar hasta él dejaría pasar más de 120 s sin golpe ni escena, interviene ya; así ningún hueco supera los 120 s (CA-WAR-2 literal). Las re-evaluaciones tras un aplazamiento no cuentan para el intervalo 15 ± 5. (2) El cerebro devuelve, con la decisión, el instante de la siguiente evaluación; su semilla vive en el estado (semilla de partida + 2): anfitrión y simulación lo llaman con el mismo contrato. (3) Un golpe que cae entre turnos se aplica sin cortar nada y Alto no es candidato. (4) El objetivo de cortes por acto se mide e imprime; no se exige en test. (5) Comentarios desde S-016 con sus pesos, registrados sin texto hasta S-017. (6) El castigo de insolencia por reloj de turno se aplica al terminar ese turno, entre turnos, por `ApplyEffect`. (7) El fin de carrera que corta el turno va con los eventos programados (S-017). (8) Liberación con solo Vacíos de la Furia dentro: no válida. (9) **R-WAR-7 manda sobre R-WAR-5**: si la evaluación que debía cumplir la garantía cae durante un duelo, un trilero o un minijuego, se aplaza y el hueco puede pasar de 120 s lo que dure aquello; CA-WAR-2 lo comprueba así y el informe imprime el hueco máximo real. (10) **La Furia iguala a todos** (20/10/2026, cambia R-INS-3): la Furia castiga al jugador por no atender, no al personaje; el Vacío de la Furia es **otro sitio** con sus reglas: no valen Desvío, Liberación, la salida del Eco (R-ECO-5) ni la escalera de Mictlantecuhtli; todos juegan una **escalera propia de datos, más dura** (`furyEscapeLadder`, valor inicial difícil, difícil, normal) con el tope de cuatro turnos del pilar 2. Nombre y aspecto del sitio, del autor, con la interfaz | Autor | Decidido | |
| **Expulsión del juego y la pared de almas** (idea del autor, 20/10/2026): a la segunda Furia (otras N insolencias tras la primera, propuesta N = 5) el Warden expulsa al jugador con un evento propio ("sale cabreado"), su asiento pasa a un bot y el espíritu de su Eco o Elegido vuela a una pared de la Cripta donde se ven las almas atrapadas; al final de la partida, los que pierden van a la misma pared (gane el Warden o gane uno). Diseño decidido; implementación por partes: la regla con la story de bots (GDD 08, tras el 30/11), la pared con el arte de la Cripta, las escenas con las cinemáticas. **Nombre: el Muro de los Perdidos** (`WallOfTheLost`, propuesta de código), decidido el 20/10/2026; al glosario con la story que lo implemente | Autor | Decidido el diseño; implementación tras la entrega | |
| Sintetizador de voz concreto y licencia | Autor | Spike | Libre y sin conexión; se decide en el spike |
| Redacción de las frases (unas 150) | Autor, con ayuda | Sprint del Warden | Originales; ninguna de material ajeno |
| Cuándo termina el Warden la Noche de los Ecos | Autor | Sprint del Warden | Propuesta: al primer robo de un Eco a un Elegido, o pasadas N rondas sin robo |
| ¿Deben los bots ser insolentes a veces, para probar la Furia en simulación? | Autor | Spike | Propuesta: un dummy "insolente" solo para tests |
