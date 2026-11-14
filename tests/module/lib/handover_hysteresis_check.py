#!/usr/bin/env python3
#
# Checks the run of a HandoverHysteresis_*.test: the UE hands over from gnb to
# gnb2 in the first round of beacons in which gnb2's beacon is received
# stronger than gnb's by more than the hysteresis, and not before.
#
# usage: handover_hysteresis_check.py <hysteresis in dB> <reception trace> <vector file>
#
# Prints one PASS line, or FAIL lines that say what was wrong.
#
import re
import sys

TOLERANCE = 0.4       # dB; the difference grows by less than 0.2 dB per round
HANDOVER_TIME = 0.05  # s, from the trigger to the new serving cell (handoverLatency)

def beacon_rounds(trace_file):
    """time -> {cell name: mean SINR of its beacon in dB}"""
    rounds = {}
    line_re = re.compile(r'^(\S+) WI BEACON (\S+)>\S+ .* values=\[([^\]]*)\]')
    for line in open(trace_file):
        m = line_re.match(line)
        if not m:
            continue
        time, cell, values = float(m.group(1)), m.group(2), m.group(3)
        total = count = 0
        for run in values.split(','):
            bands, value = run.split(':')
            first, _, last = bands.partition('-')
            n = int(last or first) - int(first) + 1
            total += n * float(value)
            count += n
        rounds.setdefault(time, {})[cell] = total / count
    return rounds

def handover_time(vector_file, cell_id):
    """when the NR leg's serving cell becomes cell_id, or None"""
    vector_id = None
    for line in open(vector_file):
        fields = line.split()
        if len(fields) >= 4 and fields[0] == 'vector' and fields[2].endswith('.nrConnectionControl') and fields[3].startswith('servingCell'):
            vector_id = fields[1]
        elif vector_id is not None and len(fields) == 4 and fields[0] == vector_id and float(fields[3]) == cell_id:
            return float(fields[2])
    return None

def main():
    hysteresis = float(sys.argv[1])
    rounds = beacon_rounds(sys.argv[2])
    handover = handover_time(sys.argv[3], 2)
    failures = []
    if handover is None:
        failures.append("the UE did not hand over to gnb2")
    else:
        trigger = handover - HANDOVER_TIME
        times = sorted(t for t in rounds if 'gnb' in rounds[t] and 'gnb2' in rounds[t])
        before = [t for t in times if t <= trigger + 1e-3]
        if not before:
            failures.append("no round of beacons before the handover at %g s" % handover)
        else:
            trigger_round = before[-1]
            difference = lambda t: rounds[t]['gnb2'] - rounds[t]['gnb']
            for t in before[:-1]:
                if difference(t) > hysteresis + TOLERANCE:
                    failures.append("at %g s gnb2's beacon was %.2f dB above gnb's, more than the hysteresis of %g dB, and no handover was triggered until %g s"
                                    % (t, difference(t), hysteresis, trigger_round))
                    break
            if difference(trigger_round) < hysteresis - TOLERANCE:
                failures.append("the handover was triggered at %g s, when gnb2's beacon was %.2f dB above gnb's, less than the hysteresis of %g dB"
                                % (trigger_round, difference(trigger_round), hysteresis))
            if not failures:
                print("PASS: handover triggered at %g s, gnb2's beacon %.2f dB above gnb's, hysteresis %g dB"
                      % (trigger_round, difference(trigger_round), hysteresis))
    for failure in failures:
        print("FAIL: " + failure)

main()
