# Checklist de revisión

Dos listas: la primera para un GDD de sistema, la segunda para un PR de código. El resultado siempre es una lista de hallazgos, cada uno con sección o archivo, gravedad (bloquea / importante / menor) y propuesta de arreglo. No apruebes tú: el autor aprueba.

# A. Revisión de un GDD de sistema

Aplícala antes de que un GDD pase a `Aprobado` y antes de generar user stories a partir de él.

## 1. Completitud

- [ ] Las once secciones de la plantilla están presentes. Las que no aplican dicen "No aplica" y por qué.
- [ ] No queda ningún `[Pendiente]` en Reglas, Estados, Casos límite ni Criterios de aceptación.
- [ ] Cada regla tiene identificador `R-…` y cada criterio cita la regla que comprueba.

## 2. Implementabilidad

- [ ] Un programador podría implementar las reglas sin hacer una sola pregunta. Señala cada frase que necesite interpretación.
- [ ] Los estados cubren todas las transiciones; no hay forma de quedarse en un estado sin salida.
- [ ] Las fórmulas producen valores sensatos en los extremos: prueba con el mínimo y el máximo de cada variable y anota si algo sale negativo, infinito o absurdo.
- [ ] Todo valor numérico está en Parámetros de ajuste, no escondido en el texto de una regla.
- [ ] Nada en las reglas necesita un mundo cargado, un actor o `GetWorld()`. Si lo necesita, está en la sección de presentación, no en Reglas.

## 3. Coherencia

- [ ] No contradice ningún GDD ya aprobado. Comprueba las dependencias declaradas en ambos sentidos.
- [ ] Usa solo nombres del glosario.
- [ ] Respeta las restricciones fijas: pantalla plana, 40 minutos, seis reinos, sin material de terceros, lógica sin mundo.
- [ ] Lo que se deja fuera está en "Fuera de alcance" con su fase, no simplemente omitido.

## 4. Pilares

Para cada uno de los cuatro pilares, responde en una línea cómo lo sirve o por qué no aplica. Luego:

- [ ] ¿Funcionaría igual sin el Warden? Si sí, el sistema no da nada al pilar 1: o le da un papel al Warden o se justifica como infraestructura.
- [ ] ¿Qué hace un jugador que no está en su turno mientras este sistema actúa? Si la respuesta es "nada", falla el pilar 2.
- [ ] ¿Puede un jugador nuevo explicar en una frase cada consecuencia de este sistema? Si no, falla el pilar 3.
- [ ] ¿Cada castigo o efecto sobre un jugador tiene nombre, cámara, sonido y frase del Warden en la sección de presentación? Si no, falla el pilar 4.

## 5. Las cinco preguntas por mecánica

Para cada mecánica del sistema, en una línea cada una:

1. **Claridad**: ¿el jugador entiende qué puede hacer y qué acaba de pasar?
2. **Motivación**: ¿por qué querría hacerlo? ¿Qué gana o evita?
3. **Respuesta**: ¿el juego reacciona de forma inmediata y visible?
4. **Satisfacción**: ¿el resultado se siente merecido, o injusto de una forma divertida?
5. **Encaje**: ¿sirve a algún pilar, o está porque el juego de referencia lo tenía?

Una mecánica con dos o más respuestas flojas se marca como importante.

## 6. Criterios de aceptación

- [ ] Cada criterio es comprobable con un test o con una observación concreta de playtest. Frases como "funciona bien", "es divertido" o "está equilibrado" no son criterios; propón la reescritura.
- [ ] Hay al menos un criterio por regla y uno por fórmula.
- [ ] Los criterios de presentación dicen qué se ve y qué se oye, no "hay feedback".

## 7. Métricas

- [ ] El documento dice qué métrica de diversión mueve (M1 a M4) y en qué dirección.
- [ ] Si introduce un evento del Warden, dice a quién afecta y qué hacen los demás mientras tanto.

## Formato del informe

```
## Revisión: [sistema] — [fecha]
Veredicto propuesto: Aprobar / Aprobar con cambios menores / Devolver

### Bloquea
- [sección] hallazgo → propuesta

### Importante
- ...

### Menor
- ...

### Pilares
1. ... 2. ... 3. ... 4. ...
```

# B. Revisión de un PR de código

Aplícala a cada PR de story, con la story y el GDD al lado. Lo que bloquea se arregla antes de fusionar.

## 1. Trazabilidad

- [ ] Cada criterio de la story tiene al menos un test que lo cita (`CA-XXX-n`, `R-XXX-n`) y el test falla si se quita la implementación.
- [ ] No hay reglas de juego que la story no pida. Si las hay, están en "Decisiones no solicitadas" del PR y esperan al autor.
- [ ] Los nombres nuevos están en el glosario o propuestos en el PR. Nada se llama por el patrón que usa (`XxxFactory`, `XxxStrategy`) si el glosario tiene un nombre de dominio para ello.
- [ ] La bitácora tiene su entrada y el PR sus cuatro secciones.

## 2. Arquitectura

- [ ] Nada en `SoulboundCore` toca `UWorld`, `UObject`, `Engine` ni `GetWorld()`. Los actores y subsistemas de `Soulbound` solo traducen entrada, replicación y presentación; ninguna regla vive allí.
- [ ] Toda mutación de `FMatchState` o `FBoardState` pasa por una función de reglas que valida antes de escribir; no hay campos modificados sueltos desde fuera.
- [ ] Cada clase o struct tiene una sola razón de cambio. Ojo especial al cerebro del Warden (evaluar, aplicar, notificar son razones distintas) y a lo que se migre del spike (que no crezca un objeto que lo hace todo).
- [ ] Una implementación nueva de una interfaz (`IDecider`, bots) cumple el mismo contrato que las existentes: mismos tiempos, mismos resultados posibles, ningún caso que solo ella "no soporte".
- [ ] Si la story toca red: el servidor valida todo lo que llega del cliente antes de aplicar una regla, y el PR responde en una línea "¿qué pasa si un cliente miente o repite este mensaje?".

## 3. Código

- [ ] Sin números mágicos: los valores de juego son parámetros con nombre y valor inicial documentado.
- [ ] Sin duplicación de reglas entre `SoulboundCore` y `Soulbound`, ni entre dos funciones del motor.
- [ ] Un `switch` que crece con cada tipo nuevo (efectos, direcciones, decisores) es candidato a datos o a una interfaz; se señala aunque no se cambie en esta story.
- [ ] Sin excepciones de C++; la validación devuelve un resultado.
- [ ] Las funciones públicas nuevas de `SoulboundCore` llevan comentario de cabecera. La deuda es `// TODO(R-XXX-n): motivo`, nunca un TODO mudo.
- [ ] Sin código muerto ni código del spike traído "por si acaso".

## 4. Tests

- [ ] La mayoría de los tests nuevos están en `SoulboundCore`; los de `Soulbound` son pocos y comprueban integración, no reglas.
- [ ] Los tests preparan su estado con una función común cuando se repite, en vez de copiar el montaje.
- [ ] Un test determinista por semilla fija la semilla y lo dice.
- [ ] `.\Scripts\test.ps1` en verde en la máquina del revisor, no solo según el PR.

## 5. Seguridad y entrega

- [ ] Ningún secreto, ni ruta local, ni identificador de EOS en el diff.
- [ ] Ningún asset de Unreal salvo que la story lo pida; si lo pide, commit aparte, descripción en el PR y verificado en el editor.
- [ ] Los mensajes que vea el jugador no revelan rutas, versiones internas ni detalles de sesión.

Formato del informe: el mismo que en A, con archivo y línea en lugar de sección.
