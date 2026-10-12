#!/usr/bin/env python3
"""Compare two Cmdenv event-banner traces event by event.

usage: compare.py B.banners SPIKE.banners [context]

An event is (time, module path, message class, message name). Normalized:
- events at a module whose path ends in 'radio' (or 'nrRadio'): the arrival of
  a frame (B: simu5g::AirFrame from the medium; spike: WirelessSignal) and the
  end of its reception (B: the same AirFrame as a self-message; spike:
  receptionTimer) both become ('radio-rx'); a frame from the PHY stays
  AirFrame in both, so 'AirFrame' at a radio is ambiguous in B -- both B
  AirFrame events and both spike events map to 'radio-frame' and are compared
  by count and position only; transmissionTimer stays itself.
- module ids and event numbers are ignored.
"""
import re, sys
pat = re.compile(r"^\*\* Event #(\d+)\s+t=(\S+)\s+on (.*?) \(([^)]*)\)\s+in (\S+) \(")

def load(path):
    evs = []
    for line in open(path):
        m = pat.match(line)
        if not m:
            continue
        num, t, name, cls, mod = m.groups()
        leaf = mod.rsplit('.', 1)[-1]
        if leaf in ('radio', 'nrRadio'):
            if cls in ('simu5g::AirFrame', 'inet::physicallayer::WirelessSignal') or name == 'receptionTimer':
                cls, name = 'radio-frame', ''
        evs.append((t, mod, cls, name, int(num)))
    return evs

a, b = load(sys.argv[1]), load(sys.argv[2])
ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 5
n = min(len(a), len(b))
for i in range(n):
    if a[i][:4] != b[i][:4]:
        print(f"first difference at index {i} (B event #{a[i][4]}, spike event #{b[i][4]}):")
        for j in range(max(0, i - ctx), min(n, i + ctx + 1)):
            mark = '>>' if j == i else '  '
            print(f"{mark} B    {a[j][0]:>12} {a[j][1]:<55} {a[j][2]:<28} {a[j][3]}")
            print(f"{mark} SPK  {b[j][0]:>12} {b[j][1]:<55} {b[j][2]:<28} {b[j][3]}")
        sys.exit(1)
if len(a) != len(b):
    print(f"equal prefix of {n} events; lengths differ: B {len(a)}, spike {len(b)}")
    sys.exit(1)
print(f"identical: {n} events")
