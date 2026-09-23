#!/bin/sh
# Filtro "clean" de Git para Config/DefaultEngine.ini: vacía las claves de EOS y de Redpoint
# antes de que el archivo entre en el repositorio. Los valores reales viven en Config/Local/EOS.ini (ignorado).
# Se activa una vez por clon con:  sh Scripts/eos-secrets/install.sh
sed -E 's/^((ProductId|SandboxId|DeploymentId|ClientId|ClientSecret|FreeEditionLicenseKey|TitleStorageClientId|TitleStorageClientSecret|DedicatedServerClientId|DedicatedServerClientSecret|TrustedClientPublicKey)=).*/\1/'
