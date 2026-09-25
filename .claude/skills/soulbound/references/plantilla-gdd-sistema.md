# Plantilla de GDD por sistema

Adaptada de la plantilla de Claude Code Game Studios (MIT) y recortada a lo que un juego de mesa digital necesita. Copia este esqueleto a `docs/gdd/NN-<sistema>.md` y rellena sección a sección siguiendo el bucle de autoría. Deja `[Pendiente]` en lo que no esté decidido; nunca rellenes con suposiciones.

Por qué esta estructura: cada sección responde a una pregunta que alguien hará después. Reglas para el programador, estados para el test, casos límite para el revisor, parámetros para el equilibrado, criterios de aceptación para saber cuándo está hecho. Si una sección no aplica al sistema, escribe "No aplica" y el motivo; no la borres.

---

# [Nombre del sistema]

> **Estado**: Borrador | En revisión | Aprobado | Implementado
> **Autor**: Lluis Alonso
> **Fecha**: [aaaa-mm-dd]
> **Fase**: 1 | 2 | 3
> **Pilares que sirve**: [números]
> **Fuente**: GDD v0.1 secciones [x.y]

## Resumen

[Dos o tres frases: qué es, qué hace por el jugador, por qué existe. Sin jerga. Quien lea solo esto debe saber si necesita leer el resto.]

## Fantasía del jugador

[Qué debe sentir el jugador al interactuar con este sistema. Esta sección guía todas las decisiones de detalle. Si no hay una emoción clara, el sistema probablemente es infraestructura: dilo.]

## Reglas

[Reglas precisas y sin ambigüedad. Un programador debe poder implementarlas sin preguntar. Numeradas si son secuenciales, con viñetas si son propiedades. Cada regla lleva un identificador `R-<sistema>-<n>` para poder citarla desde tests y user stories.]

## Estados y transiciones

[Si el sistema tiene estados, todos, con cada transición válida.]

| Estado | Condición de entrada | Condición de salida | Comportamiento |
|---|---|---|---|

## Interacción con otros sistemas

[Qué datos entran, qué datos salen, quién es responsable de qué.]

| Sistema | Dirección | Naturaleza |
|---|---|---|
| [Turnos] | Este depende de Turnos | Necesita saber el jugador activo |

## Fórmulas y probabilidades

[Toda fórmula o tabla de probabilidad, con sus variables.]

| Variable | Tipo | Rango | Origen | Descripción |
|---|---|---|---|---|

## Casos límite

[Qué pasa en situaciones raras. Cada caso con resolución y motivo.]

| Escenario | Comportamiento esperado | Motivo |
|---|---|---|

## Parámetros de ajuste

[Todo valor que se pueda tocar para equilibrar, con valor actual, rango seguro y efecto.]

| Parámetro | Valor actual | Rango seguro | Al subir | Al bajar |
|---|---|---|---|---|

## Presentación en pantalla plana

[Qué información se muestra, dónde, cuándo, y qué feedback visual y sonoro tiene cada evento del sistema. Es la sección que más falta en el GDD v0.1.]

| Evento o información | Dónde | Feedback visual | Feedback sonoro | Texto del Warden |
|---|---|---|---|---|

Si el sistema tiene pantalla, panel o HUD propios, además:

- **Estados**: qué se ve en estado normal, vacío (sin datos aún, con qué puede hacer el jugador), error (qué pasó y qué hacer, sin rutas ni códigos internos) y carga o espera. Con texto y valores de juego reales, nunca texto de relleno.
- **Legibilidad**: contraste mínimo 4.5:1 entre texto y fondo en HUD y subtítulos, comprobado en Steam Deck a 1280x800; toda opción seleccionable sigue legible mientras corre el tiempo.
- **Entrada**: la misma pantalla se maneja con ratón y con mando; elegir entre pocas opciones y confirmar, nunca cursor libre.

## Contribución a los pilares y a las métricas

[Cómo sirve este sistema a los pilares que declara arriba, y qué métrica de diversión mueve. Si no mueve ninguna, justifica por qué existe.]

## Criterios de aceptación

[Uno por regla y por fórmula, en formato Dado / Cuando / Entonces. Cada uno debe poder convertirse en un test sobre el módulo de reglas o en una comprobación de playtest concreta.]

- [ ] **CA-<sistema>-1** (R-<sistema>-1). Dado [estado], cuando [acción], entonces [resultado].
- [ ] Sin valores fijos en el código: todos los parámetros de ajuste salen de datos.
- [ ] Si hay pantalla: un criterio por estado (normal, vacío, error, carga) que diga qué se ve, y uno de legibilidad en Deck.

## Fuera de alcance de esta fase

[Lo que el GDD v0.1 describe para este sistema y se deja para Fase 2 o 3, con una línea cada uno.]

## Cuestiones abiertas

| Cuestión | Responsable | Plazo | Resolución |
|---|---|---|---|
