# S-014. Catálogo de efectos y eventos

**Sistema**: castigos, Nicho y Vacío (GDD 05). **Módulos**: `SoulboundCore` y `Soulbound` (vista y comandos). **Rama base**: `main`. **Tamaño**: grande.

## Contexto

El Warden necesita un repertorio: cosas que puede hacer a alguien, con una dirección ("al activo", "al líder", "a quien señales") y combinadas en eventos que se escriben en datos sin tocar código ("dos turnos extra para ti, o mandas a casa a quien señales"). Esta story construye ese motor: la lista cerrada de efectos con sus direcciones permitidas, el filtro de objetivos válidos, el Líder, los decretos y los dilemas con su reloj, y la regla de "desatender al Warden nunca sale gratis". Implementa los trece efectos que se resuelven en el acto; los tres que encadenan con minijuego, trilero o duelo son de S-015, y los dos que interrumpen un turno, del Warden. Hasta que el Warden exista, los eventos se lanzan con un comando y en los tests.

Decidido por el autor el 10/10/2026: (1) el GDD 05 se reparte en S-014 (esta) y S-015 (Prueba del Warden, Robo decretado, Duelo forzado); el Warden pasa a S-016 y S-017 y la victoria a S-018; (2) los eventos se aplican entre turnos hasta las interrupciones de S-016; (3) el cerebro del spike no cambia: los eventos se lanzan con un comando y en los tests; (4) una ronda es cada vuelta completa del orden de turno.

## Reglas a implementar

