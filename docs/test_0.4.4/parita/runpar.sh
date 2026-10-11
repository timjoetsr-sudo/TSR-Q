#!/bin/bash
# uso: runpar.sh base|new  -> esegue tutte le suite di parita JS = C++ per quel motore
V=$1; P=/home/claude/lab/par; cd $P
if [ $V = base ]; then CORE=/home/claude/lab/base043/dsp.js; else CORE=/home/claude/lab/new/dsp_core.js; fi
export CORE DUMP=$P/dump_$V DUMP2=$P/dump2_$V TAG=$V
mkdir -p $P/log_$V
for s in js_master js_new js_rand js_stems js_peak; do node $s.js > log_$V/$s.log 2>&1; echo "$s exit $?" >> log_$V/$s.log; done
