#!/bin/sh
# Filtro "smudge" de Git para Config/DefaultEngine.ini: al hacer checkout, rellena las claves de EOS
# y de Redpoint con los valores de Config/Local/EOS.ini (ignorado por Git; una línea Clave=Valor por clave).
# Si el archivo local no existe, deja el contenido tal cual (claves vacías).
raiz="$(git rev-parse --show-toplevel 2>/dev/null)"
local="$raiz/Config/Local/EOS.ini"
if [ ! -f "$local" ]; then
  cat
  exit 0
fi
expr=""
while IFS= read -r linea || [ -n "$linea" ]; do
  linea="$(printf '%s' "$linea" | tr -d '\r')"
  # Solo líneas Clave=Valor donde la clave son letras (descarta comentarios, secciones, BOM y líneas vacías).
  case "$linea" in
    [A-Za-z]*=*) ;;
    *) continue ;;
  esac
  clave="${linea%%=*}"
  valor="${linea#*=}"
  case "$clave$valor" in
    *[!A-Za-z0-9._-]*) echo "eos-secrets: la clave '$clave' tiene caracteres no permitidos (solo letras, cifras, . _ -); se ignora" >&2; continue ;;
  esac
  expr="$expr -e s|^$clave=.*|$clave=$valor|"
done < "$local"
if [ -z "$expr" ]; then
  cat
else
  sed $expr
fi