- **R-EFE-0.** Catálogo cerrado de 18 efectos, cada uno con código, gravedad y direcciones permitidas (tabla del GDD, reproducida al final). Los 18 existen en el catálogo; en esta story se aplican 13: Revelar Brazal, Vuelta al hogar, Turno perdido, Sello, Sello contra rival, Marcado, Ley de Sombras, Perder una runa, el Vacío, Turno extra, Runa regalada, Liberación, Noche de los Ecos. Alto, Consulta forzada, Prueba del Warden, Robo decretado y Duelo forzado se cargan y validan, pero aplicarlos devuelve "pendiente de su story" sin cambiar nada.
- **R-EFE-2.** Revelar Brazal: los tipos de runa del objetivo quedan visibles para todos durante un número de rondas de datos (valor inicial 1); si ya estaba revelado, se prolonga.
- **R-EFE-4, R-EFE-5, R-EFE-15.** Vuelta al hogar (ya existe, S-013). Turno perdido: uno o dos (cantidad en datos), con el tope de datos (valor inicial 2); lo que exceda se convierte en Vuelta al hogar. Turno extra: uno o dos (cantidad en datos), con el tope que ya existe; se cancelan uno a uno con los perdidos (ya implementado). Un asiento en el Vacío no es objetivo válido de Turno extra (decisión del autor del 08/10/2026, GDD 05).
- **R-EFE-6, R-EFE-7.** Sello: el Warden sella una casilla válida (R-TAB-22); cuál, al azar entre las válidas con el sorteo que se le pase (el cerebro real elegirá en su story). Sello contra rival: la primera casilla sellable del camino más corto desde el rival hasta su objetivo más probable (la Fuente con reserva de un tipo que le falta más cercana; el Altar si tiene los seis tipos); si no hay casilla válida en ese camino, cualquier válida a distancia mínima de él.
- **R-EFE-8.** Marcado: el objetivo queda Marcado durante N rondas (datos, valor inicial 3); quien le perjudique con una acción dirigida (robarle, expulsarle, ganarle un duelo, mandarle al hogar o al Vacío con un efecto que él dirigió) gana un turno extra y la marca termina; marcar a un Marcado reinicia la duración; varias marcas conviven. Nadie se marca a sí mismo.
- **R-EFE-9, R-EFE-18.** Noche de los Ecos: los Ecos transitan por cualquier Dominio (permiso temporal de R-TAB-20) y pueden robar allí, pero no obtienen runas de Fuentes con dueño; termina con la Ley de Sombras o por un tope de rondas de datos (valor inicial 8). Ley de Sombras: todos los Ecos a su hogar; termina la Noche si estaba activa.
- **R-EFE-11.** Perder una runa: vuelve a su Fuente (R-RUN-6); variante en datos: al azar o a elección del objetivo con reloj corto (si no elige, al azar). Sin runas, no hay efecto. La afinidad de Koschei no se aplica: no es un robo.
- **R-EFE-14, R-EFE-16, R-EFE-17.** El Vacío y Liberación (ya existen, S-013). Runa regalada: el objetivo elige un tipo entre las Fuentes con reserva, con reloj corto, y la recibe; la reserva baja en uno; si no elige a tiempo, no recibe nada; sin Fuentes con reserva, el efecto no es válido. Un Eco puede recibir así runas de cualquier Dominio.
- **R-DIR-1.** Ocho direcciones: Activo (o el último que jugó, si el evento llega entre turnos), Rival señalado (el activo elige entre los válidos, con reloj corto), Dos rivales señalados, Líder (más tipos distintos; con empate, más runas; si persiste, todos los empatados), Siguiente, Todos, Al azar, Ecos.
- **R-DIR-2, R-DIR-3.** El motor filtra objetivos válidos antes de ofrecer la elección: en el Vacío solo Liberación; en su hogar no vale Vuelta al hogar; sin runas no vale Perder una runa, Revelar Brazal ni ser víctima de robo. Sin objetivos válidos, el efecto no se aplica y queda registrado como burla.
- **R-DIR-4.** Reloj agotado al elegir: un regalo (Runa regalada) se pierde, con burla; al señalar la víctima de un efecto dañino, el efecto cae sobre uno mismo (con "dos rivales señalados", contra un rival al azar); al elegir variante de un castigo propio (qué runa perder), al azar. En todos los casos sube la insolencia.
- **R-COM-1, R-COM-2, R-COM-3.** Un evento se compone de texto, forma de entrega (Warden directo u Oráculo; en esta story solo se aplica la directa) y de una a cuatro opciones (número en datos); cada opción, una lista ordenada de aplicaciones (efecto, dirección, cantidad). Una opción: decreto, se aplica sin elegir. Varias: dilema que el activo elige dentro de un reloj (base más incremento por opción; datos: 8 s y 3 s); agotado, el Warden resuelve en su contra: una opción al azar, sus efectos dañinos sobre el propio jugador, sus regalos descartados, insolencia. Las aplicaciones de una opción se resuelven en orden y cada una se valida por separado (R-DIR-2 y 3).
- **Registro**: cada aplicación de efecto emite un evento con efecto, dirección, objetivos y causa; marcas, revelaciones, Noche y Ley de Sombras también; los dilemas, su presentación, su elección o su resolución en contra.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-EFE-1** (R-EFE-0). El catálogo tiene exactamente 18 efectos, cada uno con gravedad y direcciones permitidas; un archivo de eventos con un efecto, dirección o cantidad fuera de lo permitido se rechaza al cargar, con motivo.
- **CA-EFE-2, 3, 4, 11** (R-EFE-2, 4, 5, 15). Revelar Brazal se ve y caduca por rondas, y se prolonga si ya estaba; Vuelta al hogar y sus objetivos no válidos; Turno perdido con tope que se convierte en Vuelta al hogar; Turno extra que cancela un perdido; Turno extra a quien está en el Vacío, no válido.
- **CA-EFE-5** (R-EFE-7). Sello contra rival sella una casilla válida del camino más corto entre el rival y su objetivo probable; sin casilla válida en el camino, una válida a distancia mínima; si el activo no elige a tiempo a quién, se aplica a él mismo.
- **CA-EFE-6** (R-EFE-8). Marcado se cobra con cada una de las acciones de la lista (turno extra al que la cobra y marca fuera); caduca por rondas sin cobrarse; dos Marcados conviven con duraciones propias; re-marcar reinicia; un efecto "al azar" o "al líder" no cobra la marca de nadie.
- **CA-EFE-8** (R-EFE-11). Perder una runa devuelve a la reserva; Koschei no aparta; la variante "a elección" con reloj, y al azar si no elige (con insolencia); sin runas, burla.
- **CA-EFE-12** (R-EFE-16). Runa regalada: elige y la recibe con la reserva una menos; sin Fuentes con reserva, no válido; sin elegir a tiempo, nada, burla e insolencia.
- **CA-EFE-13** (R-EFE-18, 9). Con la Noche activa un Eco alcanza Dominios con dueño y no obtiene runas en sus Fuentes; la Ley de Sombras lo manda a casa y lo confina de nuevo; el tope de rondas, igual.
- **CA-DIR-1** (R-DIR-1). Líder con sus dos desempates y el empate total (todos los empatados reciben el efecto).
- **CA-DIR-2** (R-DIR-2, 3). Dirección sin objetivos válidos: no se aplica y hay evento de burla. Un jugador en el Vacío no es objetivo de nada salvo Liberación.
- **CA-DIR-3** (R-DIR-4). Las tres formas de desatender, con insolencia en todas.
- **CA-COM-1** (R-COM-1 a 3). Los seis ejemplos de R-COM-3, escritos en `eventos.json`, se cargan y producen exactamente lo descrito, sin código específico para ninguno. (Los que usan Duelo forzado quedan como "pendiente de S-015" y lo dicen.)
- **CA-COM-2** (R-COM-2). Un dilema de cuatro opciones tiene reloj base más tres incrementos; agotado, una opción al azar, daños sobre el activo, regalos descartados, insolencia.
- **Entre turnos.** Un evento aplicado entre turnos con una elección pendiente (dilema, rival señalado, runa regalada, runa a perder) detiene la máquina hasta resolverse; el siguiente turno empieza después. Las elecciones pueden ser de un asiento que no es el activo (Runa regalada "al siguiente").
- **Efectos pendientes de story.** Aplicar Alto, Consulta forzada, Prueba del Warden, Robo decretado o Duelo forzado devuelve "no implementado" sin cambiar el estado ni el registro.
- **Registro** y **sorteo**: los eventos nuevos; las elecciones de los efectos no consumen el sorteo de los dados (el Sello y las direcciones "al azar" usan el sorteo que se les pase, el del Warden).
- **Simulación.** Sin cambios: el cerebro del spike no lanza eventos. Los 156 tests actuales siguen en verde.

