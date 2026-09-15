---
name: soulbound
description: Conocimiento del proyecto Soulbound (party game de terror social con anfitrión IA, Unreal Engine 5 / C++, TFM). Úsala siempre que se trabaje en cualquier cosa de este repositorio, aunque el usuario no la nombre - diseño, GDD de un sistema, reglas de juego, el Warden, runas, tablero, user stories, criterios de aceptación, arquitectura, tests, nombres de clases o de conceptos, revisión de un documento de diseño, playtests o métricas de diversión. Contiene el glosario oficial, los pilares con sus pruebas de diseño, las restricciones fijas, la plantilla de GDD por sistema, el bucle de autoría con aprobación y la checklist de revisión.
---

# Soulbound

## Qué es este proyecto

Soulbound es un party game de terror social para PC y VR, de uno a cuatro jugadores. Atrapados en una cripta viviente, compiten en un juego de mesa mortal dirigido por un anfitrión sádico controlado por IA, el Warden, que cambia las reglas a su antojo. Para escapar hay que vincularse a uno de los seis [CLASE], reunir seis runas y sobrevivir a sus caprichos antes de que el tiempo se agote.

Este repositorio contiene **Soulbound Core**, la primera fase: tablero multijugador en pantalla plana, motor de reglas y una versión mínima de la IA directora del Warden. Es un Trabajo de Fin de Máster de un máster sobre desarrollo con IA, así que el **proceso importa tanto como el producto**: especificaciones antes que código, motor de reglas testeable sin editor, agentes que implementan user stories, revisión automática, integración continua. Cada decisión de diseño se registra con su motivo.

El autor y única persona que decide es Lluis Alonso. Los agentes proponen, él aprueba.

## Cómo usar esta skill

1. Lee esta página entera antes de tocar nada de diseño o de reglas.
2. Para nombres de conceptos, abre `references/glosario.md`. No inventes nombres nuevos ni uses los antiguos.
3. Para escribir o revisar el GDD de un sistema, usa `references/plantilla-gdd-sistema.md` y sigue el bucle de autoría de abajo.
4. Para revisar un documento de diseño, aplica `references/checklist-revision.md`.
5. El detalle completo de pilares, anti-pilares y arco emocional está en `docs/gdd/00-concepto-y-pilares.md` del repositorio. Si no existe todavía en este checkout, pide al usuario dónde está.

## Los cuatro pilares, en corto

Cada propuesta de diseño se contrasta con estos cuatro, en este orden de prioridad. Si dos chocan, gana el de arriba.

1. **El Warden es el motor de la partida.** En cualquier tramo de dos minutos hay una intervención suya que afecta a alguien. *Prueba*: una mecánica que funciona igual sin el Warden no pertenece al juego.
2. **El turno de los demás también es tu turno.** Nadie pasa más de 90 segundos sin que algo le afecte o sin poder influir. *Prueba*: todo evento es una escena pública para la mesa entera; al menos uno de cada tres eventos da a los espectadores algo que hacer (apostar, votar, elegir).
3. **Caos legible.** Ningún castigo sin nombre, causa visible y consecuencia clara. *Prueba*: si un jugador nuevo no puede explicar en una frase por qué perdió una runa, se simplifica.
4. **El Warden te habla a ti.** Las consecuencias son personales: en PC se ven y se oyen con nombre; en VR se sienten. *Prueba*: un castigo que solo mueve una ficha está incompleto.

**Anti-pilares**: no es un juego de estrategia (nadie planifica más de un turno), no es cooperativo (derrota colectiva, victoria individual), no es survival horror (tensión con humor negro; los sustos son del Warden, no del entorno), no es VR primero, no es adaptación de ningún juego de mesa existente.

## Restricciones fijas

Estas no se discuten en cada tarea; si una propuesta las rompe, se descarta o se escala al autor.

- **Plataforma de la Fase 1: PC, pantalla plana, ratón y teclado o mando.** Ninguna mecánica de Fase 1 depende de gestos. La VR es Fase 3 y solo condiciona la arquitectura: el motor de reglas no sabe cómo se introdujo una acción.
- **Duración única de 40 minutos** con tres actos de 5, 22 y 13 minutos aproximados. Sin guardado.
- **Seis reinos, seis [CLASE], seis runas, siempre.** Con N jugadores, el Warden corrompe 6 menos N reinos al azar (Reinos Olvidados).
- **De 1 a 4 jugadores.** Los huecos los ocupan bots. En Fase 1, un bot básico con una heurística simple, diseñado para que añadir bots nuevos sea añadir una implementación de la misma interfaz de decisión. Los dummies sin criterio existen solo como herramienta de pruebas y simulación.
- **Todo el material es propio.** Ni nombres, ni textos, ni arte, ni tablas de ningún juego existente. Las inspiraciones se citan como género, no como título. Los assets de terceros con licencia (Synty, Fab) no se versionan en este repositorio.
- **Lógica de reglas sin mundo.** El estado de partida son structs; las reglas y el cerebro del Warden son funciones o objetos que no necesitan un nivel cargado ni `GetWorld()`. Los actores son adaptadores: entrada, replicación y presentación. Así los tests corren sin abrir el editor y el Warden se puede simular miles de veces.
- **Los commits los firma solo el autor.** No añadir líneas de coautoría de agentes.
- **Idioma**: documentación, user stories y textos del juego en español. Identificadores de código en inglés, con la correspondencia en el glosario.

