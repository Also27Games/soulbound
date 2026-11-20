# ADR-006. Presupuesto de renderizado: Steam Deck a 60 FPS y renderizador móvil como límite

**Estado**: aceptado el 23/09/2026 (NFR del 21/09 y spike, paso 0.2). Pendiente de verificar en la Deck (paso 6).

## Contexto

Requisito no funcional: 60 FPS estables en Steam Deck (build de Windows bajo Proton, 1280x800). La Fase 3 apunta a VR autónoma (Quest), cuyo renderizador es el móvil forward. La Deck es el peor PC, no el techo. El proyecto se creó con el preajuste Maximum (Lumen, raytracing, Substrate, sombras virtuales, sin luz precalculada), y UGT impone Lumen por defecto desde sus ajustes guardados.

## Decisión

- Fijado en `Config/DefaultEngine.ini` antes del primer mapa: iluminación global y reflejos dinámicos a ninguno, Lumen por hardware fuera, raytracing fuera, sombras virtuales fuera, Nanite fuera, campos de distancia fuera, Substrate fuera, luz precalculada permitida.
- Toda técnica visual del diseño tiene que existir en el renderizador móvil forward: siluetas de fichas tapadas con material "siempre encima" y no con posproceso; el Vacío por parámetros de material y no por posproceso de pantalla completa; sin efectos de espacio de pantalla.
- `ForwardShading` y el antialiasing se deciden con la Deck delante (paso 6): forward con MSAA es lo natural para Deck y Quest, y cambiarlo después solo recompila shaders, no invalida contenido.
- Los valores de vídeo por defecto de UGT y su preset de Deck tienen que coincidir con esto (ADR-005).
- Se mide en la Deck en cada hito con `stat fps`; el criterio manda sobre cualquier lujo visual.

## Aclaración del 29/09/2026: el límite son las técnicas, no la calidad

Medido en la Deck el 29/09: 60 FPS fijos en el nivel del spike. Este ADR veta técnicas (Lumen, raytracing, Nanite, sombras virtuales, Substrate, posproceso de pantalla completa), no la calidad visual. Soulbound es casi estático (una sala, una mesa, cámara sentada): el caso más favorable para luz precalculada de calidad, texturas y materiales con relieve. Presupuestos orientativos, a verificar en la Deck con contenido real: hasta 50.000 triángulos por Dominio, unos 300.000 en escena, menos de 150 llamadas de dibujo (la guía de Meta para Quest 2 es de 750.000 a 1.000.000 de triángulos y 80 a 200 llamadas). La austeridad de las pruebas de arte viene de la producción (un autor, sin artista, arte generado por código), no del rendimiento. Dirección probada y descartada el mismo día: miniatura pintada con texturas generadas (el autor la encontró amable y "amateur"). **Dirección preferida para el tablero: low-poly facetado y oscuro, color por vértice, sin texturas** (prueba C, ronda 2), con la Cripta en realista a partir de assets del autor; a validar en Unreal con luz horneada. El realismo es viable por rendimiento en la Deck y, con oficio, en Quest; se descarta en la Fase 1 por producción.

## Alternativas descartadas

- Maximum ahora y recortar después: gratis hoy con cero contenido, días dentro de un mes (Substrate invalida materiales; la luz precalculada obliga a rehacer mapas).
- Lumen ligero de 5.8 (60 FPS en Switch 2): tentador, pero incompatible con la Quest y con el estilo (mesa con luz fija).

## Consecuencias

- Iluminación precalculada como norma; la luz dura y fija del diseño encaja con ello.
- El arte (vóxel o dibujado) se prueba con este presupuesto, no al revés.
- Story pendiente: ajustar los valores por defecto de UGT (ADR-005) y decidir forward y antialiasing tras el paso 6.

## Nota del 20/11/2026, al cierre del TFM

La medida en la Steam Deck es la del nivel de la prueba técnica (29/09/2026). Con el tablero real no se ha vuelto a medir; queda declarado como Fase 2. El arte entregado es un kit propio de piezas low-poly, generado por código en Blender, y placeholders generados con IA.
