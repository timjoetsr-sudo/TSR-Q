#!/bin/bash
# suite di interfaccia sul prototipo 0.4.5 (porta 8771): come 0.4.4, senza i test delle funzioni tolte su richiesta (etichette dei picchi, Spectrum Grab, Freeze) + test nuovi 0.4.5
cd /home/claude/lab/web/uitest; mkdir -p log045
for t in ui_all ui_new_045 ui_master_045 ui_clip ui_bar; do timeout 900 python3 $t.py > log045/$t.log 2>&1; echo "exit $?" >> log045/$t.log; done
timeout 300 python3 ui_045.py http://localhost:8771/TSR_Q_test_ESO_SAFE.html > log045/ui_045.log 2>&1; echo "exit $?" >> log045/ui_045.log
