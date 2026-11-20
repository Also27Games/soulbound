# Documentos de diseño (GDD)

El diseño de Soulbound, un documento por sistema. Se escribieron y aprobaron en septiembre de 2026, antes de programar, con el bucle de autoría de la skill del proyecto (`.claude/skills/soulbound/SKILL.md`): contexto, preguntas, opciones con recomendación, y redacción solo después de la decisión del autor.

Se publican como se aprobaron, sin reescribirlos. Cada uno lleva al principio una nota, **"Estado en la entrega del TFM"**, que dice qué parte está implementada, qué se hizo de otra manera y qué queda para la Fase 2. Donde un documento no coincida con el juego, mandan el glosario, la bitácora (`docs/bitacora.md`) y las stories (`docs/stories/`).

| Nº | Documento | En la entrega |
|---|---|---|
| 00 | [Concepto y pilares](00-concepto-y-pilares.md) | Vigente; la Fase 1 se entregó más corta de lo que describe |
| 01 | [Tablero y movimiento](01-tablero-y-movimiento.md) | Implementado |
| 02 | [Turnos y dados](02-turnos-y-dados.md) | Implementado, salvo apuestas y desconexiones |
| 03 | [Carrera y Elegidos](03-carrera-y-elegidos.md) | Implementado casi entero |
| 04 | [Runas y poderes](04-runas-y-poderes.md) | Implementado, salvo apuestas y trampas del desafío final |
| 05 | [Castigos, Nicho y Vacío](05-castigos-nicho-y-vacio.md) | Implementado, salvo una regla de los dilemas |
| 06 | [El Warden](06-warden.md) | Implementado el cerebro; sin voz, máscara ni cinemáticas |
| 07 | [Interfaz en plano](07-interfaz-en-plano.md) | Fase 2; la interfaz entregada es otra, descrita en las stories |
| 08 | [Multijugador y bots](08-multijugador-y-bots.md) | Implementadas la interfaz de decisión y la simulación; lo demás, Fase 2 |
| 09 | [Minijuego](09-minijuego.md) | Implementada la versión mínima del anillo |

Las decisiones de arquitectura están en [`docs/adr/`](../adr/README.md), y el mapa de lectura para quien evalúa, en [`docs/decisiones.md`](../decisiones.md).
