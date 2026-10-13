#!/bin/bash
# TSR Q · eseguito dall'installer PRIMA di installare: elimina ogni versione gia installata (sistema e utenti), cosi resta solo quella nuova.
# Uso: rimuovi_vecchio.sh au|vst3|app   (ogni componente del pacchetto toglie il proprio formato)
R="${TSRQ_ROOT:-}"; LOG=/tmp/tsrq_installer.log; echo "$(date) TSR Q preinstall $1" >> "$LOG"
del () { if [ -e "$1" ]; then rm -rf "$1" && echo "  eliminato: $1" >> "$LOG"; fi; }
for H in "$R"/Users/*; do [ -d "$H/Library" ] || continue
  case "$1" in
    au)   del "$H/Library/Audio/Plug-Ins/Components/TSR Q.component" ;;
    vst3) del "$H/Library/Audio/Plug-Ins/VST3/TSR Q.vst3" ;;
    app)  del "$H/Applications/TSR Q.app" ;;
  esac
done
case "$1" in
  au)   del "$R/Library/Audio/Plug-Ins/Components/TSR Q.component"
        for H in "$R"/Users/*; do del "$H/Library/Caches/AudioUnitCache"; del "$H/Library/Preferences/com.apple.audio.InfoHelper.plist"; done ;;
  vst3) del "$R/Library/Audio/Plug-Ins/VST3/TSR Q.vst3" ;;
  app)  del "$R/Applications/TSR Q.app" ;;
esac
exit 0
