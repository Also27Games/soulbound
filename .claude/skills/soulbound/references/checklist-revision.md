# Checklist de revisión de un GDD de sistema

Aplícala antes de que un GDD pase a `Aprobado` y antes de generar user stories a partir de él. El resultado es una lista de hallazgos, cada uno con sección, gravedad (bloquea / importante / menor) y propuesta de arreglo. No apruebes tú: el autor aprueba.

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