Manuales, en PIE con paso a paso y comandos, con captura:

- **Decreto**: `SbEvent pierdes-un-turno` sobre un dummy: la crónica lo cuenta y el dummy salta su turno.
- **Dilema**: `SbEvent dos-turnos-o-casa` siendo el activo: la vista presenta las dos opciones con el reloj; eliges la B y señalas a quién mandar a casa; se ve volver a su Nicho. Repite sin contestar: te vas a casa tú y la crónica dice "insolencia".
- **Marcado y Noche**: `SbEvent marca-al-lider`; cobras la marca robando o expulsando y ganas un turno extra. `SbEvent noche`: un Eco entra en un Dominio con dueño; `SbEvent ley-de-sombras`: vuelve a casa.

## Notas para quien lo implemente

- Nombres del glosario: Efecto `Effect`, Dirección `EffectTarget`, Sello contra rival `SealAgainstRival`, Prueba del Warden `WardenTrial`, Noche de los Ecos `EchoNight`, Runa regalada `GiftRune`, Liberación `ReleaseVoid`, Consulta forzada `ForcedOracle`, Robo decretado `DecreedTheft`, Duelo forzado `ForcedDuel`, Marcado `Mark`, Ley de Sombras `ShadowLaw`, Alto `Halt`, Turno perdido `LoseTurn`, Turno extra `ExtraTurn`, Vuelta al hogar `SendHome`, el Vacío `Banish`, Revelar Brazal `RevealBracer`, Perder una runa `LoseRune`, Insolencia `Insolence`. Los códigos de la tabla son los identificadores en datos. Nombres nuevos (catálogo, aplicación, evento en curso, ronda, estado de marca y de revelación, tipos de elección, eventos del registro, archivo y cargador) se proponen en el plan y en el PR.
- **Catálogo en código, eventos en datos.** El catálogo (18 efectos, gravedad, direcciones permitidas) es una tabla fija del motor: la lista es cerrada por regla. Los eventos viven en `Content/Data/Events/eventos.json` con cargador y validación como los demás archivos: identificador, texto, entrega, opciones con sus aplicaciones. Esquema de ejemplo:

  ```json
  {"id": "dos-turnos-o-casa", "text": "Dos turnos extra para ti, o mandas a casa a quien señales",
   "delivery": "direct",
   "options": [
     [{"effect": "ExtraTurn", "target": "ActivePlayer", "amount": 2}],
     [{"effect": "SendHome", "target": "ChosenRival"}]
   ]}
  ```

  Los seis ejemplos de R-COM-3 van en el archivo con estos identificadores: `pierdes-un-turno`, `dos-turnos-o-casa`, `duelo-por-todo`, `noche`, `liberacion-y-a-casa`, `lider-ensena-y-pierde`. Más `marca-al-lider` y `ley-de-sombras` para las pruebas manuales.