## Alcance del TFM (Soulbound Core)

Incluye: sprints de Nodos del Poder, carrera por el vínculo y carrera hacia la victoria; Warden con tres actos, cinco o seis eventos y dos disparadores contextuales; sistema de insolencia completo, con tabla escalonada y Furia del Warden; bot básico con una heurística simple, construido sobre una interfaz de decisión que permita añadir otros bots sin tocar el motor; puertas; un minijuego de habilidad corto (timing o reacción) con tres presentaciones, duelo, escape del Vacío y desafío final; el robo de runas como interacción de elegir una esfera entre varias mezcladas; arte de placeholder con assets con licencia (Synty); build de Windows, documentación, slides y vídeo.

Excluye: VR, voz espacial, niveles de dificultad de los bots, arte propio final.

El alcance es amplio para el tiempo disponible: la meta es la ronda de diciembre de 2026 y la red es la de enero. La lista de recortes está en la decisión de TFM; si hay que recortar, se recorta en ese orden y se documenta, nunca en silencio.

Si una tarea propone algo fuera del alcance, márcalo como "Fase 2" o "Fase 3" en el documento y no lo implementes.

## Bucle de autoría de un GDD de sistema

Nunca generes un GDD entero y lo presentes como hecho. El autor decide cada sección, y un documento escrito sin él es un documento que no defiende en el tribunal.

Para cada sección de la plantilla:

1. **Contexto**: qué dice el GDD v0.1 sobre esto, qué dicen los pilares, qué dependencias hay con otros sistemas ya escritos.
2. **Preguntas**: las dos o tres decisiones abiertas que la sección exige.
3. **Opciones**: para cada pregunta, dos o tres opciones con su consecuencia en una línea. Recomienda una y di por qué.
4. **Decisión**: espera la respuesta del autor. Si es Claude Code, usa la herramienta de preguntas; si no, pregunta en texto y para.
5. **Borrador**: redacta la sección con la decisión tomada.
6. **Aprobación**: el autor aprueba, corrige o rehace.
7. **Escritura**: escribe la sección en el archivo. Una sección aprobada es un commit en el repositorio de notas mientras el GDD esté en privado, o en `docs/gdd/` cuando sea público.

Escribe cada sección al archivo en cuanto se aprueba, no al final: si la sesión se corta, lo aprobado sobrevive. Al retomar, lee el archivo del disco y continúa por la primera sección con `[Pendiente]`.

Orden de los GDD de sistema, que es el orden de implementación: tablero y movimiento; turnos y dados; carrera por el vínculo y [CLASE]; runas y poderes; castigos, Sepulcro y Vacío; Warden; interfaz en plano; multijugador y dummies.

## Criterios de aceptación y user stories

Cada regla del GDD acaba en un criterio de aceptación en formato Dado / Cuando / Entonces, y cada criterio acaba en un test de automatización sobre el módulo de reglas. Un criterio que no se puede comprobar con un test o con un playtest concreto no es un criterio; reescríbelo.

Las user stories siguen el formato "Como [rol], quiero [acción], para [beneficio]" con criterios de aceptación en lista, como las SOU-xx heredadas del planning original. Los roles son: Jugador, [CLASE] concreto, Eco Perdido, Sistema, IA Directora, Desarrollador.

## Métricas de diversión

Toda propuesta de mecánica dice cómo se mediría. Las métricas base, que se registran en cada playtest:

- **M1, segundos entre momentos por jugador**: tiempo medio entre dos hechos que afectan a un jugador concreto fuera de su turno. Objetivo: menos de 90.
- **M2, minuto 10**: qué hacen los jugadores que no tienen el turno (hablan, se pican, miran el móvil).
- **M3, risas por el Warden**: veces que alguien se ríe por algo que hace o dice. Objetivo: tres o más por partida.
- **M4, revancha**: cuántos jugadores quieren otra partida al terminar.

El motor de reglas emite un registro de eventos con marca de tiempo y destinatario para que M1 se calcule automáticamente.

## Convenciones de documentación

- `docs/gdd/00-concepto-y-pilares.md`: concepto, pilares, anti-pilares, arco emocional. Es la referencia para todo lo demás.
- `docs/gdd/NN-<sistema>.md`: un GDD por sistema con la plantilla.
- `docs/adr/`: decisiones de arquitectura, una por archivo, con contexto, decisión y consecuencias.
- Las decisiones de diseño llevan fecha y motivo. No se borran; se marcan como sustituidas.
- Estado de un GDD: `Borrador`, `En revisión`, `Aprobado`, `Implementado`. Solo el autor pasa a `Aprobado`.

## Lo que no está decidido todavía

- El nombre de la clase de personajes. Hasta que se decida se escribe `[CLASE]`. El nombre antiguo no se usa.
- El rediseño del cuarto personaje (antes "El Barón").
- Si el Warden habla solo por texto en Fase 1 o también con voz sintética de placeholder.

Si una tarea necesita uno de estos, pregunta al autor antes de seguir.
