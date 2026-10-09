#!/bin/bash
# aggiorna l'interfaccia del plugin dal prototipo HTML (stessa interfaccia del browser)
# uso: ./scripts/build_web.sh percorso/TSR_Q_09_ceramica_smart.html
set -euo pipefail; cd "$(dirname "$0")/.."
python3 - "$1" <<'PY'
import re,sys
s=open(sys.argv[1],encoding='utf-8').read()
s=re.sub(r'<link rel="stylesheet" href="https://fonts.googleapis.com[^>]*>','',s)   # nel plugin niente rete
open('web/index.html','w',encoding='utf-8').write(s)
PY
python3 tools/html_ascii.py web/index.html web/index.html
