#!/usr/bin/env python3
#
#                  Simu5G
#
# Copyright (C) 2026 OpenSim Ltd.
#
# This file is part of a software released under the license included in file
# "license.pdf". Please read LICENSE and README files before using it.
#
# Renders IDE-style sequence charts (module lifelines x time, message arrows)
# of sidelink communication from OMNeT++ eventlog (.elog) files, without
# needing the Eclipse-based IDE. See the showcase document for the recording
# commands that produce the input eventlogs.
#
# Usage: make-sequence-charts.py  (paths are relative to this script)

import os
import re

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def tokens(line):
    """Parse an elog line into (entrytype, {key: value})."""
    parts = line.split()
    entry = parts[0]
    kv = {}
    i = 1
    while i + 1 < len(parts) + 1 and i + 1 <= len(parts):
        key = parts[i]
        if i + 1 < len(parts):
            val = parts[i + 1]
            if val.startswith('"'):  # re-join quoted names
                j = i + 1
                while not parts[j].endswith('"'):
                    j += 1
                val = " ".join(parts[i + 1:j + 1]).strip('"')
                i = j - 1
            kv[key] = val
        i += 2
    return entry, kv


class Eventlog:
    def __init__(self, path):
        self.modules = {}       # id -> (name, parentId)
        self.sends = []         # (msgId, name, sendTime, srcModuleId)
        self.deliveries = {}    # msgId -> (time, moduleId)  (next delivery after the send)
        self.parse(path)

    def full_path(self, mid):
        parts = []
        while mid in self.modules:
            name, pid = self.modules[mid]
            parts.append(name)
            mid = pid
        return ".".join(reversed(parts))

    def parse(self, path):
        cur_time, cur_module = 0.0, None
        with open(path, errors="replace") as f:
            for line in f:
                if line.startswith("MC "):
                    _, kv = tokens(line)
                    self.modules[int(kv["id"])] = (kv["n"], int(kv.get("pid", -1)))
                elif line.startswith("E "):
                    _, kv = tokens(line)
                    cur_time = float(kv["t"])
                    cur_module = int(kv["m"])
                    msg = int(kv.get("msg", -1))
                    if msg >= 0:
                        self.deliveries.setdefault(msg, []).append((cur_time, cur_module))
                elif line.startswith("BS "):
                    _, kv = tokens(line)
                    self.sends.append((int(kv["id"]), kv.get("n", "?"), cur_time, int(kv.get("m", kv.get("sm", cur_module or -1)))))

    def arrows(self):
        """Yield (name, t1, srcModId, t2, dstModId) for every send with a delivery."""
        # a message that travels over several hops (or is rescheduled) produces
        # one send and one delivery per hop, both in chronological order:
        # pair the k-th send of a message with its k-th delivery
        send_index = {}
        for msgId, name, t1, src in self.sends:
            k = send_index.get(msgId, 0)
            send_index[msgId] = k + 1
            dl = self.deliveries.get(msgId, [])
            if k < len(dl) and dl[k][0] >= t1:
                t2, dst = dl[k]
                yield name, t1, src, t2, dst


def draw(log, rowmap_fn, row_labels, arrows_filter, t0, t1, title, outfile,  # noqa: E501
         highlight=("slAirFrame",), annotate=None):
    fig, ax = plt.subplots(figsize=(11, 0.62 * len(row_labels) + 1.6))

    for i, label in enumerate(row_labels):
        ax.axhline(i, color="#cccccc", linewidth=0.8, zorder=0)

    seen_labels = set()
    for name, ta, src, tb, dst in log.arrows():
        if not (t0 <= ta <= t1):
            continue
        ra, rb = rowmap_fn(src), rowmap_fn(dst)
        if ra is None or rb is None:
            continue
        if not arrows_filter(name, ra, rb):
            continue
        color = "#c0392b" if name in highlight else "#2c3e50"
        style = "-"
        if ra == rb:  # self-message: draw a small marker
            ax.plot([ta], [ra], marker="D", color="#7f8c8d", markersize=3, zorder=3)
            continue
        ax.annotate("", xy=((tb - t0) * 1000, rb), xytext=((ta - t0) * 1000, ra),
                    arrowprops=dict(arrowstyle="->", color=color, linewidth=1.1, linestyle=style),
                    zorder=4)
        # label roughly at the middle, once per (name, direction) pair to limit
        # clutter; up and down arrows get opposite label offsets
        key = (name, ra, rb)
        if key not in seen_labels:
            seen_labels.add(key)
            offset = 0.22 if rb < ra else -0.12
            ax.text(((ta + tb) / 2 - t0) * 1000, (ra + rb) / 2 + offset, name,
                    fontsize=7, color=color, ha="center", zorder=5)

    if annotate:
        span = (t1 - t0) * 1000
        for (tx, row, text) in annotate:
            x = (tx - t0) * 1000
            ha = "right" if x > 0.7 * span else "left"
            ax.text(x, row + 0.28, text, fontsize=8, color="#8e44ad", ha=ha, zorder=6)

    ax.set_yticks(range(len(row_labels)))
    ax.set_yticklabels(row_labels, fontsize=9)
    ax.set_ylim(len(row_labels) - 0.5, -0.7)  # top-down like the IDE
    ax.set_xlim(-0.2, (t1 - t0) * 1000 + 0.2)  # annotate() arrows do not autoscale
    ax.set_xlabel("time within the excerpt [ms]")
    ax.set_title(title, fontsize=11)
    ax.grid(True, axis="x", alpha=0.25)
    fig.tight_layout()
    fig.savefig(outfile, dpi=130)
    print("wrote", outfile)


