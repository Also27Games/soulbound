# ADR-007. Unreal Engine 5.8

**Estado**: aceptado el 23/09/2026 (spike, paso 0). **Relacionado**: ADR-003.

## Contexto

El proyecto anterior estaba en 5.6. La recomendación inicial fue 5.7 (madura, una por debajo de la última). La Free Edition de Redpoint solo se distribuye para la última versión de Unreal (5.8 en septiembre de 2026) y exige actualizar cuando sale versión nueva. Epic anuncia 5.8 como la última versión mayor prevista de UE5 (5.9 solo "si hace falta"; después, UE6). 5.8 incluye el plugin experimental Unreal MCP.

## Decisión

Unreal **5.8.3** (último hotfix al decidir). Rider como IDE con el toolchain de Visual Studio 2022. SDK de .NET 10 global cuyo runtime coincida con el del motor (10.0.7 hoy), porque Redpoint recompila UnrealBuildTool con el SDK global.

## Alternativas descartadas

- 5.6: online funcionando sin tocar nada, pero licencia de Redpoint en zona gris y dos versiones de migración al terminar el TFM.
- 5.7: la única que exigía programar antes de saber nada (pasar `A27Online` al EOS del motor).

Decidió "cuándo cuesta equivocarse": con cero contenido, probar 5.8 costaba una tarde y volver, cinco minutos.

## Consecuencias

- Licencia de Redpoint limpia; sin deuda de migración al terminar el TFM; MCP disponible.
- Atadura "siempre a la última versión" mientras se use Redpoint gratuito; llevadera porque 5.8 es la última mayor prevista. La salida es el EOS del motor (ADR-003).
- 5.8 genera soluciones `.slnx` (ignoradas); Rider abre el `.uproject`. Compilación limpia del editor en menos de 2 minutos; empaquetado Win64 Development en 67 s (20 s incremental).
