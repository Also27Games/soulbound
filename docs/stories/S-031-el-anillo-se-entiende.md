# S-031. El anillo se entiende: dónde pulsar y cuánto falta

**Sistema**: minijuego del anillo (S-026, R-MIN-2, R-MIN-4) e interfaz en plano (GDD 07; pilar 3, caos legible). **Módulos**: `Soulbound` (presentación: `Presentation/RingPanel`, el modelo del panel en la vista y una función pura). `SoulboundCore` no cambia: ni la secuencia, ni la precisión, ni el criterio. **Rama base**: `main`, con S-027 fusionada. **Tamaño**: pequeño.

## Contexto

El panel del anillo de S-026 pinta la runa como dos aros azules, la zona buena como una línea verde fina y casi transparente, y el aro que se cierra en blanco. Tras cada pulso enseña su precisión como un número de 0 a 1 y una marca de color. El autor, que conoce la regla, sigue sin ver claro dónde hay que pulsar; un probador lo verá menos. Y nadie sabe cuánto necesita para superar la prueba: el umbral (media de precisiones mayor o igual que el de la dificultad) no aparece en ninguna parte.

Decidido por el autor el 10/11/2026:

- **La zona buena se ve**: una franja verde ancha alrededor de la runa, y el aro cambia de color mientras está dentro.
- **El progreso se ve**: una barra que se llena con cada pulso y una marca en el objetivo. **Sin números ni frases**: ni "objetivo 195", ni "llevas 45", ni "necesitas 75 en cada pulso". El juego es de terror social, no un arcade de puntuación; la barra dice lo que hace falta.
- **La runa es una imagen**, no círculos: una sola textura provisional, que importa el autor.
- Sin sonido, sin tocar la secuencia ni los umbrales.

## Reglas a implementar (`Soulbound`, `Presentation/`)

- **R-VIS-14 (precisión). La franja de acierto.** La zona buena se pinta como una franja verde translúcida entre el borde de la runa y el radio en que el aro entra en la zona (el `ZoneFraction` que el panel ya calcula). La runa deja de ser dos aros azules: en el centro va **la imagen de una runa** (R-VIS-18) y el único aro de la pantalla es el que se mueve. El aro que se cierra es blanco fuera de la zona y **verde y más grueso** mientras el instante actual está dentro de la zona buena del pulso en curso, también en su mitad tardía, cuando el aro ya está sobre la runa. Si el pulso se resuelve o vence, vuelve a blanco con el siguiente.
- **R-VIS-19 (precisión). El contacto es la coincidencia.** Hoy el aro y la runa tienen grosor y sus bordes se tocan antes del instante de coincidencia: unos 0,07 s antes con el cierre de 1,6 s y 0,04 s con el de 0,9 s. La instrucción dice "cuando la toque", y en las actas del 10/11/2026 el autor pulsó entre 0,10 y 0,15 s antes en el anillo fácil y falló dos pruebas por centésimas. Desde esta story, **el borde interior del aro toca el borde de la runa exactamente en la coincidencia**: el radio que se pinta se calcula para eso con una función pura (`RingInnerRadius`, nombre propuesto: del cierre, el radio de la runa y el radio máximo). Después de la coincidencia, durante la media zona tardía, el aro se queda quieto sobre ese borde.
- **R-VIS-15 (precisión). Dentro de la zona, como dato.** Una función sin mundo dice si un instante está dentro de la zona buena del pulso en curso (`FRingTrialRun::IsInSweetZone(Now)`, nombre propuesto): verdadero entre la coincidencia menos media zona y la coincidencia más media zona del primer pulso sin resolver; falso en el anuncio, entre pulsos y con la prueba terminada. El panel la lee a través de su modelo (`FRingPanelModel`), igual que hoy lee el cierre.
- **R-VIS-16 (precisión). La barra de progreso.** Bajo el anillo, una barra horizontal. Su relleno es la suma de las precisiones resueltas dividida por el número de pulsos de la prueba: vacía al empezar, llena con todos los pulsos perfectos, y solo crece. Con el criterio de media y umbral (Fuente, Prueba del Warden, escape del Vacío), una marca vertical en el umbral de la dificultad (`PassThreshold`): la prueba se supera si al acabar la barra llega a la marca. En el duelo y en el desafío final (sin fallos) la barra no lleva marca. El relleno toma el color de la última precisión, como las marcas de los pulsos. Una función pura devuelve el relleno y, si la hay, la posición de la marca (`RingProgress`, nombre propuesto, junto al modelo del panel).
- **R-VIS-17 (precisión). Sin cifras.** El panel no enseña la precisión como número ni ningún texto de puntuación. Se conservan el título de la prueba, el contador "pulso k de N", las marcas de color de cada pulso y, al acabar, el resultado ("superada", "fallada"). En el duelo, al acabar, el panel no enseña "RENDIMIENTO" ni ninguna cifra: solo la barra y las marcas; quién gana lo dice el panel de decisión (R-DEC-10, S-033). La instrucción del anuncio pasa a "Haz clic cuando el aro entre en la franja verde".

