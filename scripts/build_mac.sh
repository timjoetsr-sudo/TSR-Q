#!/bin/bash
# TSR Q · build + test + pacchetto per macOS (TSR Audio)
# Uso:  ./scripts/build_mac.sh
# Facoltativo (distribuzione senza avvisi di Gatekeeper):
#   DEV_ID_APP="Developer ID Application: TSR Audio (TEAMID)"
#   DEV_ID_INSTALLER="Developer ID Installer: TSR Audio (TEAMID)"
#   NOTARY_PROFILE="tsr-notary"   (creato con: xcrun notarytool store-credentials tsr-notary ...)
# Facoltativo (Pro Tools): AAX_SDK_PATH=/percorso/aax-sdk  → costruisce anche il formato AAX
#   (per Pro Tools normale l'AAX va poi firmato con PACE/wraptool: account PACE Eden necessario)
set -euo pipefail
cd "$(dirname "$0")/.."
VER=$(sed -n 's/^project(TSRQ VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
OUT="dist/TSR-Q-$VER-mac"; rm -rf "$OUT"; mkdir -p "$OUT"
command -v cmake >/dev/null || { echo "ERRORE: serve CMake (brew install cmake)"; exit 1; }
xcode-select -p >/dev/null || { echo "ERRORE: servono gli Xcode Command Line Tools (xcode-select --install)"; exit 1; }

echo "== 1/6 configurazione (universale arm64 + x86_64, macOS 11+) =="
cmake -B build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 ${JUCE_DIR:+-DJUCE_DIR="$JUCE_DIR"} ${AAX_SDK_PATH:+-DAAX_SDK_PATH="$AAX_SDK_PATH"}
AAX=""; [ -n "${AAX_SDK_PATH:-}" ] && AAX="TSRQ_AAX"

echo "== 2/6 build AU, VST3, Standalone, test =="
cmake --build build-mac --config Release --target TSRQ_AU TSRQ_VST3 TSRQ_Standalone $AAX tsrq_dsp_tests tsrq_stem_tests -- -quiet
A=build-mac/TSRQ_artefacts/Release
lipo -info "$A/AU/TSR Q.component/Contents/MacOS/TSR Q"

echo "== 3/6 test del motore =="
"build-mac/Release/tsrq_dsp_tests" | tee "$OUT/test_dsp.txt"
if [ -d "${STEMS_DIR:-}" ]; then "build-mac/Release/tsrq_stem_tests" "$STEMS_DIR" | tee "$OUT/test_stem.txt"; else echo "(test sugli stem saltato: imposta STEMS_DIR con i file .f32)"; fi

echo "== 4/6 firma =="
SIGN="${DEV_ID_APP:--}"     # senza Developer ID: firma ad-hoc (va bene sul tuo Mac)
BUNDLES=("$A/AU/TSR Q.component" "$A/VST3/TSR Q.vst3" "$A/Standalone/TSR Q.app"); [ -n "$AAX" ] && BUNDLES+=("$A/AAX/TSR Q.aaxplugin")
for b in "${BUNDLES[@]}"; do
  codesign --force --deep --options runtime --timestamp=none -s "$SIGN" "$b"; codesign --verify --deep --strict "$b"; done

echo "== 5/6 validazione nei formati Apple =="
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/Components/"TSR Q.component" ~/Library/Audio/Plug-Ins/VST3/"TSR Q.vst3"
cp -R "$A/AU/TSR Q.component" ~/Library/Audio/Plug-Ins/Components/
cp -R "$A/VST3/TSR Q.vst3" ~/Library/Audio/Plug-Ins/VST3/
killall -9 AudioComponentRegistrar 2>/dev/null || true; sleep 2
auval -v aufx Tsrq Tsra | tee "$OUT/auval.txt" | tail -3
grep -q "AU VALIDATION SUCCEEDED" "$OUT/auval.txt" || { echo "ERRORE: auval non superato"; exit 1; }
if command -v pluginval >/dev/null || [ -x /Applications/pluginval.app/Contents/MacOS/pluginval ]; then
  PV=$(command -v pluginval || echo /Applications/pluginval.app/Contents/MacOS/pluginval)
  "$PV" --strictness-level 10 --validate-in-process "$A/VST3/TSR Q.vst3" | tee "$OUT/pluginval_vst3.txt" | tail -2
  "$PV" --strictness-level 10 --validate-in-process "$A/AU/TSR Q.component" | tee "$OUT/pluginval_au.txt" | tail -2
else echo "(pluginval non trovato: https://github.com/Tracktion/pluginval/releases)"; fi

echo "== 6/6 installer .pkg + zip =="
STG=$(mktemp -d); mkdir -p "$STG/au" "$STG/vst3" "$STG/app"
cp -R "$A/AU/TSR Q.component" "$STG/au/"; cp -R "$A/VST3/TSR Q.vst3" "$STG/vst3/"; cp -R "$A/Standalone/TSR Q.app" "$STG/app/"
pkgbuild --root "$STG/au"   --identifier com.tsraudio.tsrq.au   --version "$VER" --install-location "/Library/Audio/Plug-Ins/Components" "$STG/au.pkg"
pkgbuild --root "$STG/vst3" --identifier com.tsraudio.tsrq.vst3 --version "$VER" --install-location "/Library/Audio/Plug-Ins/VST3" "$STG/vst3.pkg"
pkgbuild --root "$STG/app"  --identifier com.tsraudio.tsrq.app  --version "$VER" --install-location "/Applications" "$STG/app.pkg"
AAXPKG=()
if [ -n "$AAX" ]; then mkdir -p "$STG/aax"; cp -R "$A/AAX/TSR Q.aaxplugin" "$STG/aax/"
  pkgbuild --root "$STG/aax" --identifier com.tsraudio.tsrq.aax --version "$VER" --install-location "/Library/Application Support/Avid/Audio/Plug-Ins" "$STG/aax.pkg"; AAXPKG=(--package "$STG/aax.pkg"); fi
productbuild --package "$STG/au.pkg" --package "$STG/vst3.pkg" --package "$STG/app.pkg" ${AAXPKG[@]+"${AAXPKG[@]}"} ${DEV_ID_INSTALLER:+--sign "$DEV_ID_INSTALLER"} "$OUT/TSR Q $VER (TSR Audio).pkg"
if [ -n "${NOTARY_PROFILE:-}" ] && [ -n "${DEV_ID_INSTALLER:-}" ]; then
  xcrun notarytool submit "$OUT/TSR Q $VER (TSR Audio).pkg" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$OUT/TSR Q $VER (TSR Audio).pkg"; fi
( cd "$STG" && zip -qry "$OLDPWD/$OUT/TSR-Q-$VER-mac-plugins.zip" au vst3 app )
echo; echo "FATTO → $OUT"; ls -la "$OUT"
