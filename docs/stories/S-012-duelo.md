# S-012. Duelo

**Sistema**: runas y poderes (GDD 04). **Módulos**: `SoulboundCore` y `Soulbound` (vista). **Rama base**: `main`. **Tamaño**: media. **Va después de S-011** (herramientas de depuración), que se usa para sus criterios manuales.

## Contexto

El robo del Eco y el trilero (S-010) ya mueven runas entre jugadores. Falta la forma que tienen los Elegidos de quitárselas entre sí: el **duelo**. Hay dos maneras de retar, el poder Duelo (a alguien de tu casilla) y el Nodo de Duelo (a cualquiera del tablero, tengas o no el poder), y un solo desenlace: el ganador roba una runa al perdedor con el trilero. El minijuego de duelo no existe aún; como en S-008, el motor espera un resultado y los decisores lo dan: cada participante "rinde" un valor y gana el mayor. Con esta story se completa la acción contra un jugador: un Elegido puede tener Duelo y Expulsión a la vez y elige una.

Decidido por el autor el 05/10/2026: (1) quién gana mientras no hay minijuego se decide por el rendimiento de cada decisor, con el margen de Shuten-dōji en datos; (2) el Nodo de Duelo no funciona hasta que acaba la carrera, igual que el robo del Eco; (3) las apuestas quedan aplazadas hasta después del 25/10.

## Reglas a implementar

- **R-POD-3.** Duelo portátil: quien tiene el poder Duelo y termina su movimiento en una casilla ocupada puede retar a uno de los que están en ella.
- **R-DUE-2.** Nodo de Duelo: acabada la carrera, quien termina su movimiento en él puede retar a cualquier jugador del tablero, esté donde esté, sin necesitar el poder. Vale para Elegidos y para Ecos. Durante la carrera es una casilla normal (decisión del autor).
- **R-INT-1, R-INT-2.** Primero el efecto de la casilla, después como mucho una acción contra un jugador: duelo portátil, reto a distancia, Expulsión o robo. Quien tiene varias elige una, contra un solo objetivo, o ninguna.
- **R-DUE-3.** Un duelo no se puede rechazar: el retado no elige nada.
- **R-DUE-4.** Si el retado no tiene runas, no hay duelo y la acción se pierde; el registro lo deja.
- **R-DUE-1, R-ELE-2 (Shuten-dōji).** El duelo enfrenta a retador y retado y devuelve siempre un ganador. Shuten-dōji tiene ventaja sea retador o retado, con un margen que sale de datos.
- **R-DUE-5.** El ganador roba una runa al perdedor con el trilero (S-010), sea retador o retado. Koschei perdedor aparta su runa como en cualquier robo.
- **R-DUE-6.** Si el perdedor no tiene runas (solo puede ser un retador sin runas desde un Nodo de Duelo), vuelve a su hogar; el registro lo deja.
- **R-INT-4.** El duelo no cambia turnos extra ni perdidos de nadie.
- **Registro**: reto, duelo sin runas, resultado del duelo y perdedor enviado a su hogar, con marca de tiempo y los dos asientos.

## Criterios de aceptación

Automáticos, en `SoulboundCore`:

- **CA-POD-1** (R-POD-3, 4; R-INT-2). Dado un Elegido con Duelo y Expulsión que termina en una casilla con dos jugadores, se le ofrecen las cuatro opciones (duelo o expulsión, con cada uno) y "nada"; elige una sola. Sin el poder Duelo y fuera de un Nodo de Duelo, no se ofrece el duelo.
- **CA-DUE-1** (R-DUE-2). Dado un asiento sin el poder Duelo, Elegido o Eco, que termina en un Nodo de Duelo acabada la carrera, se le ofrece retar a cada uno de los otros asientos, estén donde estén. Durante la carrera (aunque el tablero esté Despierto), o pasando por el Nodo sin acabar en él, no.
- **Nodo de Duelo ocupado con poder Duelo.** Dado un asiento con el poder en un Nodo de Duelo ocupado, cada objetivo aparece una sola vez como duelo, y hace una sola acción.
- **CA-DUE-6** (R-DUE-3). Dado un reto válido, el retado no recibe ninguna elección y el duelo empieza.
- **CA-DUE-2** (R-DUE-4). Dado un reto a un asiento sin runas, no hay duelo, nada cambia de mano, el registro lleva el evento y el turno termina.
- **CA-DUE-3** (R-DUE-5). Dado un duelo con ganador G y perdedor P con runas, empieza un trilero con G de ladrón y P de víctima, sea quien sea el retador; si G es el retado, la elección de esfera se le pide a él, que no tiene el turno. Al acabar el trilero termina el turno del retador.
- **CA-DUE-4** (R-DUE-6). Dado un retador sin runas que pierde, está en su hogar, no hay trilero y el registro lleva el evento. Si gana, roba con trilero.
- **CA-ELE-3** (R-ELE-2). Dados dos rendimientos iguales, gana Shuten-dōji, sea retador o retado; con el rival por delante en menos que el margen de datos, también; por delante en más que el margen, pierde. Entre dos asientos sin la afinidad, gana el mayor rendimiento, y el empate es del retado.
- **Koschei perdedor.** Dado Koschei que pierde un duelo con varias runas, aparta una antes del trilero; con una sola, el robo se anula.
- **CA-INT-1** (R-INT-1). Dado un Elegido con Duelo que termina en una Fuente ocupada, primero obtiene la runa y después se le ofrece el duelo.
- **CA-INT-3** (R-INT-4). Tras un duelo, los turnos extra y perdidos de los dos no cambian.
- **El sorteo de los dados no depende de los duelos**: como en S-010, con y sin duelos el consumo por turno es el mismo.
- **Registro.** Los eventos nuevos, con los dos asientos, dato y marca de tiempo.
- **Simulación.** Los dummies retan al azar entre sus opciones y rinden al azar con su generador. Determinista por semilla y bajo el segundo. El informe añade duelos por partida (objetivo del GDD: 3 a 8) y cuántos gana el retado.
- Los tests existentes siguen en verde.