- **R-VIS-18 (precisión). La imagen de la runa.** El panel pinta en el centro una textura, del tamaño del diámetro de la runa de hoy, teñida: con el color del Dominio de la runa cuando la prueba es de una Fuente, y con el azul de hoy en el resto. La textura se indica por configuración (`RingRuneTexture`, `FSoftObjectPath`, Config, en `DefaultGame.ini`). Si está vacía o el asset no existe, el panel pinta un disco relleno del mismo color y lo dice una vez en el registro: el código compila, pasa los tests y funciona sin el asset.
- **R-VIS-20 (precisión). El anillo, centrado en el hueco.** Añadido por el autor el 12/11/2026, visto en S-033: la capa del anillo (`RingOverlay`) y la del botón de volver al menú (`ReturnToMenuOverlay`) van con `HAlign_Center` y márgenes desiguales (16 a la izquierda, la columna de paneles más 16 a la derecha), y Slate las coloca media columna a la izquierda del centro del hueco (`AlignChild`, `LayoutUtils.h`). Se corrigen como la capa de la frase en S-033 (R-DEC-13): la capa ocupa el hueco libre y el contenido se centra dentro, sin márgenes laterales.

## Asset que pide esta story

Esta story **sí pide un asset**, uno solo, y lo hace el autor, no el agente:

- `Content/Art/Anillo/T_RunaAnillo.uasset`: textura de 512x512, un símbolo de runa **blanco sobre transparente** (para poder teñirlo), arte provisional generado con IA.
- Lo importa el autor en el editor (grupo de textura UI, compresión `UserInterface2D`, sin mipmaps, sin streaming) y lo sube en **un commit suyo aparte** del código. El PNG de origen no se versiona.
- El agente no crea, importa ni modifica el asset, ni por el MCP ni de otra forma. Deja `RingRuneTexture=/Game/Art/Anillo/T_RunaAnillo.T_RunaAnillo` en `DefaultGame.ini` y comprueba que sin el asset sale el disco.
- En el PR: la descripción textual del asset y la declaración de arte provisional generado con IA, como el resto del arte del proyecto.

## Criterios de aceptación

Automáticos, en `Soulbound`, sin mundo (`Private/Tests/RingTrialRunTest.cpp` y `Private/Tests/RingPanelModelTest.cpp`, nombres `Soulbound.Game.Presentation.Anillo.<QueSeComprueba>`):

