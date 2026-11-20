# Soulbound: concepto y pilares

> **Estado en la entrega del TFM (noviembre de 2026)**: Los pilares, los anti-pilares, la duración (40 minutos en tres actos) y la estructura de seis Dominios siguen vigentes y están en el motor de reglas. La Fase 1 se entregó más corta de lo que aquí se describe: un jugador contra tres dummies en local, el Warden con subtítulos y sin voz, y el minijuego en su versión mínima (S-026). Pasan a la Fase 2: la partida en línea, el bot básico, la voz y el audio (salvo el golpe del corte, S-050), la interfaz diegética, el mando, las apuestas de espectadores, la medición de M1 a M4, Steam y Steam Deck.
>
> Este documento se publica como se aprobó en septiembre de 2026, sin reescribirlo. Donde no coincida con el juego, mandan el glosario (`.claude/skills/soulbound/references/glosario.md`), la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`). Las menciones a "v0.1" remiten al primer documento de diseño, de 2025, anterior al máster y no publicado.

> **Estado**: Aprobado v0.2 (15/09/2026)
> **Autor**: Lluis Alonso
> **Fecha**: 15/09/2026
> **Sustituye a**: sección 1 del GDD v0.1 (26/07/2025)

## Pitch

Soulbound es un party game de terror social para PC y VR, de uno a cuatro jugadores. Atrapados en una cripta viviente, compiten en un juego de mesa mortal dirigido por un anfitrión sádico controlado por IA, el Warden. Para escapar deben forjar un vínculo con uno de los seis Elegidos, usar sus habilidades, reunir seis runas y sobrevivir a los caprichos del Warden antes de que el tiempo se agote y sus almas queden atrapadas para siempre.

## Fantasía central

Estás a merced de alguien que se divierte contigo. Escapar significa ganarle a él y a tus amigos a la vez, y cada partida deja una anécdota que contar.

## Género y plataforma

- **Género**: party game social. Subgéneros: juego de mesa digital, tensión con humor negro.
- **Plataforma principal**: PC (Steam), pantalla plana, ratón y teclado o mando.
- **Plataforma premium posterior**: VR (Meta Quest, PC VR), donde la fantasía se completa con interacción corporal.
- **Fases**: 1) tablero multijugador en plano, motor de reglas completo, Warden completo con voz sintética, bot básico y minijuego de habilidad (TFM); 2) voz real, más contenido, bots con dificultad, reconexión y migración; 3) VR, con la VR autónoma como objetivo. (Actualizado el 22/09/2026 tras los GDD 01 a 09.)
- **Jugadores**: 1 a 4. Los huecos hasta cuatro los ocupan bots. En Fase 1 hay un bot básico con criterio sobre una interfaz de decisión extensible (GDD 08); los dummies son solo para pruebas y simulación.
- **Seis Dominios, seis Elegidos, seis runas, siempre.** Al empezar, el Warden corrompe 6 menos N Dominios al azar (Dominios Olvidados): Nicho cerrado, Fuente de Poder en la que un Elegido debe superar el minijuego en fácil y un Eco no, aspecto oscurecido. Quedan exactamente N Elegidos disponibles para N jugadores. Las seis runas siguen en juego, así que hay que entrar en los Dominios corrompidos. Tras la carrera, los Ecos quedan confinados a ellos y al Altar. Cada partida activa una combinación distinta de Dominios.
- **El tablero es un Altar central con seis Dominios.** Todos empiezan en el Altar; los Dominios se conectan con el Altar y entre vecinos. Con seis runas distintas se vuelve al Altar, se engarzan y arranca el desafío final. (Decidido el 15/09/2026; los detalles, en el GDD de tablero.)

## Aestéticas objetivo (MDA)

| Rango | Aestética | Cómo la entrega Soulbound |
|---|---|---|
| 1 | Compañerismo | Voz, alianzas, traiciones, apuestas de espectadores, derrota colectiva |
| 2 | Fantasía | Ser el peón de un anfitrión omnipotente; vincularse a un ser oscuro |
| 3 | Desafío | Gestionar el azar y al Warden con decisiones cortas |
| 4 | Sensación | En VR: dados, Oráculo, castigos corporales. En PC: cámara, sonido, voz del Warden |
| No | Narrativa | El lore es ambiente, no trama. No hay historia que seguir |
| No | Descubrimiento | No hay secretos que explorar; todo está a la vista desde la primera partida |

## Pilares

### Pilar 1: El Warden es el motor de la partida

**Definición**: en cualquier tramo de dos minutos de partida hay al menos una intervención del Warden que afecta a algún jugador. Si se quita al Warden, el juego deja de funcionar.

**Aestéticas**: fantasía, compañerismo.

**Prueba de diseño**: entre dos mecánicas equivalentes, elegimos la que le da al Warden algo que hacer, decir o decidir. Una mecánica que funciona igual sin el Warden no pertenece al juego.

| Departamento | Este pilar dice... | Ejemplo |
|---|---|---|
| Diseño | Los eventos no salen de cartas anónimas, salen del Warden con intención | Castiga al líder, sacude la partida estancada, busca al ignorado |
| Arte | Presencia en dos capas: entre intervenciones solo rastros (trono vacío, ojos en la oscuridad, susurros); al intervenir se manifiesta, habla y desaparece. Cada aparición es un pequeño susto. El acto no se muestra en ningún indicador: se lee en su máscara y en su tono; los golpes y escenas lo materializan, los comentarios son solo su voz | Trueno y manifestación; máscara que se rompe por actos |
| Audio | Su voz es el sonido principal del juego | Voz sintética pregenerada con subtítulo en Fase 1; actor después |
| Narrativa | Todo el lore se cuenta a través de él, nunca en pantallas de texto | Presentación, fin de carrera, furia, derrota |
| Ingeniería | Su cerebro es una clase sin mundo, de estado y semilla a decisión, alojada en un subsystem de servidor; simulable | Diez mil partidas para ajustar pesos |

**Lo sirve**: sacos de eventos por acto, disparadores contextuales, insolencia, modo aniquilación.
**Lo viola**: eventos aleatorios sin voz ni objetivo, un tablero que se juega solo, castigos "del sistema".

### Pilar 2: El turno de los demás también es tu turno

**Definición**: ningún jugador pasa más de 90 segundos sin que algo le afecte o sin poder influir en lo que pasa, aunque no sea su turno.

**Aestéticas**: compañerismo, desafío.

**Prueba de diseño**: todo evento se diseña como escena para toda la mesa: la víctima lo sufre y los demás lo ven y lo oyen. Un castigo a un solo jugador es válido; lo que no es válido es que ocurra en privado o en silencio. Además, al menos uno de cada tres eventos ofrece a los espectadores una acción: apostar, votar o elegir. Si la métrica de 90 segundos falla en playtest, se sube esa proporción o se acortan los turnos.

| Departamento | Este pilar dice... | Ejemplo |
|---|---|---|
| Diseño | Dilemas simultáneos, apuestas en duelos, el Warden se dirige a los espectadores | "¿A quién apoyáis?" antes de cada duelo |
| Arte | La pantalla muestra siempre a los cuatro, nunca solo al activo | Fila de retratos permanente con estado de cada uno |
| Audio | El Warden nombra a quien no está jugando | "Tú, el callado, ¿qué crees que va a sacar?" |
| Narrativa | Los castigos son públicos y teatrales | Foco de evento |
| Ingeniería | Reloj de turno de 30 segundos; métrica de segundos entre momentos por jugador | Registrada en cada partida |

**Lo sirve**: apuestas de espectadores, votos en dilemas, decretos globales, derrota colectiva por tiempo.
**Lo viola**: turnos largos, esperar sin nada que hacer, eventos privados que los demás no ven.

### Pilar 3: Caos legible

**Definición**: el azar y la traición son la diversión, pero el jugador siempre entiende por qué le ha pasado algo y quién se lo ha hecho. Ningún castigo sin nombre, causa visible y consecuencia clara.

**Aestéticas**: desafío, compañerismo.

**Prueba de diseño**: si un jugador nuevo no puede explicar en una frase por qué acaba de perder una runa, la mecánica se simplifica o se explica mejor, nunca se esconde.

| Departamento | Este pilar dice... | Ejemplo |
|---|---|---|
| Diseño | Pocos tipos de casilla, pocas reglas, muchas combinaciones | Seis poderes, seis casillas, un anfitrión |
| Arte | Cada estado tiene un icono y un color únicos | Runas por color, Sellos visibles, marca del perseguido |
| Audio | Cada evento tiene un sonido propio | Trueno del Warden, tintineo de runa |
| Narrativa | El Warden explica lo que hace mientras lo hace | "Te la quito porque ibas ganando" |
| Ingeniería | Un registro de eventos en pantalla y en fichero | Historial de la partida consultable |

**Lo sirve**: Grimorio, revelación de brazales, mensajes del Warden con causa.
**Lo viola**: efectos ocultos, reglas de excepción acumuladas, castigos sin explicación.

### Pilar 4: El Warden te habla a ti

**Definición**: las consecuencias son personales. Cuando el Warden actúa sobre un jugador, ese jugador lo ve, lo oye y lo siente de forma distinta a los demás, y los demás ven que le pasa a él.

**Aestéticas**: fantasía, sensación.

**Prueba de diseño**: un castigo que solo mueve una ficha está incompleto. Debe tener, como mínimo, nombre, cámara, sonido y una frase del Warden dirigida a la víctima. En VR, además, se siente en el cuerpo.

| Departamento | Este pilar dice... | Ejemplo |
|---|---|---|
| Diseño | Los castigos se diseñan como escenas, no como restas | Vacío: aislamiento, voz distorsionada, grieta en el asiento |
| Arte | La cámara y la interfaz aíslan a la víctima | Zoom, pantalla en gris, panel que tiembla |
| Audio | Filtros de voz por estado | Eco del Vacío |
| Narrativa | El Warden usa tu nombre y tu situación | "Otra vez tú, pequeño" |
| Ingeniería | Cada evento del núcleo lleva víctima, causa y texto | Estructura de evento con destinatario |

**Lo sirve**: foco de evento, sistema del Vínculo, transformación en Elegido.
**Lo viola**: castigos silenciosos, mensajes genéricos, tratar a todos igual.

## Anti-pilares

- **NO es un juego de estrategia.** Nadie debería planificar más de un turno por delante. Si una mecánica exige calcular, se simplifica. Protege el pilar 3 y la duración.
- **NO es cooperativo.** Solo uno escapa. La derrota colectiva por tiempo crea tensión compartida, pero la victoria es individual. Las alianzas existen para romperse.
- **NO es survival horror.** Es tensión con humor negro. Los sustos son puntuales y siempre del Warden, nunca del entorno. Protege el pilar 1.
- **NO es VR primero.** La Fase 1 es pantalla plana. Ninguna mecánica de Fase 1 depende de gestos. Protege el alcance del TFM.
- **NO es una adaptación de ningún juego de mesa existente.** Diseño, nombres, personajes y textos son propios. La inspiración es un género, no un título.

## Resolución de conflictos entre pilares

| Prioridad | Pilar | Por qué gana |
|---|---|---|
| 1 | El Warden es el motor | Sin él no hay juego que distinguir de otros |
| 2 | El turno de los demás también es tu turno | Es la diferencia entre un juego que se repite y uno que se abandona |
| 3 | Caos legible | Si no se entiende, la diversión se convierte en frustración |
| 4 | El Warden te habla a ti | Es intensidad; se puede rebajar sin romper el juego |

## Arco emocional de una partida estándar (40 minutos)

| Fase | Duración | Emoción objetivo | Pilar | Mecánica |
|---|---|---|---|---|
| Presentación | 0-1 min | Inquietud, risa nerviosa | 1, 4 | El Warden corrompe reinos y presenta a los Ecos |
| Carrera | 1-6 min | Prisa, competición | 1, 2 | Un dado, salida del Altar, Nichos limitados, Acto I ligero |
| Caza | 6-28 min | Rivalidad, alianzas, traiciones | 2, 3 | Runas, duelos, dilemas, Oráculo |
| Clímax | 28-38 min | Persecución, injusticia divertida | 1, 4 | Acto III: aniquilación del líder, marcado |
| Cierre | 38-40 min | Triunfo o derrota compartida, revancha | 2 | Desafío final o victoria del Warden |

Nota: la duración es única (40 min); no hay variante larga.
| Gancho | tras la partida | "Otra, que esta vez te la devuelvo" | 2 | Resultados con la anécdota de la partida |

## Duración

- Una sola duración: 40 minutos, actos de 5, 22 y 13 minutos aproximados. Un solo equilibrado.
- Sin sistema de guardado.
- Si los jugadores piden partidas más largas, se añadirá como opción más adelante; no se diseña ahora.

## Público objetivo

- **Principal**: grupos de dos a cuatro amigos que juegan en PC por voz una noche entre semana. Valoran las risas, las anécdotas y que la partida quepa en una hora.
- **Secundario**: jugadores sociales de VR, cuando llegue la Fase 3.
- **Amplificador**: creadores de contenido; el juego está pensado para verse.

## Referencias

| Referencia | Qué tomamos | Qué hacemos distinto | Pilar |
|---|---|---|---|
| Juegos de mesa con anfitrión en vídeo de los 90 | Un anfitrión que interrumpe la partida y dicta castigos | El anfitrión reacciona al estado de la partida y tiene memoria de cada jugador | 1 |
| Jackbox | Anfitrión con personalidad; todos participan siempre | Tablero y objetivo de escape | 2 |
| Demeo | Juego de mesa 3D legible en pantalla y en VR | Competitivo, con anfitrión hostil | 3 |
| Inscryption | Un anfitrión que manipula las reglas y rompe la cuarta pared | Multijugador y social | 1, 4 |
| Phasmophobia, Lethal Company | Miedo y pánico hilarante entre amigos por voz | Sin exploración; todo en una mesa | 2 |
| Mario Party | Caos de dados y traiciones en pantalla plana | Sin minijuegos cada ronda; el Warden hace ese papel | 2, 3 |

**Inspiraciones no videojuego**: cine gótico expresionista, Tim Burton, la figura del maestro de ceremonias sádico.

## Marketing (resumen)

- El Warden lleva las redes del proyecto con su personalidad, antes de que exista el juego.
- Objetivo de Fase 1: página de Steam y lista de deseos abiertas al terminar el TFM.
- Ninguna referencia a títulos ajenos en materiales públicos.

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
| Nombre de la clase de personajes | Autor | Antes del GDD de carrera inicial | Elegidos (15/09/2026) |
| Rediseño del cuarto personaje | Autor | Antes del GDD de runas y poderes | Resuelta el 16/09/2026: los seis Elegidos son figuras mitológicas (GDD 03) |
| Voz del Warden en Fase 1: texto solo o texto más voz sintética de placeholder | Autor | Antes del sprint del Warden | Resuelta el 18/09/2026: voz sintética pregenerada con subtítulo (GDD 06) |
