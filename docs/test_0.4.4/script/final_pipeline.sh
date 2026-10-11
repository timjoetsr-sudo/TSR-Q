#!/bin/bash
# pipeline finale 0.4.4 (motore v19): prototipo, interfaccia, plugin, test del plugin, misure prima/dopo, render A/B. Tutto in sequenza (misure di CPU senza disturbi).
set -u; L=/home/claude/lab; W=$L/web; FL=$L/final_logs; R=/home/claude/tsr-q; S2=/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad; Q=/tmp/claude-0/-home-claude/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad
mkdir -p $FL; log () { echo "$(date +%H:%M:%S) $*" >> $FL/PIPELINE.log; }
log "inizio"
# 1. prototipo e pagine
python3 $W/mk044.py $L/base043/proto.html $L/new/dsp_core.js $W/proto044.html >> $FL/PIPELINE.log 2>&1 || { log "ERRORE mk044"; exit 1; }
cp $W/proto044.html $W/srv044/TSR_Q_09_ceramica_smart.html
python3 $W/build_clip_page.py $S2 $W/srv044/TSR_Q_09_ceramica_smart.html $W/srv044/TSR_Q_test_ESO_SAFE.html >> $FL/PIPELINE.log 2>&1
python3 $W/build_ab_page.py $W/srv044/TSR_Q_test_ESO_SAFE.html $L/base043/dsp.js $W/ab/TSR_Q_ascolto_AB.html >> $FL/PIPELINE.log 2>&1 && cp $W/ab/TSR_Q_ascolto_AB.html $W/srv044/
log "pagine pronte"
# 2. repo + plugin Linux
cp $L/new/TsrqDsp.h $R/Source/TsrqDsp.h; (cd $R && ./scripts/build_web.sh $W/proto044.html >> $FL/PIPELINE.log 2>&1)
(cd $R && tar --exclude=.git --exclude='build*' --exclude=dist -cf - . | (cd /home/claude/TSRQ && tar -xf -))
(cd /home/claude/TSRQ && cmake --build build --config Release -j2 > $FL/build_linux_044.log 2>&1) || { log "ERRORE build"; exit 1; }
log "plugin compilato"
# 3. test dell'interfaccia (154) + pagina A/B
(cd $W/uitest && rm -rf log && ./run_ui.sh); (cd $W/uitest && timeout 400 python3 ui_ab.py > log/ui_ab.log 2>&1)
log "test interfaccia finiti"
# 4. test del plugin
export DISPLAY=:99
(cd /home/claude/TSRQ/build && ./tsrq_dsp_tests > $FL/dsp_tests_044.log 2>&1; ./tsrq_stem_tests $Q/stems > $FL/stem_tests_044.log 2>&1)
timeout 1500 /tmp/pv/pluginval --strictness-level 10 --validate-in-process "/home/claude/TSRQ/build/TSRQ_artefacts/Release/VST3/TSR Q.vst3" > $FL/pluginval_044.txt 2>&1; echo "exit $?" >> $FL/pluginval_044.txt
bash $Q/selftest.sh > /dev/null 2>&1; cp $Q/selftest.txt $FL/selftest_044.txt
g++ -O2 -std=c++17 -ffp-contract=off $Q/par/dump2.cpp -o $S2/clip/dump2 && (cd $S2/clip && timeout 3000 python3 clip_vst.py > $FL/clip_vst_044.log 2>&1)
(cd $S2/ext && timeout 3000 python3 confronto.py > $FL/confronto_044.log 2>&1)
log "test del plugin finiti"
# 5. parita JS = C++ (motore finale)
(cd $L/par && for f in dump dump2 sc_h coef; do g++ -O2 -std=c++17 -ffp-contract=off -I$L/new $f.cpp -o ${f}_new; done; rm -rf log_new sc_new; ./runsc.sh new; ./runpar.sh new; node coef.js $L/new/dsp_core.js 20000 > coef_new.log 2>&1)
log "parita finita"
# 6. misure prima/dopo: fase A solo sul nuovo motore (i risultati 0.4.3 sono deterministici e gia misurati), fase B (tempi) su entrambi, alternati
cd $L; export TSRQ_CLIP=$L/eso.f32
g++ -O2 -std=c++17 -ffp-contract=off -DNEWENG -I $L/new lab.cpp -o fin_lab_new; for x in dbg2 dbg4 dbg7d cpuchar; do g++ -O2 -std=c++17 -ffp-contract=off -I $L/new $x.cpp -o fin_${x}_new; done
O=res/final/new; ./fin_lab_new precision $O/precision.csv; ./fin_lab_new precisionx $O/precisionx.csv; ./fin_lab_new recip $O/recip.csv; ./fin_lab_new stability > $O/stability.json; ./fin_lab_new block > $O/block.json; ./fin_lab_new nan > $O/nan.json
./fin_lab_new extreme > $O/extreme.json; for s in 1 2 3; do ./fin_lab_new fuzz $s 60; done > $O/fuzz.json
DYNLOG=1 ./fin_lab_new dynacc > $O/dynacc.json 2> $O/dynacc_cases.txt; ./fin_lab_new sigverify res/base/verify_cases.csv > $O/sigverify.json; ./fin_dbg2_new > $O/detector_cal.txt; ./fin_dbg4_new > $O/detector_band.txt; ./fin_dbg7d_new 0 0 > $O/dyn_residue.txt
log "fase A finita"
for v in base new; do O=res/final/$v; ./fin_lab_$v designgrid > $O/designgrid.json; ./fin_lab_$v designgridfast > $O/designgridfast.json; ./fin_lab_$v designcost > $O/designcost.json; done
for v in base new; do O=res/final/$v; ./fin_lab_$v cpuworst 5 > $O/cpuworst.json; done
for v in base new; do O=res/final/$v; ./fin_lab_$v cpuauto 5 > $O/cpuauto.json; ./fin_lab_$v cpuautohf 5 > $O/cpuautohf.json; ./fin_lab_$v denormal > $O/denormal.json; ./fin_cpuchar_$v > $O/cpuchar.txt; done
for v in base new; do O=res/final/$v; ./fin_lab_$v cpu 5 > $O/cpu.json; done
for v in base new; do O=res/final/$v; C=$L/base043/dsp.js; [ $v = new ] && C=$L/new/dsp_core.js; for i in 0 1 2 3 4 5 6 7 8 9; do node lab.js $C cpu1 $i; done > $O/cpu_js.jsonl; node lab.js $C setstate > $O/setstate_js.json; node lab.js $C nan > $O/nan_js.json; done
(cd $L/jsopt && node hoverbench.js > $L/res/final/hover_ui.txt 2>&1)
log "fase B finita"
# 7. render A/B con i plugin veri
(cd $L/ab && python3 render_ab.py > $FL/render_ab.log 2>&1)
log "render A/B finiti"
log "FINE"