- **Evento en curso** como estado del motor: el evento, la opción elegida (o pendiente), el índice de la aplicación que toca, y quién lo dirige (el activo). La máquina de turnos, en `BetweenTurns`, resuelve el evento en curso antes de empezar el siguiente turno; cada elección que haga falta (dilema, rival, runa regalada, runa a perder) es una elección pendiente (`FPendingChoice`, S-009) del asiento que corresponda, con su reloj: no un mecanismo aparte. Una función `ApplyEvent(State, EventId, Now, Random)` lo arranca; `AdvanceTurn` lo lleva.
- **Rondas**: contador en el estado que sube cada vez que el orden de turno da la vuelta (cuando empieza turno el primero del orden). Marcado, Brazal revelado y la salvaguarda de la Noche caducan por ese contador.
- **Marcado**: cobrarlo exige saber quién dirige una acción. Las acciones de jugador ya lo saben (`Seat` de la acción); los efectos dirigidos llevan al asiento que señaló. Un efecto "al azar" o "al líder" no lo dirige nadie.
- **Noche de los Ecos**: entra en `MakeMoverTraits` (el permiso temporal) y en `ResolvePowerSourceLanding` (sin runas en Fuentes con dueño).
- **Direcciones "al azar" y el Sello**: usan el sorteo que reciba la función (el anfitrión pasa el del Warden). Nada toca el sorteo de la partida.
- Comandos: `SbEvent <id>` (lanza el evento como entrega directa del Warden, entre turnos) y `SbEvents` (lista los identificadores cargados). Patrón de S-011, con evento de depuración.
- Vista: el evento en curso con su texto y, si es dilema, sus opciones numeradas; marcas y Brazal revelado en la leyenda de asientos; "NOCHE DE LOS ECOS" mientras dure; crónica. Solo lectura.
- Nada de `Engine` en `SoulboundCore`.

## Catálogo (R-EFE-0)

| # | Efecto | Código | Gravedad | Direcciones permitidas | En esta story |
|---|---|---|---|---|---|
| 1 | Alto | `Halt` | Leve | Activo | No (Warden) |
| 2 | Revelar Brazal | `RevealBracer` | Leve | Activo, rival señalado, líder, todos | Sí |
| 3 | Consulta forzada al Oráculo | `ForcedOracle` | Leve | Activo, rival señalado, siguiente | No (Warden) |
| 4 | Vuelta al hogar | `SendHome` | Medio | Activo, rival señalado, líder, siguiente, todos, al azar | Sí |
| 5 | Turno perdido | `LoseTurn` | Medio | Activo, rival señalado, líder, siguiente, al azar | Sí |
| 6 | Sello | `PlaceSeal` | Medio | Tablero (el Warden elige) | Sí |
| 7 | Sello contra rival | `SealAgainstRival` | Medio | Rival señalado, líder | Sí |
| 8 | Marcado | `Mark` | Medio | Rival señalado, líder, al azar | Sí |
| 9 | Ley de Sombras | `ShadowLaw` | Medio | Ecos | Sí |
| 10 | Prueba del Warden | `WardenTrial` | Medio | Activo, rival señalado, siguiente | No (S-015) |
| 11 | Perder una runa | `LoseRune` | Grave | Activo, rival señalado, líder, todos, al azar | Sí |
| 12 | Robo decretado | `DecreedTheft` | Grave | Activo roba a rival señalado, a todos o al azar | No (S-015) |
| 13 | Duelo forzado | `ForcedDuel` | Grave | Activo contra rival señalado; dos rivales señalados; dos al azar | No (S-015) |
| 14 | El Vacío | `Banish` | Grave | Activo, rival señalado, líder, al azar | Sí |
| 15 | Turno extra | `ExtraTurn` | Regalo | Activo, siguiente | Sí |
| 16 | Runa regalada | `GiftRune` | Regalo | Activo, siguiente, al azar | Sí |
| 17 | Liberación | `ReleaseVoid` | Regalo | Todos los del Vacío | Sí |
| 18 | Noche de los Ecos | `EchoNight` | Regalo | Ecos | Sí |

Direcciones: `ActivePlayer`, `ChosenRival`, `TwoChosenRivals`, `Leader`, `NextPlayer`, `Everyone`, `RandomPlayer`, `Echoes`.

## Fuera de esta story

- Prueba del Warden, Robo decretado, Duelo forzado: S-015.
- Alto, Consulta forzada, entrega por Oráculo, votaciones, cuándo y a quién lanza cada evento, insolencia con castigos, Furia: Warden (S-016, S-017).
- Los 38 eventos del GDD 06: S-016 (aquí solo los ocho de prueba).
- Bots que eligen opciones y objetivos con criterio: GDD 08.
- Presentación (cartas de piedra, foco de evento, marcas flotantes): interfaz.
