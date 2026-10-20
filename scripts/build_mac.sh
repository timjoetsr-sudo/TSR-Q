#!/bin/bash
# TSR Q · build + test + pacchetto per macOS (TSR Audio)
# Uso:  ./scripts/build_mac.sh
# Facoltativo (distribuzione senza avvisi di Gatekeeper):
#   DEV_ID_APP="Developer ID Application: TSR Audio (TEAMID)"
#   DEV_ID_INSTALLER="Developer ID Installer: TSR Audio (TEAMID)"
#   NOTARY_PROFILE="tsr-notary"   (creato con: xcrun notarytool store-credentials tsr-notary ...)
# Formati: solo VST3 e AAX. AAX con l'SDK contenuto in JUCE 8; AAX_SDK_PATH=/percorso/aax-sdk per usarne un altro.
#   L'AAX firmato solo qui si carica in Pro Tools Developer; per Pro Tools normale va firmato con PACE/wraptool (account PACE Eden)
set -euo pipefail
cd "$(dirname "$0")/.."
VER=$(sed -n 's/^project(TSRQ VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
OUT="dist/TSR-Q-$VER-mac"; rm -rf "$OUT"; mkdir -p "$OUT"
command -v cmake >/dev/null || { echo "ERRORE: serve CMake (brew install cmake)"; exit 1; }
xcode-select -p >/dev/null || { echo "ERRORE: servono gli Xcode Command Line Tools (xcode-select --install)"; exit 1; }

echo "== 1/6 configurazione (universale arm64 + x86_64, macOS 11+) =="
cmake -B build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 ${JUCE_DIR:+-DJUCE_DIR="$JUCE_DIR"} ${AAX_SDK_PATH:+-DAAX_SDK_PATH="$AAX_SDK_PATH"}

echo "== 2/6 build VST3, AAX, test =="
cmake --build build-mac --config Release --target TSRQ_VST3 TSRQ_AAX tsrq_dsp_tests tsrq_stem_tests -- -quiet
A=build-mac/TSRQ_artefacts/Release
V3="$A/VST3/TSR Q.vst3"; AX="$A/AAX/TSR Q.aaxplugin"
[ -d "$V3" ] || { echo "ERRORE: manca $V3"; exit 1; }
[ -d "$AX" ] || { echo "ERRORE: manca $AX"; exit 1; }
lipo -info "$V3/Contents/MacOS/TSR Q"
lipo -info "$AX/Contents/MacOS/TSR Q"

echo "== 3/6 test del motore =="
"build-mac/Release/tsrq_dsp_tests" | tee "$OUT/test_dsp.txt"
if [ -d "${STEMS_DIR:-}" ]; then "build-mac/Release/tsrq_stem_tests" "$STEMS_DIR" | tee "$OUT/test_stem.txt"; else echo "(test sugli stem saltato: imposta STEMS_DIR con i file .f32)"; fi

echo "== 4/6 firma =="
SIGN="${DEV_ID_APP:--}"     # senza Developer ID: firma ad-hoc (va bene sul tuo Mac)
for b in "$V3" "$AX"; do
  codesign --force --deep --options runtime --timestamp=none -s "$SIGN" "$b"; codesign --verify --deep --strict "$b"; done

echo "== 5/6 validazione VST3 (pluginval; per l'AAX non esiste un validatore fuori da Pro Tools) =="
if command -v pluginval >/dev/null || [ -x /Applications/pluginval.app/Contents/MacOS/pluginval ]; then
  PV=$(command -v pluginval || echo /Applications/pluginval.app/Contents/MacOS/pluginval)
  "$PV" --strictness-level 10 --validate-in-process "$V3" | tee "$OUT/pluginval_vst3.txt" | tail -2
  grep -q "SUCCESS" "$OUT/pluginval_vst3.txt" || { echo "ERRORE: pluginval VST3 non superato"; exit 1; }
else echo "(pluginval non trovato: https://github.com/Tracktion/pluginval/releases)"; fi

echo "== 6/6 installer .pkg + zip (solo VST3 + AAX) =="
STG=$(mktemp -d); mkdir -p "$STG/vst3" "$STG/aax"
cp -R "$V3" "$STG/vst3/"; cp -R "$AX" "$STG/aax/"
# installazione pulita: ogni componente prima elimina la versione gia installata (sistema e utenti: scripts/pkg/*/preinstall)
# e non e "rilocabile": macOS non puo aggiornare una vecchia copia trovata altrove invece di installare nella cartella giusta
nonreloc () { pkgbuild --analyze --root "$1" "$2"; perl -0pi -e 's#<key>BundleIsRelocatable</key>\s*<(true|false)/>\s*##g; s#<key>RootRelativeBundlePath</key>#<key>BundleIsRelocatable</key><false/><key>RootRelativeBundlePath</key>#g' "$2"; cat "$2"
  if grep -A1 "BundleIsRelocatable" "$2" | grep -q "<true/>"; then echo "ERRORE: $2 ancora rilocabile"; exit 1; fi; grep -A1 "BundleIsRelocatable" "$2" | grep -q "<false/>" || { echo "ERRORE: BundleIsRelocatable assente in $2"; exit 1; }; }
nonreloc "$STG/vst3" "$STG/vst3.plist"; nonreloc "$STG/aax" "$STG/aax.plist"
pkgbuild --root "$STG/vst3" --identifier com.tsraudio.tsrq.vst3 --version "$VER" --component-plist "$STG/vst3.plist" --scripts scripts/pkg/vst3 --install-location "/Library/Audio/Plug-Ins/VST3" "$STG/vst3.pkg"
pkgbuild --root "$STG/aax"  --identifier com.tsraudio.tsrq.aax  --version "$VER" --component-plist "$STG/aax.plist"  --scripts scripts/pkg/aax  --install-location "/Library/Application Support/Avid/Audio/Plug-Ins" "$STG/aax.pkg"
PKG="$OUT/TSR Q $VER VST3-AAX (TSR Audio).pkg"
productbuild --package "$STG/vst3.pkg" --package "$STG/aax.pkg" ${DEV_ID_INSTALLER:+--sign "$DEV_ID_INSTALLER"} "$PKG"
if [ -n "${NOTARY_PROFILE:-}" ] && [ -n "${DEV_ID_INSTALLER:-}" ]; then
  xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$PKG"; fi
echo "== contenuto del pacchetto =="
pkgutil --payload-files "$STG/vst3.pkg" | head -3; pkgutil --payload-files "$STG/aax.pkg" | head -3
( cd "$STG" && zip -qry "$OLDPWD/$OUT/TSR-Q-$VER-mac-VST3-AAX.zip" vst3 aax )
echo; echo "FATTO → $OUT"; ls -la "$OUT"
