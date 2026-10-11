#!/bin/bash
# scenari dinamici sulla clip (21): C++ -> o_*.f32, poi parita JS sugli stessi scenari
V=$1; P=/home/claude/lab/par; S2=/tmp/claude-0/-home-claude-tsr-q/9256c548-5213-5d65-8a81-a8df32b70ebe/scratchpad
if [ $V = base ]; then CORE=/home/claude/lab/base043/dsp.js; else CORE=/home/claude/lab/new/dsp_core.js; fi
O=$P/sc_$V; mkdir -p $O; N=$((48000*12))
SC="static drag solo_static solo_drag solo_q solo_toggle bypass_band used type slope place dyn gbypass character outstep instep polarity pan warm_static subtle_toggle sub_static"
for s in $SC; do $P/sc_h_$V $S2/clip/eso_lp.f32 $O/o_$s.f32 $N $s & done; wait
ODIR=$O node $P/sc_js.js $CORE $SC > $P/sc_$V.log 2>&1; echo "exit $?" >> $P/sc_$V.log