- **CA-VIS-14** (zona, R-VIS-15). Con cierre de 1,2 s y zona de 0,18 (0,216 s), en el primer pulso: 0,2 s antes de la coincidencia, fuera; 0,1 s antes, dentro; en la coincidencia, dentro; 0,1 s después, dentro; 0,12 s después, fuera. En el anuncio, entre dos pulsos y con la prueba terminada, fuera.
- **CA-VIS-15** (barra, R-VIS-16). Con tres pulsos y precisiones 0,45 y 0,28 resueltas, el relleno es 0,73 / 3; sin ninguna, 0; con tres de 1, 1. El relleno nunca baja al añadir un pulso.
- **CA-VIS-16** (marca, R-VIS-16). Con el criterio de media y umbral 0,55, la marca está en 0,55; en el duelo y en el desafío final no hay marca.
- **CA-VIS-17** (misma regla, R-VIS-16). Para las precisiones de CA-MIN-12 (0,6 / 0,5 / 0,55 y 0,6 / 0,5 / 0,5 con umbral 0,55), "el relleno final llega a la marca" coincide con lo que devuelve `RingTrialPassed`: la barra no dice nada distinto del motor.

- **CA-VIS-18** (contacto, R-VIS-19). Con el cierre en 1 (la coincidencia), el borde interior del aro es igual al radio de la runa; con el cierre en 0, al radio máximo; y decrece sin saltos entre los dos. Pasada la coincidencia no baja del radio de la runa.

Manuales, por el autor, en PIE y en la build empaquetada, a 1024x768 y a 1920x1080, con captura:

- Diez anillos fáciles seguidos pulsando "al tocar": en el acta (S-030), el desfase medio de las pulsaciones queda dentro de ±0,06 s. Antes de esta story era de −0,13 s.

- La franja verde se distingue de la runa y del fondo; el aro se vuelve verde al entrar en ella y se ve el cambio también en la dificultad difícil.
- La barra crece tras cada pulso y la marca del objetivo se ve en Fuente, Prueba del Warden y Vacío; en un duelo (`SbDuel` no vale, lo fija; retar a un dummy) no hay marca.
- No queda ninguna cifra de precisión ni frase de puntuación en el panel.
- El panel del anillo queda centrado bajo el tablero, en el hueco a la izquierda de la columna, y a 1024x768 no se corta por la izquierda; el botón de volver al menú, igual.
- Con el asset importado, la runa se ve en el centro, nítida a los dos tamaños, teñida con el color del Dominio en una Fuente; con `RingRuneTexture` vacío sale el disco y el registro lo dice.
- Jugando diez anillos seguidos, el autor acierta más que antes o igual: si la franja distrae, se dice en el PR.

## Notas para quien lo implemente

- Todo está en `SRingPanel::OnPaint` y en `ABoardViewActor::RingPanelModel`; el panel ya recibe un modelo por fotograma con el reloj real. La franja es un `DrawCircle` grueso; la runa, un `FSlateDrawElement::MakeBox` con un pincel hecho de la textura (`FSlateImageBrush` o equivalente de la 5.8), que la vista mantiene vivo mientras exista el panel.
- `ZoneFraction` ya es media zona entre el cierre: el radio exterior de la franja es el `ZoneRadius` de hoy.
- La barra no duplica la regla: enseña la suma y el umbral que el motor ya usa, y CA-VIS-17 lo ata a `RingTrialPassed`.
- El panel vive dentro de la interfaz del editor en PIE y se pinta con el reloj, no con datos: comprobar que se repinta cada fotograma (el 9 y 10/11/2026 se midió que sí, 60 pintados por segundo).
- Tests primero, en rojo. El pintado solo tiene criterio manual.
- Al cerrar: la frase de la Ayuda `instruccion:anillo` y la línea de "Cómo se juega" que habla del anillo tienen que decir "la franja verde"; son datos del autor, se avisa en el PR.

## Fuera de esta story

- Los umbrales y las zonas de cada dificultad (`partida.json`): valores de playtest del autor.
- Cambiar la secuencia: pulsos previos de ritmo, cuenta atrás, trampas del Warden.
- Enseñar el rendimiento del rival en el duelo.
- Sonido y partículas.
- Una runa distinta por Dominio y el arte definitivo: llegan con el Brazal.