def make_blindretx_chart():
    elog = os.path.join(HERE, "../../basic/results/Broadcast-Tr37885-BlindRetx/seqchart.elog")
    log = Eventlog(elog)

    # lifelines: sender protocol stack of ue[0], receiver stack of ue[1]
    rows = ["ue[0] app+IP", "ue[0] PDCP/RLC", "ue[0] slMac", "ue[0] slPhy",
            "ue[1] slPhy", "ue[1] slMac", "ue[1] PDCP/RLC", "ue[1] app+IP"]

    paths = {mid: log.full_path(mid) for mid in log.modules}

    def rowmap(mid):
        p = paths.get(mid, "")
        for k, ue in ((0, "ue[0]"), (4, "ue[1]")):
            if f".{ue}." not in p and not p.endswith(ue):
                continue
            if ".slPhy" in p:
                return k + 3 if k == 0 else k
            if ".slMac" in p:
                return k + 2 if k == 0 else k + 1
            if ".cellularNic" in p:
                return k + 1 if k == 0 else k + 2  # pdcp/rlc entities, muxes, ip2nic
            return k + 0 if k == 0 else k + 3  # app, transport, ipv4 ...
        return None

    def arrfilter(name, ra, rb):
        interesting = ("Alert", "slAirFrame", "LteMacSduRequest", "lteRlcFragment", "SlMacPdu")
        return any(name.startswith(i) for i in interesting)

    # window: chosen to show one TB whose initial transmission is lost and
    # whose blind copy (one reservation period = 20 ms later) is delivered
    t0, t1, f, g = pick_retx_window(log, rowmap)
    draw(log, rowmap, rows, arrfilter, t0, t1,
         "Blind HARQ retransmission over PC5 (600 m, CQI 12): initial TB lost, blind copy delivered",
         os.path.join(HERE, "media/seqchart-blindretx.png"),
         annotate=[(f, 4.0, "initial TX: TB lost at the receiver"),
                   (g, 4.0, "blind copy (rv=1): soft-combined, delivered")])


def pick_retx_window(log, rowmap):
    """Find a TB whose initial transmission (a TX slot with an RLC SDU request,
    i.e. new data) is lost at ue[1], and whose blind copy one reservation
    period (20 ms) later - a TX slot with no SDU request - is delivered."""
    tx_frames = []   # (sendTime, arrivalTime) of slAirFrame ue[0] -> ue[1]
    deliveries = []  # times of SlMacPdu going up at ue[1] (slPhy -> slMac)
    requests = []    # times of LteMacSduRequest at ue[0] (new-data TX slots)
    for name, ta, src, tb, dst in log.arrows():
        if name == "slAirFrame" and rowmap(dst) == 4:
            tx_frames.append((ta, tb))
        if name == "SlMacPdu" and rowmap(src) == 4 and rowmap(dst) == 5:
            deliveries.append(ta)
        if name == "LteMacSduRequest" and rowmap(src) == 2:
            requests.append(ta)

    def has_request(t):
        return any(abs(r - t) < 1e-9 for r in requests)

    def delivered(arrival):
        return any(0 <= d - arrival < 0.001 for d in deliveries)

    for (fa, fb) in tx_frames:
        if not has_request(fa) or delivered(fb):
            continue  # want: new-data TX whose TB was lost
        for (ga, gb) in tx_frames:
            if abs(ga - (fa + 0.02)) < 1e-6 and not has_request(ga) and delivered(gb):
                return fa - 0.004, ga + 0.004, fa, ga
    print("note: no lost-initial + delivered-copy TB found, using the first 50 ms")
    return 0.0, 0.05, 0.0, 0.0


def make_fanout_chart():
    elog = os.path.join(HERE, "../results/Highway-Small/seqchart.elog")
    log = Eventlog(elog)
    paths = {mid: log.full_path(mid) for mid in log.modules}

    # sender = the UE whose slPhy transmits first in the window; expanded
    # lifelines for it, one collapsed lifeline per receiving vehicle
    sender = None
    t_first = None
    for name, ta, src, tb, dst in log.arrows():
        if name == "slAirFrame" and ta > 0.05:
            sender = re.search(r"(ue\[\d+\])", paths[src]).group(1)
            t_first = ta
            break

    receivers = []
    for name, ta, src, tb, dst in log.arrows():
        if name == "slAirFrame" and abs(ta - t_first) < 1e-9:
            rx = re.search(r"(ue\[\d+\])", paths[dst]).group(1)
            if rx != sender and rx not in receivers:
                receivers.append(rx)
    shown = receivers[:6]

    rows = [f"{sender} app+IP", f"{sender} PDCP/RLC", f"{sender} slMac", f"{sender} slPhy"] + \
           [f"{rx} (node)" for rx in shown]

    def rowmap(mid):
        p = paths.get(mid, "")
        m = re.search(r"(ue\[\d+\])", p)
        if not m:
            return None
        ue = m.group(1)
        if ue == sender:
            if ".slPhy" in p: return 3
            if ".slMac" in p: return 2
            if ".cellularNic" in p: return 1
            return 0
        if ue in shown:
            return 4 + shown.index(ue)
        return None

    def arrfilter(name, ra, rb):
        interesting = ("Alert", "slAirFrame", "LteMacSduRequest", "lteRlcFragment", "SlMacPdu")
        return any(name.startswith(i) for i in interesting)

    n_more = len(receivers) - len(shown)
    draw(log, rowmap, rows, arrfilter, t_first - 0.006, t_first + 0.004,
         f"CAM broadcast fan-out on the highway: one TB from {sender} reaches every vehicle in range"
         + (f" ({n_more} more receivers not shown)" if n_more > 0 else ""),
         os.path.join(HERE, "media/seqchart-fanout.png"))


if __name__ == "__main__":
    os.makedirs(os.path.join(HERE, "media"), exist_ok=True)
    make_blindretx_chart()
    make_fanout_chart()
