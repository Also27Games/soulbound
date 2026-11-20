# ADR-004. Secretos de EOS fuera del repositorio con filtro de Git y hook

**Estado**: aceptado el 23/09/2026 (spike, paso 0.4). **Relacionado**: ADR-003.

## Contexto

El repositorio del TFM es público por exigencia de las reglas y, además, es el repositorio del producto. Redpoint lee sus identificadores solo de `Config/DefaultEngine.ini` (no de la capa de usuario `UserEngine.ini`, probado) y además escribe en ese archivo sin preguntar (clave de licencia al activarse; clave de cifrado si está vacía). Git no olvida: un secreto committeado una vez queda en el histórico.

## Decisión

- Los seis valores que no deben publicarse (`ProductId`, `SandboxId`, `DeploymentId`, `ClientId`, `ClientSecret`, `FreeEditionLicenseKey`) viven en `Config/Local/EOS.ini`, ignorado por Git.
- Un **filtro clean/smudge de Git** sobre `Config/DefaultEngine.ini` (`Scripts/eos-secrets/`): al hacer checkout inyecta los valores en el archivo de trabajo; al hacer commit los vacía. En disco lleno (editor, empaquetado y Redpoint contentos), en Git vacío, y `git status` limpio. Se activa por clon con `sh Scripts/eos-secrets/install.sh`.
- Un **hook de pre-commit** (`Scripts/git-hooks/`, activado por el mismo script) rechaza cualquier commit con esas claves con valor y cualquier archivo prohibido (`Config/User*.ini`, `Build/NoRedist/`, plugins y contenido de terceros, material de referencia de terceros).
- `PlayerDataEncryptionKey` **se versiona a propósito**: solo cifra Player Data Storage y Title Storage (no usados en la Fase 1), va dentro de cada binario como el `ClientSecret`, y Redpoint la regenera si falta.

## Alternativas descartadas

- `Config/UserEngine.ini`: Unreal la lee, Redpoint no, y el empaquetado tampoco.
- Committear los identificadores por ser "públicos de facto" (van en cada binario): defendible para cuatro de ellos, no para la clave de licencia personal; y el mecanismo cuesta lo mismo para seis que para una.
- Repositorio privado: las reglas del TFM lo admiten solo justificado, y no resuelve el histórico si algún día se abre.

## Consecuencias

- Puesta en marcha en cada clon: instalar el filtro, rellenar `Config/Local/EOS.ini`, regenerar el archivo de trabajo (documentado en el README).
- Redpoint puede seguir reescribiendo `DefaultEngine.ini`; el filtro y el hook lo absorben.
- Los valores admiten cualquier carácter salvo `|`, `&`, `\` y espacios (limitación del filtro, avisada por el propio script).

## Nota del 20/11/2026, al cierre del TFM

En dos repasos del historial completo (6/11 y 20/11/2026) ninguna de las seis claves ha llegado a un commit con valor.
