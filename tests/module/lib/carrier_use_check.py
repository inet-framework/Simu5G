#!/usr/bin/env python3
#
# Checks the run of CarrierAggregationOneUe.test: in each direction, the UE's
# data is received on both carriers of its cell in about as many slots.
#
# usage: carrier_use_check.py <reception trace> <number of slots with traffic>
#
# Prints one PASS line per direction, or FAIL lines that say what was wrong.
#
import collections
import sys

MIN_SHARE_OF_OTHER = 0.8   # the less used carrier against the more used one
MIN_SHARE_OF_SLOTS = 0.6   # the more used carrier against the slots with traffic

def main():
    slots = int(sys.argv[2])
    receptions = {'DL': collections.Counter(), 'UL': collections.Counter()}
    for line in open(sys.argv[1]):
        fields = line.split()
        # <t> RX <tx>><rx> <MHz>MHz <RAT> <dir> ...
        if len(fields) > 5 and fields[1] == 'RX' and fields[5] in receptions:
            receptions[fields[5]][fields[3]] += 1
    for direction in ('DL', 'UL'):
        counts = receptions[direction]
        text = ', '.join('%s: %d' % (carrier, counts[carrier]) for carrier in sorted(counts)) or 'none'
        if len(counts) < 2:
            print("FAIL: %s: receptions on fewer than two carriers (%s)" % (direction, text))
        elif min(counts.values()) < MIN_SHARE_OF_OTHER * max(counts.values()):
            print("FAIL: %s: one carrier has less than %d%% of the other's receptions (%s)" % (direction, 100 * MIN_SHARE_OF_OTHER, text))
        elif max(counts.values()) < MIN_SHARE_OF_SLOTS * slots:
            print("FAIL: %s: no carrier is used in %d%% of the %d slots (%s)" % (direction, 100 * MIN_SHARE_OF_SLOTS, slots, text))
        else:
            print("PASS: %s receptions in %d slots (%s)" % (direction, slots, text))

main()
