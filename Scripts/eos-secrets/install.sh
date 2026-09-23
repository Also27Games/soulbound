#!/bin/sh
# Activa en este clon el filtro de secretos de EOS y el hook de pre-commit. Ejecutar una vez:
#   sh Scripts/eos-secrets/install.sh
# Después, crear Config/Local/EOS.ini a partir de Config/Local/EOS.ini.example y hacer:
#   git checkout -- Config/DefaultEngine.ini
# para que el filtro rellene los valores en el archivo de trabajo.
set -e
raiz="$(git rev-parse --show-toplevel)"
cd "$raiz"
git config filter.eos-secrets.clean  "sh Scripts/eos-secrets/clean.sh"
git config filter.eos-secrets.smudge "sh Scripts/eos-secrets/smudge.sh"
git config filter.eos-secrets.required true
git config core.hooksPath Scripts/git-hooks
mkdir -p Config/Local
if [ ! -f Config/Local/EOS.ini ]; then
  cp Scripts/eos-secrets/EOS.ini.example Config/Local/EOS.ini
  echo "Creado Config/Local/EOS.ini: rellena los valores (nunca entra en Git)."
fi
echo "Filtro eos-secrets y hook de pre-commit activados en $raiz"
