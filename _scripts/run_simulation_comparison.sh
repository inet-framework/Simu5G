#!/bin/bash

set -e  # Exit on first error

# Configuration
SIM_PATH="simulations/nr/test_numerology"
SIM_CONFIG="MultiCell-CBR-DL"
SIMTIME_LIMIT=5s

run_simulation() {
    local commit=$1
    local results_suffix=$2

    echo "Processing commit: $commit"
    git checkout $commit
    make cleanall && make makefiles && make MODE=debug -j12
    cd $SIM_PATH
    simu5g_dbg -c $SIM_CONFIG -r 0 --sim-time-limit=$SIMTIME_LIMIT \
       -u Cmdenv --cmdenv-express-mode=false --cmdenv-log-simulation=false \
       --cmdenv-output-file='${resultdir}/cmdenv.out' --cmdenv-redirect-output=true --result-dir=$results_suffix
    cd - > /dev/null
}

# Run simulations
run_simulation "before" "results-before"
run_simulation "topic/maccid" "results-after"

# Diff the results
echo diff -u "$SIM_PATH/results-before/cmdenv.out"  "$SIM_PATH/results-after/cmdenv.out"
echo diff -u "$SIM_PATH/results-before/*.sca" "$SIM_PATH/results-after/*.sca"

echo "Done."
