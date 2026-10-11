#!/bin/bash
# batteria finale prima/dopo: C++ (base = 0.4.3, new = 0.4.4) e JS. Fase A in parallelo (2 core), fase B (tempi) in sequenza, alternando i motori.
L=/home/claude/lab; cd $L; export TSRQ_CLIP=$L/eso.f32
for v in base new; do I=$L/base043; D=""; [ $v = new ] && { I=$L/new; D="-DNEWENG"; }
  g++ -O2 -std=c++17 -ffp-contract=off $D -I $I lab.cpp -o fin_lab_$v || exit 1
  for x in dbg2 dbg4 dbg7d cpuchar; do g++ -O2 -std=c++17 -ffp-contract=off -I $I $x.cpp -o fin_${x}_$v || exit 1; done
  mkdir -p res/final/$v; done
phaseA () { v=$1; O=res/final/$v
  ./fin_lab_$v precision $O/precision.csv; ./fin_lab_$v precisionx $O/precisionx.csv; ./fin_lab_$v recip $O/recip.csv
  ./fin_lab_$v stability > $O/stability.json; ./fin_lab_$v block > $O/block.json; ./fin_lab_$v nan > $O/nan.json; ./fin_lab_$v extreme > $O/extreme.json
  for s in 1 2 3; do ./fin_lab_$v fuzz $s 60; done > $O/fuzz.json
  DYNLOG=1 ./fin_lab_$v dynacc > $O/dynacc.json 2> $O/dynacc_cases.txt; ./fin_lab_$v sigverify res/base/verify_cases.csv > $O/sigverify.json
  ./fin_dbg2_$v > $O/detector_cal.txt; ./fin_dbg4_$v > $O/detector_band.txt
  if [ $v = base ]; then { echo "# soglia nominale"; ./fin_dbg7d_$v 0 0; echo "# stesso punto di lavoro (soglie +2.9 RMS / +6.02 Peak)"; ./fin_dbg7d_$v 2.9 6.02; } > $O/dyn_residue.txt; else ./fin_dbg7d_$v 0 0 > $O/dyn_residue.txt; fi
  echo "$v fase A finita" ; }
phaseA base > res/final/phaseA_base.log 2>&1 & phaseA new > res/final/phaseA_new.log 2>&1 & wait
for v in base new; do O=res/final/$v; ./fin_lab_$v designgrid > $O/designgrid.json; ./fin_lab_$v designgridfast > $O/designgridfast.json; ./fin_lab_$v designcost > $O/designcost.json; done
for v in base new; do O=res/final/$v; ./fin_lab_$v cpuworst 5 > $O/cpuworst.json; done
for v in base new; do O=res/final/$v; ./fin_lab_$v cpuauto 5 > $O/cpuauto.json; ./fin_lab_$v cpuautohf 5 > $O/cpuautohf.json; ./fin_lab_$v denormal > $O/denormal.json; ./fin_cpuchar_$v > $O/cpuchar.txt; done
for v in base new; do O=res/final/$v; ./fin_lab_$v cpu 5 > $O/cpu.json; done
for v in base new; do O=res/final/$v; C=$L/base043/dsp.js; [ $v = new ] && C=$L/new/dsp_core.js
  for i in 0 1 2 3 4 5 6 7 8 9; do node lab.js $C cpu1 $i; done > $O/cpu_js.jsonl; node lab.js $C setstate > $O/setstate_js.json; node lab.js $C nan > $O/nan_js.json; done
echo FINITO > res/final/DONE