Manuales, en PIE con paso a paso y las herramientas de S-011, con captura:

- **Retar desde un Nodo de Duelo.** Cayendo en el Nodo, la vista ofrece retar a cada asiento; el duelo se resuelve, la crónica dice quién gana, y sigue el trilero.
- **Duelo portátil.** Con la runa que da Duelo, cayendo donde hay otro, la vista ofrece duelo y, si tienes Expulsión, también expulsar.
- **Ganar siendo retado.** Un dummy te reta, ganas (con `SbDuel win`), y la vista te pide la esfera aunque no sea tu turno.

## Notas para quien lo implemente

- Nombres del glosario: Duelo (poder) `PowerDuel` (`EPower::Duel`), Nodo de Duelo (`EPowerNodeEffect::Duel`), acción contra un jugador `EPlayerAction` en `FPlayerActionOption`, Trilero `ShellGame` (`BeginSteal`), afinidad `EAffinity::DuelAdvantage`, Hogar `HomeTile`. Nombres nuevos (estado del duelo, fase, intención, función del decisor, parámetro del margen, eventos) se proponen en el plan y en el PR.
- **Acción**: el duelo entra en `PlayerActionOptions` como una acción más. Propón si el duelo portátil y el reto a distancia son una sola acción con objetivos distintos (preferible) o dos; el registro debe poder distinguir si el reto salió de un Nodo.
- **El duelo es una espera, como el minijuego de S-008**, pero de dos asientos: el motor guarda retador y retado, y espera el rendimiento de cada uno (una intención por participante, en cualquier orden); cuando tiene los dos, decide. El rendimiento es un número entre 0 y 1 que da `IDecider` con una función nueva; los dummies, al azar con su generador; para el humano responde el dummy del anfitrión, como en S-008, y la crónica dice "resuelto al azar". Sin plazo en esta story (`TODO(R-MIN)`).
- **Margen de Shuten-dōji**: parámetro en `partida.json` (`duelAdvantageMargin`, valor inicial 0,15, validado entre 0 y 1). Se suma a su rendimiento. Es cómo aplica la afinidad el doble de prueba; el minijuego real la aplicará como margen de acierto.
- **Botín**: reutiliza `BeginSteal` con el ganador de ladrón. El trilero ya admite un ladrón sin turno; al resolverse, termina el turno del asiento activo (el retador). No se toca `ShellGameRules` salvo que haga falta, y si hace falta se dice en el plan.
- Quien reta no ve el Brazal ajeno: los asientos sin runas son objetivos válidos y R-DUE-4 se resuelve al retar.
- Efecto de los Nodos: `EPowerNodeEffect::Duel` ya se sortea y se enseña en la etiqueta. El Oráculo sigue sin efecto.
- **Herramienta**: comando `SbDuel win|lose`, con el patrón de S-011, que fija el resultado del próximo duelo en que participe el humano.
- Vista: las opciones de la acción con su texto ("Duelo con el asiento 2", "Retar al asiento 3 (Nodo)", "Expulsar al asiento 2"); una línea para el duelo en curso y su resultado; crónica. Solo lectura.
- Nada de `Engine` en `SoulboundCore`.

## Fuera de esta story

- Apuestas y premio (R-DUE-7): después del 25/10.
- Duelo forzado por el Warden (R-DUE-8) y burlas: Warden.
- Inmunidad de quien está en el Vacío (R-INT-3): story del Vacío.
- Oráculo: Warden.
- Minijuego real de duelo, foco de evento: sus stories.
